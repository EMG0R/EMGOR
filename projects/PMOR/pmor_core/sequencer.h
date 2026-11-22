// sequencer.h — three lane-sets (drums, synth, sampler), each with its own bars/steps,
// MANUAL / PLAY-IN / GENERATIVE modes, live recording, and the grow/shrink rules.
// Data is bitmasks: lane = pad. For the synth a "lane" is a scale degree (pad→note).
#pragma once
#include <stdint.h>
#include "core_config.h"
#include "param.h"

enum Engine : uint8_t { ENG_DRUMS = 0, ENG_SYNTH, ENG_SMP, ENG_COUNT };
enum SeqMode : uint8_t { SEQ_MANUAL = 0, SEQ_PLAY, SEQ_GEN, SEQ_MODE_COUNT };

struct LaneSet {
    uint16_t step[MAXSTEPS];      // bit k = pad/lane k fires on this 16th
    uint8_t  bars;                // 1..16
    uint8_t  steps_per_bar;       // 1..16
    uint8_t  mode;                // SeqMode
    // generative controls (Params live here so the editor can point at them)
    float    probability;         // 0..1 master generative amount
    float    density;             // 0..1 how many events per bar in GEN
    // synth-only
    float    polyphony;           // 1..8 (drums/sampler ignore)
    float    oct_amount;          // 0..5 octaves (continuous)
    float    oct_prob;            // 0..1
    bool     arp;                 // plain arpeggiator on held/sequenced notes
    float    arp_rate;            // in 16ths: 1,2,4 (quantized)
    bool     recording;           // live record armed (PLAY mode + record gesture)
    // additive (sequencer.cpp), report per AGENT_RULES: float shadows so the
    // universal Param editor (float* only) can point at fields that are
    // otherwise uint8_t/bool. Kept in sync by seqSetBars/seqSetStepsPerBar
    // and seqFire (arp_on -> arp each step).
    float    bars_f;              // mirrors `bars`
    float    steps_f;             // mirrors `steps_per_bar`
    float    arp_on;              // mirrors `arp`, 0.f/1.f, PF_ENUM OFF/ON
    // additive (sequencer.cpp), report per AGENT_RULES: float shadow of `mode`
    // (SeqMode) so the universal Param editor can expose a MODE selector on
    // the SEQ card, same scheme as arp_on. Synced -> mode each seqFire().
    float    mode_f;
    float    rec_on;              // PLAY mode: record pad hits into the sequence (0/1), default OFF
    float    hand_f;              // RETIRED 2026-10-05 (the HANDSEQ list item is gone: the step editor is entered with joystick
                                  // UP/DOWN on the SEQ card, ui/nav.cpp). Field kept ONLY so sizeof(LaneSet) -- and so the patch
                                  // blob layout (store/patch.cpp memcpy of seq.lane) -- does not change. Unused.
};

struct SeqState {
    LaneSet lane[ENG_COUNT];
};
extern SeqState seq;

// ---- lifecycle
void seqInit();
void seqReseed(uint32_t seed);   // generative dice (0 = original fixed seeds)
static inline uint32_t seqLoopLen(const LaneSet& l) { return (uint32_t)l.bars * l.steps_per_bar; }
uint32_t seqLongestLoop();                      // max over engines → clockSetLoopLen()

// ---- editing (CONTROL plane)
void seqSetBars(Engine e, uint8_t bars);        // grow = duplicate existing loop to fill; shrink keeps hidden data
void seqSetStepsPerBar(Engine e, uint8_t spb);
void seqToggle(Engine e, uint32_t step, uint8_t lane);
void seqClear(Engine e);
void seqClearLane(Engine e, uint8_t lane);   // erase one pad's whole sequence
// live play-in: quantizes `now` to the nearest step of that engine's loop and sets the bit
void seqRecordHit(Engine e, uint8_t lane, uint32_t now_sample);

// ---- "wait for first hit" recording (loop start = the player's first hit)
// REC armed on an EMPTY lane records nothing until the first pad hit. That hit becomes step 0 of the lane's loop:
//  * own clock (standalone / PMOR master) and no other lane has content: the transport restarts AT the hit (clockReanchorStamped, hit =
//    exact downbeat; the MIDI clock out follows through the grid-beat re-announce, exactly like a tap) and the offset stays 0;
//  * a followed clock (Live / STUDIO / AUTO-follow) cannot be moved, or other lanes are playing and must not be disturbed: the hit is snapped
//    to the nearest 16th of the existing grid and the lane stores a start offset, so it plays with that phase relative to the bar and stays locked.
// Lane step s fires on global step (s + start_off) % loop_len. Offset 0 = the old behaviour, bit-identical.
bool     seqLaneEmpty(Engine e);                  // no step bit inside the visible loop
bool     seqWaitingFirstHit(Engine e);            // PLAY + REC armed + empty lane (OLED badge)
void     seqSetGridFollowed(bool followed);       // firmware: true while the grid is slaved to an external clock (clock_engine)
uint16_t seqStartOffset(Engine e);                // 0 .. loop-1
void     seqSetStartOffset(Engine e, uint16_t off);   // patch load (patch extension)

// ---- AUDIO plane: called once per 16th by the ISR after clockTick() reports a step.
// Returns the lane mask to fire for this engine on this global step, already including
// generative decisions (probability/density) and the arp. `held` = pads physically held.
// `gen_tick_count` (2026-10, TASK 1): a monotonic count of engine/clock.h's independent
// generative-subdivision ticks (sum of every clockGenTick() return value since boot,
// accumulated by the caller in audio_callback.cpp). SEQ_GEN's dice-rolling (the
// density*probability bed and the held-pad live roll) only re-rolls when this has
// advanced since the engine's last roll -- i.e. generative decisions fire on the
// GENRATE subdivision, not unconditionally on every 16th. MANUAL/PLAY lanes ignore it
// entirely (unaffected). See sequencer.cpp's seqFire() SEQ_GEN branch for the gate.
uint16_t seqFire(Engine e, uint32_t gstep, uint16_t held, uint32_t gen_tick_count);
// synth helpers used by the synth engine when firing generative notes
int      seqPickOctaveOffset(const LaneSet& l);  // -N..+N semitone-free octave offset per oct_amount/oct_prob

// ---- params for the editor
const ParamPage* seqParamPage(Engine e);
