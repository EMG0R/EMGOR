import Foundation

/// Fetches real current weather from the FREE Open-Meteo APIs (no key) and turns
/// it into a `weather` line for the watch. Mirrors the Android `WeatherRepository`.
///
/// ── Location strategy (documented choice) ────────────────────────────────────
/// We deliberately use **IP geolocation** (https://ipapi.co/json/) rather than
/// CoreLocation. CLLocationManager on macOS would pop a Location-Services TCC
/// prompt AND require an `NSLocationUsageDescription` in Info.plist; for a
/// menu-bar utility that only needs a coarse city for weather, that prompt is not
/// worth it. IP-geo gives lat/lon + city over HTTPS with zero prompts and no new
/// Info.plist keys. (If precise location is ever wanted, a CoreLocation path can
/// be added behind an already-granted `authorizedAlways` check.)
///
/// All endpoints are HTTPS, so no App Transport Security exception is required.
final class WeatherController {

    struct Conditions: Equatable {
        var tempC: Double
        var humidity: Int
        var windKmh: Int
        var description: String
        var locationName: String
        var aqi: Int            // US AQI (-1 = unavailable)
    }

    /// Last successfully-fetched conditions (for /health). Read/written on `queue`.
    private(set) var last: Conditions?
    private let lock = NSLock()

    private let session: URLSession = {
        let cfg = URLSessionConfiguration.ephemeral
        cfg.timeoutIntervalForRequest = 12
        cfg.waitsForConnectivity = false
        return URLSession(configuration: cfg)
    }()

    /// A snapshot of `last`, thread-safe.
    func snapshot() -> Conditions? {
        lock.lock(); defer { lock.unlock() }
        return last
    }

    /// Fetch conditions and, on success, hand a framed `weather` line to `send`.
    /// Fully async (URLSession completion chain); `done` reports success on the
    /// main queue. Safe to call from a Timer on the main thread.
    func fetchAndSend(send: @escaping (Data) -> Void, done: ((Bool) -> Void)? = nil) {
        resolveLocation { [weak self] loc in
            guard let self, let loc else {
                DispatchQueue.main.async { done?(false) }
                return
            }
            self.fetchForecast(lat: loc.lat, lon: loc.lon) { forecast in
                guard var cond = forecast else {
                    DispatchQueue.main.async { done?(false) }
                    return
                }
                cond.locationName = loc.name
                // AQI is best-effort; send weather regardless of AQI success.
                self.fetchAqi(lat: loc.lat, lon: loc.lon) { aqi in
                    cond.aqi = aqi
                    self.lock.lock(); self.last = cond; self.lock.unlock()
                    let line = GbProtocol.weatherLine(
                        tempCelsius: cond.tempC,
                        humidityPct: cond.humidity,
                        description: cond.description,
                        windKmh: cond.windKmh,
                        location: cond.locationName,
                        aqi: cond.aqi)
                    send(line)
                    NSLog("[weather] sent %.1f°C %d%% %@ AQI=%d @ %@",
                          cond.tempC, cond.humidity, cond.description, cond.aqi, cond.locationName)
                    DispatchQueue.main.async { done?(true) }
                }
            }
        }
    }

    // MARK: - Location (IP geolocation)

    private struct Loc { let lat: Double; let lon: Double; let name: String }

    private func resolveLocation(_ completion: @escaping (Loc?) -> Void) {
        guard let url = URL(string: "https://ipapi.co/json/") else { completion(nil); return }
        var req = URLRequest(url: url)
        req.setValue("TheWatch-Mac/1.0", forHTTPHeaderField: "User-Agent")
        session.dataTask(with: req) { data, resp, _ in
            guard let data, (resp as? HTTPURLResponse)?.statusCode == 200,
                  let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
                  let lat = (obj["latitude"] as? NSNumber)?.doubleValue,
                  let lon = (obj["longitude"] as? NSNumber)?.doubleValue
            else { completion(nil); return }
            let city = (obj["city"] as? String)
            let region = (obj["region"] as? String)
            let name = [city, region].compactMap { $0?.isEmpty == false ? $0 : nil }.first
                ?? String(format: "%.2f,%.2f", lat, lon)
            completion(Loc(lat: lat, lon: lon, name: name))
        }.resume()
    }

    // MARK: - Open-Meteo forecast

    private func fetchForecast(lat: Double, lon: Double,
                               completion: @escaping (Conditions?) -> Void) {
        let str = "https://api.open-meteo.com/v1/forecast" +
            "?latitude=\(lat)&longitude=\(lon)" +
            "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m" +
            "&temperature_unit=celsius&wind_speed_unit=kmh&timezone=auto"
        guard let url = URL(string: str) else { completion(nil); return }
        session.dataTask(with: url) { data, resp, _ in
            guard let data, (resp as? HTTPURLResponse)?.statusCode == 200,
                  let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
                  let cur = obj["current"] as? [String: Any]
            else { completion(nil); return }
            let temp = (cur["temperature_2m"] as? NSNumber)?.doubleValue ?? 0
            let hum = (cur["relative_humidity_2m"] as? NSNumber)?.doubleValue ?? 0
            let wind = (cur["wind_speed_10m"] as? NSNumber)?.doubleValue ?? 0
            let code = (cur["weather_code"] as? NSNumber)?.intValue ?? -1
            completion(Conditions(tempC: temp,
                                  humidity: Int(hum.rounded()),
                                  windKmh: Int(wind.rounded()),
                                  description: Self.wmoText(code),
                                  locationName: "",     // filled in by caller
                                  aqi: -1))
        }.resume()
    }

    private func fetchAqi(lat: Double, lon: Double, completion: @escaping (Int) -> Void) {
        let str = "https://air-quality-api.open-meteo.com/v1/air-quality" +
            "?latitude=\(lat)&longitude=\(lon)&current=us_aqi&timezone=auto"
        guard let url = URL(string: str) else { completion(-1); return }
        session.dataTask(with: url) { data, resp, _ in
            guard let data, (resp as? HTTPURLResponse)?.statusCode == 200,
                  let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
                  let cur = obj["current"] as? [String: Any],
                  let v = (cur["us_aqi"] as? NSNumber)?.doubleValue
            else { completion(-1); return }
            completion(Int(v.rounded()))
        }.resume()
    }

    // MARK: - WMO code -> short text (mirrors Android wmoText)

    static func wmoText(_ code: Int) -> String {
        switch code {
        case 0: return "Clear"
        case 1: return "Mainly clear"
        case 2: return "Partly cloudy"
        case 3: return "Overcast"
        case 45, 48: return "Fog"
        case 51, 53, 55: return "Drizzle"
        case 56, 57: return "Freezing drizzle"
        case 61, 63, 65: return "Rain"
        case 66, 67: return "Freezing rain"
        case 71, 73, 75: return "Snow"
        case 77: return "Snow grains"
        case 80, 81, 82: return "Showers"
        case 85, 86: return "Snow showers"
        case 95: return "Thunderstorm"
        case 96, 99: return "Thunderstorm hail"
        default: return "Unknown"
        }
    }
}
