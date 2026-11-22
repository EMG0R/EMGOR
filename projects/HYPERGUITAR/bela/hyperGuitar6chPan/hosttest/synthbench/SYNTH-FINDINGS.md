# Synth rework findings (2026-10-01, host only, not deployed, not committed)

Files: `synth.cpp` (rewritten engine + timbres), `render.cpp` (SYNTH_NAMES table + gate-knob comment only; gSynthGateThr default unchanged 0.010).
Bench: `hosttest/synthbench` (`make && ./synthbench`, `./synthbench centroid`). The tracker is EMULATED (scripted env / attackBlank / pitchConf / dbgGate / releaseBlank), so these numbers prove the engine logic, not the real tracker on the SubSix.

## Engine (poly + mono share synStep/synHop)
- Hop detection = `v->hopCountdown == HOP` (render.cpp resets it right before runSynthPoly).
- Note-on: pick pending until attackBlank==0, pitchConf>=0.90, dbgGate not 1/2, releaseBlank==0, env>2*floor, env>0.35*gOtherMax, period stable within 3%, for 2 consecutive hops (accept hop + 1). Pending expires after 40 hops.
- Velocity: peak env over 15 ms after the pick / per-string reference (init 0.08, floor 0.008, ~60 s decay, raised by louder picks) ^0.7, min 0.10. Amp = 0.6*vel (SYN_AMP_MAX), held.
- Note-off: env < max(floor, peak*0.01 [-40 dB]) 4 hops; OR damped 2 hops (tracker releaseBlank/dbgGate 1, or own envS-based detector: envS < 75% of 4 hops ago, armed 16 hops after note-on); OR dbgGate 2 for 6 hops. Release ramp 90 ms; downward re-level of a sounding note on a softer re-pick 20 ms.
- floor = clamp(gSynthGateThr*0.15, 0.0008, 0.006): AUX-click points 0.004/0.010/0.025 -> 0.0008 / 0.0015 (default) / 0.00375. Old saved values keep working.
- Legato: >=0.7 st with 3 hops agreeing within 0.5 st while held -> pitch snap, same velocity, no new attack. Sub-threshold changes follow as a smooth bend (pRef follows 0.5/hop, osc glides 0.02/sample). Pitch frozen while releaseBlank>0 / dbgGate==1 / gate closed. 1-2 hop glitches never move pitch.
- Pressure follower: implemented, SYN_PRESSURE_AMT = 0 (off).
- Velocity->brightness SYN_VEL_BRIGHT = 0.3 (brt = 1 - 0.3*(1-vel)); affects MOOG, ACID, SYNC, FM only (others have no filter env to scale).
- Mono: same engine on gLeadStr; a lead hand-over while sounding carries the note. NOTE: render.cpp's lead selection still uses tonalHops>=SYNTH_TONAL_HOPS (14) to pick a new lead (render.cpp not in scope) - mono lead switching is still slow; also gLeadIncTgt is now ignored.

## Measured (emulated tracker, accept = attackBlank clears)
| Test | Result |
|---|---|
| Note-on latency after acceptance | 1.73 hops (5.0 ms); design = 2 hops (5.8 ms) max. Old: tonalHops>=14 (40.6 ms) after acceptance |
| Hard pick (A0 0.30) vs soft (0.06 after a 0.30 pick, same string) | vel 1.00 / 0.33, amp 0.600 / 0.199 |
| Soft pick on a fresh session (0.04, ref init 0.08) | vel 0.62 (reference adapts; first soft pick reads fairly loud) |
| 6 s ring 0.05 -> 0.004 | note 8.3 s (string > 0.004 for 6.0 s); ends at floor 0.0015. Old synth died ~3 s |
| Hard 0.30 ring -> 0.0015 over 6 s | note 5.2 s (ends at -40 dB of its own peak, 0.003) vs string > 0.0015 for 6.0 s |
| Fret release at 2 s, tracker releaseBlank raised | 1 note-on, off ~10 ms after the damp, pitch frozen, no retrigger |
| Fret release, tracker flag NOT raised (own detector) | 1 note-on, off 30 ms after the damp, pitch frozen |
| Hammer-on +2 st at 1 s (no onset) | 1 note-on (no retrigger), same velocity, pitch +2.00 st |
| 2-hop wrong confident pitch (+3.2 st) | pitch unchanged (0.000 st), no retrigger |
| Env 0.05 under 0.20 neighbour (bleed guard) | 0 notes |
| Bleed only, dbgGate 2, with a false onset | 0 notes |

IMPORTANT tracker observation: `releaseBlank` can essentially never fire from the current `v->env` - env is a 0.9995/sample peak-hold (tau ~45 ms) so it cannot fall to <50% in one hop or <35% in 4 hops. The synth therefore carries its own damp detector on `v->sh.envS` (pitch-scaled mean|x| follower). Untested on real strings: SYN_DAMP_RATIO 0.75 / 16-hop arming may need a nudge.

## Timbre changes (all steady, no attack-then-settle)
MOOG: filter-env snap removed -> fixed opening (x brt). ACID: sweep removed -> fixed position (x brt). SYNC: decaying ratio sweep -> fixed 2.65 + 0.25*LFO. FM GROWL: decaying index 6->1 -> constant 3.5*brt. WAVEFOLDER/PWM/vowel/drift LFO movement kept. Gain matching of the changed modes not re-voiced by ear (static settings are mid-points of the old sweeps).

## Mode order by steady-state centroid (196 Hz, 1 s, vel 1.0; vel 0.5 differs only for MOOG 731, ACID 751, FM 1075, SYNC 6596)
| new | name | centroid Hz | old idx |
|---|---|---|---|
| 0 | DRIFT PAD | 223.8 | 14 |
| 1 | GLASS | 250.8 | 11 |
| 2 | RINGMOD BELL | 287.7 | 4 |
| 3 | SUB GROWL | 288.5 | 8 |
| 4 | SOFT SQUARE | 473.5 | 12 |
| 5 | WARM TRI | 518.0 | 10 |
| 6 | WAVEFOLDER | 525.6 | 3 |
| 7 | MOOG LEAD | 828.4 | 1 |
| 8 | ACID SQUELCH | 875.4 | 2 |
| 9 | FM GROWL | 1035.2 | 6 |
| 10 | FORMANT VOWEL | 1254.2 | 9 |
| 11 | SUPERSAW | 1730.4 | 13 |
| 12 | CHIP LEAD | 2932.6 | 7 |
| 13 | SAW LEAD | 3766.8 | 0 |
| 14 | SYNC LEAD | 6721.1 | 5 |
(Raw oscillator output, before the mono-only 0.55 tilt lowpass. RINGMOD/SUB are a tie at 288.)

## Index references outside my scope that MUST be updated
- `presets.cpp:65` `{ PAD_SINE, 10.0f, 1.00f, "SYNTHESIS" }` (WARM TRI) -> `5.0f`; comment at presets.cpp:53 "default mode 10 WARM TRI" -> mode 5.
- `hg_lanes.cpp` `gSynthEntries[]` p1 values (names stay): SAW LEAD 0->13, MOOG LEAD 1->7, ACID SQUELCH 2->8, WAVEFOLDER 3->6, RINGMOD BELL 4->2, SYNC LEAD 5->14, FM GROWL 6->9, CHIP LEAD 7->12, SUB GROWL 8->3, FORMANT VOWEL 9->10. (Or reorder the list and keep the order dark->bright.)
- The saved preset/state file stores `synthMode` as an index (presets.cpp:544/606): a state saved before this change will boot a different timbre until re-saved.
- `hyperguitar.h` comment on SYNTH_NMODES ("10-14 = chill modes") is stale; SYNTH_TONAL_HOPS still used by render.cpp's mono lead picker.
- NEPTR-side UI (other repo) if it mirrors SYNTH_NAMES by index.

## Verification state
- synth.cpp: `-Wall -Wextra -std=c++11` clean. No allocation/syscalls (powf/logf/sinf per hop or per sample as before).
- `hosttest/run_tests` rebuilt; it does not link synth.cpp. It reports 1 FAILURE (formant_1.4_lpc, centroid 979->1073) - unrelated to my files; dsp_core/shifter/lpc/hyperguitar.h carry other agents' uncommitted edits.
## Unverified on device
Real tracker timing (acceptance/attackBlank, dbgGate behaviour), real env levels (SYN_VEL_REF_INIT 0.08 and SYN_AMP_MAX 0.6 are guesses: old amp was env*2 ~0.1-0.6 -> overall loudness will differ, retune SYN_AMP_MAX / gSynthLevel by ear), damp detector thresholds, bleed behaviour in a strum, mono lead hand-over, timbre loudness balance after the static changes, CPU (extra work is per hop only; 1 powf only while velocity window open).
