import Foundation

/// Builders + parsers for the "Bangle.js / Gadgetbridge" wire protocol, extended
/// per PROTOCOL_V2.md (multi-source: this app is `src:"mac"`).
///
/// Written byte-for-byte against:
///   - firmware/the_watch/src/ble/ble_gadgetbridge.cpp (RX parsing / framing)
///   - companion-app .../ble/GbProtocol.kt (the Android reference)
///   - PROTOCOL_V2.md (the source of truth)
///
/// central -> watch framing: `\x10GB({...json...})\n`  (0x10 DLE prefix, watch strips it)
/// watch -> central framing: bare `{...json...}` + `\r\n` (we split on '\n', drop '\r')
enum GbProtocol {

    /// 0x10 (DLE) — the control byte Gadgetbridge prefixes; the watch strips any
    /// leading byte <= 0x20 that isn't '{'.
    static let dle: UInt8 = 0x10

    // MARK: - JSON helpers

    /// Compact JSON encode. Key order is irrelevant to the firmware (ArduinoJson
    /// parses by key), so `.sortedKeys` just makes the wire output deterministic.
    private static func jsonString(_ obj: [String: Any]) -> String {
        guard let data = try? JSONSerialization.data(withJSONObject: obj, options: [.sortedKeys]),
              let s = String(data: data, encoding: .utf8) else {
            return "{}"
        }
        return s
    }

    /// Frame a JSON object as a `\x10GB({...})\n` line (raw bytes ready to write).
    static func gbLine(_ obj: [String: Any]) -> Data {
        var data = Data([dle])
        data.append("GB(".data(using: .utf8)!)
        data.append(jsonString(obj).data(using: .utf8)!)
        data.append(")\n".data(using: .utf8)!)
        return data
    }

    /// Raw `setTime(...)` / `E.setTimeZone(...)` line (Espruino REPL JS). epoch is
    /// UTC seconds; offset is the LOCAL offset in HOURS (may be negative/fractional).
    static func setTimeLine(date: Date = Date()) -> Data {
        let epoch = Int(date.timeIntervalSince1970)
        let offsetHours = Double(TimeZone.current.secondsFromGMT(for: date)) / 3600.0
        let offStr: String = (offsetHours == offsetHours.rounded())
            ? String(Int(offsetHours))
            : String(offsetHours)
        let js = "setTime(\(epoch));E.setTimeZone(\(offStr));\n"
        var data = Data([dle])
        data.append(js.data(using: .utf8)!)
        return data
    }

    // MARK: - central -> watch builders (all carry src:"mac")

    static func helloLine(name: String) -> Data {
        gbLine(["t": "hello", "src": "mac", "name": name])
    }

    /// devBattery (generalizes phoneBattery). n in 0..100.
    static func devBatteryLine(percent: Int, charging: Bool) -> Data {
        gbLine(["t": "devBattery", "src": "mac",
                "n": max(0, min(100, percent)), "chg": charging])
    }

    static func musicInfoLine(artist: String, album: String, track: String,
                              audiobook: Bool) -> Data {
        gbLine(["t": "musicinfo", "src": "mac",
                "artist": artist, "album": album, "track": track, "ab": audiobook])
    }

    /// state is "play" or "pause" (firmware treats anything != "play" as paused).
    static func musicStateLine(playing: Bool, positionSec: Int) -> Data {
        gbLine(["t": "musicstate", "src": "mac",
                "state": playing ? "play" : "pause", "position": positionSec])
    }

    /// permReq — the remote-approval "super key". `id` is a sender-chosen 32-bit int
    /// the watch echoes back in its permResp.
    static func permReqLine(id: Int32, title: String, body: String, origin: String) -> Data {
        gbLine(["t": "permReq", "id": Int(id),
                "title": title, "body": body, "origin": origin])
    }

    /// permCancel — dismiss a still-showing/queued permission card when its request
    /// was resolved by any means OTHER than a tap on that same watch card (menu-bar
    /// approve/deny, HTTP timeout, or requesting client disconnect). Mirrors
    /// PROTOCOL_V2.md. NEVER send this for an id the watch itself already answered.
    static func permCancelLine(id: Int32) -> Data {
        gbLine(["t": "permCancel", "id": Int(id)])
    }

    /// weather — current conditions. Temperature is sent in KELVIN (celsius+273.15)
    /// to match the firmware's converter. Mirrors the Android GbProtocol.weatherLine,
    /// including the US-AQI `aqi` field (-1 = unavailable). See PROTOCOL_V2.md.
    static func weatherLine(tempCelsius: Double, humidityPct: Int, description: String,
                            windKmh: Int, location: String, aqi: Int) -> Data {
        gbLine(["t": "weather",
                "temp": tempCelsius + 273.15,   // KELVIN
                "hum": humidityPct,
                "txt": description,
                "wind": windKmh,
                "loc": location,
                "aqi": aqi])
    }

    /// sound — mute / unmute the watch buzzer (chimes). `on:false` mutes,
    /// `on:true` unmutes. Same `\x10GB({...})\n` framing as other phone→watch msgs.
    static func soundLine(on: Bool) -> Data {
        gbLine(["t": "sound", "on": on])
    }

    // MARK: - watch -> central parsing

    struct Inbound {
        let type: String
        let json: [String: Any]
    }

    /// Parse one assembled line from the watch. Returns nil if not valid JSON.
    static func parseInbound(_ line: String) -> Inbound? {
        let trimmed = line.trimmingCharacters(in: .whitespacesAndNewlines)
        guard trimmed.first == "{",
              let data = trimmed.data(using: .utf8),
              let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any]
        else { return nil }
        let t = (obj["t"] as? String) ?? ""
        return Inbound(type: t, json: obj)
    }
}
