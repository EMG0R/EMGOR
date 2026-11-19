# HYPERTRUMPET V2: power design (board's own USB-C, no panel breakout)

> **2026-11-12:** written for the Bela Gem Stereo on 1S2P (3.8/5.7 W loads, ideal-diode OR, bq24074 USB charging idea). Trumpet is now a Daisy Seed 3 on 1 cell; current runtime and chain are in `../../SHARED_POWER_KIT.md`. Kept for the reasoning (boost choice, protection, reverse polarity).


Date 2026-11-11. Link collection only; nothing purchased, nothing built or measured.
Revised 2026-11-12: no panel USB-C breakout (see sec. 2-3 and 11). Supersedes the power sections of `PARTS_POWER_SENSORS.md` (sec. B, C) and the "VBAT into P2.14" line in `GROUNDING_PLAN.md` sec. 4.
Tags: **[V]** read directly from a primary source (cited), **[I]** inferred from a primary source, **[U]** unverified, check on the bench or with the vendor.

**Fixed by Emory (2026-11-11):** the Gem does not run reliably from a raw 1S cell. It gets a clean 5 V boost. The VBAT-direct path is not designed here. The chain is charger power path -> VSYS -> low-noise 5 V boost (at least 2 A at 5 V with the cell at 3.0 V) -> Bela. A separate LDO feeds the wireless TX. The LED supply stays.

**Design change (Emory, 2026-11-12):** no panel USB-C breakout. The board's own USB port (PB2 J14 on the Gem Stereo, or the Daisy Seed 3's USB) is exposed through the housing. USB powers the board only (the Gem's internal ideal-diode power-OR combines it with the boost feeding VIN); LEDs, sensors and the TX stay on the battery boost. Removed: Adafruit 6153 panel USB-C, plug breakout/pigtail, 5.1 k CC resistors, the Adafruit LM66200 external OR, and the USB VBUS sense via 100 ohm. Chain: cell, protection board, switch, Pololu U3V40F5 5 V boost, then Gem VIN plus LED/TX/sensor rails. The charger-board sections (4.2-4.3, 5, 6: bq24074, U3V70F5, fuse, reverse-polarity FET) are earlier research, superseded by `../../SHARED_POWER_KIT.md` (external charging only, no charger board, no fuse, U3V40F5).
**Ferrite rule (Emory, 2026-11-12):** no ferrite or inductor in any ground or return path; ferrites allowed in series on supply rails (boost output to LEDs/TX/analog LDO input).

**Amazon:** `amazon.com/s?k=...` returned HTTP 503 again via WebFetch (2026-11-11), and search never surfaced a `/dp/` page. No Amazon ASIN is listed anywhere in this doc.

## 0. Sources

| Tag | Source |
|---|---|
| DS | Bela Gem Stereo/Multi datasheet v1.0, 2026-08-04, https://bela.io/datasheets/bela_gem_datasheet.pdf (read as text: Tables 3, 4, 6, sec. 2.4) |
| SCH | Bela Gem Stereo rev A4 KiCad source, https://github.com/BelaPlatform/bela-hardware/tree/master/GemStereo/GemStereo_rev_A4 (`.kicad_pcb` netlist read directly, `pwr_management`, `usb_host` sheets) |
| PB2 | PocketBeagle 2 schematic PDF v1.0, https://github.com/beagleboard/pocketbeagle-2/blob/main/pocketbeagle2_sch.pdf (page 5 power path, page 14 USB-C). Rendered and read directly. |
| PB2D | PocketBeagle 2 design doc, https://docs.beagleboard.io/boards/pocketbeagle-2/03-design-and-specifications.html (no USB/VBUS text, no thresholds) |
| BELA | Bela powering page, https://learn.bela.io/using-bela/bela-techniques/powering-bela |
| TIUSB | TI AM62x USB2.0 hardware FAQ/guidelines, https://www.ti.com/document-viewer/lit/html/SPRAD05E/GUID-57E10AA3-00B2-4126-A212-D2EE76A0BE14 (the numeric divider section lives in the device datasheet and was not retrieved) |
| BQ | TI bq24074 datasheet, https://www.ti.com/lit/ds/symlink/bq24074.pdf (read as text) |

## 1. Gem power inputs (answer to Q1)

### 1.1 What each input is

The Gem Stereo has no power path of its own. The PocketBeagle 2 (PB2) plugs into it; all three sources are PB2 rails.

| Input | Pin | Net on PB2 / Gem | Feeds | Valid window |
|---|---|---|---|---|
| USB-C VBUS | PB2 J14 VBUS; also Gem P1.05 and P1.07 | `USB_5V` (Gem net `/USB_VBUS`) | (a) LM73100 ideal diode -> VSYS; (b) the USB0_VBUS sense divider; (c) BQ21040 charger input; (d) on Gem rev A4 and older the USB-A host VBUS directly | 3.8 V to 5.5 V. UV rising 3.8 / falling 3.5; OV rising 5.5 / falling 5.05 [V, PB2 p.5] |
| VIN | P1.01 | `VIN_5V`; **unconnected on the Gem PCB** (A4 netlist: P1.1 `unconnected`), so it is a pure PB2 pin | LM73100 -> VSYS | 3.8 V to 5.5 V, same thresholds [V, DS sec. 2.4; PB2 p.5] |
| VBAT | P2.14 | Gem net `/BAT.VIN`, goes only to the pin (Stereo has no battery connector) | LM73100 -> VSYS, and the BQ21040 charger output | 3.2 V to 4.6 V. UV rising 3.2 / falling 2.9; OV rising 4.6 / falling 4.2 [V]. "Turned off if either VIN_5V or USB_5V is present" [V, PB2 p.5 note] |
| VSYS | P1.24, P2.13 ("5V") | output of the three-way OR | everything on the PB2 and the Gem: PMIC TPS6521903, the Gem's `+5V`, `+5VA` (via ferrite FB2), `+3V3`, 3.3VA/1.8V LDOs (MIC5317), USB-A host (rev A5+) | do not drive directly. External source only through a diode [V, DS sec. 2.4] |

Power OR-ing: yes. Three LM73100 ideal diodes with the thresholds above; BQ21040 single-cell charger on VBAT, `ISET = 540 mA` (R39/R41 network on PB2 p.5) [V]. DS says "selects the power source with the highest voltage"; the Beagle design doc says VIN_5V has priority. The two statements disagree [V, both]. Design so it does not matter: keep VIN_5V above USB_5V.

### 1.2 Resolving the "3.2-4.6 V" versus "4.5 V" conflict

Both statements are true; they describe different circuits.

- "VBAT 3.2-4.6 V" is the validity window of the PB2 power path (the VBAT ideal diode). It says the SoC board will boot from a 1S cell. [V, DS, PB2]
- "Below 4.5 V the analog inputs and outputs misbehave and the USB host stops" is a Gem-level statement. [V, BELA: "Powering any Bela board from a battery with less than 4.5V will cause the analog inputs and outputs to misbehave. The USB Host also stops working below this level."] The Gem derives its analog and codec rails from VSYS (`+5VA` through a ferrite, 3.3VA and 1.8V through MIC5317 LDOs, [I from SCH pwr_management; I did not trace every pin]) and the ADS816x uses its internal 4.096 V reference, which needs a high enough supply [I]. A cell at 3.7 V gives VSYS of about 3.6 V.
- Bela's own workaround exists: bridge solder jumper JP2 (Stereo) to select the 3.3 V reference and change two ADS8166 registers (`PD_REF` = 1, `OFST_CAL` = 011b) [V, BELA]. That costs the 4.096 V range, which the Hall and slider chain is scaled for. Not used here.

**Decision: feed 5.0-5.2 V into VIN_5V (P1.01), ground to a GND pin. Do not use VBAT. Do not connect anything to P2.14.** This matches the Bela recommendation (booster output to P1.01, not VBAT) [V, BELA] and Emory's constraint.

### 1.3 Can VIN and USB VBUS be present together?

Yes. The ideal diodes block reverse current, so the sources cannot short each other [V, DS sec. 2.4: "mutually exclusively"]. Two caveats for this design:

1. If USB_5V is the winning source, the Bela's whole load (1.2-2.2 W) comes from the host, which can overload a laptop port (500-900 mA). Keep VIN_5V at 5.1-5.2 V (boost output) so it wins over a 5.0 V host under both the "highest voltage" and the "VIN priority" readings [I]. This internal OR is the only USB/boost combiner; there is no external ideal-diode board.
2. BQ21040 charges VBAT from USB_5V. With P2.14 left floating it just idles at about 4.2 V on the pin. Harmless; do not wire the cell there. Its input draw comes from the host's USB_5V.

## 2. USB device enumeration (answer to Q2)

**Facts**
- The Bela's USB-C is the PB2's own connector J14, wired to **USB0** of the AM6232 [V, PB2 p.14]. The Gem's header USB pins (P1.09 DN, P1.11 DP, P1.13 ID) are **USB1**, which goes only to the Gem's USB-A host port [V, SCH J5; DS Table 3]. So the data lines cannot be soldered to a Gem header. The host cable must plug into J14 itself.
- `USB0_VBUS` (ball AC11) is fed by a divider from the J14 VBUS net: 20 k (R132) in series, 10 k (R94) to ground, so the pin sees VBUS/3, about 1.67 V at 5 V. No clamp zener on this port (USB1 has one, 1k/20k plus a 3.3 V zener) [V reading of the rendered schematic; the net that R132 taps is `USB_5V` by position, not by label: I].
- J14 CC1/CC2 already have 5.1 k pull-downs (R128, R129): the PB2 is a UFP/sink. [V]
- TI: scaled VBUS is "recommended when the USB interface is configured for device mode"; a permanent on-board supply must not drive it; the sense path is needed to tell the controller session valid/end [V, TIUSB and TI forum summaries]. I found no statement that device mode works with VBUS absent, and the Bela image's kernel behaviour with no VBUS is not documented [U].

**Conclusion:** assume VBUS must be present at J14 for the Bela to enumerate as a USB gadget. Bench-verify (sec. 8 item 1).

**How VBUS reaches the Bela (changed 2026-11-12): straight from the host, no panel breakout, no sense wiring.**

The board's own USB-C (PB2 J14) goes directly to the host. There is no panel connector, no plug breakout, no pigtail, no R_sense resistor and no switch pole B on the VBUS line. Host VBUS lands on `USB_5V`, which feeds the USB0_VBUS divider (so enumeration works) and the Gem's internal LM73100 ideal-diode power-OR (sec. 1.1). USB therefore powers the board only. LEDs, sensors, the TX regulator and the analog LDO stay on the battery boost rail, never on USB.

- The OR is internal: USB_5V, VIN_5V and VBAT are combined by the three LM73100 ideal diodes; reverse current is blocked [V, DS sec. 2.4, PB2 p.5]. No external ideal-diode board is used.
- Keep the boost at 5.1-5.2 V so VIN_5V wins over a 5.0 V host under both the "highest voltage" and "VIN priority" readings [I]. With the hard switch open, the board runs from USB alone (board only, no LEDs/sensors/TX), which is fine for bench work and does not charge the cells.
- A laptop host sees only the Bela's own draw (1.2-2.2 W worst case if USB won), not the LED/TX load.
- Rev check: on A4 and older the USB-A VBUS hangs on `USB_5V`; on A5+ USB-A is powered from VSYS [V, DS sec. 2.4]. Read the revision off the board [U]. Do not use the USB-A port in this build.

## 3. Board USB-C in the housing (answer to Q3)

**Part:** none to buy. The connector is J14 on the PocketBeagle 2 (Bela Gem Stereo build); on the Daisy Seed 3 build it is the Seed's onboard USB. CC1/CC2 already have 5.1 k pull-downs on the PB2 (R128, R129; sink/UFP) [V, PB2 p.14], so the host sees a normal sink. No CC resistors are added anywhere.

**Housing requirement (Emory, 2026-11-12):**
- Print a **strain-relief cradle** at the board's USB port that captures the board (or the plug body) so cable force is carried by the housing, never by the board's soldered connector.
- Cut an **opening sized for a right-angle or slim USB-C plug** (allow the overmold plus clearance for the cable bend). Verify against the assembled Gem + PB2 stack that J14 is reachable and the plug seats fully [U, check against the board].
- Use a short right-angle or slim-overmold USB-C cable; high-speed (480 Mbit/s, used by Bela USB audio) is the sensitive case, so avoid long unshielded adapters [I].

**Grounding:** the host cable's shield/GND reaches the Bela `GND` through J14. That is a second return path to STAR (alongside the supply leg D); see `GROUNDING_PLAN.md` sec. 4 and bench item 10. Keep LED/boost return currents off it.

**Wiring diagram**

```
 host --USB-C cable (right-angle/slim)--> [PB2 J14, strain-relief cradle in the housing]
                                              |  D+/D-, VBUS (USB_5V), GND
                                              v
                                   Gem internal ideal-diode OR (LM73100 x3) --> VSYS (board only)
                                              ^
 boost 5.1 V --> Gem VIN_5V (P1.01) ----------+
```

## 4. Charge manager and boost (answer to Q4)

### 4.1 Load and current budget

Load figures are from `PARTS_POWER_SENSORS.md` B.1: 3.8 W typical, 5.7 W worst at the 5 V rail. That table's wireless TX line (1.0 W) has no part behind it and does not say whether 1 W is at the TX or at the rail. A 3.8 V LDO from the 5.15 V rail passes the TX current unchanged, so 1 W at 3.8 V is 1.35 W at the rail. If the 1 W is at the TX terminals, add 0.35 W (4.15 W typical, 6.05 W worst); runtimes below scale by 0.92 [I].

Boost input current (88 % efficiency, Emory's figure):

| Rail load | Cell at 4.2 V | at 3.6 V | at 3.0 V |
|---|---|---|---|
| 3.8 W | 1.03 A | 1.20 A | 1.44 A |
| 5.7 W | 1.54 A | 1.80 A | 2.16 A |
| 10 W (the 2 A at 5 V requirement) | 2.7 A | 3.2 A | 3.8 A |

### 4.2 Candidates, as found

| Part | USB-C | Power path | Output / limit | Verdict | Link |
|---|---|---|---|---|---|
| **Adafruit Universal USB/DC/Solar charger, bq24074** (product 4755), $14.95 | **yes** | **yes**, battery supplements | OUT regulated to **4.4 V max**, follows the cell down to about 3 V. Board states 1.5 A load; chip allows 4.5 A on OUT [V, BQ Table: IOUT 4.5 A]. USB/DC input 5-10 V, 28 V protected. Charge 1.0 A default, jumper 0.5/1.5 A | **Pick** for the charger power path | https://www.adafruit.com/products/4755 |
| Adafruit bq25185 + 5 V boost (ADA6106) | yes | yes | boost 1 A | rejected: 5 V x 1 A too small; 6 h timer cannot be disabled | https://learn.adafruit.com/adafruit-bq25185-usb-dc-solar-charger-with-5v-boost-board/overview |
| Adafruit PowerBoost 1000C (2465) | no (micro-B) | yes | 5.2 V, about 1 A (TPS61090, 2 A switch) | rejected: 1 A | https://www.adafruit.com/product/2465 |
| BQ25895 / BQ25896 | possible | yes | boost mode 3.1 A | rejected: boost mode is OTG (VBUS becomes an output), cannot charge while powering from the same port; no breakout found (only an EVM and a third-party mini board, eicut) | https://www.ti.com/product/BQ25895 |
| MAX77751 | not found | | | no board found; not evaluated |  |
| SparkFun PRT-14411 | no | partial | 1 A | rejected | https://www.sparkfun.com/sparkfun-lipo-charger-booster-5v-1a.html |
| IP5306/IP5328/SW6106 bank boards | usually | usually | about 2 A | rejected: no board verified; low-current auto-off is the known trap (below), plus switching noise | none |

### 4.3 Recommended architecture

```
[superseded: bq24074 charger path; see banner] OUT(VSYS 3.0-4.4 V)--[main switch]--> [boost 5.1 V]--> pi filter --> Bela VIN_5V (P1.01)
                         |  |                                                          |
                    BAT <-+  +-- CHG/PGOOD to Bela GPIO                                +--> NeoPixels (own wire to STAR)
                    pack + PCM + fuse + rev-pol FET                                    +--> TX LDO 3.8 V (from the 5 V rail)
                    MAX17048 on I2C (Bela P1.26/P1.28)
```

- **Charger: Adafruit bq24074 USB-C (4755).** It powers the system first and charges the battery with whatever input is left. The battery supplements when the system load exceeds the input limit [V, BQ sec. 9.3.4.1.3]. OUT is the system voltage, so it feeds the boost directly.
- **Boost: Pololu U3V70F5, 5 V step-up, item 2891, $25.95.** Input 2.9-5 V, 40.6 x 15.2 x 4.6 mm. Pololu rates the input current at 7 A prolonged / 8 A for several seconds, and says max output current is about input current x efficiency x (VIN / 5 V). At 3.0 V in and 88 % that is about 3.7 A out [I, from the page's own formula; the output-current graph values were not in the retrieved text]. Spec says typical 80-95 % efficiency. **ENABLE below 0.4 V turns the load off completely** (true shutdown). No ripple spec and no switching frequency are given [V, page]; noise must be measured. https://www.pololu.com/product/2891
- **Alternates, each with a flaw:**
  - Pololu U3V50F5 (item 2565, $29.95, 5 A input typical, about 2.6 A out at 3.0 V [I]): **input passes through to the output when the board is disabled**, so a disabled boost would hand 3-4.4 V to the Bela rail. Only usable if the main switch cuts the boost input rather than ENABLE (as designed here), and even then the 5 V pass-through when VSYS is 4.4 V is below the Bela 4.5 V floor. https://www.pololu.com/product/2565
  - Pololu S13V30F5 (item 4082, $17.95, 5 V 3 A step-up/step-down): the 3 A rating is not stated for a 3.0 V input; unverified at the 2 A / 3.0 V point [U]. https://www.pololu.com/product/4082/specs
  - Pololu S18V20F5 (item 2574, $29.95): about 2 A typical only when VIN is near 5 V, SEPIC, 3 V minimum: fails the 2 A at 3.0 V test. https://www.pololu.com/product/2574
- **Noise.** Treat the boost as the noisiest part. The Gem filters only with a ferrite and its own LDOs. Add at the Bela end of the supply: 2.2-4.7 uH (3 A+) in series with +5 V, then 47-100 uF polymer/ceramic plus 10 uF plus 100 nF to ground, placed at P1.01. Per the ferrite rule (no ferrite or inductor in any ground/return path; ferrites allowed in series on supply rails), this series inductor or a ferrite bead in the +5 V lead is permitted; nothing goes in any ground. Measure first (scope AC-coupled at P1.24 and P1.14, audio running, LEDs animating) before adding it.
- **Auto-shutoff trap (power-bank ICs).** Power-bank chips shut off below roughly 50-100 mA of load [PARTS_POWER_SENSORS.md; not verified]. This chain avoids it: bq24074 has no load-detect shutdown, and the Gem idles near 240 mA at 5 V (1.21 W) anyway. Two other traps in the recommended chain:
  1. **Pass-through on the wrong boost.** See U3V50F5 above.
  2. **Charge timer vs a loaded charger.** bq24074 fast-charge timer slows when charge current is reduced by load (TI: timer clock scales with charge current) and stops charging when it expires [V, BQ sec. 9.3.x]. A 9 Ah pack at 1 A needs 9 h or more. Use the 1.5 A jumper and set TMR (18-72 k ohm; time = 480 s/kohm x R, so 72 k = 9.6 h) or disable it only after PCM and fuse are in place [BQ; formula from its "10 x 48 s/kohm" note; the default duration was not retrieved: U].
- **Host current limit.** The Adafruit board's default ILIM resistor sets about 1.6 A input (KILIM 1610 / R). A laptop USB-C port offers 0.5-0.9 A and its VBUS will trip or sag when the rig draws 1.5 A. TI's VIN-DPM folds the input current back when VBUS droops [V, BQ], but treat it as a protection, not a plan. Add a two-position input-limit switch: EN1 high / EN2 low = **500 mA** ("host") versus EN1 low / EN2 high = ILIM-resistor ("wall") [V, Adafruit guide table]. On a 500 mA input the cell supplies the difference, so a laptop session drains the pack slowly.
- **Battery connector, charger board:** it has its own JST battery port; the board has an optional 10 k NTC input. Use it with a thermistor in the cell holder if you want charge temp cutoff (unverified how the TS pad is exposed; the Adafruit guide describes it).

## 5. Cells and runtime (answer to Q5)

Runtime = N x Wh/cell x 0.90 usable x 0.88 boost / load. Wh/cell = rated mAh x 3.6 V nominal: P45B 4500 mAh = 16.2 Wh, 30Q 3000 mAh = 10.8 Wh (nominal vendor figures, not measured).

| Pack | Cells | Pack Wh | Usable at the rail | 3.8 W typical | 5.7 W worst | Est. cell mass |
|---|---|---|---|---|---|---|
| 2x Molicel P45B (21700), 1S2P | 2 | 32.4 | 25.7 Wh | **6.75 h** | **4.50 h** | about 140 g [U, about 70 g each] |
| 3x Molicel P45B (21700), 1S3P | 3 | 48.6 | 38.5 Wh | 10.1 h | 6.75 h | about 210 g [U] |
| 3x Samsung 30Q (18650), 1S3P | 3 | 32.4 | 25.7 Wh | 6.75 h | 4.50 h | about 138 g [U, about 46 g each] |
| 4x Samsung 30Q (18650), 1S4P | 4 | 43.2 | 34.2 Wh | 9.0 h | 6.0 h | about 184 g [U] |

At 1 W TX counted at the rail rather than the terminals (4.15 W / 6.05 W), multiply every runtime by 0.92. Per-cell current is below 1.2 A at the worst case with 2 cells at 3.0 V (2.16 A / 2); P45B is rated 45 A and 30Q 15 A, so neither is stressed. Runtimes assume the cell sits at nominal voltage on average; a 3.0 V cutoff costs a few percent, partly covered by the 90 % usable assumption.

**Pick: 2x Molicel INR21700-P45B, 1S2P.**
- Same 32.4 Wh as 3x 30Q in two cells instead of three, so one fewer holder, fewer parallel junctions, and 21700 gives a 4.5 h worst-case floor that already clears Emory's 4 h target.
- If the 4 h worst-case margin is not enough, the 3x P45B pack reaches 6.75 h worst case at about 70 g more. Mass in the trumpet is a hand-feel question for Emory.
- Samsung sources: several US resellers state that Samsung cells are not authorized for sale to individual consumers; the 30Q is also widely counterfeited and was sold in both protected button-top and unprotected flat-top forms under one model name [V, search summaries]. Molicel is no easier, see below.

**Vendor links (authorization is NOT verified for any of them).** I found no authorized-distributor list on molicel.com (it lists only E-One Moli Energy (Canada), Pitt Meadows BC, +1 604 466 6654, molicel@molienergy.com, and a contact page https://www.molicel.com/contact/ ) or for Samsung SDI. Email Molicel for its current US dealer list before ordering.
- Fogstar (UK) states it is an authorised Molicel distributor sourcing from the factory (self-claim): https://www.fogstar.co.uk/collections/all/capacity_4500mah
- One forum post says Liionwholesale was the only Molicel-listed authorized distributor (second-hand, date unknown, no URL verified).
- NKON (EU) lists the P45B (no authorization statement): https://www.nkon.nl/en/rechargeable/molicel-inr21700-p45b-4500mah-45a.html
- 18650 Battery Store (listing only; P45B was sold out and authorization is unstated): https://www.18650batterystore.com/products/molicel-p45b ; Samsung 30Q: https://www.18650batterystore.com/products/samsung-30q
- Molicel P45B datasheet mirror on IMR: https://www.imrbatteries.com/content/molicel_p45b.pdf
- Authenticity checks (carried over): buy flat-top unprotected cells; weigh and capacity-check each; match cells (same lot, within 0.1 V) before paralleling.

## 6. Holder and protection (answer to Q6)

**Holders.** I could not find a spring-loaded multi-cell parallel snap-in holder with a confirmed part number and listing. Use single open holders wired in parallel; they have fitted nickel-plated contacts.
- 21700, one cell each, through-hole solder lug: Keystone 1121, DigiKey 36-1121-ND: https://www.digikey.com.mx/en/products/detail/keystone-electronics/1121/12751670 ; SMD tab variant Keystone 1122, 36-1122-ND, $7.13 seen on a DigiKey regional page: https://www.digikey.com.au/en/products/detail/keystone-electronics/1122/12751696 . Keystone says it takes 20700 and 21700 from all makers. Two 1121 wired in parallel = 1S2P. Height 17.3 mm above board.
- 21700, two cells in one body, PC pin: Keystone 1123: https://www.digikey.ca/en/products/detail/keystone-electronics/1123/12751701 . **Whether the two cells are wired in series or parallel inside is not stated in what I could read** (the DigiKey page returned HTTP 403). Do not buy it for 1S2P until the datasheet is checked [U].
- 18650 alternative (single, through-hole), Keystone 1043 (36-1043-ND, about $2.90): https://www.digikey.com/en/products/detail/keystone-electronics/1043/2745669 ; 1043P polarized: https://uk.farnell.com/keystone/1043p/battery-holder-18650-th/dp/2673609 ; 1042 SMD: https://www.digikey.co.za/en/products/detail/keystone-electronics/1042/2745668 . (All from the earlier research, links confirmed then.)
- Eagle Plastic Devices BHC-21700 (Mouser, stainless nickel-plated 0.3 mm contacts, 21 x 70 mm) and Myoung BH-21700-B1BJ001 (SMD, JLCPCB C20606791) exist; no distributor purchase link verified for either. The "MPD BH-21700" part number named in the request does not appear on DigiKey or Mouser in my search [U].
- Open holders do not retain cells under vibration. Plan a printed cage and a rubber pad on the lid.

**Protection chain, in order from the cells:**
`cells in parallel -> PCM board -> 3 A fuse -> reverse-polarity FET -> bq24074 BAT port (and MAX17048 gauge on the same node) -> charger`

- **PCM/protection board.** 3 A-class boards (about 3 A overcurrent) will nuisance-trip: peak input current is 2.2 A at the worst case and 3.8 A at the 2 A-at-5 V ceiling, plus inrush into the boost's input caps. Use a board rated 5 A continuous with a higher trip. Candidates found: Tenergy 1S 3.6/3.7 V PCM, 5 A working, 15 A cutoff: https://power.tenergy.com/protection-circuit-module-pcb-for-3-6v-3-7v-1s-li-ion-battery-pack-working-5a-cutoff-15a ; Ayaatech PCM-LI01S6-264, 5 A continuous, overcharge 4.25 V +/-0.025: https://www.ayaatech.com/wp-content/uploads/2025/05/PCM-LI01S6-264.pdf . Neither verified for stock or for dissipation at 2-4 A; a reviewer quote says small boards should stay under 3 A or get a heat sink [V, search summary]. Overdischarge on these boards is typically 2.3-2.5 V: below the boost's 2.9 V minimum input, so the boost drops out first.
- **3 A fuse.** Bourns MF-R300 PTC, 3 A hold / 6 A trip, 30 V, radial, DigiKey 259975, $0.57-0.85: https://www.digikey.com/en/products/detail/bourns-inc/MF-R300/259975 . A PTC derates with temperature (hold current falls near 60 C in a case) and at the worst case the pack's 2.16 A is close to the limit. A fast 3 A glass/blade fuse is the cleaner choice; no link verified.
- **Reverse polarity.** Use the polarized JST on the charger, plus a P-channel MOSFET in the positive lead: drain to the battery side, source to the load side, gate to ground (body diode points from battery toward load). No part or link verified. Do not use a series Schottky: 0.3 V at 2 A is 0.6 W and shifts the charger's termination voltage.
- **Main switch.** SPST latching, rated 3 A or more: a single pole in the cell positive (B+) ahead of the boost; no second pole is needed (there is no VBUS sense line). With the switch open the battery chain is off; the Bela can still be powered by its own USB (board only). The boost ENABLE is not used as the off switch. No switch part or link verified.
- **Gauge.** Adafruit MAX17048 (product 5580): I2C 0x36, works on 3.3/5 V, takes a 1S pack on a JST port; DigiKey 1528-5580-ND: https://www.digikey.bg/en/products/detail/adafruit-industries-llc/5580/16719287 ; guide: https://learn.adafruit.com/adafruit-max17048-lipoly-liion-fuel-gauge-and-battery-monitor . Connect its battery JST to the pack node after the PCM and fuse. To Bela I2C P1.26 (SDA) / P1.28 (SCL) or Qwiic J6. The Gem already has 10 k pull-ups [V, SCH]; each breakout adds its own, so check the bus rise time as in `GROUNDING_PLAN.md`. It reports voltage and state of charge, not current.
- **bq24074 PGOOD/CHG.** Both are active-low open-drain pins; route to two Bela GPIOs through a 3.3 V pull-up (Bela GPIO is 3.3 V only [V, DS]). Level of CHG pin pull-up: use 10 k to 3V3 [U].

## 7. TX and LED supply

- Both draw from the **5 V boost rail**, never from VSYS or the Bela's 5 V pins. VSYS sags to 3.0 V at low cell, below the 3.8 V a TX would want.
- **TX regulator: TBD.** The TX is not named (`PARTS_POWER_SENSORS.md` open item 2). No verified breakout fits (3.8 V, at least 0.5 A, low noise, 5 V in). Dropout is not an issue (5.1 V in, 3.8 V out). A fixed 3.8 V LDO at 0.3-0.5 A dissipates (5.1 - 3.8) x 0.26 A = 0.35 W, tolerable. Search of Adafruit, SparkFun and Pololu found nothing; LT3045/LT3042 eval boards found on DigiKey are 3.3 V, 0.5 A / 0.2 A (DC2491A, DC2246B) [V, DigiKey listing summary], not a match.
- **LEDs:** own wire from the boost output to STAR, with the 470 uF and 330-470 ohm data resistor from `PARTS_POWER_SENSORS.md` C.5. Cap brightness so the LED load stays near 1 W. Never return LED current through a Bela pin.

## 8. Bench checks before trusting this

1. **Enumeration with the boost running:** plug the host into J14 with VIN_5V fed; read `cat /sys/class/udc/*/state` on the Bela and `dmesg` on the host. Then repeat with the boost off (switch open): the Bela should run from USB alone, no chatter.
2. USB_5V at P1.05/P1.07 with the host plugged in: 4.5 V or more under load.
3. With both sources present, confirm VIN_5V (boost, 5.1-5.2 V) is the winning source and the host current stays low.
4. VSYS at P1.24 with the boost running: 5.0-5.2 V.
5. Scope P1.24 and P1.14 at 20 MHz bandwidth, audio running, LEDs animating, battery at 4.2 V and 3.2 V. Add the LC filter only if the ripple shows up in audio.
6. Boost at 3.0 V cell: load the 5 V rail at 2 A, check input current (about 3.8 A) against the PCM and fuse, and the rail for sag under 4.75 V.
7. Plug into a laptop port with the boost running: verify the host current stays within 500 mA.
8. Re-check grounding with `GROUNDING_PLAN.md` sec. 7 items 8, 10, 11: noise with LEDs animating, move the USB plug's GND, charge-plus-load ripple.

## 9. Open decisions for Emory

1. Resolved 2026-11-12: a series inductor or ferrite in the +5 V supply lead is allowed; no ferrite or inductor in any ground or return path. (Still measure first.)
2. Which wireless TX, and is its 1 W measured at the terminals or the rail?
3. 2x or 3x P45B: 4.5 h or 6.75 h at worst case, 70 g difference.
4. Which cell vendor, after emailing Molicel for its dealer list.

## 10. BOM

Prices are as seen in search results or product pages on 2026-11-11 and move; "n/a" means not found. Nothing is purchased.

| Part | Qty | Link | Price | Status |
|---|---|---|---|---|
| Adafruit bq24074 USB/DC/Solar Li-Ion charger (4755) | 1 | https://www.adafruit.com/products/4755 | $14.95 | confirmed (search summary) |
| Pololu 5V Step-Up Regulator U3V70F5 (2891) | 1 | https://www.pololu.com/product/2891 | $25.95 | confirmed; ripple unspecified |
| Molicel INR21700-P45B | 2 | https://www.nkon.nl/en/rechargeable/molicel-inr21700-p45b-4500mah-45a.html | n/a | authorization unverified |
| Keystone 1121 21700 single holder (through-hole) | 2 | https://www.digikey.com.mx/en/products/detail/keystone-electronics/1121/12751670 | n/a | confirmed existence |
| 1S PCM, 5 A continuous (Tenergy 5 A / 15 A cutoff) | 1 | https://power.tenergy.com/protection-circuit-module-pcb-for-3-6v-3-7v-1s-li-ion-battery-pack-working-5a-cutoff-15a | n/a | not verified in stock |
| Bourns MF-R300 PTC 3 A hold (or a 3 A fast fuse) | 1 | https://www.digikey.com/en/products/detail/bourns-inc/MF-R300/259975 | $0.57-0.85 | confirmed |
| P-channel MOSFET (reverse polarity) | 1 | none verified | n/a | part TBD |
| DPST/DPDT main switch, 3 A or more | 1 | none verified | n/a | part TBD |
| Adafruit MAX17048 gauge (5580) | 1 | https://www.digikey.bg/en/products/detail/adafruit-industries-llc/5580/16719287 | about $12-13 elsewhere, DigiKey price not seen | confirmed |
| 2.2-4.7 uH, 3 A+ inductor, 47-100 uF + 10 uF + 100 nF caps (supply filter) | 1 set | none | n/a | generic, use only if scope shows need |
| TX 3.8 V LDO (0.5 A, low noise) | 1 | none verified | n/a | TBD, TX not named |
| Optional alternates: Pololu S13V30F5 (4082), U3V50F5 (2565) | 0 | https://www.pololu.com/product/4082/specs , https://www.pololu.com/product/2565 | $17.95 , $29.95 | flawed, see sec. 4.3 |
| Optional 3x 18650 path: Samsung 30Q x3-4, Keystone 1043 x3-4 | 0 | https://www.18650batterystore.com/products/samsung-30q , https://www.digikey.com/en/products/detail/keystone-electronics/1043/2745669 | n/a , about $2.90 | not recommended |

## 11. Block diagram

```
                                           HYPERTRUMPET V2 POWER

 [2x P45B in parallel]--[PCM 5 A]--[hard switch, B+]--+--> [MAX17048] --I2C--> Bela P1.26/P1.28
                                                      |
                                              [Pololu U3V40F5 boost, 5 V]
                                                      |
                       +------------------------------+--------------------------+
                       |                              |                          |
          [optional series ferrite]      [optional series ferrite]      [optional series ferrite]
                       |                              |                          |
          Gem VIN_5V (P1.01),            NeoPixels / NeoTrellis          TX LDO 3.8 V (TBD) -> TX,
          Bela GND (P2.15)               (own wires, return to STAR)     analog LDO input
                       |
   Gem internal ideal-diode OR (LM73100 x3) <--- USB_5V <--- board's own USB-C (PB2 J14) <--- host
                       |                                  (straight to the host, board only,
                      VSYS                                 strain-relief cradle in the housing)

 B- == STAR (battery negative). No ferrite or inductor in any ground or return path.
```
