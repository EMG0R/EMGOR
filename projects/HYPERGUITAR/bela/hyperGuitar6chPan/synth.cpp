/*
 * synth.cpp - synthOsc timbres + the MONO/POLY synth-lead logic.
 *
 * REWORK 2026-11-05 (owner: synth dynamics must follow the string "super
 * deeply", more confident notes, fewer false triggers, longer graceful
 * sustain). Supersedes the velocity-held gate described below:
 *  - AMPLITUDE follows the string continuously: pitch-synchronous RMS of the
 *    tracker's band-limited signal (gBandLvl, pitch_tracker.cpp), normalised
 *    by a slow per-string reference, one-pole smoothed, amp = AMP_MAX *
 *    (lvl/ref)^SYN_GAMMA. Velocity now only sets static brightness.
 *  - SUSTAIN (gSynSustain, HGSET syn_sustain): the amplitude may rise
 *    instantly but falls no faster than an exponential cap (off at 0).
 *  - NOTE END: amp 60 dB under reference, or damp evidence -> exponential
 *    fade, tau 200 ms. No linear ramps, no peak-relative cut-off.
 *  - NOTE-ON GATING: neighbour-bleed ghost test, double-trigger test; every
 *    verdict is logged ("syn sN ON/OFF ...", via the ShDiag ring).
 *
 * REWORK 2026-10-01 (owner complaints: notes died after ~3 s, late
 * note-on, stray notes on fret moves, "aggressive attack then quieter",
 * no dynamics):
 *
 *  - ENGINE (synStep/synHop, shared by poly and mono): a note is a
 *    VELOCITY-HELD gate, not a copy of the string envelope.
 *      NOTE-ON   as soon as the tracker has accepted the post-pick pitch
 *                (attackBlank==0, pitchConf>=0.90, dbgGate not 1/2, not
 *                bled over by a louder string) for SYN_ACCEPT_HOPS hops.
 *                No 14-hop tonalHops wait.
 *      VELOCITY  peak of v->env over the first SYN_VEL_WIN_SEC after the
 *                pick, normalised by a slow per-string reference (decays
 *                ~60 s, floored) so quiet plain strings reach full
 *                velocity, then ^SYN_VEL_CURVE.
 *      HOLD      amplitude = velocity (x optional pressure follower)
 *                until a note-off condition fires:
 *                 a) env < max(floor, 40 dB under the note's own peak)
 *                    for 4 hops          (string really dead)
 *                 b) damped/released string for 2 hops: tracker
 *                    releaseBlank/dbgGate==1, or the synth's own fast
 *                    detector (envS fell to <75% in 4 hops)
 *                 c) tracker dbgGate==2 (silence / pure bleed) for 6 hops
 *                then a SYN_RELEASE_SEC linear ramp.
 *                floor = clamp(gSynthGateThr * 0.15, 0.0008, 0.006), so
 *                the existing AUX-click gate points 0.004/0.010/0.025 =
 *                0.0008 (longest tails) / 0.0015 (default) / 0.00375.
 *      LEGATO    pitch change >= 0.7 st with 3 agreeing hops while held
 *                (no onset) snaps pitch, SAME velocity, no new attack.
 *                Pitch targets from the tracker are ignored while
 *                releaseBlank>0 / dbgGate==1 (fret release / palm mute)
 *                and single-hop glitches never move the pitch at all.
 *  - TIMBRES: no attack-then-settle behaviour anywhere (filter-env
 *    snaps, decaying FM index, sweeps removed -> static). Slow LFO
 *    movement kept. Velocity sets brightness statically per note.
 *  - Modes are ordered darkest -> brightest by measured steady-state
 *    spectral centroid (table above SYNTH_NAMES in render.cpp).
 *
 * All new per-string state is file-static here (hyperguitar.h is not
 * touched). No allocation, no syscalls.
 */
#include "hyperguitar.h"
#include <cmath>
#include <cstring>

/* ---------------------------- tunables ---------------------------- */
#define SYN_ACCEPT_CONF    0.90f   /* pitchConf needed for note-on (pitch consensus, unchanged) */
#define SYN_ACCEPT_HOPS    2       /* consecutive good hops (accept hop + 1) */
#define SYN_PEND_MAX_HOPS  40      /* give up on an onset that never resolves */
#define SYN_BLEED_REL      0.20f   /* env must exceed this x loudest other string (was 0.35 = -9 dB, which
                                      vetoed a real soft pick 12 dB under a hard neighbour; the level-
                                      based ghost test below does the real bleed rejection now) */
#define SYN_LEG_ST         0.7f    /* legato retrigger threshold, semitones */
#define SYN_LEG_HOPS       3       /* consensus hops for legato */
#define SYN_VEL_WIN_SEC    0.015f  /* velocity measuring window (now only drives static brightness) */
#define SYN_VEL_REF_INIT   0.08f   /* per-string velocity reference at boot */
#define SYN_VEL_REF_FLOOR  0.008f
#define SYN_VEL_REF_DECAY  0.99995f/* per hop (~60 s time constant) */
#define SYN_VEL_CURVE      0.7f    /* vel = (peak/ref)^curve */
#define SYN_VEL_MIN        0.10f
#define SYN_AMP_MAX        0.60f   /* amplitude at the string's reference level (was env*2.0) */
#define SYN_ATTACK_SEC     0.003f  /* rise smoothing of the final amplitude (note-on click guard) */
#define SYN_FALL_SEC       0.003f  /* fall smoothing of the final amplitude (re-level / note end click guard) */
#define SYN_VEL_BRIGHT     0.30f   /* velocity -> brightness amount (0..1) */
#define SYN_GLIDE          0.02f   /* held-note pitch glide per sample */

/* --- 2026-11-05 continuous dynamics --- */
#define SYN_GAMMA          1.0f    /* amp = AMP_MAX*(lvl/ref)^gamma; 1.0 = the synth is exactly as loud
                                      (in dB) as the string. <1 compresses, >1 expands. */
#define SYN_LVL_SMOOTH_SEC 0.003f  /* one-pole on the hop-rate level target: removes zipper from the
                                      2.9 ms level steps without adding audible lag (spec 3-5 ms) */
#define SYN_LREF_INIT      0.030f  /* per-string level reference at boot (band-limited RMS units) */
#define SYN_LREF_FLOOR     0.004f  /* reference never below this: a dying string can't be normalised up to full */
#define SYN_LREF_DECAY     0.99995f/* per hop (~60 s): a one-off hard pick does not mute the string forever.
                                      Normalises plain strings (4.6 dB quieter) to the wound ones. */
#define SYN_NOISE_FRAC     0.2f    /* band level below frac*synFloor() is treated as 0 so converter noise
                                      cannot hold a note open at -50 dB */
#define SYN_END_REL        0.001f  /* note ends when the (limited) amp is 60 dB under the reference */

/* --- sustain limiter / fades --- */
#define SYN_SUS_FAST_DBS   60.0f   /* fall-rate cap at sustain->0+ (dB/s); at sustain==0 the limiter is off */
#define SYN_SUS_SLOW_DBS   1.5f    /* fall-rate cap at sustain 1 (dB/s) */
#define SYN_SUS_OFF_BELOW  0.005f  /* sustain under this = exact string match, no limiter */
#define SYN_FADE_TAU_SEC   0.200f  /* damped string: exponential fade time constant */
#define SYN_OFF_DAMP_HOPS  2       /* hops of damp evidence before the fade starts */
#define SYN_DAMP_RATIO     0.25f   /* band level fell to this x its value 4 hops (12 ms) ago = -12 dB in
                                      12 ms; a natural decay is ~1 dB there. Replaces the 75%/4-hop envS
                                      detector that fired on ordinary decay (tracker releaseBlank cannot
                                      fire: env is a 0.9995 peak-hold, it falls at most 0.19 dB/ms). */
#define SYN_DAMP_AGE_HOPS  4       /* damp detector armed this long after note-on (attack settling) */

/* --- ghost / double-trigger rejection (note-on gating only) --- */
#define SYN_GHOST_DT_MS    10.0f   /* a neighbour onset within +-this makes an onset suspect */
#define SYN_CPL_ADJ        0.100f  /* amplitude coupling, adjacent string: -20 dB (measured -23) */
#define SYN_CPL_NEXT       0.0251f /* next-but-one: -32 dB (measured -37) */
#define SYN_CPL_FAR        0.0100f /* further: -40 dB */
#define SYN_GHOST_MARGIN   2.0f    /* own level rise must exceed this x predicted bleed */
#define SYN_PRE_HOPS       8       /* pre-onset level = min of band level over this many hops (23 ms) */
#define SYN_DBL_WIN_HOPS   6       /* level-jump measuring window after an onset (~15-17 ms) */
#define SYN_DBL_JUMP       1.4125f /* +3 dB re-rise needed to re-attack a sounding same-pitch note */
#define SYN_DBL_PITCH_ST   0.5f    /* "same pitch" tolerance, semitones */
#define SYN_DBL_MIN_SEC    0.030f  /* min spacing of accepted onsets unless the level re-rose */
#define SYN_DBL_MIN_PERIODS 3.0f   /* ...or this many periods, whichever is longer */

enum { SYN_W_ACCEPTED = 0, SYN_W_GHOST, SYN_W_DBL, SYN_W_TIMEOUT, SYN_W_FLOOR, SYN_W_DAMP };
const char *synWhyName(int why)
{
	static const char *const n[] = { "accepted", "ghost", "dbl", "timeout", "floor", "damp" };
	return (why >= 0 && why < 6) ? n[why] : "?";
}

/* persisted mix param syn_sustain (hg_mon.cpp). 0 = synth follows the
 * string's level exactly (owner default); 1 = falls at most ~1.5 dB/s. */
float gSynSustain = 0.0f;

/*
 * synthOsc: one sample of the selected synth timbre. Shared by MONO
 * (one global voice) and POLY (six independent per-string voices) -
 * same code path, so every timbre works identically in both modes.
 * inc = cycles per sample. All saw/square use polyBLEP (alias-free).
 */
static inline float synthOsc(int mode, float *ph, float inc, float lfo,
		float *filt, float sr, float brt)
{
	switch(mode) {
	case 0: { // DRIFT PAD - three sines, one slowly drifting, an octave air
		ph[0] += inc;                                   if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		ph[1] += inc * (1.003f + 0.002f * lfo);          if(ph[1] >= 1.0f) ph[1] -= 1.0f;
		ph[2] += inc * 2.001f;                           if(ph[2] >= 1.0f) ph[2] -= 1.0f;
		return (sinf(6.2831853f * ph[0]) + sinf(6.2831853f * ph[1])
			+ 0.3f * sinf(6.2831853f * ph[2])) * 0.45f;
	}
	case 1: { // GLASS - sine with a whisper of 2nd and 3rd, organ-soft
		ph[0] += inc; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		float p = 6.2831853f * ph[0];
		return (sinf(p) + 0.22f * sinf(2.0f * p) + 0.07f * sinf(3.0f * p)) * 0.75f;
	}
	case 2: { // RINGMOD BELL - carrier x irrational-ratio modulator.
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
	case 3: { // SUB GROWL - fundamental + sub-octave sine through a
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
	case 4: { // SOFT SQUARE - band-limited square, two gentle lowpasses
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
	case 5: { // WARM TRI - two triangles 4 cents apart, rounded off
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
	case 6: { // WAVEFOLDER - sine through a tanh wavefolder, fold depth
		  // breathing on the slow LFO (continuous movement, no attack). West Coast/Buchla character:
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
	case 7: { // MOOG LEAD - one fat saw through a resonant 2-pole
		  // lowpass, filter opening held static (velocity-set).
		float d = inc;
		ph[0] += d; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		float saw = 2.0f * ph[0] - 1.0f - pblep(ph[0], d);
		// STATIC filter opening (no pick-snap decay): velocity sets
		// the brightness once per note via brt.
		float fc = inc * sr * (2.5f + 9.0f * 0.35f) * brt;
		if(fc > 0.45f * sr) fc = 0.45f * sr;
		if(fc < 40.0f) fc = 40.0f;
		float f = 2.0f * sinf((float)M_PI * fc / sr);
		if(f > 1.2f) f = 1.2f;
		filt[0] += f * filt[1];
		float hi = saw - filt[0] - 0.35f * filt[1];
		filt[1] += f * hi;
		return filt[0] * 1.6f;
	}
	case 8: { // ACID SQUELCH - 303-style pulse through a NEAR-SELF-
		  // OSCILLATING lowpass, fast filter-env sweep - the classic
		  // squelchy acid bite.
		float d = inc;
		ph[0] += d; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		float pw = 0.35f;
		float sq = ph[0] < pw ? 1.0f : -1.0f;
		sq += pblep(ph[0], d);
		float tp = ph[0] - pw; if(tp < 0.0f) tp += 1.0f;
		sq -= pblep(tp, d);
		// STATIC sweep position (was a decaying filter env)
		float fc = inc * sr * (1.5f + 14.0f * 0.35f) * brt;
		if(fc > 0.45f * sr) fc = 0.45f * sr;
		if(fc < 40.0f) fc = 40.0f;
		float f = 2.0f * sinf((float)M_PI * fc / sr);
		if(f > 1.2f) f = 1.2f;
		filt[0] += f * filt[1];
		float hi = sq - filt[0] - 0.12f * filt[1];  // very low damping = high resonance
		filt[1] += f * hi;
		return filt[0] * 1.1f;
	}
	case 9: { // FM GROWL - 2:1 ratio, constant index set by velocity -
		  // loud pick = brighter snarl, soft = mellower, held steady.
		ph[0] += inc; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		ph[1] += inc * 2.0f; if(ph[1] >= 1.0f) ph[1] -= 1.0f;
		// STATIC index, velocity-scaled once per note (was decaying)
		float idx = (1.0f + 2.5f) * brt;
		float m = sinf(2.0f * (float)M_PI * ph[1]);
		return sinf(2.0f * (float)M_PI * ph[0] + idx * m) * 0.85f;
	}
	case 10: { // FORMANT VOWEL - a pulse source through TWO real
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
	case 11: { // SUPERSAW - six saws spread +-19 cents, softened top
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
	case 12: { // CHIP LEAD - stepped duty-cycle pulse, deliberately raw
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
	case 13: { // SAW LEAD - 2 saws, ~7 cents (the one that already works)
		float d1 = inc, d2 = inc * 1.004f;
		ph[0] += d1; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		ph[1] += d2; if(ph[1] >= 1.0f) ph[1] -= 1.0f;
		float s1 = 2.0f * ph[0] - 1.0f - pblep(ph[0], d1);
		float s2 = 2.0f * ph[1] - 1.0f - pblep(ph[1], d2);
		return 0.5f * (s1 + s2);
	}
	case 14: { // SYNC LEAD - a slave oscillator hard-resets on every
		  // master cycle; the slave:master ratio is held, drifting
		  // slowly on the LFO.
		float dm = inc;
		float prevPh = ph[0];
		ph[0] += dm; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		int reset = ph[0] < prevPh;
		// STATIC ratio, slow LFO drift for movement (was a decaying sweep)
		float ratio = (1.6f + 3.0f * 0.35f + 0.25f * lfo) * (0.85f + 0.15f * brt);
		if(reset) ph[1] = 0.0f;
		float ds = dm * ratio;
		ph[1] += ds; if(ph[1] >= 1.0f) ph[1] -= 1.0f;
		float saw = 2.0f * ph[1] - 1.0f - pblep(ph[1], ds);
		return saw * 0.75f;
	}
	}
	return 0.0f;
}


/* ======================= analog voice layer (2026-11-05) =======================
 * 8 new analog-style patches (modes 0..7) and a shared filter stage used by
 * EVERY patch. The old 15 patches keep their synthOsc() code untouched and are
 * now modes 8..22 (synthOsc(mode - SYN_NNEW)).
 *
 * Cutoff of the shared filter, per voice:
 *     fc = base x keytrack x velocity-brightness x 2^( -N x (1 - level) + drift )
 *   keytrack = (f0 / 196 Hz)^kt   (kt 0..1, per patch)
 *   level    = the engine's continuous level, pAmp / AMP_MAX, 0..1
 *   N        = SYN_FMOD_OCT x gSynFmod (the pot / HGSET syn_fmod), 0..4 octaves
 * i.e. `base` is where the filter sits at the string's reference level and a
 * quieter string closes it by up to N octaves (louder = brighter, an analog
 * "velocity to cutoff"). At N = 0 the filter does not move with level at all.
 * Old patches: base = wide open, and the filter is bypassed (bit-exact) when
 * gSynFmod is ~0, cross-faded in over the first 5 % of travel.
 * fc is recomputed every SYN_UPD samples, one-pole smoothed (~2 ms) and the
 * filter coefficient is linearly interpolated per sample, so neither the pot nor
 * the level zippers.
 */
float gSynFmod = 0.5f;          /* persisted mix param syn_fmod, 0..1 (hg_mon.cpp), pot while SYNTH is armed */

#define SYN_NNEW           8        /* new patches at modes 0..7; old patches = mode - 8 */
#define SYN_FMOD_OCT       4.0f     /* syn_fmod 1.0 = level opens the filter by up to 4 octaves (owner spec) */
#define SYN_FREF_HZ        196.0f   /* keytrack reference pitch (G3, mid-range of the six strings) */
#define SYN_UPD            32       /* coefficient update interval, samples (0.73 ms): transcendentals amortised */
#define SYN_FC_SMOOTH_SEC  0.002f   /* cutoff one-pole time constant (owner spec ~2 ms) */
#define SYN_FC_MIN         30.0f    /* cutoff floor, Hz */
#define SYN_FC_MAXFRAC     0.40f    /* cutoff ceiling as a fraction of sr: ladder g stays < 0.92, SVF well away from Nyquist */
#define SYN_FM_BYPASS      0.002f   /* old patches: gSynFmod at or under this = filter bypassed (bit-exact old sound) */
#define SYN_FM_FULLWET     0.05f    /* old patches: filter fully in at this much pot travel (no click when it engages) */
#define SYN_OLD_FC_HZ      17000.0f /* old patches: cutoff at full level = wide open (just under the 0.4 sr ceiling) */
#define SYN_OLD_KT         0.5f     /* old patches: keytrack amount; base is scaled so C2 still sits at SYN_OLD_FC_HZ */
#define SYN_OLD_DAMP       1.2f     /* old patches: SVF damping k (~Butterworth 1.41 is -3 dB at fc; 1.2 is nearly flat) */
#define SYN_DRIFT_CENTS    4.0f     /* analog pitch drift scale: smoothed noise peaks +-2..4 cents */
#define SYN_DRIFT_HOLD_SEC 0.5f     /* drift: new random target every 0.5 s ... */
#define SYN_DRIFT_TAU_SEC  0.25f    /* ... one-pole tau 0.25 s => ~0.1-0.5 Hz wander */
#define SYN_DRIFT_CUT_OCT  0.12f    /* analog cutoff drift, +-octaves */
#define SYN_DRIFT_CUT_HOLD 1.5f     /* cutoff drift target interval, s (slower than pitch) */
#define SYN_DRIFT_CUT_TAU  0.6f     /* cutoff drift one-pole tau, s */
#define SYN_CENT           0.00057779f /* ln(2)/1200: 1 cent as a fractional frequency change (linear, exact enough < 10 c) */
#define SYN_SWELL_PARTIAL  0.35f    /* a re-pick on a sounding pad keeps this much of the bloom (never fully chops the pad) */
#define SYN_CHOR_N         512      /* per-voice chorus delay line, samples (power of 2; 11.6 ms at 44.1 kHz) */
#define SYN_TWO_PI         6.2831853f

struct SynDrift { float v, tgt; int left; unsigned int rng; };
struct AVoice {
	int init, mode, idle, upd, gen, fcInit;
	float ph[4];            /* oscillator phases */
	float lph[3];           /* slow LFO phases (PWM / vibrato / chorus) */
	float lad[7];           /* ladder y0..y3 + cached tanh(y0..y2) */
	float sv1, sv2;         /* TPT SVF state */
	float hpX, hpY;         /* one-pole highpass state (CS80) */
	float fc;               /* smoothed cutoff, Hz */
	float g, gStep, a1, a1Step;
	float keyF0, keyFac;
	float swell, swA;
	SynDrift dp[3], dc;     /* pitch drift (3 independent), cutoff drift */
	float pCoef, cCoef;
	int pHold, cHold;
	int w;
	float buf[SYN_CHOR_N];
};
static AVoice sAV[NUM_STRINGS + 1];   /* [6] = the MONO voice */

struct PFilt {
	signed char ladder;   /* 1 = 4-pole ladder, 0 = 2-pole SVF */
	float base;           /* cutoff Hz at f0 = 196 Hz and level 1 */
	float kt;             /* keytrack 0..1 */
	float k;              /* ladder feedback 0..~3.8 or SVF damping (lower = more resonance) */
	float drive;          /* pre-filter tanh drive (gentle saturation) */
	float lvlMul;         /* scales the level->cutoff octaves for this patch */
	float swCut;          /* 0..1: how much the bloom swell also opens the cutoff */
	float swellSec;       /* bloom time to ~90 % */
	float outGain;        /* output trim (calibrated to roughly SAW LEAD loudness) */
};
static const PFilt kPF[SYN_NNEW] = {
	/* ladder base    kt   k     drive lvlMul swCut swell  outGain */
	{ 1,     2600.0f, 0.6f, 0.9f, 1.0f, 1.0f, 0.4f, 0.18f, 1.55f },  /* 0 JUNO PAD */
	{ 0,     2200.0f, 0.7f, 1.1f, 1.2f, 1.0f, 0.4f, 0.25f, 0.85f },  /* 1 OB PAD */
	{ 1,     3200.0f, 0.8f, 1.5f, 1.3f, 1.2f, 0.5f, 0.12f, 1.00f },  /* 2 PROPHET PAD */
	{ 0,     4200.0f, 0.5f, 1.0f, 1.0f, 0.6f, 0.3f, 0.35f, 1.80f },  /* 3 STRING MACH */
	{ 1,     3000.0f, 0.7f, 1.1f, 1.2f, 1.2f, 0.5f, 0.30f, 1.20f },  /* 4 CS80 PAD */
	{ 1,     4200.0f, 0.9f, 2.4f, 1.4f, 1.6f, 1.0f, 0.08f, 1.10f },  /* 5 ANALOG BRASS */
	{ 1,     2400.0f, 0.6f, 0.7f, 1.2f, 1.0f, 0.4f, 0.40f, 1.25f },  /* 6 DRIFT POLY */
	{ 1,      900.0f, 0.5f, 0.35f, 1.0f, 0.8f, 0.3f, 0.22f, 1.00f },  /* 7 WARM SUB PAD */
};

/* rational tanh, exact at 0 and +-3 (=+-1), monotone, ~6 flops */
static inline float synTanh(float x)
{
	if(x > 3.0f) return 1.0f;
	if(x < -3.0f) return -1.0f;
	float x2 = x * x;
	return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

static inline float synDriftStep(SynDrift &d, float coef, int hold)
{
	if(--d.left <= 0) {
		d.left = hold;
		d.rng = d.rng * 1664525u + 1013904223u;
		d.tgt = (float)(int)(d.rng >> 8) * (1.0f / 8388608.0f) - 1.0f;
	}
	d.v += coef * (d.tgt - d.v);
	return d.v;
}

static inline float synTri(float p) { float x = p - 0.5f; if(x < 0.0f) x = -x; return 4.0f * x - 1.0f; }

static inline float synPulse(float ph, float d, float pw)
{
	float s = ph < pw ? 1.0f : -1.0f;
	s += pblep(ph, d);
	float tp = ph - pw; if(tp < 0.0f) tp += 1.0f;
	s -= pblep(tp, d);
	return s;
}
static inline float synSaw(float ph, float d) { return 2.0f * ph - 1.0f - pblep(ph, d); }
static inline void synAdv(float &p, float d) { p += d; if(p >= 1.0f) p -= 1.0f; }

static void synAVInit(AVoice &av, int idx, float sr)
{
	memset(&av, 0, sizeof(av));
	av.init = 1;
	av.mode = -1;
	av.idle = 1;
	for(int k = 0; k < 3; k++) {
		av.dp[k].rng = 0x9E3779B9u * (unsigned int)(idx * 4 + k + 1);   /* per-string, per-osc seed: voices differ */
		av.dp[k].tgt = 0.0f; av.dp[k].left = 1 + (int)((float)((av.dp[k].rng >> 8) & 0xFFFF) * (1.0f / 65536.0f) * SYN_DRIFT_HOLD_SEC * sr);
	}
	av.dc.rng = 0x85EBCA6Bu * (unsigned int)(idx + 1); av.dc.left = 1;
	av.pCoef = 1.0f - expf(-1.0f / (SYN_DRIFT_TAU_SEC * sr));
	av.cCoef = 1.0f - expf(-1.0f / (SYN_DRIFT_CUT_TAU * sr));
	av.pHold = (int)(SYN_DRIFT_HOLD_SEC * sr);
	av.cHold = (int)(SYN_DRIFT_CUT_HOLD * sr);
	/* different LFO start phases per string so a six-voice chord does not breathe in lockstep */
	for(int k = 0; k < 3; k++) av.lph[k] = fmodf(0.173f * (float)(idx + 1) * (float)(k + 1), 1.0f);
}

/* note (re)starts from silence: clear audio state, keep drift/LFO phase */
static void synAVWake(AVoice &av, int mode, float sr)
{
	memset(av.lad, 0, sizeof(av.lad));
	av.sv1 = av.sv2 = 0.0f; av.hpX = av.hpY = 0.0f;
	av.swell = 0.0f; av.fcInit = 0; av.keyF0 = 0.0f; av.upd = 0;
	av.w = 0; memset(av.buf, 0, sizeof(av.buf));
	for(int k = 0; k < 4; k++) av.ph[k] = 0.0f;
	if(mode < SYN_NNEW)
		av.swA = 1.0f - expf(-2.3f / (kPF[mode].swellSec * sr));
}

/* recompute the smoothed cutoff + filter coefficients (every SYN_UPD samples) */
static inline void synAVUpdate(AVoice &av, const PFilt &P, float f0, float lvlC, float brt, float sr, float fcStart)
{
	if(av.keyF0 <= 0.0f || fabsf(f0 - av.keyF0) > 0.002f * av.keyF0) {
		av.keyF0 = f0;
		av.keyFac = exp2f(P.kt * log2f((f0 > 20.0f ? f0 : 20.0f) * (1.0f / SYN_FREF_HZ)));
	}
	float N = SYN_FMOD_OCT * gSynFmod;
	float oct = -N * P.lvlMul * (1.0f - lvlC) + SYN_DRIFT_CUT_OCT * av.dc.v;
	float fcT = P.base * av.keyFac * brt * exp2f(oct);
	float fmax = SYN_FC_MAXFRAC * sr;
	if(fcT > fmax) fcT = fmax;
	if(fcT < SYN_FC_MIN) fcT = SYN_FC_MIN;
	if(!av.fcInit) {
		/* fcStart > 0: begin there and glide to the target (old patch engaging from bypass = wide open) */
		av.fc = fcStart > 0.0f ? fcStart : fcT; av.fcInit = 1;
		float g0 = P.ladder ? 1.0f - expf(-SYN_TWO_PI * av.fc / sr) : tanf(3.14159265f * av.fc / sr);
		av.g = g0;
		av.a1 = P.ladder ? 0.0f : 1.0f / (1.0f + g0 * (g0 + P.k));
	} else {
		av.fc += (1.0f - expf(-(float)SYN_UPD / (SYN_FC_SMOOTH_SEC * sr))) * (fcT - av.fc);
	}
	float gT, aT = 0.0f;
	if(P.ladder) gT = 1.0f - expf(-SYN_TWO_PI * av.fc / sr);
	else { gT = tanf(3.14159265f * av.fc / sr); aT = 1.0f / (1.0f + gT * (gT + P.k)); }
	av.gStep = (gT - av.g) * (1.0f / (float)SYN_UPD);
	av.a1Step = (aT - av.a1) * (1.0f / (float)SYN_UPD);
}

/* one sample through the shared filter. ladder: tanh on the input and on every
 * stage (Huovilainen/Stilson-Smith style); SVF: TPT 2-pole lowpass. */
static inline float synAVFilter(AVoice &av, const PFilt &P, float x)
{
	av.g += av.gStep;
	av.a1 += av.a1Step;
	if(P.ladder) {
		float *s = av.lad, g = av.g;
		float u = synTanh(x - P.k * s[3]);
		s[0] += g * (u - s[4]);        s[4] = synTanh(s[0]);
		s[1] += g * (s[4] - s[5]);     s[5] = synTanh(s[1]);
		s[2] += g * (s[5] - s[6]);     s[6] = synTanh(s[2]);
		s[3] += g * (s[6] - synTanh(s[3]));
		return s[3] * (1.0f + 0.5f * P.k);   /* partial passband compensation for the resonance dip */
	}
	float g = av.g;
	float hp = (x - (P.k + g) * av.sv1 - av.sv2) * av.a1;
	float bp = g * hp + av.sv1; av.sv1 = g * hp + bp;
	float lp = g * bp + av.sv2; av.sv2 = g * bp + lp;
	return lp;
}

static inline float synChorTap(const AVoice &av, float d)
{
	float pos = (float)av.w - d;
	int i0 = (int)floorf(pos);
	float fr = pos - (float)i0;
	float a = av.buf[i0 & (SYN_CHOR_N - 1)], b = av.buf[(i0 + 1) & (SYN_CHOR_N - 1)];
	return a + fr * (b - a);
}

/*
 * One sample of any patch (0..22) including the shared filter stage.
 *   lvl = engine level 0..1 of AMP_MAX; gen = engine note counter (bloom restart)
 * Returns the unscaled oscillator signal; the caller multiplies by pAmp etc.
 */
static inline float synthVoiceSample(AVoice &av, int mode, float *ph, float *filt, float inc,
		float lfo, float sr, float brt, float lvl, int gen, int idx)
{
	if(!av.init) synAVInit(av, idx, sr);
	if(mode >= SYN_NNEW) {
		/* ---- old patch: unchanged oscillator + the shared SVF after it ---- */
		float y = synthOsc(mode - SYN_NNEW, ph, inc, lfo, filt, sr, brt);
		float fm = gSynFmod;
		if(!(fm > SYN_FM_BYPASS)) { av.fcInit = 0; return y; }
		static const PFilt oldP = { 0, SYN_OLD_FC_HZ * 1.7321f, SYN_OLD_KT, SYN_OLD_DAMP, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f };
		if(av.mode != mode) { av.mode = mode; av.fcInit = 0; }
		if(!av.fcInit) {            /* engaging from bypass: warm the SVF at the signal (lowpass state = input), open cutoff, glide down */
			av.sv1 = 0.0f; av.sv2 = y; av.upd = 0;
		}
		if(--av.upd <= 0) {
			av.upd = SYN_UPD;
			synAVUpdate(av, oldP, inc * sr, lvl, 1.0f, sr, SYN_FC_MAXFRAC * sr);
		}
		float f = synAVFilter(av, oldP, y);
		float wet = fm * (1.0f / SYN_FM_FULLWET);
		if(wet > 1.0f) wet = 1.0f;
		return y + wet * (f - y);
	}

	/* ---- new analog patch ---- */
	const PFilt &P = kPF[mode];
	if(lvl <= 1e-7f) { av.idle = 1; return 0.0f; }
	if(av.idle || av.mode != mode) {
		synAVWake(av, mode, sr);
		av.mode = mode; av.idle = 0; av.gen = gen;
	} else if(av.gen != gen) {
		av.gen = gen;                       /* re-pick on a sounding pad: bloom restarts partially */
		av.swell *= SYN_SWELL_PARTIAL;
	}
	av.swell += av.swA * (1.0f - av.swell);

	/* analog drift: slow smoothed noise, seeded per string */
	float dr0 = synDriftStep(av.dp[0], av.pCoef, av.pHold);
	float pm = 1.0f + SYN_DRIFT_CENTS * SYN_CENT * dr0;
	synDriftStep(av.dc, av.cCoef, av.cHold);
	float d = inc * pm;
	float src = 0.0f;

	switch(mode) {
	case 0: {   /* JUNO PAD: PWM pulse + sub, slow PWM sweep */
		av.lph[0] += 0.45f / sr; if(av.lph[0] >= 1.0f) av.lph[0] -= 1.0f;
		float pw = 0.5f + 0.30f * synTri(av.lph[0]);
		synAdv(av.ph[0], d);
		synAdv(av.ph[1], 0.5f * d);
		src = 0.55f * synPulse(av.ph[0], d, pw) + 0.28f * synPulse(av.ph[1], 0.5f * d, 0.5f);
		break; }
	case 1: {   /* OB PAD: two saws +-8 cents */
		float d1 = d * (1.0f + 8.0f * SYN_CENT), d2 = d * (1.0f - 8.0f * SYN_CENT);
		synAdv(av.ph[0], d1); synAdv(av.ph[1], d2);
		src = 0.5f * (synSaw(av.ph[0], d1) + synSaw(av.ph[1], d2));
		break; }
	case 2: {   /* PROPHET PAD: saw + slowly modulated pulse */
		av.lph[0] += 0.28f / sr; if(av.lph[0] >= 1.0f) av.lph[0] -= 1.0f;
		float pw = 0.5f + 0.12f * synTri(av.lph[0]);
		float d2 = d * (1.0f + 3.0f * SYN_CENT);
		synAdv(av.ph[0], d); synAdv(av.ph[1], d2);
		src = 0.5f * synSaw(av.ph[0], d) + 0.35f * synPulse(av.ph[1], d2, pw);
		break; }
	case 3: {   /* STRING MACH: 8' saw pair + 4' saw */
		float d1 = d * (1.0f + 3.0f * SYN_CENT), d2 = d * (1.0f - 3.0f * SYN_CENT), d3 = 2.0f * d;
		synAdv(av.ph[0], d1); synAdv(av.ph[1], d2); synAdv(av.ph[2], d3);
		src = 0.35f * (synSaw(av.ph[0], d1) + synSaw(av.ph[1], d2)) + 0.30f * synSaw(av.ph[2], d3);
		break; }
	case 4: {   /* CS80 PAD: two saws, slow vibrato */
		av.lph[0] += 0.7f / sr; if(av.lph[0] >= 1.0f) av.lph[0] -= 1.0f;
		float vb = 1.0f + 6.0f * SYN_CENT * synTri(av.lph[0]);
		float d1 = d * vb * (1.0f + 5.0f * SYN_CENT), d2 = d * vb * (1.0f - 5.0f * SYN_CENT);
		synAdv(av.ph[0], d1); synAdv(av.ph[1], d2);
		src = 0.5f * (synSaw(av.ph[0], d1) + synSaw(av.ph[1], d2));
		/* highpass half of the CS80 band shape: one pole at 0.35 x f0 */
		float hc = 1.0f / (1.0f + SYN_TWO_PI * (0.35f * d) );   /* fc/sr = 0.35 d */
		av.hpY = hc * (av.hpY + src - av.hpX); av.hpX = src; src = av.hpY;
		break; }
	case 5: {   /* ANALOG BRASS: two saws 3 cents apart */
		float d1 = d * (1.0f + 3.0f * SYN_CENT), d2 = d * (1.0f - 3.0f * SYN_CENT);
		synAdv(av.ph[0], d1); synAdv(av.ph[1], d2);
		src = 0.5f * (synSaw(av.ph[0], d1) + synSaw(av.ph[1], d2));
		break; }
	case 6: {   /* DRIFT POLY: three saws, each with its own heavy drift */
		float dd1 = synDriftStep(av.dp[1], av.pCoef, av.pHold), dd2 = synDriftStep(av.dp[2], av.pCoef, av.pHold);
		float d0 = inc * (1.0f + SYN_CENT * (-9.0f + 2.2f * SYN_DRIFT_CENTS * dr0));
		float d1 = inc * (1.0f + SYN_CENT * (2.2f * SYN_DRIFT_CENTS * dd1));
		float d2 = inc * (1.0f + SYN_CENT * (10.0f + 2.2f * SYN_DRIFT_CENTS * dd2));
		synAdv(av.ph[0], d0); synAdv(av.ph[1], d1); synAdv(av.ph[2], d2);
		src = 0.34f * (synSaw(av.ph[0], d0) + synSaw(av.ph[1], d1) + synSaw(av.ph[2], d2));
		break; }
	default: {  /* 7 WARM SUB PAD: triangle + square sub-octave */
		synAdv(av.ph[0], d); synAdv(av.ph[1], 0.5f * d);
		src = 0.7f * synTri(av.ph[0]) + 0.30f * synPulse(av.ph[1], 0.5f * d, 0.5f);
		break; }
	}

	/* coefficient update (also feeds the bloom into the cutoff) */
	if(--av.upd <= 0) {
		av.upd = SYN_UPD;
		float lvlC = lvl * (1.0f - P.swCut + P.swCut * av.swell);
		synAVUpdate(av, P, inc * sr, lvlC, brt, sr, 0.0f);
	}
	float y = synAVFilter(av, P, synTanh(src * P.drive)) * P.outGain;

	/* built-in chorus / ensemble (per voice, mono) */
	if(mode == 0) {         /* Juno chorus: two triangle-modulated taps in antiphase */
		av.lph[1] += 0.62f / sr; if(av.lph[1] >= 1.0f) av.lph[1] -= 1.0f;
		float m = synTri(av.lph[1]);
		av.buf[av.w & (SYN_CHOR_N - 1)] = y;
		float a = synChorTap(av, (0.0033f + 0.0014f * m) * sr);
		float b = synChorTap(av, (0.0033f - 0.0014f * m) * sr);
		av.w++;
		y = 0.5f * y + 0.25f * (a + b);
	} else if(mode == 3) {  /* Solina ensemble: 3 taps 120 degrees apart, slow + fast shimmer */
		av.lph[1] += 0.55f / sr; if(av.lph[1] >= 1.0f) av.lph[1] -= 1.0f;
		av.lph[2] += 6.2f / sr;  if(av.lph[2] >= 1.0f) av.lph[2] -= 1.0f;
		av.buf[av.w & (SYN_CHOR_N - 1)] = y;
		float acc = 0.0f;
		for(int k = 0; k < 3; k++) {
			float p1 = av.lph[1] + (float)k * 0.33333334f; if(p1 >= 1.0f) p1 -= 1.0f;
			float p2 = av.lph[2] + (float)k * 0.33333334f; if(p2 >= 1.0f) p2 -= 1.0f;
			acc += synChorTap(av, (0.0055f + 0.0018f * synTri(p1) + 0.00015f * synTri(p2)) * sr);
		}
		av.w++;
		y = 0.3f * y + 0.7f * (acc * 0.33333334f);
	}
	return y * av.swell;
}

static inline float synLvl(float amp)
{
	float l = amp * (1.0f / SYN_AMP_MAX);
	return l > 1.0f ? 1.0f : l;
}

/* ------------------------- per-string engine ------------------------- */
struct SynStr {
	int init;
	unsigned int onsetSeen;
	int on, pend, pendHops, confHops;
	float lastPer;
	int velLeft, velOpen, velLive;
	float vpeak, velNext, vel, ref;
	float targInc, pRef, chgPer;
	int dampHops, age, chgHops, snap, hIdx, fade;
	/* level / dynamics */
	float lref, xTgt, xs, xa, lim, susMul, fadeMul, lvlH[4];
	int susBypass;
	/* onset verdict bookkeeping */
	float preLvl, pendMax, pendBleed, pendOth;
	unsigned int lastAccOnset;
	int haveAcc;
	int noteGen;               /* ++ on every accepted note-on: restarts the pad bloom (analog layer only reads it) */
};
static SynStr sSt[NUM_STRINGS];
static unsigned char sStale[NUM_STRINGS] = { 1, 1, 1, 1, 1, 1 };
static int sMonoLast = -1;

static float synFloor(void)
{
	float f = gSynthGateThr * 0.15f;
	if(f < 0.0008f) f = 0.0008f;
	if(f > 0.006f) f = 0.006f;
	return f;
}

static float synVelCurve(float peak, float ref)
{
	if(ref < peak) ref = peak;
	if(ref < SYN_VEL_REF_FLOOR) ref = SYN_VEL_REF_FLOOR;
	float x = peak / ref;
	if(x > 1.0f) x = 1.0f;
	float v = powf(x, SYN_VEL_CURVE);
	return v < SYN_VEL_MIN ? SYN_VEL_MIN : v;
}

static void synReset(SynStr &q, const Voice *v, int s)
{
	(void)v;
	float keepRef = q.init ? q.ref : SYN_VEL_REF_INIT;
	float keepL = q.init ? q.lref : SYN_LREF_INIT;
	q = SynStr();
	q.init = 1;
	q.ref = keepRef;
	q.lref = keepL;
	q.vel = 0.5f;
	q.velNext = 0.5f;
	q.onsetSeen = gOnsetStamp[s];
	q.pRef = 1.0f;
	q.susMul = 1.0f;
	q.fadeMul = 1.0f;
	q.xa = 0.01f;
}

/* predicted bleed amplitude on string s from the other strings' band levels */
static void synOthers(int s, float *bleed, float *loud)
{
	float b = 0.0f, l = 0.0f;
	for(int o = 0; o < NUM_STRINGS; o++) {
		if(o == s) continue;
		int d = o > s ? o - s : s - o;
		float c = d == 1 ? SYN_CPL_ADJ : (d == 2 ? SYN_CPL_NEXT : SYN_CPL_FAR);
		float lv = gBandLvl[o];
		if(lv * c > b) b = lv * c;
		if(lv > l) l = lv;
	}
	*bleed = b;
	*loud = l;
}

/* ms to the nearest OTHER string's onset (either side); 9999 = none known */
static int synNearestOnsetMs(int s, unsigned int mine, float sr)
{
	float best = 9999.0f;
	for(int o = 0; o < NUM_STRINGS; o++) {
		if(o == s || gOnsetStamp[o] == 0u) continue;
		int d = (int)(mine - gOnsetStamp[o]);
		if(d < 0) d = -d;
		float ms = 1000.0f * (float)d / sr;
		if(ms < best) best = ms;
	}
	return best > 9999.0f ? 9999 : (int)best;
}

static void synEvOn(int s, int why, float T, float conf, float lvl, float oth, int dtN)
{
	ShDiagEv e;
	e.code = SHDIAG_SYN_ON; e.s = s;
	e.i[0] = why; e.i[1] = dtN; e.i[2] = e.i[3] = 0;
	e.f[0] = T; e.f[1] = conf; e.f[2] = lvl; e.f[3] = oth; e.f[4] = e.f[5] = 0.0f;
	shDiagPush(&e);
}

static void synEvOff(int s, int why, int ageMs)
{
	ShDiagEv e;
	e.code = SHDIAG_SYN_OFF; e.s = s;
	e.i[0] = why; e.i[1] = ageMs; e.i[2] = e.i[3] = 0;
	e.f[0] = e.f[1] = e.f[2] = e.f[3] = e.f[4] = e.f[5] = 0.0f;
	shDiagPush(&e);
}

/* verdict on a pitch-resolved onset: SYN_W_ACCEPTED / GHOST / DBL */
static int synVerdict(SynStr &q, const Voice *v, int dtN)
{
	float pre = q.preLvl > 1e-6f ? q.preLvl : 1e-6f;
	float rise = q.pendMax - q.preLvl;
	if(rise < 0.0f) rise = 0.0f;
	/* (a) ghost: a neighbour picked at the same moment and this string's own
	 * rise is no more than the bleed that neighbour should deposit here */
	if((float)dtN <= SYN_GHOST_DT_MS && rise < SYN_GHOST_MARGIN * q.pendBleed)
		return SYN_W_GHOST;
	int jump = q.pendMax >= SYN_DBL_JUMP * pre;
	/* (b) removed 2026-11-05: rejecting every same-pitch re-pick without a
	 * +3 dB jump ate equal-force tremolo picking (host: 1-3/6 re-attacks at
	 * 10-30 dB/s decay). Double triggers are <50 ms (live log), so the
	 * spacing rule below alone covers them. */
	/* (b2) too soon after the previous accepted onset */
	if(q.haveAcc && !jump) {
		float minS = SYN_DBL_MIN_SEC * v->srate;
		float per3 = SYN_DBL_MIN_PERIODS * v->period;
		if(per3 > minS) minS = per3;
		if((float)(q.onsetSeen - q.lastAccOnset) < minS)
			return SYN_W_DBL;
	}
	return SYN_W_ACCEPTED;
}

/* once per tracker hop (v->hopCountdown == HOP right after the hop) */
static void synHop(SynStr &q, Voice *v, int s)
{
	float floorA = synFloor();
	q.ref *= SYN_VEL_REF_DECAY;
	if(q.ref < SYN_VEL_REF_FLOOR) q.ref = SYN_VEL_REF_FLOOR;
	q.lref *= SYN_LREF_DECAY;
	if(q.lref < SYN_LREF_FLOOR) q.lref = SYN_LREF_FLOOR;

	/* velocity window bookkeeping (brightness only) */
	if(q.velOpen) {
		q.velNext = synVelCurve(q.vpeak, q.ref);
		if(q.velLive) q.vel = q.velNext;
		if(q.velLeft <= 0) {
			if(q.vpeak > q.ref) q.ref = q.vpeak;
			q.velNext = synVelCurve(q.vpeak, q.ref);
			if(q.velLive) q.vel = q.velNext;
			q.velOpen = 0;
		}
	}

	int released = (v->releaseBlank > 0 || v->dbgGate == 1);

	/* ---- continuous level target ---- */
	float lvlRaw = gBandLvl[s];
	float eff = lvlRaw - SYN_NOISE_FRAC * floorA;
	if(eff < 0.0f) eff = 0.0f;
	float lvlOld = q.lvlH[q.hIdx];
	q.lvlH[q.hIdx] = eff;
	q.hIdx = (q.hIdx + 1) & 3;
	if(q.on && eff > q.lref) q.lref = eff;
	{
		float r = eff / q.lref;
		if(r > 1.0f) r = 1.0f;
		if(SYN_GAMMA != 1.0f) r = powf(r, SYN_GAMMA);
		q.xTgt = SYN_AMP_MAX * r;
	}
	/* per-hop constants: smoothing, sustain limiter slope, fade slope */
	q.xa = 1.0f - expf(-1.0f / (SYN_LVL_SMOOTH_SEC * v->srate));
	{
		float sus = gSynSustain;
		if(!(sus == sus)) sus = 0.0f;
		if(sus < 0.0f) sus = 0.0f;
		if(sus > 1.0f) sus = 1.0f;
		q.susBypass = sus < SYN_SUS_OFF_BELOW;
		float rate = SYN_SUS_FAST_DBS * powf(SYN_SUS_SLOW_DBS / SYN_SUS_FAST_DBS, sus);
		q.susMul = powf(10.0f, -rate / (20.0f * v->srate));
		q.fadeMul = expf(-1.0f / (SYN_FADE_TAU_SEC * v->srate));
	}

	/* ---- pending note-on after a pick ---- */
	int justOn = 0;
	if(q.pend) {
		q.pendHops++;
		float bleed, loud;
		synOthers(s, &bleed, &loud);
		if(q.pendHops <= SYN_DBL_WIN_HOPS && lvlRaw > q.pendMax) q.pendMax = lvlRaw;
		if(bleed > q.pendBleed) q.pendBleed = bleed;
		if(loud > q.pendOth) q.pendOth = loud;
		int ok = (v->attackBlank == 0 && v->pitchConf >= SYN_ACCEPT_CONF
			&& v->dbgGate != 1 && v->dbgGate != 2
			&& v->releaseBlank == 0
			&& v->env > 2.0f * floorA
			&& v->env > SYN_BLEED_REL * gOtherMax[s]);
		if(ok && (q.confHops == 0 ||
		   fabsf(v->period - q.lastPer) < 0.03f * q.lastPer))
			q.confHops++;
		else
			q.confHops = ok ? 1 : 0;
		q.lastPer = v->period;
		if(q.confHops >= SYN_ACCEPT_HOPS) {
			q.pend = 0;
			int dtN = synNearestOnsetMs(s, q.onsetSeen, v->srate);
			int why = synVerdict(q, v, dtN);
			synEvOn(s, why, v->period, v->pitchConf, q.pendMax, q.pendOth, dtN);
			if(why == SYN_W_ACCEPTED) {
				q.on = 1;
				justOn = 1;
				q.vel = q.velNext;
				q.velLive = q.velOpen;
				q.dampHops = 0;
				q.fade = 0;
				q.age = 0;
				q.chgHops = 0;
				q.pRef = v->period;
				q.snap = 1;
				q.lim = q.xs;          /* start / re-level at the string's own level */
				q.lastAccOnset = q.onsetSeen;
				q.haveAcc = 1;
				q.noteGen++;
			}
		} else if(q.pendHops > SYN_PEND_MAX_HOPS) {
			q.pend = 0;
			int dtN = synNearestOnsetMs(s, q.onsetSeen, v->srate);
			float rise = q.pendMax - q.preLvl;
			int ghost = (float)dtN <= SYN_GHOST_DT_MS && rise < SYN_GHOST_MARGIN * q.pendBleed;
			synEvOn(s, ghost ? SYN_W_GHOST : SYN_W_TIMEOUT, v->period, v->pitchConf,
				q.pendMax, q.pendOth, dtN);
		}
	}

	if(q.on && !justOn) {
		q.age++;

		/* ---- damp evidence -> graceful exponential fade (limiter bypassed) ---- */
		int damped = (q.age >= SYN_DAMP_AGE_HOPS && v->attackBlank == 0
				&& lvlOld > 4.0f * SYN_NOISE_FRAC * floorA
				&& eff < SYN_DAMP_RATIO * lvlOld);
		q.dampHops = (released || damped) ? q.dampHops + 1 : 0;
		if(q.dampHops >= SYN_OFF_DAMP_HOPS)
			q.fade = 1;

		/* ---- pitch: follow slow bends, legato on jumps. Frozen while the
		 * tracker is not confident (decaying tail, damp): low confidence
		 * never retunes or ends the note. ---- */
		if(!released && v->attackBlank == 0 && v->dbgGate == 0
		   && v->pitchConf >= SYN_ACCEPT_CONF) {
			float dev = 17.3123f * logf(v->period / q.pRef);
			if(fabsf(dev) < SYN_LEG_ST) {
				q.pRef += 0.5f * (v->period - q.pRef);
				q.chgHops = 0;
			} else {
				if(q.chgHops > 0 && fabsf(17.3123f *
				   logf(v->period / q.chgPer)) < 0.5f)
					q.chgHops++;
				else {
					q.chgHops = 1;
					q.chgPer = v->period;
				}
				if(q.chgHops >= SYN_LEG_HOPS) {
					q.pRef = v->period;
					q.snap = 1;      /* legato: same velocity */
					q.chgHops = 0;
				}
			}
		} else {
			q.chgHops = 0;
		}
	}
	q.targInc = v->beta / q.pRef;
}

/*
 * One sample of engine for string s. amp/inc are the caller's smoothed
 * amplitude + oscillator increment (Voice pAmp/pInc or the mono globals).
 * Returns the velocity (0..1) for the static brightness map.
 */
static inline float synStep(SynStr &q, Voice *v, int s, float *amp, float *inc)
{
	if(gOnsetStamp[s] != q.onsetSeen) {
		q.onsetSeen = gOnsetStamp[s];
		q.pend = 1; q.pendHops = 0; q.confHops = 0;
		q.vpeak = 0.0f;
		q.velLeft = (int)(SYN_VEL_WIN_SEC * v->srate);
		q.velOpen = 1;
		q.velLive = !q.on;
		/* level before the pick: the minimum of the last few hop levels */
		float pre = 1e9f;
		for(int k = 1; k <= SYN_PRE_HOPS; k++) {
			float h = gBandHist[s][(gBandHistIdx[s] - k) & (BAND_HIST_N - 1)];
			if(h < pre) pre = h;
		}
		q.preLvl = pre;
		q.pendMax = 0.0f; q.pendBleed = 0.0f; q.pendOth = 0.0f;
	}
	if(q.velLeft > 0) {
		if(v->env > q.vpeak) q.vpeak = v->env;
		q.velLeft--;
	}
	if(v->hopCountdown == HOP)
		synHop(q, v, s);

	/* string level (smoothed) -> sustain limiter / fade -> amplitude */
	q.xs += q.xa * (q.xTgt - q.xs);
	if(q.on) {
		if(q.fade) {
			q.lim *= q.fadeMul;                 /* exponential, never linear */
			if(q.xs > q.lim) {                  /* string re-rose: not damped after all */
				q.fade = 0; q.dampHops = 0; q.lim = q.xs;
			}
		} else if(q.susBypass || q.xs >= q.lim) {
			q.lim = q.xs;                       /* rise is instant, sustain 0 is exact */
		} else {
			q.lim *= q.susMul;                  /* fall no faster than the cap */
			if(q.lim < q.xs) q.lim = q.xs;
		}
		if(q.lim < SYN_END_REL * SYN_AMP_MAX) {
			synEvOff(s, q.fade ? SYN_W_DAMP : SYN_W_FLOOR,
				(int)(1000.0f * (float)q.age * (float)HOP / v->srate));
			q.on = 0;
			q.fade = 0;
		}
	}
	float tgt = q.on ? q.lim : 0.0f;
	if(tgt > *amp)
		*amp += (1.0f / (SYN_ATTACK_SEC * v->srate)) * (tgt - *amp);
	else
		*amp += (1.0f / (SYN_FALL_SEC * v->srate)) * (tgt - *amp);
	if(*amp < 1e-7f) *amp = 0.0f;               /* no denormals */
	if(q.snap) {
		*inc = q.targInc;
		q.snap = 0;
	} else if(q.on && v->releaseBlank == 0 && v->dbgGate != 1) {
		*inc += SYN_GLIDE * (q.targInc - *inc);
	}
	/* gate closed / damped: pitch frozen */
	return q.vel;
}


static inline float synBright(float vel)
{
	return 1.0f - SYN_VEL_BRIGHT * (1.0f - vel);
}

// SYNTH POLY: this string drives its OWN oscillator at its own
// tracked fundamental - six independent voices, true polyphony
void runSynthPoly(Voice *v, int s, float *mixed)
{
	if(!(gSineAmtS > 0.002f && gSynthPoly)) {
		sStale[s] = 1;
		return;
	}
	sMonoLast = -1;
	SynStr &q = sSt[s];
	if(sStale[s] || !q.init) {
		synReset(q, v, s);
		v->pAmp = 0.0f;
		sStale[s] = 0;
	}
	float vel = synStep(q, v, s, &v->pAmp, &v->pInc);
	v->pLfo += 0.35f / v->srate;                 // ~0.35 Hz PWM sweep
	if(v->pLfo >= 1.0f) v->pLfo -= 1.0f;
	float lfo = sinf(2.0f * (float)M_PI * v->pLfo);
	int mode = gSynthMode;
	if(mode < 0 || mode >= SYNTH_NMODES) mode = 0;
	float syn = synthVoiceSample(sAV[s], mode, v->pPh, v->sFilt, v->pInc, lfo,
			v->srate, synBright(vel), synLvl(v->pAmp), q.noteGen, s)
			* v->pAmp * gSynthLevel * SINE_LEVEL;
	*mixed = *mixed * (1.0f - gSineAmtS) + syn * gSineAmtS;
}

// SYNTH MONO: one voice on the lead string (render.cpp picks it), same
// engine + timbres as poly.
void runSynthMono(float *outL, float *outR, float sr)
{
	if(!(gSineAmtS > 0.002f && !gSynthPoly)) {
		sMonoLast = -1;
		return;
	}
	int ls = gLeadStr;
	Voice *lv = &gV[ls];
	SynStr &q = sSt[ls];
	if(sMonoLast != ls) {
		if(sMonoLast >= 0 && sSt[sMonoLast].on) {
			/* lead hand-over while sounding: carry the note over */
			SynStr old = sSt[sMonoLast];
			q = old;
			q.onsetSeen = gOnsetStamp[ls];
			q.pend = 0;
			q.pRef = lv->period;
			q.snap = 1;
		} else {
			synReset(q, lv, ls);
			gLeadAmp = 0.0f;
		}
		sMonoLast = ls;
	}
	if(!q.init) synReset(q, lv, ls);
	float vel = synStep(q, lv, ls, &gLeadAmp, &gLeadInc);

	static float gLeadPh[6] = { 0, 0, 0, 0, 0, 0 };
	static float gLeadFilt[4] = { 0, 0, 0, 0 };
	static float gLeadLfo = 0.0f;
	gLeadLfo += 0.35f / sr;
	if(gLeadLfo >= 1.0f) gLeadLfo -= 1.0f;
	int mode = gSynthMode;
	if(mode < 0 || mode >= SYNTH_NMODES) mode = 0;
	float lead = synthVoiceSample(sAV[NUM_STRINGS], mode, gLeadPh, gLeadFilt, gLeadInc,
			sinf(2.0f * (float)M_PI * gLeadLfo), sr, synBright(vel),
			synLvl(gLeadAmp), q.noteGen, NUM_STRINGS);
	gLeadLPz += 0.55f * (lead - gLeadLPz); // bright tilt, ~6k
	lead = gLeadLPz * gLeadAmp * gSynthLevel * SINE_LEVEL;

	*outL = *outL * (1.0f - gSineAmtS) + lead * gSineAmtS;
	*outR = *outR * (1.0f - gSineAmtS) + lead * gSineAmtS;
}
