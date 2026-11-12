package com.emgor.thewatch.ble

import org.json.JSONObject
import java.util.TimeZone

/**
 * Builders + parsers for the "Bangle.js / Gadgetbridge" wire protocol that THE
 * WATCH FIRMWARE ALREADY SPEAKS. This file was written directly against
 * firmware/the_watch/src/ble/ble_gadgetbridge.cpp so the watch needs NO changes.
 *
 * ── Transport (Nordic UART Service, watch is the GATT peripheral/server) ──────
 *   Service  6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 *   RX write 6E400002-…  phone -> watch   (this app WRITES here)
 *   TX notify 6E400003-… watch -> phone   (this app SUBSCRIBES here)
 *   Advertised name: "Bangle.js EMGOR" (must start "Bangle.js").
 *
 * ── phone -> watch framing (what the firmware's RxCallbacks parses) ───────────
 *   The watch assembles bytes until '\n', strips '\r', then strips any leading
 *   control byte <= 0x20 that is not '{'. It then (a) sniffs setTime()/E.setTimeZone()
 *   as raw Espruino JS and (b) extracts JSON from GB({...}) or a bare {...}.
 *   We prefix GB()/JS lines with 0x10 (DLE) exactly like Gadgetbridge; the watch
 *   strips it. Every line ends in '\n'.
 *
 *   setTime : "setTime(<utcEpochSeconds>);E.setTimeZone(<offsetHours>);\n"
 *             firmware reads the integer epoch and the (possibly fractional,
 *             possibly negative) timezone offset in HOURS.
 *   notify  : GB({"t":"notify","id":<int>,"src":<app>,"title":<title>,"body":<body>})
 *             firmware reads: id, src|app, title|sender, body.
 *   notify- : GB({"t":"notify-","id":<int>})            firmware reads: id.
 *   musicinfo : GB({"t":"musicinfo","artist":..,"album":..,"track":..})
 *   musicstate: GB({"t":"musicstate","state":"play"|"pause",...})  (reads "state")
 *   weather : GB({"t":"weather","temp":<KELVIN>,"hum":<int>,"txt":<desc>,"wind":<int>,"loc":<name>})
 *             firmware: celsius = (temp > 100) ? temp-273.15 : temp — so we send
 *             KELVIN (always > 100 for real temps) to hit the documented path.
 *
 *   Custom (firmware follow-up required to DISPLAY — see README):
 *   phoneBattery : GB({"t":"phoneBattery","n":<pct 0..100>,"chg":<bool>})
 *   calendar     : GB({"t":"calendar","title":<str>,"time":<utcEpochSeconds>,"loc":<str>})
 *
 * ── watch -> phone framing (what the firmware SENDS; bare JSON + "\r\n") ───────
 *   {"t":"music","n":"play|pause|next|previous|rewind|forward|volumeup|volumedown"}
 *     (rewind/forward = seek -/+ 30s, used for audiobooks/podcasts)
 *   {"t":"findPhone","n":<bool>}
 *   {"t":"notify","n":"DISMISS"|"REPLY","id":<int>[,"msg":<str>]}
 *   {"t":"camera","n":"open"|"capture"}  (also legacy: "shutter"|"video"|"mode")
 *   We RECEIVE these on the TX characteristic and act on them.
 */
object GbProtocol {

    private const val DLE = "\u0010" // 0x10 (DLE) — the control byte Gadgetbridge prefixes; the watch strips it

    // ---- phone -> watch : raw JS ---------------------------------------------

    /** setTime line. epoch is UTC seconds; offset is LOCAL offset in HOURS. */
    fun setTimeLine(epochSeconds: Long = System.currentTimeMillis() / 1000L): String {
        val tz = TimeZone.getDefault()
        val offsetHours = tz.getOffset(System.currentTimeMillis()) / 3600000.0
        // Format offset without a trailing ".0" when whole, matching Espruino style.
        val offStr = if (offsetHours == offsetHours.toLong().toDouble())
            offsetHours.toLong().toString() else offsetHours.toString()
        return DLE + "setTime(" + epochSeconds + ");E.setTimeZone(" + offStr + ");\n"
    }

    // ---- phone -> watch : GB(...) JSON ---------------------------------------

    private fun gbLine(obj: JSONObject): String = DLE + "GB(" + obj.toString() + ")\n"

    fun notifyLine(id: Int, app: String, title: String, body: String): String {
        val o = JSONObject()
        o.put("t", "notify")
        o.put("id", id)
        o.put("src", app)
        o.put("title", title)
        o.put("body", body)
        return gbLine(o)
    }

    fun notifyRemoveLine(id: Int): String {
        val o = JSONObject()
        o.put("t", "notify-")
        o.put("id", id)
        return gbLine(o)
    }

    /**
     * musicinfo. [audiobook] is true when the active media app is an audiobook/
     * podcast player (e.g. Audible); the watch uses it to show rewind/forward
     * (skip 30s) transport controls instead of the music prev/next controls.
     */
    fun musicInfoLine(
        artist: String,
        album: String,
        track: String,
        audiobook: Boolean = false
    ): String {
        val o = JSONObject()
        o.put("t", "musicinfo")
        o.put("artist", artist)
        o.put("album", album)
        o.put("track", track)
        o.put("ab", audiobook)
        return gbLine(o)
    }

    /** state must be "play" or "pause" (firmware treats anything != "play" as paused). */
    fun musicStateLine(playing: Boolean, positionSec: Int = 0): String {
        val o = JSONObject()
        o.put("t", "musicstate")
        o.put("state", if (playing) "play" else "pause")
        o.put("position", positionSec)
        return gbLine(o)
    }

    /** Weather. Temperature is sent in KELVIN to match the firmware's converter. */
    fun weatherLine(
        tempCelsius: Double,
        humidityPct: Int,
        description: String,
        windKmh: Int,
        location: String,
        aqi: Int = -1
    ): String {
        val o = JSONObject()
        o.put("t", "weather")
        o.put("temp", (tempCelsius + 273.15))   // KELVIN
        o.put("hum", humidityPct)
        o.put("txt", description)
        o.put("wind", windKmh)
        o.put("loc", location)
        o.put("aqi", aqi)                        // US AQI (-1 = n/a)
        return gbLine(o)
    }

    // ---- custom types (need a small firmware follow-up to render) ------------

    fun phoneBatteryLine(pct: Int, charging: Boolean): String {
        val o = JSONObject()
        o.put("t", "phoneBattery")
        o.put("n", pct.coerceIn(0, 100))
        o.put("chg", charging)
        return gbLine(o)
    }

    fun calendarLine(title: String, startEpochSeconds: Long, location: String): String {
        val o = JSONObject()
        o.put("t", "calendar")
        o.put("title", title)
        o.put("time", startEpochSeconds)
        o.put("loc", location)
        return gbLine(o)
    }

    /**
     * Camera state report so the watch page can reflect the real phone state.
     * mode is "photo" or "video"; rec is whether a video recording is in progress.
     */
    fun cameraStateLine(open: Boolean, videoMode: Boolean, recording: Boolean): String {
        val o = JSONObject()
        o.put("t", "cameraState")
        o.put("open", open)
        o.put("mode", if (videoMode) "video" else "photo")
        o.put("rec", recording)
        return gbLine(o)
    }

    /**
     * Sleep-timer state so the watch can show a live countdown. active=false means
     * the timer is idle/cancelled/fired; remaining is whole seconds left.
     */
    fun sleepStateLine(active: Boolean, remainingSeconds: Int): String {
        val o = JSONObject()
        o.put("t", "sleepState")
        o.put("active", active)
        o.put("remaining", remainingSeconds.coerceAtLeast(0))
        return gbLine(o)
    }

    // ---- watch -> phone : parse inbound bare-JSON lines ----------------------

    data class Inbound(val type: String, val json: JSONObject)

    /** Parse one assembled line from the watch. Returns null if not valid JSON. */
    fun parseInbound(line: String): Inbound? {
        val trimmed = line.trim()
        if (trimmed.isEmpty() || trimmed[0] != '{') return null
        return try {
            val o = JSONObject(trimmed)
            Inbound(o.optString("t", ""), o)
        } catch (e: Exception) {
            null
        }
    }
}
