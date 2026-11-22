# HYPERTRUMPET V2: bill of materials

Canonical parts list for building the instrument: a Daisy Seed 3 trumpet (C++ DSP, 4 FX slots) with a bell mic, analog Hall valve sensors (via ADS1115), NeoTrellis feedback and a 2-cell (1S2P) battery. The earlier Bela Gem Stereo variant is in `_attic/BELA_VARIANT/`. Written for an outside builder.

- Source: `docs/BOM_SHOPPING_2026-11-12.md` (Amazon-first revision). Prices are ESTIMATES: Amazon pages did not load when this was written, and ASINs came from search results. Verify seller, price and specs in the browser.
- "Product" gives the Amazon search link and, where one was found, the product URL. "Reference" is the manufacturer / Adafruit / Pololu / DigiKey page.
- "Owned" means Emory already has it; outside builders still need it.
- Items marked **verify** are unconfirmed in the source.
- Power chain and charging station: see [`../SHARED_POWER_KIT.md`](../SHARED_POWER_KIT.md). Section "Power" below only lists what this build needs from it.

Amazon-risk flags: fake or misrated risk on N52 magnets (grade), 1S "10 A" PCMs (bench-load it), op-amps (OPA1612/OPA2134, unfamiliar sellers), LT3045 modules (generic), XTAR/Nitecore chargers (buy sold by brand), ADS1115 lookalike ICs. Never buy the Molicel cells on Amazon.

## As-built parts (2026-11-12)

Exact parts Emory is using. Cart subtotal $477.35, 19 lines (Amazon, prices at cart time; shared cart across hat + trumpet). Where this section and the design reference below differ, this section wins.

### Amazon and ordered

| Part (exact listing name) | Qty used in this build | Source | Price paid ea (listing price) | Role | Notes |
|---|---|---|---|---|---|
| BLLNDX 3.5mm Stereo Panel Mount Jack 2PCS | 1 | Amazon | $5.99 | Wired stereo out, 1 per instrument | Unswitched, so the TX stays in parallel. |
| 3.5mm to Dual 6.35mm Y-Adapter Splitter 0.6 ft | 1 | Amazon | $9.99 | Stereo out into L/R TX inputs; also the Xvive test | Cart qty 2 (1 per instrument). |
| 10-Pack High-Current 21700 Battery Holder for PCB Mount (40A, red copper nickel-plated contacts), ASIN B0GFSMXJF7 | 2 | Amazon | $22.99 (10-pack) | Single-cell holders, wired in parallel | Never series. 10-pack: 4 used, 6 spare. |
| Twidec 10Pcs Mini Toggle Switch SPST ON/OFF 5A | 1 | Amazon | $9.99 | Main power | 10-pack. |
| eMagTech 5 Pack 1S 10A 4 MOS BMS Protection Board | 1 | Amazon | $9.09 | Cell protection; TX battery pads are fed from its output | 5-pack. |
| ELEGOO PETG 1.75mm Transparent 1KG | [confirm] | Amazon | $16.99 | Snap-fit shells, valve caps, mounts | No screws. |
| Xvive A58 Wireless Guitar System 5.8GHz | bench test (shared) | Amazon | $149 | Mod/bench test unit | Qty 1 in cart. If it passes, 5 more make the 3 stereo pairs. |
| ELEGOO 120pcs Dupont Wire | shared | Amazon | $6.98 | Jumper wiring | Cart qty 2. |
| BTF-LIGHTING FCOB COB WS2812B 5V 3.2ft 160LED/m 5mm IP30 | 1 | Amazon | $13.99 | LED strip, about 157 px | Cart qty 2 (one hat, one trumpet). Firmware brightness cap about 20/255; data through a level shifter. |
| Teyleten KY-023 Dual-axis Joystick 5pcs | 1 | Amazon | $9.99 (5 pcs) | Joystick | Firmware centre deadzone. |
| Senyulian 12 Bays Smart Universal Charger (18650/21700) | shared | Amazon | $39.68 | Charging station | External charging only. |
| LinkWheres 20Pcs 49E Linear Hall Effect Sensor (OH49E/SS49E TO-92S) | 3 | Amazon | $9.99 (20 pcs) | Valve position sensors | On Seed ADC directly (no ADS1115), at 3.3 V, 100 nF cap at each sensor, firmware oversampling, per-valve calibration. |
| MAGABALLNET 60 Pack 10×3mm Neodymium Magnets | 3 (valve caps) [confirm] | Amazon | $14.99 (60 pcs) | Valve-cap magnets | Grade not stated in the listing (N35-N52). Keep a minimum 5 mm gap to the Hall to avoid saturation. |
| uxcell 5pcs 45mm B10K Slide Fader | [confirm] | Amazon | $9.79 (5 pcs) | Trumpet sliders |  |
| FSR402 Force Sensor 3 pcs | [confirm] | Amazon | $22.99 | Force sensing | Cart qty 2. |
| Taiss 5Pcs KY-040 Rotary Encoder | [confirm] | Amazon | $8.99 (5 pcs) | Encoders | Firmware debounce. |
| XIYANGJUAN 4Pcs Metal Guitar Knobs Bronze | [confirm] | Amazon | $12.99 (4 pcs) | Knobs | Role/instrument not logged [confirm]. |
| 21700 5000 mAh flat-top, unprotected (IMR Batteries); 2 per instrument plus spares | 2 (plus spares) | imrbatteries.com (non-Amazon) | [confirm] | Cells, 1S2P | Emory ordered; ~~exact model and qty not logged~~ model/qty confirmed 2026-11-12 (above); IMR part number and price still not logged. Unprotected cells: the BMS is the only protection, never run without it. |

### Already owned (qty used)

- Daisy Seed 3: 1
- Adafruit NeoTrellis 4x4: 1
- TPS61088 5V boost module: 1 (set output to 5.0-5.1 V with a meter before connecting)
- 74AHCT-class level shifter: 1 (per strip)
- Potentiometers (incl. a 10k)
- Hookup wire, protoboard, capacitors
- USB-C cable (Seed onboard USB-C is exposed through the shell)
- MPU6050 IMU and VL53L0X lidar
- 10k pot as the trumpet mic level
- Dynamic clip-on mic (direct into Seed line in, no preamp)

### Not used in this build

- BME680 breath sensor: not wanted
- Arcade button: not used
- ADS1115: Seed has 12 ADC channels
- MAX17048 gauge: fixed 4 h plus swaps instead
- LT3045/LDOs: TX battery pads are fed straight from the protected cell (BMS output); the Seed has onboard regulation
- NE5532 and LM386 boards: NE5532 needs >=6 V; LM386 is a speaker amp and noisy
- INA217: needs about 9 V or more
- Contact mics: not used
- NeoDriver/seesaw: not used
- Qwiic cables and JST pigtails: soldered direct
- Heat shrink: not used
- Screws/inserts: snap-fit shells
- Panel USB-C breakout and LM66200: Seed onboard USB-C exposed through the shell

### Power chain (as built)

```
2 cells in parallel (1S2P) -> eMagTech BMS -> Twidec toggle -> TPS61088 boost (5.0-5.1 V) -> Seed VIN + FCOB strip (strip data through a level shifter).
Xvive TX battery pads come off the BMS output.
Grounds: power, LED and TX return go straight to the star at battery negative; sensors to Seed DGND; audio to Seed AGND.
```

Runtime, 2 cells (5000 mAh cells: 5 Ah x 3.6 V = 18 Wh nominal x 0.90 usable x 0.88 boost = 14.3 Wh each new, 11.4 Wh at 80 % age; 2 cells = 28.5 / 22.8 Wh):
- Trumpet, about 4.0 W (now has the strip): 28.5 / 4.0 = 7.1 h new, 22.8 / 4.0 = 5.7 h aged.
- Hat, about 3.7-4.9 W: 28.5 / 3.7 = 7.7 h and 28.5 / 4.9 = 5.8 h new; 22.8 / 3.7 = 6.2 h and 22.8 / 4.9 = 4.7 h aged.
- Single cell is half of each (trumpet 3.6 / 2.9 h; hat 3.9-2.9 h new, 3.1-2.3 h aged). Estimates, not measured.

| Estimated runtime on the 21700 5000 mAh cells (2026-11-12) | 2 cells new (28.5 Wh) | 2 cells aged 80 % (22.8 Wh) | 1 cell new (14.3 Wh), reference | 1 cell aged (11.4 Wh), reference |
|---|---|---|---|---|
| Trumpet, about 4.0 W | 7.1 h | 5.7 h | 3.6 h | 2.9 h |
| Hat, 3.7-4.9 W | 7.7-5.8 h | 6.2-4.7 h | 3.9-2.9 h | 3.1-2.3 h |

1-cell columns are half the 2-cell times (rounded), for reference only: the instruments are built 1S2P. Estimates, not measured.

## Design reference (pre-build)

Note: the as-built table above wins where they differ. Superseded rows below are struck through, not removed.

## 1. Core

| Part | Qty | Spec floor | Product | Reference | Substitutes | ~Price | Notes |
|---|---|---|---|---|---|---|---|
| Daisy Seed 3 | 1 | TAC5242 codec, USB-C, stereo line in/out (~1 Vrms in), 12 ADC (0-3.3 V), I2C1 on D11/D12; with headers | Electro-Smith / daisy.audio or reseller (not Amazon-first) | https://daisy.audio | none (Seed 1/2 have a different codec and micro-USB) | $34.99 | **Buy 1** (price from a forum post, unconfirmed). Emory's existing Seed 3 goes to the hat. No mic bias: bell mic needs the external preamp. Pins: `docs/PINMAP_SEED3.md`. |

## 2. Power (shared chain, see SHARED_POWER_KIT.md)

Trumpet = **2 cells in parallel (1S2P), 2 single-cell holders**, same as the hat (decided 2026-11-12). Chain: cell, protection board, switch, U3V40F5 5 V boost, then Seed VIN (39; original Seed accepts 4-17 V, Seed 3 range to verify) plus the NeoTrellis/TX/preamp rails. The Seed's onboard USB-C goes straight to the host (data + board power; original Seed allows VIN and USB together, Seed 3 [U]).

| Part | Qty | Spec floor | Product | Reference | Substitutes | ~Price | Notes |
|---|---|---|---|---|---|---|---|
| ~~Molicel INR21700-P45B cells~~ (SUPERSEDED 2026-11-12, see As-built) | 2 (of an 8 buy) | Genuine, flat-top, one lot, matched pair | authorized vendor, not Amazon | https://imrbatteries.com/products/molicel-p45b-21700-4500mah-45a-battery | liionwholesale.com, 18650batterystore.com, Fogstar, nkon.nl | $6.50 | Non-Amazon by decision (fakes). Swap rules: power off, swap the pair together, matched pairs, keyed bays (kit). |
| ~~Single 21700 holder~~ (SUPERSEDED 2026-11-12, see As-built) | 2 | 1 cell each, spring contacts, >=5 A, never 2-4 slot | [Bistook 1-slot 10-pack](https://www.amazon.com/s?k=Bistook+21700+battery+holder+1-slot+PCB+mount+10+pack), https://www.amazon.com/dp/B0BSC61X69 | https://www.digikey.com/en/products/detail/keystone-electronics/1121/12751670 | B0GFSMXJF7, B0FMGNW4WL, Keystone 1121 | $2.00 | Wire the pair in parallel, short equal leads. |
| ~~1S PCM >=5 A, multi-FET~~ (SUPERSEDED 2026-11-12, see As-built) | 1 | see kit | [ACEIRMC search](https://www.amazon.com/s?k=ACEIRMC+1S+3.7V+10A+6MOS+BMS+protection+board+18650), https://www.amazon.com/dp/B08MPXHFJB | https://power.tenergy.com/protection-circuit-module-pcb-for-3-6v-3-7v-1s-li-ion-battery-pack-working-5a-cutoff-15a | eMagTech, Acxico | $1.30 | Bench-load to 5 A. |
| ~~Pololu U3V40F5 5 V boost~~ (SUPERSEDED 2026-11-12, see As-built) | 1 | see kit | [search](https://www.amazon.com/s?k=Pololu+U3V40F5+5V+step-up) | https://www.pololu.com/product/4012 | none under-current | $9.95 | Buy from Pololu. |
| ~~MAX17048 gauge~~ (SUPERSEDED 2026-11-12, see As-built) | 1 | I2C 0x36 | [search](https://www.amazon.com/s?k=MAX17048+LiPoly+battery+monitor+STEMMA+QT+Qwiic), https://www.amazon.com/dp/B0HL8QMYTN | https://www.adafruit.com/product/5580 | B0HHH929C6 | $6.00 | |
| ~~Hard switch >=3 A~~ (SUPERSEDED 2026-11-12, see As-built) | 1 | latching, in B+ | [search](https://www.amazon.com/s?k=SPST+toggle+switch+5A+panel+mount+mini+latching) | - | rocker/slide | $3.00 | |
| ~~JST-PH 2.0 pigtails~~ (SUPERSEDED 2026-11-12, see As-built) | 2 | 2.0 mm PH, 22 AWG | [search](https://www.amazon.com/s?k=JST+PH+2.0+2+pin+cable+22AWG) | - | any | $1.50 | Check polarity. |

## 3. Audio

USB: no panel USB-C breakout. The Seed 3's own USB-C is exposed through a housing opening (see Housing requirement below). Seed line in L = bell mic preamp out; R free.

| Part | Qty | Spec floor | Product | Reference | Substitutes | ~Price | Notes |
|---|---|---|---|---|---|---|---|
| ~~Bell mic capsule, WM-61A-class omni electret~~ (SUPERSEDED 2026-11-12, see As-built) | 2 (2-pack) | Omni 2-wire electret, 6 mm; mount 8-15 cm off-axis, never in the bell throat; gain pad for bell SPL | [search](https://www.amazon.com/s?k=WM-61A+omnidirectional+electret+condenser+microphone+capsule), https://www.amazon.com/dp/B018HXM9RS | https://www.micbooster.com/ | Primo EMU4535 (B08T28PG6P); DPA 4061 / Countryman B3 (not Amazon, SPL unverified) | $7.00 | S/N >62 dB (EM272 ~80 dB). Max SPL ~115 dB **verify**. Cut lead, solder direct, no connector. |
| ~~Preamp op-amp OPA1612, DIP-8 breakout~~ (SUPERSEDED 2026-11-12, see As-built) | 1 (buy 2) | <=5 nV/rtHz, >=4.5 V single supply, no AGC; one half preamp, other half bias buffer | [search](https://www.amazon.com/s?k=OPA1612+DIP8+dual+op+amp+audio+low+noise), https://www.amazon.com/dp/B0HFZRX3K6 | https://www.ti.com/product/OPA1612 | OPA1642 SOP-8 + adapter; OPA2134PA (8 nV, needs >=5 V) | $9.00 | Counterfeit risk: compare noise on a loopback. Do not power from the Seed 3V3. |
| ~~Low-noise LDO module (LT3045/LT3042)~~ (SUPERSEDED 2026-11-12, see As-built) | 1 | ~1 uVrms class, >=200 mA, >=60 dB PSRR at 1 kHz, not AMS1117; ~4.5 V from the 5 V boost | [search](https://www.amazon.com/s?k=LT3045+low+noise+LDO+module+adjustable), https://www.amazon.com/dp/B0GW4SCBNM | https://www.analog.com/en/products/lt3045.html | LT3042 modules (B0H2PS6PN6, B0GSR2Q39N) | $9.00 | Needs adjustable output; else RC-filter the 5 V. |
| ~~Electret bias + film-cap set~~ (SUPERSEDED 2026-11-12, see As-built) | 1 | 2.2 k bias R from RC-filtered rail (1 k + 100 uF + 100 nF); 1-10 uF film coupling | [Wima MKS2 assortment](https://www.amazon.com/s?k=Wima+MKS2+film+capacitor+assortment) | - | any metal-film R 1%, film caps | $4.00 | |
| ~~Shielded mic cable ~1.5 m~~ (SUPERSEDED 2026-11-12, see As-built) | 1 | 2-conductor + shield, low capacitance | [Canare L-4E6S](https://www.amazon.com/s?k=Canare+L-4E6S+mic+cable+per+foot) | https://www.canare.com/ | generic quad-star | $6.00 | Shield to ground at the PREAMP end only. |
| Preamp protoboard | 1 | plated-through FR4, 2.54 mm, ~5x7 cm | [search](https://www.amazon.com/s?k=double+sided+prototype+PCB+board+5x7cm+2.54mm+plated+through) | - | any plated-through | $3.00 | Multipack. |
| ~~Switched 3.5 mm stereo panel jack (TRS)~~ (SUPERSEDED 2026-11-12, see As-built) | 1 | panel mount, 5-lug with break contacts | [search](https://www.amazon.com/s?k=3.5mm+stereo+panel+mount+jack+switched+5+pin) | - | Switchcraft 35RAPC4BH3 **verify** | $3.00 | Check photos for 5 lugs. |

## 4. Sensors

| Part | Qty | Spec floor | Product | Reference | Substitutes | ~Price | Notes |
|---|---|---|---|---|---|---|---|
| Linear Hall, TO-92 (SS49E) x3 (50-pack) | 3 | Linear ratiometric analog, 3.3 V; not 5 V-only A1324 | [search](https://www.amazon.com/s?k=SS49E+linear+hall+effect+sensor+TO-92+50+pcs), https://www.amazon.com/dp/B0HCKBZHZ1 | https://www.digikey.com/en/products/detail/texas-instruments/DRV5055A2QLPG/9371281 | DRV5055A1 SOT-23 2-pack (B0DYSM41PJ) on SOT-23 adapters (garbled titles, fake risk) | $9.00 | SS49E is ~3x less sensitive than DRV5055A2 (9 vs 30 mV/mT at 3.3 V): closer gap or 8x3 magnet, RC 1k+100nF, oversample, per-valve LUT. DRV5055A2 TO-92 is the better part (DigiKey). |
| N52 disc magnets 6 x 3 mm, AXIAL | 1 pack (also 8x3) | Axially magnetized, N50+ stated grade, NiCuNi | [search](https://www.amazon.com/s?k=N52+neodymium+disc+magnet+6mm+x+3mm+axial+50+pack) | https://kjmagnetics.com/ | K&J Magnetics (verified grade, not Amazon) | $9.00 | Fake-grade risk: reject listings with no stated grade. |
| ~~ADS1115 16-bit ADC (3 Halls on A0-A2)~~ (SUPERSEDED 2026-11-12, see As-built) | 1 | Real TI chip, 0x48 (ADDR GND), 3.3 V, STEMMA QT | [search](https://www.amazon.com/s?k=Adafruit+ADS1115+16-bit+ADC+4+channel), https://www.amazon.com/dp/B00QIW4MGW | https://www.adafruit.com/product/1085 | generic STEMMA QT B0FXR583CC; ADS1015 | $15.00 | Beware lookalike ICs. Slow controls (sliders, pots). |
| ~~Qwiic/STEMMA QT JST-SH 4-pin cable assortment (50/100/200 mm)~~ (SUPERSEDED 2026-11-12, see As-built) | 1 | JST-SH 1.0 mm, GND/3V3/SDA/SCL | [search](https://www.amazon.com/s?k=STEMMA+QT+JST+SH+4+pin+cable+assortment+50mm+100mm+200mm) | https://www.adafruit.com/search?q=stemma+qt+cable | SparkFun Qwiic kit | $9.00 | 6 I2C devices. |
| ~~Qwiic/STEMMA QT JST-SH to 4-pin female header cable, 100-150 mm~~ (SUPERSEDED 2026-11-12, see As-built) | 1 | JST-SH 1.0 mm, GND/3V3/SDA/SCL | [search](https://www.amazon.com/s?k=Qwiic+JST+SH+4+pin+to+female+header+cable+150mm) | https://www.adafruit.com/search?q=stemma+qt+cable | SparkFun Qwiic kit | $5.00 | Daisy has no Qwiic port: to D11 SCL / D12 SDA / 3V3 / GND. |
| v1 sensors: BME680, VL53L0X, MPU6500, joystick, 3 sliders, 2 pots, 2 encoders, FSR, arcade button | 1 set | reuse from v1 | - | - | rebuy ~: BME680 $18, VL53L0X $15, MPU6500 $6 | - | Owned by Emory (confirm each). |
| Pull-ups, Hall filters, decoupling | | 10 k to 3V3 on digital lines, 1k + 100 nF RC per Hall, 100 nF at VCC | from the resistor and capacitor kits (Wiring) | - | - | - | No extra purchase. |

I2C map: ADS1115 0x48, MAX17048 0x36, NeoTrellis 0x2E, BME680 0x77, VL53L0X 0x29, MPU6500 0x68: no conflicts. Daisy documents no pull-ups; each breakout adds its own: measure the parallel value (keep >= ~2 k).

## 5. LEDs

| Part | Qty | Spec floor | Product | Reference | Substitutes | ~Price | Notes |
|---|---|---|---|---|---|---|---|
| NeoTrellis 4x4 | 1 | Adafruit NeoTrellis, I2C 0x2E | - | https://www.adafruit.com/search?q=neotrellis | none | - | Owned by Emory. 5 V power from the boost, own wire to STAR. **NeoPixel strips, seesaw driver and 74AHCT125 dropped on the trumpet**: the Trellis lights (PINMAP_SEED3 s7). |

## 6. Wiring and consumables

| Part | Qty | Spec floor | Product | Reference | Substitutes | ~Price | Notes |
|---|---|---|---|---|---|---|---|
| Silicone hookup wire kit 22 + 26 AWG | 1 | stranded, tinned copper (not CCA) | [search](https://www.amazon.com/s?k=silicone+stranded+hookup+wire+22AWG+26AWG+kit) | https://www.adafruit.com/product/2003 | any | $25.00 | |
| Resistor assortment 1% metal film | 1 | 1/4 W, incl 330, 1k, 2.2k, 10k | [search](https://www.amazon.com/s?k=1%25+metal+film+resistor+assortment+kit+1%2F4W) | - | any | $14.00 | |
| Capacitor assortment | 1 | C0G/X7R (no Y5V), 1-10 uF, 47/100 uF low-ESR, 1000 uF | [search](https://www.amazon.com/s?k=ceramic+capacitor+assortment+kit+X7R+100nF+10uF+electrolytic) | - | any | $18.00 | |
| ~~JST-PH 2.0 connector kit with crimps~~ (SUPERSEDED 2026-11-12, see As-built) | 1 | 2.0 mm | [search](https://www.amazon.com/s?k=JST+PH+2.0+connector+kit+with+crimp) | https://www.adafruit.com/product/261 | any | $12.00 | |
| Wago 221-415 / terminal block (star ground) | 2 | low resistance | [search](https://www.amazon.com/s?k=Wago+221-415+lever+nut+5+conductor) | - | terminal block | $3.00 | GROUNDING_PLAN section 4. |
| TPU 95A filament 1 kg (liner) | 1 | 1.75 mm, dry | [search](https://www.amazon.com/s?k=TPU+95A+filament+1.75mm+1kg) | https://us.store.bambulab.com/ | Overture/Sunlu | $20.00 | |
| ~~Heat shrink + zip ties~~ (SUPERSEDED 2026-11-12, see As-built) | 1 | 2:1 / 3:1 adhesive-lined | [search](https://www.amazon.com/s?k=heat+shrink+tubing+assortment+adhesive+lined) | - | any | $10.00 | |
| Ferrite beads (supply rails only) | 1 | >=100 ohm at 100 MHz, rated for the rail current | [search](https://www.amazon.com/s?k=ferrite+bead+assortment+through+hole) | - | any | $8.00 | Rule: no ferrite or inductor in any ground or return path; ferrites allowed in series on supply rails (boost output to LEDs/TX/analog LDO input). No clip-ons on audio/sensor cables. |

## 7. Tools

| Part | Qty | Spec floor | Product | Reference | Substitutes | ~Price | Notes |
|---|---|---|---|---|---|---|---|
| Solder 63/37 0.5 mm, flux pen, desolder braid, Kapton | 1 | no-acid flux | [search](https://www.amazon.com/s?k=63%2F37+solder+0.5mm+flux+pen+desoldering+braid+kit) | - | own stock | $20.00 | Owned by Emory (confirm). |
| Soldering iron, multimeter (CC check, bench-load, polarity), scope for noise checks | 1 each | - | - | - | - | - | Not in the shopping BOM. |
| 3D printer (Bambu X1C used) | 1 | TPU capable | - | - | any printer or service | - | Owned by Emory. |

Estimated cost (trumpet excl. owned, from source): **$155.24** (2 holders now, +$1.00; was $140.25 with the Bela: + Daisy Seed 3 $34.99, + header cable $5.00, - holder $1.00, - contact-mic buffer $14.00, - NeoPixel driver $6.00, - 74AHCT125 $5.00); shared kit $280.00 (includes the 8 cells, one 8-bay charger, consumables; was $308.00). Excludes shipping/tax and wireless.

## Housing requirement (USB)
Printed strain-relief cradle at the Daisy Seed 3 USB-C port, plus an opening sized for a right-angle or slim USB-C plug, so cable force never loads the board's connector. No panel breakout, no pigtail.

## Why these parts
- **Daisy Seed 3 (decided 2026-11-12):** only 3-4 effects run at once, all v2 DSP is C++, no USB audio needed. Shares one portable C++ DSP library and one param-schema editor (PMOR V3 pattern: USB CDC, schema, web editor over WebSerial) with VOCAL_HAT. Slot engine: 4 FX slots, any library effect per slot, CPU bounded by the worst 4. Bela variant kept in `_attic/BELA_VARIANT/`.
- **Analog Halls on the valves via ADS1115:** continuous valve position, clean 16-bit. SS49E is the Amazon-only stand-in; DRV5055A2 is better.
- **Bell mic via preamp into line in L:** no mic bias on the Seed; MAX9814 was rejected for its AGC.
- **Shield grounded at the preamp end only:** avoids ground loops (see GROUNDING_PLAN).
- **2 cells, 1S2P, like the hat (2026-11-12):** parallel pair for capacity to run the Seed plus a stereo pair of 2 TX (~0.75 W each) from the instrument battery. Trumpet load 2.7 W: 9.5 h new / 7.6 h aged on 2 cells (4.7 / 3.8 h on 1 cell). The boost, MAX17048 (1S gauge) and no balance wiring are unchanged. Estimates; see the runtime table in `../SHARED_POWER_KIT.md`.
- **Single holders only:** multi-slot holders wire cells in series.
- **U3V40F5 boost:** the Amazon-listed Pololu boosts are under-current.
- **Hard switch in B+:** true off for the battery chain.
- **External charging, no fuse or charger board.**
- **Seed's own USB-C, no panel breakout:** strain relief printed into the housing.
- **No NeoPixel strips:** the NeoTrellis lights; removes a driver, a level shifter and an LED-return noise path next to the preamp.
- **No ferrite or inductor in ground/return paths (supply-rail ferrites allowed), star ground at battery negative:** lessons from hyperGuitar.

DSP credit: the flanger and grain delay are by **Iain McCurdy** (Csound examples, CC BY-NC-SA); credit and share-alike apply.

## Suggested wireless (optional)
Not in the order. Written as an open-source suggestion for builders.

- **Provisional lead: Boss WL-20L (Emory is researching a replacement unit; do not treat this as the pick).** Boss's FAQ says WL-20L suits active-pickup, preamp-equipped and aerophone/line-level sources; WL-20 adds a cable-tone roll-off meant for passive guitars. 2.3 ms latency, 14 systems per space, ~50 ft. Amazon (from search results): WL-20L B07F2PY7DD, WL-20 B07F2NZWY5, spare TX WL-T B07DVZH2LN (pages gave 503; price ~$200 per set [unverified], stock unconfirmed).
- **Stereo-pair approach:** the WL-20 family is mono (only tip/L is transmitted). Stereo = two matched sets of the same model, one per channel. Three stereo instruments = 6 sets (~$1,200); trumpet + hat = 4 sets (~$800). 14 systems per space leaves headroom for 6 links. A fixed latency offset between L/R can be delay-compensated in DSP; per-packet jitter cannot.
- **Level matching:** Seed 3 line out full scale is 1 Vrms single-ended / 2 Vrms differential at the TAC5242 chip (board-level [U]). WL-T max input is unpublished, so start with a ~-26 dB pad per TX (20 k series / 1 k shunt, or a 10 k trimmer), then trim up. Optional parts: 1/4 in mono panel jack per TX (WL-T is a plug-in unit), trimmers.
- **Requirement (2026-11-12): the TX must run continuously from external 5 V/USB power with its internal cell removed or bypassed.** The TX are powered from the instrument battery (the 5 V boost rail through a low-noise LDO), never their own internal battery. Each instrument carries a stereo pair of 2 TX, budgeted at about 0.75 W each (an estimate, 1.5 W per instrument; see the runtime table in `SHARED_POWER_KIT.md`).
- **Powering caveats:** the Boss WL-20 / WL-T has a non-removable battery and cannot run while charging (Boss FAQ), so it is a poor fit for this requirement unless bench-proven (unknown fuel-gauge/protection behavior when fed externally). Any candidate unit must pass the external-power requirement above on the bench first. The RX can run while charging, but USB adapter noise can drop audio. Keep TX antennas off brass (15-30 cm, perpendicular to tubing).
- **Alternatives:** Rode Wireless GO II 2-person kit in split mode (2 TX to 1 RX, TX1 = L, TX2 = R, matched latency by construction, max input -20 dBV so a pad is mandatory, ~$299 per stereo link); Boya BY-WM4 Pro K2 (~$100-130, latency unverified); Xvive U2 / Line 6 G10 as cheap mono pairs; Shure GLXD1+ pairs (~$1,000+) or Sennheiser EW-D (UHF, best through brass, ~$2,000+) for pro. Don't mix two 2.4 GHz families (reported dropouts).
- **Guitar:** future, 2 more WL-20L sets (~$400) plus the 6-ch wired path.
