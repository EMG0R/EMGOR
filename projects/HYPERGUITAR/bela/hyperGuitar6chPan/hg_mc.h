/*
 * hg_mc.h - multicore audio-block support (2026-10-12).
 *
 * The six per-string voice chains run as jobs on up to three cores. The
 * audio thread (core 3) keeps its Bela-assigned place and owns one job;
 * RT worker threads (EVL, pinned to cores 1/2) own the others. The
 * ALGORITHM is unchanged: every cross-string read in the voice path is an
 * ORDERED read that returns exactly what the old sample-by-sample, string-
 * by-string loop would have seen (see hgXWait / hgTrkRead / hgOnsRead), so
 * the output is bit-identical to the single-threaded build.
 */
#ifndef HG_MC_H
#define HG_MC_H

#include <stdint.h>

/* thread-local, local-exec model: the project is linked as an executable, so this is a
 * plain tpidr_el0-relative access (no __tls_get_addr call in the sample loop) */
#define HG_TLS __thread __attribute__((tls_model("local-exec")))

#define HG_MC_MAXB 128           /* longest block the job path handles */

extern HG_TLS int gBlkN;       /* sample index inside the current block (this thread) */
extern int gXMode;               /* 1 = ordered cross-string reads (job path), 0 = live reads (legacy loop) */
extern volatile long gXTimeouts; /* ordered-read waits that gave up (should stay 0) */

/* progress: number of samples string o has fully completed this block */
void hgXWait(int o, int need);
void hgXProgress(int s, int done);
void hgXBlockBegin(void);

/* shifter.cpp: strum-coherence onset state, versioned per block */
void hgOnsBeginBlock(void);
void hgOnsRead(int o, int s, unsigned int *stamp, unsigned int *hold);

/* pitch_tracker.cpp: neighbour bleed state, versioned per block */
void hgTrkBeginBlock(int B);

/* thread pool */
typedef void (*HgJobFn)(void *ctx, int job);
int  hgMcStart(void *belaContext);     /* returns number of worker threads running */
void hgMcStop(void);
int  hgMcWorkers(void);                /* workers currently usable */
/* run fn(ctx, 0) on the caller and fn(ctx, k+1) on worker k. Returns 1 when
 * every job completed, 0 if no workers were usable (caller must run ALL
 * jobs itself), -1 if a worker failed to finish in time (MC is disabled). */
int  hgMcRun(HgJobFn fn, void *ctx, int periodNs);
int  hgMcPublish(HgJobFn fn, void *ctx, int periodNs);   /* 1 = workers have the block, 0 = not usable */
int  hgMcJoin(void);                                      /* after job 0 and the merge: 1 ok, -1 worker lost */
int  hgXAvail(int need);                                  /* samples complete for ALL strings (waits for at least need) */
/* 2026-11-01 lpc-w: LPC analysis worker (lpc.cpp lpcWorkerPoll). Start from setup(), stop from cleanup().
 * Returns 1 if the worker is up (lpcSubmit then publishes to it), 0 if not (analysis stays inline). */
int  hgLpcWorkerStart(void);
void hgLpcWorkerStop(void);
void hgMcStats(unsigned long *fallbacks, unsigned long *timeouts);

/* flush-to-zero for denormals (FPCR.FZ): per-thread state, so every thread that runs voice code must call it */
static inline void hgSetFtz(void)
{
#ifdef HG_FTZ
	uint64_t fpcr;
	__asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
	fpcr |= (1ull << 24);
	__asm__ volatile("msr fpcr, %0" : : "r"(fpcr));
#endif
}

#ifdef __cplusplus
#include <atomic>
extern std::atomic<int> gRenderCpu;   /* written by the render thread every block */
#endif

static inline uint64_t hgTicks(void)
{
#if defined(__aarch64__)
	uint64_t v;
	__asm__ volatile("isb; mrs %0, cntvct_el0" : "=r"(v));
	return v;
#else
	return 0;   /* 2026-11-01 tier-A: host build on non-ARM (timing telemetry reads 0) */
#endif
}

/* 2026-11-01 tier-A: counter frequency (Hz) for converting hgTicks() to time */
static inline uint64_t hgTickHz(void)
{
#if defined(__aarch64__)
	uint64_t f;
	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(f));
	return f ? f : 1;
#else
	return 1;
#endif
}

#endif
