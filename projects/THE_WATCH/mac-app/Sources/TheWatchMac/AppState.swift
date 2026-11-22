import Foundation
import AppKit

/// Central coordinator: wires BLE <-> battery <-> media <-> permission bridge.
/// Owns all subsystems and implements the two delegate/responder protocols.
final class AppState: NSObject, WatchBLEDelegate, PermissionResponder {

    let ble = WatchBLEManager()
    let bridge: PermissionBridge
    let media = MediaController()
    let weather = WeatherController()

    // UI hooks (StatusBarController subscribes).
    var onStatusChange: ((_ connected: Bool, _ detail: String) -> Void)?
    /// Fired whenever the set of PENDING permission requests changes (delivered on
    /// the main queue). StatusBarController rebuilds its Approve/Deny section from it.
    var onPendingChange: ((_ items: [PendingItem]) -> Void)?

    /// Lightweight snapshot of a pending request for the menu bar.
    struct PendingItem {
        let id: Int32
        let title: String
        let origin: String
    }

    private(set) var connected = false
    private(set) var statusDetail = "Starting…"

    // Battery + media + weather heartbeats.
    private var batteryTimer: Timer?
    private var mediaTimer: Timer?
    private var weatherTimer: Timer?
    private var lastBattery: BatteryMonitor.State?
    private var lastNowPlaying: MediaController.NowPlaying?
    private let mediaQueue = DispatchQueue(label: "com.emgor.thewatch.media")

    /// One pending permission round-trip. Shared source of truth for BOTH the
    /// watch-response path and the menu-bar approve/deny path (and cancel/timeout).
    private final class PendingRequest {
        let id: Int32
        let title: String
        let body: String
        let origin: String
        let sem = DispatchSemaphore(value: 0)
        var result: PermResult?
        var resolved = false
        init(id: Int32, title: String, body: String, origin: String) {
            self.id = id; self.title = title; self.body = body; self.origin = origin
        }
    }

    // Pending permission round-trips, keyed by permReq id. Guarded by pendingLock —
    // the single registry through which the watch, menu, timeout, and disconnect
    // paths all resolve, so responses never cross-wire between concurrent requests.
    private let pendingLock = NSLock()
    private var pending: [Int32: PendingRequest] = [:]

    private let deviceName: String

    // MARK: - Watch sound (chime) mute

    /// UserDefaults key persisting the user's "Mute watch sounds" choice.
    private static let soundMutedKey = "watchSoundMuted"

    /// True = watch buzzer muted (we send sound on:false). Default = unmuted.
    /// Persisted so a watch reboot/reconnect can't silently un-mute the user.
    private(set) var isSoundMuted: Bool = UserDefaults.standard.bool(forKey: AppState.soundMutedKey)

    init(bridgePort: UInt16 = 8765) {
        self.bridge = PermissionBridge(port: bridgePort)
        self.deviceName = Host.current().localizedName ?? "Mac"
        super.init()
        ble.delegate = self
        bridge.responder = self
    }

    func start() {
        bridge.start()
        ble.start()
        // Battery heartbeat every 60s (plus change-driven sends on connect/poll).
        batteryTimer = Timer.scheduledTimer(withTimeInterval: 60, repeats: true) { [weak self] _ in
            self?.pollBattery(force: true)
        }
        // Media poll every 4s (AppleScript is cheap when the player is idle).
        mediaTimer = Timer.scheduledTimer(withTimeInterval: 4, repeats: true) { [weak self] _ in
            self?.pollMedia(force: false)
        }
        // Weather heartbeat every 20 min. The Mac sends weather as a FALLBACK:
        // the firmware prefers the PHONE's weather while it is fresh (<25 min)
        // and only falls back to the Mac's when the phone's has gone stale. The
        // `src:"mac"` tag (carried via the hello line) lets the firmware tell the
        // two sources apart, so it is always safe for the Mac to send.
        weatherTimer = Timer.scheduledTimer(withTimeInterval: 20 * 60, repeats: true) { [weak self] _ in
            self?.refreshWeather()
        }
    }

    // MARK: - WatchBLEDelegate

    func bleDidChangeConnection(_ connected: Bool) {
        self.connected = connected
        if connected {
            // On EVERY (re)connect: announce ourselves, sync time, re-assert the
            // user's mute choice, then push current state. The watch loses all of
            // this across a reboot/reflash, so it must be re-sent each time the
            // NUS characteristics come ready — not only on the first connect.
            NSLog("[ble] connected — re-sending hello/setTime/sound + mac battery/media")
            ble.send(GbProtocol.helloLine(name: deviceName))
            ble.send(GbProtocol.setTimeLine())
            assertSoundState()          // re-mute if the user chose to
            pollBattery(force: true)
            pollMedia(force: true)
            // Send weather as a FALLBACK (phone-preferred in firmware). Retry at
            // ~3s/~10s so a first send that races GATT/MTU negotiation still lands.
            sendWeatherWithRetries()
        } else {
            // Watch dropped: fail every in-flight permission request fast so the
            // Claude hook falls back to the terminal prompt instead of blocking
            // for the full ~60s timeout while the watch is gone.
            failAllPending()
        }
        emitStatus(connected ? "Connected" : "Disconnected")
    }

    /// Fetch + send weather now, and again after ~3s and ~10s to survive a first
    /// send that races the just-established GATT link / MTU negotiation.
    private func sendWeatherWithRetries() {
        refreshWeather()
        for delay in [3.0, 10.0] {
            DispatchQueue.main.asyncAfter(deadline: .now() + delay) { [weak self] in
                guard let self, self.connected else { return }
                NSLog("[weather] retry send (+%.0fs after connect)", delay)
                self.refreshWeather()
            }
        }
    }

    // MARK: - Watch sound (chime) mute

    /// Toggle/set the watch mute state. Persists the choice and, if connected,
    /// immediately tells the watch. Called from the menu bar.
    /// muted == true  → send sound on:false (silence the buzzer)
    /// muted == false → send sound on:true  (restore chimes)
    func setSoundMuted(_ muted: Bool) {
        isSoundMuted = muted
        UserDefaults.standard.set(muted, forKey: AppState.soundMutedKey)
        assertSoundState()
    }

    /// (Re)send the current mute state to the watch. Safe to call on every
    /// reconnect so a watch reboot can't leave chimes on after the user muted.
    private func assertSoundState() {
        guard connected else { return }
        NSLog("[sound] asserting watch sound on:%@ (muted=%@)",
              isSoundMuted ? "false" : "true", isSoundMuted ? "true" : "false")
        ble.send(GbProtocol.soundLine(on: !isSoundMuted))
    }

    func bleDidChangeState(_ description: String) {
        emitStatus(description)
    }

    func bleDidReceive(_ inbound: GbProtocol.Inbound) {
        switch inbound.type {
        case "music":
            if let n = inbound.json["n"] as? String {
                media.handleTransport(n)
            }
        case "permResp":
            handlePermResp(inbound.json)
        default:
            // findPhone / notify / camera route to the phone; ignored on Mac.
            break
        }
    }

    // MARK: - Battery

    private func pollBattery(force: Bool) {
        let state = BatteryMonitor.read()
        guard state.percent >= 0 else { return }   // desktop Mac: nothing to send
        if force || state != lastBattery {
            lastBattery = state
            if connected {
                ble.send(GbProtocol.devBatteryLine(percent: state.percent, charging: state.charging))
            }
        }
    }

    // MARK: - Media

    private func pollMedia(force: Bool) {
        mediaQueue.async { [weak self] in
            guard let self else { return }
            let np = self.media.snapshot()
            DispatchQueue.main.async {
                guard self.connected else { self.lastNowPlaying = np; return }
                let changed = (np != self.lastNowPlaying)
                self.lastNowPlaying = np
                guard let np else { return }   // nothing playing: leave last state on watch
                if force || changed {
                    self.ble.send(GbProtocol.musicInfoLine(artist: np.artist, album: np.album,
                                                           track: np.track, audiobook: np.audiobook))
                    self.ble.send(GbProtocol.musicStateLine(playing: np.playing,
                                                            positionSec: np.positionSec))
                }
            }
        }
    }

    // MARK: - Weather

    private func refreshWeather() {
        guard connected else { return }
        weather.fetchAndSend(send: { [weak self] line in
            self?.ble.send(line)
        })
    }

    // MARK: - PermissionResponder (called on the bridge's background queue)

    func requestPermission(title: String, body: String, origin: String,
                           onRegister: (Int32?) -> Void) -> PermResult {
        // Only proceed when the RX/TX characteristics are actually ready (live
        // GATT check, not the cached flag). If not ready, fail fast to timedOut
        // so the permReq is never silently queued into the outbox to expire.
        guard ble.isReady else {
            // Watch unreachable: never block forever — tell the caller to fall back.
            onRegister(nil)
            return PermResult(approve: false, scope: "once", timedOut: true)
        }
        // Unique id per request; retry the (astronomically unlikely) collision so
        // concurrent requests can never share a registry slot.
        pendingLock.lock()
        var id = Int32(truncatingIfNeeded: Int.random(in: 1...Int(Int32.max)))
        while pending[id] != nil {
            id = Int32(truncatingIfNeeded: Int.random(in: 1...Int(Int32.max)))
        }
        let req = PendingRequest(id: id, title: title, body: body, origin: origin)
        pending[id] = req
        pendingLock.unlock()
        emitPending()

        ble.send(GbProtocol.permReqLine(id: id, title: title, body: body, origin: origin))
        onRegister(id)   // hand the id to the bridge for client-disconnect cancellation

        let outcome = req.sem.wait(timeout: .now() + 60)

        if outcome == .timedOut {
            // Try to claim the timeout. If we win, the watch never answered → dismiss
            // its card. If someone else resolved first, use their result.
            if resolve(id: id, with: PermResult(approve: false, scope: "once", timedOut: true),
                       sendCancel: true) {
                return PermResult(approve: false, scope: "once", timedOut: true)
            }
        }
        return req.result ?? PermResult(approve: false, scope: "once", timedOut: true)
    }

    /// The one place a pending request transitions to resolved. Atomic + idempotent:
    /// the FIRST caller for a given id wins (removes it, signals its waiter), everyone
    /// else no-ops. Returns true iff THIS call performed the resolution.
    ///
    /// `sendCancel` controls whether the watch is told to drop the card:
    ///   - watch answered (permResp)  → false (that id is already gone on the watch)
    ///   - menu approve/deny / timeout / client disconnect → true
    @discardableResult
    private func resolve(id: Int32, with result: PermResult, sendCancel: Bool) -> Bool {
        pendingLock.lock()
        guard let req = pending[id], !req.resolved else {
            pendingLock.unlock(); return false
        }
        req.resolved = true
        req.result = result
        pending[id] = nil
        pendingLock.unlock()

        req.sem.signal()
        if sendCancel && connected {
            ble.send(GbProtocol.permCancelLine(id: id))
        }
        emitPending()
        return true
    }

    /// Resolve EVERY in-flight request as timedOut. Called when the watch
    /// disconnects so blocked /permission callers return immediately (the hook
    /// then falls back to the terminal prompt) instead of hanging ~60s. No
    /// permCancel is sent — the link is already down. Idempotent via resolve().
    private func failAllPending() {
        pendingLock.lock()
        let ids = Array(pending.keys)
        pendingLock.unlock()
        for id in ids {
            resolve(id: id, with: PermResult(approve: false, scope: "once", timedOut: true),
                    sendCancel: false)
        }
    }

    /// Called by the bridge when the requesting HTTP client disconnects mid-wait.
    func cancelPermission(id: Int32) {
        resolve(id: id, with: PermResult(approve: false, scope: "once", timedOut: true),
                sendCancel: true)
    }

    /// Called from the menu bar. Resolves the HTTP request AND dismisses the watch card.
    func resolveFromMenu(id: Int32, approve: Bool) {
        resolve(id: id, with: PermResult(approve: approve, scope: "once", timedOut: false),
                sendCancel: true)
    }

    /// Snapshot of currently-pending requests for the menu bar.
    func pendingSnapshot() -> [PendingItem] {
        pendingLock.lock(); defer { pendingLock.unlock() }
        return pending.values
            .sorted { $0.id < $1.id }
            .map { PendingItem(id: $0.id, title: $0.title, origin: $0.origin) }
    }

    private func emitPending() {
        let items = pendingSnapshot()
        DispatchQueue.main.async { [weak self] in self?.onPendingChange?(items) }
    }

    func healthStatus() -> [String: Any] {
        // Report the LIVE connection state (RX/TX ready), not the cached flag that
        // is updated asynchronously on the main queue. Then force the human status
        // string to agree with it, so /health can never say "Connected" while
        // `connected` is false (or vice-versa).
        let live = ble.isReady
        var statusStr = statusDetail
        if live && statusStr != "Connected" { statusStr = "Connected" }
        if !live && statusStr == "Connected" { statusStr = "Disconnected" }
        var status: [String: Any] = [
            "connected": live,
            "status": statusStr,
            "device": deviceName,
            "bridgePort": Int(bridge.port),
            "pendingPermissions": pendingSnapshot().count,
            "battery": lastBattery.map { ["percent": $0.percent, "charging": $0.charging] } as Any,
            "nowPlaying": lastNowPlaying.map {
                ["artist": $0.artist, "track": $0.track, "playing": $0.playing]
            } as Any
        ]
        if let w = weather.snapshot() {
            status["weather"] = [
                "tempC": w.tempC, "humidity": w.humidity, "windKmh": w.windKmh,
                "description": w.description, "location": w.locationName, "aqi": w.aqi
            ]
        }
        return status
    }

    private func handlePermResp(_ json: [String: Any]) {
        guard let idNum = json["id"] as? NSNumber else { return }
        let id = idNum.int32Value
        let approve = (json["approve"] as? Bool) ?? false
        let scope = (json["scope"] as? String) ?? "once"
        // Watch answered: resolve WITHOUT permCancel (that id is already gone there),
        // and it drops out of the menu list via emitPending().
        resolve(id: id, with: PermResult(approve: approve, scope: scope, timedOut: false),
                sendCancel: false)
    }

    // MARK: - status

    private func emitStatus(_ detail: String) {
        statusDetail = detail
        onStatusChange?(connected, detail)
    }
}
