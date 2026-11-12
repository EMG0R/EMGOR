# PMOR v3 — Interface & Engine Spec (for sign-off)

Date: 2026-09-14. Source: planning session with EMGOR. Nothing in here is built until
EMGOR signs off. Items marked **[?]** are my proposals still awaiting a yes/no.

Rule of the build: **from-scratch rewrite in `DaisyExamples/seed/PMOR_V3/`**, modular,
porting only the code EMGOR likes and the locked hardware routines verbatim.
`PMOR_V2/` stays flashable as the fallback. Elegance over cleverness. One clean change at a time.

---

## 0. Hardware & locked facts (carried from PMOR-V2-STATE.md — DO NOT CHANGE)

- Daisy Seed 3, libDaisy v4.0.0, `APP_TYPE = BOOT_SRAM`, flash via Daisy bootloader (`DAISY_INFINITE_TIMEOUT`).
- All I2C bit-bang. NeoTrellis bus D11/D12; sensor bus D13/D14. Keep `waitScl` hardening, `BBus::recover()`, 250 ms backoff.
- SD: bit-bang SPI D25–D28, 3.3 V only, init clock `DelayUs(5)`, 250 ms settle, 160 preamble clocks, 5 s ACMD41, 3 retries. FAT32.
- OLED SSD130x 4-wire SPI (D8/D10/D7/D3/D4), `0xA0`+`0xC0`, `nop_delay = 48`, contrast `0x01`, `NVIC_DisableIRQ(SPI1_IRQn)` after init, 30-frame config re-assert.
- MPU6050 (IDs 0x68/0x69/0x98), no magnetometer probe. Accelerometer only — **gyro unused**.
- VL53L1X lidar on the sensor bus — **driver overhaul needed** (§11).
- Joystick X=D16 (adc1), Y=D17 (adc2), SW=D24. FSR = adc0. Encoder = enc1 (the only encoder).
- USB: TinyUSB composite (UAC2 capture + MIDI + CDC) from `usb/` — port as-is.
- Audio: 48 kHz / 64-sample blocks. Two planes: audio ISR (sequencer clock + DSP, never blocks) and main loop (I2C, OLED, sensors).
- Safety invariants stay: master clock untouchable by FX; trellis-down = all-notes-off + FX clear; orphan-voice sweep; 8 s gate cap; NaN flush; hard ±1 output.

### Calibration
- FSR idle raw ≈ **1600** → that is zero. 330 R pulldown, cubed curve, full span (`_fsr_scale.h`).
- Joystick: usable max raw = **950** per axis (edge catch is acceptable). Centre measured at boot (200-poll defer, 64-sample average).

---

## 1. Physical controls — global rules

| Control | Always | Notes |
|---|---|---|
| **Left encoder turn** | moves left/right through the current menu ring | hard stops at both ends, no wrap |
| **Left encoder click** | **master TAP TEMPO** | acts on press (impact). Nothing else is ever on this button. |
| **Joystick click** | context-dependent: push into a submenu / back out / set a parameter / un-focus a pad | acts on press |
| **Both buttons** | **instant VOLUME** shortcut | the second press undoes whatever the first single press did |
| **Joystick move** | context-dependent (nav / param / performance / FX push) | game-console nav: one flick = one step, hold = repeat at 8 steps/s after a short pause |
| **FSR** | modulates the held FX / amount | |
| **Accel X** | speed of any LFO-type FX; cutoff in synth performance | |
| **Accel Y** | reserved for FX mappings (§8) | |
| **Lidar** | siren pitch (theremin), freeze/grain FX, more at discretion | |

### Tap tempo
- Live from tap 2. Rolling average of the latest N taps, N grows 2 → 5. Next downbeat snaps to the latest tap (phase + period). Taps older than 2 s drop out.
- Tapped BPM is **unquantized**. BPM set via the joystick editor is **quantized to whole BPM**.
- Feeds the sequencer master clock and clock-out.

### Universal parameter editor (one function, every 1-D param)
- Click a param → full screen: title top, horizontal slider. Value **jumps to the stick immediately**.
- Joystick click = set & exit. Encoder turn = set & move to the next menu item (fast workflow).
- Quantized vs continuous decided per param; **KEY is continuous (−12…+12 st, microtonal allowed)**.

### 2-D parameter editor (only where two params pair naturally)
- Joystick = XY pad, big square on screen. Same set/exit rules.
- Approved pairs: **octave amount × octave probability**, **WIDTH × width probability**, **ADSR** (2×2 pages), **filter cutoff × resonance**, **filter env**, **pitch env** (glide in × glide out), per-algorithm timbre pairs at discretion. Nothing 2-D in GLOBALS.

---

## 2. Top-level menu ring (left encoder)

`DRUMS · SYNTH · SAMPLER · FX · PATCHES · GLOBALS · DASH` — hard stops, boots on last-loaded patch.

- Remove the menu-map strip at the top of the OLED. **Keep the saving star exactly as is**; particles never draw over it.
- Engines (DRUMS/SYNTH/SAMPLER) each show three big rounded cards left→right: **TIMBRE · SEQUENCING · PERFORMANCE**. Joystick L/R selects, click pushes in, click again backs out.

---

## 3. SYNTH

### 3.1 TIMBRE
- **Algorithm** selector (each algorithm = one saved preset with its own params + envelope): THIEF (plain-saw branch removed), FM, WAVE (wavetable), PLUCK (Karplus), SUPER (5-saw), CZ (phase distortion), FOLD, ADD (8-partial additive). DECIDED 2026-09-14.
- Per-algorithm unique params (2-D where natural).
- **ENVELOPE** — ADSR as 2-D pages.
- **FILTER** — cutoff × resonance, keytracked, main lowpass.
- **FILTER ENV**, **PITCH ENV** (glide in × glide out; separate from performance octaving).
- **WIDTH** — 2-D: width × probability of going that wide. Mono → fully generative panning.
- Engine volume 0–125 %.

### 3.2 SEQUENCING
- **MANUAL** is always the first mode (direct play, no recording).
- Sequencer is **played in, not typed in**.
- **ARP** button — plain arpeggiator over held/sequenced notes.
- **POLYPHONY** (min 1) next to **PROBABILITY** (master generative amount, also drives the arp).
  - poly 1: intelligent voice handling (fast ~20 ms release on steal, never a hard cut).
  - poly 2: voices at 25 % / 75 %. poly 3: 15 / 50 / 85. WIDTH ignored at 2 & 3. poly 4+: even spread, WIDTH active again.
- **Generative mode** = generative arpeggiator over the current scale; holding pads constrains it to those notes; release all → back to the full scale.
- **Octave displacement** — 2-D: amount (continuous, e.g. 1.5 or 3.3 octaves) × probability; max = all 5 octaves equal.
- **Bars** (≤16) and **steps per bar** (≤16), set separately on the pads (pad N = count), joystick click sets & exits. Shrinking keeps hidden data; growing duplicates the loop to fill.

### 3.3 PERFORMANCE
- Joystick **Y = direct octave up/down** with glide (Thief-style slide). Release → home octave.
- Joystick **X = master probability**.
- **Accel X = note-envelope macro**: plucky (short, sustain low) → long ambient swells (scales the timbre ADSR proportionally). DECIDED 2026-09-14.
- Cutoff override in performance: `syn.perf_cutoff` exists in the engine but is not mapped to a sensor (accel X is taken by the envelope macro).
- Cannot record/erase sequences here.

---

## 4. DRUMS (synthesized only — no samples)

### 4.1 Pads
- Quadrants: **bottom-left = KICKS, bottom-right = SNARES/CLAPS, top-right = HATS, top-left = PERCUSSION** (4 pads each).
- Each pad has **16 sounds**, index 0 = classic → 15 = wonky. Recipes in `research/drum-synthesis.md`.
- Default patch: kicks = phone-speaker kick, sub kick, 808, 909; hats = 2 short (hard-panned 25 %/75 %), long crash, medium; snares = tight, medium, big, clap; perc = 808 tom (random scale note), chirp (random note), + 2.
- Panning: kick & snare centre; hats fixed 25/75; **percussion picks a new random pan 25–75 % per hit**. No WIDTH for drums.
- Key: **kick = root always**; toms weighted by circle-of-fifths distance (never 1; 5 most likely, then 2, 3, 6, 7, exponentially decaying), relative to the mode. Snares/noise ignore key.

### 4.2 TIMBRE
- Pads play normally. **Hold a pad → it goes white and stays focused** after release; further clicks keep focus; joystick L/R swipes its 16 sounds; clicking another pad moves focus; **joystick click un-focuses**.
- Per-sound uniform params: **length ratio** (scales every envelope time together), pitch, tone, plus envelope / filter / pitch-env pages at discretion (same scheme as synth).
- **Reverse gesture**: while a pad is focused, **two full joystick circles** CCW = reverse (time-reversed envelopes), CW = forward. Broad detection.
- Group volumes at the bottom: **KICK / HAT / SNARE / PERC, 0–125 %**. Sounds level-matched at build time.

### 4.3 SEQUENCING
- Grid sequencer, **all 16 pads independent lanes**. MANUAL first. Bars/steps as §3.2. Generative mode with master probability.

### 4.4 PERFORMANCE
- Joystick: **UP = spring reverb, RIGHT = delay throw (~50/50), DOWN = FL-style distortion (moderate), LEFT = bitcrush**. Release cuts the input and lets tails ring.
- No accel/gyro for drums. Generative + performance combo: open brief — "go nuts", intuitive.

---

## 5. SAMPLER

- 16 slots (one per pad), on SD. Filled pads always visibly brighter, even in sequencer mode.
- **Record starts exactly on press, stops on release**, with anti-click fades. Cannot record over a slot: **hold 5 s = delete** (pad fills as a visual countdown), then re-record.
- TIMBRE: pad focus as drums; **joystick Y = pitch ±1 octave, X = formant**, snapping live while focused. Filter kept, open by default. WIDTH as synth. Reverse gesture reverses the audio.
- SEQUENCING: as drums, incl. delay throws. TIMBRE and PERFORMANCE cannot record/erase sequences.
- PERFORMANCE: as drums (throws etc.).
- Engine volume 0–125 %.

---

## 6. FX (master menu; submenus collapsed into one joystick space)

- Home page = 16 pads, close to the current layout, lightly reorganized: strongest / most dramatic nearest the centre.
- Push the joystick in a direction → that family's row slides to the bottom and **12 more pads of that family** appear. 4 × 12 + 16 = **64 effect pads**. **No latch** — snaps home on release. A pad held while the stick moves stays held → effects from different directions stack.
- Families: **UP = stutter / glitch / stabs + spatialization**, **DOWN = distortion / ring mod**, **LEFT = reverse**, **RIGHT = modulation** (chorus, pan, pan-LFO, siren…).
- Cut the weak effects (e.g. mono); keep it full.
- Every effect is **FSR-modulated only**. LFO-type effects: FSR = amount, accel X = speed, smooth on release.
- **FILTER pad**: hold → joystick L/R sweeps the filter; release leaves it where it is.
- **SIREN**: pitch on the lidar (theremin). **Lidar freeze/grain** effect approved, placement at discretion.
- FX automation spans the **longest** engine loop, not one bar.
- Anti-click fades on every on/off and dry/wet; **5 ms crossfade** when switching distortion algorithms. Distortion algorithm set to be revisited and locked.
- Colours: four family colours, home page blends all four, colour animates toward the push direction.

---

## 7. PATCHES

- 4 pages × 16 slots = 64 patches on SD. Joystick L/R pages. Click = load (sound only, stays on page). Hold 5 s = delete (countdown fill).
- No per-page colours (keep the yellow).
- Autosave debounced ~2 s after the last change; star shows only when written.

---

## 8. GLOBALS (1-D params only)

KEY (continuous ±12 st) · SCALE · BPM (quantised) · INPUT GAIN · TRELLIS BRIGHTNESS (default 0.45; 0 → max) · OLED BRIGHTNESS (contrast 1..32, default 1 = the locked-safe value) · CLOCK OUT MODE (PO sync SY0–SY5) · ROTATE (trellis 0/90/180/270) · SWING · FACTORY RESET. No output volume here (that's the both-button shortcut).

**Global, not per-patch (2026-09-14):** master fader (shortcut), input gain, trellis brightness, OLED brightness, key/scale, swing, BPM. Stored in QSPI, saved 3 s after the last change. Everything else lives in the patch.

---

## 9. Output stage & levels

- Master fader: 0–75 % of today's range **is** the new 0–100 %. Above that only drives the clipper harder.
- **Always-on Fruity-Soft-Clipper-style end stage**: memoryless soft-knee waveshaper (parabolic knee, slope 1 at knee, flat at ceiling), default threshold −4.4 dB, no lookahead, 1× (2× optional). Transparent on quiet material. Reference in `research/clipper-and-posync.md`.
- Engine and group volumes max 125 % → lean into the clipper tastefully. Sampler's hot level tamed.

---

## 10. Screen & trellis visuals

- OLED 128×64 landscape. Three-card menus; full-screen 1-D/2-D editors.
- **Screensaver**: amplitude-normalized so it fills the screen quiet or loud; particles moving along the outline, fully audio-reactive; stays on while pads are played, dismissed by nav only; ≤ ~96 particles, fixed-point, every other frame.
- Particles during synthesis in general. Dashboard reorganized (no magnetometer rows, joy raw rows cleaned).
- Trellis: **lava-lamp base under everything, no pad ever fully off**. Full palette redesign around housing **RGB(52,235,171)** (mint) — swatches for approval before use.
- Trellis 90° rotation: still wanted (remap LED buffer and key decode).

---

## 11. Lidar overhaul

- Reads 0 at ~4 in; fine further out. Same sensor works close-up on another MCU → driver issue. Check range-status handling, distance mode (short vs long), timing budget, and ROI in `VL53L1X_mini`. Skip-on-fail at boot **[?]**.

---

## 12. Build order (each stage flashable and tested before the next)

1. Scaffold + verbatim hardware port (I2C, trellis, OLED, SD, USB, DFU) — boots, blank UI.
2. Nav framework: menu ring, cards, 1-D/2-D editor, tap tempo, both-button volume, game-style repeat.
3. Output stage (clipper, level plan) + GLOBALS.
4. DRUMS (engine, 64 sounds, timbre/seq/perf).
5. SYNTH.
6. SAMPLER.
7. FX space.
8. PATCHES, screensaver/particles, palette, dashboard, lidar, clock in/out.

---

## Decided 2026-09-14
1. Synth performance: joystick Y = octave; accel X = envelope macro.
2. Algorithms: all eight above.
3. Git: PMOR_V3 is a repo from the first commit (no remote yet — push it somewhere).
4. Lidar: init failure never blocks boot (it already didn't); driver overhauled (per-read DSS update, manual calibration, SHORT mode 33 ms / 40 ms IM, centre 8×8 ROI, 3-read hold before reporting 0).

## FX timing rule (2026-09-14)
Clocked effects engage on the NEXT boundary of their subdivision (ST4 = beat, ST8 = 8th, ST16/BZZ/gates/repeats = 16th, REV/HALF/DBL = beat). A press within 12 % of a 16th after a boundary counts as on time. Release is immediate (with fade). Releasing before the boundary cancels.
