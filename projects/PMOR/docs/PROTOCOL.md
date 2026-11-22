# PMOR STUDIO protocol v1

Spec for the host side (Max external / device, test client). PMOR = Daisy Seed 3 firmware, USB composite: **CDC-ACM** (this protocol) + **USB-MIDI** (clock, notes, CCs). No USB audio.
Status column in section 12 says what firmware implements today. Everything else here is the contract.

## 1. Transport and framing (CDC)

Line based, ASCII, robust to partial reads and resync by construction.

```
frame  := '!' BODY '*' CRC '\n'
BODY   := TAG ' ' CMD { ' ' ARG }
CRC    := 4 upper-case hex digits, CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflect, no xorout) over the BODY bytes
```
* BODY is printable ASCII 0x20..0x7E without `*`. Max frame 480 bytes incl. `!`..`\n`. Binary data is base64 (RFC 4648, with `=` padding).
* TAG = 1..4 chars `[A-Za-z0-9]`, chosen by the host; every direct reply carries the request TAG. Unsolicited device lines use TAG `-`.
* Receiver rule (both sides): accumulate bytes; on `\n` take the line; if it does not start with `!` it is ignored by the host and passed to the legacy text console by the device (`dfu`, `status`, `help`... keep working); otherwise verify CRC. Bad length/CRC: drop it. A device that drops a host frame answers `!- NAK bad_crc*..` (rate limited to 10/s) so the host can resend. Resync after garbage or a half frame = wait for the next `\n`. Nothing else is needed.
* Reply classes: `OK <CMD> [k=v ...]` (final, success), `ERR <CMD> <code> [text]` (final, failure), and data lines (`SCHEMA`, `P`, `M`, `C`, ...) that precede the final `OK`. A request is complete only when its `OK`/`ERR` arrives. One request outstanding per TAG; different TAGs may overlap. One multi-line request (GET_SCHEMA, GET_PARAMS, GET_SEQ, GET_MANIFEST, GET_PATCH, PULL_SAMPLE) at a time: a second one while it runs gets `ERR <CMD> busy`. Single-line requests (HELLO PING SET_PARAM AUDITION ASSIGN_SAMPLE ...) are always answered immediately, even during a multi-line reply.
* Numbers: integers decimal; floats decimal with up to 6 fractional digits (`-0.25`, `440`, `0.333333`), no exponent. Hex is upper case, no prefix.
* Port: open the CDC port **with DTR asserted** (PMOR treats DTR low / port closed as "host gone", section 3). 115200 8N1 is ignored (USB), any baud works.
* Example (CRC illustrative): `!a1 HELLO STUDIO 1*XXXX\n`.

## 2. Modes

Mode is chosen by the host's `HELLO` and is lost when the host goes away.

| mode | meaning | clock | speakers |
|---|---|---|---|
| STANDALONE | no hello (default after boot, after BYE, after unplug / port close / liveness timeout) | internal / today's rules | on |
| NEPTR | the Pi/NEPTR rig (today's behaviour, protocol is read-only) | unchanged (PMOR clock / CC116 roles) | on |
| STUDIO | Max device in perform mode, Ableton is clock master | follows Live's USB MIDI clock exactly (section 6) | **muted** |
| EDIT | Max device editor open | same as STUDIO | **muted** |

`HELLO <NEPTR|STUDIO|EDIT> <host-proto-version>` -> `OK HELLO fw=<git-short> proto=1 mode=<MODE> schema=<8hex> patch=<slot 0..63> clk=<internal|slave|master|follow> sr=48000 block=64`
* `fw` is the firmware git short hash (string, no spaces), `schema` the param-schema hash (section 4). Host compares both against what it was built for and warns on mismatch; the schema it uses is always the one it reads from the device (`GET_SCHEMA`).
* HELLO again while connected = mode change / resync request; the device answers the same `OK HELLO` and then re-pushes nothing (host pulls state, section 5).
* `BYE` -> `OK BYE`, device returns to STANDALONE (speakers unmute, clock back to its settings).
* `PING` -> `OK PING up=<ms>`. In STUDIO/EDIT the host must send any frame at least every **2 s** (PING if idle); the device falls back to STANDALONE after **5 s** of silence. NEPTR mode has no timeout.
* Replug / reopen: port closed or DTR low -> STANDALONE immediately, speakers unmute. The host reopens, sends HELLO, then performs the **full resync from PMOR**: `GET_PARAMS`, `GET_SEQ 0..2`, `GET_MANIFEST` (section 5). The host never pushes its remembered state back.

Allowed commands per mode (anything else -> `ERR <CMD> mode`):

| cmd | STANDALONE | NEPTR | STUDIO | EDIT |
|---|---|---|---|---|
| HELLO BYE PING | yes | yes | yes | yes |
| GET_SCHEMA GET_PARAMS GET_SEQ GET_MANIFEST GET_PATCH STATUS | no | yes | yes | yes |
| SET_PARAM | no | no | yes | yes |
| AUDITION | no | no | yes | yes |
| PULL_SAMPLE | no | no | yes | yes |
| ASSIGN_SAMPLE DELETE_SAMPLE CLEAR_PATCH CLOCKMODE | no | no | no | yes |
| device pushes (`P`, `PATCH_CHANGED`, `SEQ_CHANGED`) | no | no | yes | yes |

## 3. Priority and bandwidth rules

1. USB-MIDI clock and notes always win. The device never emits a CDC line while a MIDI clock/note packet is waiting, and sends **at most one bulk chunk (section 8) per 2 ms** and only when the MIDI TX FIFO is empty. Bulk traffic is host-paced (pull model): the host decides when to ask for the next chunk, and SHOULD pause pulling while it is itself busy with clock/notes.
2. Param pushes (`P`) are small and coalesced: one line per id per 5 ms at most, latest value wins.
3. SD is touched only from the device's background loop, never from the audio callback. A request that needs SD (GET_MANIFEST, PULL_SAMPLE of a non-live sample, ASSIGN_SAMPLE) may reply late; the final `OK`/`ERR` is the only completion signal.
4. Serial work runs only while a host is connected (DTR high); otherwise the protocol layer is idle (zero cost).

## 4. Schema (GET_SCHEMA)

Every engine and FX parameter is `{id, name, min, max, default}` (+ `flags`, `group`). `id = (domain<<10) | index`, ids are stable forever (append only).

`GET_SCHEMA` -> n lines `SCHEMA <id> <name> <min> <max> <def> <flags> <group>` then `OK GET_SCHEMA n=<count> hash=<8hex>`.
* `id`: decimal. domain 1 SYNTH, 2 DRUMS, 3 SAMPLER, 4 SEQ, 5 FX, 6 GLOBAL, 7..10 DRUM SYNTH (pad p: domain 7 + p/4, index (p%4)*256 + k; each pad runs its group's engine (KICK/SNARE/HAT/PERC: one core, four exposed parameter sets with engine-specific names, 64/80/70/87 params), groups `<ENGINE>.P<pad>.<LAYER>`; id index = global field k (drum_super.cpp SDEF_PARAMS order, append-only; sparse per engine); changing the pad's SOUND loads that preset's params into them).
* `flags` (decimal bitmask): 1 QUANT (integer steps), 2 PAIR_X, 4 PERCENT, 8 ENUM, 16 BIPOLAR, 32 HIDDEN, 64 INT.
* `group`: e.g. `SYN.A3`, `DRUM.PAD8`, `SMP.SLOT2`, `SEQ.SYN`, `FX5`, `CLOCK`.
* hash = FNV-1a 32 over, per entry in table order, `id name min max def flags group` as the same decimal text joined by single spaces and entries by `\n`. The host can recompute it from the dump.
* Index layout: SYNTH `algo*32+k` (algo knobs 0-3, env 4-7, filter 8-14, pitch 15-17, width 18-19), 256 ALGO, 257 VOLUME, 258..261 LFO. DRUMS `pad*8+k` (0 LENGTH 1 PITCH 2 TONE 3 REV 4 VOL 5 SOUND), 256..259 group volumes, 260 VOLUME, 272+g*8+k engine depth knobs, 320+g*16+k deep recipe multipliers. SAMPLER `slot*8+k`, 256.. mix. SEQ `engine*32+k` (engine 0 drums 1 synth 2 sampler). FX index = effect id `bank*16+pad` (0..127): a 0/1 *engage*; 128 = distortion algo. GLOBAL 0 BPM 1 SWING 2 FADER 3 INGAIN 4 KEY 5 SCALE 6 METRO.

## 5. State and sync (PMOR is the source of truth)

* `GET_PARAMS` -> lines `PARAMS <id>=<val> <id>=<val> ...` (as many per line as fit) then `OK GET_PARAMS n=<count>`. Values are the live engine values.
* `GET_SEQ <engine 0..2>` -> one or more `SEQ <engine> <bars> <steps_per_bar> <mode> <step_offset> <hex>` lines (each step a 16-bit lane mask = 4 hex digits, up to 90 steps per line, offsets ascending, bars*steps_per_bar steps in total) then `OK GET_SEQ`.
* `GET_PATCH` = `GET_PARAMS` + `GET_SEQ 0..2` + `PATCHINFO slot=<n> samples=<16 lens>` + `OK GET_PATCH slot=<n>`; always the **current** patch. The device never reads or writes other slots on request, and **never accepts a whole patch from the host**: patches are created/edited/saved on PMOR only.
* `SET_PARAM <id> <value>` -> `OK SET_PARAM id=<id> val=<stored>` (value clamped/quantised to the schema; `stored` is what PMOR now holds) or `ERR SET_PARAM range|unknown`. PMOR does not echo this to the originating host.
* **Push (PMOR -> host), every edit, live:** `!- P <id> <value>*CRC` whenever a schema param changes on PMOR by any cause (encoder, joystick, pad, IMU macro, patch load). Pushes are only sent in STUDIO/EDIT. **Loop rule:** the host must not turn a received `P` into a `SET_PARAM`.
* `!- PATCH_CHANGED <slot>*` after PMOR loads/creates/switches a patch. The host then pulls `GET_PATCH` (whole thing) and replaces its model. Pushed `P` lines for the changed values are suppressed while the patch load applies (PATCH_CHANGED is the single event).
* `!- SEQ_CHANGED <engine>*` when sequencer steps/bars/mode of an engine change (host re-pulls `GET_SEQ`). Debounced 100 ms.
* Non-schema live state the host may display but cannot set: `STATUS` -> `OK STATUS mode= clk= running=<0|1> bpm= bar= cpu=<0..100> audio_overruns= midi_drops=`.

## 6. Clock and transport in STUDIO / EDIT

* Ableton Live sends its MIDI clock output directly to PMOR's USB-MIDI input port (not through Max). PMOR becomes a pure follower: 24 PPQN `F8`, `FA` Start, `FB` Continue, `FC` Stop, `F2` Song Position Pointer (units of 6 clocks = one 16th) are honoured. Start = position 0 and the grid re-anchors on that tick; SPP+Continue = jump to that 16th and run; Stop halts the sequencer (no steps fire; held voices release); tempo is the PLL on the incoming ticks (no BPM param writes from the host are honoured while following: `SET_PARAM` of BPM answers `ERR SET_PARAM follow`).
* PMOR emits **no MIDI clock/start/stop** while following (the Pi/NEPTR tap roles `CC116/118/119/120/121 ch16` are NEPTR-mode only and are not emitted in STUDIO/EDIT).
* Speakers: muted in STUDIO/EDIT (all of PMOR's own audio out). `AUDITION` is the one exception: it sounds on PMOR for its duration even in these modes.
* Manual clock-mode setting (PMOR settings, stored in QSPI globals): `AUTO` (follow what the hello says, default), `INTERNAL`, `FOLLOW`, `MASTER`. A non-AUTO setting overrides the clock column in section 2 but not the mute.

## 7. MIDI (USB-MIDI, one cable, 16 channels) - what records into Live

Everything that sounds is also sent as plain MIDI so a recorded take plays back exactly. Channels (1-based):

| ch | content | detail |
|---|---|---|
| 1 | drums | note = 36 + pad (pad 0..15 = trellis index row*4+col). Note-on vel = hit velocity*127 (>=1). Note-off 30 ms later (one-shots). Sequenced, generative, stutter-retriggered and hand hits all emit. |
| 2 | synth | sequencer **and generative** output as plain notes: note-on at note-trigger with the *resolved* pitch (after scale, key, octave/oct_prob/arp decisions), nearest MIDI note of the synth's actual Hz (`round(69+12*log2(hz/440))`), vel*127; note-off at the moment the voice enters release (sequenced gate end or pad release). Voices are never multiplexed: simultaneous notes of the same pitch get a note-off first. Microtonal key offset is sent as pitch bend (+-2 st, 14 bit) before the note-on when non-zero. |
| 3 | sampler | note = 36 + slot (0..15), vel = hit velocity; note-off 100 ms after note-on (one-shot). |
| 4 | control | CC1 FSR, CC2 lidar, CC3 accel X, CC4 accel Y, CC5 joystick X, CC6 joystick Y (all 0..127, bipolar axes centred 64), CC7 master fader. Sent on change, <=100 Hz. CC110..CC113 = random-seed (below). |
| 5 | fx engage | CC number = fx id (bank*16+pad, 0..127): 127 on engage, 0 on release. |
| 16 | NEPTR roles | unchanged from today (CC112/116/118-121), NEPTR mode only. |

* Host -> PMOR MIDI in: clock/transport (section 6). Notes/CC into PMOR are ignored in STUDIO/EDIT (the Max core device renders audio; PMOR is a controller + clock follower there).
* **Random-FX / generative seeds.** PMOR's random streams (FX noise/glitch/stutter RNGs, sampler pan RNG, generative sequencer dice, synth per-note character) are all LCGs. PMOR reseeds them from one 28-bit seed `S` at every transport Start (and SPP jump to 0). Right after reseeding, before the first tick it sends on ch 4: `CC110=S>>21, CC111=(S>>14)&127, CC112=(S>>7)&127, CC113=S&127` (hi..lo). The Max core device applies the same seed on receipt of CC113 (the last one) to its identical engine (`pmorReseed(S)` in `pmor_core`, same code) so playback of the recorded notes/CCs reproduces the sound. `S=0` means the legacy fixed seeds (bit-identical to firmware before STUDIO). PMOR chooses S from the transport start sample count (any value; recorded in the clip as those four CCs).

## 8. Samples

Format: mono, 48000 Hz, int16 little-endian (the `.AUD` sidecar format), up to 96000 samples (2 s) per slot.
* `GET_MANIFEST` -> lines `M <id> <len_samples> <rate> <ch> <ver8hex> <name>` then `OK GET_MANIFEST n=<count>`. `id` = `patch*16+slot` (0..1023) naming a sample of a patch slot; opaque to the host otherwise. `ver` = opaque 32-bit version TOKEN for host caching: key (id, len, ver). It is **not** a CRC-32 the host can verify a download against (the field is named `crc` in some code and in section 13, but treat it as a version token): chunk CRCs on `C`/`F` lines are the integrity check. For the current patch live slots it is the CRC-32/IEEE of the int16 data, for samples that sit on the card it is the file timestamp (FAT date<<16 | time) or 0. `name` is `P<nn>S<ss>`. The current patch's live slots are included with their live contents. Scanning SD happens in the background; the reply may take up to seconds.
* `PULL_SAMPLE <id> <byte_offset> <max_bytes> [count]` (max_bytes <= 192, even; count 1..16, default 1 = number of consecutive chunks to send, pipelined, each at least 2 ms apart) -> `count` x `C <id> <offset> <nbytes> <crc32hex of this chunk> <base64>` (offsets ascending, each chunk `max_bytes` long except the last of the sample) then `OK PULL_SAMPLE id=<id> offset=<offset> n=<nbytes> total=<total_bytes>`. When `offset >= total`: `OK PULL_SAMPLE id=<id> offset=<total> n=0 total=<total>`. The host verifies each chunk CRC and re-requests on mismatch; **resume = request from the first offset not yet verified**; a whole-stream CRC-32 is not provided by the device (the host computes it while pulling). The final `OK` carries the offset and size of the LAST chunk sent. Host-paced: at most one outstanding PULL per id; measured on the real PMOR: ~28 chunks/s with count=1, see section 12 for count=8. The device serves chunks only in bus gaps (section 3), expect ~100-500 chunks/s.
* `ASSIGN_SAMPLE <slot 0..15> <id 0..65535>` (EDIT only) -> copies pool sample `id` into the current patch's sampler slot. `OK ASSIGN_SAMPLE slot= id=` immediately when queued, then `!- SAMPLE_READY <slot>*` (or `!- SAMPLE_ERR <slot> <code>*`) after the background load. The patch becomes dirty (autosaved by PMOR as usual).
* `DELETE_SAMPLE <slot>` (EDIT only) -> empties the current patch's slot in memory (same as the on-device delete). Never touches files; hard delete is a separate editor tool.
* `CLEAR_PATCH YES` (EDIT only) -> resets the *live* patch to boot defaults, **without** marking it dirty, so nothing is overwritten on the card unless the player then edits and PMOR autosaves. `ERR CLEAR_PATCH arg` without `YES`. Followed by `PATCH_CHANGED`.
* `AUDITION D <pad> <vel0-127>` / `AUDITION S <pad> <vel>` (synth pad -> scale note, auto note-off after 600 ms) / `AUDITION M <slot> <vel>` -> `OK AUDITION`. Plays on PMOR's output even when muted; also emits the usual MIDI notes.

## 9. Errors

`ERR <CMD> <code> [text]`; codes: `mode` (not allowed in current mode), `unknown` (id/cmd), `arg`, `range`, `busy` (retry later), `follow`, `io` (SD), `crc`, `nohello`.
Unknown commands: `ERR <CMD> unknown`.

## 10. Host implementation notes

* Send HELLO first, always. Until then the device ignores everything except `HELLO`, `PING`, `BYE` and legacy text lines.
* Keep one reader that splits on `\n`; never assume one `read()` = one frame.
* Recompute and check the schema hash; if it differs from the compiled-in one, use the dumped schema (it is authoritative).
* Reference implementation (encoder/decoder, CRC, all commands) is `tools/pmorctl.cpp` (host C++17, runs against the simulator and the real port) - copy its framing code.

## 11. Versioning

`proto=1`. Additions append commands/fields; a changed meaning bumps `proto`. The schema hash changes whenever parameters change.

## 12. Implementation status (twink/pmor-studio)

Implemented in firmware and verified on the real PMOR with `tools/pmorctl port /dev/ttyACM0 test` (42 checks) - see docs/STUDIO_STATUS.md for numbers and known gaps:
framing/CRC/NAK/resync, HELLO modes + liveness + DTR-drop to STANDALONE, per-mode command matrix, GET_SCHEMA/PARAMS/SEQ/PATCH/MANIFEST/STATUS, SET_PARAM (+live P pushes, PATCH_CHANGED, SEQ_CHANGED), AUDITION, PULL_SAMPLE (CRC per chunk, resume by offset, `count` pipelining), ASSIGN/DELETE_SAMPLE, CLEAR_PATCH, CLOCKMODE (QSPI-persisted), MIDI notes ch1-3 + CCs ch4/ch5 + seed CC110-113, STUDIO/EDIT external-clock follower (Start/Continue/Stop/SPP) with speaker mute.
Deviations from the text above: `M` lines carry a `ver` that is 00000000 for card samples (the Pi-side card has no RTC, FAT stamps are 0) - key host caches on (id, len); the control-CC rate is <=100 Hz with a deadband of 2.

## 13. Sample ids after the SD-layout migration (docs/STORAGE.md)
* `GET_MANIFEST` serves the new MANIFEST.TXT pool when it exists: `M <id> <len> <rate> <ch> <crc32hex> S<id>`; new pool ids are >= 1024 and global (a sample shared by several patches appears once; the `crc32` token covers the PCM at export time but is a version token for hosts, see section 8). Without a manifest the legacy `patch*16+slot` ids are served as before.
* `PULL_SAMPLE`, `ASSIGN_SAMPLE` accept both (`ASSIGN_SAMPLE` id range is 0..65535 on macbert/max: twink/pmor-studio still rejects ids > 1023 with `ERR ASSIGN_SAMPLE arg`, contradicting this line; the device side `patchAssignSample` already handles pool ids): a pool id, or an old `patch*16+slot` id (alias; resolved to the pool copy when the migration exported it).
* `PATCHINFO` carries `ids=<16 comma-separated pool ids>` (0 = empty slot) for the current patch so a host knows which pool samples belong to it (the old `id/16 == patch` rule no longer holds for pool ids). Absent on a card that has not been migrated.
* `STATUS` has `store=<state>/<pending>/<exported>/<verified>/<skipped>/<errors> pool=<n>`: state 0 idle, 1 migrating, 2 done, 3 done with errors.

## 14. Bulk transfer, file dumps, recovery commands (phase 4)
* **Chunk size:** `PULL_SAMPLE` / `PULL_FILE` accept `max_bytes` up to **1024** (was 192) and `count` up to 16; one `C`/`F` line is then up to ~1.4 KB. The device bursts up to 4 ms per main-loop pass, servicing USB and the MIDI clock between chunks. Hosts should request `1024 8` or `1024 16`; 192 still works.
* **`PULL_FILE <kind> <slot> <offset> <max> [count]`** (STUDIO/EDIT, read-only, `F <kind> <slot> <offset> <n> <crc32hex> <b64>` lines then `OK PULL_FILE kind= slot= offset= n= total=`): kinds `P` patch .PMR, `K` .PMR.BAK, `A` .AUD, `L` .AUD.BAK (slot 0..63); new tree: `M` MANIFEST.TXT, `T` PATCHES/Pnn.TXT (slot), `R` PRESETS/<engine#0..11>.TXT, `W` SAMPLES/S<id>.WAV (slot = pool id up to 65535). A request at offset 0 re-opens the file (never stale); sequential offsets reuse one open handle.
* **`SAVE_SCRATCH <slot>`** (EDIT): writes the live patch into an EMPTY slot through the normal dual-layout save path; refused (`ERR SAVE_SCRATCH mode`) if the slot holds a patch or is the current one.
* **`STORE_RETRY`** (STUDIO/EDIT): clears a sticky skip-storage boot flag and re-queues all storage tasks (migration, PRESETS) without a power cycle -> `OK STORE_RETRY`. `patchStoreRetry()` (store/patch.h) is the hook for a UI menu entry (not wired: ui/ is builder 2's).
* **`NOSAVE <0|1>`**, **`CLOCKMODE`**: unchanged. NOTE `NOSAVE 1` also pauses the background storage job (it returns early from the patch tick), so do not leave it on while a migration is expected to run.
* **STATUS** fields added: `sd=<sectors>/<spi_us>/<service_us>` (card reads since boot), `boot=<state>/<count>`, `stack_free=<bytes>`, `store=<state>/<pending>/<exported>/<written>/<verified>/<skipped>/<errors>`, `pool=<n>`, `ferr_us=<mean>/<p99>/<max>`, `rx=<irq>/<cb>/<pk>/<nostamp>`, `late_us`, `iv_us`.

## 15. JOIN (host connects while the DAW already plays)
A host that connects mid-song receives a running 0xF8 stream but no 0xFA/0xFB, so PMOR would stay `running=0`. **`JOIN <spp16>`** (STUDIO/EDIT, proto stays 1, additive, schema hash unchanged):
* Meaning: *the 0xF8 nearest in time to this request's arrival is tick 0 of 16th `<spp16>` (0..16383), running.* The request is stamped by the USB interrupt when its line arrives (not when it is parsed). If the last processed tick is within half a tick period of that stamp it is used; if the next tick is nearer it is awaited (up to 500 ms); up to two ticks back are considered when the request is stamped before the last tick.
* PMOR then runs the SAME path as Continue-with-SPP (grid re-anchored at `<spp16>`, follower statistics reset, running = true); the tick counter is aligned so later phase corrections land on tick indices 0, 6, 12 counted from the chosen tick; the tempo estimator is kept (the stream has been running). Already running: it re-anchors anyway (a relocate).
* Reply, after it is applied: `OK JOIN spp16=<n> tick_at=<late_us>` (`late_us` = request time minus the chosen tick, microseconds, signed). Errors: `ERR JOIN noclock` (not following, or no F8 within the last 500 ms, or the awaited tick never came), `ERR JOIN arg`, `ERR JOIN mode`.
* **Host contract:** send JOIN at a Live 16th boundary (a Max transport-synced metro at 16n) with `spp16` = that boundary's 16th index (Live song position / 6 ticks). Latency up to +-half a tick period (~10 ms at 124 BPM) is absorbed; the host must not delay it further. After JOIN, send `GET_PARAMS`/`GET_PATCH` as usual; `STATUS running=1`.
* Test (`tests/test_follower`): stream running without Start; JOIN at 5 boundaries with -5..+4.5 ms latency, with and without jitter/bunching: |offset mean| <= 0.12 ms, ferr p99 <= 1.0 ms, labelled positions identical to a normal Start run for 32 bars; JOIN with no clock refused.

**STATUS / HELLO `role=<master|follow>`** (additive): the LIVE clock role right now, i.e. what AUTO resolved to (or the forced mode): `follow` while a bus clock arrives and PMOR follows it, `master` when PMOR drives the clock (AUTO returns to master ~2 s after a followed clock stops). Independent of the STUDIO/EDIT session; `clk=` keeps its meaning (`follow` there means the STUDIO follower is forced). **TX watchdog `tx_stalls=<n>`** (additive): count of self-heals. If bytes are waiting (ring non-empty or the CDC FIFO full) and the host accepts nothing for 2 s while the port is open, the device drops the queued pushes, clears the CDC TX FIFO and the session (back to STANDALONE; the host re-sends HELLO, which is also always answered: a HELLO with no room in the queue flushes the stale backlog first). No USB replug needed.

**Start alignment and the metronome (follower).** Start/Continue re-anchor the grid at the 0xFA/0xFB's own IRQ stamp (not at main-loop time) and the first boundary IS step `pos` (0 for Start, the SPP for Continue; before this the first fired step was labelled pos+1, i.e. the pattern sat one 16th off the host bar). A Start that shares a USB transfer with clock ticks is no longer back-dated like a tick, and no phase correction is measured against the grid while a re-anchor is queued: a main-loop stall of 16 ms around Start moved one boundary by ~16-20 ms (the "2nd note after Start" lateness), now <0.5 ms. The metronome is gated by the followed transport the moment the 0xFA/0xFB/0xFC is parsed: silent from Stop until Start/Continue (STUDIO/EDIT gate sequencer + metronome; AUTO-follow outside a session gates the metronome only), first click on the Start tick, accent on the bar downbeat at the SPP. The OLED metro dot and the playhead (OLED hand-sequencer and pads) use the same gate (`pmorFollowedRun()`): dark while a followed transport is stopped. Master / standalone: ungated. **Tempo learn:** the follower now takes its first tempo from the 3rd tick after a cold Start (was the 7th, with a median of 3+): a cold Start no longer runs ~a beat at the previous tempo (steps rushed by up to ~60 ms; converged only after ~12 steps, now after 3). Measured: long-run lock 100 BPM, +/-200 ppm host clock, 5 min: |offset| < 0.55 ms, no drift (`test_follower`).

**`TEMPO <bpm_milli>`** (STUDIO/EDIT; additive): seeds the follower tempo. Range 40000..400000 (e.g. `TEMPO 100000` = 100.000 BPM), else `ERR TEMPO arg`; reply `OK TEMPO bpm=<milli>`. When no F8 arrived in the last 200 ms (or the transport is stopped) the engine tempo and so the grid period are set at once; while F8 streams the PLL still rules (no override, no phase jump) and the value is only kept as a hint. At the next cold Start the hint pre-fills the tempo estimator (median of 5), so the first real estimates replace it one by one: a cold Start lands at the right tempo from tick 0 (measured: PMOR at 120, seed 100, every boundary from the 2nd within 0.45 ms; `TEMPO 101000` mid-stream at 100: max offset 0.08 ms). Hosts should send it on connect, on a Live tempo change and right before Start.

**FX over MIDI** (STUDIO/EDIT sessions; additive). PMOR is where FX are held, recorded and looped; Live records/edits the same automation and can drive PMOR back.

| ch | CC | meaning | value |
|---|---|---|---|
| 5 | 0..127 | engage of FX id = bank*16+pad (names in GET_SCHEMA group FX0..FX7) | 127 on / 0 off |
| 6 | 1 | FX amount (effective FSR macro: live FSR, recorded loop amount or MIDI-in, the max) | 0..127 = 0..1 |
| 6 | 16..23 | bank 1..8 AMOUNT floor (schema 0x1000+130..137... = PD_FX 130+k) | 0..127 = 0..1 |
| 6 | 24..31 | bank 1..8 LIDAR floor (PD_FX 140+k) | 0..127 = 0..1 |
| 6 | 33,34,36..39 | bank 2,3,5..8 MIX wet/dry (PD_FX 151,152,154..157; bank 1 has no MIX). **CC35 (bank 4 MIX, PD_FX 153) is NOT sent or accepted as MIX: it is the octave LSB below; bank 4 MIX stays reachable by SET_PARAM / P pushes** | 0..127 = 0..1 (127 = untouched) |
| 6 | 2 | perform: tilt filter amount (`fx.tilt_t`) | 0..127 = 0..1 |
| 6 | 3 (MSB) + 35 (LSB) | perform: synth octave (`syn.perf_octave`, stick Y), 14 bit `8192 + clamp(oct,-2,2)*4095.5`; both CCs sent whenever the 14 bit value changes (MSB first) | 8192 = 0, +-2 octaves at the ends |
| 6 | 4 | perform: envelope macro (`syn.perf_env_macro`) | 0..127 = 0..1 |
| 6 | 8,9,10,11 | perform: drum/sampler throws up/right/down/left (`fx.perf_up/right/down/left`) | 0..127 = 0..1 |
| 6 | 12 | perform: engine (`fx.perf_engine`) | 0 drums, 1 synth, 2 sampler, 127 none |

The perform CCs (2,3/35,4,8..12) are perform STATE for the host: the MIDI note-on still carries only the scale pitch (the octave is a smoothed multiplier applied inside the synth, so folding it into the note would double count). Changes only (deadband 2, exact at 0/127; CC12 on any change), same token bucket as the other ch6 CCs, gated every 5 ms like `fxMidiPoll`, baseline recorded without sending on session arm. They are output-only: incoming ch6 CC 2/3/4/8..12/35 are ignored by the firmware. Not exposed as schema params (one mechanism).

**Follower clock lead** (schema `PD_GLOBAL` 7 `CLKLEAD`, group `CLOCK`, -20..+40 ms, integer, default 0, additive): while following an external clock the grid boundary is held this many ms EARLIER than the followed 16th (target phase error = -lead), compensating the host's output latency (positive = PMOR plays earlier). Applies to the phase lock, the cold snap and the freewheel-resume tick choice; no effect while PMOR is clock master. Persisted per setup in the QSPI Globals record (appended field, GLOB_VERSION unchanged: an old record's erased tail reads NaN and falls back to 0); not saved in patches. Test: `tests/test_follower.cpp` "clock lead".

(ch 4 stays sensors CC1-7 + seeds 110-113, ch 1 drums, ch 2 synth, ch 3 sampler, ch 16 sync; ch 5/6 were free apart from the old engage CCs on ch 5, now the effective state.) The ch5 CC is the EFFECTIVE engage (physical hold | PMOR's loop playing | MIDI-in), so Live sees PMOR's loop play back, not only live holds. The three param groups are schema params (GET_SCHEMA / GET_PARAMS / `P` pushes / SET_PARAM, group `FX<k>`, units %, linear): PMOR's DSP has exactly one FSR macro and one lidar macro per bank (the effects' depth/rate/feedback all ride them), so those are the per-bank dials; a per-effect rate/time/mix would need each of the 128 verbatim v2 blocks rewritten and is not done. They are session-live (not stored in patches, not recorded in PMOR's loop); the loop records engage + amount only. Defaults (AMOUNT 0, LIDAR 0, MIX 1) are identical to v2 (golden renders bit-identical). **Rules.** Only changes are sent, <= 200 CC/s total (token bucket, burst 16); nothing is sent until a session is armed (the baseline is the state at that moment). PMOR never re-sends a value it received. An incoming CC that equals what PMOR last sent, or arrives within 100 ms of PMOR's own send for that id, is an echo and ignored: Live echoing PMOR's loop neither double-applies nor re-records (recording reads only the physical hold). A genuine incoming engage edit takes that FX over from PMOR's loop (the loop is paused for that FX) until the next physical hold of it; incoming CCs are only accepted while a STUDIO/EDIT session is active. **Schema:** adding the 23 params changed the schema (509 -> 532 entries, hash `70033068` -> `9C759588`); hosts re-read GET_SCHEMA on a hash change as usual.

**`SAVE_PRESET <engine#0..11> <n> <name<=9> [pad 0..15]`** (EDIT; additive): writes a USER preset into the engine's PRESETS file with the verified writer the OLED browser uses (`storeDrumUserSave` / `storeSynUserSave`: temp + verify, `.BAK` kept). Engines 0..3 = drums in group order (KICK SNARE HAT PERC), n 0..31, stored as DrumUser = the base sound of the optional `pad` (drums only; it must belong to that engine's group, else `ERR SAVE_PRESET arg`; default the group's first pad) + that engine's 4 depth knobs as they are now; names are kept literal on the wire (`_` stays `_`, any other char <= space becomes `_`) and round-trip through the PRESETS file; engines 4..11 = SYN0..SYN7, n 5..31 (0..4 are factory), stored as the algorithm's current SynPreset. Reply `OK SAVE_PRESET engine= n= name=` after the write verified; then the unsolicited line `PRESETS_CHANGED <engine>` is pushed so hosts re-pull `PULL_FILE R <engine#>` (the file's own `end <crc>` is the version). Errors: `ERR SAVE_PRESET arg` (range / name too long / synth n<5), `ERR SAVE_PRESET nosave` (NOSAVE session), `ERR SAVE_PRESET io` (no card / skip-storage boot / write failed), `ERR SAVE_PRESET mode` (not EDIT).

**Recycle bin** (EDIT only; additive; the card is precious: delete never erases). `DELETE_POOL <id> [force]` moves `SAMPLES/S<id>.WAV` to `PMOR3/BIN/` (a rename, byte-identical, the file is never rewritten), appends it to `BIN/INDEX.TXT` (`B id len rate ch crc32 deleted_at`, `END n crc`, atomic temp+verify) and drops it from `MANIFEST.TXT`. It first scans every patch's `PATCHES/Pnn.TXT` for references (16 patch headers per main-loop pass); a referenced sample is refused with `ERR DELETE_POOL referenced refs=<n>` unless `force`. Other errors: `ERR DELETE_POOL notfound | binfull | nosave | io | arg`. Success: `OK DELETE_POOL id= bin=1 refs=`. `BIN_LIST` -> `B <id> <len> <name> <deleted_at>` lines (name `S<id>`, deleted_at = seconds of uptime: the card has no RTC) then `OK BIN_LIST n=<n> token=<8 hex>`. `BIN_RESTORE <id>` renames it back and re-adds the manifest entry (`ERR BIN_RESTORE notfound | exists | io`). `BIN_EMPTY <token>` is the ONLY erase path: the token comes from the last BIN_LIST and changes whenever the bin changes, a wrong/stale token erases nothing (`ERR BIN_EMPTY token`); `OK BIN_EMPTY n=<files erased>`. Modes: STUDIO gets `ERR <cmd> mode`; a NOSAVE session gets `ERR DELETE_POOL nosave`. Sample names in the pool stay `S<id>.WAV`; the manifest crc32 field is the real CRC-32 (IEEE, 0xEDB88320) of the PCM data after the 44-byte header (a host check: tests/test_storage_layout.cpp verifies every sample against an independent implementation).

**ARCHIVE_PATCHES** (EDIT only; one-time clear of every patch, every sample kept; rename only, nothing is erased). `ARCHIVE_PATCHES` with no argument scans the card (16 slots per main-loop pass) and replies `OK ARCHIVE_PATCHES token=<8 hex> patches=<n>`; the token hashes the patch files present (changes whenever they change). `ARCHIVE_PATCHES <token>` re-scans and, only if the token matches, STARTS a device-owned background job and replies at once `OK ARCHIVE_PATCHES started dir=ARCHIVE_<k> patches=<n>`. The job runs from the main loop (patchTick), ONE file per pass, and does not depend on the session: a liveness/stall reset, BYE or a new HELLO never stops it, and it finishes with nobody connected. While it runs, autosave, the patch export and every patch-file write (patchSave/patchDelete) are suppressed. Files moved: the legacy `Pnn.PMR/.PMR.BAK/.AUD/.AUD.BAK/.EXT/.EXT.BAK` and the new tree's `PATCHES/Pnn.TXT/.TXT.BAK` (kept under `ARCHIVE_<k>/PATCHES/`). `SAMPLES/`, `MANIFEST.TXT`, `BIN/`, `PRESETS/` and the globals are never touched and nothing is overwritten (a name already in the archive folder gets a `.1`..`.9` suffix). A per-file failure is skipped and recorded (first 6 names), never aborts. The folder is reused across retries: `ARCHIVE_<k>` = the highest existing folder that has no `DONE.TXT`; `DONE.TXT` is written only by a run with zero failures, after which the next start makes `ARCHIVE_<k+1>`. Asking again while it runs (`ARCHIVE_PATCHES` with or without a token) replies `OK ARCHIVE_PATCHES running state=running k= moved= failed=` and starts nothing. Afterwards the live patch becomes the boot default (nothing reloaded from the archive, no file written). Errors: `ERR ARCHIVE_PATCHES arg | token | nosave | io | busy | mode`.

**ARCHIVE_LIST** (STUDIO or EDIT, read-only). One line `A <slot> <names>` per slot that still has patch files (`PMR PMR.BAK AUD ... PATCHES/TXT`), then `OK ARCHIVE_LIST remaining=<files> slots=<n> state=running|idle k=<k> moved=<m> failed=<f> [fail=<name,name>]`. Progress while the archive runs, and the list of what is left (including files that failed). Finish a stuck archive: `pmorctl port DEV script "HELLO EDIT 1" "ARCHIVE_PATCHES"` (token) then the same with `"ARCHIVE_PATCHES <token>"`, then `"ARCHIVE_LIST"` until `state=idle remaining=0`. To undo, copy the files back from `ARCHIVE_<k>/` (card reader or PULL_FILE).

**Sampler slot extras** (schema `PD_SAMPLER` 512 + slot*8 + k, group `SMP.SLOT<n>`; session-live): k0 `START` and k1 `END` (0..1 of the recording, shown %; defaults 0 / 1; an empty or inverted window plays the whole recording), k2 `LOOP` (0/1 enum; loops the START..END region until the slot is hit again, hard stop after 8 s: there is no note-off on this one-shot engine), k3 `ATTACK` 1..2000 ms and k4 `RELEASE` 1..4000 ms (the voice's fade in / natural-end fade; defaults 2 ms = the old fixed fades). Schema flag bit 7 (value 128) is new: `PF_LOG`, the param wants a logarithmic dial (`min*(max/min)^t`, used by ATTACK/RELEASE). Defaults are bit-identical to the previous voice (`tests/test_sampler_ext.cpp`). The patch blob layout is frozen, so these are saved in the additive **patch extension record** (`PMOR3/Pnn.EXT`, see below); reverse already existed (`REVERSE`).

**Synth LFO** (schema `PD_SYNTH` 258..261, group `SYN.LFO`, engine-wide): 258 `LFORATE` 0.05..20 Hz (`PF_LOG`, default 3), 259 `LFODEPTH` 0..1 (shown %, default 0 = OFF: nothing is computed, output bit-identical to the engine without it), 260 `LFOSHAPE` enum SINE/TRI/SQUARE/S&H, 261 `LFODEST` enum CUTOFF (+-2 octaves at depth 1) / PITCH (+-2 semitones) / AMP (tremolo down to 50 %). Block-rate (1.33 ms) modulator; applies to every voice of every algorithm.

**Drum deep recipe knobs** (schema `PD_DRUMS` 320 + group*16 + k, k 0..7, groups `DRUM.DEEP0..3` = KICK SNARE HAT PERC): multipliers 0.25..4 (`PF_LOG`, default 1 = neutral, bit-identical) applied at trigger time to a copy of the *selected sound's* recipe (`SoundDef`) for every pad of that engine group: k0 `PITCH` (f0), k1 `DECAY` (all t60s, tail, ping), k2 `LAYER2` (layer 2/3 gains), k3 `NOISEF` (noise filter freq / metal bands), k4 `NOISEQ` (noise / band Q), k5 `NOISEDEC` (noise decay + hold), k6 `SWEEP` (sweep time, click time, hold, 2nd-hit delay), k7 `FM` (FM indices + self-FM). A knob a given recipe does not use is inert for that sound (coverage per group in `tests/test_deep.cpp`; e.g. PITCH moves 9/24 kicks, 1/24 hats). Editing mid-note affects the next hit.

**Patch extension record** `PMOR3/Pnn.EXT` (beside `Pnn.PMR`; old firmware ignores it): header `{'PEXT', version 1, body_len, crc32(body)}` then 16 x sampler extras (start end loop attack release), 3 x first-hit lane start offset (u16, +pad), 4 x synth LFO, 4x8 drum deep knobs. Absent / unreadable / CRC-bad = defaults (the `.BAK` is tried first); a shorter (older) body is accepted with defaults for what it lacks, a longer one is read up to what this firmware knows. Written with the patch only when something is non-default or the file exists.

**First-hit recording** (no schema change): REC armed on an EMPTY lane waits for the first pad hit; on an own clock the transport restarts at it, on a followed clock (Live/STUDIO) the hit snaps to the nearest 16th and the lane keeps a start offset. Schema now 648 entries, hash `D5796435` (the 612 older entries are byte-identical: `tests/test_schema.cpp` re-derives the old hash `776F12EA` from them).

**Sampler stutter (ST4/8/16/32, BZZ) only chops.** The slot's playing voice keeps advancing silenced (a shadow playhead) while the chops are separate voices; on release the chops fade out (5 ms) and the shadow fades back in at the position the sample would have had without the stutter (`tests/test_sampler_stutter.cpp`: post-release samples identical to a never-stuttered render). Previously a retrigger choked the real voice, so the sample never resumed. This intentionally changed the sampler-stutter renders `fx_000..003` and `fx_010` in `tests/golden` (every other golden is untouched).

**Session re-entry (replug).** **NEVER clear the CDC TX FIFO** (0c48eaa did, on every session reset; hardware: after a fresh open the IN endpoint stayed busy forever, HELLO and console replies never reached the host while RX kept working). A reset drops only PMOR's own queue; stale FIFO bytes may still arrive first, so the first frame after a reset is preceded by `\n` (hosts skip empty lines). `tools/fwcheck.sh` rule 6 forbids it, and the sim models the real TinyUSB/DWC2 TX path (FIFO, IN transfer in flight, banner on DTR rise, cold boot) with a negative control (`pmorctl simbad`). Any fresh HELLO is answered within one poll in every state: it aborts a running multi-line job, drops the stale TX backlog and the push shadow (counted in `resets`), and a HELLO is answered even with DTR low (a host that never asserts it, or asserts it late; a DTR rise right after a handled line does not kill that fresh session). A DTR fall or rise, a USB mount/unmount/suspend/resume edge, the 5 s liveness timeout and the TX watchdog each start a clean session: queue, job, partial RX line, mode and the CDC TX FIFO are dropped. A partial line left from before an unplug and glued in front of a request does not eat it: a frame starts at any `!` whose `!BODY*CRC` checks out. STATUS `resets=<dtr_fall>/<dtr_rise>/<usb>/<liveness>/<bye>/<tx_stall>/<hello>` counts them by cause. **Boot:** HELLO is answered once the main loop runs (a replug power-cycles the board; there is deliberately NO boot-time CDC pump: 852e4af answered HELLO from inside the SD/OLED service loops with `booting=1`, never finished booting on hardware and was rolled back). The ~12 s the replug used to take was the synchronous sampler-audio restore in `patchInit` (up to 16 slots x 96000 samples = 3 MB through FatFS with a USB service yield per sector); it is now a background job stepped from `patchTick` (<= 4 KB per main-loop pass, `.BAK` / pool-id fallbacks kept, no save/assign until it finishes), so the main loop starts and HELLO is answered right after the SD mount and patch load. The boot watchdog budget: the IWDG is only fed by `bootguardKick()`, which stops feeding 12 s after the first kick until `main()` declares the boot finished, so a wedged boot resets and the skip-storage flag arms (`tools/fwcheck.sh` rule 5).

**MIDI out** (STUDIO/EDIT): drum (ch1) and sampler (ch3) hits send a bare note-on; a retrigger note-off precedes it only when that same pitch is still held (its timed off, 30 ms drum / 100 ms sampler, not yet sent). **Seed CC110-113 (ch4)** are sent once per accepted transport Start/Continue (0xFA/0xFB) while STUDIO/EDIT follows the bus (counted at IRQ time, so a Stop->Start faster than one main-loop pass still reseeds); not sent as master/standalone or while the session is STANDALONE. Full STATUS field list: `mode role tx_stalls= cm=<setting>/<policy> sd= calls= ffo= why= job= boot=<state>/<count> stack_free= store= pool= ferr_us= rx= late_us= iv_us= clk= running= bpm= svc_gap_ms= gap_in= cpu= cpu_max= audio_overruns= midi_drops= ev_drops= chunks= lp_us=`.
