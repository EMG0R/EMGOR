# CC Remap & Effects Overhaul Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remap all MIDI CC assignments, restructure menus from 7 to 6, add WAH effect, split stutter into 4 glitch effects, fix vibrato clicking, and implement CC29 sticky dispatch.

**Architecture:** CC swap (22<>21, 72<>71) frees enc3 for new effects. Menu merge (TIME+TEXTURE) and glitch split create 6 balanced menus. New gkLastToggle29 tracker implements sticky CC29 behavior.

**Tech Stack:** Csound 6.x (`.orc`/`.csd`), Python/Kivy UI

---

## File Structure

| File | Action | Responsibility |
|------|--------|---------------|
| `csound/globals.orc` | Modify | Add WAH, glitch, vibrato feedback globals + buses |
| `csound/effects/vibrato.orc` | Modify | Fix clicking (portk), add feedback path |
| `csound/effects/wah.orc` | Create | Manual wah pedal (moogladder) |
| `csound/effects/stutter.orc` | Rewrite | Split into 4 independent glitch instruments |
| `csound/signal_chain.orc` | Modify | Add WAH slot, replace stutter with 4 glitches |
| `csound/midi_handler.orc` | Rewrite | CC swap, 6-menu system, all remappings, CC29 sticky |
| `neptrPhase4_UI.py` | Modify | 6-menu structure, new effect params |

## Menu Layout (Post-Change)

```
0: VOLUME  s0=Vol(74/24) s1=RM(75/25) s2=Leslie(76/26) s3=Attk(71/21) s4=Drop(72/22)
1: PITCH   s0=PS(74/24)  s1=GPS(75/25) s2=Fmt(76/26)
2: MOD     s0=Vib(74/24) s1=Cho(75/25) s2=Pha(76/26) s4=Fla(72/22)
3: DRIVE   s0=Dist(74/24) s1=TS(75/25) s2=Auto(76/26) s4=WAH(72/22)
4: TIME    s0=Dly(74/24) s1=Gran(75/25) s2=Spec(76/26) s3=Vrb(71/21)
5: GLITCH  s0=MOOD(74/24) s1=BBcut(75/25) s2=WGde(76/26) s3=SqMod(71/21) s4=FShift(72/22)
```

## CC Mapping Reference

**Accumulators (per-menu rotation = amount for that slot's effect):**
- CC24: Vol(m0), PS semi(m1), Vib depth(m2), Dist amt(m3), Dly mix(m4), MOOD mix(m5)
- CC25: RM freq(m0), GPS semi(m1), Cho mix(m2), TS drive(m3), Gran mix(m4), BBcut drywet(m5)
- CC26: Leslie depth(m0), Fmt shift(m1), Pha depth(m2), Auto sens(m3), Spec mix(m4), WGde drywet(m5)
- CC21: Attk sens(m0), Vrb mix(m4), SqMod drywet(m5)
- CC22: Drop amt(m0), Fla depth(m2), FShift drywet(m5)

**CC20 (lastToggle dispatch):**
2=Attk hold, 3=PS dry%, 4=GPS mix, 5=Fmt mix, 6=Vib freq, 7=Cho rate, 8=Pha rate, 9=Fla rate, 11=RM depth, 14=Auto res, 15=Dly time, 16=Spec maxdel, 17=MOOD clock, 19=Vrb room, glitch group=BPM

**CC23 (lastToggle dispatch):**
4=GPS fback, 6=Vib fback, 7=Cho depth, 8=Pha stages, 9=Fla fback, 15=Dly fback, 16=Spec FB, 17=MOOD dens

**CC29 (lastToggle29 sticky dispatch):**
1=Vol, 2=Attk sens, 3=PS semi, 4=GPS semi, 5=Fmt shift, 8=Pha fback, 10=Leslie speed, 11=RM freq, 12=Dist amt, 13=TS drive, 15=Dly time, 22(WAH)=freq, glitch group=probability

---

### Task 1: CC Swap + New Globals

**Files:**
- Modify: `csound/globals.orc`
- Modify: `csound/midi_handler.orc` (CC swap only)

- [ ] **Step 1:** In midi_handler.orc, rename `kCC == 72` to `kCC == 71` and `kCC == 22` to `kCC == 21` in existing handlers. Update header comments.
- [ ] **Step 2:** Add to globals.orc: WAH variables, glitch shared vars, individual glitch buses, vibrato feedback, gkLastToggle29
- [ ] **Step 3:** Commit: "refactor: CC swap 22<>21, 72<>71 + add new globals"

### Task 2: Vibrato Fix + Feedback

**Files:**
- Modify: `csound/effects/vibrato.orc`

- [ ] **Step 1:** Add portk smoothing on depth to eliminate clicking
- [ ] **Step 2:** Add feedback path with gkVIB_fback (safe range, default 0)
- [ ] **Step 3:** Commit: "fix: vibrato clicking on depth change + add optional feedback"

### Task 3: WAH Effect

**Files:**
- Create: `csound/effects/wah.orc`

- [ ] **Step 1:** Create wah.orc with moogladder filter, CC29-driven frequency sweep (200-5000Hz), resonance 0.7
- [ ] **Step 2:** Commit: "feat: add manual wah pedal effect"

### Task 4: Split Stutter into 4 Glitch Effects

**Files:**
- Rewrite: `csound/effects/stutter.orc`

- [ ] **Step 1:** Rewrite stutter.orc with 4 independent instruments: BBcut(90), WGuide(91), SqMod(92), FreqShift(93). Each reads shared gkGLITCH_bpm/chance, has own bypass and drywet.
- [ ] **Step 2:** Commit: "refactor: split stutter into 4 independent glitch effects"

### Task 5: Signal Chain Update

**Files:**
- Modify: `csound/signal_chain.orc`

- [ ] **Step 1:** Add WAH (instr 52) after AutoMoog, before volume pedal
- [ ] **Step 2:** Replace single stutter slot with 4 glitch effect slots (BBcut, WGuide, SqMod, FreqShift)
- [ ] **Step 3:** Add WAH + glitch bus clears
- [ ] **Step 4:** Commit: "feat: signal chain - add WAH, split glitch effects"

### Task 6: MIDI Handler Overhaul

**Files:**
- Rewrite: `csound/midi_handler.orc`

- [ ] **Step 1:** Rewrite toggle sections (CC74/75/76/71/72) for 6-menu layout
- [ ] **Step 2:** Rewrite accumulator rotations (CC24/25/26/21/22) per menu mapping table
- [ ] **Step 3:** Rewrite CC20/CC23 lastToggle dispatch with glitch group handling
- [ ] **Step 4:** Rewrite CC29 dispatch with gkLastToggle29 sticky behavior + glitch group
- [ ] **Step 5:** Update FX_TOGGLE calls with new toggle IDs (WAH=22, WGuide=23, SqMod=24, FShift=25)
- [ ] **Step 6:** Update OSC feedback section for new effects + removed effects
- [ ] **Step 7:** Update header comments
- [ ] **Step 8:** Commit: "feat: midi handler - 6-menu system, CC remapping, CC29 sticky dispatch"

### Task 7: UI Update

**Files:**
- Modify: `neptrPhase4_UI.py`

- [ ] **Step 1:** Update MENUS list to 6 menus matching new layout
- [ ] **Step 2:** Update EFFECT_PARAMS with WAH, glitch effects, vibrato feedback param
- [ ] **Step 3:** Commit: "feat: UI - 6-menu structure + new effect params"

### Task 8: Include WAH in CSD

**Files:**
- Modify: `csound/neptrPhase4.csd`

- [ ] **Step 1:** Add `#include "effects/wah.orc"` include line
- [ ] **Step 2:** Commit: "feat: include wah.orc in main CSD"
