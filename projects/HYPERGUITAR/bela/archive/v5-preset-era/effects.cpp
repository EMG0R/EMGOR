/*
 * effects.cpp - the per-string effect blocks that used to be welded
 * directly into render()'s giant per-sample loop as inline code.
 * Extracted into real functions, called from render.cpp in the EXACT
 * SAME ORDER as before (effects are chained through `mixed`). Bodies
 * are byte-for-byte identical to the original.
 */
#include "hyperguitar.h"
#include <cmath>

// FM: phase-modulate the ACTUAL string signal (index 2,
// modulator = sine at ratio x tracked f0)
void runFM(Voice *v, float *mixed)
{
	v->fmbuf[v->fmWrite & VBUF_MASK] = *mixed;
	if(gFmAmtS > 0.002f && gFmActive) {
		float ratio = gFmRatioVal;
		v->fmPhase += 2.0f * (float)M_PI * ratio / v->period;
		if(v->fmPhase > 2.0f * (float)M_PI)
			v->fmPhase -= 2.0f * (float)M_PI;
		float d = (v->fmA + 8.0f) + v->fmA * sinf(v->fmPhase);
		float fmOut = bufAtM(v->fmbuf, VBUF_MASK, v->fmWrite, d);
		*mixed = *mixed * (1.0f - gFmAmtS) + fmOut * gFmAmtS;
	}
	v->fmWrite++;
}

// AUTOWAH
void runAutowah(Voice *v, float *mixed)
{
	if(gEnvFAmtS > 0.002f) {
		v->svfF += 0.01f * (v->svfFT - v->svfF);
		// dynamics-driven bite (Mu-Tron/Q-Tron style): a harder
		// pick tightens the resonance for more "quack", a quiet
		// note stays smoother - real envelope filters do this,
		// a static Q doesn't.
		float qDyn = gEnvfQ * (0.6f + 1.2f * fminf(v->env * 3.0f, 1.0f));
		float f = v->svfF, q = 1.0f / qDyn;
		v->svfLow += f * v->svfBand;
		float hi = *mixed - v->svfLow - q * v->svfBand;
		v->svfBand += f * hi;
		*mixed += gEnvFAmtS * (1.1f * v->svfLow - *mixed);
	}
}

// 11 RESONATOR: rings a harmonic of the note; dynamics choose which
void runResonator(Voice *v, float *mixed)
{
	if(gResAmtS > 0.002f) {
		float re = v->env * gResSens;
		if(re > 1.0f) re = 1.0f;
		int harm = 2 + (int)(4.0f * re);      // 2nd .. 6th harmonic
		float dTarget = v->period / (float)harm;
		// dynamics crossing a harmonic boundary jumps this delay
		// by a large discrete step - smooth it; fast enough to
		// feel immediate, slow enough to not click.
		v->resDelaySm += 0.25f * (dTarget - v->resDelaySm);
		float ring = combRun(&v->resC, *mixed * 0.35f, v->resDelaySm,
				gResFb, 0.12f);
		*mixed += gResAmtS * gResGain * ring;
	} else {
		v->resC.buf[v->resC.wr & COMB_MASK] = 0.0f;
		v->resC.wr++; v->resC.lp = 0.0f;
	}
}

// 13 JAWARI BUZZ: short metallic comb rattle, harder = buzzier.
// ADDED on top, so the dry level is never touched or clipped.
void runJawari(Voice *v, float *mixed)
{
	if(gJawAmtS > 0.002f) {
		float je = v->env * gJawSens;
		if(je > 1.0f) je = 1.0f;
		float exc = tanhf(*mixed * (0.15f + 0.55f * je) * 1.3f) * (1.0f / 1.3f);
		float jfb = 0.86f + 0.08f * je;
		float ring = combRun(&v->jawC, exc, gJawPitch, jfb, 0.55f);
		*mixed += gJawAmtS * gJawBuzz * 2.0f * je * ring;
	} else {
		v->jawC.buf[v->jawC.wr & COMB_MASK] = 0.0f;
		v->jawC.wr++; v->jawC.lp = 0.0f;
	}
}

// 12 SUSTAINER: comb at the string's own period, near-unity feedback -
// grabs the fundamental instantly and holds it.
void runSustainer(Voice *v, float *mixed)
{
	if(gSusAmtS > 0.002f) {
		float inputLevel = 0.15f + 0.35f * fminf(v->env / 0.5f, 1.0f);
		float ring = combRun(&v->susC, *mixed * inputLevel, v->period,
				v->susTime, gSusDamp);
		*mixed += gSusAmtS * gSusLevel * ring;
	} else {
		v->susC.buf[v->susC.wr & COMB_MASK] = 0.0f;
		v->susC.wr++; v->susC.lp = 0.0f;
	}
}

// 15 SITAR - part 1: the jawari "zing" on the played note. The
// wide flat bridge throws energy into the high partials.
void runSitarZing(Voice *v, float *mixed)
{
	if(gSitarAmtS > 0.002f) {
		v->sitarHp += 0.22f * (*mixed - v->sitarHp);
		float bright = *mixed - v->sitarHp;      // high band
		float zing = tanhf(bright * (1.0f + 6.0f * gSitarBuzz));
		// gated by gSitarLevel too - MAIN is now the ONE wet
		// control for the whole pad, zing included
		*mixed += gSitarAmtS * gSitarLevel * gSitarBuzz * 1.3f * zing;
	}
}

// 14 SYMPATHETIC: this string's comb is tuned to ITS OPEN pitch
// (a sitar's tarab strings are tuned to fixed scale notes) and
// is rung by the OTHER five strings' RAW signal - no feedback.
void runSympathetic(Voice *v, int s, const float *dryIn, float *mixed)
{
	if(gSympAmtS > 0.002f) {
		float inject = 0.0f;
		for(int o = 0; o < NUM_STRINGS; o++)
			if(o != s) inject += dryIn[o] * gSympW[s][o];
		// x2: the relatedness weights cut the average injection,
		// so consonant pairs now ring HARDER than the old flat
		// sum while unrelated ones nearly vanish
		inject *= 2.0f * gSympCoupling / (float)(NUM_STRINGS - 1);
		if(inject > 1.5f) inject = 1.5f;
		if(inject < -1.5f) inject = -1.5f;
		float ring = combRun(&v->sympC, inject * 0.5f, v->p0,
				gSympFb, 0.20f);
		*mixed += gSympAmtS * 3.0f * ring;
	} else {
		v->sympC.buf[v->sympC.wr & COMB_MASK] = 0.0f;
		v->sympC.wr++; v->sympC.lp = 0.0f;
	}
}

// PLUCK SAG: transient pitch spike-then-settle on note attack
void runPluckSag(Voice *v, float *mixed, float sr)
{
	v->sagBuf[v->sagWrite & VBUF_MASK] = *mixed;
	if(gSagAmtS > 0.002f) {
		float sagDepthSmp = gSagDepthSec * sr;
		float dly = 4.0f + sagDepthSmp * v->sagEnv;
		float sagged = bufAtM(v->sagBuf, VBUF_MASK, v->sagWrite, dly);
		*mixed = *mixed * (1.0f - gSagAmtS) + sagged * gSagAmtS;
	}
	v->sagWrite++;
	v->sagEnv *= v->sagDecayCoef;
}

// VIBRATO (NEPTR unipolar sine sweep). Depth follows the string's own
// envelope (floored, never fully off) so it naturally eases as a note
// decays instead of wobbling a dead/dying string at full throw.
void runVibrato(Voice *v, int s, float *mixed, float vibDepthSmp, float sr)
{
	v->vbuf[v->vWrite & VBUF_MASK] = *mixed;
	float A = gVibAmtS * vibDepthSmp
			* (0.4f + 0.6f * fminf(v->env * 3.0f, 1.0f));
	float dly = 4.0f + A * (1.0f + sinf(v->vPhase));
	*mixed = bufAtM(v->vbuf, VBUF_MASK, v->vWrite, dly);
	v->vWrite++;
	v->vPhase += 2.0f * (float)M_PI * VIB_HZ[s] * gVibRateScale / sr;
	if(v->vPhase > 2.0f * (float)M_PI)
		v->vPhase -= 2.0f * (float)M_PI;
}

// POG SWELL: per-note ~100ms attack bloom
void runPogSwell(Voice *v, float *mixed, float pogStep)
{
	if(v->pogGain < 1.0f) {
		v->pogGain += pogStep;
		if(v->pogGain > 1.0f) v->pogGain = 1.0f;
	}
	if(gPogAmtS > 0.002f) {
		// THE SWELL: volume envelope fading in over gPogAttackSec.
		float pg = v->pogGain * v->pogGain; // perceptual curve
		float voiced;
		if(fabsf(gPogModDepth) > 0.05f) {
			// optional filter sweep riding the swell's timing
			float cutoffHz = 1200.0f * exp2f(gPogModDepth * v->pogGain);
			if(cutoffHz < 200.0f) cutoffHz = 200.0f;
			if(cutoffHz > 8000.0f) cutoffHz = 8000.0f;
			float fcoef = 2.0f * sinf((float)M_PI * cutoffHz / v->srate);
			if(fcoef > 1.0f) fcoef = 1.0f;
			v->pogLpZ += fcoef * (*mixed - v->pogLpZ);
			voiced = v->pogLpZ * pg;
		} else {
			voiced = *mixed * pg; // clean swell, no tone change
		}
		*mixed = *mixed * (1.0f - gPogAmtS) + voiced * gPogAmtS;
	}
}

// CHORUS: subtle voice mirrored across the image (width). Triangle LFO
// (not sine) - the classic analog chorus sweeps its delay with an
// integrator-derived triangle.
void runChorus(Voice *v, int s, float *mixed, float chorBaseSmp, float chorDepthSmp,
		float *wideOut, float sr)
{
	v->cbuf[v->cWrite & VBUF_MASK] = *mixed;
	float wide = 0.0f;
	if(gChorAmtS > 0.002f) {
		float ct = v->cPhase * (0.5f / (float)M_PI); // 0..1
		float tri = ct < 0.5f ? (4.0f * ct - 1.0f) : (3.0f - 4.0f * ct);
		float cd = chorBaseSmp + chorDepthSmp * (1.0f + tri);
		wide = bufAtM(v->cbuf, VBUF_MASK, v->cWrite, cd);
	}
	v->cWrite++;
	v->cPhase += 2.0f * (float)M_PI * CHOR_HZ[s] * gChorRateScale / sr;
	if(v->cPhase > 2.0f * (float)M_PI)
		v->cPhase -= 2.0f * (float)M_PI;
	*wideOut = wide;
}

// 15 SITAR - part 2: the tarab bank itself. All 14 scale-tuned
// resonators excited once per sample by the summed dry signal.
void runSitarTarab(const float *dryIn, float *outL, float *outR)
{
	if(gSitarAmtS > 0.002f) {
		float exc = 0.0f;
		for(int s = 0; s < NUM_STRINGS; s++)
			exc += dryIn[s];
		exc *= 0.32f;
		if(exc > 1.5f) exc = 1.5f;
		if(exc < -1.5f) exc = -1.5f;
		float tl = 0.0f, tr = 0.0f;
		for(int k = 0; k < TARAB_N; k++) {
			float ring = combRun(&gTarab[k], exc, gTarabDelay[k],
					gSitarRing, 0.5f);
			tl += ring * (1.0f - gTarabPan[k]);
			tr += ring * gTarabPan[k];
		}
		// strip anything that drifted into the mid band ("wuuuhh") -
		// a 1-pole highpass around ~900 Hz so only the high vowelly
		// shimmer ("eeaaaiiiooowww") survives into the mix
		static float hpL = 0.0f, hpR = 0.0f;
		float hpCoef = 0.05f;  // lowpass tracker @ ~350 Hz; hi = tl - lp
		hpL += hpCoef * (tl - hpL);
		hpR += hpCoef * (tr - hpR);
		float hiL = tl - hpL;
		float hiR = tr - hpR;
		// fixed wet fraction ON TOP of the untouched dry signal -
		// dry is never attenuated, wet is just added in at gSitarLevel
		float g = gSitarAmtS * gSitarLevel;
		*outL += g * hiL;
		*outR += g * hiR;

		static float dbgPk = 0.0f, dbgExcPk = 0.0f;
		static int dbgCtr = 0;
		float wetPk = fabsf(g * hiL) > fabsf(g * hiR) ? fabsf(g * hiL) : fabsf(g * hiR);
		if(wetPk > dbgPk) dbgPk = wetPk;
		if(fabsf(exc) > dbgExcPk) dbgExcPk = fabsf(exc);
		if(++dbgCtr >= 22050) {
			rt_printf("SITAR-DBG amtS=%.3f exc_pk=%.4f wet_pk=%.4f\n",
					gSitarAmtS, dbgExcPk, dbgPk);
			dbgCtr = 0; dbgPk = 0.0f; dbgExcPk = 0.0f;
		}
	}
}
