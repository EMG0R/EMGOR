# STORAGE (SD layout, migration, patch v8)

Code: `store/sdlayout.{h,cpp}` (hardware-free), `store/sdlayout_fatfs.{h,cpp}` (device StoreFs + shared ctx).
Tests: `tests/test_storage_layout.cpp`, `tests/test_storage_migration.sh`, `tests/test_patch_roundtrip.cpp`, `tests/test_synth_filter.cpp`.
All SD access is control plane (blocking). Background loop only, never the audio callback.

## Layout (under `PMOR3/`, beside the legacy files)
| path | content |
|---|---|
| `MANIFEST.TXT` | `PMORSTORE 1`, then `M <id> <len_samples> <rate> <ch> <crc32 of PCM, hex> <file>` per sample, `END <count> <crc of all text above>` |
| `SAMPLES/S%04u.WAV` | 16-bit mono 48 kHz PCM WAV, 44-byte header. CRC in the manifest is over the PCM bytes (what a chunked pull streams) |
| `PATCHES/P%02u.TXT` | `PMORPATCH 1`, `slot`, `fmt v8 7540`, `src_pmr <size> <stamp> <crc>`, `src_aud <size> <stamp> <AUD|BAK|->`, 16 x `s <slot> <sample id>` (0 = empty), the v8 patch blob as hex (`b ` lines, 64 B each), `end <crc>` |
| `PRESETS/<ENGINE>.TXT` | KICK SNARE HAT PERC (24 built-in sounds each) and SYN0..SYN7 (5 built-in presets per algorithm). `PMORPRESET 2`, `engine`, `blob <size>`, `f <n> <name> <hex>` factory entries (the firmware recipe bytes: drums `SoundDef`, synth `SynPreset`), `u <n> <name> <hex>` user entries (synth: a `SynPreset` at index 5..31; drums: `DrumUser` = base sound + 4 depth knobs), `end <crc>` |

Global pool: sample ids are unique across the card and deduplicated by (CRC, length); a patch references ids, any patch can reference any id (`storeAssign`).
Legacy: `P%02u.PMR/.AUD/.BAK` stay exactly as they are; old firmware boots the card unchanged.

## Migration (additive, idempotent, verified)
`storeMigrateStep(ctx)` = one patch per call (background loop); `storeMigrateAll` loops it.
- Reads legacy `.PMR` (v7 or v8) and `.AUD` (falls back to `.AUD.BAK` like the legacy loader); only complete AUDs (both magics, size matches) are used.
- Writes only inside the new tree. `pathOkToWrite()` refuses any path that is not `MANIFEST.TXT`, `SAMPLES/S*`, `PATCHES/P*`, `PRESETS/*`, or that contains `.PMR/.AUD/.BAK/..`. Nothing is deleted or renamed in the legacy set.
- Every written file is committed from `.TMP`, read back, size + CRC compared; `errors` counts failures, `verified == written` is the success condition. Each patch TXT is re-parsed and compared to the legacy-derived blob.
- Re-run: a patch whose TXT matches the current legacy size/mtime/CRC and whose sample ids all exist is skipped (0 writes). A lost manifest/WAV/TXT is rebuilt; an identical orphan WAV is adopted by CRC.
- Replacing a new-tree file keeps the previous copy as `<name>.BAK` (except pool WAVs, which are content-addressed).

## Dual-write, load, ids (wired in store/patch.cpp)
- **Save**: legacy `.PMR` / `.AUD` are written exactly as before (atomic, `.BAK` kept). When one completes, the slot is queued and the background job (`patchTick`, only when nothing is dirty/saving, >= 1.5 s after the last save) re-exports it from the verified legacy files: samples to the pool (deduped by CRC), manifest, patch TXT. A new legacy save aborts an in-flight export and re-queues it. A power cut leaves only orphan `.TMP` files; legacy files and every committed new-tree file stay valid.
- **Boot**: `patchInit` queues all 64 slots; the job skips up-to-date patches (TXT src stamps == legacy size/mtime/CRC, all ids in the manifest), so a migrated card costs ~64 small reads. Status: `patchStoreStatus()` (state idle/running/done/errors, pending, exported, written, verified, skipped, errors, samples).
- **Load**: legacy `.PMR` is read as before; `storeFreshIds` (TXT header + stats) says whether the new layout is in sync. In sync -> sampler audio comes from the pool WAVs, else legacy `.AUD`, and the slot is re-queued. If the `.PMR` and `.BAK` are both unreadable, the TXT blob is a third recovery source (then the legacy file is rewritten by autosave). Delete drops the TXT mirror so it cannot resurrect a patch.
- **Ids**: new sample ids are >= 1024 (`STORE_ID_BASE`), global across patches. Ids < 1024 are the old `patch*16+slot` aliases; `patchPoolRead` / `patchAssignSample` accept both, `patchStoreAlias(old)` maps to the new id. ASSIGN copies into the live slot, sets `audio_dirty`; the next save writes both layouts (no TXT-only edits, so no stale copy).
- Not done (builder #1 / protocol): GET_MANIFEST and the UI should enumerate `patchStoreSampleCount/At` and send new ids; the protocol code still speaks `(patch, slot)`.
- Limits: freshness uses FAT mtime (2 s) + size + PMR CRC; an `.AUD` rewritten within the same 2 s bucket with the same size would be missed (an export always follows >= 1.5 s after a save). Job SD cost per autosave burst: TXT 17 KB written + read back; samples only when new audio appears.

## Patch v8
Magic `OPG8` (`PATCH_MAGIC`), version 3, size 7540 = v7 prefix (7476) + `drum_eng[4][4]` floats before the CRC. `patchFromBlob()` accepts v8 or v7 (`OPG7`, 7476 B; knobs 0, CRC refreshed). `patchLoad` tries the file, then the `.BAK` (which stays v7 until a v8 save rotates it in). A v7 QSPI mirror is accepted at boot. Downgrading to v7 firmware: v8 `.PMR` is rejected by magic; v7 loads the `.BAK` (previous save) or default. Sequence matters: keep the pre-v8 backup.

## Synth filter
Chamberlin SVF stable iff `f*q < 2` and `f^2 + 2fq < 4`. `svfMaxF(q)` = min(0.9, 0.95 x limit). Old flat 0.9 cap was unstable for q ~ 2 (low reso).

## Preset files
- Boot job (after the patches, one engine per `storeJobTick`): a **missing** file is written from the firmware tables; an **existing** valid file is installed over them and never rewritten (edited or not). Unusable files (bad CRC, or blob size != this firmware's struct) are ignored and left byte-identical; the firmware tables stay in force. FX have no preset tables in firmware, so there is no FX file.
- Drums: `drumDef(group, sound)` (used by the two trigger sites) returns the file's recipe if installed (control plane, boot only, one write per row; no runtime un-install), else `DRUM_DEFS`. Synth: `synLoadPreset` consults the file table (`synFileSet/Get`, indexes < 32) before the firmware tables. Exported -> installed is byte-identical to the tables, so sound is unchanged (goldens unaffected).
- User save: `storeSynUserSave` / `storeDrumUserSave` rewrite the engine file (verified, `.BAK` kept); user slots only (synth idx >= 5). The UI still only offers `SYN_PRESETS` = 5 per algorithm: reaching user presets needs UI work.
- RAM: the install tables live in SDRAM. DTCM is at ~98% (128012 / 131072 B) after this change.
- UI: browser rows come from RAM tables (`synFileName/Get`, `drumsUserGet`), filled at boot from the files and updated by each save; saving is `patchUserSaveSynth/Drum`. Drum user slots exposed: `DRUM_USER_SLOTS` = 8 (the file allows 32). DTCM headroom after this change: 18020 B (gate: 16384).
