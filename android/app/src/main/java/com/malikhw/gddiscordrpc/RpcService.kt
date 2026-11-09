package com.malikhw.gddiscordrpc

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import androidx.core.app.NotificationCompat
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.SocketAddress
import java.net.SocketTimeoutException
import java.util.concurrent.ConcurrentLinkedQueue

class RpcService : Service() {
    companion object {
        const val APP_ID = 356942674672091136L
        const val PORT = 47653
        private const val ACTION_START = "com.malikhw.gddiscordrpc.START"
        private const val ACTION_STOP = "com.malikhw.gddiscordrpc.STOP"
        private const val CHANNEL = "rpc_status"
        private const val NOTIF_ID = 1

        private const val TICK_MS = 250L
        private const val RESEND_MS = 30_000L
        private const val CLEAR_FLUSH_MS = 1_500L
        private const val DISCONNECT_FLUSH_MS = 1_000L

        fun start(ctx: Context) {
            val i = Intent(ctx, RpcService::class.java).setAction(ACTION_START)
            if (Build.VERSION.SDK_INT >= 26) ctx.startForegroundService(i) else ctx.startService(i)
        }

        fun stop(ctx: Context) {
            ctx.startService(Intent(ctx, RpcService::class.java).setAction(ACTION_STOP))
        }
    }

    private val main = Handler(Looper.getMainLooper())
    private val outbox = ConcurrentLinkedQueue<String>()

    @Volatile private var socket: DatagramSocket? = null
    @Volatile private var peer: SocketAddress? = null
    @Volatile private var listening = false

    private var gameSeen = false
    private var current: Presence? = null
    private var lastSentAt = 0L
    private var retryAt = 0L
    private var retryDelay = 5_000L

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_START -> startHosting()
            ACTION_STOP -> {
                if (RpcState.phase == RpcState.Phase.Stopped) stopSelf() else beginStop()
            }
            else -> if (RpcState.phase == RpcState.Phase.Stopped) stopSelf()
        }
        return START_NOT_STICKY
    }

    override fun onDestroy() {
        // killed from outside
        if (RpcState.phase != RpcState.Phase.Stopped) {
            listening = false
            closeSocket()
            try {
                Native.clear(); Native.tick()
                Native.disconnect(); Native.tick()
                Native.destroy()
            } catch (_: Throwable) {}
            RpcState.update(RpcState.Phase.Stopped, "Not running", "")
        }
        main.removeCallbacksAndMessages(null)
        super.onDestroy()
    }

    // starting shit
    private fun startHosting() {
        enterForeground("Starting…")
        if (RpcState.phase != RpcState.Phase.Stopped && RpcState.phase != RpcState.Phase.Starting) return

        RpcState.update(RpcState.Phase.Starting, "Starting…", "")
        RpcState.log("starting")
        try {
            Native.load()
            Native.init(APP_ID)
        } catch (t: Throwable) {
            RpcState.log("couldn't start the Discord SDK: $t")
            failStart("Couldn't start the Discord SDK: ${t.message}")
            return
        }

        gameSeen = false
        current = null
        retryAt = 0L
        retryDelay = 5_000L
        listening = true
        Thread(::netLoop, "gdrpc-net").start()

        RpcState.update(RpcState.Phase.Running, waiting(), "")
        updateNotification(waiting())
        main.post(tick)
    }

    private fun failStart(msg: String) {
        listening = false
        RpcState.update(RpcState.Phase.Stopped, msg, "")
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    private fun waiting() = "Listening on 127.0.0.1:$PORT, waiting for Geometry Dash"

    private val tick = object : Runnable {
        override fun run() {
            if (RpcState.phase == RpcState.Phase.Stopped) return
            try {
                Native.tick()
                Native.drain().forEach(::onNativeEvent)
                if (RpcState.phase == RpcState.Phase.Running) housekeeping()
            } catch (t: Throwable) {
                RpcState.log("tick failed: $t")
            }
            main.postDelayed(this, TICK_MS)
        }
    }

    private fun housekeeping() {
        val now = System.currentTimeMillis()

        val p = current
        if (retryAt != 0L && now >= retryAt && p != null) {
            retryAt = 0L
            RpcState.log("retrying presence update")
            Native.update(p)
        }
    }

    private fun onNativeEvent(ev: String) {
        val parts = ev.split('\t', limit = 3)
        when (parts[0]) {
            "L" -> RpcState.log(parts.getOrElse(1) { "" })
            "S" -> RpcState.log("client status: ${parts.getOrElse(1) { "?" }}")
            "P" -> if (parts.getOrNull(1) == "1") {
                retryAt = 0L
                retryDelay = 5_000L
                val s = current?.summary() ?: return
                RpcState.log("Discord accepted the presence")
                RpcState.update(status = "Live on Discord", sending = s)
                updateNotification("Live: $s")
                reply("presence updated")
            } else {
                val err = parts.getOrElse(2) { "unknown error" }
                RpcState.log("Discord rejected the presence: $err")
                reply("presence update failed: $err")
                if (current != null && RpcState.phase == RpcState.Phase.Running) {
                    RpcState.update(status = "Discord rejected the update ($err), retrying in ${retryDelay / 1000}s")
                    retryAt = System.currentTimeMillis() + retryDelay
                    retryDelay = (retryDelay * 2).coerceAtMost(30_000L)
                }
            }
        }
    }

    private fun onPacket(text: String, from: SocketAddress) {
        if (RpcState.phase != RpcState.Phase.Running) return

        if (!gameSeen || from != peer) {
            gameSeen = true
            peer = from
            RpcState.log("Geometry Dash connected ($from)")
            reply("hello")
        }

        when (text) {
            "ping" -> return
            "disconnect", "quit" -> {
                RpcState.log("game closed, disconnecting (same as the Stop presence button)")
                beginStop()
                return
            }
        }

        val p = Presence.parse(text)
        if (p == null) {
            RpcState.log("couldn't parse packet: ${text.take(200)}")
            return
        }

        val now = System.currentTimeMillis()
        if (p == current && now - lastSentAt < RESEND_MS) return

        RpcState.log("sending to Discord: ${p.summary()}")
        RpcState.update(status = "Sending to Discord…", sending = p.summary())
        current = p
        lastSentAt = now
        retryAt = 0L
        Native.update(p)
    }

    private fun beginStop() {
        if (RpcState.phase == RpcState.Phase.Stopping || RpcState.phase == RpcState.Phase.Stopped) return
        RpcState.update(RpcState.Phase.Stopping, "Stopping…", "")
        RpcState.log("stopping")
        listening = false
        closeSocket()
        current = null
        retryAt = 0L
        try { Native.clear() } catch (_: Throwable) {}

        main.postDelayed({
            try { Native.disconnect() } catch (_: Throwable) {}
            main.postDelayed(::finishStop, DISCONNECT_FLUSH_MS)
        }, CLEAR_FLUSH_MS)
    }

    private fun finishStop() {
        try {
            Native.tick()
            Native.drain().forEach(::onNativeEvent)
            Native.destroy()
        } catch (_: Throwable) {}
        RpcState.log("stopped")
        RpcState.update(RpcState.Phase.Stopped, "Not running", "")
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    private fun enterForeground(text: String) {
        val nm = getSystemService(NotificationManager::class.java)
        if (Build.VERSION.SDK_INT >= 26) {
            nm.createNotificationChannel(NotificationChannel(CHANNEL, "Discord RPC status", NotificationManager.IMPORTANCE_LOW))
        }
        val n = notification(text)
        if (Build.VERSION.SDK_INT >= 34) {
            startForeground(NOTIF_ID, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE)
        } else {
            startForeground(NOTIF_ID, n)
        }
    }

    private fun updateNotification(text: String) {
        getSystemService(NotificationManager::class.java).notify(NOTIF_ID, notification(text))
    }

    private fun notification(text: String): Notification {
        val open = PendingIntent.getActivity(
            this, 0, Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )
        return NotificationCompat.Builder(this, CHANNEL)
            .setSmallIcon(applicationInfo.icon)
            .setContentTitle("Discord RPC")
            .setContentText(text)
            .setStyle(NotificationCompat.BigTextStyle().bigText(text))
            .setContentIntent(open)
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            .setShowWhen(false)
            .build()
    }

    private fun reply(msg: String) {
        if (outbox.size < 100) outbox.add(msg.take(1500))
    }

    private fun closeSocket() {
        try { socket?.close() } catch (_: Exception) {}
    }

    private fun flush(s: DatagramSocket) {
        val to = peer ?: return
        while (true) {
            val msg = outbox.poll() ?: break
            try {
                val b = msg.toByteArray(Charsets.UTF_8)
                s.send(DatagramPacket(b, b.size, to))
            } catch (_: Exception) {}
        }
    }

    private fun netLoop() {
        val s: DatagramSocket
        try {
            s = DatagramSocket(null).apply {
                reuseAddress = true
                bind(InetSocketAddress(InetAddress.getByName("127.0.0.1"), PORT))
                soTimeout = TICK_MS.toInt()
            }
            socket = s
        } catch (t: Throwable) {
            RpcState.log("couldn't open port $PORT: $t")
            main.post {
                if (RpcState.phase == RpcState.Phase.Running) {
                    RpcState.log("is another copy of the app already running?")
                    beginStop()
                }
            }
            return
        }

        RpcState.log("host up on 127.0.0.1:$PORT")
        val buf = ByteArray(2048)
        try {
            while (listening) {
                try {
                    val pkt = DatagramPacket(buf, buf.size)
                    s.receive(pkt)
                    val text = String(pkt.data, 0, pkt.length, Charsets.UTF_8)
                    val from = pkt.socketAddress
                    main.post { onPacket(text, from) }
                } catch (_: SocketTimeoutException) {}
                flush(s)
            }
        } catch (t: Exception) {
            if (listening) RpcState.log("host error: $t")
        } finally {
            try { flush(s) } catch (_: Exception) {}
            s.close()
            socket = null
            RpcState.log("host closed")
        }
    }
}
