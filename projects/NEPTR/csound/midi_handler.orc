;==============================================================
; MIDI_HANDLER.ORC — Phase 4 Menu-Based MIDI Control System
;==============================================================
;
; ENCODER HARDWARE LAYOUT (7 encoders on Teensy) + ROLES.
;   Hardware enc -> CC (button / rotation), fixed by the Teensy firmware:
;     E1 70/20  E2 71/21  E3 72/22  E4 73/23  E5 74/24  E6 75/25  E7 76/26
;
;   ROLES (current pedalboard layout — applied by the remap in instr 99):
;     E7 = PREVIOUS-MENU encoder: BUTTON press = prev menu; ROTATION = secondary param
;     E5 = NEXT-MENU encoder:     BUTTON press = next menu; ROTATION = tertiary param
;     Slots, physical RIGHT -> LEFT = E1, E4, E3, E2, E6 = slot0 .. slot4
;       each: encoder BUTTON = toggle that effect, SAME encoder ROTATION = its dry/wet
;
;   The remap translates these to the LOGICAL CCs the dispatch below uses
;   (slot0 btn74/rot24, slot1 75/25, slot2 76/26, slot3 71/21, slot4 72/22;
;   menu prev=70, next=73; secondary param=20, tertiary=23), so the per-menu
;   effect tables below are UNCHANGED. Menu nav is a BUTTON press (not rotation).
;   The two menu encoders' rotations drive the last-toggled effect's secondary/
;   tertiary params (CC20/CC23) — the "other aspects" knobs.
;
; Encoder rotations: val>64 = CW increment, val<64 = CCW decrement
; CC29 = expression pedal (ABSOLUTE 0-127)
;
; 17 MENUS (CC70=prev, CC73=next, wrap 0-16) — 2026-07-15 reorder: GLITCH
; moves to sit right after CHASE BLISS, so menu-prev from PRESETS (wrapping)
; hits IR VERB, then NAM, GLITCH, CHASE BLISS, LOOPER:
;   0=PRESETS  1=VOLUME  2=SPECTRAL  3=PITCH  4=FILTERING  5=MOD  6=DRIVE
;   7=PREAMP  8=TIME  9=BASS  10=SYNTH  11=AIR
;   12=LOOPER  13=CHASE BLISS  14=GLITCH  15=NAM  16=IR VERB
;
; MENU 0  (PRESETS):   s4=Overdrive(72)    s3=SolidDist(71)    s2=Phaser(76)        s1=Tremolo(75)    s0=Ambient(74)
; MENU 1  (VOLUME):    s0=VolPedal(74)     s1=RingMod(75)      s2=Leslie(76)        s3=Attack(71)     s4=Tape(72)
; MENU 2  (SPECTRAL):  s4=BinCrush(72)     s3=BinRedu(71)      s2=SpecDly(76)       s1=Formant(75)    s0=Forms(74)
; MENU 3  (PITCH):     s0=H90(74)          s1=OldPS(75)        s2=GPS(76)           s3=PitchFork(71)  s4=—(72)
; MENU 4  (FILTERING): s0=LOSSY(74)        s1=RESONATR(75)     s2=WAH(76)           s3=AutoMoog(71)   s4=—(72)
; MENU 5  (MOD):       s0=Vibrato(74)      s1=Chorus(75)       s2=Phaser(76)        s3=Tremolo(71)    s4=Flanger(72)
; MENU 6  (DRIVE):     s0=TubeScrm(74)     s1=Marshall(75)     s2=RATxMZ(76)        s3=DS1K(71)       s4=BigMuff(72)
; MENU 7  (PREAMP):    s0=MainPre(74)      s1=BassPreL(75)     s2=BassPreH(76)      s3=BassDist(71)   s4=Compress(72)
; MENU 8  (TIME):      s0=Delay(74)        s1=—(75)            s2=Reverb(76)        s3=Granular(71)   s4=MOOD(72)
; MENU 9  (BASS):      s0=BassPreL(74)     s1=BassPreH(75)     s2=BassDist(76)      s3=OCTAVER(71)    s4=ENVFILTER(72)
;   BPL/BPH/BDS reuse toggles 40/41/42 (same effects as PREAMP menu);
;   OCTAVER=54 (instr 170), ENVFILTER=55 (instr 171).
; MENU 10 (SYNTH):     s0=TRACKSYN(74)     s1=PLLFUZZ(75)      s2=TONE EQ(76)       s3=—(71)          s4=—(72)
; MENU 11 (AIR):       s0=FEEDBACKR(74)    s1=DIFFUSION(75)    s2=—(76)             s3=—(71)          s4=CHAIN FB(72)
;   s2 emptied 2026-07-13: SHIMMER (toggle 65, instr 188) removed at the user's
;   call — a shimmer is achievable by routing the pitch shifter into reverb.
; MENU 12 (LOOPER):    s0=ch1(74)          s1=ch2(75)          s2=ch3(76)           s3=ch4(71)         s4=ch5(72)
; MENU 13 (CHASE BLISS): s0=BLOOPER(74)   s1=ONWARD(75)       s2=HABIT(76)         s3=MOOD II(71)    s4=—(72)
;   Sits right after the LOOPER on purpose: BLOOPER granulates the looper's
;   own loops. BLOOPER=56 (instr 208), ONWARD=45, HABIT=36. See the notes below.
; MENU 14 (GLITCH v2):  s0=IMPULSE(74)     s1=BBCUT(75)        s2=REPEAT(76)        s3=CHOP(71)       s4=RVRS(72)
;   2026-07-15 TOTAL REBUILD (see stutter.orc header) — sits right after CHASE
;   BLISS. Every effect is CLOCKED to the shared CHASE BLISS subdivision tick
;   (instr 89), with a per-effect PROBABILITY knob on P2/CC20 and the
;   expression pedal (CC29) as an instant force-trigger. REPEAT/CHOP/RVRS
;   reuse the old WGuide/SqMod/FreqShift toggles+buses (23/24/25); BBCUT (20)
;   and IMPULSE (48) are unchanged ids. WGUIDE/SQMOD/FSHIFT/old-BBCUT-v1 are
;   retired (effects/_retired/stutter_v1_glitchmenu.orc).
; MENU 15 (NAM):       s0=VoxAC30(74)      s1=DeluxeR(75)      s2=FenderSR(76)      s3=JC120(71)      s4=TwinR(72)
; MENU 16 (IR VERB):   s0=SPRING(74)       s1=PLATE(75)        s2=HALL(76)          s3=CHAMBER(71)    s4=WOODRM(72)
;   NAM-style: slot button = row on/off (toggles 49-53), slot rotation =
;   variant browse within the row (gkIRSel[r]).
;
; NAM / IR PARAMS LIVE ON THE MENU ENCODERS (fixed 2026-08-11). These two
;   menus are the only ones that spend ALL FIVE slot rotations on their
;   variant browse, so the usual "slot rotation = this effect's dry/wet"
;   P1 convention is unreachable inside them — and the on-device UI draws
;   those sliders on exactly those pages, so both the encoder and the touch
;   slider were dead. CC20 (P2) and CC23 (P3) are the two rotations no menu
;   shadows, so:
;     NAM   (toggle 37): CC20 = gkNAMInGain  (pre,  -12..+18 dB, unity 0)
;                        CC23 = gkNAMOutGain (post, -18..+18 dB, unity 0)
;     IR    (toggles 49-53): CC20 = gkIRRowMix[r] per-row DRY/WET 0..1
;                            CC23 = gkIRDamp master wet tone 0..1
;   gkIRLevel lost its encoder in the same change: it scaled the wet on top
;   of the per-row dry/wet — two knobs for one job. It stays pinned at 1.0.
;   Every NAM row's slot button now sets gkLastToggle = 37 (only row 4's
;   did before, so engaging any other amp left the gains unarmed).
;
; 2026-07-13 CHASE BLISS MENU (13 since the 2026-07-15 reorder) — the three
;   Chase Bliss ports gathered into
;   one home, placed RIGHT AFTER the LOOPER (menu 12) because BLOOPER is a
;   granular modifier OF the looper's loops. NAM/IR/BASS/SYNTH/AIR each shifted
;   up one (historical; superseded by the 2026-07-15 reorder, which also
;   inserts GLITCH between CHASE BLISS and NAM — the wrap modulus stays 17).
;   The three were MOVED, not copied: BLOOPER left FILTERING s4, ONWARD left
;   PITCH s4, HABIT left TIME s1; all three vacated slots are now EMPTY.
;     BLOOPER 56 (instr 208): P1 CC24 level, P2 CC20 motion, P3 CC23 SCATTER,
;                 P4 CC25 grain size, P5 CC26 grain character. CC29 = manual
;                 chaos push (sticky claim gkLastToggle29 = 56). PLAIN TOGGLE —
;                 the old raw press/release gesture machine is gone.
;     ONWARD  45: P1 CC22 -> CC25 (pad level). No fills.
;     HABIT   36: P1 CC25 -> CC26 (mix). Blend stays on CC22. Its BUTTON is no
;                 longer a plain toggle: click = ON + recording, click again =
;                 freeze/unfreeze the ring buffer, double-click = off
;                 (gkHab_rec — see habit.orc).
;   P2 (CC20), P3 (CC23) and the CC29 expression mappings are lastToggle-keyed,
;   NOT slot-derived, so they are untouched by the move.
;   s3 is MOOD II (toggle 66, instr 189); s4 is free.
;
; 2026-07-13 BLOOPER REARCHITECTURE — BLOOPER (56, instr 208) is NOT a looper.
;   It owns no buffer, records nothing and has no transport of its own. It is a
;   GRANULAR MODIFIER of the 5-channel LOOPER: when on, signal_chain swaps the
;   looper's direct playback for BLOOPER's granular render of the very same
;   buffers (a REPLACE, never a sum — loops are never doubled). At SCATTER=0
;   that render is a Hann/COLA reconstruction of normal playback: an exact null.
;   looper.orc and handler_looper.orc are NOT touched; BLOOPER only READS
;   gkLpState/Len/RdPos/SpIdx and the giLpBuf ftables.
;   LOSSY (s0) and RESONATR (s1) deliberately STAY in FILTERING.
;
; 2026-07-13 EFFECT BATCH: toggles 57=COMPRESSOR(180) 58=TONE EQ(181)
;   60=CHAIN FB(183) 61=TRACKSYNTH(184) 62=PLLFUZZ(185) 63=FEEDBACKER(186)
;   64=DIFFUSION(187). (65/188 = SHIMMER, REMOVED 2026-07-13.) Two NEW menus: 11 SYNTH (tracked-pitch
;   voices + the tone stack that feeds them; s3/s4 free) and 12 AIR
;   (space/texture/feedback; s3 free). COMPRESSOR lands in the PREAMP menu's
;   free s4 (dynamics belong with the front end).
;   TAPESTOP (was toggle 59 / AIR s3) REMOVED 2026-07-13 at the user's call —
;   the TAPE machine (toggle 21) already covers tape behaviour; its file is
;   retired to effects/_retired/tapestop.orc. Toggle 59 + instr 182 are FREE.
;   Next free toggle: 59 (then 66). Next free instr: 182 (then 189).
;   DELAY also gained a REVERSE mode (gkDlyMode, P5 fill on CC25, OSC /dly_mode).
;   PER-PRESET OUTPUT TRIM: menu 0 + CC20 nudges giPresetTrim[gkCurrentPreset]
;   +/-6 dB in 0.5 dB steps; applied at the master stage; OSC /preset_trim.
;
; 2026-07-13 batch: toggles 45=ONWARD(155) 46=LOSSY(123) 47=RESONATOR(157)
;   48=IMPULSE(156) 49-53=IR VERB rows(240) 54=OCTAVER(170) 55=ENVFILTER(171).
;   GLITCH menu shifted one slot left (s0 is IMPULSE, a PLAIN toggle —
;   2026-07-15: now also fully v2, see MENU 14 above; IMPULSE is a clocked
;   member of the same contract, not an outlier). WAH/AutoMoog moved to
;   FILTERING s2/s3; H90 moved to PITCH s0 (ONWARD takes s4).
;
; DRIVE MENU 2026 (menu 5) — toggle IDs 31-35, instrs 130-134.
;   Uniform control scheme for all 5 pedals:
;     CC29 expression (abs) -> drive amount   (lastToggle29-dispatched)
;     slot encoder rotation -> tone
;     CC20 (E7 rot)         -> wildcard 1     (lastToggle-dispatched)
;     CC23 (E5 rot)         -> wildcard 2     (lastToggle-dispatched)
;   Wildcards: OD(TS): body (hump corner) / clean blend.  MAR: presence / tightness.
;   DHG(DS-1K): fat (low corner+shelf) / edge (LED asym + presence).
;   RMZ: mid-sweep freq / mid gain.  MUF: mid recovery / voicing morph.
;   NOTE: the new DRIVE pedals are fully separate from the always-on
;   PREAMP front-end (effects/preamp.orc) — different instrs/globals.
;   Spec: docs/specs/2026-06-11-drive-menu-distortion-redesign.md
;
; PRESET SYSTEM (menu 0) — physical layout left→right: OD, SOLID-DIST, PHASER, TREM, REVERB
;   OD, SOLID-DIST and REVERB are mutually-exclusive presets; PHASER and TREM are
;   stackable modifiers (toggle on top of whatever preset is active, cleared on
;   preset select). Pressing an active preset button again → goes dry.
;   gkCurrentPreset: -1=dry, 1=overdrive, 2=solid-dist, 3=ambient/reverb
;
;   OVERDRIVE (s4/CC72, leftmost):  the mixable dirt channel. Vibrato + spring
;       delay base (unified giPreset* values) + OVERDRIVE (pure Tube Screamer).
;       Default drive 0.7 = halfway breakup->max; CC29 expression pedal sweeps
;       gkOD_drive 0..1 = clean..dirty. CC22 (slot enc) also sweeps drive so
;       you can nudge a new home position. MasterVol stays at 1.0 (full).
;   SOLID DIST (s3/CC71):  the heavy channel. Same vib + spring delay base +
;       DUAL HIGH-GAIN (inline) at the dialed-in spot (drive 0.5). CC29 on
;       master vol (swell/volume control). CC21 (slot enc) sweeps gkDHG_drive.
;   PHASER       (s2/CC76):  modifier. 4-stage MXR-style, 2x faster, warmer (analog).
;   TREMOLO      (s1/CC75):  modifier. amplitude trem at gkTremRate (13.125 Hz).
;   REVERB/AMBIENT (s0/CC74, rightmost): vibrato + spring delay base + spring reverb
;       (FB=0.92, mix=0.70) + master vol pedal via CC29.
;
;   Preset-owned effects: VIB, Delay, TS, Reverb, VolPedal, Tremolo, Phaser.
;   Other effects (granular, etc.) are untouched by preset switches.
;
; PRESET FIXED SETTINGS:
;   Vibrato (all presets):  freq/depth/fback from giPresetVib* (depth 0.001265 = +10%)
;   Spring delay (all presets): giPresetDel* (time ~60.6ms, fback 0.62, wet 0.108 = +20%)
;   TS drive (OD preset):  default 0.7, CC29 expr pedal sweeps 0..1
;   Reverb (amb):   FB=0.92, mix=0.70
;   VolPedal (amb): starts at 1.0, CC29 controls it (gkLastToggle29=30)
;   NOTE: preset vib/delay use giPreset* values — DISTINCT from the manual-toggle
;   init defaults (gkVIB_depth=0.004 / gkDelMix=0.5). Bump lives in globals.orc.
;
; ACCUMULATOR RULE: Each slot's rotation CC controls "amount" for that slot's effect.
;   CC24(s0), CC25(s1), CC26(s2), CC21(s3), CC22(s4)
;
; CC20/CC23: lastToggle-dispatched (secondary/tertiary params)
; CC29: lastToggle29-dispatched (sticky — only updates when toggling CC29-using effect)
;
; GLITCH v2 (menu 14): BBCUT(20), REPEAT(23, reuses WGuide bypass/mix),
;   CHOP(24, reuses SqMod bypass/mix), RVRS(25, reuses FShift bypass/mix),
;   IMPULSE(48). No shared group any more — each effect owns its own
;   gk*_prob (P2/CC20) and gk*_expr (CC29 instant force-trigger); all five
;   fire off the one shared CHASE BLISS subdivision tick (instr 89, stutter.orc).
;==============================================================
;
; CONTROL INPUT TRANSPORT (2026-08-12)
; -----------------------------------
; MIDI is no longer the only way in. The engine also listens for OSC on
; $CTL_OSC_PORT (default 7770), which carries two verbs:
;
;   /cc  <ch> <cc> <val>   an ordinary control change. It is injected into the
;                          SAME kCh/kCC/kVal the midiin call produces, so it
;                          falls through handler_remap -> handler_looper ->
;                          handler_menus completely unmodified. A /cc is
;                          indistinguishable from the Teensy's own MIDI.
;   (a /p verb for absolute parameter writes lands next; it calls the
;    generated ExprSet UDO in handler/expr_registry.orc.)
;
; WHY. The Teensy streams a 100 Hz frame carrying two LiDARs, a 3-axis accel,
; two FSRs, a joystick and two piezo mics on top of the encoders. `midiin`
; returns ONE message per k-block, so intake caps at kr — 750 messages/sec at
; ksmps 64. Twelve sensors at 100 Hz is 1200/sec before a single encoder is
; touched. The MIDI path physically cannot carry the sensor set; OSC, drained
; in a loop, can. Verified: a burst of 20 messages drains in one k-cycle.
;
; MIDI REMAINS THE FALLBACK, NOT A LEGACY PATH. -Ma stays in CsOptions and the
; midiin call below is untouched, still consuming exactly one message per
; k-block as it always has. If the Python control plane is not running, the
; Teensy's direct USB-MIDI still plays the instrument with identical feel.
; Encoder and toggle behaviour is therefore unchanged either way.
;==============================================================

; Control-input OSC port. Distinct from 8000, which this engine SENDS state on
; (handler_osc.orc) and the UIs listen to. Override with --omacro:CTL_OSC_PORT=n
#ifndef CTL_OSC_PORT
#define CTL_OSC_PORT #7770#
#endif
giCtlOSC OSCinit $CTL_OSC_PORT

; HIGH-RESOLUTION SENSOR FEED (2026-11-05, EMGOR: every continuous control >= 1024
; steps). serial_bridge.py sends each analog (jx jy exp fsr tof accel mic) as
;   /ccf <ch> <cc> <val>   "iif": val is a FRACTIONAL 0..127 (float32, >> 1024 steps)
; to the same port. It enters the dispatch exactly like /cc (kFromOsc = 1) with
; a fractional kVal; kNorm = kVal/127 is then a fine 0..1. The 7-bit copies of
; those same CCs (wire MIDI from the bridge, and the pool's /cc 1 29 relay) are
; DROPPED while the /ccf for that CC has been seen in the last giHrHold seconds, so the
; quantized and fine values can never fight. When the bridge is down or silent
; (no pedal in the jack, UI slider on /cc 1 29) nothing is dropped.
; giHrT[cc] = (time of last /ccf for ch1 cc) + 1; 0 = never.
giHrT ftgen 0, 0, 128, -2, 0
giHrHold = 1.0

; CLOCK CONTROLS: EVENT-DISPATCH, NOT ctrl7 (fixed 2026-10-01).
; ---------------------------------------------------------------
; All four ch16 clock controllers (CC119 master BPM, CC116 role, CC114
; tempo-source-real, CC104 manual BPM) used to be read with `ctrl7`, a
; continuous per-k-block read of Csound's internal MIDI controller table.
; That table is NEVER updated on this rig: instr 99 opens the port with
; `midiin`, which consumes the raw MIDI event itself (see the "CONTROL
; EVENT LOOP" comment above instr 99 -- "still consuming exactly one
; message per k-block as it always has"), so the controller table ctrl7
; reads from never sees a write. Measured on hardware 2026-10-01:
; `kMasterBpm ctrl7 16, 119, 40, 400` read a constant 0 all day (-> decoded
; to the floor, 40 BPM) while `aseqdump -p 14:0` showed real CC119 traffic
; on the wire and the live `/beat_period` OSC output was locked to exactly
; 1.5 s = 40 BPM, while the daemon itself held 124 BPM. ctrl7 is dead on
; this orchestra; midiin is the only mechanism that actually receives
; anything here, same as every other working control in this file (the
; ch16 CC102/103/105-107 handlers a few hundred lines down, which read
; kCh/kCC/kVal straight off the midiin-fed dispatch loop).
;
; FIX: all four moved onto that same midiin/OSC dispatch loop (see the ch16
; CC119/CC114/CC104 blocks inside the CONTROL EVENT LOOP below -- CC116 is
; read already, for the DUODECIMUS preset-capture gesture, unrelated to
; clock). One mechanism for reading MIDI in this file, not two.
;
; initc7 is gone with it -- it existed only to paper over ctrl7's "unset
; reads as 0" behaviour, which is moot once nothing reads ctrl7 at all.
; The event-dispatch replacement simply never writes gkManualBpm /
; gkGLITCH_bpmExternal until a real event for that CC arrives, which is
; what "leave the global at its init value" actually means (see
; Requirement 4 in the migration notes below, and globals.orc's own
; gkManualBpm init 120 / gkGLITCH_bpmExternal init, both NOT touched here).

; Local one-shot state for the CC119 edge detector (owned by this file,
; not globals.orc): the last dispatched/decoded master-BPM value, so the
; edge fires on "value actually changed from the last one we saw" even
; though midiin events are no longer run through Csound's own `changed()`
; controller-table bookkeeping. -1 is not a reachable decode (range is
; 40..400), so the very first CC119 event always edges, matching the old
; ctrl7 + changed() behaviour where the first touch of an untouched
; (0-reading) controller counted as a change too.
gkMidiH_prevMasterBpm init -1
; 14-bit tempo (ch16 CC120 MSB + CC121 LSB, 2026-10-05): the CANONICAL wire value,
; 40 + ((MSB*128)+LSB)*360/16383 (0.022 BPM/step). CC119 above is the legacy 7-bit
; value (2.8 BPM/step) and is only trusted while no 14-bit pair has arrived in the
; last 3 s -- so the number NEPTR holds (and shows) is the one PMOR/the Pi hold.
gkMidiH_wireMsb      init 0
gkMidiH_wireHaveMsb  init 0
gkMidiH_wireT        init -1000
gkMidiH_oscBpmT      init -1000   ; last /clk/bpm (pmor-clock-relay), see below

#ifndef CLK_EXT_FRESH
#define CLK_EXT_FRESH #3.0#
#endif
instr 99

  ; Master clock sync — DEMIURGE broadcasts BPM on ch16 CC119 (0..127 → 40..200,
  ; sent once per beat, continuously, whether or not the tempo actually
  ; changed — see demiurge/docs/clock.md). The raw value ALWAYS mirrors into
  ; gkGLITCH_bpmExternal here, unconditionally — this instrument no longer
  ; decides whether it's authoritative for gkGLITCH_bpm; that arbitration
  ; (external vs loop-derived) happens in chase_bliss_clock.orc instr 100,
  ; which runs immediately after this instrument every k-block (100 > 99,
  ; ascending instrument-number execution order — see that file's header)
  ; and can therefore act on gkGLITCH_extBpmEdge from the SAME block it was
  ; set here.
  ; Range is 40..400, NOT 40..200: demiurge-clock.cpp encodes the broadcast as
  ; bpm = 40 + (val/127)*360. Decoding at 40..200 misread every tempo (a real
  ; 120 BPM arrived as 75.3). clock.md's prose says 40..240 and is stale — the
  ; daemon source is authority. Fixed 2026-07-14.
  ;
  ; 2026-10-01: moved off `ctrl7` onto the midiin/OSC dispatch loop below (see
  ; the "CLOCK CONTROLS: EVENT-DISPATCH, NOT ctrl7" note above this instr) --
  ; ctrl7 was reading a controller table `midiin` never populates and was
  ; measured returning a constant 0 (-> 40 BPM floor) on live hardware while
  ; CC119 traffic was confirmed present on the wire. gkGLITCH_extBpmEdge is
  ; cleared here, once per k-block, and raised inside the dispatch loop's ch16
  ; CC119 branch only when a genuine event's decoded value differs from the
  ; last one seen (gkMidiH_prevMasterBpm) -- a one-k-block pulse, same
  ; contract chase_bliss_clock.orc already expects from `changed()`.
  ; gkGLITCH_bpmExternal is left untouched here (and therefore holds
  ; whatever globals.orc initialised it to) until that first real CC119
  ; event arrives -- an absent controller must never decode to the 40 BPM
  ; floor, which is the exact bug this migration fixes.
  gkGLITCH_extBpmEdge = 0
  ; Latch "a real external clock is attached" — see globals.orc
  ; gkGLITCH_extBpmSeen. Used by looper.orc to gate CLOCKED grid-true
  ; capture vs UNCLOCKED free capture + trailing-silence trim.
  ;
  ; HISTORY (2026-08-12): used to latch on `gkGLITCH_extBpmEdge`, i.e. any
  ; CHANGE in the CC119 tempo broadcast — but CC119 is demiurge-clock's OWN
  ; broadcast, sent once per beat whether or not anything external exists.
  ; gkGLITCH_bpmExternal inits to 95 and the clock broadcasts the live tempo
  ; (124 here), so the first beat after every boot was a change, the latch
  ; fired, and it "stays latched for the process lifetime" by design. The
  ; engine therefore believed an external clock was attached within one beat
  ; of EVERY boot, having heard only itself. That version was reverted —
  ; DO NOT bring it back by latching on a CC119 edge.
  ;
  ; HISTORY (2026-08-12 fix, since superseded): switched to keying on the
  ; clock's ROLE instead (CC116: 1 = daemon is FOLLOWING an external master,
  ; 2 = daemon IS master). That was correct back when PMOR was unconditionally
  ; master, so CC116==2 only happened once PMOR had gone fully silent and the
  ; daemon reclaimed — a real, if coarse, signal. It broke the moment PMOR was
  ; fixed to stop claiming the clock while silent: the daemon now correctly
  ; holds MASTER (CC116=2) as its NORMAL resting state, so CC116==2 no longer
  ; tells you anything about whether a real grid exists. Latching on it (as
  ; before) misses every real daemon-master tempo; latching on CC116==2 as
  ; well as ==1 would reproduce the ORIGINAL 2026-08-12 bug by a different
  ; route — it would be true whenever the daemon is merely alive, and a bare
  ; `live.conf bpm = 124` default is not a grid anyone chose.
  ;
  ; THIS KEYS ON CC114 — the tempo-source CLASS, not the clock's role
  ; (2026-10-01). demiurge-clock.cpp already runs a tempo-source priority
  ; ladder (CLOCK_SRC_* in demiurge-clock-pll.h: HUMAN_GESTURE, EXTERNAL_MIDI,
  ; CC118_STEER, LINK, each a REAL driver, vs LIVE_CONF_BPM, the startup/
  ; default value) to decide g_bpm/CC119 in the first place — CC114 just puts
  ; that same ladder's verdict on the bus: 127 = some real source (an Ableton
  ; Link peer actually driving tempo, an external MIDI clock, a CC118 steer, a
  ; human tap-tempo gesture) is currently authoritative; 0 = tempo is only the
  ; live.conf/startup default, nobody is actually driving it. That is the
  ; honest answer to "is there a grid I did not create", independent of
  ; whether the daemon happens to be calling itself master or follower right
  ; now.
  ;
  ; kClockRole (raw CC116 role) was READ here historically "in case anything
  ; else needs role" but nothing in this orchestra ever did — grepped
  ; 2026-10-01, the only other CC116 reader in this file is the unrelated
  ; ch7 DUODECIMUS preset-capture gesture a few hundred lines down (different
  ; channel, different meaning entirely). Per the "one mechanism, not two"
  ; rule, the dead kClockRole read is DELETED along with the ctrl7 migration
  ; below rather than carried forward unused. If a future consumer needs the
  ; daemon's role, add a ch16 CC116 branch to the dispatch loop the same way
  ; CC114/CC119/CC104 were added, not a new ctrl7 read.
  ;
  ; The consequence that made this matter in the first place lands on the
  ; looper: with the grid marked available, LP_QCLOSE takes the CLOCKED branch
  ; for the very first loop and quantises it to the current tempo — closing
  ; "early" parks the channel in state 5, still recording, waiting for a grid
  ; line. Solo, with nothing plugged in and nothing real driving tempo, that
  ; reads as the looper hanging on a clock that does not exist. The first loop
  ; is supposed to DEFINE the length, not be snapped to an arbitrary default.
  ;
  ; Still a ONE-WAY latch, same as every prior version: a master that drops
  ; out mid-set must not start baking unquantised silence into new loops —
  ; once a real source has been seen, this engine keeps believing a grid
  ; exists for the rest of the process, even if that source later goes quiet.
  ;
  ; Threshold convention: >=64 means "true" on this bus, same as CC117
  ; (transport, >=64 = start) — demiurge-clock.cpp only ever sends CC114 as
  ; 0 or 127, so this is a hard boundary, not a near-miss.
  ;
  ; 2026-10-01: the actual latch write moved off `ctrl7` onto the dispatch
  ; loop's ch16 CC114 branch below, for the same reason as CC119 above —
  ; ctrl7 never saw a real CC114 event on this rig, so `kTempoReal >= 64`
  ; was permanently false and the latch never fired at all (a second,
  ; independent symptom of the same dead-ctrl7 root cause as the 40 BPM
  ; floor). No local state needed here: the latch only ever sets to 1 and
  ; never resets, so there's nothing to clear each k-block the way
  ; gkGLITCH_extBpmEdge needs to be.

  ; UTILITY page manual master BPM — ch16 CC104. 2026-10-01: moved off
  ; `ctrl7` (and the `initc7 16, 104, 0.3076923077` pre-seed that existed
  ; only to paper over ctrl7's "unset reads as 0" behaviour) onto the same
  ; midiin/OSC dispatch loop as CC119/CC114 below, for the identical reason:
  ; ctrl7 never saw a real event here either. gkManualBpm is simply left
  ; alone until a genuine CC104 event arrives, so it keeps whatever
  ; globals.orc initialised it to (120, globals.orc:356) rather than being
  ; forced to a floor or re-seeded by hand.

  ; UI late-join sync (demiurge/docs/engine-ui.md): clear last cycle's pulse
  ; before this cycle's MIDI can raise it again — see globals.orc gkOscRefresh.
  gkOscRefresh = 0
  ; Step the staggered refresh sweep (globals.orc gkOscRefSlot): started the
  ; cycle after a UI sync request, one slot per k-cycle, then idle (-1).
  if (gkOscSweepReq == 1) then
    gkOscSweepReq = 0
    gkOscRefSlot = 0
  elseif (gkOscRefSlot >= 0) then
    gkOscRefSlot += 1
    if (gkOscRefSlot >= $OSC_REF_SLOTS) then
      gkOscRefSlot = -1
    endif
  endif

  ; BLOOPER OFF WITH NO LOOPS (2026-11-01, EMGOR): when every looper channel is
  ; empty/stopped (state 0 idle or 4 stopped; 1,2,3,5,6 = recording/playing/
  ; overdub/arming) force gkBLP_bypass = 1. Edge-triggered on the "loops active"
  ; -> "none" transition (and once at engine start), so a manual toggle is never
  ; fought while loops exist.
  kBlpAny = 0
  kBlpS1 = gkLpState1
  kBlpS2 = gkLpState2
  kBlpS3 = gkLpState3
  kBlpS4 = gkLpState4
  kBlpS5 = gkLpState5
  if (kBlpS1 == 1 || kBlpS1 == 2 || kBlpS1 == 3 || kBlpS1 == 5 || kBlpS1 == 6 || kBlpS2 == 1 || kBlpS2 == 2 || kBlpS2 == 3 || kBlpS2 == 5 || kBlpS2 == 6 || kBlpS3 == 1 || kBlpS3 == 2 || kBlpS3 == 3 || kBlpS3 == 5 || kBlpS3 == 6 || kBlpS4 == 1 || kBlpS4 == 2 || kBlpS4 == 3 || kBlpS4 == 5 || kBlpS4 == 6 || kBlpS5 == 1 || kBlpS5 == 2 || kBlpS5 == 3 || kBlpS5 == 5 || kBlpS5 == 6) then
    kBlpAny = 1
  endif
  kBlpPrev init 1
  if (kBlpAny == 0 && kBlpPrev == 1) then
    gkBLP_bypass = 1
  endif
  kBlpPrev = kBlpAny

  ; ---------------- CONTROL EVENT LOOP ----------------
  ; One pass per pending control event. The FIRST pass takes the MIDI message
  ; from midiin — exactly the one-per-k-block behaviour the rig has always had,
  ; unchanged. Subsequent passes drain the OSC queue until it is empty.
  ;
  ; The dispatch body below is #included ONCE (the preprocessor expands text),
  ; but the `while` executes it per event, so a burst of sensor traffic is
  ; fully consumed in the k-block it arrived in instead of backing up.
  kOscCh  init 0
  kOscCC  init 0
  kOscVal init 0
  kOscGot init 0
  kMoreCtl init 1
  kFromOsc init 0   ; 1 when this pass's message came from OSC, not midiin
  kOscValF init 0
  kFromHi  init 0   ; 1 when this pass's message is a high-res /ccf
  kHrDrop  init 0   ; 1 = 7-bit duplicate of a live /ccf control: skip dispatch

  kSt, kCh, kCC, kVal midiin
  kMoreCtl = 1

  while (kMoreCtl == 1) do

    ; kSt == 0 means "no message in hand" — either midiin had nothing this
    ; block, or the previous pass already consumed its event. Pull the next
    ; OSC event; when that runs dry the loop ends.
    kFromOsc = 0
    kFromHi  = 0
    kHrDrop  = 0
    if (kSt == 0) then
      ; /ccf first: its mark must be set before the 7-bit /cc relay of the same
      ; movement is judged below.
      kOscGot OSClisten giCtlOSC, "/ccf", "iif", kOscCh, kOscCC, kOscValF
      if (kOscGot == 1) then
        kSt  = 176
        kCh  = kOscCh
        kCC  = kOscCC
        kVal = kOscValF     ; fractional 0..127
        kFromOsc = 1
        kFromHi  = 1
      else
        kOscGot OSClisten giCtlOSC, "/cc", "iii", kOscCh, kOscCC, kOscVal
        if (kOscGot == 1) then
          kSt  = 176          ; present it as a channel-1-style CC status
          kCh  = kOscCh
          kCC  = kOscCC
          kVal = kOscVal
          kFromOsc = 1        ; see the pedal-demotion block below
        else
          kMoreCtl = 0
        endif
      endif
    endif

    ; HIGH-RES BOOKKEEPING (see giHrT). Only ch1, the bridge's analog CCs.
    if (kMoreCtl == 1 && kSt >= 176 && kSt < 192 && kCh == 1 && ((kCC >= 27 && kCC <= 36) || kCC == 78 || kCC == 79)) then
      kHrNow times
      if (kFromHi == 1) then
        tablew kHrNow + 1, kCC, giHrT
      elseif (tab:k(kCC, giHrT) > 0 && (kHrNow + 1) - tab:k(kCC, giHrT) < giHrHold) then
        kHrDrop = 1
      endif
    endif

    ; ONBOARD EXPRESSION PEDAL -> CC30 (2026-09-15). TWO PEDALS, ONE CC, AND
    ; THEY FIGHT.
    ;
    ; The NEPTR body's own Teensy sends its expression pedal as ch1 CC29 over
    ; USB-MIDI, and demiurge-midi-connect.sh fans EVERY physical port into Midi
    ; Through 14:0, which is what this engine opens. So once the body is plugged
    ; in, its pedal and the DUODECIMUS treadle (funnelled to ch1 CC29 just
    ; below) are the same message arriving from two places, and by then they are
    ; indistinguishable.
    ;
    ; They do not average, they fight: expr_cc29.orc's soft-takeover DISARMS on
    ; every owner change and re-arms only after a physical move >0.03, so a
    ; second writer makes the pedal intermittently ignore the player. That is
    ; exactly the reported symptom — the treadle worked perfectly while the body
    ; was unplugged and "started tweaking again" the moment it was plugged in.
    ;
    ; The user's call: the DUODECIMUS treadle IS the expression pedal, and the
    ; onboard one does nothing for now. So the onboard pedal is parked on CC30,
    ; which no handler reads — it stays wired and re-pointable later without
    ; touching firmware, but it cannot contend for CC29.
    ;
    ; ORDER IS LOAD-BEARING: this runs BEFORE the DUODECIMUS funnel, so the
    ; funnel's own output (ch1 CC29) is never demoted by it.
    ;
    ; kFromOsc EXEMPTS the OSC path on purpose. The web panel's virtual NEPTR
    ; and demiurge-io's sensor router both push ch1 CC29 as `/cc`, and those are
    ; deliberate software writes with no second physical writer behind them -
    ; demoting those would break testing-with-no-hardware, which is the normal
    ; case on a bare Pi.
    ;
    ; SOURCE SELECTION MOVED TO THE POOL (2026-10-22). This engine has ONE
    ; expression input: ch1 CC29 delivered as OSC /cc (kFromOsc == 1) by
    ; demiurge-io. WHICH physical pedal feeds it - the DUODECIMUS treadle (wire
    ; ch7 CC14/15) when it is plugged in, the body pedal (wire ch1 CC29) when it
    ; is not - is decided by presence-conditional maps in the pool
    ; (pool/neptr_default.map.json, docs/expression_pedal.md), not here. So a
    ; ch1 CC29 that arrives on the MIDI WIRE is always parked on CC30 (nothing
    ; reads it), and the old ch7 CC14/15 -> ch1 CC29 rewrite is gone: the pool
    ; reads those from the same Midi Through bus. One mechanism.
    ;
    ; ch16 CC122 (any value, normally from the pool) = "the expression SOURCE
    ; changed": bump the epoch so expr_cc29.orc re-arms soft takeover instead of
    ; slamming the target to the new pedal's resting position.
    if (kSt >= 176 && kSt < 192 && kCh == 16 && kCC == 122) then
      gkExprSrcEpoch = gkExprSrcEpoch + 1
    endif
    if (kFromOsc == 0 && kSt >= 176 && kSt < 192 && kCh == 1 && kCC == 29) then
      kCC = 30
    endif

    ; DUODECIMUS SHOW LAYER (2026-11-01), ch7 only. docs/duodecimus_show_map.md.
    ; Wire (firmware): encoder N rotate = CC(19+N) RELATIVE (127 = CW, 0 = CCW);
    ; encoder N press = CC(101+N) 127 down / 0 up; CC116 value=N = 5 s hold;
    ; CC117 value=N = 8 s hold. Numbered column by column: top row 1,3,5,7,9,11,
    ; bottom row 2,4,6,8,10,12.
    ;   enc1  AMP      rotate = step the NAM model (flat 0..24, wraps); press = NAM on/off
    ;   enc2  CLEAN    recall slot 2 (every FX bypassed)
    ;   enc3 DRIVE  4 VIBRATO  5 SWELL  6 CHORUS  8 LESLIE  9 SPECTRAL
    ;   enc10 TAPE DELAY  12 AMBIENT REVERB   = recall bank slot N; pressing the
    ;         encoder of the ACTIVE FX preset recalls CLEAN (slot 2) instead.
    ;   enc7  VOCALS   press = voc_on toggle; rotate = voc_level +-0.05, 0..2
    ;   enc11 BLOOPER  press = on at MAX density (gkBLP_Scat 1) / off; rotate = Scat
    ; Amp (NAM row/variant/bypass/gains) and master levels are excluded from
    ; preset recall (state_preset_exclude_*), so the amp is its own layer.
    ; Holds (CC116 capture / CC117 delete) act on the FX slots only; ignored on 1, 7, 11.
    ; Everything else on ch7 is dropped by the gate below. state_persist.orc
    ; instr 97 consumes gkPresetOp/gkPresetSlot.
    if (kSt >= 176 && kSt < 192 && kCh == 7) then
      if (kCC >= 102 && kCC <= 113 && kVal == 127) then
        kDuoN = kCC - 101
        kDuoFxOk = (kDuoN == 2 || kDuoN == 4 || kDuoN == 6 || kDuoN == 8 || kDuoN == 9 || kDuoN == 10 || kDuoN == 12) ? 1 : 0
        ; 2026-11-02 EMGOR: 1, 3, 5 = NAM-ONLY presets (amp model + in/out gain;
        ; recall changes nothing else). gkPresetOp 5 = NAM recall (state_persist).
        ; TOGGLE: press = that NAM preset on; press the active one again = amp off.
        if (kDuoN == 1 || kDuoN == 3 || kDuoN == 5) then
          if (gkDuoNam == kDuoN && gkNAMBypass < 0.5) then
            gkNAMBypass = 1
            gkDuoNam    = 0
          else
            gkPresetOp   = 5
            gkPresetSlot = kDuoN
            gkDuoNam     = kDuoN
          endif
        elseif (kDuoN == 7) then
          kDuoVon chnget "voc_gate"
          chnset (kDuoVon >= 0.5 ? 0 : 1), "voc_gate"   ; x0/x1 pre-gain, not the FX switch
        elseif (kDuoN == 11) then
          if (gkBLP_bypass >= 0.5) then
            gkBLP_bypass = 0
            gkBLP_Scat   = 1
            gkBLP_expr   = 0.5     ; pedal neutral (see toggle_slot0.orc BLOOPER note)
          else
            gkBLP_bypass = 1
          endif
        elseif (kDuoFxOk == 1) then
          gkPresetOp = 2
          if (gkDuoFx == kDuoN) then
            gkPresetOp   = 6       ; radio toggle off: every GUITAR effect off (2026-11-02)
            gkDuoFx      = 0
          else
            gkPresetSlot = kDuoN
            gkDuoFx      = kDuoN
          endif
        endif
      elseif (kCC == 116 || kCC == 117) then
        if (kVal == 2 || kVal == 4 || kVal == 6 || kVal == 8 || kVal == 9 || kVal == 10 || kVal == 12) then
          gkPresetOp   = (kCC == 116 ? 1 : 3)
          gkPresetSlot = kVal
        elseif (kVal == 1 || kVal == 3 || kVal == 5) then
          gkPresetOp   = (kCC == 116 ? 4 : 3)   ; 4 = NAM-only capture
          gkPresetSlot = kVal
        endif
      ; (enc 1 rotation removed 2026-11-02, EMGOR: NAM is press-toggle + save only)
      elseif (kCC - 19 == 7 && kVal != 64) then
        kDuoVl chnget "voc_level"
        chnset limit(kDuoVl + (kVal > 64 ? 0.05 : -0.05), 0, 2), "voc_level"
      elseif (kCC - 19 == 11 && kVal != 64) then
        gkBLP_Scat = limit(gkBLP_Scat + (kVal > 64 ? 0.05 : -0.05), 0, 1)
      endif
    endif

  ; The `&& kCh != 7` below makes the engine IGNORE every OTHER ch7 message:
  ; the DUODECIMUS presets are handled just above; expression was rewritten to
  ; ch1; and nothing else on ch7 should reach the normal dispatch. This also
  ; stops ch7 button CCs from colliding with channel-agnostic engine handlers
  ; (e.g. the poly-delay CC110/111 below, which don't filter channel).
  if (kMoreCtl == 1 && kSt >= 176 && kSt < 192 && kCh != 7 && kHrDrop == 0) then

#include "handler/handler_remap.orc"

    kNorm = kVal / 127.0

    ; Behind #ifdef MIDIDBG since 2026-08-10: one blocking write() to
    ; journald from the SCHED_FIFO audio thread PER incoming CC. A single
    ; knob sweep or expression-pedal move is a burst of hundreds of CCs,
    ; i.e. a burst of RT-thread syscalls exactly while the player is
    ; interacting. Re-enable with --omacro:MIDIDBG=1.
#ifdef MIDIDBG
    printks "MIDI CC: ch=%d cc=%d val=%d\n", 0, kCh, kCC, kVal
#endif

    ; MASTER CLOCK BPM — ch16 CC119 (2026-10-01: migrated off the dead ctrl7
    ; read; see the "CLOCK CONTROLS: EVENT-DISPATCH, NOT ctrl7" note near the
    ; top of this file and the comment above instr 99 for the full story).
    ; Decode EXACTLY as demiurge-clock.cpp encodes it: bpm = 40 + val*(360/127)
    ; (span 360, range 40..400 — NOT the 260 span CC104 uses below; mixing
    ; these up silently detunes the whole rig). Only ever written on a real
    ; incoming event, so gkGLITCH_bpmExternal sits at globals.orc's init value
    ; until the first genuine CC119 arrives — never forced to the 40 BPM
    ; floor. gkGLITCH_extBpmEdge (cleared once per k-block above, before the
    ; event loop) pulses high for this k-block only when the decoded value
    ; actually differs from the last one dispatched — the same one-shot
    ; "a NEW tempo just landed" contract chase_bliss_clock.orc already expects
    ; from the old ctrl7 + changed() pairing.
    kNowT times
    if (kCh == 16 && kCC == 119 && (kNowT - gkMidiH_wireT) > 3.0) then
      ; legacy 7-bit tempo -- ignored while a fresh 14-bit pair is on the bus (see CC120/121 below)
      kNewMasterBpm = 40 + kVal * (360.0 / 127.0)
      if (kNewMasterBpm != gkMidiH_prevMasterBpm) then
        gkGLITCH_extBpmEdge   = 1
        gkMidiH_prevMasterBpm = kNewMasterBpm
      endif
      gkGLITCH_bpmExternal = kNewMasterBpm
    endif

    ; TEMPO AS A NUMBER -- ch16 CC120 (MSB) then CC121 (LSB), sent back to back by
    ; the Pi clock daemon and by PMOR. Same one-shot edge contract as CC119 above.
    if (kCh == 16 && kCC == 120) then
      gkMidiH_wireMsb     = kVal
      gkMidiH_wireHaveMsb = 1
    endif
    if (kCh == 16 && kCC == 121 && gkMidiH_wireHaveMsb == 1 && (kNowT - gkMidiH_oscBpmT) > 3.0) then
      kNewMasterBpm = 40 + (gkMidiH_wireMsb * 128 + kVal) * (360.0 / 16383.0)
      gkMidiH_wireT = kNowT
      if (abs(kNewMasterBpm - gkMidiH_prevMasterBpm) > 0.001) then
        gkGLITCH_extBpmEdge   = 1
        gkMidiH_prevMasterBpm = kNewMasterBpm
      endif
      gkGLITCH_bpmExternal = kNewMasterBpm
    endif

    ; EXTERNAL-CLOCK-SEEN ONE-WAY LATCH — ch16 CC114 (2026-10-01: migrated
    ; off the dead ctrl7 read; see the long HISTORY block above instr 99 for
    ; why CC119 edges and CC116 role were both tried and rejected before
    ; CC114 — the tempo-source CLASS — was settled on). Threshold convention
    ; >=64 = true, same as CC117 transport; demiurge-clock.cpp only ever
    ; sends CC114 as 0 or 127. Never un-latches once set — a master that
    ; later goes quiet must not un-arm the looper's clocked-capture behaviour.
    if (kCh == 16 && kCC == 114 && kVal >= 64) then
      gkGLITCH_extBpmSeen = 1
    endif

    ; UI SYNC REQUEST — ch16 CC102 press (companion_input.EngineOut, sent by
    ; the touch UI at startup). Answer: pulse gkOscRefresh so every OSC
    ; feedback macro rebroadcasts its current value this/next cycle. ch16 is
    ; the utility channel (touch CC80-95, clock CC114/CC119); CC102 was free.
    if (kCh == 16 && kCC == 102 && kVal == 127) then
      gkOscRefresh = 1
      gkOscSweepReq = 1
    endif

    ; UTILITY page absolute params (2026-09-01), ch16 (utility channel), CC
    ; that was free: 103 = master-out volume (0..127 -> 0..1.5, unity ~85).
    ; Rec is UI-side only.
    if (kCh == 16 && kCC == 103) then
      gkMasterOut = kVal * (1.5/127)
    endif

    ; UTILITY page manual master BPM — ch16 CC104 (2026-10-01: migrated off
    ; the dead ctrl7 read and its initc7 pre-seed; see the note near the top
    ; of this file). Decode: bpm = 40 + val*(260/127) — span 260, matching
    ; neptrPhase4_UI.py's slider (MIN=40, MAX=300), which is DIFFERENT from
    ; CC119's 360 span above; do not swap them. Only written on a real
    ; incoming event, so gkManualBpm stays at globals.orc's documented
    ; default (120, globals.orc:356) until a genuine CC104 arrives.
    if (kCh == 16 && kCC == 104) then
      gkManualBpm = 40 + kVal * (260.0 / 127.0)
    endif

    ; VOCAL page (menu 21, 2026-10-22) absolute params, ch16 CC40..51 (the
    ; block 30..69 is read by nothing else; ch16 only, so no collision with the
    ; channel-agnostic dispatch numbers). 0..127 -> the voc_* chn_k channels of
    ; effects/vocal_chain.orc. MIXER (8ch) builds only: chnset on an unused
    ; channel is harmless elsewhere. Ranges here are the performance ranges;
    ; neptrPhase4_UI.py VOC_PARAMS mirrors this table exactly.
    if (kCh == 16 && kCC >= 40 && kCC <= 51) then
      kVocN = kVal / 127
      if (kCC == 40) then
        chnset (kVal >= 64 ? 1 : 0), "voc_on"
      elseif (kCC == 41) then
        chnset 40 + kVocN * 260, "voc_hpf_hz"
      elseif (kCC == 42) then
        chnset -12 + kVocN * 24, "voc_mid_db"
      elseif (kCC == 43) then
        chnset 300 * pow(5000 / 300, kVocN), "voc_mid_hz"
      elseif (kCC == 44) then
        chnset 0.3 + kVocN * 3.7, "voc_mid_q"
      elseif (kCC == 45) then
        chnset 50 + kVocN * 1450, "voc_dly_ms"
      elseif (kCC == 46) then
        chnset kVocN * 0.9, "voc_dly_fb"
      elseif (kCC == 47) then
        chnset kVocN, "voc_dly_wet"
      elseif (kCC == 48) then
        chnset kVocN * 0.94, "voc_rev_fb"
      elseif (kCC == 49) then
        chnset 1000 * pow(16, kVocN), "voc_rev_cut"
      elseif (kCC == 50) then
        chnset kVocN, "voc_rev_wet"
      elseif (kCC == 51) then
        chnset kVocN * 4, "voc_level"
      endif
    endif
    ; VOCAL mic PREAMP (2026-10-24), ch16 CC53 (free): 0..127 -> 0..40 dB.
    if (kCh == 16 && kCC == 54) then
      chnset (kVal >= 64 ? 1 : 0), "lp_relock"     ; loop re-lock to PMOR beat (see /clk/beat)
    endif
    if (kCh == 16 && kCC == 55) then
      chnset (kVal >= 64 ? 1 : 0), "voc_gate"     ; VOCAL GATE x0/x1 (web VOCAL toggle)
    endif
    if (kCh == 16 && kCC == 53) then
      chnset (kVal / 127) * 40, "voc_pre_db"
    endif

    ; NAM quality (UTILITY page, 2026-10-23), ch16 CC52: 127 = MICRO, 0 = FULL.
    ; -> chn "nam_size" (globals.orc) -> NAMProcess kSize (loader-thread reload).
    if (kCh == 16 && kCC == 52) then
      chnset (kVal >= 64 ? 1 : 0), "nam_size"
    endif

    ; PRESET SYSTEM (2026-09-09), ch16 UI CCs — the touchscreen PRESETS page
    ; drives the SAME in-engine bank as the DUODECIMUS foot, with the SLOT in the
    ; value (1..17; onboard menu-0 rows map to 13..17). 105 = capture, 106 =
    ; recall, 107 = delete. state_persist.orc instr 97 consumes gkPresetOp.
    if (kCh == 16 && kCC == 105 && kVal >= 1) then
      gkPresetOp   = 1
      gkPresetSlot = kVal
    endif
    if (kCh == 16 && kCC == 106 && kVal >= 1) then
      gkPresetOp   = 2
      gkPresetSlot = kVal
    endif
    if (kCh == 16 && kCC == 107 && kVal >= 1) then
      gkPresetOp   = 3
      gkPresetSlot = kVal
    endif

    ; POLY DLY v5 touch-only absolute controls (2026-10-22) — the six
    ; encoders are taken by Num/Den L/R + Attack/Decay, so Repeats and Interval
    ; ride dedicated ch1 CCs: 110 = Repeats (1..8), 111 = Interval (-12..+12).
    ; Applied regardless of menu (harmless when POLY DLY is bypassed).
    if (kCC == 110) then
      gkMvdReps = 1 + int(kVal * 7 / 127 + 0.5)
    endif
    if (kCC == 111) then
      gkMvdIntv = int(kVal * 24 / 127 + 0.5) - 12
    endif

#include "handler/handler_looper.orc"

#include "handler/handler_menus.orc"

    endif ; end CC dispatch

  endif ; end control-event dispatch

    ; Event consumed. Clearing kSt makes the next pass pull from OSC.
    kSt = 0

  od ; ---------------- end CONTROL EVENT LOOP ----------------

  ; ---------------- ABSOLUTE PARAMETER WRITES ----------------
  ; /p <id> <value 0..1> — set an expression destination directly, without the
  ; pedal and without the encoder accumulator. `id` is a toggle id: the same
  ; number gkLastToggle29 holds, dispatched by the generated ExprSet UDO, so
  ; every one of the 35 pedal destinations is reachable at full float
  ; resolution by anything that can send a UDP packet — a LiDAR, the gyro, a
  ; joystick axis, a phone fader.
  ;
  ; This is what makes the pedal non-special. It is now one source among many
  ; pointing at a shared table, rather than the only road to those parameters.
  ;
  ; Drained in its own loop for the same reason as /cc: a sensor stream must
  ; not back up at one message per k-block.
  ;
  ; NOTE: no slew argument yet. Smoothing currently happens upstream, in the
  ; router's conditioning stage. Per-destination portk inside the registry
  ; needs a k-var per destination and is a deliberate later step.
  kPid   init 0
  kPval  init 0
  kPgot  init 0
  kPmore init 1
  kPmore = 1

  while (kPmore == 1) do
    kPgot OSClisten giCtlOSC, "/p", "if", kPid, kPval
    if (kPgot == 1) then
      ExprSet kPid, kPval
    else
      kPmore = 0
    endif
  od

  ; SOUNDMAGIC rack control (spectral/CONTRACT.md). /st fx on : toggle;
  ; /sp fx p val : absolute param write, clamped to the registry min/max.
  ; Drained like /cc and /p. fx 23 (spare) p 0 = rack default FFT index.
  kStFx  init 0
  kStOn  init 0
  kStGot init 0
  kStMore init 1
  kStMore = 1
  while (kStMore == 1) do
    kStGot OSClisten giCtlOSC, "/st", "ii", kStFx, kStOn
    if (kStGot == 1) then
      if (kStFx >= 0 && kStFx <= 22) then
        tablew (kStOn > 0 ? 1 : 0), kStFx, giSMon
        if (kStOn > 0) then
          gkLastToggle = 75 + kStFx
          gkLastToggle29 = 75 + kStFx
        endif
      endif
    else
      kStMore = 0
    endif
  od

  ; /clk/bpm f : PRECISE external tempo (2026-10-24). pmor-clock-relay measures
  ; PMOR's real 0xF8 rate over a long window and converts it into this engine's
  ; audio-clock time base (the HAT clock runs ~-57 ppm vs system time). The 14-bit
  ; CC120/121 number is 0.022 BPM per step (~240 ppm at 124 BPM: a loop captured
  ; against it drifts ~14 ms/min vs PMOR). While this is fresh (<3 s) CC120/121
  ; and CC119 are ignored. Same one-shot edge contract as CC121.
  kCbOscBpm  init 0
  kCbOscGot  init 0
  kCbOscMore init 1
  kCbOscMore = 1
  while (kCbOscMore == 1) do
    kCbOscGot OSClisten giCtlOSC, "/clk/bpm", "f", kCbOscBpm
    if (kCbOscGot == 1) then
      if (kCbOscBpm >= 40 && kCbOscBpm <= 400) then
        gkMidiH_oscBpmT = times:k()
        gkMidiH_wireT   = gkMidiH_oscBpmT
        if (abs(kCbOscBpm - gkMidiH_prevMasterBpm) > 0.001) then
          gkGLITCH_extBpmEdge   = 1
          gkMidiH_prevMasterBpm = kCbOscBpm
        endif
        gkGLITCH_bpmExternal = kCbOscBpm
        gkGLITCH_extBpmSeen  = 1
      endif
    else
      kCbOscMore = 0
    endif
  od
  ; external clock is LIVE only while /clk/bpm keeps arriving (see globals.orc
  ; gkClkExtLive). instr 99 runs before 100 (tempo) and 209 (loop capture).
  gkClkExtLive = ((times:k() - gkMidiH_oscBpmT) < $CLK_EXT_FRESH ? 1 : 0)

  ; ---- PMOR BEAT COUNTER gkLpB (2026-11-05) -----------------------------------
  ; Loops derive their read position from this (effects/looper.orc BEAT LOCK),
  ; so their phase follows PMOR's beat through any tempo change. gkLpB integrates
  ; the smoothed external tempo (0.05 s half-time: a tempo step becomes a fast
  ; varispeed ramp, not a click) and is pinned to PMOR's real beat by /clk/beat
  ; below (absolute beat index + fraction, 2026-11-06). Frozen while the clock is
  ; not live. lp_relock (CC54) switches the whole thing.
  kBlOn chnget "lp_relock"
  kBpmFresh = (gkClkExtLive == 1 && gkGLITCH_bpmExternal >= 40 ? 1 : 0)
  kBpmS init 0
  if (kBpmFresh == 1) then
    kBpmS = (kBpmS < 40 ? gkGLITCH_bpmExternal : kBpmS + (gkGLITCH_bpmExternal - kBpmS) * (1 - exp(-0.693147 * ksmps / sr / 0.05)))
    gkLpB += kBpmS / 60 * ksmps / sr
  else
    kBpmS = 0                     ; re-seed on the next live block
  endif
  gkLpBpmS  = (kBpmS > 0 ? kBpmS : gkLpBpmS)
  gkLpBLock = (kBpmFresh == 1 && kBlOn >= 0.5 && kBpmS > 0 ? 1 : 0)

  ; 2026-11-06: the anchor slew below is only the fallback (beat lock off); with
  ; gkLpBLock on, /clk/beat drives gkLpB's ABSOLUTE position (bar lock, see
  ; docs/LOOP_BAR_LOCK.md).
  ; ---- LOOP RE-LOCK to PMOR's beat (2026-10-24) -------------------------------
  ; pmor-clock-relay sends "/clk/beat" "if" (beat index since PMOR's 0xFA Start,
  ; samples elapsed since that beat in this engine's audio-clock time) once per
  ; beat, only while PMOR is master, playing and tempo-locked. Here: where did
  ; that beat land in the LOOPS' beat grid? phase = (beat sample - anchor of the
  ; lowest active channel) mod one beat, wrapped to +-half a beat. It is smoothed
  ; (EMA 0.3) and a single outlier (> 15 % of a beat off the estimate) is
  ; ignored unless 4 in a row agree (PMOR restarted). Then every k-block ALL
  ; channel anchors move together by at most LP_RELOCK_MAX samples (0.125/block =
  ; 0.1 % speed while correcting; the read is tablei-interpolated so the step is
  ; smooth), so relative alignment between loops never changes. Never corrects
  ; while any channel records / sync-closes / trims, or plays off 1x / tempo trim
  ; (those positions are not anchor-derived). Deadband 0.5 ms.
  ; OFF: ch16 CC54 < 64 (on >= 64). Default ON at boot. lp_relock_ofs_ms shifts
  ; the target (+ = loop later) for output-latency tuning.
  kRlD    init 0
  kRlB    init 0
  kRlGot  init 0
  kRlMore init 1
  kRlE    init 0
  kRlHave init 0
  kRlOut  init 0
  kRlLast init -1000
  kBpHave init 0
  kBpEst  init 0
  kBpOut  init 0
  kBpThen init 0
  kBpE    init 0
  kBpOfs  init 0
  kBpSnap init 0
  kBpInt  init 0
  kBpStep init 0
  kRlMore = 1
  while (kRlMore == 1) do
    kRlGot OSClisten giCtlOSC, "/clk/beat", "if", kRlB, kRlD
    if (kRlGot == 1) then
      kRlLast = times:k()
      ; ---- beat phase of gkLpB vs this PMOR beat (see gkLpB above) ----
      ; this block's gkLpB is the same instant as gkLpClock + ksmps below
      kBpOfs chnget "lp_relock_ofs_ms"
      kBpThen = gkLpB - gkLpBpmS / 60 * (kRlD + ksmps / 2 + kBpOfs * sr / 1000) / sr
      ; ABSOLUTE error (2026-11-06): gkLpB is PMOR's beat position since its
      ; bar-1 start (relay /clk/beat index is absolute), so the target for this
      ; message is exactly kRlB and the error keeps its integer part. A
      ; whole-beat mismatch means PMOR restarted / we booted mid-song.
      kBpE    = kBpThen - kRlB
      if (kBpHave == 0) then
        kBpEst = kBpE
        kBpHave = 1
        kBpOut = 0
        kBpSnap = 1                 ; first acquisition: snap integer AND fraction
      elseif (abs(kBpE - kBpEst) > 0.15) then
        kBpOut += 1
        if (kBpOut >= 4) then
          kBpEst = kBpE
          kBpOut = 0
        endif
      else
        kBpOut = 0
        kBpEst = kBpEst + 0.3 * (kBpE - kBpEst)
      endif
      kRlRef = -1
      kRlRef = (kRlRef < 0 && gkLpState5 >= 2 && gkLpState5 <= 4 ? 5 : kRlRef)
      kRlRef = (gkLpState4 >= 2 && gkLpState4 <= 4 ? 4 : kRlRef)
      kRlRef = (gkLpState3 >= 2 && gkLpState3 <= 4 ? 3 : kRlRef)
      kRlRef = (gkLpState2 >= 2 && gkLpState2 <= 4 ? 2 : kRlRef)
      kRlRef = (gkLpState1 >= 2 && gkLpState1 <= 4 ? 1 : kRlRef)
      kRlAnc = (kRlRef == 1 ? gkLpAnchor1 : kRlRef == 2 ? gkLpAnchor2 : kRlRef == 3 ? gkLpAnchor3 : kRlRef == 4 ? gkLpAnchor4 : gkLpAnchor5)
      kRlBs  = gkLpBeatSamples
      if (kRlRef > 0 && kRlBs > 1000) then
        kRlOfs chnget "lp_relock_ofs_ms"
        ; instr 99 runs before 209 bumps gkLpClock, so this block's looper clock is
        ; gkLpClock + ksmps; the message waited ~ksmps/2 on average for this cycle.
        kRlPos = gkLpClock + ksmps - (kRlD + ksmps / 2) - kRlOfs * sr / 1000
        kRlP   = kRlPos - kRlAnc
        kRlP   = kRlP - kRlBs * floor(kRlP / kRlBs)
        kRlP   = (kRlP >= kRlBs / 2 ? kRlP - kRlBs : kRlP)
        if (kRlHave == 0) then
          kRlE = kRlP
          kRlHave = 1
          kRlOut = 0
        elseif (abs(kRlP - kRlE) > 0.15 * kRlBs) then
          kRlOut += 1
          if (kRlOut >= 4) then
            kRlE = kRlP
            kRlOut = 0
          endif
        else
          kRlOut = 0
          kRlE = kRlE + 0.3 * (kRlP - kRlE)
        endif
      else
        kRlHave = 0
      endif
    else
      kRlMore = 0
    endif
  od
  ; apply: slew all anchors together
  kRlOn chnget "lp_relock"
  kRlBusy = (gkLpState1 == 1 || gkLpState1 >= 5 || gkLpState2 == 1 || gkLpState2 >= 5 || gkLpState3 == 1 || gkLpState3 >= 5 || gkLpState4 == 1 || gkLpState4 >= 5 || gkLpState5 == 1 || gkLpState5 >= 5) ? 1 : 0
  kRlOff1x = (abs(gkLpEffSpd1 - 1) > 0.0005 || abs(gkLpEffSpd2 - 1) > 0.0005 || abs(gkLpEffSpd3 - 1) > 0.0005 || abs(gkLpEffSpd4 - 1) > 0.0005 || abs(gkLpEffSpd5 - 1) > 0.0005) ? 1 : 0
  if (kRlBusy == 1 || times:k() - kRlLast > 3) then
    kRlHave = 0                     ; stale or recording: re-acquire from scratch
  endif
  ; retired while beat lock is active (gkLpBLock): gkLpB is the single phase mechanism
  if (kRlOn >= 0.5 && gkLpBLock == 0 && kRlHave == 1 && kRlBusy == 0 && kRlOff1x == 0 && abs(kRlE) > 0.0005 * sr) then
    kRlStep = limit(kRlE, -$LP_RELOCK_MAX, $LP_RELOCK_MAX)
    gkLpAnchor1 += kRlStep
    gkLpAnchor2 += kRlStep
    gkLpAnchor3 += kRlStep
    gkLpAnchor4 += kRlStep
    gkLpAnchor5 += kRlStep
    kRlE -= kRlStep
  endif
  ; apply the beat-phase correction to gkLpB. Idle loops: snap. Playing loops:
  ; proportional pull (0.4 s time constant) capped at 1.5 % rate offset so it is
  ; a short smooth varispeed, never a jump. Not while any channel records.
  kBpRec = (gkLpState1 == 1 || gkLpState1 >= 5 || gkLpState2 == 1 || gkLpState2 >= 5 || gkLpState3 == 1 || gkLpState3 >= 5 || gkLpState4 == 1 || gkLpState4 >= 5 || gkLpState5 == 1 || gkLpState5 >= 5) ? 1 : 0
  kBpIdle = ((gkLpState1 == 0 || gkLpState1 == 4) && (gkLpState2 == 0 || gkLpState2 == 4) && (gkLpState3 == 0 || gkLpState3 == 4) && (gkLpState4 == 0 || gkLpState4 == 4) && (gkLpState5 == 0 || gkLpState5 == 4)) ? 1 : 0
  if (gkLpBLock == 0 || times:k() - kRlLast > 3) then
    kBpHave = 0
  endif
  ; snap: first acquisition, or no loop playing (kBpIdle). Playing: a whole-beat
  ; error (accepted only after the 4-in-a-row outlier rule above) is snapped,
  ; the remaining fraction is pulled smoothly.
  if (kBpHave == 1 && gkLpBLock == 1 && kBpRec == 0 && (kBpSnap == 1 || abs(kBpEst) > 0.0005 * gkLpBpmS / 60)) then
    if (kBpSnap == 1 || kBpIdle == 1) then
      kBpStep = kBpEst
      kBpSnap = 0
    else
      kBpInt  = round(kBpEst)
      kBpStep = kBpInt + limit((kBpEst - kBpInt) * ksmps / sr / 0.4, -0.015 * gkLpBpmS / 60 * ksmps / sr, 0.015 * gkLpBpmS / 60 * ksmps / sr)
    endif
    gkLpB   -= kBpStep
    kBpEst  -= kBpStep
  endif
  gkLpRelockErr = kRlE              ; samples, for meters/debug
  kSpFx  init 0
  kSpP   init 0
  kSpV   init 0
  kSpGot init 0
  kSpMore init 1
  kSpMore = 1
  while (kSpMore == 1) do
    kSpGot OSClisten giCtlOSC, "/sp", "iif", kSpFx, kSpP, kSpV
    if (kSpGot == 1) then
      if (kSpFx >= 0 && kSpFx <= 22 && kSpP >= 0 && kSpP <= 31) then
        kSpIx = kSpFx * 32 + kSpP
        tablew limit(kSpV, tab:k(kSpIx, giSMPmin), tab:k(kSpIx, giSMPmax)), kSpIx, giSMP
      elseif (kSpFx == 23 && kSpP == 0) then
        tablew limit(round(kSpV), 0, 7), 736, giSMP
      endif
    else
      kSpMore = 0
    endif
  od

  ; /sh fx k idx : set hero slot k (0..6) of an effect to absolute param idx
  ; (-1 = none; else must lie inside that effect's 32-slot block).
  ; /shp fx idx : set the pedal (CC29) target of an effect (same idx rule).
  kShFx  init 0
  kShK   init 0
  kShIdx init 0
  kShGot init 0
  kShMore init 1
  kShMore = 1
  while (kShMore == 1) do
    kShGot OSClisten giCtlOSC, "/sh", "iii", kShFx, kShK, kShIdx
    if (kShGot == 1) then
      kShFxC = limit(kShFx, 0, 23)
      kShKC = limit(kShK, 0, 6)
      if (kShIdx == -1 || (kShIdx >= kShFxC * 32 && kShIdx <= kShFxC * 32 + 31)) then
        tablew kShIdx, kShFxC * 8 + kShKC, giSMhero
      endif
    else
      kShMore = 0
    endif
  od
  kShpFx  init 0
  kShpIdx init 0
  kShpGot init 0
  kShpMore init 1
  kShpMore = 1
  while (kShpMore == 1) do
    kShpGot OSClisten giCtlOSC, "/shp", "ii", kShpFx, kShpIdx
    if (kShpGot == 1) then
      kShpFxC = limit(kShpFx, 0, 23)
      if (kShpIdx == -1 || (kShpIdx >= kShpFxC * 32 && kShpIdx <= kShpFxC * 32 + 31)) then
        tablew kShpIdx, kShpFxC, giSMpedal
      endif
    else
      kShpMore = 0
    endif
  od

  ; USER PRESETS hold progress -> UI "/phold" "if" (slot, seconds held), 10 Hz
  ; while a slot button is down, then one (slot, -1) on release. A button that
  ; never sends its release is dropped after 15 s with no action.
  kPgTick metro 10
  kPgChg changed gkPgHeld
  kPgOut = 0
  kPgSecs init -1
  kPgLast init 1
  kPgNow times
  if (gkPgHeld > 0) then
    kPgLast = gkPgHeld
    kPgSecs = kPgNow - gkPgT0
    kPgOut = kPgTick
    if (kPgSecs > 15) then
      gkPgHeld = 0
    endif
  else
    kPgSecs = -1
  endif
  if (kPgChg == 1) then
    kPgOut = 1
  endif
  OSCsend kPgOut, "127.0.0.1", 8000, "/phold", "if", kPgLast, kPgSecs

  $LP_HOLD_WATCHER

#include "handler/handler_osc.orc"
  $LP_OSC_FEEDBACK

endin
