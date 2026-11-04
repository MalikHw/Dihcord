package com.malikhw.gddiscordrpc

import org.json.JSONObject

data class Presence(
    val name: String,
    val state: String,
    val details: String,
    val largeKey: String,
    val largeText: String,
    val smallKey: String,
    val smallText: String,
    val start: Long
) {
    companion object {
        fun parse(json: String): Presence? = try {
            val o = JSONObject(json)
            Presence(
                o.optString("name", "Geometry Dash"),
                o.optString("state"),
                o.optString("details"),
                o.optString("largeKey"),
                o.optString("largeText"),
                o.optString("smallKey"),
                o.optString("smallText"),
                o.optLong("start")
            )
        } catch (_: Exception) {
            null
        }
    }
}
