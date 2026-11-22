/*
 * lpc.cpp - true spectral-envelope formant mode (the "LPC" half of the
 * GRAIN/LPC toggle on the FORMANT pad).
 *
 * Per hop, on locked strings: order-16 LPC over the last 512 input
 * samples (lag-windowed + noise-loaded so the model rides the envelope
 * instead of individual harmonics), then the envelope is evaluated on a
 * 33-point frequency grid, warped E'(w) = E(w/phi), refit to a new
 * all-pole model via cosine-sum autocorrelation + Levinson.
 *
 * Per sample: whiten the (formant-preserving, phi=1) shifter output with
 * the analysis lattice, re-color with the warped-envelope lattice.
 * Reflection coefficients are slewed per sample - a convex combination
 * of stable lattices is stable, so no frame-boundary zipper.
 */
#include "hyperguitar.h"
#include <cmath>

/* Levinson-Durbin: autocorr r[0..p] -> reflection k[0..p-1], a[1..p].
 * Returns final prediction error, or -1 on an unstable frame. */
static float levinson(const float *r, int p, float *k, float *a)
{
	float E = r[0];
	if(E < 1e-12f)
		return -1.0f;
	for(int j = 0; j <= p; j++) a[j] = 0.0f;
	a[0] = 1.0f;
	for(int i = 1; i <= p; i++) {
		float acc = r[i];
		for(int j = 1; j < i; j++)
			acc += a[j] * r[i - j];
		float ki = -acc / E;
		if(ki > 0.998f || ki < -0.998f)
			return -1.0f;
		k[i - 1] = ki;
		a[i] = ki;
		{	/* order-update with a temp copy (p is small; clarity wins) */
			float tmp[LPC_ORDER + 1];
			for(int j = 1; j < i; j++)
				tmp[j] = a[j] + ki * a[i - j];
			for(int j = 1; j < i; j++)
				a[j] = tmp[j];
		}
		E *= (1.0f - ki * ki);
		if(E < 1e-12f)
			return -1.0f;
	}
	return E;
}

void lpcAnalyze(Voice *v)
{
	Shifter *sh = &v->sh;

	/* lag window: ~60Hz Gaussian bandwidth expansion, computed once */
	static float lagW[LPC_ORDER + 1];
	static int lagInit = 0;
	if(!lagInit) {
		for(int i = 0; i <= LPC_ORDER; i++) {
			float x = 2.0f * (float)M_PI * 60.0f * (float)i / 44100.0f;
			lagW[i] = expf(-0.5f * x * x);
		}
		lagInit = 1;
	}

	/* autocorrelation over the newest LPC_WIN input samples */
	float w[LPC_WIN];
	unsigned int wp = v->writePos;
	for(int i = 0; i < LPC_WIN; i++)
		w[i] = v->buf[(wp - LPC_WIN + i) & BUF_MASK];
	float r[LPC_ORDER + 1];
	for(int lag = 0; lag <= LPC_ORDER; lag++) {
		float acc = 0.0f;
		for(int i = lag; i < LPC_WIN; i++)
			acc += w[i] * w[i - lag];
		r[lag] = acc * lagW[lag];
	}
	if(r[0] < 1e-8f) {      /* silence: nothing to model */
		sh->lpcValid = 0;
		return;
	}
	r[0] *= 1.001f;          /* -30dB noise loading vs harmonic overfit */

	float k[LPC_ORDER], a[LPC_ORDER + 1];
	float E = levinson(r, LPC_ORDER, k, a);
	if(E <= 0.0f) {
		sh->lpcValid = 0;
		return;
	}

	/* envelope on a 33-point grid: Env[m] = 1/|A(e^jw)| at w = pi*m/32 */
	float env[LPC_EPTS];
	for(int m = 0; m < LPC_EPTS; m++) {
		float wm = (float)M_PI * (float)m / (float)(LPC_EPTS - 1);
		float re = 1.0f, im = 0.0f;
		for(int j = 1; j <= LPC_ORDER; j++) {
			re += a[j] * cosf(wm * (float)j);
			im -= a[j] * sinf(wm * (float)j);
		}
		float mag2 = re * re + im * im;
		env[m] = 1.0f / sqrtf(mag2 + 1e-9f);
	}

	/* warp: E'(w) = E(w/phi), straight from the per-hop glided value */
	float phi = v->formant;
	if(phi < 0.4f) phi = 0.4f;
	if(phi > 2.4f) phi = 2.4f;
	float envW[LPC_EPTS];
	float eIn = 0.0f, eOut = 0.0f;
	for(int m = 0; m < LPC_EPTS; m++) {
		float q = (float)m / phi;
		if(q >= (float)(LPC_EPTS - 1)) {
			envW[m] = env[LPC_EPTS - 1];
		} else {
			int qi = (int)q;
			float fr = q - (float)qi;
			envW[m] = env[qi] * (1.0f - fr) + env[qi + 1] * fr;
		}
		float wgt = (m == 0 || m == LPC_EPTS - 1) ? 0.5f : 1.0f;
		eIn += wgt * env[m] * env[m];
		eOut += wgt * envW[m] * envW[m];
	}
	float norm = sqrtf(eIn / (eOut + 1e-12f));
	for(int m = 0; m < LPC_EPTS; m++)
		envW[m] *= norm;

	/* refit: rw[lag] = sum_m W[m] * EnvW[m]^2 * cos(w_m * lag) */
	float rw[LPC_ORDER + 1];
	for(int lag = 0; lag <= LPC_ORDER; lag++) {
		float acc = 0.0f;
		for(int m = 0; m < LPC_EPTS; m++) {
			float wgt = (m == 0 || m == LPC_EPTS - 1) ? 0.5f : 1.0f;
			float wm = (float)M_PI * (float)m / (float)(LPC_EPTS - 1);
			acc += wgt * envW[m] * envW[m] * cosf(wm * (float)lag);
		}
		rw[lag] = acc * lagW[lag];
	}
	rw[0] *= 1.001f;

	float kr[LPC_ORDER], ar[LPC_ORDER + 1];
	float Er = levinson(rw, LPC_ORDER, kr, ar);
	if(Er <= 0.0f) {
		sh->lpcValid = 0;
		return;
	}

	/* gain: residual (var ~E) -> synthesis lattice modeled on rw (its own
	 * residual var Er, total power rw[0]); match output power to input */
	float g = sqrtf((r[0] * Er) / (E * rw[0] + 1e-12f) + 1e-12f);
	if(g > 8.0f) g = 8.0f;

	for(int i = 0; i < LPC_ORDER; i++) {
		sh->lpcK[i] = k[i];
		sh->lpcKr[i] = kr[i];
	}
	sh->lpcGain = g;
	sh->lpcValid = 1;
}

float lpcProcess(Voice *v, float x)
{
	Shifter *sh = &v->sh;

	/* bypass crossfade (~5ms) while analysis is invalid (attacks, mutes) */
	float mixT = sh->lpcValid ? 1.0f : 0.0f;
	sh->lpcMix += 0.005f * (mixT - sh->lpcMix);
	if(sh->lpcMix < 0.001f)
		return x;

	/* slew coefficients: convex combo of stable lattices stays stable */
	for(int i = 0; i < LPC_ORDER; i++) {
		sh->lpcKW[i] += 0.002f * (sh->lpcK[i] - sh->lpcKW[i]);
		sh->lpcKrW[i] += 0.002f * (sh->lpcKr[i] - sh->lpcKrW[i]);
	}
	sh->lpcGainW += 0.002f * (sh->lpcGain - sh->lpcGainW);

	/* whiten (FIR lattice) */
	float f = x, b = x;
	for(int m = 0; m < LPC_ORDER; m++) {
		float bd = sh->lpcZf[m];
		sh->lpcZf[m] = b;
		float fN = f + sh->lpcKW[m] * bd;
		b = bd + sh->lpcKW[m] * f;
		f = fN;
	}

	/* re-color (IIR lattice) with the warped envelope */
	float y = f * sh->lpcGainW;
	for(int m = LPC_ORDER - 1; m >= 0; m--) {
		y -= sh->lpcKrW[m] * sh->lpcZg[m];
		if(m < LPC_ORDER - 1)
			sh->lpcZg[m + 1] = sh->lpcZg[m] + sh->lpcKrW[m] * y;
	}
	sh->lpcZg[0] = y;

	/* NaN guard: a poisoned lattice would be permanent - scrub in place */
	if(!(fabsf(y) < 16.0f)) {
		for(int m = 0; m < LPC_ORDER; m++)
			sh->lpcZf[m] = sh->lpcZg[m] = 0.0f;
		y = 0.0f;
	}

	return x * (1.0f - sh->lpcMix) + y * sh->lpcMix;
}
