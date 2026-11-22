// metro.h — the record-mode count: one quiet, tiny click per beat, locked to the master grid.
//
// WHAT IT IS: not a voice and not an FX. A short damped sine ("tick") summed into the master bus just
// before the output stage (so the master fader scales it with everything else) and AFTER the FX chain
// (so stutters / ring / crush never touch it). It is deliberately far below the engines: at the default
// level its peak is ~-26 dBFS before the master stage, accent ~-22 dBFS.
//
// WHEN: audible only while RECORD mode is on (any lane's REC param, seq.lane[e].rec_on -- the single record
// flag). It arms the same block REC turns on and stops scheduling new clicks when REC turns off; a click
// already sounding rings out. PLAY mode alone does not arm it. METRO = 0% in GLOBALS silences it.
//
// TEMPO: there is no tempo here. metroStep() is called by the audio callback on every master 16th
// (engine/clock.cpp's grid, the one clock the Pi follows/drives), so the click is the same grid the
// sequencer and the MIDI clock use and follows tap / slaved tempo changes for free.
#pragma once
#include <stddef.h>
#include <stdint.h>

struct MetroState {
    float level;              // 0..1 control (GLOBALS "METRO"). 0 = off. Default 0.25 -> ~-26 dBFS peak.
    volatile bool armed;      // AUDIO-plane mirror of record mode (any lane rec_on), set each block by metroBlock()
    // audio-plane private
    float env;                // current click envelope 1 -> 0
    float phase;              // sine phase 0..1
    float inc;                // phase increment per sample
    float amp;                // peak amplitude of the click in flight
};
extern MetroState metro;

void metroInit();
// AUDIO plane
void metroBlock(bool any_lane_recording);            // once per audio block: armed = record mode on (any lane's rec_on)
void metroStep(uint32_t gstep, uint32_t bar_steps);  // once per master 16th: clicks on beats (every 4th), accent on the bar start
void metroProcess(float* L, float* R, size_t n);     // sums the click into the master bus
