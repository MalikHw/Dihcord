package com.malikhw.gddiscordrpc

import android.os.Handler
import android.os.Looper
import android.util.Log
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

object RpcState {
    enum class Phase { Stopped, Starting, Running, Stopping }

    interface Listener {
        fun onStatusChanged() {}
        fun onLog(line: String, trimmedOldest: Boolean) {}
    }

    private const val MAX_LINES = 500
    private val main = Handler(Looper.getMainLooper())
    private val lines = ArrayDeque<String>()
    private val listeners = LinkedHashSet<Listener>()

    var phase = Phase.Stopped; private set
    var status = "Not running"; private set
    var sending = ""; private set

    private fun onMain(block: () -> Unit) {
        if (Looper.myLooper() == Looper.getMainLooper()) block() else main.post(block)
    }

    fun addListener(l: Listener) { listeners.add(l) }
    fun removeListener(l: Listener) { listeners.remove(l) }
    fun snapshot(): List<String> = lines.toList()

    fun update(phase: Phase? = null, status: String? = null, sending: String? = null) = onMain {
        if (phase != null) this.phase = phase
        if (status != null) this.status = status
        if (sending != null) this.sending = sending
        listeners.toList().forEach { it.onStatusChanged() }
    }

    fun log(msg: String) {
        Log.i("GDRPC", msg)
        val stamp = SimpleDateFormat("HH:mm:ss", Locale.US).format(Date())
        onMain {
            for (part in msg.split('\n')) {
                if (part.isBlank()) continue
                val line = "$stamp  $part"
                lines.addLast(line)
                val trimmed = lines.size > MAX_LINES
                if (trimmed) lines.removeFirst()
                listeners.toList().forEach { it.onLog(line, trimmed) }
            }
        }
    }
}
