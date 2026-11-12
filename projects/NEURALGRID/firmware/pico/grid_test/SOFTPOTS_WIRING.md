# 4x SoftPot ribbons → 74HC4067 mux → Pico ADC

Covers wiring the north/south/east/west SoftPot ribbons (one per slanted side)
through a 74HC4067 16-channel analog mux into a single Pico ADC pin. Code side
is in `grid_test.ino` (`muxSelect`/`muxRead`/`scanSoftpots`).

## Pins used

| Signal | Pico pin | Notes |
|---|---|---|
| Mux S0 | GP2 | address bit 0 |
| Mux S1 | GP3 | address bit 1 |
| Mux S2 | GP6 | address bit 2 |
| Mux S3 | GP7 | address bit 3 |
| Mux SIG | GP26 (ADC0) | shared analog input, reads whichever channel is selected |
| Mux EN | GND | tie permanently low — mux is always enabled, no GPIO needed |
| Mux VCC | 3.3V | **must** be 3.3V, not 5V — Pico ADC input is not 5V-tolerant |
| Mux GND | GND | common with Pico GND |

Softpots land on mux channels **Y0, Y1, Y2, Y3** (of 16). That leaves Y4–Y15
open for the FSR pads and piezo later — same mux, same ADC pin, no new wiring
once you get there, just extend `softpotChannel[]`.

## Walkthrough: wiring the first SoftPot (do this one first, then repeat ×3)

**1. Identify the 3 pins with a multimeter (resistance mode).**
A linear SoftPot strip has 3 leads/pads at one end. Manufacturers don't always
label them consistently, so verify by measurement rather than trusting silkscreen:

- Measure resistance between every pair of the 3 pins.
- **Two pins will read a fixed, full-scale resistance no matter where you
  press the strip** (typically ~10kΩ for common 4.5"–10" strips) — those two
  are the **ends** (call them End A and End B).
- **The third pin's resistance to either end changes as you slide your finger
  along the strip** — that one is the **wiper**.

**2. Decide physical orientation before you solder anything.**
You said: top of the panel has an arrow printed on the strip, and the strip
runs down to the far side at the bottom. Lay the strip in place on the panel
first and note which physical end (top-near-arrow vs bottom-far-side) is End A
vs End B from step 1. Whichever way you pick, **use the same orientation
convention on all 4 sides** so "press near top" always moves the ADC reading
the same direction on every softpot — that consistency matters more than which
literal end you chose.

**3. Wire it as a voltage divider:**

```
3.3V ──── End A (top, arrow end)
GND  ──── End B (bottom, far side)
Wiper ─── Mux channel Y0 (the mux's "Y0" pin, NOT SIG directly)
```

Pressing near the top reads closer to 3.3V (higher `analogRead()` value, up
near 1023); pressing near the bottom reads closer to 0V (near 0). If your
first unit reads backwards, don't rewire — just flip the interpretation in
software (`1023 - value`) once all 4 are in and you can see if any strip
was physically mounted flipped relative to the others.

**4. Add a small filter cap (recommended, not strictly required).**
Solder a 0.1µF ceramic capacitor from the wiper line to GND, as close to the
softpot's own leads as you can. SoftPots are resistive membranes and can be
noisy, especially over longer wire runs to the mux — this cap smooths that out
before it ever reaches the ADC. Skip it for the first bring-up test if you
just want to see numbers move; add it once you're wiring for real.

**5. Wire the mux's Y0 pin to the wiper**, and confirm mux VCC is 3.3V (from
the Pico's `3V3` pin or the same 3.3V rail powering logic — do NOT feed it 5V,
since SIG passes whatever voltage is on the selected channel straight to the
Pico's ADC pin).

**6. Test before moving to the other 3.**
Flash `grid_test.ino` (already updated with the mux/softpot code) and open the
Serial Monitor at 115200 baud. You'll see a line like:

```
softpot: 512,0,0,0
```

at ~50Hz. Only the first number should move as you
slide your finger on this first strip; channels 1–3 will read noise/floating
values near 0 until their softpots are wired in (that's expected — an
unconnected mux channel floats).

## Repeat for the other 3

Same steps, same End-A/End-B convention, landing on mux channels **Y1, Y2,
Y3** for south/east/west respectively (matching `softpotChannel[] = {0,1,2,3}`
in the firmware — reorder that array if you wire them to different channels).
All 4 share the same 3.3V/GND rails and the same 4 select lines; only the
wiper goes to a distinct mux channel pin.

## Sanity checks if a channel doesn't move

- **Reads near 0 or 1023 and never changes** → wiper not connected to that
  mux channel, or the mux channel pin itself is floating/unrouted.
- **Reads jump wildly with no finger on it** → missing the 0.1µF filter cap,
  or a loose/long unshielded wiper wire picking up noise.
- **All 4 channels show the same value** → mux select lines (S0–S3) aren't
  wired/driven — check continuity from GP2/GP3/GP6/GP7 to the mux's S0–S3 pins.
