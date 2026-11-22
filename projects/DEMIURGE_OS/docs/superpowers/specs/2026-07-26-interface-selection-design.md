# DEMIURGE Audio Interface Selection — Design

Date: 2026-07-26
Status: approved (conversation, 2026-07-26)

## Problem

Bypass mode carried a Scarlett-specific ALSA card id (`audio_interface =
hw:CARD=USB,DEV=0`) in every set file. Any other brand's interface (e.g.
Volt 2 = card id `V2`) failed to open and demiurge.service restart-looped.
There was no way to see or choose the active interface from any UI.

## Identity: names, not hw:N

Interfaces are identified by **ALSA product name** (the bracket text in
`aplay -l`, e.g. `Volt 2`, `Scarlett Solo USB`) — stable across reboots and
port moves, human-readable in UIs. Matching is case-insensitive substring,
with spaces/underscores treated as equal (PipeWire node names encode the same
product string with underscores). Card numbers and card ids are resolved from
the name at launch time, never stored.

## Selection state (two layers)

1. **Per-set explicit** — optional key in a set's `.conf` (hence in live.conf
   after `demiurge-set`):

       interface = Volt 2

   Absent by default. If present and that device is attached, it wins.

2. **Global remembered** — `~/demiurge/interface.conf`, a single line holding
   the name of the last interface the user explicitly picked in any UI.
   Lives OUTSIDE live.conf because `demiurge-set` overwrites live.conf
   wholesale on every patch switch.

## Resolution order (one resolver, in the launcher)

1. live.conf `interface` name, if attached
2. global `~/demiurge/interface.conf` name, if attached
3. legacy live.conf `audio_interface` `CARD=` id, if attached
4. best attached real interface: first playback-capable USB card —
   never HDMI (`vc4hdmi*`), never loopback (`DemiurgeLoop`/`Loopback`),
   never MIDI-only devices
5. existing fallbacks: bypass mode keeps waiting/retrying; sync-layer mode
   clocks off snd-aloop

The resolved device is FORCED into the engine at launch:
- bypass: `DEMIURGE_AUDIO_IF=hw:CARD=<id>,DEV=0` → wrapper → Csound
  `-iadc/-odac plughw:...` (never the wrapper's hardcoded default)
- sync layer: the resolved card's `alsa_output.usb-*` node is ordered first
  (front pair / clock master) in aggregate.rs

Every status surface reports what was actually resolved, not what was asked.

## Write path: one CLI

`demiurge-interface` (bash, `src/wrappers/`, installed to
`/opt/demiurge/bin/`):

- `demiurge-interface` — human list: attached interfaces, marks
  `*` resolved-active and `>` global choice, shows per-set override if any
- `demiurge-interface --list` — machine lines `name|card_id|flags`
  (flags: comma-joined `active`, `chosen`)
- `demiurge-interface <name>` — validates against attached (warns if absent
  but still writes), writes the global file, `sudo systemctl restart demiurge`

No UI edits config directly — same pattern as demiurge-set / demiurge-power.

## UI surfaces (thin wrappers over the CLI)

- **Mac TUI `/opt/homebrew/bin/demiurge`**: dashboard line showing active
  interface; new `i` key → full-screen page (telemetry-style) listing
  attached interfaces 1-9, digit selects via ssh `demiurge-interface <name>`.
- **NEPTR pygame UI**: 4th companion tab `AUDIO` (companion.py SCREENS +
  clamp, COMPANION_TABS/TITLES, item counts, `_render_audio`, activate
  branch). List from `NEPTR_IFACE_LIST_CMD` (default
  `/opt/demiurge/bin/demiurge-interface --list`), select via
  `NEPTR_IFACE_SET_CMD` (default `... {name}`), on a daemon thread like
  patch switching. Mac/Pi copies stay byte-identical.
- **Web UI**: interfaces included in `/api/status` (via `--list`), new
  `POST /api/interface {name}` → CLI; frontend gets an "INTERFACE" section of
  tiles like patches, active one highlighted.

## Out of scope (YAGNI)

Multi-interface simultaneous routing, per-channel mapping, separate
input/output device selection, editing the per-set `interface` key from UIs
(hand-edit set files for that).

## Testing

- web: extend MockProbes + fixtures, tests for the new endpoint/parser
- launcher: build on Pi; verify resolution order by plugging Volt with/without
  global file and with a set override
- end-to-end: switch interface from each UI, confirm Csound reopens on it
