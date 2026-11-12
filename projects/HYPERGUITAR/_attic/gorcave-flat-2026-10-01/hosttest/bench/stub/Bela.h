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

#define rt_printf(...) ((void)0)   /* bench: no I/O inside the timed region */

typedef void *AuxiliaryTask;

typedef struct {
	float audioSampleRate;
	uint32_t audioFrames;
	uint32_t digitalFrames;
	uint32_t audioInChannels;
} BelaContext;

/* --- bench hooks: real audio in/out driven by bench_main.cpp --- */
#define HG_BENCH_CHANS 16
extern float gBenchIn[HG_BENCH_CHANS][1024];
extern float gBenchOut[2][1024];
static inline float audioRead(BelaContext *c, int n, int ch) { (void)c; return gBenchIn[ch][n]; }
static inline void audioWrite(BelaContext *c, int n, int ch, float v) { (void)c; gBenchOut[ch][n] = v; }


/* --- augmented for full-project host syntax check (not the real Bela) --- */
#ifndef HG_FULLSTUB_EXTRA
#define HG_FULLSTUB_EXTRA
#define INPUT 0
#define OUTPUT 1
static inline int  digitalRead(BelaContext*, unsigned int, unsigned int){ return 0; }
static inline void pinMode(BelaContext*, unsigned int, unsigned int, int){}
typedef void* AuxiliaryTask;
template<typename F> static inline AuxiliaryTask Bela_createAuxiliaryTask(F, int, const char*){ return 0; }
static inline void Bela_scheduleAuxiliaryTask(AuxiliaryTask){}
static inline void Bela_requestStop(void){}
#endif

#endif
