# NEPTR engine ↔ UI link

How the on-device touch UI (`NEPTR_phase4/neptrPhase4_UI.py`) and the NEPTR
csound engine (`neptrPhase4.csd`) talk to each other, as of 2026-08-09.
There are exactly **two transports**, both pre-existing — nothing new was
invented:

1. **engine → UI: OSC** on `127.0.0.1:8000` (the engine's `$OSC_I/$OSC_F`
   feedback macros, triple-send UDP). Slot on/off state (`/xxx_state`),
   every parameter, menu (`/menu`), last-toggled effect (`/last_toggle`),
   looper transport (`/lpN_state` etc.).
2. **UI → engine: MIDI CCs** on the shared `Midi Through` ALSA-seq pool
   (the same bus the Teensy, clock and touch daemons use; engine reads it
   with `-M`). The UI emits the **exact physical CCs the Teensy encoders
   emit** on ch1 — the engine cannot tell touch from hardware, so nothing
   engine-side needed rewiring for control.

## UI → engine: Teensy-identical CCs (ch1)

The engine's `handler_remap.orc` permutes PHYSICAL encoder CCs into LOGICAL
ones, so the UI (like the Teensy) sends the physical numbers. Buttons send
press `127` then release `0`; rotations send relative pulses (CW=`127`,
CCW=`0`, one per detent).

**Slot toggles** — UI menu row → physical button CC. Every menu draws row0 =
hardware slot4 (verified against every `gkCurrentMenu` branch of
`handler_menus.orc`; the LOOPER alone lists CH1..CH5 = slot0..slot4):

| UI row (top→bottom) | hw slot | logical CC | **physical CC sent** |
|---|---|---|---|
| 0 | slot4 | 72 | **75** |
| 1 | slot3 | 71 | **71** |
| 2 | slot2 | 76 | **72** |
| 3 | slot1 | 75 | **73** |
| 4 | slot0 | 74 | **70** |

**Menu paging** — horizontal swipes on the normal menu views send the
engine's own menu prev/next button CCs (physical **76** = prev, **74** =
next); the `/menu` echo drives the view change. The LOOPER view is
excluded (its rows own the touch surface as transport buttons).

**PRESETS banks** — the UI renders whichever bank `/preset_bank` reports:
0 = surf, 1 = bass, 2 = GORCORE (ids 10-14, lit from `/preset`). Taps on
the PRESETS menu always send the slot CC (preset select / stackable /
go-dry semantics live engine-side).

LOOPER rows (CH1..CH5 = slot0..slot4) send physical **70, 73, 72, 71, 75**.
Looper rows are held buttons, not taps: the UI maps finger-down → press CC,
finger-up → release CC, so the engine's own gesture classifier (tap /
double-tap clear / 0.4 s hold overdub) works from touch with true timing.

**Param sliders** — real-time absolute: while dragging, the thumb IS the
finger (both directions in one drag); the UI drives the relative encoder
path with error-proportional bursts (up to 10 detents per 30 Hz tick,
~300 detents/s) so the engine value tracks the finger live, and the OSC
echo only reconciles after release (2.5 % deadband). The rotation
dispatch is `gkLastToggle`-keyed engine-side, so the UI gates each slider
on the OSC-tracked `/last_toggle`; the per-param CC/toggle table is
generated mechanically from the orc sources by
`NEPTR_phase4/scripts/gen_param_cc_map.py`.

**Expression pedal** — every menu view carries an unlabeled full-width
slider pinned to its bottom: absolute ch1 **CC29**, exactly what the
physical pedal sends (sticky `gkLastToggle29` routing identical). Tap =
jump, drag = ride; ~30 msg/s + final value on release; silent until first
touched; thumb is UI-local (CC29 has no OSC echo).

**Raw evdev touch (gestures round 4 root cause)** — SDL2/KMSDRM delivers
ZERO touch motion on the ADS7846 (every swipe = a down/up pair at a stale
coordinate), so the UI reads `/dev/input` directly
(`NEPTR_phase4/touch_evdev.py`, daemon-style: same parsing as
`src/wrappers/demiurge-touch`; multiple readers on one node are fine) and
ignores SDL finger/mouse events entirely when a device exists. The cal
file applies directly on raw coords. The stream is conditioned by a
70 ms contact-bounce bridge (resistive pressure dips chop one swipe into
many taps), an EMA smoother with a 2 px deadband, and stale-release
protection. `NEPTR_TOUCH_DEBUG=1` logs every gesture classification.
MIDI-rate discipline: slider bursts are capped at 4 detents / 30 Hz tick
(~120 msg/s) — csound's midiin drains ONE event per k-cycle (~375/s at
ksmps 128) and a 300 msg/s flood was observed to stall the engine's MIDI
input until restart. CC102 sync catch-up echoes update state silently
(no auto-navigation into param views).

**Gesture model (2026-08-09 rework)** — touch is note-on/note-off:
finger-down starts a candidate and immediately draws the element's
WOULD-BE state (OFF slot renders ON, tab renders selected) plus an accent
overlay; the release fires the tap; crossing the swipe threshold cancels
the preview (quick fade) and pages instead — no action ever fires from a
swipe. Slot taps are symmetric toggles (OFF → on + the engine echo opens
the param view; ON → bypass, and a viewed effect STAYS open showing its
dimmed off-state). The one down-fire exception is the LOOPER (below).

**Why this direction is MIDI and not OSC:** toggling/param edits already
had a complete, menu-aware inbound path (the Teensy dispatch). Reusing it
means one state machine, one collision surface, and hardware/touch can
never disagree. (See the "don't build parallel systems" rule.)

## Engine → UI: OSC state, plus the late-join sync request

Slot/param state was ALREADY broadcast over OSC on every change — the UI's
visuals are engine-authoritative (a touch toggle does not flip the slot
locally; the engine's `/xxx_state` echo does). The only gap was a UI that
(re)starts mid-session: change-triggered macros had nothing to say to it.

**`ch16 CC102` — UI sync request.** On startup the UI sends CC102=127 (then
0) on the utility channel (retrying up to 3× until any OSC arrives). The
engine (instr 99, `midi_handler.orc`) answers by pulsing `gkOscRefresh` for
one k-cycle, which makes **every** `$OSC_I/$OSC_F` macro fire
unconditionally — a full state+parameter snapshot lands on the UI.

Collision check (2026-08-09): ch16 carries touch `CC80-95` and clock
`CC116-119`; `CC96-115` were unused across DEMIURGE_OS and NEPTR_phase4.
`CC102` allocated to this. The clock.md protocol table lists it.

Cost note: the refresh burst is a few hundred small localhost UDP packets
in one k-cycle (sub-ms). It fires only on UI (re)start.

### Retry policy (2026-08-10 rework — cold boot correctness)

The first cut asked 3 times, 1.5 s apart, and treated *any* inbound OSC as
"synced". Both halves were wrong on the device:

* **Cold boot.** `neptr-ui` is up seconds after login; `demiurge` needs far
  longer (pipewire, interface probe, csound compile, `state_persist` restore).
  All three requests were spent before the engine could read MIDI at all, and
  the UI then displayed *defaults* on top of a correctly-restored engine — the
  reported "on reboot it saved dsp setting but ui didn't reflect them".
* **One unrelated broadcast** (`/cb_bpm` clock telemetry, a looper position)
  satisfied "any OSC arrived" and cancelled the retries before a snapshot
  existed.

Current rules (`neptrPhase4_UI.py`):

| Rule | Value |
|---|---|
| "synced" means | ≥ `SYNC_MIN_ADDRS` (8) **distinct** OSC addresses seen |
| retry interval | 1.5 s, ×1.6 per attempt, capped at 15 s |
| give up after | 900 s without a snapshot (logs a warning) |
| re-sync trigger | any traffic after `RESYNC_AFTER_SILENCE` (30 s) of silence — i.e. the engine restarted (patch/interface switch, `redem`, crash) |

The re-sync request is issued on the **next loop pass**, not inline, so the
message that revealed the restart is still handled as a normal navigable
event and only the answering burst falls inside the 2 s quiet window.

`EngineOut` also no longer latches a failed MIDI open: a missing Through pool
(normal for a few seconds on boot) is retried with backoff instead of
disabling touch→engine control — and the sync request with it — for the whole
session.

## UI-owned position: `~/demiurge/ui-state.conf`

The engine owns DSP state; the UI owns **where the user was**. That second
half now survives restarts and reboots, in its own file (never `live.conf` —
that file is rewritten by `demiurge-set` and by config tooling, and non-audio
keys have been lost from it more than once):

    view = menu6          # 'menuN' or an effect code
    menu = 6              # the menu an effect view sits in
    companion = 0         # companion menu open?
    companion_screen = 1  # DASHBOARD/PATCHES/POWER/AUDIO/WAVE
    companion_sel = 3
    scroll_patches = -1   # -1 = follow the cursor
    scroll_audio = -1

Written atomically (tmp + rename), debounced 2 s, and only when the position
actually changed; flushed on SIGTERM so `systemctl restart neptr-ui` keeps the
last position. Any read problem — missing, truncated, garbage, wrong types —
falls back to defaults per key, then to the whole default set.

**Nothing the engine owns is stored here** (no effect on/off, no parameters,
no preset bank, no looper transport, no NAM/IR selection). A restored *effect
view* is therefore only re-entered once the engine's own broadcast confirms
both that its menu still holds that effect and that the effect is ON;
otherwise the UI stays on the menu view. `tests/test_ui_state.py` asserts no
DSP-shaped key can appear in the file.

## Engine-side changes (surgical)

- `csound/globals.orc` — `gkOscRefresh init 0`; the two OSC macros gained
  `|| gkOscRefresh == 1`.
- `csound/midi_handler.orc` — clears `gkOscRefresh` at the top of instr 99
  each cycle; sets it when `ch16 CC102 val 127` arrives. Instruments
  numbered >99 see the pulse the same k-cycle, <99 the next — each macro
  fires exactly once per request.

Everything else (slot toggles, looper transport, param edits) reuses the
existing dispatch untouched.

## Third client: the web UI (2026-08-11)

`web/demiurge_web.py` speaks the same two transports, with one asymmetry that
is a property of the protocol rather than a shortcut:

* **Write — no asymmetry at all.** It emits the identical physical ch1 CCs on
  the Through pool. The pool is many-to-many, so a third writer costs nothing
  and the engine cannot tell a phone from a knob. The tables it sends from are
  not retyped: `web/gen_neptr_model.py` lifts `MENUS`, `PARAM_CC`,
  `EFFECT_PARAMS` and the CC permutation tables out of `neptrPhase4_UI.py`
  with `ast` into `web/neptr_model.py`, and a test fails if the two drift.
  Faders run the same relative-detent chase (`PARAM_BURST` cap included), just
  server-side, because a browser is too far away to close a 30 Hz loop.

* **Read — one port, one owner.** `OSC3x_f`/`OSC3x_i` send to exactly one
  hardcoded destination, `127.0.0.1:8000`, and a UDP unicast port has one
  owner. `neptr-ui` binds it for as long as it runs. So the web listener binds
  **only if the port is free** and reports which it got:

  | `link.status` | when | what the browser shows |
  |---|---|---|
  | `live` | `ui = off`, or neptr-ui stopped | every slot lit by the engine; faders drag |
  | `port-busy` | neptr-ui running (the usual case) | controls work; lit state stated as unknown; faders fall back to single detents |

  It deliberately does **not** set `SO_REUSEPORT`. Two listeners would make
  the kernel hand each datagram to one of them, silently half-blinding the
  panel — the surface the user plays with their hands. Nor does it ever infer
  a slot state from a CC it sent: engine-authoritative or unknown, never
  optimistic.

  To give both surfaces live state at once the engine would need a second OSC
  destination (two lines in `csound/globals.orc`'s `OSC3x_*` UDOs) or
  `neptr-ui` would need a configurable listen port. Neither exists today.

## Known quirks

- PREAMP menu: the engine's `midi_handler.orc` header comment claims
  s0-first, but the live dispatch is default-order (row0 = slot4).
  The COMPRESSOR toggle used to be a dead `gkCurrentMenu == 7` branch in
  the CC72 chain (MAIN PREAMP matched first, so neither the physical
  encoder nor the touch slot did anything). FIXED 2026-08-11: the branch
  moved to the CC74 chain, which is where UI row 4 sends.
- NAM and IR VERB params ride the MENU-encoder rotations, not the slot
  rotations every other effect's dry/wet uses. Menus 17/18 spend all five
  slot rotations on their variant browse, so a slot-rotation home is
  unreachable inside the very menu whose page draws the slider. Logical
  CC20 (P2) and CC23 (P3) are the two rotations no menu shadows:
  NAM in gain = CC20, NAM out gain = CC23 (both armed by toggle 37, which
  every NAM row's slot button now sets); IR row r dry/wet = CC20 armed by
  toggle 49+r, IR damp = CC23. Fixed 2026-08-11 — before that both the
  encoders and the sliders were dead on those two pages.
- Sliders for params with no encoder path (e.g. MOVE DELAY mode, TAPE
  DELAY head) are inert; ones whose effect is not the engine's
  last-toggled show "KNOBS ARMED ELSEWHERE — TOGGLE THE SLOT TO ARM".
