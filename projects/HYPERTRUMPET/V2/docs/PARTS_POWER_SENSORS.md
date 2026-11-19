# HYPERTRUMPET V2: parts, power, sensors

> **2026-11-12:** Bela-era power numbers (2 cells, Gem loads). See `../../SHARED_POWER_KIT.md` (runtime) and `PINMAP_SEED3.md`.


Research date 2026-11-11. Link-collection only; nothing purchased.

**Link status key.** CONFIRMED = a product page I found in search results. UNCONFIRMED = not verified.
**Amazon: I could not confirm any Amazon product URL or ASIN.** amazon.com returned 503 to my fetch and search never surfaced a /dp/ page. No ASINs are listed. Where Amazon would be the first choice, the Amazon search URL is given as a search, not a product. Distributor links are the real ones.

---

## 0. Bela Gem Stereo facts that drive everything

Source: Bela Gem datasheet v1.0 (2026-08-04), https://bela.io/datasheets/bela_gem_datasheet.pdf ; Bela powering page, https://learn.bela.io/using-bela/bela-techniques/powering-bela

- Analog header A0..A7 + GND + REF is on the Gem Stereo pinout (datasheet Fig. 1). ADC is ADS8166, 16-bit, up to 250 kHz across all channels.
- **Input range: 0 to 4.096 V** (ADC internal 4.096 V reference, default, per the Bela powering page). The datasheet itself states no analog-input range. 1 LSB = 62.5 uV. JP2 on the Gem Stereo switches the reference to the PB2 3.3 V rail (then range is 0 to 3.3 V, needs matching ADS8166 register changes in software).
- Do not feed the Hall sensors from 5 V: a DRV5055 at 5 V swings to 4.8 V and would clip the 4.096 V ADC. Feed them from the 3.3 V pin (P1.14 / P2.23).
- **Bela needs a true 5 V supply.** Bela states that below ~4.5 V "the USB and analog I/Os misbehave". So a 1S cell wired straight to VBAT (P2.14) is not acceptable for the sensors. Feed a boost converter output into VIN_5V (P1.01), per Bela: "wire the booster output to P1.01 and ground to any available ground, not to the VBAT or battery socket." Valid VIN_5V range is 3.8 to 5.5 V; target >= 4.75 V.
- Power on-board: ideal-diode OR of USB_5V, VIN_5V, VBAT; BQ21040 charger on VBAT only, up to 540 mA (not used in this design).
- I2C: P1.26 (SDA) / P1.28 (SCL) and a Qwiic connector; 3.3 V logic. Fuel gauge goes here.
- Measured power (Bela datasheet Table 6, 5 V via USB-C): PB2 + Gem Stereo = **1.21 W idle, 1.32 W audio + 1 core, 2.15 W audio + 4 cores.**

---

## A) Analog Hall sensors for the valve caps

### A.1 Candidates, checked against datasheets

| Part | 3.3 V OK? | Sensitivity @3.3 V | Linear range @3.3 V | Verdict |
|---|---|---|---|---|
| **TI DRV5055 A1** | yes, spec 3.0-3.63 V | 60 mV/mT (57-63) | +/-22 mT | good, needs gap >= 8 mm with 6x3 magnet |
| **TI DRV5055 A2** | yes | 30 mV/mT (28.5-31.5) | +/-44 mT | **recommended** |
| TI DRV5055 A3 / A4 | yes | 15 / 7.5 mV/mT | +/-88 / +/-176 mT | too weak for a 20 mm throw |
| TI DRV5055 A8 | yes | 40 mV/mT | +/-39 mT | fine alternative |
| Honeywell SS49E | spec says 2.7-6.5 V (RS listing); typ 1.4 mV/G = 14 mV/mT at 5 V, ~9 mV/mT at 3.3 V (ratiometric, my scaling) | ~9 mV/mT | wide | works, but 3x less sensitive and noisier. Cheapest backup. |
| Allegro A1324 | **no**, 4.5-5.5 V supply (DigiKey listing) | n/a | n/a | rejected: would need 5 V and would clip the ADC. A1325/A1326 are same family, not verified at 3.3 V. |
| AH49E | not verified | | | skip |

DRV5055 datasheet facts (https://www.ti.com/lit/ds/symlink/drv5055.pdf, read directly): output = VCC/2 (1.65 V) at 0 mT; usable output 0.2 V to VCC-0.2 V (so 0.2-3.1 V at 3.3 V); ICC 2 mA typ, 4 mA max at 3.3 V; 20 kHz bandwidth; noise 0.2 mT p-p at 3.3 V (unfiltered); linearity error +/-1 %; ratiometric. North and south poles give different voltages (bipolar).

### A.2 Recommendation: DRV5055A2, TO-92, + N52 disc 6 mm x 3 mm

Why A2: ±44 mT linear range survives print tolerance in the gap without saturating; 30 mV/mT gives ~1.15 V of swing over the stroke, all inside the ADC range.

Magnet: **N52 axial disc, 6 mm dia x 3 mm thick**, one per cap, north face toward sensor, all three the same way. Field computed with the on-axis cylinder formula, Br = 1.45 T (my calculation, not a vendor number; verify with a gauss meter):

| Valve travel (mm) | Gap (mm) | B (mT) | DRV5055A2 out (V) | ADC counts (16 bit) |
|---|---|---|---|---|
| 0 (valve up, closest) | 6 | 39.3 | 2.83 | 45,280 |
| 2 | 8 | 20.6 | 2.27 | 36,294 |
| 4 | 10 | 12.0 | 2.01 | 32,164 |
| 8 | 14 | 5.1 | 1.80 | 28,828 |
| 12 | 18 | 2.6 | 1.73 | 27,636 |
| 18 | 24 | 1.2 | 1.69 | 26,958 |
| 20 | 26 | 0.9 | 1.68 | 26,846 |

Design rules that follow:
- Set the closest gap to 6 mm (A2). Closer than ~5 mm exceeds 44 mT and leaves the linear range. Use the A1 instead with gap >= 8 mm if you want more sensitivity at the far end (A1 6x3 @ 8 mm: 20.6 mT -> 2.89 V down to 1.71 V at 26 mm).
- Field falls ~1/z^3, so the response is steeply nonlinear: ~45% of the voltage swing happens in the first 2 mm. Linearize in software with a per-valve 16-point lookup table (calibrate by pressing each valve through its range). z is proportional to B^(-1/3) as a first guess.
- Far-end resolution is the weak spot: at 1.2 mT the signal is ~6x the 0.2 mT p-p unfiltered noise. Add an RC low-pass at each sensor output (e.g. 1 kohm + 100 nF, 1.6 kHz corner; valve motion is far slower) and oversample on the Bela (250 kHz ADC).
- Alternative that fixes the 1/z^3 curve: slide the magnet *past* the sensor (sensor sees the perpendicular field component, which crosses through zero), giving a bipolar S-curve centered on 1.65 V over several mm and more linear mid-travel. More mechanical work, so treat as a v2.1 experiment.
- Cross-talk: valves sit ~20-25 mm apart; a 39 mT magnet at 6 mm falls to <2 mT at the neighbor. Alternate magnet polarity between adjacent valves or just include the neighbor terms in calibration. Brass is non-magnetic.
- **Stroke: I could not verify "18-20 mm".** Search found no published piston travel; measure each valve with calipers before fixing the sensor mounts. The table covers 0-20 mm.

### A.3 Where to buy (links)

**Sensors (qty: 3 needed, order 6-10, A2 + a few A1 to tune):**
- DigiKey DRV5055A2QLPG (TO-92-3 through-hole, industrial grade): https://www.digikey.es/en/products/detail/texas-instruments/DRV5055A2QLPG/9371281 (CONFIRMED; change the domain to digikey.com)
- DigiKey DRV5055A1QLPG: https://www.digikey.com.br/en/products/detail/texas-instruments/DRV5055A1QLPG/9371280 (CONFIRMED)
- DigiKey DRV5055A1ELPGMQ1 (automotive grade, TO-92 short body): https://www.digikey.com/en/products/detail/texas-instruments/DRV5055A1ELPGMQ1/11205796 (CONFIRMED); A2 equivalent https://www.digikey.ca/en/products/detail/texas-instruments/DRV5055A2ELPGMQ1/10454019 (CONFIRMED)
- Mouser SKUs 595-DRV5055A2QLPG and 595-DRV5055A1QLPG (from an aggregator listing; I did not open a Mouser page: UNCONFIRMED URL)
- TI product page: https://www.ti.com/product/drv5055 (CONFIRMED)
- SS49E backups: LCSC https://www.lcsc.com/product-detail/C85697.html ; RS (5-pack) https://uk.rs-online.com/web/p/motion-sensor-ics/2362760 ; Tayda https://taydaelectronics.com/ss49e-linear-hall-effect-sensor-ic.html (out of stock when seen). All CONFIRMED pages; no Amazon listing found.
- Amazon search (not a product link): https://www.amazon.com/s?k=DRV5055+hall+sensor . Generic "49E module" boards on Amazon usually add an LM393 and run 5 V; check each is 3.3 V and exposes the analog pin. UNCONFIRMED.
- There is **no Adafruit/SparkFun breakout for the DRV5055 or SS49E** (search found none). TI sells an EVM: https://www.ti.com/tool/drv5055evm (CONFIRMED page). For a dead-bug build, the bare TO-92 with 3 wires + 100 nF at the sensor is enough.

**Magnets (N52 6x3 mm axial, want ~10 + spares; also grab 8x3 and 10x3 to try):**
- K&J Magnetics (US, sells true N52 and publishes pull force): https://kjmagnetics.com/dc6-n52-neodymium-disc-magnet . Note this page is the DC6 N52 disc; check its dimensions against 6x3 before ordering (UNCONFIRMED match to 6x3). 
- JC Magnetics 6x3 N52 (**diametric**, wrong magnetization, avoid): https://www.jc-magnetics.com/6mm-x-3mm-n52-diametric-neodymium-magnet ; axial version: https://www.jc-magnetics.com/3mmx6mm-N52-Neodymium-magnet-cylinders (CONFIRMED page; check axial)
- Apex Magnets 6x3 is N48, not N52: https://www.apexmagnets.com/magnets/6mm-x-3mm-disc-neodymium-magnets
- Amazon search: https://www.amazon.com/s?k=N52+neodymium+disc+magnet+6mm+x+3mm (search only). Amazon "N52" listings are often unlabeled grade; the field table above assumes 1.45 T.
- Magnets are axially magnetized in most listings; "diametric" ones have the poles on the curved sides. You want **axial**.

### A.4 Alternative: 3-axis I2C magnetometer

| | Adafruit TMAG5273 A2 | Adafruit MLX90393 | Adafruit TLV493D |
|---|---|---|---|
| Range | +/-133 / 266 mT | +/-5 to +/-50 mT | ~+/-130 mT class, 12 bit |
| Bits | 12 | 16 | 12 |
| Link | https://www.adafruit.com/product/6490 | https://learn.adafruit.com/mlx90393-wide-range-3-axis-magnetometer (product 4022) | https://learn.adafruit.com/adafruit-tlv493-triple-axis-magnetometer (product 4366) |

(TMAG5273 A1 is +/-40/80 mT; A2 is the product 6490 page, $5.95; CONFIRMED.) The TLV493D range figure is from my memory of the Infineon part, not verified here.

Pros: frees all 8 Bela analog channels (use the 3 only for valves, others for pressure/etc.); gives 3 axes, so a side-pass magnet gives position + tilt; digital, immune to ADC reference/ground noise; STEMMA QT cabling.
Cons: **all three share one fixed I2C address** (TMAG5273 has address variants, check per product; TLV493D and MLX90393 need address pins/jumpers or a mux like the TCA9548A), so 3 valves need address selection or a mux; I2C polling latency (hundreds of us to ms, vs ADC at ~30 us/sample); wire length on I2C is fragile on a vibrating instrument; the analog DRV5055 needs no firmware, no bus, 20 kHz bandwidth, 10 us propagation delay. For a performance instrument where valve depth drives audio, analog is the lower-latency, lower-risk choice. Magnetometer is the fallback if analog noise on the Bela proves bad.

---

## B) Power budget and runtime

Goal: >= 4 h continuous. Method: Wh = P x 4 h / 0.85 (converter) x 1.3 (margin).

### B.1 Load estimate (5 V-rail equivalent, W)

| Item | Typical | Worst | Basis |
|---|---|---|---|
| PB2 + Gem Stereo | 1.5 | 2.15 | Bela datasheet Table 6: 1.32 W (1 core) / 2.15 W (4 core) |
| 15 NeoPixels, brightness 60/255, purple animation | 0.5 | 1.06 | 60 mA/px full white x 0.235 = 14 mA/px worst (all channels); purple is R+B only, animated duty ~50 %. WS2812-class, my estimate |
| NeoTrellis 4x4 (16 px + SAMD09) | 0.5 | 1.13 | same method; assumes brightness also ~60/255 |
| BME680, VL53L0X, MPU6500, 3x DRV5055 | 0.15 | 0.2 | ~5 + 20 + 3 + 3x2 mA at 3.3 V, from the 3.3 V rail; datasheet figures from memory except DRV5055 (read: 2 mA typ) |
| Stereo wireless TX | 1.0 | 1.0 | **no part specified; 1 W is an assumption.** If it runs from its own battery, subtract it. |
| Mic preamp | 0.15 | 0.15 | assumption, small op-amp/discrete preamp, no phantom power |
| **Total** | **3.8 W** | **5.7 W** | |

If the TX is self-powered: 2.8 W typical / 4.7 W worst.

### B.2 Battery size

Typical 3.8 W: 3.8 x 4 / 0.85 x 1.3 = **23.2 Wh**. Worst 5.7 W: **34.9 Wh**.

Cell energy = rated mAh x 3.6 V nominal (so "runtime" below is nominal-voltage; real cutoff at 3.0-3.2 V costs ~5 % but the 30 % margin covers that).

| Cell | Wh | Typical (3.8 W): cells for 23.2 Wh | runtime, with 30 % margin | Worst (5.7 W): cells for 34.9 Wh | runtime, with 30 % margin |
|---|---|---|---|---|---|
| 18650 3000 mAh (Samsung 30Q, Sony VTC6) | 10.8 | 3 | 5.6 h | 4 | 5.0 h |
| 18650 3500 mAh (class of Sanyo/Panasonic GA) | 12.6 | 2 | 4.3 h | 3 | 4.3 h |
| 21700 4000 mAh (Samsung 40T) | 14.4 | 2 | 5.0 h | 3 | 5.0 h |
| 21700 4200 mAh (Molicel P42A) | 15.1 | 2 | 5.2 h | 3 | 5.2 h |
| 21700 4500 mAh (Molicel P45B) | 16.2 | 2 | 5.6 h | 3 | 5.6 h |
| 21700 5000 mAh (Samsung 50E; maker says 4900 min) | 18.0 | 2 | 6.2 h | 2 | 4.1 h |

Without the 30 % margin, 2x 50E give 5.4 h even at the worst case (my calc).

### B.3 Cell picks and where to get them

Pick for this build: **2x 21700 Molicel P45B (1S2P, 32.4 Wh nominal)** if a pack of two fits; otherwise **3x 18650 Samsung 30Q (1S3P, 32.4 Wh)** since 18650s are shorter and more holders exist. Either gives 5.6 h typical / ~4.3-5 h worst with the full margin. Add P42A/50E as acceptable substitutes. Discharge current is trivial (worst ~5.7 W / 0.85 / 3.6 V = 1.9 A total, <1 A per cell in 1S2P). 30Q is rated 15 A, P45B 45 A.

Data and product pages (18650 Battery Store, which lists datasheets):
- Samsung 30Q: https://www.18650batterystore.com/products/samsung-30q (CONFIRMED); datasheet https://cdn.shopify.com/s/files/1/0481/9678/0183/files/samsung_30q_data_sheet.pdf
- Samsung 40T: https://www.18650batterystore.com/products/samsung-40t
- Samsung 50E: https://www.18650batterystore.com/products/samsung-50e
- Molicel P42A: https://www.18650batterystore.com/products/molicel-p42a (CONFIRMED, listed $5.99 sale)
- Molicel P45B: https://www.18650batterystore.com/products/molicel-p45b (was SOLD OUT when I looked; CONFIRMED page)
- Sony/Murata VTC6: https://www.18650batterystore.com/products/sony-vtc6
(all product-page URLs derived from the store's data-sheets page; 30Q, P42A pages confirmed directly.)

**Authenticity warnings.**
- Counterfeit and rewrapped cells are common on Amazon, eBay, AliExpress and Walmart marketplace listings, especially Samsung 30Q/50E and Sony VTC6 (rewrapped pulls or low-capacity cells). Do not buy cells there, including from "ships from Amazon" third-party listings. Never buy "9800 mAh" or other impossible capacities.
- 18650batterystore.com states it is the official site, operated by "Epoch Batteries LLC", and warns about lookalike sites (https://www.18650batterystore.com/pages/beware-of-imitation-websites). One search result called the operator EDG Sales International LLC (Stockbridge, GA): conflicting. **Neither source states it is an authorized Samsung/Molicel dealer.** I found no authorization statement for it or for IMRbatteries (https://www.imrbatteries.com/about-us/); Trustpilot reviews for IMR are mixed. Both are widely used reputable resellers, but "authorized" is unverified. To be sure, check Samsung SDI's and Molicel's own dealer lists, or buy from DigiKey/Mouser (e.g. Samsung INR21700-50E is carried by authorized distributors). Typed URL check before paying.
- Buy unwrapped/flat-top cells and wrap them yourself or use holders; avoid pre-built "protected" cells (button-top with PCM) unless the holder is sized for them (longer, Keystone 1043P class).
- Verify new cells: weigh them (a real 21700 50E ~70 g, 30Q ~46 g) and check capacity once on a charger that reports mAh.

---

## C) Holder, 1S-parallel vs 2S, and charging

### C.1 1S-parallel vs 2S: choose 1S-parallel

- Bela Gem Stereo needs 5 V, not ~7.4 V, so a 2S pack still needs a converter; 1S needs a boost. Nothing in the Bela stack takes 2S directly.
- Chargers that matched your spec (PowerBoost 1000C, bq25185 boards, SparkFun PRT-14411) are single-cell only; 2S would need a different class of charger/balancer.
- A single MAX17048 fuel gauge is a 1S device (Adafruit breakout described for a 1S 3.7-4.2 V cell).
- 1S-parallel means no balance wiring and the cells self-equalize; match cells (same model, same lot, same state of charge before paralleling, within 0.1 V).
- Cost of 1S: higher battery-side current at the boost, up to ~1.9 A worst. The PowerBoost 1000C's switch is 2 A (~2.5 A peak limiting), so worst case is tight (see C.3).

### C.2 Holder (snap-in, with contacts/springs)

- **Keystone 1043** (18650, single cell, PC-pin through-hole, DigiKey 36-1043-ND, ~$2.90): https://www.digikey.com/en/products/detail/keystone-electronics/1043/2745669 (CONFIRMED). 1043P is the polarized/longer-cell variant, in stock at Mouser/Farnell (https://uk.farnell.com/keystone/1043p/battery-holder-18650-th/dp/2673609, CONFIRMED).
- **Keystone 1042** (18650, single cell, SMD tab): https://www.digikey.co.za/en/products/detail/keystone-electronics/1042/2745668 (CONFIRMED)
- For 1S3P of 18650, use three 1043 holders in a printed rack with the PC pins soldered to a perf board/bus bar. Keystone's 21700 holder part number: **not verified**; search DigiKey for "21700 battery holder Keystone" (UNCONFIRMED).
- Multi-cell snap-in spring holders (1S2P/1S3P/2S) on Amazon: could not confirm any listing. Search URLs only: https://www.amazon.com/s?k=3x+18650+battery+holder+parallel and https://www.amazon.com/s?k=21700+battery+holder+spring+contact . Look for holders with thick leads (>= 20 AWG), nickel-plated steel contacts (Keystone-style), and avoid cheap thin wires that drop >50 mV at 2 A.
- For a trumpet (vibration), consider a retention clip or a 3D-printed lid with a rubber pad; Keystone's open holders will let a cell jump out.

### C.3 Charger / power path

Requirements: USB-C input; charge 1S cells; power the 5 V load while charging; output clean 5 V into P1.01; no brown-out of Bela.

| Option | USB-C | Charge while powering | Out | Notes | Link |
|---|---|---|---|---|---|
| **Adafruit PowerBoost 1000C (2465)** | no, micro-USB | **yes** (MCP73871 load sharing) | 5.2 V, 1 A+ (2 A switch, ~2.5 A peak) | 1000 mA charge (chip max 1.8 A), 90 %+ efficient, LBO <3.2 V, EN pin to GND = off (20 uA), $19.95. Bela's own docs recommend a booster of this style. | https://www.adafruit.com/product/2465 (CONFIRMED) |
| Adafruit bq25185 USB/DC/Solar charger with 5 V boost (ADA6106) | **yes** | yes (power path) | 1 A (TPS61023) | 5 V from a boost, enable pad, **6 h charge safety timeout that cannot be disabled** (charging stops, then restart by re-plug). | https://learn.adafruit.com/adafruit-bq25185-usb-dc-solar-charger-with-5v-boost-board/overview (CONFIRMED); https://thepihut.com/products/adafruit-bq25185-usb-dc-solar-charger-with-5v-boost-board |
| SparkFun LiPo Charger/Booster 5V/1A (PRT-14411) | no, micro-USB (500 mA) | partial | 5 V 1 A | MCP73831 + PAM2401, **no battery protection**, 600 mA onboard switch. | https://www.sparkfun.com/sparkfun-lipo-charger-booster-5v-1a.html (CONFIRMED) |
| DFRobot Solar Power Manager 5V (DFR0559) | no | **not confirmed** | 5 V 1 A | CN3165-based, 900 mA charge, 73-86 % efficient; has reverse/overdischarge protection. I could not confirm simultaneous charge-and-power. | https://wiki.dfrobot.com/dfr0559/ (CONFIRMED page, power-path claim UNCONFIRMED) |
| TI BQ25895 (breakout) | USB-C possible | yes (power path) | boost to 3.1 A | Best headroom (3.25 A input limit, 3.1 A boost) but I found no ready breakout to link; TI EVM BQ25895EVM-664 only. | https://www.ti.com/product/BQ25895 (CONFIRMED) |
| IP5306 / IP5328 power-bank boards | usually | usually | ~2 A | **Could not verify any board or its behavior**. Cheap power-bank ICs often shut off at low load (<50-100 mA): the Gem idles above that, but check. Not recommended without a bench test. | UNCONFIRMED |
| Pololu U3V50F5 / S7V8F5 | no | no | boost only | plain regulators, no charger; Pololu says ripple is uncharacterized. U3V50F5 passes input to output when disabled. | https://www.pololu.com/product/2565 ; https://www.pololu.com/product/2123 (CONFIRMED) |

**Recommended: Adafruit PowerBoost 1000C**, with the 1S pack on its JST BAT port. It is the only board in the list where I confirmed charge-while-run, an enable pin and low-battery flag, and 5 V. Gap: micro-USB input (use a USB-C to micro-B cable/adapter, or swap to the bq25185+boost board if you want native USB-C). The 1 A+ rating is the hard limit: 5.2 V x 1 A = 5.2 W. **Typical load (3.8 W = 0.73 A) fits; the 5.7 W worst case (1.1 A) does not.** Fix: run the wireless TX from its own battery (worst case 4.7 W = 0.9 A) or add a second regulator, or fall back to a BQ25895 design. Do not run a PowerBoost at its limit on the Bela rail: a brownout below 4.5 V makes the analog inputs misbehave.

Charge current: set 1000 mA for the 1S3P pack (0.33C) or 500 mA for 1S2P. While the rig is running, charge current is reduced by the load (load sharing); a 4 h session needs ~3 h on USB to refill.

### C.4 Protection

- **Per-pack PCM/BMS:** plain cells in holders have no protection, and PowerBoost has none (SparkFun PRT-14411 explicitly has none). Add a single-cell protection board (DW01A + dual FET type, overcharge 4.25 V / overdischarge ~2.5 V / overcurrent ~3 A) wired between pack and PowerBoost. Specific listing not confirmed; search "1S Li-ion protection module 3A DW01A" at Adafruit/SparkFun, e.g. check Adafruit "Protection Circuit Module". UNCONFIRMED.
- **Fuse:** 3 A resettable PTC or a fast 3 A fuse on the pack positive (pack output ~1.9 A worst). Plus a ~5 A series fuse/PTC for each parallel branch if cell-matching is imperfect.
- **Reverse polarity:** keyed holders plus a P-channel FET (or a Schottky if drop is acceptable) at the pack input; the PowerBoost's JST connector is polarized.
- **Power switch:** a slide switch from the PowerBoost EN pin to GND turns the 5 V output off (20 uA standby). A latching or tactile power button isn't needed; Bela also has a PWR BTN pin (P2.12) if you want a soft-off later.
- **Fuel gauge:** Adafruit MAX17048 breakout, product 5580, I2C 0x36, 3.3/5 V logic, JST jumper: https://www.digikey.bg/en/products/detail/adafruit-industries-llc/5580/16719287 (CONFIRMED) ; library page https://learn.adafruit.com/adafruit-max17048-lipoly-liion-fuel-gauge-and-battery-monitor (CONFIRMED). Connect to the Bela I2C (P1.26/P1.28 or Qwiic), battery pins on the pack positive/negative **before** the PowerBoost. It reports voltage and state of charge; wire PowerBoost LBO to a Bela digital input as a hard low-battery alarm.

### C.5 Keeping the audio quiet

Boost converter switching noise (hundreds of kHz-MHz fundamentals plus load-step ripple) appears on VSYS, then on the codec/analog supply and the 3.3 V rail the Halls use. The Hall sensor is ratiometric to 3.3 V, so rail noise becomes sensor noise.

Mitigations in order:
1. **Measure first** with a scope on P1.24 (5 V) and P1.14 (3.3 V), AC coupled, 20 MHz BW limit, with audio running and LEDs animating. Pololu notes ripple depends on load/wiring and must be measured in the system; I did not find vendor ripple specs for the PowerBoost either.
2. **Pi filter at the Bela feed:** ferrite bead (~600 ohm @100 MHz, 2 A+) in series, 47-100 uF low-ESR ceramic/polymer bulk + 10 uF + 100 nF on each side, placed at the Bela end, not the boost end. Alternatively an LC: 2.2-4.7 uH (2+ A rated) + 100 uF. 
3. **LDO post-regulation:** because the PowerBoost output is 5.2 V and Bela's floor is 4.5 V, a low-dropout post-regulator at ~4.9-5.0 V is feasible (dropout must be <=200 mV at 2 A). Use a low-noise, high-PSRR LDO class (TI TPS7A-series or Analog Devices LT3042/LT3045-class). Part numbers here are from memory, not checked. Dropout and the 2 A requirement are the constraints; check before choosing.
4. **Layout / wiring:** star ground at the Bela GND pin; boost board far from the audio jacks and Hall wiring; twisted pair for 5 V/GND; 100 nF + 1 uF at each Hall right at the sensor; keep NeoPixel (800 kHz, high-di/dt) supply wires separate from the Halls and mic preamp, with 470 uF at the strip start plus a 330-470 ohm series resistor on the data line; run the LED power from the PowerBoost directly (not through the Bela).
5. **Separate the load of NeoPixels/TX** from the audio rail if hiss persists: a second small boost for LEDs only.
6. **Fallback test:** run the rig from a USB-C power bank first to learn how much of any hum/whine is the boost.

---

## D) Wiring block diagram

```
USB-C wall/bank
      |
      v   (micro-B adapter)
 +-----------------+      +-------------------+
 | PowerBoost 1000C |<-----| pack: 1S2P 21700  |<-- [PCM] <-- [3 A fuse]
 | MCP73871 + TPS61090|    | or 1S3P 18650     |        |
 | load sharing     |      +-------------------+     MAX17048 (I2C 0x36)
 | EN <- slide sw   |                                   |
 | LBO -> Bela D#   |                                   |
 +---------+--------+                                   |
           | 5.2 V                                      |
           v                                            |
   [ferrite bead + LC/pi filter]--->[LDO 4.9-5.0 V]     |
           |                                            |
           +--------> Bela P1.01 (VIN_5V), GND          |
                      Bela Gem Stereo + PB2             |
                      |  3.3 V (P1.14) ---+-> 3x DRV5055 (A0-A2, 1k+100nF RC each)
                      |                   +-> BME680, VL53L0X, MPU6500, NeoTrellis (I2C, P1.26/28)
                      |  I2C (Qwiic) ----> MAX17048
                      |  digital D# ---> NeoPixel 15 px data (level-shift/330 ohm)
                      |  LINE IN  <------ mic preamp
                      |  LINE OUT/HP ---> stereo wireless TX
           5.2 V rail also feeds NeoPixels + preamp + TX (if on same pack), each with local bulk caps
```

NeoPixels run at 5 V data; the Bela GPIO is 3.3 V, so use a level shifter (74AHCT125) or put the first pixel's supply at ~4.8 V (not verified for your strip).

---

## Source list

- Bela Gem Stereo/Multi datasheet v1.0: https://bela.io/datasheets/bela_gem_datasheet.pdf
- Bela powering page: https://learn.bela.io/using-bela/bela-techniques/powering-bela
- TI DRV5055: https://www.ti.com/product/drv5055 ; datasheet https://www.ti.com/lit/ds/symlink/drv5055.pdf
- Honeywell SS49E: RS listing https://uk.rs-online.com/web/p/motion-sensor-ics/2362760 ; datasheet mirror https://www.mikrocontroller.net/attachment/423738/SS49E_Hall_Sens_Datasheet.PDF (spec figures quoted via search summary, not read directly)
- Allegro A1324 at DigiKey: https://www.digikey.com.mx/en/products/detail/allegro-microsystems/A1324LUA-T/2728144
- Adafruit PowerBoost 1000C: https://www.adafruit.com/product/2465
- Adafruit bq25185 + 5 V boost: https://learn.adafruit.com/adafruit-bq25185-usb-dc-solar-charger-with-5v-boost-board/overview
- SparkFun PRT-14411 hookup guide: https://learn.sparkfun.com/tutorials/sparkfun-5v1a-lipo-chargerbooster-hookup-guide
- DFRobot DFR0559: https://wiki.dfrobot.com/dfr0559/
- TI BQ25895: https://www.ti.com/product/BQ25895
- Adafruit MAX17048 (5580): https://learn.adafruit.com/adafruit-max17048-lipoly-liion-fuel-gauge-and-battery-monitor
- Adafruit TMAG5273: https://www.adafruit.com/product/6490 ; MLX90393: https://learn.adafruit.com/mlx90393-wide-range-3-axis-magnetometer ; TLV493D: https://learn.adafruit.com/adafruit-tlv493-triple-axis-magnetometer
- Keystone 1042 / 1043 at DigiKey (links above)
- 18650 Battery Store: https://www.18650batterystore.com/pages/data-sheets
- Pololu U3V50F5 / S7V8F5 (links above)

## Open items for Emory

1. Measure real valve stroke with calipers (18-20 mm is unverified).
2. Name the stereo wireless TX; its draw (1 W assumed) decides whether one PowerBoost is enough.
3. Verify a real Amazon source for magnets/holders yourself; I confirmed none.
4. Scope the 5 V rail with audio + LEDs before adding the LDO.
