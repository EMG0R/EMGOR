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

  kSt, kCh, kCC, kVal midiin
  kMoreCtl = 1

  while (kMoreCtl == 1) do

    ; kSt == 0 means "no message in hand" — either midiin had nothing this
    ; block, or the previous pass already consumed its event. Pull the next
    ; OSC event; when that runs dry the loop ends.
    kFromOsc = 0
    if (kSt == 0) then
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
    if (kFromOsc == 0 && kSt >= 176 && kSt < 192 && kCh == 1 && kCC == 29) then
      kCC = 30
    endif

    ; DUODECIMUS (oFOOTS v2) EXPRESSION FUNNEL (2026-09-08). The foot
    ; controller sends EVERYTHING on ch7. Its Jack-A treadle is ch7 CC14 (Jack-B
    ; expression, if a jack is configured that way, is ch7 CC15). Rewrite either
    ; into the ONBOARD expression message — ch1 CC29 — HERE, before the dispatch
    ; gate, so the DUODECIMUS pedal drives the EXACT same expression with
    ; identical scaling AND the same soft-takeover (stale-pedal disarm until a
    ; physical move >0.03) as the onboard pedal via expr_cc29.orc. One funnel at
    ; the source, not a parallel copy.
    ; PEDAL_VIA_POOL (2026-10-07, docs/NEPTR_POOL_BENCH.md): when launched with
    ; --omacro:PEDAL_VIA_POOL=1 the funnel is OFF here because demiurge-io
    ; (--midi --midi-neptr) performs the SAME rewrite at the source and delivers the
    ; pedal as `/cc 1 29` over OSC (kFromOsc = 1, so exempt from the demotion above).
    ; One mechanism for one job: never both. Default (macro absent) = today, unchanged.
#ifndef PEDAL_VIA_POOL
    if (kSt >= 176 && kSt < 192 && kCh == 7 && (kCC == 14 || kCC == 15)) then
      kCh = 1
      kCC = 29
    endif
#end

    ; DUODECIMUS PRESET CONTROL (2026-09-09), ch7. The foot controller drives
    ; the in-engine preset bank directly (no daemon): encoder tap = recall,
    ; 5s hold = capture, 8s hold = delete. Slots 1..12 are the 12 foot encoders.
    ;   CC102..113 press (127) -> RECALL slot (cc-101); ignore the 0 release
    ;   CC116 value=N          -> CAPTURE slot N   (firmware one-shot at 5s)
    ;   CC117 value=N          -> DELETE  slot N   (firmware one-shot at 8s)
    ; These set the single gkPresetOp/gkPresetSlot command that state_persist.orc
    ; instr 97 consumes. Handled HERE, before the gate below drops all other ch7.
    if (kSt >= 176 && kSt < 192 && kCh == 7) then
      if (kCC >= 102 && kCC <= 113 && kVal == 127) then
        gkPresetOp   = 2
        gkPresetSlot = kCC - 101
      elseif (kCC == 116) then
        gkPresetOp   = 1
        gkPresetSlot = kVal
      elseif (kCC == 117) then
        gkPresetOp   = 3
        gkPresetSlot = kVal
      endif
    endif

  ; The `&& kCh != 7` below makes the engine IGNORE every OTHER ch7 message:
  ; the DUODECIMUS presets are handled just above; expression was rewritten to
  ; ch1; and nothing else on ch7 should reach the normal dispatch. This also
  ; stops ch7 button CCs from colliding with channel-agnostic engine handlers
  ; (e.g. the poly-delay CC110/111 below, which don't filter channel).
  if (kMoreCtl == 1 && kSt >= 176 && kSt < 192 && kCh != 7) then

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
    if (kCh == 16 && kCC == 121 && gkMidiH_wireHaveMsb == 1) then
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

  $LP_HOLD_WATCHER

#include "handler/handler_osc.orc"
  $LP_OSC_FEEDBACK

endin
