/*
 * test_main.cpp - host-side verification of the pitch/formant engine.
 *
 * Links dsp_core.cpp + pitch_tracker.cpp (+ shifter.cpp/lpc.cpp once they
 * exist) against stub Bela headers and drives synthetic strings through
 * the exact hop cadence render.cpp uses. Asserts the properties the
 * redesign promises: pitch accuracy, sustain evenness (COLA), single-copy
 * attacks, pitch/formant decoupling, LPC stability.
 */
#include "hyperguitar.h"
#include "hg_enc.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

/* ---- global definitions (render.cpp owns these on the device) ---- */
const float OPEN_HZ[NUM_STRINGS] = { 69.30f, 92.50f, 123.47f, 164.81f, 207.65f, 277.18f };
const int STRING_CHANNEL[NUM_STRINGS] = { 5, 3, 2, 4, 8, 9 };
const float STRING_PAN[NUM_STRINGS] = { -0.9f, 0.55f, -0.2f, 0.2f, -0.55f, 0.9f };
const float HARM_CUT[NUM_STRINGS] = { 9.0f, 9.0f, 9.0f, 9.0f, 9.0f, 9.0f };
const float LFO_PH[NUM_STRINGS] = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f };

volatile int gPitchActive = 0;
volatile int gFormantActive = 0;
volatile int gFmActive = 0;
volatile int gPresetFormantWobble = 0;
volatile int gVibOn = 0, gChorusOn = 0, gEnvFOn = 0, gFWahOn = 0;
volatile int gSineOn = 0, gPogOn = 0, gResOn = 0, gSusOn = 0;
volatile int gJawOn = 0, gSitarOn = 0, gPanOn = 0;
volatile int gSagOn = 0, gSympOn = 0;
volatile float gBetaBase[NUM_STRINGS] = { 1, 1, 1, 1, 1, 1 };
volatile float gFormantBase[NUM_STRINGS] = { 1, 1, 1, 1, 1, 1 };
volatile float gDryTarget = 1.0f;
volatile float gWetTarget = 0.0f;
float gFormantVal = 1.0f;
float gFormantGlide = 0.10f;
float gFormantTiltBias = 0.0f;
float gFWahSens = 1.2f;
float gEnvfSens = 1.0f, gEnvfQ = 1.0f;
float gFmRatioVal = 1.0f, gFmIndexVal = 0.2f;
int gFmQuantized = 0;
float gPitchSemitones = 0.0f, gPitchCents = 0.0f, gPitchWetPct = 100.0f;
int gPitchCentsMode = 0;
float gPanSpread = 0.0f, gPanDyn = 0.0f;
float gSagDepthSec = 0.0f, gSagSettleSec = 0.1f;
volatile int gBendOn = 0;
float gBendDepthSemis = 5.0f, gBendSettleSec = 0.12f;
float gJoyWhammySemis = 0.0f;
/* always on here: this harness drives the shifter directly and has no
 * render.cpp main loop to compute the real resource gate every block. */
volatile int gWetEngineActive = 1;
/* hg_mc.cpp stubs (multicore worker pool - not linked here; gXMode=0 makes
 * hgOnsRead()/shifterOnset() take the plain single-threaded path, so the
 * real file's logic is exercised unchanged). */
int gXMode = 0;
__thread int gBlkN = 0;
void hgXWait(int, int) {}
float gSusFb = 0.99f, gSusDamp = 0.2f, gSusLevel = 0.5f;
float gOtherMax[NUM_STRINGS] = { 0, 0, 0, 0, 0, 0 };
float gHannT[HANN_N + 1];
Voice gV[NUM_STRINGS];
volatile int gFormantMode = 0;
unsigned int gSampleStamp = 0;
unsigned int gOnsetStamp[NUM_STRINGS] = { 0, 0, 0, 0, 0, 0 };

/* ---- synth.cpp globals (render.cpp owns them on the device) ---- */
volatile int gSynthMode = 0;
volatile int gSynthPoly = 1;
float gSynthLevel = 1.0f;
float gSynthGateThr = 0.010f;
float gSineAmtS = 0.0f;
int gLeadStr = 0;
float gLeadInc = 0.002f, gLeadIncTgt = 0.002f, gLeadAmp = 0.0f, gLeadLPz = 0.0f;
#include "names.inc"   /* SYNTH_NAMES, extracted from render.cpp by the Makefile */
#include "../synth.cpp"

#define SR 44100.0f

static void voiceSetup(Voice *v, int s)
{
	memset(v, 0, sizeof(Voice));
	v->p0 = SR / OPEN_HZ[s];
	v->period = v->p0;
	v->beta = 1.0f;
	v->formant = 1.0f;
	v->pogGain = 1.0f;
	v->harmCut = HARM_CUT[s];
	v->srate = SR;
	v->fcCur = v->harmCut * OPEN_HZ[s];
	updateLP(v);
	shifterInit(v);
	v->lagMax = (int)(v->p0 * TUNE_MARGIN / (float)DECIM);
	v->lagMin = (int)(v->p0 / (MAX_FRET_RATIO * (float)DECIM));
	if(v->lagMin < 4) v->lagMin = 4;
	v->corrWin = (int)(2.0f * v->p0 / (float)DECIM);
	v->hopCountdown = 1;
}

/* replicate render.cpp's per-sample cadence for one voice */
static void runVoice(Voice *v, int s, const float *in, float *wet, float *dry, int n)
{
	for(int i = 0; i < n; i++) {
		if(--v->hopCountdown <= 0) {
			trackPitch(v, s);
			updateModulation(v, s);
			updateLP(v);
			v->hopCountdown = HOP;
		}
		voiceProcess(v, in[i], 1, &wet[i], &dry[i]);
		gSampleStamp++;
	}
}

/* ---- signal generators / analyzers ---- */

static void genSaw(std::vector<float> &out, float f0, float seconds, float gain)
{
	int n = (int)(seconds * SR);
	out.assign(n, 0.0f);
	for(int k = 1; k <= 20; k++) {
		float a = gain / (float)k;
		float w = 2.0f * (float)M_PI * f0 * (float)k / SR;
		if(f0 * k > 0.45f * SR) break;
		for(int i = 0; i < n; i++)
			out[i] += a * sinf(w * (float)i);
	}
}

/* give a signal FORMANTS: two +12dB resonant peaks (500Hz, 2kHz). A pure
 * saw has a 1/f envelope, which is scale-invariant - formant shifting it
 * is invisible. Real instruments have envelope structure; so must tests. */
static void applyFormants(std::vector<float> &x)
{
	for(int r = 0; r < 2; r++) {
		float f0 = r == 0 ? 500.0f : 2000.0f;
		float A = powf(10.0f, 12.0f / 40.0f), Q = 4.0f;
		float w0 = 2.0f * (float)M_PI * f0 / SR;
		float al = sinf(w0) / (2.0f * Q);
		float b0 = 1 + al * A, b1 = -2 * cosf(w0), b2 = 1 - al * A;
		float a0 = 1 + al / A, a1 = b1, a2 = 1 - al / A;
		float z1 = 0, z2 = 0;
		for(size_t i = 0; i < x.size(); i++) {
			float in = x[i];
			float out = (b0 * in + z1) / a0;
			z1 = b1 * in - a1 * out + z2;
			z2 = b2 * in - a2 * out;
			x[i] = out;
		}
	}
}

/* silence, then a sharp-attack saw burst (2ms ramp) */
static void genBurst(std::vector<float> &out, float f0, float silence, float seconds, float gain)
{
	std::vector<float> saw;
	genSaw(saw, f0, seconds, gain);
	int ns = (int)(silence * SR);
	int ramp = (int)(0.002f * SR);
	out.assign(ns + (int)saw.size(), 0.0f);
	for(size_t i = 0; i < saw.size(); i++) {
		float g = (int)i < ramp ? (float)i / (float)ramp : 1.0f;
		out[ns + i] = saw[i] * g;
	}
}

/* NSDF-style period measurement with parabolic refine */
static float measurePeriod(const float *x, int n, float lagLo, float lagHi)
{
	int lo = (int)lagLo, hi = (int)lagHi;
	if(hi > n / 2) hi = n / 2;
	float best = -2.0f;
	int bl = lo;
	std::vector<float> vals(hi + 2, -2.0f);
	for(int lag = lo; lag <= hi; lag++) {
		double acf = 0, e1 = 0, e2 = 0;
		for(int i = 0; i < n - hi; i++) {
			acf += (double)x[i] * x[i + lag];
			e1 += (double)x[i] * x[i];
			e2 += (double)x[i + lag] * x[i + lag];
		}
		vals[lag] = (float)(acf / sqrt(e1 * e2 + 1e-12));
	}
	/* first peak above 0.85 * global max (avoid octave-low winners) */
	for(int lag = lo; lag <= hi; lag++)
		if(vals[lag] > best) best = vals[lag];
	float bar = 0.85f * best;
	for(int lag = lo + 1; lag < hi; lag++) {
		if(vals[lag] >= bar && vals[lag] >= vals[lag - 1] && vals[lag] >= vals[lag + 1]) {
			bl = lag;
			break;
		}
	}
	float ym = vals[bl - 1], y0 = vals[bl], yp = vals[bl + 1];
	float den = ym - 2.0f * y0 + yp;
	float off = fabsf(den) > 1e-9f ? 0.5f * (ym - yp) / den : 0.0f;
	return (float)bl + off;
}

static float rms(const float *x, int n)
{
	double s = 0;
	for(int i = 0; i < n; i++) s += (double)x[i] * x[i];
	return (float)sqrt(s / (n > 0 ? n : 1));
}

static float peakAbs(const float *x, int n)
{
	float p = 0.0f;
	for(int i = 0; i < n; i++) if(fabsf(x[i]) > p) p = fabsf(x[i]);
	return p;
}

/* magnitude at harmonic k*f0 via Goertzel-ish direct DFT */
static float harmMag(const float *x, int n, float fHz)
{
	double re = 0, im = 0;
	double w = 2.0 * M_PI * fHz / SR;
	for(int i = 0; i < n; i++) {
		re += x[i] * cos(w * i);
		im += x[i] * sin(w * i);
	}
	return (float)(sqrt(re * re + im * im) / n);
}

static float centroid(const float *x, int n, float f0)
{
	double num = 0, den = 0;
	for(int k = 1; k <= 20; k++) {
		if(f0 * k > 0.45f * SR) break;
		float m = harmMag(x, n, f0 * (float)k);
		num += (double)k * f0 * m;
		den += m;
	}
	return den > 1e-12 ? (float)(num / den) : 0.0f;
}

/* two saws butt-joined with a 20ms equal-power crossfade at tSwitch -
 * models a hammer-on/fret change: pitch jumps, amplitude doesn't dip,
 * so the onset detector must NOT fire and the tracker must snap */
static void genTwoNote(std::vector<float> &out, float f1, float f2,
		float tSwitch, float tTotal, float gain)
{
	std::vector<float> a, b;
	genSaw(a, f1, tTotal, gain);
	genSaw(b, f2, tTotal, gain);
	int n = (int)(tTotal * SR);
	int ns = (int)(tSwitch * SR);
	int xf = (int)(0.020f * SR);
	out.assign(n, 0.0f);
	for(int i = 0; i < n; i++) {
		float m = i < ns ? 0.0f : (i < ns + xf ? (float)(i - ns) / (float)xf : 1.0f);
		out[i] = a[i] * sqrtf(1.0f - m) + b[i] * sqrtf(m);
	}
}

/* sliding pitch trace: measured f0 every stepSec, window winSec */
static void traceF0(const float *x, int n, float winSec, float stepSec,
		std::vector<float> &times, std::vector<float> &freqs)
{
	int win = (int)(winSec * SR), step = (int)(stepSec * SR);
	times.clear();
	freqs.clear();
	for(int i = 0; i + win <= n; i += step) {
		float p = measurePeriod(&x[i], win, SR / 500.0f, SR / 40.0f);
		times.push_back((float)i / SR);
		freqs.push_back(SR / p);
	}
}

static int gFails = 0;
#define CHECK(name, cond, fmt, ...) do { \
	if(cond) printf("PASS %-22s " fmt "\n", name, __VA_ARGS__); \
	else { printf("FAIL %-22s " fmt "\n", name, __VA_ARGS__); gFails++; } \
} while(0)

static float centsOff(float pMeas, float pExp)
{
	return 1200.0f * log2f(pMeas / pExp);
}

static void resetGlobals(void);
#include "test_synth.inc"

static void resetGlobals(void)
{
	gPitchActive = 0;
	gFormantActive = 0;
	gFWahOn = 0;
	gFormantMode = 0;
	for(int s = 0; s < NUM_STRINGS; s++) {
		gBetaBase[s] = 1.0f;
		gFormantBase[s] = 1.0f;
		gOtherMax[s] = 0.0f;
	}
	gFormantVal = 1.0f;
	gFormantGlide = 0.10f;
}

int main(void)
{
	for(int i = 0; i <= HANN_N; i++)
		gHannT[i] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * i / (float)HANN_N));

	std::vector<float> in, wet, dry;
	Voice *v = &gV[2]; /* "D string", open 123.47 Hz: room to shift both ways */
	const int S = 2;

	/* ---------- passthrough sanity ---------- */
	{
		resetGlobals();
		voiceSetup(v, S);
		genSaw(in, 165.0f, 2.0f, 0.30f);
		wet.assign(in.size(), 0.0f);
		dry.assign(in.size(), 0.0f);
		runVoice(v, S, in.data(), wet.data(), dry.data(), (int)in.size());
		int n0 = (int)(1.0f * SR), n1 = (int)in.size();
		float ri = rms(&in[n0], n1 - n0), ro = rms(&wet[n0], n1 - n0);
		float dB = 20.0f * log10f((ro + 1e-9f) / (ri + 1e-9f));
		int ok = fabsf(dB) < 3.0f && std::isfinite(ro);
		CHECK("passthrough", ok, "wet/in = %+.2f dB", dB);
	}

	/* ---------- pitch accuracy: -7 semitones ---------- */
	{
		resetGlobals();
		voiceSetup(v, S);
		gPitchActive = 1;
		float beta = powf(2.0f, -7.0f / 12.0f);
		for(int s = 0; s < NUM_STRINGS; s++) gBetaBase[s] = beta;
		genSaw(in, 165.0f, 2.0f, 0.30f);
		wet.assign(in.size(), 0.0f);
		dry.assign(in.size(), 0.0f);
		runVoice(v, S, in.data(), wet.data(), dry.data(), (int)in.size());
		float fExp = 165.0f * beta;
		float pMeas = measurePeriod(&wet[(int)SR], (int)SR, SR / 500.0f, SR / 40.0f);
		float cents = centsOff(SR / pMeas, fExp);
		CHECK("pitch_-7st", fabsf(cents) < 10.0f, "err %+.1f cents", cents);
	}

	/* ---------- pitch accuracy: +12 semitones ---------- */
	{
		resetGlobals();
		voiceSetup(v, S);
		gPitchActive = 1;
		for(int s = 0; s < NUM_STRINGS; s++) gBetaBase[s] = 2.0f;
		genSaw(in, 165.0f, 2.0f, 0.30f);
		wet.assign(in.size(), 0.0f);
		dry.assign(in.size(), 0.0f);
		runVoice(v, S, in.data(), wet.data(), dry.data(), (int)in.size());
		float pMeas = measurePeriod(&wet[(int)SR], (int)SR, SR / 500.0f, SR / 40.0f);
		float cents = centsOff(SR / pMeas, 330.0f);
		CHECK("pitch_+12st", fabsf(cents) < 10.0f, "err %+.1f cents", cents);
	}

	/* ---------- sustain evenness (the COLA guarantee) ---------- */
	{
		resetGlobals();
		voiceSetup(v, S);
		gPitchActive = 1;
		float beta = powf(2.0f, -5.0f / 12.0f);
		for(int s = 0; s < NUM_STRINGS; s++) gBetaBase[s] = beta;
		genSaw(in, 165.0f, 2.0f, 0.30f);
		wet.assign(in.size(), 0.0f);
		dry.assign(in.size(), 0.0f);
		runVoice(v, S, in.data(), wet.data(), dry.data(), (int)in.size());
		/* frames = 4 OUTPUT periods: a frame that holds a non-integer
		 * number of periods beats against the waveform's own periodic
		 * envelope and measures that, not engine AM */
		int f = (int)roundf(4.0f * SR / (165.0f * beta));
		int n0 = (int)(0.5f * SR), n1 = (int)(1.5f * SR);
		float mn = 1e9f, mx = 0.0f;
		for(int i = n0; i + f <= n1; i += f) {
			float r = rms(&wet[i], f);
			if(r < mn) mn = r;
			if(r > mx) mx = r;
		}
		float dB = 20.0f * log10f(mx / (mn + 1e-9f));
		CHECK("evenness_-5st", dB < 0.5f, "frame RMS spread %.2f dB", dB);
	}

	/* ---------- single-copy attack ---------- */
	{
		resetGlobals();
		voiceSetup(v, S);
		gPitchActive = 1;
		for(int s = 0; s < NUM_STRINGS; s++) gBetaBase[s] = 0.5f;
		genBurst(in, 196.0f, 0.4f, 0.8f, 0.30f);
		wet.assign(in.size(), 0.0f);
		dry.assign(in.size(), 0.0f);
		runVoice(v, S, in.data(), wet.data(), dry.data(), (int)in.size());
		int on = (int)(0.4f * SR);
		int w40 = (int)(0.040f * SR);
		float pIn = peakAbs(&in[on], w40);
		float pOut = peakAbs(&wet[on], w40 + (int)(0.010f * SR));
		float ratio = pOut / (pIn + 1e-9f);
		CHECK("single_attack", ratio < 1.6f, "attack peak ratio %.2f", ratio);
	}

	/* ---------- formant decoupling (GRAIN mode) ---------- */
	{
		resetGlobals();
		voiceSetup(v, S);
		gFormantActive = 1;
		gFormantVal = 1.3f;
		for(int s = 0; s < NUM_STRINGS; s++) gFormantBase[s] = 1.3f;
		genSaw(in, 165.0f, 2.0f, 0.30f);
		applyFormants(in);
		wet.assign(in.size(), 0.0f);
		dry.assign(in.size(), 0.0f);
		runVoice(v, S, in.data(), wet.data(), dry.data(), (int)in.size());
		int n0 = (int)(1.0f * SR), nA = (int)(0.9f * SR);
		float pMeas = measurePeriod(&wet[n0], nA, SR / 500.0f, SR / 40.0f);
		float cents = centsOff(SR / pMeas, 165.0f);
		float cIn = centroid(&in[n0], nA, 165.0f);
		float cOut = centroid(&wet[n0], nA, 165.0f);
		int ok = fabsf(cents) < 10.0f && cOut > 1.12f * cIn;
		CHECK("formant_1.3_grain", ok, "pitch err %+.1f cents, centroid %.0f -> %.0f Hz",
				cents, cIn, cOut);
	}

	/* ---------- note change: snap, don't sweep (the anti-laser test) ---------- */
	{
		resetGlobals();
		voiceSetup(v, S);
		gPitchActive = 1;
		float beta = powf(2.0f, -5.0f / 12.0f);
		for(int s = 0; s < NUM_STRINGS; s++) gBetaBase[s] = beta;
		genTwoNote(in, 165.0f, 220.0f, 0.9f, 1.8f, 0.30f);
		wet.assign(in.size(), 0.0f);
		dry.assign(in.size(), 0.0f);
		runVoice(v, S, in.data(), wet.data(), dry.data(), (int)in.size());
		std::vector<float> tt, ff;
		traceF0(wet.data(), (int)in.size(), 0.040f, 0.010f, tt, ff);
		float fB = 220.0f * beta;
		/* locked to the new note within 45ms of the switch and rock solid
		 * after - a slew-style laser sweep is still hundreds of cents off
		 * at 45-100ms, so this catches it. (Windows overlapping the 20ms
		 * input crossfade itself read dual-pitch combination lags in the
		 * ANALYZER and are excluded - the trace shows a clean jump.) */
		float worst = 0.0f;
		for(size_t i = 0; i < tt.size(); i++) {
			float t = tt[i] + 0.020f; /* window center */
			if(t > 0.945f) {
				float c = fabsf(centsOff(ff[i], fB));
				if(c > worst) worst = c;
			}
		}
		int ok = worst < 25.0f;
		CHECK("note_change_snap", ok, "worst err after switch+45ms: %.1f cents",
				worst);
	}

	/* ---------- repick: onset path returns without pitch excursion ---------- */
	{
		resetGlobals();
		voiceSetup(v, S);
		gPitchActive = 1;
		for(int s = 0; s < NUM_STRINGS; s++) gBetaBase[s] = 0.5f;
		/* two picks of the same note: 0.4s..0.9s, then again from 0.95s */
		std::vector<float> b1, b2;
		genBurst(b1, 196.0f, 0.4f, 0.5f, 0.30f);
		genBurst(b2, 196.0f, 0.95f, 0.85f, 0.45f);
		in.assign((size_t)(1.8f * SR), 0.0f);
		for(size_t i = 0; i < in.size(); i++) {
			float acc = 0.0f;
			if(i < b1.size()) acc += b1[i];
			if(i < b2.size()) acc += b2[i];
			in[i] = acc;
		}
		wet.assign(in.size(), 0.0f);
		dry.assign(in.size(), 0.0f);
		runVoice(v, S, in.data(), wet.data(), dry.data(), (int)in.size());
		std::vector<float> tt, ff;
		traceF0(wet.data(), (int)in.size(), 0.040f, 0.010f, tt, ff);
		float fT = 196.0f * 0.5f;
		float worst = 0.0f;
		for(size_t i = 0; i < tt.size(); i++) {
			float t = tt[i] + 0.020f;
			if(t > 1.08f && t < 1.75f) {
				float c = fabsf(centsOff(ff[i], fT));
				if(c > worst) worst = c;
			}
		}
		CHECK("repick_stability", worst < 25.0f, "worst post-repick err %.1f cents", worst);
	}

	/* ---------- LPC mode: stability on noise ---------- */
	{
		resetGlobals();
		voiceSetup(v, S);
		gFormantActive = 1;
		gFormantMode = 1;
		gFormantVal = 1.4f;
		for(int s = 0; s < NUM_STRINGS; s++) gFormantBase[s] = 1.4f;
		int n = (int)(20.0f * SR);
		in.assign(n, 0.0f);
		unsigned int seed = 12345;
		for(int i = 0; i < n; i++) {
			seed = seed * 1664525u + 1013904223u;
			in[i] = 0.25f * ((float)(seed >> 9) / 4194304.0f - 1.0f);
		}
		wet.assign(n, 0.0f);
		dry.assign(n, 0.0f);
		runVoice(v, S, in.data(), wet.data(), dry.data(), n);
		float p = peakAbs(wet.data(), n);
		int ok = std::isfinite(p) && p < 8.0f;
		CHECK("lpc_stability", ok, "noise peak %.3f", p);
	}

	/* ---------- LPC mode: formant motion on a real tone ---------- */
	{
		resetGlobals();
		voiceSetup(v, S);
		gFormantActive = 1;
		gFormantMode = 1;
		gFormantVal = 1.4f;
		for(int s = 0; s < NUM_STRINGS; s++) gFormantBase[s] = 1.4f;
		/* 2026-11-01 lpc-q: 0.15 (was 0.30): the formant boost on a 0.30 saw peaks ~1.4 and the new output soft knee (>0.9) rightly compresses it; this test measures formant shaping, not the knee */
		genSaw(in, 165.0f, 2.0f, 0.15f);
		applyFormants(in);
		wet.assign(in.size(), 0.0f);
		dry.assign(in.size(), 0.0f);
		runVoice(v, S, in.data(), wet.data(), dry.data(), (int)in.size());
		int n0 = (int)(1.0f * SR), nA = (int)(0.9f * SR);
		float pMeas = measurePeriod(&wet[n0], nA, SR / 500.0f, SR / 40.0f);
		float cents = centsOff(SR / pMeas, 165.0f);
		float cIn = centroid(&in[n0], nA, 165.0f);
		float cOut = centroid(&wet[n0], nA, 165.0f);
		int ok = fabsf(cents) < 10.0f && cOut > 1.12f * cIn;
		CHECK("formant_1.4_lpc", ok, "pitch err %+.1f cents, centroid %.0f -> %.0f Hz",
				cents, cIn, cOut);
	}

	/* ---------- 2026-11-01 enc: quadrature decoder ---------- */
	{
		/* CW = 00>10>11>01 ; one detent = 4 edges */
		static const int cw[4][2] = {{0,0},{1,0},{1,1},{0,1}};
		HgEnc e; int tot = 0; hgEncFeed(e, 0, 0, 1000);
		/* clean: 5 CW detents then 3 CCW */
		for(int i = 1; i <= 20; i++) { int k = i & 3; tot += hgEncFeed(e, cw[k][0], cw[k][1], 1000); }
		int cwTot = tot;
		for(int i = 19; i >= 8; i--) { int k = i & 3; tot += hgEncFeed(e, cw[k][0], cw[k][1], 1000); }
		int ok1 = (cwTot == 5 * ENC_DIR) && (tot == 2 * ENC_DIR) && e.invalid == 0;
		/* bounce: each edge chatters old>new>old>new, 3 detents CW */
		HgEnc f; int t2 = 0; hgEncFeed(f, 0, 0, 1000);
		for(int i = 1; i <= 12; i++) {
			int k = i & 3, o = (i - 1) & 3;
			t2 += hgEncFeed(f, cw[k][0], cw[k][1], 1000);
			t2 += hgEncFeed(f, cw[o][0], cw[o][1], 1000);
			t2 += hgEncFeed(f, cw[k][0], cw[k][1], 1000);
		}
		int ok2 = (t2 == 3 * ENC_DIR) && f.invalid == 0;
		/* skipped state counted invalid, no phantom step */
		HgEnc g; int t3 = 0; t3 += hgEncFeed(g, 0, 0, 1000); t3 += hgEncFeed(g, 1, 1, 1000);
		int ok3 = (t3 == 0) && g.invalid == 1;
		/* lost edge: 3 of 4 edges, then settle -> 1 step, acc cleared */
		HgEnc h; int t4 = 0; hgEncFeed(h, 0, 0, 5);
		for(int i = 1; i <= 3; i++) t4 += hgEncFeed(h, cw[i][0], cw[i][1], 5);
		for(int i = 0; i < 6; i++) t4 += hgEncFeed(h, cw[3][0], cw[3][1], 5);
		int ok4 = (t4 == ENC_DIR) && h.acc == 0;
		/* fast: 20 detents in 80 edges one frame each, none lost */
		HgEnc q; int t5 = 0; hgEncFeed(q, 0, 0, 1000);
		for(int i = 1; i <= 80; i++) { int k = i & 3; t5 += hgEncFeed(q, cw[k][0], cw[k][1], 1000); }
		int ok5 = (t5 == 20 * ENC_DIR);
		CHECK("enc_decoder", ok1 && ok2 && ok3 && ok4 && ok5, "clean%d bounce%d skip%d lost%d fast%d", ok1, ok2, ok3, ok4, ok5);
	}

	synthTests();

	printf(gFails ? "\n%d FAILURE(S)\n" : "\nALL PASS (%d)\n", gFails ? gFails : 18);
	return gFails ? 1 : 0;
}
