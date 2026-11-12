# MIXER page (menu 22)

Added 2026-10-22. Four stereo channel strips that sum into the MAIN outs.
An encoder menu page in the normal cycle (after VOCAL), not a touch-only page:
nothing is hard-mapped, encoders reassign by selection. Not heard yet: compile
checked offline only (no audio run).

## Channels (input pairs on the 8x8 HAT)

| row / slot | channel | inputs | volume (primary) | A: CC20 "previous" enc | B: CC23 "next" enc |
|---|---|---|---|---|---|
| row0 / slot4 | GUITAR | 1,2 (rack output, post-FX) | `mx_gtr_vol` | pan `mx_gtr_pan` | width `mx_gtr_wid` |
| row1 / slot3 | TRUMPET | 3,4 (reserved) | `mx_tpt_vol` | pan | width |
| row2 / slot2 | VOCAL | 5,6 (vocal chain) | `voc_level` | delay wet `voc_dly_wet` | reverb wet `voc_rev_wet` |
| row3 / slot1 | PMOR | 7,8 (groovebox) | `mx_pmr_vol` | pan | width |

Slot 0 (rightmost encoder) is unused (explicit no-op so the focus-keyed chain
never fires on this page). Rows are the same slot order as every menu
(`slot_button_cc`: row0 = slot4).

## Controls

- Rotate a channel's encoder: that channel's volume. 0..1.5 linear, 0.025 per
  detent, 1.0 = unity.
- Click a channel's encoder (or tap its row): selects it (`mx_sel`, echoed
  `/mx_sel`). Screen: selected row filled, bottom panel shows its A and B.
- Previous encoder (E7 rotation, CC20) = A of the selected channel; next encoder
  (E5 rotation, CC23) = B. Pan -1..1 step 0.05 (balance: only attenuates the far
  side, centre adds no gain); width 0..1 step 0.05 (M/S, 1 = untouched, 0 = mono);
  wets 0..1 step 0.02.
- Why pan + width for guitar/trumpet/PMOR: they are stereo pairs, pan/width are
  the two useful non-level controls with no existing home. Nothing is above unity.
- Defaults: guitar 1.0, vocal 1.0 (voc_level), PMOR 1.0, trumpet 0. Pan centre,
  width 1. No AGC.

## Audio

`MixerPage` UDO (`csound/mixer_page.orc`, MIXER builds), called in `signal_chain.orc`
after the last effect and before MASTER OUT: guitar strip on the rack output, plus
trumpet (`inch 3,4`), vocal chain (`inch 5,6` through `VocalChain`), PMOR
(`inch 7,8`), all summed into aPrevL/R, so they leave on the main outs under
MASTER OUT. Vocal is still also put on direct buses 5,6 (outs 5,6) as before.
Strips use portk (0.03) smoothing.

**Routing: no change.** `~/.config/neptr/routing` stays main_l 1, main_r 2,
rack_l 1, rack_r 2; no MIXER_DIRECT is needed because the strips are summed
in-engine, not through the matrix cells. Pool gains on inputs 3..8 stay unity.
The `VocalChain` call moved from the OUTPUT block into `MixerPage` (same
function, now also feeding main).

## State

chn_k channels (chnget/chnset, like `voc_*`). `giNeptrState` slots 331..339 =
`mx_gtr_vol, mx_gtr_pan, mx_gtr_wid, mx_tpt_*, mx_pmr_*`, stored +100 (0 = absent).
Appended, no renumber; unpack now covers 0..339. Preset recall excluded
(rig-level). OSC echo: `/mx_sel`, `/mx_<gtr|tpt|pmr>_<vol|pan|wid>`; vocal values
use the existing `/voc_level`, `/voc_dly_wet`, `/voc_rev_wet`.

## VOCAL page decision

One mechanism per job. VOCAL's level, delay wet and reverb wet are owned by
MIXER (they ARE `voc_level`/`voc_dly_wet`/`voc_rev_wet`, no copies). The VOCAL
page (menu 21, touch-only) keeps the rest of the chain: ON/DRY, HPF, mid EQ,
delay time/feedback, reverb feedback/cutoff; those three sliders are no longer
drawn there. Their ch16 CC47/50/51 decode and `VOC_PARAMS` entries stay (state
echo and the existing lockstep tests); only the rows were removed.

## Menu ids

MIXER = 22, HYPERGUITAR 22 -> 23 (`$HG_MENU`, `HG_MENU = MENU_COUNT = 23`).
`menus.json` order: ... NORRIS, VOCAL, MIXER, CHASE BLISS ...; `menu_cycle.orc`
regenerated with `tools/gen_menus.py`.
