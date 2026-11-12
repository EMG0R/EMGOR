# IR REVERB library — csound/ir/

Impulse responses for the IR REVERB menu (`csound/effects/ir_reverb.orc`).
Fetched + converted by `scripts/fetch_irs.sh` (re-runnable).

All files: **48 kHz / 24-bit / stereo WAV**, peak-normalized to **-6 dBFS**,
trimmed per type with a 50 ms fade-out. Naming follows the NAM convention
`<row+1><variant+1>_label.wav` so the UI can scan this directory for live
variant labels. Variants are ordered short/tight → long/washy within a row.
Defaults (variant 0 of each row): 11, 21, 31, 41, 51.

B-format (first-order ambisonic) sources were decoded to stereo as
mid/side: `L = W + 0.7071·Y`, `R = W − 0.7071·Y`.

## Sources & licenses

| Source | URL | License |
|---|---|---|
| OpenAIR (AudioLab, University of York) | https://www.openairlib.net / https://openair.hosted.york.ac.uk | **CC BY 4.0** — attribute "OpenAIR, AudioLab, University of York" |
| Voxengo IMreverbs (Aleksey Vaneev) | https://www.voxengo.com/impulses/ | Royalty-free for any purpose incl. commercial (pack `license.txt`). **Do not redistribute altered copies publicly** — these converted files are for this rig only. |
| Studio Nord Bremen analog reverbs | http://www.studio-nord.net (linked from http://www.grgr.de/IR/) | Offered as a free download by the studio; **no formal license text** on the site. Fine for personal/musical use; treat public redistribution as not cleared. |
| EchoThief (Dr. Chris Warren, SDSU) | https://www.echothief.com | Free download; **no explicit license text** (© Chris Warren). Fine for personal/musical use; redistribution not cleared. |

**Honesty note (springs):** no license-clean *Fender 6G15* spring-tank
captures were found from a reputable free source. The Studio Nord AKG
spring (a studio spring reverb unit, BX-style tank) is the best free real
spring available and is what row 0 uses — drippy and springy, but it is a
hi-fi studio spring, not an amp tank. (The old Fokke van Saane and
Dubbhism spring sets are dead links / unreachable hosts as of 2026-07.)

## Files

### Row 0 — SPRING (Studio Nord "Studio Nord Spring.zip", AKG spring tank)
| file | original | trim |
|---|---|---|
| 11_spring_short_flat.wav | Studio Nord Spring 1,5 flat.wav | 1.8 s |
| 12_spring_med_flat.wav | Studio Nord Spring 2,5 flat.wav | 2.0 s |
| 13_spring_long_flat.wav | Studio Nord Spring 3,5 flat.wav | 2.0 s |
| 14_spring_med_bright.wav | Studio Nord Spring 2,5 +treble.wav | 2.0 s |
| 15_spring_med_dark.wav | Studio Nord Spring 2,5 +bass.wav | 2.0 s |

### Row 1 — PLATE (Studio Nord EMT 140 plate + EMT 240 gold foil)
| file | original | trim |
|---|---|---|
| 21_plate_emt140_1s.wav | Studio Nord Plate 1sec.wav | 2.0 s |
| 22_plate_emt140_2s.wav | Studio Nord Plate 2sec.wav | 2.5 s |
| 23_plate_emt140_3s.wav | Studio Nord Plate 3sec.wav | 3.0 s |
| 24_plate_foil_short.wav | Studio Nord Foil 1,5sec flat.wav | 2.0 s |
| 25_plate_foil_long.wav | Studio Nord Foil 3sec flat.wav | 3.0 s |

### Row 2 — HALL
| file | original | source/license | trim |
|---|---|---|---|
| 31_hall_jack_lyons.wav | jack-lyons-concert-hall b-format/rir_jack_lyons_lp2_96k.wav (B-fmt decode) | OpenAIR CC BY 4.0 | 3.5 s |
| 32_hall_central_york.wav | central-hall-university-york b-format/ir_row_1l_sl_centre.wav (B-fmt decode) | OpenAIR CC BY 4.0 | 2.5 s |
| 33_hall_usina_arte.wav | usina-del-arte-symphony-hall stereo/usina_main_s1_p2.wav | OpenAIR CC BY 4.0 | 2.1 s |
| 34_hall_wide_echo.wav | Large Wide Echo Hall.wav | Voxengo IMreverbs | 3.0 s |
| 35_hall_musikverein.wav | Musikvereinsaal.wav | Voxengo IMreverbs | 3.0 s |

### Row 3 — CHAMBER
| file | original | source/license | trim |
|---|---|---|---|
| 41_chamber_york_guildhall.wav | york-guildhall-council-chamber b-format/councilchamber_s1_r1_ir_1_96000.wav (B-fmt decode) | OpenAIR CC BY 4.0 | 3.0 s |
| 42_chamber_mausoleum.wav | hamilton-mausoleum stereo/hm2_000_ortf_48k.wav | OpenAIR CC BY 4.0 | 4.0 s |
| 43_chamber_cathedral.wav | Underground/CathedralRoom.wav | EchoThief (free, no formal license) | 4.0 s |
| 44_chamber_silo.wav | In The Silo Revised.wav | Voxengo IMreverbs | 2.0 s |
| 45_chamber_columns.wav | Five Columns.wav | Voxengo IMreverbs | 2.5 s |

### Row 4 — WOOD ROOM
| file | original | source/license | trim |
|---|---|---|---|
| 51_wood_terrys_typing.wav | terrys-typing-room stereo/terrys_typing_ortf.wav | OpenAIR CC BY 4.0 | 2.0 s |
| 52_wood_genesis6_live.wav | genesis-6-studio examples/average_space_ir_0.wav (B-fmt decode) | OpenAIR CC BY 4.0 | 1.5 s |
| 53_wood_spokane_club.wav | spokane-womans-club stereo/spokane_womans_club_ir.wav | OpenAIR CC BY 4.0 | 2.0 s |
| 54_wood_rare_books.wav | Miscellaneous/WangenheimRareBooksRoom.wav | EchoThief (free, no formal license) | 1.5 s |
| 55_wood_drum_room.wav | Nice Drum Room.wav | Voxengo IMreverbs | 1.5 s |

## Verification

`scripts/bench/analyze_ir.py` (drives `scripts/bench/ir_bench.csd`)
renders an impulse through instr 240 for all 25 IRs and asserts: no
NaN/clipping, sane RMS, and render tail == source tail within type range.
Status 2026-07-13: **ALL PASS (25 IRs)**.
