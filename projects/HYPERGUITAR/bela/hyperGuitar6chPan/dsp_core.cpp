/*
 * dsp_core.cpp - shared low-level DSP primitives, split out of the
 * former monolithic render.cpp. Pure mechanical extraction: bodies are
 * byte-for-byte identical to the original, only made non-static (or
 * moved from "static inline") where another file now needs to call
 * them, and given hyperguitar.h prototypes.
 */
#include "hyperguitar.h"
#include <cmath>

/*
 * (2026-08-14) The per-sample analysis amortizers - trackPitchSlice() /
 * lpcAnalyzeSlice() and their Rearm() partners - are GONE. They spread
 * each hop's correlation work across the 128 samples of the previous hop
 * to lower PEAK cost so the audio block could be shrunk below 64; we are
 * staying at 64, so the instrument was paying the mean-cost half of that
 * trade (measured 2.04x on the tracker's inner loop, the hottest loop in
 * the engine) for a latency benefit nobody was taking. See the header
 * comment in pitch_tracker.cpp for the full measurement and for what the
 * peak-cost consequence is if block 32/16 is revisited.
 */

float pblep(float t, float dt)
{
	if(t < dt) {
		float x = t / dt;
		return x + x - x * x - 1.0f;
	}
	if(t > 1.0f - dt) {
		float x = (t - 1.0f) / dt;
		return x * x + x + x + 1.0f;
	}
	return 0.0f;
}

#ifdef HG_OPCOUNT
/* host-only: bufAtM call counter + per-sample (all-strings) tally.
 * Never compiled on the device. */
unsigned long gOpCalls = 0, gOpSlotMax = 0, gOpTotal = 0, gOpSamples = 0;
static unsigned long sOpSlot = 0;
static unsigned int sOpStamp = 0xffffffffu;
static void opSlotNote(unsigned long d)
{
	if(gSampleStamp != sOpStamp) {
		if(sOpStamp != 0xffffffffu) {
			gOpTotal += sOpSlot;
			gOpSamples++;
			if(sOpSlot > gOpSlotMax) gOpSlotMax = sOpSlot;
		}
		sOpSlot = 0;
		sOpStamp = gSampleStamp;
	}
	sOpSlot += d;
}
#endif

float bufAtM(const float *buf, unsigned int mask,
		unsigned int wr, float delay)
{
#ifdef HG_OPCOUNT
	gOpCalls++;
#endif
	float readPos = (float)(wr & mask) - delay;
	int i1 = (int)floorf(readPos);
	float fr = readPos - (float)i1;
	float y0 = buf[(i1 - 1) & mask];
	float y1 = buf[i1 & mask];
	float y2 = buf[(i1 + 1) & mask];
	float y3 = buf[(i1 + 2) & mask];
	float a = 0.5f * (-y0 + 3.0f * y1 - 3.0f * y2 + y3);
	float b = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
	float c = 0.5f * (y2 - y0);
	return ((a * fr + b) * fr + c) * fr + y1;
}

float combRun(Comb *c, float in, float delay, float fb, float damp)
{
	if(delay < 8.0f) delay = 8.0f;
	if(delay > (float)(COMB_SIZE - 8)) delay = (float)(COMB_SIZE - 8);
	float y = bufAtM(c->buf, COMB_MASK, c->wr, delay);
	c->lp += damp * (y - c->lp);        // damping = string/air losses
	float v = in + c->lp * fb;
	if(v > 4.0f) v = 4.0f;              // bounded: cannot run away
	if(v < -4.0f) v = -4.0f;
	c->buf[c->wr & COMB_MASK] = v;
	c->wr++;
	return c->lp;
}

/* -------------------- per-sample voice processing -------------------- */
/*
 * The pitch/formant work all happens in shifterRun() (shifter.cpp) - one
 * unified pitch-synchronous engine per string. This function owns the
 * front end (DC block, onset detector, buffers, tracker feed) and the
 * back end (auto-EQ tilt, optional LPC formant mode).
 */

void voiceProcess(Voice *v, float in, int dryNeeded,
		float *wetOut, float *dryOut)
{
	(void)dryNeeded;
	float x = in - v->dcX1 + 0.995f * v->dcY1;
	v->dcX1 = in;
	v->dcY1 = x;
	float full = x;

	float ax = fabsf(full);
	v->env = ax > v->env ? ax : v->env * 0.9995f;

	/* per-sample onset detector: fast/slow envelope ratio with period-
	 * scaled time constants (fast ~0.3 periods, slow ~4 periods). This is
	 * what makes attacks land within a few ms instead of a hop later. */
	Shifter *sh = &v->sh;
	sh->envF += sh->onsCF * (ax - sh->envF);
	sh->envS += sh->onsCS * (ax - sh->envS);
	if(sh->onsetRefract > 0)
		sh->onsetRefract--;
	/*
	 * threshold 2.2: high enough that vibrato/tremolo dynamics never
	 * fire it - a false onset is far worse than a slightly late one.
	 *
	 * THE ADDITIVE FLOOR IS A NOISE FLOOR, NOT A MUSICAL ONE (2026-09-15).
	 *
	 * It was 0.004, and 0.004 is not a converter noise floor - it is a
	 * LEVEL, and one picked on host-bench material whose plucks run
	 * amp 0.10..0.32. The instrument is ~10x quieter than that, and the
	 * hex pickup delivers the two PLAIN top strings several dB below the
	 * four wound ones on top of it: a ringing top string sits at env
	 * ~0.01-0.015, so envF at a pick is ~0.006-0.009 and the old floor
	 * alone ate 45-90% of it. The ratio test then needed 3.5-5.0 instead
	 * of 2.2 and most picks simply never registered as onsets at all.
	 *
	 * MEASURED, host bench, identical 7 s of material, scene 6 (+12 and
	 * formant), counting shifterOnset calls against the generator's own
	 * pluck count, at device level (0.1x plus the pickup taper) with a
	 * 3e-4 converter noise floor:
	 *
	 *                s0    s1    s2    s3    s4(B)  s5(e)
	 *   picks        14     9    10    13    15     13
	 *   floor 0.004   4     6     2     4     1      1     <- 7%, 8%
	 *   floor 4e-4    6     8     5     7     6      7     <- 40%, 54%
	 *
	 * i.e. the two strings the player says sound worst were losing
	 * ~90% of their picks. A missed onset is not a missed nicety: the
	 * engine never enters TRANS, so the transient is never passed
	 * through the direct tap, attackBlank is never armed and the tracker
	 * chases the pick transient, and the mark pickers are never reset
	 * onto the new note - the attack is granulated from the PREVIOUS
	 * note's lattice. That is the smeared, pixely top end.
	 *
	 * 4e-4 is chosen as a converter/crosstalk floor rather than a level:
	 * it is the same order as GATE_RMS (3e-4), the bar the engine
	 * already calls silence. Verified in the other direction too - with
	 * every channel carrying 3e-4 of white noise and NO plucks, all six
	 * strings produce ZERO onsets over 7 s, the same as the old floor.
	 */
	/*
	 * RELATIVE GATE (2026-10-01, Cycfi's rule): a pick on THIS string must
	 * also put this string within 12 dB of the loudest OTHER string. Live
	 * log mining showed 26-32% of the plain strings' onsets firing on a
	 * string carrying no note of its own - a neighbour's -23 dB bleed
	 * passing the ratio test - and every ghost re-armed the refractory,
	 * TRANS, attackBlank, the LPC snap and the synth. gOtherMax is the
	 * per-block max of the other strings' peak-hold env; v->env is this
	 * string's, already updated this sample. Refractory 50 -> 25 ms: a
	 * real pick inside a strum must not be eaten by the previous one.
	 */
	/*
	 * THE DECAY-TAIL GHOST (2026-10-01, device log, 1240 onsets in one
	 * 30 s take): 70% of the B/e onsets fired 150-400 ms INTO a note at
	 * envF 0.0007-0.002 against envS 0.0001-0.0005 - noise on the decay
	 * tail passing the ratio test over a 4e-4 floor that is below the
	 * channels' real noise. Every re-fire restarts the engine (TRANS, the
	 * attack blank, the LPC snap, the synth) = the "restart / bad loop" on
	 * the plain strings, which survived two engine rewrites because this
	 * detector was in both. Real picks on the plain strings read envF
	 * 0.006-0.04, so: floor 2.5e-3, and the fast follower must also reach
	 * a quarter of the string's own peak-hold env (a real pick lifts env
	 * instantly; a tail blip sits far under it).
	 */
	if(sh->onsetRefract == 0 && sh->envF > 2.2f * sh->envS + 2.5e-3f
			&& sh->envF > 0.25f * v->env
			&& v->env > 0.25f * gOtherMax[(int)(v - gV)]) {
		shifterOnset(v, (int)(v - gV));
		sh->onsetRefract = (int)(0.025f * v->srate);
	}

	/* tracker front end: the 2-biquad lowpass now feeds ONLY the /4
	 * decimated pitch buffer (as its anti-alias filter) - the audio path
	 * is full-band through the shifter, so pick noise gets shifted WITH
	 * the note instead of pasted on top of it. */
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

	/*
	 * v8 RESOURCE GATE: the whole shifter back end (the single hottest loop
	 * in this engine - see the file-header note) is skipped while nothing
	 * needs the wet leg. gWetEngineActive covers the target AND the still-
	 * decaying smoothed gWetS, so a fade-out is never cut short - see
	 * render.cpp. dryOut/full above is always computed regardless (DC
	 * block, onset detection, envelope - every other effect depends on
	 * those), only the pitch/formant-specific back end pauses.
	 */
	float p = full;
	if(gWetEngineActive) {
#ifdef HG_OPCOUNT
		unsigned long opBefore = gOpCalls;
#endif
		p = shifterRun(v, full);
#ifdef HG_OPCOUNT
		opSlotNote(gOpCalls - opBefore);
#endif

		// LPC formant mode FIRST: the envelope analysis/filter must see the
		// UNSHAPED shifter output, not a tilted copy of it.
		if(gFormantActive && gFormantMode == 1)
			p = lpcProcess(v, p);

		// AUTO EQ tilt (the H90 trick), static per interval - unity at no shift.
		// After lpcProcess on purpose (see above).
		v->tiltLp += 0.16f * (p - v->tiltLp);
		p = v->tiltLp * v->tiltLoG + (p - v->tiltLp) * v->tiltHiG;
	}

	*wetOut = p;
	*dryOut = full;
	v->writePos++;
}

/* ------------------- filters / modulation ------------------- */

void updateLP(Voice *v)
{
	static const float Q[2] = { 0.54119610f, 1.30656296f };
	float fc = v->harmCut * v->srate / v->period;
	if(fc > 0.40f * v->srate) fc = 0.40f * v->srate;
	if(fc < 60.0f) fc = 60.0f;
	v->fcCur += 0.25f * (fc - v->fcCur);
	fc = v->fcCur;
	float w0 = 2.0f * (float)M_PI * fc / v->srate;
	float cw = cosf(w0), sw = sinf(w0);
	for(int sec = 0; sec < 2; sec++) {
		float alpha = sw / (2.0f * Q[sec]);
		float a0inv = 1.0f / (1.0f + alpha);
		float b1 = (1.0f - cw) * a0inv;
		v->lpCoef[sec][0] = 0.5f * b1;
		v->lpCoef[sec][1] = b1;
		v->lpCoef[sec][2] = 0.5f * b1;
		v->lpCoef[sec][3] = -2.0f * cw * a0inv;
		v->lpCoef[sec][4] = (1.0f - alpha) * a0inv;
	}
}

void updateModulation(Voice *v, int s)
{
	// pitch: straightforward static shifters only (RANDOM/DYNAMIC archived)
	// BEND (v8): attack pitch-envelope multiplies on top, riding the SAME
	// real shifter PITCH uses - not a delay trick like PLUCKSAG. Active even
	// with PITCH off (applyMixRouting() opens the wet leg for gBendOn too).
	float bendMul = gBendOn
			? powf(2.0f, (gBendDepthSemis * v->bendEnv) / 12.0f) : 1.0f;
	// JOYSTICK WHAMMY (v8): same multiplicative-offset principle, continuous
	// rather than attack-triggered - compounds on top of whatever PITCH/BEND
	// already set, exactly like a real whammy bar bends whatever note is
	// already sounding. gJoyWhammySemis is 0 at rest (hg_ctrl.cpp's own
	// small deadzone), so this is a no-op multiply-by-1 when untouched.
	float whammyMul = powf(2.0f, gJoyWhammySemis / 12.0f);
	float betaT = (gPitchActive ? gBetaBase[s] : 1.0f) * bendMul * whammyMul;
	float formT = gFormantActive ? gFormantBase[s] : 1.0f;

	// FORMANT WAH: matched dynamic sensitivity, wider throw
	if(gFWahOn) {
		float e = v->env * gFWahSens;
		if(e > 1.0f) e = 1.0f;
		formT *= 0.50f + 1.55f * e;   // DRAMATIC: huge vowel swing on attack
	}
	// TALKBOX's identity: a slow, gentle vowel-breathing wobble
	if(gPresetFormantWobble) {
		v->formantWobblePhase += 2.0f * (float)M_PI * 0.15f / v->srate;
		if(v->formantWobblePhase > 2.0f * (float)M_PI)
			v->formantWobblePhase -= 2.0f * (float)M_PI;
		formT *= 1.0f + 0.04f * sinf(v->formantWobblePhase);
	}
	if(formT < 0.40f) formT = 0.40f;
	if(formT > 2.40f) formT = 2.40f;

	v->beta += 0.3f * (betaT - v->beta);
	// FORMANT-WAH needs to track the pick attack, not ease into it
	float formGlide = gFWahOn ? 0.20f : gFormantGlide;
	v->formant += formGlide * (formT - v->formant);

	/*
	 * AUTO EQ (the H90's trick): a down-shift darkens and smears the
	 * attack, so tilt the shifted leg BRIGHTER; an up-shift thins it,
	 * so tilt it back toward the lows. Pivot ~1.1kHz, scale with how
	 * far the shift is from unity (neutral at beta=1).
	 */
	float tt = log2f(v->beta);
	if(tt > 1.0f) tt = 1.0f;
	if(tt < -1.0f) tt = -1.0f;
	float tf = log2f(v->formant) * gFormantTiltBias;
	if(tf > 1.0f) tf = 1.0f;
	if(tf < -1.0f) tf = -1.0f;
	v->tiltLoG = 1.0f + 0.20f * tt + 0.20f * tf; // down: lows eased, up: body restored
	v->tiltHiG = 1.0f - 0.35f * tt - 0.35f * tf; // down: highs boosted, up: fizz eased

	// AUTOWAH sweep - sensitivity halved per feel
	float e = v->env * gEnvfSens;
	if(e > 1.0f) e = 1.0f;
	float fcEnv = ENVF_FMIN * exp2f(ENVF_OCTS * e);
	v->svfFT = 2.0f * sinf((float)M_PI * fcEnv / v->srate);

	// FM modulator geometry: A = INDEX * period/(2*pi*ratio)
	if(gFmActive) {
		float ratio = gFmRatioVal < 0.05f ? 0.05f : gFmRatioVal;
		float idx = gFmIndexVal;
		float idxMax = 0.85f / ratio;
		if(idx > idxMax) idx = idxMax;
		float A = idx * v->period / (2.0f * (float)M_PI);
		if(A > FM_MAX_A) A = FM_MAX_A;
		v->fmA += 0.3f * (A - v->fmA);
	}

	// SYNTH LEAD: fundamental-locked portamento (ignores attack jitter)
	v->sinePer += 0.06f * (v->period - v->sinePer);
	float se = v->env * SINE_ENV_SENS;
	if(se > 1.0f) se = 1.0f;
	v->sineEnvSm += 0.35f * (se - v->sineEnvSm);

	// POG onset detection: retrigger the ~100ms bloom on each new pick
	if(v->pogRefract > 0)
		v->pogRefract--;
	if(v->pogRefract == 0 && v->env > 2.0f * v->envHold + 0.004f) {
		v->pogGain = 0.05f;
		v->pogRefract = 20; // ~58ms at HOP rate
	}
	v->envHold = v->env > v->envHold ? v->env : v->envHold * 0.985f;

	/*
	 * STEREO SPREAD: pad OFF = mono (everything centred). ON = the
	 * per-string pan pattern scaled by MAIN, plus AUX dynamic panning.
	 */
	{
		float pan = 0.0f;
		// OCT DOWN collapses the image to mono: a sub-octave voice
		// smeared across the field reads as two bass amps fighting;
		// centred it reads as one big one. Any other pitch voicing
		// keeps the spread; toggling pitch off restores it (the 5ms
		// point-of-use pan smoother glides both transitions).
		int octDown = gPitchActive && gPitchSemitones < -11.5f;
		if(gPanOn && !octDown) {
			pan = STRING_PAN[s] * gPanSpread;
			if(gPanDyn > 0.001f) {
				float e2 = v->env * 3.0f;
				if(e2 > 1.0f) e2 = 1.0f;
				float dir = STRING_PAN[s] >= 0.0f ? 1.0f : -1.0f;
				pan += dir * gPanDyn * e2;
			}
			if(pan < -1.0f) pan = -1.0f;
			if(pan > 1.0f) pan = 1.0f;
		}
		float angle = (pan + 1.0f) * 0.25f * (float)M_PI;
		v->gainL = cosf(angle);
		v->gainR = sinf(angle);
	}

	/*
	 * TONAL GATE + CROSSTALK REJECTION (see original render.cpp history
	 * for the full rationale, preserved verbatim in git log).
	 */
	{
		// stability measured on the RAW accepted reads, not the smoothed
		// period (the smoothing itself used to fake instability mid-walk)
		float dev2 = fabsf(v->periodGood - v->rawLast) / (v->periodGood + 1e-9f);
		v->rawLast = v->periodGood;
		int dominant = v->env > 0.5f * gOtherMax[s];
		if(v->pitchConf > PITCH_CONF_MIN && dev2 < 0.02f && dominant) {
			if(v->tonalHops < 200) v->tonalHops++;
		} else {
			v->tonalHops = 0;
		}
	}

	// (attack handling moved to the per-sample onset detector in
	// voiceProcess -> shifterOnset; envPrev survives because the
	// tracker's decay-tail freeze still reads it)
	v->envPrev = v->env > v->envPrev ? v->env : v->envPrev * 0.985f;

	// PLUCK SAG onset detection + decay coefficient (settle time is live)
	v->sagDecayCoef = expf(-1.0f / (gSagSettleSec * v->srate));
	if(v->sagRefract > 0)
		v->sagRefract--;
	if(v->sagRefract == 0 && v->env > 1.8f * v->sagEnvHold + 0.003f) {
		v->sagEnv = 1.0f;
		v->sagRefract = 20;
	}
	v->sagEnvHold = v->env > v->sagEnvHold ? v->env : v->sagEnvHold * 0.985f;

	/*
	 * BEND onset detection + decay (v8). Same env-spike pick detector as
	 * SAG above, same refract period - but bendEnv is consumed ONCE PER HOP
	 * right here (via betaT above), not per-sample like sagEnv is in
	 * runPluckSag(), so its coefficient is scaled for HOP samples, not 1.
	 */
	v->bendDecayCoef = expf(-(float)HOP / (gBendSettleSec * v->srate));
	if(v->bendRefract > 0)
		v->bendRefract--;
	if(v->bendRefract == 0 && v->env > 1.8f * v->bendEnvHold + 0.003f) {
		v->bendEnv = 1.0f;
		v->bendRefract = 20;
	}
	v->bendEnvHold = v->env > v->bendEnvHold ? v->env : v->bendEnvHold * 0.985f;
	v->bendEnv *= v->bendDecayCoef;

	/*
	 * SUSTAINER feedback gain, period-independent decay time.
	 */
	{
		const float SUS_REF_PERIOD = 300.0f;
		float fbNote = powf(gSusFb, v->period / SUS_REF_PERIOD);
		if(fbNote > 0.9999f) fbNote = 0.9999f;
		v->susTime = fbNote;
	}

	/*
	 * unified shifter per-hop duties (mark LP, mode machine, LPC analysis) -
	 * v8 RESOURCE GATE: skipped while nothing needs the wet leg (gWetEngine
	 * tracks target OR still-decaying gWetS, so the engine stays alive
	 * through a fade-out and wakes the instant anything re-engages it - see
	 * gWetEngineActive in render.cpp). Everything ABOVE this line (env,
	 * onset detection, the pitch-tracker front end) stays unconditional:
	 * autowah/synth/sustainer/etc. all read v->env regardless of whether
	 * the shifter itself is doing anything.
	 */
	if(gWetEngineActive)
		shifterHop(v, s);
}

float softClip(float x)
{
	if(x > 1.0f) return 2.0f / 3.0f;
	if(x < -1.0f) return -2.0f / 3.0f;
	return x - (x * x * x) / 3.0f;
}
