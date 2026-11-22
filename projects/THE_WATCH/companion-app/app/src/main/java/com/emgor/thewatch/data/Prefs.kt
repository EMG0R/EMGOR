package com.emgor.thewatch.data

import android.content.Context
import android.content.SharedPreferences
import androidx.core.content.edit

/**
 * Thin SharedPreferences wrapper for all user settings. No third-party
 * persistence library — keeps the app dependency-light and fully self-owned.
 */
object Prefs {
    private const val FILE = "the_watch_prefs"

    // Keys
    private const val K_UNITS = "units"                 // "F" or "C" (app display only)
    private const val K_WEATHER_MODE = "weather_mode"   // "phone" or "manual"
    private const val K_MANUAL_LAT = "manual_lat"
    private const val K_MANUAL_LON = "manual_lon"
    private const val K_MANUAL_NAME = "manual_name"
    private const val K_WEATHER_MINUTES = "weather_minutes"
    private const val K_MUTED_PKGS = "muted_pkgs"       // CSV of muted package names
    private const val K_LAST_DEVICE = "last_device_addr"
    private const val K_SEND_BATTERY = "send_battery"
    private const val K_SEND_CALENDAR = "send_calendar"

    private fun sp(ctx: Context): SharedPreferences =
        ctx.getSharedPreferences(FILE, Context.MODE_PRIVATE)

    /** "F" (default) or "C". Only affects the app's own display; the watch stores Celsius. */
    fun units(ctx: Context): String = sp(ctx).getString(K_UNITS, "F") ?: "F"
    fun setUnits(ctx: Context, v: String) = sp(ctx).edit { putString(K_UNITS, v) }

    /** "phone" (default, use device location) or "manual". */
    fun weatherMode(ctx: Context): String = sp(ctx).getString(K_WEATHER_MODE, "phone") ?: "phone"
    fun setWeatherMode(ctx: Context, v: String) = sp(ctx).edit { putString(K_WEATHER_MODE, v) }

    fun manualLat(ctx: Context): Double =
        sp(ctx).getString(K_MANUAL_LAT, null)?.toDoubleOrNull() ?: Double.NaN
    fun manualLon(ctx: Context): Double =
        sp(ctx).getString(K_MANUAL_LON, null)?.toDoubleOrNull() ?: Double.NaN
    fun manualName(ctx: Context): String = sp(ctx).getString(K_MANUAL_NAME, "") ?: ""
    fun setManualLocation(ctx: Context, lat: Double, lon: Double, name: String) =
        sp(ctx).edit {
            putString(K_MANUAL_LAT, lat.toString())
            putString(K_MANUAL_LON, lon.toString())
            putString(K_MANUAL_NAME, name)
        }

    /** Weather refresh interval in minutes (default 60). */
    fun weatherMinutes(ctx: Context): Int = sp(ctx).getInt(K_WEATHER_MINUTES, 60)
    fun setWeatherMinutes(ctx: Context, m: Int) = sp(ctx).edit { putInt(K_WEATHER_MINUTES, m) }

    /** Default forward ALL: the muted set starts empty. */
    fun mutedPackages(ctx: Context): Set<String> {
        val csv = sp(ctx).getString(K_MUTED_PKGS, "") ?: ""
        return if (csv.isBlank()) emptySet() else csv.split(",").toSet()
    }
    fun isMuted(ctx: Context, pkg: String): Boolean = mutedPackages(ctx).contains(pkg)
    fun setMuted(ctx: Context, pkg: String, muted: Boolean) {
        val set = mutedPackages(ctx).toMutableSet()
        if (muted) set.add(pkg) else set.remove(pkg)
        sp(ctx).edit { putString(K_MUTED_PKGS, set.joinToString(",")) }
    }

    fun lastDevice(ctx: Context): String? = sp(ctx).getString(K_LAST_DEVICE, null)
    fun setLastDevice(ctx: Context, addr: String?) = sp(ctx).edit { putString(K_LAST_DEVICE, addr) }

    fun sendBattery(ctx: Context): Boolean = sp(ctx).getBoolean(K_SEND_BATTERY, true)
    fun setSendBattery(ctx: Context, v: Boolean) = sp(ctx).edit { putBoolean(K_SEND_BATTERY, v) }

    fun sendCalendar(ctx: Context): Boolean = sp(ctx).getBoolean(K_SEND_CALENDAR, true)
    fun setSendCalendar(ctx: Context, v: Boolean) = sp(ctx).edit { putBoolean(K_SEND_CALENDAR, v) }
}
