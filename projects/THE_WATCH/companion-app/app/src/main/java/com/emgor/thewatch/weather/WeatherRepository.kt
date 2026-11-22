package com.emgor.thewatch.weather

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.location.Geocoder
import android.location.Location
import android.location.LocationManager
import android.util.Log
import androidx.core.content.ContextCompat
import com.emgor.thewatch.ble.GbProtocol
import com.emgor.thewatch.data.Prefs
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import java.util.Locale
import kotlin.math.roundToInt

/**
 * Fetches current conditions from Open-Meteo (free, no API key) for the phone's
 * location (or a manual override) and returns a ready-to-send `weather` line.
 *
 * The watch stores temperature in Celsius (converting from the Kelvin we send),
 * so we always request Celsius from Open-Meteo. The app's °F/°C setting only
 * affects the app's OWN display, not what the watch shows.
 */
object WeatherRepository {
    private const val TAG = "TheWatch/Weather"

    data class Conditions(
        val tempC: Double,
        val humidity: Int,
        val windKmh: Int,
        val description: String,
        val locationName: String,
        val aqi: Int   // US AQI (-1 = unavailable)
    )

    /** Fetch conditions; returns null if location/network unavailable. */
    fun fetch(context: Context): Conditions? {
        val ctx = context.applicationContext
        val loc = resolveLocation(ctx) ?: run {
            Log.w(TAG, "no location available"); return null
        }
        val (lat, lon, name) = loc
        val url = URL(
            "https://api.open-meteo.com/v1/forecast" +
                "?latitude=$lat&longitude=$lon" +
                "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m" +
                "&temperature_unit=celsius&wind_speed_unit=kmh&timezone=auto"
        )
        return try {
            val conn = (url.openConnection() as HttpURLConnection).apply {
                connectTimeout = 12000
                readTimeout = 12000
                requestMethod = "GET"
                setRequestProperty("User-Agent", "TheWatch-Companion/1.0")
            }
            val code = conn.responseCode
            if (code != 200) { Log.e(TAG, "HTTP $code"); return null }
            val body = conn.inputStream.bufferedReader().use { it.readText() }
            conn.disconnect()
            val cur = JSONObject(body).getJSONObject("current")
            Conditions(
                tempC = cur.optDouble("temperature_2m", 0.0),
                humidity = cur.optDouble("relative_humidity_2m", 0.0).roundToInt(),
                windKmh = cur.optDouble("wind_speed_10m", 0.0).roundToInt(),
                description = wmoText(cur.optInt("weather_code", -1)),
                locationName = name,
                aqi = fetchAqi(lat, lon)
            )
        } catch (e: Exception) {
            Log.e(TAG, "fetch failed", e); null
        }
    }

    /** Fetch US AQI from Open-Meteo air-quality API (free, no key). -1 on failure. */
    private fun fetchAqi(lat: Double, lon: Double): Int {
        return try {
            val url = URL(
                "https://air-quality-api.open-meteo.com/v1/air-quality" +
                    "?latitude=$lat&longitude=$lon&current=us_aqi&timezone=auto"
            )
            val conn = (url.openConnection() as HttpURLConnection).apply {
                connectTimeout = 12000; readTimeout = 12000; requestMethod = "GET"
                setRequestProperty("User-Agent", "TheWatch-Companion/1.0")
            }
            if (conn.responseCode != 200) { conn.disconnect(); return -1 }
            val body = conn.inputStream.bufferedReader().use { it.readText() }
            conn.disconnect()
            val v = JSONObject(body).optJSONObject("current")?.optDouble("us_aqi", Double.NaN)
                ?: Double.NaN
            if (v.isNaN()) -1 else v.roundToInt()
        } catch (e: Exception) {
            Log.w(TAG, "aqi fetch failed", e); -1
        }
    }

    /** Fetch and emit a framed weather line via [send]. Returns true on success. */
    fun fetchAndSend(context: Context, send: (String) -> Unit): Boolean {
        val c = fetch(context) ?: return false
        send(
            GbProtocol.weatherLine(
                tempCelsius = c.tempC,
                humidityPct = c.humidity,
                description = c.description,
                windKmh = c.windKmh,
                location = c.locationName,
                aqi = c.aqi
            )
        )
        Log.i(TAG, "sent weather: ${c.tempC}C ${c.humidity}% ${c.description} AQI=${c.aqi} @ ${c.locationName}")
        return true
    }

    private data class Loc(val lat: Double, val lon: Double, val name: String)

    private fun resolveLocation(ctx: Context): Loc? {
        if (Prefs.weatherMode(ctx) == "manual") {
            val lat = Prefs.manualLat(ctx)
            val lon = Prefs.manualLon(ctx)
            if (!lat.isNaN() && !lon.isNaN()) {
                val nm = Prefs.manualName(ctx).ifBlank { fmtCoord(lat, lon) }
                return Loc(lat, lon, nm)
            }
        }
        // Phone location.
        val fine = ContextCompat.checkSelfPermission(ctx, Manifest.permission.ACCESS_FINE_LOCATION)
        val coarse =
            ContextCompat.checkSelfPermission(ctx, Manifest.permission.ACCESS_COARSE_LOCATION)
        if (fine != PackageManager.PERMISSION_GRANTED &&
            coarse != PackageManager.PERMISSION_GRANTED
        ) return null

        val lm = ctx.getSystemService(Context.LOCATION_SERVICE) as? LocationManager ?: return null
        val best = bestLastKnown(lm) ?: return null
        val name = reverseGeocode(ctx, best.latitude, best.longitude)
            ?: fmtCoord(best.latitude, best.longitude)
        return Loc(best.latitude, best.longitude, name)
    }

    @Suppress("MissingPermission") // guarded by caller
    private fun bestLastKnown(lm: LocationManager): Location? {
        var best: Location? = null
        for (p in lm.getProviders(true)) {
            val l = runCatching { lm.getLastKnownLocation(p) }.getOrNull() ?: continue
            val b = best
            if (b == null || l.time > b.time) best = l
        }
        return best
    }

    private fun reverseGeocode(ctx: Context, lat: Double, lon: Double): String? {
        return try {
            @Suppress("DEPRECATION")
            val list = Geocoder(ctx, Locale.getDefault()).getFromLocation(lat, lon, 1)
            val a = list?.firstOrNull() ?: return null
            a.locality ?: a.subAdminArea ?: a.adminArea ?: a.countryName
        } catch (e: Exception) {
            null
        }
    }

    private fun fmtCoord(lat: Double, lon: Double): String =
        String.format(Locale.US, "%.2f,%.2f", lat, lon)

    /** WMO weather interpretation codes -> short human text (Bangle-friendly). */
    private fun wmoText(code: Int): String = when (code) {
        0 -> "Clear"
        1 -> "Mainly clear"
        2 -> "Partly cloudy"
        3 -> "Overcast"
        45, 48 -> "Fog"
        51, 53, 55 -> "Drizzle"
        56, 57 -> "Freezing drizzle"
        61, 63, 65 -> "Rain"
        66, 67 -> "Freezing rain"
        71, 73, 75 -> "Snow"
        77 -> "Snow grains"
        80, 81, 82 -> "Showers"
        85, 86 -> "Snow showers"
        95 -> "Thunderstorm"
        96, 99 -> "Thunderstorm hail"
        else -> "Unknown"
    }
}
