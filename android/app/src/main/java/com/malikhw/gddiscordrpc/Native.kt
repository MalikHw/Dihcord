package com.malikhw.gddiscordrpc

object Native {
    private var loaded = false

    @Synchronized
    fun load() {
        if (loaded) return
        // the sdk registers its jni bits when it's loaded by name from java
        try { System.loadLibrary("discord_partner_sdk") } catch (_: Throwable) {}
        System.loadLibrary("gdrpc")
        loaded = true
    }

    fun init(appId: Long) = nativeInit(appId)

    fun update(p: Presence) = nativeUpdate(
        p.name, p.state, p.details, p.largeKey, p.largeText, p.smallKey, p.smallText, p.start
    )

    fun clear() = nativeClear()
    fun tick() = nativeTick()

    private external fun nativeInit(appId: Long)
    private external fun nativeUpdate(
        name: String, state: String, details: String,
        largeKey: String, largeText: String, smallKey: String, smallText: String, start: Long
    )
    private external fun nativeClear()
    private external fun nativeTick()
}
