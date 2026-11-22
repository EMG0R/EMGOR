import Foundation
import IOKit.ps

/// Reads the internal battery via IOKit power sources
/// (IOPSCopyPowerSourcesInfo / IOPSGetPowerSourceDescription).
enum BatteryMonitor {

    struct State: Equatable {
        let percent: Int      // 0..100, -1 if unknown (e.g. a desktop Mac)
        let charging: Bool
    }

    /// Snapshot current internal battery. Returns percent = -1 on Macs with no
    /// battery (desktops) so callers can choose to skip sending.
    static func read() -> State {
        guard let blob = IOPSCopyPowerSourcesInfo()?.takeRetainedValue(),
              let sources = IOPSCopyPowerSourcesList(blob)?.takeRetainedValue() as? [CFTypeRef]
        else {
            return State(percent: -1, charging: false)
        }

        for source in sources {
            guard let desc = IOPSGetPowerSourceDescription(blob, source)?.takeUnretainedValue()
                    as? [String: Any] else { continue }

            // Only the internal battery, not UPS etc.
            let type = desc[kIOPSTypeKey] as? String
            guard type == kIOPSInternalBatteryType else { continue }

            let cur = desc[kIOPSCurrentCapacityKey] as? Int ?? 0
            let mx  = desc[kIOPSMaxCapacityKey] as? Int ?? 100
            let pct = mx > 0 ? Int((Double(cur) / Double(mx) * 100.0).rounded()) : cur

            let isCharging = (desc[kIOPSIsChargingKey] as? Bool) ?? false
            let state = desc[kIOPSPowerSourceStateKey] as? String
            let onAC = (state == kIOPSACPowerValue)
            // "charging" = actively charging, OR plugged in and already full.
            let charging = isCharging || (onAC && pct >= 100)

            return State(percent: max(0, min(100, pct)), charging: charging)
        }
        return State(percent: -1, charging: false)
    }
}
