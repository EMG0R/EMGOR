# demiurge-notify — COD's unified voice hook

`demiurge-notify <im_done|attention>` plays COD's "I'm done" / "attention" sound.
Same call on Pi and Mac; the script picks the route.

## Calling it
Claude Code hooks (settings.json `Stop` -> `demiurge-notify im_done`,
`PermissionRequest` -> `demiurge-notify attention`). It detaches itself
(setsid + disown), so the hook returns immediately and the sound survives the
process-group kill.

`DRY_RUN=1 demiurge-notify im_done` prints the exact command and plays nothing.

## Routing
| Platform | Command | Output |
|---|---|---|
| Linux / Demiurge Pi | `nice -n 19 pw-play --media-role Notification --latency 200ms <wav>` | PipeWire default sink (no `--target`) |
| macOS | `afplay <wav>` | Mac default output |

Optional Linux opt-in: `DEMIURGE_NOTIFY_TARGET=<node.name>` adds `--target`.
Off by default: the script never pins or holds a device.

## Where the wavs go
Lookup order: `DEMIURGE_NOTIFY_WAV` (explicit file), then `$DEMIURGE_NOTIFY_DIR`,
`~/demiurge/sounds/`, `/opt/demiurge/sounds/`, the Mac's current location
`~/Documents/_EMGOR_SYNTH/______2026NEW/`, `~/EMGOR_SKILLS/sounds/`.
Names: `im_done.wav`, `attention.wav`. Recommended Pi home: `~/demiurge/sounds/`.
A missing wav exits 0 so it never breaks a hook.

## Invariants
- Never RT priority: nice 19, SCHED_OTHER, large latency hint. No chrt, no rtkit.
- Never steal from Csound/Cardinal: it never sets quantum, rate, or defaults,
  never touches `demiurge-sink`, and never restarts audio services.
- Never forces or holds a device; no default-sink changes, no persistent stream.

## Open item: the "utility audio" sink
Today Demiurge has no dedicated utility sink. `demiurge-sink` is a 4ch
loopback with `priority.session = 0` (not default); the physical interface
has priority 2000 and wins as default. So on the Pi the sound goes to the
physical interface's default, mixed with the master. A true JACK-locked
utility client needs a new PipeWire node (own `node.name`, low priority,
no RT); once it exists, set `DEMIURGE_NOTIFY_TARGET` to it or make it default.
