# Body control map (measured 2026-10-22)

Ground truth: `docs/control_capture_partial_214416.json` (encoders),
`docs/control_capture_20261022-214545.json` (buttons). Physical gestures by Emory,
raw MIDI from both paths.

## Single source of truth
Canonical numbering = what the engine's `csound/handler/handler_remap.orc` expects per
physical POSITION = the tables in `TEENSYY.ino` (`ENC_CC`, `ENC_FLIP`, `BTN_CC`,
`ENCBTN_CC`) = the same four tables now in `csound/serial_bridge.py`.
The Teensy's USB-MIDI already emitted this numbering; the bridge assumed
`e_i -> CC 19+i` and did not apply the flips, so it was wrong for 6 of 7 encoders and
8 of 9 buttons and CW was 0 on three encoders. Engine and UI needed no change: the UI
reads USB-MIDI (arcade/joystick/sensors), which is now numerically identical to the
bridge. Engine consumes the bridge only (Teensy MIDI is disconnected from 14:0).
Change a number in one place only: the bridge tables (mirror to the firmware tables
if it is ever reflashed). Test: `tests/test_serial_bridge.py` replays the captures.

## Encoders (CW = 127, CCW = 0 on both paths)
| position | serial | role | rot CC | click CC | bridge before (rot/click, CW) |
|---|---|---|---|---|---|
| top left | e2 | E7 menu PREV click; rot = secondary param (-> logical CC20) | 26 | 76 | 21 / 71, 127 |
| top right | e4 | E5 menu NEXT click; rot = tertiary param (-> CC23) | 24 | 74 | 23 / 73, 127 |
| bottom 1 | e5 | E6 slot4 (leftmost): click toggle, rot dry/wet | 25 | 75 | 24 / 74, **0** |
| bottom 2 | e1 | E2 slot3 | 21 | 71 | 20 / 70, **0** |
| bottom 3 | e3 | E3 slot2 | 22 | 72 | 22 / 72, **0** |
| bottom 4 | e6 | E4 slot1 | 23 | 73 | 25 / 75, 127 |
| bottom 5 | e7 | E1 slot0 (rightmost) | 20 | 70 | 26 / 76, 127 |

Engine remap: PREV click 76->70, NEXT click 74->73, rot 26->20 (PREV), 24->23 (NEXT);
slot rot/click 20/70->24/74, 23/73->25/75, 22/72->26/76, 21/71 unchanged, 25/75->22/72.
MIXER page: A = PREV-encoder rotation (logical CC20), B = NEXT-encoder rotation (CC23).

## Buttons (press 127 / release 0)
| button | serial | CC (canonical = USB) | bridge before |
|---|---|---|---|
| ARCADE | b1 | 60 | 60 |
| UP | b6 | 61 | 66 (b7) |
| DOWN | b9 | 62 | 68 |
| LEFT | b8 | 63 | 67 |
| RIGHT | b7 | 64 | 66 |
| A | b2 | 65 | 61 |
| B | b5 | 66 | 64 |
| X | b4 | 67 | 63 |
| Y | b3 | 68 | 62 |

Neither the engine nor the UI consumes CC61-68 today (UI telemetry only; UI uses 60).

## TODO: RIGHT / UP
The capture recorded UP and RIGHT as the same CC (usb 64, bridge 66). Firmware table
says RIGHT=b7=64, UP=b6=61; the other 7 buttons match the firmware exactly, so the UP
step was most likely a mis-press that hit RIGHT. UP=61 is therefore UNMEASURED.
Re-capture UP and RIGHT (`tools/map_controls.py`) before trusting either.
