package com.malikhw.gddiscordrpc

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import android.util.Log
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.SocketTimeoutException

class RpcService : Service() {
    companion object {
        const val APP_ID = 1567396198929793025L
        const val PORT = 47653
        private const val CHANNEL = "rpc"
        private const val IDLE_TIMEOUT_MS = 60_000L
        private const val RESEND_MS = 30_000L
    }

    @Volatile private var running = false

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        startInForeground()
        if (!running) {
            running = true
            Thread(::loop, "gdrpc").start()
        }
        return START_NOT_STICKY
    }

    override fun onDestroy() {
        running = false
        super.onDestroy()
    }

    private fun startInForeground() {
        val nm = getSystemService(NotificationManager::class.java)
        if (Build.VERSION.SDK_INT >= 26) {
            nm.createNotificationChannel(
                NotificationChannel(CHANNEL, "Discord RPC", NotificationManager.IMPORTANCE_MIN)
            )
        }
        val n = Notification.Builder(this, CHANNEL)
            .setSmallIcon(applicationInfo.icon)
            .setContentTitle("Discord RPC")
            .setContentText("Sharing your Geometry Dash activity")
            .setOngoing(true)
            .build()

        if (Build.VERSION.SDK_INT >= 34) {
            startForeground(1, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE)
        } else {
            startForeground(1, n)
        }
    }

    private fun loop() {
        var socket: DatagramSocket? = null
        try {
            Native.load()
            Native.init(APP_ID)

            socket = DatagramSocket(null).apply {
                reuseAddress = true
                bind(InetSocketAddress(InetAddress.getByName("127.0.0.1"), PORT))
                soTimeout = 250
            }

            val buf = ByteArray(2048)
            var lastPacket = System.currentTimeMillis()
            var lastSent = 0L
            var lastData: Presence? = null

            while (running) {
                try {
                    val pkt = DatagramPacket(buf, buf.size)
                    socket.receive(pkt)
                    lastPacket = System.currentTimeMillis()

                    val text = String(pkt.data, 0, pkt.length, Charsets.UTF_8)
                    if (text == "quit") break

                    val p = Presence.parse(text)
                    // gd resends every few seconds, only bother discord on changes
                    if (p != null && (p != lastData || lastPacket - lastSent > RESEND_MS)) {
                        Native.update(p)
                        lastData = p
                        lastSent = lastPacket
                    }
                } catch (_: SocketTimeoutException) {
                }

                Native.tick()

                if (System.currentTimeMillis() - lastPacket > IDLE_TIMEOUT_MS) break
            }
        } catch (t: Throwable) {
            Log.e("GDRPC", "service loop died", t)
        } finally {
            try { Native.clear(); Native.tick() } catch (_: Throwable) {}
            socket?.close()
            running = false
            stopForeground(STOP_FOREGROUND_REMOVE)
            stopSelf()
        }
    }
}
