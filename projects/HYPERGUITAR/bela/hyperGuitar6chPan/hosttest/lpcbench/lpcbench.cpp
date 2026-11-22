/*
 * lpcbench.cpp - host harness for lpc.cpp (formant mode).
 *
 * Build: make            (current ../../lpc.cpp)   -> ./lb_new
 *        make old        (frozen e4b7844 lpc.cpp)  -> ./lb_old
 * Run:   ./lb_new [all|steady|attack|ident|cpu] [string]
 *
 * Material: stiff-string stacks f_k = k f0 sqrt(1+B k^2) with amplitude
 * EXACTLY E_in(f_k) (known envelope: -4 dB/oct tilt + three resonances),
 * resampled by a plain windowed-sinc resampler at rho. The resampler output
 * y is what the pitch stage hands lpcProcess. lpcAnalyze sees the input ring
 * at the same SOURCE time (conservative: zero lead).
 *
 *  (a) envelope error: dB RMS over output partials in 100 Hz..8 kHz between
 *      the measured partial level and E_in(f'/phi), gain removed.
 *  (b) attack: 10-90% rise of a 1 ms RMS envelope (output time), vs y.
 *  (c) steady gain: 10log10(P_out / P_y), ideal 0.
 */
#include "hyperguitar.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <chrono>
#include <algorithm>

#define SR 44100.0
Voice gV[NUM_STRINGS];
unsigned int gSampleStamp = 0;
unsigned int gOnsetStamp[NUM_STRINGS] = { 0, 0, 0, 0, 0, 0 };

#include LPC_SRC

static const double OPEN[6] = { 65.41, 87.31, 116.54, 155.56, 196.0, 261.63 };

/* ---------------- known envelopes ---------------- */
struct Env { double c[3], bw[3], gd[3]; double tilt; };
static double envDb(const Env &e, double f)
{
	double d = e.tilt * log2(std::max(f, 50.0) / 200.0);
	for(int i = 0; i < 3; i++) {
		double x = (f - e.c[i]) / (0.5 * e.bw[i]);
		d += e.gd[i] / (1.0 + x * x);
	}
	return d;
}
static double envAmp(const Env &e, double f) { return pow(10.0, envDb(e, f) / 20.0); }
static const Env ENV_A = { { 450, 1500, 3300 }, { 200, 500, 900 }, { 16, 14, 12 }, -5.0 };
static const Env ENV_B = { { 700, 2100, 4200 }, { 250, 600, 1000 }, { 15, 13, 12 }, -4.0 };

static unsigned int rs = 1234567u;
static double rnd(void) { rs = rs * 1664525u + 1013904223u; return (double)(rs >> 8) / 16777216.0; }

/* additive stiff-string stack; amp = E(f_k) * gate(t) */
static void synthStack(std::vector<float> &x, int i0, int n, double f0, double B, const Env &e,
		double peak, double atkTau, double decTau, bool constAmp)
{
	std::vector<double> ph, fk, ak;
	for(int k = 1; k < 200; k++) {
		double f = k * f0 * sqrt(1.0 + B * k * k);
		if(f > 0.46 * SR) break;
		fk.push_back(f);
		ak.push_back(envAmp(e, f));
		ph.push_back(rnd() * 6.2831853);
	}
	double pk = 0;
	for(size_t k = 0; k < ak.size(); k++) pk += ak[k];
	for(int i = 0; i < n; i++) {
		double t = i / SR, s = 0;
		for(size_t k = 0; k < fk.size(); k++)
			s += ak[k] * sin(6.2831853 * fk[k] * t + ph[k]);
		double g = constAmp ? 1.0 : (1.0 - exp(-t / atkTau)) * exp(-t / decTau);
		if((size_t)(i0 + i) < x.size()) x[i0 + i] += (float)(peak * g * s / pk * 4.0);
	}
}

/* windowed-sinc resampler: y[m] = x(m*rho + off), lowpass at min(1,1/rho) Nyquist */
static double sincInterp(const std::vector<float> &x, double t, double fc)
{
	const int H = 24;
	int i0 = (int)floor(t);
	double s = 0;
	for(int j = i0 - H + 1; j <= i0 + H; j++) {
		if(j < 0 || j >= (int)x.size()) continue;
		double d = t - j, a = d / H;
		if(fabs(a) >= 1) continue;
		double sc = (fabs(d) < 1e-9) ? fc : sin(M_PI * fc * d) / (M_PI * d);
		double w = 0.5 + 0.5 * cos(M_PI * a);
		s += x[j] * sc * w;
	}
	return s;
}
static void resample(const std::vector<float> &x, std::vector<float> &y, double rho, int n)
{
	y.assign(n, 0.0f);
	double fc = rho > 1.0 ? 1.0 / rho : 1.0;
	for(int m = 0; m < n; m++) y[m] = (float)sincInterp(x, m * rho, fc);
}

static long gInvalid = 0, gHops = 0;
static double gBenchF0 = 0;   /* tracker f0 hint -> sh.Tsm (the engine always has one) */
static void resetVoice(int s, double rho, double phi)
{
	Voice *v = &gV[s];
	memset(v, 0, sizeof(Voice));
	v->sh.Tsm = gBenchF0 > 0 ? (float)(SR / gBenchF0) : 0.0f;
	v->beta = (float)rho;
	v->formant = (float)phi;
	v->srate = (float)SR;
	gSampleStamp = 100000;
	for(int i = 0; i < NUM_STRINGS; i++) gOnsetStamp[i] = 0;
}

/* loud enough that the silence test is not what made it invalid */
static bool r0Loud(Voice *v, int wp)
{
	double e = 0; for(int i = 1; i <= 256; i++) { float q = v->buf[(wp - i) & BUF_MASK]; e += q * q; }
	return e > 1e-3;
}

/* run: input x (source-time ring), y (shifted), output z. onsets: output-sample
 * indices at which the engine's onset detector fires. */
static void runLpc(int s, const std::vector<float> &x, const std::vector<float> &y, double rho,
		double phi, std::vector<float> &z, const std::vector<int> &onsets, double *usPerHop = NULL)
{
	resetVoice(s, rho, phi);
	Voice *v = &gV[s];
	int n = (int)y.size();
	z.assign(n, 0.0f);
	int wp = 0, hopC = 0, oi = 0;
	double tAn = 0; int nAn = 0;
	for(int m = 0; m < n; m++) {
		int need = (int)floor(m * rho) + 1;
		while(wp < need && wp < (int)x.size()) { v->buf[wp & BUF_MASK] = x[wp]; wp++; }
		v->writePos = wp;
		while(oi < (int)onsets.size() && onsets[oi] <= m) { gOnsetStamp[s] = gSampleStamp; oi++; }
		if(++hopC >= HOP) {
			hopC = 0;
			auto t0 = std::chrono::high_resolution_clock::now();
			lpcAnalyze(v);
			if(fabs(rho - phi) > 0.01 * rho && v->srate > 0 && r0Loud(v, wp)) { gHops++; if(!v->sh.lpcValid) gInvalid++; }
			auto t1 = std::chrono::high_resolution_clock::now();
			tAn += std::chrono::duration<double, std::micro>(t1 - t0).count(); nAn++;
		}
		z[m] = lpcProcess(v, y[m]);
		gSampleStamp++;
	}
	if(usPerHop) *usPerHop = nAn ? tAn / nAn : 0;
}

/* single-bin windowed amplitude */
static double binAmp(const float *x, int n, double f)
{
	double re = 0, im = 0, ws = 0;
	for(int i = 0; i < n; i++) {
		double w = 0.5 - 0.5 * cos(2 * M_PI * (i + 0.5) / n);
		double a = 2 * M_PI * f * i / SR;
		re += w * x[i] * cos(a); im -= w * x[i] * sin(a); ws += w;
	}
	return 2.0 * sqrt(re * re + im * im) / ws;
}

struct SteadyRes { double errDb, gainDb; };
static SteadyRes steadyMeasure(const std::vector<float> &out, const std::vector<float> &y, int i0, int n,
		double f0, double B, double rho, double phi, const Env &e)
{
	std::vector<double> d;
	for(int k = 1; k < 400; k++) {
		double fs = k * f0 * sqrt(1.0 + B * k * k);
		if(fs > 0.46 * SR / std::max(1.0, rho)) break;     /* survives the resampler */
		double fo = fs * rho;
		if(fo < 100 || fo > 8000) continue;
		double meas = binAmp(&out[i0], n, fo);
		double tgt = envAmp(e, fo / phi);
		if(meas < 1e-9) meas = 1e-9;
		d.push_back(20 * log10(meas / tgt));
	}
	SteadyRes r = { 0, 0 };
	if(d.empty()) return r;
	double mean = 0;
	for(double q : d) mean += q;
	mean /= d.size();
	double ss = 0;
	for(double q : d) ss += (q - mean) * (q - mean);
	r.errDb = sqrt(ss / d.size());
	double po = 0, py = 0;
	for(int i = 0; i < n; i++) { po += (double)out[i0 + i] * out[i0 + i]; py += (double)y[i0 + i] * y[i0 + i]; }
	r.gainDb = 10 * log10(po / py);
	return r;
}

static double riseTime(const std::vector<float> &x, int t0, int len, double *peakOut)
{
	const int W = 44;
	std::vector<double> env(len, 0.0);
	double acc = 0;
	for(int i = 0; i < len + W; i++) {
		double a = (t0 - W + i >= 0 && t0 - W + i < (int)x.size()) ? x[t0 - W + i] : 0.0;
		acc += a * a;
		if(i >= W) { double b = x[t0 - W + i - W]; acc -= b * b; }
		if(i >= W) env[i - W] = sqrt(std::max(acc, 0.0) / W);
	}
	double mx = 0;
	for(int i = 0; i < len; i++) mx = std::max(mx, env[i]);
	if(peakOut) { double pk = 0; for(int i = 0; i < len; i++) pk = std::max(pk, (double)fabsf(x[t0 + i])); *peakOut = pk; }
	int a = -1, b = -1;
	for(int i = 0; i < len; i++) {
		if(a < 0 && env[i] > 0.1 * mx) a = i;
		if(b < 0 && env[i] > 0.9 * mx) { b = i; break; }
	}
	return (a < 0 || b < 0) ? -1.0 : (b - a) * 1000.0 / SR;
}

static const double RHOS[5] = { 0.5, 0.75, 1.0, 1.5, 2.0 };
static const double PHIS[3] = { 0.75, 1.0, 1.3 };

static void steadyAll(int s, double f0, double B)
{
	gBenchF0 = f0;
	printf("\nSTEADY  string %d  f0=%.1f Hz  B=%.1e   envelope err dB RMS (bypass err / lpc err) | gain dB\n", s, f0, B);
	printf("  rho \\ phi |");
	for(double ph : PHIS) printf("   phi=%.2f: by   lpc  gain |", ph);
	printf("\n");
	double sumE = 0, sumB = 0, sumG = 0; int cnt = 0;
	for(double rho : RHOS) {
		printf("  %5.2f    |", rho);
		int N = (int)(2.0 * SR), M = (int)(N / rho) - 64;
		std::vector<float> x(N + 4096, 0.0f);
		rs = 99; synthStack(x, 0, (int)x.size(), f0, B, ENV_A, 0.3, 1, 1, true);
		std::vector<float> y, z;
		resample(x, y, rho, M);
		for(double ph : PHIS) {
			std::vector<int> none;
			runLpc(s, x, y, rho, ph, z, none);
			int i0 = M - (int)(0.5 * SR), n = (int)(0.4 * SR);
			SteadyRes lp = steadyMeasure(z, y, i0, n, f0, B, rho, ph, ENV_A);
			SteadyRes by = steadyMeasure(y, y, i0, n, f0, B, rho, ph, ENV_A);
			printf("   %5.2f %5.2f %5.2f |", by.errDb, lp.errDb, lp.gainDb);
			sumE += lp.errDb; sumB += by.errDb; sumG += fabs(lp.gainDb); cnt++;
		}
		printf("\n");
	}
	printf("  MEAN: bypass %.2f dB, lpc %.2f dB, |gain| %.2f dB\n", sumB / cnt, sumE / cnt, sumG / cnt);
}

static void attackAll(int s, double f0)
{
	gBenchF0 = f0;
	printf("\nATTACK  string %d  note B at %.1f Hz after note A (different envelope); rise 10-90%% in ms, output time\n", s, f0);
	printf("  rho  phi |  y(ref)  lpc   delta  | peak ratio lpc/y | level dB @20-30ms\n");
	double sumD = 0, sumL = 0, sumP = 0; int cnt = 0;
	for(double rho : RHOS) for(double ph : PHIS) {
		int N = (int)(1.6 * SR);
		std::vector<float> x(N + 4096, 0.0f);
		rs = 5;
		synthStack(x, 0, (int)(0.8 * SR), f0 * 0.8, 2e-4, ENV_B, 0.3, 0.001, 0.5, false);
		int tB = (int)(0.8 * SR);
		synthStack(x, tB, N - tB, f0, 2e-4, ENV_A, 0.3, 0.0008, 0.5, false);
		int M = (int)((N - 2000) / rho);
		std::vector<float> y, z;
		resample(x, y, rho, M);
		std::vector<int> ons;
		ons.push_back(1);
		ons.push_back((int)(tB / rho));
		runLpc(s, x, y, rho, ph, z, ons);
		int t0 = (int)(tB / rho) - 8, len = (int)(0.03 * SR);
		double pk1, pk2;
		double ry = riseTime(y, t0, len, &pk1), rz = riseTime(z, t0, len, &pk2);
		double ey = 0, ez = 0;
		for(int i = (int)(0.02 * SR); i < (int)(0.03 * SR); i++) { ey += (double)y[t0 + i] * y[t0 + i]; ez += (double)z[t0 + i] * z[t0 + i]; }
		double ld = 10 * log10(ez / ey);
		printf("  %4.2f %4.2f | %6.2f %6.2f %+6.2f  | %5.2f            | %+5.2f\n", rho, ph, ry, rz, rz - ry, pk2 / pk1, ld);
		sumL += fabs(ld); sumP += fabs(20 * log10(pk2 / pk1));
		if(fabs(rho - ph) > 0.01) { sumD += rz - ry; cnt++; }
	}
	printf("  MEAN delta rise (non-bypass cases) %+.3f ms | mean |peak dB| %.2f | mean |level dB| %.2f\n", sumD / cnt, sumP / 15, sumL / 15);
}

/* after a re-pick: envelope error of the NEW note in successive 25 ms windows
 * (partials measured against E_A(f'/phi)); shows how long stale lattices linger */
static void convergeAll(int s, double f0)
{
	static const double T[] = { 0.000, 0.010, 0.020, 0.035, 0.050, 0.080, 0.130, 0.250 };
	const int NT = 8;
	gBenchF0 = f0;
	printf("\nCONVERGE string %d  new note %.1f Hz after a different-envelope note; env err dB (25 ms windows starting at +t ms)\n", s, f0);
	printf("  rho  phi |");
	for(int i = 0; i < NT; i++) printf(" +%3.0f", T[i] * 1000);
	printf("   | mean(0..80ms)\n");
	double RH[] = { 1.0, 1.0, 1.5, 0.75 }, PH[] = { 1.3, 0.75, 1.0, 1.0 };
	double gm = 0; int gc = 0;
	for(int c = 0; c < 4; c++) {
		double rho = RH[c], phi = PH[c];
		int N = (int)(1.4 * SR), tB = (int)(0.6 * SR);
		std::vector<float> x(N + 4096, 0.0f);
		rs = 5;
		synthStack(x, 0, tB + 2000, f0 * 0.8, 2e-4, ENV_B, 0.3, 0.001, 2.0, false);
		synthStack(x, tB, N - tB, f0, 2e-4, ENV_A, 0.3, 0.0008, 3.0, false);
		int M = (int)((N - 2000) / rho);
		std::vector<float> y, z;
		resample(x, y, rho, M);
		std::vector<int> ons; ons.push_back(1); ons.push_back((int)(tB / rho));
		runLpc(s, x, y, rho, phi, z, ons);
		printf("  %4.2f %4.2f |", rho, phi);
		double m80 = 0; int k80 = 0;
		for(int i = 0; i < NT; i++) {
			int i0 = (int)(tB / rho) + (int)(T[i] * SR), n = (int)(0.025 * SR);
			SteadyRes r = steadyMeasure(z, y, i0, n, f0, 2e-4, rho, phi, ENV_A);
			printf(" %4.1f", r.errDb);
			if(T[i] <= 0.08) { m80 += r.errDb; k80++; }
		}
		printf("   | %.2f\n", m80 / k80);
		gm += m80 / k80; gc++;
	}
	printf("  MEAN(0..80ms) %.2f\n", gm / gc);
}

/* rho == phi == 1 (and rho == phi): output must be the input */
static void identAll(int s)
{
	printf("\nIDENTITY string %d  max|out - y| / peak(y)  (rho=phi cases are a plain resample)\n", s);
	double RP[3] = { 1.0, 1.5, 0.75 };
	for(int c = 0; c < 3; c++) {
		int N = (int)(1.0 * SR), M = (int)(N / RP[c]) - 64;
		std::vector<float> x(N + 4096, 0.0f), y, z;
		rs = 3; synthStack(x, 0, (int)x.size(), OPEN[s] * 1.5, 1e-4, ENV_A, 0.3, 1, 1, true);
		resample(x, y, RP[c], M);
		std::vector<int> none;
		gBenchF0 = OPEN[s] * 1.5;
		runLpc(s, x, y, RP[c], RP[c], z, none);
		double md = 0, pk = 0;
		for(int i = 0; i < M; i++) { md = std::max(md, (double)fabsf(z[i] - y[i])); pk = std::max(pk, (double)fabsf(y[i])); }
		printf("  rho=phi=%.2f  %.3e (%.1f dBFS rel)\n", RP[c], md / pk, md > 0 ? 20 * log10(md / pk) : -999.0);
	}
}

static void cpuAll(int s)
{
	std::vector<float> x(40000, 0.0f), y, z;
	rs = 7; synthStack(x, 0, (int)x.size(), 196.0, 1e-4, ENV_A, 0.3, 1, 1, true);
	resample(x, y, 0.5, 30000);
	std::vector<int> none;
	double us = 0;
	runLpc(s, x, y, 0.5, 1.0, z, none, &us);
	auto t0 = std::chrono::high_resolution_clock::now();
	for(int rep = 0; rep < 3; rep++) {
		resetVoice(s, 0.5, 1.0);
		Voice *v = &gV[s];
		for(int i = 0; i < 4096; i++) v->buf[i] = x[i];
		v->writePos = 4096;
		v->sh.lpcValid = 1; v->sh.lpcMix = 1.0f;
		for(int i = 0; i < 20000; i++) { volatile float o = lpcProcess(v, y[i]); (void)o; }
	}
	auto t1 = std::chrono::high_resolution_clock::now();
	double perS = std::chrono::duration<double, std::nano>(t1 - t0).count() / 60000.0;
	printf("\nCPU string %d: analyse %.1f us/hop (host), process %.1f ns/sample (host)\n", s, us, perS);
}

#ifdef LPC_BENCH
static double oracleA(double f) { return envDb(ENV_A, f); }
/* pure envelope-fit fidelity of the analysis stage (true-domain grid vs E_in) */
static void fitAll(int s, double f0, double B)
{
	gBenchF0 = f0;
	double rho = 1.0, phi = 1.3;
	int N = (int)(0.6 * SR);
	std::vector<float> x(N + 4096, 0.0f), z;
	rs = 99; synthStack(x, 0, (int)x.size(), f0, B, ENV_A, 0.3, 1, 1, true);
	std::vector<float> y(x.begin(), x.begin() + N);
	std::vector<int> none;
	runLpc(s, x, y, rho, phi, z, none);
	/* partials */
	std::vector<double> dp, dg;
	for(int k = 1; k < 400; k++) {
		double fk = k * f0 * sqrt(1.0 + B * k * k);
		if(fk > 8000) break;
		if(fk < 100) continue;
		double fm = fk / (SR / 2) * (LPC_NG - 1), fr = fm - floor(fm);
		int i = (int)fm;
		double g = (1 - fr) * gLpcDbgP[i] + fr * gLpcDbgP[i + 1];
		dp.push_back(10 * log10(g) - envDb(ENV_A, fk));
	}
	for(int m = 0; m < LPC_NG; m++) {
		double f = m * (SR / 2) / (LPC_NG - 1);
		if(f < 100 || f > 8000) continue;
		dg.push_back(10 * log10(gLpcDbgP[m]) - envDb(ENV_A, f));
	}
	auto rms = [](std::vector<double> &d) { double mu = 0; for(double q : d) mu += q; mu /= d.size(); double ss = 0; for(double q : d) ss += (q - mu) * (q - mu); return sqrt(ss / d.size()); };
	if(getenv("FITDUMP")) for(int m = 0; m < LPC_NG; m += 2) { double f = m * (SR / 2) / (LPC_NG - 1); printf("  %7.0f Hz  model %7.2f  true %7.2f\n", f, 10 * log10(gLpcDbgP[m]), envDb(ENV_A, f)); }
	printf("FITREFIT w %.2f r %.2f\n", gLpcDbgRefitErr[0], gLpcDbgRefitErr[1]);
	printf("FIT string %d f0 %.0f: at-partials %.2f dB, whole-grid %.2f dB\n", s, f0, rms(dp), rms(dg));
}
#endif

int main(int argc, char **argv)
{
	const char *what = argc > 1 ? argv[1] : "all";
	int s = argc > 2 ? atoi(argv[2]) : 4;
	double f0 = OPEN[s] * 1.0;
	bool all = !strcmp(what, "all");
#ifdef LPC_BENCH
	if(getenv("ORACLE")) gLpcOracle = oracleA;
	if(!strcmp(what, "fit")) {
		for(int ss = 0; ss < 6; ss++) { fitAll(ss, OPEN[ss], 1e-4); fitAll(ss, OPEN[ss] * 2, 3e-4); }
		return 0;
	}
	if(!strcmp(what, "fithi")) {   /* sparse-harmonic high notes: poles chasing single partials */
		static const double F[4] = { 440, 660, 880, 1100 };
		for(int i = 0; i < 4; i++) fitAll(5, F[i], 5e-4);
		return 0;
	}
#endif
#ifdef LPC_BENCH
	if(getenv("ORACLE")) gLpcOracle = oracleA;
#endif
	if(all || !strcmp(what, "steady")) {
		steadyAll(s, f0, 1e-4);
		steadyAll(s, f0 * 2.0, 3e-4);   /* sparser, higher note */
	}
	if(all || !strcmp(what, "attack")) attackAll(s, f0 * 1.26);
	if(all || !strcmp(what, "converge")) convergeAll(s, f0 * 1.26);
	if(all || !strcmp(what, "ident")) identAll(s);
	if(all || !strcmp(what, "cpu")) cpuAll(s);
	printf("\nINVALID analyses while loud (non-bypass): %ld of %ld hops\n", gInvalid, gHops);
	return 0;
}
