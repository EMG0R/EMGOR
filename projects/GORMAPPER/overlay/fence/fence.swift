// GORMAPPER cursor fence + window clamp.
//
// While the cursor is on the projector display, it is kept inside the
// screen-zone rect UNION an escape corridor that runs from the zone to the
// display edge facing the primary display — free movement in the zone,
// always a way home to the main screen, never into the decorations.
// Real macOS windows on the projector display are clamped into the zone via
// the Accessibility API. Bare M (cursor on the projector display) instantly
// disables everything; Cmd+Alt+F in the app re-arms.
//
//   swiftc -O fence.swift -o fence          (or: sh build.sh)
//   ./fence --print-mouse                   coordinate test mode: prints CG
//                                           display bounds + cursor position
//                                           and which display it is on
//
// stdin protocol (JSON lines, CG global points — origin top-left of primary,
// y down; verified identical to Electron's display.bounds on macOS):
//   {"display":{x,y,w,h},"rect":{x,y,w,h},"corridor":{x,y,w,h}|null}
//   {"enabled":false}
// Exits 0 on stdin EOF. Exits 2 + FENCE_NO_PERMISSION if the tap can't be
// created. Prints FENCE_PANIC_OFF when M disables the fence (the app watches
// for it and stays disarmed until an explicit re-toggle).

import Foundation
import CoreGraphics
import AppKit
import ApplicationServices

func elog(_ s: String) {
    FileHandle.standardError.write((s + "\n").data(using: .utf8)!)
}

// ---- shared state -----------------------------------------------------------

final class FenceState {
    private let lock = NSLock()
    private var enabled = false
    private var display = CGRect.zero
    private var rect = CGRect.zero
    private var corridor: CGRect? = nil

    func set(display: CGRect, rect: CGRect, corridor: CGRect?) {
        lock.lock(); defer { lock.unlock() }
        self.display = display
        self.rect = rect
        self.corridor = corridor
        self.enabled = rect.width > 4 && rect.height > 4
    }

    func disable() {
        lock.lock(); defer { lock.unlock() }
        enabled = false
    }

    func get() -> (Bool, CGRect, CGRect, CGRect?) {
        lock.lock(); defer { lock.unlock() }
        return (enabled, display, rect, corridor)
    }
}

let state = FenceState()
var tapPort: CFMachPort? = nil
var panicKeyAvailable = false

// ---- coordinate test mode ---------------------------------------------------

func displayList() -> [(CGDirectDisplayID, CGRect, Bool)] {
    var count: UInt32 = 0
    var ids = [CGDirectDisplayID](repeating: 0, count: 16)
    CGGetActiveDisplayList(16, &ids, &count)
    return (0..<Int(count)).map { (ids[$0], CGDisplayBounds(ids[$0]), CGDisplayIsMain(ids[$0]) != 0) }
}

if CommandLine.arguments.contains("--print-mouse") {
    setvbuf(stdout, nil, _IONBF, 0)   // unbuffered so piped output arrives live
    print("fence --print-mouse: CG global coordinates (origin top-left of primary, y down)")
    for (id, b, main) in displayList() {
        print("display \(id): x=\(b.minX) y=\(b.minY) w=\(b.width) h=\(b.height)"
            + " pixelsWide=\(CGDisplayPixelsWide(id))" + (main ? " [main]" : ""))
    }
    let t = Timer(timeInterval: 0.25, repeats: true) { _ in
        guard let e = CGEvent(source: nil) else { return }
        let l = e.location
        let on = displayList().first { $0.1.contains(l) }
        let name = on.map { $0.2 ? "PRIMARY" : "display \($0.0)" } ?? "nowhere?"
        print(String(format: "mouse: (%7.1f, %7.1f)  on %@", l.x, l.y, name))
    }
    RunLoop.current.add(t, forMode: .common)
    RunLoop.current.run()
    exit(0)
}

// ---- cursor clamping --------------------------------------------------------

// nearest point strictly INSIDE r (1pt inset) so the warped position never
// sits on a boundary that immediately re-triggers
func clampInto(_ p: CGPoint, _ r: CGRect) -> CGPoint {
    let rr = r.insetBy(dx: 1, dy: 1)
    return CGPoint(x: min(max(p.x, rr.minX), rr.maxX),
                   y: min(max(p.y, rr.minY), rr.maxY))
}

func dist2(_ a: CGPoint, _ b: CGPoint) -> CGFloat {
    let dx = a.x - b.x, dy = a.y - b.y
    return dx * dx + dy * dy
}

let M_KEYCODE: Int64 = 46   // kVK_ANSI_M

func handle(proxy: CGEventTapProxy, type: CGEventType, event: CGEvent,
            refcon: UnsafeMutableRawPointer?) -> Unmanaged<CGEvent>? {
    if type == .tapDisabledByTimeout || type == .tapDisabledByUserInput {
        if let tap = tapPort { CGEvent.tapEnable(tap: tap, enable: true) }
        return Unmanaged.passUnretained(event)
    }

    let (enabled, display, rect, corridor) = state.get()
    if !enabled { return Unmanaged.passUnretained(event) }

    // ---- panic key: bare M while the cursor is on the projector display ----
    if type == .keyDown {
        let keycode = event.getIntegerValueField(.keyboardEventKeycode)
        let mods: CGEventFlags = [.maskCommand, .maskControl, .maskAlternate]
        if keycode == M_KEYCODE && event.flags.intersection(mods).isEmpty {
            let mouse = CGEvent(source: nil)?.location ?? CGPoint(x: -99999, y: -99999)
            if display.contains(mouse) {
                state.disable()
                elog("FENCE_PANIC_OFF: M pressed with cursor on projector display — fence + window clamp disabled")
                return nil   // consume the keystroke
            }
        }
        return Unmanaged.passUnretained(event)   // any other key: untouched
    }

    // ---- mouse clamping: only ever act on the projector display ------------
    let loc = event.location
    if !display.contains(loc) { return Unmanaged.passUnretained(event) }
    if rect.contains(loc) { return Unmanaged.passUnretained(event) }
    if let c = corridor, c.contains(loc) { return Unmanaged.passUnretained(event) }

    // outside the allowed region: warp to the nearest point inside the union
    var best = clampInto(loc, rect)
    if let c = corridor {
        let alt = clampInto(loc, c)
        if dist2(alt, loc) < dist2(best, loc) { best = alt }
    }
    CGWarpMouseCursorPosition(best)
    event.location = best
    return Unmanaged.passUnretained(event)
}

// ---- window clamping (Accessibility API) ------------------------------------
// Windows mostly on the projector display get pushed (resized if needed)
// fully into the screen zone. Our own app (parent pid) is skipped.

func axFrame(_ win: AXUIElement) -> CGRect? {
    var posV: AnyObject?, sizeV: AnyObject?
    guard AXUIElementCopyAttributeValue(win, kAXPositionAttribute as CFString, &posV) == .success,
          AXUIElementCopyAttributeValue(win, kAXSizeAttribute as CFString, &sizeV) == .success
    else { return nil }
    var p = CGPoint.zero, s = CGSize.zero
    guard AXValueGetValue(posV as! AXValue, .cgPoint, &p),
          AXValueGetValue(sizeV as! AXValue, .cgSize, &s) else { return nil }
    return CGRect(origin: p, size: s)
}

func setAXFrame(pid: pid_t, matching frame: CGRect, position: CGPoint, size: CGSize?) -> Bool {
    let appEl = AXUIElementCreateApplication(pid)
    var value: AnyObject?
    guard AXUIElementCopyAttributeValue(appEl, kAXWindowsAttribute as CFString, &value) == .success,
          let windows = value as? [AXUIElement] else { return false }
    for w in windows {
        guard let f = axFrame(w) else { continue }
        if abs(f.minX - frame.minX) < 3 && abs(f.minY - frame.minY) < 3
            && abs(f.width - frame.width) < 3 && abs(f.height - frame.height) < 3 {
            var p = position
            if let pv = AXValueCreate(.cgPoint, &p) {
                AXUIElementSetAttributeValue(w, kAXPositionAttribute as CFString, pv)
            }
            if var s = size, let sv = AXValueCreate(.cgSize, &s) {
                AXUIElementSetAttributeValue(w, kAXSizeAttribute as CFString, sv)
            }
            return true
        }
    }
    return false
}

func num(_ v: Any?) -> CGFloat? {
    if let n = v as? NSNumber { return CGFloat(truncating: n) }
    return nil
}

func clampWindows() {
    let (enabled, display, rect, _) = state.get()
    guard enabled else { return }
    let ownPids: Set<pid_t> = [getppid(), getpid()]
    guard let list = CGWindowListCopyWindowInfo([.optionOnScreenOnly, .excludeDesktopElements],
                                                kCGNullWindowID) as? [[String: Any]] else { return }
    for info in list {
        guard let layer = info[kCGWindowLayer as String] as? Int, layer == 0,
              let pidN = info[kCGWindowOwnerPID as String] as? NSNumber,
              let bd = info[kCGWindowBounds as String] as? [String: Any],
              let x = num(bd["X"]), let y = num(bd["Y"]),
              let w = num(bd["Width"]), let h = num(bd["Height"])
        else { continue }
        let pid = pid_t(truncating: pidN)
        if ownPids.contains(pid) { continue }
        let frame = CGRect(x: x, y: y, width: w, height: h)
        if frame.width < 40 || frame.height < 40 { continue }   // palettes/tooltips
        // act only on windows that live mostly on the projector display
        let inter = frame.intersection(display)
        if inter.isNull || inter.width * inter.height < frame.width * frame.height * 0.5 { continue }
        // already inside the zone (small tolerance)?
        if rect.insetBy(dx: -2, dy: -2).contains(frame) { continue }
        let name = (info[kCGWindowOwnerName as String] as? String) ?? "pid \(pid)"
        let nw = min(frame.width, rect.width - 4)
        let nh = min(frame.height, rect.height - 4)
        let nx = min(max(frame.minX, rect.minX + 2), rect.maxX - 2 - nw)
        let ny = min(max(frame.minY, rect.minY + 2), rect.maxY - 2 - nh)
        let resized = nw != frame.width || nh != frame.height
        if setAXFrame(pid: pid, matching: frame,
                      position: CGPoint(x: nx, y: ny),
                      size: resized ? CGSize(width: nw, height: nh) : nil) {
            elog(String(format: "fence: clamped window '%@' (%.0f,%.0f %.0fx%.0f) -> (%.0f,%.0f%@)",
                        name, frame.minX, frame.minY, frame.width, frame.height,
                        nx, ny, resized ? String(format: " %.0fx%.0f", nw, nh) : ""))
        }
    }
}

// AXObserver on the frontmost app for immediate reaction to move/resize;
// the 1.5s sweep below is the fallback that also re-targets the observer.
var axObserver: AXObserver? = nil
var observedPid: pid_t = 0
var clampPending = false

func scheduleClamp() {
    if clampPending { return }
    clampPending = true
    DispatchQueue.main.asyncAfter(deadline: .now() + 0.4) {
        clampPending = false
        clampWindows()
    }
}

let axCallback: AXObserverCallback = { _, _, _, _ in scheduleClamp() }

func watchFrontmost() {
    guard let app = NSWorkspace.shared.frontmostApplication else { return }
    let pid = app.processIdentifier
    if pid == observedPid || pid == getppid() || pid == getpid() { return }
    if let o = axObserver {
        CFRunLoopRemoveSource(CFRunLoopGetMain(), AXObserverGetRunLoopSource(o), .defaultMode)
        axObserver = nil
        observedPid = 0
    }
    var o: AXObserver?
    guard AXObserverCreate(pid, axCallback, &o) == .success, let obs = o else { return }
    let appEl = AXUIElementCreateApplication(pid)
    AXObserverAddNotification(obs, appEl, kAXWindowMovedNotification as CFString, nil)
    AXObserverAddNotification(obs, appEl, kAXWindowResizedNotification as CFString, nil)
    CFRunLoopAddSource(CFRunLoopGetMain(), AXObserverGetRunLoopSource(obs), .defaultMode)
    axObserver = obs
    observedPid = pid
}

// ---- stdin protocol reader --------------------------------------------------

func rectFrom(_ obj: Any?) -> CGRect? {
    guard let d = obj as? [String: Any],
          let x = num(d["x"]), let y = num(d["y"]),
          let w = num(d["w"]), let h = num(d["h"]) else { return nil }
    return CGRect(x: x, y: y, width: w, height: h)
}

func startStdinReader() {
    DispatchQueue.global(qos: .userInitiated).async {
        while let line = readLine(strippingNewline: true) {
            guard !line.isEmpty, let data = line.data(using: .utf8),
                  let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any]
            else { continue }
            if let en = obj["enabled"] as? Bool, en == false {
                state.disable()
                elog("fence: disarmed")
                continue
            }
            if let display = rectFrom(obj["display"]), let rect = rectFrom(obj["rect"]) {
                let corridor = rectFrom(obj["corridor"])
                state.set(display: display, rect: rect, corridor: corridor)
                elog(String(format: "fence: armed  display(%.0f,%.0f %.0fx%.0f)  zone(%.0f,%.0f %.0fx%.0f)  corridor %@",
                            display.minX, display.minY, display.width, display.height,
                            rect.minX, rect.minY, rect.width, rect.height,
                            corridor.map { String(format: "(%.0f,%.0f %.0fx%.0f)", $0.minX, $0.minY, $0.width, $0.height) } ?? "none"))
                DispatchQueue.main.async { scheduleClamp() }
            }
        }
        elog("fence: stdin closed, exiting")
        exit(0)
    }
}

// ---- main -------------------------------------------------------------------

let mouseMask: CGEventMask =
    (1 << CGEventType.mouseMoved.rawValue) |
    (1 << CGEventType.leftMouseDragged.rawValue) |
    (1 << CGEventType.rightMouseDragged.rawValue) |
    (1 << CGEventType.otherMouseDragged.rawValue)
let keyMask: CGEventMask = 1 << CGEventType.keyDown.rawValue

var tap = CGEvent.tapCreate(
    tap: .cgSessionEventTap, place: .headInsertEventTap, options: .defaultTap,
    eventsOfInterest: mouseMask | keyMask, callback: handle, userInfo: nil)
if tap != nil {
    panicKeyAvailable = true
} else {
    // some setups gate keyboard taps separately — fall back to mouse-only
    tap = CGEvent.tapCreate(
        tap: .cgSessionEventTap, place: .headInsertEventTap, options: .defaultTap,
        eventsOfInterest: mouseMask, callback: handle, userInfo: nil)
    if tap != nil { elog("fence: keyboard tap unavailable — panic key M disabled (Cmd+Alt+F still works)") }
}

guard let activeTap = tap else {
    elog("""
    FENCE_NO_PERMISSION: could not create the mouse event tap.
    Grant Accessibility permission once: System Settings → Privacy & Security
    → Accessibility → enable GORMAPPER (or the app that launched it).
    """)
    exit(2)
}

tapPort = activeTap
startStdinReader()

let source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, activeTap, 0)
CFRunLoopAddSource(CFRunLoopGetMain(), source, .commonModes)
CGEvent.tapEnable(tap: activeTap, enable: true)
elog("fence: event tap active (panic key M " + (panicKeyAvailable ? "armed" : "unavailable") + ")")

// window-clamp sweep + frontmost-app observer refresh
let sweep = Timer(timeInterval: 1.5, repeats: true) { _ in
    watchFrontmost()
    clampWindows()
}
RunLoop.main.add(sweep, forMode: .common)

CFRunLoopRun()
