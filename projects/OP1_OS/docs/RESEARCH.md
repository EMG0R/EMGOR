# OP FREAK research notes (R246, original OP-1)

Method: read-only. Sources are the vendored repos (`vendor/`, hashes in VENDOR.md), the stock `op1_246.op1`
(hash below) unpacked with `vendor/op1-sdk/tools/unpack_op1.py` + `memimage.py`, and a
`bfin-elf-objdump` listing of the SDRAM code. Nothing here was run on the device except the three MEASURED
file facts from Emory's backup (marked MEASURED). Everything else is read from code or docs by other people.
Tags: MEASURED = observed on Emory's unit/backup or by us on the stock file. CODE = read in firmware or source.
DOC = stated in a vendored doc, not re-checked. UNVERIFIED = inference or hearsay.

Stock firmware SHA-256: `c5315218f825f143b415ca554516541898abee843d3a236df0b54c04e1fb13a9` (MEASURED, matches op1-sdk
`tools/build.py:62`), 13,039,128 bytes, downloaded from https://teenage.engineering/_software/op-1/op1_246.op1
(link on teenage.engineering/downloads/op-1). Contents (MEASURED by unpack): OP1_vdk.ldr, te-boot.ldr, content/*.db, display/*.svg.


## Device note 2026-11-06 (Emory)
`opfreak_probe_v1` flashed and boots; the Mac sees the OP-1 as USB audio, 2 in / 2 out at 44.1 kHz. Emory reports the USB crackle seems to happen only when audio is sent INTO the OP-1 over USB (host to device), not on the to-host stream. Inference for the suspects below (not measured): the to-host ring/packet path (suspects 1, 2, 4) is the one the probe's page 2 measures, so a crackle that exists only on playback into the OP-1 points at the host-to-device path (OUT endpoint, its ring / clock handling, a different code path not yet read or instrumented) or at CPU load while mixing the USB input; read the OUT-endpoint code before more probe work. Probe readings (`TX PK`, `VOL K`, page 2) have NOT been captured yet.

## DECISION SUMMARY (2026-11-04, platform research)

Tags: MEASURED = observed on Emory's unit/backup or by us on the stock file or in the simulator. CODE = read in the firmware image / source. DOC = stated in a vendored doc. UNVERIFIED = inference, hearsay, or built but never run on the device.

- **CPU:** ADSP-BF524C2, ~400 MHz (399 MHz measured by te-op1-cfw), **no FPU** (fixed-point only; float is emulated). Block = 64 frames @ 44.1 kHz = 580,499 cycles. Voices render 128 samples @ 88.2 kHz (4,535 cycles/sample). About 300-324 MHz free with one voice, no FX (others' measurement). A 5-voice flagship at 15 % margin = ~48 MHz (~544 cycles per 88.2k sample) per voice.
- **One synth engine at a time:** YES. One engine type, 6 voice objects (L1 scratchpad `0xffb00000`); the "8 sounds" are knob sets. Idle legacy engines cost 0 CPU. A 5-voice PURP fits the 6 slots; its voice state must live in our own SDRAM.
- **Memory:** 64 MB SDRAM; free for us 2 MB at `0x01e00000` (probe image uses 41 KB; +17 KB with gain tables); likely-free 8 MB `0x02800000-0x02ffffff` (statically unreferenced, ~85 %, never tested). User disk = one 384 MiB FAT image file in YAFFS (`tape_c.raw`, 402.65 MB): tape 127.1 MB + album 127.0 MB + ~148.5 MB drum/synth. Tape files are preallocated at image creation (size fixed unless the image is rebuilt; `noalbum` rebuild empties tape + album). Update budget 15,728,640 B vs v1 12.78 MB (probe/gain images 12.78 MB, v1x 14.99 MB). `noalbum` frees ~128 MB of runtime flash, not update size; `drumtrim` -1.43 MB. Stereo tape at 180 s = 31,752,000 B/track, same as mono 360 s; flash bandwidth 705.6 KB/s (4 tracks) vs 352.8 KB/s.
- **Feasibility:** (a) T1-T4 + encoder push: **YES** (filter hooks on_key/on_knob; draw over TE's screen). (b) engine banks: **PARTIAL** (synth: new rows ahead of stock + rename = LEGACY by naming; no proven runtime hide; drum: one engine row only; sequencer: 6 fixed types, host ours on tombola). (c) empty clean samplers: **YES synth** (new sampler preset rows in a new folder, no engine row), drum partial. (d) volume knob pin + bipolar gain: **YES** (knob is digital: TWI byte -> software output ramp; pin by hook, gain in the audio wrapper); UNVERIFIED on device. (e) boot screen: **PARTIAL** ("OP FREAK" in the 8-byte boot text is free and device-verified as a mechanism; animation depends on frame hooks during boot, untested). (f) purple theme: **YES** (palette `colors.svg` + hard-coded hex in 61 SVGs = build-time recolour; runtime LUT alternative; display is OLED per a forum quote). (g) tilt: **YES, hardware exists** (accelerometer on TWI 0x53, int16 X/Y/Z at `0x01dff338`, ADXL345-like UNVERIFIED); read the words, never issue our own TWI. (h) USB audio: endpoint is **asynchronous + implicit feedback** with a 512-frame ring as the only rate match; suspects ranked below; probe image measures them. (i) stereo tape: **PARTIAL/hard** (mixer, recorder, resampler are in XTEA-encrypted L1; no key; cheapest honest path = pair tracks 1+3 / 2+4 as L/R or own recorder into the stereo album files).
- **Built this session:** probe image `build/opfreak_probe_v1.op1` (overlay off by default, **shift + speech** toggles), gain stage (host-tested + Blackfin-simulator bit-exact; demos in `build/demos/gain_*.wav`), image `build/opfreak_gain_v1.op1` (built only). Nothing was run on a device.
- **Blockers / open:** tx word format (left-justified 32-bit assumed from the USB ring code) must be read on the probe (`TX PK`) before the gain image is ever flashed; knob byte range (`VOL K`) calibrates the knob map; CPU cost of the gain stage not measurable in the simulator (CYCLES reads 0).

---

## Earlier notes (2026-11-03, read-only study; kept as written)

## a) Chip, RAM, FPU

- Analog Devices Blackfin **ADSP-BF524C2** (string in image; `vendor/op1-sdk/docs/internals.md:77`). ~400 MHz, no FPU
  (`vendor/te-op1-cfw/docs/HANDOVER.md:201`, `engine-interface.md:88`; `pcppcp-op1r-src/synth_sdk/PLUGINS.md:81`
  "There is no FPU; float is emulated and far too slow. Use Q15"). Fixed-point only: 16-bit dual-MAC, 40-bit accumulators.
  The SDK compiles with `-mcpu=bf524` (`op1-sdk/tools/build.py:65`) and has no float path. TE's own code does use soft-float
  in places (e.g. `audio.cpp:882 gain >= -34.5f`, string in image), not in the audio inner loops that we know of (UNVERIFIED).
- RAM: 64 MB SDRAM (`op1-sdk/docs/lessons.md` "OP-1 has 64 MB of SDRAM"). Map (`te-op1-cfw/docs/memory-map.md` section 2/3,
  `op1-sdk/docs/internals.md` "Chip and image"):
  - image `0x01000000-0x01dfff34` code/data/bss (code ~`0x018ab7cc-0x01aa7078`), free tail `0x01e00000-0x01ffffff` (2 MB; the SDK/op1r load here).
  - heap 0 `0x100-0x00fffffc`, heap 1 `0x03040000-0x03fffdb0` (table at `0x018e628c`); samples live in heap 1.
  - TLSF pool for `operator new`: `0x0106b7cc-0x018ab7cc` (8.25 MB), `init_memory_pool` call `0x01a5e364`.
  - `0x02040000-0x027d1fe0`: 7,938,016-byte array (~90 s mono at 44.1 kHz), cache for audio, owner not identified (memory-map.md DOC, [I]).
  - framebuffers `0x02000000` / `0x03000000` (RGB565, double-buffered).
  - L1: instruction SRAM `0xffa00000` 48 KB, data A `0xff800000`, data B `0xff900000` (31.5 KB). **L1 code and most L1 data are
    XTEA-encrypted in 8 of every 24 bytes** (`internals.md` "Booting TE's firmware in the simulator"), so the tape mixer / USB push / audio ISR
    cannot be read or patched wholesale. op1r/op1-sdk patch them only through precomputed byte diffs (`op1r-src/synth_sdk/l1_patches.json`).
- Cache: SDRAM is write-back cached (DCPLB flags `0x0003101d`), display pages write-through (`memory-map.md` section 4).
  DMA (screen, audio) only sees data after an explicit flush (`op1-sdk/include/op1/te246.h:166`).

## b) Tape storage and playback

MEASURED (Emory's unit, disk mode backup `~/Documents/_OP1_BACKUP_2026-11-03/files`, read-only):
- `tape/track_1..4.aif`: 31,785,072 bytes each, 1 ch, 44100 Hz, Int16, 360.37 s (preallocated). AIFC `sowt` (little-endian PCM).
  Header re-read by us: COMM numSampleFrames = 0x00F28000 = 15,892,480 frames; SSND chunk starts at byte 0x68. 31,785,072 = 112 B header +
  485 x 65,536 B.
- `album/side_a.aif`, `side_b.aif`: 63,504,112 bytes each, 2 ch, 44100 Hz, Int16 interleaved (= 15,876,000 x 4 + 112). The COMM frame count
  in side_a's header is 0x000134C0 (79,040 frames), i.e. the header holds the recorded length while the file is padded to full size
  (op1-sdk lessons.md: "reserved at full length, padded with an AIFF SKIP chunk"). The master/album bounce is already stereo.

CODE / DOC:
- Usable tape length `TE_TAPE_MAX = 15,876,000` samples = exactly 360.000 s (`op1-sdk/include/op1/te246.h:268`; seek asserts beyond it).
  The file holds 485 chunks x 32,768 samples = 15,892,480 frames (slack of 16,480 frames) (`pcppcp-op1r-src/synth_sdk/tape/erase.c`: `TE_CHUNKS 485`, `TE_CHUNK_SHIFT 15`).
- Where it lives: `/yaffs2/user/tape_c.raw` is the 384 MiB FAT image that disk mode exports; the tape reads/writes **in place** inside
  `tape/track_N.aif` in that image (`op1-sdk/docs/lessons.md` 2026-10-18). Track byte offsets are computed at boot by the loop at
  `0x019970a6` calling `0x01997c58` (cluster chain to byte offset, via FAT code) for the four strings at `0x018d9eac` (stride 0x18),
  stored in the table at `0x018c1810` (`TAPE_BASE` in `erase.c`); album sides' offsets go to `0xff9002c0/c4`.
  Strings: `/tape/track_N.aif` (`0x018d9eac...`), `/album/side_a.aif` (`0x018d9f0c`), `tapefile.raw`, `/user/tape_c.raw` (`0x018d9e98`) (strings in image, MEASURED by `strings`).
- Clip model: the tape is a set of clips `{channel, start, stop}` (db2/Clips.cpp, up to 512, `MAX_CLIPS`; table `0x01AA8D70`, has-flags `0x01AAA570`; asserts
  `m_channel < kTapeNumTracks`, `stop <= kTape_length`). Clips are cached in RAM and written back by `tape_c::service` (`0x01A54710`) with
  `yaffs_write(fd, data, 64 KB)` per chunk (`tape/safewrite.c`: call at `0x01A54842`; erase zeroes 485 chunks x 4 tracks x 64 KB = 124 MB, `tape/erase.c`).
  Tape file handle `0x01B4EF44`, 64 KB zero buffer `0x01B4EF8C`. `tape_c.cpp` assertion at line 1039 is in L1 (the resampler/mixer, `hook.S` comments).
- Storage medium: raw NAND with YAFFS2 (2 KB pages, 128 KB blocks) (`te-op1-cfw/docs/app-boot-chain.md:39`, `firmware-246-baseline.md:74`).
  YAFFS frees space lazily (lessons.md). **Read/write bandwidth: no figure in any vendored doc or source. UNVERIFIED.** Known facts: a clip save is one
  64 KB write (= 0.74 s of one mono track at 88.2 KB/s); 4 tracks streaming mono is 352.8 KB/s. Stereo at the same duration is 705.6 KB/s.
  Needs a measurement on the device (see README roadmap, profiling step): time `tape_c::service` writes with `CYCLES`.
- Playback path (L1, encrypted): per audio block of 64 frames, tape mix loops at `0xFFA0709C` (forward) / `0xFFA0712C` (reverse) sum the 4 tracks into two
  source buffers (L at `0x01B5F434`, R at `0x01B5EF84`, 600 samples each), then the resampler `0xFFA06194` (16 taps, 64 phases, coefficients `0xFF802154`,
  position/increment in 1/64 sample, gain ramp) produces the 64 output frames; this gives tape speed/varispeed (`usbtrack.c` header; `hook.S:310-380`).
  Recording writes the mono synth bus (`BUS = 0xFF800BB4`) after `Core_mix_master` through `tape_record_block` (`screens/fxroute.c` header and defines).
- Stereo tape plan implications (all from the above): (1) disk layout: track files are `1 ch` with 485 x 64 KB chunking; stereo means 2 ch frames of 4 bytes
  so 180 s fits the same 31.78 MB file; `kTape_length`, `TE_TAPE_MAX`, chunk math (`>> 15` samples per chunk, `<< 1` bytes) and the clip record are
  all in code to patch; (2) the L1 mixer/recorder where the sample size is baked in is encrypted and only patchable by byte diffs; (3) the 0x02040000 audio
  cache and RAM clip buffers double; (4) album/side files are already 2 ch 4-byte frames so the bounce path is a template. Whether NAND/YAFFS sustains 2x: UNVERIFIED.

## c) Mixer / master bus

- Mixer UI: `MasterController` vtable `0x01910aac`, pages levels/EQ/FX/compressor (`te246.h:305-319`).
- Tracks are **mono** files. Per-track state arrays in L1 data: volume `0xFF801928`, level `0xFF8018B4`, mute fade `0xFF8018C4`, **pan `0xFF801930`**
  (`screens/fxroute.c` defines), i.e. panning is applied in the tape mix (`tape_play_mix`, L1) when the 4 mono tracks are summed into the L/R source buffers
  above, before the resampler. The block output buffers are `TAPE_L = 0xFF800D34`, `TAPE_R = 0xFF800DB4` (64 x int16 each). So the master bus is **stereo**
  (L/R int16) from tape mix onward. `Core_mix_master` adds the instrument (synth/drum, mono `synth_mix_bus` `0xFF800BB4`) into tape L/R and records the bus (fxroute.c).
- Master FX is stereo (position 2) with params at `0xFF8016C4`, type `0xFF801090` (`op1_plugin.h` FX section, `te246.h:312-314`); stereo FX vtable +0x14
  process(l, r, params), factory `0x01a1b5f0` (`te246.h:351-359`).
- Output to codec: SPORT0 I2S, 32-bit slots, 64 stereo frames per block via callback pointer `0xff80036c` (`te246.h:341-347`), 44.1 kHz "probably" (UNVERIFIED);
  engines render internally at 88.2 kHz (op1-sdk README "internal 88.2 kHz"; `engine-interface.md`: voice render every 128 samples at 88.2 kHz).

## d) USB audio (R246 and op1r)

R246 (CODE, `usbtrack.c` comments on stock behaviour, `memory-map.md`):
- USB device audio class ("usb_audio", descriptor init `0x01A6A2F0`); to-host stream is stereo, 24-bit little-endian (3 bytes), packet = frames x 6 bytes, max
  46 frames per iso packet, 44.1 kHz (the tooling records at 44100: `op1r-src/tools/usb_check.py:85`). Full-speed iso (1023-byte limit, `usbtrack.c`).
- Producer: L1 `usb_audio::push_lr` (`0xFFA049FC`) / `push_interleaved` (`0xFFA0487C`) called from the audio block callback, wrappers `0x019E1B3C` / `0x019E1A88`.
  Ring: 512 frames at `0x01AAEB60`, write index `0x018B5FE4`, read index `0x018B5FE8` (64 frames written per audio block, read by the packet builder `0x019E161C`).
  iso IN queue `0x01A6A474`; USB device heap is a 96 KB TLSF pool (`memory-map.md`, `0x1a71350`).
- Clocking: the audio clock is the codec (SPORT master). USB frame timing is the host SOF. The stock code has no asynchronous feedback; `usbtrack.c:139` says
  "an underrun repeats the last one; a resync jump is rare" (read index re-synced by frame counts). So the ring is the only rate-match: UNVERIFIED whether the
  endpoint is declared adaptive/async (descriptor templates are built at runtime, not in the image: I dumped `0x18C1A38` and it is zeroed).
- op1r (pcppcp op1r 48): widens the stream to 6 channels (L/R + 4 tracks, 24-bit, `USB_MAXPKT = 46*6*3 = 828` bytes) by patching descriptors and adding a parallel
  4-channel ring (`usb6_ring[512][4]`) written right after the stock push. No fix for the stock clocking.

Likely crackle causes (HYPOTHESES from code; none reproduced or measured yet):
1. Rate mismatch: codec-clocked producer (64 frames / 1.45 ms) vs host-clocked 1 ms iso reads with a 512-frame ring and no feedback: slow drift ends in an underrun
   (repeats last frame) or a resync jump = click (`usbtrack.c:139`).
2. Packet size granularity: 44.1 kHz needs 44 or 45 frames per ms with a burst of 46; any off-by-one in the frames-per-packet logic gives periodic ticks.
3. Cache coherence: ring and packet buffers are in write-back SDRAM (DCPLB `0x0003101d`) and the USB DMA only sees flushed data (`te246.h:166`); a missed flush
   would be intermittent garbage. Need to check the flush near the iso IN call (not yet done).
4. CPU overload / interrupt latency: one core runs voices, FX, tape, UI and both interrupts; op1r's own docs report crackle when the block time budget is exceeded
   (`op1r-src` hook.S:443 "made the audio crackle"; te-op1-cfw device-tests). Heavy engines + USB push in the same ISR chain.
5. The 96 KB USB TLSF heap and `std` allocations in the USB class event callback running while the TLSF mutex is held elsewhere (UNVERIFIED).
First test to separate them: record USB with `op1r-src/tools/usb_check.py` (gap detector, lines ~99-110) while idle vs under load, and read ring fill from the
resource monitor hook (`sysmon.c`).

## e) How op1-sdk features hook in (for a generative sequencer host)

All via `tools/compose.py` shared hook points (`op1-sdk/docs/features.md` table):
- clock tick: `seq_tick`, call site `0x01a431aa`, once per 64-frame audio block (1.45 ms), runs in the sequencer thread woken by the audio interrupt.
  Tempo math uses 44.1 kHz and `2646000 = 44100*60`; 96 ticks per beat assumed (UNVERIFIED) (`te246.h:253-255`; tempo in op1.db `tempobpm`).
- note events into engines: `kb_note_on/off` (`0x019ac290/0x019ac3a6`) and `midi_note_on/off` (`0x01a85bc6/0x01a8599e`) into VoiceCtl; a sequencer plays by
  calling VoiceCtl note-on `0x01a85014` (velocity, MIDI out) as NOTE ROLL does (`features/noteroll/README.md` "Playback").
- all MIDI: `midi_in`, entry `0x01a85c20` (`std::vector<uint8_t>*`).
- encoders/keys: `on_key(id, down)` `0x019b6c74`, `on_knob(knob, int16 *diff)` `0x019b6c0c`; key ids in `te246.h:234-243`, device-verified sheet in `op-1 key mapping/`.
- screen: `frame` hook `0x019ab8b2` (after TE rendered; draw with `include/op1/gfx.h`, UI 320x156, panel buffers `0x02000000`/`0x03000000`).
- boot/main loop: `boot_done` `0x019f09d6`, `main_tick` `0x019f0a6a`; persistent state in TE's op1.db via `TE_DB_*` from the main loop.
- sound: engines hosted by `enginehost` on the drwave voice; FX via pointer redirects; plugin API for op1r in `op1_plugin.h` (per-engine `render` 64 samples, 4 knobs).
- Rules (op1r PLUGINS.md): audio-ISR code, no float, no division per sample, no libc, no malloc; `TE_HEAP_MALLOC(TE_HEAP_SAMPLES, n)` from the main loop for big buffers.
- Reference sequencer implementations: `features/noteroll`, `features/drumroll`, `features/requantize` (op1-sdk), `seq/` + `lfo/` (op1r-src).

## f) CPU / RAM budgets and the on-device monitor

- Block budget: 64 frames at 44.1 kHz = 1.451 ms = ~580k cycles at 400 MHz (`te-op1-cfw/docs/marbles-port.md:113`). Free CPU with one phase voice, no FX: ~81% (~324 MHz),
  measured by te-op1-cfw "probe v3" (`engine-interface.md:119`, `device-tests.md`). Stock phone FX ~35 MHz master / ~16 MHz synth (te-op1-cfw docs). Typical engines 35-70 MHz,
  analog drums 84-91 MHz (te-op1-cfw `next-projects.md`). Other people's numbers, DOC only.
- RAM: image defines ~25 MB; TE's two malloc heaps own the rest; heap 0 16 MB, heap 1 15.75 MB; TLSF 8.25 MB; free for us: 2 MB at `0x01e00000` (~336 KB left in op1plus)
  (`lessons.md`, `memory-map.md`). Update package limit ~15 MiB (`lessons.md`), app NAND area 124 blocks ~15.5 MB, stock uses 2.2 MB (`brick-safety.md`, `app-boot-chain.md`).
- On-device resource monitor: op1r `screens/sysmon.c` (clock screen + T1, shift+help). CPU = 100 - idle% from VDK idle-thread run cycles (`kTE_Idle::run` hook, stats at thread+0x34);
  AUDIO = busy cycles of the audio DMA callback (`0xFFA04804`) per block, with a peak; RAM = both heaps via `0x01A9F2E2`; DISK = yaffs free/total (`0x01A9B1C0/0x01A9B284`);
  MIDI rx/tx ring fill (1024 entries). op1-sdk has `features/meminfo` (SDRAM panel) and `diskinfo` (flash usage) in `recipes/op1mem.json` / `op1disk.json`.
  te-op1-cfw's probe reports MHz and free % acoustically (device-tests.md).

---

# Part 2: platform research (2026-11-04)

Sources: our own `objdump` listing of the stock SDRAM image (`build/analysis/mem/01000000.bin`, `bfin-elf-objdump -D -b binary -m bfin --adjust-vma=0x01000000`), op1-sdk docs/headers, te-op1-cfw docs, pcppcp-op1r-src (GPL, reference only; see VENDOR.md), and two read-only research passes whose scratch files were `/tmp/op1a`, `/tmp/op1_unp_r1`. Paths below are relative to `vendor/`.

## 1. Chip, FPU, render rate, cycle budget
- ADSP-BF524C2 (string in the image; `op1-sdk/docs/internals.md:77`). Clock 399 MHz MEASURED by te-op1-cfw (`te-op1-cfw/docs/device-tests.md:1079`). **No FPU**: `te-op1-cfw/docs/HANDOVER.md:201`, `pcppcp-op1r-src/synth_sdk/PLUGINS.md:81` ("float is emulated and far too slow. Use Q15"). Our gain stage and probe are integer only; the Blackfin compile of the gain stage contains no libgcc mul/div calls (CODE, `objdump` of `_ofp_gain_block`).
- Voices render **128 samples per call at 88.2 kHz**, ~689 calls/s per sounding voice (`te-op1-cfw/docs/engine-interface.md:88-90`, MEASURED by them). Output/codec rate 44.1 kHz (audio block = 64 frames).
- Budget at 400 MHz (computed): 580,499 cycles per 64-frame block; 9,070 cycles per 44.1k sample; 4,535 per 88.2k sample; a 128-sample voice call may use the whole block interval.
- Measured by others: ~324 MHz (81 %) free with one phase voice and no FX (`engine-interface.md:119`, `device-tests.md:131`); phone FX ~35 MHz master / ~16 MHz synth (`next-projects.md:53`); a cache miss ~100 cycles; 16 data CPLB slots (`memory-map.md:4`).
- 5 voices with 15 % margin for tape/USB/UI: ~240 MHz total, ~48 MHz or ~544 cycles per 88.2k sample per voice. Lever: run the ladder at 44.1k with 2x upsample (UNVERIFIED).

## 2. One synth engine at a time?
- YES (`te-op1-cfw/docs/engine-interface.md:36-48`): the factory `0x01a1b7d4` fills `producers[0..5]` from table `0x018f5a04` (6 voices per engine; drum/dbox 12), constructed in L1 scratchpad at `0xffb00000 + i*28/32`. "Only one synth engine is active at a time." The 8 "sound slots" are knob value sets (`op1-sdk/docs/lessons.md:21-25`). Only sounding voices render; a released voice renders through its tail (`engine-interface.md:92`). Whether synth and drum sound together under `s_producers[instance]` is UNVERIFIED. Consequence: PURP = 5 voices in our own SDRAM state with the engine host's 6 slots as render shells; legacy engines cost 0 CPU while another engine is selected.

## 3. Memory audit
**SDRAM (64 MB, `0x00000000-0x03ffffff`)** (`te-op1-cfw/docs/memory-map.md`, `op1-sdk/docs/internals.md`, our `build/analysis/mem/layout.json`, `op1-sdk/features/meminfo`):
| range | use | status |
|---|---|---|
| `0x00000100-0x00ffffff` | VDK malloc heap 0 (16 MB) | CODE |
| `0x01000000-0x01dfff34` | image: bss, TLSF pool `0x0106b7cc-0x018ab7cc` (8.25 MB, `operator new`), const ~420 KB, code ~1.6 MB, more bss | CODE |
| `0x01e00000-0x01ffffff` | **free, 2 MB, our code** (op1plus leaves ~336 KB; probe image 41 KB, + gain tables ~17 KB) | CODE |
| `0x02000000`, `0x03000000` | framebuffers (150 KB / 256 KB, RGB565, double-buffered) | CODE |
| `0x02040000-0x027d1fe0` | 7,938,016 B (about 90 s of mono 44.1k), consumer unknown; LDR FILL, zero at load | CODE |
| `0x02800000-0x02ffffff` | **8 MB statically unreferenced** (`0x02800000-0x02bfffff` has no data CPLB: access faults). Candidate for flagship state, confidence ~85 %, NEVER tested | UNVERIFIED |
| `0x03040000-0x03fffdb0` | VDK heap 1 (15.75 MB): sample buffers (`SampleBuffer.cpp`), whole drum files, updates | CODE |
| L1 | inst `0xffa00000` 48 KB (XTEA-encrypted 8 of 24 bytes), data A/B `0xff800000/0xff900000`, scratchpad `0xffb00000` 4 KB (voice + FX objects) | CODE |

**Flash.** Raw NAND with YAFFS2 (2 KB pages, 128 KB blocks) (`te-op1-cfw/docs/app-boot-chain.md:39-41`). Boot area 128 blocks = 16 MB: te-boot blocks 0-3, app LDR from block 4, ~15.5 MB room, stock uses 2.17 MB. YAFFS partition and total NAND size are in no doc: UNVERIFIED. Free space on a stock unit ~75 MB (`lessons.md`, noalbum README).

**User disk.** `/yaffs2/user/tape_c.raw` = 402,653,184 B (384 MiB) FAT image stored as one YAFFS file; disk mode exports it (402.7 MB FAT16, MEASURED). Contents MEASURED: tape 4 x 31,785,072 = 127,140,288 B; album 2 x 63,504,112 = 127,008,224 B; ~148.5 MB for drum/synth user files. **Where the tape budget lives:** the four tape files are created at full length when the image is built ("Creating FAT image", create check `0x019972c4`; geometry `0x019761e6/ea`; size `0x01a9a098`). Length literal 15,876,000 occurs at `0x018f6314`, `0x018fb5d0`, `0x018fbcf0` and as immediates at `0x019987c0/e6` (album create), `0x01996cf2` (album import); the tape-track create site was not located. The size cannot change without rebuilding the image; `noalbum` proves a rebuild works (384 -> 256 MiB) but empties tape and album on first boot (`op1-sdk/features/noalbum/README.md`).

**Update-image budget.** Limit 15 MiB = 15,728,640 B = the 15.7 MB free on te-boot's upload disk (`lessons.md`). v1 12,779,521 B (headroom 2.95 MB); probe v1 12,782,314 B; gain v1 12,785,757 B; v1x 14,992,131 B (headroom 0.74 MB). Stock tar 26,368,000 B (LZMA 13.04 MB); `OP1_vdk.ldr` 2,171,624 B; raw audio content 21.6 MB (factory_drum 11.5, drum slots 6.1, factory_synth 3.4, synth 0.55); duplicated default samples ~2 MB. **noalbum** does not shrink the update (it shrinks the runtime image by 128 MB). **drumtrim** takes factory_drum 11.18 -> 9.76 MB (-1.43 MB of update).

**Stereo tape at 180 s/track.** 180 x 44100 x 2 ch x 2 B = 31,752,000 B per track = same as stock mono 360 s; 4 tracks 127,008,000 B: disk layout unchanged. RAM: the `0x02040000` cache would hold ~45 s stereo instead of 90 s. Flash: 176.4 KB/s per track, 705.6 KB/s for 4 vs 88.2 / 352.8 KB/s; a 64 KB chunk write recurs every 0.37 s instead of 0.74 s per recording track. NAND/YAFFS throughput is unmeasured: UNVERIFIED (the probe image measures tape write times, page 2).

## 4. PLAN items

### (a) T1-T4 and encoder push in synth/drum mode: FEASIBLE
- `on_key(key, down)` entry `0x019b6c74` and `on_knob` `0x019b6c0c` are filter hooks in compose (nonzero return skips TE's controller): T1-T4 = ids 6-9, pushes 59-62 (`op1-sdk/include/op1/te246.h:216-244`). Releases arrive as key events too: consume with a bitmask as WARP does (`features/warp/warp.c:95-110`).
- Stock behaviour: synth T1-T4 select subscreens engine/envelope/fx/lfo (`TE_SYNTH_SUBSCREEN` `0x01b4ee88`, DOC `te-op1-cfw/docs/screen-interface.md:379-413`); shift+T6/8/9 open preset/FX/LFO browsers (leave shifted keys alone). Stock synth ignores pushes 59-62 (DOC, device-verified by engine_plus). Drum controller (vtable `0x0190fa64`) has the same key slots (`device-tests.md:782`).
- Drawing: our page grammar draws over TE's screen from the `frame` hook (`0x019ab8b2`): WARP/TRIO/WHEEL blank y 0-140 and draw their own page, gated by `TE_EDIT_MODE==0 && SUBSCREEN==0` (`warp.c:91,201-256`). Replacing TE's SVG screen needs a screen-id swap and the table is full (68 entries, `next-projects.md:100-102`); op1r's `op1_screen_t` is GPL reference only.
- Push+turn works because on_knob also filters. "Long hold" needs timing in `main_tick` (8 ms): UNVERIFIED. Risk: swallowing T6-9 strands TE's own sub-pages unless we write `*_SUBSCREEN` ourselves or draw envelope/fx/lfo too; drum mode untested.

### (b) Engine banks main vs LEGACY: PARTIAL
- Synth engine list = table `synth_types` in `content/op1_factory.db` (10 rows; no sampler, no iter; MEASURED via sqlite). Picker runs `SELECT type FROM synth_types WHERE id=%i` (string `0x018c7aec`; compose point `engine_name` call site `0x01940374`). Preset browser groups by `folder` of `synth_presets`/`synth_user` (order UNVERIFIED). Limits: 176 non-sampler factory presets, 42 sampler, 318 total; a bad row halts boot (`next-projects.md:194-205`).
- Cheapest mechanism, no deletion: add our rows via `content_sql` with ids ahead of the stock ones, show stock rows under a "legacy" display name through the `engine_name` rewrite (the LOFI/WARP pattern); type strings must stay stock-valid or boot asserts (`patch.cpp:390`). A true runtime toggle that hides rows is UNVERIFIED: the synth type-list builder was not traced (`0x19707d8` is mapped only for `seq_types`, `sequencer-interface.md:306`).
- Drum: `drum_types` has one row (dbox, MEASURED); kits are preset-only (folders `drum` 14, `dbox` 10). A new drum engine owns the drum voice render/note_on slots, as DRUM ROLL does (`drumroll/feature.json:65-71`).
- Sequencer: six fixed types (`SeqIds` table `0x018fa634`); `SetPlayerType` `0x01a0d050` asserts on index >= 6; an unknown name in the `sequencer` table is a fatal boot assert (`sequencer-interface.md:17,217`). Proven route: host the generator on tombola (its controller uses no keys), keep the `tombola` name in the DB, keep the marker in a private op1.db table (`sequencer-interface.md:260-275`). "Main = ours, LEGACY = stock" therefore becomes a per-type swap. Tick: 96 per beat in the Seq thread, NoteOn from it works (`sequencer-interface.md:309`).

### (c) Truly empty clean samplers: FEASIBLE (synth), PARTIAL (drum)
- The stock sampler is **not** a `synth_types` row: it appears only as `synth_presets` rows with `"type":"sampler"` in folder `sampler` (9 rows, ids 161-169, MEASURED). A clean copy = new preset rows in a new folder with unique names (TE finds the current sound by type + name, `lofi/README:47-49`), using 1 of the 33 free sampler slots each; knob slots 5-8 are the sampler's own, so no marker there. Factory sampler audio lives in `content/audio/factory_synth/<name>.raw`, copied to the slot on pick; the stock slot defaults `synth_0..6.raw` are 1 byte (MEASURED), so an "empty" sample is a legal state; a 1-byte raw preset is UNVERIFIED. Drum: same recipe in `drum_presets` (`start`/`end` arrays of 24 zeros) + `factory_drum/<name>.raw`; "Invalid drum patch" validator (`patch.cpp:221`) unchecked. Your existing sounds live in `synth/user`, `synth/snapshot`, `drum/user`, `drum/snapshot` and `op1.db`; factory rows are read at boot and never copied, so adding rows touches none of them.

### (d) Volume knob pin + bipolar gain: FEASIBLE (UNVERIFIED on a device)
- **The knob is digital.** TWI read by an L1 callback (`0xffa0521c`, registered by the function at `0x01a10e58` via `0x01a69238`) into the byte `0x018b62a4` (starts `0xff`: full volume until the first read; op1r patches it to 0 for that reason, `pcppcp-op1r-src/synth_sdk/make_firmware.py:315,353-355`). The function at `0x01a10de4` (CODE, our disassembly) turns the byte into a software gain: target = 2 * (((K << 7) - 2560) * 0x4d00 >> 15) (saturating, floor 0), clamped to 0x7fff; **targets saturate from K ~ 233**; it slews W[`0x01de4f70`] toward the target and writes a step to +2 (limits +-6). Boot state `0x01a10e84` sets the gain word to 0x7fff. The only SDRAM reference to `0x018b62a4` is that function; the knob generates no key event (`te246.h:246`). The codec model is unidentified (UNVERIFIED); docs list "TWI codec", "Class D amp", "potentiometer" drivers (`firmware-246-baseline.md:77`), so a hardware gain stage after the DAC cannot be excluded.
- **Mechanism (built, `src/platform/ofp_gain/`):** entry hook on `0x01a10de4` (first instruction `P1.L = 0x62a4`, replayable) records the real byte and writes `0xfe` back before TE reads it, so TE runs at its flat top (full level); a one-byte patch starts the byte at 0 (silent until the first reading, no boot burst). The bipolar gain runs in our wrapper around the render callback on TE's output words. USB audio is pushed inside TE's callback before our stage, so it is unaffected by the knob/gain (UNVERIFIED which side of TE's own gain the USB ring is on).
- **Output word format (assumed, MUST be confirmed with the probe's TX PK):** the USB ring code copies the output block as 8-byte frames of two 32-bit words (`0x019e1a88`, CODE) and the USB path reads them as 24-bit packets, which matches left-justified words (`int16 << 16`, full scale +-2^31). SPORT0 is configured with 32-bit slots (`TCR2` = 0x021f, `0x191abe0`).

### (e) Custom animated boot screen "OP FREAK": PARTIAL
- Boot text = 8 bytes at `0x018f738a` ("R. 00246"), shown under the TE logo; "OP FREAK" is exactly 8 characters; mechanism device-verified with "op1sdk +" (`te246.h:89`, `build.py:248-253`). The DMA0 frame-done ISR `0x01a2fb68` branches at `0x01a2fb8a` on byte `0x018b6460` to the console puts `0x019416ea` with the string at `0x018f7358` (17 newlines, 33 spaces, "R. 00246"); the assertion printer uses the same puts (`0x01919a20`). The TE logo's source bitmap was not found (4,696 non-zero bytes preloaded at `0x02000000`, not decoded).
- Timing: screens load at `0x019f07d2`, display callback registered `0x019f07de`, DBs load later, `boot_done` hook `0x019f09d6` (main thread, before the main loop). The logo stays until init finishes (`device-tests.md:728,1625`). Whether the `frame` hook fires during init is UNVERIFIED; direct writes to `0x02000000/0x03000000` flicker once TE renders. Plan: probe first (a `boot_done` counter + frame marker), then either draw ~2 s in the frame hook, or block in `boot_done` and drive `TE_FRAME_PRESENT` `0x01a2fb18` with a cache flush. Risk: a stuck init hangs boot; COM at power-on still reaches te-boot (`app-boot-chain.md:35`).

### (f) Purple theme: FEASIBLE
- Palette is data: `content/display/colors.svg` has 20 named colours for dynamic parts (`black #010000`, `darkblue #383572`, `purple #5F4270`, MEASURED); the static parts carry hard-coded hex in each of 61 SVGs (MEASURED counts: stroke `#FF3A5D` 979, `#DFD9FF` 955, `#698EFF` 674, fill `#010101` 120). SVGs load from `/yaffs2/content/display/%s` (`screen-interface.md:82`); recolouring = a build-time find/replace (parser is strict: at most 4 decimals, a bad file halts boot, `op1hacks-docs svg.md:60`, `HANDOVER.md:191`). The back-buffer clear colour is code, not SVG (UNVERIFIED); only 3 SVGs have wide rects.
- Runtime alternative: remap pixels in the `frame` hook through a 128 KB RGB565 LUT over 51,200 pixels (~0.3 M cycles estimated, UNVERIFIED); remap only near-black pixels to keep anti-aliased edges.
- Panel: OLED per a forum quote (`op1hacks-docs hardware.md:63`, DOC; `screen-interface.md:138` says "LCD" generically). OLED caveat stands: a lit background costs power + burn-in; keep the background under ~RGB(12,0,20) (a design limit from PLAN, not a measurement).

### (g) G-sensor / tilt: FEASIBLE, the hardware exists
- Evidence: `op1hacks-docs hardware.md:64` ("345B is an accelerometer IC", DOC); `firmware-246-baseline.md:77` lists an accelerometer driver; image strings `..\src\bf\xyz.cpp:40 twi_rx`, `:41 s_read_xyz`, `:74 s_read_xyz < 5` (`0x0190be4c`). CODE: `0x01a8e340` requests TWI device `0x53`, register `0x32`, 6 bytes; the completion callback `0x01a8e2a8` stores little-endian int16 X, Y, Z at `0x01dff338 / 33a / 33c`; init `0x01a8e3b0` called from OP_Main at `0x019f059a`. Address 0x53 + register 0x32 matches an ADXL345 DATAX0..DATAZ1 (UNVERIFIED, datasheet memory). Other TWI devices: MCP23017 expanders `0x22` (master) and `0x23-0x26` key banks (+ `0x20`), TWI codec, LTC2941 gauge, RTC (`screen-interface.md:356-365`; `te246.h:203` says 0x20/0x27: mismatch).
- Read the three int16 words directly from a hook. Never issue our own TWI transactions: more than 5 outstanding reads hits a fatal assert (`0x01a8e36e`). Polling is by L1 (no SDRAM caller of `0x01a8e340`). Update rate, axis orientation and scale UNVERIFIED: **the probe image shows the raw words (page 1, "G x y z")**.

### (h) USB audio code path and crackle suspects
- Stock path: L1 `usb_audio::push_lr` `0xffa049fc` / `push_interleaved` `0xffa0487c` (callback side) write 64 frames per block into a **512-frame ring at `0x01aaeb60`, 8 bytes per frame (two 32-bit words)**, write index `0x018b5fe4`, read index `0x018b5fe8` (free-running; `(wr + f) & 0x1ff`); the packet builder `0x019e161c` reads it per 1 ms iso packet (<= 46 frames x 6 bytes, 24-bit). Iso IN call sites `0x019e151c`, `0x019e1812` (start-up, silent), `0x019e161c` (data). An underrun repeats the last frame; a resync jump is rare (`usbtrack.c:139`).
- **MEASURED (our dump at `0x018ff1c8`): `86 25 14 01 01`** = endpoint 0x86 IN, `bmAttributes 0x25` = isochronous, **asynchronous**, **implicit-feedback data** endpoint, `wMaxPacketSize` 276 (46 x 6), bInterval 1. This resolves the earlier "adaptive/async unknown": the device is the rate master (codec clock) and must vary packet length (44/45/46 frames) to follow it; the producer is 64 frames per 1.451 ms, the consumer 1 packet per 1 ms. TE's template is built at runtime, so the endpoint reading is an inference from the stock template bytes.
- Ranked suspects (HYPOTHESES, none reproduced): (1) packet-size logic vs the codec clock (off-by-one, or fixed 44/45 pattern that does not track the ring lead) -> drift to under/overrun; (2) CPU peaks / interrupt latency (one core runs voices, FX, tape, UI and both interrupts; op1r hook.S:443 reports crackle when the block budget is exceeded); (3) tape/flash writes stalling the main thread while USB runs (YAFFS lock); (4) cache coherence on the iso buffers (not the stock path; only op1r's 6-channel `iso_build` calls USB_ISO_IN from write-back SDRAM with no visible flush); (5) 96 KB USB TLSF pool contention.
- **Probe instrumentation (built, page 2):** ring lead sampled before every push (min/max/now), UND (lead <= 0), OVR (lead > 480 after push), JUMP (lead deviates > 192 from the prediction = resync), DRIFT (mean lead change since the stream started, in frames, with elapsed seconds), LATE blocks while USB is active; page 1: CPU %, audio ISR busy %, HOT / LATE blocks, jitter. Test plan: record USB with `pcppcp-op1r-src/tools/usb_check.py` (gap detector) while the overlay runs: idle vs heavy engines vs tape writing; correlate gaps with UND/JUMP/LATE counts.

### (i) Stereo tape: PARTIAL (faithful rewrite: no; alternatives: yes)
- Plain SDRAM, patchable: length literals (`0x018f6314`, `0x018fb5d0`, `0x018fbcf0`), `TE_TAPE_MAX` seek assert, chunk math (`<< 16` bytes, `>> 15` samples, `tape/erase.c`, `safewrite.c`), clip table (`0x01aa8d70`, 512 records in samples), `tape_c::service` `0x01a54710` + `write_chunk` `0x01a54b08`, the `0x02040000` cache sizing (consumer unknown).
- **Encrypted L1 (only 8 of every 24 bytes are XTEA-scrambled, key in OTP/te-boot, which we do not have):** tape mixer loops `0xffa0709c` (fwd) / `0xffa0712c` (rev), the resampler `0xffa06194`, the record path (`tape_c.cpp:1039` assert). op1r patches L1 only by precomputed byte diffs from a private tool that holds the key (`l1_patches.json`, 10 ops; trampolines at `0xffa0bfb4`, `make_firmware.py:146-151`). Keeping the units as 16-bit interleaved samples would leave file layout, chunk math and `kTape_length` unchanged, but record, play and mix still need L1 changes; on-screen counters, tempo sync and markers would double their time scale.
- Cheapest alternatives: (1) track pairs: tracks 1+3 and 2+4 as L/R, the pan array `0xff801930` already pans per track and the master bus is stereo (recording stays mono per pass); (2) our own recorder/player in the audio-callback wrapper writing the already-stereo album sides (63.5 MB, 2 ch, 360 s each): no layout change, no L1 patch, but we then own the recorder and player and bypass stock clips.
- Flash bandwidth for 2x is UNVERIFIED (probe measures `tape write` ms).

## 5. Audio callback facts used by the platform layer (CODE, our disassembly)
- The render callback pointer is `[0xff80036c]`. `audio::init` (`0x0191ab40`) stores `0xffa04804` there directly (`R1.L/H` at `0x0191ab52/56`, no setter); other code re-installs it through the setter `0x0191a7cc` (callers at `0x019ab368/380/398/3b8/3d8`, restore at `0x019ab300`) with the muted/other variants `0x0191a7a8` and `0x0191a880`. SPORT0 `TCR2` = 0x021f (32-bit slots); DMA = 128 words = 64 stereo frames per block; the L1 interrupt handler calls the callback with `(tx, rx)`.
- **Wrapper (ofp_audio):** entry hook (filter) on the setter + a `boot_done` fix-up for the first install; the original callback is always called. ISR cost of the wrapper: two `CYCLES` reads, 4 loads, one weak-function check per feature; the probe adds a 128-word peak scan.
- CPU %: idle thread = `kTE_Idle::run` (vtable slot `0x01911b98`, stock `0x01a5e240`: loops on `IDLE` then runs a MAC kernel); VDK stats block at thread+0x34, total run cycles at stats+0x20 (struct offsets from op1r's sysmon, DOC/GPL reference: facts only, code is ours). Tape write timing: `lseek` call site `0x01a5482e` (-> `0x01a9a0e4`) and `yaffs_write` site `0x01a54842` (-> `0x01a99f84`) in `tape_c::service`.
- Free RAM: TLSF stats at pool `0x0106b7cc` (+4 used, +8 peak; signature `0x2a59fa59`, from op1-sdk `meminfo`); VDK heaps via `0x01a9f2e2(i)` under the scheduler lock `0x01a9f76c/78e`; flash free via `0x01a9b1c0/0x01a9b284` (op1r sysmon; UNVERIFIED during disk mode).
- **Never run on a device:** the wrapper, setter hook, idle hook, tape stamps, gain stage, knob pin. Compile + hook checks only (build.py verifies every hook against the stock bytes).
