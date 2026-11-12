import AppKit

/// Menu-bar (NSStatusItem) UI. Shows connection state, a live list of PENDING
/// permission requests (each with Approve / Deny), and a small menu.
final class StatusBarController {

    private let statusItem: NSStatusItem
    private let appState: AppState

    private let menu = NSMenu()
    private let connMenuItem = NSMenuItem(title: "Disconnected", action: nil, keyEquivalent: "")

    // Section for pending permission requests, rebuilt live. We keep markers so we
    // can splice the pending items in/out without rebuilding the whole menu.
    private let pendingHeader = NSMenuItem(title: "No pending approvals", action: nil, keyEquivalent: "")
    private let pendingSeparator = NSMenuItem.separator()
    private var pendingItems: [NSMenuItem] = []

    // "Mute watch sounds" checkable toggle.
    private let muteItem = NSMenuItem(title: "Mute watch sounds",
                                      action: #selector(toggleMute), keyEquivalent: "")

    init(appState: AppState) {
        self.appState = appState
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)

        if let button = statusItem.button {
            button.image = NSImage(systemSymbolName: "applewatch", accessibilityDescription: "The Watch")
            button.image?.isTemplate = true
        }

        connMenuItem.isEnabled = false
        menu.addItem(connMenuItem)
        menu.addItem(NSMenuItem.separator())

        // Pending-approvals section (header + dynamic rows, then a separator).
        pendingHeader.isEnabled = false
        menu.addItem(pendingHeader)
        menu.addItem(pendingSeparator)

        // Mute watch sounds toggle (reflects/persists via AppState).
        muteItem.target = self
        muteItem.state = appState.isSoundMuted ? .on : .off
        menu.addItem(muteItem)
        menu.addItem(NSMenuItem.separator())

        menu.addItem(NSMenuItem(title: "Bridge: 127.0.0.1:\(appState.bridge.port)",
                                action: nil, keyEquivalent: ""))
        menu.addItem(NSMenuItem.separator())
        let quit = NSMenuItem(title: "Quit The Watch", action: #selector(quit), keyEquivalent: "q")
        quit.target = self
        menu.addItem(quit)
        statusItem.menu = menu

        appState.onStatusChange = { [weak self] connected, detail in
            DispatchQueue.main.async { self?.render(connected: connected, detail: detail) }
        }
        appState.onPendingChange = { [weak self] items in
            DispatchQueue.main.async { self?.renderPending(items) }
        }
        renderPending(appState.pendingSnapshot())
    }

    private func render(connected: Bool, detail: String) {
        connMenuItem.title = detail
        if let button = statusItem.button {
            let symbol = connected ? "applewatch" : "applewatch.slash"
            button.image = NSImage(systemSymbolName: symbol, accessibilityDescription: "The Watch")
            button.image?.isTemplate = true
        }
    }

    // MARK: - Pending permission requests

    private func renderPending(_ items: [AppState.PendingItem]) {
        // Remove previously-inserted pending rows.
        for item in pendingItems { menu.removeItem(item) }
        pendingItems.removeAll()

        pendingHeader.title = items.isEmpty ? "No pending approvals"
                                            : "Pending approvals (\(items.count))"

        // Insert each request as a submenu with Approve / Deny, right after the header.
        var insertIndex = menu.index(of: pendingHeader) + 1
        for req in items {
            let row = NSMenuItem(title: label(for: req), action: nil, keyEquivalent: "")
            let sub = NSMenu()

            let approve = NSMenuItem(title: "Approve", action: #selector(approveTapped(_:)), keyEquivalent: "")
            approve.target = self
            approve.representedObject = req.id
            sub.addItem(approve)

            let deny = NSMenuItem(title: "Deny", action: #selector(denyTapped(_:)), keyEquivalent: "")
            deny.target = self
            deny.representedObject = req.id
            sub.addItem(deny)

            row.submenu = sub
            menu.insertItem(row, at: insertIndex)
            insertIndex += 1
            pendingItems.append(row)
        }
        // Update the badge on the status button so pending requests are noticeable.
        if let button = statusItem.button {
            button.title = items.isEmpty ? "" : " \(items.count)"
        }
        // Show the pending indicator on the menu-bar glyph too.
        statusItem.button?.appearsDisabled = false
    }

    /// e.g. "Approve tool use? — project (session ab12)".
    private func label(for req: AppState.PendingItem) -> String {
        let title = req.title.isEmpty ? "Approve?" : req.title
        return "\(title) — \(req.origin)"
    }

    @objc private func approveTapped(_ sender: NSMenuItem) {
        guard let id = sender.representedObject as? Int32 else { return }
        appState.resolveFromMenu(id: id, approve: true)
    }

    @objc private func denyTapped(_ sender: NSMenuItem) {
        guard let id = sender.representedObject as? Int32 else { return }
        appState.resolveFromMenu(id: id, approve: false)
    }

    @objc private func toggleMute() {
        let nowMuted = !appState.isSoundMuted
        appState.setSoundMuted(nowMuted)
        muteItem.state = nowMuted ? .on : .off
    }

    @objc private func quit() {
        NSApp.terminate(nil)
    }
}
