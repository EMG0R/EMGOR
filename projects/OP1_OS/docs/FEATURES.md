# op1-sdk feature map (R246, vendor/op1-sdk @ ae1945a)

Sources: each `features/<name>/feature.json` + README. "Status" is the manifest status; "Device evidence" is what the README records beyond it. Nothing here was re-run on a device by us except proof_v0 (key_probe, flash + boot OK).

## Replace vs add (the key fact)

No SDK feature removes a stock engine, FX or stock preset. Every new engine/FX is a NEW row in TE's list, implemented by hooking a stock "host" slot and gating on a marker stored in knob/fx_params slots 5-8. Stock sounds on the host (pulse, drwave, cwo, spring, phone) have no marker and run TE's code untouched. Costs of hosting:
- LOFI changes pulse's patch validator (range check). A LOFI sound must not be loaded by stock firmware (it halts at boot). Before reflashing stock, change every LOFI sound to another engine.
- SDK engines (WHEEL/WARP/TRIO/STATION 1/MONO 3) share drwave via enginehost; their presets load on stock as plain drwave.
- RESO/SHIMMER/FX MIX presets load on stock as cwo/spring/phone.
- TE caps factory non-sampler presets at 176 (stock 160); each engine adds 1, so 6 engines = 166, fits.
- engine_plus hooks the render of stock fm/digital/string/cluster/phase/dna/voltage/iter (0 = bit-exact stock; adds hidden params per knob click).

## Table

| Feature | Type | Manifest status | Device evidence (README) | Appears as | Access | Notes / conflicts |
|---|---|---|---|---|---|---|
| enginehost | library | device-verified | yes | none | none | required by WARP/TRIO/WHEEL/M1/MONO; hooks drwave render+note_on |
| WARP | synth engine | device-verified | yes | ADDED engine row "warp" + preset folder warp; host drwave tag 6 | engine list; shift+blue turn = bank (16); knob push = pages | none |
| TRIO | synth engine | device-verified | yes | ADDED "trio"; drwave tag 4 | engine list; shift+blue turn = bank (16) | none |
| WHEEL | synth engine | sim-verified | v0.1/0.4 device-tested; 0.7-0.9 not | ADDED "wheel"; drwave tag 5 | engine list; bank 24 | README: power-cycle with WHEEL current sound untested at 0.9; CPU with rotary+drive+heavy FX untested |
| LOFI | synth engine | sim-verified | v0.1 device-verified; boot-fix versions (0.3+) awaiting device test | ADDED "lofi"; host pulse via range-check hook | engine list; bank 14 | highest risk: stock firmware halts on a LOFI sound; hooks pulse validator |
| STATION 1 (m1) | synth engine | sim-verified | 2026-10-18 device: all presets sound | ADDED "station 1"; drwave tag 3 | engine list; bank 28 | loads 1.4 MB sample ROM from flash (not MIT, see features/m1/build/NOTICE); takes a buffer from TE sample heap at first use |
| MONO 3 | synth engine | sim-verified | first device test done (level fixed in 0.3); 0.5 not | ADDED "mono 3"; drwave tag 2 | engine list; bank 16 | 4 voices |
| engine_plus | engine layer | sim-verified | generic DRIVE/FILTER/RESO/CRUSH device-verified; hidden params sim only | MODIFIES stock fm, digital, string, cluster, phase, dna, voltage, iter (extra params, 0 = stock) | click a knob on those engines' screens | ~4-5% CPU for 6 voices; takes plain knob clicks on stock engines |
| RESO | FX | sim-verified | none ("awaiting device test") | ADDED fx row "reso"; host cwo | FX list (synth/drum/master) | ~3-4% CPU |
| SHIMMER | FX | sim-verified | none | ADDED "shimmer"; host spring | FX list | ~6% CPU, 2x42 KB RAM |
| FX MIX | FX | sim-verified | none | ADDED "fx mix"; host phone | FX list; TYPE on blue knob page 1 | ~1-3.5% CPU; 16-row FX list unseen on device |
| CHORD+ | performance | device-verified | yes ("chord+ is nice") | overlay panel, no slot | shift+insert (insert again pins) | claims shift+insert: conflicts with quantize (excluded) |
| DRUM ROLL | performance | sim-verified | v1.0/1.1 device-tested, 2.1 not | overlay on drum finger sequencer | shift+blue press on finger sequencer screen (drums) | patterns kept in op1.db; hooks drum sampler render + finger player; swaps TE's step reader |
| NOTE ROLL | performance | sim-verified | none ("awaiting device test") | overlay on synth finger sequencer | shift+blue press on finger sequencer (synth) | hooks finger player call site |
| SYNTH ROLL (requantize) | performance | sim-verified | 3.1 device-tested "very nice", 4.3 not | overlay | shift+blue press on tape screen (not in drum mode) | depends quantcore |
| quantize | performance | sim-verified | n/a | live quantizer | shift+insert | CONFLICTS with CHORD+; removed from SDK image by author; excluded |
| drumtrim | content | sim-verified | none ("no device run yet") | changes 14 factory drum kits (silent tails cut, -1.43 MB) | none | built from our stock file |
| noalbum / diskinfo / meminfo | debug / disk | experimental | none | patches FAT geometry / panels | | not used |
| fxdiag, midi_filter | debug / example | experimental / sim | | | | not used |
| key_probe | debug | device-verified | proof_v0 | | | excluded by request |

## v1 (recipes/v1.json): device-verified, touches no stock engine

CHORD+, WARP, TRIO (+ enginehost, pulled in as dependency). All three are marked device-verified in the manifest. Nothing stock is replaced or lost: only the drwave host is hooked, and non-tagged drwave sounds run TE's code.

Excluded from v1: the rest, because they are sim-verified or earlier-version-only. engine_plus excluded despite generic stages being device-verified since it hooks the render of 8 stock engines with sim-only internals. quantize conflicts with CHORD+. key_probe by request.

## v1x (recipes/v1x.json): everything in op1plus, UNVERIFIED as a whole

CHORD+, WARP, TRIO, LOFI, WHEEL, STATION 1, MONO 3, RESO, SHIMMER, FX MIX, DRUM ROLL, NOTE ROLL, SYNTH ROLL, engine_plus, drumtrim. Same feature set as the SDK's op1plus. drumtrim is a no-op in this build: its trimmed kit files (build/factory_drum/*.raw) were not generated, so compose left them out (stock kits unchanged). Lost stock content: none. Image 14.99 MB vs 15.7 MB free on te-boot: fits, 0.7 MB margin. Not for first flash.

v1 image: 12.78 MB. Compose notes: WARP and TRIO share modal knob inputs, but only one engine screen is ever open at a time (compose note only, no conflict).

## OP FREAK platform features (src/platform/, built with scripts/compose_ofp.sh; none run on a device)

| Feature | Status | What | Hooks | Notes |
|---|---|---|---|---|
| ofp_audio 0.1 | built | wraps TE's audio render callback; optional per-block hooks for gain (`ofp_gain_process`) and measurement (`ofp_probe_block`) | entry hook on the callback setter `0x0191a7cc` + `boot_done` | other features depend on it |
| ofp_probe 0.1 | built, host + simulator tested (overlay screens) | measurement overlay, OFF by default, **shift + speech** cycles off / page 1 / page 2 / off | `on_key` filter (shift+speech), `frame`, idle vtable slot `0x01911b98`, call sites `0x01a5482e` / `0x01a54842` | claims shift+speech: conflicts with `meminfo` |
| ofp_gain 0.1 | DSP host-tested + Blackfin simulator bit-exact; glue built only | volume knob -> -100..+100 master gain, TE volume pinned at max | entry hook `0x01a10de4`, data patch `0x018b62a4` (ff -> 00) | UNVERIFIED: output word format, knob range |

Recipes: `recipes/probe_v1.json` (v1 + ofp_probe) -> `build/opfreak_probe_v1.op1`; `recipes/gain_v1.json` (v1 + probe + gain) -> `build/opfreak_gain_v1.op1` (do not flash before the probe image has confirmed `TX PK` and `VOL K`). Image size 12.78 MB each (budget 15.7 MB).

## v2 additions (2026-11-06)

| feature | state | what | hooks | notes |
|---|---|---|---|---|
| ofp_common 0.1 | library, host + simulator tested | op_common.c (PRNG, tables, ADSR, LFO, GEN) | none | |
| ofp_purp 0.1 | host + simulator tested (`tests/v2sim`), never on a device | THE PURP engine row `purp` on the engine host (tag 1), PURP screen, GEN, 8 presets | `on_key`/`on_knob` filters (engine screen of a PURP sound only), `frame`, `engine_name`, `init` | mono, TE envelope, 5 voices |
| ofp_seq 0.1 | host + simulator tested, never on a device | six generative modes on the synth sequencer screen with tombola picked | `on_key`/`on_knob` filters, `frame`, `seq_tick`, `kb_note_on/off` observers, `init` | open = orange push, close = shift + orange push |

Recipe `recipes/opfreak_v2.json` -> `build/opfreak_v2.op1` (12.80 MB, budget 15.7 MB). Guide: OPFREAK_V2_GUIDE.md.
