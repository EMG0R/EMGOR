# CLOCK TEST PLAN, 2026-10-05 (bench, by hand)

You hear it, you decide. Nothing here was flashed or restarted by the agent. Stop and tell me if any step surprises you.

## 0. Deploy (order matters)
1. **Pi daemon first** (it is backward compatible with old firmware). Files: `DEMIURGE_OS/src/demiurge-clock.cpp` + `src/demiurge-transport.h` + `src/wrappers/demiurge-sync`. Build on the Pi (flags from `docs/clock.md` "Rebuilding the daemon", unchanged). Install `demiurge-clock`; the launcher supervises it, so kill the old process and it respawns (unverified: if it does not, that is a service restart = audio, ask first).
2. **NEPTR bits** (csound restart needed = audio, your call): `neptr/csound/midi_handler.orc`, `handler/handler_osc.orc`, `neptr/neptrPhase4_UI.py`, `neptr/csound/serial_bridge.py` (restart the bridge service).
3. **PMOR firmware**: `seed/PMOR_V3_clockagent` branch `clock-agent-2026-10-05`. `make`, then your usual `dfu` + `make program-dfu`. Back up the current working image first (your convention).
4. Probe anytime over CDC: send `status` -> `mode=internal|slave|master bpm_x100=... wire=0|1`.

## 1. PMOR standalone (USB unplugged, battery) -- must work fully
- OLED top-right shows `120.0` + a box with **I**.
- Tap the encoder 4 times at your tempo: tempo shows live from tap 2, steady by tap 4, **grid lands on your taps** (the click triggers the note, as before). Tap a sloppy 5th: tempo barely moves.
- Edit BPM in GLOBALS to 95: it **stays** 95 (this used to snap back to the old bus tempo). Tap again: new tempo.
- 6 s of nothing, then play pads: still I, still your tempo.

## 2. PMOR slaved to NEPTR/Pi
- Plug in, Pi up. Within ~3 s the box flips to **S** and the number matches the Pi: `demiurge-sync` / web sync page / `clock_status.conf bpm_wire` must read the same value as the PMOR OLED, to 0.1.
- Clock out is muted in S (no double clock): `aseqdump -p "Midi Through"` shows 0xF8 from ONE source only.
- Tap the PMOR encoder 4x: the tempo readout **inverts for a blink per tap** (the Pi got it), the local grid does NOT jump, then within ~100 ms of tap 2 the number on **both** PMOR and the Pi changes to your tempo and PMOR's grid follows. `clock_status.conf` shows `last_tap_source = pmor`, `tap_count` rising.
- Unplug the USB: within ~1 s the box goes **I**; tap and BPM edit work again. Replug: back to S, adopts the Pi number.

## 3. NEPTR arcade tap
- Pi master, PMOR slaved (as 2). Tap the arcade button 4x at a different tempo: `last_tap_source = neptr`; NEPTR UTILITY page big number, PMOR OLED and `bpm_wire` all show the **same** new tempo.
- If PMOR was the master: 1 tap changes nothing; a tap **pair** takes master back (PMOR flips to S).
- Hold the arcade 1 s: blackout still toggles (one stray tap, harmless). Say if the companion hold gets annoying.

## 4. Loop tap-in feel (the one you can feel)
- PMOR, drums, SEQ mode PLAY, REC on, 120 BPM (or your tempo), 1 bar.
- Play a steady 16th hi-hat pad **a hair late and a hair early on purpose**, ~30 hits. Compare with the previous firmware if you can (`PMOR_V3/build` image): the old one drops hits onto the next/previous step when you play a little late; the new one should put every hit on the step you meant, as long as you are within half a 16th.
- Do it while slaved (S) too: recorded hits should line up with the Pi grid.
- Host measurement of this (so you know what to expect): at 20 / 30 / 45 ms event delay the old logic mis-stepped 8 % / 17 % / 34 % of hits, the new 0 %.

## 5. Both displays show the received tempo
- Change tempo on the Pi side (`demiurge-set`/steer, or tap on NEPTR). PMOR shows it as S; NEPTR UTILITY shows it; neither shows an invented 120. NEPTR with no clock yet shows `--.-`.

## What would make me wrong (tell me if you see it)
- PMOR stuck on S while the Pi is dead > 3 s (fallback broken).
- A slaved tap that does nothing (old daemon still running).
- Tempo flapping between two numbers (CC119 vs CC120/121 fighting: NEPTR csound not restarted with the new `midi_handler.orc`).
- Any extra 0xF8 source while in S.
