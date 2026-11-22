# HYPERTRUMPET V2 — grounding plan (Bela Gem Stereo)

Status: plan, 2026-11-11. Nothing built or measured. Every hardware claim carries a tag:
**[V]** verified against a primary source (cited), **[I]** inferred from a primary source, **[U]** unverified, check on the bench.
Companion: `PINMAP_V2.md` (exact header pins).

## 0. Sources

| Tag | Source |
|---|---|
| DS | Bela Gem Stereo/Multi datasheet v1.0, 2026-08-04: https://bela.io/datasheets/bela_gem_datasheet.pdf (Fig. 1 pinout, Tables 1, 3, 4, sec. 2.4) |
| SCH | Bela hardware repo, Gem Stereo rev A4 KiCad source (schematic + PCB netlist, read directly): https://github.com/BelaPlatform/bela-hardware/tree/master/GemStereo/GemStereo_rev_A4 and Gem Multi rev A4: https://github.com/BelaPlatform/bela-hardware/tree/master/GemMulti/GemMulti_rev_A4 . The datasheet says "Revision A"; A4 is the repo copy. Rev differences are not checked [U]. |
| HW | Bela hardware page (older Bela/Bela Mini, pre-Gem): https://learn.bela.io/using-bela/about-bela/bela-hardware/ |
| MIG | Migrating to Bela Gem: https://learn.bela.io/get-started-guide/migrating-to-bela-gem |
| ADC | TI ADS8166/7/8 datasheet SBAS817D: https://www.ti.com/document-viewer/ADS8168/datasheet/GUID-3893B402-8A32-46EA-9AFE-E923A462AF4C |
| PB2 | PocketBeagle 2 design doc: https://docs.beagleboard.io/boards/pocketbeagle-2/03-design-and-specifications.html (no rail current limits given) |
| HG | hyperGuitar `docs/NOISE-HANDOFF.md` (2026-09-23), `memory/projects/HYPERGUITAR/` |

## 1. What happened on hyperGuitar (root-cause history)

Record: `hyperGuitar/docs/NOISE-HANDOFF.md` section 0, status RESOLVED 2026-09-23 [HG].

Symptoms (Gem Multi, rig moved into a new housing, contact mics added behind BC517 Darlington buffers):
- NeoTrellis noise was tonal, its pitch followed LED **colour**; two lit pads sounded like phased analog oscillators; brightness decay was audible when switching menus; every pad press made a "note".
- It scaled with LED current, not I2C traffic. LEDs at brightness 0 with identical I2C traffic gave zero noise. Hot-unplugging the board stopped it. Moving the board 3.3 V -> 5 V made it much worse.
- Lidar (VL53L1X) gave ticking at the poll rate; silent when LEDs off and piezo muted.
- Things that did nothing: 1 uF caps at the sensors, 100 ohm series resistors in SDA/SCL, slowing I2C. A 1000 uF cap across NeoTrellis power made the rig self-oscillate at power-up (brownout loop, no heat).
- Fix, found by Emory: **move the NeoTrellis ground wire to a different ground pin on the board.** Which pin to which pin was **not recorded** [U].
- The BC517 buffer was not at fault: it reproduced the ground it was referenced to.

Mechanism (established as a shared ground return; the rest is inferred):
- Everything that changed the symptom (brightness, supply voltage, LED colour) changed the current in a ground wire. Everything that did not touch the return path (caps across the sensor, series R, I2C speed) did nothing. [HG]
- The Gem boards carry two ground nets, `GND` and `GNDA`, joined at one net-tie (NT1) [V, SCH: Gem Stereo NT1 pads GNDA/GND; Gem Multi NT1 pads GNDA/GND]. Audio jack/line in/out/headphone grounds and the 3.3VA audio supply are `GNDA`; header 5V/3V3 power ground, analog-input header ground, Qwiic ground, battery ground are `GND` [V, SCH PCB netlists: Stereo J1/J2/J9 pin 1 = GNDA, J3 pin 2 = GND, J6 pin 1 = GND; Multi J11/J9/J7 = GNDA, J8/J10/J3/J4/J12 = GND].
- A NeoPixel pulls current in short, colour-dependent bursts (each channel 0-20 mA per LED, 16 LEDs on a NeoTrellis, refresh at a fixed rate). Return that current through a pin or wire that is also the reference for the piezo buffer or codec input and the wire's resistance and inductance (a few tens of milliohms for a header pin plus wire) turns it into a voltage in series with the audio signal. Estimate, not measured: 200 mA x 30 mOhm = 6 mV, about -44 dB re 1 V, clearly audible on a buffered piezo. [I]
- Colour dependence: R/G/B bytes set the PWM duty per channel, so the ripple spectrum and level follow the colour. That fits "pitch tracks colour". [I]
- 5 V made it worse because the LED current rose; "touch to 3.3 V reboots the board" says the 3.3 V pin was also being asked for more than it could give [I, HG]. Whether the NeoTrellis was fed from the audio-side 3.3 VA header or the digital 3V3 is not recorded [U].
- Not ground-related, do not confuse: input ch5 DC offset (bent pin, ESD latch), and the USB ground-loop isolator that could not power the Bela (error -71 boot loop) [HG memory].

Lesson that goes into this plan: the cheap test (move the ground wire) was the one that worked. Build V2 so that test is a 10-second job, and design out the shared return.

## 2. Gem Stereo facts

| Item | Value | Tag / source |
|---|---|---|
| Audio in | 2 ch, AC-coupled line in, TLV320AIC3104 codec; inputs `INL`/`INR` on a 3-pin header (GNDA, INL, INR) and a 4-pole 3.5 mm jack (T=L, R1=R, R2=GND, S=NC) | [V] DS Table 1, Fig. 1; SCH J1 |
| On-board input coupling | 1 uF series cap + 220 ohm + 47 pF + ferrite bead + BAS70 clamp on the input net | [V] SCH audio_in_net (C16, R7, C17, FB3, D2) |
| Mic bias | Not shown on the Stereo header or jack. MicBias is listed only on the Multi. A capsule needs an external bias/preamp | [V] DS Fig. 1 vs Fig. 2 (absence only) |
| Audio out | 2 ch: headphone (min. 1 ohm) and differential line out (`LEFT-/+`, `RIGHT-/+`) | [V] DS Table 1, Fig. 1 |
| Analog in | 8 ch, 16-bit, 250 kS/s total across channels, DC-coupled, 10-pin header `REF, GND, A7..A0` | [V] DS Table 1, Fig. 1; SCH J3 (pad 1 REFP, pad 2 GND, pads 3-10 A7..A0) |
| Analog ADC part | datasheet says ADS8166; the rev A4 schematic symbol says ADS8168. Same family. | [V] DS Table 2 vs SCH |
| Analog input range | 0 to 4.096 V single-ended, **inferred**: the ADS816x internal reference is 4.096 V and input FSR is 0..VREF; the JP2 jumper that ties 3V3 to REFIO is an open solder jumper on the schematic. The older Bela page also states 0-4.096 V, 5 V tolerance. Not stated in the Gem datasheet. | [I] ADC; SCH JP2; HW |
| Analog ground | **The analog-in header ground is `GND`, not `GNDA`.** There is no AGND pin on the header. | [V] SCH J3 pad 2 = GND |
| Digital I/O | 16 lines D0-D15, 3.3 V logic. Never apply more than 3.3 V (older-Bela docs; CPU pins) | [V] DS Table 1; [I] HW |
| Internal pull-ups on D0-D15 | Not documented. Use external pull-ups | [U] |
| I2C | Header pins P1.26 SDA, P1.28 SCL (named I2C2 in the schematic) and a Qwiic connector J6 (GND, 3V3, SDA, SCL). **10 k pull-ups to 3V3 are on the board** (R13/R14, note "I2C pull-ups!"). Codec I2C (AUD SDA/SCL, P1.33/P1.36) is a separate bus, do not use. | [V] DS Table 3; SCH i2c sheet |
| Linux bus number for that I2C | Not documented here. hyperGuitar (Gem Multi) used `/dev/i2c-1` | [U] on the Stereo; HG |
| 3.3 V pins | P1.14, P2.23. Current limit not documented | [V] pins DS Tables 3/4; [U] limit |
| 5 V pins (VSYS output) | P1.24, P2.13 "5V" are outputs of the PocketBeagle 2 power path; DS says not to connect an external source to them directly. Current limit not documented | [V] DS sec. 2.4; [U] limit |
| Power in | USB-C 5 V; P1.01 VIN_5V (3.8-5.5 V); P2.14 VBAT (3.2-4.6 V, single-cell Li-ion; battery charger up to 540 mA when USB 5 V present). Gem Stereo has no LiPo connector (Multi has) | [V] DS sec. 2.4, Table 1 |
| Power draw | PB2 + Gem Stereo: 1.21 W idle, 2.15 W audio + 4 cores | [V] DS Table 6 |
| Ground pins on the PB2 positions | P1.16, P1.22, P2.15, P2.21 = GND. P1.15 is **USBGND**, not for sensors | [V] DS Tables 3/4 |
| Audio ground | `GNDA` at the line-in header pin, line-out header pin, headphone header pin | [V] SCH J1, J9, J2 pin 1 |

Known unknown: how the PB2 P1/P2 positions (digital, 3V3, 5V, GND) are physically reached on an assembled Stereo. DS Fig. 1 shows only the analog header, line in/out, headphones, jacks. Confirm on the board in hand before cutting wire [U].

## 3. Why different sensors need different grounds

1. Two nets by design. `GNDA` (audio) and `GND` (digital, power, analog-in header, Qwiic) meet at one point (NT1) [V]. Current that returns through `GND` flows through the plane to that tie, and any resistance between the tie and a victim becomes a signal. A victim that is referenced to a different ground wire than the aggressor sees (I x R) between them.
2. Analog sensors (pots, sliders, FSR, joystick, analog Halls) are DC-coupled to the ADC. Their return wire carries sub-milliamp currents and must be quiet, because any millivolt on it is read as signal. Their ground goes to the analog header's own GND pin [V], which sits next to the ADC inputs.
3. Digital loads (buttons, encoders, I2C bus, lidar laser pulses, IMU, BME680) are low-current but edgy. They return on the digital GND pins.
4. LED current is the big one: 16 NeoTrellis LEDs plus 15 NeoPixels, tens to hundreds of mA, switching. It must not return through any Bela pin at all (section 4).
5. Audio is referenced to `GNDA` at the audio headers. Piezo buffers and mic preamps are the most sensitive nodes in the system (unity gain to the codec). Their ground is `GNDA`, taken at the line-in header GND pin, with nothing else on it.
6. Return currents add where wires share. Two sensors on one ground wire read each other's current. This is the hyperGuitar failure: NeoTrellis current in a wire shared with the audio side.

## 4. Star-ground plan

Star point (**STAR**): a terminal block or soldered pad cluster at the **battery / power-source negative terminal**, not a Bela pin. Every branch is its own wire to STAR. Only branches A-D go to the Bela, each to exactly one named pin.

```
 BATTERY(-) / boost(-)  == STAR
   |-- A  AUDIO      -> Gem Stereo LINE IN header GND (GNDA, J1 pin 1)   [mic buffers, bell mic preamp, audio jacks]
   |-- B  ANALOG SNS -> Gem Stereo ANALOG header GND (J3 pin 2, "GND")   [sliders, FSR, joystick pots, Hall analog out]
   |-- C  DIGITAL    -> P1.22 GND  (between D0 and D1)                   [buttons, encoders, I2C/Qwiic GND, ADS1115, IMU, lidar, BME680]
   |-- D  Bela power -> P2.15 GND (with 5V/VIN feed) or USB-C shield    [only if powering the Bela from the battery rail via P1.01]
   `-- E  LED/POWER  -> NeoDriver + NeoTrellis + strips power-in (-)     [NO Bela pin. Straight to STAR.]
```

Assignments (one ground wire per group, labelled at both ends):

| Group | Ground pin on Gem Stereo | Tag |
|---|---|---|
| A Audio: bell mic preamp, contact mic 1 and its buffer, TRRS/TRS jack sleeves | Line-in header pin GND = `GNDA` (Fig. 1 LINE IN GND; SCH J1 pin 1) | [V] |
| B Analog sensors: 3 sliders-group on the Bela ADC, FSR, joystick X/Y, 3 Hall outputs, contact mic 2 buffer | Analog header GND (J3 pin 2; one pin only) | [V] |
| C Digital: joystick button, arcade button, 2 encoders (A/B/btn), Qwiic trunk, ADS1115 GND, IMU, BME680, VL53L0X | P1.22 (alt. P1.16) | [V] |
| D Bela supply return | P2.15 or P2.21 | [V] |
| E LEDs: NeoDriver power terminal, strips, NeoTrellis if powered from 5 V | none; to STAR only | design |

Rules that follow:
- **Group B's single GND pin** feeds a small local star at the sensor board (a bus bar, not daisy chain). 3.3 V for sliders/pots/FSR/joystick comes from a filtered 3.3 V branch (100 ohm + 10 uF) taken at P1.14. Hall sensors draw several mA each (typically 6-10 mA for classic linear Halls [U, part-dependent]) so power them directly from 3V3 with local decoupling, not through the 100 ohm.
- **ADS1115's analog inputs** (Pot 1, Pot 2, Slider 2, Slider 3) reference the sensor local star (B), and its GND/3V3 supply pins also come from the sensor local star. Its I2C pins ride the Qwiic trunk (C). Run the Qwiic cable and the B-group wires as one bundle to keep the loop between C and B small.
- **Piezo buffers/mic preamps** (A) take 3.3 V from a dedicated RC-filtered branch (100 ohm + 47 uF) of P2.23 3V3 [not from the 5V], referenced to `GNDA` only.
- **I2C bus**: Qwiic J6 is the trunk (GND, 3V3, SDA, SCL). **Do not add pull-ups**: 10 k pull-ups are on the board [V]. Each Adafruit-style breakout adds its own pull-up (typically 10 k [U, per board]); with 5-6 of them in parallel the bus sees roughly 1.5-2 kohm. Check rise time with a scope or logic analyser; if slow, remove the pull-up pads/jumpers on the breakouts rather than adding more.
- **NeoTrellis (and NeoPixel) power: never from a Bela 3V3 or 5V pin.** Feed 5 V from the LED branch (E). Confirm the NeoTrellis' I2C logic level when powered at 5 V (it is a 3.3 V I2C part with onboard regulation? [U]); hyperGuitar saw it get worse at 5 V on a shared ground, which is a ground-sharing result, not a verdict on the part [HG]. Cap brightness in software (Emory does not need full brightness [HG]).
- **Ferrite rule (Emory, 2026-11-12): no ferrite or inductor in any ground or return path. Ferrites are allowed in series on supply rails only** (boost output to the LEDs/TX, boost output to the analog LDO input). Clip-on ferrites are not used on audio or sensor cables (their conductors include the return). No second USB data connection (Emory's constraint, hyperGuitar [HG]). The battery branch is the answer to "separate supply": a boost converter from the cell feeds the Bela VIN_5V (P1.01) and the LED/TX/sensor rails; VBAT/P2.14 stays unconnected. The board's own USB-C goes straight to the host and powers only the board (Gem internal ideal-diode OR, see `POWER_DESIGN.md`).
- Do not wire any sensor to P1.15 USBGND [V, DS Table 3].
- Do not use the Gem's `5V` pins to power LED strips: the datasheet gives no current limit and the pins are the PB2 system rail [U].
- Whenever the Bela's own USB-C is plugged in (bench or performance), USB shield ground reaches `GND` through the host. That makes a second path to STAR (a ground loop). Do not rely on a USB ground-loop isolator to power the Bela (it browns out; HG memory `bela-usb-power-isolator.md`).

## 5. Return currents that must never share a wire

1. LED/NeoPixel return and anything in A (audio) or B (analog sensors). Absolute.
2. LED return and the Bela GND pins (C, D). LED return goes to STAR, period.
3. Audio ground (A) and any digital ground (C). A and C meet only at STAR and at the board's own net tie NT1 [V]; do not bridge them at the sensors.
4. Contact mic buffer ground and the bell mic preamp ground can share a wire with each other (both A), but neither shares with a Hall sensor ground.
5. Hall sensor ground (B) and the ADS1115/pot ground (B local star) share only at the local star, on separate legs.
6. Battery charge/boost-converter input ripple current and any of A or B. The boost converter's input and output grounds sit at STAR with their own wire each.
7. Joystick/slider/FSR 3.3 V feed and the I2C 3V3: separate branches from the pin (P1.14 for sensors, J6/P2.23 for I2C).

## 6. Decoupling per group (all values are starting points [U], not measured)

| Node | Cap | Where |
|---|---|---|
| Sensor 3V3 branch | 100 ohm + 10 uF + 100 nF | at the sensor local star, after the RC |
| Each Hall sensor | 100 nF at VCC/GND pins; 10 nF to GND on output (keep source impedance low for the ADC input) | at the sensor |
| Slider/pot/FSR wipers | 100 nF wiper to local star GND | at the sensor board |
| Joystick X/Y | 100 nF each, wiper to GND | at the sensor board |
| Contact mic 2 (to Bela A0) | buffer output biased at 2.048 V mid-scale (the input is unipolar 0-4.096 V [I]); 100 nF + 10 uF on its supply; 1 uF coupling per the buffer design | at the buffer |
| Contact mic 1 and bell mic preamp | buffer/preamp supply: 100 ohm + 47 uF + 100 nF from 3V3; input already AC-coupled on the Gem (1 uF on board [V]) | at the audio board |
| ADS1115 | 100 nF + 10 uF at VDD/GND, ADDR to GND (0x48) | at the chip |
| I2C breakouts | 100 nF + 10 uF per breakout if not already on the board [U] | each board |
| NeoDriver / NeoPixel strips | 1000 uF electrolytic across the strip power in, **only fed from a supply that can charge it softly**; on hyperGuitar a 1000 uF across a weak 3.3 V made the rig brown-out loop [HG]. 300-500 ohm series on the strip data line | at the strip |
| Boost converter output | per converter datasheet | at the converter |

## 7. Bench checklist (do each before connecting the next group; mark readings)

Power off, battery disconnected, between every wiring change.

1. Continuity, no power: P1.22, P1.16, P2.15, P2.21, J3 pin 2, J6 pin 1, J1 pin 1 (GNDA). Expect each to read < 0.5 ohm to the others (NT1 ties GND and GNDA) [V, SCH]. Reading is not a verdict on the star plan; it only confirms the board.
2. Continuity STAR to each of A, B, C, D, E leg: record milliohms. E must **not** read continuity to any Bela GND pin except through STAR (lift the Bela-side wires to test).
3. Power on, no LEDs, no sensors: P1.14 and P2.23 = 3.3 V +/- 3%; P1.24 and P2.13 = about 5 V, USB or battery as fed. Record.
4. Add sensor 3V3 branch (after the RC): voltage at the sensor star under load. Hall rail must stay above its minimum supply with all three Halls on.
5. Scope on audio input with inputs shorted to GNDA: baseline noise in mVpp and as codec RAW-IN dc/peak (use the existing RAW-IN debug line, dc must stay near 0). Compare after each group is added. Any step that raises it is the offender.
6. Add I2C trunk. i2cdetect shows 0x2E, 0x29, 0x68, 0x77, 0x60, 0x48. Scope SDA rise time, goal < 300 ns at 400 kHz [U]. Bus must stay up through reboot (HG saw a loose connection fake a dead bus).
7. Ground the Bela's analog inputs through each sensor and read idle counts for 30 s: spread (peak-peak) per channel; Halls at no-magnet should read their mid-supply point.
8. Add LEDs at 1/4 brightness: repeat the audio noise measurement (step 5) while the LED animation runs and while changing colour. **Pass: no change from baseline.** Then go to the brightness you intend to use.
9. Pad-press and valve press with LEDs lit: no tones, no clicks on the audio output. Check the contact-mic channel while a valve key slaps.
10. The fast test: lift each group's ground wire one at a time and move it to another GND pin (P1.16, P2.21). If anything changes, that wire shared a return. This is the hyperGuitar fix, run it deliberately.
11. Charge/load: battery on charger with Bela running; record 3V3 and 5V ripple (mVpp) with LEDs animating. Brownout log: `journalctl -k | grep -E 'usb .*disconnect|-71'`.
12. Rail current: measure actual NeoTrellis current at the chosen brightness and the Bela 3V3 draw (neither was measured on hyperGuitar [HG]). Record. The Gem/PB2 rail limits are undocumented here [U]; this measurement decides if any sensor can stay on a Bela 3V3 pin.

## 8. Open items (unverified)

- Physical access to P1/P2 positions on an assembled Gem Stereo (sec. 2).
- Analog input range on the Gem (inferred 0-4.096 V).
- Internal pull-ups on D0-D15 (undocumented).
- Linux I2C bus number for P1.26/P1.28.
- Current limit of 3V3 and 5V header pins.
- Rev A4 (repo) vs shipped rev A5+ (e.g. USB-A power source changed in A5; DS sec. 2.4).
- Which ground pins moved in the hyperGuitar fix (not recorded).
- NeoTrellis I2C logic behaviour when VIN is 5 V.

## 9. Seed 3 notes (2026-11-12; board changed from Bela Gem Stereo to Daisy Seed 3)
Sections 1-8 describe the Bela Gem Stereo (GND/GNDA, NT1 net-tie). Principles carry over unchanged, pins differ. Pins: `PINMAP_SEED3.md`.
- **Star ground at battery negative.** Boost return, LED/NeoTrellis return, TX return, preamp ground and the Seed each get their own wire to STAR.
- **LED returns away from the audio ground.** NeoTrellis current never returns through a Seed pin; it goes to STAR on its own wire. Strips are dropped on the trumpet, which removes the largest LED load.
- **Ferrites only on supply rails** (boost to NeoTrellis/TX/LDO input). None in any ground or return path.
- Seed has AGND (pin 20) and DGND (pin 40) [V, original Seed datasheet]. Mic preamp and shield ground go to AGND, sensor and I2C ground to DGND, as the Gem GNDA/GND split. Whether AGND/DGND are tied on the board and where [U]: continuity-check Seed 3 before wiring.
- The Seed has no separate analog-input ground header; ADC sliders/pots/FSR/joystick return to DGND at the star-side branch used for sensors (still a quiet wire shared by sensors only).
- The Gem-specific facts in section 2 (net-tie, 0-4.096 V ADC, Qwiic ground) do not apply. Seed ADC is 0-3.3 V.
- Mic cable shield grounded at the preamp end only (unchanged).
- Seed 3 powered from 5 V into VIN (39) while USB is plugged in: original Seed allows both together [V DS]; Seed 3 [U]. Host USB ground shares with the instrument via the USB cable; if hum appears, test with a USB isolator (note hyperGuitar's isolator could not power its Bela, but the Seed is powered from VIN, so an isolator is viable [I]).
- Re-run the bench checklist (section 7) with Seed pins: NeoTrellis ground move test is still a 10-second job if each group has its own star branch.

## Bench-verified 2026-11-16 (Seed 3, HYPERTRUMPET bring-up)
- **AGND must be tied to DGND with one short wire at the Seed.** Without it, as soon as audio starts (StartAudio) every ADC channel reads one constant (0x7FFF / 0xBFFF / 0x8000) and the line input is nearly dead (-90..-70 dBFS). Proven with a 30-line libDaisy repro (fw/repro09.cpp, variants 09a-d): ADC fine with audio off, frozen with audio on, fixed instantly by the AGND-DGND wire. Same symptom as libDaisy issue #725 (open) and the Daisy forum "Can't get ADC working with audio callback" thread. One tie point only; audio grounds (mic shield/black, preamp GND, Xvive cable) stay on AGND, sensors on DGND.
- Star ground is at BMS P- (not B-). Power, LED strip and Xvive returns go to the P- star on their own wires.
- 3V3A measured 3.2 V under full sensor load with audio running: the analog rail was not the cause.
