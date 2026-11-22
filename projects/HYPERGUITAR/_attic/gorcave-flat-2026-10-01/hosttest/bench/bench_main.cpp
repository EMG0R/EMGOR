/*
 * bench_main.cpp - FULL-CHAIN host benchmark.
 *
 * Links the REAL render.cpp (and therefore the real effects.cpp, synth.cpp,
 * dsp_core.cpp, pitch_tracker.cpp, shifter.cpp, lpc.cpp, presets.cpp,
 * trellis_ui.cpp) against the augmented Bela stub and drives render()
 * block-by-block with realistic INHARMONIC, JITTERY guitar material on all
 * six string channels.
 *
 * Measures: per-block wall time (mean / p50 / p99 / max) and, optionally,
 * dumps the stereo output for the discontinuity detector.
 */
#include "Bela.h"
#include "hyperguitar.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <vector>
#include <algorithm>

/* the stub's audioRead/audioWrite talk to these */
float gBenchIn[HG_BENCH_CHANS][1024];
float gBenchOut[2][1024];

extern bool setup(BelaContext *, void *);
extern void render(BelaContext *, void *);

#define SRATE 44100.0f
#define BS    64

/* ---------------- realistic material generator ----------------
 *
 * Per string: a sequence of plucks. Each pluck is a stiff-string partial
 * stack (INHARMONIC: f_k = k*f0*sqrt(1+B k^2)) with per-partial decay
 * (higher partials die faster), per-partial phase jitter, a slow vibrato,
 * an optional bend, and pluck TIMING jitter. Plus one 6-string strum.
 * Clean synthetic saws hide bugs here; this does not.
 */
static unsigned int rngS = 987654321u;
static float frnd(void)
{
	rngS = rngS * 1664525u + 1013904223u;
	return (float)(rngS >> 9) / 8388608.0f;      /* 0..1 */
}

struct Pluck {
	float t0;        /* seconds */
	float f0;
	float amp;
	float bendCents; /* total bend over the note */
	float vibHz, vibCents;
	float decay;     /* seconds */
};

static void renderPluck(std::vector<float> &out, const Pluck &p, float B)
{
	int n0 = (int)(p.t0 * SRATE);
	int len = (int)(p.decay * 4.0f * SRATE);
	if(n0 + len > (int)out.size()) len = (int)out.size() - n0;
	if(len <= 0) return;
	const int NP = 14;
	float ph[NP], det[NP], dk[NP];
	for(int k = 0; k < NP; k++) {
		ph[k] = frnd() * 6.28318f;
		det[k] = 1.0f + 0.0015f * (frnd() - 0.5f);   /* partial jitter */
		dk[k] = 1.0f / (p.decay / (1.0f + 0.55f * (float)k));
	}
	float phase[NP];
	for(int k = 0; k < NP; k++) phase[k] = ph[k];
	for(int i = 0; i < len; i++) {
		float t = (float)i / SRATE;
		float bend = powf(2.0f, (p.bendCents * fminf(t / 0.35f, 1.0f)) / 1200.0f);
		float vib = powf(2.0f, (p.vibCents * sinf(6.28318f * p.vibHz * t)
				* fminf(t / 0.25f, 1.0f)) / 1200.0f);
		float f0 = p.f0 * bend * vib;
		/* pick-position comb + attack noise burst */
		float env = expf(-t * 1.0f / p.decay);
		float atk = i < 40 ? (float)i / 40.0f : 1.0f;
		float acc = 0.0f;
		for(int k = 0; k < NP; k++) {
			float kk = (float)(k + 1);
			float fk = kk * f0 * sqrtf(1.0f + B * kk * kk) * det[k];
			if(fk > 0.45f * SRATE) break;
			phase[k] += 6.28318f * fk / SRATE;
			if(phase[k] > 6.28318f) phase[k] -= 6.28318f;
			float a = 1.0f / (kk * kk * 0.35f + 0.65f * kk);
			acc += a * sinf(phase[k]) * expf(-t * dk[k]);
		}
		float noise = i < 300 ? 0.06f * (frnd() - 0.5f) * (1.0f - (float)i / 300.0f) : 0.0f;
		out[n0 + i] += p.amp * atk * (acc * env + noise);
	}
}

/* ---------------- LEVEL REALISM (2026-09-15) ----------------
 *
 * THE BENCH HAS ALWAYS PLAYED TOO LOUD, AND EQUALLY LOUD.
 *
 * Every pluck above is amp 0.10..0.32 on all six channels. The instrument
 * is neither: the device traces put a ringing string's env at 0.01..0.1,
 * i.e. ~10x below the bench, and the hex pickup does NOT deliver the six
 * strings at one level - the two PLAIN top strings (B and high e; here
 * indices 4 and 5) move far less metal past the pole piece than the four
 * wound ones and sit several dB down.
 *
 * That combination is exactly what the 2026-09-09 investigation found the
 * engine mis-calibrated for, and a bench that cannot express it cannot
 * regression-test the fixes. So: two environment knobs, both no-ops when
 * unset, so every number ever recorded from this bench stays reproducible.
 *
 *   HG_BENCH_PICKUP=g   multiply ALL material by g (device realism: 0.1)
 *                       and apply the per-string PICKUP[] taper below.
 *   HG_BENCH_SOLO=s     generate material for string s ONLY, so the
 *                       stereo dump and disc.py measure ONE string
 *                       instead of a six-way sum in which a top-string
 *                       artifact is buried under four louder neighbours.
 *
 * PICKUP[] is a plain magnetic-hex profile: wound strings at unity, the
 * plain B and high e ~5 and ~7 dB down. It is a MODEL, not a measurement
 * - the point is only that the top two are quieter, which the device
 * trace establishes independently.
 */
static const float PICKUP[NUM_STRINGS] =
	{ 1.00f, 1.00f, 0.95f, 0.85f, 0.55f, 0.45f };

static float benchPickup(void)
{
	const char *e = getenv("HG_BENCH_PICKUP");
	return e ? (float)atof(e) : 0.0f;    /* 0 = feature off, legacy levels */
}
static int benchSolo(void)
{
	const char *e = getenv("HG_BENCH_SOLO");
	return e ? atoi(e) : -1;
}
/*
 * HG_BENCH_NOISE=rms adds white noise to EVERY channel, including silent
 * ones. This is what makes a level threshold falsifiable in both
 * directions: lowering a gate until quiet notes pass is only half the
 * job, and a bench whose "silence" is exact zeros can never show the
 * other half. A silent string that still produces onsets, mode flips or
 * marks under noise is a gate set too low. 3e-4 is the same order as
 * GATE_RMS, i.e. the level the engine already calls silence.
 */
static float benchNoise(void)
{
	const char *e = getenv("HG_BENCH_NOISE");
	return e ? (float)atof(e) : 0.0f;
}

/* returns total seconds */
static float genMaterial(std::vector<float> ch[NUM_STRINGS], float seconds)
{
	int n = (int)(seconds * SRATE);
	for(int s = 0; s < NUM_STRINGS; s++) ch[s].assign(n, 0.0f);
	float pu = benchPickup();
	int solo = benchSolo();
	/* GROUND TRUTH for the onset detector: how many picks this material
	 * actually contains, per string. The engine's own HG_PROBE "O" events
	 * are only meaningful against this number - 18 onsets is a perfect
	 * score or a 40% miss depending on how many picks were played. */
	int nPluck[NUM_STRINGS];
	for(int s = 0; s < NUM_STRINGS; s++) nPluck[s] = 0;
	/* C standard, played 25 cents FLAT of the engine's OPEN_HZ table -
	 * mirrors how the instrument is actually tuned (slightly flat by
	 * taste) and permanently stress-tests the TUNE_MARGIN headroom */
	static const float OPEN[NUM_STRINGS] =
		{ 64.47f, 86.06f, 114.87f, 153.33f, 193.19f, 257.88f };
	static const float STIFF[NUM_STRINGS] =
		{ 8e-5f, 6e-5f, 4e-5f, 2.5e-5f, 1.5e-5f, 8e-6f };

	for(int s = 0; s < NUM_STRINGS; s++) {
		float t = 0.12f + 0.9f * frnd();
		while(t < seconds - 0.4f) {
			Pluck p;
			p.t0 = t;
			int fret = (int)(frnd() * 8.0f);
			p.f0 = OPEN[s] * powf(2.0f, (float)fret / 12.0f);
			p.amp = 0.10f + 0.22f * frnd();
			p.bendCents = (frnd() < 0.3f) ? (frnd() * 180.0f - 30.0f) : 0.0f;
			p.vibHz = 4.5f + 2.0f * frnd();
			p.vibCents = 12.0f + 30.0f * frnd();
			p.decay = 0.55f + 1.2f * frnd();
			renderPluck(ch[s], p, STIFF[s]);
			nPluck[s]++;
			/* repick jitter: sometimes a fast repick of the same note */
			if(frnd() < 0.25f) {
				Pluck q = p;
				q.t0 = t + 0.13f + 0.09f * frnd();
				q.amp *= 0.8f;
				q.bendCents = 0.0f;
				if(q.t0 < seconds - 0.4f) {
					renderPluck(ch[s], q, STIFF[s]);
					nPluck[s]++;
				}
			}
			t += 0.30f + 0.85f * frnd();
		}
	}
	/* a 6-string strum ~60% in: strings staggered by 8 ms, all at once */
	{
		float t0 = seconds * 0.6f;
		for(int s = 0; s < NUM_STRINGS; s++) {
			Pluck p;
			p.t0 = t0 + 0.008f * (float)s;
			p.f0 = OPEN[s] * powf(2.0f, (float)((s * 2) % 5) / 12.0f);
			p.amp = 0.26f;
			p.bendCents = 0.0f;
			p.vibHz = 5.0f; p.vibCents = 8.0f;
			p.decay = 1.1f;
			renderPluck(ch[s], p, STIFF[s]);
			nPluck[s]++;
		}
	}
	printf("PICKS      ");
	for(int s = 0; s < NUM_STRINGS; s++)
		printf(" s%d:%3d", s, (solo >= 0 && s != solo) ? 0 : nPluck[s]);
	printf("\n");
	/* level realism applied LAST, so the material itself (and therefore
	 * every pluck timing, bend and rng draw) is bit-identical to the
	 * legacy bench and only the gain differs */
	if(pu > 0.0f || solo >= 0) {
		for(int s = 0; s < NUM_STRINGS; s++) {
			float g = (solo >= 0 && s != solo) ? 0.0f
				: (pu > 0.0f ? pu * PICKUP[s] : 1.0f);
			if(g == 1.0f) continue;
			for(int i = 0; i < n; i++) ch[s][i] *= g;
		}
	}
	/* noise goes on AFTER the taper: it models the CONVERTER, which is
	 * downstream of the pickup and therefore identical on all six */
	{
		float nz = benchNoise();
		if(nz > 0.0f)
			for(int s = 0; s < NUM_STRINGS; s++)
				for(int i = 0; i < n; i++)
					ch[s][i] += nz * 3.46f * (frnd() - 0.5f);
	}
	return seconds;
}

/* ------------------------- scenes ------------------------- */

static void allOff(void)
{
	gPitchActive = 0; gFormantActive = 0; gFmActive = 0;
	gVibOn = gChorusOn = gEnvFOn = gFWahOn = 0;
	gSineOn = gPogOn = gResOn = gSusOn = 0;
	gJawOn = gSitarOn = 0;
	gSagOn = gSympOn = 0;
	gPresetFormantWobble = 0;
	gPanOn = 1;                      /* device default */
	applyPitch();
	applyFormant();
}

static void scene(int which)
{
	allOff();
	if(which == 1) {                 /* FORMANT ONLY - the crackling config */
		gFormantActive = 1;
		gFormantVal = 1.35f;
		applyFormant();
	} else if(which == 2) {          /* several effects on */
		gFormantActive = 1;
		gFormantVal = 1.35f;
		gPitchActive = 1;
		gPitchSemitones = -12.0f;
		gVibOn = 1; gChorusOn = 1; gResOn = 1; gSusOn = 1;
		applyPitch();
		applyFormant();
	} else if(which == 3) {          /* formant only, LPC mode */
		gFormantActive = 1;
		gFormantMode = 1;
		gFormantVal = 1.35f;
		applyFormant();
	} else if(which == 4) {          /* PITCH UP only, +7 */
		gPitchActive = 1;
		gPitchSemitones = 7.0f;
		applyPitch();
	} else if(which == 5) {          /* PITCH UP only, +12 */
		gPitchActive = 1;
		gPitchSemitones = 12.0f;
		applyPitch();
	} else if(which == 6) {          /* PITCH UP +12 stacked with FORMANT */
		gPitchActive = 1;
		gPitchSemitones = 12.0f;
		gFormantActive = 1;
		gFormantVal = 1.35f;
		applyPitch();
		applyFormant();
	} else if(which == 7) {          /* PITCH UP +7 stacked with FORMANT down */
		gPitchActive = 1;
		gPitchSemitones = 7.0f;
		gFormantActive = 1;
		gFormantVal = 0.80f;
		applyPitch();
		applyFormant();
	}
}

/* ---------- scheduled UI events (slot presses + encoder detents) ----------
 * These are the parameter STEPS the smoothers exist to hide. The schedule
 * engages and disengages exactly the effects whose smoothers were changed
 * (vibrato / chorus / POG / sitar-tarab), in both directions, on top of
 * live decaying material, plus encoder detents while they are engaged. */
static void fireEvent(int id)
{
#ifndef NUM_CATEGORIES
	/* the archived v5 tree has a flat pad map and no slot/category API;
	 * there the schedule degrades to plain on/off toggles. */
	switch(id) {
	case 0: gVibOn = 1; break;
	case 2: gChorusOn = 1; break;
	case 3: gPogOn = 1; break;
	case 4: gSitarOn = 1; break;
	case 7: gVibOn = 0; gChorusOn = 0; break;
	case 8: gPogOn = 0; gSitarOn = 0; break;
	default: break;
	}
#else
	const FxSlot *sl;
	switch(id) {
	case 0: gVibOn = 1;   sl = &gCatSlot[CAT_MOD][1];  applySlotPreset(sl); break;
	case 1: applyEncoders(gCatSlot[CAT_MOD][1].fx, 3, 2, 0, 0); break;
	case 2: gChorusOn = 1; sl = &gCatSlot[CAT_MOD][2]; applySlotPreset(sl); break;
	case 3: gPogOn = 1;    sl = &gCatSlot[CAT_FILTER][1]; applySlotPreset(sl); break;
	case 4: gSitarOn = 1;  sl = &gCatSlot[CAT_SPECTRAL][2]; applySlotPreset(sl); break;
	case 5: applyEncoders(gCatSlot[CAT_SPECTRAL][2].fx, -4, 3, 0, 0); break;
	case 6: applyEncoders(gCatSlot[CAT_MOD][2].fx, 5, -3, 0, 0); break;
	case 7: gVibOn = 0; gChorusOn = 0; break;
	case 8: gPogOn = 0; gSitarOn = 0; break;
	default: break;
	}
#endif
}
static const float EVENT_T[9] =
	{ 0.9f, 1.5f, 2.1f, 2.7f, 3.3f, 3.9f, 4.3f, 4.8f, 5.4f };

static const char *sceneName(int w)
{
	switch(w) {
	case 0: return "a_all_off";
	case 1: return "b_formant_only";
	case 2: return "c_several_fx";
	case 3: return "d_formant_lpc";
	case 4: return "e_up7";
	case 5: return "f_up12";
	case 6: return "g_up12_formant";
	case 7: return "h_up7_formdown";
	}
	return "?";
}

/* ---- effects-chain-only microbench: isolates suspect #1 exactly ----
 * Runs render.cpp's per-sample effect order for all six strings, with
 * NOTHING else (no tracker, no shifter, no mix/limiter), so the cost of
 * the BYPASSED effect path is measured on its own. */
static int effectsOnlyBench(BelaContext *ctx, int enable)
{
	if(!setup(ctx, NULL)) return 1;
	scene(enable);
	float sr = SRATE;
	float vibDepthSmp = 0.00106f * sr, chorBaseSmp = 0.008f * sr;
	float chorDepthSmp = 0.002f * sr, pogStep = 1.0f / (0.18f * sr);
	const int NS = 2000000;
	float dryIn[NUM_STRINGS];
	for(int s = 0; s < NUM_STRINGS; s++) {
		gV[s].env = 0.2f;
		gV[s].period = gV[s].p0;
	}
	double bestUs = 1e18;
	for(int rep = 0; rep < 3; rep++) {
		float acc = 0.0f;
		auto t0 = std::chrono::steady_clock::now();
		for(int n = 0; n < NS; n++) {
			float ph = (float)n * 0.01f;
			for(int s = 0; s < NUM_STRINGS; s++)
				dryIn[s] = 0.2f * sinf(ph * (1.0f + 0.1f * (float)s));
			float outL = 0.0f, outR = 0.0f;
			for(int s = 0; s < NUM_STRINGS; s++) {
				Voice *v = &gV[s];
				float mixed = dryIn[s];
				runFM(v, &mixed);
				runAutowah(v, &mixed);
				runResonator(v, &mixed);
				runJawari(v, &mixed);
				runSustainer(v, &mixed);
				runSitarZing(v, &mixed);
				runSympathetic(v, s, dryIn, &mixed);
				runPluckSag(v, &mixed, sr);
				runVibrato(v, s, &mixed, vibDepthSmp, sr);
				runPogSwell(v, &mixed, pogStep);
				float wide = 0.0f;
				runChorus(v, s, &mixed, chorBaseSmp, chorDepthSmp, &wide, sr);
				outL += mixed; outR += mixed + wide;
			}
			runSitarTarab(dryIn, &outL, &outR);
			acc += outL + outR;
		}
		auto t1 = std::chrono::steady_clock::now();
		double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
		if(us < bestUs) bestUs = us;
		if(!std::isfinite(acc)) printf("(non-finite acc)\n");
	}
	printf("EFFECTS-ONLY scene %-16s %10.2f ns/sample (6 strings, 12 blocks)\n",
			sceneName(enable), bestUs * 1000.0 / (double)NS);
	return 0;
}

int main(int argc, char **argv)
{
	int which = argc > 1 ? atoi(argv[1]) : 1;
	float seconds = argc > 2 ? (float)atof(argv[2]) : 6.0f;
	const char *dump = argc > 3 ? argv[3] : NULL;
	int reps = argc > 4 ? atoi(argv[4]) : 3;

	BelaContext ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.audioSampleRate = SRATE;
	ctx.audioFrames = BS;
	ctx.digitalFrames = BS;
	ctx.audioInChannels = 16;

	if(which >= 10)
		return effectsOnlyBench(&ctx, which - 10);

	std::vector<float> ch[NUM_STRINGS];
	genMaterial(ch, seconds);
	int nTot = (int)ch[0].size();
	int nBlocks = nTot / BS;

	std::vector<double> best;
	std::vector<float> outL, outR;

	for(int r = 0; r < reps; r++) {
		if(!setup(&ctx, NULL)) { printf("setup failed\n"); return 1; }
		gFormantMode = 0;
		scene(which);
		std::vector<double> us(nBlocks);
		outL.assign(nTot, 0.0f);
		outR.assign(nTot, 0.0f);
		int nextEv = 0;
		for(int b = 0; b < nBlocks; b++) {
			if(dump && which < 4) {   /* artifact run: fire the scheduled UI
			                           * events (pitch-up scenes 4+ measure the
			                           * steady engaged path - no UI events) */
				float t = (float)(b * BS) / SRATE;
				while(nextEv < 9 && t >= EVENT_T[nextEv])
					fireEvent(nextEv++);
			}
			for(int s = 0; s < NUM_STRINGS; s++)
				for(int i = 0; i < BS; i++)
					gBenchIn[STRING_CHANNEL[s]][i] = ch[s][b * BS + i];
			auto t0 = std::chrono::steady_clock::now();
			render(&ctx, NULL);
			auto t1 = std::chrono::steady_clock::now();
			us[b] = std::chrono::duration<double, std::micro>(t1 - t0).count();
			for(int i = 0; i < BS; i++) {
				outL[b * BS + i] = gBenchOut[0][i];
				outR[b * BS + i] = gBenchOut[1][i];
			}
		}
		if(r == 0 || us.size() != best.size()) best = us;
		else for(size_t i = 0; i < us.size(); i++)
			if(us[i] < best[i]) best[i] = us[i];
	}

	std::vector<double> sorted = best;
	std::sort(sorted.begin(), sorted.end());
	double sum = 0.0;
	for(size_t i = 0; i < best.size(); i++) sum += best[i];
	double mean = sum / (double)best.size();
	double budget = (double)BS / (double)SRATE * 1e6;
	printf("SCENE %-16s blocks %d  mean %8.3f us  p50 %8.3f  p99 %8.3f  max %8.3f"
			"  (host budget %.1f us -> mean %.2f%%)\n",
			sceneName(which), nBlocks, mean,
			sorted[sorted.size() / 2], sorted[(size_t)(sorted.size() * 0.99)],
			sorted.back(), budget, 100.0 * mean / budget);

	if(dump) {
		FILE *f = fopen(dump, "wb");
		if(f) {
			fwrite(outL.data(), sizeof(float), outL.size(), f);
			fwrite(outR.data(), sizeof(float), outR.size(), f);
			fclose(f);
		}
	}
	return 0;
}
