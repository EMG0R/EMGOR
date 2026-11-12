/*
 * shifter.cpp - pitch+formant engine, one instance per string.
 *
 * ARCHITECTURE (v2, conservative): the proven old topology with new
 * precision. Pitch DOWN is a SOLAD drifting-delay resampler - a true
 * frequency scaler whose rate is set by the ratio alone, so it can never
 * chirp, and it works on perfectly periodic content (mark-repositioned
 * COLA grains mathematically CANNOT down-shift periodic content: moving
 * grains by whole periods of a periodic signal is an identity - that was
 * the "laser"/unshifted failure of v1). Pitch UP and FORMANT use
 * mark-synchronous COLA grain streams (window = 2x launch spacing, 50%
 * overlap, sums to a constant: zero grain-rate AM - measured 0.0dB),
 * with the source advancing exactly one pitch mark per launch. The
 * formant leg runs in series on the pitch-stage output, exactly like the
 * old engine, but mark-based and COLA-exact.
 *
 * Attacks are never granulated: a per-sample onset detector snaps a
 * direct tap to the onset and the transient passes exactly once at unity
 * gain (the loud-plosive fix). During mutes/scratches with no pitch
 * lock, a correlation-refined dual-tap splicer takes over.
 *
 * THE ANTI-LASER RULE: the scheduler period IS the tracker period,
 * updated at hop boundaries only. The tracker already snaps on confirmed
 * note changes and glides only on real bends - any extra smoothing layer
 * on top turns every note change into an audible pitch sweep.
 */
#include "hyperguitar.h"
#include <cmath>
#include <cstring>

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

#define SH_MODE_GRAINS 0
#define SH_MODE_TRANS  1
#define SH_MODE_DRIFT  2

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
static float gSplA[(SPL_MAXWIN >> 1) + 1];
static float gSplB[SPL_MAXWIN + 9];

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
	while(dc < need)
		dc += T;
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
	while(dc > ceil2 && dc - T >= need)
		dc -= T;

	float d0 = dc + S * phi;
	if(d0 > (float)(FXBUF_MASK - 16)) {
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

/* -------- SOLAD continuous downshifter (drifting-delay resampler; the
 * drift RATE is 1-rho, so the shift amount can never glide or chirp -
 * the period estimate only sets the jump size/phase) -------- */

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
 * The park is only ever there to give the NEXT down-shift a sane
 * starting delay, so it costs nothing to wait until the leg has actually
 * faded out of the mix. `mayPark` is that permission, passed from the
 * one call site that knows the leg gain. While the leg is still audible
 * at unity ratio the delay simply FREEZES where it is - a fixed-delay
 * read of the input, perfectly continuous - and any crossfade already in
 * flight is allowed to finish rather than being abandoned mid-fade
 * (abandoning it was a second, smaller step in the same sample).
 */
static inline float soladRun(Shifter *sh, Voice *v, float rho, int mayPark)
{
	float drift = 1.0f - rho;
	if(drift < 0.01f) {
		if(mayPark) {
			sh->sdDelay = 0.5f * sh->sdHiThresh;
			sh->sdFading = 0;
			return bufAtM(v->buf, BUF_MASK, v->writePos, sh->sdDelay);
		}
		drift = 0.0f;    /* hold the read; let a live fade run out */
	} else if(!sh->sdFading && sh->sdDelay > sh->sdHiThresh) {
		float J = v->period;
		HG_EV("S %u %d %.1f-%.1f\n", gSampleStamp, (int)(v - gV),
				sh->sdDelay, J);
		sh->sdOldDelay = sh->sdDelay;
		sh->sdDelay -= J;
		if(sh->sdDelay < 8.0f) sh->sdDelay = 8.0f;
		sh->sdFading = 1;
		sh->sdFadePh = 0.0f;
		sh->sdFadeStep = 1.0f / (0.5f * J);
	}

	float out;
	if(sh->sdFading) {
		float t = sh->sdFadePh;
		float g = t * t * (3.0f - 2.0f * t);
		out = bufAtM(v->buf, BUF_MASK, v->writePos, sh->sdOldDelay) * (1.0f - g)
		    + bufAtM(v->buf, BUF_MASK, v->writePos, sh->sdDelay) * g;
		sh->sdFadePh += sh->sdFadeStep;
		if(sh->sdFadePh >= 1.0f)
			sh->sdFading = 0;
		sh->sdOldDelay += drift;
	} else {
		out = bufAtM(v->buf, BUF_MASK, v->writePos, sh->sdDelay);
	}
	sh->sdDelay += drift;
	return out;
}

/* ------------------------- lifecycle ------------------------- */

void shifterInit(Voice *v)
{
	Shifter *sh = &v->sh;
	memset(sh, 0, sizeof(Shifter));
	sh->Tsm = v->p0;
	sh->mkIn.markDelay = v->p0;
	sh->mkFx.markDelay = v->p0;
	pickerReset(&sh->mkIn, v->p0);
	pickerReset(&sh->mkFx, v->p0);
	legReset(&sh->up, v->p0);
	legReset(&sh->fm, v->p0);
	sh->sdHiThresh = SD_CEIL(v->p0);
	sh->sdDelay = 0.5f * sh->sdHiThresh;
	sh->mode = SH_MODE_GRAINS;
	sh->grainMix = 1.0f;
	sh->tapDelay = 64.0f;
	sh->aaCoef = 1.0f;
	sh->onsCF = 1.0f / (0.3f * v->p0);
	sh->onsCS = sh->onsCF / 12.0f;
	sh->lpcGain = sh->lpcGainW = 1.0f;
}

/* per-hop duties: picker coefficients, leg blends, mode transitions, LPC */
void shifterHop(Voice *v, int s)
{
	(void)s;
	Shifter *sh = &v->sh;
	HG_SETSTR(s);
	int prModePre = sh->mode;

	/* THE ANTI-LASER RULE (see file header): trust the tracker. */
	sh->Tsm = v->period;

	float f0 = v->srate / v->period;
	pickerSetF0(&sh->mkIn, f0, v->srate);
	pickerSetF0(&sh->mkFx, f0 * (v->beta > 1.0f ? 1.0f : v->beta), v->srate);

	/* down-shift delay budget, tracking the live period (see SD_CEIL).
	 * RAISING it is free - the delay is already below the new ceiling, so
	 * nothing splices. DROPPING it is what a legato move up the neck does,
	 * and dropping it in one step would fire the whole catch-up as a burst
	 * of back-to-back splices. Pace the drop against the engine's OWN
	 * splice rate instead: the delay naturally covers (1-rho)*HOP samples
	 * per hop, so conceding a quarter of that per hop can never add more
	 * than 25% to the splice rate, whatever the shift ratio - and a
	 * full-octave budget change still completes inside ~50ms. */
	{
		float want = SD_CEIL(sh->Tsm > 24.0f ? sh->Tsm : 24.0f);
		float rho = v->beta < 1.0f ? (v->beta > 0.5f ? v->beta : 0.5f) : 1.0f;
		float drift = 1.0f - rho;
		if(want >= sh->sdHiThresh || drift < 0.01f) {
			/* not down-shifting => soladRun is parking the delay at
			 * 0.5*ceiling and never splices: retune instantly */
			sh->sdHiThresh = want;
		} else {
			float drop = 0.25f * drift * (float)HOP;
			sh->sdHiThresh -= drop;
			if(sh->sdHiThresh < want)
				sh->sdHiThresh = want;
		}
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

	/* anti-alias one-pole for formant-up grain reads (write-side) */
	float phiT = v->formant > 1.0f ? v->formant : 1.0f;
	float fcA = 0.45f * v->srate / phiT;
	sh->aaCoef = 1.0f - expf(-2.0f * (float)M_PI * fcA / v->srate);

	/* pitch leg blends and the formant engage blend used to be one-poled
	 * HERE, at hop rate. They are now stepped per sample in shifterRun()
	 * with the identical time constant - see LEG BLEND RATES there. */

	/*
	 * THE SILENT-STRING FREEZE (2026-09-15).
	 *
	 * Device trace, ordinary playing: ~6.6 mode transitions PER SECOND,
	 * 2539 GRAINS->DRIFT against 1713 back the other way, and a large
	 * share of them firing at window energies of 1e-07..1e-09 with
	 * pitchConf slamming bimodally between 0.00 and ~1.00 hop to hop.
	 * That is not a mode machine reacting to the music. It is one
	 * reacting to the NOISE FLOOR: the tracker's silence gate zeroes
	 * conf, noLockHops walks past its leash, GRAINS->DRIFT; a hop or
	 * three later the correlation finds enough structure in the same
	 * noise to relock, DRIFT->GRAINS; repeat forever. Each round trip
	 * swaps the grain engine for the period-REPEATING splicer, which is
	 * the "looping" texture, and it is worst on the two plain top
	 * strings for the same reason everything else in this file has been:
	 * they have the lowest pickup output, so they spend the largest part
	 * of every note down where the silence bar lives.
	 *
	 * A string below the tracker's own silence bar has no audible output
	 * for the mode to be right or wrong ABOUT, so the honest answer is to
	 * decide nothing: hold the mode, and hold the lock counters with it,
	 * so the machine wakes up in the state it went to sleep in instead of
	 * with noLockHops already past 10 and the first marginal hop of the
	 * next note pushing it straight out of GRAINS. Nothing is lost by
	 * waiting - a real pick enters TRANS through shifterOnset regardless
	 * of what this machine thinks, and that is the path that matters.
	 *
	 * NOTE WHAT THIS DOES NOT DO: it does not add an energy threshold.
	 * dbgGate == 2 is the tracker's OWN verdict from its own GATE_RMS
	 * (hysteretic since 2026-09-09), so there is still exactly one
	 * definition of silence in the engine. Inventing a second bar here
	 * is precisely the mistake that has cost the top two strings twice
	 * now, and the obvious candidate would have been fatal: the `st`
	 * diagnostic below uses env > 0.01f, but the plain strings' whole
	 * device env range is ~0.01-0.05, so a 0.01 bar gates out exactly
	 * the strings this is meant to help.
	 */
	int silent = v->dbgGate == 2;

	/* re-lock accounting */
	if(silent) {
		/* hold both counters: see THE SILENT-STRING FREEZE */
	} else if(v->attackBlank == 0 && v->pitchConf >= PITCH_CONF_MIN) {
		if(sh->relockHops < 100) sh->relockHops++;
		sh->noLockHops = 0;
	} else {
		sh->relockHops = 0;
		if(v->pitchConf < NSDF_CONF && sh->noLockHops < 100)
			sh->noLockHops++;
	}

	/* mode machine (transitions at hop rate; fades run at sample rate).
	 * CONSERVATIVE: 3 confident hops to re-enter grains, long leash
	 * before falling back - mode flapping reads as warble.
	 *
	 * THE FORMANT GATE (the "pixely" formant texture).
	 *
	 * grainMix keys the chain<->tap crossfade to THIS machine, and this
	 * machine used to key every transition to the PITCH tracker's
	 * confidence. The formant leg does not care about pitch lock - it
	 * needs its own mkFx marks, nothing else - but with the pitch legs
	 * idle every mid-note confidence wobble (decay, vibrato, a bend)
	 * still dropped GRAINS->DRIFT, and in DRIFT at rho==1 the tap is
	 * PLAIN DRY: the whole formant effect faded to dry in 2ms, sat there
	 * 100-1000ms, then faded back. Host probe, formant-only scene, 12s of
	 * realistic material: 2-6 such mid-note dropouts per string, up to
	 * 4.4 SECONDS of a 12s take with the effect gated off - while mkFx
	 * kept launching healthy grains the entire time (launch holes only
	 * ever span 1-2 launch periods, at TRANS entries).
	 *
	 * So when the pitch legs are idle (tap and chain then differ only by
	 * the formant leg) the machine keys on the FX picker's health
	 * instead: stay in / return to GRAINS while mkFx has fresh marks,
	 * whatever the pitch tracker thinks. Truly aperiodic material (palm
	 * mutes) still falls through to the tap: mkFx goes stale there, and
	 * fxAlive with it, which is exactly the case DRIFT exists for.
	 * Shifted configs (either pitch leg audible) take the old paths
	 * bit-identically. */
	int pitchIdle = sh->downAmt < 0.01f && sh->upAmt < 0.01f;
	/* HYSTERESIS, not an instantaneous test. The first version keyed the
	 * transitions on this hop's freshness alone, and a MARGINAL picker
	 * (marks landing on some search cycles and not others) ping-ponged
	 * the mode every few hops - the wet/dry gate then flapped at tens of
	 * Hz, which is a texture, not a transition (device trace 2026-09-01).
	 * Re-entering GRAINS now takes 6 consecutive fresh hops; falling to
	 * DRIFT takes 8 consecutive stale ones. A picker that only limps
	 * parks in DRIFT and STAYS there instead of strobing the formant. */
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
	/*
	 * MARK HEALTH ON THE INPUT STREAM, the twin of the fxAlive test above
	 * for mkIn. No hysteresis counter because there is exactly one
	 * decision that reads it - the dead-zone escape below, once per note -
	 * so there is nothing for a marginal hop to ping-pong.
	 */
	int mkAlive = sh->mkIn.haveMark >= 2
			&& sh->mkIn.markDelay < 3.0f * sh->Tsm;
	if(sh->mode == SH_MODE_TRANS) {
		sh->transAge++;
		int relocked = sh->relockHops >= 3
				|| (pitchIdle && fxAlive && sh->transAge > 6);
		/*
		 * THE DEAD-ZONE ESCAPE HAD ONE EXIT AND IT WAS THE WRONG ONE.
		 *
		 * transAge > 25 fires when confidence hovers in 0.45..0.92, which
		 * advances NEITHER counter - a periodic note whose period is
		 * being measured noisily. That is the normal state of a quiet
		 * string, and it used to dump the voice into DRIFT, i.e. into the
		 * splicer that repeats whole periods. Measured: with the onset
		 * detector fixed (see dsp_core.cpp) the top strings' picks are
		 * detected again, every one of them enters TRANS, and ~15% of
		 * them left it through THIS branch - so fixing the onsets alone
		 * moved audible DRIFT residency from 4.0% to 7.5% of sounding
		 * hops. The engine was only "not flapping" before because it was
		 * not noticing the picks.
		 *
		 * noLockHops > 18 is a different and correct case: 18 hops of
		 * conf < 0.45 is genuinely aperiodic material (palm mute,
		 * scratch), which is what DRIFT exists for. Split the two, and
		 * let the dead zone go to GRAINS whenever the mark stream the
		 * legs actually run on is healthy - a noisily-measured period
		 * still granulates far better than it splices.
		 */
		int deadZone = sh->transAge > 25 && sh->noLockHops <= 18;
		if(deadZone && mkAlive)
			relocked = 1;
		if(relocked && (int)(gSampleStamp - sh->transHoldUntil) >= 0) {
			sh->mode = SH_MODE_GRAINS;
			legReset(&sh->up, sh->mkIn.markDelay);
			legReset(&sh->fm, sh->mkFx.markDelay);
			/* post-attack bass tightness: restart the down leg near the
			 * write head (the old "transient re-anchor", done ONCE at a
			 * clean boundary instead of mid-flight).
			 *
			 * BOTH RE-ANCHORS ARE CROSSFADED. The tap one used to be a
			 * bare `tapDelay = 32` with tapFading cleared - "inside
			 * bounds: no splice on exit" only means the SPLICER will not
			 * fire, not that the jump is inaudible. It moves the tap read
			 * 4 -> 32 samples in one sample, and the tap is at nearly
			 * full gain here (grainMix has decayed to ~0 during TRANS, so
			 * gT ~ 1). The probe measures it at 9x the local
			 * first-difference distribution on EVERY pick with the wet
			 * leg open - masked by the attack, but there. The splicer's
			 * own de-glitching crossfade is right here; use it. */
			{
				float Tref = v->periodGood > 24.0f ? v->periodGood : v->p0;
				if(!sh->tapFading && fabsf(sh->tapDelay - 32.0f) > 0.5f) {
					sh->tapOld = sh->tapDelay;
					sh->tapFading = 1;
					sh->tapFadePh = 0.0f;
					sh->tapFadeStep = 1.0f / (0.5f * Tref);
				}
				sh->tapDelay = 32.0f;
				if(fabsf(sh->sdDelay - 32.0f) > 0.5f) {
					sh->sdOldDelay = sh->sdDelay;
					sh->sdFading = 1;
					sh->sdFadePh = 0.0f;
					sh->sdFadeStep = 1.0f / (0.5f * Tref);
				}
				sh->sdDelay = 32.0f;
			}
		} else if(sh->noLockHops > 18 || (deadZone && !mkAlive)) {
			/* the attack never resolved and the marks are gone with it:
			 * fall through to the splicer, which still SHIFTS - staying
			 * here would play the whole note unshifted at full wet.
			 * (The dead zone WITH live marks is handled above; it now
			 * resolves to GRAINS instead of landing here.) */
			sh->mode = SH_MODE_DRIFT;
		}
	} else if(sh->mode == SH_MODE_GRAINS) {
		/* DWELL FLOOR (2026-09-09): the device trace measured 196k mode
		 * transitions in ONE session - conf hovering at a gate flapped
		 * GRAINS<->DRIFT continuously, and each flap swaps the grain
		 * engine for the period-REPEATING splicer: the "looping" sound.
		 * Falling out of GRAINS now requires 150ms of residency first;
		 * recovery into GRAINS stays fast (relock is already 3 hops).
		 * The grain legs coast fine on a stale period for 150ms - the
		 * mark pickers are energy-based and keep their own lattice. */
		if(!silent && sh->noLockHops > 10 && sh->modeDwell > 52
				&& !(pitchIdle && !fxDead))
			sh->mode = SH_MODE_DRIFT;
	} else { /* DRIFT */
		/*
		 * THE DWELL FLOOR IS SYMMETRIC NOW. It was one-directional:
		 * leaving GRAINS cost 52 hops of residency, coming back cost
		 * nothing, so the cycle time of a flap was set entirely by the
		 * GRAINS side and the machine could still round-trip ~3 times a
		 * second - which is what the device measured. A floor here too
		 * halves the achievable flap rate outright.
		 *
		 * It is deliberately SHORTER than 52 (18 hops, ~52ms), because
		 * the two directions are not symmetric in cost: dwelling in
		 * GRAINS on a stale period is free (the mark pickers keep their
		 * own lattice), while dwelling in DRIFT at rho == 1 means the
		 * tap is PLAIN DRY and the whole formant effect is gated off -
		 * the dropout the FORMANT GATE comment above exists to stop.
		 * 52ms is long enough to outlast the relock/lose-lock round trip
		 * that drives the oscillation and short enough to stay under the
		 * ~100ms where a wet/dry dropout stops being a transition and
		 * starts being a hole.
		 */
		if(!silent && sh->modeDwell > 18
				&& (sh->relockHops >= 3 || (pitchIdle && fxAlive))) {
			sh->mode = SH_MODE_GRAINS;
			legReset(&sh->up, sh->mkIn.markDelay);
			legReset(&sh->fm, sh->mkFx.markDelay);
			/* same re-anchor, same reason to crossfade it (see above) */
			if(fabsf(sh->sdDelay - 32.0f) > 0.5f) {
				float Tref = v->periodGood > 24.0f ? v->periodGood : v->p0;
				sh->sdOldDelay = sh->sdDelay;
				sh->sdFading = 1;
				sh->sdFadePh = 0.0f;
				sh->sdFadeStep = 1.0f / (0.5f * Tref);
			}
			sh->sdDelay = 32.0f;
		}
	}

#ifdef HG_PROBE
	if(sh->mode != prModePre)
		HG_EV("M %u %d %d->%d\n", gSampleStamp, s, prModePre, sh->mode);
	/* per-hop state trace: effective output delay is dominated by
	 * (1-grainMix)*tapDelay in TRANS/DRIFT and by the leg delays in
	 * GRAINS - log everything that feeds it */
	/* env/gate are here so mode RESIDENCY can be weighted by whether the
	 * string was audible at the time: "40% of hops in DRIFT" means
	 * nothing if most of those hops were silence, and the whole point of
	 * THE SILENT-STRING FREEZE is that the mode during silence is not a
	 * fact about the sound. Host-only, like the rest of this block. */
	HG_EV("P %u %d %d gm %.3f tap %.1f sd %.1f Tsm %.1f fmsrc %.1f"
			" env %.3e gate %d\n",
			gSampleStamp, s, sh->mode, sh->grainMix, sh->tapDelay,
			sh->sdDelay, sh->Tsm, sh->fm.srcDelay, v->env, v->dbgGate);
#endif
	/* TEMP ON-DEVICE DIAGNOSTIC (2026-09-01, "top two strings tweak
	 * below ~G with formant on"): the host bench cannot reproduce it, so
	 * trace the real instrument. rt_printf is the deferred RT-safe
	 * print; mode flips are rare and the summary is 1 line/2s/string.
	 * REMOVE once the artifact is caught. */
	if(sh->mode != prModePre)
		sh->modeDwell = 0;
	else if(sh->modeDwell < 10000)
		sh->modeDwell++;
	if(sh->mode != prModePre)
		rt_printf("md s%d %d->%d T%.0f c%.2f nl%d rl%d g%d E%.1e\n", s,
				prModePre, sh->mode, sh->Tsm, v->pitchConf,
				sh->noLockHops, sh->relockHops, v->dbgGate, v->dbgE0);
	{
		static int diagCd[NUM_STRINGS];
		if(++diagCd[s] >= 689) {          /* ~2s at hop rate */
			diagCd[s] = 0;
			if(v->env > 0.01f)
				rt_printf("st s%d T%.0f c%.2f m%d e%.3f fA%.2f g%d E%.1e\n",
						s, sh->Tsm, v->pitchConf, sh->mode,
						v->env, sh->formantAmt, v->dbgGate, v->dbgE0);
		}
	}
	if(gFormantActive && gFormantMode == 1 && sh->mode == SH_MODE_GRAINS)
		lpcAnalyze(v);
	else
		sh->lpcValid = 0;
}

/*
 * THE ATTACK ENTRY IS A CROSSFADE, NOT A TELEPORT.
 *
 * shifterOnset used to slam `tapDelay = 4` and clear tapFading. That is the
 * exact mirror of the TRANS-exit bug fixed above, except it fires on EVERY
 * pick and the tap is the whole output there (grainMix has usually decayed
 * to 0, so gT ~ 1). The tap read teleports from wherever it was - 64 at
 * rest, 32 after a grains exit, 200..1800 while DRIFT is drifting it - to
 * 4, in one sample, between two completely decorrelated points of the ring.
 * Before the pick that point is pre-attack near-silence; after it, the live
 * transient. The output therefore steps from ~0 straight to the transient's
 * instantaneous value with no ramp at all: the pick's own 2ms rise is
 * skipped, and what the user hears on impact is that step, not the pick.
 *
 * Measured with the host attack probe (6 voices, decaying/vibrato'd/
 * inharmonic plucks, repeated picks, an 8ms strum, re-picks into a ringing
 * note; -12/-5/+7/+12 and unity):
 *
 *   69 of 79 flagged discontinuities land on EXACTLY the onset sample,
 *   every one of them a tapDelay->4 teleport, median 17x and worst 43x the
 *   local first-difference distribution (detector control on the raw input:
 *   0 hits, so none of this is the pick itself).
 *
 * The fix must not cost attack time - the tap sitting at 4 samples IS the
 * feature. So the tap still moves to 4 THIS SAMPLE; only its GAIN ramps,
 * against a held read of what was already playing. Two choices matter:
 *
 *   - SMOOTHSTEP, not equal-power. The outgoing leg at an onset is
 *     near-silence, so there is no power to preserve, and sqrt() has
 *     INFINITE slope at t=0 - it would re-create a step at the fade's own
 *     start. Smoothstep has zero derivative at both ends and a bounded
 *     1.5/N slope in the middle, so the step is spread, not moved.
 *   - 32 SAMPLES (0.7ms). Long enough that 1.5/32 caps the residual at ~5%
 *     of the old step, short enough to finish long before the pluck's ~2ms
 *     rise reaches its peak - the hosttest attack peak ratio is unchanged
 *     at 1.36.
 *
 * An onset arriving DURING a live fade no longer abandons it (that was a
 * second step in the same sample, seen on 11 of the 69). It is folded in by
 * carrying the dominant leg forward as the new outgoing read; both fades
 * that can be in flight here are between period- or correlation-aligned
 * taps, so the dominant leg is nearly the whole of the mixture.
 */
#define ONSET_XF 32.0f

/* per-sample onset: called from voiceProcess the moment a pick lands */
void shifterOnset(Voice *v, int s)
{
	Shifter *sh = &v->sh;
	HG_EV("O %u %d mode %d tap %.1f\n", gSampleStamp, s, sh->mode, sh->tapDelay);
	sh->mode = SH_MODE_TRANS;
	/* Fold any in-flight fade into this one, then arm the entry fade. If a
	 * fade is running, its dominant leg becomes the new outgoing read
	 * (tapOld is already that leg while tapFadePh < 0.5); otherwise the
	 * single current read is. Either way the outgoing read is what the
	 * output was actually playing, so the fade starts continuous. */
	if(!sh->tapFading || sh->tapFadePh >= 0.5f)
		sh->tapOld = sh->tapDelay;
	sh->tapDelay = 4.0f;
	sh->tapFading = 1;
	sh->tapFadePh = 0.0f;
	sh->tapFadeStep = 1.0f / ONSET_XF;
	sh->relockHops = 0;
	sh->noLockHops = 0;      /* stale silence-count must not bounce us to DRIFT */
	sh->transAge = 0;        /* ditto the TRANS dead-zone escape counter: it was
	                          * never reset on a re-pick, so a fresh onset could
	                          * inherit transAge > 25 from the PREVIOUS pick and
	                          * fall to DRIFT one hop later - with the tap parked
	                          * at 4 and rho > 1 that drifted the read past the
	                          * write head (probe: tapDelay -3/-7, stale-ring
	                          * click on every fast re-pick while shifting up) */
	pickerReset(&sh->mkIn, sh->Tsm);
	pickerReset(&sh->mkFx, sh->Tsm);
	/* strum coherence: if a neighbour picked <10ms ago, exit together so
	 * a strummed chord resolves as one gesture */
	unsigned int win = (unsigned int)(0.010f * v->srate);
	unsigned int hold = gSampleStamp + win;
	for(int o = 0; o < NUM_STRINGS; o++) {
		if(o == s) continue;
		if(gSampleStamp - gOnsetStamp[o] < win) {
			Shifter *so = &gV[o].sh;
			if(so->transHoldUntil > hold) hold = so->transHoldUntil;
		}
	}
	sh->transHoldUntil = hold;
	gOnsetStamp[s] = gSampleStamp;
	/* the old hop-rate detector's side effects, now at sample precision */
	v->attackBlank = ATTACK_BLANK_HOPS;
	v->tonalHops = 0;
}

/* ------------------------- per-sample engine ------------------------- */

/*
 * LEG BLEND RATES.
 *
 * downAmt/upAmt/formantAmt are CROSSFADE GAINS between decorrelated
 * signals: the dry input against a ~200-sample-delayed SOLAD copy,
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
	HG_SETSTR((int)(v - gV));

	pickerRun(&sh->mkIn, full, T);

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

	/* ---- pitch stage: SOLAD down / mark grains up / passthrough ---- */
	float rho = v->beta;
	if(rho < 0.5f) rho = 0.5f;
	if(rho > 2.0f) rho = 2.0f;
	float dA = sh->downAmt, uA = sh->upAmt;
	float p = full * (1.0f - dA - uA);
	if(dA > 0.002f)
		p += dA * soladRun(sh, v, rho < 1.0f ? rho : 1.0f, 0);
	else
		soladRun(sh, v, 1.0f, 1);       /* warm, and safe to re-park */
	if(uA > 0.002f) {
		float S = T / (rho > 1.0f ? rho : 1.0f);
		if(S < 24.0f) S = 24.0f;
		p += uA * legRun(&sh->up, &sh->mkIn, v->buf, BUF_MASK,
				v->writePos, S, 1.0f, T, 0);
	}

	v->fxbuf[v->writePos & FXBUF_MASK] = p;

	/* ---- formant stage: series mark grains on the pitch-stage output.
	 * Output period after the pitch stage is T/rho for the shifted legs;
	 * unshifted when the pitch stage is idle. ---- */
	float Tout = dA > 0.5f ? T / rho : (uA > 0.5f ? T / rho : T);
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

	/* ---- tap leg: transient pass-through / no-lock drift splicer ---- */
	float tap;
	if(sh->mode == SH_MODE_TRANS) {
		if(sh->tapFading) {
			/* attack entry fade (see THE ATTACK ENTRY IS A CROSSFADE).
			 * tapDelay is ALREADY 4 - only the gain ramps, so the
			 * transient's timing is untouched. Smoothstep: zero slope at
			 * both ends, so neither the fade's start nor its end is
			 * itself a step. tapOld is a fixed delay here (TRANS never
			 * drifts), i.e. a perfectly continuous read of whatever was
			 * playing before the pick. ~32 samples per pick. */
			float t2 = sh->tapFadePh;
			float g = t2 * t2 * (3.0f - 2.0f * t2);
			tap = (1.0f - g) * bufAtM(v->buf, BUF_MASK, v->writePos, sh->tapOld)
			    + g * bufAtM(v->buf, BUF_MASK, v->writePos, sh->tapDelay);
			sh->tapFadePh += sh->tapFadeStep;
			if(sh->tapFadePh >= 1.0f)
				sh->tapFading = 0;
		} else {
			tap = bufAtM(v->buf, BUF_MASK, v->writePos, sh->tapDelay);
		}
	} else {
		float Tref = v->periodGood > 24.0f ? v->periodGood : v->p0;
		if(sh->mode == SH_MODE_DRIFT) {
			sh->tapDelay += 1.0f - rho;
			/* while a fade blocks the splice, the live tap must not
			 * drift past the write head (rho > 1 drifts toward it) */
			if(sh->tapFading && sh->tapDelay < 3.0f)
				sh->tapDelay = 3.0f;
		}
		/*
		 * THE UP-SHIFT STALE-RING CLICK (the pitch-up crackle).
		 *
		 * For rho > 1 the drift is NEGATIVE: the tap runs INTO the write
		 * head. With lo = 8 the splice fires with tapOld ~ 7, and during
		 * the 0.5*Tref crossfade tapOld keeps drifting at (1-rho) per
		 * sample - it crosses ZERO ~7 samples in and bufAtM wraps past
		 * the write head into ring data a full buffer old, at nearly
		 * full outgoing gain. Host probe, +12, realistic material: 720
		 * low-bound splices -> 245 click clusters, every one 7-9 samples
		 * after its splice (random-position null: thousands of samples).
		 * Pitch DOWN drifts away from the head and never sees this,
		 * which is exactly why only up-shift crackled.
		 *
		 * Fix: the low bound must leave room for the OUTGOING leg to
		 * finish its fade while still causal - lo covers the fade's own
		 * drift, (rho-1)*0.5*Tref, plus the old 8-sample floor. The hi
		 * bound and the down-shift path are byte-identical to before.
		 */
		/*
		 * THE DRIFT LATENCY BUDGET (the "low string feels late" bug).
		 *
		 * hi was 2.5*Tref + 256, so at rho < 1 the tap sawed between
		 * ~1.5T and 2.5T of delay: 34ms AVERAGE on the low E, held for
		 * as long as DRIFT dwells (host trace, oct-down realistic
		 * material: whole seconds at 16-33ms). The ceiling buys nothing:
		 * a splice retreats exactly one period, so the splice RATE is
		 * T/(1-rho) wherever the saw sits - all hi sets is the resting
		 * ALTITUDE. The GRAINS-mode SOLAD runs the same drifting-delay
		 * scheme at 1.15T + 32 with a LIVE period; DRIFT's periodGood is
		 * staler, so keep extra margin, but 2.5T was pure latency:
		 * 1.35T + 64 saws 0.35T..1.35T (mean ~0.85T = 12ms low E), and
		 * the post-splice floor 0.35T + 64 still clears lo with the
		 * whole fade's drift to spare.
		 */
		float lo = 8.0f, hi = 1.35f * Tref + 64.0f;
		if(rho > 1.0f)
			lo += (rho - 1.0f) * 0.55f * Tref;
		if(!sh->tapFading && (sh->tapDelay < lo || sh->tapDelay > hi)) {
			/* splice: jump one period back toward the window, refined
			 * +-4 samples by normalized cross-correlation so the two
			 * taps are phase-aligned at the fade (H949's de-glitcher) */
			float dir = sh->tapDelay < lo ? 1.0f : -1.0f;
			float base = sh->tapDelay + dir * Tref;
			int win = (int)(Tref < 300.0f ? Tref : 300.0f);
			float bestv = -2.0f, besto = 0.0f;
			/*
			 * SHARED READS - see THE SPLICE BURST above. Two facts,
			 * both exact:
			 *   1. the sh->tapDelay+i window does not depend on o, so
			 *      it (and its energy e1) is interpolated ONCE, not
			 *      nine times. Same values, same accumulation order.
			 *   2. the nine base+o windows are nine views of ONE
			 *      contiguous run of positions: base+o+i over
			 *      o in [-4,4] and even i is exactly base-4 .. base+
			 *      win+3, and offset o's window is that run read from
			 *      index o+4. Sweeping the run once feeds all nine.
			 * (2) is bit-exact only if base+o is an exact float for
			 * every o - then (base-4)+k and (base+o)+i are two
			 * spellings of the same real number, so both round to the
			 * same float and bufAtM returns the same sample. That
			 * holds unless base+-4 crosses a binade; the guard tests
			 * that directly (the differences below are exact by
			 * Sterbenz, so they equal 4 iff the sums were exact), and
			 * on failure we fall through to the original nine-window
			 * loop - slow, vanishingly rare, and identical by
			 * construction.
			 */
			if(win <= SPL_MAXWIN
					&& (base + 4.0f) - base == 4.0f
					&& base - (base - 4.0f) == 4.0f) {
				float c0 = base - 4.0f;
				int nk = ((win - 1) & ~1) + 9;   /* k = 0 .. maxi+8 */
				float e1 = 0.0f;
				for(int k = 0; k < nk; k++)
					gSplB[k] = bufAtM(v->buf, BUF_MASK, v->writePos,
							c0 + (float)k);
				for(int i = 0; i < win; i += 2) {
					float sa = bufAtM(v->buf, BUF_MASK, v->writePos,
							sh->tapDelay + (float)i);
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
			} else {
				for(int o = -4; o <= 4; o++) {
					float acf = 0.0f, e1 = 0.0f, e2 = 0.0f;
					for(int i = 0; i < win; i += 2) {
						float sa = bufAtM(v->buf, BUF_MASK,
								v->writePos,
								sh->tapDelay + (float)i);
						float sb = bufAtM(v->buf, BUF_MASK,
								v->writePos,
								base + (float)o + (float)i);
						acf += sa * sb;
						e1 += sa * sa;
						e2 += sb * sb;
					}
					float val = acf / sqrtf(e1 * e2 + 1e-12f);
					if(val > bestv) { bestv = val; besto = (float)o; }
				}
			}
			HG_EV("T %u %d %.1f->%.1f\n", gSampleStamp,
					(int)(v - gV), sh->tapDelay, base + besto);
			sh->tapOld = sh->tapDelay;
			sh->tapDelay = base + besto;
			sh->tapFading = 1;
			sh->tapFadePh = 0.0f;
			sh->tapFadeStep = 1.0f / (0.5f * Tref);
		}
		if(sh->tapFading) {
			/* equal-power, but on a SMOOTHSTEP-shaped phase: plain
			 * sqrt(t) has infinite slope at t=0 (the same edge the
			 * attack-entry fade had), sqrt(smoothstep(t)) starts with
			 * slope 0. Same energy contour, no step at the seam. */
			float t2 = sh->tapFadePh;
			float ss = t2 * t2 * (3.0f - 2.0f * t2);
			float gIn = sqrtf(ss), gOut = sqrtf(1.0f - ss);
			tap = gOut * bufAtM(v->buf, BUF_MASK, v->writePos, sh->tapOld)
			    + gIn * bufAtM(v->buf, BUF_MASK, v->writePos, sh->tapDelay);
			sh->tapFadePh += sh->tapFadeStep;
			if(sh->tapFadePh >= 1.0f)
				sh->tapFading = 0;
			if(sh->mode == SH_MODE_DRIFT) {
				sh->tapOld += 1.0f - rho;
				/* underrun guard: on a DRIFT entry (tap parked at 4
				 * by the attack path) the outgoing leg can still run
				 * out of causal buffer mid-fade at rho>1. Freezing it
				 * plays a perfectly continuous unshifted read at a
				 * fading gain - vs. wrapping into stale ring data. */
				if(sh->tapOld < 3.0f) sh->tapOld = 3.0f;
			}
		} else {
			tap = bufAtM(v->buf, BUF_MASK, v->writePos, sh->tapDelay);
		}
	}

	/* ---- leg blend: ~2ms into the tap (attack must land NOW), ~1
	 * output period back out to the shifter chain ---- */
	float target = sh->mode == SH_MODE_GRAINS ? 1.0f : 0.0f;
	float step = target > sh->grainMix
		? 1.0f / (T > 96.0f ? T : 96.0f)
		: 1.0f / (0.002f * v->srate);
	if(sh->grainMix < target) {
		sh->grainMix += step;
		if(sh->grainMix > target) sh->grainMix = target;
	} else if(sh->grainMix > target) {
		sh->grainMix -= step;
		if(sh->grainMix < target) sh->grainMix = target;
	}
	float gG = sqrtf(sh->grainMix), gT = sqrtf(1.0f - sh->grainMix);
#ifdef HG_PROBE
	hgTap((int)(v - gV), p, chain, tap, gG * chain + gT * tap);
#endif
	return gG * chain + gT * tap;
}
