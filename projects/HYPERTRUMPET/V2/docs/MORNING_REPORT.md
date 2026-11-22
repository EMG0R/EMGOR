# Morning report, 2026-11-17

## What is flashed
**bringup-1.12** (`TEST=10_delay`, BOOT_SRAM). Known-good backup: `bringup/fw/_old/known_good_1.12_10_delay.qspi.bin` (also `bringup/fw/dist/`). Source of the build: `bringup/fw/` (`make TEST=10_delay APP_TYPE=BOOT_SRAM BUILD_DIR=build_soak`). The 08_mic known-good bin is unchanged and still byte-identical to a fresh 08 build.

## The final physical map (details: `docs/TRELLIS_MAP.md`)
Pad 1 top-left.
```
FX page                                   HARMONY page (either encoder button toggles; effects keep running)
 1 FLANGER   2 ROAR     3 FREEZE CLEAR  4 FREEZE LET-IN        +2   +3   +4   +5
 5 RINGMOD   6 MOOD     7 BLUR          8 CHORUS               +6   +7   +8   +9
 9 DELAY ON 10 SMOOTH  11 PRESET A     12 PRESET B             +10  +11  +13  +14
13 LOOP REC 14 LOOP PLAY 15 AUTOPAN    16 WAH                  +17  +18  +20  +21
```
- **Latch pads** toggle on release; a 600 ms press focuses that effect's panel in the web UI. Pad 4 acts while held, pad 3 runs the 2.3 s freeze-factor ramp, looper pads: tap / hold 500 ms.
- **Harmony page:** latching interval pads, at most 3 voices (the 4th press steals the oldest), 20 ms fades, encoder 1 = harmony mix. Separate LED palette (amber, coral, magenta, cyan rows).
- **Faders:** 1 reverb, 2 drone, 3 grain mix, **exclusive** (last fader moved wins; the displaced effect fades out and stays blocked until its fader has been at 0 once).
- **Sensors drive params only, never switch an effect on:** accel Y -> reverb warp, pot 1 -> roar drive, FSR -> delay time ALWAYS (600 ms x sqrt(FSR), repitch glide), lidar (inverted) -> flanger mix and autopan depth/period, joystick -> MOOD pitch (X) and probability (Y).
- **Encoders:** either button = harmony page on/off; encoder 1 rotation = harmony mix on that page, nothing on the FX page.
- **Chain order (v1):** harmonizer, harmony, looper, freeze, drone, blur, flanger, autopan, chorus, MOOD, reverb, grain, delay, roar, ringmod, wah.

## CPU on the device (percent of one 48-sample callback)
| effect | avg | peak | | effect | avg | peak |
|---|---|---|---|---|---|---|
| reverb (4 lines) | 11.2 | 12.1 | | chorus | 3.8 | 4.3 |
| drone | 7.5 | 8.6 | | mood | 4.6 | 5.3 |
| freeze | 5.2 | 10.0 | | grain | 8.7 | 10.7 |
| blur | 4.9 | 6.3 (re-measured on 1.11 after the 24 kHz move: soak_20261117_0628xx fx2; admission table 0.063) | | delay | 6.1 | 6.5 |
| harmonizer (v1) | 2.3 | 2.4 | | roar (4x stage above DRIVE 0.6: 18-25 % at high fold) | 3.4 | 3.5 |
| looper | 2.4 | 2.5 | | ringmod | 1.9 | 1.9 |
| flanger | 3.0 | 3.2 | | wah / autopan | 1.0 / 1.1 | 1.1 / 1.1 |
**Combos:** harmony (3 voices, +4 +7 +11) 14.3 avg / 15.1 peak; harmony + chorus + reverb 31.1 avg / 33.4 to 34.3 peak (target < 60 %). Idle (nothing on) 1.8 %. Full tables and the host-to-device factors: `docs/FX_BUDGET.md`.

## Stability (all on the device, logs in `bringup/logs/`)
- **Map phase 51/51** (every row of `V1_MAP_AUDIT.md` plus the later decisions, through `sim`); **fx1, fx2, fx3 15/15 each, fx4 6/6**.
- **Stress: 9 runs of 4 minutes, all 15/15, 0 xrun and 0 gap in every run**, 2 sheds per run (the runtime guard shed blur each time with the seeded stress). Logs `soak_20261117_065900` to `soak_20261117_074700` (+ `_summary.md`); the earlier failing run `soak_20261117_065157` was the 709 vs 700 slow-peak case that led to the runtime guard.
- **Guards:** xrun / gap counters, per-effect NaN check with automatic reset, admission control (65 % projected peak, device-measured costs), runtime shed (8 of 32 blocks over 75 % or one over 90 %).
- Host checks (`bringup/fw/tests/`): fader dead zones, Trellis hot-plug timing, looper behaviour, param record, effect quality (clicks, DC, NaN, pitch, aliasing, RT60).

## What changed tonight
Fader pipeline with per-fader endpoints and exact 0 / 1; every HYPER_CORE effect on one generic glue template with per-effect gate (zero CPU when off); v1 pad map and control map from the audited patch; CPU budget work (reverb 29 % -> 11 %, grain, roar, chorus, delay, freeze, blur); looper with trailing-silence trim and pass cancel; autopan, MOOD BITS, grain octave-up gate from the v1 patch; harmony page; exclusivity; Trellis retry and hot-plug; strip send moved to a register-level routine; soak/sim/state/mute tooling and the 11_bench firmware; runtime guard.

## Still open, needs you
1. **Listening tests:** the 4-line reverb vs Supermassive (denser in v1: three parallel instances, level not matched), the harmony voices (time-domain shifter: sidebands about 27 Hz either side, fine for a trumpet but audible on a held note), roar at high fold, autopan, blur.
2. **Trellis LEDs and hot-plug on real power** (no power tonight): re-init and full redraw are only host-tested.
3. **Strip** (physically damaged): the blocking register-level send shows FIFO-dry gaps up to 87 us whenever an audio IRQ exceeds the 41 us the WS2812 tolerates; DMA vs blocking needs retesting on a new strip (`strip.sg`, `sgmax`, `n` in telemetry).
4. **Decisions:** latch pads (7, 8, ...) toggle on release rather than press; the harmony grid reads "#3 = +4" (major third) and "#5 = +8" (the pad labelled #5 as the sharp fifth); MOOD bitdepth is deferred (the v1 redux outputs are unconnected, so BITS defaults to off); `lil rando` has no target in v1 (no inlets, outlets or cords), so nothing reads it.
5. Blur: re-measured 4.9 % avg / 6.3 % peak on the device after the 24 kHz move; kAdmitPeak is 0.063. Closed.
