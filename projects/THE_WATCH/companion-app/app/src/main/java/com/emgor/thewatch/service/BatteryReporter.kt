package com.emgor.thewatch.service

import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import com.emgor.thewatch.ble.GbProtocol

/** Reads the phone battery and builds a phoneBattery line for the watch. */
object BatteryReporter {

    fun line(context: Context): String? {
        val ctx = context.applicationContext
        val bm = ctx.getSystemService(Context.BATTERY_SERVICE) as? BatteryManager ?: return null
        var pct = bm.getIntProperty(BatteryManager.BATTERY_PROPERTY_CAPACITY)
        var charging = bm.isCharging
        if (pct < 0) {
            // Fallback via sticky broadcast if the property is unsupported.
            val i: Intent? = ctx.registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
            if (i != null) {
                val level = i.getIntExtra(BatteryManager.EXTRA_LEVEL, -1)
                val scale = i.getIntExtra(BatteryManager.EXTRA_SCALE, -1)
                if (level >= 0 && scale > 0) pct = (level * 100) / scale
                val status = i.getIntExtra(BatteryManager.EXTRA_STATUS, -1)
                charging = status == BatteryManager.BATTERY_STATUS_CHARGING ||
                    status == BatteryManager.BATTERY_STATUS_FULL
            }
        }
        if (pct < 0) return null
        return GbProtocol.phoneBatteryLine(pct, charging)
    }
}
