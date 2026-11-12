# DEMIURGE Web UI — Design Spec

**Date:** 2026-07-13
**Status:** Approved by emgor (LAN-open, mirror-the-companion directive)
**Scope:** DEMIURGE system layer ONLY — patches/sets, power, services, telemetry, signal flow. No NEPTR effect state, no OSC effect mirror, no menu grid. That boundary is a hard requirement from the user.

## Guiding principle

The web UI is the `demiurge` companion TUI in a browser. Same fields, same names, same bar semantics, same green/yellow/red thresholds, same patch-list layout with `▸` active marker and power tags, same action set. Browser-native additions (sparklines, touch tiles, SSE push) are allowed only where they don't change the companion's vibe. When in doubt, look at `demiurge/docs/companion.md` and copy it.

## Architecture (Option B — Pi-hosted daemon)

One new service on the Pi: `demiurge-web.service`.

- **Process:** a single Python file (`web/demiurge_web.py`, ~300–400 lines), **stdlib only** — `http.server.ThreadingHTTPServer` for static files, JSON API, and Server-Sent Events. No Flask, no async framework, no pip dependencies, no build step.
- **Unit:** `User=gorpi7`, `CPUAffinity=0-2` (never the audio core 3), `Nice=10`, `WantedBy=multi-user.target`, deliberately **not** `PartOf=demiurge.service` — the UI must stay reachable when audio is stopped (that's the point of a start button).
- **Status collection:** every 2 s the daemon runs the same probes the companion/pi-monitor use, but locally (no SSH): `vcgencmd`, procfs/sysfs, `systemctl is-active`, `~/demiurge/live.conf` and `~/demiurge/sets/*.conf` parsing. Each frame is written atomically (write + rename) to `~/.demiurge/status` as JSON — the status file `next-steps.md` already planned. The web UI is its first consumer; companion and pi-monitor may converge on it later (explicitly out of scope for v1).
- **Serving:** `http://demiurge.local:8080` (8080 confirmed clear of RNBO 5678, OSC 1234/9000–9099).
  - `GET /` — static `index.html` + `app.js` + `style.css`, vanilla JS, hand-drawn canvas sparklines, CSS-div bars.
  - `GET /api/status` — one JSON frame (the `demiurge --once` equivalent).
  - `GET /api/events` — SSE stream pushing each new frame; browser never polls. On disconnect the page shows a banner and auto-retries (mirrors the companion's 1 s-while-disconnected refresh).
  - `POST /api/patch {name}` / `POST /api/power {low|med|high}` / `POST /api/audio {start|stop|restart}`.
- **Security:** LAN-open per user decision. No token, no login. Bind to all interfaces.

## Status frame — field map (companion parity)

| Field | Source | Display |
|---|---|---|
| host, uptime | `hostname`, `/proc/uptime` | header |
| temp | `vcgencmd measure_temp` | bar to 85°C, green/yellow/red at 70/80 |
| freq | `scaling_cur_freq` vs `cpuinfo_min_freq`–`scaling_max_freq` | bar over min–max |
| load | `/proc/loadavg` 1-min vs `nproc` | bar |
| mem | `/proc/meminfo` | bar |
| throttled | `vcgencmd get_throttled` | raw hex + 8-bit decode: now=red / past=yellow for UNDERVOLT · ARM-CAP · THROTTLE · TEMP-LIM |
| volt | `vcgencmd measure_volts core` | value |
| cores | `/sys/devices/system/cpu/online` + `nproc` | e.g. `0-3 (4)` — brownout canary |
| gov | `scaling_governor` | value (pinned `performance`) |
| audio | `systemctl is-active demiurge` | `● active` style |
| ui | `systemctl is-active neptr-ui` | `✓ on` style |
| pwr | derived from `scaling_max_freq`: ≤1.6 GHz LOW, ≤2.4 GHz MED, else HIGH | LOW/MED/HIGH — same rule as companion |
| patch | `# set:` tag in `~/demiurge/live.conf` | name |
| signal | `chain =` block tokens from live.conf (companion's parsing logic; one row per chain block) | `IN ─▶ neptrPhase4 ─▶ OUT` row |
| patches[] | `~/demiurge/sets/*.conf` + `# set:`/`# power:` headers, `nam` hidden | numbered 1–9 tiles, power tag, `▸` on active |

**pi-monitor additions in the same frame:** xrun 60 s rolling count (journald + launcher-log counting, `/dev/shm` history technique lifted from `pi-monitor.sh`), throttle 60 s window, PMIC temp, EXT5V volts (`pmic_read_adc`), per-core CPU % (`/proc/stat` deltas), V3D clock.

**Browser-native (kept subtle):** last ~10 min of frames held in daemon memory, served on page load, appended via SSE → sparklines for temp/load/xrun/EXT5V; a throttle-event timeline strip (when UNDERVOLT/THROTTLE bits fired) to correlate audio weirdness with power events.

## Control actions

| Action | Executes | Confirm |
|---|---|---|
| Patch switch (tap tile) | `demiurge-set <name>` (validated against live sets list) | 1-tap confirm sheet — "restarts audio" |
| Power low/med/high | the existing Pi-side power helper — **never reimplemented; user owns the power mechanism.** Exact helper name/path to be confirmed on the Pi before implementation. | none |
| Start audio | `sudo systemctl start demiurge` | none |
| Stop / restart audio | `sudo systemctl stop\|restart demiurge` | two-step arm: button turns red "CONFIRM", 3 s timeout (pi-monitor's reboot pattern) |

Actions run serially (one at a time), return exit status + captured output, trigger an immediate status frame, and append to `~/.demiurge/logs/web-actions.log`.

## Visual design

Dark, stage-friendly, portrait-and-landscape phone-first: near-black background, big type, the companion's green/yellow/red semantics, huge touch targets, `<meta name=viewport>`. Layout order mirrors the companion top-to-bottom: header → telemetry bars → throttle decode → status row (AUDIO/UI/PWR) → PATCH + SIGNAL → PATCHES tiles → action row. No widget toolkits, no CDN assets.

## Deploy

Lives in DEMIURGE_OS: `web/` (daemon + static files) + `config/demiurge-web.service`. Staged to the Pi with the standard rsync-to-/tmp-then-mv pattern; installer step appended to `setup-script/setup-demiurge.sh`. Needs the existing passwordless-sudo systemctl entries (already present for SSH use).

## Error handling

- Probe command failure → field reported as `null`, UI shows `—`; frame still ships (partial data beats stale data).
- Action command failure → non-2xx with captured stderr shown in a toast; status refresh still fires.
- SSE client drop → daemon prunes dead connections; no unbounded growth.
- live.conf/sets parse errors → patch list empty + warning banner, telemetry unaffected.

## Testing

- Unit: status-frame parser functions (throttle decode, live.conf/sets parsing, power derivation) with fixture inputs — pytest on the Mac, no Pi needed.
- Integration: run daemon on the Mac with a `--mock` probe mode (canned vcgencmd/procfs outputs) to develop the frontend without the Pi.
- On-Pi verification: `curl /api/status`, SSE smoke, one patch-switch round-trip, confirm daemon CPU ~0% and pinned off core 3 (`taskset -pc`).

## Out of scope (v1)

Companion `t` telemetry stream (Teensy MIDI monitor), converting companion/pi-monitor to read `~/.demiurge/status`, HTTPS/auth, NEPTR effect state (permanently, per user).

## Open items

1. Confirm the exact Pi-side power helper command name/path (companion docs reference Mac-side `demiurge-power-remote`; the on-device UI uses `demiurge-power`).
2. Pi is currently offline — on-Pi verification and the power-helper check wait for power-on.
