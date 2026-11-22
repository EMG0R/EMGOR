/*
 * pitch_tracker.cpp - NSDF-based per-string pitch tracker, split out of
 * the former monolithic render.cpp. Bodies are byte-for-byte identical
 * to the original.
 */
#include "hyperguitar.h"
#include <cmath>

static inline float nsdfAt(const Voice *v, const float *recent, float e0, int lag)
{
	float acf = 0.0f, el = 0.0f;
	const float *lagged = recent - lag;
	for(int i = 0; i < v->corrWin; i++) {
		acf += recent[i] * lagged[i];
		el += lagged[i] * lagged[i];
	}
	return acf / sqrtf(e0 * el + 1e-12f);
}

void trackPitch(Voice *v, int s)
{
	/*
	 * NOTE-OFF EDGE DETECTOR: releasing a fretting finger (or palm-
	 * muting) damps the string almost instantly - MUCH faster than a
	 * freely ringing string could ever decay on its own. See git log
	 * for the full rationale (unchanged, moved verbatim).
	 */
	{
		float envOld = v->envHist[v->envHistIdx];
		v->envHist[v->envHistIdx] = v->env;
		v->envHistIdx = (v->envHistIdx + 1) & 3;
		if(v->attackBlank == 0) {
			if(envOld > 0.003f && v->env < 0.35f * envOld)
				v->releaseBlank = 12;    // ~35ms
			if(v->envLastHop > 0.003f && v->env < 0.5f * v->envLastHop)
				v->releaseBlank = 12;
		}
		v->envLastHop = v->env;
	}
	if(v->releaseBlank > 0) {
		v->releaseBlank--;
		v->pitchConf = 0.0f;
		return;
	}

	float x[DBUF_SIZE];
	int need = v->corrWin + v->lagMax + 1;
	for(int i = 0; i < need; i++)
		x[i] = v->dbuf[(v->dwritePos - need + i) & DBUF_MASK];
	float *recent = x + need - v->corrWin;

	float e0 = 0.0f;
	for(int i = 0; i < v->corrWin; i++)
		e0 += recent[i] * recent[i];
	if(e0 < GATE_RMS * GATE_RMS * (float)v->corrWin) {
		v->pitchConf = 0.0f;   // silence: definitely not a pitched note
		return;
	}

	float coarse[256];
	int nC = 0;
	float best = -2.0f;
	for(int lag = v->lagMin; lag <= v->lagMax; lag += 2) {
		float val = nsdfAt(v, recent, e0, lag);
		coarse[nC++] = val;
		if(val > best) best = val;
	}
	if(best < NSDF_CONF) {
		v->pitchConf = best;   // aperiodic (tap/noise): record the low score
		return;
	}

	int bestLag = -1;
	float bar = FIRST_PEAK * best;
	for(int i = 1; i < nC - 1; i++) {
		if(coarse[i] >= bar && coarse[i] >= coarse[i - 1] && coarse[i] >= coarse[i + 1]) {
			bestLag = v->lagMin + 2 * i;
			best = coarse[i];
			break;
		}
	}
	if(bestLag < 0) {
		v->pitchConf = 0.0f;
		return;
	}
	for(int lag = bestLag - 1; lag <= bestLag + 1; lag += 2) {
		if(lag < v->lagMin || lag > v->lagMax)
			continue;
		float val = nsdfAt(v, recent, e0, lag);
		if(val > best) { best = val; bestLag = lag; }
	}

	/*
	 * Anti-octave-low guard: high on the neck the short true-period
	 * peak is under-sampled by the coarse grid and can lose to the
	 * 2P peak. If half this lag correlates nearly as well, the note
	 * is actually an octave HIGHER - take the half lag.
	 */
	int half = bestLag / 2;
	if(half >= v->lagMin) {
		float hv = -2.0f;
		int hb = half;
		for(int k = half - 1; k <= half + 1; k++) {
			if(k < v->lagMin || k > v->lagMax)
				continue;
			float val = nsdfAt(v, recent, e0, k);
			if(val > hv) { hv = val; hb = k; }
		}
		if(hv > 0.92f * best) {
			bestLag = hb;
			best = hv;
		}
	}

	float lagF = (float)bestLag;
	if(bestLag > v->lagMin && bestLag < v->lagMax) {
		float ym = nsdfAt(v, recent, e0, bestLag - 1);
		float yp = nsdfAt(v, recent, e0, bestLag + 1);
		float denom = ym - 2.0f * best + yp;
		if(fabsf(denom) > 1e-9f)
			lagF += 0.5f * (ym - yp) / denom;
	}

	float p = lagF * (float)DECIM;
	float pMin = v->p0 / MAX_FRET_RATIO;
	float pMax = v->p0 * TUNE_MARGIN;
	if(p < pMin) p = pMin;
	if(p > pMax) p = pMax;

	/*
	 * ATTACK REJECTION + CONFIDENCE GATE.
	 */
	v->pitchConf = best;
	if(v->attackBlank > 0) {
		v->attackBlank--;
		return;                       // still inside the transient - ignore
	}
	if(best < PITCH_CONF_MIN)
		return;                       // not confident: keep the last period

	/*
	 * FULL-RATE REFINEMENT.
	 */
	{
		int ip = (int)roundf(p);
		int win = (int)(1.2f * p);
		if(win > 620) win = 620;
		const float *b = v->buf;
		unsigned int wp = v->writePos;
		float re0 = 0.0f;
		for(int i = 0; i < win; i++) {
			float sm = b[(wp - 1 - i) & BUF_MASK];
			re0 += sm * sm;
		}
		float vals[7];
		int bi = -1;
		float bv = -2.0f;
		for(int j = 0; j < 7; j++) {
			int l = ip - 3 + j;
			float acf = 0.0f, el = 0.0f;
			for(int i = 0; i < win; i++) {
				float sa = b[(wp - 1 - i) & BUF_MASK];
				float sb = b[(wp - 1 - i - l) & BUF_MASK];
				acf += sa * sb;
				el += sb * sb;
			}
			vals[j] = acf / sqrtf(re0 * el + 1e-12f);
			if(vals[j] > bv) { bv = vals[j]; bi = j; }
		}
		if(bi >= 1 && bi <= 5 && bv > 0.5f) {
			float ym = vals[bi - 1], yp = vals[bi + 1];
			float denom = ym - 2.0f * bv + yp;
			float off = 0.0f;
			if(fabsf(denom) > 1e-9f) {
				off = 0.5f * (ym - yp) / denom;
				if(off > 0.5f) off = 0.5f;
				if(off < -0.5f) off = -0.5f;
			}
			float pf = (float)(ip - 3 + bi) + off;
			if(pf > 0.8f * p && pf < 1.2f * p)
				p = pf;
		}
	}

	/*
	 * HEX BLEED VETO: a quiet string reading exactly 2x the period of a
	 * LOUD physical neighbour is almost certainly pickup crosstalk
	 * hearing the neighbour's fundamental as its own octave-low. Real
	 * playing essentially never has this string this quiet while the
	 * read is this marginal - keep the last good period instead of
	 * feeding the shifter bleed.
	 */
	for(int o = s - 1; o <= s + 1; o += 2) {
		if(o < 0 || o >= NUM_STRINGS)
			continue;
		Voice *w = &gV[o];
		if(w->env > 3.0f * v->env && best < 0.96f
		   && fabsf(p - 2.0f * w->period) < 0.04f * w->period)
			return;
	}

	v->periodGood = p;

	/*
	 * DECAY-TAIL FREEZE.
	 */
	if(v->env < 0.25f * v->envPrev) {
		v->candidateAgree = 0;
		return;                       // decaying tail: hold, don't chase noise
	}

	/*
	 * NOTE-CHANGE SNAP vs BEND GLIDE, with RE-ACQUISITION CONSENSUS.
	 */
	{
		float devN = fabsf(p - v->period) / (v->period + 1e-9f);
		if(devN > 0.03f) {
			float agreeDev = fabsf(p - v->periodCandidate) / (p + 1e-9f);
			if(v->candidateAgree > 0 && agreeDev < 0.015f) {
				if(++v->candidateAgree >= 2) {
					v->period = p;   // new note, CONFIRMED: arrive NOW
					v->candidateAgree = 0;
				}
			} else {
				v->candidateAgree = 1;
			}
			v->periodCandidate = p;
		} else {
			v->candidateAgree = 0;
			v->period = 0.75f * v->period + 0.25f * p;   // bend: glide
		}
	}
}
