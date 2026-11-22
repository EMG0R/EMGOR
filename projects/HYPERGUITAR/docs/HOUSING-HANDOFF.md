# hyperGuitar — enclosure handoff

**For:** whoever designs and builds the housing.
**Status:** temporary/interim enclosure. The luthier build is stalled, so this
box needs to be *playable and serviceable*, not beautiful or permanent.
**Date:** 2026-09-21

---

## 1. What this thing is

hyperGuitar is a real-time guitar processor built on a **Bela** board. A
**hexaphonic (6-channel) pickup** feeds it — one audio channel per string —
and it runs per-string pitch shifting, formant shifting, synthesis and effects.

It is played two ways at once:

- **On the instrument**: a NeoTrellis 4×4 pad grid selects effects, two rotary
  encoders adjust them, a joystick acts as a whammy bar, and a TRS jack takes an
  expression pedal.
- **From NEPTR**: a separate Raspberry Pi rig connected over USB. It mirrors the
  guitar's state on its touchscreen and can drive it back. This is why the box
  does **not** need its own display.

**Audio out is stereo and leaves over USB** to the Pi (`audioWrite` channels
0 and 1 only). There is no analog output jack in the current design — confirm
with the owner whether one is wanted.

---

## 2. ⚠️ Things that are NOT yet measured

I am a software agent. I have **not** physically measured anything. Everything
below marked ⚠️ must be verified against the actual parts before cutting:

- ⚠️ Bela board model and exact dimensions / mounting-hole pattern
- ⚠️ Hex pickup connector type (13-pin DIN? breakout? flying leads?)
- ⚠️ NeoTrellis dimensions (Adafruit's 4×4 is nominally ~60 × 60 mm, but the
  **elastomer pad sits proud of the PCB** and needs a bezel cutout, not a flush
  hole)
- ⚠️ Joystick module footprint and shaft height above panel
- ⚠️ Whether the Bela is powered from USB or needs its own supply

Treat every dimension here as "ask the owner / measure it", not as a spec.

---

## 3. Panel controls

| # | Control | Notes for the panel |
|---|---|---|
| 1 | **NeoTrellis 4×4** | The main interface. Needs a bezel cutout for the silicone pad, which sits above the PCB. Must be the visual centre — the player reads state from its LEDs. Being replaced right now, so mount it **removably**. |
| 2 | **Encoder 1 (MAIN)** | Rotary with push-switch, detented (KY-040 style, `ENC_DETENT 2`). |
| 3 | **Encoder 2 (AUX)** | Same. **Both are required** — see §6. |
| 4 | **Joystick** | 2-axis analog + push-button. Played as a **whammy bar**, so mount it where a picking hand can reach it *while playing*, not where a menu knob would go. Needs clearance for full throw. |

**Do not substitute a potentiometer for either encoder.** Encoders are relative;
pots are absolute. With presets recalling stored values, a pot's physical
position disagrees with the loaded value and the sound jumps the first time it
is touched. This exact failure was fought and fixed on the expression pedal —
don't reintroduce it mechanically.

---

## 4. Jacks

| Jack | Type | Goes to | Notes |
|---|---|---|---|
| **Guitar in** | **TRS** | audio in **0** (tip), **1** (ring) | TRS even though the guitar is mono today. A TS plug grounds the ring, so ch1 reads silence and is quiet — no penalty now, stereo later with no rework. |
| **Contact mic 1** | TS | audio in **2** | ⚠️ **This jack is physically broken and must be replaced.** |
| **Contact mic 2** | TS | audio in **3** | ⚠️ Same broken jack assembly. |
| **Expression pedal** | **TRS** | analog in **0** (tip = wiper) | Standard expression-pedal wiring: tip = wiper, ring = +3.3V, sleeve = GND. ⚠️ Verify polarity against the actual pedal — some are reversed. |
| **Hex pickup** | ⚠️ TBD | audio in **4–9** | Six channels. Connector type unknown — see §2. |
| **USB to Pi** | USB | Bela USB | Carries audio, MIDI and the control link. **Panel-mounted or strain-relieved** — see §7. |

That is **10 of 10 audio inputs used**: 1 guitar (×2 for future stereo), 2
contact mics, 6 hex pickup, with the remaining 1 free only if the guitar stays
mono.

---

## 5. Contact mics need a buffer circuit ⚠️

Piezo contact mics are **very high impedance** (megohms). Bela's audio inputs
are far lower, which loads the element down: thin, bass-less, hum-prone. This is
almost certainly why they "sound all noisy."

**The fix is a unity-gain buffer per mic**, in the box, close to the jacks:

- A dual op-amp (TL072, OPA2134, or similar) — one half per mic
- Non-inverting, unity gain
- **~4.7 MΩ** input resistor to a mid-supply bias point (this is the part that
  matters — it's what stops the bass roll-off)
- AC-couple in and out (~100 nF input, ~10 µF output)
- Single supply from Bela's 5 V or 3.3 V rail, with a 2× 100 kΩ divider +
  decoupling cap for the mid-supply reference

Keep this board **physically close to the contact-mic jacks** and keep the
unbuffered runs short — that wiring is the antenna. ⚠️ An electronics-competent
person should review the exact values against the chosen op-amp and rail.

---

## 6. Authoritative pin map

This is read directly from the firmware and is the contract. Wire to this.

### Digital (Bela digital 0–15)
```
0   ENC1 CLK          3   ENC2 CLK
1   ENC1 DT           4   ENC2 DT
2   ENC1 SW           5   ENC2 SW
6   JOY SW  (joystick push-button)
```

### Analog in (0–3.3 V)
```
0   EXPRESSION PEDAL  (TRS tip / wiper)
1   JOYSTICK X        (left-right)
2   JOYSTICK Y        (up-down)
3-7 free
```

### Audio in (10 channels)
```
0   guitar TRS tip    (mono now, L later)
1   guitar TRS ring   (silent with a TS plug, R later)
2   contact mic 1     <-- broken jack, replace
3   contact mic 2     <-- broken jack, replace
4-9 hex pickup        STRING_CHANNEL = { 9, 7, 6, 8, 4, 5 }
                      index order = [lowE, A, D, G, B, e]
```

**Do not "tidy" the hex pickup channel order.** It is deliberately scrambled —
it was remapped on 2026-07-28 after the original 2/3 jack broke, and each string
keeps its own tuning/pan/effect identity by index.

### Audio out
```
0, 1  stereo, over USB to the Pi
```

---

## 7. Build constraints that came from real failures

These are not preferences. Each one cost a debugging session.

1. **Strain-relieve the control harness, hard.** The encoders/buttons went
   completely dead once: 1036 raw frames with every control reading zero while
   every sensor on a *different* harness streamed perfectly. The control wiring
   is the weak link — over-engineer its connector and its anchoring.

2. **The USB cable must not be able to wiggle.** Both the Bela and a Teensy on
   the same rig failed to enumerate (`can't set config #1, error -71 / -110`)
   and needed physical replugs. Power was ruled out with evidence (no
   undervoltage ever latched, zero over-current events), so connection integrity
   is the suspect. Panel-mount the USB or anchor it with a proper clamp.

3. **NeoTrellis must be removable without a full teardown.** It has already been
   swapped once (it went into another project) and is being replaced again now.

4. **Leave service access to the Bela's SD card and USB.** Firmware is flashed
   over the network but recovery needs physical access.

5. **Joystick placement is a playing decision, not a layout one.** It is a
   whammy bar. Reachable mid-phrase by the picking hand.

---

## 8. Parts manifest

| Qty | Part | Notes |
|---|---|---|
| 1 | Bela board (⚠️ confirm model) | Existing |
| 1 | NeoTrellis 4×4 + elastomer pad | Being replaced — new unit |
| 2 | Rotary encoder, detented, with push-switch | KY-040 style |
| 1 | 2-axis analog joystick + button | Whammy bar |
| 1 | TRS jack — guitar in | |
| 1 | TRS jack — expression pedal | |
| 2 | TS jack — contact mics | **Replaces the broken assembly** |
| 1 | Hex pickup connector | ⚠️ type TBD |
| 1 | Dual op-amp (TL072/OPA2134) + passives | Contact-mic buffer, §5 |
| 1 | Small perfboard/PCB | For the buffer |
| — | USB panel-mount or clamp | §7.2 |
| — | Standoffs, harness connectors, strain relief | §7.1 |

---

## 9. Open questions for the owner

1. Is an **analog audio output** wanted, or is USB-to-Pi the only path? Current
   firmware writes stereo out but it leaves over USB.
2. How does the **hex pickup** physically connect today?
3. Is the Bela **bus-powered from the Pi**, or separately powered? Given the
   enumeration failures, a separate supply is worth considering.
4. Any constraint on box size / mounting — is it worn, on a stand, on the floor?
