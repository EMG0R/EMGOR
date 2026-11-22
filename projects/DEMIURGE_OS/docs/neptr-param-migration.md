# NEPTR control inputs -> parameter pool: migration map (bathtub §3)

Status: DESIGN / INVENTORY. Nothing here is deployed, wired or run. Read from the repo (`~/_______DEMIURGE/neptr/`, `src/demiurge-io/`) on 2026-10-05; the live engine was not touched.

Plan being served: "NEPTR rewrite: the exact same control inputs but through the parameter pool. Nothing lost." Target scheme: `parameters.md` / `param-pool/pool.py` (every pool value is a float 0..1; sources `/hw/<device>/<name>`; params `/<stage>/<group>/<name>`; router modes `continuous | toggle | gate | trigger | step`).

> **Status 2026-10-07: implemented for the non-encoder controls** (see `docs/NEPTR_POOL_BENCH.md`, `docs/parameters.md` section 12). Done: pedal (`/csound/neptr/expr`, ch1 CC29 as `/cc`, source `/hw/midi/1/29`), arcade tap (`/global/tap` -> demiurge-clock), `/hw/teensy/<id>` for sensors and buttons (unmapped in NEPTR = today), the NEPTR default patch map, and the parity tests. Not done on purpose: encoders and encoder buttons keep their fixed menu roles on MIDI (rows 1-14 of 2.1, all of 2.2's rot/btn/nav intent params, the UI preset/utility CCs); the intent-param names in 2.2 are NOT introduced. Corrections to this inventory found while building it: the arcade tap is **ch16 CC113** (serial_bridge.py) / OSC `/transport/tap` (not "unused in audio"), and in the live engine ch1 CC29 from MIDI is demoted to CC30 while ch1 CC29 over OSC is exempt, which is what makes `/cc 1 29` from the pool the correct pedal write. The pool port is 9102, not 9100.

## 0. Findings that shape the whole migration

1. **There are two layers of control, not one.** (a) Absolute parameters (a drive amount, a mix, a pedal target) and (b) NEPTR's *stateful intent layer*: menu position, "last toggled effect" focus (`gkLastToggle`), the sticky pedal claim (`gkLastToggle29`), looper press/hold gestures, preset hold gestures. The encoders do NOT address parameters. Every rotation (CC20..26) is dispatched on `gkLastToggle` (full takeover, `handler_menus.orc` header), and the pedal on `gkLastToggle29`. Mapping an encoder straight to a fixed `/csound/<param>` would lose this. So the pool needs both: direct params for sensors/web/learn, and a small set of `/csound/neptr/*` intent params that carry the same events the CCs carry today.
2. **The physical->logical encoder remap (`handler/handler_remap.orc`) is a mapping table**, not engine logic. In the pool it becomes the patch's `maps[]` (E7 button -> nav prev, E5 button -> nav next, etc.). The engine's dispatch (nav/toggle/rot/expr files) stays unchanged and sits behind the intent params.
3. **Encoders are relative detent events** (CW click = value 127, CCW click = value 0, one CC per detent, `$ENC_ROT` steps a clamped accumulator by a per-param step). The pool's existing modes do not express "relative delta"; see section 4 (needs a decision).
4. **Most Teensy sensor CCs are read by nobody in Csound today.** Verified by grep: nothing in `neptr/csound/` reads ch1 CC27, 28, 31-36, 60-68, 77, 78, 79 (joystick X/Y, accel, FSR, LiDAR 2, mics, the 9 push-buttons incl. the arcade button, joystick click). They are consumed only by (i) `demiurge-monitor` / `companion_input.py` (dashboard telemetry, and the companion's navigation from joystick/arcade) and (ii) `demiurge-io` via the serial frame (`sensors.json`), whose `maps[]` is currently **empty** (`~/demiurge/sensors.json`). CC30 (LiDAR 1) is not read either; CC30 is also where the onboard pedal is parked. So sensors are a clean migration (nothing to preserve in the engine); they are listed so the map is complete.
5. **Controls bypass the router today.** `teensy.py` sends encoders/buttons straight to `send_cc` (and only if `--forward-control`), skipping conditioning and `router.py`. The pool must route them through `maps[]` for them to be mappable at all (section 8, step 9).
6. **`menus.json` slot order is not uniform** (list index 0 = slot 4 / CC72 for most menus, but PREAMP is listed s0->s4 and LOOPER/`lp0..lp4`/`nam0..nam4` follow their own order). Tabulated per menu in section 3; the ambiguous ones are flagged in section 7.
7. **Docs disagree with code on menu numbers.** `docs/neptr_full_reference.md` is a 17-menu snapshot (GLITCH=9, LOOPER=10, ...). Authoritative is `menus.json` + the `midi_handler.orc` header + `nav.orc` (21 menus, ids 0-20, `LOOPER=14` hardcoded in `handler_looper.orc`). This doc uses the `menus.json` ids.

## 1. The rule: engines read params, never hardware

- Csound (and every other stage) reads only pool parameters. Encoders, buttons, joystick, LiDAR, pedals, DUODECIMUS, hyperGuitar-state echoes, web faders and the touch UI are **sources** (`/hw/...`, `/ui/...`) that reach the engine only through a `maps[]` entry or a direct `POST /api/pool/set`.
- **The Teensy -> Csound USB-MIDI direct path is the LAST migration step** (parameters.md step 9; `demiurge-io.service` comment). Today `-Ma` plus `demiurge-midi-connect.sh` fans every USB-MIDI port into `Midi Through 14:0`, which Csound opens. Until step 9, MIDI stays the live path and `forward_control` stays OFF (turning it on early double-feeds every click: encoders move at double speed). Step 9 is rig-affecting and is done only by EMGOR, at the bench, with a rollback (re-run the connect script / flip `forward_control` back).
- **Behavior-preserving first move** (recommended): when the `midi` source + a `/cc` compat sink exist, the pool can re-emit the identical `/cc <ch> <cc> <val>` into 7770 (the engine already treats a `/cc` as indistinguishable from MIDI, `midi_handler.orc` "CONTROL INPUT TRANSPORT"). That makes step 9 a pure re-plumbing with zero DSP change. The intent-param names in section 2 then replace `/cc` one family at a time; each family is removed from the compat path only after its params exist in the registry and a bench test passes.
- Engine-side state that must stay in the engine (not pool state): `gkLastToggle`, `gkLastToggle29` + soft-takeover arming (`expr_cc29.orc`), current menu (`gkCurrentMenu`), looper state machines and hold timers, preset-bank ops (`gkPresetOp/Slot`), the `kFromOsc` pedal exemption. The pool carries events and values into them; it does not own them.

## 2. Control-input inventory and mapping

Notation: pool values are always 0..1. "native" = what the engine adapter denormalizes to at the last inch. "reg" = already generated into the registry from `handler/expr_targets.json` (path `/csound/<writes[0]>`).

### 2.1 Teensy 4.1 NEPTR body (ch1, 7-bit on the MIDI wire; the serial frame carries 12-bit)
Source ids on the pool: serial frame fields (`teensy.py`) become `/hw/teensy/<id>`; MIDI path would be `/hw/midi/1/<cc>`. Analog sources already exist in `sensors.json` with calibration (`jx jy exp fsr1 fsr2 tof1 tof2 ax ay az ml mr`), so they keep their ids.

| # | Physical | MIDI | Frame field | Meaning today | Source path | Proposed pool param / mapping | Mode |
|---|---|---|---|---|---|---|---|
| 1-7 | Enc E1..E7 rotation | ch1 CC20..26 (CW=127, CCW=0 per detent) | e1..e7 (delta) | physical->logical remap, then P1..P7 of focused effect (see 2.2) | `/hw/teensy/e1`..`e7` | `/csound/neptr/rot/<logical>` (see 2.2 remap table) | delta (needs decision, §4) |
| 8-14 | Enc E1..E7 push | ch1 CC70..76 (127/0) | eb1..eb7 | slot toggles / menu nav / looper press-release (see 2.2) | `/hw/teensy/eb1`..`eb7` | `/csound/neptr/btn/<logical>` | gate (press 1, release 0) |
| 15 | Arcade button (b1) | ch1 CC60 | b1 | Csound: unused. Companion reads it as navigation. Firmware: white burst on the LED ring; hold 1 s (joystick not pressed) toggles ring blackout (firmware-local) | `/hw/teensy/b1` | none now; reserve `/csound/neptr/arcade` (gate) if it should do something in audio; ring burst stays firmware-local | gate |
| 16-23 | Buttons b2..b9 | ch1 CC61..68 | b2..b9 | unused everywhere in the engine | `/hw/teensy/b2`..`b9` | unmapped sources, available to learn | gate/trigger |
| 24 | Joystick click | ch1 CC77 | jb | companion navigation only | `/hw/teensy/jb` | unmapped (companion keeps reading telemetry) | gate |
| 25-26 | Joystick X, Y | ch1 CC27, CC28 (center 64) | jx, jy | no engine consumer; companion navigation | `/hw/teensy/jx`, `jy` | unmapped; ready for `/p`-style routes | continuous |
| 27 | Onboard expression pedal | ch1 CC29 -> **rewritten to CC30** in `midi_handler.orc` (demoted so it cannot fight the DUODECIMUS treadle) | exp | parked, does nothing ("the user's call") | `/hw/teensy/exp` | **unmapped on purpose** (re-pointable later). `teensy.py` already reserves CC29 and sends `ONBOARD_EXPR_CC=30` | continuous |
| 28-29 | FSR1, FSR2 | CC34, CC35 | fsr1, fsr2 | no engine consumer | `/hw/teensy/fsr1`, `fsr2` | unmapped | continuous |
| 30-31 | LiDAR 1, 2 | CC30, CC36 (2-46 cm) | tof1, tof2 (mm, -1 = no reading) | no engine consumer; **CC30 collides with the parked-pedal CC30** (harmless: nothing reads it) | `/hw/teensy/tof1`, `tof2` | unmapped; dropout "hold" already in `sensors.json` | continuous |
| 32-34 | Accel X, Y, Z | CC31, 32, 33 | ax, ay, az | no engine consumer | `/hw/teensy/ax`..`az` | unmapped | continuous |
| 35-36 | Piezo mic env L, R | CC78, CC79 | ml, mr | no engine consumer | `/hw/teensy/ml`, `mr` | unmapped | continuous |
| out | NeoPixel ring (113 LEDs) | n/a | firmware-local | brightness follows the pedal ADC, bursts on arcade, blackout hold. The firmware ignores incoming MIDI (`while (usbMIDI.read()) {}`) | n/a | **output, not a control input.** Stays firmware-local. Not a pool concern unless EMGOR wants ring state driven by params (would need new firmware) | n/a |

### 2.2 NEPTR intent layer (what the CCs mean after `handler_remap.orc`)
The remap is the table below; in the pool it is the patch map (source -> target). Engine dispatch files are unchanged.

| Physical source | -> logical CC today | Proposed intent param | Mode | Meaning |
|---|---|---|---|---|
| E7 push (CC76) | 70 | `/csound/neptr/nav/prev` | trigger (on press) | previous menu (cycle from `menus.json`) |
| E5 push (CC74) | 73 | `/csound/neptr/nav/next` | trigger | next menu |
| E1 push (CC70) | 74 | `/csound/neptr/slot/0/press` | gate | slot 0 button (toggle effect; looper: press/release) |
| E4 push (CC73) | 75 | `/csound/neptr/slot/1/press` | gate | slot 1 |
| E3 push (CC72) | 76 | `/csound/neptr/slot/2/press` | gate | slot 2 |
| E2 push (CC71) | 71 | `/csound/neptr/slot/3/press` | gate | slot 3 |
| E6 push (CC75) | 72 | `/csound/neptr/slot/4/press` | gate | slot 4 |
| E1 rot (CC20) | 24 | `/csound/neptr/rot/slot0` | delta | P1 if the focused effect's home is slot 0, else P-ordered (see encoder_mapping_v2.md priority rule) |
| E4 rot (CC23) | 25 | `/csound/neptr/rot/slot1` | delta | same, slot 1 |
| E3 rot (CC22) | 26 | `/csound/neptr/rot/slot2` | delta | slot 2 |
| E2 rot (CC21) | 21 | `/csound/neptr/rot/slot3` | delta | slot 3 |
| E6 rot (CC25) | 22 | `/csound/neptr/rot/slot4` | delta | slot 4 |
| E7 rot (CC26) | 20 | `/csound/neptr/rot/p2` | delta | secondary param of the focused effect (menu 0: preset output trim +/-6 dB in 0.5 dB steps) |
| E5 rot (CC24) | 23 | `/csound/neptr/rot/p3` | delta | tertiary param of the focused effect (NAM: out gain; IR: master damp) |
| Pedal (DUODECIMUS ch7 CC14/15, touch EXPR strip, sensors via `/cc`) | ch1 CC29 | `/csound/neptr/expr` | continuous | the one expression pedal; engine routes it by `gkLastToggle29` with soft takeover (disarm on owner change, re-arm after a >0.03 move) |

Overrides that stay engine-side (checked before focus dispatch): NAM (menu 17) and IR VERB (menu 18) take all five slot rotations for variant browse (`gkNAMRowSel`, `gkIRSel`); LOOPER menu (14) takes slot rotations for per-channel speed; menu 0 slot rotations are inert and CC20 is preset trim; menu 20 lane 0 rotation is deliberately inert. These are properties of the intent params, not of the pool.

Direct-parameter equivalents (so sensors, web UI and learn can reach one value without the focus indirection) are the next two tables.

### 2.3 Expression-pedal targets (registry already exists: 36 ids)
Each is already a `/p <id> <0..1>` target via `ExprSet` (`handler/expr_targets.json`, generated). Proposed pool path = `/csound/<writes[0]>` (reg). Native decode is done by `ExprSet` today and stays there.

| id | path | native decode / note |
|---|---|---|
| 30 | /csound/gkMasterVol | 0..1 |
| 1 | /csound/gkVOLpedal | 0..1 |
| 20 / 23 / 24 / 25 / 48 | /csound/gkBBcut_expr / gkRPT_expr / gkCHP_expr / gkRVS_expr / gkIMP_expr | 0..1 instant-trigger levels (GLITCH v2) |
| 21 | /csound/gkTapeExpr | tape transport: heel=play, toe=stop |
| 3 / 4 / 5 / 28 / 27 | /csound/gkSemi / gkGPSsemis / gkFormantShift / gkFormsShift / gkSemiPF | kNorm*24-12 semitones |
| 26 | /csound/gkSemiH90 | bass bank: kNorm*12; else kNorm*24-12 |
| 38 / 44 | /csound/gkBinPedal / gkBcPedal | 0..1 (38 polarity flipped on purpose) |
| 10 | /csound/gkLeslieSpeedAcc (+ gkLeslieSpeed) | speed = 0.1 + acc*9.9 |
| 11 | /csound/gkARModFreq | 0.05*200^n*25 (exp) |
| 31-35 | /csound/gkOD_drive / gkMAR_drive / gkDHG_drive / gkRMZ_drive / gkMUF_drive | drive amount, 0..1 |
| 17 | /csound/gkMOOD_Clock | **inverted** (1 - n) |
| 22 | /csound/gkWAH_freq | 0..1 |
| 15 | /csound/gkDelayMS | kNorm*700000 |
| 36 | /csound/gkHabitScan | 0..1 |
| 45 | /csound/gkONW_follow | 0..1 |
| 66 | /csound/gkMD2_Clock | 0.5 = home |
| 61 | /csound/gkTSY_cut | 0..1 |
| 63 | /csound/gkFBK_expr | 0..1 |
| 56 | /csound/gkBLP_expr | 0..1, centred at rest |
| 69 / 70 / 72 | /csound/gkMvdExpr / gkNebWarp / gkFrzExpr | 0..1; FREEZE has hysteresis inside the engine |
| 74 | /csound/gkHgPedal | hyperGuitar pitch; relayed to the Bela as `/hg/pedal` |
| 75..98 (SOUNDMAGIC rack) | `SM_PedalSet id kNorm` (not in `expr_targets.json`) | per-effect pedal param via `giSMpedal`; **not in the generated registry** (see §7) |

### 2.4 Menu / slot / effect inventory (what each slot button toggles, what each rotation changes)
Menu cycle order is data (`menus.json` array order): 0,1,2,3,4,5,6,7,8,9,10,11,12,13,19,15,14,16,17,18,20 (UTILITY 19 sits before CHASE BLISS; HYPERGUITAR 20 is walked only while present). Slot columns give the cell as listed in `menus.json` (list index 0 first) and the physical slot where the header comment pins it. Toggle ids from the registry in `neptr_full_reference.md` §11 and the `nav`/`toggle_slot*` dispatch.

| Menu id | Name | Cells as listed in menus.json (code = effect, toggle id) | Pool intent (from slot press) |
|---|---|---|---|
| 0 | PRESETS | sld (preset 1 overdrive), hvy (preset 2 solid-dist), pha (stackable, id 8), trm (stackable, id 43), amb (preset 3 ambient) | `slot/N/press` -> sets `gkCurrentPreset` + a full parameter block (VIB, delay, OD/DHG, reverb, vol pedal, master vol, `gkLastToggle29`). Bank mode (surf / bass / gorcore) is engine-owned (`PRESET_BANK` launch flag, OSC `/preset_bank`) |
| 1 | VOLUME | tape (21), atk (2), les (10), rm (11), vol (1) | toggle id |
| 2 | SPECTRAL | bc (44), bin (38), bsc (67), fmt (5), fms (28) | toggle id |
| 3 | PITCH | -, pf (27), gps (4), ps (3), h90 (26) | toggle id |
| 4 | FILTERING | -, mog (14), wah (22), res (47), lsy (46) | toggle id |
| 5 | MOD | fla (9), trm (43), pha (8), cho (7), vib (6) | toggle id |
| 6 | DRIVE | muf (35), dhg (33), rmz (34), mar (32), od (31) | toggle id; CC29 claims drive amount |
| 7 | PREAMP | pre (39), bpl (40), bph (41), bds (42), cmp (57) (listed s0->s4, reversed vs others) | toggle id; bpl/bph radio pair |
| 8 | GRANULAR | mod (17 MOOD), grn (18), -, -, - | toggle id |
| 9 | DIGIVERB | neb (70), shm (71), frz (72, gesture), gvb (73), vrb (19) | toggle id |
| 10 | DELAY | tdl (68), sdl (16), mvd (69), -, dly (15) | toggle id |
| 11 | BASS | env (55), oct (54), bds (42), bph (41), bpl (40) | toggle id (shared with PREAMP) |
| 12 | SYNTH | -, -, teq (58), pll (62), tsy (61) | toggle id |
| 13 | AIR | cfb (60), -, -, dif (64), fbk (63) | toggle id |
| 19 | UTILITY | no slots (touch-only globals: master out, manual BPM, rec, EXPR strip) | see 2.5 |
| 15 | CHASE BLISS | -, md2 (66), hab (36), onw (45), blp (56) | toggle id; HABIT button = click on+rec / click freeze / double-click off |
| 14 | LOOPER | lp0..lp4 = 5 channels (instr 210-214; **not toggle ids**) | press/release gestures (see §4) |
| 16 | GLITCH | rvs (25), chp (24), rpt (23), bbc (20), imp (48) | press-hold semantics (GLITCH_PRESS/RELEASE) |
| 17 | NAM | nam0..nam4 = 5 amp rows, one toggle id 37 | row select + variant browse |
| 18 | IR VERB | ir4..ir0 = 5 rows, ids 53..49 | row on/off + variant browse |
| 20 | HYPERGUITAR (conditional) | 5 lanes, no `slots` in menus.json | `/hg/lane` to the Bela (see 2.6) |

Per-effect parameters (the P1..P7 the rotations drive). Pool path for each = `/csound/<gkVar>` where `<gkVar>` is the global the `$ENC_ROT(...)`/`ExprSet` line writes; for the 36 above it is known, for the rest it must be **generated** from `handler/dispatch/rot_*.orc` + `gen/expr_rot_*.orc` (see §6, "registry gaps"). Names/ranges below are the UI's `EFFECT_PARAMS` (the contract the on-screen sliders already use; the OSC echo is `/<code>_<param>`); native range is denormalized by the engine adapter.

| Code | Params (key: native lo..hi) |
|---|---|
| vol | level 0..1 |
| atk | mix 0..1, time 10..2000 ms, sens 0..1, shape, range 0..1, retrig 20..150 ms |
| tape | amt, fail, model 0..1 |
| les | speed 0.1..10 Hz, depth 0..1 |
| rm | depth 0..1, freq 0.05..250 Hz |
| bc / bin | pedal 0..1 |
| bsc | mix, scatter 0..1, fback 0.5..1.1, window 0.02..1 s |
| fmt / fms | shift -12..12, mix 0..1 |
| ps | semi -12..12, mix 0..100 |
| h90 / pf | semi -12..12, mix 0..1 |
| gps | mix, semi -12..12, fback 0..1, dly 0..0.1 |
| mog | sens, res 0..0.95, type 1..4 (stepped), level 0..2, dist |
| wah | freq 0..1, res 0.5..0.97 |
| res | mix, onset, fb, oct -1..1, frz |
| lsy | mix, loss, bw, pkt |
| fla | rate 0..12, depth, fback -1..1 |
| trm | rate 0.5..20, depth, mode 0..3 (stepped: SINE OPTO BIAS HARM) |
| pha | rate 0..12, depth, fback, stages 1..24 (stepped) |
| cho | rate 0..8, depth, mix |
| vib | rate 0..12, depth, fback 0..0.7 |
| od / mar / dhg / rmz / muf | drive, tone, + wc1/wc2 (od: morph, blend; mar: pres, tight; dhg: blend=Fat, tight=Edge; rmz: midf, midg; muf: mids, voice), all 0..1 |
| pre | voice 0..2 (stepped) |
| bpl / bph | drive 0..1 |
| bds | drive, blend, tone |
| cmp | amt, speed, makeup, blend, gate |
| mod (MOOD) | mix, clock, dens 0..1400, dly |
| grn | mix, dens 10..400, size 0.05..1.2, pitch 0..24 |
| neb | mix, dens, size |
| shm | mix, intv 0..3 (stepped +12/+7/+5/-12), bloom |
| frz | level, damp, listen (gesture: toe >0.6 opens the send) |
| gvb | mix, spacing, decay |
| vrb | mix, room 0..0.999, damp |
| tdl | mix, time 60..1200 ms, fback 0..1.15, center 250..5000, width 0.5..6, head 0..6 (stepped) |
| sdl | mix, maxdel, fback |
| mvd | POLY DLY v5: numl CC20, denl CC23, numr CC24, denr CC26 (int 1..16), att CC21, dec CC22 (0..1); plus ch1 CC110 Repeats (1..8, absolute), CC111 Interval (-12..+12, absolute); expr CC25 (see docs/specs/2026-10-22-polydelay-v5.md) |
| dly | time 0..700000, mix, fback 0..1.2, semi -5..12, mode 0..1 (NORMAL/REVERSE) |
| env / oct | env: wet, sens, mode, att, dec; oct: sub, tone, char |
| teq | mid, freq, tilt, low, high (0.5 = flat) |
| pll | mix, intv 0..4 (stepped), lock, vmix |
| tsy | mix, cut, res, glide, sub, det |
| cfb | ret, gain, tone, ceil 0.1..0.95 |
| dif | wet, depth, rate, duck |
| fbk | level, harm 0..0.75 (stored 0/.25/.5/.75, stepped), bloom, vib |
| md2 (MOOD II) | mix, clock, length, gen |
| hab | mix, size, repeats, blend, scan, rec (stepped FROZEN/REC) |
| onw | mix, blend, size, follow, density, err, oct -1..1 |
| blp | motion, scat, size, char 0..2 (stepped NEAR/THROW/FAR) |
| rvs / chp / rpt / bbc / imp | prob + mix/depth/shape/len/flt/dec/tone per effect (rpt len 0..2 stepped) |
| nam0..4 | out_gain -18..18 dB (CC20), in_gain -12..18 dB (CC23); variant browse = `gkNAMRowSel[r]`; shared wet |
| ir0..4 | mix 0..1 per row (CC20), master damp (CC23); variant browse = `gkIRSel[r]`; `gkIRLevel` pinned 1.0 (no encoder) |

### 2.5 UI, DUODECIMUS, clock and OSC-verb inputs

| Source | Wire | Meaning | Proposed pool param | Mode / note |
|---|---|---|---|---|
| Touch UI EXPR strip | ch1 CC29 absolute (`EXPR_CC=29`) | virtual expression pedal | `/csound/neptr/expr` | continuous. After migration the "demote ch1 CC29 -> CC30" hack and the `kFromOsc` exemption are unnecessary: the map decides which source feeds `expr` |
| Touch UI mvd repeats | ch1 CC110 (round((n-1)/7*127)) | POLY DLY repeats 1..8 | `/csound/gkMvdReps` | step |
| Touch UI mvd interval | ch1 CC111 (round((semi+12)/24*127)) | POLY DLY interval -12..+12 | `/csound/gkMvdIntv` | step |
| UTILITY master out | ch16 CC103 (0..127 -> 0..1.5, unity ~85) | master-out volume | `/csound/gkMasterOut` (native 0..1.5) | continuous |
| UTILITY manual BPM | ch16 CC104 (40 + v*260/127) | manual master BPM | `/global/bpm_manual` | continuous. **Span 260 here vs 360 on the clock CC119**: keep the adapters distinct |
| UI preset capture/recall/delete (onboard rows) | ch16 CC105 / CC106 / CC107, slot (1..17) carried in the VALUE | in-engine preset bank ops, slots 13..17 | `/csound/neptr/preset/<slot>/capture|recall|delete` (3 x 17 triggers) | trigger. Slot-in-value is an integer, not a 0..1 magnitude (§4) |
| UI late-join sync | ch16 CC102 = 127 | engine rebroadcasts its whole OSC state to the UI (`gkOscRefresh`) | `/global/ui_sync` | trigger. Coordination, not a value; ok to leave on MIDI/`/cc` (§4) |
| UTILITY Rec | local `demiurge-record toggle` subprocess | UI-side only, not an engine input | none | not a pool param; stays a UI action |
| WAVE editor | OSC to 127.0.0.1:8001: `/neptr/wavetable` blob (2048 f32), `/neptr/wavetable/chunk` x128 (17 floats), `/end`, `/harmonics` (129 f) | finger-drawn cycle; **no consumer in the stock chain** (patches opt in; doc `demiurge/docs/wavetable-osc.md`) | none | data stream, not a 0..1 scalar. Stays as a direct OSC feed; if a patch consumes it, it is a *table* param, outside the pool's float contract (§4) |
| DUODECIMUS treadle | ch7 CC14 (Jack A) and CC15 (Jack B), rewritten to ch1 CC29 in the engine | THE expression pedal | `/hw/midi/7/14`, `/hw/midi/7/15` -> `/csound/neptr/expr` | continuous. **CC29 stays reserved for this** (pool MIDI allowlist; never emitted by `teensy.py`) |
| DUODECIMUS preset recall | ch7 CC102..113 = 127 (12 encoders), 0 ignored | recall in-engine preset slot (cc-101) | `/csound/neptr/preset/<1..12>/recall` | trigger |
| DUODECIMUS preset capture | ch7 CC116, value = slot N (firmware one-shot at 5 s hold) | capture slot N | `/csound/neptr/preset/<N>/capture` | trigger. **ch7 CC116 vs ch16 CC116 (clock role) are different messages**; the channel gate is load-bearing |
| DUODECIMUS preset delete | ch7 CC117, value = slot N (one-shot at 8 s hold) | delete slot N | `/csound/neptr/preset/<N>/delete` | trigger. (ch16 CC117 = transport; same caveat) |
| DUODECIMUS other ch7 | everything else | ignored by design (`kCh != 7` gate) | none | stays ignored |
| Clock (daemon -> engine) | ch16 CC119 (7-bit BPM, 40..400), CC120+121 (14-bit BPM, canonical), CC114 (tempo-source class, one-way latch `gkGLITCH_extBpmSeen`), CC116 role, CC117 transport, CC118 steer, CC115 loop-held out | tempo coordination | **not parameters** (parameters.md §1: clock, transport, patch loads stay) | stays on the MIDI bus; excluded from pool sink output |
| `/cc ch cc val` (OSC 7770) | same as MIDI, injected into the same dispatch | the existing transport for sensors/web | compat sink during migration | retired family-by-family (§1) |
| `/p id val` | absolute write via `ExprSet` | the 36 pedal targets | **is already the pool egress** | keep |
| `/st fx on`, `/sp fx p val`, `/sh fx k idx`, `/shp fx idx` | SOUNDMAGIC rack: toggle, absolute param (idx = fx*32+p, clamped to registry min/max), hero-slot assign, pedal-target assign | 19 wired spectral effects (fx 0..18, toggle ids 75..98; 94-98 phase-3 not wired) | `/csound/sm/<fx>/on`, `/csound/sm/<fx>/p<n>` | on = toggle; p = continuous (engine clamps to native). `/sh`, `/shp` are *configuration* (assignments), not live values: leave as OSC verbs, or expose as step params (§4) |
| `/mc pos id`, `/mc_n n` | menu-order data push (`menu_reload.orc`) | cycle order | none | configuration, not a param; stays |
| Web | `/api/neptr/menus`, `/api/neptr/sm/param`, `/nv/param`, direct `/cc` | UI backend routes | `POST /api/pool/set` (parameters.md §7) | replaced one route at a time |

### 2.6 hyperGuitar (Bela, `192.168.7.2:7772`)
Link is a control link only (audio rides the Bela's UAC2 bridge). The Bela owns the lane state; Csound mirrors the echo.

| Direction | Message | Meaning | Proposed pool param | Mode |
|---|---|---|---|---|
| Csound -> Bela | `/hg/lane iii (lane, on, index)` | toggle lane on/off / set preset index (5 lanes; lengths 6, 9, 7, 10, 10 in `giHgLaneLen`; lane 0 = PS GLIDE, 100% wet, driven by the pedal, rotation inert) | `/hg/lane/<0..4>/on`, `/hg/lane/<0..4>/index` | on = toggle; index = step (steps = len-1, wraps) |
| Csound -> Bela | `/hg/pedal f` (0..1) | pitch interval while the guitar owns the pedal (toggle id 74 `gkHgPedal`, default 0.5) | `/hg/pedal` | continuous |
| Csound -> Bela | `/hg/sync i` | resend everything | `/hg/sync` | trigger |
| Bela -> Csound | `/hg/state iii` (lane, on, index) | echo of the lane state (the only thing that moves the mirror) | pool "report" (`/p` to 9100) so UI shows truth | continuous/step (report) |
| Bela -> Csound | `/hg/hello i` (1 Hz) | presence heartbeat, 3 missed beats = gone; gates menu 20 and `gkHgPresent` | `/hg/present` (0/1, derived by the pool) | gate (reported) |
| Encoders on menu 20 | slot press -> lane toggle; slot rotation -> index +/-1 (wrap) | HYPERGUITAR page | via `slot/N/press` and `rot/slotN` intents | unchanged |

Today `hyperguitar_link.orc` listens on 7770 and sends straight to the Bela, so it **is** a private engine<->device path; in the pool it becomes a stage with a manifest and `osc` sink (parameters.md §6) and the Csound side only publishes/receives pool values.

## 3. Rule for what moves and what does not
- **Moves to the pool**: every continuous value (pedal, sensors, UTILITY master/BPM, each effect parameter that has a UI slider or encoder), every on/off (effect bypass toggles), the intent events (nav, slot press, rotation, preset ops), hyperGuitar lanes.
- **Stays outside the pool** (parameters.md §1): clock/transport CCs, patch loads, audio-graph wiring, `/mc` menu-order push, the WAVE table blob, firmware-local LED ring.
- **Stays engine-side state**: focus (`gkLastToggle`, `gkLastToggle29`), current menu, looper machines, takeover arming.

## 4. Controls that do NOT map cleanly to a 0..1 float, and how the pool's modes cover them

Existing modes in `router.py` (verified): `continuous`, `step` (quantize to N levels), `gate` (hysteresis 0.6 up / low down, holds), `toggle` (flip on each rising edge), `trigger` (one fire on the rising edge, re-arms on release).

| Control | Why it is awkward | Proposed treatment | Needs a decision? |
|---|---|---|---|
| Slot button = toggle effect | CC127 on press, 0 on release; engine reacts on press only (and on release for looper/glitch/habit gestures) | `gate` source -> `slot/N/press` param (1 on press, 0 on release). The engine's own toggle logic stays. A pool `toggle` mode would double-toggle (engine already toggles) | No |
| Menu next/prev | pure event | `trigger` -> `nav/next|prev` | No |
| **Encoder rotation (relative)** | one CC per detent, direction in the value (127 CW / 0 CCW); `$ENC_ROT` applies a per-param step to an accumulator; router has no delta mode, and `step` quantizes an absolute value | Cheapest honest fit with today's modes: **two trigger params per encoder** (`rot/slotN/inc`, `rot/slotN/dec`), engine keeps the per-param step. Better: a new source kind `enc` (parameters.md §3.2 already names `enc` -> "step/delta") that emits signed single-detent events, mapped to one `rot/slotN` param carrying 0.5 +/- epsilon is NOT acceptable (lossy). | **Yes**: confirm "inc/dec trigger pair" vs adding a `delta` mode |
| Looper (menu 14) | press/release gestures with timing: rec -> close/play on press, hold >= 0.4 s toggles play/overdub, double-press clears, `gkLpConsumed`/`DblFired` bookkeeping; looper also drives `gkLpClock`/loop-held CC115 | `gate` source -> `slot/N/press`; **gesture timing stays in Csound** (it reads `times` between press/release). Do not try to express the gestures as pool modes | No (but keep press AND release edges, never collapse to trigger) |
| GLITCH buttons | press forces effect ON at 100% while held; release classifies click (<0.3 s) toggle vs hold | `gate`, both edges preserved. "Always send 0 on release or `_held` stays 1" (reference doc) | No |
| HABIT button | click = on+rec, click again = freeze, double-click = off | `gate`, timing in engine | No |
| Preset ops (DUODECIMUS, UI) | slot number is carried in the VALUE (CC105/106/107, CC116/117) | one `trigger` param per (op, slot) as in 2.5 (36 + 12 + 12 params); 0..1 is not asked to carry a slot index | **Yes**: 60+ triggers is noisy; alternative is one `preset/slot` step param + `preset/op` triggers |
| Stepped / enum params (type, mode, intv, head, div, harm, stages, rec, char, len, nam/ir variant) | integer enums | `step` mode with `steps = n-1`; engine adapter rounds `n*(N-1)`. Registry `native` must carry the count so the UI can label (`STEP_LABELS` in the UI is the source of labels) | No (registry needs `steps`, not yet in the schema of parameters.md §4) |
| Bank selector (surf / bass / gorcore) | launch flag `PRESET_BANK`, OSC `/preset_bank` out | engine-owned, reported only; not a pool control | Confirm it is not meant to be live-switchable |
| Menu focus ("last toggled") | implicit, set as a side effect of a slot press | not a parameter; **do not expose**. If a direct per-param route is wanted, address the param itself | No |
| Pedal claim / soft takeover | stateful, resets when ownership changes | engine keeps it; the pool's `slew` (`slew_ms`) is separate and must not be stacked on takeover without a bench check | Bench check |
| Joystick, LiDAR, accel, mics, FSR | genuinely continuous | `continuous`, `sensors.json` calibration | No |
| Arcade / b2..b9 / joystick click | unused in audio | `gate`/`trigger` when mapped | No |
| Clock CCs | coordination | not params; excluded from sink output by the pool allowlist | No |
| WAVE table | 2048-sample blob | out-of-pool OSC stream (not a float) | Confirm if a table-valued param type is ever wanted |
| `/sh`, `/shp` hero-slot / pedal-target assignment | indices, not values | OSC verbs stay (configuration), or step params with `steps = 31` | Low |

## 5. "Nothing lost" checklist (verify at the bench before each step; tick = EMGOR confirmed)
Engine behaviors that exist today and must survive the rewrite:
- [ ] All 21 menus walk in `menus.json` order; wrap; `hidden` removed; menu 20 only while the heartbeat is alive; `OSC /menu` echo to the UI.
- [ ] Slot press toggles the right effect in every menu and sets `gkLastToggle` (incl. NAM rows all setting 37; IR rows 49-53; BASS/PREAMP sharing 40/41/42).
- [ ] The seven rotations drive the focused effect's P1..P7 with the same per-param steps (`$ENC_ROT`, exp variants); NAM/IR/LOOPER/menu-0/menu-20 overrides intact; CC20 = preset trim on menu 0; CC20/23 = NAM in/out gain and IR mix/damp.
- [ ] Expression pedal: all 36 targets + SOUNDMAGIC pedal params; last-toggled-wins claim; soft-takeover disarm; HG pedal relay; the CC29-collision fix (onboard pedal parked, DUODECIMUS wins).
- [ ] Presets: surf/bass/gorcore banks, the preset parameter blocks, stackable PHASER/TREM, the in-engine bank ops from the foot (12) and the touch page (13..17), 5 s capture / 8 s delete one-shots.
- [ ] Looper: 5 channels, rec/close/play/overdub/resume/stop/clear gestures, speed rotation, clocked grid-true capture, CC115 loop-held announce.
- [ ] GLITCH press/hold/release; HABIT click/freeze/double-click; FREEZE gesture + hysteresis.
- [ ] POLY DELAY preset (CC110) and decay mode (CC111) from touch.
- [ ] UTILITY: master out (CC103), manual BPM (CC104, span 260), UI sync (CC102 -> `gkOscRefresh` full rebroadcast, needed for UI late-join).
- [ ] Clock: CC119/120/121/114/116/117/118/115 untouched and excluded from pool output; ch16 vs ch7 channel gates intact.
- [ ] Teensy: 7 encoders (CW/CCW feel unchanged, "direction flipped in new firmware"), 7 encoder buttons, b1..b9, joystick (X/Y/click), pedal (parked), FSR x2, LiDAR x2, accel x3, mic x2 are all present as sources; arcade hold ring blackout and ring brightness still firmware-local.
- [ ] DUODECIMUS: ch7 CC14/15 -> expression; CC102..113 recall; CC116/117 capture/delete; everything else ignored.
- [ ] hyperGuitar: lane toggle + index browse (wrap), lane-0 inert rotation, pedal claim on engage (id 74), `/hg/sync` on presence edge and on `gkOscRefresh`, heartbeat timeout drops menu 20 (and returns to menu 0 if on it).
- [ ] Wavetable OSC (8001) unchanged and still reaches whatever patch opts in.
- [ ] Companion/dashboard telemetry: the `demiurge-monitor` label map (jstX..eb7) still published (monitor consolidation A-C, parameters.md §3.3).
- [ ] Rollback: the step-9 flip is reversible in one command; MIDI fallback keeps the Teensy playing if `demiurge-io` dies mid-set (this property is lost the moment the Teensy is unplugged from csound, as designed; EMGOR must accept that).

## 6. Registry gaps (what must be added so the 0..1 contract can cover every row)
1. Only 36 expr targets are generated into the pool registry. Every `$ENC_ROT` / `ExprSet`-less param (about 250 across ~100 effect codes) needs a generated manifest entry: parse `handler/dispatch/rot_*.orc` + `gen/expr_rot_*.orc` for `$ENC_ROT(var'step'min'max)` and `$ENC_ROT_EXP(...)` to get `{path /csound/<var>, native {min,max,curve}, step}`. One source of truth; do not hand-author a second list.
2. Per-effect on/off needs a path: proposed `/csound/fx/<toggle id>/on` (or `.../<code>/on`). The 71 toggle ids come from the registry in `neptr_full_reference.md` §11 (plus 75..98 rack).
3. The `native` block in manifests needs `steps` (enum count) and `labels` for step params (parameters.md §4 schema does not have them yet).
4. SOUNDMAGIC rack: `sm_params.orc` is generated; the pool should read it (like `expr_targets.json`).
5. One path scheme decision: parameters.md uses `/csound/<gkVar>` (e.g. `/csound/gkMasterVol`). This doc follows it. The UI/OSC echo names (`/od_drive`) and codes (`od.drive`) are a *second* naming; keep them as aliases in the registry only, per the "one mechanism" rule.

## 7. What EMGOR must confirm (ambiguous, guessed or not determinable from the repo)
1. **Encoder relative events**: inc/dec trigger pair vs a new `delta` mode (§4). Recommendation: add `delta`, do not use triggers (rate cap and coalescing in the pool would eat detents).
2. **Preset op shape**: 60+ trigger params vs `preset/slot` + `preset/op`.
3. **Menu slot orientation** for menus whose `menus.json` order is not the common s4->s0: PREAMP (listed s0->s4 per the "quirk" note), LOOPER `lp0..lp4`, NAM `nam0..nam4`, IR `ir4..ir0`. I assumed LOOPER lp0 = slot 0 / CC74 (header: `s0=ch1(74)`), NAM nam0 = AC30 = slot 4 (row 0), IR ir4 = WOOD RM = slot 4. Confirm against the on-device UI.
4. **PRESETS menu cell names**: `sld`/`hvy` vs the header's "Overdrive/SolidDist" and UI `SLOT_TO_PRESET {0:1, 1:2, 4:3}`; and what the bass/gorcore bank cells are called. Preset bank switching (is it ever live?).
5. **Menu numbers**: `neptr_full_reference.md` is stale (17 menus); I used `menus.json`. Confirm that doc is not a target.
6. **Arcade button and b2..b9, joystick X/Y/click**: no audio function today. Should the rewrite give them one, or just list them as unmapped sources? Currently the companion uses joystick + arcade for navigation (reads the MIDI bus).
7. **Onboard pedal on CC30 and LiDAR 1 on CC30** share a CC number. Harmless today (no reader); in the pool they are separate sources (`exp` vs `tof1`), so no action unless something starts reading CC30.
8. **Touch CC80-95 (ch16)** are named in comments ("touch") but I found no consumer in `neptr/csound/` and no sender in the UI. Reserved? Dead? Not determined.
9. **Wavetable OSC** has no stock consumer. Is a table type wanted in the pool?
10. **hyperGuitar lanes**: only lane 0 (PS GLIDE) is named in the repo; lane names, index meanings and ranges live in the Bela repo (`hg_lanes.cpp`), not here. Lane lengths 6, 9, 7, 10, 10 are from `hyperguitar.orc`. The design spec the file cites (`2026-09-15-hyperguitar-neptr-link-design.md`) is not in `neptr/docs/superpowers/specs/` here.
11. **Soft takeover vs slew**: do pedal and sensors both go through the pool `slew`, or do sensors alone (pedal keeps takeover, no slew)? Bench question.
12. **MIDI-by-channel exceptions**: CC110/CC111 (POLY DELAY) are channel-agnostic in the engine and would collide with any future ch7 button CC110/111 (they are protected today only by the `kCh != 7` gate). The pool must carry the channel in the source id (`/hw/midi/<ch>/<cc>`), which the `midi` source already plans.
13. **Param path convention for non-registry params** (§6.5): `/csound/<gkVar>` (my pick, matches parameters.md) vs `/csound/<code>/<param>`.
14. **Per-patch scope**: presets (the in-engine bank: `gkPresetOp/Slot`, `state_persist.orc`) and pool per-patch maps are two persistence systems for overlapping state. Not resolved here; flag before step 3 of the migration plan.
15. **Not verified**: ~250 parameter count (counted from `EFFECT_PARAMS`, not instrumented); all per-effect `gkVar` names except the 36 expr targets; whether every menu-0 bank cell appears in `menus.json`; behavior of the live rig (nothing was run).

## 8. Order of work (extends parameters.md §9)
Steps 1-8 of parameters.md (registry, OSC ingress, `{"param"}` targets, web API, learn, `midi` source/sink, serial_auto, per-language manifests) are independent of the Teensy path. This doc adds, between 8 and 9:
- 8a. Generate the NEPTR parameter manifest from the dispatch files (§6.1-6.3), read-only, nothing sent.
- 8b. Add the `/csound/neptr/*` intent params with Csound-side `/p`-style receivers that call the **existing** dispatch bodies (touches the audio side: needs direct permission each time).
- 8c. Stand up the `/cc` compat sink so the pool can re-emit the identical CCs; test with `scripts/claude_midi.sh`-style injected CCs at the bench (note: it injects PHYSICAL CCs).
- 9. Flip: Teensy MIDI disconnected from csound, `--forward-control` on, encoders/buttons routed by the pool maps (the remap table moves into the patch map). Bench, EMGOR present, rollback ready. Last.
