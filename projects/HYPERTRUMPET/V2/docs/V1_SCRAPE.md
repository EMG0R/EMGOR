# hyperTrumpet v1 scrape

Sources (read-only): `hyperTrumpet/PERFRMNC.maxpat` (Max 9.0.9, 763 newobj, 35 subpatchers), `midiinHyperTrump.maxpat`, `hyperTrumpCode/hyperTrumpCode.ino`.
Status: complete (sections 1-8). Read-only scrape; nothing under the source dir was modified.

## 1. Hardware (from .ino)

Board: Teensy 4.1, USB MIDI (Tools > USB Type > MIDI). All MIDI on **channel 3**. Serial 115200 is debug text only (init banner). Libs: Adafruit_NeoPixel, Adafruit_NeoTrellis, Adafruit_BME680, Adafruit_VL53L0X, Encoder; IMU by raw I2C. `analogReadResolution(10)`, I2C 400 kHz on both buses.

I2C: `Wire` (SDA18/SCL19): NeoTrellis 0x2E, BME680 0x77 (fallback 0x76), VL53L0X 0x29. `Wire1` (SDA17/SCL16): MPU6500-class IMU 0x68.

### Send helpers
- `sendCC` (analog): sends only if `|new - prev| >= 2` (or first send). No further smoothing beyond per-read averaging below.
- `sendCCDigital`: sends on change only.
- `readAnalogSmoothed`: mean of 4 `analogRead` (10-bit) then `map(0..1023 -> 0..127)`.
- Loop ends with `usbMIDI.send_now(); while(usbMIDI.read());` (incoming MIDI discarded).

### Analog inputs (50 Hz block, every 20 ms)
| CC | Name | Pin | Read | Scaling | Notes |
|---|---|---|---|---|---|
| 20 | Slider 1 | A0 (14) | 4x avg | map(2..125 -> 0..127), clamp | |
| 21 | Slider 2 | A1 (15) | 4x avg | same | |
| 22 | Slider 3 | A6 (20) | 4x avg | same | |
| 24 | FSR 1 | A7 (21) | 4x avg | map(2..50 -> 0..127), clamp | very sensitive (top of range at ~1/20 scale) |
| 28 | Pot 1 | A13 (27) | 4x avg | map(2..125 -> 0..127) | |
| 29 | Pot 2 | A14 (40, bottom pad) | 4x avg | same | |
| 30 | Joystick X ("vertical") | A10 (24) | 4x avg | `127 - reading` (flipped: 127 = up) | no clamp/deadband |
| 31 | Joystick Y ("horizontal") | A11 (25) | 4x avg | direct, 0=left 127=right | |

Note: the averaged value is mapped to 0..127 first, then the 2..125 map is applied to that 0..127 value (map input range 2..125 of an already 0..127 number), so sliders/pots saturate at 125/127 raw-scaled.

### Contact mics (100 Hz, every 10 ms)
| CC | Pin | Method |
|---|---|---|
| 48 | A12 (26) | peak of `abs(analogRead - 512)` over 10 back-to-back samples; map(0..400 -> 0..127), clamp |
| 49 | A9 (23) | same |

Envelope-style peak detect, no release smoothing (only the >=2 change gate).

### Digital (polled every loop, INPUT_PULLUP, active LOW -> 127, released -> 0)
| CC | Name | Pin |
|---|---|---|
| 33 | Valve 1 (Hall sensor) | 2 |
| 34 | Valve 2 (Hall) | 3 |
| 35 | Valve 3 (Hall) | 4 |
| 32 | Joystick button | 5 |
| 40 | Arcade button | 6 |
| 38 | Encoder 1 button | 9 |
| 39 | Encoder 2 button | 12 |
No debounce in firmware.

### Encoders (every loop)
Encoder lib, pins enc1 A7/B8, enc2 A10/B11. `pos = read()/4` (one detent = 4 counts). On change sends **relative**: CC36 (enc1) / CC37 (enc2) value **127 for clockwise, 0 for counter-clockwise**, one message per detent.

### VL53L0X lidar / ToF, CC23 (30 Hz, every 33 ms)
Long-range mode. Range status 4 (out of range) -> 333 mm. Clamp 56..333 mm, 8-sample moving average (buffer preset to 180 mm), `map(avg, 56..333 -> 127..0)` so **close = 127, far = 0**.

### IMU (MPU6500 @0x68 on Wire1, 50 Hz)
Init: reg 0x6B=0 (wake), 0x1C=0x08 (accel +-4 g), 0x1B=0x08 (gyro +-500 dps), 0x1A=0x04 (DLPF). Reads big-endian int16 from 0x3B/3D/3F (accel) and 0x43/45/47 (gyro). Each `map(-8000..8000 -> 0..127)`, clamp (centre 64).
CC41/42/43 = accel X/Y/Z. CC44/45/46 = gyro X/Y/Z. (Accel at +-4 g is +-8192 LSB/g*... full scale 32768 -> +-8000 is ~+-1 g; gyro +-8000 raw at 500 dps = ~+-122 dps.)

### BME680 breath, CC47 (async, as fast as the sensor allows)
Pressure oversample 2x, temp 1x, humidity off, IIR 0, gas heater off. Baseline = mean of 10 readings at boot (hPa). Per read: `delta = max(|P - baseline| - 0.02 hPa, 0)`; `CC = clamp(delta * 127/0.3, 0..127)`. So 0.02 hPa deadband, full scale at 0.32 hPa deviation (absolute value: positive and negative pressure both register). Baseline is never re-zeroed (drift).

### NeoTrellis 4x4 (0x2E), CCs 50..65
Key n sends CC (50+n) = 127 on rising, 0 on falling. n = row*4+col (row = n/4). Also drives ripple animation.

### LEDs (output only)
- Strip 1: 9 NeoPixels pin 29; strip 2: 6 NeoPixels pin 28; brightness 60. Purple palette breathing; strip1 px0-2 flash on valves, px3-4 on breath, px5-6 on tilt, px7-8 on mic1 >30. Strip 2 breath speed from lidar, px0-1 mic1, px4-5 mic2, px2-3 breath, px0-2 valves.
- Trellis 25 Hz generative purple lava-lamp (5 summed sines through a 16-colour palette) modulated by lidar (speed 1x..4x), joystick/accel X (hue shift), sliders/joystick activity (brightness), FSR (brightness+checkerboard), pots (diagonal lines), joystick (arrow lights), ripples on key press (max 8, 2.5 s).
Not needed for the Bela audio path; keep only if the controller keeps the Teensy.

### Full CC table
20 S1, 21 S2, 22 S3, 23 lidar, 24 FSR, 28 pot1, 29 pot2, 30 joyX(flipped), 31 joyY, 32 joyBtn, 33-35 valves, 36 enc1 (127/0 rel), 37 enc2 (127/0 rel), 38 enc1 btn, 39 enc2 btn, 40 arcade, 41-43 accel, 44-46 gyro, 47 breath, 48 mic1, 49 mic2, 50-65 trellis. (CC25-27 unused.)

## 2. MIDI input layer (midiinHyperTrump.maxpat)

Structure: two `ctlin` objects (no args = **all channels, all devices**; channel ignored; no `midiin` port selection in the patch). Left outlet = value, middle = CC number. CC number -> `== N` -> opens a `gate` whose value input is the value from the second `ctlin`. Gated value goes to a `live.dial`/`live.slider`/`live.toggle`/`number` (monitor/UI, range 0-127 default) and then to a global `s <name>`. Values are passed raw 0..127 (toggles give 0/1). Receivers are `r <name>` objects in PERFRMNC.

| CC | Widget | Send name |
|---|---|---|
| 20 | live.slider | trSlid1 |
| 21 | live.slider | trSlid2 |
| 22 | live.slider | trSlid3 |
| 23 | live.dial | trLidar |
| 24 | live.dial | trFSR |
| 28 | live.dial | trPot1 |
| 29 | live.dial | trPot2 |
| 30 | live.dial | trJoyX |
| 31 | live.dial | trJoyY |
| 32 | live.toggle | trJoyButt |
| 33, 34, 35, 47 | live.dial each | **all four -> trMag1** (valves 1-3 and breath share one send; last writer wins; v1 flaw/WIP) |
| 36 | `p +-` -> buttons | trEnc1Neg (msg 0, when value 0) / trEnc1Pos (msg 1, when value 127); `delay 10` inside |
| 37 | `p +-` | trEnc2Neg / trEnc2Pos |
| 38 | live.toggle | trEnc1Butt |
| 39 | live.toggle | trEnc2Butt |
| 41 | number | trAcel (accel X only) |
| 42, 43 | number | monitor only (no send) |
| 44-46 | number | monitor only (gyro, no send) |
| 48, 49 | none | **mics not mapped** |
| 40 | none | **arcade button not mapped** |
| 50..65 | live.toggle | trButt1..trButt16 (CC50 -> trButt1 ... CC65 -> trButt16; order within is 50->1, 51->2, 52->3, 53->4, 54->5 ... 65->16). Toggle: 127 -> 1 (press), 0 -> 0 (release). |

Consequence: every trButt send emits 1 on press and 0 on release (two events per physical press); PERFRMNC counters count both events (see section 5).


## 3. Signal flow audit (PERFRMNC.maxpat)

Method: parsed JSON, walked all 35 subpatchers, built per-patcher netlists, checked for send~/receive~/throw~/catch~ (none exist), `s`/`r` pairs, unconnected boxes, and cycles. **All audio is hard-wired patch cords; there is no send~/receive~, no poly~, no gen~, no codebox, no bpatcher.** The only `s`/`r` traffic is control (trXXX from the MIDI patch, `INIT`, and looper-internal `Rec/Play/Stop/Ramp/RampEnd/deleteLAST/deleteALL/LOOP*IN`).

### 3.1 Serial chain, adc~ to dac~ (strictly serial at top level; parallelism only inside blocks)

```
adc~ (ch1, ch2; no args = 2 ch)
  |-- *~ -0.3 --> sum of BOTH adc~ channels  -> L bus   (polarity inverted, -10.5 dB)
  '-- *~ -0.3 --> sum of BOTH adc~ channels  -> R bus   (identical: dual-mono from here)
        |
 [1] p shiftr      harmonizer: dry + up to 2 pfft~ gizmo voices per channel   (enc1)
        |
 [2] rnbo~ looper  12-layer looper; dry gain = 1 - layers/12; loop sum * volume  (btn13/btn14)
        |
 [3] p freeze      dry (unity, additive) + 0.65 * [Spectral Freezing AU] * mix   (L/R swapped inside)
        |
 [4] p drone       dry*(1-mix) + [Spectral DroneMaker AU]*mix                    (slider1)
        |
 [5] p blur        dry*(1-mix) + [Spectral Blurring AU]*mix                      (btn7)
        |
 [6] p flanger     [FLANGER_CSOUND Cabbage AU], mix param only                   (lidar, inverted)
        |
 [7] p chorus      abl.dsp.chorus~, mix 0/1                                      (btn8)
        |
 [8] p MOOD        dry*(1-w) + supervp.trans~ * w ; w = random-gated mood prob   (joystick)
        |
 [9] p superMassive   dry*(1-mix) + 0.43*(3 parallel ValhallaSupermassive)*mix  (slider3 + accel X)
        |
[10] p mcCurdyGrain  octave-up AU shifter (6% gated) -> grainDelayMcCurdy; dry/wet (slider2)
        |
[11] p absDelay    abl.device.delay~ -> tanh~ -> *0.62 -> dry/wet w/ 250 ms mix ramp (btn9-12, FSR)
        |
[12] p roar        abl.device.roar~ (no external dry) * scale~ gain 0.3..1       (pot1)
        |
[13] p ringmod     abl.dsp.ringmod~  freq 0-800 Hz, mix 0/1                      (FSR, btn5)
        |
dac~ (ch1, ch2)
```
Orphans (in patch but not in the audio path): `p autopan` (obj-207, fed only by the `panr` dial; audio inlets/outlets unconnected), `p lil rando` (obj-200, no inlets/outlets, random 0-127 generator at metro 4n driving nothing).

### 3.2 Per-block detail: parallel/serial, wet/dry, gain staging

**Input stage.** Both `*~ -0.3` receive adc~ L and adc~ R summed (4 cords). If the trumpet mic is on one channel only, level = 0.3 and phase is inverted; the other channel adds its noise. No limiter or ceiling anywhere in the chain.

**[1] p shiftr** (parallel inside): per channel: dry `*~ g_dry`; voice A `pfft~ gizmo_loadme 1024 4` (L: obj-6, R: obj-102) with ratio from `transratio` obj-45; voice B `pfft~` (L: obj-87, R: obj-100) ratio from obj-86. Outputs summed: out = dry*g_dry + A*g_A + B*g_B. Gains set by state messages: neutral (idx 5): dry 1, A 0, B 0. Single voice: A 0.6, B 0, dry unchanged from previous state (1 after neutral, 0.5 after a two-voice state; state-dependent level, likely unintended). Two voice: dry 0.5, A 0.5, B 0.5. All gain changes are instantaneous (no ramp; pfft~ ratio changes are also instantaneous, clicks likely).

**[2] rnbo~ looper**: in~1/in~2 -> out: `in*(1 - n/12)` where n = layer counter 0..12 (`scale 0 12 0 1`, `-1`, `*-1`), plus loop sum * `fade~` (10 ms ramp) volume `dbtoa(scale -100..100 -> -18..+18 dB)`, param `volume` `@min -100 @max 100 @value 1` -> +0.18 dB at default, so the loop sum is ~unity. Dry fades toward 0 as layers accumulate (12 layers = dry fully muted). Looper sums 12 groove~ pairs through a chain of `+~`.

**[3] p freeze**: **L and R are swapped** (inlet 0 goes to the right dry/right AU input and vice versa; verified by inlet x-order). Mix block (`delayMIX` variant) adds dry at unity always (no dry attenuation: the (1-mix) multipliers have no signal input) and adds `0.65 * AU * mix`. The `frywet`/IN5 line~ chain is unconnected at the output. `sig bypasser`: `block` toggle (live.toggle[3], default 1 via loadmess 1 at load, btn4) drives `1 - block` through a 250 ms `line~` multiplied onto the AU inputs: block=1 -> AU input gated to 0 (it holds/frees whatever is frozen), block=0 -> input passes. Mix dial `freezing` default 127 (=1.0, loadmess 127).

**[4] p drone / [5] p blur**: parallel dry/wet: AU gets L,R; `blurMIX` = out = dry*(1-mix) + AU*mix, per channel, linear (not equal-power), mix is a float in a signal inlet (no smoothing in these two). Both AUs are in series with everything else.

**[6] p flanger**: single AU, mix param = IN2. No external dry/wet. Because the AU's internal ThruZero=1, mix=0 returns the **inverted** delayed dry (see 4.6).

**[7] p chorus**: one `abl.dsp.chorus~`, internal mix attr only.

**[8] p MOOD**: `pshiftMIX` = dry*(1-w) + supervp.trans~*w, w = `line~` 100 ms ramp (0 or 1) from `automood` outlet 1. An `abl.device.redux~` exists here but has **no signal input and its output is not connected** (dead branch; attrui/scale chain still runs).

**[9] p superMassive**: dry always goes to `verbMIX` and to **three** parallel `vst~ ValhallaSupermassive`. Wet combination (each x0.43): wetL = 0.43*(V1.L + V1.R) + 0.43*V2.L; wetR = 0.43*V2.R + 0.43*(V3.L + V3.R). V1/V3 outputs are summed L+R into one side; V2 stays stereo. Then verbMIX: dry*(1-mix) + wet*mix. Instances differ only in `Mode` (0.0417, 0.2083, 0.8333 normalized) and `DelayNote` (0.327, 0.301, 0.258).

**[10] p mcCurdyGrain**: parallel dry/wet at two stages in series: stage A `pshiftMIX`(obj-18): dry vs `Spectral Pitch Shift` AU (+1200 cents saved), mix = `autooct` line~ (0/1). Output of stage A -> `grainDelayMcCurdy` AU (internal mix 1.0, level 1) -> stage B `pshiftMIX`(obj-19): dry = original input (not the stage-A output!) vs grain wet, mix = `grains` dial (slider2). So the octave-up shifter only affects what feeds the grain engine.

**[11] p absDelay**: `abl.device.delay~` fed L/R directly (its own mix attr not driven by mix dial). Delay out -> `tanh~` -> `*~ 0.62` -> `delayMIX` wet; dry into `delayMIX` -> out = dry*(1-mix) + wet*mix with every gain through `line~ 250` (250 ms). `tanh~` is outside the feedback loop (the delay's internal feedback, attr max 0.95, is untouched by tanh) so it only soft-clips the wet output.

**[12] p roar**: `abl.device.roar~` L/R in, outputs both multiplied by `scale~ 0 1 0.3 1 2` of (1 - amount): gain = 0.3 + 0.7*(1-a)^2 (exponent 2). No external dry. Roar internal `mix`, `routing`, shaper type/filter are at object defaults (attrui present but no value stored).

**[13] p ringmod**: `abl.dsp.ringmod~` both channels; frequency from FSR (see 4.13), mix 0 at load (loadmess 0), toggled 0/1 by btn5. Ends at dac~ with no trim.

### 3.3 Feedback loops
- **Audio feedback loops in the Max graph: none** (no cycle among signal objects; verified with DFS per patcher). Feedback exists only inside externals: delay (attr 0.95 loadmess in absDelay), chorus feedback attr, flanger fback 0.9 (Csound), Supermassive Feedback 0.709, McCurdy grain, Freeze.
- Control loops (counters and index clamps) exist in `shiftr` (index number box with `+`/`*` feeding back, intentional accumulator) and in the looper (`counter` driven by `change`), all benign.

### 3.4 Dead / disconnected / unused (found by scripted audit)
- `top`: `s trEnc1Neg` (obj-59, dead sender; a real `r trEnc1Neg` exists), `r trPot2` (obj-25: **pot2 unmapped**), `p lil rando`.
- `p shiftr`: inlets idx5,6,7 (enc2 neg/pos/button) unconnected; message obj-129 dead. **Encoder 2 does nothing in v1.**
- `p clearer`: button obj-9, inlet obj-59 dead.
- `p freeze` L/R swap; `delayMIX` frywet/IN5 chain output unused.
- `p MOOD`: `abl.device.redux~` and its attrui, `*~ 0.6` x2, `scale` chain: no audio in, no audio out.
- `p autopan` entire block (orphan). Its `panr` dial still gets lidar (127 - lidar) and a loadmess 80.
- `looper`: `s LOOPdeleteLASTin` unconnected, 12 `data` objects are declarations (normal).
- `p butt tog` obj-172 (fed by `r trButt2`) has an output with no destination: **button 2 does nothing**. Unreferenced sends in the MIDI patch: `trMag1` (valves + breath), trButt1, trButt6, trButt15, trButt16, mics (CC48/49 not even mapped), gyro, accel Y/Z, arcade (CC40).
- Two attrui sets in `mcCurdyGrain` (e.g. `Pitch shift` obj-16) have no source: Spectral Pitch Shift keeps its saved +1200 cents.
- Startup: `loadbang` -> `delay 200` -> `zoomfactor 3` (UI zoom only); `loadbang` -> INIT line (0 -> 127 in 1000 ms, hold 127 for 800 ms, 127 -> 0 in 3000 ms) broadcast via `s INIT` into dials: drone, blur, clear toggle, supa (superMassive mix), grains, delau (delay mix), time. This is a 4.8 s start-up "sweep" of several effect amounts.

## 4. Effects inventory (chain order)

Conventions. "dial .1" = normalized 0..1 right outlet of a `live.dial` (range 0-127 default), so a CC value v gives v/127. "btnN" = `trButtN` = trellis pad index N-1 = CC 49+N, event 1 on press and 0 on release (two events per physical press). `p butt tog`/`p buttTOG` latches: a `counter 3` (0..3, int in left inlet = bang) counts **both** events, so output flips on every full press (verified against the counter refpage). Metro rates `16n/8n/4n` run from `transport @name internal` started at load (tempo not set in the patch: Max default 120 bpm assumed, so 16n=125 ms, 8n=250 ms, 4n=500 ms).

Third-party/proprietary summary (Bela cannot run any of these): 9 `vst~` instances = 4 Michael Norris "Spectral" AUs (manufacturer code `Mnor`, installed as `MNSpectral*.component`: DroneMaker, Blurring, Freezing, Pitch Shift), 3x ValhallaSupermassive, 2 Cabbage/Csound AUs that are Emory-built wrappers of Iain McCurdy CC BY-NC-SA csds (FLANGER_CSOUND, grainDelayMcCurdy; **source is available and portable**, see 4.6, 4.10); `supervp.trans~` (IRCAM SuperVP, in the SuperVPMaxMSP package); 5 Ableton `abl.*` objects (Max 9 "ableton-dsp" package, closed); `pfft~ gizmo_loadme` + `transratio` (Max-bundled examples, closed but trivial); `rnbo~ looper` (RNBO, exportable).

### 4.1 [1] p shiftr: dual-voice FFT harmonizer
- Algorithm: 2 `pfft~ gizmo_loadme 1024 4` voices per channel (Max `gizmo~` frequency-domain pitch shifter, FFT 1024, overlap 4), ratio `transratio` = 2^(st/12). Per-channel dry `*~` + voice A + voice B.
- State: single integer idx 0..13, init 5 (loadmess 5), `enc1 pos` = +1 (clamped at 13), `enc1 neg` = -1 (clamped at 0), `enc1 button` = reset to 5. enc2 not connected.
- idx table (A semitones, B semitones; "-" = voice off). Voice levels: single 0.6 (dry unchanged from previous), two-voice 0.5 each + dry 0.5; neutral dry 1, voices 0.

| idx | A | B | mode |
|---|---|---|---|
| 0 | -12 | - | single |
| 1 | -12 | -7 | two |
| 2 | -7 | - | single |
| 3 | -12 | -5 | two |
| 4 | -5 | - | single |
| 5 | bypass (dry 1) | | neutral/default |
| 6 | -12 | +5 | two (as read; possibly a copy-paste slip) |
| 7 | +5 | - | single |
| 8 | +12 | +5 | two |
| 9 | -12 | +7 | two |
| 10 | +7 | - | single |
| 11 | +12 | +7 | two |
| 12 | +12 | - | single |
| 13 | no handler: keeps idx 12 settings | | |
- Input: adc~ sum x -0.3.

### 4.2 [2] rnbo~ looper (12-layer looper)
- RNBO patch `p looper`: 12 stereo layers (`data Looper..Looper12 @channels 2 @sizems 80000` = 80 s each, ~368 MB at 48 kHz float), `record~`/`groove~` pairs with `@syncmode ms`, `@loop 1`. Layer counter (`counter` -> `<= n`/`> n` select recorded and playing layers; layers beyond the current count are stopped via `set stop`).
- Inlets: `in~ 1/2` audio, `in 3` (btn13), `in 4` (btn14), no MIDI used. Outlets `out~ 1/2`.
- Params (RNBO `param`, all with `@ctlin 68`, unused at the top level): `LOOPrec`, `LOOPplay`, `LOOPstop`, `LOOPdeleteLAST`, `LOOPdeleteALL` (0..1), `transp` (-12..12 st; rate = 2^(transp/12) via `expr pow(2,in1/12)`), `reverse` (0/1; `list.lookup 1 -1` direction), `volume` (-100..100 -> -18..+18 dB through `dbtoa`, `fade~ 0. 10`, default 1).
- Playback rate change goes through `fade~ 1. 50` (50 ms ramp). Comments in patch: Rec = fade out, stop playing, start recording; Play = stop recording, fade out, start playing, fade in; Stop.
- Control (inferred from netlist): **btn13 (in3) tap = rec/overdub trigger (`LOOPrecIN`), hold > 10 x 50 ms (>=0.5 s) = delete all (`LOOPdeleteALLin`)**; **btn14 (in4) tap = play (`LOOPplayIN`), hold >= 0.5 s = stop (`LOOPstopIN`)**. `metro 50` + counters implement the hold timing. Verify in Max before relying on this.
- Output: dry `in*(1 - layers/12)` + loop sum*volume (see 3.2).

### 4.3 [3] p freeze: Spectral Freezing (Norris AU)
- `vst~ "C74_AU:/Spectral Freezing"`, one attr exposed: `Freeze factor` (normalized, set by `p clearer`). Saved AU state (raw): [100.0, 0, -20.0, 0, 0, 100.0, 1, 2, 0] (9 params, names not exposed in the file).
- Wet = AU * 0.65 added to unity dry (additive). Mix dial `freezing`: default 127 (1.0); no controller drives it.
- `block` toggle (loadmess 1, **btn4 held = block off**, i.e. `r trButt4 -> == 0 -> toggle`): output of `1 - block` through `line~ 250` gates the AU input. So with btn4 up the AU input is closed and it holds its frozen spectrum; holding btn4 lets new audio in.
- `p clearer`: **btn3** (momentary `live.toggle[2]` `clear`, also hit by INIT): on turn-on, `Freeze factor` -> 0.01 over 2.3 s (`line`, 2300 ms), then 1.1 s later it heads back to `amount` (`amount` dial, loadmess 127 = 1.0) over 2.3 s. Behaves as a "release frozen sound" gesture, ~3.4 s recovery.
- Bug: L/R swapped (see 3.2).

### 4.4 [4] p drone: Spectral DroneMaker (Norris AU)
- No attrui exposed; saved AU state (18 params, raw): [2.06, 3.38, 0, -15.36, 11.68, 303.04, 7, 6, 99, 0, 2, 0.1, -60, 0, 100, 1, 1, 1.19]. Param names unknown from this file.
- Mix `drone` dial: **slider1 (CC20), linear, dial .1**; also INIT sweep. blurMIX linear crossfade.

### 4.5 [5] p blur: Spectral Blurring (Norris AU)
- Saved AU state (9 params raw): [44.9, 0, 0, 0, 0, 39.78, 1, 1, 0.0348]. No attrui.
- Mix `blur` dial: **btn7 latch**, `p butt tog` out 0 <-> 60 (dial .1 = 0.472); first press/release arms 0, second sets 60 (output changes on release). Also INIT sweep.

### 4.6 [6] p flanger: FLANGER_CSOUND (Iain McCurdy Flanger.csd, Cabbage AU, source recovered)
- Source: `/Library/Audio/Plug-Ins/Components/FLANGER_CSOUND.component/Contents/CabbagePluginEffect.csd` (CC BY-NC-SA 4.0, McCurdy 2012). Csound `flanger` opcode, per-channel UDO: `adlt = kdelay + LFO(kdepth)`; `flanger ain, adlt, kfback, 1.2` (max delay 1.2 s), `dcblock`, then `0.5*dry + 0.5*wet`; outer mix: ThruZero on: `ntrpol(-delayed dry(20 us), flanger out, kmix)`, else `ntrpol(dry, wet, kmix)`; output *level.
- Cabbage ranges: rate 0.001..40 Hz (skew 0.5, default 0.15), depth 0..0.01 s, offset(`delay`) 0.00002..0.1 s (skew 0.5), fback -1..1, mix 0..1, level 0..1, ThruZero checkbox, lfoshape combo 1..5 (parabola, sine, triangle, randomi, randomh).
- Max-side attrs (normalized 0..1): `fback` 0.95 (-> 0.9 actual), `depth` 0.78 (-> 0.0078 s), `rate` 0.08 (-> 0.257 Hz with skew 0.5), `mix` = IN2. Saved state: rate 0.257, depth 0.0078, delay 2e-5, fback 0.9, mix 0, level 1, ThruZero 1, lfoshape 2 (sine).
- Control: `flang` dial = **127 - lidar** (so near = 0, far = full), dial .1 -> mix. Thru-zero at mix 0 outputs the inverted dry (polarity flip; level ~unity) - audible only in combination with parallel dry (none here), so it is a polarity inversion of the chain at idle.

### 4.7 [7] p chorus: abl.dsp.chorus~ (Ableton Chorus-Ensemble DSP)
- attrs set at load: `gain` +3 dB (loadmess 3; range -70.6..6), `mod` 0.3 (0..1), `rate` 0.9 Hz (0.1..15), `mix` 0 (loadmess 0). Others at object defaults (width, feedback, invert unset).
- Control: **btn8 latch** (`p butt tog` 0/127 -> live.toggle -> chorus `mix` 0 or 1, i.e. 100% wet when on).

### 4.8 [8] p MOOD: supervp.trans~ pitch "mood" machine
- `supervp.trans~ 2 4000 0 4 @transients on @shapeinv on @envtrans on @maxfreq 200 @remix on @mintrans -1200` + `stereopres on`; transposition via message `transpose $1` in cents (= semitone ladder value * 100). IRCAM SuperVP phase vocoder with transient preservation and shape-invariant (formant-preserving) transposition.
- Ladder (semitones): [-24, -17, -12, -9, 0, +7, +12, +19, +24], index 0..8 = `scale 8 120 -> 0 8` of (127 - pitch dial) = joystick X raw: joyX 8 = idx 0, joyX 120+ = idx 8 (joyX high = stick up = pitch up).
- Wet/dry: `w` = 100 ms `line~` 0/1, set from a `metro 16n` random: every 16th, `random 0..1 > mood` -> w=0, else w=1. So **mood dial (joystick Y, 0..1) = probability the shifted voice is on, re-rolled each 16th note**. mood dial default 0 -> always dry unless the joystick is enabled. `auto` toggle (`live.toggle[4]`, UI only, set 1 then 0 at load) selects ladder index = `random 9` each 16th when its probability gate passes (comment "pedal vs rand") instead of joystick X.
- Controls: **joystick latch** (joy button) enables both axes; X -> `pitch` dial (obj-100, value = 127 - CC30), Y (CC31) -> `mood` dial (obj-68). First joystick press does not enable it (counter starts at 0); second press does.
- Dead: redux branch (see 3.2).

### 4.9 [9] p superMassive: 3x ValhallaSupermassive (VST3/AU, v4.0.0 state)
- Saved state (all 3): Mix 1.0 (100% wet internal), DelaySync 0.5, Delay_Ms 1.0 (normalized), Clear 1.0, Density 0, Width 1, LowCut 0, HighCut 1, ModRate 0.2738, ModDepth 0.5, `Feedback` 0.7087, `DelayWarp` 0.6457; `Mode` 0.8333 / 0.2083 / 0.0417; DelayNote 0.258 / 0.301 / 0.327.
- Max attrs (broadcast to all 3): `DelayWarp` <- `warp` dial (loadmess 90 -> 0.709), `Feedback` <- `feedback` dial (loadmess 90 -> 0.709).
- Controls: **slider3 (CC22) -> `supa` dial -> wet mix (verbMIX, linear dial .1)**; **accel X (CC41) -> `scale 0 127 -15 142` -> `warp` dial** (clipped by dial to 0..127; slope 1.236, so tilt extremes saturate); `feedback` has no live controller. INIT sweep also drives `supa`.

### 4.10 [10] p mcCurdyGrain: grainDelayMcCurdy (Iain McCurdy DelayGrain.csd, Cabbage AU, source recovered) + octave-up AU
- Source: `/Library/Audio/Plug-Ins/Components/grainDelayMcCurdy.component/Contents/CabbagePluginEffect.csd` (CC BY-NC-SA 4.0, McCurdy 2013). Csound: ring buffer 1048576 samples stereo (`giBufL/R`), grain scheduler `metro kDens`; per-grain random size (GSize1..GSize2), random delay (Dly1..Dly2, exp/lin/log distribution via `kDlyDst`), transposition random in [Trns1,Trns2] st, reverse probability, random pan (PanSpread), random amp (AmpSpread), random butlp cutoff (FiltSpread), amplitude decay vs delay (`ampdecay`), 5 grain windows (Hanning, Half sine, Decay 1, Decay 2, Gate), `mix` and `level`. Instr 1 outputs dry*(1-mix)*level at the same time.
- Max attrs (normalized -> actual): `GSize1` 0.07 -> 0.0148 s, `GSize2` 0.2 -> 0.0848 s, `Dens` 0.5 -> 500 grains/s (range 0.2..2000, skew .5), `Dly2` 0.45 -> 1.0125 s (Dly1 stays 0.0194 s), `PanSpread` 1, `AmpSpread` 0.8, `reverse` 0.7, `mix` 1; saved: FiltSpread 0.5, ampdecay 0.536, Trns1 = Trns2 = ~0 st, level 1, window 1 (Hanning), DlyDst 1 (exponential).
- Pre-stage: `Spectral Pitch Shift` AU (Norris; saved raw [1200, 0, 1, 1, 0, 0]; `Pitch shift` attr has no source so it stays +1200 cents = +1 octave) mixed in by `autooct`: every `metro 8n` (250 ms), `random 0..1`: with probability 0.06 (loadmess 0.06 -> `> 0.` right inlet) mix = 1 for the octave-up path, otherwise 0; 100 ms line~ crossfade. Stage-A output feeds the grain AU.
- Mix: **slider2 (CC21) -> `grains` dial -> stage-B pshiftMIX (dry input vs grain wet), linear dial .1**. INIT sweep also drives it.

### 4.11 [11] p absDelay: abl.device.delay~ (Ableton Delay)
- Delay `delay` attr is a 2-float (L,R seconds, 0.001..5), `feedback` 0..0.95 (loadmess 0.95), `mix`, `pingpong`, `smoothmode` (set to 1 after `delay 10020` ms; toggled 0/1 by btn10), pingpong not touched.
- Time (manual): `time` dial (value v) -> `1 - v/127` -> `scale 0..1 -> 0..60 exp 0.5` -> `/100` -> `pack f f` -> `delay` attr. With the FSR path (`127 - FSR` into `time`), **delay time = 0.6 * sqrt(FSR/127) s** (harder press = longer, 0..0.6 s; clamps to 1 ms).
- Stochastic mode: `chance` dial (default loadmess 127 = 1.0): every `metro 16n`, if `random 0..1 <= chance` -> internal delay `mix` attr set to 1 and a new random delay time (`random 100.` -> same 0..0.6 s curve, `/100.`, L=R) are set; else delay `mix` attr set to 0. Enabled only while toggle obj-75 is 1 (**btn9 held** = momentary; loadmess 1 at load so it starts enabled). Chance dial has no CC controller (only loadmess 127).
- Output: delay L/R -> `tanh~` -> `*~ 0.62` -> `delayMIX` wet, dry from input; mix `delau` dial through `line~ 250`. Delay's `smoothmode` set to 1 (or 0 via btn10 toggle).
- Controls: **btn11 latch**: `delau` ramps 0 <-> 127 (700 ms `line`) and `fb` set to 100; **btn12 latch**: `delau` ramps 0 <-> 40 (0.31), `fb` set to 30 (0.236), `time` set to 67 (0.528) after 10 ms; **FSR** drives `time` only while btn12's output is 0 (gate opens when value hits 0). **btn10 latch**: `smoothmode`. Several drivers (INIT, btn11, btn12, loadmess 100 -> `p buttTOG` obj-198 which ramps `fb` to 127 at load over 700 ms, `r INIT`) all write the same `fb` and `delau` dials with no arbitration.

### 4.12 [12] p roar: abl.device.roar~ (Ableton Roar saturator)
- attrs exposed: `input_gain` (-24..24 dB), `routing`, `tone_amount` (-1..1), `tone_frequency` (80..2000 Hz), `shaper_amount_1` (0..1), `shaper_type_1`, `shaper_bias_1` (0..1), `filter_frequency_1` (20..20000), `filter_type_1`, `filter_resonance_1` (0..1), `blend` (0..1), `mix`. **Only `shaper_amount_1` is driven** (by IN2); every other attr is object default (no values stored in the patch).
- Control: **pot1 (CC28) -> `roar` dial .1 -> `shaper_amount_1` (0..1) and output gain 0.3 + 0.7*(1-a)^2** (`scale~ 0 1 0.3 1 2` on `1 - a`).

### 4.13 [13] p ringmod: abl.dsp.ringmod~
- attrs: `frequency` (1..10000 Hz), `mix`, `drive` (0..24 dB) and `enable_drive` are exposed as attrui but not driven (defaults).
- Frequency: **FSR (CC24) -> `amount` dial (initial 127) -> `scale 0 127 0 800` -> `line` 200 ms glide -> `frequency`**: FSR 0..127 gives 0..800 Hz (clamped to 1 Hz minimum). Mix: **btn5 held (momentary)** -> `live.toggle[6]` -> `mix` 0/1 (loadmess 0).
- Because the FSR firmware range is 2..50 raw, a light touch saturates 800 Hz.

### 4.14 Orphans
`p autopan` (random auto-pan, `metro 500`, scale 200..600 ms, never audio-connected), `p lil rando` (metro 4n random 0..127), `abl.device.redux~` in MOOD.

## 5. Modes, states, presets

- **No presets, no preset storage, no pattr/preset/coll/dict** in either patch (confirmed by object census: no `preset`, `pattr`, `coll`, `dict`). Only the Max snapshots under `~/Documents/Max 9/Snapshots` for the AU plugins (dependency cache; each holds that plugin's state).
- Global modes: shifter index (14 positions), joystick enable latch, MOOD auto toggle (UI-only), looper layer state (rec/play/stop, up to 12 layers), freeze block/clear, chorus on/off, blur on/off (0/60), delay modes (btn9 stochastic enable, btn10 smoothmode, btn11/12 delay presets), ringmod mix momentary.
- Startup: 4.8 s INIT sweep; `delau`/`fb`/`chance`/`warp`/`feedback`/`freezing` defaults via loadmess (see 4.x); window set to `size 152 95 891 894`, zoom 3.

## 6. Interaction model as it stands

Contact: trumpet mic into adc~ (single feed, both channels summed). Teensy controller on the horn:
- **Valves/Hall sensors, breath (BME680), mics, IMU Y/Z/gyro, arcade button, pot2, enc2: transmitted, but unused by the patch** (the receiving objects do not exist or the senders are unmapped).
- Right-hand surface (assumed; physical layout not in the sources): 3 sliders = wet levels of **drone / grain delay / big reverb**; pot1 = **roar distortion**; FSR = **ringmod frequency and delay time**; joystick = **pitch ladder + mood probability** after pressing the stick button; accel X (tilt) = **reverb delay-warp**; lidar (hand distance) = **flanger depth/mix inverted** (far = more); enc1 = **harmonizer interval**, enc1 click = back to unison.
- Trellis 4x4 (pads 3..14 used): pad3 clear freeze, pad4 hold-to-let-in (freeze release), pad5 hold ringmod, pad7 blur toggle, pad8 chorus toggle, pad9 hold stochastic delay, pad10 delay smoothing mode, pad11 delay on (full, fb 0.79), pad12 delay on (low, fb 0.24, time fixed), pad13 looper rec/overdub (hold = clear), pad14 looper play (hold = stop). Pads 1, 2, 6, 15, 16 do nothing.
- It is a "set-and-sculpt" rig: no per-note or per-valve triggering; all effects process the live mic continuously.

## 7. Bela port notes

Constraints: Bela Gem Multi on PocketBeagle 2 (per project docs) is a small ARM A53 board with limited RAM; Max chain below is far above the budget if ported 1:1. Likely route: C++ `render.cpp` core (fixed chain, fixed-size buffers, no allocation in render) with Csound or Faust only where the code already exists. Controls: keep the Teensy over USB MIDI (CC map in section 1 unchanged, the receiver needs only ch3 CC), or read pots/sliders/FSR/joystick on Bela analog inputs and I2C sensors on Bela I2C.

| Block | v1 implementation | Bela route | CPU/porting notes |
|---|---|---|---|
| Input sum x -0.3 | adc~ | C++ | trivial; drop the polarity flip unless intended |
| [1] shifter | 4x pfft~ gizmo (FFT 1024/4) | C++ TD-PSOLA/SOLAD (reuse hyperGuitar pitch engine, monophonic trumpet suits it) or Faust `ef.transpose` (cheap, artifacts) | pfft~ has no Pd/libpd equivalent; a 2-voice phase vocoder at 1024/4 is 4 FFT chains of ~4x per channel: heavy. Dual mono -> process mono once |
| [2] looper | RNBO, 12x80 s stereo | C++ (record/play ring buffers) or RNBO C++ export | 368 MB of buffers will not fit; budget e.g. 4 layers x 30 s mono; preserve tap/hold gestures |
| [3] freeze | Spectral Freezing AU | C++ STFT magnitude hold (random phase) | closed AU: reimplement. FFT 2048/4 mono |
| [4] drone | Spectral DroneMaker AU | unknown algorithm (params list unnamed); approximate with filtered spectral hold + sustained partials | behavior must be re-derived by ear |
| [5] blur | Spectral Blurring AU | C++ STFT magnitude smoothing across frames | same STFT can be shared with [3],[4] and the shifter |
| [6] flanger | Csound `flanger` opcode (McCurdy) | **Csound on Bela** (csd reusable, `chnget` -> Bela channels) or C++ (delay line + LFO + feedback, DC block, thru-zero) | light; port 1:1 is easy |
| [7] chorus | abl.dsp.chorus~ | C++ / Faust `chorus` (2-4 modulated delay lines) | light; Ableton sound not reproducible exactly |
| [8] MOOD | supervp.trans~ | C++ PSOLA at ladder ratios + 100 ms crossfade, or phase vocoder with transient handling | SuperVP is IRCAM licensed; transient/formant preservation will not match |
| [9] reverb | 3x ValhallaSupermassive | Faust `re.zita_rev1`/`re.jpverb` or a C++ FDN; one stereo instance + modulated delay-warp parameter | 3 instances at 100% wet is too heavy; Supermassive algorithm is proprietary; warp parameter must be re-implemented as delay-line scale |
| [10] grain delay | McCurdy DelayGrain.csd + Norris AU octave | **Csound on Bela** (csd given), or C++ granulator with up to ~40 voices (dens 500/s x ~0.085 s) | densest CPU hotspot after the vocoder; each grain has a butlp; reduce density or voices. octave-up pre-stage: simple pitch-shift or skip |
| [11] delay | abl.device.delay~ + tanh~ + dry/wet 250 ms | C++ delay line (stereo, ping-pong, feedback <= 0.95, time smoothing/repitch) | light |
| [12] roar | abl.device.roar~ | C++ waveshaper (tanh/fold) + SVF + tilt EQ | approximate only; only `shaper_amount_1` is exposed |
| [13] ringmod | abl.dsp.ringmod~ | C++ sine x input, mix | trivial |
| control | Max patches + live.* | C++ MIDI CC handler on Bela (USB MIDI) | latch/hold logic (counters, 50 ms holds, ramps) must be recreated |

Will not port directly: all `vst~` AUs (Norris Spectral x4, Valhalla x3), `supervp.trans~`, `pfft~ gizmo_loadme`, the `abl.*` objects, `rnbo~` as built (memory), `transport`/`metro Nn` (replace with a tempo-less ms timer; 120 bpm assumed).
Latency/cpu concerns on Bela: sample-rate 44.1 kHz default, blocksize 16-64; FFT objects need larger hops so allow +10..20 ms; share one STFT between freeze/blur/drone; collapse dual-mono to mono internally.

## 8. Unknowns

- Max audio settings (sample rate, vector size, adc channel assignment) are not in the files; assumed 44.1/48 kHz stereo, single mic summed.
- Physical sensor-to-control mounting (which hand has which sensor) is not in code; section 6 assumes the obvious roles.
- `transport` tempo not set in the patch: 120 bpm assumed.
- Looper semantic mapping (tap/hold for btn13/btn14) is inferred from netlist labels, not run.
- Norris Spectral AU parameter names and DroneMaker algorithm: only saved raw state is available; attrs other than `Freeze factor` and `Pitch shift` are not exposed; value ranges of those two attrs (normalized assumed 0..1) are not confirmed.
- Valhalla Supermassive mode names for Mode 0.0417/0.2083/0.8333 (index = value*12 - 0.5 approx).
- `abl.*` objects other than attrs shown have no stored values; defaults come from the package (Max 9.0.9 ableton-dsp) and may vary by version.
- Whether joystick X/Y mapping (127 flipped) in MIDI patch vs the Max 127-x conversions is as intended: net effect is joystick up = pitch up, right = more mood.
- v1 anomalies that may be unintended: valves/breath all share `trMag1`; freeze L/R swap; shifter single-voice dry level state dependence; idx 6 interval pair; joystick first-press no-op; redux dead branch; autopan orphan.
- The sources do not contain the Spectral AU algorithms or a README for the instrument concept.
