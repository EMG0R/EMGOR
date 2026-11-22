# NEPTR web (preset editor)

Phone-first (iPhone Safari, 390x844 portrait; home-screen capable) and laptop-OK. NEPTR-private; not part of the public Demiurge OS.

- URL: `http://<pi>:8091/` (LAN, tailnet). No auth: anyone on the network can edit presets and move the rig's controls.
- Code: `tools/neptr_web/` (`server.py`, `store.py`, `static/`), unit `neptr-web.service` (tracked copy; installed in `/etc/systemd/system`, User=gorpi7v5, Nice=10, CPUAffinity=0-2, no After= on demiurge/neptr-ui).
- Tabs: PRESETS (17 slots: DUODECIMUS 1-12 as 2 rows x 6 columns filled column-wise, ONBOARD 1-5; load / edit / save / rename / capture / copy / clear; drag-reorder with pointer events), GUITAR (every effect: live switch, `+` add to sequence, tap name for values), NAM (radio of the 25 models + in/out gain + amp bypass + "save into preset"), VOCAL (on/off + level 0-4).

## One bank
The web app and the device share `~/demiurge/presets/surf/` (symlink to `presets/surf`): `bank.ftab`/`bank.b.ftab` (512-double state rows, unchanged), new `seq.ftab`/`seq.b.ftab` (sequences), `names.json` (names; the engine never reads it). Web saves write both A/B of both pairs with generation = highest on disk + 100 (atomic rename), back up the previous files to `surf/web-backups/` (last 80), then send OSC `/bank_reload`. Device captures/deletes rewrite the same files.

## Three sections per slot (2026-11-02)
Every slot edits as GUITAR (type-1 steps, ordered), NAM (one type-3 step, or none = leave amp alone) and VOCAL (one type-2 step ON/OFF, or none = leave alone). On SAVE they are stored as steps of the same slot: NAM, then VOCAL, then GUITAR (NAM first so the last guitar claimer, VOLUME, still owns the pedal). Absent NAM/VOCAL step = that layer untouched on recall (PresetRecallSeq only acts on steps present). Note: a sequence slot always sweeps GUITAR effects off first.

Slot kinds (2026-11-02, DUODECIMUS show layer): 1/3/5 NAM-only (NAM step only), 7 VOCAL toggle and 11 BLOOPER toggle (not presets, tiles disabled), 2/4/6/8/9/10/12 GUITAR-only (steps type 1 only), 13..17 onboard full presets (NAM + VOCAL + GUITAR). The app only writes the sections its kind allows. Caveat: `PresetRecallSeq` always sweeps GUITAR off, so NAM-only slots must be recalled by the show layer, not the sequence path.

## Sequence format (`csound/preset_seq.orc`)
Per slot 16 steps x `[type, a, b, c]`, index `((slot-1)*16+step)*4+field`; flag `1100+slot-1` (1 = sequence authoritative, 0 = legacy slot); generation at 8730, stamp 771218 at 8735 (table size 8736 = same ftsave header as the bank).

| type | step | a | b | c |
|---|---|---|---|---|
| 1 | EFFECT ON | effect id (`presets/effects.json`) | | |
| 2 | VOCAL | 0/1 | | |
| 3 | NAM AMP | flat profile 0..24 | in dB | out dB |

## Recall
Slot with flag 1: unpack row (params), excluded layers restored (NAM, vocal, generative), then every non-generative GUITAR effect is switched off and the steps run in order. Effect steps use the hand toggle-ON branch (bypass 0, `gkLastToggle`, pedal claim, radio partners, `_expr` resets) so the last claimer owns the pedal. Knob re-defaults of hand toggles (ONWARD) are NOT applied: the preset owns its values. Generative effects (BLOOPER, IMPULSE, BBCUT, REPEAT, CHOP, RVRS) are left alone by the sweep; a step can still switch one on. A NAM step sets row/variant/profile, bypass 0 and both gains; no NAM step leaves the amp alone (slots 272..280 stay in `state_preset_exclude_*.inc.orc`). Slot without flag (legacy): row as captured, then VOLUME takes the pedal (previous behaviour).

## Effect ids
`tools/gen_effect_catalog.py` derives `presets/effects.json` and `csound/preset_apply.gen.orc` (`PresetApplyFx`, `PresetOffFx`, `PresetSweepOff`, `PresetScan`) from `handler/dispatch/toggle_slot*.orc`, param names/ranges from `rot_*.orc` `$ENC_ROT`, indices from `state_persist.orc`. Re-run after editing any toggle branch; `--check` reports staleness. Ids = engine toggle ids (VOLUME 1, IR rows 49-53, FREEZE 72, SoundMagic 75..93 = 75+fx).

## Device capture records order
`PresetScan` (every 8th k-cycle, and on capture) edge-detects every effect's on-state; ON pushes the id on a 31-entry ring, OFF drops it. Capture writes the newest 15 ring ids in order, then VOLUME last if on. No NAM or VOCAL step. Boot and legacy recalls rebuild the ring from state in id order (the true order before a restart is unknowable).

## OSC added (port 7770, in instr 97)
`/fx id on`, `/nam profile inDb outDb`, `/nam_byp 0|1`, `/bank_reload 1` (staged over 4 k-cycles; reads the A files). Recall/capture reuse `/cc 16 106|105 slot`; vocal uses `/cc 16 40` (on) and `/cc 16 51` (level = value/127*4).

## NAM persistence
Verified in `state_persist.orc`: slots 272-280 (active row, bypass, in, out, per-row variant) are change-scanned into `neptr_settings.ftab` (SD, `~/demiurge/state`) and restored at boot by `state_unpack.inc.orc`, which recomputes `gkNAMProfileIdx`. Web NAM writes go through the same globals.

## Limits
- Live values in the app (switch states, NAM, vocal level) are read from `neptr_settings.ftab`, up to ~2 s old.
- Parameter values are edited in the stored preset and heard on recall; there is no live per-parameter OSC.
- Until the engine is restarted on the new orchestra, `/fx`, `/nam`, `/bank_reload` are ignored, saves reach the engine only at next boot, and a device capture rewrites the bank from engine memory (overwriting web edits made since boot).
- Race: a device capture between a web write and the reload loses one side; the web's generation is +100, so after a reload the web file wins.
