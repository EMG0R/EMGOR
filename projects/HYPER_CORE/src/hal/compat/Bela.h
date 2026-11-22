/* HYPER_CORE import
 * origin : ~/Documents/_EMGOR_SYNTH/______2026NEW/hyperGuitar/bela/hyperGuitar6chPan/hosttest/stub/Bela.h
 * commit : hyperGuitar 6088064
 * changes: none (verbatim). Minimal Bela.h surface; on the Seed these are the seams to implement (rt_printf, audioRead/Write unused by the DSP files)
 * licence: Emory (own work) */
/*
 * Host-test stub of Bela.h - just enough surface for hyperguitar.h and the
 * DSP translation units (dsp_core, pitch_tracker, shifter, lpc) to compile
 * and run on a desktop machine. render.cpp / trellis_ui.cpp are NOT linked
 * into the host tests.
 */
#ifndef BELA_STUB_H
#define BELA_STUB_H

#include <cstdio>
#include <cstring>
#include <cstdint>

#define rt_printf printf

typedef void *AuxiliaryTask;

typedef struct {
	float audioSampleRate;
	uint32_t audioFrames;
	uint32_t audioInChannels;
	uint32_t audioOutChannels;
} BelaContext;

static inline float audioRead(BelaContext *c, int n, int ch) { (void)c; (void)n; (void)ch; return 0.0f; }
static inline void audioWrite(BelaContext *c, int n, int ch, float v) { (void)c; (void)n; (void)ch; (void)v; }

#endif
