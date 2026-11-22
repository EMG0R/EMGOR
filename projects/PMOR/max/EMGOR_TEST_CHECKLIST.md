# PMOR STUDIO: bench checklist (Live + PMOR)

Before step 1: QUIT and relaunch Live. The rebuilt `pmor.proto.mxo` (sends the 9th status arg `role`) and the regenerated devices only load on a fresh Live start. Plug PMOR in first. Load: `PMOR core` on an instrument track; `PMOR lane drums` / `synth` / `sampler` / `control` on MIDI tracks whose input is PMOR's USB MIDI, filtered to ch1 / ch2 / ch3 / ch4+5.

1. Core: press CONNECT. Expect: status goes connecting -> ready STUDIO, no error. Port stays `auto`.
2. Set Live's tempo to 100, press play. Expect core line `run 100.0 | follow | ...` (role shows `follow`, not `master`) and PMOR's sequencer following Live. Stop: `stop ... | master` after ~2 s.
3. Synth VOL knob: synth lane perform view -> VOLUME (or the synth editor). Turn it down. Expect PMOR's synth gets quieter, then silent at 0. Turn the PMOR synth volume on the hardware: the editor knob follows.
4. Other volumes (drums MIX page: group VOLs + master; sampler MIX page; core editor GLOBAL: FADER, INGAIN, SWING): each changes PMOR's sound/behaviour, none jumps on open.
5. SEQ pages (drums editor SEQ, synth editor GEN, sampler editor SEQ): toggle REC, change MODE, PROB, DENSITY, BARS, STEPS; PMOR reflects each. Press PMOR's own buttons: the rows follow.
6. Knob sync both ways: drum editor (drums lane -> editor): move a pad PITCH in Live, hear/see it on PMOR; turn the same on PMOR, the Live knob moves. Moving a knob on PMOR must NOT make the Live knob bounce back or change other values.
7. Open an editor with PMOR idle: values shown equal PMOR's, nothing on PMOR changes (PMOR is source of truth).
8. Recording PMOR's notes: arm the drums, synth and sampler lane tracks, record, play PMOR's sequencer (or tap pads). Expect notes in each clip on the right track (drums ch1, synth ch2 + pitch bend, sampler ch3). Play the clips back with PMOR muted: same notes/timing.
9. Control lane: arm it, move FSR/joystick/IMU and tap FX buttons on PMOR. Expect CC data (ch4) and FX engage (ch5) recorded; seed CCs 110-113 present when a random FX fires. Playback reproduces the FX.
10. Sampler lane: trigger samples on PMOR, record; playback triggers the same slots.
11. Patch change: change patch on PMOR. Expect the editor values refresh to the new patch, samples pull with the progress bar (perform view), nothing is written back to PMOR.
12. Unplug PMOR USB. Expect status `STANDALONE (unplugged)` within ~7 s, PMOR speakers unmute, Live does not freeze.
13. Replug. Expect auto reconnect (~2-4 s) and a full resync from PMOR: knobs show PMOR's current values.
14. JOIN while playing: start PMOR's sequencer, then start Live transport mid-bar / from the middle of a bar. Expect PMOR joins on the beat, no drift, join line shows the latency.
15. Editor: click `editor` on each device. Expect its own window; closing it returns to perform (mode STUDIO). Editor shows everything (core: pool/presets, GLOBAL; lanes: pages, every param of the selected pad / algo / slot).
16. Look: dark green ground, light green text, blue/yellow/pink accents, dark teal panels; nothing clipped or overlapping in perform view or editors (lane editors 720 x 330, core editor 780 x 370, control lane 900 wide).
17. Debug: `/tmp/pmor_m4l_debug.log` must NOT grow during any step above.
18. Navigation: drums editor: pick pad 12 (KICK) then pad 14: SOUND names, LENGTH/PITCH/TONE/VOL and GROUP page depth labels change to that pad / group and show PMOR's values; nothing is sent by picking. Synth: change ALGO: K1..K4 labels + values follow the algo. Sampler: pick slot 3: PITCH/FORMANT/CUTOFF/REVERSE follow.
19. Edit persistence: change a drum SOUND + LENGTH on pad 12 in Live; on PMOR the pad sounds the same. Unplug, replug: the editor shows what PMOR has (the edit stayed on PMOR). Power-cycle PMOR if you want to confirm it was autosaved.
20. No push on load: with PMOR connected, load the drums lane into a track (or reopen the set): PMOR's sound does not change; no knob jumps. Edit something right after load (<3 s): ignored by design; after 3 s works.
21. Automation: in the drums lane perform view right-click "Drums Sel Pitch" -> Show automation; draw a ramp, play: PMOR's selected-pad pitch follows. Same for "Synth Cutoff".
22. Live tempo: change tempo while connected, press play: PMOR's tempo follows from the first tick (needs the firmware with TEMPO).
23. Next-firmware FX per-bank dials (control lane, right): disabled (`n/a`) on fw 2828e5d; on the next firmware they follow PMOR per bank.
24. Samples: assign pool sample X to sampler slot 3 (sampler editor), trigger slot 3 from the Live clip and on PMOR: same sound. Unplug/replug, change patch and back: the Live clip still plays what PMOR plays in each slot. If a slot is silent the core editor info line says why (CRC mismatch, re-pulling).
25. Delete safety: mark 1 sample, CLICK `HOLD 2 s: delete marked` once: nothing happens (message says hold 2 s). If the textbutton does not report press/release separately, the hold never arms and nothing can be deleted: tell macbert. Hold 2 s on a sample used by a slot: it only warns; hold again to delete. On firmware without the recycle bin: "no recycle bin yet", nothing deleted.
25b. Duplicate samples (same audio, several pool ids after migration): in the pool list identical audio is ONE row `S1100 x3  [S1100 slot 3; S1105 ?; S1109 ?]`. `slot n` = used by the CURRENT patch, `?` = unknown (the host only sees the current patch; nothing is ever shown as unused). Click a group row, press `copies`: one indented row per id. Mark the GROUP row, HOLD delete 2 s: only a message appears (`GROUP ...: delete <ids>, keep <ids>`, `Other patches not checked`) and nothing is sent. Hold again: ONLY the copies not used by the current patch go to the bin, at least one copy stays (all current-patch copies, else the lowest id). Before that, check on PMOR / in the other patches that the copies named in `delete` are really not used there (a slot using a deleted id goes silent; restore from the bin). If the slot map is not synced (all `?`, e.g. right after connect) the group delete refuses (`Nothing to delete`). Marking a copy under `copies` and holding keeps the old single-sample rules.

## Set <-> PMOR patch sync (2026-11-12; the pattr persistence is UNVERIFIED in Live)

26. First connect with a new `PMOR core`: SET SYNC line says `set took PMOR's patch N (M params)`. Save the Live set.
27. Reload the set (File > Open, not only reconnect): before CONNECT the line says `set holds patch N (M params), waiting for PMOR`. If it says `set: no copy yet`, the pattr did not persist: tell macbert (safe failure: first connect adopts PMOR).
28. Edit a drum / synth / sampler param in Live with PMOR UNPLUGGED, replug: line shows `writing the set to PMOR: .. left`, then `synced: N params written to PMOR patch N`; PMOR now has the Live value. No FX engage / global param (BPM, CLKLEAD) is written.
29. Change the patch on PMOR while the device is saved, reload the set, connect: line `PMOR is on patch A, the set holds patch B`, two buttons appear and NOTHING is written. `adopt PMOR's` = the set takes patch A. `HOLD 2 s: load on PMOR` = write the set's params onto PMOR's CURRENT patch A (the firmware has no patch-switch command; loading patch B by hand on PMOR also resyncs). A short click on the hold button does nothing. After either, the buttons vanish.

## Layout pass 2 (2026-11-12): EXPAND window, tabs, grid (restart Live first: new .amxd, new externals)

30. Each device shows its essentials and a big EXPAND button at the right. Click: the window opens (floats over Live), button reads MINIMIZE. Click again: window closes, button reads EXPAND. Close the window with its close box: the button must go back to EXPAND by itself (<= 1 s).
31. Resize the window and move it, close, reopen: same size and position. Save the set, reopen the set, EXPAND: same size and position (pattr in the set; UNVERIFIED in Live). If it opens at the default rect, tell macbert.
32. Drums window, PAD tab: 16 cells = PMOR's pads (top-left PERC, top-right HAT, bottom-left KICK, bottom-right SNARE), each shows its sound name; the selected cell is violet. Click a cell: LENGTH / PITCH / TONE / VOL / REV below show that pad; the chips on the right list that pad's group sounds, current one violet; click a chip: the pad's sound changes on PMOR and the cell text follows. Change a pad's sound on PMOR: the cell text follows.
33. Synth window, SOUND tab: 8 algorithm cards, current one violet; click one: PMOR's algo changes, OSC / AMP / FILTER tabs show that algorithm's knobs. Sampler window, SLOT tab: 4 x 4 slots, click selects, the dials follow; the slot selector in the strip follows too.
34. Tabs: every tab shows only its own controls (no leftovers from another tab). Core window: LINK tab mode / port menus still drive the session (change port to `a` and back to `auto` with CONNECT on; mode EDIT shows EDIT in the strip status). GLOBAL tab: CLKLEAD dial reads ms, `clock lead (ms): + plays earlier, to match Live's click`.
35. Hidden perform params still automate: in the drums strip, right-click a track's device parameter list: `Drums Sel Pitch`, `Drums Group Depth 1..4`, `Synth Attack`, `Sampler Sel Reverse`, FX bank params are still listed (they are not drawn in the strip any more). If any is missing, tell macbert.
36. Control window, METERS tab: with the window open, the meters follow FSR / lidar / accel / joystick / fader and the seed text updates; closed: nothing is drawn (CPU idle).

