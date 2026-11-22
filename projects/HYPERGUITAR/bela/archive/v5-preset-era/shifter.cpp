/*
 * shifter.cpp - pitch+formant engine, one instance per string.
 *
 * ARCHITECTURE (v2, conservative): the proven old topology with new
 * precision. Pitch DOWN is a SOLAD drifting-delay resampler - a true
 * frequency scaler whose rate is set by the ratio alone, so it can never
 * chirp, and it works on perfectly periodic content (mark-repositioned
 * COLA grains mathematically CANNOT down-shift periodic content: moving
 * grains by whole periods of a periodic signal is an identity - that was
 * the "laser"/unshifted failure of v1). Pitch UP and FORMANT use
 * mark-synchronous COLA grain streams (window = 2x launch spacing, 50%
 * overlap, sums to a constant: zero grain-rate AM - measured 0.0dB),
 * with the source advancing exactly one pitch mark per launch. The
 * formant leg runs in series on the pitch-stage output, exactly like the
 * old engine, but mark-based and COLA-exact.
 *
 * Attacks are never granulated: a per-sample onset detector snaps a
 * direct tap to the onset and the transient passes exactly once at unity
 * gain (the loud-plosive fix). During mutes/scratches with no pitch
 * lock, a correlation-refined dual-tap splicer takes over.
 *
 * THE ANTI-LASER RULE: the scheduler period IS the tracker period,
 * updated at hop boundaries only. The tracker already snaps on confirmed
 * note changes and glides only on real bends - any extra smoothing layer
 * on top turns every note change into an audible pitch sweep.
 */
#include "hyperguitar.h"
#include <cmath>
#include <cstring>

#define SH_MODE_GRAINS 0
#define SH_MODE_TRANS  1
#define SH_MODE_DRIFT  2

/* ------------------------- mark picker ------------------------- */

static void pickerSetF0(MarkPicker *m, float f0, float sr)
{
	float fc = 1.5f * f0;
	if(fc > 0.45f * sr) fc = 0.45f * sr;
	if(fc < 40.0f) fc = 40.0f;
	float w0 = 2.0f * (float)M_PI * fc / sr;
	float cw = cosf(w0), al = sinf(w0) * 0.7071f;
	float a0i = 1.0f / (1.0f + al);
	m->b1 = (1.0f - cw) * a0i;
	m->b0 = m->b2 = 0.5f * m->b1;
	m->a1 = -2.0f * cw * a0i;
	m->a2 = (1.0f - al) * a0i;
}

static void pickerReset(MarkPicker *m, float T)
{
	m->haveMark = 0;
	m->pkVal = 0.0f;
	m->searchIn = 0.0f;
	m->searchLeft = 1.5f * T;
}

/* per-sample: filter, track the once-per-period peak, commit marks.
 * T = expected period (sets the predicted search window). */
static void pickerRun(MarkPicker *m, float x, float T)
{
	float y = m->b0 * x + m->z1;
	m->z1 = m->b1 * x - m->a1 * y + m->z2;
	m->z2 = m->b2 * x - m->a2 * y;

	m->markDelay += 1.0f;
	m->pkDelay += 1.0f;
	if(m->searchIn > 0.0f) {
		m->searchIn -= 1.0f;
	} else if(m->searchLeft > 0.0f) {
		m->searchLeft -= 1.0f;
		if(y > m->pkVal) { m->pkVal = y; m->pkDelay = 0.0f; }
		if(m->searchLeft <= 0.0f) {
			if(m->pkVal > 1e-4f) {
				m->markDelay = m->pkDelay;
				if(m->haveMark < 2) m->haveMark++;
				float in = 0.75f * T - m->markDelay;
				m->searchIn = in > 0.0f ? in : 0.0f;
				m->searchLeft = 0.5f * T;
			} else {
				m->searchIn = 0.0f;
				m->searchLeft = 0.5f * T;
			}
			m->pkVal = 0.0f;
		}
	}
}

/* ------------------------- grain leg ------------------------- */

static void legReset(GrainLeg *L, float srcDelay)
{
	for(int i = 0; i < SH_NGRAINS; i++)
		L->gr[i].active = 0;
	L->launchCd = 0.0f;
	L->srcDelay = srcDelay;
}

static void legLaunch(GrainLeg *L, MarkPicker *mk, float S, float phi, float T)
{
	/*
	 * SOURCE MARK BOOKKEEPING - the PSOLA contract: the source advances
	 * exactly ONE pitch mark per launch; time skew is absorbed by
	 * whole-period drops/repeats, which splice period-aligned.
	 */
	float k = roundf((L->srcDelay - mk->markDelay) / T);
	if(k < 0.0f) k = 0.0f;               /* never source a future mark */
	float dc = mk->markDelay + k * T;

	/* causality: grain reads source [mark - S*phi, mark + S*phi]; the end
	 * must be written by the time it's read -> dc >= 3 + S*(phi-2) */
	float need = 3.0f + (phi > 2.0f ? S * (phi - 2.0f) : 0.0f);
	while(dc < need)
		dc += T;
	/* skew ceiling: drop whole periods when the source falls behind */
	float ceil2 = 2.5f * T + 128.0f + need;
	while(dc > ceil2 && dc - T >= need)
		dc -= T;

	float d0 = dc + S * phi;
	if(d0 > (float)(FXBUF_MASK - 16)) {
		L->srcDelay = mk->markDelay;     /* re-anchor and skip one launch */
		L->launchCd += S;
		return;
	}

	ShGrain *g = &L->gr[0];
	float best = -1.0f;
	for(int i = 0; i < SH_NGRAINS; i++) {
		if(!L->gr[i].active) { g = &L->gr[i]; best = 2.0f; break; }
		float prog = L->gr[i].u / L->gr[i].L;
		if(prog > best) { best = prog; g = &L->gr[i]; }
	}
	g->d0 = d0;
	g->u = 0.0f;
	g->L = 2.0f * S;
	g->phi = phi;
	g->active = 1;

	/* advance one period per launch, pre-mapped to next-launch time
	 * coordinates (all delays grow by S between launches) */
	L->srcDelay = dc - T + S;
	L->launchCd += S;
}

static float legRun(GrainLeg *L, MarkPicker *mk, const float *buf,
		unsigned int mask, unsigned int wr, float S, float phi, float T)
{
	L->srcDelay += 0.0f; /* srcDelay is in launch-time coords; see legLaunch */
	if(mk->haveMark >= 2 && (L->launchCd -= 1.0f) <= 0.0f)
		legLaunch(L, mk, S, phi, T);

	float out = 0.0f;
	for(int i = 0; i < SH_NGRAINS; i++) {
		ShGrain *g = &L->gr[i];
		if(!g->active)
			continue;
		float t = g->u / g->L;
		float w = shHann(t);
		out += w * bufAtM(buf, mask, wr, g->d0 + g->u * (1.0f - g->phi));
		g->u += 1.0f;
		if(g->u >= g->L)
			g->active = 0;
	}
	return out;
}

/* -------- SOLAD continuous downshifter (drifting-delay resampler; the
 * drift RATE is 1-rho, so the shift amount can never glide or chirp -
 * the period estimate only sets the jump size/phase) -------- */

static inline float soladRun(Shifter *sh, Voice *v, float rho)
{
	float drift = 1.0f - rho;
	if(drift < 0.01f) {
		sh->sdDelay = 0.5f * sh->sdHiThresh;
		sh->sdFading = 0;
		return bufAtM(v->buf, BUF_MASK, v->writePos, sh->sdDelay);
	}

	if(!sh->sdFading && sh->sdDelay > sh->sdHiThresh) {
		float J = v->period;
		sh->sdOldDelay = sh->sdDelay;
		sh->sdDelay -= J;
		if(sh->sdDelay < 8.0f) sh->sdDelay = 8.0f;
		sh->sdFading = 1;
		sh->sdFadePh = 0.0f;
		sh->sdFadeStep = 1.0f / (0.5f * J);
	}

	float out;
	if(sh->sdFading) {
		float t = sh->sdFadePh;
		float g = t * t * (3.0f - 2.0f * t);
		out = bufAtM(v->buf, BUF_MASK, v->writePos, sh->sdOldDelay) * (1.0f - g)
		    + bufAtM(v->buf, BUF_MASK, v->writePos, sh->sdDelay) * g;
		sh->sdFadePh += sh->sdFadeStep;
		if(sh->sdFadePh >= 1.0f)
			sh->sdFading = 0;
		sh->sdOldDelay += drift;
	} else {
		out = bufAtM(v->buf, BUF_MASK, v->writePos, sh->sdDelay);
	}
	sh->sdDelay += drift;
	return out;
}

/* ------------------------- lifecycle ------------------------- */

void shifterInit(Voice *v)
{
	Shifter *sh = &v->sh;
	memset(sh, 0, sizeof(Shifter));
	sh->Tsm = v->p0;
	sh->mkIn.markDelay = v->p0;
	sh->mkFx.markDelay = v->p0;
	pickerReset(&sh->mkIn, v->p0);
	pickerReset(&sh->mkFx, v->p0);
	legReset(&sh->up, v->p0);
	legReset(&sh->fm, v->p0);
	sh->sdHiThresh = 1.15f * v->p0 + 128.0f;
	sh->sdDelay = 0.5f * sh->sdHiThresh;
	sh->mode = SH_MODE_GRAINS;
	sh->grainMix = 1.0f;
	sh->tapDelay = 64.0f;
	sh->aaCoef = 1.0f;
	sh->onsCF = 1.0f / (0.3f * v->p0);
	sh->onsCS = sh->onsCF / 12.0f;
	sh->lpcGain = sh->lpcGainW = 1.0f;
}

/* per-hop duties: picker coefficients, leg blends, mode transitions, LPC */
void shifterHop(Voice *v, int s)
{
	(void)s;
	Shifter *sh = &v->sh;

	/* THE ANTI-LASER RULE (see file header): trust the tracker. */
	sh->Tsm = v->period;

	float f0 = v->srate / v->period;
	pickerSetF0(&sh->mkIn, f0, v->srate);
	pickerSetF0(&sh->mkFx, f0 * (v->beta > 1.0f ? 1.0f : v->beta), v->srate);

	/* onset-follower coefficients, scaled to THIS string's period so the
	 * fast follower smooths over intra-period ripple */
	{
		float cF = 1.0f / (0.3f * sh->Tsm);
		if(cF > 1.0f / 32.0f) cF = 1.0f / 32.0f;
		if(cF < 1.0f / 2000.0f) cF = 1.0f / 2000.0f;
		sh->onsCF = cF;
		sh->onsCS = cF * (1.0f / 12.0f);
	}

	/* anti-alias one-pole for formant-up grain reads (write-side) */
	float phiT = v->formant > 1.0f ? v->formant : 1.0f;
	float fcA = 0.45f * v->srate / phiT;
	sh->aaCoef = 1.0f - expf(-2.0f * (float)M_PI * fcA / v->srate);

	/* pitch leg blends (old engine behavior, proven) */
	float dT = v->beta < 0.995f ? 1.0f : 0.0f;
	float uT = v->beta > 1.005f ? 1.0f : 0.0f;
	sh->downAmt += 0.25f * (dT - sh->downAmt);
	sh->upAmt += 0.25f * (uT - sh->upAmt);

	/* formant engage blend (old formantBlend, proven) */
	float dev = fabsf(v->formant - 1.0f) * 25.0f;
	if(dev > 1.0f) dev = 1.0f;
	sh->formantAmt += 0.3f * (dev - sh->formantAmt);

	/* re-lock accounting */
	if(v->attackBlank == 0 && v->pitchConf >= PITCH_CONF_MIN) {
		if(sh->relockHops < 100) sh->relockHops++;
		sh->noLockHops = 0;
	} else {
		sh->relockHops = 0;
		if(v->pitchConf < NSDF_CONF && sh->noLockHops < 100)
			sh->noLockHops++;
	}

	/* mode machine (transitions at hop rate; fades run at sample rate).
	 * CONSERVATIVE: 3 confident hops to re-enter grains, long leash
	 * before falling back - mode flapping reads as warble. */
	if(sh->mode == SH_MODE_TRANS) {
		sh->transAge++;
		if(sh->relockHops >= 3 && (int)(gSampleStamp - sh->transHoldUntil) >= 0) {
			sh->mode = SH_MODE_GRAINS;
			legReset(&sh->up, sh->mkIn.markDelay);
			legReset(&sh->fm, sh->mkFx.markDelay);
			/* post-attack bass tightness: restart the down leg near the
			 * write head (the old "transient re-anchor", done ONCE at a
			 * clean boundary instead of mid-flight) */
			sh->sdDelay = 32.0f;
			sh->sdFading = 0;
			sh->tapDelay = 32.0f;   /* inside bounds: no splice on exit */
			sh->tapFading = 0;
		} else if(sh->noLockHops > 18 || sh->transAge > 25) {
			/* attack never resolved OR confidence hovering mid-band
			 * (0.45..0.92 advances NEITHER counter): fall through to the
			 * splicer, which still SHIFTS - staying here would play the
			 * whole note unshifted at full wet */
			sh->mode = SH_MODE_DRIFT;
		}
	} else if(sh->mode == SH_MODE_GRAINS) {
		if(sh->noLockHops > 10)
			sh->mode = SH_MODE_DRIFT;
	} else { /* DRIFT */
		if(sh->relockHops >= 3) {
			sh->mode = SH_MODE_GRAINS;
			legReset(&sh->up, sh->mkIn.markDelay);
			legReset(&sh->fm, sh->mkFx.markDelay);
			sh->sdDelay = 32.0f;
			sh->sdFading = 0;
		}
	}

	if(gFormantActive && gFormantMode == 1 && sh->mode == SH_MODE_GRAINS)
		lpcAnalyze(v);
	else
		sh->lpcValid = 0;
}

/* per-sample onset: called from voiceProcess the moment a pick lands */
void shifterOnset(Voice *v, int s)
{
	Shifter *sh = &v->sh;
	sh->mode = SH_MODE_TRANS;
	sh->tapDelay = 4.0f;
	sh->tapFading = 0;
	sh->relockHops = 0;
	sh->noLockHops = 0;      /* stale silence-count must not bounce us to DRIFT */
	pickerReset(&sh->mkIn, sh->Tsm);
	pickerReset(&sh->mkFx, sh->Tsm);
	/* strum coherence: if a neighbour picked <10ms ago, exit together so
	 * a strummed chord resolves as one gesture */
	unsigned int win = (unsigned int)(0.010f * v->srate);
	unsigned int hold = gSampleStamp + win;
	for(int o = 0; o < NUM_STRINGS; o++) {
		if(o == s) continue;
		if(gSampleStamp - gOnsetStamp[o] < win) {
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

/* ------------------------- per-sample engine ------------------------- */

float shifterRun(Voice *v, float full)
{
	Shifter *sh = &v->sh;
	float T = sh->Tsm;

	pickerRun(&sh->mkIn, full, T);

	/* ---- pitch stage: SOLAD down / mark grains up / passthrough ---- */
	float rho = v->beta;
	if(rho < 0.5f) rho = 0.5f;
	if(rho > 2.0f) rho = 2.0f;
	float dA = sh->downAmt, uA = sh->upAmt;
	float p = full * (1.0f - dA - uA);
	if(dA > 0.002f)
		p += dA * soladRun(sh, v, rho < 1.0f ? rho : 1.0f);
	else
		soladRun(sh, v, 1.0f);          /* keep the delay line warm */
	if(uA > 0.002f) {
		float S = T / (rho > 1.0f ? rho : 1.0f);
		if(S < 24.0f) S = 24.0f;
		p += uA * legRun(&sh->up, &sh->mkIn, v->buf, BUF_MASK,
				v->writePos, S, 1.0f, T);
	}

	v->fxbuf[v->writePos & FXBUF_MASK] = p;

	/* ---- formant stage: series mark grains on the pitch-stage output.
	 * Output period after the pitch stage is T/rho for the shifted legs;
	 * unshifted when the pitch stage is idle. ---- */
	float Tout = dA > 0.5f ? T / rho : (uA > 0.5f ? T / rho : T);
	if(Tout < 24.0f) Tout = 24.0f;
	if(Tout > 2800.0f) Tout = 2800.0f;
	pickerRun(&sh->mkFx, p, Tout);

	float chain = p;
	float fA = gFormantMode == 0 ? sh->formantAmt : 0.0f;
	if(fA > 0.003f) {
		float phi = v->formant;
		const float *src = v->fxbuf;
		/* formant-up reads through the anti-aliased input copy only when
		 * the pitch stage is idle (abuf mirrors the INPUT, not fxbuf) */
		if(phi > 1.1f && dA < 0.01f && uA < 0.01f)
			src = v->abuf;
		float fw = legRun(&sh->fm, &sh->mkFx, src, FXBUF_MASK,
				v->writePos, Tout, phi, Tout);
		chain = p * (1.0f - fA) + fw * fA;
	}

	/* ---- tap leg: transient pass-through / no-lock drift splicer ---- */
	float tap;
	if(sh->mode == SH_MODE_TRANS) {
		tap = bufAtM(v->buf, BUF_MASK, v->writePos, sh->tapDelay);
	} else {
		float Tref = v->periodGood > 24.0f ? v->periodGood : v->p0;
		if(sh->mode == SH_MODE_DRIFT)
			sh->tapDelay += 1.0f - rho;
		float lo = 8.0f, hi = 2.5f * Tref + 256.0f;
		if(!sh->tapFading && (sh->tapDelay < lo || sh->tapDelay > hi)) {
			/* splice: jump one period back toward the window, refined
			 * +-4 samples by normalized cross-correlation so the two
			 * taps are phase-aligned at the fade (H949's de-glitcher) */
			float dir = sh->tapDelay < lo ? 1.0f : -1.0f;
			float base = sh->tapDelay + dir * Tref;
			int win = (int)(Tref < 300.0f ? Tref : 300.0f);
			float bestv = -2.0f, besto = 0.0f;
			for(int o = -4; o <= 4; o++) {
				float acf = 0.0f, e1 = 0.0f, e2 = 0.0f;
				for(int i = 0; i < win; i += 2) {
					float sa = bufAtM(v->buf, BUF_MASK, v->writePos,
							sh->tapDelay + (float)i);
					float sb = bufAtM(v->buf, BUF_MASK, v->writePos,
							base + (float)o + (float)i);
					acf += sa * sb;
					e1 += sa * sa;
					e2 += sb * sb;
				}
				float val = acf / sqrtf(e1 * e2 + 1e-12f);
				if(val > bestv) { bestv = val; besto = (float)o; }
			}
			sh->tapOld = sh->tapDelay;
			sh->tapDelay = base + besto;
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
			if(sh->tapFadePh >= 1.0f)
				sh->tapFading = 0;
			if(sh->mode == SH_MODE_DRIFT)
				sh->tapOld += 1.0f - rho;
		} else {
			tap = bufAtM(v->buf, BUF_MASK, v->writePos, sh->tapDelay);
		}
	}

	/* ---- leg blend: ~2ms into the tap (attack must land NOW), ~1
	 * output period back out to the shifter chain ---- */
	float target = sh->mode == SH_MODE_GRAINS ? 1.0f : 0.0f;
	float step = target > sh->grainMix
		? 1.0f / (T > 96.0f ? T : 96.0f)
		: 1.0f / (0.002f * v->srate);
	if(sh->grainMix < target) {
		sh->grainMix += step;
		if(sh->grainMix > target) sh->grainMix = target;
	} else if(sh->grainMix > target) {
		sh->grainMix -= step;
		if(sh->grainMix < target) sh->grainMix = target;
	}
	float gG = sqrtf(sh->grainMix), gT = sqrtf(1.0f - sh->grainMix);
	return gG * chain + gT * tap;
}
