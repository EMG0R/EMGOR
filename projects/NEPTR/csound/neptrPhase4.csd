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

; Settings persistence (2026-07-26): knob/preset state survives restarts.
; Engine: state_persist.orc (generated), writes neptr_settings.ftab. Dir
; default /home/gorpi7/demiurge/state (STATE_DIR macro to override).
;
; 2026-08-12: looper AUDIO persistence (looper_persist.orc) used to ride on
; this same macro and is now OFF — it is guarded on LOOPER_PERSIST_AUDIO,
; which is deliberately not defined here. Restoring loop buffers also
; restored gkLpMasterLen, so the first loop recorded after any restart was
; snapped to a previous session's bar length instead of defining a new one;
; the looper could never be handed a clean slate. See the header of
; looper_persist.orc for the full reasoning. The looper now always boots
; empty with no length and no grid. Settings persistence is unaffected.
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
; SPECDLY 200). movedelay.orc (POLY DLY v5, instr 191) is self-contained
; (movedelay_modes.orc was removed 2026-10-22). grainverb.orc (188) was
; benched/written in the 2026-07-13 batch but never included/wired — fixed
; here.
#include "effects/movedelay.orc"
#include "effects/tapedelay.orc"
#include "effects/nebula.orc"
#include "effects/shimmerverb.orc"
#include "effects/freezeverb.orc"
#include "effects/grainverb.orc"
; hyperGuitar control link (menu 20), first half: globals + the
; $HG_TOGGLE/$HG_ROTATE macros. MUST precede midi_handler.orc, which includes
; the menu-20 dispatch branches that expand them.
#include "hyperguitar.orc"
; SoundMagic spectral rack (Michael Norris re-implementation; spectral/CONTRACT.md).
; Order matters: params -> core UDOs -> kernels (each defines SM_HAVE_<CODE>) ->
; rack. SM_Run in sm_rack.orc only calls kernels whose SM_HAVE_ flag exists, so
; a kernel is added by dropping sm_<name>.orc in spectral/ and listing it HERE
; (before sm_rack.orc). Must precede midi_handler.orc (SM_RotStep/SM_PedalSet).
#include "spectral/sm_params.orc"
#include "spectral/sm_core.orc"
#include "spectral/sm_freezing.orc"
#include "spectral/sm_blurring.orc"
#include "spectral/sm_averaging.orc"
#include "spectral/sm_binshift.orc"
#include "spectral/sm_pitchshift.orc"
#include "spectral/sm_shuffle.orc"
#include "spectral/sm_dronemaker.orc"
#include "spectral/sm_gateandhold.orc"
#include "spectral/sm_weave.orc"
#include "spectral/sm_shimmer.orc"
#include "spectral/sm_pulsing.orc"
#include "spectral/sm_harmonizer.orc"
#include "spectral/sm_stretch.orc"
#include "spectral/sm_tracing.orc"
#include "spectral/sm_filterbank.orc"
#include "spectral/sm_glidingfilters.orc"
#include "spectral/sm_granulation.orc"
#include "spectral/sm_emergence.orc"
#include "spectral/sm_partialglide.orc"
#include "spectral/sm_chorus.orc"
#include "spectral/sm_grainstreamer.orc"
#include "spectral/sm_ideefixer.orc"
#include "spectral/sm_superfilterbank.orc"
#include "spectral/sm_rack.orc"
; Optional 8x8 mixer (--omacro:MIXER=1, set by demiurge-run-csound from live.conf
; [mixer]); must precede signal_chain.orc, whose instr 1 calls MixIn/MixOut.
#ifdef MIXER
#include "mixer.orc"
; Vocal chain (inputs 5,6 -> outs 5,6, always on). UDO used by instr 1, so
; it must precede signal_chain.orc; 8ch/MIXER builds only.
#include "effects/vocal_chain.orc"
#end
; MIXER menu (22): chn_k state + dispatch macros (all builds: the dispatch
; branches expand them) and, under MIXER, the MixerPage audio stage. After
; vocal_chain.orc (it calls VocalChain), before signal_chain.orc and midi_handler.orc.
#include "mixer_page.orc"
#include "signal_chain.orc"
; Expression-destination registry — GENERATED by tools/gen_expr_registry.py
; from handler/expr_targets.src.orc. Defines the UDO ExprSet(kId, kNorm), so
; it must be compiled BEFORE midi_handler.orc, which calls it.
#include "handler/expr_registry.orc"
#include "midi_handler.orc"
; hyperGuitar control link (menu 20), second half. AFTER midi_handler.orc
; because instr 98 listens on giCtlOSC, which that file creates. The macros and
; globals half is included earlier, BEFORE midi_handler.orc — see there.
#include "hyperguitar_link.orc"
; Live menu-cycle reload (OSC /mc, /mc_n); needs giCtlOSC, so after midi_handler.
#include "handler/menu_reload.orc"
#include "state_persist.orc"
; SoundMagic param/on-state persistence (after state_persist.orc: shares STATE_DIR)
#include "spectral/sm_persist.orc"
; Cycle-time spike probe; LAST include so its named instr runs last each k-cycle.
#include "spike_probe.orc"

</CsInstruments>
<CsScore>
; 190 (tape delay), 193 (digiverb), 194 (shimmer) are NO LONGER started
; here. They were ALWAYSON with only an internal kGate multiplying in/out,
; so their DSP ran at full cost while bypassed (measured 2026-08-10: csound
; pinned at 73-100%% of a core regardless of what was enabled, xruns
; DSP-bound). $FX_BYPASS_TAIL in signal_chain.orc now starts them on
; enable and turnoff2s them after their tail rings out. 195 (freeze) and
; 209-214/216 (loopers, drumloops) stay ALWAYSON on purpose: the frozen bed
; and loop transports must survive bypass.
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
i195 0 z
; SoundMagic rack control (249) + persistence (250); groups 241-248 are started by 249
i249 0 z
i250 0 z
#ifdef MIXER
i282 0 1
#end
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
