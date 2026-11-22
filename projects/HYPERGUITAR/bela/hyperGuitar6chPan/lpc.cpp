/*
 * lpc.cpp - true spectral-envelope formant mode (v2, 2026-10-01).
 *
 * WHAT IT DOES. The pitch stage is a resampler in both directions, so the
 * pitch-shifted signal p carries the INPUT's spectral envelope scaled in
 * frequency by rho = v->beta:  E_p(w) = E_in(w/rho). The user's formant
 * ratio phi = v->formant is relative to the ORIGINAL formants, so the
 * wanted output envelope is E_r(w) = E_in(w/phi) (phi = 1 with any pitch =
 * true formant-corrected pitch shift; phi = rho = the plain resample).
 *
 * Per hop (lpcAnalyze), on the INPUT ring (cleanest signal):
 *   pre-emphasis -> 31-tap half-band lowpass (cutoff 0.5 Nyq, every other
 *   tap zero: 9 MAC/output) + decimate by 2 -> Hann taper
 *   -> autocorr (lag window + noise loading) -> Levinson -> envelope on a LPC_NG-point
 *   grid, tilt put back (divide by the pre-emphasis response); the model
 *   covers 0..~8.3 kHz and a -12 dB/oct tail continues it above (2026-11-01
 *   lpc-q: was held flat, which boosted pickup noise) -> TWO warped, power-normalised
 *   grids:
 *     E_w(w) = E_in(w/rho)   what p really carries  (whitening target)
 *     E_r(w) = E_in(w/phi)   what we want           (recolour target)
 *   (the filter actually fitted is the ratio R = E_r/E_w, see lpcAnalyze: an
 *   all-pole lattice by cosine-sum autocorrelation + Levinson, then
 *   bandwidth-expanded a_j *= 0.994^j and converted back to reflection
 *   coefficients, for both fitted lattices.)
 *   gain g = sqrt(Er_r / Er_w): with equal grid power both are in the same
 *   units, and g == 1 whenever the two targets coincide.
 *
 * Per sample (lpcProcess): FIR lattice B -> g -> IIR lattice 1/A1 (the ratio
 * filter; no pre/de-emphasis in the audio path, it cancels in R). Reflection
 * coefficients move by a PER-SAMPLE LINEAR RAMP (2026-11-01 lpc-q: was a
 * zero-order-held slew every 4th sample, which zippered through the high-Q
 * poles). A hop ramp is 128 samples; a convex combo of stable lattices is
 * stable. Then a slow level trim (0.7..1.4, held below a power floor), a
 * soft knee above |y| 0.9 and the NaN scrub.
 *
 * BYPASS IS EXACT: |phi - rho| < 0.2% (this includes rho = phi = 1) sets
 * lpcValid = 0 and returns before any math - the output is the input.
 *
 * ONSET SNAP (clarity). At an onset (gOnsetStamp[s] changed) the running
 * lattices are kept (the output is NOT slammed dry). Once an analysis that
 * is at least LPC_HOLD(s) samples after the onset exists, the coefficients
 * ramp linearly to it over 256 samples with the lattice states kept
 * (a crossfade of coefficient sets, no state wipe, lpcCorr kept). When the
 * string was bypassed/idle (mix < 0.5) the coefficients are set instantly,
 * the states zeroed and primed with the full 128-sample input history, and
 * the mix fades in (0.01/sample). IDLE: after 8 hops (1024 samples) of
 * dbgGate==2 the mix fades to 0 (stale-model hiss on a silent string); the
 * wake snap waits for an analysis made after the string woke.
 *
 * TRIG TABLES. cosT/sinT[m][j] = cos/sin(pi*m/(NG-1) * j) are computed
 * once. They serve the envelope evaluation AND the refit unconditionally
 * (HG_LPC_REFIT_TABLE is retired). Measured on the old build: the inline
 * cosf in the refit moved 3 of 140,573 rw[] values by one ULP (tracker
 * output bit-identical, worst audio delta -132 dBFS) - and its arguments
 * run to ~1608 rad, i.e. full argument reduction x 561 per hop per string.
 * The table removes all of them.
 *
 * WHY DECIMATE BY 2 (measured, hosttest/lpcbench): at 44.1 kHz an order-24
 * model spent its poles on 10-22 kHz partials (a pickup carries nothing
 * useful there) and fit the 100 Hz-8 kHz envelope to ~2.5 dB RMS. Analysing
 * the 0-11 kHz band at half rate doubles the poles per Hz where formants
 * live (~1.4 dB RMS) and halves the autocorrelation MACs for the same window.
 *
 * Tunables are #ifndef-guarded so hosttest/lpcbench can sweep them with -D;
 * the values below are the measured winners (see LPC-FINDINGS.md).
 */
#include "hyperguitar.h"
#include "hg_mc.h"
#include <cmath>
#include <cstring>
#include <atomic>
#ifdef HG_LPC_SPREAD   /* 2026-11-01 lpc-w: the spread analyser reads v->buf over several samples; no worker for it */
#undef HG_LPC_WORKER
#define HG_LPC_WORKER 0
#endif

#ifndef LPC_NG
#define LPC_NG 65             /* envelope grid points over 0..pi (full rate). 2026-11-01 lpc-q: 65 (was 129): -32% analysis time, +0.2 dB fit error */
#endif
#ifndef LPC_PREEMPH
#define LPC_PREEMPH 0.94f
#endif
/* 2026-11-01 lpc-q: lag-window sigma = clamp(0.5*f0, 100, 250) Hz (was 0.25*f0, 60..250: below
 * the harmonic spacing of low strings, so the envelope chased single partials) */
#ifndef LPC_LAGFIX
#define LPC_LAGFIX 1
#endif
#ifndef LPC_BW_HZ
#if LPC_LAGFIX
#define LPC_BW_HZ 100.0f      /* Gaussian lag-window bandwidth, floor */
#else
#define LPC_BW_HZ 60.0f
#endif
#endif
#ifndef LPC_BW_FRAC
#if LPC_LAGFIX
#define LPC_BW_FRAC 0.5f      /* ... grows to this fraction of f0 (sparse harmonics) */
#else
#define LPC_BW_FRAC 0.25f
#endif
#endif
#ifndef LPC_BW_MAX
#define LPC_BW_MAX 250.0f
#endif
#ifndef LPC_RBW_HZ
#define LPC_RBW_HZ 20.0f      /* lag window on the ratio fits (full rate) */
#endif
#ifndef LPC_MASK_DB
#define LPC_MASK_DB -40.0f    /* ratio is pulled to 1 where the signal is >40 dB below its peak */
#endif
#ifndef LPC_RNOISE_DB
#define LPC_RNOISE_DB -60.0f  /* loading on the ratio fits */
#endif
#ifndef LPC_NOISE_DB
#define LPC_NOISE_DB -45.0f   /* white-noise loading on r[0] (pre-emphasised) */
#endif
#ifndef LPC_ORD_LO
#define LPC_ORD_LO 24         /* strings 0,1. 2026-11-01 lpc-q: 24 (was 28): -20% on the autocorr+lattice for those strings, +0.3 dB fit error */
#endif
#ifndef LPC_ORD_HI
#define LPC_ORD_HI 24         /* strings 2..5 */
#endif
#ifndef LPC_WIN_LO
#define LPC_WIN_LO 1024       /* full-rate input samples analysed (512 at half rate) */
#endif
#ifndef LPC_WIN_HI
#define LPC_WIN_HI 1024
#endif
#ifndef LPC_DEC
#define LPC_DEC 2             /* 2 = analyse the 0-11 kHz band at half rate, 1 = full rate */
#endif
#ifndef LPC_HF_M
#define LPC_HF_M ((LPC_NG - 1) * 3 / 8)  /* grid index above which the decimated model is extrapolated (48 of 128 = 8.3 kHz) */
#endif
#ifndef LPC_HF_TAIL
#define LPC_HF_TAIL 1         /* 2026-11-01 lpc-q H2: -12 dB/oct tail above LPC_HF_M (0 = old flat hold) */
#endif
#ifndef LPC_RAMP
#define LPC_RAMP 1            /* 2026-11-01 lpc-q H1: per-sample linear coefficient ramp (0 = old 4-sample slew) */
#endif
#ifndef LPC_RAMP_N
#define LPC_RAMP_N 128        /* shortest hop ramp (analysis every hop) */
#endif
#ifndef LPC_RAMP_MAX
#define LPC_RAMP_MAX 384      /* longest: one analysis per string every 6 blocks (shifter.cpp stagger) */
#endif
#ifndef LPC_SNAP_RAMP_N
#define LPC_SNAP_RAMP_N 256   /* snap crossfade ramp length (H5d) */
#endif
#ifndef LPC_IDLE
#define LPC_IDLE 1            /* 2026-11-01 lpc-q H3: fade out after LPC_IDLE_HOPS silent hops */
#endif
#ifndef LPC_IDLE_HOPS
#define LPC_IDLE_HOPS 8
#endif
#ifndef LPC_AGC_LO
#define LPC_AGC_LO 0.7f       /* 2026-11-01 lpc-q H4 (was 0.4) */
#endif
#ifndef LPC_AGC_HI
#define LPC_AGC_HI 1.4f       /* (was 2.5) */
#endif
#ifndef LPC_AGC_FLOOR
#define LPC_AGC_FLOOR 1e-6f   /* input power below which lpcCorr is held */
#endif
#ifndef LPC_SNAP_FIX
#define LPC_SNAP_FIX 1        /* 2026-11-01 lpc-q H5: keep lpcCorr, 128-sample re-estimate/prime, 256-sample crossfade */
#endif
#ifndef LPC_GMAX
#define LPC_GMAX 2.0f         /* 2026-11-01 lpc-q H6: model gain clamp (was 8) */
#endif
#ifndef LPC_KNEE
#define LPC_KNEE 1            /* 2026-11-01 lpc-q H6: soft knee above |y| 0.9; 2026-11-01 boost: now 0.98 (range 0.02) */
#endif
#ifndef LPC_HANN
#define LPC_HANN 1            /* 2026-11-01 lpc-q: Hann taper on the decimated frame before autocorr */
#endif
#ifndef LPC_BWEXP
#define LPC_BWEXP 0.994f      /* 2026-11-01 lpc-q: a_j *= gamma^j on both fitted lattices (1.0 = off) */
#endif
#ifndef LPC_DECWIDE
#define LPC_DECWIDE 1         /* 2026-11-01 lpc-q H8: 31-tap polyphase half-band (0 = old 15-tap Hamming sinc, 0.46) */
#endif
#ifndef LPC_SLEW
#define LPC_SLEW 0.005f       /* per-sample reflection-coefficient slew */
#endif
#ifndef LPC_ONSET_SNAP
#define LPC_ONSET_SNAP 1
#endif
#ifndef LPC_HOLD_Q
#define LPC_HOLD_Q 6          /* dry hold after an onset = W*Q/8 samples */
#endif
#ifndef LPC_LOGINTERP
#define LPC_LOGINTERP 1       /* warp interpolation in log-power */
#endif
#ifndef LPC_AGC
#define LPC_AGC 1             /* slow level trim: realised output power follows input power */
#endif
#ifndef LPC_AGC_A
#define LPC_AGC_A (1.0f / 1024.0f)  /* power-average coefficient (~23 ms) */
#endif
#ifndef LPC_FADE_UP
#define LPC_FADE_UP 0.01f     /* mix fade-in rate after a snap */
#endif
#define LPC_HIST 128
#if LPC_SNAP_FIX
#define LPC_PRIME 128         /* 2026-11-01 lpc-q: prime/measure from the full history (was 96/48) */
#else
#define LPC_PRIME 96
#endif
#define LPC_TOL 0.002f
#if LPC_DECWIDE
#define LPC_DECTAPS 31
#define LPC_DECFC 0.50f       /* true half-band: h[even d != 0] = 0. Hamming, 31 taps: stopband from ~0.6 Nyq (13.3 kHz),
                               * so everything that folds into the modelled 0..8.8 kHz is >50 dB down; passband flat to ~8.7 kHz.
                               * (0.40 Nyq, as first asked, is not a half-band: no zero taps, 16 MAC/output.) */
#else
#define LPC_DECTAPS 15
#define LPC_DECFC 0.46f
#endif

#if LPC_ORD_LO > LPC_ORDER || LPC_ORD_HI > LPC_ORDER
#error "LPC_ORDER in hyperguitar.h is the max order"
#endif
#if LPC_WIN_LO > LPC_WIN || LPC_WIN_HI > LPC_WIN
#error "LPC_WIN in hyperguitar.h is the max window"
#endif

static inline int ordFor(int s) { return s < 2 ? LPC_ORD_LO : LPC_ORD_HI; }
static inline int winFor(int s) { return s < 2 ? LPC_WIN_LO : LPC_WIN_HI; }

/* ------------------------ precomputed tables ------------------------ */

static HG_TLS float lagW[LPC_ORDER + 1];   /* analysis-rate lag window (rebuilt per hop from f0): per-thread scratch, MC (hg_mc.h) */
static float lagWF[LPC_ORDER + 1];  /* full-rate lag window for the ratio fits */
static float cosT[LPC_NG][LPC_ORDER + 1];
static float sinT[LPC_NG][LPC_ORDER + 1];
static float preM2[LPC_NG];          /* |1 - mu e^-jw|^2 on the full-rate grid */
static float decH[(LPC_DECTAPS + 1) / 2]; /* symmetric lowpass, h[LPC_DECTAPS/2] = centre */
static float decO[LPC_DECTAPS / 4 + 1];   /* half-band: odd-distance taps only, decO[q] = h[c +- (2q+1)] */
static float hannT[2][LPC_WIN];      /* 2026-11-01 lpc-q: Hann taper for the analysis frame, [0]=strings 0-1, [1]=2-5 */
static int tabInit = 0;

static void lpcTablesInit(void)
{
	/* lag window at the ANALYSIS rate (half rate when decimating) */
	for(int i = 0; i <= LPC_ORDER; i++) {
		lagW[i] = 1.0f;
		float xf = 2.0f * (float)M_PI * LPC_RBW_HZ * (float)i / 44100.0f;
		lagWF[i] = expf(-0.5f * xf * xf);
	}
	for(int m = 0; m < LPC_NG; m++) {
		float wm = (float)M_PI * (float)m / (float)(LPC_NG - 1);
		for(int j = 0; j <= LPC_ORDER; j++) {
			cosT[m][j] = cosf(wm * (float)j);
			sinT[m][j] = sinf(wm * (float)j);
		}
		float re = 1.0f - LPC_PREEMPH * cosf(wm), im = LPC_PREEMPH * sinf(wm);
		preM2[m] = re * re + im * im;
	}
	{	/* 2026-11-01 lpc-q: Hamming-windowed sinc, cutoff LPC_DECFC*Nyquist (0.40, 31 taps), unity DC gain */
		const int c = LPC_DECTAPS / 2;
		float full[LPC_DECTAPS], sum = 0.0f;
		for(int k = 0; k < LPC_DECTAPS; k++) {
			float d = (float)(k - c);
			float sc = (k == c) ? LPC_DECFC : sinf((float)M_PI * LPC_DECFC * d) / ((float)M_PI * d);
			float hw = 0.54f - 0.46f * cosf(2.0f * (float)M_PI * (float)k / (float)(LPC_DECTAPS - 1));
			full[k] = sc * hw;
			sum += full[k];
		}
		for(int k = 0; k <= c; k++)
			decH[k] = full[k] / sum;
#if LPC_DECWIDE
		for(int q = 0; 2 * q + 1 <= c; q++)
			decO[q] = decH[c - (2 * q + 1)];
#endif
	}
	for(int g = 0; g < 2; g++) {     /* 2026-11-01 lpc-q: Hann taper tables */
		const int NA = (g == 0 ? LPC_WIN_LO : LPC_WIN_HI) / LPC_DEC;
		for(int i = 0; i < NA; i++)
			hannT[g][i] = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * ((float)i + 0.5f) / (float)NA);
	}
	tabInit = 1;
}

void lpcInitOnce(void)
{
	if(!tabInit)
		lpcTablesInit();
}

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
		{
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

/* 2026-11-01 lpc-q: bandwidth expansion. a_j *= gamma^j, then step-down a -> k
 * (a is overwritten with the expanded polynomial). Returns 0 on |k| >= 0.9999. */
static int bwExpand(float *a, int p, float *k, float gamma)
{
	if(gamma >= 1.0f)
		return 1;
	float gm = 1.0f;
	for(int j = 1; j <= p; j++) {
		gm *= gamma;
		a[j] *= gm;
	}
	float t[LPC_ORDER + 1], tmp[LPC_ORDER + 1];
	for(int j = 1; j <= p; j++) t[j] = a[j];
	for(int i = p; i >= 1; i--) {
		float ki = t[i];
		if(!(ki < 0.9999f && ki > -0.9999f))
			return 0;
		k[i - 1] = ki;
		float den = 1.0f / (1.0f - ki * ki);
		for(int j = 1; j < i; j++)
			tmp[j] = (t[j] - ki * t[i - j]) * den;
		for(int j = 1; j < i; j++)
			t[j] = tmp[j];
	}
	return 1;
}

/* warp a grid: out[m] = in(m/ratio), interpolated (log-power or linear),
 * tail beyond pi held flat; then scaled to carry the same trapezoid power
 * as the source (sumRef) - warping never changes level, the gain stage
 * owns that. */
static void warpGrid(const float *in, const float *lin, float ratio, float sumRef, float *out)
{
	float sum = 0.0f;
	(void)in; (void)lin;
	for(int m = 0; m < LPC_NG; m++) {
		float q = (float)m / ratio;
		float v;
		if(q >= (float)(LPC_NG - 1)) {
#if LPC_LOGINTERP
			v = expf(lin[LPC_NG - 1]);
#else
			v = in[LPC_NG - 1];
#endif
		} else {
			int qi = (int)q;
			float fr = q - (float)qi;
#if LPC_LOGINTERP
			v = expf(lin[qi] + (lin[qi + 1] - lin[qi]) * fr);
#else
			v = in[qi] + (in[qi + 1] - in[qi]) * fr;
#endif
		}
		out[m] = v;
		sum += ((m == 0 || m == LPC_NG - 1) ? 0.5f : 1.0f) * v;
	}
	float sc = sumRef / (sum + 1e-30f);
	for(int m = 0; m < LPC_NG; m++)
		out[m] *= sc;
}

/* refit an all-pole lattice to a power grid (pre-emphasised domain) */
/* all-pole fit of a power grid T on the full-rate grid: T ~ E/|A|^2.
 * Returns E (or -1), reflection coefficients in k, polynomial in a. */
static float fitGrid(const float *T, int p, float *k, float *a)
{
	float rw[LPC_ORDER + 1];
	for(int lag = 0; lag <= p; lag++) {
		float acc = 0.0f;
		for(int m = 0; m < LPC_NG; m++) {
			float wgt = (m == 0 || m == LPC_NG - 1) ? 0.5f : 1.0f;
			acc += wgt * T[m] * cosT[m][lag];
		}
		rw[lag] = acc * lagWF[lag];
	}
	rw[0] *= 1.0f + powf(10.0f, LPC_RNOISE_DB * 0.1f);
	/* the cosine sum is (NG-1)/... scaled; E is returned in the same units
	 * as T because rw[0] ~ sum(T), so divide by the grid weight sum */
	float E = levinson(rw, p, k, a);
	if(E > 0.0f && !bwExpand(a, p, k, LPC_BWEXP))   /* 2026-11-01 lpc-q: 0.994^j bandwidth expansion */
		return -1.0f;
	return E > 0.0f ? E / (float)(LPC_NG - 1) : E;
}

/* |A(e^jw_m)|^2 on grid point m */
static inline float polyMag2(const float *a, int p, int m)
{
	float re = 1.0f, im = 0.0f;
	for(int j = 1; j <= p; j++) {
		re += a[j] * cosT[m][j];
		im -= a[j] * sinT[m][j];
	}
	return re * re + im * im;
}

#ifdef LPC_BENCH
float gLpcDbgP[LPC_NG];   /* lpcbench: analysed true-domain envelope */
float gLpcDbgRefitErr[2];
double (*gLpcOracle)(double hz) = 0;   /* lpcbench: replace the analysed envelope with the truth (dB) */
#endif

#ifndef HG_LPC_SPREAD

/*
 * 2026-11-01 lpc-w: ANALYSIS JOB / RESULT.
 * lpcCompute() is a pure function of an LpcJob (a snapshot of the string's input ring plus the
 * scalars the analysis reads) and writes an LpcResult. It touches no Voice/Shifter and no shared
 * mutable scratch (lagW is __thread; cosT/sinT/preM2/decH/decO/hannT/lagWF are read-only after
 * lpcInitOnce()), so it runs identically on the audio thread (lpcAnalyze, direct) or on the
 * worker (lpcWorkerPoll).
 *   LpcJob    4*(1055+8) ~ 4.2 KB: x[0] = sample before the window, x[1..NU] = the NU newest samples
 *   LpcResult 4*(2*32+4)+... ~ 280 B: k[], kr[] (zero padded to LPC_ORDER), gain, valid, stamp, epoch
 */
struct LpcJob {
	float x[LPC_WIN + LPC_DECTAPS + 1];
	float beta, formant, Tsm, srate;
	unsigned int stamp;     /* gSampleStamp at snapshot: becomes lpcFreshStamp (snap test + ramp length use THIS) */
	unsigned int epoch;
	int s;
};
struct LpcResult {
	float k[LPC_ORDER], kr[LPC_ORDER];
	float gain;
	int valid;
	unsigned int stamp, epoch;
};

volatile unsigned int gTelLpcLate = 0;       /* jobs not finished when due (cumulative) */
volatile unsigned int gTelLpcWorkerUs = 0;   /* max us of one worker run; monitor zeroes */

static void lpcCompute(const LpcJob &J, LpcResult &OUT)
{
	const int s = J.s;
	const int p = ordFor(s), W = winFor(s);
	OUT.valid = 0;
	OUT.stamp = J.stamp;
	OUT.epoch = J.epoch;

	if(!tabInit)
		lpcTablesInit();

	/* exact bypass: formant == pitch ratio is the plain resample */
	float rho = J.beta, phi = J.formant;
	if(rho < 0.25f) rho = 0.25f;
	if(rho > 4.0f) rho = 4.0f;
	if(phi < 0.4f) phi = 0.4f;
	if(phi > 2.4f) phi = 2.4f;
	if(fabsf(phi - rho) < LPC_TOL * rho) {
		OUT.valid = 0;
		return;
	}

	/* lag-window bandwidth follows the note: on sparse high-note harmonics
	 * (spacing >> formant width) a fixed 60 Hz window lets the model chase
	 * single partials; widening it to ~f0/4 smooths across them. */
	{
		float f0 = (J.Tsm > 8.0f) ? J.srate / J.Tsm : 0.0f;
		float bw = LPC_BW_FRAC * f0;
		if(bw < LPC_BW_HZ) bw = LPC_BW_HZ;
		if(bw > LPC_BW_MAX) bw = LPC_BW_MAX;
		const float c = 0.5f * powf(2.0f * (float)M_PI * bw * (float)LPC_DEC / 44100.0f, 2.0f);
		for(int i = 0; i <= p; i++)
			lagW[i] = expf(-c * (float)(i * i));
	}

	/* newest W input samples (+ filter history), pre-emphasised, flat copy */
	float w[LPC_WIN + LPC_DECTAPS + 1];
	const int NU = W + (LPC_DEC == 2 ? LPC_DECTAPS - 1 : 0);
	float prev = J.x[0];   /* 2026-11-01 lpc-w: snapshot of the input ring (x[0] = sample before the window) */
	for(int i = 0; i < NU; i++) {
		float xi = J.x[1 + i];
		w[i] = xi - LPC_PREEMPH * prev;
		prev = xi;
	}
#if LPC_DEC == 2
	/* causal symmetric FIR then every 2nd sample (phase irrelevant to r[]) */
	float d[LPC_WIN / 2];
	const int ND = W / 2, c = LPC_DECTAPS / 2;
	for(int j = 0; j < ND; j++) {
		const float *u = &w[2 * j];          /* taps u[0..LPC_DECTAPS-1], centre u[c] */
#if LPC_DECWIDE
		float acc = decH[c] * u[c];       /* 2026-11-01 lpc-q: half-band, only odd-distance taps are non-zero */
		for(int q = 0; 2 * q + 1 <= c; q++)
			acc += decO[q] * (u[c - 2 * q - 1] + u[c + 2 * q + 1]);
#else
		float acc = decH[c] * u[c];
		for(int k = 0; k < c; k++)
			acc += decH[k] * (u[k] + u[LPC_DECTAPS - 1 - k]);
#endif
		d[j] = acc;
	}
	float *an = d;
	const int NA = ND;
#else
	float *an = w;
	const int NA = W;
#endif
#if LPC_HANN
	{	/* 2026-11-01 lpc-q: Hann taper (rectangular frame = pitch-synchronous coefficient flutter) */
		const float *ht = hannT[s < 2 ? 0 : 1];
		for(int i = 0; i < NA; i++)
			an[i] *= ht[i];
	}
#endif
	float r[LPC_ORDER + 1];
	for(int lag = 0; lag <= p; lag++) {
		float acc = 0.0f;
		for(int i = lag; i < NA; i++)
			acc += an[i] * an[i - lag];
		r[lag] = acc * lagW[lag];
	}
	if(r[0] < 1e-8f) {
		OUT.valid = 0;
		return;
	}
	r[0] *= 1.0f + powf(10.0f, LPC_NOISE_DB * 0.1f);

	float k[LPC_ORDER], a[LPC_ORDER + 1];
	float E = levinson(r, p, k, a);
	if(E <= 0.0f) {
		OUT.valid = 0;
		return;
	}

	/* true-domain power envelope on the FULL-RATE grid: 1/|A|^2 / |pre|^2.
	 * Decimated: the model's grid point 2m (cosT/sinT row 2m of the full
	 * table is the half-rate angle for full-rate index m) covers 0..pi at
	 * half rate = 0..11 kHz; indices above LPC_HF_M hold flat. */
	float P[LPC_NG], L[LPC_NG];
	float sumP = 0.0f;
	for(int m = 0; m < LPC_NG; m++) {
#if LPC_DEC == 2 && LPC_HF_TAIL
		if(m > LPC_HF_M) {   /* 2026-11-01 lpc-q: -12 dB/oct (power ~ f^-4) tail from the value at LPC_HF_M */
			float rr = (float)LPC_HF_M / (float)m;
			P[m] = P[LPC_HF_M] * (rr * rr) * (rr * rr);
		} else {
#endif
#if LPC_DEC == 2
		int mm = m < LPC_HF_M ? m : LPC_HF_M;
		const float *ct = cosT[2 * mm], *st = sinT[2 * mm];
#else
		const float *ct = cosT[m], *st = sinT[m];
#endif
		float re = 1.0f, im = 0.0f;
		for(int j = 1; j <= p; j++) {
			re += a[j] * ct[j];
			im -= a[j] * st[j];
		}
		P[m] = 1.0f / ((re * re + im * im + 1e-9f) * preM2[m]);
#if LPC_DEC == 2 && LPC_HF_TAIL
		}
#endif
#if LPC_LOGINTERP
		L[m] = logf(P[m]);
#else
		L[m] = 0.0f;
#endif
		sumP += ((m == 0 || m == LPC_NG - 1) ? 0.5f : 1.0f) * P[m];
	}
#ifdef LPC_BENCH
	if(gLpcOracle) {
		sumP = 0.0f;
		for(int m = 0; m < LPC_NG; m++) {
			P[m] = (float)pow(10.0, gLpcOracle(m * 22050.0 / (LPC_NG - 1)) / 10.0);
			L[m] = logf(P[m]);
			sumP += ((m == 0 || m == LPC_NG - 1) ? 0.5f : 1.0f) * P[m];
		}
	}
	for(int m = 0; m < LPC_NG; m++) gLpcDbgP[m] = P[m];
#endif

	/* what p really carries, and what we want it to carry */
	float Pw[LPC_NG], Pr[LPC_NG];
	warpGrid(P, L, rho, sumP, Pw);
	warpGrid(P, L, phi, sumP, Pr);

	/* THE FILTER IS THE RATIO. R = E_r/E_w is smooth and low-dynamic-range
	 * (peaks AND dips, no tilt: it needs no pre-emphasis), so two small
	 * all-pole fits reproduce it far better than two whole-envelope fits:
	 *   R ~ (E1/E2) |B|^2/|A1|^2,  A1 from R, B from 1/(R|A1|^2/E1).
	 * Analysis error common to both warps cancels in the ratio. Both grids
	 * carry equal power, so |H|^2 = R preserves the level of a signal that
	 * has spectrum E_w: no extra gain term. */
	float R[LPC_NG], T2[LPC_NG];
	float pmax = 0.0f;
	for(int m = 0; m < LPC_NG; m++)
		if(Pw[m] > pmax) pmax = Pw[m];
	const float eps = pmax * powf(10.0f, LPC_MASK_DB * 0.1f);
	for(int m = 0; m < LPC_NG; m++) {
		/* where the signal is (nearly) empty the ratio is meaningless, and
		 * a +50 dB hump there would eat poles that the signal band needs:
		 * pull it toward unity, R^a with a = Pw/(Pw+eps) */
		float al = Pw[m] / (Pw[m] + eps);
		R[m] = expf(al * logf(Pr[m] / (Pw[m] + 1e-30f) + 1e-30f));
	}
	float k1[LPC_ORDER], a1[LPC_ORDER + 1], k2[LPC_ORDER], a2[LPC_ORDER + 1];
	float E1 = fitGrid(R, p, k1, a1);
	if(E1 <= 0.0f) {
		OUT.valid = 0;
		return;
	}
	float M1[LPC_NG];                                     /* |A1|^2, reused by the gain pass */
	for(int m = 0; m < LPC_NG; m++) {
		M1[m] = polyMag2(a1, p, m);
		T2[m] = E1 / (R[m] * M1[m] + 1e-30f);          /* 1/R2, R2 = R|A1|^2/E1 */
	}
	float E2 = fitGrid(T2, p, k2, a2);
	if(E2 <= 0.0f) {
		OUT.valid = 0;
		return;
	}
#ifdef LPC_BENCH
	{	/* ratio-fit error over 100 Hz..8 kHz in dB RMS (level included) */
		double sd2 = 0; int n = 0;
		for(int m = 0; m < LPC_NG; m++) {
			double f = m * 22050.0 / (LPC_NG - 1);
			if(f < 100 || f > 8000) continue;
			double h = (double)E1 / E2 * polyMag2(a2, p, m) / polyMag2(a1, p, m);
			double d = 10 * log10(h / R[m]);
			sd2 += d * d; n++;
		}
		gLpcDbgRefitErr[0] = (float)sqrt(sd2 / n);
	}
#endif
	/* gain: make the REALISED filter power-preserving on the w-spectrum
	 * (model fit error would otherwise leak into the level) */
	float num = 0.0f, den = 0.0f;
	for(int m = 0; m < LPC_NG; m++) {
		float wgt = (m == 0 || m == LPC_NG - 1) ? 0.5f : 1.0f;
		float h = polyMag2(a2, p, m) / M1[m];
		num += wgt * Pw[m] * h;
		den += wgt * Pw[m];
	}
	float g = sqrtf(den / (num + 1e-30f));
	if(g > LPC_GMAX) g = LPC_GMAX;   /* 2026-11-01 lpc-q: was 8 */
	if(g < 0.125f) g = 0.125f;

	for(int i = 0; i < p; i++) {
		OUT.k[i] = k2[i];      /* FIR lattice: B */
		OUT.kr[i] = k1[i];     /* IIR lattice: 1/A1 */
	}
	for(int i = p; i < LPC_ORDER; i++)
		OUT.k[i] = OUT.kr[i] = 0.0f;
	OUT.gain = g;
	OUT.valid = 1;
}



/* ---- snapshot / apply (audio thread) ---- */
static void lpcSnapshot(const Voice *v, LpcJob &J)
{
	const int s = (int)(v - gV);
	const int NU = winFor(s) + (LPC_DEC == 2 ? LPC_DECTAPS - 1 : 0);
	J.s = s;
	J.beta = v->beta; J.formant = v->formant;
	J.Tsm = v->sh.Tsm; J.srate = v->srate;
	J.stamp = gSampleStamp;
	/* x[i] = buf[(wp - NU - 1 + i) & MASK], i = 0..NU: one or two contiguous copies */
	const unsigned int start = (v->writePos - (unsigned int)NU - 1u) & BUF_MASK;
	const int n = NU + 1;
	const int first = (int)(BUF_SIZE - start) < n ? (int)(BUF_SIZE - start) : n;
	memcpy(J.x, &v->buf[start], (size_t)first * sizeof(float));
	if(first < n)
		memcpy(J.x + first, &v->buf[0], (size_t)(n - first) * sizeof(float));
}

static void lpcApply(Shifter *sh, const LpcResult &R)
{
	if(!R.valid) {
		sh->lpcValid = 0;
		return;
	}
	for(int i = 0; i < LPC_ORDER; i++) {
		sh->lpcK[i] = R.k[i];
		sh->lpcKr[i] = R.kr[i];
	}
	sh->lpcGain = R.gain;
	sh->lpcValid = 1;
	sh->lpcFreshStamp = R.stamp;   /* lpcProcess re-arms its per-sample ramp when this changes */
}

/* direct path (HG_LPC_WORKER 0, worker not running, host tests, lpcbench): identical math, on the caller */
void lpcAnalyze(Voice *v)
{
	LpcJob J;
	LpcResult R;
	lpcSnapshot(v, J);
	J.epoch = 0;
	lpcCompute(J, R);
	lpcApply(&v->sh, R);
}

#if HG_LPC_WORKER
/*
 * WORKER HAND-OFF. Per string: two job buffers (audio writes the one NOT being read), two result
 * buffers (worker writes the one NOT last published). jobSeq = jobs published, doneSeq = newest
 * job index the worker finished, rseq = results published (seqlock for the audio-side copy).
 * The audio thread never waits: no result yet -> it keeps ramping toward the last targets.
 */
struct alignas(64) LpcSlot {
	LpcJob job[2];
	LpcResult res[2];
	std::atomic<unsigned int> jobSeq, doneSeq, rseq;
	unsigned int adopted;       /* audio thread only: last rseq adopted */
	unsigned int epoch;         /* audio thread only: bumped by lpcCancel */
	unsigned int pubStamp;      /* audio thread only: stamp of the newest publish */
	int lateFlag;               /* audio thread only: this job already counted late */
};
static LpcSlot gLS[NUM_STRINGS];
static std::atomic<int> gLpcWorkerOn(0);

void lpcWorkerEnable(int on)
{
	if(on) {
		for(int s = 0; s < NUM_STRINGS; s++) {
			LpcSlot &q = gLS[s];
			q.jobSeq.store(0); q.doneSeq.store(0); q.rseq.store(0);
			q.adopted = 0; q.pubStamp = 0; q.lateFlag = 0;
		}
	}
	gLpcWorkerOn.store(on, std::memory_order_release);
}

void lpcSubmit(Voice *v)
{
	if(!gLpcWorkerOn.load(std::memory_order_acquire)) {
		lpcAnalyze(v);
		return;
	}
	const int s = (int)(v - gV);
	LpcSlot &q = gLS[s];
	const unsigned int js = q.jobSeq.load(std::memory_order_relaxed);
	const unsigned int dn = q.doneSeq.load(std::memory_order_acquire);
	if(js - dn >= 2u) {          /* worker is a full job behind: drop this one rather than touch a buffer it may read */
		gTelLpcLate++;
		return;
	}
	if(js != dn && !q.lateFlag)  /* previous job still unfinished after a whole cadence */
		gTelLpcLate++;
	const unsigned int n = js + 1u;
	LpcJob &J = q.job[n & 1u];
	lpcSnapshot(v, J);
	J.epoch = q.epoch;
	q.pubStamp = J.stamp;
	q.lateFlag = 0;
	q.jobSeq.store(n, std::memory_order_release);
}

void lpcCancel(int s)
{
	if(gLpcWorkerOn.load(std::memory_order_relaxed))
		gLS[s].epoch++;   /* results of jobs published before now are dropped at adoption */
}

void lpcAdoptAll(void)
{
	if(!gLpcWorkerOn.load(std::memory_order_acquire))
		return;
	for(int s = 0; s < NUM_STRINGS; s++) {
		LpcSlot &q = gLS[s];
		const unsigned int js = q.jobSeq.load(std::memory_order_relaxed);
		if(js != q.doneSeq.load(std::memory_order_acquire) && !q.lateFlag
				&& (int)(gSampleStamp - q.pubStamp) > 192) {   /* due within 3 blocks, not here */
			q.lateFlag = 1;
			gTelLpcLate++;
		}
		for(int tries = 0; tries < 2; tries++) {
			const unsigned int r = q.rseq.load(std::memory_order_acquire);
			if(r == q.adopted)
				break;
			LpcResult tmp = q.res[r & 1u];
			std::atomic_thread_fence(std::memory_order_acquire);
			if(q.rseq.load(std::memory_order_relaxed) != r)
				continue;     /* worker published again mid-copy: take the newer one */
			q.adopted = r;
			if(tmp.epoch == q.epoch)
				lpcApply(&gV[s].sh, tmp);
			break;
		}
	}
}

/* worker side: run every string's newest published job; returns jobs run (0 = idle) */
int lpcWorkerPoll(void)
{
	int ran = 0;
	for(int s = 0; s < NUM_STRINGS; s++) {
		LpcSlot &q = gLS[s];
		const unsigned int js = q.jobSeq.load(std::memory_order_acquire);
		if(js == q.doneSeq.load(std::memory_order_relaxed))
			continue;
		const uint64_t t0 = hgTicks();
		const unsigned int r = q.rseq.load(std::memory_order_relaxed) + 1u;
		lpcCompute(q.job[js & 1u], q.res[r & 1u]);
		q.rseq.store(r, std::memory_order_release);
		q.doneSeq.store(js, std::memory_order_release);
		const uint64_t us = (hgTicks() - t0) * 1000000ull / hgTickHz();
		if(us > gTelLpcWorkerUs) gTelLpcWorkerUs = (unsigned int)us;
		ran++;
	}
	return ran;
}
#else  /* !HG_LPC_WORKER */
void lpcSubmit(Voice *v) { lpcAnalyze(v); }
void lpcCancel(int) {}
void lpcAdoptAll(void) {}
int  lpcWorkerPoll(void) { return 0; }
void lpcWorkerEnable(int) {}
#endif


#else  /* HG_LPC_SPREAD */

/*
 * LPC ANALYSIS, SPREAD OVER THE NEXT SAMPLES (2026-10-12, multicore work).
 *
 * The refit is ~60-130 us of straight-line math at one sample of one hop. Here
 * it is cut into four stages that run HG_LPC_GAP samples apart on that same
 * string: stage A (window copy, decimation, autocorrelation, Levinson) at the
 * hop exactly as before, then the envelope + warps, the ratio fit, and the
 * second fit + gain + commit. Every stage works from the intermediates of the
 * one before, so the RESULT (lpcK, lpcKr, lpcGain) is identical to the
 * one-shot analysis of the same window - the only change is WHEN the new
 * targets become visible to lpcProcess: 3*HG_LPC_GAP samples after the hop
 * instead of at it. lpcFreshStamp keeps the HOP's stamp, so the onset-snap
 * freshness rule classifies analyses exactly as it did.
 */
#ifndef HG_LPC_GAP
#define HG_LPC_GAP 24
#endif
struct alignas(64) LpcWork {
	int stage;                 /* 0 idle, else the next stage to run (1..3) */
	int cd;                    /* samples until it runs */
	unsigned int stamp;        /* gSampleStamp at the hop */
	float rho, phi;
	float E;
	float a[LPC_ORDER + 1];
	float sumP;
	float P[LPC_NG], L[LPC_NG], Pw[LPC_NG], Pr[LPC_NG];
	float R[LPC_NG], M1[LPC_NG], T2[LPC_NG];
	float E1, k1[LPC_ORDER], a1[LPC_ORDER + 1];
	float k2[LPC_ORDER], a2[LPC_ORDER + 1];
};
static LpcWork gLW[NUM_STRINGS];

void lpcSpreadAbort(Voice *v)
{
	gLW[(int)(v - gV)].stage = 0;
}

/* stage A: at the hop */
void lpcAnalyze(Voice *v)
{
	Shifter *sh = &v->sh;
	int s = (int)(v - gV);
	const int p = ordFor(s), W = winFor(s);
	LpcWork &w = gLW[s];
	w.stage = 0;

	if(!tabInit)
		lpcTablesInit();

	float rho = v->beta, phi = v->formant;
	if(rho < 0.25f) rho = 0.25f;
	if(rho > 4.0f) rho = 4.0f;
	if(phi < 0.4f) phi = 0.4f;
	if(phi > 2.4f) phi = 2.4f;
	if(fabsf(phi - rho) < LPC_TOL * rho) {
		sh->lpcValid = 0;
		return;
	}
	{
		float f0 = (sh->Tsm > 8.0f) ? v->srate / sh->Tsm : 0.0f;
		float bw = LPC_BW_FRAC * f0;
		if(bw < LPC_BW_HZ) bw = LPC_BW_HZ;
		if(bw > LPC_BW_MAX) bw = LPC_BW_MAX;
		const float c = 0.5f * powf(2.0f * (float)M_PI * bw * (float)LPC_DEC / 44100.0f, 2.0f);
		for(int i = 0; i <= p; i++)
			lagW[i] = expf(-c * (float)(i * i));
	}
	float wbuf[LPC_WIN + LPC_DECTAPS + 1];
	const int NU = W + (LPC_DEC == 2 ? LPC_DECTAPS - 1 : 0);
	unsigned int wp = v->writePos;
	float prev = v->buf[(wp - NU - 1) & BUF_MASK];
	for(int i = 0; i < NU; i++) {
		float xi = v->buf[(wp - NU + i) & BUF_MASK];
		wbuf[i] = xi - LPC_PREEMPH * prev;
		prev = xi;
	}
#if LPC_DEC == 2
	float d[LPC_WIN / 2];
	const int ND = W / 2, c = LPC_DECTAPS / 2;
	for(int j = 0; j < ND; j++) {
		const float *u = &wbuf[2 * j];
#if LPC_DECWIDE
		float acc = decH[c] * u[c];       /* 2026-11-01 lpc-q: half-band, only odd-distance taps are non-zero */
		for(int q = 0; 2 * q + 1 <= c; q++)
			acc += decO[q] * (u[c - 2 * q - 1] + u[c + 2 * q + 1]);
#else
		float acc = decH[c] * u[c];
		for(int k = 0; k < c; k++)
			acc += decH[k] * (u[k] + u[LPC_DECTAPS - 1 - k]);
#endif
		d[j] = acc;
	}
	float *an = d;
	const int NA = ND;
#else
	float *an = wbuf;
	const int NA = W;
#endif
#if LPC_HANN
	{	/* 2026-11-01 lpc-q: Hann taper */
		const float *ht = hannT[s < 2 ? 0 : 1];
		for(int i = 0; i < NA; i++)
			an[i] *= ht[i];
	}
#endif
	float r[LPC_ORDER + 1];
	for(int lag = 0; lag <= p; lag++) {
		float acc = 0.0f;
		for(int i = lag; i < NA; i++)
			acc += an[i] * an[i - lag];
		r[lag] = acc * lagW[lag];
	}
	if(r[0] < 1e-8f) {
		sh->lpcValid = 0;
		return;
	}
	r[0] *= 1.0f + powf(10.0f, LPC_NOISE_DB * 0.1f);
	float k[LPC_ORDER];
	w.E = levinson(r, p, k, w.a);
	if(w.E <= 0.0f) {
		sh->lpcValid = 0;
		return;
	}
	w.rho = rho; w.phi = phi;
	w.stamp = gSampleStamp;
	w.stage = 1;
	w.cd = HG_LPC_GAP;
}

/* stage B: true-domain envelope on the full-rate grid, the two warped grids */
static void lpcStageB(Voice *v, int s, LpcWork &w)
{
	(void)v;
	const int p = ordFor(s);
	float sumP = 0.0f;
	for(int m = 0; m < LPC_NG; m++) {
#if LPC_DEC == 2 && LPC_HF_TAIL
		if(m > LPC_HF_M) {   /* 2026-11-01 lpc-q: -12 dB/oct tail (see lpcAnalyze) */
			float rr = (float)LPC_HF_M / (float)m;
			w.P[m] = w.P[LPC_HF_M] * (rr * rr) * (rr * rr);
		} else {
#endif
#if LPC_DEC == 2
		int mm = m < LPC_HF_M ? m : LPC_HF_M;
		const float *ct = cosT[2 * mm], *st = sinT[2 * mm];
#else
		const float *ct = cosT[m], *st = sinT[m];
#endif
		float re = 1.0f, im = 0.0f;
		for(int j = 1; j <= p; j++) {
			re += w.a[j] * ct[j];
			im -= w.a[j] * st[j];
		}
		w.P[m] = 1.0f / ((re * re + im * im + 1e-9f) * preM2[m]);
#if LPC_DEC == 2 && LPC_HF_TAIL
		}
#endif
#if LPC_LOGINTERP
		w.L[m] = logf(w.P[m]);
#else
		w.L[m] = 0.0f;
#endif
		sumP += ((m == 0 || m == LPC_NG - 1) ? 0.5f : 1.0f) * w.P[m];
	}
	w.sumP = sumP;
	warpGrid(w.P, w.L, w.rho, sumP, w.Pw);
	warpGrid(w.P, w.L, w.phi, sumP, w.Pr);
}

/* stage C: the ratio and the first all-pole fit */
static int lpcStageC(Voice *v, int s, LpcWork &w)
{
	(void)v;
	const int p = ordFor(s);
	float pmax = 0.0f;
	for(int m = 0; m < LPC_NG; m++)
		if(w.Pw[m] > pmax) pmax = w.Pw[m];
	const float eps = pmax * powf(10.0f, LPC_MASK_DB * 0.1f);
	for(int m = 0; m < LPC_NG; m++) {
		float al = w.Pw[m] / (w.Pw[m] + eps);
		w.R[m] = expf(al * logf(w.Pr[m] / (w.Pw[m] + 1e-30f) + 1e-30f));
	}
	w.E1 = fitGrid(w.R, p, w.k1, w.a1);
	if(w.E1 <= 0.0f)
		return 0;
	for(int m = 0; m < LPC_NG; m++) {
		w.M1[m] = polyMag2(w.a1, p, m);
		w.T2[m] = w.E1 / (w.R[m] * w.M1[m] + 1e-30f);
	}
	return 1;
}

/* stage D: second fit, gain, commit */
static int lpcStageD(Voice *v, int s, LpcWork &w)
{
	Shifter *sh = &v->sh;
	const int p = ordFor(s);
	float E2 = fitGrid(w.T2, p, w.k2, w.a2);
	if(E2 <= 0.0f)
		return 0;
	float num = 0.0f, den = 0.0f;
	for(int m = 0; m < LPC_NG; m++) {
		float wgt = (m == 0 || m == LPC_NG - 1) ? 0.5f : 1.0f;
		float h = polyMag2(w.a2, p, m) / w.M1[m];
		num += wgt * w.Pw[m] * h;
		den += wgt * w.Pw[m];
	}
	float g = sqrtf(den / (num + 1e-30f));
	if(g > LPC_GMAX) g = LPC_GMAX;   /* 2026-11-01 lpc-q: was 8 */
	if(g < 0.125f) g = 0.125f;
	for(int i = 0; i < p; i++) {
		sh->lpcK[i] = w.k2[i];
		sh->lpcKr[i] = w.k1[i];
	}
	for(int i = p; i < LPC_ORDER; i++)
		sh->lpcK[i] = sh->lpcKr[i] = 0.0f;
	sh->lpcGain = g;
	sh->lpcValid = 1;
	sh->lpcFreshStamp = w.stamp;      /* the HOP's stamp: freshness is judged on the window, not the commit */
	return 1;
}

/* called once per sample from lpcProcess */
static inline void lpcSpreadTick(Voice *v, int s)
{
	LpcWork &w = gLW[s];
	if(!w.stage || --w.cd > 0)
		return;
	switch(w.stage) {
	case 1:
		lpcStageB(v, s, w);
		w.stage = 2; w.cd = HG_LPC_GAP;
		break;
	case 2:
		if(!lpcStageC(v, s, w)) { v->sh.lpcValid = 0; w.stage = 0; break; }
		w.stage = 3; w.cd = HG_LPC_GAP;
		break;
	default:
		if(!lpcStageD(v, s, w)) v->sh.lpcValid = 0;
		w.stage = 0;
		break;
	}
}

/* 2026-11-01 lpc-w: worker API stubs for the spread analyser */
volatile unsigned int gTelLpcLate = 0, gTelLpcWorkerUs = 0;
void lpcSubmit(Voice *v) { lpcAnalyze(v); }
void lpcCancel(int) {}
void lpcAdoptAll(void) {}
int  lpcWorkerPoll(void) { return 0; }
void lpcWorkerEnable(int) {}
#endif /* HG_LPC_SPREAD */

/* one sample through the ratio filter: g * B(z)/A1(z), FIR lattice then IIR lattice */
static inline float lattice(Shifter *sh, int p, float x)
{
	/* the six arrays are distinct members of Shifter; telling the compiler so lets it issue the
	 * coefficient/state loads ahead of the serial f/b and y chains (in-order A53). Same
	 * operations in the same order, so the result is bit-identical. */
	float *__restrict zf = sh->lpcZf;
	float *__restrict zg = sh->lpcZg;
	const float *__restrict kw = sh->lpcKW;
	const float *__restrict kr = sh->lpcKrW;
	float f = x, b = x;
	for(int m = 0; m < p; m++) {
		float bd = zf[m];
		zf[m] = b;
		float fN = f + kw[m] * bd;
		b = bd + kw[m] * f;
		f = fN;
	}
	float y = f * sh->lpcGainW;
	for(int m = p - 1; m >= 0; m--) {
		y -= kr[m] * zg[m];
		if(m < p - 1)
			zg[m + 1] = zg[m] + kr[m] * y;
	}
	zg[0] = y;
	return y;
}

float lpcProcess(Voice *v, float x)
{
	Shifter *sh = &v->sh;
	const int s = (int)(v - gV);
	const int p = ordFor(s);
#ifdef HG_LPC_SPREAD
	lpcSpreadTick(v, s);
#endif

	/* onset: keep the running lattices, re-snap on the next post-onset analysis */
#if LPC_ONSET_SNAP
	unsigned int st = gOnsetStamp[s];
	if(st != sh->lpcOnsetSeen) {
		/* 2026-10-01: this used to also do `lpcMix = 0` - a hard drop to
		 * DRY for the 17 ms hold, on EVERY onset stamp. On the plain
		 * strings the onset detector re-fires on notes that are already
		 * ringing, and each re-fire replayed as a level/timbre "restart"
		 * (device capture: 2x level steps every ~300 ms on B/e, none on
		 * the wound G). The running lattices are a perfectly good filter
		 * for the next 17 ms; keep them and only SNAP when the fresh
		 * post-onset analysis exists. */
		sh->lpcOnsetSeen = st;
		sh->lpcSnap = 1;
	}
#endif
	if(!sh->lpcValid)
		sh->lpcSnap = 1;

	/* 2026-11-01 lpc-q H3: idle. dbgGate==2 is the verdict shifter.cpp uses to skip
	 * analysis (the model freezes). After LPC_IDLE_HOPS hops (hop = 128 samples) of it
	 * the mix fades to 0 so a frozen stale model stops colouring pickup noise. */
#if LPC_IDLE
	const int idleLim = LPC_IDLE_HOPS * 128;
	if(v->dbgGate == 2) {
		if(sh->lpcSilentN < idleLim && ++sh->lpcSilentN == idleLim) {
			sh->lpcIdle = 1;
			sh->lpcSnap = 1;
		}
	} else {
		if(sh->lpcSilentN >= idleLim)
			sh->lpcWakeStamp = gSampleStamp;
		sh->lpcSilentN = 0;
	}
	const int asleep = sh->lpcSilentN >= idleLim;
#else
	const int asleep = 0;
#endif

	if(sh->lpcSnap && sh->lpcValid && !asleep) {
		int fresh = 1;
#if LPC_ONSET_SNAP
		fresh = (int)(sh->lpcFreshStamp - sh->lpcOnsetSeen) >= winFor(s) * LPC_HOLD_Q / 8 || (int)(gSampleStamp - sh->lpcOnsetSeen) > 4 * LPC_WIN;
#endif
#if LPC_IDLE
		/* the wake snap must use an analysis made after the string woke, not the frozen one */
		if(sh->lpcIdle && (int)(sh->lpcFreshStamp - sh->lpcWakeStamp) <= 0)
			fresh = 0;
#endif
		if(fresh) {
#if LPC_SNAP_FIX
			if(sh->lpcMix >= 0.5f && !sh->lpcIdle) {
				/* 2026-11-01 lpc-q H5: the lattices are running - crossfade the coefficient set over
				 * LPC_SNAP_RAMP_N samples, states and lpcCorr kept (no state wipe, no re-estimate). */
				const float inv = 1.0f / (float)LPC_SNAP_RAMP_N;
				for(int i = 0; i < p; i++) {
					sh->lpcDK[i] = (sh->lpcK[i] - sh->lpcKW[i]) * inv;
					sh->lpcDKr[i] = (sh->lpcKr[i] - sh->lpcKrW[i]) * inv;
				}
				sh->lpcDG = (sh->lpcGain - sh->lpcGainW) * inv;
				sh->lpcRamp = LPC_SNAP_RAMP_N;
				sh->lpcRampSnap = 1;
				sh->lpcRampStamp = sh->lpcFreshStamp;
				sh->lpcSnap = 0;
			} else
#endif
			{
				/* from bypass/idle: the states are stale, so set instantly, zero and prime */
				for(int i = 0; i < p; i++) {
					sh->lpcKW[i] = sh->lpcK[i];
					sh->lpcKrW[i] = sh->lpcKr[i];
					sh->lpcZf[i] = sh->lpcZg[i] = 0.0f;
				}
				sh->lpcGainW = sh->lpcGain;
				sh->lpcRamp = 0;
				sh->lpcRampSnap = 0;
				sh->lpcRampStamp = sh->lpcFreshStamp;
				float pi2 = 0.0f, po2 = 0.0f;
				for(int i = 0; i < LPC_PRIME; i++) {
					float xi = sh->lpcHist[(sh->lpcHistPos - LPC_PRIME + i) & (LPC_HIST - 1)];
					float yo = lattice(sh, p, xi);
#if LPC_SNAP_FIX
					pi2 += xi * xi; po2 += yo * yo;                          /* full 128 samples */
#else
					if(i >= LPC_PRIME / 2) { pi2 += xi * xi; po2 += yo * yo; }   /* settled half only */
#endif
				}
#if LPC_SNAP_FIX
				sh->lpcPin = pi2 * (1.0f / (float)LPC_PRIME);
				sh->lpcPout = po2 * (1.0f / (float)LPC_PRIME);
				if(!(sh->lpcCorr > 0.0f))
					sh->lpcCorr = 1.0f;       /* first ever snap only; otherwise lpcCorr is kept */
#else
				sh->lpcPin = pi2 * (2.0f / (float)LPC_PRIME);
				sh->lpcPout = po2 * (2.0f / (float)LPC_PRIME);
				sh->lpcCorr = 1.0f;
#endif
				sh->lpcIdle = 0;
				sh->lpcSnap = 0;
			}
		}
	}
	sh->lpcHist[sh->lpcHistPos] = x;
	sh->lpcHistPos = (sh->lpcHistPos + 1) & (LPC_HIST - 1);

	/* bypass crossfade while analysis is invalid / idle / waiting for a wake snap.
	 * A pending snap on a RUNNING string is NOT a reason to go dry. */
	float mixT = (sh->lpcValid && !asleep && !sh->lpcIdle) ? 1.0f : 0.0f;
	sh->lpcMix += (mixT > sh->lpcMix ? LPC_FADE_UP : 0.005f) * (mixT - sh->lpcMix);
	if(sh->lpcMix < 0.001f)
		return x;

#if LPC_RAMP
	/* 2026-11-01 lpc-q H1: per-sample linear ramp. A new analysis (new lpcFreshStamp) starts a
	 * LPC_RAMP_N-sample ramp from where the coefficients are to the new targets; every sample
	 * adds a fixed delta (2p+1 adds). A snap crossfade ramp (above) holds hop ramps off until
	 * it ends, then the next sample restarts a hop ramp toward the current targets. */
	if(!sh->lpcRampSnap && sh->lpcFreshStamp != sh->lpcRampStamp && sh->lpcValid) {
		const unsigned int fs = sh->lpcFreshStamp;
		/* ramp length = distance to the previous analysis (128 at the old cadence, 384 staggered),
		 * so the coefficients arrive just as the next target does; one divide per ANALYSIS, none per sample */
		int rn = (int)(fs - sh->lpcRampStamp);
		if(rn < LPC_RAMP_N) rn = LPC_RAMP_N;
		if(rn > LPC_RAMP_MAX) rn = LPC_RAMP_MAX;
		const float inv = 1.0f / (float)rn;
		for(int i = 0; i < p; i++) {
			sh->lpcDK[i] = (sh->lpcK[i] - sh->lpcKW[i]) * inv;
			sh->lpcDKr[i] = (sh->lpcKr[i] - sh->lpcKrW[i]) * inv;
		}
		sh->lpcDG = (sh->lpcGain - sh->lpcGainW) * inv;
		sh->lpcRamp = rn;
		sh->lpcRampStamp = fs;
	}
	if(sh->lpcRamp > 0) {
		float *__restrict kwp = sh->lpcKW, *__restrict krp = sh->lpcKrW;
		const float *__restrict dk = sh->lpcDK, *__restrict dkr = sh->lpcDKr;
		for(int i = 0; i < p; i++) {
			kwp[i] += dk[i];
			krp[i] += dkr[i];
		}
		sh->lpcGainW += sh->lpcDG;
		if(--sh->lpcRamp == 0 && sh->lpcRampSnap) {
			sh->lpcRampSnap = 0;
			sh->lpcRampStamp = sh->lpcFreshStamp - 1;   /* force a hop ramp toward the current targets */
		}
	}
#else
	/* slew every 4th sample at 4x the rate (same time constant, a quarter of the MACs) */
	if(!(gSampleStamp & 3)) {
		float *__restrict kwp = sh->lpcKW, *__restrict krp = sh->lpcKrW;
		const float *__restrict kt = sh->lpcK, *__restrict krt = sh->lpcKr;
		for(int i = 0; i < p; i++) {
			kwp[i] += 4.0f * LPC_SLEW * (kt[i] - kwp[i]);
			krp[i] += 4.0f * LPC_SLEW * (krt[i] - krp[i]);
		}
		sh->lpcGainW += 4.0f * LPC_SLEW * (sh->lpcGain - sh->lpcGainW);
	}
#endif

	float y = lattice(sh, p, x);

#if LPC_AGC
	/* SLOW level trim. The model gain preserves power on the smooth
	 * envelope, but a sparse line spectrum samples that envelope at a few
	 * partials, so the realised level can differ by a few dB (worst when
	 * the pitch is up an octave). Track in/out power over ~23 ms and nudge
	 * the output so the realised level matches; slow enough not to touch
	 * attacks (those leave through the model gain), measured pre-trim so
	 * there is no feedback loop. 2026-11-01 lpc-q: clamp 0.7..1.4 (was
	 * 0.4..2.5 = +8 dB, which lifted the noise floor) and held while the
	 * input power is under LPC_AGC_FLOOR. */
	sh->lpcPin += LPC_AGC_A * (x * x - sh->lpcPin);
	sh->lpcPout += LPC_AGC_A * (y * y - sh->lpcPout);
	if(!(gSampleStamp & 7) && sh->lpcPin > LPC_AGC_FLOOR) {
		float tgt = sqrtf((sh->lpcPin + 1e-9f) / (sh->lpcPout + 1e-9f));
		if(tgt > LPC_AGC_HI) tgt = LPC_AGC_HI;
		if(tgt < LPC_AGC_LO) tgt = LPC_AGC_LO;
		sh->lpcCorr += 0.05f * (tgt - sh->lpcCorr);
	}
	if(!(sh->lpcCorr > 0.0f))
		sh->lpcCorr = 1.0f;      /* never-snapped string */
	y *= sh->lpcCorr;
#endif

	if(!(fabsf(y) < 16.0f)) {
		for(int m = 0; m < LPC_ORDER; m++)
			sh->lpcZf[m] = sh->lpcZg[m] = 0.0f;
		y = 0.0f;
	}
#if LPC_KNEE
	else {
		/* 2026-11-01 lpc-q H6: smooth knee, slope 1 at the knee start, asymptote 1.0 (the scrub runs first
		 * so inf/NaN still wipe the states).
		 * 2026-11-01 boost: knee start 0.9 -> 0.98, range 0.1 -> 0.02. TIMBRE went quiet on hard playing;
		 * the downstream output limiter (render.cpp gLimEnv) is the real safety, this only stops gross overs. */
		float ay = fabsf(y);
		if(ay > 0.98f) {
			float kn = 0.98f + 0.02f * tanhf((ay - 0.98f) / 0.02f);
			y = y < 0.0f ? -kn : kn;
		}
	}
#endif

	return x * (1.0f - sh->lpcMix) + y * sh->lpcMix;
}
