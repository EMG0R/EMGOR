/*
 * pitch_tracker.cpp - per-string pitch tracker, v2 (2026-09-24).
 *
 * v1 (NSDF over a fixed 2-open-string-period window, 6-hop attack blank,
 * 2-hop consensus) is archived verbatim at
 *     bela/archive/pitch_tracker_v1.cpp.txt
 * Restore it by copying that file over this one. Design note with sources
 * and before/after measurements:
 *     docs/pitch-tracker-overhaul.md
 * Offline harness that produced every number quoted below:
 *     hosttest/pitchbench/  (make && make old; ./pb_new all -23 add)
 *
 * Same interface as v1: trackPitch(v, s) once per HOP from render.cpp,
 * writing v->period / periodGood / pitchConf / attackBlank / dbgGate /
 * dbgE0 with the same meanings, so the shifter, synth and effects need no
 * change. All new state is file-static.
 *
 * WHAT A GUITAR-TO-MIDI CONVERTER DOES THAT v1 DID NOT
 *
 *  1. The pick is a TIME REFERENCE. v1 blanked a fixed 6 hops (17 ms) after
 *     every onset and then correlated a window that still held the PREVIOUS
 *     note, so a new note paid blank + window flush + 2 hops of consensus:
 *     22-50 ms. v2 only correlates data from after the pick. The pick is
 *     located retrospectively (shifterOnset can fire 10-16 ms late on the
 *     low strings, see localizeOnset), and a lag is evaluated as soon as
 *     0.75 of its own period of post-pick data sits behind it. High notes
 *     resolve in ~2 of their own short periods, low notes in ~2 of theirs:
 *     the practical floor for hex trackers (Derrien, DAFx-14: 1/f0 is the
 *     absolute floor; Reboursiere et al., NIME 2010: ~2 open-string periods).
 *
 *  2. The known string range is a hard prior (open string with TUNE_MARGIN
 *     of flat headroom, up to MAX_FRET_RATIO). The first key maximum wins
 *     (McLeod), so an octave-low 2P peak cannot beat the real P peak. Three
 *     guards cover the other direction, which a bridge pickup makes likely
 *     because it starves the fundamental: early reads need a second hop
 *     unless 2x the lag has been checked; a clearly higher NSDF at 2-4x the
 *     lag promotes to that lag (harmonic lock-on); and v1's half-lag guard
 *     stays.
 *
 *  3. The window follows the NOTE: at most 2 periods of the candidate lag
 *     while a note is new or moving, 4 once it has held still for 8 hops.
 *     A legato tripwire and a bend detector drop it back to 2. v1 always
 *     used 2 OPEN-string periods, so a 12th-fret note carried 4 of its own
 *     periods of history.
 *
 *  4. The analysis signal is the tracker's OWN fixed lowpass (per string,
 *     1.3x the highest note, floor 1 kHz), not v->dbuf. dbuf's lowpass
 *     follows the tracked pitch and glides after every note change, and a
 *     filter whose group delay changes during the window bends the measured
 *     period (+40..55 cents transients). Its harmCut-10 cutoff also aliased
 *     past the decimator on the top strings and capped clarity below the
 *     bar for 100s of ms (v1: high C 10th fret, 506 ms).
 *
 *  5. Crosstalk is handled with a LEVEL MODEL, not an absolute gate. Each
 *     string's ~10 ms power is compared with the bleed its neighbours would
 *     put on it at a pessimistic coupling (-17 dB adjacent, -29 dB next but
 *     one; the rig measured -23/-37). A string at or under 2x its predicted
 *     bleed never starts a note. A string within 9 dB of it whose candidate
 *     equals a clearly louder neighbour's pitch is that neighbour. Bleed is
 *     reported like silence (dbgGate 2, conf 0, period held): the string has
 *     no note of its own, and the shifter's silent-string freeze is the
 *     designed response to that.
 *
 *  6. Note changes SNAP after an onset and need CONSENSUS without one.
 *     After an onset the first estimate that clears 0.90 is the note (0.75
 *     with two agreeing hops, for a quiet string whose clarity is capped by
 *     a neighbour's bleed). Without one (hammer-on, pull-off, missed onset)
 *     a jump needs 2 agreeing hops, and 3 for an exact octave or a weak read.
 *
 *  7. attackBlank ends when the tracker has a trustworthy post-onset
 *     estimate, not after a fixed 6 hops. It still counts down, so a note
 *     that never resolves behaves as it did under v1.
 *
 * Kept from v1 because each was tuned on the device: the note-off edge
 * detector (releaseBlank), the hysteretic GATE_RMS sustain gate (measured
 * on v->dbuf exactly as before), the decay-tail freeze, and a full-rate
 * refinement, now on the filtered ring with a +-1.8% search instead of v1's
 * +-3 samples.
 *
 * REAL-TIME: no allocation, no locks, no syscalls, no recursion. State is
 * file-static: 6 x ~12 KB of rings, plus ~16 KB of shared scratch.
 * trackPitch runs to completion on the audio thread and is never
 * re-entered. Host op count over the harness scenes: mean 3.9k MACs per
 * call against v1's 9.1k, worst call 51k against 65k. The front end adds 2
 * biquads per sample per string. Silent and bleed-only strings exit before
 * the sweep, as in v1.
 */
#include "hyperguitar.h"
#include <cmath>

#if DECIM != 4
#error "pitch_tracker.cpp: the analysis ring maps raw 4j+3 -> decimated j"
#endif

/* ------------------------------ tunables ------------------------------ */

/* full-rate samples skipped after the located pick: the scrape itself plus
 * the analysis lowpass's group delay (~0.4 ms at 1 kHz) */
#define PT_SKIP        48
/* a lag is evaluable once this many periods of post-onset data sit behind it */
#define PT_WMIN_FRAC   0.75f
/* steady-state window: at most this many periods of the candidate lag */
#define PT_WCAP_FRAC   2.0f
/* ...and once a note has held still this many hops, the windows grow to
 * PT_WCAP_STEADY periods (coarse, capped at corrWin) and 3 periods
 * (refine): steady-state jitter under neighbour bleed is what the shifter
 * hears, and a longer window averages the beat against the bleed */
#define PT_STEADY_HOPS 8
#define PT_WCAP_STEADY 4.0f
/* confidence needed to take the first post-onset estimate as the note */
#define PT_ACCEPT      0.90f
/* ...or this, when two consecutive hops agree (bleed-capped clarity) */
#define PT_ACCEPT_LO   0.75f
/* bleed model: POWER coupling from a neighbour at distance 1 and 2 */
#define PT_BLEED_C1    0.020f      /* -17 dB, 6 dB more than measured */
#define PT_BLEED_C2    0.00126f    /* -29 dB */
/* below this multiple of predicted bleed, a string never starts a note */
#define PT_BLEED_HARD  2.0f        /* +3 dB */
/* below this multiple, a candidate at a clearly louder neighbour's exact
 * pitch is treated as that neighbour's bleed. Unison only: octave
 * relations are everywhere in real chords (power chords, open shapes) and
 * v1's x2 rule rejected real notes in the strum test. */
#define PT_BLEED_SOFT  8.0f        /* +9 dB */
#define PT_BLEED_PTOL  0.02f       /* pitch-match tolerance, relative */

#define PT_MAXC        1024        /* >= lagMax + corrWin + 2, in decimated samples */

/* the tracker's OWN analysis front end: a FIXED 4th-order Butterworth
 * lowpass per string on the full-band input, kept at full rate (for the
 * refinement) and decimated by DECIM (for the sweep). See header, 4.
 * Cutoff: 1.3x the string's highest note (4.5x open), floor 1 kHz, i.e.
 * 1.0 / 1.0 / 1.0 / 1.0 / 1.15 / 1.53 kHz for C-F-Bb-Eb-G-C. Low enough
 * that the NSDF peak is several decimated lags wide (a 2 kHz cutoff made
 * it ~1 lag wide and the step-2 grid could miss it entirely), high enough
 * that the open strings keep >= 5 partials. */
#define PT_AA_FLOOR    1000.0f
#define PT_AA_TOPMUL   1.3f
#define PT_FBUF        2048        /* full-rate filtered ring, > 620 + lagMax*4 + 8 */
#define PT_FMASK       (PT_FBUF - 1)
#define PT_TBUF        1024        /* decimated filtered ring */
#define PT_TMASK       (PT_TBUF - 1)

typedef struct {
	int init;
	unsigned int rd;          /* v->buf read cursor for the front end */
	float pw;                 /* ~10 ms mean-square of the full-band input */
	float z[2][2];            /* anti-alias biquad states */
	float fbuf[PT_FBUF];      /* filtered, full rate, indexed by raw position */
	float tbuf[PT_TBUF];      /* filtered, decimated: raw 4j+3 -> j */
	unsigned int onsetSeen;   /* last gOnsetStamp[s] consumed */
	unsigned int onsetPos;    /* v->writePos-domain index of that onset */
	int haveOnset;
	int state;                /* PT_IDLE / PT_ACQ / PT_LOCK */
	int agree;                /* consecutive agreeing hops for a jump */
	int steady;               /* LOCK hops since the last onset/jump/motion */
	float motion;             /* signed smoothed period change per hop, relative */
	float cand;               /* the candidate those hops agree on */
} TrkState;

#define PT_IDLE 0
#define PT_ACQ  1
#define PT_LOCK 2

static TrkState gT[NUM_STRINGS];
static float gAA[NUM_STRINGS][2][5];   /* per-string fixed coefficients */
/* level every string publishes for its neighbours' bleed test */
static float gPw[NUM_STRINGS];

#ifdef PT_OPCOUNT
long gPtMacs = 0;
#define PT_COUNT(n) (gPtMacs += (long)(n))
#else
#define PT_COUNT(n) do { } while(0)
#endif

/* shared scratch: trackPitch runs to completion on one audio thread and is
 * never re-entered, so the six strings can share it (as v1's stack arrays
 * effectively did, minus the 4 KB of stack per call). */
static float gX[PT_MAXC];
static double gPre[PT_MAXC + 1];
static float gNs[PT_MAXC];

/* normalized correlation of the newest W samples against the same span tau
 * earlier. x has n samples, pre[] is its prefix sum of squares. */
static inline float nsdfW(const float *x, const double *pre, int n, int tau, int W)
{
	const float *a = x + n - W;
	const float *b = a - tau;
	float acf = 0.0f;
	for(int i = 0; i < W; i++)
		acf += a[i] * b[i];
	PT_COUNT(W);
	double e0 = pre[n] - pre[n - W];
	double el = pre[n - tau] - pre[n - tau - W];
	return (float)(acf / sqrt(e0 * el + 1e-18));
}

/* window cap for this call, in periods of the lag: PT_WCAP_FRAC while a
 * note is being acquired or has just moved, PT_WCAP_STEADY once it has sat
 * still for PT_STEADY_HOPS (set once per trackPitch call; single thread) */
static float gCapFrac = PT_WCAP_FRAC;

static inline int winFor(int tau, int n, int corrWin)
{
	int W = n - tau;
	int cap = (int)(gCapFrac * (float)tau);
	if(cap < 24) cap = 24;
	if(cap > corrWin) cap = corrWin;
	if(W > cap) W = cap;
	return W;
}

static inline int evaluable(int tau, int W)
{
	return W >= 12 && (float)W >= PT_WMIN_FRAC * (float)tau;
}

static void aaInit(int s, float sr, float fOpen)
{
	static const float Q[2] = { 0.54119610f, 1.30656296f };
	float fc = PT_AA_TOPMUL * MAX_FRET_RATIO * fOpen;
	if(fc < PT_AA_FLOOR) fc = PT_AA_FLOOR;
	if(fc > 0.2f * sr) fc = 0.2f * sr;
	float w0 = 2.0f * (float)M_PI * fc / sr;
	float cw = cosf(w0), sw = sinf(w0);
	for(int sec = 0; sec < 2; sec++) {
		float alpha = sw / (2.0f * Q[sec]);
		float a0inv = 1.0f / (1.0f + alpha);
		float b1 = (1.0f - cw) * a0inv;
		gAA[s][sec][0] = 0.5f * b1;
		gAA[s][sec][1] = b1;
		gAA[s][sec][2] = 0.5f * b1;
		gAA[s][sec][3] = -2.0f * cw * a0inv;
		gAA[s][sec][4] = (1.0f - alpha) * a0inv;
	}
}

/*
 * Front end: consume the full-rate samples written since the last hop.
 * ~12 multiply-adds per sample. Runs for every string, every hop, so the
 * level is valid for the neighbours' bleed tests even while this string
 * is silent, and the analysis rings are always current.
 *
 * WHY A FIXED FILTER and not v->dbuf (header, 4): dbuf's lowpass follows
 * the TRACKED period (harmCut x f0, re-aimed every hop with a 0.25 glide).
 * (a) After a note change the cutoff glides for several hops, and a
 * lowpass whose group delay changes during the window stretches or
 * squeezes the waveform: measured +40..+55 cents of transient error on a
 * low-string note change. (b) harmCut 10 on the top strings puts the
 * cutoff far above the decimated Nyquist (5.5 kHz), and a box-car
 * decimator does not stop it: aliased, inharmonic upper partials of a
 * fretted plain string capped clarity at ~0.88, under the confidence bar,
 * for hundreds of ms (high C at the 10th fret: 506 ms in v1).
 */
static void frontEnd(Voice *v, TrkState *t, int s)
{
	unsigned int n = v->writePos - t->rd;
	if(n > 4u * HOP) {                  /* first call / resync */
		t->rd = v->writePos - 4u * HOP;
		n = 4u * HOP;
	}
	const float c = 1.0f / (0.010f * (v->srate > 0.0f ? v->srate : 44100.0f));
	float pw = t->pw;
	for(unsigned int i = 0; i < n; i++) {
		unsigned int idx = t->rd + i;
		float x = v->buf[idx & BUF_MASK];
		pw += c * (x * x - pw);
		float y = x;
		for(int sec = 0; sec < 2; sec++) {
			const float *cf = gAA[s][sec];
			float *z = t->z[sec];
			float o = cf[0] * y + z[0];
			z[0] = cf[1] * y - cf[3] * o + z[1];
			z[1] = cf[2] * y - cf[4] * o;
			y = o;
		}
		t->fbuf[idx & PT_FMASK] = y;
		if((idx & 3u) == 3u)
			t->tbuf[(idx >> 2) & PT_TMASK] = y;
	}
	if(pw < 1e-20f) pw = 0.0f;           /* no denormals on a dead channel */
	t->pw = pw;
	t->rd = v->writePos;
}

/*
 * RETROSPECTIVE ONSET LOCALIZATION. shifterOnset's detector uses time
 * constants scaled to the OPEN string's period (fast env ~0.3 periods), so
 * on the low strings it can fire 10-16 ms after the pick (measured: low C
 * open, +16.3 ms). Every one of those ms was new-note data the window threw
 * away. The pick itself is a broadband burst, so: 32-sample block energies
 * of the first difference over the 25 ms before the stamp (and whatever has
 * been written since), find the loudest block, walk back while the energy
 * stays above a fifth of it. Never later than the stamp. Runs once per
 * onset: at most ~1500 samples of two flops.
 */
#define PT_LOC_BACK  1088      /* 34 blocks of 32 = 24.7 ms */
static unsigned int localizeOnset(const Voice *v, unsigned int stampPos)
{
	unsigned int end = v->writePos;
	unsigned int start = stampPos - PT_LOC_BACK;
	int nb = (int)((end - start) / 32u);
	if(nb < 2 || nb > 64)
		return stampPos;
	float E[64];
	int m = 0;
	for(int b = 0; b < nb; b++) {
		float e = 0.0f;
		unsigned int i0 = start + 32u * (unsigned int)b;
		float prev = v->buf[(i0 - 1u) & BUF_MASK];
		for(int i = 0; i < 32; i++) {
			float x = v->buf[(i0 + (unsigned int)i) & BUF_MASK];
			float d = x - prev;
			prev = x;
			e += d * d;
		}
		E[b] = e;
		if(e > E[m]) m = b;
	}
	int b = m;
	while(b > 0 && E[b - 1] > 0.2f * E[m])
		b--;
	unsigned int pos = start + 32u * (unsigned int)b;
	/* earlier than the stamp only: (stampPos - pos) as signed distance */
	if((int)(stampPos - pos) > 0)
		return pos;
	return stampPos;
}

/* bleed power the neighbours would put on string s */
static float predictedBleed(int s)
{
	float b = 0.0f;
	for(int o = s - 2; o <= s + 2; o++) {
		if(o < 0 || o >= NUM_STRINGS || o == s)
			continue;
		b += (o == s - 1 || o == s + 1 ? PT_BLEED_C1 : PT_BLEED_C2) * gPw[o];
	}
	return b;
}

/* is period p a clearly LOUDER (>= +3 dB) locked neighbour's own period? */
static int matchesLouderNeighbour(int s, float p, float myPw)
{
	for(int o = s - 2; o <= s + 2; o++) {
		if(o < 0 || o >= NUM_STRINGS || o == s)
			continue;
		if(gPw[o] <= 2.0f * myPw || gT[o].state != PT_LOCK)
			continue;
		float q = gV[o].period;
		if(fabsf(p - q) < PT_BLEED_PTOL * q)
			return 1;
	}
	return 0;
}

/* hold: this hop says nothing about the pitch */
static inline void holdSilent(Voice *v, TrkState *t, int gate)
{
	v->pitchConf = 0.0f;
	v->dbgGate = gate;
	t->state = PT_IDLE;
	t->agree = 0;
	v->candidateAgree = 0;
}

void trackPitch(Voice *v, int s)
{
	TrkState *t = &gT[s];
	if(!t->init) {
		t->init = 1;
		float sr = v->srate > 0.0f ? v->srate : 44100.0f;
		aaInit(s, sr, sr / v->p0);
		t->rd = v->writePos;
		t->onsetSeen = gOnsetStamp[s];
		t->state = PT_IDLE;
	}
	frontEnd(v, t, s);
	gPw[s] = t->pw;

	/* ---- onset bookkeeping: shifterOnset stamps gOnsetStamp[s] at the
	 * exact sample of the pick. Convert it into this string's buffer
	 * position so the analysis can start AFTER it. ---- */
	if(gOnsetStamp[s] != t->onsetSeen) {
		t->onsetSeen = gOnsetStamp[s];
		unsigned int age = gSampleStamp - gOnsetStamp[s];
		t->onsetPos = localizeOnset(v, v->writePos - age);
		t->haveOnset = 1;
		t->state = PT_ACQ;
		t->steady = 0;
		t->agree = 0;
		v->candidateAgree = 0;
	}

	/*
	 * NOTE-OFF EDGE DETECTOR (unchanged from v1): releasing a fretting
	 * finger or palm-muting damps the string far faster than it can decay
	 * on its own. 8 hops, re-armed while the damping continues.
	 */
	{
		float envOld = v->envHist[v->envHistIdx];
		v->envHist[v->envHistIdx] = v->env;
		v->envHistIdx = (v->envHistIdx + 1) & 3;
		if(v->attackBlank == 0) {
			if(envOld > 0.003f && v->env < 0.35f * envOld)
				v->releaseBlank = 8;
			if(v->envLastHop > 0.003f && v->env < 0.5f * v->envLastHop)
				v->releaseBlank = 8;
		}
		v->envLastHop = v->env;
	}
	v->dbgGate = 0;
	if(v->releaseBlank > 0) {
		v->releaseBlank--;
		v->pitchConf = 0.0f;
		v->dbgGate = 1;
		t->state = PT_IDLE;
		t->agree = 0;
		return;
	}

	/*
	 * SILENCE GATE - v1's, byte-for-byte in what it measures (v->dbuf over
	 * the newest corrWin samples, the calibration the device traces were
	 * taken against) and in its sustain hysteresis. A string that is
	 * acquiring after an onset stays in ACQ through it.
	 */
	int corrWin = v->corrWin;
	int lagMin = v->lagMin, lagMax = v->lagMax;
	{
		float e0 = 0.0f;
		for(int i = 0; i < corrWin; i++) {
			float x = v->dbuf[(v->dwritePos - corrWin + i) & DBUF_MASK];
			e0 += x * x;
		}
		v->dbgE0 = e0 / (float)corrWin;
		float gateRms = (t->state == PT_LOCK && v->pitchConf > NSDF_CONF)
				? GATE_RMS * (1.0f / 3.0f) : GATE_RMS;
		if(v->dbgE0 < gateRms * gateRms) {
			v->pitchConf = 0.0f;
			v->dbgGate = 2;
			if(t->state != PT_ACQ) {
				t->state = PT_IDLE;
				t->agree = 0;
				v->candidateAgree = 0;
			}
			return;
		}
	}

	int isSteady = t->state == PT_LOCK && t->steady >= PT_STEADY_HOPS;
	gCapFrac = isSteady ? PT_WCAP_STEADY : PT_WCAP_FRAC;

	/* ---- how much post-onset data exists, in decimated samples ---- */
	int nMax = lagMax + corrWin + 1;
	if(nMax > PT_MAXC) nMax = PT_MAXC;
	int count = (int)(v->writePos >> 2);     /* tbuf[j] complete for j < count */
	int n = nMax;
	if(t->haveOnset) {
		unsigned int since = v->writePos - t->onsetPos;
		if(since < (unsigned int)(DECIM * (nMax + 8)) + PT_SKIP) {
			/* decimated samples since onset + skip (wrap-safe: unsigned
			 * distance, never absolute positions) */
			int d = since > (unsigned int)PT_SKIP
					? (int)((since - (unsigned int)PT_SKIP) >> 2) : 0;
			if(d < n) n = d;
		} else {
			t->haveOnset = 0;             /* old enough: full window */
		}
	}

	/* newest n decimated samples, contiguous, plus prefix energies */
	for(int i = 0; i < n; i++)
		gX[i] = t->tbuf[(unsigned int)(count - n + i) & PT_TMASK];
	gPre[0] = 0.0;
	for(int i = 0; i < n; i++)
		gPre[i + 1] = gPre[i] + (double)gX[i] * (double)gX[i];

	/*
	 * CROSSTALK, HARD BAND: at or under the bleed the neighbours are
	 * predicted to put here, this string has no note of its own. A string
	 * that is mid-note (LOCK) is allowed to decay down to the bleed before
	 * it is released, so a quiet top string ringing beside a loud wound one
	 * is not cut off early.
	 */
	float bleed = predictedBleed(s);
	int underBleed = t->pw < PT_BLEED_HARD * bleed;
	if(underBleed && t->state != PT_LOCK) {
		if(v->attackBlank > 0) v->attackBlank--;
		holdSilent(v, t, 2);
		return;
	}

	/* not enough post-onset data for even the shortest lag: wait */
	if(n < lagMin + 12) {
		v->pitchConf = 0.0f;
		v->dbgGate = 6;
		if(v->attackBlank > 0) v->attackBlank--;
		return;
	}

	/* LEGATO TRIPWIRE: in steady mode the long window would hold on to
	 * the old note through a hammer-on. The newest 2 periods at the
	 * current lag say at once whether the note is still there. */
	if(isSteady) {
		int cl = (int)(v->period / (float)DECIM + 0.5f);
		if(cl >= lagMin && cl <= lagMax) {
			int Wc = (int)(PT_WCAP_FRAC * (float)cl);
			if(Wc + cl + 1 <= n && nsdfW(gX, gPre, n, cl, Wc) < 0.6f) {
				isSteady = 0;
				t->steady = 0;
				gCapFrac = PT_WCAP_FRAC;
			}
		}
	}

	/* ---- coarse sweep over the evaluable part of the string's range ---- */
	float best = -2.0f;
	int hiEval = lagMin - 1;
	for(int lag = lagMin; lag <= lagMax; lag += (lag < 20 ? 1 : 2)) {
		int W = winFor(lag, n, corrWin);
		if(!evaluable(lag, W))
			break;                        /* longer lags need more data */
		float val = nsdfW(gX, gPre, n, lag, W);
		gNs[lag] = val;
		hiEval = lag;
		if(val > best) best = val;
	}
	if(hiEval < lagMin + 2) {
		v->pitchConf = 0.0f;
		v->dbgGate = 6;
		if(v->attackBlank > 0) v->attackBlank--;
		return;
	}
	if(best < NSDF_CONF) {
		v->pitchConf = best;              /* aperiodic: tap / scrape / noise */
		v->dbgGate = 3;
		if(v->attackBlank > 0) v->attackBlank--;
		return;
	}

	/*
	 * PEAKS, then the first key maximum (McLeod). Every coarse local max
	 * worth considering has its skipped odd neighbours filled in FIRST, so
	 * the key-max test compares true peak heights, not whatever the step-2
	 * grid happened to land on. A local max on the last evaluated lag only
	 * counts if the string's range really ends there.
	 */
	int pkLag[24];
	float pkVal[24];
	int npk = 0;
	{
		float floorV = 0.5f * best;
		int prev = -1, cur = -1;
		for(int lag = lagMin; ; lag += (lag < 20 ? 1 : 2)) {
			int end = lag > hiEval;
			if(cur >= 0 && gNs[cur] >= floorV
			   && (prev < 0 || gNs[cur] >= gNs[prev])
			   && (end ? cur >= lagMax - 1 : gNs[cur] >= gNs[lag])) {
				int bl = cur;
				float bv = gNs[cur];
				if(cur >= 20) {
					for(int l2 = cur - 1; l2 <= cur + 1; l2 += 2) {
						if(l2 < lagMin || l2 > lagMax)
							continue;
						int W = winFor(l2, n, corrWin);
						if(!evaluable(l2, W))
							continue;
						float vv = nsdfW(gX, gPre, n, l2, W);
						gNs[l2] = vv;
						if(vv > bv) { bv = vv; bl = l2; }
					}
				}
				if(npk < 24) { pkLag[npk] = bl; pkVal[npk] = bv; npk++; }
				if(bv > best) best = bv;
			}
			if(end)
				break;
			prev = cur;
			cur = lag;
		}
	}
	int bestLag = -1;
	for(int k = 0; k < npk; k++) {
		if(pkVal[k] >= FIRST_PEAK * best) {
			bestLag = pkLag[k];
			best = pkVal[k];
			break;
		}
	}
	if(bestLag < 0) {
		v->pitchConf = 0.0f;
		v->dbgGate = 4;
		if(v->attackBlank > 0) v->attackBlank--;
		return;
	}

	/* HARMONIC LOCK-ON GUARD. A bridge pickup starves the fundamental, so
	 * during a noisy attack an upper partial alone can pass the key-max
	 * bar (measured: the 4th partial of an open Eb, 629 Hz, conf 0.906,
	 * with a 3x pick scrape). For a truly tau-periodic signal the NSDF at
	 * 2, 3, 4 tau can only be LOWER than at tau (decay, inharmonicity);
	 * if a multiple is clearly HIGHER, that multiple is the period. */
	{
		int promoted = bestLag;
		float pv = best;
		for(int k = 2; k <= 4; k++) {
			int ml = k * bestLag;
			if(ml > hiEval || ml > lagMax)
				break;
			float mv = -2.0f;
			for(int l2 = ml - 1; l2 <= ml + 1; l2++) {
				if(l2 < lagMin || l2 > lagMax)
					continue;
				int W = winFor(l2, n, corrWin);
				if(!evaluable(l2, W))
					continue;
				float vv = nsdfW(gX, gPre, n, l2, W);
				if(vv > mv) { mv = vv; if(mv > pv + 0.05f) promoted = l2; }
			}
			if(promoted != bestLag) {
				best = mv;
				break;
			}
		}
		bestLag = promoted;
	}

	/* anti-octave-low guard (v1): if half the lag correlates nearly as
	 * well, the note is an octave higher */
	{
		int half = bestLag / 2;
		if(half >= lagMin) {
			float hv = -2.0f;
			int hb = half;
			for(int k = half - 1; k <= half + 1; k++) {
				if(k < lagMin || k > lagMax)
					continue;
				int W = winFor(k, n, corrWin);
				if(!evaluable(k, W))
					continue;
				float val = nsdfW(gX, gPre, n, k, W);
				if(val > hv) { hv = val; hb = k; }
			}
			if(hv > 0.92f * best) {
				bestLag = hb;
				best = hv;
			}
		}
	}

	/* parabolic interpolation on the decimated grid */
	float lagF = (float)bestLag;
	if(bestLag > lagMin && bestLag < lagMax) {
		int Wm = winFor(bestLag + 1, n, corrWin);
		if(evaluable(bestLag + 1, Wm)) {
			float ym = nsdfW(gX, gPre, n, bestLag - 1, winFor(bestLag - 1, n, corrWin));
			float yp = nsdfW(gX, gPre, n, bestLag + 1, Wm);
			float denom = ym - 2.0f * best + yp;
			if(fabsf(denom) > 1e-9f) {
				float off = 0.5f * (ym - yp) / denom;
				if(off > 0.5f) off = 0.5f;
				if(off < -0.5f) off = -0.5f;
				lagF += off;
			}
		}
	}

	float p = lagF * (float)DECIM;
	float pMin = v->p0 / MAX_FRET_RATIO;
	float pMax = v->p0 * TUNE_MARGIN;
	if(p < pMin) p = pMin;
	if(p > pMax) p = pMax;

	/*
	 * FULL-RATE REFINEMENT on the filtered full-rate ring. v1 searched a
	 * fixed +-3 samples around the coarse lag on the raw full-band ring and
	 * silently gave up when the answer sat outside that (a 1% error on a
	 * low note is 7 samples). Here: a +-1.8% grid, then halving steps down
	 * to one sample, then a parabola. The window never reaches back past
	 * the onset.
	 */
	{
		int ip = (int)roundf(p);
		int win = isSteady ? (int)(3.0f * p) : (int)(1.2f * p);
		if(win > (isSteady ? 1000 : 620)) win = isSteady ? 1000 : 620;
		if(t->haveOnset) {
			int avail = (int)(v->writePos - t->onsetPos) - PT_SKIP - ip - 16;
			if(win > avail) win = avail;
		}
		if(win >= (int)(0.6f * p) && win >= 16) {
			const float *f = t->fbuf;
			unsigned int wp = v->writePos;
			float re0 = 0.0f;
			for(int i = 0; i < win; i++) {
				float sm = f[(wp - 1 - i) & PT_FMASK];
				re0 += sm * sm;
			}
			PT_COUNT(win);
			int h = (int)(0.006f * p);
			if(h < 1) h = 1;
			int cLag[20];
			float cVal[20];
			int nc = 0;
			/* evaluate (memoised) the normalized correlation at lag l */
#define PT_REF(l, out) do { \
				int _l = (l); float _v = -2.0f; int _k; \
				for(_k = 0; _k < nc; _k++) if(cLag[_k] == _l) { _v = cVal[_k]; break; } \
				if(_k == nc) { \
					float _a = 0.0f, _e = 0.0f; \
					for(int _i = 0; _i < win; _i++) { \
						float _sa = f[(wp - 1 - _i) & PT_FMASK]; \
						float _sb = f[(wp - 1 - _i - _l) & PT_FMASK]; \
						_a += _sa * _sb; _e += _sb * _sb; \
					} \
					PT_COUNT(2 * win); \
					_v = _a / sqrtf(re0 * _e + 1e-12f); \
					if(nc < 20) { cLag[nc] = _l; cVal[nc] = _v; nc++; } \
				} \
				(out) = _v; \
			} while(0)
			int c = ip;
			float cv;
			PT_REF(c, cv);
			for(int j = -3; j <= 3; j++) {
				if(j == 0) continue;
				float vv;
				PT_REF(ip + j * h, vv);
				if(vv > cv) { cv = vv; c = ip + j * h; }
			}
			for(int st = h / 2; st >= 1; st /= 2) {
				float a, b2;
				PT_REF(c - st, a);
				PT_REF(c + st, b2);
				if(a > cv && a >= b2) { cv = a; c -= st; }
				else if(b2 > cv) { cv = b2; c += st; }
			}
			float ym, yp;
			PT_REF(c - 1, ym);
			PT_REF(c + 1, yp);
#undef PT_REF
			if(cv > 0.5f && cv >= ym && cv >= yp) {
				float denom = ym - 2.0f * cv + yp;
				float off = 0.0f;
				if(fabsf(denom) > 1e-9f) {
					off = 0.5f * (ym - yp) / denom;
					if(off > 0.5f) off = 0.5f;
					if(off < -0.5f) off = -0.5f;
				}
				float pf = (float)c + off;
				if(pf > 0.97f * p && pf < 1.03f * p)
					p = pf;
			}
		}
	}

	/*
	 * CROSSTALK, SOFT BAND: within 9 dB of the predicted bleed, a pitch
	 * that is a clearly louder neighbour's own pitch is that neighbour.
	 */
	if(t->pw < PT_BLEED_SOFT * bleed && matchesLouderNeighbour(s, p, t->pw)) {
		if(v->attackBlank > 0) v->attackBlank--;
		holdSilent(v, t, 2);
		return;
	}
	if(underBleed) {
		/* LOCK string decayed into the bleed: hold the note, no updates */
		v->pitchConf = best < PITCH_CONF_MIN ? best : PITCH_CONF_MIN - 0.01f;
		return;
	}

	v->pitchConf = best;

	/* ---------------------- note state machine ---------------------- */
	/*
	 * Two confidence bars. PT_ACCEPT (0.90) takes a post-onset estimate
	 * on its own. PT_ACCEPT_LO (0.75) takes it only when two consecutive
	 * hops agree: that is the case of a quiet string next to a louder one
	 * playing a DIFFERENT note, whose bleed caps this string's clarity at
	 * 0.75-0.92 for the whole note (strum test, G string in a C chord).
	 * v1 never resolved those at all. pitchConf still reports the honest
	 * clarity, so the shifter's own 0.92 relock bar and the synth gate are
	 * untouched; what changes is that period/periodGood - which the DRIFT
	 * splicer, the effects and the next relock all use - are the right
	 * note instead of the previous one.
	 */
	int agreeWithCand = t->agree > 0 && fabsf(p - t->cand) < 0.015f * p;
	/* OCTAVE SAFETY for early reads: right after the pick only short lags
	 * are evaluable, and a strong upper partial (a bridge pickup favours
	 * partials 3-5) can look periodic on its own for a few ms - measured: a
	 * 670 Hz flash on an open G for 9 ms. A single hop is only enough once
	 * twice the lag has been evaluated too (or the string's range ends
	 * first), i.e. once "an octave lower" has had its chance. Before that
	 * it takes two consecutive agreeing hops - one extra hop, instead of
	 * waiting ~1.75 more periods for the 2x lag to become evaluable. */
	int rangeOk = hiEval >= lagMax - 1 || hiEval >= 2 * bestLag + 1;
	if(t->state == PT_ACQ) {
		/* first estimate after an onset IS the note (see header, 6) */
		if((best >= PT_ACCEPT && (rangeOk || agreeWithCand))
		   || (best >= PT_ACCEPT_LO && agreeWithCand)) {
			v->period = p;
			v->periodGood = p;
			t->state = PT_LOCK;
			t->steady = 0;
			t->agree = 0;
			v->candidateAgree = 0;
			v->attackBlank = 0;
		} else {
			t->agree = best >= PT_ACCEPT_LO ? 1 : 0;
			t->cand = p;
			if(v->attackBlank > 0) v->attackBlank--;
			if(v->pitchConf >= PITCH_CONF_MIN)
				v->pitchConf = PITCH_CONF_MIN - 0.01f;
		}
		return;
	}
	if(v->attackBlank > 0) v->attackBlank--;
	if(best < PT_ACCEPT_LO)
		return;                           /* not confident: keep the last period */
	int strong = best >= PITCH_CONF_MIN;

	if(t->state == PT_IDLE) {
		/* periodic signal with no onset (missed pick, or legato out of
		 * silence): 2 agreeing hops when strong, 3 when not */
		if(agreeWithCand) {
			if(++t->agree >= (strong && rangeOk ? 2 : 3)) {
				v->period = p;
				v->periodGood = p;
				t->state = PT_LOCK;
				t->steady = 0;
				t->agree = 0;
			}
		} else {
			t->agree = 1;
		}
		t->cand = p;
		v->candidateAgree = t->agree;
		v->periodCandidate = p;
		return;
	}

	/* PT_LOCK */
	v->periodGood = p;

	/* decay-tail freeze (v1): a tail at a quarter of its recent peak is
	 * decaying into noise - hold, do not chase */
	if(v->env < 0.25f * v->envPrev) {
		t->agree = 0;
		v->candidateAgree = 0;
		return;
	}

	float dev = fabsf(p - v->period) / (v->period + 1e-9f);
	if(dev > 0.03f) {
		/* a jump without an onset: legato, or a glitch. Consensus: 2 hops,
		 * 3 for an exact octave (the classic tracker glitch) or when the
		 * reads are only moderately confident. */
		float r = p / v->period;
		int octave = (r > 1.94f && r < 2.06f) || (r > 0.485f && r < 0.515f);
		int need = (octave || !strong) ? 3 : 2;
		if(agreeWithCand) {
			if(++t->agree >= need) {
				v->period = p;
				t->agree = 0;
				t->steady = 0;
			}
		} else {
			t->agree = 1;
		}
		t->cand = p;
		v->candidateAgree = t->agree;
		v->periodCandidate = p;
	} else {
		t->agree = 0;
		v->candidateAgree = 0;
		/* bends and vibrato move fast; sub-10-cent wander is estimator
		 * jitter and is smoothed harder */
		float w = dev < 0.006f ? 0.25f : 0.6f;
		if(!strong) w *= 0.5f;
		float step = w * (p - v->period);
		v->period += step;
		/* a bend or vibrato is MOTION: drop back to the short windows so
		 * the long ones do not lag it. SIGNED average over ~4 hops, so a
		 * bend (one direction for many hops) trips it at ~2 cents/hop
		 * while zero-mean wobble - the beat against a neighbour's bleed,
		 * ~25 ms period for a major third - averages out and does not
		 * throw the string back into its jittery short-window mode. */
		t->motion += 0.25f * (step / v->period - t->motion);
		if(fabsf(t->motion) > 0.0012f)
			t->steady = 0;
		else if(t->steady < 1000)
			t->steady++;
	}
}
