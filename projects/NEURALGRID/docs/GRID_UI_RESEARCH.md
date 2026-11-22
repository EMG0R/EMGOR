# Grid-native UI research + Direct Lab v2 grammar proposal

Screenless performance UI for neuralGrid's 8×8 RGB pad grid, 4 edge
ribbons, 4 FSRs, 4 lidars, targeting the Machine Lab (7 robots, 74
strikes). Written against current state: `septagraph/py/septagraph/pi/README.md`,
`DEMO_TUESDAY.md`, `docs/SEPTAGRAPH_PLAN.txt` §8.

## 1. What the monome/grid lineage actually does (no screen, ever)

**mlr** — each row = one sample, playhead is a moving light, no mode
key at all: the *row* is the mode. A held key = loop-start; a second
press elsewhere in the row = loop-end, i.e. hold-then-press composes a
range instead of a discrete toggle. Top row is dimly-lit meta-row
(stop group 1 / stop group 2), spatially separated from playable rows
so a mis-press can't be a musical note. [monome/docs](https://monome.org/docs/grid/app/sum/), [MLR looping modification thread](http://archive.monome.org/community/discussion/3477/mlr-looping-modification/p1)

**kria** — 4 tracks × 4 parameter-pages (trigger/note/octave/duration),
each page independently clock-divided/probabilistic, selected by a
dedicated row of "page" keys, not by holding anything — page selection
and step-editing live in different rows so they're never ambiguous.
Meta-sequencing (chaining pattern banks) needs an explicit hold+press:
hold the CONFIG key, press the pattern-page key, to touch the
meta-sequence layer instead of the current pattern. [kria docs](https://monome.org/docs/ansible/kria/), [meta-sequencing tutorial](https://llllllll.co/t/ansible-kria-meta-sequencing-tutorial-video/10075)

**white whale** — 16-step loop, columns = steps, one row's worth of
dedicated trigger-lanes; presets recalled from a fixed corner region so
"which pattern is live" is always a lit key, never remembered state.
[White Whale docs](https://monome.org/docs/whitewhale/)

**meadowphysics** — 8 independent counters, one per row, cascading
toward zero; a row *is* a channel, with no page/mode concept needed —
proof that dedicating whole rows to whole functions removes an entire
class of "what mode am I in" bugs. [Meadowphysics docs](https://monome.org/docs/ansible/meadowphysics/)

**earthsea** — grid as a tone map / extended keyboard; multi-press
gestures recall "shape memories," and per-shape modulation is set by a
physical knob, not a grid gesture — offloading a continuous parameter
to a *different control surface* rather than overloading the grid.
Directly analogous to using our ribbons instead of grid chords. [Earthsea docs](https://monome.org/docs/ansible/earthsea/)

**Novation Launchpad** — mode selection is a **dedicated side/top
row of buttons that is never part of the playable surface**: Session /
Drum / Note / Custom keys live outside the 8×8 in their own strip, lit
to show current mode; side arrows page/transpose. This is the cleanest
"selector row physically separate from content" pattern in the
lineage. [Launchpad Mini Session mode](https://userguides.novationmusic.com/hc/en-gb/articles/23731303692306-Using-Launchpad-Mini-s-Session-mode), [Launchpad X Note mode](https://userguides.novationmusic.com/hc/en-gb/articles/23731440690706-Using-Launchpad-X-s-Note-mode)

**Ableton Push 2** — step sequencer with a loop-length row (pads
double as bar-length selectors when a modifier is held), and **hold a
step + turn an encoder** to edit that step's Nudge/Length/Fine/Velocity
— the canonical hold-object-then-modify-with-a-continuous-control
gesture, same shape we want for hold-step + slide-ribbon. Note Repeat:
holding a pad + choosing a rate row sets retrigger rate, and *pressure*
on the held pad controls the repeated notes' volume — pressure-as-a-
continuous-modulator-of-a-held-gesture is exactly our FSR use case.
[Push 2 manual](https://www.ableton.com/en/manual/using-push-2/)

**Elektron (Digitakt/Analog Rytm/Digitone) parameter locks** — hold a
TRIG key, turn a DATA ENTRY encoder: the step's LED graphic *inverts*
while a locked parameter is being set, so the surface itself shows
"you are in per-step-edit mode right now," not a separate mode
indicator. Trig conditions (probability/every-other-loop) are a
second, discoverable layer under the same hold-key gesture. [Elektron Digitakt manual p.33](https://www.manualslib.com/manual/1275776/Elektron-Digitakt.html?page=33), [P-locks vs conditions discussion](https://www.elektronauts.com/t/p-locks-vs-trig-conditions/42492)

**Yamaha Tenori-On** — 16 full-grid "layers"; **one dedicated key**
(R5) instantly recalls/switches layers, explicitly described as
"performable" switching — the switch key becomes part of the
performance vocabulary rather than an interruption. [Yamaha design notes](https://www.yamaha.com/en/tech-design/design/insights/id_005/)

**Synthstrom Deluge** — near-zero menu diving via *shortcuts on
virtually every button* (hold a function pad + tap a param pad), and a
fixed side column dedicated to audition/mute/solo per row, spatially
separate from the sequencing grid, same separation principle as
Launchpad's side row. [Deluge product page](https://synthstrom.com/product/deluge/), [Deluge community docs](https://seangoodvibes.github.io/DelugeDocs/documentation/sequencer/sequencer-specifications/)

### Recurring interaction grammar (the 8 questions)

1. **Mode/page select without a screen**: either (a) a *whole row/
   region is permanently reserved* for mode keys, spatially separate
   from playable cells (Launchpad top/side row, mlr's meta-row, Deluge
   mute/solo column), or (b) *one dedicated single key* recalls/pages
   (Tenori-On R5, kria's page-select row). Nobody overloads an
   ordinary playable pad to mean "now everything means something else"
   without a lit, distinct visual state.
2. **Held modifier = second layer**: hold key/step + press or turn
   something else (Elektron p-locks, Push 2 note settings, Deluge
   shortcuts). The held object's LED **inverts or pulses** for the
   duration so the state is visible the whole time it's active, never
   only at press or only at release.
3. **Sequencer on RGB pads**: playhead = one color sweeping (mlr,
   meadowphysics use a moving/lit column); active step = a second,
   steady color; per-step velocity = brightness or a 3-tier color ramp
   (our sequencer's existing 3-level velocity cycle is exactly this);
   pattern bank = a small fixed row/corner of pads, one per bank slot,
   lit = has content, bright = currently selected.
4. **Tempo/swing/length/quantize without a display**: never on the
   sequencing grid itself — Push 2 puts loop-length on a *dedicated
   row* of pads (tap a pad = set loop length to N steps, immediate
   visual bar). Elektron uses physical encoders. Our analogue is
   ribbons: a continuous 1-D control is a *better* fit than any grid
   chord for a scalar like tempo, because position=value is legible at
   a glance (LED marker at the ribbon's mapped point) with no chords
   to remember.
5. **Mute/solo**: dedicated column/row, one pad per track, tap toggles,
   held = solo (Deluge's fixed audition/mute/solo column; Launchpad
   Drum mode's per-row mute).
6. **Copy/chain patterns**: hold source pattern-slot + press destination
   slot (universal "hold A, press B = A→B" gesture across kria meta-seq
   and Elektron pattern chaining).
7. **Instrument vs sequencer, unambiguous**: dedicating *entire rows or
   an entire half of the grid* to one function for the whole session
   (meadowphysics: row=channel, never ambiguous) beats any global mode
   flag, because the ambiguity a global mode flag creates (which mode
   am I in *right now*?) is exactly what all screen-only devices need
   a screen to resolve — a spatial split needs no memory at all.
8. **Documented failures**: forum/community threads (OXI Instruments
   forum "Issues with Monome Grid mode," multi-page) and general
   consensus repeatedly cite the same three failure modes: (i) a mode
   change with no distinct-enough LED cue gets missed and the performer
   plays into the wrong mode; (ii) a modifier key released a fraction
   too early silently reverts to the base function mid-gesture, so a
   parameter edit "didn't take"; (iii) global mode toggles that live on
   an ordinary grid pad get triggered by accident during energetic
   play. [OXI forum: Issues with Monome Grid mode](https://oxiinstruments.com/forum/d/304-solved-issues-with-monome-grid-mode/60) The counter-pattern that avoids all three: modifiers
   and mode keys live in a **physically separate, dimly-idle-lit
   region**, never inside a region that's also "played," and every
   hold-state has a **sustained** (not blink-once) LED tell.

## 2. neuralGrid constraints that shape the design

From `pi/README.md` + `DEMO_TUESDAY.md`: today's Direct Lab already
does the "reserve whole rows" trick (rows 0-3 = play, rows 4-7 = seq).
4 ribbons run at ~42 fps, giving 0-1 position + touched flag — fast
enough for tap/hold/slide discrimination but *not* fast enough to also
require sub-10ms decisions; softpots are the SEPTAGRAPH_PLAN's chosen
"live per-note/global offset" surface (§8: N=rate, E=loud, S=space,
W=harm) — that assignment is for the *continuum* engine, not Direct
Lab, so Direct Lab v2 is free to assign the ribbons differently. FSRs
are velocity-capable strike pads today (fw6, `fsr.py`, onset at 0.08
rising / release at 0.04 falling, gamma-curve tuned live). Lidar is
optional/experimental per plan (§8: height→space axis) and explicitly
should stay non-destructive. No LED language exists yet for Direct Lab
specifically (SEPTAGRAPH_PLAN §8's warm-white/blue-green/violet/amber
scheme is for the window/continuum system) — Direct Lab v2 needs its
own, consistent with that palette's *spirit* (color = category, pulse
= prediction/pending, sweep = playhead) but its own concrete mapping.

## 3. Tap/hold/slide discrimination (ribbons, 42 fps)

- **Tap**: touch→release in **< 150 ms**, |Δposition| over the touch
  **< 0.03**. Fires on release.
- **Hold**: touched continuously **≥ 300 ms** with |Δposition| staying
  **< 0.03** the whole time. Fires (transitions into "held" state) at
  the 300ms mark, not on release — so a hold's second-layer effect is
  live for as long as it's held, matching the "sustained LED tell"
  finding above.
- **Slide**: at any point after touch-down, |Δposition| since touch-
  down or since the last slide-sample **> 0.05** → classified as slide,
  cancels any pending tap/hold classification for that touch and drives
  a continuous value stream instead (position itself, sampled every
  frame ~24 ms).
- A touch that starts as a hold and then moves past the 0.05 slide
  threshold **downgrades to slide-while-held** (this is exactly the
  Push-2/Elektron "hold object, drag a continuous control" gesture) —
  the held object's identity is latched at 300ms and the slide only
  changes its parameter, never re-picks the object.
- Ambiguity window (150-300 ms, position stable): no action yet,
  ribbon LED shows a **dim pending pulse** at the touch point so the
  performer can see the hold is "arming" — addresses failure mode (i)
  from §1.8 directly.

## 4. Direct Lab v2 — three candidate grammars

### Candidate A: "Row-locked, ribbon-paged" (minimal change)
Keep today's static row split (0-3 play / 4-7 seq) permanently; ribbons
never change mode, only page/parameter within whichever half a pad-
press is in. Simplest, closest to current code, but keeps today's
weakness: the split is fixed 4/4 even when a performer wants all 8 rows
of play (e.g. Marimbot solo) or all 8 rows of sequencing.

### Candidate B: "N-ribbon function row, virtual" (monome-style, no
extra hardware)
Reuse the physical grid's row 0 as a **virtual Launchpad side-row**:
row 0 pads are never played, always mode/page keys, warm-white dim
idle / bright when active. Rows 1-7 are content. Closest analogue to
Launchpad's dedicated mode row. Costs one full row of playable surface
permanently (7 rows of instrument content instead of 8), which on
Marimbot's 32-note layout means an extra page to page through — a real
cost given "8 points per page" is already tight.

### Candidate C (recommended): "Ribbons own mode/page/param, all 64
pads stay playable"
Nothing on the 8×8 is ever sacrificed to chrome. The four ribbons take
over *all* mode/page/parameter duties, exactly as SEPTAGRAPH_PLAN
already intends ribbons to be the performance-modulator surface (§8) —
this proposal extends that principle from the continuum engine to
Direct Lab. Grid pads are always either "instrument keys" or "step
keys," never "menu keys," so a mis-press during energetic play is
always a musical event, never a lost mode — directly answering failure
mode (iii). This is the recommended grammar; detailed below.

## 5. Recommended grammar (Candidate C) — pages & ribbon table

**Global page ring** (N ribbon, tap-zones): the N ribbon is divided
into **4 tap-zones** (position 0.0-0.25 / 0.25-0.5 / 0.5-0.75 /
0.75-1.0), one per page: **PLAY / SEQ / SPLIT / MIX**. A *tap* inside a
zone (per §3 thresholds) jumps directly to that page — no chording,
no "next/prev," so the page is always exactly where the performer's
finger lands (like Push 2's loop-length row: tap position = value
directly, not a stepped increment). A **hold** anywhere on N (≥300ms,
stationary) instead arms the *secondary* N-function: while held, N
becomes a fine "scroll" for whatever the active page's scrollable thing
is (PLAY: instrument catalog page; SEQ: pattern bank; MIX: nothing).
Releasing N always returns it to the page-tap-zone behavior.

**Per-page ribbon table**:

| Page | N ribbon (tap) | N held+slide | S ribbon | E ribbon | W ribbon | Lidar (any edge) | FSR0-3 |
|---|---|---|---|---|---|---|---|
| PLAY | jump to PLAY/SEQ/SPLIT/MIX zone | scroll instrument-page (Marimbot pg 1-4 etc) | (unused, idle-dim) | velocity offset for *next* strike, ±0.3, position 0.5=neutral | instrument select: 4 zones = 4 rows' bound instruments, tap swaps a row's instrument | height over S = global velocity scale (optional, non-destructive: only active while a hand is present, else last strike's raw velocity used) | 4 assignable velocity strike pads (as today) |
| SEQ | jump to page | scroll pattern bank (8 slots) | tempo (40-240 BPM, position→BPM linear) tap=nudge±1 hold+slide=sweep | **held-step p-lock**: hold a step pad ≥300ms, then slide E = set that step's velocity (3-tier LED ramp updates live, Elektron-style); no held step → E sets swing 0-60% | track select: 4 zones = 4 tracks, tap arms a track for editing | height over S = swing feel (only while sequencer running AND a hand present; otherwise last set swing holds) | per-track mute (tap) / solo (hold ≥300ms) |
| SPLIT | jump to page | scroll (top-half instrument page) | tempo (steps 4-7 are the sequencer half) | top-half=velocity offset; bottom-half hold-step+slide=p-lock (E is spatially aware: reads which half a currently-held touch belongs to) | top-half instrument select | same as SEQ, applies to bottom half only | top FSRs=strike pads, unused for bottom (or track-mute if held ≥300ms) |
| MIX | jump to page | scroll instrument list (>4 tracks) | per-instrument velocity scale (0-2x), select instrument by which grid row is currently lit-active, then slide S | (unused, idle-dim) | (unused, idle-dim) | (unused) | per-instrument mute (tap) / solo (hold) — mirrors SEQ's FSR mapping so muscle memory carries over |

Rationale for reusing FSR for mute/solo on SEQ/MIX rather than adding a
new gesture: FSRs are the one control that's a *discrete pad-like
object* (not a continuous ribbon), so "tap vs hold" on an FSR reads
exactly like "tap vs hold" on a grid pad — same grammar, different
finger. This keeps FSRs' meaning page-dependent but never overloaded
within a page (never simultaneously "strike" and "mute" on the same
page).

### ASCII per-page grid layout

```
PLAY (rows 0-7, W ribbon zones bind row->instrument, 8 pts/page)
row7 [ i i i i i i i i ]  W-zone3
row6 [ i i i i i i i i ]  W-zone3
row5 [ i i i i i i i i ]  W-zone2
row4 [ i i i i i i i i ]  W-zone2
row3 [ i i i i i i i i ]  W-zone1
row2 [ i i i i i i i i ]  W-zone1
row1 [ i i i i i i i i ]  W-zone0
row0 [ * i i i i i i i ]  W-zone0   (* = pad0,0 page indicator)

SEQ (rows 0-7 = 8 tracks x 8 steps, amber col = playhead)
row7 [ s s s s s s s s ]  track7
 ..   ...
row1 [ s s s s s s s s ]  track1
row0 [ * s s s s s s s ]  track0     (* = page indicator)
        ^ playhead column sweeps L->R, amber

SPLIT (today's default: top play, bottom seq)
row7..4 [ i i i i i i i i ]   PLAY rows (row=instrument)
row3..1 [ s s s s s s s s ]   SEQ rows (row=track)
row0    [ * s s s s s s s ]   * = page indicator, row0 still a seq track

MIX (row = instrument, bar-graph = velocity scale, red=mute, green=solo)
row7 [ # # # # # . . . ]  instrument D  (5/8 = scale 1.25x)
row3 [ # # # # # # # # ]  instrument C  SOLO (bright green, others dim)
row0 [ * . . . . . . . ]  * = page indicator
```

## 6. LED language, Direct Lab v2 (per pad, all state must be visible)

- **PLAY pads**: warm white (per SEPTAGRAPH_PLAN §8 — this stays
  literally the "physical catalog point" color) at rest; **bright
  amber flash** on strike (fixed ~120ms decay so a fast run of hits is
  still individually visible); **dim warm-white pulse** on a pad
  currently bound as an FSR's assigned strike target, so "which pad
  does FSR2 mirror" is never hidden state.
- **SEQ pads**: step **off** (empty), **steady blue-green** (has a
  hit, low velocity), **brighter blue-green** (mid), **white-blue**
  (high velocity) — a 3-tier ramp reusing the existing 3-level velocity
  cycle, now spatially shown instead of requiring a long-press-to-
  cycle-and-check. **Amber sweep** = playhead column, matches
  SEPTAGRAPH_PLAN's existing "amber = playhead" convention exactly.
  A step currently latched under a p-lock hold: **inverted flash**
  (Elektron's own convention) — steady bright white fill instead of
  its color, for as long as the hold lasts.
- **SPLIT**: PLAY rules on rows 0-3, SEQ rules on rows 4-7,
  simultaneously — this is today's behavior, unchanged, just formally
  a page in the ring instead of the only option.
- **MIX**: each row = one instrument; the row's 8 pads show a **bar
  graph** of that instrument's current velocity scale (e.g. scale=1.2
  lights ~5 of 8 pads dim green ascending); muted instrument's whole
  row goes to a single dim red; soloed instrument's row goes bright
  green, all *other* rows dim to near-off so solo state is unmistakable
  at a glance.
- **N ribbon LED marker** (ribbons apparently already carry an LED
  strip per HANDOFF hardware, confirm in firmware — if not, this is a
  Pi-side no-op and the page state must instead show via one dedicated
  always-visible grid pad, see below): 4-color block per zone, current
  zone brightened; a small moving dot at the touch position while
  held+scrolling.
- **Always-visible page indicator on the grid itself** (redundant with
  the ribbon, belt-and-suspenders against failure mode (i)): pad (0,0)
  is reserved on every page as a fixed indicator — never playable,
  colored per current page (PLAY=warm white, SEQ=blue-green,
  SPLIT=half/half diagonal, MIX=violet) — a single fixed reference the
  performer can glance at without hunting, cheaper than reserving a
  whole row (Candidate B) since it's one pad, not eight.
- **Hidden-state checklist** (everything above, listed flat): current
  page (ribbon zone + pad 0,0), which row/track is armed on SEQ/SPLIT
  (W ribbon zone brightened), which instrument each row plays on
  PLAY/SPLIT (persists from W ribbon's last tap, shown as a dim colored
  edge-tick on that row's pad 7 matching an instrument-ID color), N-held
  scroll position (ribbon dot), FSR strike-pad assignment (dim pulse
  on PLAY pads, §above), per-step velocity tier (pad color, SEQ),
  p-lock-armed step (inverted flash), mute/solo per track (MIX row
  color / SEQ FSR long-press — shown identically both places), swing
  and tempo value (no persistent grid LED needed since S ribbon itself
  is the display: LED dot at the ribbon position corresponding to the
  current value, always live).

## 7. Accidental mode-switch protection

- Page changes are **N-ribbon-only**, never a grid-pad gesture — a
  fast flurry of pad hits during PLAY can never accidentally reach SEQ
  or MIX, because paging lives on hardware nothing else writes to.
- The 150-300ms ambiguity window (§3) means a quick brushing touch on N
  while reaching across the instrument (physically plausible, ribbons
  run the N/S/E/W edges) reads as neither tap nor hold if it's under
  150ms and doesn't sit in one zone — no state changes on a graze.
  Combined with requiring stillness (|Δpos|<0.03) for both tap and
  hold classification, a brief incidental contact that itself involves
  sliding motion (e.g. a hand pushed past the ribbon) auto-classifies
  as "slide" which on N with no page context yet queued does nothing
  destructive (only affects instrument-scroll when a page is already
  active, and scroll amount for a <150ms brush is negligible).
- FSR mute/solo (SEQ/MIX) requires the *same* 300ms hold threshold as
  ribbon holds, so a hard incidental strike (someone leaning on an
  FSR) reads as a strike/onset, not a mute-toggle, unless sustained.

## 8. Cheat sheet (8 lines, memorizable)

1. N ribbon taps a zone = jump PLAY/SEQ/SPLIT/MIX. Hold N = scroll.
2. Pad (0,0) always shows your page color. When in doubt, look there.
3. PLAY: pads = instruments/notes. W ribbon zones pick each row's
   instrument. E ribbon = velocity offset for your next hit.
4. SEQ: pads = steps, amber sweep = playhead. S ribbon = tempo.
5. Hold a step + slide E = set that step's velocity (p-lock). Release
   E's hold and the step stays at whatever you left it.
6. FSR tap = strike (PLAY) or mute (SEQ/MIX). FSR hold (300ms+) =
   solo instead of mute.
7. SPLIT = today's rows 0-3 play / 4-7 seq, always available as its
   own page, not a separate toggle.
8. Lidar only ever nudges velocity/swing while a hand is actually
   present over S — remove your hand, it does nothing, last value
   holds.

## 9. Firmware vs Pi-side

**No firmware changes required.** Everything above consumes exactly
the channels already shipped: ribbon position (0-1) + touched flag at
~42fps, FSR pressure (0-1, onset/release thresholds already in
`fsr.py`), lidar height (100-800mm). Tap/hold/slide classification,
zone mapping, the page ring, p-lock storage, and all LED composition
are Pi-side state machines layered on top of the existing
`GridController`/`Sequencer`/mode-dispatch architecture described in
`pi/README.md`. The one true prerequisite already flagged as a known
gap in that README: **confirm fw6 is actually flashed** (FSR channels
present) before any of §5's FSR mute/solo/p-lock-adjacent behavior can
be demoed — softpot-only parts of this grammar (page ring, tempo/
swing, velocity offset, W-ribbon instrument select) work today
regardless of FSR firmware status.
