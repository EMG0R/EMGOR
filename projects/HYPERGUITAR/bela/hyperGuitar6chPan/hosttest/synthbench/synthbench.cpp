/*
 * synthbench.cpp - host harness for synth.cpp. #includes the .cpp so the
 * file-static engine state can be reset between scenarios. The tracker is
 * EMULATED: after a pick, attackBlank holds for ACQ_HOPS hops (dbgGate 6),
 * then the post-pick pitch is accepted (conf 0.95, dbgGate 0). Everything
 * else (env, releaseBlank, period glitches, bleed) is scripted per scenario.
 */
#include "hyperguitar.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

float pblep(float t, float dt)   /* verbatim from dsp_core.cpp */
{
	if(t < dt) { float x = t / dt; return x + x - x * x - 1.0f; }
	if(t > 1.0f - dt) { float x = (t - 1.0f) / dt; return x * x + x + x + 1.0f; }
	return 0.0f;
}
volatile int gSynthMode = 0;
volatile int gSynthPoly = 1;
float gSynthLevel = 1.0f;
float gSynthGateThr = 0.010f;
float gSineAmtS = 1.0f;
float gOtherMax[NUM_STRINGS];
unsigned int gSampleStamp = 0;
unsigned int gOnsetStamp[NUM_STRINGS];
Voice gV[NUM_STRINGS];
int gLeadStr = 0;
float gLeadInc = 0.002f, gLeadIncTgt = 0.002f, gLeadAmp = 0.0f, gLeadLPz = 0.0f;

/* 2026-11-05: stubs for the tracker-published band level + diag ring synth.cpp now uses.
 * This harness predates the continuous-dynamics engine (its scripted env no longer drives
 * the amplitude); the authoritative synth tests are hosttest/test_synth.inc. */
float gBandLvl[NUM_STRINGS];
float gBandHist[NUM_STRINGS][BAND_HIST_N];
int   gBandHistIdx[NUM_STRINGS];
void shDiagPush(const ShDiagEv *) {}
int  shDiagPop(ShDiagEv *) { return 0; }

#include "names.inc"   /* SYNTH_NAMES, extracted from render.cpp by the Makefile */
#include "synth.cpp"

#define SR 44100.0f
static const int S = 4;                       /* test string */

static void resetAll(float f0)
{
	memset(gV, 0, sizeof(gV));
	for(int s = 0; s < NUM_STRINGS; s++) {
		gV[s].srate = SR; gV[s].beta = 1.0f; gV[s].period = SR / f0;
		gV[s].hopCountdown = HOP;
		gOtherMax[s] = 0.0f; gOnsetStamp[s] = 0;
		sStale[s] = 1; sSt[s] = SynStr();
	}
	gSampleStamp = 0; gSineAmtS = 1.0f; gSynthPoly = 1; gSynthMode = 0;
	gSynthGateThr = 0.010f;
}

struct Scn {
	float A0, ringSec, endA;     /* pick level; decay so A hits endA at ringSec */
	float dampAt;                /* fret release / palm mute time (s), <0 none */
	int   dampFlag;              /* tracker raises releaseBlank at the damp */
	float hammerAt, hammerSt;    /* hammer-on time & semitones, <0 none */
	int   bleedOnly;             /* no real note: env 0.01, dbgGate 2, false onset */
	int   acqHops;               /* emulated tracker post-pick acceptance, hops */
	float pickAt;
	float total;
	float glitchAt;              /* 2 hops of a wrong confident pitch, <0 none */
	float otherMax;              /* loudest other string (bleed guard) */
};

struct Res {
	float vel, peakAmp, tOnAfterAcc_ms, tOnAfterPick_ms, noteLen, strLen, hopsAfterAcc;
	int onEvents; float ampAtRetrig; float pitchJumpSt; int pitchFrozenOK;
};

static Res run(const Scn &c, int verbose)
{
	resetAll(196.0f);
	Voice *v = &gV[S];
	Res r; memset(&r, 0, sizeof(r));
	int n = (int)(c.total * SR);
	int pickN = (int)(c.pickAt * SR);
	int accN = pickN + c.acqHops * HOP;
	float tau = c.ringSec / logf(c.A0 / c.endA);
	float A = 0.0f, Aenv = 0.0f;
	float period = SR / 196.0f;
	int released = 0, relCnt = 0, wasOn = 0, firstOn = -1, offN = -1;
	float lastInc = 0, incAtDamp = 0, incMaxDevAfterDamp = 0;
	int strDeadN = -1;
	for(int i = 0; i < n; i++) {
		float t = (float)i / SR;
		if(!c.bleedOnly) {
			if(i == pickN) A = c.A0;
			if(i > pickN) {
				A *= expf(-1.0f / (tau * SR));
				if(c.dampAt >= 0 && t > c.dampAt) A *= expf(-1.0f / (0.020f * SR));
			}
			if(A > c.endA && i > pickN) strDeadN = -1; else if(strDeadN < 0 && i > pickN) strDeadN = i;
			Aenv = A;
		} else {
			Aenv = 0.010f;
		}
		/* env = peak follower: instant attack, 0.9995/sample release */
		v->env = Aenv > v->env ? Aenv : v->env * 0.9995f;
		v->sh.envS += (0.64f * Aenv - v->sh.envS) * (1.0f / (3.6f * SR / 196.0f));
		if(i == pickN || (c.bleedOnly && i == pickN)) {
			gOnsetStamp[S] = (unsigned)i + 1;      /* stamp != 0 */
		}
		if(--v->hopCountdown <= 0) {
			v->hopCountdown = HOP;
			/* ---- tracker emulation ---- */
			v->dbgGate = 0; v->pitchConf = 0.95f;
			if(i >= pickN && i < accN && gOnsetStamp[S]) { v->attackBlank = 1; v->dbgGate = 6; v->pitchConf = 0.0f; }
			else v->attackBlank = 0;
			if(i == pickN + HOP && 0) {}
			if(c.bleedOnly) { v->dbgGate = 2; v->pitchConf = 0.0f; v->attackBlank = 0; }
			if(c.dampAt >= 0 && t > c.dampAt && c.dampFlag) {
				if(relCnt == 0) v->releaseBlank = 8;
				relCnt = 1;
			}
			if(v->releaseBlank > 0) { v->releaseBlank--; v->pitchConf = 0; v->dbgGate = 1; }
			/* garbage pitch while the string is being damped / released */
			if(c.dampAt >= 0 && t > c.dampAt) { period = SR / (196.0f * (1.0f + 0.15f * sinf(0.7f * (float)i))); v->pitchConf = c.dampFlag ? 0.0f : 0.5f; }
			else period = SR / 196.0f;
			if(c.hammerAt >= 0 && t > c.hammerAt) period = SR / (196.0f * powf(2.0f, c.hammerSt / 12.0f));
			if(c.glitchAt >= 0 && t > c.glitchAt && t < c.glitchAt + 2.0f * HOP / SR) period = SR / (196.0f * 1.2f);
			v->period = period;
			gOtherMax[S] = c.otherMax;
			(void)released;
		}
		float mixed = 0.0f;
		float inc0 = v->pInc;
		runSynthPoly(v, S, &mixed);
		gSampleStamp++;
		(void)inc0;
		if(v->pAmp > r.peakAmp) r.peakAmp = v->pAmp;
		int on = sSt[S].on;
		if(on && !wasOn) {
			r.onEvents++;
			if(firstOn < 0) {
				firstOn = i;
				r.vel = sSt[S].vel;
				r.hopsAfterAcc = (float)(i - accN) / HOP;
				r.tOnAfterAcc_ms = 1000.0f * (i - accN) / SR;
				r.tOnAfterPick_ms = 1000.0f * (i - pickN) / SR;
			} else r.ampAtRetrig = v->pAmp;
		}
		if(!on && wasOn) offN = i;
		wasOn = on;
		if(c.dampAt >= 0 && t > c.dampAt && i == (int)(c.dampAt * SR) + 1) incAtDamp = v->pInc;
		if(c.dampAt >= 0 && t > c.dampAt) {
			float d = fabsf(v->pInc - incAtDamp) / (incAtDamp + 1e-9f);
			if(d > incMaxDevAfterDamp) incMaxDevAfterDamp = d;
		}
		if(c.hammerAt >= 0 && fabsf(t - (c.hammerAt - 0.05f)) < 0.5f / SR) lastInc = v->pInc;
		if(c.glitchAt >= 0 && fabsf(t - (c.glitchAt - 0.05f)) < 0.5f / SR) lastInc = v->pInc;
		if(verbose && (i % (int)(SR / 4)) == 0) printf("  t=%.2f env=%.4f amp=%.3f on=%d\n", t, v->env, v->pAmp, on);
	}
	if(firstOn >= 0) {
		int endN = offN >= 0 ? offN : n;
		r.noteLen = (float)(endN - firstOn) / SR;
	}
	r.strLen = strDeadN >= 0 ? (float)(strDeadN - pickN) / SR : c.total - c.pickAt;
	r.pitchJumpSt = lastInc > 0 ? 17.3123f * logf(v->pInc / lastInc) : 0.0f;
	r.pitchFrozenOK = incMaxDevAfterDamp < 0.01f;
	r.ampAtRetrig += 0.0f;
	return r;
}

/* ---- centroid ---- */
static void fft(std::vector<float> &re, std::vector<float> &im)
{
	size_t n = re.size();
	for(size_t i = 1, j = 0; i < n; i++) {
		size_t bit = n >> 1;
		for(; j & bit; bit >>= 1) j ^= bit;
		j ^= bit;
		if(i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
	}
	for(size_t len = 2; len <= n; len <<= 1) {
		float ang = -2.0f * (float)M_PI / (float)len;
		for(size_t i = 0; i < n; i += len)
			for(size_t k = 0; k < len / 2; k++) {
				float wr = cosf(ang * k), wi = sinf(ang * k);
				float ur = re[i + k], ui = im[i + k];
				float xr = re[i + k + len / 2] * wr - im[i + k + len / 2] * wi;
				float xi = re[i + k + len / 2] * wi + im[i + k + len / 2] * wr;
				re[i + k] = ur + xr; im[i + k] = ui + xi;
				re[i + k + len / 2] = ur - xr; im[i + k + len / 2] = ui - xi;
			}
	}
}

static float centroid(int mode, float f0, float brt, float lvl = 1.0f)
{
	float ph[6] = { 0, 0, 0, 0, 0, 0 }, filt[4] = { 0, 0, 0, 0 }, lfo = 0, lph = 0;
	static AVoice av; memset(&av, 0, sizeof(av));
	float inc = f0 / SR;
	const int N = 65536;
	std::vector<float> re(N, 0.0f), im(N, 0.0f);
	int total = (int)SR;
	/* 0.5 s warm-up, then 1 s measured (LFO phase swept through) */
	for(int i = 0; i < total + (int)(0.5f * SR); i++) {
		lph += 0.35f / SR; if(lph >= 1) lph -= 1;
		lfo = sinf(2.0f * (float)M_PI * lph);
		float y = synthVoiceSample(av, mode, ph, filt, inc, lfo, SR, brt, lvl, 0, 0);
		int k = i - (int)(0.5f * SR);
		if(k >= 0 && k < N) {
			float w = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * k / (float)(total - 1));
			re[k] = (k < total ? y * w : 0.0f);
		}
	}
	fft(re, im);
	double num = 0, den = 0;
	for(int k = 1; k < N / 2; k++) {
		double m = sqrt((double)re[k] * re[k] + (double)im[k] * im[k]);
		double f = (double)k * SR / N;
		num += f * m; den += m;
	}
	return (float)(num / (den + 1e-12));
}

int main(int argc, char **argv)
{
	if(argc > 1 && !strcmp(argv[1], "centroid")) {
		/* syn_fmod default 0.5; centroid at level 1 (filter at its base) and at level 0.3 (closed by the level tracking) */
		printf("mode name  centroid@196Hz: level1  level0.3 (fmod %.2f)\n", gSynFmod);
		for(int m = 0; m < SYNTH_NMODES; m++) {
			float c1 = centroid(m, 196.0f, 1.0f, 1.0f);
			float c2 = centroid(m, 196.0f, 1.0f, 0.3f);
			printf("%2d %-14s %8.1f %8.1f\n", m, SYNTH_NAMES[m], c1, c2);
		}
		return 0;
	}
	if(argc > 1 && !strcmp(argv[1], "cpu")) {
		/* ns per voice-sample for each patch: 6 s of audio, level 0.7, 196 Hz, fmod from argv[2] (default 0.5) */
		if(argc > 2) gSynFmod = (float)atof(argv[2]);
		printf("mode name            ns/sample (fmod %.2f)   peak   rms\n", gSynFmod);
		for(int m = 0; m < SYNTH_NMODES; m++) {
			float ph[6] = { 0, 0, 0, 0, 0, 0 }, filt[4] = { 0, 0, 0, 0 };
			AVoice av; memset(&av, 0, sizeof(av));
			const int N = (int)(6.0f * SR);
			double acc = 0; float pk = 0; double e = 0;
			struct timespec t0, t1;
			clock_gettime(CLOCK_MONOTONIC, &t0);
			for(int i = 0; i < N; i++) {
				float y = synthVoiceSample(av, m, ph, filt, 196.0f / SR, 0.0f, SR, 1.0f, 0.7f, 0, 1);
				acc += y; e += (double)y * y; if(fabsf(y) > pk) pk = fabsf(y);
			}
			clock_gettime(CLOCK_MONOTONIC, &t1);
			double ns = ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / N;
			printf("%2d %-14s %8.1f %12.3f %6.3f  %s\n", m, SYNTH_NAMES[m], ns, pk, sqrt(e / N), acc != acc ? "NaN" : "");
		}
		return 0;
	}
	int verbose = argc > 1 && !strcmp(argv[1], "-v");
	Scn base = { 0.30f, 6.0f, 0.004f, -1, 0, -1, 0, 0, 14, 0.5f, 9.0f, -1, 0 };
	printf("acq = emulated tracker acceptance %d hops after the pick (%.1f ms)\n\n", base.acqHops, 1000.0f * base.acqHops * HOP / SR);

	/* 1/2 hard vs soft, then velocity-ref adaptation */
	{
		resetAll(196.0f);
		Scn h = base; h.A0 = 0.30f; h.ringSec = 6.0f; h.endA = 0.004f;
		Res rh = run(h, verbose);
		/* carry the adapted reference across: soft pick after a hard one */
		float refKeep = sSt[S].ref;
		Scn so = base; so.A0 = 0.04f; so.endA = 0.004f; so.ringSec = 6.0f;
		/* run() resets everything: emulate session by presetting ref afterwards */
		Res rs_fresh = run(so, 0);
		printf("[hard pick  A0=0.30] vel=%.2f peakAmp=%.3f on %.1f ms after acceptance (%.2f hops) / %.1f ms after pick\n", rh.vel, rh.peakAmp, rh.tOnAfterAcc_ms, rh.hopsAfterAcc, rh.tOnAfterPick_ms);
		printf("   note %.2f s vs string above env 0.004: %.2f s\n", rh.noteLen, rh.strLen);
		printf("[soft pick  A0=0.04, fresh session] vel=%.2f peakAmp=%.3f (ref adapts: first pick ~full)\n", rs_fresh.vel, rs_fresh.peakAmp);
		/* session: hard then soft on the same string -> custom loop */
		resetAll(196.0f);
		sSt[S].ref = refKeep; sSt[S].init = 1;
		(void)refKeep;
	}
	/* session test: hard, then soft, same engine state (reference carried) */
	{
		resetAll(196.0f);
		Voice *v = &gV[S];
		float amps[2] = { 0, 0 }, vels[2] = { 0, 0 };
		float A0s[2] = { 0.30f, 0.06f };
		int seg = (int)(2.0f * SR);
		float A = 0, period = SR / 196.0f;
		for(int k = 0; k < 2; k++) {
			int pickN = 10;
			for(int i = 0; i < seg; i++) {
				if(i == pickN) { A = A0s[k]; gOnsetStamp[S] = gSampleStamp + 1; }
				else A *= expf(-1.0f / (1.2f * SR));
				v->env = A > v->env ? A : v->env * 0.9995f;
				v->sh.envS += (0.64f * A - v->sh.envS) * 0.005f;
				if(--v->hopCountdown <= 0) {
					v->hopCountdown = HOP;
					v->dbgGate = 0; v->pitchConf = 0.95f; v->attackBlank = 0;
					if(i < 14 * HOP) { v->attackBlank = 1; v->dbgGate = 6; v->pitchConf = 0; }
					v->period = period;
				}
				float m = 0; runSynthPoly(v, S, &m); gSampleStamp++;
				if(i == 20000) amps[k] = v->pAmp;
				if(i == 20000) vels[k] = sSt[S].vel;
			}
		}
		printf("[session hard->soft, same string] hard: vel=%.2f amp=%.3f | soft (0.06/0.30): vel=%.2f amp=%.3f  (ratio amp %.2f, input ratio 0.20)\n\n", vels[0], amps[0], vels[1], amps[1], amps[1] / (amps[0] + 1e-9f));
	}
	/* 3 long ring */
	{
		Scn c = base; c.A0 = 0.05f; c.ringSec = 6.0f; c.endA = 0.004f; c.total = 9.0f;
		Res r = run(c, verbose);
		printf("[6 s ring 0.05 -> 0.004] note %.2f s, string above 0.004 for %.2f s (old synth: dies when env<0.005 after ~3 s)\n", r.noteLen, r.strLen);
		Scn c2 = base; c2.A0 = 0.30f; c2.ringSec = 6.0f; c2.endA = 0.0015f; c2.total = 12.0f;
		Res r2 = run(c2, 0);
		printf("[hard 0.30 -> 0.0015 over 6 s] note %.2f s, string above 0.0015 for %.2f s\n\n", r2.noteLen, r2.strLen);
	}
	/* 4 fret release at 2 s */
	{
		for(int fl = 1; fl >= 0; fl--) {
			Scn c = base; c.A0 = 0.10f; c.ringSec = 6.0f; c.endA = 0.004f; c.dampAt = 2.0f; c.dampFlag = fl; c.total = 5.0f; c.pickAt = 0.0f;
			c.pickAt = 0.1f;
			Res r = run(c, 0);
			printf("[fret release at 2.0 s, tracker releaseBlank=%d] note-ons=%d, note ended %.2f s after on (release at %.2f s after on), pitch frozen=%s\n", fl, r.onEvents, r.noteLen, 2.0f - 0.1f - r.tOnAfterPick_ms / 1000.0f, r.pitchFrozenOK ? "yes" : "NO");
		}
		printf("\n");
	}
	/* 5 hammer-on */
	{
		Scn c = base; c.A0 = 0.20f; c.ringSec = 8.0f; c.endA = 0.01f; c.hammerAt = 1.0f; c.hammerSt = 2.0f; c.total = 2.0f; c.pickAt = 0.1f;
		Res r = run(c, 0);
		printf("[hammer-on +2 st at 1.0 s] note-ons=%d (1 = legato kept the note, no retrigger), vel=%.2f, pitch change applied: %+.2f st\n", r.onEvents, r.vel, r.pitchJumpSt);
		/* glitch: 2 hops of wrong pitch must NOT move pitch */
		printf("\n");
	}
	/* glitch + bleed guard */
	{
		Scn c = base; c.A0 = 0.20f; c.ringSec = 8.0f; c.endA = 0.01f; c.glitchAt = 1.0f; c.total = 1.5f; c.pickAt = 0.1f;
		Res r = run(c, 0);
		printf("[2-hop wrong confident pitch (+3.2 st) at 1.0 s] note-ons=%d, pitch change applied: %+.3f st\n", r.onEvents, r.pitchJumpSt);
		Scn d = base; d.A0 = 0.05f; d.otherMax = 0.20f; d.total = 2.0f; d.pickAt = 0.2f;
		Res rd = run(d, 0);
		printf("[bleed guard: env 0.05 under a 0.20 neighbour] note-ons=%d\n\n", rd.onEvents);
	}
	/* 6 bleed only */
	{
		Scn c = base; c.bleedOnly = 1; c.total = 2.0f; c.pickAt = 0.2f;
		Res r = run(c, 0);
		printf("[bleed-only, dbgGate 2 + a false onset] note-ons=%d peakAmp=%.4f\n", r.onEvents, r.peakAmp);
	}
	return 0;
}
