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
} BelaContext;

static inline float audioRead(BelaContext *c, int n, int ch) { (void)c; (void)n; (void)ch; return 0.0f; }
static inline void audioWrite(BelaContext *c, int n, int ch, float v) { (void)c; (void)n; (void)ch; (void)v; }

#endif
