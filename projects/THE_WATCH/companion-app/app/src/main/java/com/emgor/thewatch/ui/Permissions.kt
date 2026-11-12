package com.emgor.thewatch.ui

import android.Manifest
import android.app.NotificationManager
import android.content.ComponentName
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.PowerManager
import android.provider.Settings
import androidx.core.content.ContextCompat
import com.emgor.thewatch.service.WatchNotificationListener

/** Pure permission-status checks (no UI). */
object Permissions {

    fun hasNotificationAccess(ctx: Context): Boolean {
        val flat = Settings.Secure.getString(ctx.contentResolver, "enabled_notification_listeners")
            ?: return false
        val cn = ComponentName(ctx, WatchNotificationListener::class.java)
        return flat.split(":").any {
            val c = ComponentName.unflattenFromString(it)
            c != null && (c == cn || c.packageName == ctx.packageName)
        }
    }

    fun hasBluetooth(ctx: Context): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            granted(ctx, Manifest.permission.BLUETOOTH_CONNECT) &&
                granted(ctx, Manifest.permission.BLUETOOTH_SCAN)
        } else {
            // Pre-12 uses install-time BLUETOOTH/ADMIN; location needed for scan.
            granted(ctx, Manifest.permission.ACCESS_FINE_LOCATION)
        }
    }

    fun hasLocation(ctx: Context): Boolean =
        granted(ctx, Manifest.permission.ACCESS_FINE_LOCATION) ||
            granted(ctx, Manifest.permission.ACCESS_COARSE_LOCATION)

    fun hasPostNotifications(ctx: Context): Boolean =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
            granted(ctx, Manifest.permission.POST_NOTIFICATIONS) else true

    fun hasCalendar(ctx: Context): Boolean = granted(ctx, Manifest.permission.READ_CALENDAR)

    fun isIgnoringBatteryOptimizations(ctx: Context): Boolean {
        val pm = ctx.getSystemService(Context.POWER_SERVICE) as PowerManager
        return pm.isIgnoringBatteryOptimizations(ctx.packageName)
    }

    fun hasDndAccess(ctx: Context): Boolean {
        val nm = ctx.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        return nm.isNotificationPolicyAccessGranted
    }

    /** Runtime permissions to request together for the "Bluetooth + location" button. */
    fun runtimePermsToRequest(): Array<String> {
        val list = mutableListOf(
            Manifest.permission.ACCESS_FINE_LOCATION,
            Manifest.permission.ACCESS_COARSE_LOCATION,
            Manifest.permission.READ_CALENDAR,
        )
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            list.add(Manifest.permission.BLUETOOTH_CONNECT)
            list.add(Manifest.permission.BLUETOOTH_SCAN)
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            list.add(Manifest.permission.POST_NOTIFICATIONS)
        }
        return list.toTypedArray()
    }

    private fun granted(ctx: Context, perm: String): Boolean =
        ContextCompat.checkSelfPermission(ctx, perm) == PackageManager.PERMISSION_GRANTED
}
