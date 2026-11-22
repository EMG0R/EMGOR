# Expression pedal

One path, two possible sources.

## The path
The engine has ONE expression input: **ch1 CC29 delivered as OSC `/cc` by the
Demiurge parameter pool** (`demiurge-io`). A ch1 CC29 that arrives on the MIDI
wire is parked on CC30 (nothing reads it), and the old ch7 CC14/15 rewrite is
gone. `csound/handler/dispatch/expr_cc29.orc` turns the input into `kNorm`,
applies soft-takeover (disarmed after any owner change or source change,
re-armed by movement > 0.03), and calls `ExprSet(gkLastToggle29, kNorm)`. The
destination is the sticky `gkLastToggle29` (last-toggled effect wins, or a
preset declares it; menu-0 presets declare 30 = `gkMasterVol`). Ranges and
curves live per destination in `handler/expr_targets.src.orc`. Preset recall
does not touch `gkLastToggle29`. The engine never learns which pedal it is.

## Source selection: presence-conditional maps (2026-10-22)
Which pedal feeds the input is a mapping rule, not engine code:

| condition (`when`) | source (wire) | pool map ids |
|---|---|---|
| `{"present": "DUODECIMUS"}` | treadle, ch7 CC14 Jack-A / CC15 Jack-B | `expr-duodecimus-jack-a/b` |
| `{"absent": "DUODECIMUS"}` | body pedal, ch1 CC29 via NEPTR Serial Bridge | `expr-body-pedal` |

Rules live in `pool/neptr_default.map.json` (source of truth, `profiles`), and
the live copy is `~/demiurge/sensors.json` (installed with
`python3 pool/install_map.py`; demiurge-io hot-reloads it). All targets are
`{"param": "/csound/neptr/expr"}`: a pool parameter that sends `/cc 1 29`.
`demiurge-io` reads the Midi Through bus, re-evaluates `when` at 4 Hz (USB
product / kernel ALSA client name, hotplug both ways) and only the active
profile's maps write. The general mechanism (syntax, adding a rule for another
controller, simulating presence) is in `~/demiurge/docs/pool-presence-maps.md`.

- Body pedal: Teensy `exp` field, 12-bit 128..3992 -> 0..127 linear
  (`serial_bridge.py:exp_scale`), sent as ch1 CC29 over the bridge.
- On a switch the pool sends `/cc 16 122` once (before the new pedal's first
  value): the engine bumps `gkExprSrcEpoch`, which re-arms soft-takeover so the
  target is not slammed to the other pedal's resting position. Destination
  ownership and per-preset behaviour are untouched.
- OSC `/cc` writes from other software (web panel) still reach the same input.
- The `ch1 CC29 belongs to the DUODECIMUS` rule (memory) was about two writers
  fighting on one CC. It still holds in effect: at any time exactly one map is
  active for the target, and the wire CC29 is ignored by the engine.

## Detection
Presence = a USB device whose product/manufacturer/vid:pid, or a kernel ALSA-seq
client name, contains `DUODECIMUS` (`demiurge_io/presence.py`). Why attachment
and not traffic: a parked treadle sends nothing, so a timeout would hand
expression back to the body whenever the player stops moving. Limit: it detects
the controller, not a pedal in its jack: DUODECIMUS attached with an empty jack
leaves expression with it (dead). Unplug the DUODECIMUS to use the body pedal.

Failure mode: if `demiurge-io.service` is down, the expression pedal (either
one) is dead until it restarts (`Restart=always`, ~2 s). Encoders, buttons and
DUODECIMUS presets ride the wire and are unaffected. The service runs on cores
0-2, normal priority.

## Deploy
Engine (`midi_handler.orc`, `globals.orc`) needs `demiurge.service` restarted;
`serial_bridge.py` needs `neptr-bridge.service`; the pool is
`demiurge-io.service` (`/opt/demiurge/src/demiurge-io`, unit rendered with the
rig user). Install the map with `pool/install_map.py`.

Bridge wiring (fixed 2026-10-22): `mido.open_output(..., virtual=True)` only
named the ALSA *port*; the *client* was "RtMidiOut Client", so the unit's
`aconnect "NEPTR Serial Bridge":0 14:0` (client-name match) failed. The bridge
now passes `client_name=MIDI_PORT_NAME`.

Tests: `tests/test_expression_pedal.py` (engine side),
`src/demiurge-io/tests/test_presence.py` (conditions, hotplug, the switch).
Retired in favour of the pool: `duodecimus_presence.py`, `gkDuoPresent`, the ch7
CC14/15 funnel.
