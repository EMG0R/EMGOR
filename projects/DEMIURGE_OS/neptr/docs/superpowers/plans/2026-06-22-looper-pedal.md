# Looper Pedal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a 5-channel looper (one channel per push-encoder) with per-channel record/play/overdub, pure varispeed speed+reverse, and master-loop fraction quantization, exposed as a new `LOOPER` menu in the pygame UI.

**Architecture:** The looper's *logic* (per-channel state machine + quantization math) is first built and unit-tested as a pure Python reference model — this is the executable spec. It is then ported verbatim into Csound: a single always-on engine instrument (94) records/plays 5 mono buffers tapped from the end of the signal chain, driven by a tap/hold/double gesture detector and a symmetric varispeed encoder map in `midi_handler.orc`. The pygame UI is a read-only OSC display that renders 5 channel strips with live playheads.

**Tech Stack:** Csound (orchestra `.orc`/`.csd`), Python 3 (pygame UI + pytest reference model), OSC (python-osc ↔ Csound `OSCsend`).

## Global Constraints

- `sr = 48000`, `ksmps = 128`, `nchnls = 2`, `0dbfs = 1` (from `globals.orc:6-13`) — every Csound buffer/length figure assumes 48 kHz.
- Csound bypass convention: `< 0.5` = active, `>= 0.5` = bypassed (`globals.orc:21`).
- Encoder rotation CC convention: `kVal > 64` = CW/increment, `kVal < 64` = CCW/decrement (`globals.orc:86`).
- Button press = CC value `127`, release = `0`.
- OSC feedback is sent to `127.0.0.1:8000` via `OSC3x_f` / `OSC3x_i` (`globals.orc:1008-1028`); the UI listens there (`neptrPhase4_UI.py:977`).
- Python must run in the project venv (never system Python). Run pytest as `python3 -m pytest`.
- UI rendering is hand-drawn pygame only — no widget toolkit, minimal CPU (`neptrPhase4_UI.py:5-9`).
- Menu count is currently 9 (indices 0–8); the looper becomes menu index **9** (the 10th menu).
- Speed sweep is **pure varispeed** (resampled table read) — no time-stretch / pitch-preservation.
- Symmetric speed stops: `+2× +1× +½× +¼× ‖ −¼× −½× −1× −2×`.
- State codes (shared Python↔Csound): `0=EMPTY 1=RECORDING 2=PLAYING 3=OVERDUBBING 4=STOPPED`.
- Gesture codes: `0=TAP 1=HOLD 2=DOUBLE`.
- Action codes: `0=NONE 1=START_REC 2=SET_LENGTH 3=WIPE 4=STOP 5=PLAY 6=START_OVERDUB 7=COMMIT_OVERDUB 8=CANCEL_OVERDUB 9=CLEAR`.

---

## File Structure

**New files**
- `looper_ref/__init__.py` — package marker for the reference model.
- `looper_ref/model.py` — pure logic: quantization math + per-channel state machine + 5-channel aggregate. No Csound/pygame deps.
- `looper_ref/test_model.py` — pytest suite; the executable spec.
- `csound/effects/looper.orc` — the Csound engine: `LoopTransition` UDO, `LoopChan` UDO, always-on `instr 94`.

**Modified files**
- `csound/globals.orc` — looper globals, buffers, gesture-detector state, speed-stop table (append near end of declarations, before UDOs).
- `csound/neptrPhase4.csd` — add `#include "effects/looper.orc"` after line 51; add `i94 0 z` to the score.
- `csound/signal_chain.orc` — tap input before NAM (line 350-351), sum looper output before `outs` (line 362-363), clear looper out buses at end of instr 1.
- `csound/midi_handler.orc` — `%9→%10` wrap (lines 166,170); menu-9 button dispatch (gesture detector → state machine); menu-9 encoder dispatch (speed map).
- `neptrPhase4_UI.py` — add `LOOPER` to `MENUS`; add channel-strip renderer; route `/loop_*` OSC.

---

## Milestone 1 — Tested Logic Core (Python reference)

This milestone has zero Csound/pygame dependencies and is the spec the Csound port must match. Fully TDD.

### Task 1: Power-of-two length quantization

**Files:**
- Create: `looper_ref/__init__.py` (empty)
- Create: `looper_ref/model.py`
- Test: `looper_ref/test_model.py`

**Interfaces:**
- Produces: `nearest_pow2_ratio(x, min_exp=-2, max_exp=3) -> float`, `quantize_length(recorded_len, master_len, min_exp=-2, max_exp=3) -> float`

- [ ] **Step 1: Write the failing test**

```python
# looper_ref/test_model.py
import math
import pytest
from looper_ref.model import nearest_pow2_ratio, quantize_length


def test_nearest_pow2_ratio_snaps_to_octave_grid():
    assert nearest_pow2_ratio(1.05) == 1.0
    assert nearest_pow2_ratio(0.6) == 0.5      # cut just past half -> half
    assert nearest_pow2_ratio(0.3) == 0.25     # quarter region
    assert nearest_pow2_ratio(1.5) == 2.0      # round-half-up -> double
    assert nearest_pow2_ratio(3.0) == 4.0


def test_nearest_pow2_ratio_clamps_to_exp_bounds():
    assert nearest_pow2_ratio(0.01) == 0.25    # clamp low (min_exp=-2)
    assert nearest_pow2_ratio(50.0) == 8.0     # clamp high (max_exp=3)


def test_nearest_pow2_ratio_rejects_nonpositive():
    with pytest.raises(ValueError):
        nearest_pow2_ratio(0.0)


def test_quantize_length_rounds_against_master():
    # master cycle = 2.0 s
    assert quantize_length(1.1, 2.0) == 1.0    # cut just past halfway -> half
    assert quantize_length(0.9, 2.0) == 1.0    # clearly short -> half
    assert quantize_length(3.9, 2.0) == 4.0    # "two rotations" -> x2 longer
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python3 -m pytest looper_ref/test_model.py -v`
Expected: FAIL with `ModuleNotFoundError` / `ImportError: cannot import name 'nearest_pow2_ratio'`

- [ ] **Step 3: Write minimal implementation**

```python
# looper_ref/model.py
import math


def nearest_pow2_ratio(x, min_exp=-2, max_exp=3):
    """Snap a positive ratio to the nearest 2**n with min_exp <= n <= max_exp.
    Uses round-half-up in log2 space so 1.5 -> 2.0 (not banker's rounding)."""
    if x <= 0:
        raise ValueError("ratio must be positive")
    n = math.floor(math.log2(x) + 0.5)
    n = max(min_exp, min(max_exp, n))
    return 2.0 ** n


def quantize_length(recorded_len, master_len, min_exp=-2, max_exp=3):
    """Channel loop length snaps to master_len * nearest power-of-two ratio."""
    if master_len <= 0:
        raise ValueError("master_len must be positive")
    ratio = nearest_pow2_ratio(recorded_len / master_len, min_exp, max_exp)
    return master_len * ratio
```

- [ ] **Step 4: Run test to verify it passes**

Run: `python3 -m pytest looper_ref/test_model.py -v`
Expected: PASS (5 tests)

- [ ] **Step 5: Commit**

```bash
git add looper_ref/__init__.py looper_ref/model.py looper_ref/test_model.py
git commit -m "feat(looper): power-of-two length quantization (reference model)"
```

### Task 2: Phase-snap (relative sync, no audio shift)

**Files:**
- Modify: `looper_ref/model.py`
- Test: `looper_ref/test_model.py`

**Interfaces:**
- Produces: `snap_phase(phase, subdivisions) -> float` (returns grid-aligned phase in `[0,1)`; a *playback offset only* — callers never trim/shift recorded audio).

- [ ] **Step 1: Write the failing test**

```python
# append to looper_ref/test_model.py
from looper_ref.model import snap_phase


def test_snap_phase_rounds_to_nearest_subdivision():
    assert snap_phase(0.26, 4) == 0.25
    assert snap_phase(0.10, 4) == 0.0
    assert snap_phase(0.40, 4) == 0.5
    assert snap_phase(0.95, 4) == 0.0    # wraps 1.0 -> 0.0


def test_snap_phase_is_idempotent_and_pure():
    # Contract: snap_phase computes a boundary ALIGNMENT only (audio is never
    # touched). Snapping an already-snapped phase is stable, and the map is
    # deterministic with no hidden state between calls.
    once = snap_phase(0.3, 8)
    assert snap_phase(once, 8) == once     # idempotent: grid point stays put
    assert snap_phase(0.3, 8) == once      # deterministic across calls


def test_snap_phase_rejects_bad_subdivisions():
    with pytest.raises(ValueError):
        snap_phase(0.5, 0)
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python3 -m pytest looper_ref/test_model.py::test_snap_phase_rounds_to_nearest_subdivision -v`
Expected: FAIL with `ImportError: cannot import name 'snap_phase'`

- [ ] **Step 3: Write minimal implementation**

```python
# append to looper_ref/model.py
def snap_phase(phase, subdivisions):
    """Snap normalized phase [0,1) to the nearest 1/subdivisions grid point.
    This is the relative-sync alignment: it moves the loop BOUNDARY to the grid,
    never the recorded audio."""
    if subdivisions < 1:
        raise ValueError("subdivisions must be >= 1")
    return (round(phase * subdivisions) / subdivisions) % 1.0
```

- [ ] **Step 4: Run test to verify it passes**

Run: `python3 -m pytest looper_ref/test_model.py -v`
Expected: PASS (8 tests)

- [ ] **Step 5: Commit**

```bash
git add looper_ref/model.py looper_ref/test_model.py
git commit -m "feat(looper): phase-snap relative sync (reference model)"
```

### Task 3: Per-channel state machine

**Files:**
- Modify: `looper_ref/model.py`
- Test: `looper_ref/test_model.py`

**Interfaces:**
- Produces: enums `State`, `Gesture`, `Action`; dict `TRANSITIONS[(State, Gesture)] -> (State, Action)`; class `Channel` with `.state` and `.feed(gesture) -> Action`.

- [ ] **Step 1: Write the failing test**

```python
# append to looper_ref/test_model.py
from looper_ref.model import State, Gesture, Action, Channel


def test_record_play_overdub_cycle():
    ch = Channel()
    assert ch.state == State.EMPTY
    assert ch.feed(Gesture.TAP) == Action.START_REC
    assert ch.state == State.RECORDING
    assert ch.feed(Gesture.TAP) == Action.SET_LENGTH
    assert ch.state == State.PLAYING
    assert ch.feed(Gesture.HOLD) == Action.START_OVERDUB
    assert ch.state == State.OVERDUBBING
    assert ch.feed(Gesture.TAP) == Action.COMMIT_OVERDUB
    assert ch.state == State.PLAYING


def test_hold_cancels_in_progress_recording():
    ch = Channel()
    ch.feed(Gesture.TAP)                       # -> RECORDING
    assert ch.feed(Gesture.HOLD) == Action.WIPE
    assert ch.state == State.EMPTY


def test_overdub_hold_cancels_layer_session():
    ch = Channel()
    ch.feed(Gesture.TAP); ch.feed(Gesture.TAP)  # -> PLAYING
    ch.feed(Gesture.HOLD)                        # -> OVERDUBBING
    assert ch.feed(Gesture.HOLD) == Action.CANCEL_OVERDUB
    assert ch.state == State.PLAYING


def test_stop_then_play_toggle():
    ch = Channel()
    ch.feed(Gesture.TAP); ch.feed(Gesture.TAP)  # -> PLAYING
    assert ch.feed(Gesture.TAP) == Action.STOP
    assert ch.state == State.STOPPED
    assert ch.feed(Gesture.TAP) == Action.PLAY
    assert ch.state == State.PLAYING


def test_double_clears_from_every_live_state():
    for setup in (
        [Gesture.TAP, Gesture.TAP],                 # PLAYING
        [Gesture.TAP, Gesture.TAP, Gesture.HOLD],   # OVERDUBBING
        [Gesture.TAP, Gesture.TAP, Gesture.TAP],    # STOPPED
    ):
        ch = Channel()
        for g in setup:
            ch.feed(g)
        assert ch.feed(Gesture.DOUBLE) == Action.CLEAR
        assert ch.state == State.EMPTY


def test_transition_table_is_total():
    # every (state, gesture) pair must be defined — no KeyError under any input
    from looper_ref.model import TRANSITIONS
    for s in State:
        for g in Gesture:
            assert (s, g) in TRANSITIONS
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python3 -m pytest looper_ref/test_model.py -k "cycle or cancel or toggle or clears or total" -v`
Expected: FAIL with `ImportError: cannot import name 'State'`

- [ ] **Step 3: Write minimal implementation**

```python
# append to looper_ref/model.py
from enum import IntEnum


class State(IntEnum):
    EMPTY = 0
    RECORDING = 1
    PLAYING = 2
    OVERDUBBING = 3
    STOPPED = 4


class Gesture(IntEnum):
    TAP = 0
    HOLD = 1
    DOUBLE = 2


class Action(IntEnum):
    NONE = 0
    START_REC = 1
    SET_LENGTH = 2
    WIPE = 3
    STOP = 4
    PLAY = 5
    START_OVERDUB = 6
    COMMIT_OVERDUB = 7
    CANCEL_OVERDUB = 8
    CLEAR = 9


TRANSITIONS = {
    (State.EMPTY,       Gesture.TAP):    (State.RECORDING,   Action.START_REC),
    (State.EMPTY,       Gesture.HOLD):   (State.EMPTY,       Action.NONE),
    (State.EMPTY,       Gesture.DOUBLE): (State.EMPTY,       Action.NONE),
    (State.RECORDING,   Gesture.TAP):    (State.PLAYING,     Action.SET_LENGTH),
    (State.RECORDING,   Gesture.HOLD):   (State.EMPTY,       Action.WIPE),
    (State.RECORDING,   Gesture.DOUBLE): (State.EMPTY,       Action.WIPE),
    (State.PLAYING,     Gesture.TAP):    (State.STOPPED,     Action.STOP),
    (State.PLAYING,     Gesture.HOLD):   (State.OVERDUBBING, Action.START_OVERDUB),
    (State.PLAYING,     Gesture.DOUBLE): (State.EMPTY,       Action.CLEAR),
    (State.OVERDUBBING, Gesture.TAP):    (State.PLAYING,     Action.COMMIT_OVERDUB),
    (State.OVERDUBBING, Gesture.HOLD):   (State.PLAYING,     Action.CANCEL_OVERDUB),
    (State.OVERDUBBING, Gesture.DOUBLE): (State.EMPTY,       Action.CLEAR),
    (State.STOPPED,     Gesture.TAP):    (State.PLAYING,     Action.PLAY),
    (State.STOPPED,     Gesture.HOLD):   (State.OVERDUBBING, Action.START_OVERDUB),
    (State.STOPPED,     Gesture.DOUBLE): (State.EMPTY,       Action.CLEAR),
}


class Channel:
    def __init__(self):
        self.state = State.EMPTY

    def feed(self, gesture):
        new_state, action = TRANSITIONS[(self.state, gesture)]
        self.state = new_state
        return action
```

- [ ] **Step 4: Run test to verify it passes**

Run: `python3 -m pytest looper_ref/test_model.py -v`
Expected: PASS (14 tests)

- [ ] **Step 5: Commit**

```bash
git add looper_ref/model.py looper_ref/test_model.py
git commit -m "feat(looper): per-channel state machine (reference model)"
```

### Task 4: 5-channel aggregate + master tracking

**Files:**
- Modify: `looper_ref/model.py`
- Test: `looper_ref/test_model.py`

**Interfaces:**
- Produces: class `LooperModel(n=5)` with `.feed(ch, gesture, recorded_len=None) -> (Action, finalized_len_or_None)`, attributes `.master_len` (None until first loop set) and `.channels`.
- Rule: the first channel to produce `SET_LENGTH` becomes the master and fixes `master_len = recorded_len`; later `SET_LENGTH` returns `quantize_length(recorded_len, master_len)`. `master_len` resets to None only when **all** channels are `EMPTY`.

- [ ] **Step 1: Write the failing test**

```python
# append to looper_ref/test_model.py
from looper_ref.model import LooperModel


def test_first_loop_sets_master_length():
    m = LooperModel()
    m.feed(0, Gesture.TAP)                                  # ch0 record
    action, length = m.feed(0, Gesture.TAP, recorded_len=2.0)
    assert action == Action.SET_LENGTH
    assert length == 2.0
    assert m.master_len == 2.0


def test_second_channel_quantizes_to_master():
    m = LooperModel()
    m.feed(0, Gesture.TAP); m.feed(0, Gesture.TAP, recorded_len=2.0)
    m.feed(1, Gesture.TAP)
    action, length = m.feed(1, Gesture.TAP, recorded_len=1.1)   # cut ~half
    assert length == 2.0      # nearest pow2 ratio of 0.55 is x1 -> 2.0
    m.feed(2, Gesture.TAP)
    _, length3 = m.feed(2, Gesture.TAP, recorded_len=3.9)       # ~2x
    assert length3 == 4.0


def test_master_persists_until_all_channels_empty():
    m = LooperModel()
    m.feed(0, Gesture.TAP); m.feed(0, Gesture.TAP, recorded_len=2.0)
    m.feed(1, Gesture.TAP); m.feed(1, Gesture.TAP, recorded_len=2.0)
    m.feed(0, Gesture.DOUBLE)             # clear the master channel
    assert m.master_len == 2.0           # other loop still on the grid
    m.feed(1, Gesture.DOUBLE)             # clear the last loop
    assert m.master_len is None          # grid resets when everything empty
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python3 -m pytest looper_ref/test_model.py -k master -v`
Expected: FAIL with `ImportError: cannot import name 'LooperModel'`

- [ ] **Step 3: Write minimal implementation**

```python
# append to looper_ref/model.py
class LooperModel:
    def __init__(self, n=5):
        self.channels = [Channel() for _ in range(n)]
        self.master_len = None

    def feed(self, ch, gesture, recorded_len=None):
        action = self.channels[ch].feed(gesture)
        finalized = None
        if action == Action.SET_LENGTH:
            if self.master_len is None:
                self.master_len = recorded_len
                finalized = recorded_len
            else:
                finalized = quantize_length(recorded_len, self.master_len)
        if action in (Action.CLEAR, Action.WIPE):
            if all(c.state == State.EMPTY for c in self.channels):
                self.master_len = None
        return action, finalized
```

- [ ] **Step 4: Run test to verify it passes**

Run: `python3 -m pytest looper_ref/test_model.py -v`
Expected: PASS (17 tests)

- [ ] **Step 5: Commit**

```bash
git add looper_ref/model.py looper_ref/test_model.py
git commit -m "feat(looper): 5-channel aggregate + master tracking (reference model)"
```

---

## Milestone 2 — Csound Looper Engine

Ports the reference model into Csound and adds the DSP. Csound DSP is verified with offline render tests: render a `.csd` test harness with `csound --nosound -W -o <wav>` feeding a known signal via a score, then assert on the WAV with a tiny Python checker. Buffer/timing tuning that surfaces here is expected and handled by these render checks — not deferred.

### Task 5: Looper globals + buffers

**Files:**
- Modify: `csound/globals.orc` (append after line 697, before the `STEP SIZE VARIABLES` block at line 699)

**Interfaces:**
- Produces globals consumed by `looper.orc`, `signal_chain.orc`, `midi_handler.orc`: arrays `gkLOOP_state[]`, `gkLOOP_len[]`, `gkLOOP_speed[]`, `gkLOOP_startPhase[]`, `gkLOOP_pos[]`; scalars `gkLOOP_masterLen`, `gkLooperMix`; buses `gaLooperSendL/R`, `gaLooperOutL/R`; gesture state `gkLOOP_pressT[]`, `gkLOOP_lastShortT[]`; tables `giLOOP_bufN`, `giLOOP_snapN` (N=1..5); consts `giLOOP_holdThresh`, `giLOOP_dblWin`, `giLOOP_len`; speed-stop table `giLOOP_speedTab`.

- [ ] **Step 1: Add the globals block**

```csound
;==============================================================
; LOOPER (5-channel, instr 94) — spec docs/superpowers/specs/
; 2026-06-22-looper-pedal-design.md
;==============================================================
giLOOP_len init 2097152          ; 2^21 ~= 43.7 s per channel @48k (mono)
; per-channel record buffers + overdub-undo snapshots (1-indexed tables)
giLOOP_buf1 ftgen 0, 0, 2097152, 2, 0
giLOOP_buf2 ftgen 0, 0, 2097152, 2, 0
giLOOP_buf3 ftgen 0, 0, 2097152, 2, 0
giLOOP_buf4 ftgen 0, 0, 2097152, 2, 0
giLOOP_buf5 ftgen 0, 0, 2097152, 2, 0
giLOOP_snap1 ftgen 0, 0, 2097152, 2, 0
giLOOP_snap2 ftgen 0, 0, 2097152, 2, 0
giLOOP_snap3 ftgen 0, 0, 2097152, 2, 0
giLOOP_snap4 ftgen 0, 0, 2097152, 2, 0
giLOOP_snap5 ftgen 0, 0, 2097152, 2, 0
; lookup of the 5 buffer / snapshot table numbers, read by instr 94 per channel
giLOOP_bufs  ftgen 0, 0, 8, -2, giLOOP_buf1, giLOOP_buf2, giLOOP_buf3, giLOOP_buf4, giLOOP_buf5
giLOOP_snaps ftgen 0, 0, 8, -2, giLOOP_snap1, giLOOP_snap2, giLOOP_snap3, giLOOP_snap4, giLOOP_snap5

; per-channel state (codes: 0=empty 1=rec 2=play 3=overdub 4=stopped)
gkLOOP_state[] init 5
gkLOOP_len[]   init 5            ; finalized loop length in samples (0 until set)
gkLOOP_speed[] init 5            ; signed varispeed factor; +1 default
gkLOOP_pos[]   init 5            ; read pointer (samples, float)
gkLOOP_wpos[]  init 5            ; write/record pointer (samples, float)
gkLOOP_startPhase[] init 5       ; relative-sync phase offset (0..1)

gkLOOP_masterLen init 0          ; 0 = no master yet (samples)
gkLooperMix init 0.85

; gesture detector state (per channel)
gkLOOP_pressT[]    init 5        ; perf-time when button went down
gkLOOP_held[]      init 5        ; 1 while physically held
gkLOOP_lastShortT[] init 5       ; perf-time of last short release (for double-tap)
giLOOP_holdThresh init 0.30      ; >= this on release => HOLD
giLOOP_dblWin     init 0.28      ; second short within this => DOUBLE

; default speed = +1 on every channel (set once at init via instr 94)
; symmetric speed stops the encoder snaps to: -2 -1 -0.5 -0.25 0.25 0.5 1 2
giLOOP_speedTab ftgen 0, 0, 8, -2, -2, -1, -0.5, -0.25, 0.25, 0.5, 1, 2

; audio buses
gaLooperSendL init 0
gaLooperSendR init 0
gaLooperOutL  init 0
gaLooperOutR  init 0
```

- [ ] **Step 2: Verify the orchestra still compiles**

Run: `cd "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/NEPTR phase4/csound" && csound --syntax-check-only neptrPhase4.csd 2>&1 | tail -20`
Expected: no new errors referencing `globals.orc` (the looper `.orc` is not included yet, so undefined-instr-94 is fine; you are only checking the new ftgen/array lines parse).

- [ ] **Step 3: Commit**

```bash
git add csound/globals.orc
git commit -m "feat(looper): globals, 5 mono buffers, gesture + speed tables"
```

### Task 6: `LoopTransition` UDO (state machine parity)

**Files:**
- Create: `csound/effects/looper.orc`
- Test: `csound/effects/test_looper_transition.csd` (temporary harness; deleted at end of milestone)

**Interfaces:**
- Produces: `kNew, kAction LoopTransition kState, kGesture` — exact parity with Python `TRANSITIONS`.

- [ ] **Step 1: Write the failing render test harness**

```csound
; csound/effects/test_looper_transition.csd
<CsoundSynthesizer>
<CsOptions>
--nosound
</CsOptions>
<CsInstruments>
sr=48000
ksmps=128
nchnls=2
0dbfs=1
#include "looper.orc"
instr 1
  ; EMPTY(0)+TAP(0) -> RECORDING(1), START_REC(1)
  kN, kA LoopTransition 0, 0
  printf "case1 %d %d\n", 1, kN, kA
  ; PLAYING(2)+DOUBLE(2) -> EMPTY(0), CLEAR(9)
  kN2, kA2 LoopTransition 2, 2
  printf "case2 %d %d\n", 1, kN2, kA2
  turnoff
endin
</CsInstruments>
<CsScore>
i1 0 0.01
</CsScore>
</CsoundSynthesizer>
```

- [ ] **Step 2: Run it and confirm it fails (UDO undefined)**

Run: `cd csound/effects && csound test_looper_transition.csd 2>&1 | grep -E "case|error"`
Expected: FAIL — `error: cannot find opcode 'LoopTransition'`

- [ ] **Step 3: Implement the UDO**

```csound
; csound/effects/looper.orc — 5-channel looper engine
; State codes 0..4, gesture 0..2, action 0..9 (see globals.orc / spec).

opcode LoopTransition, kk, kk
  kState, kGest xin
  kNew = kState
  kAct = 0                       ; NONE
  if (kState == 0) then          ; EMPTY
    if (kGest == 0) then
      kNew = 1
      kAct = 1                   ; START_REC
    endif
  elseif (kState == 1) then      ; RECORDING
    if (kGest == 0) then
      kNew = 2
      kAct = 2                   ; SET_LENGTH
    else                         ; HOLD or DOUBLE
      kNew = 0
      kAct = 3                   ; WIPE
    endif
  elseif (kState == 2) then      ; PLAYING
    if (kGest == 0) then
      kNew = 4
      kAct = 4                   ; STOP
    elseif (kGest == 1) then
      kNew = 3
      kAct = 6                   ; START_OVERDUB
    else
      kNew = 0
      kAct = 9                   ; CLEAR
    endif
  elseif (kState == 3) then      ; OVERDUBBING
    if (kGest == 0) then
      kNew = 2
      kAct = 7                   ; COMMIT_OVERDUB
    elseif (kGest == 1) then
      kNew = 2
      kAct = 8                   ; CANCEL_OVERDUB
    else
      kNew = 0
      kAct = 9                   ; CLEAR
    endif
  elseif (kState == 4) then      ; STOPPED
    if (kGest == 0) then
      kNew = 2
      kAct = 5                   ; PLAY
    elseif (kGest == 1) then
      kNew = 3
      kAct = 6                   ; START_OVERDUB
    else
      kNew = 0
      kAct = 9                   ; CLEAR
    endif
  endif
  xout kNew, kAct
endop
```

- [ ] **Step 4: Run the harness and verify parity**

Run: `cd csound/effects && csound test_looper_transition.csd 2>&1 | grep case`
Expected: `case1 1 1` and `case2 0 9`

- [ ] **Step 5: Commit**

```bash
git add csound/effects/looper.orc
git commit -m "feat(looper): LoopTransition UDO matching Python reference table"
```

### Task 7: `LoopChan` UDO — record, varispeed play, overdub (single channel)

**Files:**
- Modify: `csound/effects/looper.orc`
- Test: `csound/effects/test_looper_chan.csd` (temporary)

**Interfaces:**
- Produces: `aOut LoopChan aIn, kCh, ifn, isnap` where `kCh` is the 0-based channel index (reads `gkLOOP_state[kCh]`, `gkLOOP_len[kCh]`, `gkLOOP_speed[kCh]`, `gkLOOP_pos[kCh]`, `gkLOOP_wpos[kCh]`), `ifn`=record buffer table, `isnap`=undo snapshot table. Records on state 1/3, plays (varispeed, signed) on state 2/3, silent on 0/4. Overdub (state 3) sums input into the buffer.

- [ ] **Step 1: Write the failing render test**

Feed a 1.0 s 220 Hz tone while forcing channel 0 into RECORD for 1.0 s then PLAY at speed +1, render 3 s, and confirm the output after record is a non-silent loop. The harness writes WAV; a Python checker asserts RMS of the play region > 0.01 and that the play region repeats (loop period ≈ 1.0 s).

```csound
; csound/effects/test_looper_chan.csd
<CsoundSynthesizer>
<CsOptions>
-W -o test_chan.wav
</CsOptions>
<CsInstruments>
sr=48000
ksmps=128
nchnls=2
0dbfs=1
#include "looper.orc"
instr 1
  ; drive channel 0 by score-set globals (instr 2 sets state)
  ain oscili 0.3, 220
  aL LoopChan ain, 0, giLOOP_buf1, giLOOP_snap1
  outs aL, aL
endin
instr 2  ; state driver: p4 = state code, p5 = speed
  gkLOOP_state[0] = p4
  gkLOOP_speed[0] = p5
  turnoff
endin
</CsInstruments>
<CsScore>
; init speed +1, length unset; record 0..1s, then set length and play
i2 0    0.01 1 1            ; state=RECORDING
i2 1.0  0.01 2 1            ; state=PLAYING  (length captured by engine at switch)
i1 0    3.0
e
</CsScore>
</CsoundSynthesizer>
```

```python
# csound/effects/check_chan.py
import sys, wave, struct
w = wave.open("test_chan.wav", "rb")
n = w.getnframes(); sr = w.getframerate()
data = struct.unpack("<%dh" % (n * w.getnchannels()), w.readframes(n))
left = [data[i] / 32768.0 for i in range(0, len(data), w.getnchannels())]
def rms(a, b):
    seg = left[int(a*sr):int(b*sr)]
    return (sum(x*x for x in seg) / max(1, len(seg))) ** 0.5
play_rms = rms(1.2, 2.8)
print("play_rms", round(play_rms, 4))
sys.exit(0 if play_rms > 0.01 else 1)
```

- [ ] **Step 2: Run and confirm failure (UDO undefined → no/empty WAV)**

Run: `cd csound/effects && csound test_looper_chan.csd 2>&1 | grep -iE "error|LoopChan"`
Expected: FAIL — `cannot find opcode 'LoopChan'`

- [ ] **Step 3: Implement `LoopChan`**

```csound
; append to csound/effects/looper.orc
; Single-channel record / varispeed-play / overdub. Per-sample loop so the
; read pointer can wrap at an arbitrary loop length (Lk need not be 2^n) and
; run backwards for reverse. Pure linear-interp varispeed read (table3-grade
; cubic optional — start linear, upgrade in Task tuning if zipper appears).
opcode LoopChan, a, aiii
  ain, iCh, ifn, isnap xin
  kstate = gkLOOP_state[iCh]
  kspeed = gkLOOP_speed[iCh]
  klen   = gkLOOP_len[iCh]
  kw     = gkLOOP_wpos[iCh]
  kr     = gkLOOP_pos[iCh]
  aout init 0
  ksmpl = 0
  while ksmpl < ksmps do
    kin = ain[ksmpl]
    ko = 0
    if (kstate == 1) then                 ; RECORDING: write, advance write head
      tablew kin, kw, ifn
      kw = kw + 1
      if (kw >= giLOOP_len) then
        kw = giLOOP_len - 1               ; clamp at buffer end (cap ~43.7s)
      endif
    elseif (kstate == 2 || kstate == 3) then   ; PLAYING / OVERDUBBING
      if (klen > 0) then
        ; linear-interp read at fractional kr
        ki0 = int(kr)
        kfrac = kr - ki0
        ki1 = ki0 + 1
        if (ki1 >= klen) then
          ki1 = 0
        endif
        ks0 table ki0, ifn
        ks1 table ki1, ifn
        ko = ks0 + (ks1 - ks0) * kfrac
        if (kstate == 3) then             ; OVERDUB: sum input into buffer at read pos
          tablew ks0 + kin, ki0, ifn
        endif
        kr = kr + kspeed
        if (kr >= klen) then
          kr = kr - klen
        elseif (kr < 0) then
          kr = kr + klen                  ; reverse wrap
        endif
      endif
    endif
    aout[ksmpl] = ko
    ksmpl = ksmpl + 1
  od
  gkLOOP_wpos[iCh] = kw
  gkLOOP_pos[iCh]  = kr
  xout aout
endop
```

> Note: the RECORDING→PLAYING length capture (setting `gkLOOP_len[iCh]` from `kw`) is done by the gesture handler in Task 12 (`SET_LENGTH`), not here — this UDO only consumes `gkLOOP_len`. For this isolated test, add a one-line score-driven length set in instr 2 at t=1.0 (`gkLOOP_len[0] = 48000`) so the play region has a length.

Add to instr 2 in the harness: `if (p4 == 2) then \n gkLOOP_len[0] = 48000 \n gkLOOP_pos[0] = 0 \n endif`.

- [ ] **Step 4: Render and verify the loop plays**

Run: `cd csound/effects && csound test_looper_chan.csd && python3 check_chan.py`
Expected: prints `play_rms <>0.01` and exits 0.

- [ ] **Step 5: Commit**

```bash
git add csound/effects/looper.orc
git commit -m "feat(looper): LoopChan record/varispeed-play/overdub UDO"
```

### Task 8: Reverse + speed correctness render test

**Files:**
- Test: `csound/effects/test_looper_chan.csd` (extend), `csound/effects/check_chan.py` (extend)

**Interfaces:**
- Consumes: `LoopChan`, `gkLOOP_speed[]`. No new production code expected unless the test surfaces a bug.

- [ ] **Step 1: Add a reverse + double-speed assertion**

Render two extra passes from the same recorded buffer: speed `-1` (reverse) and `+2` (double). Assert: reverse region RMS > 0.01 (audio present, runs backward) and the `+2` region's dominant period is half the `+1` period (zero-cross count ≈ 2×). Extend `check_chan.py` with a zero-crossing counter over a 0.5 s window for each region.

```python
# append to check_chan.py
def zcr(a, b):
    seg = left[int(a*sr):int(b*sr)]
    return sum(1 for i in range(1, len(seg)) if seg[i-1] < 0 <= seg[i])
# +1 region 1.2-1.7, +2 region must have ~2x crossings
# (wire the score so the +2 pass renders at 4.x s; assert zcr_2x ~ 2*zcr_1x +/-15%)
```

- [ ] **Step 2: Run to verify reverse/speed behave**

Run: `cd csound/effects && csound test_looper_chan.csd && python3 check_chan.py`
Expected: exit 0; printed zero-cross ratio within 1.85–2.15.

- [ ] **Step 3: If zipper/aliasing appears at ¼×/2×, upgrade interpolation**

Only if audible/measurable: replace the two-point linear read in `LoopChan` with a 4-point cubic (Hermite) read using `km1,k0,k1,k2` samples (same pattern as the `OS4*` UDOs in `globals.orc:1170-1194`). Re-run Step 2.

- [ ] **Step 4: Commit**

```bash
git add csound/effects/looper.orc csound/effects/check_chan.py csound/effects/test_looper_chan.csd
git commit -m "test(looper): verify reverse and symmetric varispeed playback"
```

### Task 9: Engine instr 94 — 5 channels + master phase + OSC

**Files:**
- Modify: `csound/effects/looper.orc`

**Interfaces:**
- Produces: always-on `instr 94` that (a) one-time inits all `gkLOOP_speed=1`, (b) sums `gaLooperSend` mono = `(gaLooperSendL+gaLooperSendR)*0.5`, (c) calls `LoopChan` for channels 0–4, (d) sums their output into `gaLooperOutL/R` scaled by `gkLooperMix`, (e) advances a master phase counter `gkLOOP_masterPhase` (0..1) when `gkLOOP_masterLen>0`, (f) emits per-channel OSC (`/loop_chN_state`, `/loop_chN_pos`, `/loop_chN_speed`, `/loop_chN_ratio`).

- [ ] **Step 1: Implement instr 94**

```csound
; append to csound/effects/looper.orc
gkLOOP_masterPhase init 0
gkLOOP_didInit init 0

instr 94
  if (gkLOOP_didInit == 0) then
    kc = 0
    while kc < 5 do
      gkLOOP_speed[kc] = 1
      kc = kc + 1
    od
    gkLOOP_didInit = 1
  endif

  aMono = (gaLooperSendL + gaLooperSendR) * 0.5
  a0 LoopChan aMono, 0, giLOOP_buf1, giLOOP_snap1
  a1 LoopChan aMono, 1, giLOOP_buf2, giLOOP_snap2
  a2 LoopChan aMono, 2, giLOOP_buf3, giLOOP_snap3
  a3 LoopChan aMono, 3, giLOOP_buf4, giLOOP_snap4
  a4 LoopChan aMono, 4, giLOOP_buf5, giLOOP_snap5
  aSum = (a0 + a1 + a2 + a3 + a4) * gkLooperMix
  gaLooperOutL = aSum
  gaLooperOutR = aSum

  ; master phase for the playhead + relative-sync grid
  if (gkLOOP_masterLen > 0) then
    gkLOOP_masterPhase = gkLOOP_masterPhase + (ksmps / gkLOOP_masterLen)
    gkLOOP_masterPhase = gkLOOP_masterPhase % 1
  endif

  ; OSC feedback (state on change; pos throttled to ~30 Hz via metro)
  kc = 0
  kosc metro 30
  while kc < 5 do
    kst = gkLOOP_state[kc]
    kstChg changed kst
    if (kstChg == 1) then
      Sst sprintf "/loop_ch%d_state", kc
      OSC3x_i Sst, kst
      Ssp sprintf "/loop_ch%d_speed", kc
      OSC3x_f Ssp, gkLOOP_speed[kc]
      Srt sprintf "/loop_ch%d_ratio", kc
      kratio = (gkLOOP_masterLen > 0 ? gkLOOP_len[kc] / gkLOOP_masterLen : 1)
      OSC3x_f Srt, kratio
    endif
    if (kosc == 1) then
      kpos = (gkLOOP_len[kc] > 0 ? gkLOOP_pos[kc] / gkLOOP_len[kc] : 0)
      Spo sprintf "/loop_ch%d_pos", kc
      OSC3x_f Spo, kpos
    endif
    kc = kc + 1
  od
endin
```

- [ ] **Step 2: Syntax-check the engine in isolation**

Run: `cd csound/effects && csound --syntax-check-only test_looper_chan.csd 2>&1 | tail -5`
Expected: no errors (instr 94 + UDOs parse).

- [ ] **Step 3: Delete temporary harnesses, keep the checker pattern noted**

```bash
rm csound/effects/test_looper_transition.csd csound/effects/test_looper_chan.csd csound/effects/test_chan.wav
git add -A csound/effects/
git commit -m "feat(looper): instr 94 — 5-channel engine, master phase, OSC feedback"
```

---

## Milestone 3 — Csound Integration (chain + MIDI)

### Task 10: Wire engine into the signal chain

**Files:**
- Modify: `csound/neptrPhase4.csd` (include line + score)
- Modify: `csound/signal_chain.orc` (tap + sum-back + clear)

**Interfaces:**
- Consumes: `gaLooperSendL/R`, `gaLooperOutL/R` from Task 5/9. Produces: looper records the processed signal and its playback reaches `outs`.

- [ ] **Step 1: Add the include and score line**

In `csound/neptrPhase4.csd`, after line 51 (`#include "effects/stutter.orc"`), add:

```csound
#include "effects/looper.orc"
```

In the `<CsScore>` block (after `i1  0 z`), add:

```csound
i94 0 z
```

- [ ] **Step 2: Tap the processed signal into the looper send (record input)**

In `csound/signal_chain.orc`, immediately before the NAM input capture (line 352, `aNAM_inL = aPrevL`), add:

```csound
  ; --- LOOPER record tap: capture fully-processed signal pre-NAM ---
  gaLooperSendL = aPrevL
  gaLooperSendR = aPrevR
```

- [ ] **Step 3: Sum looper playback into the final mix**

In `csound/signal_chain.orc`, after the NAM blend (line 361) and before `outs` (line 363), add:

```csound
  ; --- LOOPER playback sum into final stereo mix ---
  aPrevL = aPrevL + gaLooperOutL
  aPrevR = aPrevR + gaLooperOutR
```

- [ ] **Step 4: Clear the looper out buses at end of instr 1**

At the very end of instr 1 (after `outs`, with the other bus clears), add:

```csound
  gaLooperOutL = 0
  gaLooperOutR = 0
```

- [ ] **Step 5: Full compile + smoke run**

Run: `cd csound && csound --syntax-check-only neptrPhase4.csd 2>&1 | tail -10`
Expected: compiles with no errors. (Live audio is verified on the Pi later.)

- [ ] **Step 6: Commit**

```bash
git add csound/neptrPhase4.csd csound/signal_chain.orc
git commit -m "feat(looper): tap record signal + sum playback in signal chain"
```

### Task 11: Gesture detector + channel apply UDO

**Files:**
- Modify: `csound/effects/looper.orc`

**Interfaces:**
- Produces: `LoopButton iCh, kVal` — call on a slot's button CC; classifies press/hold/short/double using `gkLOOP_pressT/held/lastShortT[]` and `giLOOP_holdThresh/dblWin`, runs `LoopTransition`, and applies the resulting Action (state change + length capture + overdub snapshot/restore + pointer resets). Acts immediately; a second short within `giLOOP_dblWin` yields DOUBLE (clear), superseding the prior tap.

- [ ] **Step 1: Implement the button handler**

```csound
; append to csound/effects/looper.orc
; Apply an Action's side effects to channel iCh.
opcode LoopApply, 0, ki
  kAct, iCh xin
  if (kAct == 1) then               ; START_REC: reset write head
    gkLOOP_wpos[iCh] = 0
  elseif (kAct == 2) then           ; SET_LENGTH: capture recorded length
    krec = gkLOOP_wpos[iCh]
    if (gkLOOP_masterLen <= 0) then
      gkLOOP_masterLen = krec       ; this channel is the master
      gkLOOP_len[iCh] = krec
    else
      kratio nearestpow2 krec / gkLOOP_masterLen
      gkLOOP_len[iCh] = gkLOOP_masterLen * kratio
    endif
    gkLOOP_pos[iCh] = 0
  elseif (kAct == 3 || kAct == 9) then   ; WIPE / CLEAR
    gkLOOP_len[iCh] = 0
    gkLOOP_pos[iCh] = 0
    gkLOOP_wpos[iCh] = 0
    ; if all channels empty, drop the master grid
    ksum = gkLOOP_state[0]+gkLOOP_state[1]+gkLOOP_state[2]+gkLOOP_state[3]+gkLOOP_state[4]
    if (ksum == 0) then
      gkLOOP_masterLen = 0
    endif
  elseif (kAct == 6) then           ; START_OVERDUB: snapshot buffer for undo
    LoopSnapshot iCh, 1
    gkLOOP_pos[iCh] = 0
  elseif (kAct == 8) then           ; CANCEL_OVERDUB: restore snapshot
    LoopSnapshot iCh, 0
  elseif (kAct == 5) then           ; PLAY (from stopped): re-align to grid
    gkLOOP_pos[iCh] = 0
  endif
endop

; nearest power-of-two ratio, clamped 2^-2..2^3 (mirrors Python nearest_pow2_ratio)
opcode nearestpow2, k, k
  kx xin
  kn = int(floor(log(kx)/log(2) + 0.5))
  kn = (kn < -2 ? -2 : (kn > 3 ? 3 : kn))
  xout pow(2, kn)
endop

; copy record buffer <-> snapshot (idir 1 = save, 0 = restore)
opcode LoopSnapshot, 0, ki
  kStub, iCh xin
  ; resolve table numbers from the lookup tables
  ibuf table iCh, giLOOP_bufs
  isnap table iCh, giLOOP_snaps
  ; (implemented as a one-shot copy instrument to avoid a 2M-sample k-loop;
  ;  schedule instr 95 with p4=src p5=dst)
  if (kStub == 1) then
    event "i", 95, 0, 0.001, ibuf, isnap
  else
    event "i", 95, 0, 0.001, isnap, ibuf
  endif
endop

instr 95   ; buffer copy helper: p4 = src ftable, p5 = dst ftable
  isrc = p4
  idst = p5
  icopy tablecopy idst, isrc
  turnoff
endin

; Button handler — classify gesture, transition, apply.
opcode LoopButton, 0, ki
  kVal, iCh xin
  know times
  if (kVal == 127) then                         ; PRESS
    gkLOOP_pressT[iCh] = know
    gkLOOP_held[iCh] = 1
  elseif (kVal == 0 && gkLOOP_held[iCh] == 1) then   ; RELEASE
    gkLOOP_held[iCh] = 0
    kdt = know - gkLOOP_pressT[iCh]
    kgest = 0                                   ; default TAP
    if (kdt >= giLOOP_holdThresh) then
      kgest = 1                                 ; HOLD
    elseif (know - gkLOOP_lastShortT[iCh] < giLOOP_dblWin) then
      kgest = 2                                 ; DOUBLE (2nd short in window)
    endif
    if (kgest != 1) then
      gkLOOP_lastShortT[iCh] = know
    endif
    kNew, kAct LoopTransition gkLOOP_state[iCh], kgest
    gkLOOP_state[iCh] = kNew
    LoopApply kAct, iCh
  endif
endop
```

> `tablecopy` and `tablew`/`table` are stock Csound opcodes. `iCh` reaches `LoopApply`/`LoopSnapshot` as an i-arg because the caller (Task 12) passes a constant channel index per slot.

- [ ] **Step 2: Syntax-check**

Run: `cd csound && csound --syntax-check-only neptrPhase4.csd 2>&1 | tail -10`
Expected: compiles clean.

- [ ] **Step 3: Commit**

```bash
git add csound/effects/looper.orc
git commit -m "feat(looper): gesture detector, length capture, overdub undo, clear"
```

### Task 12: Menu-9 button dispatch + menu count

**Files:**
- Modify: `csound/midi_handler.orc`

**Interfaces:**
- Consumes: `LoopButton`. Maps slot buttons (CC74/75/76/71/72) to channels 0–4 when `gkCurrentMenu == 9`.

- [ ] **Step 1: Extend the menu wrap to 10**

In `csound/midi_handler.orc` line 166 change `% 9` → `% 10` and the `+ 9` offset → `+ 10`:

```csound
  gkCurrentMenu = (gkCurrentMenu - 1 + 10) % 10
```

and line 170:

```csound
  gkCurrentMenu = (gkCurrentMenu + 1) % 10
```

- [ ] **Step 2: Add the menu-9 button block**

Find the button-CC dispatch ladder (the `if (gkCurrentMenu == N)` chain that handles CC74/75/76/71/72 presses, e.g. the GLITCH block near line 288). Add a new branch that routes each slot's button CC — for BOTH press (127) and release (0) — to `LoopButton`. Channel order: rightmost slot 0 = CC74 → channel 0 … leftmost slot 4 = CC72 → channel 4:

```csound
  elseif (gkCurrentMenu == 9) then
    ; LOOPER: 5 push-encoders = 5 loop channels. Button = transport.
    if (kCC == 74) then
      LoopButton kVal, 0
    elseif (kCC == 75) then
      LoopButton kVal, 1
    elseif (kCC == 76) then
      LoopButton kVal, 2
    elseif (kCC == 71) then
      LoopButton kVal, 3
    elseif (kCC == 72) then
      LoopButton kVal, 4
    endif
```

> Important: looper buttons need the **release** event (`kVal == 0`), which most effect toggles ignore. Ensure this branch is reached for releases too — if the surrounding ladder is gated on `kVal == 127`, add this menu-9 block in the part of the handler that runs for every CC value (i.e. before/outside the `kVal == 127` gate), or duplicate the guard so releases reach `LoopButton`.

- [ ] **Step 3: Syntax-check**

Run: `cd csound && csound --syntax-check-only neptrPhase4.csd 2>&1 | tail -10`
Expected: clean compile.

- [ ] **Step 4: Commit**

```bash
git add csound/midi_handler.orc
git commit -m "feat(looper): menu-9 transport dispatch + extend menu wrap to 10"
```

### Task 13: Encoder speed map (symmetric varispeed)

**Files:**
- Modify: `csound/effects/looper.orc` (add `LoopEncoder` UDO)
- Modify: `csound/midi_handler.orc` (menu-9 rotation block)

**Interfaces:**
- Produces: `LoopEncoder iCh, kVal` — steps the channel's speed along `giLOOP_speedTab` (`-2 -1 -0.5 -0.25 0.25 0.5 1 2`). CW (`kVal>64`) moves toward index 7 (+2), CCW toward index 0 (−2). Default speed `+1` = index 6.

- [ ] **Step 1: Implement `LoopEncoder`**

```csound
; append to csound/effects/looper.orc
gkLOOP_spIdx[] init 5      ; current index into giLOOP_speedTab (default 6 = +1)
gkLOOP_spInit init 0

opcode LoopEncoder, 0, ki
  kVal, iCh xin
  if (gkLOOP_spInit == 0) then
    ki = 0
    while ki < 5 do
      gkLOOP_spIdx[ki] = 6     ; +1
      ki = ki + 1
    od
    gkLOOP_spInit = 1
  endif
  kidx = gkLOOP_spIdx[iCh]
  if (kVal > 64) then
    kidx = (kidx < 7 ? kidx + 1 : 7)
  elseif (kVal < 64) then
    kidx = (kidx > 0 ? kidx - 1 : 0)
  endif
  gkLOOP_spIdx[iCh] = kidx
  kspeed table kidx, giLOOP_speedTab
  gkLOOP_speed[iCh] = kspeed
endop
```

- [ ] **Step 2: Add the menu-9 rotation block**

Find the encoder-rotation dispatch ladder (the `if (gkCurrentMenu == N)` chain handling CC24/25/26/21/22, e.g. DRIVE block near line 819). Add:

```csound
  elseif (gkCurrentMenu == 9) then
    ; LOOPER per-channel speed/reverse
    if (kCC == 24) then
      LoopEncoder kVal, 0
    elseif (kCC == 25) then
      LoopEncoder kVal, 1
    elseif (kCC == 26) then
      LoopEncoder kVal, 2
    elseif (kCC == 21) then
      LoopEncoder kVal, 3
    elseif (kCC == 22) then
      LoopEncoder kVal, 4
    endif
```

- [ ] **Step 3: Syntax-check**

Run: `cd csound && csound --syntax-check-only neptrPhase4.csd 2>&1 | tail -10`
Expected: clean compile.

- [ ] **Step 4: Commit**

```bash
git add csound/effects/looper.orc csound/midi_handler.orc
git commit -m "feat(looper): symmetric varispeed encoder map per channel"
```

---

## Milestone 4 — UI (pygame OSC display)

### Task 14: Register the LOOPER menu + route OSC

**Files:**
- Modify: `neptrPhase4_UI.py` (`MENUS` list line 100-110; `route_osc` line 866-880)

**Interfaces:**
- Produces: menu index 9 named `LOOPER`; `route_osc` stores `/loop_chN_{state,pos,speed,ratio}` into `ui.param_values`.

- [ ] **Step 1: Add the menu entry**

Append to `MENUS` (after the NAM entry, line 109). Use 5 pseudo "effect" codes so the existing menu plumbing accepts it, but the renderer (Task 15) special-cases menu 9:

```python
    {"name": "LOOPER", "effects": [("lp0", "CH 1"), ("lp1", "CH 2"), ("lp2", "CH 3"), ("lp3", "CH 4"), ("lp4", "CH 5")]},
```

- [ ] **Step 2: Route looper OSC into param_values**

In `route_osc` (line 866), add a branch before the generic `_`-split handler:

```python
    elif addr.startswith('/loop_ch'):
        ui._touch()
        ui.param_values[addr] = val
        if ui.current_view == 'menu9':
            ui.dirty = True
        return
```

- [ ] **Step 3: Run the UI headless-ish to confirm it starts**

Run: `NEPTR_WINDOWED=1 python3 neptrPhase4_UI.py & sleep 2; kill %1`
Expected: starts without exception; logs `OSC listening on 127.0.0.1:8000`.

- [ ] **Step 4: Commit**

```bash
git add neptrPhase4_UI.py
git commit -m "feat(looper-ui): register LOOPER menu and route /loop_ch* OSC"
```

### Task 15: Channel-strip renderer with playheads

**Files:**
- Modify: `neptrPhase4_UI.py` (`render` dispatch line 492-495; `_signature` line 435-451; new `_render_looper`)

**Interfaces:**
- Consumes: `ui.param_values['/loop_chN_state'|'_pos'|'_speed'|'_ratio']`. Produces: `_render_looper(c)` drawing 5 vertical strips: state color, playhead progress, speed/reverse label, ratio.

- [ ] **Step 1: Add the signature case (so repaints fire on looper changes)**

In `_signature` (line 437), before the `view.startswith('menu')` generic case, add:

```python
        if view == 'menu9':
            vals = tuple(
                self.param_values.get(f'/loop_ch{n}_{k}', 0)
                for n in range(5) for k in ('state', 'pos', 'speed', 'ratio')
            )
            return ('looper', vals)
```

- [ ] **Step 2: Dispatch menu 9 to the looper renderer**

In `render` (line 492), change the menu branch to special-case 9:

```python
        if view == 'menu9':
            self._render_looper(c)
        elif view.startswith('menu'):
            self._render_menu(c, int(view[4:]))
        elif view in EFFECT_PARAMS:
            self._render_effect(c, view)
```

- [ ] **Step 3: Implement `_render_looper`**

```python
    # add to NeptrUI
    LOOP_STATE_COLORS = {
        0: (55, 25, 62),     # EMPTY  -> EMPTY_OUTLINE
        1: (245, 44, 90),    # RECORDING -> red
        2: (138, 245, 120),  # PLAYING -> green
        3: (245, 200, 44),   # OVERDUBBING -> amber
        4: (113, 64, 117),   # STOPPED -> dim purple
    }
    LOOP_STATE_NAMES = {0: "", 1: "REC", 2: "PLAY", 3: "DUB", 4: "STOP"}

    def _render_looper(self, c):
        n = 5
        col_spacing = 14
        col_w = (W - 2 * PAD_X - (n - 1) * col_spacing) / n
        track_top = CONTENT_TOP + 8
        track_bottom = CONTENT_BOTTOM - 150
        for i in range(n):
            x0 = PAD_X + i * (col_w + col_spacing)
            st = int(self.param_values.get(f'/loop_ch{i}_state', 0))
            pos = float(self.param_values.get(f'/loop_ch{i}_pos', 0.0))
            speed = float(self.param_values.get(f'/loop_ch{i}_speed', 1.0))
            ratio = float(self.param_values.get(f'/loop_ch{i}_ratio', 1.0))
            color = self.LOOP_STATE_COLORS.get(st, EMPTY_OUTLINE)

            tr = pygame.Rect(int(x0 + 12), track_top,
                             int(col_w - 24), track_bottom - track_top)
            pygame.draw.rect(c, color, tr, 3)
            # playhead: filled bar from bottom rising with loop position
            if st in (2, 3) and pos > 0:
                ph = int((tr.height - 8) * max(0.0, min(1.0, pos)))
                fr = pygame.Rect(tr.x + 4, tr.bottom - 4 - ph, tr.width - 8, ph)
                pygame.draw.rect(c, color, fr)

            cx = x0 + col_w / 2
            blit_centered(c, text_fit(self.LOOP_STATE_NAMES.get(st, ""), 40, color,
                                      col_w - 8), cx, CONTENT_BOTTOM - 124)
            # speed / reverse label, e.g. "2x", "1/2x", "<1x" for reverse
            if speed < 0:
                slabel = f"<{abs(speed):g}x"
            else:
                slabel = f"{speed:g}x"
            blit_centered(c, text_fit(slabel, 50, FG, col_w - 8),
                          cx, CONTENT_BOTTOM - 80)
            rlabel = f"x{ratio:g}" if ratio >= 1 else f"/{1/ratio:g}"
            blit_centered(c, text_fit(rlabel, 36, OFF_TEXT, col_w - 8),
                          cx, CONTENT_BOTTOM - 40)
```

- [ ] **Step 4: Visual smoke test with synthetic OSC**

Run a 6-line throwaway script (in scratchpad) that opens a UDP socket to `127.0.0.1:8000` and sends `/menu 9`, then `/loop_ch0_state 2`, `/loop_ch0_pos 0.5`, `/loop_ch0_speed -1`, `/loop_ch0_ratio 2` while the windowed UI runs; confirm channel 1 shows green PLAY, a half-filled playhead, `<1x`, `x2`.

Run: `NEPTR_WINDOWED=1 python3 neptrPhase4_UI.py` (in one shell) + the sender script (in another).
Expected: strip 1 renders PLAY/green, half playhead, `<1x`, `x2`.

- [ ] **Step 5: Commit**

```bash
git add neptrPhase4_UI.py
git commit -m "feat(looper-ui): 5-channel strip renderer with live playheads"
```

---

## Final Verification (on the Pi)

Not a code task — the hardware acceptance gate the spec §8 requires.

- [ ] Deploy per the NEPTR deploy checklist; `redem` to restart demiurge; restart the UI.
- [ ] Navigate to the LOOPER menu (it is now the 10th menu). Record channel 1 (tap), confirm it loops; tap to stop/start; hold to overdub a layer; hold again to cancel the layer; double-tap to clear.
- [ ] Record channel 2 off the beat; confirm it phase-locks to channel 1 in a clean fraction with no audible time-shift of your playing.
- [ ] Twist each encoder through `+2× +1× +½× +¼× ‖ −¼× −½× −1× −2×`; confirm clean varispeed (no zipper/aliasing) and correct reverse.
- [ ] Verify CPU headroom and memory with all 5 channels recorded + an active overdub snapshot; if tight, drop `giLOOP_len` from `2097152` (2^21) to `1048576` (2^20) in `globals.orc` and the matching `ftgen` sizes.
- [ ] Confirm loops keep playing when you navigate to other effect menus and back.

---

## Adversarial Logic Review (per user directive)

Before the Pi gate, dispatch independent agents to refute the logic:
- One agent re-derives the `LoopTransition` ladder from the spec §2 table and diffs it against both `looper_ref/model.py::TRANSITIONS` and `looper.orc::LoopTransition`, reporting any mismatch.
- One agent checks the quantization parity: `nearestpow2` (Csound) vs `nearest_pow2_ratio` (Python) across ratios 0.1–10.
- One agent audits the double-tap/hold timing in `LoopButton` for the "act immediately, double supersedes" contract and the master-reset-when-all-empty rule.
Fix any divergence before deploying.
