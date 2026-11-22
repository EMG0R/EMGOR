# Bell mic chain (bench-verified 2026-11-16)

Dynamic clip-on bell mic -> resistor pad -> MAX4466 module (MusRock 3-pack, external-mic pads) -> Seed AUDIO IN L. Dry mono through the firmware, duplicated to out L and R, then the Output gain (saved on the Seed).

## Wiring
- **330 ohm across the preamp mic input (MIC+ to MIC-)**, no series resistor. Mic red -> MIC+, mic black + shield -> MIC-.
- 330 ohm shunt was found on the bench by Emory and is the right value for this mic + trumpet. My earlier suggestions (10 k series pads, 4.7 k, 750 ohm) were wrong.
- MAX4466 VCC -> 3V3A, GND -> AGND, OUT -> AUDIO IN L.
- Why a preamp at all: direct mic -> line in was ~70 dB below the preamp path on this Seed 3; the MAX4466's minimum gain (~+28 dB) clipped without the pad.
- AGND must be tied to DGND (see GROUNDING_PLAN, 2026-11-16), or the input is dead.

## Noise
- Slight hiss at high preamp gain. Best signal-to-noise: as little pad as works plus the lowest preamp gain that reaches the target (loud playing peaks -12..-6 dBFS on the page meter). A firmware noise gate on the dry path is the planned fix for silence between phrases.
