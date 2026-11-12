# CLOCK STATE, 2026-10-05 (Agent C)

**PMOR <-> Pi link = USB-MIDI** (TinyUSB composite: UAC2 capture + USB-MIDI + CDC, proven on hardware 2026-08-11). Not serial. PMOR enumerates as `Pocket OpGorator`, streams 0xF8, announces CC116=1.
Branch `clock-agent-2026-10-05` (git worktree `seed/PMOR_V3_clockagent`; Emory's `PMOR_V3` untouched). Backup of the pre-change tree: `seed/PMOR_V3_backups/1_preClockAgent_2026-10-05/`. Nothing flashed.

## What already existed (and was kept, not duplicated)
- **CC116 role** (1 = PMOR master, 2 = Demiurge master) + boot listen + sounding/CC115 election + tap "human gesture" (`usb/clock_engine.c`). The bathtub `clock_mode internal/slave/master` is this same mechanism renamed.
- **`demiurge-clock`** (Pi): follower PLL over 24 ticks (0 ms-interval gate, freewheel), tempo-source ladder, CC118/119, `clock_status.conf`, `demiurge-sync`, web/TUI sync pages.
- PMOR clock engine: Q32.32 grid, MIDI clock out derived from the same grid, follower phase-lock.
- "Always wrong" is **not** the MIDI clock (measured 120.03-120.08 BPM). The ~3 % duplicate 0xF8 is below the app (TinyUSB/DWC2); the PLL already gates it. Not chased.

## What was actually wrong (found, not assumed)
| # | Finding | Where |
|---|---|---|
| 1 | CC120/121 "14-bit canonical tempo" was **documented, implemented nowhere** (daemon, PMOR, NEPTR all 7-bit CC119, 2.8 BPM/step). Followers displayed a PLL jitter average or a 7-bit rounding. | `docs/clock.md` vs code |
| 2 | NEPTR's big tempo number was `gkChaseBlissBPM` = **loop-implied tempo or the default 120**, never the received transport tempo. | `handler_osc.orc:15`, UI default 120.0 |
| 3 | PMOR tap timestamp = main-loop time, **after** the 8 ms debounce, with the loop stalling on OLED blits/SD/I2C. The grid snapped to that late stamp (error = the stall). | `main.cpp`/`input.cpp`/`clock.cpp` |
| 4 | Loop tap-in stamped the pad at **UI-event processing time** (pads polled every 20 ms, then queued), and `seqRecordHit` measured it against the *current* boundary, so a boundary fired in between put the hit on the wrong step. | `nav.cpp`, `sequencer.cpp` |
| 5 | **Standalone-breaking:** while role = DEMIURGE_MASTER (which silence puts you in) the main loop overwrote `clk.bpm` with the stale last-bus tempo every pass. A BPM edit could not stick with nothing plugged in. | `main.cpp` follower branch |
| 6 | NEPTR arcade button (ch1 CC60) had **no tap path at all**; Csound never reads CC60. | `TEENSYY.ino`, `serial_bridge.py` |
| 7 | A tap on a slaved PMOR stole master (CC116=1) instead of going to the Pi. | `clock_engine_human_gesture` |

## Delta (spec section 2 -> code)
| Spec | Done as |
|---|---|
| `clock_mode` internal/slave/master | `clock_engine_clock_mode()`: view over role + liveness. MASTER = role PMOR + USB host enumerated. SLAVE = role Demiurge + 0xF8 within 1 s. INTERNAL = everything else. Auto-switch to slave = the existing CC116=2 handshake. Fallback = 1 s (`BUS_SILENCE_SAMPLES`). Clock out muted in slave = existing role gate (no loopback). Shown on OLED (I/S/M) and in CDC `STATUS`. |
| Standalone keeps tap + clock | INTERNAL: local analysis, local snap, BPM editor live (finding 5 fixed). |
| Tap = timestamped event into the pool | PMOR slave: `ch16 CC112` (value = age ms, ISR-stamped). NEPTR: `ch16 CC113` from `serial_bridge.py`. Any pool source: OSC `/transport/tap ,if` -> UDP 9100. |
| Pi analyses once, broadcasts | `demiurge-clock` handles CC112/113/OSC with the **shared analyzer**, quantises to the wire grid, broadcasts CC119 + CC120/121 + `/transport/tempo` immediately. |
| Same tap code in PMOR | ONE header, `DEMIURGE_OS/src/demiurge-transport.h`, included by the daemon and by the firmware (`-I$(DEMIURGE_SRC)`). No copy. |
| ISR timestamp, quantise to master | Encoder click stamped in the audio ISR (`hw/controls.cpp`); pads stamped at the poll (-10 ms estimate); `seqRecordHit` rewritten to quantise a press of any age to the master grid. |
| Both displays show received tempo | PMOR `g.bpm_shown` (bus number when slaved). NEPTR `/clock_bpm` now = `gkGLITCH_bpm` (arbitrated, fed by CC120/121); UI shows `--.-` until the first value. |

## Known limits / not done
- JACK transport / BBT timebase not wired (transport = MIDI + OSC only). No sample-clock work (bathtub 1.2 says JACK owns it).
- NEPTR arcade tap is stamped Pi-side (+5 ms mid-frame guess). A Teensy-side timestamp needs a Teensy flash.
- Arcade b1 also does a 1 s blackout hold and companion hold/activate: a tap fires on every press; a hold produces one stray tap (a lone tap does nothing).
- Pad press time is an estimate (seesaw FIFO has no timestamps): half the 20 ms poll back. Hardware could narrow it further (seesaw INT pin) if wired.
- Deploy order: Pi daemon first (backward compatible), then firmware. A new PMOR slaved to an OLD daemon sends taps nobody analyses.
