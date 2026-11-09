package com.malikhw.gddiscordrpc

import android.Manifest
import android.content.ComponentName
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.drawable.Icon
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.PowerManager
import android.provider.Settings
import android.view.ViewGroup
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import android.content.pm.ShortcutInfo
import android.content.pm.ShortcutManager
import com.google.android.material.button.MaterialButton
import com.google.android.material.checkbox.MaterialCheckBox
import com.google.android.material.color.DynamicColors
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.discord.socialsdk.DiscordSocialSdkInit

class MainActivity : AppCompatActivity(), RpcState.Listener {
    companion object {
        private const val GEODE_PACKAGE = "com.geode.launcher"
        private const val GEOMETRY_DASH_ACTIVITY = "com.geode.launcher.GeometryDashActivity"
    }

    private lateinit var statusText: TextView
    private lateinit var sendingText: TextView
    private lateinit var button: MaterialButton
    private lateinit var openGameCheckBox: MaterialCheckBox
    private lateinit var logView: RecyclerView
    private val adapter = LogAdapter()

    private var permissionsAsked = false
    private var askedNotifications = false
    private var askedBattery = false

    private val notificationRequest =
        registerForActivityResult(ActivityResultContracts.RequestPermission()) { requestNextPermission() }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        DynamicColors.applyToActivityIfAvailable(this)
        setContentView(R.layout.activity_main)
        permissionsAsked = savedInstanceState?.getBoolean("permsAsked") ?: false

        // the SDK wants an activity to hang on to
        try { DiscordSocialSdkInit.setEngineActivity(this) } catch (t: Throwable) {
            RpcState.log("SDK activity setup failed: $t")
        }

        statusText = findViewById(R.id.status)
        sendingText = findViewById(R.id.sending)
        button = findViewById(R.id.toggle)
        openGameCheckBox = findViewById(R.id.open_game)
        findViewById<MaterialButton>(R.id.add_shortcut).setOnClickListener { addGameShortcut() }
        logView = findViewById(R.id.log)
        logView.layoutManager = LinearLayoutManager(this)
        logView.adapter = adapter
        logView.itemAnimator = null

        button.setOnClickListener { onToggle() }

        if (!permissionsAsked) {
            permissionsAsked = true
            showPermissionPopup()
        }

    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        outState.putBoolean("permsAsked", permissionsAsked)
    }

    override fun onStart() {
        super.onStart()
        RpcState.addListener(this)
        adapter.setAll(RpcState.snapshot())
        if (adapter.itemCount > 0) logView.scrollToPosition(adapter.itemCount - 1)
        render()
    }

    override fun onStop() {
        RpcState.removeListener(this)
        super.onStop()
    }

    override fun onStatusChanged() = render()

    override fun onLog(line: String, trimmedOldest: Boolean) {
        val atBottom = !logView.canScrollVertically(1)
        adapter.add(line, trimmedOldest)
        if (atBottom) logView.scrollToPosition(adapter.itemCount - 1)
    }

    private fun render() {
        statusText.text = RpcState.status
        sendingText.text = if (RpcState.sending.isEmpty()) "Nothing is being sent to Discord" else "Sending: ${RpcState.sending}"
        when (RpcState.phase) {
            RpcState.Phase.Stopped -> {
                button.text = "Start Listening for presence"
                button.isEnabled = true
            }
            RpcState.Phase.Starting, RpcState.Phase.Running -> {
                button.text = "Stop presence"
                button.isEnabled = true
            }
            RpcState.Phase.Stopping -> {
                button.text = "Stopping…"
                button.isEnabled = false
            }
        }
    }

    private fun onToggle() {
        when (RpcState.phase) {
            RpcState.Phase.Stopped -> startPresence(openGameCheckBox.isChecked)
            RpcState.Phase.Starting, RpcState.Phase.Running -> RpcService.stop(this)
            RpcState.Phase.Stopping -> Unit
        }
    }

    private fun startPresence(openGame: Boolean) {
        RpcState.update(RpcState.Phase.Starting, "Starting…", "")
        try {
            RpcService.start(this)
            if (openGame) openGeometryDash()
        } catch (t: Throwable) {
            RpcState.log("couldn't start the service: $t")
            RpcState.update(RpcState.Phase.Stopped, "Couldn't start: ${t.message}", "")
        }
    }

    private fun openGeometryDash() {
        val intent = Intent().setComponent(ComponentName(GEODE_PACKAGE, GEOMETRY_DASH_ACTIVITY))
        try {
            if (packageManager.resolveActivity(intent, PackageManager.MATCH_DEFAULT_ONLY) == null) {
                throw IllegalStateException("$GEOMETRY_DASH_ACTIVITY is not installed")
            }
            startActivity(intent)
        } catch (t: Throwable) {
            RpcState.log("couldn't open Geometry Dash: $t")
            Toast.makeText(this, "Couldn't open Geometry Dash", Toast.LENGTH_LONG).show()
        }
    }

    private fun addGameShortcut() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            Toast.makeText(this, "Home screen shortcuts need Android 8.0 or newer", Toast.LENGTH_LONG).show()
            return
        }

        val shortcutIntent = Intent(this, ShortcutActivity::class.java)
            .setAction(Intent.ACTION_VIEW)
        val shortcut = ShortcutInfo.Builder(this, "geometry-dash-rpc")
            .setShortLabel("Geometry Dash (RPC)")
            .setLongLabel("Geometry Dash (RPC)")
            .setIcon(Icon.createWithResource(this, R.mipmap.ic_launcher))
            .setIntent(shortcutIntent)
            .build()

        val shortcutManager = getSystemService(ShortcutManager::class.java)
        if (shortcutManager.isRequestPinShortcutSupported &&
            shortcutManager.requestPinShortcut(shortcut, null)
        ) {
            Toast.makeText(this, "Choose where to add the shortcut", Toast.LENGTH_LONG).show()
        } else {
            Toast.makeText(this, "This launcher doesn't support home screen shortcuts", Toast.LENGTH_LONG).show()
        }
    }

    private fun notificationsGranted() =
        Build.VERSION.SDK_INT < 33 ||
            checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED

    private fun batteryExempt() =
        getSystemService(PowerManager::class.java).isIgnoringBatteryOptimizations(packageName)

    private fun showPermissionPopup() {
        val needs = mutableListOf<String>()
        if (!notificationsGranted()) needs += "• Notifications: shows the status notification that keeps the app alive"
        if (!batteryExempt()) needs += "• Unrestricted battery: so Android doesn't silently kill the presence"
        if (needs.isEmpty()) return

        MaterialAlertDialogBuilder(this)
            .setTitle("Permissions needed")
            .setMessage("Please accept these so presence keeps working while Geometry Dash is open:\n\n" + needs.joinToString("\n"))
            .setCancelable(false)
            .setNegativeButton("Not now", null)
            .setPositiveButton("Continue") { _, _ -> requestNextPermission() }
            .show()
    }

    private fun requestNextPermission() {
        if (!askedNotifications && !notificationsGranted()) {
            askedNotifications = true
            notificationRequest.launch(Manifest.permission.POST_NOTIFICATIONS)
            return
        }
        if (!askedBattery && !batteryExempt()) {
            askedBattery = true
            try {
                startActivity(
                    Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS, Uri.parse("package:$packageName"))
                )
            } catch (t: Throwable) {
                RpcState.log("battery prompt failed: $t")
            }
        }
    }

    private class LogAdapter : RecyclerView.Adapter<LogAdapter.VH>() {
        private val items = ArrayList<String>()

        class VH(val tv: TextView) : RecyclerView.ViewHolder(tv)

        fun setAll(lines: List<String>) {
            items.clear()
            items.addAll(lines)
            notifyDataSetChanged()
        }

        fun add(line: String, trimmedOldest: Boolean) {
            if (trimmedOldest && items.isNotEmpty()) {
                items.removeAt(0)
                notifyItemRemoved(0)
            }
            items.add(line)
            notifyItemInserted(items.size - 1)
        }

        override fun getItemCount() = items.size

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): VH {
            val tv = TextView(parent.context).apply {
                layoutParams = RecyclerView.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
                )
                typeface = android.graphics.Typeface.MONOSPACE
                textSize = 11f
                setTextIsSelectable(true)
                val pad = (2 * resources.displayMetrics.density).toInt()
                setPadding(0, pad, 0, pad)
            }
            return VH(tv)
        }

        override fun onBindViewHolder(holder: VH, position: Int) {
            holder.tv.text = items[position]
        }
    }
}
