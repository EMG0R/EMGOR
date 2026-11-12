# PMOR controls map

Source: `ui/nav.cpp`, `ui/input.cpp`, `main.cpp`, `hw/sensors.cpp`, `pmor_core/fx.cpp`, `config.h` (commit d84a734 base).
Physical inputs: 16 trellis pads, 1 encoder (turn + press), 1 joystick (X/Y + press), FSR, lidar, IMU (accel XYZ), nothing else
(no extra buttons). "FREE" = the input does nothing in that state today and may be assigned.

## 1. Global rules (every menu)

| input | meaning |
|---|---|
| encoder turn | at root ring: previous/next main menu (DRUMS SYNTH SAMPLER FX PATCHES GLOBALS DASH, hard stops). Anywhere deeper: BACK one level per detent. |
| encoder press | tap tempo, always. Also closes the volume overlay. |
| joystick click | context (see per-menu). Never changes menu. |
| both buttons within 40 ms | volume overlay (the first single press is undone). Pads 0..15 or stick X set the master fader. |
| joystick flick | `IN_JOY_NAV`: one event per flick, auto-repeat 8 Hz after 350 ms |
| joystick 2 full circles | `IN_JOY_GESTURE`: reverse/forward on the focused pad (DRUMS/SAMPLER TIMBRE only) |
| pad press / release | per menu |
| pad hold | events at 500 ms then every 100 ms: DRUMS/SAMPLER TIMBRE = focus pad at 500 ms; SAMPLER = delete slot at 5 s (`NAV_HOLD_DELETE_MS`); PATCHES = delete patch at 5 s |
| FSR | FX depth (all FX bank logic reads `g.fsr`); recorded into FX automation. Nothing else. |
| lidar | FX only (DUB time, COMB pitch, GRAIN scrub, BUILDUP...). Nothing else. |
| accel X | synth PERF card: env macro (plucky..ambient); FX page tilt filter; passed to `fxProcess`. DASH shows it. |
| accel Y | synth PERF filter, **compiled out** (`PERF_ACCEL_FILTER 0`); FX page tilt |
| accel Z / gyro | displayed on DASH + screensaver only. FREE for control. |

Saver: 25 s idle -> screensaver; any non-pad input wakes it.

## 2. Per menu

Levels: RING (menu top), CARD (list), EDIT1 (stick-X slider), EDIT2 (XY), PADCOUNT, HANDSEQ, VOLUME.

### DRUMS / SYNTH / SAMPLER (the three engine menus)

Cards: TIMBRE, SEQ, PERFORM (joystick L/R at RING).

| level | pads | joystick | click | encoder turn |
|---|---|---|---|---|
| RING, TIMBRE card | play (drums trigger; synth note on/off; sampler play, or RECORD on an empty slot while held) | L/R = card; U/D **FREE** | enter CARD list | next/prev menu |
| RING, SEQ card | play + record hit if rec_on. **REC on an EMPTY lane waits for the first hit** (OLED: WAITING FOR 1ST HIT): that hit is step 0 of the loop. Own clock and nothing else playing: the transport restarts at the hit. Followed clock (Live/STUDIO/AUTO-follow) or other lanes playing: snapped to the nearest 16th, the lane keeps a start offset. Non-empty lane: overdub as before; CLEAR waits again | L/R = card; U/D = enter step editor | enter CARD list | menu |
| RING, PERFORM card | DRUMS/SAMPLER: play + route perf bus; SYNTH: chord memory (select-only in GEN/arp) | L/R = card; U/D **FREE** | enter CARD (no list -> back to RING, effectively **FREE**) | menu |
| CARD, TIMBRE | play | U/D = cursor; L/R = page (SYNTH 7 pages; DRUMS/SAMPLER have 1 page, so L/R **FREE**) | open editor on item (or CLEAR / BARS / STEPS special) | back |
| CARD, TIMBRE, pad focused (DRUMS) | tap another pad = move focus | L/R = sound -/+ (hard stop 0..15); U/D = pad volume +-10 %; 2 circles = reverse | un-focus | un-focus |
| CARD, TIMBRE, pad focused (SAMPLER) | tap another = move focus | X = formant, Y = pitch, live; 2 circles = reverse | un-focus | un-focus |
| CARD, SEQ | play | U/D = cursor; L/R **FREE** | edit item | back |
| CARD, PERFORM | DRUMS/SAMPLER: play; SYNTH: chord | live: SYNTH Y = octave, X = generative probability; DRUMS/SAMPLER = FX perf bus up/down/left/right throw | back to RING | back |
| EDIT1 | **FREE** (pads do nothing) | X = value (+-0.55 throw = full range); Y **FREE** | back | back |
| EDIT2 | **FREE** | X, Y = the pair | back | back |
| PADCOUNT | pad n = n+1 bars/steps | **FREE** | back (cancel) | back |
| HANDSEQ | pads = steps of the shown page | L/R = page; U/D = exit | next lane (auditioned) | back to RING |

Hold events: DRUMS/SAMPLER TIMBRE only. SYNTH pad hold, any SEQ/PERFORM pad hold, FREE.

Parameter pages today:
* DRUMS TIMBRE: 1 page. No pad focused = MIX (KICK SNARE HAT PERC volumes); pad focused = PAD (LENGTH PITCH TONE REV VOL). Sound choice is only reachable by focusing (hold pad 0.5 s) then L/R.
* SYNTH TIMBRE: 7 pages ALGO(ALGO, KEY) / algo knobs(4) / ENV(4) / FILTER(7) / PITCH(3) / WIDTH(2) / MIX(VOL).
* SAMPLER TIMBRE: 1 page. No pad focused = SAMPLER mix (VOLUME WIDTH WIDTHPRB POLY); focused = SAMPLE (PITCH FORMANT CUTOFF REVERSE).
* SEQ card: per-engine list from `seqParamPage`.

### FX

Never leaves RING.

| input | meaning |
|---|---|
| pads 0..15 | hold = engage that FX of the current bank (v2 punch-in), release frees it |
| joystick L/R | previous/next bank FX1..FX8 |
| joystick U/D | **FREE** (header comment in nav.cpp says the view follows the stick; only L/R is implemented) |
| joystick click HOLD | record automation for the current step (empty hands scrub it clean); a plain click does nothing |
| encoder turn | menu |
| FSR | depth of every held FX |
| lidar / accel | per-FX modulation |
| pad hold | **FREE** (no hold logic; FX uses plain press/release) |

### PATCHES

| input | meaning |
|---|---|
| pads 0..15 | load patch (page*16+pad) if it exists, else start a fresh one there |
| pad hold 5 s | delete that patch slot |
| joystick L/R | patch page A..D |
| joystick U/D | **FREE** |
| joystick click | **FREE** |
| encoder turn | menu (also `nav.cursor` mirrors the last pad) |

### GLOBALS

Lands straight in CARD. Items: KEY SCALE BPM INGAIN BRIGHT CLOCK ROTATE SWING GENRATE METRO FACTORY.

| input | meaning |
|---|---|
| joystick U/D | cursor |
| joystick L/R | **FREE** |
| joystick click | open editor (X = value); FACTORY item resets patch |
| pads | **FREE** (do nothing at all) |
| encoder turn | back, then menu |

### DASH

Read-only sensor page. Joystick click enters an empty CARD (a second click returns). Every input except encoder turn and the global rules is **FREE**: pads, joystick U/D/L/R, click.

## 3. FREE-control summary

Largest free real estate, in order of usefulness:

1. **Joystick U/D at RING on the TIMBRE and PERFORM cards of all three engines** (only SEQ card uses it, for the step editor). Natural: next/previous sound (engine presets).
2. **Joystick L/R in CARD on DRUMS, SAMPLER and the SEQ card** (single page, nothing to page).
3. **Joystick click on PATCHES RING and DASH**, and **joystick U/D on PATCHES and FX RING**.
4. **All 16 pads on GLOBALS, DASH, EDIT1, EDIT2** (pads are inert there).
5. **Pad hold on SYNTH, FX, and the SEQ/PERFORM cards** (hold gesture only exists on DRUMS/SAMPLER TIMBRE and PATCHES).
6. **Joystick Y in EDIT1**, **joystick stick in PADCOUNT**.
7. **Joystick button hold** everywhere except FX (hold timing never measured for the button; `g.joy_sw` is the raw level).
8. **Joystick button + pad chords**, **encoder turn while a pad is held** (no chord exists).
9. **Sensors**: FSR and lidar outside FX; accel Z / gyro anywhere.

## 4. Allocation (implemented on this branch)

Rule of thumb: **joystick U/D (or any flick) on the TIMBRE ring opens the tool of that page**, like U/D on the SEQ ring opens the step editor.

| function | control |
|---|---|
| sound browser, DRUMS / SYNTH (engine -> presets) | joystick U/D on the TIMBRE ring (initial flick). Inside: L/R = engine, U/D = preset, pad = pick the browsed pad (drums) or play (synth), click/encoder = leave |
| drum engine depth knobs (CLICK/SUB/DRIVE/CRUSH etc.) | DRUMS CARD, joystick L/R to page 2 |
| SYNTH pages | 4 pages (SOUND FILTER ENV MOTION), see docs/SYNTH_MENU.md |
| single sampler pad clear | hold the pad **3 s** (ramp on the pad). RAM length only, no file touched |
| blank ALL sampler slots | hold the joystick button **3 s** in SAMPLER (restores the state the click changed; OLED countdown bar). Nothing deleted |
| patch hard delete | **removed from the device** (pad-hold delete called `patchDelete`, which unlinks files). Belongs to the editor |
| sampler pool browser | SAMPLER > click > page 0 "POOL" (first sub-menu). U/D = walk samples from all patches, click = pick (whole grid flashes: bright = empty slot, red = would replace), the sample under the cursor auditions after the cursor rests 250 ms (background SD read, 1024 samples per main-loop pass, plays once loaded; scrolling stops it and reads nothing), tap a pad = assign (`patchAssignSample`), encoder/click = put down |
| reorder samples | SAMPLER ring, joystick U/D: tap source then target pad = swap |
| reorder patches | PATCHES ring, joystick U/D: tap source then an EMPTY target = `patchMoveSlot()` (weak hook, not yet in store/) |
| editor value behaviour | relative stick (no snap on open, re-anchors at centre) in every EDIT1/EDIT2 |
| synth RELEASE | ENV page flat with RELEASE first; and joystick U/D on the SYNTH PERFORM ring = release x1.25 / x0.8 (toast shows seconds) |
| manual clock mode | GLOBALS item CLKMODE (AUTO INTERNAL FOLLOW MASTER), `studioSetClockMode` |

Still FREE after this: joystick U/D on the DRUMS/SAMPLER PERFORM rings; L/R on SEQ card list; all pads on GLOBALS/DASH/EDIT; pad hold on SYNTH/FX/SEQ/PERFORM;
joystick-button hold outside SAMPLER/FX; chords; encoder turn while a pad is held; FSR/lidar outside FX; accel Z/gyro.

## Sound browser: user presets (DRUMS / SYNTH, joystick U/D on the TIMBRE ring)
| input | meaning |
|---|---|
| SYNTH joystick L/R | algorithm |
| SYNTH joystick U/D | preset 1..32 of the algorithm: 1-5 factory (INIT PLUCK PAD LEAD BASS, read-only), 6-32 user. Empty user slots show `-EMPTY-`, can be browsed, and do not change the sound |
| DRUMS joystick L/R, U/D | engine; sound 1..24 of the focused pad, then user entries 25..32 (base sound + the engine's 4 depth knobs). Empty ones show `-EMPTY-` |
| joystick HOLD 1.5 s | save the current sound into the **next free** user slot of this engine (never overwrites, factory slots are untouched); bar shows "SAVE USER", toast "SAVED U05". "USER FULL" when none is free. Verified write, `.BAK` kept; blocks ~0.5 s |
| joystick click | leave the browser (unchanged) |

## GLOBALS: RETRYSTO
Only listed (last item) when this boot skipped storage (boot guard). Open it and push the stick right to YES: runs `patchStoreRetry()` (re-enables and re-queues the storage job), toast "STORAGE RETRY" / "RETRY FAILED", item disappears after a successful retry.
