package com.malikhw.gddiscordrpc

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import com.discord.socialsdk.DiscordSocialSdkInit

// Invisible. The mod starts this once per game launch, it sets the sdk up,
// starts the service and gets out of the way.
class RpcActivity : Activity() {
    private var goBack = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setup()
    }

    override fun onNewIntent(intent: Intent?) {
        super.onNewIntent(intent)
        setup()
    }

    private fun setup() {
        try {
            DiscordSocialSdkInit.setEngineActivity(this)
            Native.load()
            startForegroundService(Intent(this, RpcService::class.java))
        } catch (t: Throwable) {
            Log.e("GDRPC", "setup failed", t)
        }
        goBack = true
    }

    override fun onResume() {
        super.onResume()
        if (goBack) {
            goBack = false
            Handler(Looper.getMainLooper()).post { moveTaskToBack(true) }
        }
    }
}
