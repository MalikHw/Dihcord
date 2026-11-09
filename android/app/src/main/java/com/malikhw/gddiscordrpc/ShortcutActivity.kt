package com.malikhw.gddiscordrpc

import android.app.Activity
import android.content.ComponentName
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.widget.Toast

class ShortcutActivity : Activity() {
    companion object {
        private const val GEODE_PACKAGE = "com.geode.launcher"
        private const val GEOMETRY_DASH_ACTIVITY = "com.geode.launcher.GeometryDashActivity"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        RpcState.update(RpcState.Phase.Starting, "Starting…", "")
        try {
            RpcService.start(this)
            val gameIntent = Intent().setComponent(
                ComponentName(GEODE_PACKAGE, GEOMETRY_DASH_ACTIVITY)
            )
            if (packageManager.resolveActivity(gameIntent, PackageManager.MATCH_DEFAULT_ONLY) == null) {
                throw IllegalStateException("$GEOMETRY_DASH_ACTIVITY is not installed")
            }
            startActivity(gameIntent)
        } catch (t: Throwable) {
            RpcState.log("shortcut could not start presence or Geometry Dash: $t")
            Toast.makeText(this, "Couldn't start Geometry Dash", Toast.LENGTH_LONG).show()
        } finally {
            finish()
        }
    }
}
