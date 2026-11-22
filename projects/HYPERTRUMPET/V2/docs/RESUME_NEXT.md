# Resume next session (2026-11-12)

The pinout and implementation-plan agent was stopped at Emory's request, before it finished. No code has been generated yet, by request.

## When resuming
1. Generate the **pinouts from the official Electrosmith Seed 3 pinout and datasheet** (daisy.audio / the electro-smith GitHub), cross-checked against the PMOR V3 + libDaisy Seed 3 defs. Read those PMOR/libDaisy files only; never edit them.
   - Write `HYPERTRUMPET_V2/docs/PINOUT_TRUMPET.md` and `VOCAL_HAT/docs/PINOUT_HAT.md`, then update `PINMAP_SEED3.md` in place.
   - Cover the WS2812 method and pin (SPI+DMA or TIM+DMA), with no clash with the codec SAI, I2C or USB.
   - Check whether the TAC5242 has an input PGA, and the line-in full scale.
   - Fill the cell placeholder: "21700 5000 mAh flat-top (IMR Batteries), 2 per instrument plus spares".
2. Write `HYPERINSTRUMENTS_IMPLEMENTATION_PLAN.md` using the bring-up order below. Plan only, no code.

## Bring-up order: one thing at a time, with a noise check after each
Each step adds one item. After each one, re-check that everything earlier still reads clean, and re-measure the audio noise floor (live input meter over CDC) from step 3 on.
1. **Power only, no Seed:** cells, BMS, toggle, TPS61088. Meter 5.0–5.1 V. Wire the star ground.
2. **Seed alone:** USB-C, DFU flash, blink, CDC probe.
3. **Audio baseline, nothing else connected:** line in with the input shorted, then the mic. Record the noise floor. This number is the reference for every later step.
4. **Quiet analog sensors, one at a time:** faders, then pots, then FSR, then joystick (with deadzone).
5. **Halls**, one valve at a time: calibrate each, then test crosstalk with all 3 pressed.
6. **Digital inputs:** encoders, then joystick button (debounce).
7. **I2C sensors, one at a time:** MPU6050, then VL53L0X.
8. **NeoTrellis** keys first, LEDs off. Then LEDs on, and re-measure the noise floor.
9. **FCOB strip:** low brightness first, then up to the cap. Re-measure the noise floor. This and step 10 are the most likely noise sources.
10. **Xvive TX**, fed from the BMS output. Re-measure the noise floor and check that the wired out is still clean.
11. **Hat only:** MAX4466 preamp in place of the direct mic path. Set the gain trimmer to about −6 dBFS peak.

The rule: if the noise floor rises at step N, fix N before adding N+1. The noisy parts (LEDs and TX) come last, so each one is isolated when it's added.
