# HYPERTRUMPET V2 — wireless audio research

Collected 2026-11-11. Research only: nothing bought, links only. Prices are street/list USD, approximate.

Tags: **[S]** = from a manufacturer/datasheet/manual source; **[R]** = retailer listing or review (can be wrong); **[U]** = unverified / my estimate / from memory — measure or confirm before relying on it.

## 0. Source signal (what the Bela side gives us)

- Gem Stereo: 2 in / 2 out, TLV320AIC3104 codec, "headphones (min. 1 ohm), differential line out", USB-C device port + USB-A host port (host port "supports Wi-Fi, Bluetooth or Ethernet peripherals"). [S] Bela Gem datasheet Table 1.
- Bela's datasheet does NOT state line-out level or output impedance. [S, gap]
- TI AIC3104 datasheet: differential line out full-scale 1.414 Vrms (4 Vpp) into 10 kohm, 0 dB gain; single-ended line out 0.707 Vrms. Gain adjustable 0 to 9 dB. [R: read via search summary of TI SLAS510G; confirm on the PDF]
- Output impedance of the Gem line out: not published anywhere I found. [U] Measure it (load with 10 k / 1 k, compare voltage). Assume low hundreds of ohms or less for a codec line driver, but that is a guess.
- Power in: USB_5V / VIN_5V valid 3.8 to 5.5 V; **VBAT valid 3.2 to 4.6 V** (turned off if the others are present); onboard BQ21040 charger up to 540 mA. [S] Bela Gem datasheet. So the 1S Li-ion can feed the Gem directly; the 5 V boost rail is optional.
- Practical consequence: a +3 dBV (1.414 Vrms) line signal is hot for anything built for instrument or mic input. Most TX inputs need an attenuator pad (see section 5).

## 1. Candidate comparison

Embeddability notes: the TX has to fit inside a trumpet, run off the rig, and accept line level.

| System | Band / type | Latency | Bits / rate | Stereo? | Simultaneous channels | Range (LOS) | TX size / power | Price (approx) |
|---|---|---|---|---|---|---|---|---|
| Xvive U2 | 2.4 GHz digital | ~5-6 ms [R] | 24 / 48 [R] | mono | 4 systems [R] | 70-100 ft (listings disagree) [R] | small TX, internal Li-ion, ~5 h [R] | ~$70-90 [U] |
| Xvive U4 (IEM) | 2.4 GHz digital | <5 ms [S] | 24 / 48 [R] | **TX is mono** (XLR or 1/4" TS in); 3.5 mm is on the receiver. Not stereo. [S] [R] | 6 [S] | ~90 ft [R] | TX 100x30x28 mm [S]; internal Li-ion, 5 h, 5 V USB charge [S]; voltage/capacity not stated [S gap] | ~$130-150 set [U]; U4R add-on RX |
| Shure GLX-D (original) | 2.4 GHz, picks best 3 of pool | 4.0 ms (freq table) [S] | 24 / 48 [U] | mono | 4 typical, up to 8 ideal [R, conflicting] | ~65 ft [U] | Li-ion SB902 3.7 V 1900 mAh [S]; discontinued | used market only [U] |
| Shure GLX-D+ dual band | 2.4 + 5.8 GHz (208.5 MHz total, 83.5 + 125) [R] | 4.6 ms (Shure comparison) [S] | 24 / 48 [U] | mono | 4 typical / 8 ideal; some retailers say 11-16 for rack systems [R, conflicting] | ~ 100 ft [U] | GLXD1+ battery SB904 3.6 V 2420 mAh (8.71 Wh) [R] | GLXD14+ guitar system $499-599 [R] |
| Sennheiser XSW-D Instrument | 2.4 GHz | <4 ms (family spec, lavalier set PDF) [S] | 24-bit [U] | mono | one TX at a time per RX; channel count not found [R] | 75 m optimistic [R] | 6.3 mm plug TX, Li-ion [U] | ~$300 [U] |
| Sennheiser EW-D SK + EM | **UHF digital** (520-608 MHz etc., region-specific) | 1.9 ms [R/S] | 24-bit [U] | mono | scales to dozens with frequency planning [U] | 100+ m [U] | 2x AA (1.5 V) or BA 70 pack [S]; 3.5 mm lockable TRS in, max input listed 2-4.35 Vrms [R, odd, confirm]; 10 mW | ~$500 TX + ~$500 RX [U] |
| Line 6 Relay G10 (G10TII) | 2.4 GHz | <2.9 ms [R] | 24-bit [R] | mono | up to 14 [R] | ~ 60 ft [U] | plug-in TX, internal Li-ion [U] | ~$140-170 [U] |
| Line 6 Relay G30 | 2.4 GHz | <2.9 ms [U] | 24-bit [R] | mono | 6 [R] | ~100 ft [R] | internal Li-ion [U] | ~$230 [U] |
| Line 6 Relay G70 | 2.4 GHz | claim <1.5 ms, vendor claim [R] | 24-bit [U] | mono | channel count not confirmed [U] | n/a | n/a | ~$400+ [U], availability dwindling [R] |
| Boss WL-20 / WL-20L | 2.4 GHz | **2.3 ms** (manual) [S] | 24-bit [U], dyn. range >=110 dB [R] | mono | 14 channels, 14 systems [S] | 50 ft [S] | TX docks into RX to pair/charge; internal cell [U] | ~$200 [U] |
| Boss WL-50 | 2.4 GHz | same tech as WL-20, no number [R] | - | mono | - | - | - | ~$300 [U] |
| Nux B-2 Plus | 2.4 GHz | <5 ms [R] | - | mono | 4 [R] | - | - | ~$70 [U] |
| Nux B-5RC | 2.4 GHz | <5 ms [R] | 24 / 44.1 [R] | mono | 6 [R] | - | charging-case TX [U] | ~$110 [U] |
| Nux B-8 | 2.4 GHz | 2.5 ms (min buffer) to 8.7 ms (max retransmit) [R] | 24 / 48 [R] | mono | listings conflict (1 vs "up to 79" with narrow channels) [R] | - | - | ~$160 [U] |
| Nux C-5RC | **5.8 GHz** | 4 ms [R] | - | mono | - | - | - | ~$100 [U] |
| JOYO JW-02S, Getaria GWS-26, Lekato | 5.8 GHz | JOYO none stated; Getaria <5.6 ms; Lekato <6 ms (all listings) [R] | Getaria listing says 24/48 [R] | **All are guitar-jack TX, mono as far as I can verify.** One Newegg Getaria listing says "stereo digital"; I treat that as unverified marketing. [R/U] | 4 channel banks (JOYO) [R] | 20-ish m [R] | cheap, no real specs [U] | ~$40-80 [U] |
| Shure PSM300 (P3TR) | analog UHF FM, stereo (19 kHz MPX pilot) | <0.7 ms [R] | analog, companded (ARC) | **yes, stereo** | many, with frequency coordination [U] | ~90 m [R] | **The TX is a half-rack box; only the bodypack is the receiver. Cannot be embedded.** | ~$400-600 [U] |
| Rode Wireless GO II (2 TX + 1 RX) | 2.4 GHz digital (Series IV) | 3.5-4 ms [R/S] | 24-bit [U] | **Split mode = 2 TX, one RX, TX1 = L, TX2 = R** [R] | 4 TX per band [R] | 200 m LOS (claim) [R] | TX 3.5 mm TRS input, max level **-20 dBV**, +20 dB preamp [S]; TX internal Li-ion, ~7 h [U] | $299 for 2-person kit [U] |
| Rode Wireless PRO | 2.4 GHz | no number, "near zero" [R] | 32-bit float [R] | Split mode, stereo out on 3.5 mm + USB-C [R] | - | - | 3.5 mm TRS in; ~7 h [R] | ~$399 [U] |
| Boya BY-WM4 Pro K2 | 2.4 GHz | **not found** [gap] | - | RX has stereo/mono modes; TX has 3.5 mm Line In [S/R] | - | - | - | ~$100-130 [U] |
| Hollyland Lark 150 | 2.4 GHz | <5 ms claim; reviewer 6-10 ms [R] | - | stereo/mono/safety-track mode on RX [R] | - | 100 m claim [R] | - | ~$150 [U] |
| Hollyland Lark Max 2 | 2.4 GHz | 20-25 ms [R] | - | - | - | - | - | too slow |
| KT0803L-class FM stereo TX chip | analog FM, 70-108 MHz | ~0 + RX DSP [U] | analog, 15 kHz BW [U] | stereo (MPX) | limited by FM band occupancy | meters, legally limited [U] | <17 mA, 1.6-3.6 V chip supply [S/R] | chip ~$1-3, dev breakouts ~$5 [U] |
| Bela Wi-Fi/JackTrip/SonoBus | 2.4/5 GHz Wi-Fi + UDP | JackTrip: Wi-Fi adds >=5 ms best case, 3 to >100 ms typical, with jitter [S] | any (16/24, 48 k) | native stereo | limited by AP; no | AP-dependent | needs USB Wi-Fi dongle on host port + Linux driver [U] | ~$30 dongle + Pi |

Notes:
- Deity (lav systems): not researched in depth; same class as Rode/Boya/Hollyland. [gap]
- Sennheiser XSW-D and EW-D are mono systems. I found no stereo instrument or line TX from Shure/Sennheiser/Line 6/Boss/Nux/Xvive. The only stereo-out path among the "mono" systems is Rode/Boya/Hollyland, where stereo means two TX into one RX.
- U4 is explicitly mono at the TX (listing + a Thomann reviewer: "It's MONO!"). The premise "U4 is stereo" is wrong; stereo is only the IEM receiver headphone out. [S][R]
- A Line 6 G90 user reported U4 dropouts when the G90 powered on. [R] This is the practical risk of mixing 2.4 GHz families.
- KT0646: no datasheet found. [gap]

## 2. Technology comparison

**2.4 GHz digital (U2/U4/G10/WL-20/Nux/Rode/Boya/Hollyland/GLX-D)**
- Latency 2-6 ms typical on the vendor claims; some devices trade latency vs. retransmit depth (Nux B-8: 2.5 to 8.7 ms).
- 24-bit / 44.1-48 kHz typical. Stereo only by pairing two TX into one RX.
- Channels: 4-14 per family, but all share one 83.5 MHz band with Wi-Fi/Bluetooth/other wireless. Frequency-hopping helps but different families do not coordinate. Dropout behaviour: usually a brief mute or burst of digital artifacts, then recovery. Not graceful (no hiss) — it clicks or silences.
- Body/brass absorption is worst at 2.4 GHz (Shure notes body absorption hurts 2.4 GHz more than UHF). [S]
- Cheap and tiny. Weakest in a crowded venue.

**5.8 GHz digital (JOYO, Getaria, Lekato, Nux C-5RC, GLX-D+ upper band)**
- 125 MHz of band [R], much less Wi-Fi traffic than 2.4 GHz in most venues (but 5 GHz Wi-Fi UNII-3 channels 149-165 overlap 5.725-5.85 GHz, so not empty).
- Shorter wavelength (5.2 cm): easier to shadow, worse through bodies and brass; range smaller.
- The cheap units are mono. GLX-D+ is the only name-brand dual-band system found.

**UHF digital (Sennheiser EW-D/EW-DX, Shure QLX-D/ULX-D/ADX)**
- Latency 1.9 ms (EW-D) [R/S]; ULX-D/QLX-D 2.9 ms (Shure comparison) [S].
- Best for brass and bodies: longer wavelength (~50 cm), diffracts around the player, fewer dropouts. Scales to dozens of coordinated channels with a frequency plan. Need regional UHF license/band legality (the US TV-band rules changed; check [U]).
- Bodypacks are larger and run on 2xAA or a 3.7 V pack (BA 70 / SB900-class) at about 200-400 mA average [U]. Expensive: ~$500+ per link per side.
- Mono only.

**Analog RF (PSM300, older UHF analog)**
- Lowest latency (<0.7 ms listed) [R]; graceful degradation (hiss, not mute) but companding artifacts and a 19 kHz stereo pilot (PSM300) [R].
- Stereo exists, but **the stereo transmitters are rack IEM boxes**, so not embeddable.
- Analog FM modules (KT0803) are embeddable but low fidelity (FM stereo noise floor ~50-60 dB SNR [U]), regulatory power limits, and the 87.5-108 MHz band is full of broadcast stations. Fine for a gimmick, not a performance link.

**Wi-Fi/Ethernet route (Bela -> UDP -> Pi running JackTrip/SonoBus)**
- Real stereo, uncompressed, with software-controlled buffer; zero extra hardware cost besides a USB Wi-Fi dongle. Needs a Linux driver for the dongle on the PocketBeagle 2 image [U].
- JackTrip support says Wi-Fi adds at least 5 ms even on Wi-Fi 6 and often much more, with jitter that causes glitches and dropouts. CCRMA's JackTrip docs say to avoid Wi-Fi in favour of Ethernet. A larger receive queue reduces dropouts at the price of latency. [S]
- Sonobus on a Pi: anecdotal random pauses reported, also anecdotal success. [R]
- Honest verdict: 10-30 ms with a safe buffer, not deterministic, and it lives in the same 2.4 GHz crowd as everything else (use 5 GHz if the dongle supports it). Not recommended as the primary audio link. Good for control/OSC and as a **backup** channel.

## 3. Direct answers

**1. Are two mono instrument links (L and R) viable?**
Yes, with caveats:
- Fixed latency difference is fine: measure it with an impulse through both links and add a compensating delay in the Demiurge chain on the faster side. That is a sample-accurate fix.
- What cannot be fixed is per-packet jitter and retransmit variation (e.g. Nux B-8 at 2.5 to 8.7 ms depending on retransmit depth, or any system that re-buffers on a bad channel). One-sided dropouts also collapse the stereo image.
- Inter-channel timing: 1 ms is ~34 cm of path difference, so a skew of a few hundred microseconds is audible as image shift on a headphone/studio setup, but hardly matters live. Skew above ~1 ms smears the image.
- Use two identical units of the same firmware. Do not mix families.
- Two independent systems have independent ADC/DAC clocks; at the receivers, run both outputs into one multichannel interface so the sampling clock is shared. Clock drift between two TX is not an issue (they sample analog independently), but a one-sample slip at the RX is possible on some designs. [U]
- Best-matched: **Rode Wireless GO II / PRO in split mode**: both TX talk to one RX which time-slots them in one frame, so latency is matched by construction, and the RX outputs L/R on a single 3.5 mm. [R]

**2. Best stereo option under ~$300 per instrument**
**Rode Wireless GO II 2-person kit in split mode** (~$299 [U]; two TX + one RX; 3.5-4 ms stated). Constraints: TX max input is -20 dBV, so a ~26 dB pad is mandatory on each channel; the TX jack supplies plug-in power bias, so use a DC block on the line side; the TX battery is internal and its charger/fuel gauge may not behave with the cell removed. [S/U] Runner-up: Boya BY-WM4 Pro K2 (~$100-130 [U]; explicit TX Line In and stereo mode, latency not published — would need measuring).
Cheapest mono pair: Xvive U2 x2 (~$150) or Line 6 G10 x2 (~$300) with latency matching via DSP.

**3. Best pro option**
Two **Shure GLXD1+ (GLXD14+) links** (dual band 2.4/5.8 GHz, 4.6 ms; ~$500-600 each [R] => ~$1,000-1,200). For the best brass-and-body performance, two **Sennheiser EW-D SK + EW-D EM** (UHF digital, 1.9 ms, ~$1,000 per link [U] => ~$2,000+). Mono each, so L/R is a pair of mono links with DSP delay matching. There is no pro stereo bodypack TX that I could verify.

**4. Which scale to 6+ concurrent links**
- UHF digital (EW-D/EW-DX, QLX-D/ULX-D, GLX-D+ with its frequency manager): the realistic path. Plan frequencies, spread across bands.
- Boss WL-20/WL-50: 14 systems per space [S]; Line 6 G10: up to 14 channels [R]. Stereo = 2 links, so 3 stereo instruments or one stereo + 4 mono.
- Xvive U4, Nux B-5RC, Line 6 G30: 6 channels each [S/R]; 6 links is exactly the ceiling for that family, leaving no headroom.
- Rode Wireless GO II: 4 TX per band => one stereo trumpet plus one more stereo pair; does not scale. [R]
- Mix families across bands: 2.4 GHz family A, 5.8 GHz family B, UHF family C. Do not run two 2.4 GHz families at once without testing (U4 vs G90 dropouts reported [R]).

**Full-system costs (6 mono links, hardware only, approx [U])**
- Cheap 2.4/5.8 (U2/Nux/JOYO): ~$250-500
- Mid (G10/WL-20/U4): ~$800-1,200
- Pro (GLXD1+): ~$3,000-3,600; Pro UHF (EW-D): ~$6,000

## 4. Integration: powering the TX from the rig

- **What the TX expects**: all the small digital TXs here (U2, U4, G10, WL-20, Rode, Boya, GLXD1+) run from a 1S Li-ion/Li-Po cell = 3.0 to 4.2 V, nominal 3.6-3.7 V. Shure SB902/SB904 are 3.7/3.6 V. [S/R]
- **Current**: U4 TX battery life 5 h, U2 ~5 h [S/R], Rode ~7 h [R]; with cell capacities typically 300-700 mAh [U] that is roughly 60-150 mA average. GLXD1+ at 2420 mAh [R] and ~12 h advertised [U] is ~200 mA. Design the feed for 300 mA continuous per TX with a 500 mA peak, per TX [U]. Measure each TX with a bench supply and an ammeter before choosing a regulator.
- **Recommended feed**: take it from the rig's 1S pack (3.2-4.2 V) or from the boost rail, not both. The Gem's VBAT input accepts 3.2-4.6 V [S] so the pack can run everything. Feed each TX through:
  - a ferrite bead + 10-100 uF + 100 nF pi filter,
  - a low-noise LDO (for example an ADP7118 or LT3042-class device [U]) set to ~3.8 V, sourced from the 5 V rail, so the TX sees a quiet, constant "cell voltage" regardless of boost ripple; the 5 V boost is a switching rail and is the main noise source into audio, which is why not to tap it raw.
  - Do NOT tap the TX supply directly from the Gem's 5 V pin, which is VSYS (an output; "an external power source should not be connected to it directly"). [S]
- **The battery it replaces**: remove the cell and solder the filtered feed to the cell pads. Disconnect the TX's USB VBUS from the charge IC (or remove the charger IC feed) so the TX charger cannot backfeed the rig or try to charge a missing cell. Risks to check: some TXs have fuel-gauge/protection chips (thermistor pin, NTC) that shut the TX down with no cell; fake the NTC with a 10 k resistor if so. [U]
- Keep the TX's own power button wired (or bridge it) so it boots on power-up; check pairing persistence across power cycles (the U4 guide forbids use while charging, so remove the USB path). [S]
- 1S pack protection: add an inline fuse/PTC (e.g. 1 A) in front of each TX. [U]

## 5. Level matching and noise

- **Level**: Gem line out is up to +3 dBV (1.414 Vrms differential) [R]; Rode TX max input is -20 dBV [S]. Required attenuation >=23 dB, ~26 dB gives headroom: a divider like 20 k series / 1 k shunt on each channel, sized so total shunt > 10 x the Gem output impedance and < the TX input impedance (unknown, check) [U]. For instrument-style TX (U2/G10) set the divider for ~1 Vrms max [U]. For EW-D SK the listed max input is 2-4.35 Vrms [R], which suits line level with no pad; confirm in the product spec.
- **Differential to single ended**: the Gem line out is differential; feed the TX from one leg (+ ground) for simplest wiring (-6 dB, i.e. 0.707 Vrms), or use a differential receiver (an instrumentation amp such as an INA134-style stage) for 1.414 Vrms. [U]

**Ground loop prevention with TX, USB-C charger and wired 3.5 mm out sharing ground**
1. Break the loop at the cable, not at the charger: put a 1:1 audio isolation transformer (e.g. Jensen / Lundahl / Ebtech-class stereo line isolator) in the wired 3.5 mm output path, or build one in. An isolator on the output keeps a floating instrument from forming a loop through mixer ground plus charger earth. [U]
2. If the USB-C is power-only: charger-side common-mode choke / ferrite on the cable (e.g. clip-on or an in-line CMC) plus an isolated DC-DC (e.g. a 5 V to 5 V isolated module) in front of the rig, so the charger's leakage never references to audio ground. [U]
3. If the USB-C also carries data (Bela, MIDI): use a USB isolator such as an ADuM3160/ADuM4160-class part [U] or use the host's ground with power only.
4. Star ground: one tie point between analog ground, digital ground and TX ground, near the Gem's audio connector.
5. RF immunity: ferrite on each audio lead entering the TX, 100 pF to ground at the jack, a 10-100 uF + 100 nF bypass at the TX supply.
6. Keep the TX antenna away from the DC-DC inductor and Gem 5 V boost; switching harmonics land in the audio and in 2.4 GHz noise floor. [U]

**3.5 mm jack switch (disconnect the TX when a cable is plugged in)**
- Use a stereo 3.5 mm panel jack with built-in normalled switches (tip and ring switches, one for each channel); a closed-when-empty switch pulls the gate of a P-FET load switch (e.g. TI TPS22918-class [U]) that powers both TXs. Plugging a cable opens the switch, turns the TXs off, and wires the Gem out to the cable.
- Do not switch the audio path alone; kill TX power so it stops radiating. Delay re-enabling on cable removal by ~1 s so the TX boot and pairing finish cleanly (pair time unknown, [U], measure it). On the 'wired' branch, the isolation transformer must remain in circuit.
- Muting: also mute the Bela output for ~50 ms during plug events to avoid clicks.

## 6. Antenna placement near a brass bell

- Brass is a conductor: it shadows and detunes a nearby antenna. At 2.4 GHz the wavelength is 12.5 cm (quarter wave ~3 cm), so a trumpet bell (about 12 cm) and leadpipe are comparable to a wavelength; at 5.8 GHz (5.2 cm) a bell blocks line of sight fully; UHF (~50 cm) diffracts around it far better.
- Guidance from production-sound sources: keep antennas off the body and 15-30 cm from large metal surfaces when they must run parallel; do not coil or bend them; aim for line-of-sight and use opposite-side receiver antennas so the performer's body never blocks both. [R: Shure, local695, filmtvsound guides] There are no measurements for brass specifically. [gap]
- Practical plan:
  1. Do not leave the TX's stock PCB/chip antenna inside the tubing or the bell. Move it out through a short u.FL/IPEX pigtail to a small external dipole or whip, mounted on a 3D-printed non-conductive standoff (>=1 cm, ideally 2-3 cm) off the brass.
  2. Place the antenna on the **valve-casing side facing the audience/receiver and away from the player's body**, not under the bell and not in the player's lip/hand zone. Avoid mounting inside the bell throat.
  3. Orient the antenna perpendicular to the main tube axis (vertical when played level) and keep both TX antennas in the same polarization; do the same for the two TXs of a stereo pair, ideally a few centimetres apart but no closer than ~5 cm to avoid desense.
  4. Run range tests at 2 m, 5 m and 10 m at all playing angles (bell up, bell down, facing away), logging dropouts. Use two receiver antennas on opposite sides of the stage.
  5. Expect 2.4/5.8 GHz to lose range when the performer turns away; UHF digital will degrade the least. [U]
- Detuning: a quarter-wave stub antenna near metal will shift its resonance and mismatch; use the manufacturer's antenna kept clear of brass or retune on the bench with a NanoVNA (S11) before closing the instrument. [U]

## 7. Recommendation

- **Build now (under $300)**: Rode Wireless GO II 2-person kit (~$299), split mode, 2 TX gutted, 26 dB pad each, powered from a filtered 3.8 V LDO rail, antennas externalised. Matched L/R by design, 3.5-4 ms. Watch items: TX min input -20 dBV, unknown fuel-gauge behaviour with the cell removed.
- **Cheaper fallback**: 2x Xvive U2 (~$150) or 2x Boya WM4 Pro K2 (~$100-130; measure latency first).
- **Pro**: 2x Shure GLXD1+ (~$1,000-1,200) or 2x Sennheiser EW-D SK/EM (~$2,000+) if the rig needs UHF-grade dropout behaviour; plan frequencies across 2.4/5.8/UHF to reach 6+ links.
- **Wired fallback**: Gem differential line out -> isolation transformer -> 3.5 mm TRS switched jack, power from USB-C through an isolated DC-DC; TX powered down by the jack switch.
- Do not use Wi-Fi/JackTrip, KT0803 FM or Bluetooth as the main link.

## 8. Open items to measure on the bench

1. Gem Stereo line-out voltage and output impedance (datasheet gap).
2. Each candidate TX: current at 3.8 V, whether it boots with the cell removed, NTC/fuel-gauge needs.
3. Latency of Boya WM4 Pro K2 and Rode GO II line-in path (loopback impulse measurement), including L/R skew.
4. Real concurrent-link count in the performance room (spectrum scan 2.4/5.8 GHz and UHF).
5. Antenna position test on the actual trumpet.

## Sources

- Bela Gem datasheet: https://bela.io/datasheets/bela_gem_datasheet.pdf
- Bela Gem Stereo shop page: https://shop.bela.io/products/bela-gem-stereo
- TI TLV320AIC3104 datasheet: https://www.ti.com/document-viewer/TLV320AIC3104/datasheet
- Xvive U4 page: https://xvive.com/audio/product/u4-in-ear-monitor-wireless-system/ and https://xvive.com/audio/?p=269
- Xvive U4 retail: https://www.thomannmusic.com/xvive_u4_wireless_system_transmitter.htm , https://www.zzounds.com/item--XVIU4SET , https://www.bhphotovideo.com/c/product/1518271-REG/xvive_audio_u4_wireless_in_ear_monitor.html
- Xvive U2 retail: https://www.zzounds.com/item--XVIU2 , https://www.adorama.com/xvu2black.html
- Shure GLX-D / GLX-D+: https://www.shure.com/en-US/products/accessories/sb902 , https://www.manualslib.com/manual/678125/Shure-Glx-D.html?page=8 , https://cdn.shopify.com/s/files/1/1639/9445/files/GLX-D_plus_Dual_Band_SpecSheet_A4_EN.pdf , https://www.shure.com/en-US/insights/which-shure-digital-wireless-system-is-right-for-you , https://geartechs.com/products/shure-glxd14-instrument-system-glx-d-dual-band-wireless
- Sennheiser EW-D SK spec: https://sennheiser.com/globalassets/digizuite/49044-en-ew-d_sk_product_specification_v1.4_en.pdf ; system spec: https://www.sennheiser.com/globalassets/digizuite/49049-en-ew-d_system_specification_v1.3_en.pdf ; XS Wireless Digital: https://www.sennheiser.com/globalassets/digizuite/41716-en-sp_1227_v1.2_xs_wireless_digital_portable_lavalier_set_product_specification_en.pdf ; XSW-D instrument: https://www.sennheiser.com/en-us/catalog/products/wireless-systems/xsw-d-instrument-base-set/
- Line 6 Relay: https://www.adorama.com/line-6-relay-digital-wireless-guitar-system-g10tii-transmitter-g10r-receiver/p/lireg10ws , https://guitarworld.com/gear/review-line-6-relay-g30-g50-and-g90-digital-wireless-guitar-systems , https://visionguitar.com/products/line-6-relay-g70-digital-wireless-guitar-system
- Boss WL-20: https://www.boss.info/us/products/wl-20_wl-20l ; manual https://cdn.shopify.com/s/files/1/0523/4648/6935/files/Boss_WL-20_Wireless_System_Owner_s_Manual.pdf
- Nux: https://reverb.com/item/91001539-nux-b-8-professional-2-4ghz-guitar-wireless-system , https://metronomemusic.com/products/nux-b-5rc-wireless-guitar-system , https://mixdownmag.com.au/news/nu-xs-new-wireless-range-provides-something-for-everyone/
- JOYO JW-02S: https://www.joyoaudio.co.uk/joyo-5-8ghz-jw-02s-wireless-guitar-system-low-latency-digital-transmitter-and-receiver ; Getaria GWS-26 (Newegg): https://www.newegg.com/p/1VH-01XN-019C1
- Shure PSM300: https://www.adorama.com/shure-psm-300-stereo-personal-wireless-monitor-system-se112/p/shp3tr112h20 , https://www.manualslib.com/manual/973316/Shure-Psm-300.html?page=13
- Rode Wireless GO II: https://edge.rode.com//pdf/products/1237/WIRELESS_GO_II_TX_DS_V02.pdf , https://www.bhphotovideo.com/c/product/1622642-REG/rode_wigoii_wireless_go_ii_compact.html , https://www.newsshooter.com/2021/02/27/rode-wireless-go-ii-review/
- Rode Wireless PRO: https://edge.rode.com//pdf/products/1267/WIRELESS%20PRO_Datasheet_05_FA.pdf
- Boya BY-WM4 Pro: https://manuals.plus/asin/B08DS3R8RM.pdf , https://www.adorama.com/bywm4prorxd.html
- Hollyland Lark 150: https://www.cined.com/hollyland-lark-150-dual-wireless-audio-system-announced/ , https://fstoppers.com/audio/lark-150-microphone-system-reviewed-effortless-wireless-audio-540151 , Lark Max 2: https://fstoppers.com/audio/hollyland-lark-max-2-review-feature-rich-package-wireless-audio-712034
- KT0803: https://www.radiolocman.com/datasheet/pdf.html?di=186075 , https://done.land/components/data/datatransmission/wireless/fmpublicbroadcast/sender/hw-158/
- JackTrip over Wi-Fi: https://support.jacktrip.com/wired-internet-versus-wi-fi , https://support.jacktrip.com/how-to-optimize-latency-when-using-jacktrip , https://ccrma.stanford.edu/docs/common/JackTrip.html , https://groups.google.com/g/sonobus-users/c/26l2dxKDApc
- Antenna placement: https://www.shure.com/en-us/insights/avoid-these-five-mistakes-in-wireless , https://www.shure.com/en-gb/insights/two-most-wanted-enemies-of-wireless-avoid-at-all-cost , https://filmtvsound.com/index.php/microphones/wireless/123-rigging-wireless-microphones?showall=1 , https://local695.com/magazine/radio-mike-redux
