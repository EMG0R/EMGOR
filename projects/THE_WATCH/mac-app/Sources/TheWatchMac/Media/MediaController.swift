import Foundation
import AppKit
import CoreGraphics

/// Media reporting + control for the Mac.
///
/// ── Why this design (macOS Sonoma/Sequoia/Tahoe, 2025–2026) ──────────────────
/// The classic route for *reading* now-playing was the private MediaRemote
/// framework (`MRMediaRemoteGetNowPlayingInfo`). As of macOS 15.4 Apple gated it
/// behind a private entitlement: `mediaremoted` now denies non-entitled clients,
/// so third-party apps get `nil`. (Control via `MRMediaRemoteSendCommand` still
/// works, but the framework is private and can vanish.) See README for links.
///
/// Chosen, viable paths (no private frameworks, no disabling SIP):
///   • READ now-playing  → AppleScript against Music.app and Spotify.app
///     (only queried when the app is already running; needs Automation TCC grant).
///   • CONTROL transport → system media keys via CGEvent / NSEvent (works for any
///     player; needs Accessibility TCC grant). Seek (audiobook ±30s) + volume use
///     AppleScript against the running player.
///
/// Documented-but-stubbed fallbacks (see README): MediaRemote via the
/// `mediaremote-adapter` (entitled /usr/bin/perl) helper for universal reads.
final class MediaController {

    struct NowPlaying: Equatable {
        var artist: String
        var album: String
        var track: String
        var playing: Bool
        var positionSec: Int
        var audiobook: Bool
    }

    private let musicBundleID = "com.apple.Music"
    private let spotifyBundleID = "com.spotify.client"

    // MARK: - Reading now-playing

    /// Current now-playing across supported players, or nil if nothing is running
    /// with a track loaded. Runs AppleScript synchronously — call off the main queue.
    func snapshot() -> NowPlaying? {
        if isRunning(musicBundleID), let np = queryMusic() { return np }
        if isRunning(spotifyBundleID), let np = querySpotify() { return np }
        return nil
    }

    private func isRunning(_ bundleID: String) -> Bool {
        NSWorkspace.shared.runningApplications.contains { $0.bundleIdentifier == bundleID }
    }

    private func queryMusic() -> NowPlaying? {
        let script = """
        tell application "Music"
            if player state is stopped then return "stopped"
            try
                set trk to name of current track
                set art to artist of current track
                set alb to album of current track
            on error
                return "stopped"
            end try
            set st to (player state as string)
            set pos to (player position as integer)
            return st & "\\n" & trk & "\\n" & art & "\\n" & alb & "\\n" & pos
        end tell
        """
        return runNowPlayingScript(script, audiobook: false)
    }

    private func querySpotify() -> NowPlaying? {
        let script = """
        tell application "Spotify"
            if player state is stopped then return "stopped"
            set trk to name of current track
            set art to artist of current track
            set alb to album of current track
            set st to (player state as string)
            set pos to (player position as integer)
            return st & "\\n" & trk & "\\n" & art & "\\n" & alb & "\\n" & pos
        end tell
        """
        return runNowPlayingScript(script, audiobook: false)
    }

    private func runNowPlayingScript(_ src: String, audiobook: Bool) -> NowPlaying? {
        guard let out = runAppleScript(src), out != "stopped" else { return nil }
        let parts = out.components(separatedBy: "\n")
        guard parts.count >= 5 else { return nil }
        let playing = parts[0].lowercased().contains("playing")
        return NowPlaying(artist: parts[2], album: parts[3], track: parts[1],
                          playing: playing, positionSec: Int(parts[4]) ?? 0,
                          audiobook: audiobook)
    }

    // MARK: - Control (watch -> mac transport commands)

    /// Handle a `{"t":"music","n":...}` command from the watch.
    func handleTransport(_ command: String) {
        switch command {
        case "play", "pause":
            postMediaKey(NX_KEYTYPE_PLAY)          // toggle (system play/pause)
        case "next":
            postMediaKey(NX_KEYTYPE_NEXT)
        case "previous":
            postMediaKey(NX_KEYTYPE_PREVIOUS)
        case "volumeup":
            postMediaKey(NX_KEYTYPE_SOUND_UP)
        case "volumedown":
            postMediaKey(NX_KEYTYPE_SOUND_DOWN)
        case "rewind":
            seekRelative(-30)                      // audiobook/podcast skip back
        case "forward":
            seekRelative(30)
        default:
            break
        }
    }

    /// Seek the running player by delta seconds (AppleScript player position).
    private func seekRelative(_ delta: Int) {
        let body = "set player position to (player position + (\(delta)))"
        if isRunning(musicBundleID) {
            _ = runAppleScript("tell application \"Music\" to \(body)")
        } else if isRunning(spotifyBundleID) {
            _ = runAppleScript("tell application \"Spotify\" to \(body)")
        } else {
            // No seekable player — fall back to track skip via media key.
            postMediaKey(delta < 0 ? NX_KEYTYPE_PREVIOUS : NX_KEYTYPE_NEXT)
        }
    }

    // MARK: - AppleScript

    @discardableResult
    private func runAppleScript(_ source: String) -> String? {
        var errorInfo: NSDictionary?
        guard let script = NSAppleScript(source: source) else { return nil }
        let result = script.executeAndReturnError(&errorInfo)
        if errorInfo != nil { return nil }
        return result.stringValue
    }

    // MARK: - Media keys (CGEvent / NSEvent systemDefined)

    // NX_KEYTYPE_* codes from <IOKit/hidsystem/ev_keymap.h>.
    private let NX_KEYTYPE_SOUND_UP: Int32   = 0
    private let NX_KEYTYPE_SOUND_DOWN: Int32 = 1
    private let NX_KEYTYPE_PLAY: Int32       = 16
    private let NX_KEYTYPE_NEXT: Int32       = 17
    private let NX_KEYTYPE_PREVIOUS: Int32   = 18

    /// Emit a system media key (down + up) as an NSSystemDefined event. Requires
    /// Accessibility permission to be honored by other apps.
    private func postMediaKey(_ key: Int32) {
        func emit(down: Bool) {
            let flags: NSEvent.ModifierFlags = down ? NSEvent.ModifierFlags(rawValue: 0xa00)
                                                    : NSEvent.ModifierFlags(rawValue: 0xb00)
            let state = down ? 0xa : 0xb
            let data1 = Int((key << 16) | (Int32(state) << 8))
            guard let ev = NSEvent.otherEvent(with: .systemDefined,
                                              location: .zero,
                                              modifierFlags: flags,
                                              timestamp: 0,
                                              windowNumber: 0,
                                              context: nil,
                                              subtype: 8,
                                              data1: data1,
                                              data2: -1),
                  let cg = ev.cgEvent else { return }
            cg.post(tap: .cghidEventTap)
        }
        emit(down: true)
        emit(down: false)
    }
}
