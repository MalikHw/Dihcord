package com.malikhw.gddiscordrpc

import org.json.JSONObject

data class Presence(
    val details: String,
    val state: String,
    val largeKey: String = "",
    val largeText: String = "",
    val smallKey: String = "",
    val smallText: String = "",
    val startMs: Long = 0
) {
    fun summary(): String =
        listOf(details, state).filter { it.isNotBlank() }.joinToString(" - ").ifEmpty { "(empty presence)" }

    companion object {
        fun parse(json: String): Presence? = try {
            val o = JSONObject(json)
            var start = o.optLong("start", 0)
            if (start in 1 until 100_000_000_000L) start *= 1000
            Presence(
                details = o.optString("details"),
                state = o.optString("state"),
                largeKey = o.optString("largeKey"),
                largeText = o.optString("largeText"),
                smallKey = o.optString("smallKey"),
                smallText = o.optString("smallText"),
                startMs = start
            )
        } catch (_: Exception) {
            null
        }
    }
}
