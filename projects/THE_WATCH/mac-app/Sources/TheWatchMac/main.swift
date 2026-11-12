import AppKit

/// Entry point. Menu-bar-only app (LSUIElement / .accessory activation policy).
///
/// Optional args / env:
///   THEWATCH_BRIDGE_PORT=<port>   override the default 8765 HTTP bridge port.
final class AppDelegate: NSObject, NSApplicationDelegate {
    private var appState: AppState!
    private var statusBar: StatusBarController!

    func applicationDidFinishLaunching(_ notification: Notification) {
        let port = ProcessInfo.processInfo.environment["THEWATCH_BRIDGE_PORT"]
            .flatMap { UInt16($0) } ?? 8765
        appState = AppState(bridgePort: port)
        statusBar = StatusBarController(appState: appState)
        appState.start()
    }
}

let app = NSApplication.shared
app.setActivationPolicy(.accessory)   // no Dock icon, no menu bar app menu
let delegate = AppDelegate()
app.delegate = delegate
app.run()
