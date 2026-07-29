------<CsoundSynthesizer>
<CsOptions>
;COMPUTER
;-odac
 ;PIs
-odac:hw:2,0
 -iadc
 -b128
 -B256
-Ma
 -+rtmidi=alsa
--daemon
</CsOptions>
<CsInstruments>

; NEPTR Phase 4 - Modular Effects Processor
; Menu-based MIDI control with Chuck CC assignments
; All DSP from Phase 2, reorganized into separate files

; SURF bank (default). Switch banks via demiurge-set:
;   demiurge-set neptr      → surf presets (this file)
;   demiurge-set neptr_bass → bass presets (neptrPhase4_bass.csd)
#ifndef PRESET_BANK
#define PRESET_BANK #0#
#endif

; Settings + looper-audio persistence (2026-07-26): state survives restarts.
; Engine: state_persist.orc (generated) + looper_persist.orc. Dir default
; /home/gorpi7/demiurge/state (STATE_DIR macro to override).
#define STATE_PERSIST ##

; Drum-loop sample folder for looper ch1 sample mode (effects/drumloops.orc)
#ifndef DRUMLOOP_DIR
#define DRUMLOOP_DIR #/home/gorpi7/demiurge/samples/drumloops#
#endif

#include "globals.orc"
#include "effects/preamp.orc"
#include "effects/attack.orc"
#include "effects/h90.orc"
#include "effects/pitchfork.orc"
#include "effects/pitchshifter.orc"
#include "effects/gps.orc"
#include "effects/phaser.orc"
#include "effects/flanger.orc"
#include "effects/chorus.orc"
#include "effects/vibrato.orc"
#include "effects/leslie.orc"
#include "effects/overdrive.orc"
#include "effects/marshall.orc"
#include "effects/dualhigain.orc"
#include "effects/ratmz.orc"
#include "effects/bigmuff.orc"
#include "effects/bass_preamp.orc"
#include "effects/bass_dist.orc"
#include "effects/tremolo.orc"
#include "effects/forms.orc"
#include "effects/formant.orc"
#include "effects/bincrush.orc"
#include "effects/bincrusher.orc"
#include "effects/lossy.orc"
#include "effects/tape.orc"
#include "effects/ringmod.orc"
#include "effects/automoog.orc"
#include "effects/wah.orc"
#include "effects/nam.orc"
#include "effects/spectraldelay.orc"
#include "effects/habit.orc"
#include "effects/mood.orc"
#include "effects/granular.orc"
#include "effects/delay.orc"
#include "effects/reverb.orc"
#include "effects/stutter.orc"
#include "effects/looper.orc"
#include "effects/drumloops.orc"
#include "looper_persist.orc"
#include "chase_bliss_clock.orc"
#include "onset_pick.orc"
#include "effects/onward.orc"
#include "effects/impulse_synth.orc"
#include "effects/resonator.orc"
#include "effects/ir_reverb.orc"
#include "effects/octaver.orc"
#include "effects/envfilter.orc"
#include "effects/blooper.orc"
#include "effects/mood2.orc"
; 2026-07-13 effect batch (toggles 57-58, 60-65; instrs 180-181, 183-188).
; tracksynth needs onset_pick.orc (included above) before it.
#include "effects/compressor.orc"
#include "effects/toneeq.orc"
#include "effects/chainfb.orc"
#include "effects/tracksynth.orc"
#include "effects/pllfuzz.orc"
#include "effects/feedbacker.orc"
#include "effects/diffusion.orc"
; 2026-07-26 delay/verb expansion batch (toggles 67-73; instrs 188, 190-191,
; 193-195, 201; docs/specs/2026-07-26-delay-verb-expansion.md). BINSCATTER
; (201) lives inside spectraldelay.orc (already included above, alongside
; SPECDLY 200). movedelay_modes.orc (mode data tables) is included BEFORE
; movedelay.orc per its own header instruction. grainverb.orc (188) was
; benched/written in the 2026-07-13 batch but never included/wired — fixed
; here.
#include "effects/movedelay_modes.orc"
#include "effects/movedelay.orc"
#include "effects/tapedelay.orc"
#include "effects/nebula.orc"
#include "effects/shimmerverb.orc"
#include "effects/freezeverb.orc"
#include "effects/grainverb.orc"
#include "signal_chain.orc"
#include "midi_handler.orc"
#include "state_persist.orc"

</CsInstruments>
<CsScore>
i99  0 z
i1   0 z
i209 0 z
i210 0 z
i211 0 z
i212 0 z
i213 0 z
i214 0 z
i216 0 z
; 2026-07-26 delay/verb expansion: TAPE DELAY (190), NEBULA (193), SHIMMER
; (194), FREEZE (195) are ALWAYSON (self-contained internal bypass
; crossfade/gate — kByp/kGate in their own .orc files — so state/tails stay
; live across a bypass toggle instead of being killed by turnoff2). MOVE
; DELAY (191), BINSCATTER (201) and GRAINVERB (188) have no internal bypass
; gating and are started/stopped normally via $FX_BYPASS in signal_chain.orc
; (same convention as DELAY 62 / REVERB 70 / SPECDLY 200) — no score line.
i190 0 z
i193 0 z
i194 0 z
i195 0 z
</CsScore>
</CsoundSynthesizer>


















<bsbPanel>
 <label>Widgets</label>
 <objectName/>
 <x>100</x>
 <y>100</y>
 <width>320</width>
 <height>240</height>
 <visible>true</visible>
 <uuid/>
 <bgcolor mode="background">
  <r>240</r>
  <g>240</g>
  <b>240</b>
 </bgcolor>
</bsbPanel>
<bsbPresets>
</bsbPresets>
