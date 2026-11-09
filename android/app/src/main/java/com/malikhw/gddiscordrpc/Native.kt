package com.malikhw.gddiscordrpc

// bridge to libgdrpc.so (presence.cpp)
object Native {
    private var loaded = false

    @Synchronized
    fun load() {
        if (loaded) return
        try { System.loadLibrary("discord_partner_sdk") } catch (_: Throwable) {}
        System.loadLibrary("gdrpc")
        loaded = true
    }

    fun init(appId: Long): Boolean = nativeInit(appId)

    fun update(p: Presence) {
        val fields = listOf(p.details, p.state, p.largeKey, p.largeText, p.smallKey, p.smallText)
            .joinToString("\u001f") { it.replace('\u001f', ' ') }
        nativeUpdate(fields.toByteArray(Charsets.UTF_8), p.startMs)
    }

    fun clear() = nativeClear()

    fun disconnect() = nativeDisconnect()

    fun destroy() = nativeDestroy()
    fun tick() = nativeTick()

    fun drain(): List<String> {
        val s = String(nativeDrain(), Charsets.UTF_8)
        return if (s.isEmpty()) emptyList() else s.split('\n')
    }

    private external fun nativeInit(appId: Long): Boolean
    private external fun nativeUpdate(fields: ByteArray, startMs: Long)
    private external fun nativeClear()
    private external fun nativeDisconnect()
    private external fun nativeDestroy()
    private external fun nativeTick()
    private external fun nativeDrain(): ByteArray
}
