# Bass Preset Menu + Menu Reorg — Design Spec

- **Date:** 2026-06-22
- **Status:** Approved design, pending spec review → implementation plan
- **Scope:** NEPTR phase4 (Csound + pygame UI bass/guitar pedal)

---

## 1. Goal

Add a **bass mode** to NEPTR without forking the program: the same `.csd` and the
same UI boot in either **surf** or **bass** mode, decided by a single launch flag.
Bass mode gives the player a 5-button preset menu tuned for bass — two bass
preamps, a bass-tuned chorus, a bass-optimized distortion, and an up-only pitch
"wammy" — while keeping the core principle: **presets hide nothing**. Every effect
a preset recalls also lives as an ordinary, individually-toggleable menu effect
with normal default-init values. Presets are just macros that recall a vibe.

Secondary cleanups carried in the same refactor (because we're already touching
the menus and the preset system):

- Pull **TREMOLO** out of the preset menu into the MOD menu as a normal effect.
- Extract the always-on **MAIN PREAMP** into a toggleable effect (no longer baked
  into the input).
- Add a **PREAMP** menu page to house the main preamp + the two bass preamps + the
  bass distortion.
- Refactor **all pitch shifters** to support an internal clampable range (used
  first by the bass H90 wammy preset).
- Reserve a **LOOPER** menu slot (stub only) so the in-flight looper work inserts
  cleanly before NAM.

### Non-goals / out of scope

- **Looper internals.** Not built yet and being developed separately. We only
  reserve its menu slot (2nd-to-last, before NAM) and wire navigation so it
  inserts cleanly. We do **not** implement looper behavior.
- **No power/throttle changes.** `live.conf`'s `power=` knob and
  `demiurge-power.service` remain user-owned; this work only adds a runtime
  `preset_bank` key and never touches power plumbing.
- **No native-Python changes** (venv-only rule).

---

## 2. Core principle: presets are macros, nothing is hidden

A preset button sets specific parameter values on the *same* effects that live in
the normal menus. Launch any effect the normal way and it uses its own default
init values; presets only recall a curated combination. Nothing is reachable
*only* through a preset. This is already how the surf presets behave (they set a
batch of `gk…` globals on effects that also live in DRIVE/MOD/TIME) — bass mode
follows the identical pattern.

---

## 3. Menu reorganization

### 3.1 Before (9 menus)

```
PRESETS · VOLUME · PITCH · FILTERING · MOD · DRIVE · TIME · GLITCH · NAM
```

- PRESETS (Menu 0) doubled as effect toggles: OVERDRIVE / SOLID-DIST / PHASER /
  TREMOLO / AMBIENT.
- MOD held only 4 effects (FLANGER / PHASER / CHORUS / VIBRATO) — one free slot.
- DRIVE was full (BIGMUFF / DS-1K / RAT-MZ / MARSHALL / TUBESCREAM).
- TREMOLO existed only as a preset-menu toggle (instr 101), not a normal effect.
- MAIN PREAMP was inherent on the input (always on, not bypassable).

### 3.2 After (11 menus)

```
PRESETS · VOLUME · PITCH · FILTERING · MOD · DRIVE · PREAMP · TIME · GLITCH · [LOOPER] · NAM
```

- **PRESETS** becomes *pure macro launchers* — its 5 slots no longer host any
  effect that lives only here. Content is **mode-switched** (surf bank vs bass
  bank). See §5.
- **MOD** gains **TREMOLO** in its free slot (now a normal toggleable effect).
- **DRIVE** unchanged (5 guitar drives stay together).
- **PREAMP** is a new page (after DRIVE). See §4.
- **LOOPER** is a reserved stub inserted before NAM. Navigation/labels exist; guts
  are out of scope.

Menu reorg is UI + `midi_handler.orc` navigation only — no engine/signal changes
beyond the new effects. The Teensy→logical CC remap must not be touched.

---

## 4. New PREAMP menu page

A new menu page named **"PREAMP"**, sitting after DRIVE. Holds the preamp/voicing
family plus the bass distortion. The page grouping is a UI concern; each effect's
actual signal-chain position is set independently in `signal_chain.orc`.

| Slot | Effect | Toggle ID | Chain position | Notes |
|------|--------|-----------|----------------|-------|
| s0 | **MAIN PREAMP** | 37 | front input stage (where it was inherent) | Existing always-on preamp, extracted to a toggleable effect. **Default OFF.** |
| s1 | **BASS PREAMP LIGHT** | 38 | front input stage | Clean DI-ish bass voicing. Radio-paired with HEAVY. |
| s2 | **BASS PREAMP HEAVY** | 39 | front input stage | Crunchy preamp-driven bass voicing. Radio-paired with LIGHT. |
| s3 | **BASS DISTORTION** | 40 | drive region | Bass-optimized dynamic clipper, clean-low blend. |
| s4 | *(free)* | — | — | reserved |

Toggle-ID registry additions: **37 MainPreamp · 38 BassPreampLight · 39
BassPreampHeavy · 40 BassDist · 41 Tremolo.** (Previous next-free was 37.)

### 4.1 Extracting the MAIN PREAMP

The existing inherent input preamp becomes a normal bypassable effect:

- Add `gkPreamp_bypass init 1` (default bypassed/OFF) and guard the existing inline
  `Preamp` opcode call with `if (gkPreamp_bypass < 0.5)` at its current
  front-of-chain position (stage 0). It's an inline opcode, not an FX_BYPASS instr —
  see §12.1.
- **Consequence:** the default/dry sound and every existing preset previously got
  the preamp for free. After extraction it is OFF by default, so:
  - **All surf presets are reformatted to toggle MAIN PREAMP on first** (§6), so
    they sound exactly as before.
  - Bass presets use the *bass* preamps, not the main one.

### 4.2 The two bass preamps

Both **modeled on the existing complex MAIN PREAMP topology** (the user already
likes how it sits), but **re-voiced for bass**:

- Low end kept full and clean (don't clip/compress the fundamentals).
- Gain/crunch shaped on the mids and upper-mids so the tone stays tight rather
  than "farting out."
- **LIGHT** = clean DI-ish, subtle warmth. **HEAVY** = preamp-driven crunch, still
  short of full distortion.
- Voicing targets bass frequencies specifically — not a reuse of the guitar curve.

### 4.3 Bass distortion

A dedicated, bass-optimized, dynamic distortion (its own effect/toggle, reachable
individually in the PREAMP page):

- Clean low-band blend so the fundamentals stay solid under heavy clipping.
- Asymmetric/dynamic clipping voiced on the mids+highs for grind without mud.
- Lives in the drive region of the signal chain.

---

## 5. Mode switch: surf vs bass

One launch flag, consumed by **Csound only**. The UI never reads it.

### 5.1 Flag flow

1. **`live.conf`** carries a runtime key, e.g. `preset_bank=bass` (default `surf`
   when absent). Harmless/inert for a plain Demiurge user not running Csound.
   **Note:** the active config is `~/demiurge/live.conf` on the Pi (the repo's
   `live.conf` is stale) — see §12.2 for the real plumbing path and the open item.
2. The **launch path** reads that key and passes it to Csound as
   `--omacro:PRESET_BANK=…` (active path = the demiurge launcher; legacy reference
   = `scripts/launch_phase4.sh` `CSD_OPTS`).
3. Csound sets an internal `gkPresetBank` (0=surf, 1=bass).
4. The 5 PRESETS-button handlers **branch on `gkPresetBank`**: surf param-sets vs
   bass param-sets. In bass mode the surf param-sets are simply never loaded.
5. Csound emits one **`/preset_bank` OSC int** to the UI.

### 5.2 Layering (kept decoupled)

- **Layer A — what you launch:** the UI's companion preset menu selects a Demiurge
  patch (`demiurge-patch <name>`, the existing `~/demiurge/patches/*.conf`
  mechanism). A bass patch can set `preset_bank=bass`.
- **Layer B — the bank flag:** Csound only ever cares about the *flag value*.
  Whatever sets it (a live.conf default or the launched patch) is a separate
  concern. The patch *feeds* the flag; they are not welded together.

### 5.3 UI labels (single identical program)

The UI is byte-for-byte identical every launch and never reads the flag. It holds
**both** label tables (surf + bass) and draws the correct set based on the
`/preset_bank` OSC int. This matches the existing OSC-mirror pattern (the UI
already mirrors Csound menu state over OSC on `127.0.0.1:8000`).

---

## 6. Surf bank (reformatted)

Surf bank keeps its existing structure (exclusive base + stackable modifiers) but:

- Each surf preset **toggles MAIN PREAMP on first**, preserving the pre-extraction
  sound now that the preamp is no longer inherent.
- The PRESETS menu slots are pure macro launchers (TREMOLO/PHASER toggles are no
  longer *defined* here — those effects live in MOD; presets may still recall
  them as part of a macro).

---

## 7. Bass bank behavior

The 5 physical PRESETS buttons in bass mode:

| Slot | Button | Behavior |
|------|--------|----------|
| s0 | **PREAMP LIGHT** | radio → selects light bass preamp (**default on at boot**) |
| s1 | **PREAMP HEAVY** | radio → selects heavy bass preamp (deselects light) |
| s2 | **BASS CHORUS** | stackable modifier — bass-tuned chorus, low end kept dry |
| s3 | **BASS DIST** | stackable modifier — bass distortion, clean-low blend |
| s4 | **H90 WAMMY** | stackable modifier — H90 clamped `[0,+12]`, expression sweep, up-only |

Rules:

- **PREAMP LIGHT / HEAVY are a radio pair**: mutually exclusive, *always exactly
  one active*, default LIGHT. Selecting one deselects the other.
- **CHORUS / DIST / WAMMY are independent stackable modifiers**: they toggle on top
  of whatever preamp is selected and **never change the preamp selection**. E.g.
  HEAVY + DIST + CHORUS can run together; toggling CHORUS does not revert to LIGHT.
- This mirrors the surf bank's "exclusive base + stackable modifiers" UX, so the
  interaction model is consistent across modes.

### 7.1 Preset recipes (intent; exact param values tuned on hardware)

| # | Vibe | Recipe |
|---|------|--------|
| 1 | Clean preamp | Select BASS PREAMP LIGHT. Flat/clean DI, optional hair of attack/comp. |
| 2 | Crunchy, chill | Select BASS PREAMP HEAVY. Moderate preamp grit, low end clean, short of full dist. |
| 3 | Hip bass chorus | CHORUS modifier on current preamp: slowish rate, moderate depth, ~40% mix, low end kept dry. |
| 4 | Bass drive/dist | BASS DIST modifier on current preamp: high drive + clean-low blend. |
| 5 | Up-only wammy | H90 modifier on current preamp: clamp `[0,+12]`, expression pedal heel=unison → toe=+octave, mix to taste. |

---

## 8. Pitch shifter clampable-range refactor

Add an **internal min/max clamp** on the pitch-control of **all four** shifters —
H90, P-FORK, GPS, OLD PS.

- **Not exposed in the UI** and not a user-facing param.
- **Defaults = each shifter's current full range**, so normal launches behave
  exactly as today (no audible/behavioral change).
- Presets may set a non-default clamp. **Bass preset #5 is the first consumer**:
  it sets H90 to `[0, +12]` (up-only) and binds the **expression pedal** to sweep
  heel=unison → toe=+12, hard-clamped so it can never dip below unison.
- Mechanism: per-shifter `kMinShift` / `kMaxShift` (or equivalent) applied to the
  control value before it reaches the pitch engine; the expression-pedal mapping
  scales into `[kMinShift, kMaxShift]`.

H90 already handles up-shifts well (transient sidechain + anti-alias/warmth chain),
so it's the natural first home for the wammy.

---

## 9. Files touched (per the add-effect checklist)

Each new effect follows the established 5-file pattern. New effects:
MAIN PREAMP (extract), BASS PREAMP LIGHT, BASS PREAMP HEAVY, BASS DISTORTION,
TREMOLO (relocate to MOD as a normal effect).

- `csound/effects/<name>.orc` — new effect instrs (preamps, bass dist); tremolo
  already exists (instr 101) and gets a normal toggle.
- `csound/globals.orc` — bypass + param globals + audio buses for each new effect;
  `gkPreamp_bypass`, `gkPresetBank`, bass-preamp radio state.
- `csound/signal_chain.orc` — `FX_BYPASS` blocks at correct positions (preamps at
  front, bass dist in drive region); wrap the extracted main preamp.
- `csound/midi_handler.orc` — PREAMP menu slot toggles; TREMOLO MOD slot toggle;
  bass-bank vs surf-bank preset handlers branching on `gkPresetBank`; radio logic
  for bass preamps; `/preset_bank` OSC send; reformatted surf preset blocks
  (preamp-on-first); pitch clamp dispatch; toggle-ID registry + menu tables.
- `csound/neptrPhase4.csd` — `#include` new effect files; read `PRESET_BANK`
  macro into `gkPresetBank`.
- `neptrPhase4_UI.py` — add PREAMP menu + LOOPER stub to `MENUS`; both surf/bass
  label tables for the PRESETS menu; `/preset_bank` OSC handler; `EFFECT_PARAMS`
  for new effects.
- Launch script / `live.conf` — read `preset_bank` key, pass `--omacro`.

---

## 10. Testing & deploy

- Render test + smoketest of the UI (pygame) before pushing to the Pi.
- Verify both boots: default/surf (`preset_bank` absent) and `preset_bank=bass`.
- Confirm: main preamp OFF by default; surf presets still sound correct (preamp
  re-enabled); bass radio defaults to LIGHT; modifiers stack without resetting the
  preamp; H90 wammy clamps up-only on the expression pedal; all other pitch
  shifters behave exactly as before (default clamp = full range).
- Deploy/flash per the Teensy + `redem` workflow.

---

## 11. Open assumptions (correct if wrong)

- New page is literally named **"PREAMP"**.
- CHORUS / DIST / WAMMY in the bass bank are independently stackable (not mutually
  exclusive with each other).
- Bass distortion sits in the drive region of the chain; the three preamps sit at
  the front.
- `preset_bank` lives in `live.conf` (runtime key only).

---

## 12. Implementation anchors

All paths relative to project root unless absolute.

### 12.1 MAIN PREAMP (the inherent input preamp)

- **File:** `csound/effects/preamp.orc` (lines 1–145). Hudson Broadcast model —
  `opcode Preamp, aa, aa` (line 124), helper `opcode BroadcastChan, a, akiiiiii`
  (line 89). Discrete class-A germanium voicing: even-harmonic seasoning + 25 Hz
  HP, envelope-driven bias asymmetric tanh, 250 Hz transformer split with +1 dB
  @ 80 Hz / HF rolloff, 9 kHz fizz rolloff + makeup gain.
- **Called inline** in `signal_chain.orc` lines 54–57 (**stage 0**, first after
  input summing, before all effects). Comment: `; Bypass: none — inherently active`.
- **Voice selector:** `giPreVoice init 1` (preamp.orc line 87), i-rate: 0=SOFT,
  1=MEDIUM (default), 2=MAX.
- **Extraction:** it's an *inline opcode call*, not an FX_BYPASS instr, so the
  clean extraction is a `gkPreamp_bypass`-guarded if-block (not the instr-scheduling
  FX_BYPASS macro):
  ```csound
  gkPreamp_bypass init 1            ; globals.orc (near line 495); 0=ON,1=OFF
  ; signal_chain.orc ~line 56:
  if (gkPreamp_bypass < 0.5) then
    aPrevL, aPrevR Preamp aPrevL, aPrevR
  endif
  ```
- **Bass preamps** are modeled on this opcode; likely implemented the same way
  (inline opcodes guarded by their own bypass globals + a radio rule), bass-voiced.
  Inline-vs-instr for the bass preamps is an implementation-plan decision.

### 12.2 Launch / flag plumbing — IMPORTANT correction to §5

- **Repo `live.conf` is STALE** (header verified 2026-06-11). Active config is
  `~/demiurge/live.conf` on the Pi; `/boot/firmware/demiurge.conf` points
  `launch =` there. Csound is launched by the **demiurge launcher**
  (`demiurge-mode-select.sh`, referenced from the active live.conf), **not** by the
  repo's `scripts/launch_phase4.sh` (legacy, superseded 2026-04-24).
- **Legacy reference path** (`scripts/launch_phase4.sh`): parses `phase4.conf` via
  `get_conf()` (lines 47–51); builds `CSD_OPTS` (lines 403–415) and invokes
  `csound "${CSD_OPTS[@]}" "$CSD_ABS"` (~line 414). Adding `-+omacro:PRESET_BANK=…`
  to `CSD_OPTS` works here for local/legacy testing.
- **Active path (real target):** the `preset_bank` key goes in `~/demiurge/live.conf`
  and the demiurge launcher must pass `--omacro:PRESET_BANK=…` to csound. That
  launcher source is **not in this repo** → **OPEN ITEM** for the plan: confirm
  where demiurge injects csound options and whether we edit its launcher or wrap it.
  This sits in the user-owned demiurge/live.conf zone — coordinate, don't assume.
- **Csound side:** read the macro at init, e.g. `gkPresetBank init $PRESET_BANK`
  (with a `#ifndef PRESET_BANK` default of 0) in `globals.orc`/`neptrPhase4.csd`.

### 12.3 OSC `/preset_bank` (mirror the `/menu` pattern)

- **UDOs:** `OSC3x_i` / `OSC3x_f` in `globals.orc` (lines 1008–1027) — each send
  fires 3× to `127.0.0.1:8000` for reliability. Macro `$OSC_I(kVar'Saddr'uid)`
  (globals.orc lines 100–107) gates on `changed`.
- **`/menu` is sent** via `OSC3x_i "/menu", gkCurrentMenu` (midi_handler.orc lines
  167, 171) and `$OSC_I(gkCurrentMenu'/menu'menu)` in the instr 99 feedback block
  (line 1179).
- **Add:** `gkPresetBank init 0` (globals.orc), and
  `$OSC_I(gkPresetBank'/preset_bank'pbnk)` in the feedback block (~line 1183). Also
  send `OSC3x_i "/preset_bank", gkPresetBank` wherever the bank is set at init.

### 12.4 Surf preset handler blocks (add preamp-on-first)

Menu 0 dispatch in `midi_handler.orc` (physical layout comment lines 53–54: OD,
SOLID-DIST, PHASER, TREM, REVERB; OD/SOLID-DIST/REVERB mutually exclusive,
PHASER/TREM cross-preset modifiers). State var: `gkCurrentPreset init -1`
(globals.orc line 495). **No `SLOT_TO_PRESET` table on the Csound side** — presets
are a direct enum (1=OD, 2=SOLID, 3=AMBIENT, -1=DRY).

| Preset | Block lines | Sets (highlights) |
|--------|-------------|-------------------|
| **OVERDRIVE** (1) | 604–664 (ON ~627–651) | `gkCurrentPreset=1`, VIB on, spring delay on, `gkOD_bypass=0` (drive 0.75), DHG/reverb/trem/phaser off |
| **SOLID-DIST** (2) | 485–540 (ON ~509–534) | `gkCurrentPreset=2`, `gkDHG_bypass=0` (drive 0.4), OD/reverb off |
| **AMBIENT/REVERB** (3) | 179–259 (ON ~231–256) | `gkCurrentPreset=3`, `gkverbBypass=0` (FB 0.92, mix 0.70), `gkAmbBoost=2.1` |
| PHASER modifier | 395–441 | toggles `gkPHA_bypass`, sets Phase-90 params |
| TREMOLO modifier | 313–319 | toggles `gkTremBypass` (depth 0.7, rate 7.38 Hz) |

Preset base-layer globals: `giPresetVib*` / `giPresetDel*` (globals.orc lines
509–514). **Change:** add `gkPreamp_bypass = 0` as the first assignment in each of
the three preset ON-blocks so they re-enable the (now-toggleable) main preamp.

> Note: TREMOLO currently lives only as this Menu-0 modifier. The refactor moves it
> to a normal MOD effect (toggle ID 41); the surf preset macros may still set its
> params, but it is no longer *defined* solely in the preset menu.
