/*
 * hg_mc.cpp - ordered cross-string reads + the RT worker pool.
 * See hg_mc.h. Design notes live in /root/hg_bench/DESIGN-mc.txt.
 */
#include "hyperguitar.h"
#include "hg_mc.h"
#include <atomic>
#include <thread>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#ifdef BELA_EVL
#include <evl/evl.h>
#endif
#include "RtThread.h"

HG_TLS int gBlkN = 0;
int gXMode = 0;
volatile long gXTimeouts = 0;
#ifdef HG_MC_STATS
uint64_t gMcSt[16];   /* 0 main job ticks, 1 main wait-for-workers ticks, 2.. worker job ticks, 5 xwait spins ticks, 6 xwait count, 7 worker start latency ticks, 8 blocks */
#endif

#define PROG_TIMEOUT_TICKS 800000u     /* 4 ms at 200 MHz: a wait that long = a dead worker */

#ifdef HG_MC_STATS
long gOnsTO[NUM_STRINGS]; long gOnsTOinfo;
#endif
struct alignas(64) Prog { std::atomic<int> v; char pad[60]; };
static Prog gProg[NUM_STRINGS];

void hgXBlockBegin(void)
{
	for(int s = 0; s < NUM_STRINGS; s++)
		gProg[s].v.store(0, std::memory_order_relaxed);
}

void hgXProgress(int s, int done)
{
	gProg[s].v.store(done, std::memory_order_release);
}

void hgXWait(int o, int need)
{
	if(gProg[o].v.load(std::memory_order_acquire) >= need)
		return;
	uint64_t t0 = hgTicks();
	unsigned int spins = 0;
#ifdef HG_MC_STATS
	gMcSt[6]++;
	struct W { uint64_t t0; ~W() { gMcSt[5] += hgTicks() - t0; } } w_{t0};
#endif
	while(gProg[o].v.load(std::memory_order_acquire) < need) {
		__asm__ volatile("yield");
		if((++spins & 255u) == 0 && hgTicks() - t0 > PROG_TIMEOUT_TICKS) {
			gXTimeouts++;
#ifdef HG_MC_STATS
			gOnsTO[o]++; gOnsTOinfo = gBlkN * 100 + need;
#endif
			return;
		}
	}
}

/* ------------------------------ worker pool ------------------------------ */

#define MAX_WORKERS 3
struct alignas(64) WorkerSlot {
	std::atomic<unsigned int> doneSeq;
	char pad[60];
};
static WorkerSlot gWs[MAX_WORKERS];
static RtThread gThr[MAX_WORKERS];
static int gNWorkers = 0;
static std::atomic<int> gAlive;           /* workers that finished init */
static std::atomic<int> gQuit(0);
static std::atomic<int> gUsable(0);       /* main may dispatch */
static std::atomic<unsigned int> gSeq(0);
static HgJobFn gFn = 0;
static void *gCtx = 0;
static volatile int gPeriodNs = 1451247;
static std::atomic<uint64_t> gPubAt[4];   /* CLOCK_MONOTONIC ns of publish number (seq & 3) */
static unsigned long gFallbacks = 0, gTimeouts = 0;
/* CPU the render thread runs on (Bela starts it unpinned; on this image it ends up on
 * core 0, the 'bela-audio' I/O thread being pinned to the last core). The workers move
 * themselves to cores that are neither: a worker sharing the render thread's core
 * could never run while the render thread spins on it. */
std::atomic<int> gRenderCpu(-1);
static std::atomic<int> gPlaced[MAX_WORKERS];
#ifdef HG_MC_STATS
static volatile uint64_t gPubTick;
#endif

static inline uint64_t nowNs(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec;
}

/* k-th worker core given the render thread's core r: prefer 1, 2, then 0, never r,
 * never the pinned I/O core (nproc-1) unless nothing else is left */
static int workerCpu(int k, int r, int nproc)
{
	int order[4] = { 1, 2, 0, 3 };
	int n = 0;
	for(int i = 0; i < 4 && i < nproc; i++) {
		int c = order[i];
		if(c == r || c == nproc - 1) continue;
		if(n++ == k) return c;
	}
	return 1 + k;
}

static void workerMain(void *arg)
{
	const int k = (int)(intptr_t)arg;
	unsigned int seen = 0;
	const int nproc = (int)std::thread::hardware_concurrency();
	int myRc = -2, myCpu = 1 + k;
	hgSetFtz();
	/* first rt_printf from a thread sets up its print buffer (an in-band excursion): do it here, at start-up,
	 * not on the first onset of a set. */
	rt_printf("hg-mc: worker %d up (prio %d)\n", k, 94);
	gAlive.fetch_add(1);
	while(!gQuit.load(std::memory_order_relaxed)) {
		/* wait for a new block: spin, with an oob timed sleep covering most
		 * of the gap so the core is not burned (and the EVL watchdog never
		 * sees a thread that never blocks) */
		uint64_t spinStart = nowNs();
		for(;;) {
			/* follow the render thread's core (cheap check once per block; the affinity
			 * call itself is a one-off at start-up) */
			{
				int rc = gRenderCpu.load(std::memory_order_relaxed);
				if(rc != myRc) {
					int want = workerCpu(k, rc, nproc);
					if(rc >= 0 && want != myCpu) {
						cpu_set_t cs;
						CPU_ZERO(&cs);
						CPU_SET((unsigned)want, &cs);
						if(pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs) == 0)
							myCpu = want;
					}
					myRc = rc;
					gPlaced[k].store(rc >= 0 && myCpu == want ? 1 : 0, std::memory_order_release);
				}
			}
			unsigned int sq = gSeq.load(std::memory_order_acquire);
			if(sq != seen) { seen = sq;
				break; }
			if(gQuit.load(std::memory_order_relaxed)) return;
			uint64_t t = nowNs();
			uint64_t pub = gPubAt[seen & 3].load(std::memory_order_relaxed);   /* when the block we last ran was published */
			uint64_t per = (uint64_t)gPeriodNs;
			/* predicted next publish = pub + per; wake 300 us before it */
			uint64_t wake = pub + per - 300000ull;
#ifdef HG_MC_STATS

#endif
			if(t + 60000ull < wake && t - spinStart < 4 * per) {
#ifdef BELA_EVL
				struct timespec ts;
				ts.tv_sec = wake / 1000000000ull; ts.tv_nsec = wake % 1000000000ull;
				evl_sleep_until(EVL_CLOCK_MONOTONIC, &ts);
#else
				struct timespec ts;
				ts.tv_sec = wake / 1000000000ull; ts.tv_nsec = wake % 1000000000ull;
				clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, 0);
#endif
				continue;
			}
			if(t - spinStart > 3 * per) {
				/* nothing published for three periods (audio stalled?): nap 0.5 ms
				 * at a time rather than spinning forever */
#ifdef BELA_EVL
				evl_usleep(500);
#else
				usleep(500);
#endif
				spinStart = nowNs() - 2 * per;
				continue;
			}
			__asm__ volatile("yield");
		}
#ifdef HG_MC_STATS
		uint64_t j0 = hgTicks();
		gMcSt[7] += j0 - gPubTick;
#endif
		gFn(gCtx, k + 1);
#ifdef HG_MC_STATS
		gMcSt[2 + k] += hgTicks() - j0;
#endif
		gWs[k].doneSeq.store(seen, std::memory_order_release);
	}
}

int hgMcStart(void *belaContext)
{
	(void)belaContext;
	if(gNWorkers)
		return gNWorkers;
	const char *e = getenv("HG_MC_WORKERS");
	int want = e ? atoi(e) : 2;
	if(want > MAX_WORKERS) want = MAX_WORKERS;
	if(want <= 0)
		return 0;
	unsigned int nproc = std::thread::hardware_concurrency();
	if(nproc < 3)
		return 0;
	lpcInitOnce();
	gQuit.store(0);
	gAlive.store(0);
	gSeq.store(0);
	for(int k = 0; k < MAX_WORKERS; k++) gWs[k].doneSeq.store(0);
	int prio = 94;     /* audio thread is 96, Bela's own MP threads use audio-2 */
	for(int k = 0; k < want; k++) {
		cpu_set_t cs;
		CPU_ZERO(&cs);
		/* initial placement; they re-pin themselves once the render thread's core is known */
		CPU_SET((unsigned)(1 + k), &cs);
		char nm[32];
		snprintf(nm, sizeof(nm), "hg-mc-%d", k);
		if(gThr[k].create(nm, prio, workerMain, (void *)(intptr_t)k, &cs, 1024 * 1024)) {
			fprintf(stderr, "hg-mc: worker %d failed to start\n", k);
			break;
		}
		gNWorkers++;
		/* 2026-10-20: stagger the thread spin-up instead of back-to-back -
		 * this only runs once in hgMcStart() (called from setup(), before
		 * the first render() block), so it is free on the audio clock.
		 * Chasing the same Pi-side power-sag brownout as the render.cpp
		 * setup() stagger - see its comment. */
		usleep(100000);
	}
	/* wait (bounded) for the workers to come up */
	for(int i = 0; i < 200 && gAlive.load() < gNWorkers; i++)
		usleep(1000);
	if(gAlive.load() == gNWorkers && gNWorkers > 0)
		gUsable.store(1);
	else
		fprintf(stderr, "hg-mc: only %d/%d workers alive - single-thread\n", gAlive.load(), gNWorkers);
	return gUsable.load() ? gNWorkers : 0;
}

void hgMcStop(void)
{
	gUsable.store(0);
	gQuit.store(1);
	for(int k = 0; k < gNWorkers; k++)
		gThr[k].join();
	gNWorkers = 0;
}

int hgMcWorkers(void) { return gUsable.load() ? gNWorkers : 0; }

void hgMcStats(unsigned long *fb, unsigned long *to) { if(fb) *fb = gFallbacks; if(to) *to = gTimeouts; }

/* publish job k+1 to worker k; the caller then runs job 0 and later calls hgMcJoin().
 * Returns 1 when the workers have the block, 0 when they are not usable (nothing was started). */
static unsigned int gPendingSeq = 0;
int hgMcPublish(HgJobFn fn, void *ctx, int periodNs)
{
	if(!gUsable.load(std::memory_order_relaxed)) {
		gFallbacks++;
		return 0;
	}
	/* every worker must have settled on its core for the render thread's current core */
	for(int k = 0; k < gNWorkers; k++)
		if(!gPlaced[k].load(std::memory_order_acquire)) {
			gFallbacks++;
			return 0;
		}
	gFn = fn; gCtx = ctx; gPeriodNs = periodNs;
	unsigned int sq = gSeq.load(std::memory_order_relaxed) + 1;
	gPubAt[sq & 3].store(nowNs(), std::memory_order_relaxed);
#ifdef HG_MC_STATS
	gPubTick = hgTicks();
#endif
	gSeq.store(sq, std::memory_order_release);     /* publishes fn/ctx/block data */
	gPendingSeq = sq;
	return 1;
}

/* wait until every worker has finished the published block. 1 = all done, -1 = a worker is gone (MC disabled). */
int hgMcJoin(void)
{
	unsigned int sq = gPendingSeq;
	uint64_t t0 = hgTicks();
	unsigned int spins = 0;
	for(int k = 0; k < gNWorkers; k++) {
		while(gWs[k].doneSeq.load(std::memory_order_acquire) != sq) {
			__asm__ volatile("yield");
			if((++spins & 255u) == 0 && hgTicks() - t0 > 2 * PROG_TIMEOUT_TICKS) {
				/* a worker is not coming back: stop dispatching, let the engine
				 * carry on single-threaded. THIS block is damaged. */
				gUsable.store(0);
				gTimeouts++;
				return -1;
			}
		}
	}
	return 1;
}

int hgMcRun(HgJobFn fn, void *ctx, int periodNs)
{
	if(!hgMcPublish(fn, ctx, periodNs))
		return 0;
	fn(ctx, 0);
	return hgMcJoin();
}

/* lowest per-string progress: how many samples of the block are complete for ALL strings */
int hgXAvail(int need)
{
	int a = 1 << 30;
	for(int s = 0; s < NUM_STRINGS; s++) {
		int v = gProg[s].v.load(std::memory_order_acquire);
		if(v < a) a = v;
	}
	if(a >= need)
		return a;
	uint64_t t0 = hgTicks();
	unsigned int spins = 0;
	for(;;) {
		a = 1 << 30;
		for(int s = 0; s < NUM_STRINGS; s++) {
			int v = gProg[s].v.load(std::memory_order_acquire);
			if(v < a) a = v;
		}
		if(a >= need)
			return a;
		__asm__ volatile("yield");
		if((++spins & 255u) == 0 && hgTicks() - t0 > PROG_TIMEOUT_TICKS) {
			gXTimeouts++;
			return need;       /* give up waiting: the rows may be stale, but never hang */
		}
	}
}


/* ------------------------------------------------------------------------
 * 2026-11-01 lpc-w: LPC ANALYSIS WORKER.
 * One RT thread (RtThread = the same helper the dead MC pool uses, so it is EVL-attached when built
 * for EVL), FIFO prio 90 (audio thread 96, MC workers 94 - never started), pinned to core 1, FTZ on.
 * Wake-up: it POLLS the per-string job sequence numbers (lpcWorkerPoll) and, when idle, sleeps
 * 100 us with the EVL/clock sleep hg_mc already uses (evl_usleep under BELA_EVL, usleep otherwise).
 * The audio thread therefore publishes with a single release store: no syscall, no semaphore post
 * from the RT loop. A kernel semaphore (evl_sem/evl_event) would wake sooner but its API could not
 * be verified without the device; ~50 us average wake latency is irrelevant against the 384-sample
 * (8.7 ms) cadence and the 1-2 block adoption budget.
 * ------------------------------------------------------------------------ */
static RtThread gLpcThr;
static std::atomic<int> gLpcQuit(0), gLpcUp(0), gLpcStarted(0);

static void lpcWorkerMain(void *)
{
	hgSetFtz();
	rt_printf("hg-lpc: worker up (prio 90, core 1)\n");   /* first rt_printf sets up this thread's print buffer: do it now */
	gLpcUp.store(1);
	while(!gLpcQuit.load(std::memory_order_relaxed)) {
		if(lpcWorkerPoll())
			continue;     /* more may be pending: look again before sleeping */
#ifdef BELA_EVL
		if(evl_usleep(100) < 0)
			usleep(100);
#else
		usleep(100);
#endif
	}
}

int hgLpcWorkerStart(void)
{
#if HG_LPC_WORKER && !defined(HG_LPC_SPREAD)
	if(gLpcStarted.load())
		return gLpcUp.load();
	const char *e = getenv("HG_LPC_WORKER");
	if(e && atoi(e) == 0)
		return 0;
	if(std::thread::hardware_concurrency() < 3)
		return 0;
	lpcInitOnce();   /* tables must be complete before the worker reads them */
	gLpcQuit.store(0);
	gLpcUp.store(0);
	cpu_set_t cs;
	CPU_ZERO(&cs);
	CPU_SET(1u, &cs);
	if(gLpcThr.create("hg-lpc", 90, lpcWorkerMain, 0, &cs, 256 * 1024)) {
		fprintf(stderr, "hg-lpc: worker failed to start - LPC stays on the audio thread\n");
		return 0;
	}
	gLpcStarted.store(1);
	for(int i = 0; i < 200 && !gLpcUp.load(); i++)
		usleep(1000);
	if(!gLpcUp.load()) {
		fprintf(stderr, "hg-lpc: worker did not come up - LPC stays on the audio thread\n");
		gLpcQuit.store(1);
		gLpcThr.join();
		gLpcStarted.store(0);
		return 0;
	}
	lpcWorkerEnable(1);
	return 1;
#else
	return 0;
#endif
}

void hgLpcWorkerStop(void)
{
	if(!gLpcStarted.load())
		return;
	lpcWorkerEnable(0);          /* audio thread goes back to inline analysis */
	gLpcQuit.store(1);
	gLpcThr.join();
	gLpcStarted.store(0);
	gLpcUp.store(0);
}
