# SEQUENCER + METRO TEST PLAN, 2026-10-05 (Agent C, change set 2)

Nothing here has been flashed or heard. Build on branch `clock-agent-2026-10-05`. Backup of the tree before this change: `seed/PMOR_V3_backups/2_preSequencer_2026-10-05/`. Flash the usual way (`flash.sh`, buttonless DFU is untouched).

## Controls (new, final)
| Where | Gesture | Does |
|---|---|---|
| DRUMS / SYNTH / SAMPLER, ring level, SEQ card selected | joystick UP or DOWN (one flick) | enter step edit |
| in step edit | joystick UP or DOWN | leave step edit |
| in step edit | joystick LEFT / RIGHT | previous / next page (page = one bar), stops at the ends |
| in step edit | pads | toggle that step for the target lane |
| in step edit | joystick CLICK | next lane (pad), auditioned on drums/sampler |
| anywhere on the engine pages | tap a pad | plays it AND makes it the target lane |
| in step edit | encoder turn | back to the ring (same as every other level) |
Encoder press = tap tempo, unchanged. Old "hold a pad 500 ms" place mode and the synth HANDSEQ list item are gone.

## A. Hand-sequence a drum pattern (the main test, ~2 min)
1. DRUMS menu. Flick joystick RIGHT once: card = SEQ. Screen shows `UP/DN = STEP EDIT`.
2. Tap the bottom-left quadrant pad (a kick) so it is the target lane. You hear it.
3. Flick joystick UP. OLED = `DRUMS PAD n`, `PAGE 1/1  MAN`, 16 boxes. Pads show the kick colour dim.
4. Press pads 0, 4, 8, 12 (a four-on-the-floor, one hit per 4 steps). Each lit step should light its pad.
   EXPECT: after the first step the OLED mode flips `MAN` -> `PLAY` and the pattern starts playing on the grid (this was the main breakage: MANUAL never played a placed step).
5. Press JOYSTICK CLICK: lane advances to the next pad, you hear it, the pads re-draw for that lane. Place a backbeat on 4 and 12 on a snare-quadrant pad. Click until you land on it, or tap it before entering.
6. Flick joystick DOWN: back to the ring (SEQ card). Pattern keeps playing. Flick UP again: the pattern is still there.
7. PASS = you built a kick+snare loop in under two minutes with no menu diving.

## B. Page navigation
1. From the ring on the SEQ card, press the joystick to open the list, set BARS to 4 (BARS item, click, pick 4 on the pads), turn the encoder to back out to the ring.
2. Flick UP to enter step edit. `PAGE 1/4`. Flick RIGHT: `PAGE 2/4`, pads now show bar 2. RIGHT x3: stops at 4/4 (no wrap). LEFT back down: stops at 1/4.
3. Place a hit on page 3 only; confirm it plays only in bar 3 while the loop runs. The playhead underline on the OLED and the white wash on the pad show only on the page the playhead is in.

## C. Edit toggle behaves (held stick must not flap)
1. On the SEQ card ring, push the joystick UP and HOLD it ~2 s. EXPECT: enters step edit once and STAYS in. (Before: auto-repeat would toggle at 8 Hz.)
2. Release, push DOWN and hold. EXPECT: leaves once, stays out.
3. Inside the SEQ LIST (joystick click from the ring), UP/DOWN must still move the list cursor (the toggle only acts at the ring).

## D. Synth and sampler (same editor, same gestures)
1. SYNTH menu, SEQ card, tap a pad (note), flick UP, place steps. Lane = that note. Click advances the note (silent, no audition on synth). Mode flips to PLAY and the note sequence plays.
2. SAMPLER menu with a recorded slot: tap the slot pad, flick UP, place steps. Click advances and plays the next slot if it has audio.
3. CONFIRM (open question): Emory, is "one lane at a time, pads = steps" right for the synth, or do you want the old chord-per-step editor back?

## E. Regressions to watch (hardware-quirks.md)
- Pad panning stays swapped (pan L/R mirrored vs pad layout): hit pads left vs right, confirm unchanged.
- Trellis dropped events: spam steps fast; a missed press just means that step is not toggled. Stuck pads must not occur after leaving edit mid-press (8 s sweep still there).
- Pads held while in step edit must NOT trigger GEN rolls or the arp (they belong to nobody in the editor).
- Buttonless DFU: `printf 'dfu\r\n' > /dev/cu.usbmodem*` still reboots to DFU.
- Patch autosave: place steps, wait ~3 s (star goes hollow -> solid), power cycle, steps survive. (Step edits and CLEAR/BARS/STEPS now mark the patch dirty; they did not before.)

## F. Play-mode metronome
Arms when a lane's MODE is toggled to PLAY (MODE editor, or the first step placed in step edit). Runs while any lane is still in PLAY. Click per beat (every 4th 16th), higher pitch on the bar start.
1. Start with all three engines MANUAL (set MODE on each if needed). Nothing clicks.
2. DRUMS > SEQ list > MODE > pick PLAY. EXPECT: within one beat a small soft tick starts, on the grid, accented (higher, slightly louder) on beat 1.
3. Compare to a drum hit: the tick should sit ~20 dB under the quietest kick and feel like a gentle count, not a drum voice. Measured on the host: beat peak -26.4 dBFS, downbeat -22.8 dBFS (before the master stage), tail < -60 dB after 32 ms.
4. Tempo: tap in ~70 BPM, then ~120, then ~170 (encoder press). The tick follows each, locked to the hat/kick grid, no drift against a sequenced hit. At 170 it must still be a clean tick, not a smear.
5. GLOBALS > METRO: turn it up and down (default 25%). 0% = silent. (Not saved across reboot: it comes back at 25%.)
6. Set every engine's MODE back to MANUAL/GEN: the tick stops. Toggle PLAY again: it restarts.
7. Slaved to the Pi (clock_mode S): the tick must stay locked to the received tempo (same grid as the sequencer).
8. NOTE the tick is on the master bus, so it ALSO goes out the USB capture to the Pi. If that is unwanted, say so (it is a one-line move to speakers only).

## Known / not covered
- Page = one bar only (STEPS per bar up to 16 = the 16 pads). No sub-16 paging.
- Lane picking inside the editor is click = next only (no previous). Tap a pad outside the editor to jump straight to a lane.
- Synth is one note lane at a time, not the old chord-armed editor.
- The OLED metro DOT (top-left) from the earlier overhaul is unrelated and unchanged; it shows on every page whether or not PLAY is on.
