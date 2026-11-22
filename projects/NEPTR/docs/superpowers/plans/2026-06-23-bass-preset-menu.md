# Bass Preset Menu + Menu Reorg — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a launch-selected **bass mode** to NEPTR — same `.csd` and same UI boot surf-or-bass via one flag — giving bass a 5-button preset bank (two bass preamps as a radio pair + stackable bass chorus/distortion/up-only-wammy), while extracting the always-on main preamp into a toggleable effect, relocating tremolo, and reserving a looper menu slot.

**Architecture:** Presets are macros over ordinary, individually-reachable effects (nothing hidden). A `PRESET_BANK` Csound macro (sourced from `live.conf`) sets `gkPresetBank`; the 5 PRESETS-button handlers branch surf-vs-bass; Csound emits a `/preset_bank` OSC int and the mode-blind UI picks one of two label tables. New effects follow the existing 5-touchpoint pattern (`.orc` + `globals.orc` + `signal_chain.orc` + `midi_handler.orc` + UI).

**Tech Stack:** Csound (orchestra `.orc`/`.csd`, `FX_BYPASS`/`FX_TOGGLE`/`ENC_ROT`/`OSC_I` macros), Python 3 + pygame UI (python-osc `ThreadingOSCUDPServer` on `127.0.0.1:8000`), JACK/PipeWire audio.

## Global Constraints

- **Venv-only Python.** Never touch the native/system Python. UI is hand-drawn pygame — no widget toolkits.
- **Repo is NOT git-initialized.** Either run `git init` first (recommended, enables the frequent-commit workflow below) or treat each **Commit** step as a review checkpoint. Commit message footers per house style if you do commit.
- **Subagents cannot exec `csound`** (per `docs/neptr_full_reference.md`). Every `csound --syntax-check-only` / render step **must run in the main session**, not inside an implementation subagent.
- **User-owned zones — coordinate, do not edit unilaterally:** the active config `~/demiurge/live.conf`, the demiurge launcher (`demiurge-mode-select.sh`), `live.conf`'s `power=` knob, and `demiurge-power.service`. The repo's `live.conf` is **stale**; the repo's `scripts/launch_phase4.sh` is legacy/testing-only.
- **Do NOT touch the Teensy→logical-CC remap** in `midi_handler.orc`.
- **Toggle-ID registry additions (exact):** `37 MainPreamp · 38 BassPreampLight · 39 BassPreampHeavy · 40 BassDist · 41 Tremolo`. Previous next-free was 37.
- **Bypass convention:** every `gk<FX>_bypass` is `init 1` (0 = effect ON, 1 = bypassed/OFF).
- **Looper internals are OUT OF SCOPE.** Only the reserved menu slot + navigation.
- **Verification reality:** DSP voicing quality is verified on-ear on hardware; those steps are marked **[on-ear]** and are not fake unit assertions. Objective gates are `csound --syntax-check-only`, `scripts/ui_render_test.py`, and `scripts/osc_smoketest.py`.

---

## File Structure

**Modified (existing):**
- `csound/globals.orc` — new bypass/param globals, `gkPresetBank`, pitch-clamp globals, OSC `/preset_bank` + `/preamp_state` feedback.
- `csound/signal_chain.orc` — guard the inline `Preamp` call; `FX_BYPASS` blocks for bass preamps (front) + bass distortion (drive region).
- `csound/midi_handler.orc` — menu-count wrap (9→11), menu-index dispatch remap for shifted menus, PREAMP-menu toggles, tremolo MOD toggle, surf-preset preamp-on, bass-bank preset handlers, pitch-clamp/expression dispatch, OSC feedback.
- `csound/neptrPhase4.csd` — `#include` new effect files, read `PRESET_BANK` macro.
- `neptrPhase4_UI.py` — `MENUS` reorg (PREAMP + LOOPER), dual PRESETS label tables, `/preset_bank` handler, new `EFFECT_PARAMS`.
- `csound/effects/h90.orc`, `pitchfork.orc`, `gps.orc`, `pitchshifter.orc` — clampable-range guard.

**Created:**
- `csound/effects/bass_preamp_light.orc` — bass preamp (light), Preamp-derived opcode.
- `csound/effects/bass_preamp_heavy.orc` — bass preamp (heavy), Preamp-derived opcode.
- `csound/effects/bass_dist.orc` — bass-optimized dynamic distortion.

**Reference template:** the **FORMS** effect (toggle 28) is the canonical 5-touchpoint example; its exact form is reproduced in Task 5 for copy-paste.

---

## Task 1: Extract MAIN PREAMP into a toggleable effect (surf presets keep their sound)

Makes the always-on Hudson Broadcast preamp bypassable (default OFF) and re-enables it inside each surf preset so they sound identical to today.

**Files:**
- Modify: `csound/globals.orc` (near line 495, the preset-globals area)
- Modify: `csound/signal_chain.orc:54-57` (inline `Preamp` call, stage 0)
- Modify: `csound/midi_handler.orc` — surf preset ON-blocks: OVERDRIVE (~627), SOLID-DIST (~509), AMBIENT (~231); OSC feedback footer (~line 1235 area)

**Interfaces:**
- Produces: global `gkPreamp_bypass` (init 1; 0=ON,1=OFF); OSC address `/preamp_state` (int). Consumed later by Task 4 (PREAMP menu toggle).

- [ ] **Step 1: Add the bypass global.** In `csound/globals.orc`, just below `gkCurrentPreset init -1` (line ~495):

```csound
gkPreamp_bypass init 1   ; MAIN PREAMP (Hudson Broadcast): 0=ON, 1=OFF. Default OFF (was inherent).
```

- [ ] **Step 2: Guard the inline Preamp call.** In `csound/signal_chain.orc` lines 54-57, wrap the existing call (the comment `; Bypass: none — inherently active` becomes obsolete — update it):

```csound
; Stage 0 — MAIN PREAMP (Hudson Broadcast). Now toggleable via gkPreamp_bypass.
if (gkPreamp_bypass < 0.5) then
  aPrevL, aPrevR Preamp aPrevL, aPrevR
endif
```

- [ ] **Step 3: Re-enable preamp first in each surf preset.** In `csound/midi_handler.orc`, add `gkPreamp_bypass = 0` as the **first** assignment inside each preset's ON-block — OVERDRIVE (~line 627, before `gkCurrentPreset = 1`... actually immediately after entering the ON branch), SOLID-DIST (~509), AMBIENT (~231):

```csound
        gkPreamp_bypass = 0      ; surf presets always run the main preamp
```

- [ ] **Step 4: Add OSC feedback.** In `midi_handler.orc` instr 99 footer (with the other `$OSC_I` calls, ~line 1235):

```csound
  ; Toggle 37: MAIN PREAMP (Hudson Broadcast)
  $OSC_I(gkPreamp_bypass'/preamp_state'preByp)
```

- [ ] **Step 5: Syntax-check (main session).**

Run: `csound --syntax-check-only "csound/neptrPhase4.csd"`
Expected: compiles, no errors (exit 0). Watch for unbalanced `if/endif`.

- [ ] **Step 6: [on-ear] Verify on hardware.** Boot; confirm (a) the dry/default tone is now the cleaner preamp-OFF sound, and (b) each surf preset (OD / SOLID-DIST / AMBIENT) sounds identical to before this change. Deploy via `redem`.

- [ ] **Step 7: Commit.**

```bash
git add csound/globals.orc csound/signal_chain.orc csound/midi_handler.orc
git commit -m "feat(preamp): make main preamp toggleable; surf presets re-enable it"
```

---

## Task 2: Menu reorg — PREAMP page + LOOPER stub + index remap

Inserts two menus and **re-synchronizes every menu-index reference** between the UI and Csound. Target order (0-based):
`0 PRESETS · 1 VOLUME · 2 PITCH · 3 FILTERING · 4 MOD · 5 DRIVE · 6 PREAMP · 7 TIME · 8 GLITCH · 9 LOOPER · 10 NAM`

Index shifts vs today: **TIME 6→7, GLITCH 7→8, NAM 8→10**; PREAMP=6 and LOOPER=9 are new.

**Files:**
- Modify: `neptrPhase4_UI.py:100-110` (`MENUS`)
- Modify: `csound/midi_handler.orc:165-171` (wrap bounds) and every `gkCurrentMenu == N` dispatch for TIME/GLITCH/NAM

**Interfaces:**
- Produces: menu indices PREAMP=6, LOOPER=9 (consumed by Tasks 3–6, 11). `handle_menu_change` already clamps to `len(MENUS)-1`, so no UI bound edit is needed.

- [ ] **Step 1: Reorder `MENUS` in the UI.** Replace lines 100-110 so PREAMP follows DRIVE and LOOPER precedes NAM (PREAMP/LOOPER slots are stubs filled by later tasks; `"—"` renders as an empty outlined slot):

```python
MENUS = [
    {"name": "PRESETS", "effects": [("sld", "OVERDRIVE"), ("hvy", "SOLID DIST"), ("pha", "PHASER"), ("trm", "TREMOLO"), ("amb", "AMBIENT")]},
    {"name": "VOLUME",  "effects": [("tape", "TAPE"), ("atk", "ATTACK"),  ("les", "LESLIE"),  ("rm",  "RINGMOD"), ("vol", "VOLUME")]},
    {"name": "PITCH",   "effects": [("h90", "H90"),     ("pf",  "P-FORK"),  ("gps", "GPS"),     ("ps",  "OLD PS"),  ("sdl", "SPECDLY")]},
    {"name": "FILTERING","effects":[("wah", "WAH"),     ("mog", "AUTOMOOG"),("tb2", "—"),       ("fmt", "FORMANT"), ("fms", "FORMS")]},
    {"name": "MOD",     "effects": [("fla", "FLANGER"), ("pha", "PHASER"),  ("cho", "CHORUS"),  ("vib", "VIBRATO")]},
    {"name": "DRIVE",   "effects": [("muf", "BIGMUFF"), ("dhg", "DS-1K"),   ("rmz", "RAT-MZ"), ("mar", "MARSHALL"), ("od", "TUBESCREAM")]},
    {"name": "PREAMP",  "effects": [("pre", "MAIN PRE"), ("bpl", "BASS PRE L"), ("bph", "BASS PRE H"), ("bds", "BASS DIST"), ("tbp", "—")]},
    {"name": "TIME",    "effects": [("mod", "MOOD"),    ("grn", "GRANULAR"),("vrb", "REVERB"),  ("hab", "HABIT"),   ("dly", "DELAY")]},
    {"name": "GLITCH",  "effects": [("fsh", "FSHIFT"),  ("sqm", "SQMOD"),   ("wgd", "WGUIDE"),  ("bbc", "BBCUT")]},
    {"name": "LOOPER",  "effects": [("lp0", "—"), ("lp1", "—"), ("lp2", "—"), ("lp3", "—"), ("lp4", "—")]},
    {"name": "NAM",     "effects": [("nam0", "73 PLEXI"), ("nam1", "FENDER SR"), ("nam2", "PEAVEY"), ("nam3", "SUNN T"), ("nam4", "VOX AC30")]},
]
```

- [ ] **Step 2: Widen the Csound wrap bounds.** In `csound/midi_handler.orc:165-171`, change the modulus from 9 to 11:

```csound
    if (kCC == 70 && kVal == 127) then
      gkCurrentMenu = (gkCurrentMenu - 1 + 11) % 11
      OSC3x_i "/menu", gkCurrentMenu

    elseif (kCC == 73 && kVal == 127) then
      gkCurrentMenu = (gkCurrentMenu + 1) % 11
      OSC3x_i "/menu", gkCurrentMenu
```

- [ ] **Step 3: Remap shifted menu dispatch.** Search `midi_handler.orc` for every `gkCurrentMenu == 6`, `== 7`, `== 8` that belongs to TIME / GLITCH / NAM (the slot-toggle tree and any CC dispatch) and update: **TIME 6→7, GLITCH 7→8, NAM 8→10**. Do this carefully one match at a time; PREAMP (6) and LOOPER (9) handlers are added in later tasks. (LOOPER needs no toggle handler yet — empty slots.)

- [ ] **Step 4: Render-test the UI.**

Run: `python scripts/ui_render_test.py --out /tmp/neptr_ui_shots`
Expected: exits 0, writes PNGs for all 11 menus including a PREAMP page (MAIN PRE / BASS PRE L / BASS PRE H / BASS DIST / empty) and a LOOPER page (all empty slots).

- [ ] **Step 5: Syntax-check Csound (main session).**

Run: `csound --syntax-check-only "csound/neptrPhase4.csd"`
Expected: exit 0.

- [ ] **Step 6: [on-ear] Verify navigation.** Boot; scroll CC70/73 through all 11 menus and confirm TIME/GLITCH/NAM effects still toggle correctly (proves the index remap is right).

- [ ] **Step 7: Commit.**

```bash
git add neptrPhase4_UI.py csound/midi_handler.orc
git commit -m "feat(menu): add PREAMP + LOOPER pages, remap menu indices"
```

---

## Task 3: Relocate TREMOLO into the MOD menu as a normal effect

Tremolo already exists (`effects/tremolo.orc`, instr 101, `gkTremBypass`, defaults `gkTremRate 7.38` / `gkTremDepth 0.7`). Give it a normal MOD-menu toggle (ID 41) and rotation params.

**Files:**
- Modify: `neptrPhase4_UI.py` `MENUS` MOD entry (add `("trm","TREMOLO")`) and `EFFECT_PARAMS` (add `"trm"`)
- Modify: `csound/midi_handler.orc` — MOD menu (`gkCurrentMenu == 4`) slot toggle, CC20 dispatch, OSC feedback; remove the obsolete Menu-0 tremolo modifier (~313-319)

**Interfaces:**
- Consumes: `gkTremBypass`, `gkTremRate`, `gkTremDepth` (exist). Produces: OSC `/trm_state`, `/trm_rate`, `/trm_depth`; `EFFECT_PARAMS["trm"]`.

- [ ] **Step 1: Add tremolo to the MOD menu (UI).** In `MENUS`, MOD line, append the slot:

```python
    {"name": "MOD",     "effects": [("fla", "FLANGER"), ("pha", "PHASER"),  ("cho", "CHORUS"),  ("vib", "VIBRATO"), ("trm", "TREMOLO")]},
```

- [ ] **Step 2: Register tremolo params (UI).** In `EFFECT_PARAMS`, replace the existing `"trm"` state-only entry with real rotations:

```python
    "trm": [("rate", "Rate Hz", 0, 12), ("depth", "Depth", 0, 1)],
```

- [ ] **Step 3: Add the MOD-menu toggle (Csound).** In `midi_handler.orc`, in the `gkCurrentMenu == 4` (MOD) slot-toggle branch, add the tremolo slot (slot index 4) following the FORMS toggle pattern:

```csound
        ; TREMOLO — MOD menu, slot 4 (toggle 41)
        $FX_TOGGLE(gkTremBypass'41)
        if (gkTremBypass < 0.5) then
          gkLastToggle29 = 41
          printks "TREMOLO: ON\n", 0
        else
          printks "TREMOLO: OFF\n", 0
        endif
```

- [ ] **Step 4: Add CC20 rotation dispatch (Csound).** In the `gkLastToggle == ...` CC20 chain (~line 971 area), add:

```csound
      elseif (gkLastToggle == 41) then
        ; TREMOLO rate
        $ENC_ROT(gkTremRate'0.25'0'12)
```

- [ ] **Step 5: Add OSC feedback (Csound).** In the instr 99 footer:

```csound
  ; Toggle 41: TREMOLO
  $OSC_I(gkTremBypass'/trm_state'trmByp)
  $OSC_F(gkTremRate'/trm_rate'trmRate)
  $OSC_F(gkTremDepth'/trm_depth'trmDepth)
```

- [ ] **Step 6: Remove the obsolete Menu-0 tremolo modifier.** Delete the `~313-319` block that toggled `gkTremBypass` from the old PRESETS menu (the PRESETS menu becomes pure macro launchers in Task 11). Leave `gkTremBypass`/defaults intact in globals.

- [ ] **Step 7: Verify.**

Run: `csound --syntax-check-only "csound/neptrPhase4.csd"` → exit 0 (main session).
Run: `python scripts/ui_render_test.py` → MOD page shows 5 slots incl TREMOLO.
Run: `python scripts/osc_smoketest.py` → exercises `/trm_state` + params without error.

- [ ] **Step 8: [on-ear] Confirm** tremolo toggles from MOD and rate knob sweeps.

- [ ] **Step 9: Commit.**

```bash
git add neptrPhase4_UI.py csound/midi_handler.orc
git commit -m "feat(mod): move tremolo into MOD menu as a normal effect"
```

---

## Task 4: MAIN PREAMP gets its PREAMP-menu toggle

Wire the already-extracted main preamp (Task 1) to its PREAMP-page slot so it's individually toggleable.

**Files:**
- Modify: `csound/midi_handler.orc` — PREAMP menu (`gkCurrentMenu == 6`) slot 0 toggle
- Modify: `neptrPhase4_UI.py` `EFFECT_PARAMS` (add `"pre"`)

**Interfaces:**
- Consumes: `gkPreamp_bypass`, `/preamp_state` (Task 1). Produces: `EFFECT_PARAMS["pre"]`.

- [ ] **Step 1: Register the param (UI).** In `EFFECT_PARAMS`:

```python
    "pre": [("voice", "Voice", 0, 2)],
```

- [ ] **Step 2: Add the PREAMP-menu dispatch branch (Csound).** Add a new `gkCurrentMenu == 6` branch to the slot-toggle tree, slot 0 = main preamp:

```csound
      elseif (gkCurrentMenu == 6) then
        ; PREAMP menu
        if (kSlot == 0) then
          ; MAIN PREAMP (toggle 37)
          $FX_TOGGLE(gkPreamp_bypass'37)
          gkLastToggle29 = 37
        endif
```

(Use the same `kSlot` variable the existing menus use to index the pressed button; match the surrounding code's slot-detection idiom.)

- [ ] **Step 3: Verify.** `csound --syntax-check-only` (main session) → exit 0; `python scripts/ui_render_test.py` → PREAMP slot 0 = MAIN PRE.

- [ ] **Step 4: [on-ear] Confirm** main preamp toggles on/off from the PREAMP page.

- [ ] **Step 5: Commit.**

```bash
git add csound/midi_handler.orc neptrPhase4_UI.py
git commit -m "feat(preamp): add MAIN PREAMP toggle on PREAMP page"
```

---

## Task 5: BASS PREAMP LIGHT + HEAVY (radio pair)

Two new bass-voiced preamps derived from the Hudson Broadcast `Preamp` opcode, wired as PREAMP-page slots 1 & 2, mutually exclusive (radio). This task uses the **FORMS 5-touchpoint pattern** (reproduced below) for wiring.

> **FORMS reference (do not edit — this is the copy template):**
> - globals: `gkFormsBypass init 1` / `gkFormsShift init 0` / `gkFormsMix init 1`
> - signal_chain: `gaFormsSendL = aPrevL` / `gaFormsSendR = aPrevR` / `$FX_BYPASS(gkFormsBypass'119'gaFormsSendL'gaFormsSendR'gaFormsOutL'gaFormsOutR'fms)` / `aPrevL ntrpol gaFormsSendL, gaFormsOutL, gkFormsMix` (and R)
> - midi toggle: `$FX_TOGGLE(gkFormsBypass'28)` inside the menu branch
> - OSC: `$OSC_I(gkFormsBypass'/fms_state'fmsByp)`
> - UI: `EFFECT_PARAMS["fms"]` + MENUS slot

**Files:**
- Create: `csound/effects/bass_preamp_light.orc` (instr 160), `csound/effects/bass_preamp_heavy.orc` (instr 161)
- Modify: `csound/globals.orc` (bypass + params + buses ×2), `csound/signal_chain.orc` (two `FX_BYPASS` blocks at the front, right after the MAIN PREAMP stage), `csound/neptrPhase4.csd` (`#include` ×2), `csound/midi_handler.orc` (PREAMP slots 1 & 2 + radio + OSC), `neptrPhase4_UI.py` (`EFFECT_PARAMS` ×2)

**Interfaces:**
- Produces: `gkBPL_bypass`/`gkBPH_bypass` (init 1), `gkBPL_drive`/`gkBPH_drive`, buses `gaBPLSendL/R`/`gaBPLOutL/R`/`gaBPHSendL/R`/`gaBPHOutL/R`, OSC `/bpl_state`,`/bph_state`. Radio invariant: toggling one ON forces the other OFF. Consumed by Task 11 (bass preset slots s0/s1).

- [ ] **Step 1: Add globals.** In `csound/globals.orc` near the other effect globals:

```csound
; --- Bass preamps (radio pair, Preamp-derived) ---
gkBPL_bypass init 1
gkBPL_drive  init 0.25
gkBPH_bypass init 1
gkBPH_drive  init 0.55
gaBPLSendL init 0
gaBPLSendR init 0
gaBPLOutL  init 0
gaBPLOutR  init 0
gaBPHSendL init 0
gaBPHSendR init 0
gaBPHOutL  init 0
gaBPHOutR  init 0
```

- [ ] **Step 2: Create the LIGHT preamp instr.** `csound/effects/bass_preamp_light.orc`: **copy the `Preamp` opcode body from `csound/effects/preamp.orc`** into an `instr 160` that reads `gaBPLSendL/R` and writes `gaBPLOutL/R`, then apply the bass-voicing changes:
  - Raise the HP coupling cut from 25 Hz down to ~18 Hz (keep sub fundamentals).
  - Split-frequency: keep the low band cleaner — push the transformer LF-saturation split up so only mids saturate (target ~180–220 Hz instead of guitar's 250 Hz, and parallel-blend the clean low band).
  - Light drive: scale the bias-shift depth by `gkBPL_drive` (subtle warmth, ~clean DI).
  - `dcblock2` in, `limit ±0.99` out (per house effect rules).
  Mark the exact germanium/gain coefficients as **[on-ear]** tuning.

- [ ] **Step 3: Create the HEAVY preamp instr.** `csound/effects/bass_preamp_heavy.orc` as `instr 161` reading `gaBPHSendL/R`, writing `gaBPHOutL/R` — same structure but more pre-gain into the asymmetric tanh (scaled by `gkBPH_drive`), upper-mid emphasis for crunch, low band still parallel-clean. **[on-ear]** tuning.

- [ ] **Step 4: Include the files.** In `csound/neptrPhase4.csd` includes section:

```csound
#include "effects/bass_preamp_light.orc"
#include "effects/bass_preamp_heavy.orc"
```

- [ ] **Step 5: Insert FX_BYPASS blocks at the front of the chain.** In `csound/signal_chain.orc`, immediately after the MAIN PREAMP stage-0 guard:

```csound
; Stage 0a — BASS PREAMP LIGHT (160)
gaBPLSendL = aPrevL
gaBPLSendR = aPrevR
$FX_BYPASS(gkBPL_bypass'160'gaBPLSendL'gaBPLSendR'gaBPLOutL'gaBPLOutR'bpl)
aPrevL = gaBPLOutL
aPrevR = gaBPLOutR
; Stage 0b — BASS PREAMP HEAVY (161)
gaBPHSendL = aPrevL
gaBPHSendR = aPrevR
$FX_BYPASS(gkBPH_bypass'161'gaBPHSendL'gaBPHSendR'gaBPHOutL'gaBPHOutR'bph)
aPrevL = gaBPHOutL
aPrevR = gaBPHOutR
```

- [ ] **Step 6: Add PREAMP slots 1 & 2 with radio logic (Csound).** Extend the `gkCurrentMenu == 6` branch from Task 4:

```csound
        elseif (kSlot == 1) then
          ; BASS PREAMP LIGHT (toggle 38) — radio: turns HEAVY off
          $FX_TOGGLE(gkBPL_bypass'38)
          if (gkBPL_bypass < 0.5) then
            gkBPH_bypass = 1
          endif
          gkLastToggle29 = 38
        elseif (kSlot == 2) then
          ; BASS PREAMP HEAVY (toggle 39) — radio: turns LIGHT off
          $FX_TOGGLE(gkBPH_bypass'39)
          if (gkBPH_bypass < 0.5) then
            gkBPL_bypass = 1
          endif
          gkLastToggle29 = 39
        endif
```

- [ ] **Step 7: OSC feedback (Csound).** Footer:

```csound
  $OSC_I(gkBPL_bypass'/bpl_state'bplByp)
  $OSC_I(gkBPH_bypass'/bph_state'bphByp)
```

- [ ] **Step 8: UI params.** In `EFFECT_PARAMS`:

```python
    "bpl": [("drive", "Drive", 0, 1)],
    "bph": [("drive", "Drive", 0, 1)],
```

- [ ] **Step 9: Verify.** `csound --syntax-check-only "csound/neptrPhase4.csd"` (main session) → exit 0; `python scripts/ui_render_test.py` → PREAMP slots 1/2 labeled; `python scripts/osc_smoketest.py` → `/bpl_state` `/bph_state` exercised.

- [ ] **Step 10: [on-ear] Tune voicings.** Confirm LIGHT = clean bass DI, HEAVY = tight crunch with clean lows, and that selecting one deselects the other (radio).

- [ ] **Step 11: Commit.**

```bash
git add csound/effects/bass_preamp_light.orc csound/effects/bass_preamp_heavy.orc csound/globals.orc csound/signal_chain.orc csound/neptrPhase4.csd csound/midi_handler.orc neptrPhase4_UI.py
git commit -m "feat(bass): add light/heavy bass preamps as a radio pair"
```

---

## Task 6: BASS DISTORTION (bass-optimized dynamic clipper)

New effect on PREAMP slot 3, placed in the drive region of the chain, with a clean-low parallel blend.

**Files:**
- Create: `csound/effects/bass_dist.orc` (instr 162)
- Modify: `csound/globals.orc`, `csound/signal_chain.orc` (drive region), `csound/neptrPhase4.csd`, `csound/midi_handler.orc` (PREAMP slot 3 + CC20 dispatch + OSC), `neptrPhase4_UI.py`

**Interfaces:**
- Produces: `gkBDS_bypass` (init 1), `gkBDS_drive`, `gkBDS_blend`, `gkBDS_tone`, buses `gaBDSSendL/R`/`gaBDSOutL/R`, OSC `/bds_state`. Consumed by Task 11 (bass preset s3).

- [ ] **Step 1: Globals.**

```csound
; --- Bass distortion ---
gkBDS_bypass init 1
gkBDS_drive  init 0.6
gkBDS_blend  init 0.35   ; clean low-band parallel mix
gkBDS_tone   init 0.5
gaBDSSendL init 0
gaBDSSendR init 0
gaBDSOutL  init 0
gaBDSOutR  init 0
```

- [ ] **Step 2: Create `bass_dist.orc` (instr 162).** Reads `gaBDSSendL/R`, writes `gaBDSOutL/R`. DSP outline (concrete starter; coefficients **[on-ear]**):
  - `dcblock2` input.
  - Split a low band at ~120 Hz (`butterlp`) → keep clean.
  - Drive the full-range/high band into an asymmetric `tanh` clipper scaled by `gkBDS_drive`, upper-mid emphasis (`pareq` boost ~1.5 kHz pre-clip), `gkBDS_tone` post low-pass.
  - Recombine: `clean_low * gkBDS_blend + clipped * (1)`; `limit ±0.99`.

- [ ] **Step 3: Include.** `#include "effects/bass_dist.orc"` in `neptrPhase4.csd`.

- [ ] **Step 4: Signal-chain FX_BYPASS in the drive region.** In `signal_chain.orc`, alongside the existing drive pedals (after DRIVE block, matching their `aPrevL` handoff idiom):

```csound
; BASS DISTORTION (162) — drive region
gaBDSSendL = aPrevL
gaBDSSendR = aPrevR
$FX_BYPASS(gkBDS_bypass'162'gaBDSSendL'gaBDSSendR'gaBDSOutL'gaBDSOutR'bds)
aPrevL = gaBDSOutL
aPrevR = gaBDSOutR
```

- [ ] **Step 5: PREAMP slot 3 toggle + CC20 dispatch + OSC (Csound).** Extend the `gkCurrentMenu == 6` branch:

```csound
        elseif (kSlot == 3) then
          ; BASS DISTORTION (toggle 40)
          $FX_TOGGLE(gkBDS_bypass'40)
          gkLastToggle29 = 40
        endif
```

CC20 chain:

```csound
      elseif (gkLastToggle == 40) then
        $ENC_ROT(gkBDS_drive'0.02'0'1)
```

Footer OSC: `$OSC_I(gkBDS_bypass'/bds_state'bdsByp)`

- [ ] **Step 6: UI params.**

```python
    "bds": [("drive", "Drive", 0, 1), ("blend", "Clean Low", 0, 1), ("tone", "Tone", 0, 1)],
```

- [ ] **Step 7: Verify.** `csound --syntax-check-only` (main session) → 0; `ui_render_test.py`; `osc_smoketest.py` exercises `/bds_state`.

- [ ] **Step 8: [on-ear] Tune** — heavy grind with the low end staying solid (raise `gkBDS_blend` if it thins out).

- [ ] **Step 9: Commit.**

```bash
git add csound/effects/bass_dist.orc csound/globals.orc csound/signal_chain.orc csound/neptrPhase4.csd csound/midi_handler.orc neptrPhase4_UI.py
git commit -m "feat(bass): add bass-optimized distortion with clean-low blend"
```

---

## Task 7: Pitch-shifter clampable-range refactor

Add an internal `[min,max]` semitone clamp to all four shifters, **defaulting to the current full range** so normal behavior is unchanged. Not exposed in the UI.

**Files:**
- Modify: `csound/globals.orc` (clamp globals ×4), `csound/effects/h90.orc` (instr 48), `pitchfork.orc` (49), `gps.orc` (55), `pitchshifter.orc` (50)

**Interfaces:**
- Produces: `gkH90_min/max`, `gkPF_min/max`, `gkGPS_min/max`, `gkPS_min/max` (defaults −12/+12). Consumed by Task 11 (H90 wammy sets 0/+12).

- [ ] **Step 1: Add clamp globals (defaults = current full range).**

```csound
; --- Pitch-shifter clamp ranges (internal; default = current full range) ---
gkH90_min init -12
gkH90_max init 12
gkPF_min  init -12
gkPF_max  init 12
gkGPS_min init -12
gkGPS_max init 12
gkPS_min  init -12
gkPS_max  init 12
```

- [ ] **Step 2: Apply the clamp in each shifter.** In each `.orc`, immediately before the semitone control feeds the pitch engine, clamp it. H90 (`gkSemiH90`):

```csound
  kSemiC = limit(gkSemiH90, gkH90_min, gkH90_max)
```

…and use `kSemiC` where `gkSemiH90` was consumed. Repeat with the matching globals: P-FORK `gkSemiPF`→`gkPF_*`, GPS semitone→`gkGPS_*`, OLD PS `gkSemi`→`gkPS_*`. (P-FORK still snaps to its quantized set *after* the clamp.)

- [ ] **Step 3: Verify defaults are inert.** `csound --syntax-check-only` (main session) → 0.

- [ ] **Step 4: [on-ear] Regression** — with defaults, all four pitch shifters behave exactly as before (full −12…+12).

- [ ] **Step 5: Commit.**

```bash
git add csound/globals.orc csound/effects/h90.orc csound/effects/pitchfork.orc csound/effects/gps.orc csound/effects/pitchshifter.orc
git commit -m "feat(pitch): add internal clampable range to all pitch shifters"
```

---

## Task 8: Mode flag — Csound side (`PRESET_BANK` macro + `gkPresetBank` + OSC)

**Files:**
- Modify: `csound/neptrPhase4.csd` (macro default), `csound/globals.orc` (`gkPresetBank`), `csound/midi_handler.orc` (OSC send at init + footer)

**Interfaces:**
- Produces: `gkPresetBank` (0=surf,1=bass), OSC `/preset_bank` (int). Consumed by Task 9 (UI) and Task 11 (bank branch).

- [ ] **Step 1: Default the macro.** Near the top of `csound/neptrPhase4.csd` (orchestra header, before instruments):

```csound
#ifndef PRESET_BANK
#define PRESET_BANK #0#
#endif
```

- [ ] **Step 2: Seed the global from the macro.** In `csound/globals.orc`:

```csound
gkPresetBank init $PRESET_BANK
```

- [ ] **Step 3: Emit `/preset_bank` once at init and on the OSC footer.** In `midi_handler.orc` instr 99: send once at first k-pass and include in the changed-feedback footer:

```csound
  ; one-shot at startup
  kBankInit init 0
  if (kBankInit == 0) then
    OSC3x_i "/preset_bank", gkPresetBank
    kBankInit = 1
  endif
```

Footer: `$OSC_I(gkPresetBank'/preset_bank'pbnk)`

- [ ] **Step 4: Verify both macro values syntax-check.**

Run (main session): `csound --syntax-check-only "csound/neptrPhase4.csd"` → 0
Run: `csound --syntax-check-only -+omacro:PRESET_BANK=1 "csound/neptrPhase4.csd"` → 0

- [ ] **Step 5: Commit.**

```bash
git add csound/neptrPhase4.csd csound/globals.orc csound/midi_handler.orc
git commit -m "feat(mode): csound reads PRESET_BANK macro and emits /preset_bank"
```

---

## Task 9: Mode flag — UI side (dual PRESETS label tables + `/preset_bank` handler)

The mode-blind UI holds both label tables and renders the right one from `/preset_bank`.

**Files:**
- Modify: `neptrPhase4_UI.py` — add `SURF_PRESETS`/`BASS_PRESETS`, `self.preset_bank`, `route_osc` branch (lines 864-881), `handle_preset_bank_change`, `_render_menu` (516-540) menu-0 selection

**Interfaces:**
- Consumes: OSC `/preset_bank` (Task 8). Produces: `self.preset_bank` (0/1) and bank-aware menu-0 labels.

- [ ] **Step 1: Define both label tables.** Near `MENUS`:

```python
SURF_PRESETS = MENUS[0]["effects"]   # OVERDRIVE / SOLID DIST / PHASER / TREMOLO / AMBIENT
BASS_PRESETS = [("bp_l", "PRE LIGHT"), ("bp_h", "PRE HEAVY"), ("b_cho", "BASS CHORUS"), ("b_dst", "BASS DIST"), ("b_wam", "H90 WAMMY")]
```

- [ ] **Step 2: Initialize the field.** In `NeptrUI.__init__` (near line 240):

```python
        self.preset_bank = 0   # 0=surf, 1=bass
```

- [ ] **Step 3: Route `/preset_bank`.** In `route_osc` (after the `/preset` branch, ~line 868):

```python
    elif addr == '/preset_bank':
        ui.handle_preset_bank_change(int(val))
```

- [ ] **Step 4: Add the handler.** On `NeptrUI`:

```python
    def handle_preset_bank_change(self, bank):
        bank = 1 if bank else 0
        if bank != self.preset_bank:
            self.preset_bank = bank
            self.dirty = True
```

- [ ] **Step 5: Bank-aware menu-0 render.** In `_render_menu` (line ~517), pick the table when rendering PRESETS:

```python
        effects = ([SURF_PRESETS, BASS_PRESETS][self.preset_bank]
                   if menu_idx == 0 else MENUS[menu_idx]["effects"])
```

(Replace the existing `effects = MENUS[menu_idx]["effects"]` line.)

- [ ] **Step 6: Verify.**

Run: `python scripts/ui_render_test.py` → still renders (default bank 0 = surf labels).
Then simulate bass: `python scripts/osc_smoketest.py` (confirm no crash); manually send `/preset_bank 1` and re-render to see BASS labels (add a temporary render shot or assert in the smoketest if available).

- [ ] **Step 7: Commit.**

```bash
git add neptrPhase4_UI.py
git commit -m "feat(mode): UI dual preset label tables driven by /preset_bank"
```

---

## Task 10: Mode flag — launch plumbing (`live.conf` key → `--omacro`)

**OPEN ITEM:** the active launch path is the demiurge launcher (`demiurge-mode-select.sh`, not in this repo) reading `~/demiurge/live.conf`. This task starts with an investigation and **coordinates with the user** before editing anything in the demiurge-owned zone.

**Files:**
- Modify (legacy/testing reference): `scripts/launch_phase4.sh` (`get_conf` + `CSD_OPTS`)
- Coordinate (active): `~/demiurge/live.conf` key + the demiurge launcher's csound-option injection

**Interfaces:**
- Produces: `preset_bank` conf key → `-+omacro:PRESET_BANK=<0|1>` reaching `csound` (consumed by Task 8).

- [ ] **Step 1: Investigate the active injection point.** Locate where the demiurge launcher builds the `csound` command (ask the user where `demiurge-mode-select.sh` lives / how it sources `~/demiurge/live.conf`). Document the exact line. **Do not edit user-owned files without confirmation.**

- [ ] **Step 2: Wire the legacy/testing path.** In `scripts/launch_phase4.sh`, read a `preset_bank` key via the existing `get_conf` helper (lines 47-51) and append to `CSD_OPTS` (lines 403-415):

```bash
PRESET_BANK="$(get_conf preset_bank surf)"
[ "$PRESET_BANK" = "bass" ] && CSD_OPTS+=("-+omacro:PRESET_BANK=1") || CSD_OPTS+=("-+omacro:PRESET_BANK=0")
```

- [ ] **Step 3: Add the key to the reference conf.** Add a commented default to `phase4.conf` (and document for `~/demiurge/live.conf`):

```
# preset_bank = surf   # surf | bass — selects the PRESETS bank (csound only; harmless if unset)
preset_bank = surf
```

- [ ] **Step 4: [on-ear] Verify both boots.** Boot with `preset_bank=surf` (UI shows surf labels) and `preset_bank=bass` (UI shows bass labels). Confirm a plain Demiurge boot with the key absent still works (defaults to surf).

- [ ] **Step 5: Commit.**

```bash
git add scripts/launch_phase4.sh phase4.conf
git commit -m "feat(mode): source preset_bank from conf into csound --omacro (legacy path)"
```

---

## Task 11: Bass-bank preset handlers (radio preamps + stackable modifiers)

The 5 PRESETS-menu buttons (`gkCurrentMenu == 0`) branch on `gkPresetBank`. Surf branch = existing handlers (already preamp-on from Task 1). Bass branch = the new behavior. Also default LIGHT-on at bass boot.

**Files:**
- Modify: `csound/midi_handler.orc` — the menu-0 slot dispatch (surf blocks at 179-664) wrapped in a `gkPresetBank` branch; bass-boot init

**Interfaces:**
- Consumes: `gkBPL_bypass`/`gkBPH_bypass` (T5), `gkBDS_bypass` (T6), `gkChorusBypass`+params (existing), `gkH90*`+clamp (T7), `gkPresetBank` (T8). Mutual-exclusion + default-light invariants.

- [ ] **Step 1: Bass-boot default (LIGHT on).** Where `gkPresetBank` is seeded, add a one-shot: if bass, enable the light preamp by default:

```csound
  kBankBoot init 0
  if (kBankBoot == 0) then
    if (gkPresetBank == 1) then
      gkBPL_bypass = 0
      gkBPH_bypass = 1
    endif
    kBankBoot = 1
  endif
```

- [ ] **Step 2: Branch the menu-0 dispatch.** Wrap the existing PRESETS slot dispatch:

```csound
      if (gkCurrentMenu == 0) then
        if (gkPresetBank == 0) then
          ; ---- SURF BANK (existing handlers: OD / SOLID-DIST / PHASER / TREM / AMBIENT) ----
          ; ... existing surf preset code stays here, unchanged ...
        else
          ; ---- BASS BANK ----
          if (kSlot == 0) then
            ; PRE LIGHT — radio
            gkBPL_bypass = 0
            gkBPH_bypass = 1
          elseif (kSlot == 1) then
            ; PRE HEAVY — radio
            gkBPH_bypass = 0
            gkBPL_bypass = 1
          elseif (kSlot == 2) then
            ; BASS CHORUS — stackable modifier (does NOT touch preamp)
            $FX_TOGGLE(gkChorusBypass'7)
            if (gkChorusBypass < 0.5) then
              gkChorusRate = 0.45
              gkChorusDepth = 0.5
              gkChorusMix = 0.4
            endif
          elseif (kSlot == 3) then
            ; BASS DIST — stackable modifier
            $FX_TOGGLE(gkBDS_bypass'40)
          elseif (kSlot == 4) then
            ; H90 WAMMY — up-only, expression-swept
            $FX_TOGGLE(gkH90bypass'26)
            if (gkH90bypass < 0.5) then
              gkH90_min = 0
              gkH90_max = 12
              gkH90mix = 0.6
              gkLastToggle29 = 26   ; route expression pedal (CC29) to H90 sweep
            else
              gkH90_min = -12       ; restore default clamp on release
              gkH90_max = 12
            endif
          endif
        endif
      endif
```

(Match `gkChorusBypass`/`gkH90bypass` to the exact global names in `globals.orc`; the explore pass names chorus toggle 7 and H90 toggle 26.)

- [ ] **Step 3: Bind the expression pedal to the up-only sweep.** In the CC29 dispatch (where `gkLastToggle29` routes the expression pedal), ensure `gkLastToggle29 == 26` maps the pedal across `[gkH90_min, gkH90_max]`:

```csound
      elseif (gkLastToggle29 == 26) then
        ; expression → H90 semitone, clamped to its current range (heel=min, toe=max)
        gkSemiH90 = gkH90_min + (gkH90_max - gkH90_min) * (kVal / 127)
```

- [ ] **Step 4: Verify.**

Run (main session): `csound --syntax-check-only -+omacro:PRESET_BANK=1 "csound/neptrPhase4.csd"` → 0, and with `=0` → 0.
Run: `python scripts/osc_smoketest.py` → no crash.

- [ ] **Step 5: [on-ear] Full bass-mode pass.** Boot bass: LIGHT on by default; selecting HEAVY deselects LIGHT; toggling CHORUS/DIST/WAMMY never changes the preamp selection; the expression pedal sweeps H90 up from unison to +octave and never below. Boot surf: presets behave exactly as before.

- [ ] **Step 6: Commit.**

```bash
git add csound/midi_handler.orc
git commit -m "feat(bass): bass-bank preset handlers — radio preamps + stackable modifiers"
```

---

## Self-Review

**Spec coverage** (spec §→task): §3 preset-macro principle → all; §3 menu reorg → T2; §4 PREAMP page → T2/T4/T5/T6; §4.1 main-preamp extraction → T1; §4.2 bass preamps → T5; §4.3 bass distortion → T6; §5 mode switch → T8/T9/T10; §6 surf reformat → T1; §7 bass bank radio+modifiers → T11; §8 pitch clamp → T7; tremolo→MOD → T3; looper stub → T2. All sections covered.

**Placeholder scan:** DSP voicing steps give concrete starter algorithms + target frequencies and explicitly mark coefficient tuning as **[on-ear]** (genuine hardware work, not a placeholder). Task 10 Step 1 is a flagged investigation of a user-owned file, not a TODO. No "TBD"/"handle edge cases"/empty-test patterns.

**Type/name consistency:** bus/global names consistent across tasks (`gkBPL_*`/`gkBPH_*`/`gkBDS_*`, `gaBPL*`/`gaBPH*`/`gaBDS*`, OSC `/bpl_state`/`/bph_state`/`/bds_state`/`/preset_bank`/`/preamp_state`); toggle IDs 37–41 match the Global Constraints registry; menu indices (PREAMP=6, LOOPER=9, TIME=7, GLITCH=8, NAM=10) consistent T2↔T4–6↔T11. Verify against `globals.orc` the exact existing names for chorus/H90 bypass at implementation time (noted in T11 Step 2).
