# Vocal chain (inputs 5,6)

Rebuilt 2026-10-22 from EMGOR's spec (PRIME_V5_SWAP 8.1). Rebuild from
description, not a restore of the pre-crash chain. Levels and tone are his by ear.

## Signal flow

    in 5,6 -> HPF (12 dB/oct) -> peaking EQ -> slow delay -> reverbsc -> voc_level -> out 5,6

- Always on at engine start; no preset recall needed. `VocalChain` UDO in
  `csound/effects/vocal_chain.orc`, called inline from instr 1 (`signal_chain.orc`)
  in MIXER (8ch) builds only, when nchnls_i and nchnls are >= 6.
- Output is added to the mixer direct buses `gaMxDir[4]`/`[5]`, so it leaves on
  outs 5,6 through `MixOut` (output gain/mute still apply). Reads raw `inch 5/6`;
  the guitar/looper path is untouched.
- Delay and reverb are dry-at-unity plus wet send/return. Delay is free-running
  (not tempo-synced).
- Not present, by decision: tube preamp, vocoder front end, multiband VoxEq, AGC
  or any added input gain.

## Channels (chn_k, mode 3 in/out; chnset/chnget)

Channel limits = the VOCAL menu ranges. Values are clamped inside the UDO and
smoothed with portk. No default above unity.

| channel | range | default | UI curve |
|---|---|---|---|
| voc_on | 0/1 (1 = processed, 0 = dry mic) | 1 | button |
| voc_hpf_hz | 40..300 | 80 | lin |
| voc_mid_db | -12..+12 | -4 | lin |
| voc_mid_hz | 300..5000 | 1000 | exp |
| voc_mid_q | 0.3..4 | 1.2 | lin |
| voc_dly_ms | 50..1500 | 500 | lin |
| voc_dly_fb | 0..0.9 | 0.30 | lin |
| voc_dly_wet | 0..1 | 0.5 | lin |
| voc_rev_fb | 0..0.94 (clamped below 0.95) | 0.85 | lin |
| voc_rev_cut | 1000..16000 Hz | 10000 | exp |
| voc_rev_wet | 0..1 | 0 | lin |
| voc_level | 0..1.5 (1 = unity) | 1.0 | lin |

## Control path (added 2026-10-22)

- **Menu 21 VOCAL**: touch-only custom page (like UTILITY/NORRIS): ON/DRY button
  + 11 sliders. `menus.json` id 21, in the cycle after NORRIS; generated
  `handler/gen/menu_cycle.orc` via `csound/tools/gen_menus.py`. HYPERGUITAR
  moved 21 -> 22 (`$HG_MENU`, `HG_MENU = MENU_COUNT` in the UI).
- **Wire**: UI -> ch16 CC40..51 (40 = voc_on >=64, 41..51 = params in table order,
  0..127 across the range; mid_hz and rev_cut exponential). Decoded in
  `midi_handler.orc` (after CC104) into the chn_k channels. UI table:
  `VOC_PARAMS` in `neptrPhase4_UI.py` (must mirror the engine block).
- **Echo**: `handler_osc.orc` sends `/voc_on`, `/voc_hpf_hz` ... `/voc_level`
  on change and on the CC102 sync pulse; `route_osc` drives the sliders
  (drag-guarded). Anything that can `chnset` still works.
- **Persistence**: `giNeptrState` slots 319..330 = voc_on, hpf, mid_db, mid_hz,
  mid_q, dly_ms, dly_fb, dly_wet, rev_fb, rev_cut, rev_wet, level; stored
  **+100** (0 = absent, so older files/presets are skipped). Write side in
  `state_persist.orc`, restore in `state_unpack.inc.orc` (now slots 0..330).
  The layout stamp is unchanged (append only, no renumber).
- **Presets**: slots 319..330 are in `state_preset_exclude_save/restore` —
  a preset recall never retunes the voice (rig-level state).

## MIXER ownership (2026-10-22)

`voc_level`, `voc_dly_wet`, `voc_rev_wet` are driven from the MIXER page (menu 22,
encoders; see `mixer_page.md`) and are no longer drawn on this page. The chain now
runs inside `MixerPage`, which also sums it into the main outs.

## Not done

- Not heard. Compile checked offline (`csound --syntax-check-only`, no audio);
  needs EMGOR's ears. Hardware encoders have no VOCAL branches (touch-only).

## 2026-11-01 changes
Defaults: preamp +15 dB (was +24), dly wet 0.5, rev wet 0; one-time persisted-value migration. The vocal strip is added to the looper input (`gaLpIn`), so loops record the voice. See duodecimus_show_map.md.
