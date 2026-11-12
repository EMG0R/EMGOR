# DUODECIMUS show map (2026-11-01)

> The show bank (presets 1-17, bank + sequences + names) was cleared 2026-11-02. Backup of the full preset dir: `~/demiurge/presets/surf.bak-20261102-clear/` (also `presets/surf.bak-20261102-clear/`); text export `presets/show_bank_export_20261102_pre-clear.json`. The map below describes the cleared bank as it was; rebuild slots from the web app.

All ch7. Layout 2 rows x 6, numbered by column: top row 1,3,5,7,9,11; bottom row 2,4,6,8,10,12.
Wire: rotate = CC(19+N) relative (127 CW / 0 CCW); press = CC(101+N); hold 5 s = CC116 N; hold 8 s = CC117 N.
Decode: `csound/midi_handler.orc` ("DUODECIMUS SHOW LAYER"). Everything else on ch7 is dropped.

| enc | row | function | press | rotate |
|---|---|---|---|---|
| 1 | top | AMP | NAM on/off | step NAM model, flat 0..24, wraps (same row/variant state as the NAM menu) |
| 2 | bottom | CLEAN (slot 2) | recall | - |
| 3 | top | DRIVE (slot 3) OD drive 0.45 | recall / toggle | - |
| 4 | bottom | VIBRATO (slot 4) | recall / toggle | - |
| 5 | top | SWELL (slot 5) ATTACK 600 ms + reverb | recall / toggle | - |
| 6 | bottom | CHORUS (slot 6) | recall / toggle | - |
| 7 | top | VOCALS | voc_on toggle | voc_level +-0.05, clamp 0..2 |
| 8 | bottom | LESLIE (slot 8) 0.8 Hz, depth 0.7 | recall / toggle | - |
| 9 | top | SPECTRAL (slot 9) spectral delay mix .5 fb .6 + reverb | recall / toggle | - |
| 10 | bottom | TAPE DELAY (slot 10) mix 0.3, fb 0.4, 300 ms, heads 1+2 | recall / toggle | - |
| 11 | top | BLOOPER | on at max density (Scat 1) / off | Scat +-0.05 |
| 12 | bottom | AMBIENT REVERB (slot 12) | recall / toggle | - |

- Toggle: pressing the encoder of the active FX preset recalls CLEAN. Active slot = `gkDuoFx`.
- Holds capture/delete only slots 2,3,4,5,6,8,9,10,12; ignored on 1, 7, 11.
- Amp layer: NAM slots 272..280 + gkNAMProfileIdx are in the recall exclusion files, so FX recall never changes the amp. Master out/vol/BPM, vocal, mixer, blooper, glitch, habit also excluded.
- Rotation on FX encoders is unmapped.
- No loops (all looper channels idle/stopped): blooper is forced off on the active->none edge and once at engine start.
- Pedal: every show preset stores owner VOLUME (target 1, slot 304 = 2). Recall of slots 1..12 hands the pedal to the stored owner, sets gkVOLpedal to the pedal's current position (`gkExprPos`) and tells `expr_cc29.orc` (`gkExprHandoff`) not to disarm soft takeover, so the next pedal move drives volume with no jump. Onboard slots 13..17 keep the 2026-09-15 behaviour (owner not changed). Before: owner was excluded, and any owner change disarmed the pedal until it moved > 0.03, with volume sitting at the stored value.
- Bank authoring: `tools/author_show_presets.py` (source `presets/surf/export-20261101-before.json`; result `export-20261101-after.json`). Backup of the old bank: `~/demiurge/presets/surf.bak-20261101`.
- USB routing: no change needed. `demiurge-midi-connect.service` (reconcile loop, 3 s) wires every kernel-card ALSA client except "Teensy MIDI" and "Bela" into Midi Through 14:0; a client named DUODECIMUS is connected automatically on plug-in. The pool presence rule reads the same client name.

## Vocal fix
Cause: the gain stages stacked. voc_level 1.0 = 7x (+17 dB, 10-24 12:12) and the mic PREAMP +24 dB (10-24 13:37) were added the same day without removing each other: +41 dB total at defaults, on a persisted reverb wet of 1.0 (feedback 0.85). Persisted level read 0.0, i.e. the level fader had been pulled to the floor against it.
Now: pre +15 dB, level 1.0 (+17 dB) = +32 dB, dly wet 0.5, rev wet 0. Boot-time one-time migration (stamp = settings slot 343) rewrites the persisted values. Vocal output also feeds the looper input (`mixer_page.orc`, `gaLpIn += vocal`).


## Activation sequences and the web editor (2026-11-02)
A preset slot can carry an ordered activation sequence (effects, VOCAL on/off, NAM amp + gains) that recall replays as if pressed, so the last pedal claimer owns the pedal (VOLUME last in the show bank). The DUODECIMUS hold-capture now also records the order effects were switched on (no NAM/VOCAL step). Slots 1-12 and onboard 13-17 are edited from the phone at `:8091`. Details: `docs/neptr_web.md`.

## 2026-11-02 slot kinds (EMGOR)
- 1, 3, 5 = NAM-only presets, TOGGLES. Press = apply that slot's amp (model + in/out gain) and nothing else; press the active one again = amp off. Hold 5 s = save the current amp into it; hold 8 s = clear. A web/onboard recall or capture of these slots is routed to the same NAM-only path (state_persist.orc ops 4/5), never the full-state/sequence path.
- 7 = VOCAL toggle (press on/off, turn = level). 11 = BLOOPER toggle. Not presets.
- 2, 4, 6, 8, 9, 10, 12 = GUITAR presets, radio group: press = that preset; press the active one again = every guitar effect off. Never touch NAM or vocal.
- No encoder rotation on 1/3/5. Pick the amp on the NAM page (web or Pi), set in/out gain, then hold 1/3/5 to store it.
- Bank cleared 2026-11-02; backup ~/demiurge/presets/surf.bak-20261102-clear/.
