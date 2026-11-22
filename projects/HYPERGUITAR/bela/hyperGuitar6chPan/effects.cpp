/*
 * effects.cpp - the per-string effect blocks that used to be welded
 * directly into render()'s giant per-sample loop as inline code.
 * Extracted into real functions, called from render.cpp in the EXACT
 * SAME ORDER as before (effects are chained through `mixed`).
 *
 * ==================== CLICK AUDIT (see the smoothers below) ==========
 * Every block here reads globals that some OTHER thread writes as a
 * STEP - an encoder detent, or a slot pad through applySlotPreset() -
 * and reads per-voice values that updateModulation() rewrites once per
 * HOP (128 samples, 2.9 ms). render.cpp one-poles the on/off AMOUNTS
 * (gXxxAmtS) so engages ramp, but NOTHING smooths the VALUES. Consumed
 * per sample, each of those is a discontinuity:
 *   - a stepped GAIN is a click proportional to the signal;
 *   - a stepped DELAY LENGTH is a teleport into a different part of a
 *     ringing buffer, which is far louder than the gain case;
 *   - an on/off BRANCH taken on a threshold is a step of whatever the
 *     branch was contributing at that threshold - and for the ringing
 *     combs, "whatever it was contributing" is not small.
 * The point of use is the only place that can see the step, so all the
 * smoothing lives here. Each fix is commented with the step it kills.
 */
#include "hyperguitar.h"
#include <cmath>

/* ===================== per-sample parameter smoothers ===============
 *
 * File statics, not Voice fields: effects.cpp is the only owner, so no
 * header churn and no struct-layout change for the other agents. No
 * allocation, no branches on the audio thread beyond the seeding flag.
 *
 * Both helpers SEED from their target on first use, and a one-pole that
 * is already AT its target stays exactly there, so a parameter that
 * never moves is bit-identical to reading the raw global.
 */

/* ~10 ms. For gains/levels/blends, where the only requirement is that
 * the step becomes a ramp faster than the ear resolves it as an edge. */
#define SM_GAIN 0.0023f

/*
 * Delay lengths get a SLEW LIMIT, not a one-pole. Moving a delay tap at
 * rate r samples/sample IS a pitch shift of 1/(1+r), so what has to be
 * bounded is the RATE, not the time constant - and a one-pole's rate is
 * proportional to the size of the jump, which is exactly backwards.
 * 0.01/sample = at most ~17 cents of glide while the tap moves; 0.05 is
 * ~85 cents, audible as a short portamento but never as an edge.
 */
#define SLEW_TUNE  0.01f   /* voicing delays: chorus/vibrato depth */
#define SLEW_PITCH 0.05f   /* resonator/sustainer combs: retune, quickly */
#define TARAB_SLEW 0.06f   /* tarab bank: looser than SLEW_TUNE because the
                            * bank is retuned wholesale, see runSitarTarab */

static inline float smGain(float *st, float target)
{
	*st += SM_GAIN * (target - *st);
	return *st;
}

static inline float slewTo(float *st, float target, float rate)
{
	float d = target - *st;
	if(d > rate) d = rate;
	else if(d < -rate) d = -rate;
	*st += d;
	return *st;
}

/*
 * DISENGAGE EPSILON. The old threshold was 0.002: a ringing comb was
 * cut off while it was still contributing 0.002 * gain * ring, and with
 * gResGain/gSusLevel up at 2-3.5 and a near-unity-feedback comb that is
 * a plainly audible tick ~140 ms after the pad goes out (the probe
 * measured a 671x second-difference spike on SYMPATHETIC alone). The
 * amounts decay exponentially, so pushing the cut 5 more time constants
 * out costs ~120 ms of comb arithmetic on a voice that is already
 * fading, and shrinks the residual step by 200x.
 */
#define FX_OFF_EPS 1.0e-5f

/*
 * ============ SEED-AT-ENGAGE instead of SMOOTH-WHILE-BYPASSED =========
 *
 * Several smoothers below used to be advanced UNCONDITIONALLY, with the
 * comment "so a re-engage never picks up a stale depth". Correct goal,
 * expensive implementation: twelve effects x six strings of slew/one-pole
 * arithmetic every sample regardless of what is switched on. Measured on
 * the full-chain host bench with EVERYTHING OFF, effects.cpp executed
 * 941 line-ops/sample against the pre-session build's 412.
 *
 * The goal is met more cheaply - and in fact more exactly - by SEEDING at
 * the moment of engage rather than tracking continuously while bypassed.
 * A stale byte per string costs one store on the bypassed path, and the
 * first engaged sample writes the smoother straight onto the live target,
 * so it is not merely "not stale", it is exact.
 *
 * WHY THIS IS NOT A RE-INTRODUCED CLICK. Every seed happens on the first
 * sample where the effect's one-poled AMOUNT crosses its engage epsilon.
 * render.cpp ramps those amounts at 0.001/sample from zero, so at the
 * crossing the effect contributes ~1e-5 (1e-4 for vibrato) of its full
 * depth to the output. Snapping a smoother there is snapping it while it
 * is inaudible, which is exactly the case the smoothers were never for -
 * they exist to stop steps that happen while the effect is AUDIBLE, and
 * every one of those still runs, unchanged, inside the engaged branch.
 */
#define STALE_ALL { 1, 1, 1, 1, 1, 1 }   /* NUM_STRINGS, all stale at boot */

// FM: phase-modulate the ACTUAL string signal (index 2,
// modulator = sine at ratio x tracked f0)
void runFM(Voice *v, float *mixed)
{
	int s = (int)(v - gV);
	/*
	 * v->fmA is the modulator DEPTH IN SAMPLES of delay, and
	 * updateModulation() rewrites it once per HOP (one-poled at 0.3
	 * toward idx*period/2pi). Read per sample, that is a delay tap
	 * that sits still for 128 samples and then jumps - and on a note
	 * change, where `period` moves, the jump is tens of samples.
	 * Ramping LINEARLY across the hop reproduces the same staircase
	 * endpoints (so the FM index behaves identically) with none of
	 * the steps, and costs half a hop of lag on a parameter that is
	 * already smoothed over three.
	 */
	static float fmSm[NUM_STRINGS], fmTgt[NUM_STRINGS], fmStep[NUM_STRINGS];
	static float fmIncSm[NUM_STRINGS];
	static int fmSeeded = 0;
	if(!fmSeeded) {
		fmSeeded = 1;
		for(int i = 0; i < NUM_STRINGS; i++) {
			fmSm[i] = fmTgt[i] = gV[i].fmA;
			fmStep[i] = 0.0f;
			fmIncSm[i] = 2.0f * (float)M_PI * gFmRatioVal / gV[i].period;
		}
	}
	v->fmbuf[v->fmWrite & VBUF_MASK] = *mixed;
	/*
	 * `&& gFmActive` USED TO BE HERE. gFmAmtS is the one-poled amount
	 * that already encodes gFmActive, so the extra term did nothing on
	 * engage (amt rises from 0) but on DISENGAGE it bypassed the whole
	 * block in one sample while amt was still ~1.0 - a full-depth
	 * wet-to-dry cliff. Measured 340x on the probe; now the release
	 * rides the amount down like every other effect here.
	 */
	if(gFmAmtS > FX_OFF_EPS) {
		if(v->fmA != fmTgt[s]) {           // new hop target
			fmTgt[s] = v->fmA;
			fmStep[s] = (fmTgt[s] - fmSm[s]) * (1.0f / (float)HOP);
		}
		if((fmStep[s] > 0.0f && fmSm[s] < fmTgt[s])
				|| (fmStep[s] < 0.0f && fmSm[s] > fmTgt[s]))
			fmSm[s] += fmStep[s];
		else
			fmSm[s] = fmTgt[s];
		/*
		 * The modulator FREQUENCY is 2pi*ratio/v->period, and both
		 * terms step: v->period snaps at the hop boundary on every
		 * tracker relock, gFmRatioVal jumps a whole slot at a time.
		 * A frequency step is not a value step, but this frequency
		 * drives a DELAY TAP (d below), so a step in it is a step in
		 * the tap's VELOCITY - a kink the ear hears as a tick on
		 * every note change. Smoothing the increment makes the
		 * sidebands glide over ~10 ms into the new note instead.
		 */
		smGain(&fmIncSm[s], 2.0f * (float)M_PI * gFmRatioVal / v->period);
		v->fmPhase += fmIncSm[s];
		if(v->fmPhase > 2.0f * (float)M_PI)
			v->fmPhase -= 2.0f * (float)M_PI;
		float a = fmSm[s];
		float d = (a + 8.0f) + a * sinf(v->fmPhase);
		float fmOut = bufAtM(v->fmbuf, VBUF_MASK, v->fmWrite, d);
		*mixed = *mixed * (1.0f - gFmAmtS) + fmOut * gFmAmtS;
	}
	v->fmWrite++;
}

// AUTOWAH
void runAutowah(Voice *v, float *mixed)
{
	if(gEnvFAmtS > FX_OFF_EPS) {
		v->svfF += 0.01f * (v->svfFT - v->svfF);
		// dynamics-driven bite (Mu-Tron/Q-Tron style): a harder
		// pick tightens the resonance for more "quack", a quiet
		// note stays smoother - real envelope filters do this,
		// a static Q doesn't.
		//
		// qDyn steps (v->env has a zero-attack rise, and gEnvfQ jumps
		// a whole slot at a time) but it is NOT smoothed here on
		// purpose: q only appears in the BAND path, and the output
		// tap is v->svfLow, an integrator state. A step in q changes
		// the state's derivative, never its value, so it cannot
		// produce an output edge - and the probe agrees (zero hits
		// on this block through slot presses and a full Q sweep).
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
	int s = (int)(v - gV);
	static float resGainSm[NUM_STRINGS];
	static int resHarm[NUM_STRINGS];
	static int resSeeded = 0;
	if(!resSeeded) {
		resSeeded = 1;
		for(int i = 0; i < NUM_STRINGS; i++) {
			resGainSm[i] = gResGain;
			resHarm[i] = 2;
			/*
			 * v->resDelaySm is memset to 0 at setup, and combRun floors
			 * a delay at 8 samples - so an unseeded slew would start the
			 * comb ringing at 5.5 kHz and glide it DOWN to the note over
			 * a tenth of a second on the very first engage. (The old
			 * 0.25 one-pole did the same swoop 60x faster, which is why
			 * nobody caught it.) Seed it on the note instead.
			 */
			gV[i].resDelaySm = gV[i].period * 0.5f;
		}
	}
	if(gResAmtS > FX_OFF_EPS) {
		float re = v->env * gResSens;
		if(re > 1.0f) re = 1.0f;
		float x = 4.0f * re;
		int harm = 2 + (int)x;                // 2nd .. 6th harmonic
		if(harm > 6) harm = 6;
		/*
		 * HYSTERESIS. `harm` is a quantised function of a per-sample
		 * envelope, so a note whose dynamics sit on a boundary used
		 * to dither the comb length back and forth at envelope rate.
		 * Requiring the dynamics to be a clear 12% past the boundary
		 * before leaving the current harmonic makes the choice sticky
		 * without changing which harmonic a given attack picks.
		 */
		if(harm != resHarm[s]) {
			float lo = (float)(resHarm[s] - 2) - 0.12f;
			float hi = (float)(resHarm[s] - 2) + 1.12f;
			if(x > lo && x < hi) harm = resHarm[s];
		}
		resHarm[s] = harm;
		float dTarget = v->period / (float)harm;
		/*
		 * The old smoother here was a one-pole at 0.25 - tau of FOUR
		 * samples. Crossing a harmonic boundary moves this comb by
		 * period/6 (~100 samples on the low E) and a one-pole covers
		 * most of that inside a millisecond, which is a teleport, not
		 * a glide: the read jumps to an unrelated part of a ringing
		 * buffer. Rate-limiting instead bounds the only thing the ear
		 * actually hears - the glide is now a short portamento, ~85
		 * cents at its steepest, and it still arrives promptly
		 * because small changes complete immediately.
		 * v->period moves too (hop-rate tracker relocks), and the
		 * same limiter covers those.
		 */
		slewTo(&v->resDelaySm, dTarget, SLEW_PITCH);
		float ring = combRun(&v->resC, *mixed * 0.35f, v->resDelaySm,
				gResFb, 0.12f);
		// gResGain is a BUTTON: it cycles 2.2 -> 3.5 -> 1.0 in one
		// sample, straight onto a loudly ringing comb (measured 73x).
		*mixed += gResAmtS * smGain(&resGainSm[s], gResGain) * ring;
	} else {
		v->resC.buf[v->resC.wr & COMB_MASK] = 0.0f;
		v->resC.wr++; v->resC.lp = 0.0f;
		// stay seeded while bypassed, so re-engaging starts already on
		// the note instead of gliding onto it
		resGainSm[s] = gResGain;
		v->resDelaySm = v->period / (float)resHarm[s];
	}
}

// 13 JAWARI BUZZ: short metallic comb rattle, harder = buzzier.
// ADDED on top, so the dry level is never touched or clipped.
void runJawari(Voice *v, float *mixed)
{
	int s = (int)(v - gV);
	static float jawPitchSm[NUM_STRINGS], jawBuzzSm[NUM_STRINGS];
	static int jawSeeded = 0;
	if(!jawSeeded) {
		jawSeeded = 1;
		for(int i = 0; i < NUM_STRINGS; i++) {
			jawPitchSm[i] = gJawPitch;
			jawBuzzSm[i] = gJawBuzz;
		}
	}
	if(gJawAmtS > FX_OFF_EPS) {
		float je = v->env * gJawSens;
		if(je > 1.0f) je = 1.0f;
		float exc = tanhf(*mixed * (0.15f + 0.55f * je) * 1.3f) * (1.0f / 1.3f);
		float jfb = 0.86f + 0.08f * je;
		// gJawPitch IS the comb length. The aux encoder steps it 6
		// samples per detent and a slot press moves it 30+ at once -
		// each of those was a tap teleport inside the rattle. Slewed,
		// a detent becomes a ~14 ms glide, which is what turning a
		// "metallic pitch" knob should sound like anyway.
		float jd = slewTo(&jawPitchSm[s], gJawPitch, SLEW_TUNE);
		float ring = combRun(&v->jawC, exc, jd, jfb, 0.55f);
		// gJawBuzz multiplies the output directly; the main encoder
		// steps it 0.05 per detent and a slot jumps it outright.
		*mixed += gJawAmtS * smGain(&jawBuzzSm[s], gJawBuzz)
				* 2.0f * je * ring;
	} else {
		v->jawC.buf[v->jawC.wr & COMB_MASK] = 0.0f;
		v->jawC.wr++; v->jawC.lp = 0.0f;
		jawPitchSm[s] = gJawPitch;
		jawBuzzSm[s] = gJawBuzz;
	}
}

// 12 SUSTAINER: comb at the string's own period, near-unity feedback -
// grabs the fundamental instantly and holds it.
void runSustainer(Voice *v, float *mixed)
{
	int s = (int)(v - gV);
	static float susDlySm[NUM_STRINGS], susLevSm[NUM_STRINGS], susDampSm[NUM_STRINGS];
	static int susSeeded = 0;
	if(!susSeeded) {
		susSeeded = 1;
		for(int i = 0; i < NUM_STRINGS; i++) {
			susDlySm[i] = gV[i].period;
			susLevSm[i] = gSusLevel;
			susDampSm[i] = gSusDamp;
		}
	}
	if(gSusAmtS > FX_OFF_EPS) {
		float inputLevel = 0.15f + 0.35f * fminf(v->env / 0.5f, 1.0f);
		/*
		 * v->period is the TRACKER's period and it does not glide -
		 * it snaps at the hop boundary when the tracker locks a new
		 * note (deliberately; see pitch_tracker.cpp). This comb runs
		 * at near-unity feedback, so it is always ringing loudly, and
		 * feeding it a snapped delay was the single worst teleport in
		 * this file. Rate-limited it retunes as a fast portamento,
		 * which is what a resonator following the note should do.
		 */
		float sd = slewTo(&susDlySm[s], v->period, SLEW_PITCH);
		/*
		 * gSusDamp is the ONE live damping coefficient in this file,
		 * and combRun's damping state IS the output (`return c->lp`).
		 * Stepping it (0.45 -> 0.10 across two slots) does not step
		 * the output value but it steps its DERIVATIVE by
		 * damp_delta * (y - lp), which on a near-unity-feedback comb
		 * is a plain tick - 71x on the probe. Every other damp in
		 * this file is a compile-time constant, so this is the only
		 * one that needs it.
		 */
		float ring = combRun(&v->susC, *mixed * inputLevel, sd,
				v->susTime, smGain(&susDampSm[s], gSusDamp));
		// gSusLevel is a BUTTON: 1.8 -> 3.0 -> 0.9 in one sample, on
		// top of that same loud comb (measured 1338x on the probe).
		*mixed += gSusAmtS * smGain(&susLevSm[s], gSusLevel) * ring;
	} else {
		v->susC.buf[v->susC.wr & COMB_MASK] = 0.0f;
		v->susC.wr++; v->susC.lp = 0.0f;
		susDlySm[s] = v->period;
		susLevSm[s] = gSusLevel;
		susDampSm[s] = gSusDamp;
	}
}

// 15 SITAR - part 1: the jawari "zing" on the played note. The
// wide flat bridge throws energy into the high partials.
void runSitarZing(Voice *v, float *mixed)
{
	int s = (int)(v - gV);
	static float zLevSm[NUM_STRINGS], zBuzzSm[NUM_STRINGS];
	static int zSeeded = 0;
	if(!zSeeded) {
		zSeeded = 1;
		for(int i = 0; i < NUM_STRINGS; i++) {
			zLevSm[i] = gSitarLevel;
			zBuzzSm[i] = gSitarBuzz;
		}
	}
	if(gSitarAmtS > FX_OFF_EPS) {
		v->sitarHp += 0.22f * (*mixed - v->sitarHp);
		float bright = *mixed - v->sitarHp;      // high band
		// gSitarBuzz is a BUTTON (0.35 -> 0.70 -> ... -> 0.2) and
		// gSitarLevel an encoder; both multiply the added zing
		// directly, and gSitarBuzz also sets the drive.
		float bz = smGain(&zBuzzSm[s], gSitarBuzz);
		float lv = smGain(&zLevSm[s], gSitarLevel);
		float zing = tanhf(bright * (1.0f + 6.0f * bz));
		// gated by gSitarLevel too - MAIN is now the ONE wet
		// control for the whole pad, zing included
		*mixed += gSitarAmtS * lv * bz * 1.3f * zing;
	} else {
		zLevSm[s] = gSitarLevel;
		zBuzzSm[s] = gSitarBuzz;
	}
}

// 14 SYMPATHETIC: this string's comb is tuned to ITS OPEN pitch
// (a sitar's tarab strings are tuned to fixed scale notes) and
// is rung by the OTHER five strings' RAW signal - no feedback.
void runSympathetic(Voice *v, int s, const float *dryIn, float *mixed)
{
	// NOTE: this comb's delay is v->p0, the string's FIXED open period.
	// It is the one comb in this file that never retunes, so it needs no
	// slew limiter. gSympCoupling/gSympW only scale the INJECTION, which
	// reaches the output a full delay later through combRun's damping
	// lowpass - a step there cannot become an output edge.
	if(gSympAmtS > FX_OFF_EPS) {
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
//
// The SETTLE is smooth (sagEnv decays per sample), but the SPIKE is not:
// updateModulation() slams v->sagEnv to 1.0 at a hop boundary on every
// detected pick, which moves this tap by the full depth - up to 441
// samples, 10 ms - in a single sample. That is a jump from the live
// attack to pre-attack silence, i.e. the loudest possible edge, and it
// fired on literally every pick (measured 218-278x). Depth changes from
// the encoder/slot land the same way.
//
// The tap has to make that jump - it IS the effect - so it is crossfaded
// rather than slewed: slewing would replace the click with a downward
// pitch dip that the effect is not supposed to have. Two reads for 5.8 ms
// after a jump, one the rest of the time, and NO added latency.
#define SAG_XFADE 256
void runPluckSag(Voice *v, float *mixed, float sr)
{
	int s = (int)(v - gV);
	static float sagPrev[NUM_STRINGS], sagOld[NUM_STRINGS], sagPh[NUM_STRINGS];
	static int sagSeeded = 0;
	if(!sagSeeded) {
		sagSeeded = 1;
		for(int i = 0; i < NUM_STRINGS; i++) {
			sagPrev[i] = -1.0f;   // < 0 = no previous tap, do not fade
			sagOld[i] = 4.0f;
			sagPh[i] = 1.0f;      // >= 1 = not fading
		}
	}
	v->sagBuf[v->sagWrite & VBUF_MASK] = *mixed;
	if(gSagAmtS > FX_OFF_EPS) {
		float sagDepthSmp = gSagDepthSec * sr;
		float dly = 4.0f + sagDepthSmp * v->sagEnv;
		/*
		 * 1 sample is a safe jump detector: the decaying settle moves
		 * this tap by depth*(1-sagDecayCoef) per sample, which is
		 * 0.04 samples even at max depth and the fastest settle.
		 */
		if(sagPh[s] >= 1.0f && sagPrev[s] >= 0.0f
				&& fabsf(dly - sagPrev[s]) > 1.0f) {
			sagOld[s] = sagPrev[s];
			sagPh[s] = 0.0f;
		}
		sagPrev[s] = dly;
		float sagged;
		if(sagPh[s] < 1.0f) {
			// old tap frozen for the fade: a constant delay is just
			// a fixed tap, it cannot generate an edge of its own
			float a = bufAtM(v->sagBuf, VBUF_MASK, v->sagWrite, sagOld[s]);
			float b = bufAtM(v->sagBuf, VBUF_MASK, v->sagWrite, dly);
			sagged = a + sagPh[s] * (b - a);
			sagPh[s] += 1.0f / (float)SAG_XFADE;
		} else {
			sagged = bufAtM(v->sagBuf, VBUF_MASK, v->sagWrite, dly);
		}
		*mixed = *mixed * (1.0f - gSagAmtS) + sagged * gSagAmtS;
	} else {
		sagPrev[s] = -1.0f;
		sagPh[s] = 1.0f;
	}
	v->sagWrite++;
	v->sagEnv *= v->sagDecayCoef;
}

// VIBRATO (NEPTR unipolar sine sweep). Depth follows the string's own
// envelope (floored, never fully off) so it naturally eases as a note
// decays instead of wobbling a dead/dying string at full throw.
//
// LATENCY: this used to read the delay line UNCONDITIONALLY, so with
// vibrato switched OFF every string still paid a permanent 4-sample read
// plus a cubic interpolation - that WAS the whole measured 4-sample dry
// path latency, spent on a delay that was never being modulated. Now the
// line is still written every sample (so it stays warm and re-engaging
// never reads a hole) but the signal passes through untouched until
// vibrato is actually asked for.
//
// The engage/disengage seam is the delicate part, because switching
// straight from "no delay" to "4 samples of delay" is a 90us phase step -
// a faint tick. Two things kill it:
//   * the 4-sample base is scaled by gVibAmtS (which render.cpp already
//     one-poles at 0.001/sample), so the base slides 0 -> 4 continuously
//     instead of stepping. x8 => fully open by gVibAmtS = 0.125, about
//     3ms in: a 0.03 samples/sample sweep, three orders of magnitude
//     slower than the vibrato sweep itself and inaudible. On release the
//     one-pole's exponential tail stretches the same 4 samples over
//     ~100ms, so the delay is already < 0.01 samples when we cut out.
//   * below 2 samples of delay the cubic interpolator's two forward taps
//     would read the far end of the ring (stale audio). Duplicating the
//     newest sample into the next two slots - they are overwritten by the
//     real samples on the following calls - makes the interpolation a
//     clean hold, and delay == 0 then returns the input bit-exactly.
//
// DEPTH was the part that still clicked. The sweep AMPLITUDE is
// vibDepthSmp * (envelope factor): gVibDepthSec jumps 8.8 samples per
// encoder detent and 100+ on a slot press, and the envelope factor steps
// because v->env has a zero-attack rise - so every pick moved the tap by
// tens of samples in one sample (measured 28-160x). The amplitude is now
// rate-limited; the LFO itself is untouched, and since the LFO sweeps at
// most 0.08 samples/sample (VIB_HZ is under 1 Hz) the limiter is nowhere
// near it. It is advanced unconditionally so re-engaging never finds a
// stale depth.
//
// THE ONE SMOOTHER THAT MUST STAY UNCONDITIONAL. Every other bypassed
// smoother in this file was converted to seed-at-engage (see the note at
// the top) and measured bit-identical through slot presses and encoder
// detents, because their targets are STATIC parameters: whether you track
// a still target continuously or stamp it once on engage, you land on the
// same number. This one's target is not static - it rides v->env - so
// seeding it at engage snapped straight past the 240 ms depth glide the
// slew performs after a slot press (measured: -26.9 dB rel. rms, the only
// deviation in the whole A/B, and it is the depth trajectory itself, not
// a click). Six slewTo() calls a sample is the price of that trajectory.
#define VIB_ENGAGE_EPS 0.0002f
void runVibrato(Voice *v, int s, float *mixed, float vibDepthSmp, float sr)
{
	static float vibDepSm[NUM_STRINGS];
	static int vibSeeded = 0;
	if(!vibSeeded) {
		vibSeeded = 1;
		for(int i = 0; i < NUM_STRINGS; i++)
			vibDepSm[i] = vibDepthSmp * 0.4f;
	}
	v->vbuf[v->vWrite & VBUF_MASK] = *mixed;
	float depTarget = vibDepthSmp
			* (0.4f + 0.6f * fminf(v->env * 3.0f, 1.0f));
	float dep = slewTo(&vibDepSm[s], depTarget, SLEW_TUNE);
	if(gVibAmtS > VIB_ENGAGE_EPS) {
		// gVibAmtS still multiplies the depth, so the release still
		// drives A to zero on exactly the old schedule - the verified
		// engage/disengage seam is unchanged.
		float A = gVibAmtS * dep;
		float base = gVibAmtS * 8.0f;
		if(base > 1.0f) base = 1.0f;        // == 1 whenever vibrato is on
		float dly = 4.0f * base + A * (1.0f + sinf(v->vPhase));
		if(dly < 2.0f) {                    // engage/release seam only
			v->vbuf[(v->vWrite + 1) & VBUF_MASK] = *mixed;
			v->vbuf[(v->vWrite + 2) & VBUF_MASK] = *mixed;
		}
		*mixed = bufAtM(v->vbuf, VBUF_MASK, v->vWrite, dly);
	}
	// the LFO free-runs whether or not we are reading it, exactly as
	// before, so re-engaging picks up the phase the sweep would have had
	v->vWrite++;
	v->vPhase += 2.0f * (float)M_PI * VIB_HZ[s] * gVibRateScale / sr;
	if(v->vPhase > 2.0f * (float)M_PI)
		v->vPhase -= 2.0f * (float)M_PI;
}

// POG SWELL: per-note ~100ms attack bloom
#define POG_FADE 1024              /* 23 ms retrigger fade, see below */
void runPogSwell(Voice *v, float *mixed, float pogStep)
{
	int s = (int)(v - gV);
	static float pogG[NUM_STRINGS], pogModSm[NUM_STRINGS];
	static float pogFrom[NUM_STRINGS], pogU[NUM_STRINGS];
	static unsigned char pogStale[NUM_STRINGS] = STALE_ALL;
	/*
	 * The SWELL STATE itself (v->pogGain) keeps climbing whether or not
	 * the pad is on - it is Voice state that updateModulation() retriggers
	 * and other code reads, and it is three ops. What is now inside the
	 * engaged branch is the RETRIGGER-FADE machinery below, whose only
	 * output is pogG[], which nothing outside this branch reads.
	 */
	if(v->pogGain < 1.0f) {
		v->pogGain += pogStep;
		if(v->pogGain > 1.0f) v->pogGain = 1.0f;
	}
	if(gPogAmtS <= FX_OFF_EPS) {
		pogStale[s] = 1;
		return;
	}
	if(pogStale[s]) {
		pogG[s] = v->pogGain;      // seed exactly on the live swell
		pogFrom[s] = pogG[s];
		pogModSm[s] = gPogModDepth;
		pogU[s] = 1.0f;            // >= 1 = not fading
		pogStale[s] = 0;
	}
	/*
	 * THE RETRIGGER CLIFF - by far the worst edge in this file (the
	 * probe measured 11113x, an order of magnitude past anything else).
	 * updateModulation() slams v->pogGain from 1.0 to 0.05 at a hop
	 * boundary on every detected pick, so a note that is still ringing
	 * loudly gets muted 26 dB in ONE sample before the swell restarts.
	 *
	 * A plain rate limit is not enough here: it removes the value step
	 * but leaves a CORNER at each end of the fade, and a gain whose
	 * slope changes abruptly still ticks (494x on the probe with a
	 * 2 ms linear fade). A SMOOTHSTEP has zero slope at both ends, so
	 * the departure from the old gain and the arrival on the new one
	 * are both C1 - and because the destination is re-read every
	 * sample, the arrival lands exactly on v->pogGain however far it
	 * has climbed in the meantime. What is left after that is the
	 * curve's own bounded curvature, 6*delta/N^2, so the length is
	 * chosen from what that has to be: 5.8 ms still measured -24 dB
	 * under the local programme peak, 23 ms puts it at -39 dB. A
	 * quintic (C2 ends) was tried and measured identically, which
	 * confirms it is the mid-curve curvature and not the joins.
	 * 23 ms is invisible against a 180-450 ms swell, and after it
	 * this tracks v->pogGain bit-exactly again, so the swell shape -
	 * the whole effect - is untouched.
	 */
	if(pogU[s] >= 1.0f && v->pogGain < pogG[s] - 1.0e-4f) {
		pogFrom[s] = pogG[s];
		pogU[s] = 0.0f;
	}
	if(pogU[s] < 1.0f) {
		float u = pogU[s];
		float ss = u * u * (3.0f - 2.0f * u);
		pogG[s] = pogFrom[s] + (v->pogGain - pogFrom[s]) * ss;
		pogU[s] += 1.0f / (float)POG_FADE;
	} else {
		pogG[s] = v->pogGain;
	}
	{
		// THE SWELL: volume envelope fading in over gPogAttackSec.
		float pg = pogG[s] * pogG[s]; // perceptual curve
		float voiced;
		/*
		 * The filter-sweep option used to be a hard branch on
		 * |gPogModDepth| > 0.05, so the encoder detent that crossed
		 * it switched a fully audible 1.2 kHz lowpass in or out in
		 * one sample. It is now a WEIGHT that reaches 1 at the same
		 * 0.05 and falls to 0 by 0.02: at and above 0.05 the result
		 * is what it always was, below 0.02 it is the untouched dry
		 * swell as before, and the branch is only skipped where its
		 * contribution is already exactly zero - so there is nothing
		 * left to step. gPogModDepth itself is smoothed so the
		 * detent that moves the cutoff cannot step it either.
		 */
		float dm = smGain(&pogModSm[s], gPogModDepth);
		float w = (fabsf(dm) - 0.02f) * (1.0f / 0.03f);
		if(w > 1.0f) w = 1.0f;
		if(w > 0.0f) {
			// optional filter sweep riding the swell's timing
			float cutoffHz = 1200.0f * exp2f(dm * pogG[s]);
			if(cutoffHz < 200.0f) cutoffHz = 200.0f;
			if(cutoffHz > 8000.0f) cutoffHz = 8000.0f;
			float fcoef = 2.0f * sinf((float)M_PI * cutoffHz / v->srate);
			if(fcoef > 1.0f) fcoef = 1.0f;
			v->pogLpZ += fcoef * (*mixed - v->pogLpZ);
			voiced = (*mixed + w * (v->pogLpZ - *mixed)) * pg;
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
	// chorDepthSmp is gChorDepthSec * sr: 13 samples per encoder detent
	// and 110+ on a slot press, applied straight to the sweep tap
	// (measured 307x). Rate-limited it becomes a depth glide; the
	// triangle LFO is untouched and sweeps at most 0.1 samples/sample,
	// well inside the limit. SEEDED AT ENGAGE, not advanced while
	// bypassed (see the seed-at-engage note at the top of this file).
	static float chorDepSm[NUM_STRINGS];
	static unsigned char chorStale[NUM_STRINGS] = STALE_ALL;
	v->cbuf[v->cWrite & VBUF_MASK] = *mixed;
	float wide = 0.0f;
	if(gChorAmtS > FX_OFF_EPS) {
		if(chorStale[s]) {
			chorDepSm[s] = chorDepthSmp;
			chorStale[s] = 0;
		}
		float cdep = slewTo(&chorDepSm[s], chorDepthSmp, SLEW_TUNE);
		float ct = v->cPhase * (0.5f / (float)M_PI); // 0..1
		float tri = ct < 0.5f ? (4.0f * ct - 1.0f) : (3.0f - 4.0f * ct);
		float cd = chorBaseSmp + cdep * (1.0f + tri);
		wide = bufAtM(v->cbuf, VBUF_MASK, v->cWrite, cd);
	} else {
		chorStale[s] = 1;
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
	/*
	 * gTarabDelay[] is not a user parameter - render.cpp REBUILDS all
	 * 14 of them once per block whenever the loudest stable string
	 * changes, so the whole bank retunes mid-ring. Measured on the probe,
	 * a handover moves these taps 18, 58, 65, 92 samples IN ONE SAMPLE -
	 * fourteen simultaneous comb teleports, which is precisely the
	 * "shimmer that ticks when you change notes" (it fired at every
	 * lead-string handover, not at any event the probe scheduled).
	 *
	 * Rate limiting ONLY - deliberately no one-pole in front of it. The
	 * bank is also nudged a fraction of a sample every hop as the
	 * tracker breathes, and that fine motion is part of the shimmer's
	 * voicing: filtering it out measured 1.6 dB of spectral deviation
	 * across the bank, where a bare slew measures 0.13 dB. A slew passes
	 * the small motion through essentially untouched and bounds only the
	 * handovers, turning a 92-sample teleport into a 35 ms glide - which
	 * is how a real tarab bank behaves when it is retuned.
	 */
	static float tdSm[TARAB_N];
	static float lvSm;
	static int tSeeded = 0;
	if(gSitarAmtS > FX_OFF_EPS) {
		/* seed at engage, not every bypassed sample: the old else-branch
		 * below re-stamped all fourteen taps plus the level once per
		 * sample forever, which is fifteen stores per sample of a bank
		 * that is not being read at all. Same guarantee, 1/15th the cost. */
		if(!tSeeded) {
			tSeeded = 1;
			for(int k = 0; k < TARAB_N; k++) tdSm[k] = gTarabDelay[k];
			lvSm = gSitarLevel;
		}
		float exc = 0.0f;
		for(int s = 0; s < NUM_STRINGS; s++)
			exc += dryIn[s];
		exc *= 0.32f;
		if(exc > 1.5f) exc = 1.5f;
		if(exc < -1.5f) exc = -1.5f;
		float tl = 0.0f, tr = 0.0f;
		for(int k = 0; k < TARAB_N; k++) {
			float d = slewTo(&tdSm[k], gTarabDelay[k], TARAB_SLEW);
			float ring = combRun(&gTarab[k], exc, d,
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
		// (smoothed: the MAIN encoder steps it 0.02 per detent onto a
		// bank of 0.999-feedback combs)
		float g = gSitarAmtS * smGain(&lvSm, gSitarLevel);
		*outL += g * hiL;
		*outR += g * hiR;
		/*
		 * NOTHING PRINTS FROM HERE. This block used to end with a
		 * SITAR-DBG rt_printf firing every 22050 samples (~2/s) while
		 * the sitar pad was on. rt_printf on the Xenomai RT audio
		 * thread can force a secondary-mode switch: it perturbs the
		 * block deadline without necessarily registering as a formal
		 * xrun, which is exactly the "some clicks, but zero underruns
		 * in the log" signature. Bench instrumentation belongs behind
		 * gRawDiag on the AuxiliaryTask, never inline here.
		 */
	} else {
		tSeeded = 0;
	}
}
