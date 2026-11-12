# PMOR flash log (twink)
Rollback images (on twink `~/pmor-flash`, copied to gorcave `~/pmor-bins`): ff86e4c rebuild `PMOR_V3_ff86e4c_twink_gcc10.3_2026-11-07.bin`; 722a311 `KNOWN_GOOD_PMOR_V3_metro_record_722a311_2026-10-06.bin`.
Every flashed image passed ./check.sh at its commit. Result column is the flash tool's result; boot verification is in the note.

| time | commit | image | hash | result | note |
|---|---|---|---|---|---|
| 2026-11-07 01:07:54 | 49ce9e6 | PMOR_V3_twink_extraction_f72cfdc_NOT_FLASHED.bin | sha256 b560a25c219a19da | FLASHED | phase-1 extraction build (f72cfdc), first flash of core extraction | boots, enumerates cafe:4f70, answers `status` over CDC (verified)
| 2026-11-07 01:08:55 | 49ce9e6 | PMOR_V3_studio_49ce9e6.bin | sha256 94d76405212a4db0 | FLASHED | phase-2 studio protocol build (49ce9e6) |
| 2026-11-07 01:12:14 | f1ec82c | PMOR_V3_studio_f1ec82c.bin | sha256 e0a7736ccd7e9411 | FLASHED | phase-2 + counters |
| 2026-11-07 01:15:22 | f1ec82c | PMOR_V3_studio_wip_f1ec82c.bin | sha256 8e6bbaeaaf657fb4 | FLASHED | tick-anchor fix + MIDI service from usb_service |
| 2026-11-07 01:17:08 | f1ec82c | PMOR_V3_studio_wip2_f1ec82c.bin | sha256 3bb110132ba2d17a | FLASHED | running reset on follow entry; counters |
| 2026-11-07 01:18:47 | f1ec82c | PMOR_V3_studio_wip3_f1ec82c.bin | sha256 bd206696ffbb0f92 | FLASHED | CC deadband |
| 2026-11-07 01:24:30 | f1ec82c | PMOR_V3_studio_wip4_f1ec82c.bin | sha256 31f5a921826cf3d2 | FLASHED | svc gap instrumentation |
| 2026-11-07 01:25:57 | f1ec82c | PMOR_V3_studio_wip5_f1ec82c.bin | sha256 755843db27f4cafe | FLASHED | loop-section timing instrumentation |
| 2026-11-07 01:27:06 | f1ec82c | PMOR_V3_studio_wip6_f1ec82c.bin | sha256 17f0a56adea746ea | FLASHED | trellis LED push keeps USB serviced (usbDelay) |
| 2026-11-07 01:28:06 | f1ec82c | PMOR_V3_studio_wip7_f1ec82c.bin | sha256 0f6968cbb63b41a6 | FLASHED | gap attribution instrumentation |
| 2026-11-07 01:29:27 | f1ec82c | PMOR_V3_studio_wip8_f1ec82c.bin | sha256 a3e0272ab214e977 | FLASHED | gap metric fix |
| 2026-11-07 01:35:31 | 1332642 | PMOR_V3_studio_1332642.bin | sha256 c9fbbd171a7c11cc | FLASHED | pipelined PULL, bring-up fixes (commit 1332642) |
| 2026-11-07 01:37:53 | 1332642 | PMOR_V3_studio_blk_1332642.bin | sha256 72b46a08af6cce1d | FLASHED | SD block cache for PULL_SAMPLE |
| 2026-11-07 01:52:05 | fa62268 | PMOR_V3.bin | sha256 405e6e00453d004c | FLASHED | gated: merged builder2 + IRQ-stamped MIDI RX + NOSAVE/PULL_FILE + deferred clockmode save |
| 2026-11-07 04:10:21 | 826a2d4 | PMOR_V3.bin | sha256 bfa4ac42d0fb0e74 | FLASHED | gated: follower ferr stats |
| 2026-11-07 04:13:25 | 98e0b3a | PMOR_V3.bin | sha256 d3211d7a93408b14 | FLASHED | gated: rx stamp diagnostics |
| 2026-11-07 04:15:28 | 355d107 | PMOR_V3.bin | sha256 df9f6934eb2b9d46 | FLASHED | gated: follower phase sign fix |
| 2026-11-07 05:45:02 | e5a3869 | PMOR_V3.bin | sha256 e1f13fc769a075f6 | FLASHED | gated: follower iv stats |
| 2026-11-07 05:47:23 | 4458d0c | PMOR_V3.bin | sha256 afda768ba98eeddb | FLASHED | gated: follower min-filter phase |
| 2026-11-07 05:50:46 | 56d9a07 | PMOR_V3.bin | sha256 abe652f5e54eba72 | FLASHED | gated: follower tempo median |
| 2026-11-07 07:29:31 | f7a6fe1 | PMOR_V3.bin | sha256 0cec1454fe2049a9 | FLASHED | gated: merged storage layout + presets + pool protocol |
| 2026-11-07 08:15:38 | 4052811 | PMOR_V3.bin | sha256 c963cad6dacb0b7c | FLASHED | gated: DTCM headroom fix (18 KB) + storage + presets |
| 2026-11-07 08:28:43 | 2d243ae | PMOR_V3.bin | sha256 707a0dac6fba33a6 | FLASHED | gated: SDRAM-ctor fix + DTCM headroom + IWDG/bootflag + storage + presets (fwcheck PASS) |
| 2026-11-07 08:48:30 | 4b6e5e3 | PMOR_V3.bin | sha256 de78a45bffb21045 | FLASHED | gated: HardFault->reset, PULL_FILE new-tree kinds, SAVE_SCRATCH |
| 2026-11-07 10:09:01 | 84edca1 | PMOR_V3.bin | sha256 e8017137030bb15d | FLASHED | gated: 1 KB chunks, 4 KB read-ahead, STORE_RETRY |
| 2026-11-07 10:12:25 | a51663b | PMOR_V3.bin | sha256 a51a923964e77876 | FLASHED | gated: bulk burst pump |
| 2026-11-07 17:02:46 | 627ba4d | PMOR_V3.bin | sha256 3372884f5619af0b | FLASHED | gated: SD timing instrumentation |
| 2026-11-07 17:07:15 | fcee1b1 | PMOR_V3.bin | sha256 7b18c024d038e607 | FLASHED | gated: fast SD SPI + open-handle reads + user presets UI merge |
| 2026-11-10 00:34:12 | e22707b | PMOR_V3.bin | sha256 e4e85c532175cbba | FLASHED | gated: merge e22707b RETRYSTO menu |
| 2026-11-10 00:48:11 | d3624c9 | PMOR_V3.bin | sha256 4e1ceed55036ec85 | FLASHED | gated: SD/handle counters |
| 2026-11-10 00:51:14 | de74444 | PMOR_V3.bin | sha256 9e37dfd7847ef243 | FLASHED | gated: file reopen / job debug counters |
| 2026-11-10 00:56:01 | 8ccd283 | PMOR_V3.bin | sha256 71d7c182a86ef3c0 | FLASHED | gated: automatic clock role, continuous clock, handle fix |
| 2026-11-10 01:49:36 | 8ef6076 | PMOR_V3.bin | sha256 1a0ab35ce55057aa | FLASHED | gated: per-call open + 16 KB read-ahead (handles reverted) |
| 2026-11-10 02:04:48 | 8ef6076 | PMOR_V3_4b6e5e3_storage_presets_2026-11-07.bin | sha256 de78a45bffb21045 | FLASHED | BISECT: known-good 4b6e5e3 (rollback path) to separate firmware vs environment |
| 2026-11-10 02:08:55 | fe9303b | PMOR_V3.bin | sha256 08fbd2efc8674f3f | FLASHED | gated: restore HEAD after bisect |
| 2026-11-10 02:15:26 | 0436fc1 | PMOR_V3.bin | sha256 5c11b6ad5cf8e730 | FLASHED | gated: burst 16 ms |
| 2026-11-10 05:26:09 | f164b44 | PMOR_V3.bin | sha256 4cb1810834efd29f | FLASHED | gated: ASSIGN_SAMPLE id<=65535 (macbert fix) before Mac move; rollback ~/pmor-flash/PMOR_V3_0436fc1_ROLLBACK.bin |
| 2026-11-11 | 65acd1c | PMOR_V3.bin | sha256 3b13f494730c4ccb | FLASHED | macbert/padlat: 2 ms keypad poll state machine, OLED/sensor/LED pieces <= ~1 ms, fxMidiPoll + CRC fixes; tools/latcheck.sh ALL OK; rollback GOOD_lagfix_669ae6e.bin |
| 2026-11-12 | 184beb9 | PMOR_V3.bin | sha256 4494cf19a7a919c8 | FLASHED | macbert/drumsynth: per-pad drum synth engine (96 presets as param sets, 136 params/pad), DRUM_MAX_VOICES 4; tools/latcheck.sh ALL OK; rollback FLASHED_archfix_00dc947.bin |
