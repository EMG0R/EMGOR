# HYPERTRUMPET V2 simulator

The firmware's C++ FX engine (`HYPER_CORE/src/dsp/fx/**`: engine, registry, every effect, `mapping/fx_mapping.cpp`)
compiled to WebAssembly, running in an AudioWorklet behind a custom-drawn canvas UI. Hear and poke the FX and the
controls before the hardware exists.

## Run
```
sim/run.sh              # builds the wasm if missing/older than any HYPER_CORE/src or sim/src file, serves, prints URL
sim/run.sh --no-build   # serve only
sim/build.sh            # build only  (needs: brew install emscripten; uses em++)
node sim/verify.mjs     # node wasm tests + headless Chrome check  (--node-only to skip the browser)
```
Default port 8765, 127.0.0.1 only. If another app owns it (on this Mac `TheWatchM` does), serve.mjs takes the next
free port; read the `URL:` line it prints. Click START (browsers need a gesture), pick SYNTH / FILE / MIC.

## Shared code with the firmware
Compiled from `HYPER_CORE/src`, untouched: `dsp/fx/fx_engine.cpp`, `fx_registry.cpp` (and the effect headers it
includes), `fx_pitchtrack.h`, `mapping/fx_mapping.cpp`. The sim adds only `src/sim_api.cpp` (flat C API),
`src/trumpet_synth.cpp` (synthetic horn). No per-effect code: the UI lists effects from the registry and draws sliders
from each effect's `Param` descriptors (`fx->pt[]`: name, min, max, step, flags, enum names, role). All control->FX
behaviour goes through `HxMapper::update()`; the UI only fills `HxControls` (packed float array) and the worklet calls
update every 4th 64-sample block (187.5 Hz of audio time, deterministic, independent of UI frame rate).
New or rewritten effects, a new mapper, a new pitch tracker: just rebuild.

Threading: single-threaded wasm. `FxEngine::setEffect` must never run in the audio thread on hardware; here it runs
from the worklet's message handler between `process()` calls, so it cannot overlap audio. Blocks are 64 samples
(2 per 128-frame quantum), the firmware block size. Arena: 4 x 12 MB static buffer, wasm memory 96 MB fixed.

Build flags: `-O3 -ffast-math -fno-finite-math-only`. Plain `-ffast-math` would let the compiler delete
`hx::finite()` (the engine's NaN/Inf output guard); `HX_FASTMATH=0 ./build.sh` builds without fast-math.

## Controls and keys
| control | mouse / touch | keys |
|---|---|---|
| valves 1 2 3 | drag sliders (continuous Hall depth, spring to 0); >= 0.5 acts pressed, below = half-valve bend | J K L (hold). A S D are Trellis keys |
| breath | BREATH slider, wheel on the TRUMPET panel; LATCH = always on | SPACE or SHIFT = gate (LATCH off) |
| tongue | TONGUE button | T |
| partial 2..9 | buttons | Up / Down |
| vibrato rate/depth | sliders | |
| faders 1-3, pots 1-2, lidar | drag | |
| FSR | drag/hold the slider, springs to 0 on release | |
| joystick + click | drag square (springs to centre); double-click = click | G (held) |
| encoders 1, 2 | drag vertically or wheel = detents; centre button = click (held) | [ ] / - =  ; click B / N |
| tilt pad | drag (springs back); MOTION = device orientation (phone, asks permission) | |
| shake | SHAKE button (held) | |
| Trellis 4x4 | click/touch; lit by `HxMapper::leds()` | 1234 / QWER / ASDF / ZXCV |
| slots | click name = effect list; ON/BYPASS; drag param bars; MAIN/ALT/CLEAR/HOLD = `Fx::action` 0..3 | |
| help | ? button | H or ? |

Input sources: SYNTH (built-in Bb trumpet: open series of Bb2, valve drops 2 / 1 / 3 semitones that add, glide 6-18 ms,
brass spectrum brighter when louder, formant bump ~1.3 kHz, breath noise, airy half-valve zone); FILE (button or
drag-drop WAV/MP3, mono-mixed, looped in the worklet); MIC (getUserMedia with echoCancellation, noiseSuppression and
autoGainControl off). In SYNTH mode the f0 derived from the valves is passed to `engine.setValveHint()`.

## Meters
* IN / OUT: peak since last read, smoothed for display (engine `readInPeak/readOutPeak`).
* WASM DSP %: wall time of one `sim_process` call (synth + engine + mapper) averaged over ~64 quanta (~170 ms), divided
  by 64 samples / 48 kHz = 1.33 ms. This is WASM on this laptop. It is NOT Seed (Cortex-M7 480 MHz) timing; do not
  convert it to a Seed budget.
* Clock: `AudioWorkletGlobalScope` may lack `performance.now` (headless Chrome did). The wasm imports `env.now_ms`;
  it uses `performance.now()` when present, else `Date.now()` (1 ms resolution). The headline number stays unbiased
  (ms quantisation averages out over many quanta) but the per-slot bars (engine EMA of single blocks) are noisy on that
  fallback; the box prints "(1ms clk)" then.
* f0 hint = pitch from the valve logic; f0 tracked = `PitchTrack` output (0 while the tracker is a stub or unvoiced).

## Known limits
Audio is for checking behaviour, not judging tone: synthetic trumpet is a simple additive model, not the real bell mic
(which is a few mV dynamic mic, no preamp). Output goes to the laptop DAC; wireless/Seed codec behaviour is not
modelled. Latency is the browser's (typically 10-30 ms), not the instrument's. Phone use works through touch events
but the layout is a scaled fixed 1280x900 canvas. Verified in headless Chrome only; see verify.mjs.
