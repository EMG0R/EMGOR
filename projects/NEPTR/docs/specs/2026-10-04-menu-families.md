# Menu families — target layout (Emory's decisions, 2026-10-04)

Context: 20 flat menus on a 2-button ring is too many steps; only 5 slot buttons exist. Direction locked
in chat 2026-10-04. Mechanism: `menus.json` + drag page (`docs/menus.md`); phase 2 of that tool (slot
registry + generic toggle-by-id) is what makes these merges data edits instead of dispatch surgery.

## Decisions
- **NAM keeps its own menu.** Never merged into AMP/DRIVE. Its slot rotations stay profile browse.
- **IR VERB merges into SPACE** as ONE IR slot whose encoder scrolls the whole IR library (same browse
  model as NAM), next to DIGIVERB and AIR. Frees four slots.
- **One menu for hyperGuitar control** (exists today: conditional menu 20 HYPERGUITAR, OSC to the Bela;
  see `hyperguitar.orc`, `docs/…hyperguitar…`). Stays conditional on the guitar being present.
- **One menu for hyperTrumpet control** (NEW). hyperTrumpet is a Teensy USB-MIDI controller on
  **MIDI channel 3** (`______2026NEW/hyperTrumpet/hyperTrumpCode/hyperTrumpCode.ino`): sliders CC20-22,
  ToF 23, FSR 24, pots 28/29, joystick 30/31 (+btn 32), valves 33-35, encoders 36/37 (+btns 38/39),
  arcade 40, IMU 41-46, breath 47, contact mics 48/49, trellis keys CC50+. Its Max performance patch is
  `PERFRMNC.maxpat`. NEPTR menu = map those to NEPTR targets (expression registry ids / SM params) and
  show the sensor state; conditional on the device being present like HYPERGUITAR.

## Target ring (order to be set by Emory on the drag page, this is only the set)
| Family | Absorbs |
|---|---|
| AMP | DRIVE, PREAMP |
| NAM | NAM (own menu, by decision) |
| TONE | FILTERING, MOD |
| PITCH | PITCH |
| SPECTRAL | SOUNDMAGIC rack + legacy spectral (custom page, shipped phase 1) |
| TIME | DELAY, time-based CHASE BLISS |
| SPACE | DIGIVERB, AIR, IR VERB as one IR browser slot |
| TEXTURE | GRANULAR, GLITCH |
| LOOPER | LOOPER |
| PRESETS | stays ITS OWN MENU (Emory 2026-10-04): the 5 slot buttons = 5 presets, mimicking DUODECIMUS, for when DUODECIMUS is not plugged in. Never folded into RIG. |
| RIG | VOLUME, UTILITY, BASS/SYNTH modes |
| HYPERGUITAR | conditional, instrument control |
| HYPERTRUMPET | conditional, instrument control (new) |

## Hybrid foot + hand control (Emory 2026-10-04)
Foot-only mode is DUODECIMUS doing preset nav. Everything else is HYBRID: the touch screen is a first-class
way to do normal menu nav and FX nav (tap a menu name to jump, tap an effect tile to toggle/focus), not just a
display. Hands on the screen + feet on DUODECIMUS/pedal at the same time is the normal performing state.
Don't be shy about moving nav onto the screen; the 5 encoder buttons remain the eyes-closed path.

## Interaction rules to build alongside
1. Each family page pins 5 favourites to the hardware slot buttons; pin changed from the touch screen.
2. Hold prev/next menu button ≥400 ms = jump to the two most-used families.
3. Touch handles the sub-level inside a family (SPECTRAL page is the template).
