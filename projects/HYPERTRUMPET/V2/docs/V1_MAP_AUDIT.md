# v1 control map, audited from the Max patch (2026-11-17)

Sources (read-only): `______2026NEW/hyperTrumpet/PERFRMNC.maxpat`, `midiinHyperTrump.maxpat`, `hyperTrumpCode/hyperTrumpCode.ino`.

Method: every patchcord in both patches was traced by script: CC -> `s trX` in midiin; `r trX` -> effect subpatcher inlet in PERFRMNC; then the inlet -> the `attrui` attribute it drives inside that subpatcher. This supersedes the pad notes in V1_SCRAPE.md where they differ.

## Pad numbering
`.ino` line 69: `CC_TRELLIS_BASE = 50`. Line 236: key k sends CC 50+k on channel 3. midiin: CC 50 -> `trButt1` ... CC 65 -> `trButt16`.
Pad n = Trellis key n-1. Pad 1 = top-left, pad 16 = bottom-right (standard orientation, key = row*4 + col).

## Audio chain (adc~ -> dac~)
harmonizer (`p shiftr`, pfft~ gizmo) -> looper (`rnbo~ looper`) -> freeze -> drone -> blur -> flanger -> chorus -> MOOD -> Supermassive -> grain (`p mcCurdyGrain`, also Spectral Pitch Shift / autooct) -> delay (`p absDelay`) -> roar -> ringmod -> dac~.
`p autopan` has a control (lidar) but is not on the traced adc~ -> dac~ path. `p lil rando` has no inlets.

## Pads
| Pad | v1 function | Mechanism |
|---|---|---|
| 1 | none | `trButt1` has no receiver |
| 2 | none (dead) | `r trButt2` -> `p butt tog` whose outlet is unconnected |
| 3 | freeze CLEAR | live.toggle -> `p clearer` -> 2300 ms ramp -> freeze `Freeze factor` |
| 4 | freeze LET-IN (hold) | `== 0` -> live.toggle -> freeze in4 -> `*-1 +1` -> 250 ms `line~` input gate |
| 5 | ringmod mix on/off | live.toggle -> ringmod `mix` |
| 6 | none | no receiver |
| 7 | blur mix latch 0 <-> 127 | `p butt tog` (counter 3) -> live.dial -> blur mix |
| 8 | chorus mix latch | `p butt tog` -> live.toggle -> chorus `mix` |
| 9 | delay on | live.toggle -> delay `mix` + `delay` |
| 10 | delay smooth-mode latch | `p buttTOG` -> live.toggle -> delay `smoothmode` |
| 11 | delay preset A | `p buttTOG`: feedback 100, mix `line 800` to 127 |
| 12 | delay preset B | `p buttTOG`: feedback 30, mix `line 800` to 40, time 67; while on, FSR drives delay time (gate) |
| 13 | looper | `rnbo~ looper` in2 |
| 14 | looper | `rnbo~ looper` in3 |
| 15 | none | no receiver |
| 16 | none | no receiver |

## Continuous controls
| Control | v1 target |
|---|---|
| Slider 1 | drone mix |
| Slider 2 | grain mix |
| Slider 3 | Supermassive mix |
| Accel | Supermassive `DelayWarp` (scale 0..127 -> -15..142); `Feedback` fixed 90 |
| Pot 1 | roar `shaper_amount_1` |
| Pot 2 | none |
| FSR | ringmod `frequency` 0..800 Hz; delay time while pad 12 is on |
| Lidar (inverted) | flanger `mix`; autopan |
| Joystick X / Y | MOOD (transpose / bitdepth), only in joystick-button counter mode 1 |
| Encoder 1 +/-, button | harmonizer interval down/up, reset |
| Encoder 2 +/-, button | shiftr in5..7, unconnected inside |

## Open
- `p butt tog` counts press and release (counter 3, `==1`/`==3`); whether a toggle lands on press or release is not settled from the patch.
- Faders: current firmware has fader 1 = Supermassive, fader 2 = drone (user instruction 2026-11-16). v1 is slider 1 drone, 2 grain, 3 Supermassive. Pending the user's choice.
