# Transparent Pitch+Formant Shifter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the four-engine pitch/formant stack in `bela/hyperGuitar6chPan` with one unified pitch-synchronous grain engine per string (pitch = grain rate, formant = grain playback speed), single-copy transient handling, plus a switchable LPC true-envelope formant mode.

**Architecture:** Per spec `docs/superpowers/specs/2026-08-01-transparent-pitch-formant-design.md`. New `shifter.cpp` holds the engine (pitch marks, grain scheduler, transient tap, no-lock fallback splicer); new `lpc.cpp` holds the LPC formant mode; `voiceProcess()` is rewritten around them; old SOLAD/`GrainStream` machinery is deleted. A host-side test harness (`hosttest/`) verifies pitch accuracy, sustain evenness (COLA), and single-copy attacks before anything is deployed.

**Tech Stack:** C++ (Bela real-time audio, per-sample), host tests with plain c++/make on macOS.

## Global Constraints

- Attack clarity/speed is the top priority (spec §Top priority).
- No dynamic allocation, no syscalls, no unbounded loops in the audio path.
- All per-string parameters scale with that string's tracked period (hex leverage, spec §3.4).
- Engine must be exact-COLA at every (ρ, φ): window length = 2 × launch spacing, Hann, 50% overlap.
- Preset values (pads 0–7) unchanged (spec §3.10).
- Deploy only via `bela/deploy.sh`; user approves by ear before the swap counts as done.
- Style: match existing code (tabs, block comments explaining *why*, flat C structs).

## Coordinate conventions (used by all tasks)

Delay coordinates: "delay" = samples behind the write head; grows by 1 every sample.
A grain launched with source-start delay `d0` and read increment `phi` reads at delay
`d0 + u*(1 - phi)` after `u` output samples. Grain covers `L = 2S` output samples,
Hann-windowed, `S` = launch spacing = `Tsm/rho`. Grain source is centered on a pitch
mark at delay `dc`, so `d0 = dc + S*phi`. Causality bound: `dc >= 3 + max(0, S*(phi-2))`;
if the newest mark is too new, use `dc = markDelay + k*Tsm` (period-multiple older copy).
Buffer bound: `d0 + guard <= BUF_MASK - 8`.

---

### Task 1: deploy.sh copies the whole project

**Files:** Modify: `bela/deploy.sh`

The project was split into 8 source files but deploy.sh still copies only render.cpp — a deploy today would build stale/missing code on the board.

- [ ] **Step 1:** Replace the single scp line with:
```bash
scp "$SRC"/*.cpp "$SRC"/*.h "root@$HOST:~/Bela/projects/$PROJECT/"
```
- [ ] **Step 2:** Commit: `git add bela/deploy.sh && git commit -m "deploy.sh: ship all split source files, not just render.cpp"`

### Task 2: Host test harness scaffold

**Files:**
- Create: `bela/hyperGuitar6chPan/hosttest/stub/Bela.h`, `hosttest/stub/libraries/Encoder/Encoder.h`, `hosttest/Makefile`, `hosttest/test_main.cpp`

**Interfaces produced:** `make -C hosttest && ./hosttest/run_tests` runs all tests; `test_main.cpp` owns definitions of every global `extern`'d in hyperguitar.h that dsp_core/pitch_tracker/shifter/lpc reference, plus `runVoiceSamples(Voice*, int s, const float *in, float *wet, float *dry, int n)` which replicates render.cpp's hop cadence (trackPitch/updateModulation/updateLP every HOP samples, voiceProcess per sample).

- [ ] **Step 1:** Write stubs. `Bela.h`: `#define rt_printf printf`, minimal `BelaContext`, `AuxiliaryTask` as `void*`, no-op `audioRead/audioWrite/Bela_createAuxiliaryTask/Bela_scheduleAuxiliaryTask`. Encoder stub: empty class with the methods render.cpp uses (not linked in tests, but header must parse).
- [ ] **Step 2:** Makefile: `c++ -O2 -std=c++11 -I stub -I .. test_main.cpp ../dsp_core.cpp ../pitch_tracker.cpp ../shifter.cpp ../lpc.cpp -o run_tests -lm` (shifter.cpp/lpc.cpp join in Tasks 4/6; start without them).
- [ ] **Step 3:** test_main.cpp scaffolding: global definitions; `gHannT` init; Voice init copied from render.cpp setup() (p0 from OPEN_HZ, lagMin/lagMax/corrWin, etc.); signal generators `genSaw(f0, seconds, gain)` (20 band-limited harmonics, 1/n amplitudes) and `genBurst` (silence, then saw with 2 ms attack ramp); analyzers `measurePeriod(buf, n, sr)` (autocorrelation peak, parabolic refine) and `frameRms(buf, n, frameLen, out)`.
- [ ] **Step 4:** First test `test_passthrough`: beta=1, formant=1, run 1 s of 110 Hz saw, assert wet output RMS within 3 dB of input RMS and no NaN. Run: `make -C bela/hyperGuitar6chPan/hosttest && ./bela/hyperGuitar6chPan/hosttest/run_tests`. Expected: PASS against the *current* engine (validates harness before surgery).
- [ ] **Step 5:** Commit.

### Task 3: hyperguitar.h — Shifter struct, Voice surgery, new globals

**Files:** Modify: `bela/hyperGuitar6chPan/hyperguitar.h`

**Produces (used by Tasks 4–7):**

```c
#define SH_NGRAINS 6
#define LPC_ORDER 16
#define LPC_WIN 512
#define LPC_EPTS 33

typedef struct {
	float d0, u, L, phi;
	int active;
} ShGrain;

typedef struct {
	/* pitch-mark picker (2nd-order LP at ~1.5*f0 + once-per-period peak pick) */
	float mkB0, mkB1, mkB2, mkA1, mkA2, mkZ1, mkZ2;
	float markDelay, markPrevDelay;
	int haveMark;
	float pkVal, pkDelay;
	float searchIn, searchLeft;
	int markUses, launchParity;
	/* scheduler */
	float Tsm, launchCd;
	ShGrain gr[SH_NGRAINS];
	/* transient path + mode machine: 0=GRAINS 1=TAP_TRANSIENT 2=TAP_DRIFT */
	int mode;
	float grainMix;          /* 0 = tap leg, 1 = grain leg (equal-power blend) */
	float tapDelay, tapOld;
	int tapFading; float tapFadePh, tapFadeStep;
	float phiRamp;           /* 0 after attack -> 1; formant eases back in */
	float envF, envS;
	int onsetRefract, relockHops, noLockHops;
	unsigned int transHoldUntil; /* absolute sample stamp for strum coherence */
	/* anti-alias write filter for phi>1 grain reads */
	float aaZ, aaCoef;
	/* LPC formant mode */
	float lpcK[LPC_ORDER], lpcKW[LPC_ORDER];   /* analysis (whiten) targets + smoothed */
	float lpcKr[LPC_ORDER], lpcKrW[LPC_ORDER]; /* re-color (warped) targets + smoothed */
	float lpcZf[LPC_ORDER], lpcZg[LPC_ORDER];  /* lattice states */
	float lpcGain, lpcGainW;
	int lpcValid;
} Shifter;
```

- [ ] **Step 1:** Add the block above; add `Shifter sh;` + `float abuf[FXBUF_SIZE];` to Voice (rename of fxbuf's storage role: anti-aliased copy for phi>1 reads). Remove from Voice: `GrainStream up, fsWet, fsDry`, `fxbuf`, `sdDelay/sdOldDelay/sdFadePhase/sdFadeStep/sdFading/sdHiThresh`, `downAmt/upAmt`, `formantBlend`. Remove `Grain`/`GrainStream` typedefs, `NGRAINS`, `HIGH_MIX`.
- [ ] **Step 2:** Add globals + prototypes:
```c
extern volatile int gFormantMode;      /* 0 = GRAIN, 1 = LPC */
extern unsigned int gSampleStamp;      /* absolute sample counter (strum coherence) */
extern unsigned int gOnsetStamp[NUM_STRINGS];
/* shifter.cpp */
void shifterInit(Voice *v);
void shifterHop(Voice *v, int s);
float shifterRun(Voice *v, float full);
void shifterOnset(Voice *v, int s);
/* lpc.cpp */
void lpcAnalyze(Voice *v);
float lpcProcess(Voice *v, float x);
void applyMixRouting(void);
```
Change `void trackPitch(Voice *v);` → `void trackPitch(Voice *v, int s);`.
Add near gHannT: `static inline float shHann(float t){ if(t<=0.0f||t>=1.0f) return 0.0f; return gHannT[(int)(t*HANN_N)]; }` and delete the private copy in dsp_core.cpp.
- [ ] **Step 3:** Full project compile is broken until Task 4 — that's expected; commit at end of Task 4 instead.

### Task 4: shifter.cpp + voiceProcess/updateModulation/render integration

**Files:**
- Create: `bela/hyperGuitar6chPan/shifter.cpp`
- Modify: `bela/hyperGuitar6chPan/dsp_core.cpp` (voiceProcess rewrite; delete launchGrain/streamRun/soladRun/hann01; updateModulation surgery; updateLP loses the formant `mult` line)
- Modify: `bela/hyperGuitar6chPan/render.cpp` (setup() init → `shifterInit`, delete sd*/GrainStream seeding; define `gFormantMode`, `gSampleStamp`, `gOnsetStamp`; `trackPitch(v)` → `trackPitch(v, s)`; `gSampleStamp++` per frame)
- Modify: `bela/hyperGuitar6chPan/pitch_tracker.cpp` (signature only, veto added in Task 7)
- Test: hosttest tests 2–5 below

**shifter.cpp core (complete):**

```c
/*
 * shifter.cpp - unified pitch+formant engine, one per string.
 *
 * One pitch-synchronous grain stream where grain LAUNCH SPACING sets the
 * pitch ratio and within-grain READ INCREMENT sets the formant ratio
 * (Lent 1989 / Bristow-Johnson 1995 / IVL US 5,567,901). Window = Hann of
 * exactly 2x the launch spacing, in the OUTPUT domain: grains always
 * overlap 50% and sum to a constant - zero grain-rate AM at any setting.
 *
 * Attacks are never granulated: a per-sample onset detector snaps a
 * direct tap to the onset and the transient passes exactly once at unity
 * gain, then grains fade back in once the tracker re-locks (Grofit &
 * Lavner 2008 "lock hop"; katjaas pointer-jump). During mutes/scratches
 * (no pitch lock) a correlation-refined dual-tap splicer takes over
 * (Eventide US 4,464,784 lineage).
 */
#include "hyperguitar.h"
#include <cmath>

#define SH_MODE_GRAINS 0
#define SH_MODE_TRANS  1
#define SH_MODE_DRIFT  2

void shifterInit(Voice *v)
{
	Shifter *sh = &v->sh;
	memset(sh, 0, sizeof(Shifter));
	sh->Tsm = v->p0;
	sh->markDelay = v->p0;
	sh->markPrevDelay = 2.0f * v->p0;
	sh->searchIn = 8.0f;
	sh->searchLeft = 0.0f;
	sh->mode = SH_MODE_GRAINS;
	sh->grainMix = 1.0f;
	sh->tapDelay = 64.0f;
	sh->phiRamp = 1.0f;
	sh->aaCoef = 1.0f;
	sh->lpcGain = sh->lpcGainW = 1.0f;
}

/* per-hop duties: mark-LP coefficients, mode transitions, LPC analysis */
void shifterHop(Voice *v, int s)
{
	Shifter *sh = &v->sh;

	/* mark-picker lowpass at 1.5*f0 (Q=0.707) */
	float fc = 1.5f * v->srate / v->period;
	if(fc > 0.45f * v->srate) fc = 0.45f * v->srate;
	if(fc < 40.0f) fc = 40.0f;
	float w0 = 2.0f * (float)M_PI * fc / v->srate;
	float cw = cosf(w0), al = sinf(w0) * 0.7071f;
	float a0i = 1.0f / (1.0f + al);
	sh->mkB1 = (1.0f - cw) * a0i;
	sh->mkB0 = sh->mkB2 = 0.5f * sh->mkB1;
	sh->mkA1 = -2.0f * cw * a0i;
	sh->mkA2 = (1.0f - al) * a0i;

	/* anti-alias one-pole for phi>1 grain reads (write-side) */
	float phiT = v->formant > 1.0f ? v->formant : 1.0f;
	float fcA = 0.45f * v->srate / phiT;
	sh->aaCoef = 1.0f - expf(-2.0f * (float)M_PI * fcA / v->srate);

	/* re-lock accounting */
	if(v->attackBlank == 0 && v->pitchConf >= PITCH_CONF_MIN) {
		if(sh->relockHops < 100) sh->relockHops++;
		sh->noLockHops = 0;
	} else {
		sh->relockHops = 0;
		if(v->pitchConf < NSDF_CONF && sh->noLockHops < 100)
			sh->noLockHops++;
	}

	/* mode transitions (hop rate is plenty - fades are sample-rate) */
	if(sh->mode == SH_MODE_TRANS) {
		if(sh->relockHops >= 2 && gSampleStamp >= sh->transHoldUntil) {
			sh->mode = SH_MODE_GRAINS;
			sh->Tsm = v->period;      /* hard re-anchor: no glide through junk */
			sh->launchCd = 0.0f;
			sh->phiRamp = 0.0f;       /* formant eases back over ~20ms */
		} else if(sh->noLockHops > 14) {
			sh->mode = SH_MODE_DRIFT; /* attack never resolved to a note */
		}
	} else if(sh->mode == SH_MODE_GRAINS) {
		if(sh->noLockHops > 6)
			sh->mode = SH_MODE_DRIFT;
	} else { /* DRIFT */
		if(sh->relockHops >= 2) {
			sh->mode = SH_MODE_GRAINS;
			sh->Tsm = v->period;
			sh->launchCd = 0.0f;
		}
	}

	if(gFormantActive && gFormantMode == 1 && sh->mode == SH_MODE_GRAINS)
		lpcAnalyze(v);
	else
		sh->lpcValid = 0;
}

/* per-sample onset: fires from voiceProcess, before shifterRun */
void shifterOnset(Voice *v, int s)
{
	Shifter *sh = &v->sh;
	sh->mode = SH_MODE_TRANS;
	sh->tapDelay = 4.0f;
	sh->tapFading = 0;
	sh->relockHops = 0;
	sh->phiRamp = 0.0f;
	/* fresh mark hunt from the onset: wide catch window */
	sh->pkVal = 0.0f;
	sh->searchIn = 0.0f;
	sh->searchLeft = 1.5f * sh->Tsm;
	sh->haveMark = 0;
	/* strum coherence: if a neighbour picked <10ms ago, exit together */
	unsigned int hold = gSampleStamp + (unsigned int)(0.010f * v->srate);
	for(int o = 0; o < NUM_STRINGS; o++) {
		if(o == s) continue;
		unsigned int dt = gSampleStamp - gOnsetStamp[o];
		if(dt < (unsigned int)(0.010f * v->srate)) {
			Shifter *so = &gV[o].sh;
			if(so->transHoldUntil > hold) hold = so->transHoldUntil;
		}
	}
	sh->transHoldUntil = hold;
	gOnsetStamp[s] = gSampleStamp;
	/* the old hop-rate detector's side effects, now at sample precision */
	v->attackBlank = ATTACK_BLANK_HOPS;
	v->tonalHops = 0;
}

static void shLaunch(Shifter *sh, Voice *v)
{
	float rho = v->beta;
	if(rho < 0.5f) rho = 0.5f;
	if(rho > 2.0f) rho = 2.0f;
	float S = sh->Tsm / rho;
	if(S < 24.0f) S = 24.0f;
	if(S > 2800.0f) S = 2800.0f;

	float phi = 1.0f;
	if(gFormantMode == 0)
		phi = 1.0f + (v->formant - 1.0f) * sh->phiRamp;

	/* source mark: newest, unless stale (anti robot-buzz: alternate) */
	float dc = sh->markDelay;
	if(sh->markUses > 2 && sh->haveMark >= 2) {
		sh->launchParity ^= 1;
		if(sh->launchParity) dc = sh->markPrevDelay;
	}
	sh->markUses++;

	/* causality: grain reads source [mark-S*phi, mark+S*phi]; the end must
	 * be written by the time it's read -> dc >= 3 + S*(phi-2). Too-new
	 * mark: step back whole periods (keeps grain phase). */
	float need = 3.0f + (phi > 2.0f ? S * (phi - 2.0f) : 0.0f);
	while(dc < need)
		dc += sh->Tsm;
	float d0 = dc + S * phi;
	if(d0 > (float)(BUF_MASK - 16))
		return;                    /* buffer can't hold it: skip one launch */

	ShGrain *g = &sh->gr[0];
	float best = -1.0f;
	for(int i = 0; i < SH_NGRAINS; i++) {
		if(!sh->gr[i].active) { g = &sh->gr[i]; best = 2.0f; break; }
		float prog = sh->gr[i].u / sh->gr[i].L;
		if(prog > best) { best = prog; g = &sh->gr[i]; }
	}
	g->d0 = d0;
	g->u = 0.0f;
	g->L = 2.0f * S;
	g->phi = phi;
	g->active = 1;

	sh->launchCd += S;
}

float shifterRun(Voice *v, float full)
{
	Shifter *sh = &v->sh;

	/* ---- pitch-mark picker (always running) ---- */
	float ml = sh->mkB0 * full + sh->mkZ1;
	sh->mkZ1 = sh->mkB1 * full - sh->mkA1 * ml + sh->mkZ2;
	sh->mkZ2 = sh->mkB2 * full - sh->mkA2 * ml;

	sh->markDelay += 1.0f;
	sh->markPrevDelay += 1.0f;
	sh->pkDelay += 1.0f;
	if(sh->searchIn > 0.0f) {
		sh->searchIn -= 1.0f;
	} else if(sh->searchLeft > 0.0f) {
		sh->searchLeft -= 1.0f;
		if(ml > sh->pkVal) { sh->pkVal = ml; sh->pkDelay = 0.0f; }
		if(sh->searchLeft <= 0.0f) {
			if(sh->pkVal > 1e-4f) {
				sh->markPrevDelay = sh->markDelay;
				sh->markDelay = sh->pkDelay;
				if(sh->haveMark < 2) sh->haveMark++;
				sh->markUses = 0;
				float in = 0.75f * sh->Tsm - sh->markDelay;
				sh->searchIn = in > 0.0f ? in : 0.0f;
				sh->searchLeft = 0.5f * sh->Tsm;
			} else {
				sh->searchIn = 0.0f;
				sh->searchLeft = 0.5f * sh->Tsm;
			}
			sh->pkVal = 0.0f;
		}
	}

	/* ---- period slew into the scheduler ---- */
	sh->Tsm += 0.0015f * (v->period - sh->Tsm);

	/* ---- formant ease-in after attacks (~20ms) ---- */
	if(sh->phiRamp < 1.0f) {
		sh->phiRamp += 1.0f / (0.020f * v->srate);
		if(sh->phiRamp > 1.0f) sh->phiRamp = 1.0f;
	}

	/* ---- grain leg ---- */
	float grains = 0.0f;
	if(sh->mode == SH_MODE_GRAINS && sh->haveMark) {
		if((sh->launchCd -= 1.0f) <= 0.0f)
			shLaunch(sh, v);
	}
	for(int i = 0; i < SH_NGRAINS; i++) {
		ShGrain *g = &sh->gr[i];
		if(!g->active) continue;
		float t = g->u / g->L;
		float w = shHann(t);
		const float *src = g->phi > 1.1f ? v->abuf : v->buf;
		grains += w * bufAtM(src, BUF_MASK, v->writePos, g->d0 + g->u * (1.0f - g->phi));
		g->u += 1.0f;
		if(g->u >= g->L) g->active = 0;
	}

	/* ---- tap leg (transient pass-through / no-lock drift splicer) ---- */
	float tap;
	if(sh->mode == SH_MODE_TRANS) {
		tap = bufAtM(v->buf, BUF_MASK, v->writePos, sh->tapDelay);
	} else {
		float rho = v->beta;
		if(rho < 0.5f) rho = 0.5f;
		if(rho > 2.0f) rho = 2.0f;
		float Tref = v->periodGood > 24.0f ? v->periodGood : v->p0;
		if(sh->mode == SH_MODE_DRIFT)
			sh->tapDelay += 1.0f - rho;
		float lo = 8.0f, hi = 2.5f * Tref + 256.0f;
		if(!sh->tapFading && (sh->tapDelay < lo || sh->tapDelay > hi)) {
			/* splice: jump a whole period (correlation-refined) back
			 * toward the middle of the window */
			float J = Tref;
			{	/* NCC refine +-4 samples on the full-rate buffer */
				float dir = sh->tapDelay < lo ? 1.0f : -1.0f;
				float base = sh->tapDelay + dir * J;
				int win = (int)(Tref < 300.0f ? Tref : 300.0f);
				float bestv = -2.0f, besto = 0.0f;
				for(int o = -4; o <= 4; o++) {
					float acf = 0.0f, e1 = 0.0f, e2 = 0.0f;
					for(int i = 0; i < win; i += 2) {
						float sa = bufAtM(v->buf, BUF_MASK, v->writePos, sh->tapDelay + (float)i);
						float sb = bufAtM(v->buf, BUF_MASK, v->writePos, base + (float)o + (float)i);
						acf += sa * sb; e1 += sa * sa; e2 += sb * sb;
					}
					float val = acf / sqrtf(e1 * e2 + 1e-12f);
					if(val > bestv) { bestv = val; besto = (float)o; }
				}
				sh->tapOld = sh->tapDelay;
				sh->tapDelay = base + besto;
			}
			sh->tapFading = 1;
			sh->tapFadePh = 0.0f;
			sh->tapFadeStep = 1.0f / (0.5f * Tref);
		}
		if(sh->tapFading) {
			float t2 = sh->tapFadePh;
			float gIn = sqrtf(t2), gOut = sqrtf(1.0f - t2);
			tap = gOut * bufAtM(v->buf, BUF_MASK, v->writePos, sh->tapOld)
			    + gIn * bufAtM(v->buf, BUF_MASK, v->writePos, sh->tapDelay);
			sh->tapFadePh += sh->tapFadeStep;
			if(sh->tapFadePh >= 1.0f) sh->tapFading = 0;
			if(sh->mode == SH_MODE_DRIFT)
				sh->tapOld += 1.0f - rho;
		} else {
			tap = bufAtM(v->buf, BUF_MASK, v->writePos, sh->tapDelay);
		}
	}

	/* ---- equal-power leg blend; ramp time ~2ms into tap, ~1 period out */
	float target = (sh->mode == SH_MODE_GRAINS && sh->haveMark) ? 1.0f : 0.0f;
	float step = target > sh->grainMix
		? 1.0f / (sh->Tsm > 96.0f ? sh->Tsm : 96.0f)
		: 1.0f / (0.002f * v->srate);
	if(sh->grainMix < target) {
		sh->grainMix += step;
		if(sh->grainMix > target) sh->grainMix = target;
	} else if(sh->grainMix > target) {
		sh->grainMix -= step;
		if(sh->grainMix < target) sh->grainMix = target;
	}
	float gG = sqrtf(sh->grainMix), gT = sqrtf(1.0f - sh->grainMix);
	return gG * grains + gT * tap;
}
```

**voiceProcess rewrite (dsp_core.cpp) — complete:**

```c
void voiceProcess(Voice *v, float in, int dryNeeded, float *wetOut, float *dryOut)
{
	(void)dryNeeded;
	float x = in - v->dcX1 + 0.995f * v->dcY1;
	v->dcX1 = in;
	v->dcY1 = x;
	float full = x;

	float ax = fabsf(full);
	v->env = ax > v->env ? ax : v->env * 0.9995f;

	/* per-sample onset detector: fast/slow envelope ratio. This is what
	 * makes attacks land in ~1ms instead of a hop later. */
	Shifter *sh = &v->sh;
	sh->envF = ax > sh->envF ? ax : sh->envF * 0.985f;
	sh->envS += 0.0019f * (ax - sh->envS);
	if(sh->onsetRefract > 0) sh->onsetRefract--;
	if(sh->onsetRefract == 0 && sh->envF > 2.5f * sh->envS + 0.004f) {
		shifterOnset(v, (int)(v - gV));
		sh->onsetRefract = (int)(0.030f * v->srate);
	}

	/* tracker front-end: the 2-biquad lowpass now feeds ONLY the decimated
	 * pitch buffer (anti-alias for the /4 decimation) - the audio path is
	 * full-band through the shifter. */
	float xt = x;
	for(int sec = 0; sec < 2; sec++) {
		float *c = v->lpCoef[sec];
		float *z = v->lpZ[sec];
		float y = c[0] * xt + z[0];
		z[0] = c[1] * xt - c[3] * y + z[1];
		z[1] = c[2] * xt - c[4] * y;
		xt = y;
	}
	v->buf[v->writePos & BUF_MASK] = full;
	sh->aaZ += sh->aaCoef * (full - sh->aaZ);
	v->abuf[v->writePos & FXBUF_MASK] = sh->aaZ;

	v->decimAcc += xt;
	if(++v->decimCount >= DECIM) {
		v->dbuf[v->dwritePos & DBUF_MASK] = v->decimAcc * (1.0f / DECIM);
		v->dwritePos++;
		v->decimAcc = 0.0f;
		v->decimCount = 0;
	}

	float p = shifterRun(v, full);

	/* AUTO EQ tilt (H90 trick), static per interval - unity at no shift */
	v->tiltLp += 0.16f * (p - v->tiltLp);
	p = v->tiltLp * v->tiltLoG + (p - v->tiltLp) * v->tiltHiG;

	if(gFormantActive && gFormantMode == 1)
		p = lpcProcess(v, p);

	*wetOut = p;
	*dryOut = full;
	v->writePos++;
}
```

**updateModulation surgery (dsp_core.cpp):** delete the `downAmt/upAmt` block, the `formantBlend` block, and the entire "Fresh pick? Blank the pitch tracker…" re-anchor block (lines ~388–409 old numbering) — keep only the `v->envPrev` update line from it (decay-tail freeze in the tracker still uses it). Add `shifterHop(v, s);` at the end. `updateLP()`: delete the `mult` line (`float mult = fabsf(v->formant…`), use `float fc = v->harmCut * v->srate / v->period;`.

**render.cpp:** in setup(): replace sd*/up/fsWet/fsDry seeding with `shifterInit(v);`; define `volatile int gFormantMode = 0; unsigned int gSampleStamp = 0; unsigned int gOnsetStamp[NUM_STRINGS] = {0};`; call `trackPitch(v, s)`; add `gSampleStamp++;` at the end of the per-frame loop. In pitch_tracker.cpp change the signature (veto body comes in Task 7). Note: `runFM` etc. read `v->fxbuf`? — verify with grep; if any effect reads fxbuf, point it at `v->buf` (it held the pitch-stage output which no longer exists as a tap point; effects run on `mixed` anyway).

- [ ] **Step 1:** Write hosttest tests (they fail to compile until the engine exists — that's the failing-test state):
```c
// test_pitch_accuracy: 110Hz saw, beta = 2^(-7/12), formant 1.0, 2s.
// Measure period over samples [1.0s, 2.0s] of wet out. Assert within 10 cents of 110*2^(-7/12).
// test_evenness: same signal, beta = 2^(-5/12): frameRms 10ms frames over [0.5s, 1.5s];
// assert max/min frame RMS < 1.5 dB. (Old engine measured >> this; this is the COLA guarantee.)
// test_single_attack: genBurst at 196Hz, beta = 0.5, wet peak in first 40ms
// <= 1.6 * input burst peak  (doubled transient would be ~2x).
// test_formant_decouple: beta=1, formant=1.3 GRAIN mode: output period within 10 cents of input;
// spectral centroid (DFT at k*f0, k=1..20) at least 15% above input centroid.
```
- [ ] **Step 2:** Implement shifter.cpp + integrations per code above. `make -C hosttest` green on all tests incl. passthrough.
- [ ] **Step 3:** Commit: "Unified pitch-synchronous shifter: one engine for pitch+formant, single-copy attacks".

### Task 5: Mix routing + LPC/GRAIN toggle + persistence

**Files:** Modify: `presets.cpp`, `trellis_ui.cpp` (FWah toggle site line ~149 calls applyMixRouting), `render.cpp` (no change beyond Task 4 defs)

- [ ] **Step 1:** presets.cpp: add
```c
void applyMixRouting(void)
{
	/* the engine is the wet leg: pitch owns the % when active; formant
	 * (incl. FORMANT-WAH) needs the engine at full wet to be heard */
	float wet = 0.0f;
	if(gPitchActive) wet = gPitchWetPct / 100.0f;
	else if(gFormantActive || gFWahOn) wet = 1.0f;
	gWetTarget = wet;
	gDryTarget = 1.0f - wet;
}
```
applyPitch(): keep beta math; replace its dry/wet lines with `applyMixRouting()`. applyFormant(): add `applyMixRouting()` at end. Call applyMixRouting() after both FWah toggle sites (presets.cpp case 13/14 area and trellis_ui.cpp:149).
- [ ] **Step 2:** PAD_FORMANT encoder case: mainClick toggles `gFormantMode` with `rt_printf("FORMANT mode: %s\n", gFormantMode ? "LPC" : "GRAIN");`.
- [ ] **Step 3:** PersistedState: append `int formantMode;` at struct end, save/load it (existing files keep loading: bump STATE_MAGIC to `0x48475332` /*HGS2*/ so stale layouts are rejected cleanly).
- [ ] **Step 4:** hosttest still green; commit.

### Task 6: lpc.cpp — true-envelope formant mode

**Files:** Create: `bela/hyperGuitar6chPan/lpc.cpp`; Test: `test_lpc_stability`, `test_lpc_formant`

Algorithm (complete): per hop on locked strings (called from shifterHop): autocorr r[0..16] over last LPC_WIN=512 samples of v->buf; lag window `r[k] *= gLagW[k]` (precomputed Gaussian, 60 Hz bandwidth expansion: `gLagW[k]=expf(-0.5f*powf(2*M_PI*60*k/sr,2)/2... use expf(-0.5f*sq((float)k*0.0086f))`) plus `r[0] *= 1.001f`; Levinson-Durbin → k[i] (reflection) + prediction error E. Reject frame (keep previous) if any |k[i]| > 0.998. Envelope warp: evaluate `P[m] = |A(e^{jw_m})|^2` at LPC_EPTS=33 freqs `w_m = pi*m/32` (direct complex sum over 16 coefs); envelope `Env[m] = 1/sqrt(P[m])`; warped `EnvW[m] = Env[clamp(m/phiEff)]` with linear interp, hold at edges, `phiEff = 1 + (v->formant-1)*sh->phiRamp`; energy-normalize EnvW to Env; new autocorr `rw[k] = sum_m W[m]*EnvW[m]^2*cos(w_m*k)` (W = trapezoid: 0.5 at ends); Levinson → kr[i]. Gain g = sqrt(E) matching residual energy. Store targets in sh->lpcK/lpcKr/lpcGain, sh->lpcValid=1.

Per sample `lpcProcess`: slew `lpcKW[i] += 0.002f*(lpcK[i]-lpcKW[i])` (same for Kr, gain); lattice analysis (whiten) with lpcKW over state lpcZf, then lattice synthesis (re-color) with lpcKrW over lpcZg; when !lpcValid, bypass (return x) with a 5 ms crossfade.

- [ ] **Step 1:** tests: `test_lpc_stability` — white noise 20 s through beta=1/formant=1.4/LPC mode: assert no NaN, |out| bounded < 8. `test_lpc_formant` — saw 110 Hz, formant 1.4 LPC: pitch unchanged (±10 cents), spectral centroid raised ≥ 12%.
- [ ] **Step 2:** Implement, tests green, commit.

### Task 7: Hex leverage — octave veto + strum coherence

**Files:** Modify: `pitch_tracker.cpp`

- [ ] **Step 1:** In trackPitch(v, s), after the refined period p is computed and before `v->periodGood = p;`: cross-string bleed veto —
```c
	/* hex bleed veto: a quiet string reading exactly 2x the period of a
	 * LOUD neighbour is almost certainly pickup crosstalk reading the
	 * neighbour's fundamental as its own octave-low. Real playing rarely
	 * has this string this quiet while that confident. */
	for(int o = s - 1; o <= s + 1; o += 2) {
		if(o < 0 || o >= NUM_STRINGS) continue;
		Voice *w = &gV[o];
		if(w->env > 3.0f * v->env && best < 0.96f
		   && fabsf(p - 2.0f * w->period) < 0.04f * w->period)
			return; /* keep last period; don't feed the shifter bleed */
	}
```
(strum coherence already landed in shifterOnset, Task 4).
- [ ] **Step 2:** hosttest green (single-voice tests unaffected), commit.

### Task 8: Deploy + bench verification

- [ ] **Step 1:** `./bela/deploy.sh` — clean build on the board, supervisor relaunch, no xruns.
- [ ] **Step 2:** Report CPU % vs the 47% baseline.
- [ ] **Step 3:** Bench A/B checklist for the user (spec §5): attack loudness/clarity on hard picking; evenness at −12/−7/−5/+12 and formant 0.75/1.30; GRAIN↔LPC click A/B; strum isolation; mute/scratch behavior. **User's ear-approval gates calling this done** (their instruction: replace in active presets after approval — presets already carry identical values, so approval = keeping the deploy).
- [ ] **Step 4:** Final commit of any bench-driven tuning.

## Self-review notes

- Spec coverage: 3.1→T4, 3.2→T4, 3.3→T4 (onset/tap/φ-ramp/relock), 3.4→T4+T7, 3.5→T4 (drift mode), 3.6→T4 (sqrt equal-power legs; correlation-refined splices), 3.7→T5+T6, 3.8→T3+T4 deletions, 3.9→T5, 3.10→no code change needed. Auto-EQ retained in voiceProcess.
- Types/signatures consistent: shifterInit/shifterHop/shifterRun/shifterOnset, lpcAnalyze/lpcProcess, applyMixRouting, trackPitch(v,s) — declared in T3, used in T4–T7.
- Known deliberate simplifications: fallback splicer skips formant on unpitched material (spec-consistent); grain↔tap re-entry not phase-aligned (equal-power fade + post-attack masking; bench item).
