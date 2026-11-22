# hyperGuitar ↔ NEPTR link — design

2026-09-15

## Why

hyperGuitar's NeoTrellis 4x4 was removed and went into the PMOR v2 rebuild. The
guitar therefore has **no control surface at all**. This is not a convenience
feature: NEPTR's menu is hyperGuitar's replacement UI, and hyperGuitar is
unplayable until it exists.

The Trellis is expected back eventually. So the link is **two-way by design, not
as an extra**: whichever surface moves last is reflected on the other. Neither is
the master.

## What already exists (verified on hardware 2026-09-15)

Nothing needs to be built for the transport. The Bela is already plugged into the
Pi over USB and fully enumerated:

```
Pi   eth1  192.168.7.1/24   <-->  Bela  192.168.7.2   (reachable, ssh root works)
Pi   eth2  192.168.6.1/24   <-->  Bela  192.168.6.2
amidi:  Bela-7D5B MIDI 1  (already fanned into Midi Through 14:0)
lsusb:  1d6b:0104 Multifunction Composite Gadget  (net + MIDI + UAC2 audio bridge)
```

`oscpkt.hh` ships in `/root/Bela/include/` on the device, so the Bela side needs
no new dependency.

hyperGuitar itself has **no OSC of any kind** today — no liblo, no sockets. That
is the only genuinely new surface on the guitar side.

## Transport and addressing

UDP OSC over the existing USB network link.

| Direction | Endpoint | Messages |
|---|---|---|
| NEPTR → hyperGuitar | `192.168.7.2:7772` | `/hg/lane ii`, `/hg/pedal f`, `/hg/sync` |
| hyperGuitar → NEPTR | `192.168.7.1:7770` | `/hg/state iii`, `/hg/hello i` |
| NEPTR engine → UI | `127.0.0.1:8000` | `/hg_present i`, `/hg_lane iii` |

7770 is the engine's **existing** control-OSC port (`giCtlOSC`, already carrying
`/cc` and `/p`). Reusing it means no new daemon and no new listener process —
the drain loop in `midi_handler.orc` gains one more address to match.

Port 7772 is new and belongs to the Bela. Checked against the NEPTR port map
(7770 control in, 8000 state out, 8001 wavetable, 8002 telemetry out, 8003 sensor
in, 8004/8005 extra telemetry) — no collision.

## Presence detection

The Bela sends `/hg/hello 1` once per second, unconditionally, from its render
thread's control tick. The engine sets `gkHgPresent = 1` on receipt and times out
to 0 after 3 seconds of silence.

This is deliberately **not** a USB/ifconfig check on the Pi. What the menu needs
to know is not "is a Bela plugged in" but "is the hyperGuitar instrument running
and reachable" — a Bela that is booting, or running a different project, must not
produce a menu that sends into a void. A heartbeat from the instrument itself is
the only signal that actually means what the menu needs it to mean.

No daemon, no polling from the Pi side, no new service unit.

## The lane model

Five lanes, one per NEPTR encoder, drawn left → right. NEPTR's UI list is drawn
left→right while the hardware slots run right→left, so:

| Lane | UI idx | HW slot | Toggle CC | Rotation CC | Name |
|---|---|---|---|---|---|
| 0 | 0 | 4 | 72 | 22 | PS GLIDE |
| 1 | 1 | 3 | 71 | 21 | PS STEPS |
| 2 | 2 | 2 | 76 | 26 | FORMANT |
| 3 | 3 | 1 | 75 | 25 | DYNAMICS |
| 4 | 4 | 0 | 74 | 24 | SYNTH |

A **lane** owns an ordered list of entries. Each entry is an `(effect id,
voicing)` pair drawn from hyperGuitar's existing `gCatSlot[][]` voicings — the
same p1/p2 values, not retuned. Button = toggle the lane on/off. Rotation =
accumulate through the lane's entries.

Two exclusivity rules, which together produce everything the user asked for:

1. **Within a lane**, at most one entry is active.
2. **Across lanes**, activating an entry deactivates any entry in another lane
   that carries the same effect id.

Rule 2 is what makes lanes 0 and 1 a radio pair without special-casing them: both
drive `PAD_PITCH`, so turning one on turns the other off. Rule 1 is what lets
lane 3 hold entries spanning four *different* effect ids (`PAD_ENVF`, `PAD_FWAH`,
`PAD_PLUCKSAG`, `PAD_POG`) and still behave like one knob.

### Lane 0 — PS GLIDE

The one genuinely new DSP behaviour. Pitch shift at **100% wet**, with
`gPitchSemitones` driven continuously by the expression pedal rather than set to
a fixed interval. Rotation scrolls the pedal's **range**:

```
0  ±12 semitones   (default — the "big change vibe")
1  ±5
2  ±7
3  ±24
4  -12..0          (down only)
5  0..+12          (up only)
```

Pedal position arrives as `/hg/pedal f` (0..1) from the engine, forwarded from
the existing expression dispatch. The Bela maps it to the active range and writes
`gPitchSemitones` per control block, with the same glide smoothing the encoder
path already uses, so a fast pedal sweep does not step.

### Lane 1 — PS STEPS

The nine stable-interval pitch voicings already in `CAT_PITCH`, in the order they
appear there:

`OCT DOWN · 4TH DOWN · 5TH DOWN · OCT UP · 5TH UP · MIN 3RD DN · MAJ 3RD DN ·
SHIMMER · DETUNE 12C`

### Lane 2 — FORMANT

The seven `PAD_FORMANT` voicings from `CAT_SPECTRAL`:

`FORM 0.75 · FORM 0.90 · FORM 1.10 · FORM 1.30 · FORM DEEP · FORM HIGH · FORM SILK`

Slots 0–3 are the v5 pads restored verbatim and are explicitly marked "do not
retune" in `presets.cpp`. They are carried through unchanged.

### Lane 3 — DYNAMICS

Everything whose character is driven by picking dynamics, ten entries:

`WAH SOFT · WAH VOCAL · WAH SCREAM · FWAH SOFT · FWAH MID · FWAH WIDE ·
SAG SUBTLE · SAG BEND · SAG WOBBLE · POG SWELL`

`PAD_PLUCKSAG` is the "pitch descending plucky" one — the pitch sags downward on
the attack and recovers as the note settles.

`PAD_FWAH` re-routes the wet/dry legs, so its entries must re-run
`applyMixRouting()` — the existing `gFx[]` apply hook already does this and the
lane dispatch goes through that table rather than around it.

### Lane 4 — SYNTH

`gSynthPoly` is forced to 1 whenever this lane is on; rotation scrolls
`gSynthMode` through all ten `SYNTH_NAMES` entries:

`SAW LEAD · MOOG LEAD · ACID SQUELCH · WAVEFOLDER · RINGMOD BELL · SYNC LEAD ·
FM GROWL · CHIP LEAD · SUB GROWL · FORMANT VOWEL`

## Two-way sync

Every lane change on the Bela — whether it arrived over OSC or (in future) from a
Trellis press — ends in one place, `hgLaneApply()`, which emits
`/hg/state <lane> <on> <index>`. The engine mirrors it into `gkHgLaneOn[]` /
`gkHgLaneIdx[]` and relays `/hg_lane` to the UI.

Because the echo is emitted by the apply function rather than by the OSC handler,
a Trellis press and an OSC message are indistinguishable downstream, and the
future Trellis gets two-way sync for free with no additional code.

`/hg/sync` asks the Bela to re-emit all five lanes; the engine sends it on
receiving a `/hg/hello` after a presence gap, so a Bela restart resyncs NEPTR
without user action.

## Menu gating

`nav.orc` currently hardcodes `% 20`. It becomes `% (20 + gkHgPresent)`, so
menu 20 exists in the cycle only while the heartbeat is alive. If presence drops
while the user is sitting on menu 20, the engine snaps `gkCurrentMenu` to 0
rather than stranding them on a dead page.

The UI's `MENUS` list gains a HYPERGUITAR entry; `MENU_COUNT` becomes a computed
floor of 20 with the entry appended, and the UI's own menu cycling and rendering
honour `/hg_present` the same way.

## Files

**Bela (new):** `hg_osc.cpp` — oscpkt send/receive on 7772, heartbeat.
**Bela (new):** `hg_lanes.cpp` — lane tables, `hgLaneApply()`, exclusivity rules.
**Bela (modified):** `render.cpp` (control tick + pedal glide), `hyperguitar.h`
(globals), `trellis_ui.cpp` (route future Trellis presses through `hgLaneApply`).

**NEPTR engine (new):** `csound/hyperguitar.orc` — `gkHg*` globals, the OSC
send UDOs and the `$HG_TOGGLE`/`$HG_ROTATE` macros.
**NEPTR engine (new):** `csound/hyperguitar_link.orc` — instr 98, presence and
the inbound state mirror.

These are **two** files, not one, and the split is forced rather than stylistic.
instr 98 listens on `giCtlOSC`, which `midi_handler.orc` creates, so it must be
included *after* that file. But the menu-20 dispatch branches that expand
`$HG_TOGGLE`/`$HG_ROTATE` are themselves included from *inside*
`midi_handler.orc`, and a csound macro must be defined before the line that
expands it — so the macro half must come *before*. One file cannot satisfy both;
the first attempt failed to compile for exactly this reason.

Both are new files rather than additions to `globals.orc`/`signal_chain.orc`:
nothing here touches NEPTR's own audio path.

**NEPTR engine (modified):** `nav.orc` (dynamic menu count), five
`toggle_slot*.orc` + five `rot_slot*.orc` (menu-20 branch), `expr_cc29.orc`
(forward the pedal, after the soft-takeover gate). `midi_handler.orc` needed **no**
change — instr 98 drains `/hg/state` and `/hg/hello` off the same `giCtlOSC`
handle directly, and `handler_osc.orc` needed none either, because instr 98
relays to the UI itself.

**NEPTR UI (modified):** `neptrPhase4_UI.py` — `HG_LANES` label tables, the
`_render_hyperguitar` page, `hg_slot`/`hg_rot` touch actions, `/hg_present` and
`/hg_lane` routing, and one widening of the OSC `enqueue` helper, which silently
truncated every message to its first argument (nothing had sent more than one
until `/hg_lane`).

**Tests (new):** `tests/test_hyperguitar_link.py` — parses `hg_lanes.cpp`,
`hyperguitar.orc` and the UI's tables and asserts all three agree label-for-label
and length-for-length, plus that every lane branch is presence-gated and lands in
the dispatch file the UI's own row→CC tables say it should.

**NEPTR UI (modified):** `neptrPhase4_UI.py` — HYPERGUITAR menu entry, presence
gating, lane rendering.

## Out of scope

- Audio does **not** cross the link. hyperGuitar's output reaches NEPTR the way
  it already does, over the UAC2 USB audio bridge. This is a control link only.
- The onboard NEPTR expression pedal stays unrouted (`maps: []`). The DUODECIMUS
  treadle is *the* expression pedal. See the separate note on the `/dev/ttyACM0`
  index collision, which is a latent footgun rather than a current fault.
