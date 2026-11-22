# Amazon add-by-hand checklist (2026-11-12)

Amazon's bot check blocks automation, so add these by hand. Already in cart: 21700 holders (Bistook 10-pack; 4 used, 2 per instrument), DSD TECH USB isolator. Cells are NOT on Amazon (8 Molicel P45B from an authorized vendor: 2 per instrument + 4 spares).
Match each item to the spec in BOM_SHOPPING_2026-11-12.md.


## Table 1: HYPERTRUMPET V2

- [ ] **1x** Daisy Seed 3 (trumpet; NOT Amazon-first: Electro-Smith/daisy.audio or reseller; Emory's owned Seed 3 goes to the hat) (#1)
- [ ] **1x** 1S Li-ion protection board (PCM), multi-FET (#4)
- [ ] **1x** Pololu 5 V step-up U3V40F5 (item 4012) (#5)
- [ ] **1x** MAX17048 fuel gauge (Adafruit 5580 or same-chip generic) (#6)
- [ ] **1x** Hard power switch on battery positive (#7)
- [ ] **1x** Switched 3.5 mm stereo panel jack (TRS) (#8)
- [ ] **2x** JST-PH 2.0 mm 2-pin battery pigtails (#9)
- [ ] **1x** Linear Hall sensors, TO-92 (SS49E) (#10)
- [ ] **1x** N52 disc magnets 6 mm x 3 mm, AXIAL (#11)
- [ ] **1x** ADS1115 16-bit ADC (3 Halls) (#12)
- [ ] **1x** Preamp op-amp, DIP-8 breakout (OPA1612) (trumpet bell) (#14)
- [ ] **1x** Low-noise LDO module for the analog rail (LT3045/LT3042 class) (#15)
- [ ] **1x** Electret bias network + signal-path caps (shared kits) (#16)
- [ ] **1x** Shielded mic cable, soldered direct (no connector) (#17)
- [ ] **1x** Protoboard for the preamp (#18)
- [ ] **1x** Qwiic/STEMMA QT JST-SH 4-pin cables, mixed 50-200 mm (#20)
- [ ] **1x** Qwiic JST-SH to 4-pin female header cable, 100-150 mm (#23)

Removed 2026-11-12: contact-mic buffer + piezos, NeoPixel seesaw driver, 74AHCT125 (trumpet). Power is now 2 cells (1S2P) in 2 holders per instrument. Holders: already in cart, 10-pack covers both builds.

## Table 2: Vocal HAT

- [ ] **1x** 1S Li-ion protection board (PCM), multi-FET (#4)
- [ ] **1x** Pololu 5 V step-up U3V40F5 (item 4012) (#5)
- [ ] **1x** MAX17048 fuel gauge (Adafruit 5580 or same-chip generic) (#6)
- [ ] **1x** Hard power switch on battery positive (#7)
- [ ] **1x** Switched 3.5 mm stereo panel jack (TRS) (#8)
- [ ] **2x** JST-PH 2.0 mm 2-pin battery pigtails (#9)
- [ ] **1x** Preamp op-amp, DIP-8 breakout (OPA1612) (headset) (#11)
- [ ] **1x** Low-noise LDO module for the analog rail (LT3045/LT3042 class) (#12)
- [ ] **1x** Electret bias network + signal-path caps (shared kits) (#13)
- [ ] **1x** Protoboard for the preamp (#15)
- [ ] **1x** BTF-Lighting FCOB WS2812B strip, 3.2 ft, 160 LED/m, 5 V (~157 px), capped ~20/255 (#18; replaces the 30-px strip)
- [ ] **1x** 5 V level shifter for NeoPixel data (74AHCT125) (#19)
- [ ] **1x** Qwiic cable to Daisy pin header + Qwiic daisy chain (#20)
- [ ] **2x** Qwiic JST-SH 4-pin cables 50-100 mm (x2) (#21)
- [ ] **1x** High-side 5 V switch for the LED strip (logic-level P-MOSFET module or load switch, >=2 A) (#22)

## Table 3: SHARED (cells, charging station, tools, consumables)

- [ ] **1x** XTAR VC8-class 8-bay charger, sold by XTAR/Orbtronic (#2; charges a full spare set)
- ~~2-bay 21700 charger (#3)~~ dropped 2026-11-12 (the 8-bay covers it)
- [ ] **1x** USB-C PD wall charger 45-65 W + 2 cables (#4)
- [ ] **2x** 21700 plastic carry cases x2 (go-bags) (#5)
- [ ] **2x** Fireproof LiPo bag x2 (optional) (#6)
- [ ] **1x** Silicone stranded hookup wire kit 22 + 26 AWG (#7)
- [ ] **1x** Resistor assortment 1% metal film (10 ohm to 1 Mohm, incl 330, 1k, 2.2k, 10 (#8)
- [ ] **1x** Capacitor assortment (ceramic X7R/C0G + electrolytic/polymer) (#9)
- [ ] **1x** JST-PH 2.0 mm connector kit (#10)
- [ ] **2x** Star-ground terminal block / Wago (#11)
- [ ] **1x** TPU 95A filament 1 kg (trumpet liner) (#12)
- [ ] **1x** Solder 63/37 0.5 mm, flux pen, desolder braid, Kapton (#13)
- [ ] **1x** Heat shrink assortment + zip ties (#14)
- [ ] **1x** Ferrite beads (supply rails only) (#15)

## Added 2026-11-12 (Halls on Amazon)
- [ ] **1x** N52 disc magnets 8 mm x 3 mm or 10 mm x 3 mm, axial, 20+ pack (compensates SS49E's ~3x lower sensitivity; test both sizes)
- [ ] Halls: try DRV5055 / DRV5053 linear Hall first; if nothing real, use the SS49E row above (TO-92, works at 3.3 V)
