/*
 * synth.cpp - synthOsc timbres + the MONO/POLY synth-lead logic that
 * used to be inlined directly in render()'s per-sample loop. Extracted
 * into real functions, called from render.cpp in the exact same place
 * / order as before. Bodies are byte-for-byte identical to the original.
 */
#include "hyperguitar.h"
#include <cmath>

/*
 * synthOsc: one sample of the selected synth timbre. Shared by MONO
 * (one global voice) and POLY (six independent per-string voices) -
 * same code path, so every timbre works identically in both modes.
 * inc = cycles per sample. All saw/square use polyBLEP (alias-free).
 */
static inline float synthOsc(int mode, float *ph, float inc, float lfo,
		float *filt, float sr, int noteOn)
{
	switch(mode) {
	case 0: { // SAW LEAD - 2 saws, ~7 cents (the one that already works)
		float d1 = inc, d2 = inc * 1.004f;
		ph[0] += d1; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		ph[1] += d2; if(ph[1] >= 1.0f) ph[1] -= 1.0f;
		float s1 = 2.0f * ph[0] - 1.0f - pblep(ph[0], d1);
		float s2 = 2.0f * ph[1] - 1.0f - pblep(ph[1], d2);
		return 0.5f * (s1 + s2);
	}
	case 1: { // MOOG LEAD - one fat saw through a resonant 2-pole
		  // lowpass with a classic fast-decay filter envelope: the
		  // "pew" of movement comes from the FILTER, not detuning.
		float d = inc;
		ph[0] += d; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		float saw = 2.0f * ph[0] - 1.0f - pblep(ph[0], d);
		if(noteOn) filt[2] = 1.0f;
		filt[2] *= 0.9993f;                    // ~1s filter-env decay
		float fc = inc * sr * (2.5f + 9.0f * filt[2]);
		if(fc > 0.45f * sr) fc = 0.45f * sr;
		if(fc < 40.0f) fc = 40.0f;
		float f = 2.0f * sinf((float)M_PI * fc / sr);
		if(f > 1.2f) f = 1.2f;
		filt[0] += f * filt[1];
		float hi = saw - filt[0] - 0.35f * filt[1];
		filt[1] += f * hi;
		return filt[0] * 1.6f;
	}
	case 2: { // ACID SQUELCH - 303-style pulse through a NEAR-SELF-
		  // OSCILLATING lowpass, fast filter-env sweep - the classic
		  // squelchy acid bite.
		float d = inc;
		ph[0] += d; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		float pw = 0.35f;
		float sq = ph[0] < pw ? 1.0f : -1.0f;
		sq += pblep(ph[0], d);
		float tp = ph[0] - pw; if(tp < 0.0f) tp += 1.0f;
		sq -= pblep(tp, d);
		if(noteOn) filt[2] = 1.0f;
		filt[2] *= 0.9985f;                    // snappier decay than Moog
		float fc = inc * sr * (1.5f + 14.0f * filt[2]);
		if(fc > 0.45f * sr) fc = 0.45f * sr;
		if(fc < 40.0f) fc = 40.0f;
		float f = 2.0f * sinf((float)M_PI * fc / sr);
		if(f > 1.2f) f = 1.2f;
		filt[0] += f * filt[1];
		float hi = sq - filt[0] - 0.12f * filt[1];  // very low damping = high resonance
		filt[1] += f * hi;
		return filt[0] * 1.1f;
	}
	case 3: { // WAVEFOLDER - sine through a tanh wavefolder, fold depth
		  // breathing on the slow LFO. West Coast/Buchla character:
		  // evolving harmonics from nonlinear shaping, not filtering.
		ph[0] += inc; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		float s = sinf(2.0f * (float)M_PI * ph[0]);
		float amt = 3.0f + 2.5f * lfo;
		float folded = tanhf(s * amt);
		float fc = inc * sr * 8.0f;
		if(fc > 0.45f * sr) fc = 0.45f * sr;
		float f = 2.0f * sinf((float)M_PI * fc / sr);
		filt[0] += f * filt[1];
		float hi = folded - filt[0] - 0.6f * filt[1];
		filt[1] += f * hi;
		return filt[0] * 0.9f;
	}
	case 4: { // RINGMOD BELL - carrier x irrational-ratio modulator.
		  // Multiplication (not addition) makes it INHARMONIC -
		  // metallic/bell tones, structurally different from any FM
		  // or saw-stack patch here.
		ph[0] += inc; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		ph[1] += inc * 1.41421356f; if(ph[1] >= 1.0f) ph[1] -= 1.0f;
		float c = sinf(2.0f * (float)M_PI * ph[0]);
		float m = sinf(2.0f * (float)M_PI * ph[1]);
		float bell = c * m;
		float fc = inc * sr * 6.0f;
		if(fc > 0.45f * sr) fc = 0.45f * sr;
		float f = 2.0f * sinf((float)M_PI * fc / sr);
		filt[0] += f * filt[1];
		float hi = bell - filt[0] - 0.7f * filt[1];
		filt[1] += f * hi;
		return filt[0] * 1.8f;
	}
	case 5: { // SYNC LEAD - a slave oscillator hard-resets on every
		  // master cycle; the slave:master ratio sweeps via the
		  // filter env for the classic screaming sync sweep.
		float dm = inc;
		float prevPh = ph[0];
		ph[0] += dm; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		int reset = ph[0] < prevPh;
		if(noteOn) filt[2] = 1.0f;
		filt[2] *= 0.999f;
		float ratio = 1.6f + 3.0f * filt[2];
		if(reset) ph[1] = 0.0f;
		float ds = dm * ratio;
		ph[1] += ds; if(ph[1] >= 1.0f) ph[1] -= 1.0f;
		float saw = 2.0f * ph[1] - 1.0f - pblep(ph[1], ds);
		return saw * 0.75f;
	}
	case 6: { // FM GROWL - 2:1 ratio, DYNAMIC index that decays from
		  // pick attack - loud pick = bright snarl, soft = mellow.
		ph[0] += inc; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		ph[1] += inc * 2.0f; if(ph[1] >= 1.0f) ph[1] -= 1.0f;
		if(noteOn) filt[2] = 1.0f;
		filt[2] *= 0.9994f;
		float idx = 1.0f + 5.0f * filt[2];
		float m = sinf(2.0f * (float)M_PI * ph[1]);
		return sinf(2.0f * (float)M_PI * ph[0] + idx * m) * 0.85f;
	}
	case 7: { // CHIP LEAD - stepped duty-cycle pulse, deliberately raw
		  // and unfiltered for an authentic 8-bit character.
		float d = inc;
		ph[0] += d; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		static const float DUTY[4] = { 0.125f, 0.25f, 0.5f, 0.75f };
		int di = (int)((lfo * 0.5f + 0.5f) * 4.0f);
		if(di < 0) di = 0;
		if(di > 3) di = 3;
		float pw = DUTY[di];
		float sq = ph[0] < pw ? 1.0f : -1.0f;
		sq += pblep(ph[0], d);
		float tp = ph[0] - pw; if(tp < 0.0f) tp += 1.0f;
		sq -= pblep(tp, d);
		return sq * 0.55f;
	}
	case 8: { // SUB GROWL - fundamental + sub-octave sine through a
		  // dark lowpass. Deep, huge low end - a totally different
		  // register from every treble-leaning saw stack here.
		float d = inc;
		ph[0] += d; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		ph[1] += inc * 0.5f; if(ph[1] >= 1.0f) ph[1] -= 1.0f;
		float saw = 2.0f * ph[0] - 1.0f - pblep(ph[0], d);
		float sub = sinf(2.0f * (float)M_PI * ph[1]);
		float src = 0.4f * saw + 0.8f * sub;
		float fc = inc * sr * 3.0f;
		if(fc > 0.45f * sr) fc = 0.45f * sr;
		if(fc < 40.0f) fc = 40.0f;
		float f = 2.0f * sinf((float)M_PI * fc / sr);
		filt[0] += f * filt[1];
		float hi = src - filt[0] - 0.6f * filt[1];
		filt[1] += f * hi;
		return filt[0] * 1.1f;
	}
	case 9: { // FORMANT VOWEL - a pulse source through TWO real
		  // resonant bandpass filters at vowel formant frequencies,
		  // morphing "ooh"<->"ah" on the slow LFO. An actual vocal
		  // filterbank, not just a detuned saw pair.
		float d = inc;
		ph[0] += d; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		float pw = 0.3f;
		float src = ph[0] < pw ? 1.0f : -1.0f;
		src += pblep(ph[0], d);
		float tp = ph[0] - pw; if(tp < 0.0f) tp += 1.0f;
		src -= pblep(tp, d);

		float t = 0.5f + 0.5f * lfo;           // 0 = "ooh", 1 = "ah"
		float f1 = 350.0f + 550.0f * t;
		float f2 = 900.0f - 200.0f * t;

		float w1 = 2.0f * sinf((float)M_PI * f1 / sr);
		filt[0] += w1 * filt[1];
		float hi1 = src - filt[0] - 0.25f * filt[1];
		filt[1] += w1 * hi1;

		float w2 = 2.0f * sinf((float)M_PI * f2 / sr);
		filt[2] += w2 * filt[3];
		float hi2 = src - filt[2] - 0.30f * filt[3];
		filt[3] += w2 * hi2;

		return (filt[1] * 1.3f + filt[3] * 0.9f) * 0.5f;
	}
	/* ---- CHILL MODES (2026-09-24). Soft, low-edge timbres; 10 is the
	 * SYNTHESIS pad's default. One-pole lowpasses use the cheap
	 * a = 2*pi*fc/sr form, clamped - exact corner is not the point. ---- */
	case 10: { // WARM TRI - two triangles 4 cents apart, rounded off
		float d1 = inc, d2 = inc * 1.0023f;
		ph[0] += d1; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		ph[1] += d2; if(ph[1] >= 1.0f) ph[1] -= 1.0f;
		float t1 = 1.0f - 4.0f * fabsf(ph[0] - 0.5f);
		float t2 = 1.0f - 4.0f * fabsf(ph[1] - 0.5f);
		float fc = inc * sr * 6.0f; if(fc > 5000.0f) fc = 5000.0f;
		float a = 6.2831853f * fc / sr; if(a > 0.9f) a = 0.9f;
		filt[0] += a * (0.5f * (t1 + t2) - filt[0]);
		return filt[0] * 1.1f;
	}
	case 11: { // GLASS - sine with a whisper of 2nd and 3rd, organ-soft
		ph[0] += inc; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		float p = 6.2831853f * ph[0];
		return (sinf(p) + 0.22f * sinf(2.0f * p) + 0.07f * sinf(3.0f * p)) * 0.75f;
	}
	case 12: { // SOFT SQUARE - band-limited square, two gentle lowpasses
		float d = inc;
		ph[0] += d; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		float sq = ph[0] < 0.5f ? 1.0f : -1.0f;
		sq += pblep(ph[0], d);
		float tp = ph[0] + 0.5f; if(tp >= 1.0f) tp -= 1.0f;
		sq -= pblep(tp, d);
		float fc = inc * sr * 4.0f; if(fc > 2500.0f) fc = 2500.0f;
		float a = 6.2831853f * fc / sr; if(a > 0.9f) a = 0.9f;
		filt[0] += a * (sq - filt[0]);
		filt[1] += a * (filt[0] - filt[1]);
		return filt[1] * 0.9f;
	}
	case 13: { // SUPERSAW - six saws spread +-19 cents, softened top
		static const float DET[6] = { -0.011f, -0.006f, -0.002f,
		                               0.002f,  0.006f,  0.011f };
		float sum = 0.0f;
		for(int k = 0; k < 6; k++) {
			float d = inc * (1.0f + DET[k]);
			ph[k] += d; if(ph[k] >= 1.0f) ph[k] -= 1.0f;
			sum += 2.0f * ph[k] - 1.0f - pblep(ph[k], d);
		}
		float fc = inc * sr * 7.0f; if(fc > 7000.0f) fc = 7000.0f;
		float a = 6.2831853f * fc / sr; if(a > 0.9f) a = 0.9f;
		filt[0] += a * (sum * 0.3f - filt[0]);
		return filt[0];
	}
	case 14: { // DRIFT PAD - three sines, one slowly drifting, an octave air
		ph[0] += inc;                                   if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		ph[1] += inc * (1.003f + 0.002f * lfo);          if(ph[1] >= 1.0f) ph[1] -= 1.0f;
		ph[2] += inc * 2.001f;                           if(ph[2] >= 1.0f) ph[2] -= 1.0f;
		return (sinf(6.2831853f * ph[0]) + sinf(6.2831853f * ph[1])
			+ 0.3f * sinf(6.2831853f * ph[2])) * 0.45f;
	}
	}
	return 0.0f;
}

// SYNTH POLY: this string drives its OWN oscillator at its own
// tracked fundamental - six independent voices, true polyphony
void runSynthPoly(Voice *v, int s, float *mixed)
{
	if(!(gSineAmtS > 0.002f && gSynthPoly))
		return;
	/*
	 * GATE with HYSTERESIS: opening demands enough level, a
	 * confidently PITCHED signal, AND not being buried under
	 * bleed from the other strings. But once OPEN it only
	 * closes on LEVEL (at half the on-threshold).
	 */
	int on;
	if(v->pGateOn)
		on = v->env > 0.5f * gSynthGateThr;
	else
		on = (v->env > gSynthGateThr)
		  && (v->env > 0.35f * gOtherMax[s])
		  && (v->tonalHops >= SYNTH_TONAL_HOPS);
	int noteOn = on && !v->pGateOn;
	if(noteOn) {
		// NOTE-ON: SNAP straight to pitch. Gliding in from the
		// previous note is what made every entrance sound detuned.
		v->pInc = v->beta / v->period;
	}
	v->pGateOn = on;
	// AMPLITUDE = THE STRING'S OWN ENVELOPE.
	float et = on ? v->env * SINE_ENV_SENS : 0.0f;
	if(et > 1.3f) et = 1.3f;
	v->pAmp += SYNTH_ENV_SLEW * (et - v->pAmp);
	if(on) {
		// held note: glide smoothly so BENDS work
		float targInc = v->beta / v->period;
		v->pInc += 0.02f * (targInc - v->pInc);
	}
	// gate closed: FREEZE the pitch - don't chase the garbage
	// the tracker reports while the note dies away.
	v->pLfo += 0.35f / v->srate;                 // ~0.35 Hz PWM sweep
	if(v->pLfo >= 1.0f) v->pLfo -= 1.0f;
	float lfo = sinf(2.0f * (float)M_PI * v->pLfo);
	float syn = synthOsc(gSynthMode, v->pPh, v->pInc, lfo,
			v->sFilt, v->srate, noteOn)
			* v->pAmp * gSynthLevel * SINE_LEVEL;
	*mixed = *mixed * (1.0f - gSineAmtS) + syn * gSineAmtS;
}

// SYNTH MONO: one voice tracking the loudest string's fundamental,
// snap-pitched and rock-solid. Same timbres as poly (shared osc).
void runSynthMono(float *outL, float *outR, float sr)
{
	if(!(gSineAmtS > 0.002f && !gSynthPoly))
		return;
	Voice *lv = &gV[gLeadStr];
	// GATE: amplitude IS the tracked string envelope - see the
	// POLY voice above for why (no held sustain, no release fade).
	static int sLeadGate = 0;
	// same hysteresis as poly: confidence + bleed check only to
	// OPEN; once sounding, only a real level drop closes it
	int on;
	if(sLeadGate)
		on = lv->env > 0.5f * gSynthGateThr;
	else
		on = (lv->env > gSynthGateThr)
		  && (lv->env > 0.35f * gOtherMax[gLeadStr])
		  && (lv->tonalHops >= SYNTH_TONAL_HOPS);
	int noteOn = on && !sLeadGate;
	if(noteOn)
		gLeadInc = gLeadIncTgt;   // NOTE-ON: snap, never glide in
	sLeadGate = on;
	float et = on ? lv->env * SINE_ENV_SENS : 0.0f;
	if(et > 1.3f) et = 1.3f;
	gLeadAmp += SYNTH_ENV_SLEW * (et - gLeadAmp);
	if(on)
		gLeadInc += 0.02f * (gLeadIncTgt - gLeadInc);  // bends glide

	static float gLeadPh[6] = { 0, 0, 0, 0, 0, 0 };
	static float gLeadFilt[4] = { 0, 0, 0, 0 };
	static float gLeadLfo = 0.0f;
	gLeadLfo += 0.35f / sr;
	if(gLeadLfo >= 1.0f) gLeadLfo -= 1.0f;
	float lead = synthOsc(gSynthMode, gLeadPh, gLeadInc,
			sinf(2.0f * (float)M_PI * gLeadLfo),
			gLeadFilt, sr, noteOn);
	gLeadLPz += 0.55f * (lead - gLeadLPz); // bright tilt, ~6k
	lead = gLeadLPz * gLeadAmp * gSynthLevel * SINE_LEVEL;

	*outL = *outL * (1.0f - gSineAmtS) + lead * gSineAmtS;
	*outR = *outR * (1.0f - gSineAmtS) + lead * gSineAmtS;
}
