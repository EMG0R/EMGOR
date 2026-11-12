# Companion apps + the Claude-Code watch-approval hook

Three pieces talk to the watch over the same BLE (Nordic UART) protocol
(PROTOCOL_V2.md): the **phone app** (data source of record), the **Mac app**
(approvals + Mac data), and a **Claude Code hook** (routes permission prompts to the wrist).

## Data ownership
| Data | Source |
|---|---|
| Weather, notifications, music, calendar, time, phone battery | **Phone (Android app)** |
| Permission-approval cards, mute command, Mac battery, Mac media (for arbitration) | **Mac app** |

The watch prefers the phone's weather and only falls back to the Mac's when the phone's is
stale/absent (firmware-side, see FIRMWARE.md).

## Android companion app (`companion-app/`)
Kotlin/Compose. Speaks the Bangle.js/Gadgetbridge wire protocol the firmware already uses
(phone→watch = `\x10GB({...})\n`; watch→phone = bare JSON + `\r\n`). Provides notifications
(NotificationListenerService), media (MediaSessionManager), weather (Open-Meteo + phone
location, incl. US AQI), calendar, phone battery, time sync, camera control (in-app CameraX
with zoom presets/flip/focus/flash/gallery; Android 14 needs a visible activity +
SYSTEM_ALERT_WINDOW — headless capture is impossible), and the audiobook sleep-timer command.
Theme: dark purple/violet to match the watch.

## Mac companion app (`mac-app/`)
Swift, menu-bar app (LSUIElement), SwiftPM. Build: `swift build -c release` then
`./make-app.sh` → `TheWatch.app`; auto-starts via a LaunchAgent
(`~/Library/LaunchAgents/com.emgor.thewatch.plist`). No third-party deps.
- **BLE central** (CoreBluetooth): scans name-prefix `Bangle.js`, NUS service
  `6E400001-…`; auto-reconnects on drop (re-discovers characteristics before marking ready).
  Sends `hello` (src:"mac"), `setTime`, mute state, battery, media on every (re)connect.
- **Mac battery** via IOKit → `devBattery` src:"mac".
- **Media**: now-playing via AppleScript (Music/Spotify); transport via CGEvent media keys;
  MediaRemote is entitlement-gated on macOS 15+ so it's not used. Reports as src:"mac".
- **Weather**: fallback source (Open-Meteo via IP-geolocation — no location prompt), sent on
  connect (+3s/+10s retries) and every 20 min. The watch prefers the phone's.
- **Mute**: menu-bar "Mute watch sounds" toggle → `sound {on:bool}` (persisted; re-asserted
  on reconnect). (Firmware sound is currently removed, so this is a no-op until re-added.)
- **Permission bridge** — localhost HTTP on `127.0.0.1:8765`:
  - `POST /permission` body `{title,body,origin}` → sends `permReq` to the watch, BLOCKS up to
    ~60s for the tap, returns `{approve,scope,timedOut}`. Disconnected watch returns
    `timedOut:true` immediately (never hangs).
  - `GET /health` → connection status, battery, now-playing, pending count.
  - Concurrency: each request gets a unique id + its own semaphore in a single locked registry,
    so N parallel windows never cross-wire. A menu-bar Approve/Deny resolves a request AND sends
    `permCancel` to dismiss the watch card (synced both directions).

## Claude-Code watch-approval hook (`claude-watch-approve/`)
Approve Claude Code permission prompts by tapping the watch.
- `watch_approve_hook.py` — a **PreToolUse hook** (Python 3 stdlib, no installs). On a
  permission-worthy tool it `POST`s to the Mac bridge `/permission`, waits for the tap, and
  emits the decision Claude Code honors:
  ```json
  {"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"allow|deny|ask","permissionDecisionReason":"..."}}
  ```
  (exit 0). Verified against current docs — this shape bypasses the interactive prompt.
- **Fail-safe:** if the bridge/watch is down or times out → returns `ask` (normal terminal
  prompt); never auto-denies on infra failure, never hangs.
- **Routing:** only routes tools that normally prompt (Bash/Write/Edit/MultiEdit/Notebook/
  WebFetch/WebSearch); read-only tools and safe Bash prefixes (`ls`, `git status`, `grep`…)
  auto-allow with no watch buzz.
- **Scopes:** watch tap = `scope:"session"` → hook caches by `session_id` and auto-approves the
  rest of that window (per-window isolation; window A's yes never leaks to window B). `always`
  persists forever to a cache file. 3 windows at once → queued on the watch, each answered to
  its own window.
- **Install:** merge the `PreToolUse` block from `settings.snippet.json` into
  `~/.claude/settings.json` (matcher `Bash|Write|Edit|MultiEdit|NotebookEdit|WebFetch|WebSearch`).
  **Hooks load at session start** — already-open Claude Code windows must be restarted (or
  `/hooks`) to pick it up; new windows get it automatically. Verify with `/hooks` and
  `/permissions`.
- Requires: the Mac app running (bridge on 8765) + watch connected. `python3` on PATH.

## Confirmed working end-to-end
Watch tap → Mac bridge → hook returned `permissionDecision:"allow"` with
`scope:"session"`; the live session auto-approved thereafter. The only reason early taps
"didn't take" was pre-existing windows not having loaded the hook yet.
