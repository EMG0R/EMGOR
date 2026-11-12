# NEPTR looper "cuts me off while recording" - audit (read-only, 2026-10-22)

Base: ~/NEPTR_phase4 (sync/gorpi7v5). Dev branch sync/gorpi7v4-mpwork does NOT touch the record/close logic
(diff of looper.orc / handler_looper.orc / chase_bliss_clock.orc = GLITCH taps, PATH_NCH, OSC prefix only). No fix there.
No engine journal with LP lines exists on this board, so nothing is measured; mechanism is from code.

## Candidates, ranked

1. Close-tap rounds BACK to nearest grid line, discarding the tail (VERIFIED in code; fits "random": depends on where the tap lands)
   - csound/handler/handler_looper.orc LP_QCLOSE, ~l.34-57 (clocked first loop): kBeats=round(raw/beat); if raw >= target -> Len=target,
     RdPos=raw%target (state 2 immediately). Audio past target is unreachable. Loss up to 0.5 beat (0.75 s at 40 BPM).
   - same macro ~l.62-107 (ch2-5, or any channel once gkLpMasterLen>0): linear nearest of {M/4,M/2,M,2M,4M}.
     raw 1.49M -> M, raw 2.99M -> 2M: up to one third of the take dropped; short-ish takes (0.7M -> M/2) also cut.
     Looks like the looper ended the recording early; the player was still playing.
   - Early taps are safe (state 5 keeps recording to the boundary), so only ~half of closes truncate.
   - Note looper_ref/model.py (reference) rounds in log2 space; engine uses linear distance. Minor divergence.
2. Spurious / double press on the looper button (ASSUMED trigger; mechanism VERIFIED)
   - handler_looper.orc LP_CHn_PRESS: any press within 0.5 s of the previous press = CLEAR (state 0, recording ends, take lost);
     any other press in state 1 = LP_QCLOSE (early close). No engine-side debounce.
   - Touch UI is the button: neptrPhase4_UI.py ~l.2460 sends engine.press on every touch 'down' (companion_input.py:164).
     Resistive panel bounces; touch_evdev.py GapBridger only bridges up/down gaps < 0.07 s. A longer pressure dip = second press.
   - Stuck/lost release (companion_input.py cc() drops on port error) is harmless for state 1.
3. Buffer-full auto-close (VERIFIED, unlikely): effects/looper.orc instr 210 l.~152; buffers are 2304000 samples = 48 s
   (globals.orc:323; comment there says 16 s, stale). Ch1 closes silently and sets MasterLen unquantised. Only if take > 48 s.
4. Unclocked trailing-silence trim (VERIFIED, minor): state 6 drops tail below 0.001 FS (-60 dB); quiet decays/rests at the end are cut.
5. Ruled out (VERIFIED by grep): no timer, no MIDI 0xFC/stop handling, no preset/engine path writes gkLpState (only handler_looper,
   looper.orc, drumloops.orc guarded by busy checks, looper_persist.orc init-only). chase_bliss_clock only reads state. Clock does not close loops.
   A engine restart/xrun burst (bus log: dropouts) would lose a take but is not a "cut".

## Fix (fix.patch, handler_looper.orc only; passes git apply --check; NOT compiled - csound must not run)
TAIL PROTECT: a late close may round back over at most LP_TAIL_TOL (default 0.125) of the chosen length; beyond that it steps to the
next boundary and uses existing state 5 (keeps recording). Applies to clocked first loop (beats) and ch2-5 (next x2 candidate).
OWNER CHOICE: tolerance value (0 = never shorten, 0.5 = legacy). Cause 2 needs a design decision (ignore presses <0.25 s after REC start
vs keeping abort-by-double-tap); not patched.

## Bench verification (no restart needed to diagnose)
After a cut-off: `journalctl -u demiurge | grep -E 'LP[1-5]:'` with timestamps. "CLOSE" or "CLEAR" at the cut with no deliberate press = cause 2.
"CLOSE" then no "SYNCED" and a shorter loop than played = cause 1. "buffer full" = cause 3.
Deploying the patch needs an engine restart = Emory's permission.
