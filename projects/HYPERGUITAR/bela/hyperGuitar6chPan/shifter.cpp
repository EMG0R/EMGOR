/*
 * shifter.cpp - pitch+formant engine, one instance per string.
 *
 * ARCHITECTURE (v3, resampler both ways): ONE SOLAD drifting-delay
 * resampler does ALL the pitch shifting. A resampler is a true frequency
 * scaler - every partial moves by exactly rho between splices, so it can
 * never chirp, never beads, and it works on perfectly periodic content
 * (mark-repositioned COLA grains mathematically CANNOT down-shift periodic
 * content: moving grains by whole periods of a periodic signal is an
 * identity - that was the "laser"/unshifted failure of v1; and in the up
 * direction, grains half a period apart turned inharmonic partial drift
 * into 25-30% slow partial-level AM, "beading", on strings 4-5).
 *
 *   rho < 1 (down): the read delay grows at (1-rho)/sample and jumps back
 *     one period at SD_CEIL. CLEAR: no overlap-add of decorrelated grains.
 *   rho > 1 (up): the read delay falls at (rho-1)/sample and jumps back N
 *     whole periods (N set by a latency budget) at a floor that leaves room
 *     for the crossfade; offset refined by correlation. SOFT: one smooth
 *     equal-power splice per N periods instead of two grains per period.
 *
 * FORMANT keeps the mark-synchronous COLA grain stream (window = 2x launch
 * spacing, 50% overlap, sums to a constant: zero grain-rate AM - measured
 * 0.0dB), in series on the resampler output, mark-based and COLA-exact.
 *
 * Attacks: pitch DOWN re-anchors the resampler to the write head at the
 * pick (the attack lands at shifted pitch, ~4 samples of delay). Pitch UP
 * (you cannot read the future) and a live formant leg pass the attack
 * through a dry tap in SH_MODE_TRANS for a few hops.
 *
 * THE ANTI-LASER RULE: the scheduler period IS the tracker period,
 * updated at hop boundaries only. The tracker already snaps on confirmed
 * note changes and glides only on real bends - any extra smoothing layer
 * on top turns every note change into an audible pitch sweep.
 */
#include "hyperguitar.h"
#include "hg_mc.h"
#include <atomic>
#include <cmath>
#include <cstring>

/* ---- 2026-11-01 tier-A: lock-free diag ring + telemetry counters ---- */
#define SHDIAG_N 64
static ShDiagEv gShDiagQ[SHDIAG_N];
static std::atomic<unsigned int> gShDiagW(0), gShDiagR(0);
void shDiagPush(const ShDiagEv *e)
{
	unsigned int w = gShDiagW.load(std::memory_order_relaxed);
	unsigned int r = gShDiagR.load(std::memory_order_acquire);
	if(w - r >= SHDIAG_N)
		return;                       /* full: drop, never block the audio thread */
	gShDiagQ[w % SHDIAG_N] = *e;
	gShDiagW.store(w + 1, std::memory_order_release);
}
int shDiagPop(ShDiagEv *e)
{
	unsigned int r = gShDiagR.load(std::memory_order_relaxed);
	if(r == gShDiagW.load(std::memory_order_acquire))
		return 0;
	*e = gShDiagQ[r % SHDIAG_N];
	gShDiagR.store(r + 1, std::memory_order_release);
	return 1;
}
volatile unsigned int gTelTrans[NUM_STRINGS];
volatile unsigned int gTelFxg[NUM_STRINGS];
volatile unsigned int gTelLoopCap;
volatile unsigned int gTelLpcMaxTk;

#ifdef HG_PROBE
/* host-only event probe: correlate shifter events with output clicks.
 * Never compiled on the device. */
#include <cstdio>
#include <cstdlib>
static FILE *hgProbeF(void)
{
	static FILE *f = NULL;
	static int tried = 0;
	if(!tried) {
		tried = 1;
		const char *p = getenv("HG_PROBE_FILE");
		if(p) f = fopen(p, "w");
	}
	return f;
}
#define HG_EV(...) do { FILE *_f = hgProbeF(); \
	if(_f) fprintf(_f, __VA_ARGS__); } while(0)
static int gPrStr = -1;
#define HG_SETSTR(s) (gPrStr = (s))
/* per-string per-sample stage taps: HG_TAP_PREFIX.sN.f32, frames of
 * [p, chain, tap, out] float32 */
static FILE *hgTapF(int s)
{
	static FILE *f[NUM_STRINGS];
	static int tried = 0;
	if(!tried) {
		tried = 1;
		const char *p = getenv("HG_TAP_PREFIX");
		if(p) for(int i = 0; i < NUM_STRINGS; i++) {
			char nm[256];
			snprintf(nm, sizeof(nm), "%s.s%d.f32", p, i);
			f[i] = fopen(nm, "wb");
		}
	}
	return f[s];
}
static void hgTap(int s, float p, float chain, float tap, float out)
{
	FILE *f = hgTapF(s);
	if(f) { float v[4] = { p, chain, tap, out }; fwrite(v, 4, 4, f); }
}
#else
#define HG_EV(...) do { } while(0)
#define HG_SETSTR(s) do { } while(0)
#endif


#define SH_MODE_RUN    0
#define SH_MODE_TRANS  1


/*
 * THE SPLICE BURST.
 *
 * The tap splicer's correlation search was the last single-sample CPU
 * spike in the engine. Measured with a host op-count profiler (6 voices,
 * realistic pluck + palm-muted scratch, which is what actually drops the
 * engine into SH_MODE_DRIFT where the splicer lives):
 *
 *   a normal sample                  12 bufAtM calls (all six strings)
 *   a sample where a splice fires  2028 bufAtM mean, 5103 worst
 *                                  (two strings splicing in one slot)
 *
 * i.e. 170-425x a normal sample, landing in ONE sample slot, roughly once
 * per period per drifting string. That is invisible on clean locked
 * material - the splicer never fires while the tracker holds lock, which
 * is why profiling a sustained note shows nothing at all - and it shows up
 * on the device as rare block overruns that get worse as the block size
 * shrinks, because the burst is divided over fewer samples.
 *
 * The cause was pure redundancy: nine offsets x win/2 positions x TWO
 * cubic interpolations, with the same reads repeated for every offset.
 * The search below now performs the identical arithmetic on shared reads
 * (see the comment at the loop), which costs ~460 interpolations instead
 * of ~2700 and chooses a bit-identical offset.
 *
 * Scratch for that: file-static, sized for the largest window the search
 * can ask for. It is live only inside the search, which runs to
 * completion inside one call on one audio thread, so the six voices share
 * it safely. No allocation, no locks.
 */
#define SPL_MAXWIN 300
static HG_TLS float gSplA[(SPL_MAXWIN >> 1) + 1];   /* per-thread scratch (MC) */
static HG_TLS float gSplB[SPL_MAXWIN + 9];

/* ------------------------- mark picker ------------------------- */

static void pickerSetF0(MarkPicker *m, float f0, float sr)
{
	float fc = 1.5f * f0;
	if(fc > 0.45f * sr) fc = 0.45f * sr;
	if(fc < 40.0f) fc = 40.0f;
	float w0 = 2.0f * (float)M_PI * fc / sr;
	float cw = cosf(w0), al = sinf(w0) * 0.7071f;
	float a0i = 1.0f / (1.0f + al);
	m->b1 = (1.0f - cw) * a0i;
	m->b0 = m->b2 = 0.5f * m->b1;
	m->a1 = -2.0f * cw * a0i;
	m->a2 = (1.0f - al) * a0i;
}

static void pickerReset(MarkPicker *m, float T)
{
	m->haveMark = 0;
	m->pkVal = 0.0f;
	m->searchIn = 0.0f;
	m->searchLeft = 1.5f * T;
}

/* per-sample: filter, track the once-per-period peak, commit marks.
 * T = expected period (sets the predicted search window). */
static void pickerRun(MarkPicker *m, float x, float T)
{
	float y = m->b0 * x + m->z1;
	m->z1 = m->b1 * x - m->a1 * y + m->z2;
	m->z2 = m->b2 * x - m->a2 * y;

	/* slow level tracker for the RELATIVE mark gate below. The old gate
	 * was `pkVal > 1e-4` ABSOLUTE - calibrated on loud host-bench plucks
	 * (amp 0.1-0.3) and ~10x above where a real string's decay tail
	 * lives (device env 0.01-0.1). On the instrument the picker starved
	 * mid-note on the top two strings, went stale, and the mode machine
	 * flapped GRAINS<->DRIFT at up to ~50Hz gating the formant wet/dry:
	 * the "buffer repeating" texture (device trace 2026-09-01: 304 mode
	 * flips on the high C in minutes of noodling, conf 0.00, all at
	 * below-G periods). Relative to the running level, a quiet-but-
	 * periodic tail keeps its marks; the 1e-7 floor only rejects
	 * denormal-land noise when the string is actually silent. */
	float ay = fabsf(y);
	m->lvl += (ay > m->lvl ? 0.005f : 0.0005f) * (ay - m->lvl);

	m->markDelay += 1.0f;
	/* 2026-11-01 tier-A: silence must not grow it without bound (4 x ~1024-sample max period) */
	if(m->markDelay > 4096.0f) m->markDelay = 4096.0f;
	m->pkDelay += 1.0f;
	if(m->searchIn > 0.0f) {
		m->searchIn -= 1.0f;
	} else if(m->searchLeft > 0.0f) {
		m->searchLeft -= 1.0f;
		if(y > m->pkVal) { m->pkVal = y; m->pkDelay = 0.0f; }
		if(m->searchLeft <= 0.0f) {
			float gate = 0.15f * m->lvl;
			if(gate < 1e-7f) gate = 1e-7f;
			if(m->pkVal > gate) {
				m->markDelay = m->pkDelay;
				if(m->haveMark < 2) m->haveMark++;
				float in = 0.75f * T - m->markDelay;
				m->searchIn = in > 0.0f ? in : 0.0f;
				m->searchLeft = 0.5f * T;
			} else {
				m->searchIn = 0.0f;
				m->searchLeft = 0.5f * T;
			}
			m->pkVal = 0.0f;
		}
	}
}

/* ------------------------- grain leg ------------------------- */

/*
 * THE RECORDED STEP (the stacked pitch+formant click).
 *
 * legReset used to deactivate every grain instantly. At a mode re-entry
 * that is NOT silent: the mark picker re-acquires within ~2T of an onset,
 * so the up leg is launching again DURING TRANS and its grains are
 * mid-window when the TRANS->GRAINS reset lands. Killing them steps the
 * pitch-stage output p from full grain amplitude to ~0 in ONE sample.
 * grainMix ~ 0 masks that step at the OUTPUT - but p is also WRITTEN
 * INTO fxbuf, and the formant leg reads fxbuf dc (~150-650) samples
 * later at full gain: every re-lock while shifting up + formant became
 * a delayed, un-crossfaded click with no event anywhere near it.
 * (Host probe, +12 & formant 1.35: the biggest step in p sits exactly ON
 * the M 1->0 transition, and every worst click follows it by exactly the
 * formant leg's read delay on that string.)
 *
 * So: a reset re-anchors the LAUNCH state only. Grains already in flight
 * finish their Hann windows (they are gone within 2S) and overlap-add
 * smoothly with the newly anchored stream - a brief bounded COLA
 * overshoot instead of a recorded discontinuity.
 */
static void legReset(GrainLeg *L, float srcDelay)
{
	L->launchCd = 0.0f;
	L->srcDelay = srcDelay;
}

/*
 * SOURCE CONTINUITY (stab) - why the formant leg does not snap to the mark.
 *
 * The mark picker takes the argmax of a lowpassed copy over a predicted
 * half-period window. That is an energy peak, not an excitation instant, so
 * on a real plucked string - where the partials decay at different rates and
 * dephase - the argmax OSCILLATES between two candidate peaks a quarter
 * period apart, launch after launch. Instrumented on the low E it reads as an
 * alternating source displacement:
 *
 *   dev = +0.240T, -0.240T, +0.190T, -0.170T, +0.249T, -0.251T, +0.172T, ...
 *
 * The formant leg has S == T, so srcDelay == the previous dc: a perfectly
 * periodic mark stream would leave dc CONSTANT and the leg would be a clean
 * COLA stream. Instead 8-10% of launches were spliced a fifth to a third of a
 * period away from where the previous grain says they belong - and unlike a
 * whole-period drop that displacement is NOT period-aligned, so it is a raw
 * phase discontinuity inside the 2-period Hann overlap. It is 15x more
 * frequent than the period drops the skew ceiling worries about, and it is
 * the roughness that was left.
 *
 * So the leg tracks the mark lattice through a one-pole instead of snapping
 * to it: dc moves a quarter of the way from continuation to the mark target
 * each launch. Zero-mean picker chatter is attenuated ~4x while a REAL epoch
 * move (a bend, a new note) is still followed with no steady-state error,
 * geometrically, inside a handful of periods - and legReset() anchors
 * srcDelay onto the mark, so every mode entry starts exact and the lag never
 * has to catch up from cold. The cost is that grains sit a mean 0.07T off the
 * picked mark, which inside a window 2T wide is nothing; the periodicity
 * measurement below is the proof that the trade is the right way round.
 *
 * Formant leg only (stab=1). The pitch-up leg runs S != T, where the source
 * MUST advance one mark per launch to shift at all, so it keeps snapping.
 *
 * ROUGHNESS, measured on realistic material only (decaying, vibrato'd,
 * INHARMONIC plucks with bends, a re-pick, a legato note change and a
 * palm-muted aperiodic section; clean saws show 0% in every configuration
 * and have hidden every bug in this engine). "residual" is 1 - max
 * period-to-period normalised correlation of the OUTPUT: formant shifting
 * does not change the period, so a transparent leg must leave the output as
 * self-similar as the input, and every mis-spliced grain breaks that. The
 * formant-OFF control measures 20.26% / 25.23% / 14.92%, i.e. the floor.
 *
 *                   low E                D               high e
 *   step >0.2T   8.22% -> 0.00%    6.47% -> 0.00%    9.61% -> 0.00%
 *   mean step   0.0348T->0.0157T  0.0338T->0.0155T  0.0388T->0.0161T
 *   period drop  1.59% -> 0.27%    1.03% -> 0.00%    0.53% -> 0.00%
 *   residual    21.17% -> 20.90%  20.39% -> 20.02%  15.03% -> 14.05%
 *   mean delay    509 -> 292        296 -> 190        144 -> 98
 *
 * i.e. every metric improves at once, the residual lands on the formant-OFF
 * floor, and the leg gets 4.9ms (low E) CHEAPER, not dearer. Verified across
 * five independent material sets (two seeds open, plus frets 5, 7 and 12):
 * the coefficient is monotone down to ~0.15 and starts to regress by 0.08,
 * so 0.25 sits clear of the over-lag knee.
 */
#define STAB_C 0.25f

static void legLaunch(GrainLeg *L, MarkPicker *mk, float S, float phi, float T,
		int stab)
{
	/*
	 * SOURCE MARK BOOKKEEPING - the PSOLA contract: the source advances
	 * exactly ONE pitch mark per launch; time skew is absorbed by
	 * whole-period drops/repeats, which splice period-aligned.
	 *
	 * NO `if(k < 0) k = 0` HERE - IT WAS A LATCHING LATENCY RATCHET.
	 *
	 * k is the whole number of periods between this leg's continuation
	 * target and the newest mark. It goes negative exactly when the picker
	 * DROPS a mark: markDelay then grows past a full period, so the newest
	 * mark is older than where this grain belongs and the honest answer is
	 * the mark one or two periods NEWER, k = -1, -2. Clamping k to 0 said
	 * "use the old mark" instead - whole periods of gratuitous delay - and
	 * because the formant leg's skew is neutrally stable (S == T, so
	 * srcDelay == dc) that delay never came back. It ratcheted up for the
	 * rest of the note and pinned dc against the skew ceiling, which is
	 * where the ceiling's own period-drop fires.
	 *
	 * It was also redundant, which is why removing it is safe: causality is
	 * enforced UNCONDITIONALLY by the `while(dc < need)` floor below, which
	 * lifts any dc - however negative - onto the first lattice point at or
	 * above need. A clamp on k cannot add anything the floor does not
	 * already guarantee. Bounded, too: dc lands within T/2 of srcDelay, and
	 * the previous launch's own floor and ceiling confine srcDelay to
	 * [need - T + S, ceil2 - T + S], so the floor can iterate at most a
	 * couple of times - measured max 2 across every scenario, both legs.
	 *
	 * Measured (same material as SOURCE CONTINUITY above):
	 *
	 *                    period drops             mean source delay
	 *                 low E     D   high e     low E     D   high e
	 *   with clamp    1.59%  1.03%  0.53%       509    296    144
	 *   without       0.53%  0.00%  0.00%       313    274    105
	 *
	 * and on the pitch-up leg, which shares this function, +12st goes
	 * 1.07% -> 0.13% drops with the mean delay 438 -> 292 samples and the
	 * output discontinuity counts bit-identical (3/32/70 either way). The
	 * ~50% "lattice jump" rate there is not an artifact: at beta=2 launches
	 * are T/2 apart and the source advances a whole mark each time, so
	 * every other launch skips a period BY DESIGN.
	 */
	float k = roundf((L->srcDelay - mk->markDelay) / T);
	float dc = mk->markDelay + k * T;
	if(stab)
		dc = L->srcDelay + STAB_C * (dc - L->srcDelay);

	/* causality: grain reads source [mark - S*phi, mark + S*phi]; the end
	 * must be written by the time it's read -> dc >= 3 + S*(phi-2) */
	float need = 3.0f + (phi > 2.0f ? S * (phi - 2.0f) : 0.0f);
	/* 2026-11-01 tier-A: bounded to 8 iterations; on cap, re-anchor below */
	int capHit = 0;
	{
		int it = 0;
		while(dc < need) {
			if(++it > 8) { capHit = 1; break; }
			dc += T;
		}
	}
	/*
	 * SKEW CEILING: drop whole periods when the source falls behind.
	 *
	 * DO NOT TIGHTEN THIS. It was once narrowed to need + T + 32 to buy
	 * latency, on the theory that a whole-period drop is period-aligned by
	 * construction and therefore inaudible. It is not. dc lives on a
	 * lattice of spacing T, and mark jitter on a real string is a large
	 * fraction of T - so a ceiling that sits one period above the floor
	 * puts dc permanently ON the boundary, and the drop fires on a third of
	 * all launches. Measured on a decaying, vibrato'd, inharmonic pluck
	 * (host A/B probe, formant leg instrumented):
	 *
	 *   ceiling          low E      D      high e
	 *   2.5*T+128+need    0.6%    0.0%     0.3%   of launches drop a period
	 *   need+T+32        35.7%   18.1%     5.1%
	 *
	 * A drop is only an identity for a perfectly periodic signal. A real
	 * string decays, bends and is inharmonic, so every drop repeats or
	 * skips a period of a CHANGING waveform inside a 2-period Hann grain:
	 * at a third of all grains that is continuous roughness, which is what
	 * "sounds like total dog shit" was. The wide ceiling is what keeps the
	 * formant leg a clean COLA stream; it costs ~7ms on the low E and is
	 * worth every sample of it.
	 */
	float ceil2 = 2.5f * T + 128.0f + need;
	{
		int it = 0;
		while(dc > ceil2 && dc - T >= need) {
			if(++it > 8) { capHit = 1; break; }
			dc -= T;
		}
	}

	float d0 = dc + S * phi;
	if(capHit) {
		gTelLoopCap++;
	}
	if(capHit || d0 > (float)(FXBUF_MASK - 16)) {
		HG_EV("A %u %d %d reanchor\n", gSampleStamp, gPrStr, stab);
		L->srcDelay = mk->markDelay;     /* re-anchor and skip one launch */
		L->launchCd += S;
		return;
	}

	ShGrain *g = &L->gr[0];
	float best = -1.0f;
	for(int i = 0; i < SH_NGRAINS; i++) {
		if(!L->gr[i].active) { g = &L->gr[i]; best = 2.0f; break; }
		float prog = L->gr[i].u / L->gr[i].L;
		if(prog > best) { best = prog; g = &L->gr[i]; }
	}
	HG_EV("L %u %d %d dj %.2f T %.1f S %.1f steal %.2f\n", gSampleStamp,
			gPrStr, stab, dc - L->srcDelay, T, S,
			best < 1.0f ? best : -1.0f);
	g->d0 = d0;
	g->u = 0.0f;
	g->L = 2.0f * S;
	g->phi = phi;
	g->active = 1;

	/* advance one period per launch, pre-mapped to next-launch time
	 * coordinates (all delays grow by S between launches) */
	L->srcDelay = dc - T + S;
	L->launchCd += S;
}

static float legRun(GrainLeg *L, MarkPicker *mk, const float *buf,
		unsigned int mask, unsigned int wr, float S, float phi, float T,
		int stab)
{
	L->srcDelay += 0.0f; /* srcDelay is in launch-time coords; see legLaunch */
	if(mk->haveMark >= 2 && (L->launchCd -= 1.0f) <= 0.0f)
		legLaunch(L, mk, S, phi, T, stab);

	float out = 0.0f;
	for(int i = 0; i < SH_NGRAINS; i++) {
		ShGrain *g = &L->gr[i];
		if(!g->active)
			continue;
		float t = g->u / g->L;
		float w = shHann(t);
		out += w * bufAtM(buf, mask, wr, g->d0 + g->u * (1.0f - g->phi));
		g->u += 1.0f;
		if(g->u >= g->L)
			g->active = 0;
	}
	return out;
}

/*
 * SOLAD DELAY BUDGET - this single number IS the down-shift latency.
 *
 * The read delay ramps up at (1-rho) samples/sample and jumps back exactly
 * one period when it crosses the ceiling, so the delay saws between
 * (ceiling - T) and ceiling. It used to be keyed to p0, the OPEN string
 * period, which made every fretted note pay the open string's budget: the
 * low E at the 12th fret sat at 542..860 samples for a budget it needed
 * 96..414 of. Keying it to the LIVE period gives that back.
 *
 * The splice RATE does not change: a splice retreats exactly one period
 * and the drift rate is fixed by the ratio alone, so splices land every
 * T/(1-rho) samples no matter where the ceiling sits. All the ceiling buys
 * is headroom, and headroom is only consumed by period ERROR between hop
 * updates (this is recomputed every hop, from the same v->period the
 * splice itself uses, so the two can never disagree WITHIN a hop):
 *   0.15*T  the tracker's own bend glide plus real bending inside one
 *           128-sample hop moves the period a few percent; 15% is several
 *           times the worst case and scales with the note, as the error
 *           does.
 *   +32     fixed floor, covering what does NOT scale: the cubic
 *           interpolator's 2-sample look-behind, and the post-splice
 *           crossfade which reads the OLD (larger) delay for 0.5*T while
 *           the new one drifts up from here. The old +128 was ~3ms of
 *           guesswork on top of a budget that was already 15% generous.
 */
#define SD_CEIL(T) (1.15f * (T) + 32.0f)

/*
 * SOLAD-UP BUDGET (the pitch-UP leg is now a resampler, not a grain stream).
 *
 * Measured diagnosis (docs/pitch-tracker-overhaul.md s3): the old up leg
 * overlap-added mark-aligned grains half a period apart. On an inharmonic
 * plain string every repeated/skipped period steps the phase of the upper
 * partials by f0*B*k^3/2 Hz-worth, and the 50%-overlap sum turns that into
 * slow partial-level AM: 25-30% "beading" on strings 4-5 at +12, with an
 * ORACLE period (so it is not the tracker). A resampler has no such
 * mechanism: between splices it is a true frequency scaler (every partial
 * moves by exactly rho), and a splice only happens when the read delay
 * hits a floor - once per N periods, not twice per period.
 *
 * Geometry. For rho > 1 the read delay FALLS by (rho-1) per sample. At the
 * floor `sdLo` the read jumps BACK by N whole periods (delay += N*T), the
 * offset refined +-4 samples by normalised correlation (corrRefine, the
 * DRIFT splicer's shared-read search). The delay therefore saws between sdLo
 * and sdLo + N*T:
 *
 *   sdLo = 5 + (rho-1) * L      L = crossfade length in samples
 *
 * The 5 is the cubic interpolator's look-behind plus a spare sample; the
 * (rho-1)*L is the room the OUTGOING read needs to finish its crossfade
 * while still drifting toward the write head (this is exactly the stale-
 * ring click the old tap splicer had - 245 clusters at +12 - so the floor
 * leaves room for the whole fade, and the old-read clamp is only a net).
 *
 * N adapts per string to a latency budget: N = clamp(round(budget/T),1,4).
 * Low strings (T ~ 400-700) use N = 1; T ~ 169 uses N = 2; the high fretted
 * range uses 3-4. Mean read delay ~ sdLo + N*T/2, i.e. ~5-8 ms.
 *
 * Crossfade: equal-power on a smoothstep phase (no step at the seam, same
 * shape as the tap splicer), length SUP_XF OUTPUT periods = SUP_XF*T/rho.
 * 1.0 is a half-period at +12; raising it toward 2.0 lengthens the overlap
 * toward 100% duty at +12 (softer, more smear). Keep it <= 2: beyond that
 * the fade outlasts the interval between splices at rho = 2, N = 1.
 *
 * A note's own history only: the ring before an onset is the PREVIOUS
 * note (or silence). A splice never jumps back past the onset stamp - it
 * takes fewer periods, or holds the read at the floor (unshifted) until
 * the note has enough history. See `maxD` in resRun.
 */
#ifndef ONSET_XF_T
#define ONSET_XF_T 0.20f    /* re-anchor crossfade as a fraction of the period... */
#endif
#define ONSET_XF 32.0f      /* ...floored here (0.7 ms) and capped at ONSET_XF_MAX */
#define ONSET_XF_MAX 80.0f
#ifndef SUP_BUDGET
#define SUP_BUDGET 350.0f   /* samples, ~8 ms at 44.1 kHz: ceiling N*T */
#endif
#define SUP_NMAX 4
#ifndef SUP_XF
#define SUP_XF 1.0f         /* crossfade length, in OUTPUT periods (<= 2) */
#endif
#ifndef SUP_XF_MAX
#define SUP_XF_MAX 240.0f   /* crossfade cap, samples (~5.4 ms): keeps the low strings' floor, hence latency, down */
#endif
#define SUP_ONSET_GUARD 6.0f /* samples kept between a jump target and the onset */

/*
 * CORRELATION SPLICE REFINEMENT (shared by the up leg and the unlocked down
 * splice). Compares the window starting at the CURRENT read delay `curD`
 * against nine windows around `base` and returns the best offset in
 * [-4, 4]. Cost ~460 interpolations (see THE SPLICE BURST).
 *
 * SHARED READS - see THE SPLICE BURST above. Two facts, both exact:
 *   1. the curD+i window does not depend on o, so it (and its energy e1)
 *      is interpolated ONCE, not nine times. Same values, same
 *      accumulation order.
 *   2. the nine base+o windows are nine views of ONE contiguous run of
 *      positions: base+o+i over o in [-4,4] and even i is exactly base-4 ..
 *      base+win+3, and offset o's window is that run read from index o+4.
 *      Sweeping the run once feeds all nine.
 * (2) is exact only up to float rounding of c0+k vs base+o+i (<= 1 ulp,
 * ~1e-4 sample), which cannot change a correlation argmax in practice. The
 * old code guarded that with a bit-exactness test and fell back to the
 * nine-window loop (2700 interpolations in ONE sample) when base+-4 crossed
 * a binade. That guard is gone: the up leg's base is NEARLY CONSTANT per
 * string (sdLo + N*T), so a string whose base sat in 508..512 took the slow
 * path on every splice - a 2700-read burst every ~750 samples on the host
 * bench - where the old DRIFT splicer's random-walking base hit it rarely.
 */
static float corrRefine(const float *buf, unsigned int mask, unsigned int wr,
		float curD, float base, int win)
{
	float bestv = -2.0f, besto = 0.0f;
	if(win > SPL_MAXWIN) win = SPL_MAXWIN;
	if(win < 8) win = 8;
	float c0 = base - 4.0f;
	int nk = ((win - 1) & ~1) + 9;   /* k = 0 .. maxi+8 */
	float e1 = 0.0f;
	for(int k = 0; k < nk; k++)
		gSplB[k] = bufAtM(buf, mask, wr, c0 + (float)k);
	for(int i = 0; i < win; i += 2) {
		float sa = bufAtM(buf, mask, wr, curD + (float)i);
		gSplA[i >> 1] = sa;
		e1 += sa * sa;
	}
	for(int o = -4; o <= 4; o++) {
		const float *sbp = &gSplB[o + 4];
		float acf = 0.0f, e2 = 0.0f;
		for(int i = 0; i < win; i += 2) {
			float sa = gSplA[i >> 1];
			float sb = sbp[i];
			acf += sa * sb;
			e2 += sb * sb;
		}
		float val = acf / sqrtf(e1 * e2 + 1e-12f);
		if(val > bestv) { bestv = val; besto = (float)o; }
	}
	return besto;
}

/* up-splice crossfade length for period Tr at ratio rho (floor 16) */
static inline float supXfLen(float Tr, float rho)
{
	float L = SUP_XF * Tr / rho;
	if(L > SUP_XF_MAX) L = SUP_XF_MAX;
	return L < 16.0f ? 16.0f : L;
}

/* start a crossfade from the current read to newD, len samples long */
static inline void resFade(Shifter *sh, float newD, float len, int eq)
{
	sh->sdOldDelay = sh->sdDelay;
	sh->sdDelay = newD;
	sh->sdFading = 1;
	sh->sdFadePh = 0.0f;
	sh->sdFadeStep = 1.0f / len;
	sh->sdEq = eq;
}

/* -------- the resampler: ONE drifting-delay engine for both directions.
 * The drift RATE is 1-rho, so the shift amount can never glide or chirp -
 * the period estimate only sets the jump size/phase. rho < 1 (down): the
 * delay grows and jumps back one period at the ceiling. rho > 1 (up): the
 * delay shrinks and jumps back N periods at the floor (SOLAD-UP BUDGET). -------- */

/*
 * PARKING IS A TELEPORT - only do it while the leg is silent.
 *
 * When the shift ratio returns to unity this used to slam sdDelay to
 * 0.5*sdHiThresh unconditionally. That is an uncrossfaded jump of the
 * read pointer - measured at +97 and +53 samples on a slot-pad voicing
 * change - and it lands while downAmt is still ~0.75, i.e. at nearly
 * full leg gain. The host discontinuity probe flags it at 26-28x the
 * local first-difference distribution: it is a click, not a seam.
 *
 * The park is only ever there to give the NEXT shift a sane starting
 * delay, so it costs nothing to wait until the leg has actually faded out
 * of the mix. `mayPark` is that permission, passed from the one call site
 * that knows the leg gain. While the leg is still audible at unity ratio
 * the delay simply FREEZES where it is - a fixed-delay read of the input,
 * perfectly continuous - and any crossfade already in flight is allowed to
 * finish rather than being abandoned mid-fade (abandoning it was a second,
 * smaller step in the same sample).
 */
static inline float resRun(Shifter *sh, Voice *v, int s, float rho, int mayPark)
{
	float drift = 1.0f - rho;
	if(fabsf(drift) < 0.01f) {
		if(mayPark) {
			sh->sdDelay = 0.5f * sh->sdHiThresh;
			sh->sdFading = 0;
			sh->sdOnsLive = 0;
			return 0.0f;     /* idle: nothing reads the leg */
		}
		drift = 0.0f;        /* hold the read; let a live fade run out */
	} else if(drift > 0.0f) {
		/* ---- DOWN: delay grows, jump back one period at the ceiling.
		 * Until the tracker has locked this note (onset re-anchored the
		 * delay to 4, so the first splice is ~2T away) v->period is still
		 * the PREVIOUS note's: let the delay run on, up to 1.5 T past the
		 * ceiling, so the lock usually lands first; if it does not, splice
		 * with the correlation search on periodGood/p0 as the old DRIFT
		 * splicer did. ---- */
		sh->sdUp = 0;
		float hi = sh->sdHiThresh;
		float hard = sh->noteLocked ? hi : hi + 1.5f * sh->Tres;
		if(!sh->sdFading && !sh->sdOnsLive && sh->sdDelay > hard) {
			if(sh->noteLocked) {
				float T1 = v->period;
				float J = (float)sh->sdN * T1;
				float base = sh->sdDelay - J;
				float o = 0.0f;
				if(base > 12.0f)
					o = corrRefine(v->buf, BUF_MASK, v->writePos, sh->sdDelay,
							base, (int)T1);
				HG_EV("S %u %d %.1f-%.1f o%.0f\n", gSampleStamp, s, sh->sdDelay, J, o);
				float nd = base + o;
				if(nd < 8.0f) nd = 8.0f;
				resFade(sh, nd, T1, 0);
			} else {
				float Tr = sh->Tres;
				float base = sh->sdDelay - Tr;
				float o = 0.0f;
				if(base > 12.0f)
					o = corrRefine(v->buf, BUF_MASK, v->writePos, sh->sdDelay,
							base, (int)Tr);
				float nd = base + o;
				if(nd < 8.0f) nd = 8.0f;
				HG_EV("S %u %d %.1f->%.1f corr\n", gSampleStamp, s,
						sh->sdDelay, nd);
				resFade(sh, nd, 0.5f * Tr, 0);
			}
		}
	} else {
		/* ---- UP: delay falls, jump back N periods at the floor ---- */
		sh->sdUp = 1;
		float lo = sh->sdLo, Tr = sh->Tres;
		if(!sh->sdFading && !sh->sdOnsLive) {
			if(sh->sdDelay < lo) {
				float maxD = (float)(gSampleStamp - gOnsetStamp[s])
						- SUP_ONSET_GUARD;
				int n = sh->sdN;
				while(n > 0 && sh->sdDelay + (float)n * Tr > maxD)
					n--;
				if(n < 1) {
					sh->sdDelay = lo;     /* not enough note yet: hold */
				} else {
					float base = sh->sdDelay + (float)n * Tr;
					float o = corrRefine(v->abuf, FXBUF_MASK, v->writePos,
							sh->sdDelay, base, (int)Tr);
					float L = supXfLen(Tr, rho);
					HG_EV("U %u %d %.1f->%.1f n%d\n", gSampleStamp, s,
							sh->sdDelay, base + o, n);
					resFade(sh, base + o, L, 1);
				}
			} else if(sh->sdDelay > lo + ((float)sh->sdN + 1.5f) * Tr) {
				/* above the ceiling (period shrank, parked high): walk back
				 * toward the floor one period at a time */
				float base = sh->sdDelay - Tr;
				float o = corrRefine(v->abuf, FXBUF_MASK, v->writePos,
						sh->sdDelay, base, (int)Tr);
				float L = supXfLen(Tr, rho);
				HG_EV("U %u %d %.1f->%.1f down\n", gSampleStamp, s,
						sh->sdDelay, base + o);
				resFade(sh, base + o, L, 1);
			}
		}
	}

	const float *rb = sh->sdUp ? v->abuf : v->buf;
	unsigned int mask = sh->sdUp ? FXBUF_MASK : BUF_MASK;
	float out;
	/* The outgoing read: a single delay, or - while a splice crossfade is
	 * in flight - the A(old)->B blend. In normal running B is sdDelay; while
	 * an attack overlay is live B is sdMid and sdDelay is the new read C. */
	float bD = sh->sdOnsLive ? sh->sdMid : sh->sdDelay;
	float mix;
	if(sh->sdFading) {
		float t = sh->sdFadePh;
		float ss = t * t * (3.0f - 2.0f * t);
		float gIn, gOut;
		if(sh->sdEq) { gIn = sqrtf(ss); gOut = sqrtf(1.0f - ss); }
		else { gIn = ss; gOut = 1.0f - ss; }
		mix = gOut * bufAtM(rb, mask, v->writePos, sh->sdOldDelay)
		    + gIn * bufAtM(rb, mask, v->writePos, bD);
		sh->sdFadePh += sh->sdFadeStep;
		if(sh->sdFadePh >= 1.0f)
			sh->sdFading = 0;
		sh->sdOldDelay += drift;
		if(sh->sdOldDelay < 3.0f) sh->sdOldDelay = 3.0f;
	} else {
		mix = bufAtM(rb, mask, v->writePos, bD);
	}
	if(sh->sdOnsLive) {
		/* ATTACK OVERLAY (see THE ATTACK ENTRY): smoothstep from whatever
		 * was playing - including a half-finished splice blend, which is
		 * why it is a third leg and not a rewrite of A/B - to the
		 * re-anchored read C. */
		float t = sh->sdOnsPh;
		float go = t * t * (3.0f - 2.0f * t);
		out = (1.0f - go) * mix + go * bufAtM(rb, mask, v->writePos, sh->sdDelay);
		sh->sdOnsPh += sh->sdOnsStep;
		sh->sdMid += drift;
		if(sh->sdMid < 3.0f) sh->sdMid = 3.0f;
		if(sh->sdOnsPh >= 1.0f) {
			sh->sdOnsLive = 0;
			sh->sdFading = 0;
		}
	} else {
		out = mix;
	}
	sh->sdDelay += drift;
	if(sh->sdDelay < 3.0f) sh->sdDelay = 3.0f;
	return out;
}

/* ------------------------- lifecycle ------------------------- */

void shifterInit(Voice *v)
{
	Shifter *sh = &v->sh;
	memset(sh, 0, sizeof(Shifter));
	sh->Tsm = v->p0;
	sh->Tres = v->p0;
	sh->noteLocked = 1;
	sh->mkFx.markDelay = v->p0;
	pickerReset(&sh->mkFx, v->p0);
	legReset(&sh->fm, v->p0);
	sh->sdHiThresh = SD_CEIL(v->p0);
	sh->sdDelay = 0.5f * sh->sdHiThresh;
	sh->sdN = 1;
	sh->sdLo = 5.0f;
	sh->mode = SH_MODE_RUN;
	sh->grainMix = 1.0f;
	sh->aaCoef = 1.0f;
	sh->onsCF = 1.0f / (0.3f * v->p0);
	sh->onsCS = sh->onsCF / 12.0f;
	sh->lpcGain = sh->lpcGainW = 1.0f;
}

/* per-hop duties: picker coefficients, resampler budget, mode machine, LPC */
void shifterHop(Voice *v, int s)
{
	Shifter *sh = &v->sh;
	HG_SETSTR(s);
	int prModePre = sh->mode;
	int fxgPre = sh->fxGate;   /* 2026-11-01 tier-A: fxGate flip telemetry */

	/* THE ANTI-LASER RULE (see file header): trust the tracker. */
	sh->Tsm = v->period;

	float f0 = v->srate / v->period;
	pickerSetF0(&sh->mkFx, f0 * (v->beta > 1.0f ? 1.0f : v->beta), v->srate);

	/*
	 * THE SILENT-STRING FREEZE (2026-09-15, kept).
	 *
	 * dbgGate == 2 is the tracker's OWN verdict from its own GATE_RMS
	 * (hysteretic since 2026-09-09): a string below it has no audible
	 * output for any decision to be right or wrong ABOUT, so decide
	 * nothing and hold the counters. There is exactly one definition of
	 * silence in the engine; inventing a second bar here is the mistake
	 * that cost the top two strings twice (the plain strings' whole device
	 * env range is ~0.01-0.05).
	 */
	int silent = v->dbgGate == 2;

	/* re-lock accounting */
	if(silent) {
		/* hold the counter: see THE SILENT-STRING FREEZE */
	} else if(v->attackBlank == 0 && v->pitchConf >= PITCH_CONF_MIN) {
		if(sh->relockHops < 100) sh->relockHops++;
		if(!sh->noteLocked)
			HG_EV("K %u %d lock T %.1f\n", gSampleStamp, s, v->period);
		sh->noteLocked = 1;
	} else {
		sh->relockHops = 0;
	}

	/* the period the RESAMPLER splices with: the live tracker period once
	 * it has locked THIS note (shifterOnset clears noteLocked); until then
	 * v->period is still the previous note's, so use periodGood/p0 and the
	 * correlation search, exactly as the old DRIFT splicer did. */
	sh->Tres = sh->noteLocked ? v->period
			: (v->periodGood > 24.0f ? v->periodGood : v->p0);
	if(sh->Tres < 24.0f) sh->Tres = 24.0f;

	float rho = v->beta < 0.5f ? 0.5f : (v->beta > 2.0f ? 2.0f : v->beta);

	/* up/down jump size: N periods per splice on the latency budget */
	{
		float Tr = sh->Tres;
		int N = (int)(SUP_BUDGET / Tr + 0.5f);
		if(N < 1) N = 1;
		if(N > SUP_NMAX) N = SUP_NMAX;
		sh->sdN = N;
	}

	/* down-shift delay budget, tracking the live period (see SD_CEIL).
	 * THE OCTAVE-DOWN "RINGMOD" (2026-10-01, owner): at rho = 0.5 a one-
	 * period splice fires every 2T, so any residual mismatch at the seam
	 * modulates the output at f0/2 - sidebands halfway between the
	 * harmonics, i.e. a subharmonic ring-mod. Three changes: the jump is
	 * sdN periods (splice rate / N where the budget allows), every splice
	 * is correlation-refined (+-4) even when locked, and the crossfade is a
	 * whole period (coherent reads: a long linear fade is right).
	 * RAISING it is free - the delay is already below the new ceiling, so
	 * nothing splices. DROPPING it is what a legato move up the neck does,
	 * and dropping it in one step would fire the whole catch-up as a burst
	 * of back-to-back splices. Pace the drop against the engine's OWN
	 * splice rate instead: the delay naturally covers (1-rho)*HOP samples
	 * per hop, so conceding a quarter of that per hop can never add more
	 * than 25% to the splice rate, whatever the shift ratio - and a
	 * full-octave budget change still completes inside ~50ms. */
	{
		float want = SD_CEIL(sh->Tsm > 24.0f ? sh->Tsm : 24.0f)
				+ (float)(sh->sdN - 1) * (sh->Tsm > 24.0f ? sh->Tsm : 24.0f);
		float rd = rho < 1.0f ? rho : 1.0f;
		float drift = 1.0f - rd;
		if(want >= sh->sdHiThresh || drift < 0.01f) {
			/* not down-shifting => the resampler is parked/held and never
			 * splices on this ceiling: retune instantly */
			sh->sdHiThresh = want;
		} else {
			float drop = 0.25f * drift * (float)HOP;
			sh->sdHiThresh -= drop;
			if(sh->sdHiThresh < want)
				sh->sdHiThresh = want;
		}
	}

	/* up-shift floor with fade room (sdN set above) */
	{
		float rU = rho > 1.0f ? rho : 1.0f;
		float L = supXfLen(sh->Tres, rU);
		sh->sdLo = 5.0f + (rU - 1.0f) * L;
	}

	/* onset-follower coefficients, scaled to THIS string's period so the
	 * fast follower smooths over intra-period ripple */
	{
		float cF = 1.0f / (0.3f * sh->Tsm);
		if(cF > 1.0f / 32.0f) cF = 1.0f / 32.0f;
		if(cF < 1.0f / 2000.0f) cF = 1.0f / 2000.0f;
		sh->onsCF = cF;
		sh->onsCS = cF * (1.0f / 12.0f);
	}

	/* anti-alias one-pole on the WRITE side of abuf. Both consumers read it
	 * at an increased rate: the formant-up grains (rate phi) and the
	 * up-shift resampler (rate rho), so the cutoff tracks the larger. */
	float phiT = v->formant > 1.0f ? v->formant : 1.0f;
	float rhoT = rho > 1.0f ? rho : 1.0f;
	float fcA = 0.45f * v->srate / (phiT > rhoT ? phiT : rhoT);
	sh->aaCoef = 1.0f - expf(-2.0f * (float)M_PI * fcA / v->srate);

	/* pitch leg blends and the formant engage blend are one-poled per
	 * sample in shifterRun() - see LEG BLEND RATES there. */

	/*
	 * MODE MACHINE (two modes; the resampler IS the splicer).
	 *
	 *   RUN    the chain (resampler -> formant leg) is the output.
	 *   TRANS  the dry tap carries the attack: entered at an onset when the
	 *          pick CANNOT be shifted in place - pitch UP (you cannot read
	 *          the future) or the formant leg (needs ~2 periods of marks).
	 *          Pitch DOWN alone never enters it: shifterOnset re-anchors
	 *          the resampler to the write head instead.
	 *
	 * There is no longer a pitch-path GRAINS/DRIFT split: the resampler
	 * shifts with or without a period lock (it splices by correlation
	 * until the tracker has locked the note), so the noLockHops leash, the
	 * dwell floors and the confidence flapping that came with them are
	 * gone. What survives is the FORMANT GATE below, for formant-only
	 * configs, whose mark picker can genuinely go stale.
	 */
	int pitchIdle = sh->downAmt < 0.01f && sh->upAmt < 0.01f;
	{
		int fresh = sh->mkFx.haveMark >= 2
				&& sh->mkFx.markDelay < 3.0f * sh->Tsm;
		if(fresh) {
			if(sh->fxAliveHops < 100) sh->fxAliveHops++;
			sh->fxDeadHops = 0;
		} else {
			sh->fxAliveHops = 0;
			if(sh->fxDeadHops < 100) sh->fxDeadHops++;
		}
	}
	int fxAlive = sh->fxAliveHops >= 6;    /* stable enough to come back */
	int fxDead = sh->fxDeadHops >= 8;      /* stale enough to fall out  */
	if(sh->mode == SH_MODE_TRANS) {
		sh->transAge++;
		/* pitch relocked (3 confident hops), or the formant marks are
		 * healthy (formant-only), or ~72 ms have passed: the resampler
		 * shifts a note it cannot lock just fine, so there is nothing to
		 * wait for beyond that. */
		int relocked = sh->relockHops >= 3
				|| (pitchIdle && fxAlive && sh->transAge > 6)
				|| sh->transAge > 25;
		if(relocked && (int)(gSampleStamp - sh->transHoldUntil) >= 0) {
			sh->mode = SH_MODE_RUN;
			sh->fxGate = 0;
			legReset(&sh->fm, sh->mkFx.markDelay);
			/*
			 * UP HAND-OFF: the resampler ran through TRANS (its output was
			 * masked by grainMix ~ 0), so its read may still be sitting in
			 * the PREVIOUS note's ring - the part before the onset stamp.
			 * Crossfade it to the newest point of this note's own history
			 * (never before the onset, never below the floor).
			 */
			if(v->beta > 1.005f && !sh->sdFading && !sh->sdOnsLive) {
				float maxD = (float)(gSampleStamp - gOnsetStamp[s])
						- SUP_ONSET_GUARD;
				if(sh->sdDelay > maxD) {
					/* 2026-10-01: this was `nd = maxD`, i.e. a read sitting
					 * exactly ON the onset sample - it REPLAYED THE ATTACK
					 * after the dry tap had already played it (the "restart"
					 * on every TRANS exit). The resting altitude of the saw,
					 * clamped inside the note, is the newest usable point. */
					float top = sh->sdLo + (float)sh->sdN * sh->Tres;
					float nd = top < maxD ? top : maxD;
					if(nd < sh->sdLo) nd = sh->sdLo;
					float L = supXfLen(sh->Tres, v->beta > 2.0f ? 2.0f : v->beta);
					HG_EV("U %u %d %.1f->%.1f handoff\n", gSampleStamp, s,
							sh->sdDelay, nd);
					sh->sdUp = 1;
					resFade(sh, nd, L, 1);
				}
			}
		}
	}

	/*
	 * THE FORMANT GATE (the "pixely" formant texture, kept minimal).
	 *
	 * With the pitch legs idle the chain and the dry tap differ only by the
	 * formant leg, and that leg needs its OWN mkFx marks. Palm mutes and
	 * scratches starve the picker; then the honest output is dry. Keyed on
	 * the picker's health with hysteresis (6 fresh hops to reopen, 8 stale
	 * to close) because a marginal picker ping-ponged the gate at tens of
	 * Hz on the device (2026-09-01). Never gates a pitch-shifted string,
	 * never gates when the formant leg is off or in LPC mode.
	 */
	{
		int fxOn = gFormantMode == 0 && sh->formantAmt > 0.01f;
		if(!pitchIdle || !fxOn)
			sh->fxGate = 0;
		else if(!silent) {
			if(fxDead) sh->fxGate = 1;
			else if(fxAlive) sh->fxGate = 0;
		}
	}

#ifdef HG_PROBE
	if(sh->mode != prModePre)
		HG_EV("M %u %d %d->%d\n", gSampleStamp, s, prModePre, sh->mode);
	/* per-hop state trace; env/gate let mode residency be weighted by
	 * whether the string was audible. Host-only. */
	HG_EV("P %u %d %d gm %.3f gate %d sd %.1f Tres %.1f N %d lo %.1f Tsm %.1f"
			" fmsrc %.1f env %.3e dbg %d\n",
			gSampleStamp, s, sh->mode, sh->grainMix, sh->fxGate,
			sh->sdDelay, sh->Tres, sh->sdN, sh->sdLo, sh->Tsm,
			sh->fm.srcDelay, v->env, v->dbgGate);
#endif
	/* TEMP ON-DEVICE DIAGNOSTIC (2026-09-01): rt_printf is the deferred
	 * RT-safe print; mode flips are rare and the summary is 1 line/2s/
	 * string. Mode 0 = RUN, 1 = TRANS. REMOVE once the artifact is caught. */
	/* 2026-11-01 tier-A: these two prints moved off the audio thread: push a
	 * compact struct, trellisTick formats + rt_printf (same text, see there). */
	if(sh->fxGate != fxgPre)
		gTelFxg[s]++;
	if(sh->mode != prModePre) {
		ShDiagEv e;
		e.code = SHDIAG_MODE; e.s = s;
		e.i[0] = prModePre; e.i[1] = sh->mode; e.i[2] = sh->noteLocked; e.i[3] = sh->relockHops;
		e.f[0] = sh->Tsm; e.f[1] = v->pitchConf; e.f[2] = (float)v->dbgGate; e.f[3] = v->dbgE0;
		e.f[4] = e.f[5] = 0.0f;
		shDiagPush(&e);
	}
	{
		static int diagCd[NUM_STRINGS];
		if(++diagCd[s] >= 689) {          /* ~2s at hop rate */
			diagCd[s] = 0;
			if(v->env > 0.01f) {
				ShDiagEv e;
				e.code = SHDIAG_STATUS; e.s = s;
				e.i[0] = sh->mode; e.i[1] = v->dbgGate; e.i[2] = e.i[3] = 0;
				e.f[0] = sh->Tsm; e.f[1] = v->pitchConf; e.f[2] = v->env;
				e.f[3] = sh->formantAmt; e.f[4] = v->dbgE0; e.f[5] = 0.0f;
				shDiagPush(&e);
			}
		}
	}
	/* LPC analysis runs every hop on a sounding string, whatever the mode:
	 * lpc.cpp expects exactly that. */
	/* A silent/bleed verdict mid-note must NOT drop the formant stage to
	 * dry (that gate flapping was the "restart" on B/e): keep the last
	 * valid analysis running and only re-analyse when the string sounds. */
	if(gFormantActive && gFormantMode == 1) {
		/* 2026-11-01 lpc-q: STAGGER. One lpcAnalyze costs ~130-195 us; with every string on a
		 * 128-sample hop, 3-4 of them landed in one 64-sample block (>600 us) and the audio
		 * thread overran. Now string s analyses once per 384 samples (6 blocks), in block
		 * (s>>1)*2+(s&1) of that cycle, so at most ONE analysis runs in any block.
		 * Needs render.cpp's hopCountdown offsets (even strings in even blocks, odd in odd). */
#ifndef HG_LPC_STAGGER
#define HG_LPC_STAGGER 1
#endif
#if HG_LPC_STAGGER && NUM_STRINGS == 6
		const unsigned int lpcKk = (gSampleStamp + 64u) / 128u - (unsigned int)(s & 1);
		const int lpcDue = (lpcKk % 3u) == (unsigned int)(s >> 1);
#else
		const int lpcDue = 1;
#endif
		if(!silent && lpcDue) {
			uint64_t lt0 = hgTicks();   /* 2026-11-01 tier-A: lpcus telemetry */
			lpcSubmit(v);   /* 2026-11-01 lpc-w: snapshot + hand to the worker (direct analysis if none running) */
			uint64_t ldt = hgTicks() - lt0;
			if(ldt > gTelLpcMaxTk) gTelLpcMaxTk = (unsigned int)ldt;
		}
	} else {
		sh->lpcValid = 0;
		lpcCancel(s);   /* 2026-11-01 lpc-w: an in-flight result must not re-validate this string */
#ifdef HG_LPC_SPREAD
		lpcSpreadAbort(v);
#endif
	}
}

/*
 * THE ATTACK ENTRY.
 *
 * Pitch DOWN: the old design routed every pick through the unshifted dry
 * tap and crossfaded back to the shifter after relock - the pick landed at
 * the original pitch and the note "morphed" down 20-50 ms later. Now the
 * resampler is re-anchored to delay 4 (the pick is read ~4 samples behind
 * the write head, i.e. at shifted pitch, at near-zero latency) through the
 * resampler's own crossfade, and the chain stays live. The read then drifts
 * away from the head at (1-rho)/sample, so the first splice is
 * (1.15T+28)/(1-rho) ~ 2T away at an octave - by when the tracker has
 * usually locked (resRun waits up to 1.5T more for it before it splices
 * blind by correlation).
 *
 * THE CROSSFADE IS A SMOOTHSTEP, NOT A TELEPORT, and 32 samples (0.7ms),
 * for the reasons measured on the old tap entry: a hard jump from the
 * pre-pick read (near-silence or the previous note) to the live transient
 * steps the output; smoothstep has zero slope at both ends (sqrt() has
 * infinite slope at t=0), and 32 samples is short enough to finish before
 * the pluck's ~2ms rise peaks. An onset DURING a live splice blend is a
 * THIRD leg: the half-finished A->B blend keeps running underneath the
 * overlay (carrying one leg forward instead stepped the output by the
 * other leg's share - 716x the local d2 scale on a -12 re-pick that landed
 * mid-splice).
 *
 * Pitch UP, or a live formant leg: you cannot read the future, and the
 * formant leg has no marks yet, so TRANS + dry tap carries the attack
 * (grainMix falls in 2ms). The resampler keeps running underneath so the
 * pitch-stage output p - which is RECORDED into fxbuf for the formant leg
 * - never steps; shifterHop re-seats its read inside the note at hand-off.
 */
/* ---- MC: strum-coherence reads/writes are versioned per block so threads
 * see exactly the (sample, string)-ordered state of the old serial loop ---- */
struct alignas(64) OnsPub {
	unsigned int oldStamp, oldHold, newStamp, newHold;
	std::atomic<int> evN;
};
static OnsPub gOP[NUM_STRINGS];

void hgOnsBeginBlock(void)
{
	for(int o = 0; o < NUM_STRINGS; o++) {
		gOP[o].oldStamp = gOnsetStamp[o];
		gOP[o].oldHold = gV[o].sh.transHoldUntil;
		gOP[o].evN.store(-1, std::memory_order_relaxed);
	}
}

void hgOnsRead(int o, int s, unsigned int *stamp, unsigned int *hold)
{
	if(!gXMode) {
		*stamp = gOnsetStamp[o];
		*hold = gV[o].sh.transHoldUntil;
		return;
	}
	hgXWait(o, o < s ? gBlkN + 1 : gBlkN);
	int ev = gOP[o].evN.load(std::memory_order_acquire);
	if(ev >= 0 && (ev < gBlkN || (ev == gBlkN && o < s))) {
		*stamp = gOP[o].newStamp;
		*hold = gOP[o].newHold;
	} else {
		*stamp = gOP[o].oldStamp;
		*hold = gOP[o].oldHold;
	}
}

static inline void hgOnsPublish(int s, unsigned int stamp, unsigned int hold)
{
	if(!gXMode)
		return;
	gOP[s].newStamp = stamp;
	gOP[s].newHold = hold;
	gOP[s].evN.store(gBlkN, std::memory_order_release);
}

/* per-sample onset: called from voiceProcess the moment a pick lands */
void shifterOnset(Voice *v, int s)
{
	Shifter *sh = &v->sh;
	HG_EV("O %u %d mode %d sd %.1f\n", gSampleStamp, s, sh->mode, sh->sdDelay);
	/* TEMP ON-DEVICE DIAG (2026-10-01): every onset event with the levels
	 * that fired it and the note age - to prove whether the "restart" on
	 * ringing plain strings is a re-fire of this detector. REMOVE after. */
	/* 2026-11-01 tier-A: moved off the audio thread (formatted in trellisTick) */
	{
		ShDiagEv e;
		e.code = SHDIAG_ONSET; e.s = s;
		e.i[0] = (int)gSampleStamp; e.i[1] = (int)(gSampleStamp - gOnsetStamp[s]); e.i[2] = e.i[3] = 0;
		e.f[0] = sh->envF; e.f[1] = sh->envS; e.f[2] = v->env;
		e.f[3] = gOtherMax[s]; e.f[4] = sh->Tsm; e.f[5] = v->pitchConf;
		shDiagPush(&e);
	}
	int down = v->beta < 0.995f;
	int up = v->beta > 1.005f || sh->upAmt > 0.01f;
	int fxOn = gFormantMode == 0 && (sh->formantAmt > 0.003f
			|| fabsf(v->formant - 1.0f) * 25.0f > 0.01f);
	if(down && !sh->sdOnsLive) {
		/* B (or the single read) becomes the outgoing read; a splice blend
		 * already in flight keeps running underneath the overlay */
		sh->sdMid = sh->sdDelay;
		sh->sdDelay = 4.0f;
		sh->sdOnsLive = 1;
		sh->sdOnsPh = 0.0f;
		float xf = ONSET_XF_T * sh->Tres;
		sh->sdOnsStep = 1.0f / (xf < ONSET_XF ? ONSET_XF : (xf > ONSET_XF_MAX ? ONSET_XF_MAX : xf));
		sh->sdUp = 0;
	}
	if(up || fxOn) {
		if(sh->mode != SH_MODE_TRANS)
			gTelTrans[s]++;       /* 2026-11-01 tier-A: telemetry */
		sh->mode = SH_MODE_TRANS;
	}
	sh->relockHops = 0;
	sh->noteLocked = 0;      /* v->period is the PREVIOUS note's until the lock */
	sh->transAge = 0;        /* a re-pick must not inherit the last pick's age */
	pickerReset(&sh->mkFx, sh->Tsm);
	/* strum coherence: if a neighbour picked <10ms ago, exit together so
	 * a strummed chord resolves as one gesture */
	unsigned int win = (unsigned int)(0.010f * v->srate);
	unsigned int hold = gSampleStamp + win;
	for(int o = 0; o < NUM_STRINGS; o++) {
		if(o == s) continue;
		unsigned int ost, oho;
		hgOnsRead(o, s, &ost, &oho);   /* what the sequential loop would see */
		if(gSampleStamp - ost < win) {
			if(oho > hold) hold = oho;
		}
	}
	sh->transHoldUntil = hold;
	gOnsetStamp[s] = gSampleStamp;
	hgOnsPublish(s, gSampleStamp, hold);
	/* the old hop-rate detector's side effects, now at sample precision */
	v->attackBlank = ATTACK_BLANK_HOPS;
	v->tonalHops = 0;
}

/* ------------------------- per-sample engine ------------------------- */

/*
 * LEG BLEND RATES.
 *
 * downAmt/upAmt/formantAmt are CROSSFADE GAINS between decorrelated
 * signals: the dry input against a ~200-sample-delayed resampler copy,
 * against a grain stream. They used to be one-poled once per hop
 * (0.25 and 0.3 per hop), which means the gain sat still for 127
 * samples and then moved by up to 0.25 in ONE sample. Multiply a 0.25
 * gain step by the difference between two decorrelated legs and you get
 * a waveform step - the host probe measures 13-18x the local
 * first-difference distribution at exactly the sample downAmt jumps,
 * on every slot-pad voicing change. At unity the gains are pinned so
 * nothing moves and the bug is invisible; it only fires when a voicing
 * changes, which is precisely when the user hears it.
 *
 * Same one-pole, same time constant, spread across the hop:
 * (1-c)^HOP == 1-k  =>  c = 1 - (1-k)^(1/HOP). Anything the old code
 * settled to, this settles to identically; it just gets there smoothly.
 */
#define BLEND_C_25 0.0022465f    /* 1 - 0.75^(1/128), was 0.25/hop */
#define BLEND_C_30 0.0027849f    /* 1 - 0.70^(1/128), was 0.30/hop */

float shifterRun(Voice *v, float full)
{
	Shifter *sh = &v->sh;
	float T = sh->Tsm;
	int s = (int)(v - gV);
	HG_SETSTR(s);

	/* leg blends, at sample rate (see LEG BLEND RATES). The targets are
	 * the same expressions shifterHop used, read from the same live
	 * v->beta / v->formant. */
	{
		float dT = v->beta < 0.995f ? 1.0f : 0.0f;
		float uT = v->beta > 1.005f ? 1.0f : 0.0f;
		sh->downAmt += BLEND_C_25 * (dT - sh->downAmt);
		sh->upAmt += BLEND_C_25 * (uT - sh->upAmt);
		float dev = fabsf(v->formant - 1.0f) * 25.0f;
		if(dev > 1.0f) dev = 1.0f;
		sh->formantAmt += BLEND_C_30 * (dev - sh->formantAmt);
	}

	/* ---- pitch stage: the resampler (down AND up) / passthrough ---- */
	float rho = v->beta;
	if(rho < 0.5f) rho = 0.5f;
	if(rho > 2.0f) rho = 2.0f;
	float dA = sh->downAmt, uA = sh->upAmt;
	float amt = dA + uA;
	if(amt > 1.0f) amt = 1.0f;
	float p;
	if(amt > 0.002f)
		p = full * (1.0f - amt) + amt * resRun(sh, v, s, rho, 0);
	else {
		resRun(sh, v, s, 1.0f, 1);      /* idle: re-park, nothing is read */
		p = full;
	}

	v->fxbuf[v->writePos & FXBUF_MASK] = p;

	/* ---- formant stage: series mark grains on the pitch-stage output.
	 * Output period after the pitch stage is T/rho for the shifted legs;
	 * unshifted when the pitch stage is idle. ---- */
	float Tout = (dA > 0.5f || uA > 0.5f) ? T / rho : T;
	if(Tout < 24.0f) Tout = 24.0f;
	if(Tout > 2800.0f) Tout = 2800.0f;
	pickerRun(&sh->mkFx, p, Tout);

	float chain = p;
	float fA = gFormantMode == 0 ? sh->formantAmt : 0.0f;
	if(fA > 0.003f) {
		float phi = v->formant;
		const float *src = v->fxbuf;
		/* formant-up reads through the anti-aliased input copy only when
		 * the pitch stage is idle (abuf mirrors the INPUT, not fxbuf) */
		if(phi > 1.1f && dA < 0.01f && uA < 0.01f)
			src = v->abuf;
		float fw = legRun(&sh->fm, &sh->mkFx, src, FXBUF_MASK,
				v->writePos, Tout, phi, Tout, 1);
		chain = p * (1.0f - fA) + fw * fA;
	}

	/* ---- tap leg: the dry input, a FIXED 4 samples behind the head. It
	 * carries the attack in TRANS and the dry signal behind a closed
	 * formant gate. Fixed means it can never teleport, so it needs no
	 * crossfade of its own: the grainMix blend below is the only fade. ---- */
	float tap = v->buf[(v->writePos - 4u) & BUF_MASK];

	/* ---- leg blend: ~2ms into the tap (attack must land NOW), ~1
	 * output period back out to the shifter chain ---- */
	float target = (sh->mode == SH_MODE_RUN && !sh->fxGate) ? 1.0f : 0.0f;
	float step = target > sh->grainMix
		? 1.0f / (T > 96.0f ? T : 96.0f)
		: 1.0f / (sh->mode == SH_MODE_TRANS ? ONSET_XF : 0.002f * v->srate);
	if(sh->grainMix < target) {
		sh->grainMix += step;
		if(sh->grainMix > target) sh->grainMix = target;
	} else if(sh->grainMix > target) {
		sh->grainMix -= step;
		if(sh->grainMix < target) sh->grainMix = target;
	}
	/* Equal-power on a SMOOTHSTEP-shaped phase, like every other seam in
	 * this file: plain sqrt(1-gm) has INFINITE slope at gm = 1, so the tap
	 * (which now carries the pick at full amplitude from its first sample -
	 * it used to be entered through its own 32-sample fade) stepped the
	 * output by ~0.15 of the transient in the first two samples. The
	 * smoothstep gives the same energy contour with slope 0 at both ends. */
	float gs = sh->grainMix * sh->grainMix * (3.0f - 2.0f * sh->grainMix);
	float gG = sqrtf(gs), gT = sqrtf(1.0f - gs);
#ifdef HG_PROBE
	hgTap(s, p, chain, tap, gG * chain + gT * tap);
#endif
	return gG * chain + gT * tap;
}
