# Cheese Synth - Architecture Analysis

## System Overview

A ChucK-based generative synthesizer with MIDI control, 4 synthesis modes, and a 9-unit stereo effects chain. Audio flows from ADC + internal synth through a serial FX chain to DAC, all coordinated through a static global state class (`G`).

---

## File Inventory

| File | Role | Lines |
|------|------|-------|
| `globals.ck` | Static state container (class `G`) - all parameters, enums, init | 518 |
| `index.ck` | **Entry point** - signal routing, FX dynamic loading, master volume | 489 |
| `synth.ck` | Synthesis engine - 4 modes, note dispatch, sequencer, manual trigger | ~1210 |
| `midi.ck` | MIDI input listener, CC mapping, OSC output to UI | 461 |
| `volume.ck` | Volume pedal FX (gain control with smoothing) | 42 |
| `vibrato.ck` | Vibrato FX (LFO-modulated delay) | 80 |
| `chorus.ck` | Chorus FX (stereo LFO delay with phase offset) | 73 |
| `phaser.ck` | Phaser FX (4-stage BiQuad all-pass with feedback) | 156 |
| `pitchShifter.ck` | Pitch shift FX (PitShift wet/dry mix) | 57 |
| `ringmod.ck` | Ring modulator FX (SinOsc amplitude modulation) | 57 |
| `spectralWah.ck` | Spectral wah FX (FFT frequency scaling with energy normalization) | 196 |
| `delay1.ck` | Delay FX (modulated delay with pitch shift in feedback + tanh saturation) | 162 |
| `reverb1.ck` | Reverb FX (NRev wet/dry mix) | 41 |
| `fixed.ck` | **Stale/duplicate** - older version of globals.ck (different defaults, missing LOCKIN/vocal modes) | 313 |

---

## Interaction Map

### Boot Sequence
```
index.ck
  |-- @import globals.ck --> G.init()
  |-- Declares all global Gain/Delay/FFT/PitShift/NRev/SinOsc nodes
  |-- Wires stereo signal chain: adc + synthInput --> Vol --> Vib --> Chorus --> Phaser --> Ring --> Spectral --> Pitch --> Delay --> Reverb --> masterGain --> dac
  |-- Machine.add("synth.ck")   -- starts synthesis engine
  |-- 50ms wait
  |-- Machine.add("midi.ck")    -- starts MIDI listener
  |-- Machine.add("volume.ck")  -- starts volume pedal (always on at boot)
  |-- spork ~ dynamicConnectLoop()  -- manages FX bypass/connect
  |-- spork ~ masterVolumeLoop()    -- applies G.gkMasterVol to master gains
```

### Data Flow: MIDI --> Globals --> Audio
```
MIDI Controller
  |
  v
midi.ck (CC listener)
  |-- Reads CC messages from all MIDI devices
  |-- Maps CCs to G.* static fields (bypass toggles, rates, depths, etc.)
  |-- Sends OSC messages to 127.0.0.1:8000 (external UI)
  |
  v
G (globals.ck) -- shared state hub
  |
  +--> synth.ck reads: synthMode, classicWaveType, bpm, UI_NOTEprob,
  |    note_probs[], note_pitches[], octaveINIT, gkLockinLength,
  |    gkLockinPitch, gkFilterOffset, pitch/filter envelope params,
  |    phys_pos_x/y, vocal_pos_x/y, physModels[][], vocalSyllables[][],
  |    playing[], gkManualFreq/Midi/Trigger
  |
  +--> index.ck reads: all gk*Bypass flags, gk*OnSmooth values
  |    (drives dynamicConnectLoop for FX loading/unloading)
  |
  +--> Each FX .ck reads: its own gk* params (rate, depth, feedback, bypass, mix)
       and gkPortTime (smoothing coefficient)
```

### Signal Chain (Stereo)
```
adc.chan(0/1) --> adcInput(L/R) --+
                                   |
synth.ck output --> synthInput(L/R) --+--> preVol(L/R)
                                           |
  [Volume Pedal] volGain(L/R) --> postVol(L/R)
                                           |
  [Vibrato] vib(L/R) delay lines --> postVib(L/R)
                                           |
  [Chorus] chor(L/R) delay lines --> postChorus(L/R)
                                           |
  [Phaser] 4x BiQuad allpass + feedback --> postPhaser(L/R)
                                           |
  [Ring Mod] ringGain(L/R) * SinOsc --> postRing(L/R)
                                           |
  [Spectral Wah] FFT/IFFT freq scale --> postSpectral(L/R)
                                           |
  [Pitch Shift] PitShift + direct --> preDelay(L/R)
                                           |
  [Delay] DelayA + PitShift feedback + tanh sat --> postDelay(L/R)
                                           |
  [Reverb] NRev + direct mix --> postReverb(L/R)
                                           |
  masterGain(L/R) --> dac.chan(0/1)
```

### Dynamic FX Loading (index.ck `dynamicConnectLoop`)

Every 10ms, checks each FX bypass flag in `G`:
- **Bypass ON + smoothing faded out**: `Machine.remove()` the FX shred, disconnect wet path, reconnect dry bypass
- **Bypass OFF**: `Machine.add()` the FX .ck file, connect wet signal path

This saves CPU by fully unloading inactive effects and their shreds.

### Synth Modes (synth.ck)

| Mode | G.synthMode | Oscillator | Filter Env | Pitch Env | Note Trigger |
|------|-------------|-----------|------------|-----------|--------------|
| LOCKIN (0) | Detuned SawOsc x4 (L/R pairs) | Per-note random LPF sweep | Per-note, 15% chance pitch decay | Sequencer + manual |
| CLASSIC (1) | Sin/Tri/Saw/Pulse/Noise (selectable) | Disabled (20kHz open) | Via `pitchEnvelope()` | Sequencer + manual |
| VOCAL (2) | VocalSynth (formant, 14 syllables) | `filterEnvelope()` shared | Via freq tracking | Sequencer + manual |
| PHYSMOD (3) | STK instruments (14 models) | `filterEnvelope()` shared | Via freq tracking | Sequencer + manual |
| WAVETABLE (99) | Disabled/skipped in menu | -- | -- | -- |

### MIDI CC Map (midi.ck)

| CC | Function | Context |
|----|----------|---------|
| 20 | Master volume / VIB rate / CHORUS rate / PHASER rate / REVERB time | Menu-dependent + lastToggle |
| 22 | Reverb mix | lastToggle == 14 |
| 23 | Note probability / VIB feedback / CHORUS feedback / PHASER feedback / DELAY pitch offset | Menu-dependent + lastToggle |
| 27 | Grid X position (physmod/vocal) + Note length (if not gridlocked) | Always grid, conditional length |
| 28 | Grid Y position (physmod/vocal) | Always |
| 29 | Expression pedal: VOL / RM rate / PSHIFT semi / DELAY time / SPECWAH / VIB depth / CHORUS depth / PHASER depth | lastToggle dependent |
| 30 | BPM | Menu 0, not gridlocked |
| 32 | Filter offset (exponential curve) | Not gridlocked |
| 35 | Pitch offset | Not gridlocked |
| 63 | Synth mode cycle (0->1->2->3->0) | Button |
| 67 | Wave type cycle (classic mode) | Button |
| 68 | Manual note trigger | Button |
| 70 | Menu previous | Button |
| 72 | Reverb toggle | Menu 4 |
| 73 | Menu next | Button |
| 74 | Toggle: VOL/VIB/PSHIFT/DELAY | Menu-dependent |
| 75 | Toggle: RING/CHORUS/SPECWAH | Menu-dependent |
| 76 | Phaser toggle | Menu 2 |
| 77 | Grid lock toggle | Button |

### OSC Output (midi.ck --> external UI at 127.0.0.1:8000)

Every parameter change sends an OSC message (e.g., `/menu0_vol`, `/vib_freq`, `/delay_time`) so an external UI can reflect state.

---

## How Files Interact - Summary

1. **globals.ck** is `@import`ed by every file. Class `G` is the single shared-state bus. No file talks directly to another; all communication is through `G.*` static fields.

2. **index.ck** is the orchestrator: it declares all `global` audio nodes, wires the stereo chain, boots synth + MIDI, and runs the dynamic FX loader.

3. **midi.ck** is the sole input handler: it reads MIDI, mutates `G.*`, and sends OSC. It also handles manual note triggering by setting `G.gkManualFreq/Midi` and signaling `G.gkManualTrigger`.

4. **synth.ck** is the sound source: its sequencer loop fires notes based on `G.bpm` and `G.UI_NOTEprob`, dispatching to the correct synth mode. Each note is a sporked shred that creates oscillators, connects to `volGainL/R`, runs filter/pitch envelopes, then disconnects and cleans up.

5. **FX files** are loaded/unloaded dynamically by index.ck. Each reads its parameters from `G.*` in a control-rate loop and writes its `gk*OnSmooth` value back for the dynamic loader to use.

6. **fixed.ck** appears to be an older/stale version of globals.ck (no LOCKIN mode, no vocal syllables, different defaults). It's not referenced by any other file.

---

## Recommendations for Efficiency

### 1. Eliminate `fixed.ck`
It's a stale duplicate of `globals.ck` with older defaults and missing features (no LOCKIN, no VOCAL mode, no phaser/spectral/delay/reverb smoothing vars). Delete it to avoid confusion.

### 2. Replace Polling Loops with Event-Driven Updates
**Problem**: Every FX shred and `dynamicConnectLoop` polls `G.*` every 1-10ms regardless of whether values changed. `midi.ck` polls all MIDI devices in a tight loop.

**Fix**: Use ChucK `Event` objects for parameter changes. When `midi.ck` sets a value, it signals the relevant event. FX shreds wait on that event instead of spinning. This especially matters for:
- `dynamicConnectLoop` (10ms poll) -- only needs to check when a bypass flag actually changes
- FX shreds that poll at 1ms/1samp rate when parameters haven't moved

### 3. Consolidate the Smoothing Pattern
**Problem**: Every FX file independently implements the same exponential smoothing pattern:
```
Math.exp(-1.0 / (control_rate * G.gkPortTime)) => float coeff;
target * (1.0 - coeff) + smooth * coeff => smooth;
```
This is duplicated ~50 times across files.

**Fix**: Add a `Smoother` class to globals.ck:
```
class Smoother {
    float value;
    float coeff;
    fun void init(float initial, float portTime, float controlRate) { ... }
    fun float update(float target) { ... }
}
```

### 4. Reduce `prev*` Variable Bloat
**Problem**: `G` has ~30 `prev*` fields (prevVibRate, prevVibAmount, prevVibBypass, etc.) that appear to be for change detection, but most FX shreds don't actually use them -- they just read the current value every tick.

**Fix**: Audit and remove unused `prev*` fields. If change detection is needed, use Events (see #2).

### 5. Unify FX Bypass Architecture
**Problem**: Each FX has a slightly different bypass implementation in `dynamicConnectLoop`. The connect/disconnect logic is ~30 lines per effect, totaling ~250 lines of nearly identical code with subtle per-effect variations.

**Fix**: Create a generic FX slot abstraction:
```
class FXSlot {
    string filename;
    int shredID;
    int wetConnected;
    Gain @ preL, preR, postL, postR;
    fun void checkBypass(float bypass, float smooth) { ... }
}
```
Then `dynamicConnectLoop` becomes a simple loop over an array of FXSlots.

### 6. Deduplicate Note Selection Logic
**Problem**: The weighted-random note selection + octave offset logic is copy-pasted identically in:
- `synth.ck:sequencer()` (lines 1150-1183)
- `midi.ck:midi_listener()` CC68 handler (lines 400-432)

**Fix**: Move to a shared function in `G` or a utility class:
```
fun static void selectAndPlayNote() { ... }
```

### 7. Reduce Per-Note Allocation in LOCKIN Mode
**Problem**: `playLockinNote` creates 4 SawOsc + 2 LPF + 2 ADSR + 2 Gain per note, then runs a sample-rate modulation loop. With high note density (320 BPM, 0.6 probability), this creates many concurrent shreds each doing per-sample work.

**Fix options**:
- **Voice pooling**: Pre-allocate N voice objects, reuse them instead of creating/destroying per note
- **Reduce modulation rate**: The 5ms control loop in LOCKIN (line 768) is reasonable, but the `filterEnvelope` function runs at 1::samp rate (lines 398-450). Use a coarser control rate (e.g., 64 samples) for filter sweeps.

### 8. Spectral Wah: Avoid Per-Hop Allocation
**Problem**: `spectralWah.ck` allocates `float new_magsL[S]` and `float new_magsR[S]` arrays every FFT hop (lines 78, 135). With `FFT_SIZE=512`, `S=256`, this is 2 array allocations per ~3ms hop.

**Fix**: Move arrays outside the loop as persistent buffers.

### 9. Separate Concerns in `synth.ck`
**Problem**: `synth.ck` is 1210 lines containing: VocalSynth class definition, filter envelope, pitch envelope, 4 note players, sequencer, and manual trigger. This makes it hard to modify one synth mode without risk to others.

**Fix**: Extract into separate files:
- `vocal_synth.ck` - VocalSynth class
- `note_lockin.ck`, `note_classic.ck`, `note_vocal.ck`, `note_physmod.ck` - per-mode players
- Keep sequencer + dispatcher in `synth.ck`

### 10. Ring Mod Runs at Sample Rate Unnecessarily
**Problem**: `ringmod.ck` uses `1::samp` control rate (line 21), meaning the smoothing loop runs at 44.1kHz. The actual modulation is done by `ringMod.last()` which updates at audio rate anyway.

**Fix**: Use 1ms or 5ms control rate like the other effects. The `SinOsc` still runs at audio rate in the signal graph; you only need to update gain values at control rate.

### 11. OSC Redundancy
**Problem**: `midi.ck` sends individual OSC messages for every parameter tweak. Some CC handlers send 3-4 OSC messages at once (e.g., CC29 for delay sends time, mix, feedback, and pitch offset).

**Fix**: Consider batching OSC updates or only sending changed values. If the external UI polls, a single periodic state dump could replace dozens of individual sends.

### 12. The `gkLastToggle` State Machine is Fragile
**Problem**: CC29 (expression pedal) behavior depends entirely on `gkLastToggle`, which is set as a side effect of toggling effects. If a user toggles an effect off and back on, `lastToggle` changes even if they wanted to keep controlling the same parameter. The mapping is implicit (8=VOL, 9=VIB, 10=RING, 11=PITCH, 12=CHORUS, 13=DELAY, 14=REVERB, 15=SPECTRAL, 16=PHASER).

**Fix**: Make the expression pedal target explicit -- either track it as a named enum or tie it to the current menu + slot rather than the last-toggled effect.

---

## Applied Fixes

| # | Fix | File(s) | What Changed |
|---|-----|---------|-------------|
| 1 | Deleted `fixed.ck` | fixed.ck (removed) | Stale duplicate of globals.ck with older defaults, unreferenced by any file |
| 2 | Ring mod: sample-rate -> control-rate | ringmod.ck | `1::samp` -> `1::ms` for smoothing loop (~44100x -> 1000x/sec) |
| 3 | Spectral wah: persistent buffers | spectralWah.ck | `new_magsL[S]` and `new_magsR[S]` moved outside loop, zeroed each hop instead of reallocated |
| 4 | Remove 30+ unused `prev*` fields | globals.ck | All `prev*` static fields and their init lines removed (never read outside globals.ck) |
| 5 | Filter envelope: 1::samp -> 64::samp | synth.ck `filterEnvelope()` | All 4 phases (attack/decay/sustain/release) now stride 64 samples instead of 1 |
| 6 | Pitch envelope: 1::samp -> 64::samp | synth.ck `pitchEnvelope()` | Attack phase now strides 64 samples instead of 1 |
| 7 | Delay: sample-rate -> control-rate | delay1.ck | `1::samp` -> `1::ms` for smoothing loop |

## Remaining Recommendations (Not Applied)

| Priority | Item | Impact | Effort | Why Not Applied |
|----------|------|--------|--------|-----------------|
| Medium | Smoother class | Code cleanliness | Medium | Structural refactor, risk of subtle behavioral changes |
| Medium | FX slot abstraction | -250 lines in index.ck | Medium | Major refactor of signal routing logic |
| Medium | Event-driven param updates | CPU (idle polling) | Medium | Requires architectural change to all FX + midi.ck |
| Medium | Extract synth.ck modules | Maintainability | Medium | File splitting changes Machine.add paths |
| Medium | Deduplicate note selection | Prevents divergence | Low | Sequencer and manual trigger intentionally use different octave distributions |
| Low | Voice pooling | CPU at high polyphony | High | Major architectural change |
| Low | OSC batching | Network efficiency | Low | Requires external UI changes |
| Low | Explicit expression target | UX robustness | Low | Behavioral change to MIDI mapping |
