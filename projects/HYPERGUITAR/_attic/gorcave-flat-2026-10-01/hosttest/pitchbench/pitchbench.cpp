/*
 * pitchbench.cpp - offline latency / accuracy / crosstalk harness for the
 * per-string pitch tracker (pitch_tracker.cpp).
 *
 * Links the REAL dsp_core.cpp + shifter.cpp + lpc.cpp and whichever
 * tracker the Makefile selects (current pitch_tracker.cpp, or the archived
 * v1 kept as ../../archive/pitch_tracker_v1.cpp.txt), and drives all SIX
 * voices with exactly render.cpp's cadence: one frame at a time, every
 * string's hop staggered by s*HOP/6, gOtherMax recomputed per 64-frame
 * block, gSampleStamp advanced per frame (so shifterOnset's gOnsetStamp
 * has the device meaning).
 *
 * MATERIAL. Synthetic, and that is the harness's limit: it proves nothing
 * about the real SubSix. What it does model on purpose, because each one
 * is a documented way guitar trackers fail:
 *   - stiff-string INHARMONICITY  f_k = k f0 sqrt(1 + B k^2), B rising
 *     with fret (shorter string)
 *   - bridge-pickup + pick-position COMB weighting (weak fundamental: the
 *     classic octave-up trap for a hex pickup ~1-2 cm from the saddle)
 *   - magnetic pickup = VELOCITY sensor (partial k weighted by k)
 *   - per-partial decay (highs die first), pitch glide of a hard pick
 *     (tension modulation: starts sharp, settles)
 *   - an APERIODIC pick-scrape noise burst + low thump at every pick
 *   - device level (~0.02-0.05 peak), top two strings -5 dB, white noise
 *     floor on every channel
 *   - ADJACENT-STRING BLEED: ch_s += g1*(s+-1) + g2*(s+-2), g1 selectable
 *     -20..-30 dB (the rig measured -23 dB and -37 dB, render.cpp 2026-09-22)
 *   - a Karplus-Strong variant (MODEL=ks) with fractional-delay tuning,
 *     pick-comb excitation and a velocity/pickup comb on the output.
 *
 * Usage:  ./pitchbench [scenario] [bleed_dB] [model]
 *   scenario: all | single | strum | legato | steady | silent
 */
#include "hyperguitar.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>

/* ---- globals render.cpp owns on the device ---- */
const float OPEN_HZ[NUM_STRINGS] = { 65.41f, 87.31f, 116.54f, 155.56f, 196.00f, 261.63f };
const int STRING_CHANNEL[NUM_STRINGS] = { 5, 4, 6, 8, 9, 7 };
const float STRING_PAN[NUM_STRINGS] = { -0.10f, +0.10f, -0.30f, +0.30f, -0.50f, +0.50f };
const float HARM_CUT[NUM_STRINGS] = { 6.0f, 6.0f, 8.0f, 8.0f, 10.0f, 10.0f };
const float LFO_PH[NUM_STRINGS] = { 0.0f, 1.1f, 2.2f, 3.3f, 4.4f, 5.5f };

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
float gSusFb = 0.99f, gSusDamp = 0.2f, gSusLevel = 0.5f;
float gOtherMax[NUM_STRINGS] = { 0, 0, 0, 0, 0, 0 };
float gHannT[HANN_N + 1];
Voice gV[NUM_STRINGS];
volatile int gFormantMode = 0;
unsigned int gSampleStamp = 0;
unsigned int gOnsetStamp[NUM_STRINGS] = { 0, 0, 0, 0, 0, 0 };

#define SR 44100.0f

/* ------------------------------------------------------------------ */
static unsigned int rngS = 12345u;
static float frnd(void) { rngS = rngS * 1664525u + 1013904223u; return (float)(rngS >> 9) / 8388608.0f; }
static float nrnd(void) { float a = frnd() + frnd() + frnd() + frnd() - 2.0f; return a * 0.866f; }

struct Note {
	int s;          /* string */
	float t0;       /* seconds, pick instant */
	float t1;       /* seconds, damped (note-off) */
	float fret;     /* semitones above open (tuning offset applied separately) */
	float amp;      /* peak-ish */
	int legato;     /* 1 = hammer-on: no pick noise, no amplitude reset */
	float bendC = 0.0f;   /* bend depth, cents, ramped over bendT0..bendT0+0.25 s */
	float bendT0 = 0.0f;
	float vibC = 0.0f;    /* vibrato depth, cents, 5 Hz, from 0.6 s */
};

static float gTuneCents[NUM_STRINGS] = { 0, 0, 0, 0, 0, 0 };
static const char *gModel = "add";
static float gPickNoise = 1.0f;

static float bendMul(const Note &n, float t)
{
	float c = 0.0f;
	if(n.bendC != 0.0f && t > n.bendT0) {
		float u = (t - n.bendT0) / 0.25f;
		if(u > 1.0f) u = 1.0f;
		c += n.bendC * u * u * (3.0f - 2.0f * u);
	}
	if(n.vibC != 0.0f && t > 0.6f)
		c += n.vibC * sinf(2.0f * (float)M_PI * 5.0f * (t - 0.6f));
	return powf(2.0f, c / 1200.0f);
}

static float noteHz(const Note &n)
{
	return OPEN_HZ[n.s] * powf(2.0f, (n.fret + gTuneCents[n.s] / 100.0f) / 12.0f);
}

/* additive stiff-string pluck through a bridge-mounted velocity pickup */
static void renderAdd(std::vector<float> &out, const Note &n)
{
	float f0 = noteHz(n);
	int i0 = (int)(n.t0 * SR), i1 = (int)(n.t1 * SR);
	if(i1 > (int)out.size()) i1 = (int)out.size();
	float Leff = powf(2.0f, -n.fret / 12.0f);          /* vibrating length / scale */
	float Bopen = n.s <= 3 ? 6e-5f : 1.2e-4f;           /* plain top strings stiffer */
	float B = Bopen / (Leff * Leff) * (getenv("PB_BSCALE") ? (float)atof(getenv("PB_BSCALE")) : 1.0f);
	float xpu = 0.035f / Leff;                           /* pickup ~2.2 cm from saddle */
	float xpk = (0.16f + 0.04f * frnd()) / Leff;         /* pick ~10 cm from bridge */
	if(xpk > 0.5f) xpk = 0.5f;
	float decay = (n.s <= 2 ? 2.5f : 1.6f) * (0.6f + 0.4f * Leff);
	const int NP = 40;
	float fk[NP], ak[NP], dk[NP], ph[NP];
	int np = 0;
	float norm = 0.0f;
	for(int k = 1; k <= NP; k++) {
		float f = k * f0 * sqrtf(1.0f + B * k * k);
		if(f > 7500.0f) break;
		float pick = sinf((float)M_PI * k * xpk) / (float)(k * k);
		float pu = sinf((float)M_PI * k * xpu);
		float a = pick * pu * (float)k;                  /* velocity pickup */
		fk[np] = f; ak[np] = a; ph[np] = (frnd() - 0.5f) * 0.6f;
		dk[np] = 1.0f / (decay / (1.0f + 0.35f * (float)(k - 1)));
		norm += fabsf(a);
		np++;
	}
	float g = n.amp / (norm + 1e-9f) * 2.2f;
	int ramp = n.legato ? 30 : 12;
	double phs[NP];
	for(int k = 0; k < np; k++) phs[k] = ph[k];
	for(int i = i0; i < i1; i++) {
		float t = (float)(i - i0) / SR;
		/* tension-modulation glide: +6 cents at the pick, settling in ~80ms */
		float glide = n.legato ? 1.0f : powf(2.0f, (6.0f * expf(-t / 0.08f)) / 1200.0f);
		glide *= bendMul(n, t);
		float atk = (i - i0) < ramp ? (float)(i - i0) / (float)ramp : 1.0f;
		float rel = (i1 - i) < 200 ? (float)(i1 - i) / 200.0f : 1.0f;   /* damp */
		float y = 0.0f;
		for(int k = 0; k < np; k++) {
			phs[k] += 2.0 * M_PI * fk[k] * glide / SR;
			y += ak[k] * expf(-t * dk[k]) * sinf((float)phs[k]);
		}
		out[i] += g * atk * rel * y;
	}
	if(!n.legato) {
		/* pick scrape: aperiodic, bright, ~4 ms; plus a low thump */
		float lp = 0.0f, hp = 0.0f;
		for(int i = i0; i < i0 + (int)(0.012f * SR) && i < (int)out.size(); i++) {
			float t = (float)(i - i0) / SR;
			float w = nrnd();
			lp += 0.35f * (w - lp);
			hp = w - lp;
			float e = expf(-t / 0.004f);
			float thump = sinf(2.0f * (float)M_PI * 40.0f * t) * expf(-t / 0.006f);
			out[i] += gPickNoise * n.amp * (0.55f * e * hp + 0.35f * thump);
		}
	}
}

/* Karplus-Strong, fractional delay by first-order allpass */
static void renderKS(std::vector<float> &out, const Note &n)
{
	float f0 = noteHz(n);
	int i0 = (int)(n.t0 * SR), i1 = (int)(n.t1 * SR);
	if(i1 > (int)out.size()) i1 = (int)out.size();
	float Pd = SR / f0 - 0.5f;               /* loop LP adds 0.5 sample delay */
	int N = (int)Pd;
	float fr = Pd - N;
	if(fr < 0.1f) { N--; fr += 1.0f; }
	float c = (1.0f - fr) / (1.0f + fr);
	std::vector<float> dl(N + 2, 0.0f);
	/* excitation: pick-position combed noise + triangle */
	float xpk = 0.18f + 0.04f * frnd();
	int dp = (int)(xpk * N);
	std::vector<float> ex(N, 0.0f);
	for(int i = 0; i < N; i++) {
		float tri = i < dp ? (float)i / dp : (float)(N - i) / (N - dp);
		ex[i] = tri + 0.5f * nrnd();
	}
	for(int i = N - 1; i >= dp; i--) ex[i] -= ex[i - dp];
	float m = 0.0f;
	for(int i = 0; i < N; i++) m += ex[i];
	m /= N;
	for(int i = 0; i < N; i++) dl[i] = ex[i] - m;
	int w = 0;
	float apX = 0.0f, apY = 0.0f, lpPrev = 0.0f;
	float loss = powf(0.001f, 1.0f / ((n.s <= 2 ? 3.0f : 2.0f) * f0));
	std::vector<float> hist(4096, 0.0f);
	int hw = 0;
	int dpu = (int)(2.0f * 0.035f * SR / f0);   /* pickup comb */
	if(dpu < 1) dpu = 1;
	float prevOut = 0.0f;
	float g = n.amp * 0.6f;
	for(int i = i0; i < i1; i++) {
		float yv = dl[w];
		float lp = 0.5f * (yv + lpPrev);
		lpPrev = yv;
		float ap = c * lp + apX - c * apY;
		apX = lp; apY = ap;
		dl[w] = ap * loss;
		w = (w + 1) % N;
		hist[hw & 4095] = yv;
		float pu = yv - hist[(hw - dpu) & 4095];
		hw++;
		float vel = pu - prevOut;   /* velocity */
		prevOut = pu;
		float rel = (i1 - i) < 200 ? (float)(i1 - i) / 200.0f : 1.0f;
		out[i] += g * 8.0f * vel * rel;
	}
	if(!n.legato) {
		float lp = 0.0f;
		for(int i = i0; i < i0 + (int)(0.012f * SR) && i < (int)out.size(); i++) {
			float t = (float)(i - i0) / SR;
			float wv = nrnd();
			lp += 0.35f * (wv - lp);
			out[i] += n.amp * 0.5f * expf(-t / 0.004f) * (wv - lp);
		}
	}
}

/* ------------------------------------------------------------------ */
struct HopRec {
	float t;         /* seconds */
	float period;    /* v->period (full-rate samples) */
	float good;      /* v->periodGood */
	float conf;
	int blank;       /* attackBlank after trackPitch */
	int gate;
	int tonal;
};

static std::vector<HopRec> gRec[NUM_STRINGS];
static double gTrackNs = 0.0;
#ifdef PT_OPCOUNT
extern long gPtMacs;
static long gMacMax = 0; static double gMacSum = 0; static long gMacCalls = 0;
static long gMacBlockMax = 0, gMacBlock = 0;
#endif
static long gTrackCalls = 0;
static int gOnsets[NUM_STRINGS];
static std::vector<float> gOnsetT[NUM_STRINGS];
static std::vector<float> gOnsetLag;
static std::vector<float> gWet[NUM_STRINGS];
static float gOracle[NUM_STRINGS];   /* >0: force this period after trackPitch (steady scen) */
static std::vector<float> gDryIn[NUM_STRINGS];

static void setupVoices(void)
{
	for(int s = 0; s < NUM_STRINGS; s++) {
		Voice *v = &gV[s];
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
		v->hopCountdown = (s * HOP) / NUM_STRINGS;
		gRec[s].clear();
		gOnsetT[s].clear();
		gOnsets[s] = 0;
	}
	gSampleStamp = 0;
	for(int s = 0; s < NUM_STRINGS; s++) gOnsetStamp[s] = 0;
}

static void runAll(const std::vector<float> *ch, int n)
{
	for(int s = 0; s < NUM_STRINGS; s++) gWet[s].assign(n, 0.0f);
	for(int i = 0; i < n; i++) {
		if((i & 63) == 0) {
#ifdef PT_OPCOUNT
			if(gMacBlock > gMacBlockMax) gMacBlockMax = gMacBlock;
			gMacBlock = 0;
#endif
			for(int s = 0; s < NUM_STRINGS; s++) {
				float m = 0.0f;
				for(int o = 0; o < NUM_STRINGS; o++)
					if(o != s && gV[o].env > m) m = gV[o].env;
				gOtherMax[s] = m;
			}
		}
		for(int s = 0; s < NUM_STRINGS; s++) {
			Voice *v = &gV[s];
			if(--v->hopCountdown <= 0) {
				auto a = std::chrono::steady_clock::now();
#ifdef PT_OPCOUNT
				long m0 = gPtMacs;
#endif
				trackPitch(v, s);
#ifdef PT_OPCOUNT
				long dm = gPtMacs - m0;
				if(dm > gMacMax) gMacMax = dm;
				gMacSum += dm; gMacCalls++;
				gMacBlock += dm;
#endif
				auto b = std::chrono::steady_clock::now();
				if(gOracle[s] > 0.0f) { v->period = gOracle[s]; v->periodGood = gOracle[s]; }
				gTrackNs += std::chrono::duration<double, std::nano>(b - a).count();
				gTrackCalls++;
				HopRec r;
				r.t = (float)i / SR;
				r.period = v->period; r.good = v->periodGood; r.conf = v->pitchConf;
				r.blank = v->attackBlank; r.gate = v->dbgGate;
				updateModulation(v, s);
				updateLP(v);
				r.tonal = v->tonalHops;
				gRec[s].push_back(r);
				v->hopCountdown = HOP;
			}
			unsigned int os = gOnsetStamp[s];
			float wet, dry;
			voiceProcess(v, ch[s][i], 1, &wet, &dry);
			gWet[s][i] = wet;
			(void)dry;
			if(gOnsetStamp[s] != os) { gOnsets[s]++; gOnsetT[s].push_back((float)i / SR); }
		}
		gSampleStamp++;
	}
}

static void synthesize(const std::vector<Note> &notes, float secs, float bleedDb,
		std::vector<float> *ch, unsigned int seed)
{
	rngS = seed;
	int n = (int)(secs * SR);
	std::vector<float> clean[NUM_STRINGS];
	for(int s = 0; s < NUM_STRINGS; s++) clean[s].assign(n, 0.0f);
	float ampMul = getenv("PB_AMP") ? (float)atof(getenv("PB_AMP")) : 1.0f;
	for(Note nt : notes) {
		nt.amp *= ampMul;
		if(nt.amp < 0.0f) {           /* negative amp = aperiodic mute/scratch burst */
			int i0 = (int)(nt.t0 * SR), i1 = (int)(nt.t1 * SR);
			float lp = 0.0f, lp2 = 0.0f;
			for(int i = i0; i < i1 && i < n; i++) {
				float t = (float)(i - i0) / SR;
				lp += 0.2f * (nrnd() - lp);
				lp2 += 0.2f * (lp - lp2);
				clean[nt.s][i] += -nt.amp * 3.0f * lp2 * expf(-t / 0.02f);
			}
			continue;
		}
		if(!strcmp(gModel, "ks")) renderKS(clean[nt.s], nt);
		else renderAdd(clean[nt.s], nt);
	}
	float g1 = powf(10.0f, bleedDb / 20.0f);
	float g2 = powf(10.0f, (bleedDb - 14.0f) / 20.0f);
	for(int s = 0; s < NUM_STRINGS; s++) {
		ch[s].assign(n, 0.0f);
		for(int i = 0; i < n; i++) {
			float y = clean[s][i];
			for(int o = 0; o < NUM_STRINGS; o++) {
				int d = abs(o - s);
				if(d == 1) y += g1 * clean[o][i];
				else if(d == 2) y += g2 * clean[o][i];
			}
			y += 1.0e-4f * nrnd();
			ch[s][i] = y;
		}
	}
}

/* ------------------------------------------------------------------ */
static float cents(float pMeas, float pTrue) { return 1200.0f * log2f(pTrue / pMeas); }

struct Stats {
	int notes = 0, lockedNotes = 0, neverCorrect = 0;
	std::vector<float> latCorrect, latLock;
	int wrongHops = 0, octaveHops = 0, judgedHops = 0;
	int transientWrong = 0;          /* notes whose period visited a WRONG note pre-lock */
	int falseLockHops = 0, falseRetune = 0, falseTonal = 0, bleedHops = 0;
};

static float pct(std::vector<float> v, float p)
{
	if(v.empty()) return NAN;
	std::sort(v.begin(), v.end());
	int k = (int)(p * (v.size() - 1) + 0.5f);
	return v[k];
}

/* judge one note on its own string */
static void judgeNote(const Note &nt, const Note *prev, float nextT, Stats &st, int verbose)
{
	const std::vector<HopRec> &R = gRec[nt.s];
	float pTrue = SR / noteHz(nt);
	float tEnd = std::min(nt.t1, nextT);
	st.notes++;
	/* latency = first hop that is correct (within 35 cents) AND stays
	 * correct for 40 ms (so a lucky transient pass does not count) */
	float tCorrect = -1.0f, tLock = -1.0f;
	int visitedWrong = 0;
	for(size_t h = 0; h < R.size(); h++) {
		if(R[h].t < nt.t0 || R[h].t >= tEnd) continue;
		float c = fabsf(cents(R[h].period, pTrue));
		if(tCorrect < 0.0f) {
			if(c < 35.0f) {
				int ok = 1;
				for(size_t k = h; k < R.size() && R[k].t < std::min(R[h].t + 0.040f, tEnd); k++)
					if(fabsf(cents(R[k].period, pTrue)) >= 35.0f) { ok = 0; break; }
				if(ok) tCorrect = R[h].t;
			} else if(prev) {
				float pPrev = SR / noteHz(*prev);
				if(fabsf(cents(R[h].period, pPrev)) > 35.0f && h > 0
				   && fabsf(R[h].period - R[h - 1].period) > 1e-3f)
					visitedWrong = 1;   /* moved, but to neither note */
			}
		}
		if(tLock < 0.0f && c < 35.0f && R[h].conf >= PITCH_CONF_MIN && R[h].blank == 0)
			tLock = R[h].t;
		if(tCorrect >= 0.0f && R[h].t > tCorrect && R[h].conf >= PITCH_CONF_MIN) {
			st.judgedHops++;
			if(c >= 50.0f) {
				if(getenv("PB_WRONGDBG") && st.wrongHops < 40)
					printf("WRONG s%d note %.1fHz t+%.1fms read %.1fHz conf %.3f\n", nt.s, noteHz(nt),
						1000 * (R[h].t - nt.t0), SR / R[h].period, R[h].conf);
				st.wrongHops++;
				float ci = cents(R[h].period, pTrue);
				float r = fabsf(fabsf(ci) - 1200.0f);
				if(r < 60.0f || fabsf(fabsf(ci) - 1902.0f) < 60.0f || fabsf(fabsf(ci) - 2400.0f) < 60.0f)
					st.octaveHops++;
			}
		}
	}
	if(visitedWrong) st.transientWrong++;
	if(tCorrect >= 0.0f) st.latCorrect.push_back(1000.0f * (tCorrect - nt.t0));
	else st.neverCorrect++;
	if(tLock >= 0.0f) { st.latLock.push_back(1000.0f * (tLock - nt.t0)); st.lockedNotes++; }
	float onLag = -1.0f;
	for(float ot : gOnsetT[nt.s]) if(ot > nt.t0 - 0.005f && ot < nt.t0 + 0.1f) { onLag = 1000.0f * (ot - nt.t0); break; }
	if(onLag > -1.0f) gOnsetLag.push_back(onLag);
	if(verbose)
		printf("  s%d fret %4.1f %6.1fHz  onset %+5.1f ms  correct %6.1f ms  lock %6.1f ms%s\n",
			nt.s, nt.fret, noteHz(nt), onLag,
			tCorrect >= 0 ? 1000.0f * (tCorrect - nt.t0) : -1.0f,
			tLock >= 0 ? 1000.0f * (tLock - nt.t0) : -1.0f,
			visitedWrong ? "  (visited wrong note first)" : "");
}

/* strings with NO notes during [ta,tb): count claims made off bleed */
static void judgeSilent(int s, float ta, float tb, Stats &st)
{
	const std::vector<HopRec> &R = gRec[s];
	float lastP = -1.0f;
	for(size_t h = 0; h < R.size(); h++) {
		if(R[h].t < ta || R[h].t >= tb) continue;
		st.bleedHops++;
		if(R[h].conf >= PITCH_CONF_MIN && R[h].blank == 0 && R[h].gate == 0)
			st.falseLockHops++;
		if(R[h].tonal >= SYNTH_TONAL_HOPS) st.falseTonal++;
		if(lastP > 0.0f && fabsf(cents(R[h].period, lastP)) > 50.0f) st.falseRetune++;
		lastP = R[h].period;
	}
}

static void printStats(const char *name, const Stats &st)
{
	printf("%-10s notes %3d | correct-pitch latency ms: p50 %6.1f p90 %6.1f max %6.1f | lock p50 %6.1f p90 %6.1f | never %d\n",
		name, st.notes, pct(st.latCorrect, 0.5f), pct(st.latCorrect, 0.9f),
		pct(st.latCorrect, 1.0f), pct(st.latLock, 0.5f), pct(st.latLock, 0.9f), st.neverCorrect);
	printf("%-10s wrong-while-confident %d/%d hops (octave %d) | notes that visited a wrong note first %d\n",
		"", st.wrongHops, st.judgedHops, st.octaveHops, st.transientWrong);
	if(st.bleedHops)
		printf("%-10s bleed-only strings: %d hops, confident-lock %d (%.1f%%), retunes %d, synth-tonal %d\n",
			"", st.bleedHops, st.falseLockHops, 100.0f * st.falseLockHops / st.bleedHops,
			st.falseRetune, st.falseTonal);
}

static void traceNotes(const std::vector<Note> &notes, int only)
{
	if(!getenv("PB_TRACE")) return;
	int ts = -1; float tf = -1;
	sscanf(getenv("PB_TRACE"), "%d:%f", &ts, &tf);
	if(only >= 0 && only != ts) return;
	for(size_t k = 0; k < notes.size(); k++) {
		if(notes[k].s != ts || notes[k].fret != tf) continue;
		int s = ts;
		float t0 = notes[k].t0;
		printf("TRACE s%d fret %.0f true %.2f Hz pick at %.4f  (other strings:", s, tf, noteHz(notes[k]), t0);
		for(size_t j = 0; j < notes.size(); j++)
			if(notes[j].s != s && fabsf(notes[j].t0 - t0) < 0.05f)
				printf(" s%d %.1fHz", notes[j].s, noteHz(notes[j]));
		printf(")\n");
		for(float ot : gOnsetT[s]) if(ot > t0 - 0.05f && ot < t0 + 0.2f) printf("  onset @ %+.1f ms\n", 1000 * (ot - t0));
		for(auto &r : gRec[s]) if(r.t > t0 - 0.006f && r.t < t0 + 0.12f)
			printf("  %+6.1f ms  f %7.2f Hz  good %7.2f  conf %.3f blank %d gate %d\n",
				1000 * (r.t - t0), SR / r.period, SR / r.good, r.conf, r.blank, r.gate);
	}
}

/* ------------------------------------------------------------------ */
/* Scenario: single notes, each string alone (neighbours only get bleed).
 * Every test note is preceded by a DIFFERENT note on the same string so
 * a tracker still sitting on the old period cannot score by accident. */
static void scenSingle(float bleedDb, int verbose, Stats &st, Stats &sil)
{
	static const float frets[] = { 0, 3, 5, 7, 10, 12, 15, 19, 22 };
	const int NF = sizeof(frets) / sizeof(frets[0]);
	for(int s = 0; s < NUM_STRINGS; s++) {
		std::vector<Note> notes;
		float t = 0.30f;
		for(int k = 0; k < NF; k++) {
			float pre = frets[(k + 4) % NF];
			if(pre == frets[k]) pre = frets[k] + 2;
			float amp = (s >= 4 ? 0.56f : 1.0f) * (0.025f + 0.02f * frnd());
			notes.push_back({ s, t, t + 0.40f, pre, amp, 0 });
			t += 0.42f;
			notes.push_back({ s, t, t + 0.55f, frets[k], amp, 0 });
			t += 0.75f;
		}
		std::vector<float> ch[NUM_STRINGS];
		synthesize(notes, t + 0.3f, bleedDb, ch, 1000u + s * 77u);
		if(getenv("PB_DUMP") && atoi(getenv("PB_DUMP")) == s) {
			FILE *df = fopen("dump.f32", "wb");
			fwrite(ch[s].data(), 4, ch[s].size(), df);
			fclose(df);
		}
		setupVoices();
		runAll(ch, (int)ch[0].size());
		if(verbose) printf(" string %d (open %.2f Hz): %d picks, %d onsets detected by shifterOnset\n", s, OPEN_HZ[s], (int)notes.size(), gOnsets[s]);
		for(size_t k = 1; k < notes.size(); k += 2)
			judgeNote(notes[k], &notes[k - 1], 1e9f, st, verbose);
		traceNotes(notes, s);
		for(int o = 0; o < NUM_STRINGS; o++)
			if(o != s && abs(o - s) <= 2) judgeSilent(o, 0.0f, t, sil);
	}
}

/* Scenario: strums - all six strings picked within 25 ms, chord shapes */
static void scenStrum(float bleedDb, int verbose, Stats &st)
{
	static const float shapes[][6] = {
		{ 0, 2, 2, 1, 0, 0 }, { 3, 2, 0, 0, 0, 3 }, { 0, 0, 2, 2, 2, 0 },
		{ 5, 7, 7, 6, 5, 5 }, { 8, 10, 10, 9, 8, 8 }, { 0, 2, 2, 0, 1, 0 },
		{ 3, 5, 5, 4, 3, 3 }, { 10, 12, 12, 11, 10, 10 }, { 1, 3, 3, 2, 1, 1 },
		{ 12, 14, 14, 13, 12, 12 }, { 7, 9, 9, 8, 7, 7 }, { 2, 4, 4, 3, 2, 2 },
	};
	const int NS = sizeof(shapes) / sizeof(shapes[0]);
	std::vector<Note> notes;
	float t = 0.3f;
	for(int k = 0; k < NS; k++) {
		for(int s = 0; s < NUM_STRINGS; s++) {
			float dt = 0.005f * s * (0.6f + 0.8f * frnd());
			float amp = (s >= 4 ? 0.56f : 1.0f) * (0.02f + 0.02f * frnd());
			notes.push_back({ s, t + dt, t + 0.85f, shapes[k][s], amp, 0 });
		}
		t += 0.9f;
	}
	std::vector<float> ch[NUM_STRINGS];
	synthesize(notes, t + 0.3f, bleedDb, ch, 4242u);
	setupVoices();
	runAll(ch, (int)ch[0].size());
	traceNotes(notes, -1);
	for(size_t k = 0; k < notes.size(); k++) {
		const Note *prev = k >= NUM_STRINGS ? &notes[k - NUM_STRINGS] : nullptr;
		if(prev && prev->fret == notes[k].fret) continue;   /* unchanged pitch, not a test */
		judgeNote(notes[k], prev, 1e9f, st, verbose > 1);
	}
}

/* Scenario: legato - a pick, then hammer-ons / pull-offs with NO pick */
static void scenLegato(float bleedDb, int verbose, Stats &st)
{
	std::vector<Note> notes;
	float t = 0.3f;
	for(int s = 0; s < NUM_STRINGS; s++) {
		static const float seq[] = { 5, 7, 9, 7, 5, 8, 10, 12 };
		float amp = (s >= 4 ? 0.56f : 1.0f) * 0.035f;
		notes.push_back({ s, t, t + 0.25f, 3, amp, 0 });
		t += 0.25f;
		for(int k = 0; k < 8; k++) {
			amp *= 0.85f;
			notes.push_back({ s, t, t + 0.18f, seq[k], amp, 1 });
			t += 0.18f;
		}
		t += 0.4f;
	}
	std::vector<float> ch[NUM_STRINGS];
	synthesize(notes, t + 0.3f, bleedDb, ch, 777u);
	setupVoices();
	runAll(ch, (int)ch[0].size());
	for(size_t k = 1; k < notes.size(); k++)
		if(notes[k].legato)
			judgeNote(notes[k], &notes[k - 1], 1e9f, st, verbose > 1);
}

/* Scenario: bends + vibrato - does the period FOLLOW a continuous pitch
 * move (no consensus stalls, no stair-steps)? error vs instantaneous truth */
static void scenBend(float bleedDb)
{
	std::vector<Note> notes;
	float t = 0.3f;
	for(int s = 0; s < NUM_STRINGS; s++) {
		Note n = { s, t, t + 1.6f, 5, (s >= 4 ? 0.56f : 1.0f) * 0.035f, 0 };
		n.bendC = 200.0f; n.bendT0 = 0.15f; n.vibC = 30.0f;
		notes.push_back(n);
		t += 1.8f;
	}
	std::vector<float> ch[NUM_STRINGS];
	synthesize(notes, t + 0.2f, bleedDb, ch, 555u);
	setupVoices();
	runAll(ch, (int)ch[0].size());
	double se = 0; int n = 0; float mx = 0;
	for(const Note &nt : notes) {
		for(auto &r : gRec[nt.s]) {
			float tt = r.t - nt.t0;
			if(tt < 0.1f || r.t > nt.t1 - 0.2f) continue;
			/* truth at the centre of the ~1-period analysis window */
			float fTrue = noteHz(nt) * bendMul(nt, tt - 1.0f / noteHz(nt));
			float c = fabsf(1200.0f * log2f((SR / r.period) / fTrue));
			se += c; n++;
			if(c > mx) mx = c;
		}
	}
	printf("bend       200c bend + 30c vibrato, all strings: period error mean %.1f c, max %.1f c over %d hops\n",
		se / n, mx, n);
}

/* Scenario: fast repeated picks (same note, then alternating) at 100 ms,
 * plus palm-muted scratches (aperiodic bursts) on otherwise idle strings */
static void scenRepeat(float bleedDb, int verbose, Stats &st, Stats &sil)
{
	std::vector<Note> notes;
	std::vector<std::pair<float, float>> muteWin[NUM_STRINGS];
	float t = 0.3f;
	for(int s = 0; s < NUM_STRINGS; s++) {
		float amp = (s >= 4 ? 0.56f : 1.0f) * 0.03f;
		for(int k = 0; k < 8; k++) { notes.push_back({ s, t, t + 0.1f, 5, amp, 0 }); t += 0.1f; }
		for(int k = 0; k < 8; k++) { notes.push_back({ s, t, t + 0.1f, (k & 1) ? 7.0f : 5.0f, amp, 0 }); t += 0.1f; }
		t += 0.3f;
		/* three mutes, 250 ms apart, nothing else on this string */
		for(int k = 0; k < 3; k++) {
			notes.push_back({ s, t, t + 0.08f, 0, -amp, 0 });
			muteWin[s].push_back({ t + 0.02f, t + 0.25f });
			t += 0.25f;
		}
		t += 0.3f;
	}
	std::vector<float> ch[NUM_STRINGS];
	synthesize(notes, t + 0.3f, bleedDb, ch, 2024u);
	setupVoices();
	runAll(ch, (int)ch[0].size());
	/* judge the alternating picks (a real pitch change each time) */
	for(size_t k = 1; k < notes.size(); k++) {
		const Note &a = notes[k], &b = notes[k - 1];
		if(a.amp > 0 && b.amp > 0 && a.s == b.s && a.fret != b.fret)
			judgeNote(a, &b, 1e9f, st, verbose > 1);
	}
	/* mutes: the string only has a scratch - any confident pitch is false */
	for(int s = 0; s < NUM_STRINGS; s++)
		for(auto &w : muteWin[s]) judgeSilent(s, w.first, w.second, sil);
}

/* Scenario: CONTEST - a quiet real note on string s while BOTH neighbours
 * play loud different notes. Relative level of the target note swept
 * 0 / -6 / -12 / -18 dB. How often is the real note still tracked? */
static void scenContest(float bleedDb)
{
	static const float rel[4] = { 0.0f, -6.0f, -12.0f, -18.0f };
	for(int r = 0; r < 4; r++) {
		Stats st;
		for(int s = 1; s < NUM_STRINGS - 1; s++) {
			std::vector<Note> notes;
			float t = 0.6f;
			for(int k = 0; k < 6; k++) {
				float loud = 0.04f;
				notes.push_back({ s - 1, t - 0.05f, t + 0.7f, (float)(k * 2 + 1), loud, 0 });
				notes.push_back({ s + 1, t - 0.03f, t + 0.7f, (float)(k * 2 + 3), loud, 0 });
				notes.push_back({ s, t - 0.28f, t - 0.05f, (float)((k * 5 + 3) % 12), loud * powf(10.0f, rel[r] / 20.0f), 0 });
				notes.push_back({ s, t, t + 0.45f, (float)(k * 3 % 12), loud * powf(10.0f, rel[r] / 20.0f), 0 });
				t += 0.8f;
			}
			std::vector<float> ch[NUM_STRINGS];
			synthesize(notes, t + 0.3f, bleedDb, ch, 900u + s);
			setupVoices();
			runAll(ch, (int)ch[0].size());
			for(size_t k = 3; k < notes.size(); k += 4)
				judgeNote(notes[k], &notes[k - 1], 1e9f, st, 0);
			if(r == 0) traceNotes(notes, s);
		}
		char nm[32];
		snprintf(nm, sizeof(nm), "contest%+.0f", rel[r]);
		printStats(nm, st);
	}
}

/* Scenario: silence + noise only: tracker must claim nothing */
static void scenSilent(Stats &sil)
{
	std::vector<Note> none;
	std::vector<float> ch[NUM_STRINGS];
	synthesize(none, 3.0f, -23.0f, ch, 99u);
	setupVoices();
	runAll(ch, (int)ch[0].size());
	for(int s = 0; s < NUM_STRINGS; s++) judgeSilent(s, 0.0f, 3.0f, sil);
}

/* Scenario: STEADY long open notes, strings 4/5 tuned flat. Measures the
 * period the shifter actually receives: bias and hop-to-hop jitter (cents),
 * and the wet leg's slow amplitude modulation with +12 pitch / formant. */
static void scenSteady(float bleedDb, float flatCents, int mode)
{
	for(int s = 0; s < NUM_STRINGS; s++) gTuneCents[s] = -flatCents;
	std::vector<Note> notes;
	for(int s = 0; s < NUM_STRINGS; s++)
		notes.push_back({ s, 0.3f + 0.05f * s, 4.3f, 0, (s >= 4 ? 0.56f : 1.0f) * 0.04f, 0 });
	std::vector<float> ch[NUM_STRINGS];
	synthesize(notes, 4.5f, bleedDb, ch, 31337u);
	for(int s = 0; s < NUM_STRINGS; s++) {
		gBetaBase[s] = mode == 1 ? 2.0f : mode == 3 ? 0.5f : mode == 4 ? 1.4983f : 1.0f;
		gFormantBase[s] = mode == 2 ? 1.3f : 1.0f;
	}
	gPitchActive = mode == 1 || mode >= 3;
	gFormantActive = mode == 2;
	int oracle = getenv("PB_ORACLE") ? 1 : 0;
	for(int s = 0; s < NUM_STRINGS; s++) {
		float B = (s <= 3 ? 6e-5f : 1.2e-4f) * (getenv("PB_BSCALE") ? (float)atof(getenv("PB_BSCALE")) : 1.0f);
		gOracle[s] = oracle ? SR / (noteHz(notes[s]) * sqrtf(1.0f + B)) : 0.0f;
	}
	setupVoices();
	runAll(ch, (int)ch[0].size());
	const char *mn[] = { "none", "+12", "formant1.3", "-12", "+7" };
	printf("steady open strings, tuned %.0f cents flat, bleed %.0f dB, shift %s\n",
		flatCents, bleedDb, mn[mode]);
	for(int s = 0; s < NUM_STRINGS; s++) {
		float pTrue = SR / noteHz(notes[s]);
		double sum = 0, sum2 = 0, sumd = 0; int n = 0; float lo = 1e9f, hi = -1e9f;
		float prev = 0.0f;
		for(auto &r : gRec[s]) {
			if(r.t < 0.8f || r.t > 3.8f) continue;
			float c = cents(r.period, pTrue);
			sum += c; sum2 += c * c; n++;
			if(c < lo) lo = c;
			if(c > hi) hi = c;
			if(getenv("PB_STEADYDBG") && fabsf(c) > 8.0f && mode == 0)
				printf("    s%d t %.3f err %+.1f c conf %.3f gate %d\n", s, r.t, c, r.conf, r.gate);
			if(prev != 0.0f) sumd += fabsf(cents(r.period, prev));
			prev = r.period;
		}
		float mean = (float)(sum / n), sd = sqrtf((float)(sum2 / n) - mean * mean);
		/* slow AM of the wet leg vs the dry input: 40 ms RMS frames,
		 * detrended by a 400 ms moving average, mean |dev| in % */
		auto amOf = [&](const std::vector<float> &x) {
			std::vector<float> fr;
			int F = (int)(0.040f * SR);
			for(int i = (int)(0.8f * SR); i + F < (int)(3.8f * SR); i += F / 2) {
				double e = 0; for(int k = 0; k < F; k++) e += x[i + k] * x[i + k];
				fr.push_back(sqrtf((float)(e / F)));
			}
			double md = 0; int nm = 0;
			for(int k = 10; k + 10 < (int)fr.size(); k++) {
				double a = 0; for(int j = -10; j <= 10; j++) a += fr[k + j];
				a /= 21.0;
				if(a > 1e-7) { md += fabs(fr[k] / a - 1.0); nm++; }
			}
			return nm ? (float)(100.0 * md / nm) : 0.0f;
		};
		float amW = amOf(gWet[s]), amD = amOf(ch[s]);
		/* per-HARMONIC slow modulation ("beading"): magnitude of partials
		 * 1..6 of the expected fundamental in 50 ms frames, detrended by a
		 * 500 ms moving average; mean |dev| in %, averaged over partials */
		auto hmOf = [&](const std::vector<float> &x, float f) {
			int F = (int)(0.050f * SR), Hs = F / 2;
			double tot = 0; int cnt = 0;
			for(int k = 1; k <= 6; k++) {
				float fk = f * k;
				if(fk > 5000.0f) break;
				std::vector<float> mg;
				for(int i = (int)(0.8f * SR); i + F < (int)(3.8f * SR); i += Hs) {
					double re = 0, im = 0;
					for(int j = 0; j < F; j++) {
						float w = 0.5f - 0.5f * cosf(6.2831853f * j / F);
						float ph = 6.2831853f * fk * (float)(i + j) / SR;
						re += w * x[i + j] * cosf(ph); im += w * x[i + j] * sinf(ph);
					}
					mg.push_back(sqrtf((float)(re * re + im * im)));
				}
				double md = 0; int nm = 0;
				for(int q = 10; q + 10 < (int)mg.size(); q++) {
					double a = 0; for(int j = -10; j <= 10; j++) a += mg[q + j];
					a /= 21.0;
					if(a > 1e-9) { md += fabs(mg[q] / a - 1.0); nm++; }
				}
				if(nm) { tot += md / nm; cnt++; }
			}
			return cnt ? (float)(100.0 * tot / cnt) : 0.0f;
		};
		float fOut = noteHz(notes[s]) * gBetaBase[s];
		float hmW = hmOf(gWet[s], fOut), hmD = hmOf(ch[s], noteHz(notes[s]));
		printf("  s%d %6.1fHz  period err mean %+6.2f c  sd %5.2f c  range [%+6.1f,%+6.1f]  mean |hop step| %5.2f c  AM wet %5.2f%% dry %5.2f%%  partial-mod wet %5.2f%% dry %5.2f%%\n",
			s, noteHz(notes[s]), mean, sd, lo, hi, (float)(sumd / n), amW, amD, hmW, hmD);
	}
	for(int s = 0; s < NUM_STRINGS; s++) { gBetaBase[s] = 1.0f; gFormantBase[s] = 1.0f; gTuneCents[s] = 0; gOracle[s] = 0.0f; }
	gPitchActive = 0; gFormantActive = 0;
}

int main(int argc, char **argv)
{
	for(int i = 0; i <= HANN_N; i++)
		gHannT[i] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * i / (float)HANN_N));
	std::string scen = argc > 1 ? argv[1] : "all";
	float bleed = argc > 2 ? (float)atof(argv[2]) : -23.0f;
	if(argc > 3) gModel = argv[3];
	if(getenv("PB_PICKNOISE")) gPickNoise = (float)atof(getenv("PB_PICKNOISE"));
	int verbose = getenv("PB_VERBOSE") ? atoi(getenv("PB_VERBOSE")) : 0;

	printf("== pitchbench  model=%s  bleed=%.0f dB (adjacent), %.0f dB (next-but-one)\n",
		gModel, bleed, bleed - 14.0f);
	if(scen == "all" || scen == "single") {
		Stats st, sil;
		scenSingle(bleed, verbose, st, sil);
		printStats("single", st);
		printStats("", sil);
	}
	if(scen == "all" || scen == "strum") {
		Stats st;
		scenStrum(bleed, verbose, st);
		printStats("strum", st);
	}
	if(scen == "all" || scen == "legato") {
		Stats st;
		scenLegato(bleed, verbose, st);
		printStats("legato", st);
	}
	if(scen == "contest")
		scenContest(bleed);
	if(scen == "all" || scen == "repeat") {
		Stats st, sil;
		scenRepeat(bleed, verbose, st, sil);
		printStats("repeat", st);
		printf("           palm-mute scratches: %d hops, confident-lock %d, retunes %d, synth-tonal %d\n",
			sil.bleedHops, sil.falseLockHops, sil.falseRetune, sil.falseTonal);
	}
	if((scen == "all" || scen == "bend") && strcmp(gModel, "ks"))
		scenBend(bleed);   /* the KS generator has no bend/vibrato */
	if(scen == "all" || scen == "silent") {
		Stats sil;
		scenSilent(sil);
		printf("silent     noise-only: %d hops, confident-lock %d, retunes %d, synth-tonal %d\n",
			sil.bleedHops, sil.falseLockHops, sil.falseRetune, sil.falseTonal);
	}
	if(scen == "steady") {
		float flat = argc > 4 ? (float)atof(argv[4]) : 25.0f;
		for(int m = 0; m < 5; m++) scenSteady(bleed, flat, m);
	}
#ifdef PT_OPCOUNT
	printf("tracker MACs: mean %.0f/call, worst call %ld, worst 64-sample block (all strings) %ld\n",
		gMacSum / (gMacCalls ? gMacCalls : 1), gMacMax, gMacBlockMax);
#endif
	printf("tracker cost (host, weak signal): %.2f us/call mean over %ld calls\n",
		gTrackNs / 1000.0 / (gTrackCalls ? gTrackCalls : 1), gTrackCalls);
	return 0;
}
