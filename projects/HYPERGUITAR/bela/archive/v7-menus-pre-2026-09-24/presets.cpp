/*
 * presets.cpp - preset application, encoder-driven manual param edits,
 * and persisted state save/load. Split out of the former monolithic
 * render.cpp. Bodies are byte-for-byte identical to the original.
 */
#include "hyperguitar.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

/*
 * ==================== v6 CATEGORY TABLE ====================
 *
 * (category, slot) -> EFFECT id + VOICING. This is the single source of
 * truth for the grid: trellis_ui.cpp renders through it and dispatches
 * through it, and applyEncoders() takes the effect id that comes back out.
 *
 * WHY a table at all: the v5 code hard-coded the pad->effect mapping in
 * three separate places (LED render, key dispatch, encoder arming). They
 * drifted - a pad could light gold while a different effect toggled. One
 * table, read by everybody, makes that class of bug impossible.
 *
 * WHY the slots carry p1/p2 (the bug this table exists to kill): v5 had
 * FOUR formant pads at ratios 0.75/0.90/1.10/1.30, each an instantly
 * recognisable vowel you could stab mid-phrase. v6 collapsed them into
 * one FORMANT pad sitting at the compiled default gFormantVal = 1.0 - and
 * a formant ratio of 1.0 is BIT-IDENTICAL to the dry signal. The pad lit
 * gold and did nothing. Same failure mode lurked on PITCH (0 semitones),
 * PAN (spread 0), SAG (depth 0) and friends.
 *
 * THE RULE, and it is not negotiable: EVERY slot below must be audible
 * the instant it is pressed. No unity ratios, no zero depths, no zero
 * wet. The encoders exist to move you off the voicing, not to find it.
 *
 * p1/p2 are the effect's primary/secondary parameters - the SAME axes the
 * main/aux encoders drive for that effect, so the knobs continue smoothly
 * from wherever the slot landed. See applySlotPreset() below for the
 * per-effect meaning; SLOT_KEEP leaves that axis at its live value.
 *
 * Layout convention within a category: variants of one effect are kept
 * adjacent and ordered gentle -> extreme, so a row of pads reads as a
 * single continuous gesture under your thumb.
 */
const FxSlot gCatSlot[NUM_CATEGORIES][SLOTS_PER_CAT] = {
	/*
	 * CAT_SPECTRAL - everything that reshapes the spectral envelope.
	 * Slots 0-3 are the v5 formant pads restored EXACTLY (FVAL[]/
	 * FGLIDE[] in bela/archive/v5-preset-era/presets.cpp); the user
	 * asked for these back by name, do not retune them. 4-6 extend the
	 * range past what v5 could reach, 7-9 are the env-driven vowel at
	 * three sensitivities, 10-11 are the resonant-body voices.
	 */
	{ { PAD_FORMANT, 0.75f,  0.06f,  "FORM 0.75"  },  /* v5 pad 0 */
	  { PAD_FORMANT, 0.90f,  0.08f,  "FORM 0.90"  },  /* v5 pad 1 */
	  { PAD_FORMANT, 1.10f,  0.10f,  "FORM 1.10"  },  /* v5 pad 2 */
	  { PAD_FORMANT, 1.30f,  0.07f,  "FORM 1.30"  },  /* v5 pad 3 */
	  /* deep chest voice, glide nearly at the slow limit so the vowel
	   * swims into place over ~200ms instead of snapping */
	  { PAD_FORMANT, 0.55f,  0.035f, "FORM DEEP"  },
	  /* small-throat / child, fast glide - up-shifted formants only read
	   * as "voice" if they arrive with the note */
	  { PAD_FORMANT, 1.60f,  0.30f,  "FORM HIGH"  },
	  /* same mild lift as 1.10 but at the SLOWEST glide (~290ms): the
	   * contrast between these two is the whole point of a glide knob */
	  { PAD_FORMANT, 1.18f,  0.012f, "FORM SILK"  },
	  /* FORMANT-WAH: p2 is the tilt bias. 0 = pure vowel, + = the vowel
	   * sweep also brightens (classic wah "opening"). */
	  { PAD_FWAH,    0.90f,  0.00f,  "FWAH SOFT"  },
	  { PAD_FWAH,    2.20f,  0.35f,  "FWAH MID"   },
	  { PAD_FWAH,    5.00f,  0.75f,  "FWAH WIDE"  },
	  { PAD_JAWARI,  0.90f, 120.0f,  "JAWARI"     },
	  { PAD_SITAR,   0.80f,  0.9992f,"SITAR"      } },

	/*
	 * CAT_PITCH - everything that moves or synthesises f0.
	 * Slots 0-3 are v5's applyPitchPreset() calls verbatim (-12/100,
	 * -5/50, -7/50, +12/100). Restored by explicit request.
	 *
	 * The 50% ones are HARMONY voicings - you need the dry note under
	 * the interval or it is a transposition, not a harmony. The 100%
	 * ones are transpositions and want the dry gone.
	 */
	{ { PAD_PITCH, -12.0f, 100.0f, "OCT DOWN"  },  /* v5 pad 4, "BASS" */
	  { PAD_PITCH,  -5.0f,  50.0f, "4TH DOWN"  },  /* v5 pad 5 */
	  { PAD_PITCH,  -7.0f,  50.0f, "5TH DOWN"  },  /* v5 pad 6 */
	  { PAD_PITCH,  12.0f, 100.0f, "OCT UP"    },  /* v5 pad 7 */
	  { PAD_PITCH,   7.0f,  50.0f, "5TH UP"    },
	  { PAD_PITCH,  -3.0f,  50.0f, "MIN 3RD DN"},
	  { PAD_PITCH,  -4.0f,  50.0f, "MAJ 3RD DN"},
	  /* octave up buried under the dry - the "shimmer" trick, where the
	   * upper octave reads as air rather than as a second guitar */
	  { PAD_PITCH,  12.0f,  35.0f, "SHIMMER"   },
	  /* fractional semitones = cents. +12c against 50% dry gives the
	   * slow chorus-beating of two nearly-tuned strings. applySlot-
	   * Preset splits this into the semitone and cents encoder axes. */
	  { PAD_PITCH,   0.12f, 50.0f, "DETUNE 12C"},
	  /* p1 = per-note attack seconds. 0.45s is a clear violin swell that
	   * still lands inside a bar; the pick attack is fully erased. */
	  { PAD_POG,     0.45f,  0.00f, "POG SWELL" },
	  /* p1 = SYNTH_NAMES index, p2 = level. Mode 0 SAW LEAD at unity is
	   * the one that tracks cleanest, so it is the pad you stab first. */
	  { PAD_SINE,    0.0f,   1.00f, "SAW LEAD"  },
	  /* p1 = carrier:modulator ratio (2.0 is on the quantized grid, so
	   * the pad lands on a detent either way), p2 = index. Index 1.2 is
	   * well past the "is it even on?" threshold. */
	  { PAD_FM,      2.00f,  1.20f, "FM GROWL"  } },

	/*
	 * CAT_MOD - time/amplitude/space modulation. Three voicings each of
	 * vibrato, chorus, pan and pluck-sag: gentle, signature, extreme.
	 * Nothing here is allowed a zero depth - a zero-depth LFO is the
	 * same silent-pad bug wearing a different hat.
	 */
	{ { PAD_VIB,      0.80f, 0.0008f, "VIB SUBTLE"  },
	  { PAD_VIB,      1.20f, 0.0018f, "VIB SINGER"  },
	  { PAD_VIB,      1.90f, 0.0035f, "VIB WIDE"    },
	  { PAD_CHORUS,   0.50f, 0.0045f, "CHOR LUSH"   },
	  { PAD_CHORUS,   1.00f, 0.0022f, "CHOR CLASSIC"},
	  { PAD_CHORUS,   2.20f, 0.0012f, "CHOR SHIMMER"},
	  { PAD_PAN,      0.60f, 0.00f,   "PAN NARROW"  },
	  { PAD_PAN,      1.60f, 0.25f,   "PAN WIDE"    },
	  /* dyn high: the strings fly outward on hard picking and collapse
	   * back to centre as the note decays */
	  { PAD_PAN,      1.20f, 0.80f,   "PAN FLYOUT"  },
	  { PAD_PLUCKSAG, 0.002f, 0.05f,  "SAG SUBTLE"  },
	  { PAD_PLUCKSAG, 0.006f, 0.10f,  "SAG BEND"    },
	  { PAD_PLUCKSAG, 0.010f, 0.28f,  "SAG WOBBLE"  } },

	/*
	 * CAT_FILTER - filters and resonators. Same gentle/signature/extreme
	 * shape. The feedback values are chosen well clear of their upper
	 * clamps: the top of gSusFb/gSympFb is effectively infinite sustain,
	 * which is a destination you steer to with the knob, not a place a
	 * pad should drop you without warning.
	 */
	{ { PAD_ENVF, 1.20f, 0.90f,   "WAH SOFT"   },
	  { PAD_ENVF, 2.20f, 2.00f,   "WAH VOCAL"  },
	  { PAD_ENVF, 4.50f, 5.00f,   "WAH SCREAM" },
	  { PAD_RES,  1.00f, 0.930f,  "RES SOFT"   },
	  { PAD_RES,  2.00f, 0.975f,  "RES RING"   },
	  { PAD_RES,  4.00f, 0.994f,  "RES METAL"  },
	  { PAD_SUS,  0.980f,  0.45f, "SUS SHORT"  },
	  { PAD_SUS,  0.9950f, 0.28f, "SUS HOLD"   },
	  { PAD_SUS,  0.9993f, 0.10f, "SUS EBOW"   },
	  { PAD_SYMP, 0.80f, 0.975f,  "SYMP SOFT"  },
	  { PAD_SYMP, 1.60f, 0.990f,  "SYMP HALL"  },
	  { PAD_SYMP, 3.00f, 0.9985f, "SYMP DRONE" } },
};

/* Short enough to survive any future 4-char OLED/log field. Index order
 * must match the CAT_* defines exactly - they index this array directly. */
const char *const gCatName[NUM_CATEGORIES] = {
	"SPECTRAL", "PITCH", "MOD", "FILTER"
};

/*
 * The unified engine IS the wet leg: pitch owns the wet % when active;
 * formant (including FORMANT-WAH's env-driven vowel) lives inside the
 * same engine, so it needs the wet leg fully open to be heard at all.
 */
void applyMixRouting(void)
{
	float wet = 0.0f;
	if(gPitchActive)
		wet = gPitchWetPct / 100.0f;
	else if(gFormantActive || gFWahOn)
		wet = 1.0f;
	gWetTarget = wet;
	gDryTarget = 1.0f - wet;
}

// Recomputes gBetaBase[]/dry-wet from the live encoder-set pitch state.
// Called on pad press (on/off) and on every encoder move while armed.
void applyPitch(void)
{
	// cents is always a fine-tune layered ON TOP of the semitone value -
	// clicking into "cents mode" just changes what the main knob nudges
	float semis = gPitchSemitones + gPitchCents / 100.0f;
	float beta = powf(2.0f, semis / 12.0f);
	for(int s = 0; s < NUM_STRINGS; s++)
		gBetaBase[s] = gPitchActive ? beta : 1.0f;
	applyMixRouting();
}

void applyFormant(void)
{
	float f = gFormantActive ? gFormantVal : 1.0f;
	for(int s = 0; s < NUM_STRINGS; s++)
		gFormantBase[s] = f;
	applyMixRouting();
}

/* ==================== SLOT VOICING APPLICATION ==================== */

/* One clamp helper so every case below reads as "axis, lo, hi" and no
 * case can accidentally clamp to a range applyEncoders() disagrees with. */
static inline float slotClamp(float v, float lo, float hi)
{
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

/* SLOT_KEEP is a sentinel far below every real parameter, so a simple
 * "is it near the sentinel" test is enough and cannot false-positive on
 * a legitimate value (the most negative real axis is gPogModDepth, -3). */
static inline int slotSet(float p)
{
	return p > (SLOT_KEEP * 0.5f);
}

/*
 * applySlotPreset: stamp a slot's voicing onto the live globals.
 *
 * CONTRACT WITH trellis_ui.cpp - order matters and getting it wrong
 * re-creates the silent-pad bug:
 *   1. toggle the effect's on/off flag FIRST
 *   2. call applySlotPreset() only when the flag came up ON
 *   3. the FxEntry apply hook may then run; it is idempotent with the
 *      apply*() calls made here, so a double call is harmless
 * Step 1 must precede step 2 because applyPitch()/applyFormant() read
 * gPitchActive/gFormantActive to decide whether to open the wet leg at
 * all - stamping a voicing while the flag is still 0 writes gBetaBase =
 * 1.0 and gWetTarget = 0, i.e. silence.
 *
 * p1 = the axis the MAIN encoder drives for this effect, p2 = the axis
 * the AUX encoder drives, with the identical clamps applyEncoders() uses.
 * That is the whole reason the axes are chosen this way: after a press,
 * turning a knob continues from the voicing instead of jumping.
 *
 * (STATE_MAGIC note for the header owner: nothing persisted changed
 * shape here - slots write the same globals PersistedState already
 * carries - so "HGS3" does NOT need another bump for this change.)
 */
void applySlotPreset(const FxSlot *s)
{
	if(!s || s->fx == FX_NONE)
		return;

	switch(s->fx) {

	/* ---------------- wet-leg effects: mix routing is life ---------- */
	case PAD_PITCH: {
		/*
		 * p1 is a SIGNED SEMITONE offset that may carry a fraction, so
		 * one number can express both "-7" and "+12 cents". Split it
		 * across the two encoder axes exactly the way applyEncoders()
		 * keeps them: whole semitones on the main knob, the remainder
		 * as cents (|frac| <= 0.5 => |cents| <= 50, inside the clamp).
		 */
		if(slotSet(s->p1)) {
			float semi = roundf(s->p1);
			float cents = (s->p1 - semi) * 100.0f;
			gPitchSemitones = slotClamp(semi, -12.0f, 12.0f);
			gPitchCents = slotClamp(cents, -50.0f, 50.0f);
			/* A pure-cents voicing wants the main knob in cents mode -
			 * a detune slot is useless if the next detent jumps you a
			 * whole semitone. Anything with a semitone component arms
			 * semitone mode, which is what an interval pad wants. */
			gPitchCentsMode = (gPitchSemitones == 0.0f
					&& gPitchCents != 0.0f) ? 1 : 0;
		}
		if(slotSet(s->p2)) {
			float wet = slotClamp(s->p2, 0.0f, 100.0f);
			/*
			 * THE BUG, guarded: gPitchWetPct IS the wet leg (see
			 * applyMixRouting). A slot that transposes the signal and
			 * then asks for 0% wet is a pad that does nothing at all.
			 * No table entry does this, but a future one must not be
			 * able to; 10% is quiet but unmistakably present.
			 */
			if(wet < 10.0f) wet = 10.0f;
			gPitchWetPct = wet;
		}
		applyPitch();          /* -> applyMixRouting(), opens the wet leg */
		break;
	}

	case PAD_FORMANT:
		if(slotSet(s->p1)) {
			float f = slotClamp(s->p1, 0.4f, 2.2f);
			/*
			 * THE ORIGINAL BUG, guarded at the source. The unified
			 * engine warps the spectral envelope by this RATIO, so
			 * 1.0 is arithmetically the identity - v6 shipped the
			 * FORMANT pad sitting exactly there and it was inaudible.
			 * Nudge anything inside a hair of unity out to a ratio
			 * that is at least a few cents of formant movement.
			 */
			if(fabsf(f - 1.0f) < 0.03f)
				f = (f < 1.0f) ? 0.97f : 1.03f;
			gFormantVal = f;
		}
		if(slotSet(s->p2)) {
			/* per-hop smoothing coefficient: LOW = slow glide. Same
			 * clamp as the aux encoder (which steps it negatively). */
			gFormantGlide = slotClamp(s->p2, 0.01f, 0.35f);
		}
		applyFormant();        /* -> applyMixRouting(), wet = 100% */
		break;

	case PAD_FWAH:
		if(slotSet(s->p1))
			gFWahSens = slotClamp(s->p1, 0.2f, 8.0f);
		if(slotSet(s->p2))
			gFormantTiltBias = slotClamp(s->p2, -1.0f, 1.0f);
		/*
		 * FORMANT-WAH lives inside the shifter's wet leg exactly like
		 * FORMANT does, so it is the third effect that is silent
		 * without mix routing. applyMixRouting() reads gFWahOn, which
		 * the caller has already set - see the contract comment.
		 */
		applyMixRouting();
		break;

	/* ---------------- post-mix effects: no routing needed ---------- */
	case PAD_FM:
		if(slotSet(s->p1))
			gFmRatioVal = slotClamp(s->p1, 0.1f, 8.0f);
		if(slotSet(s->p2))
			gFmIndexVal = slotClamp(s->p2, 0.02f, 3.0f);
		break;

	case PAD_SINE:
		if(slotSet(s->p1)) {
			/* p1 is a SYNTH_NAMES index, not a continuous axis - the
			 * main encoder walks it by whole steps and wraps. */
			int m = (int)(s->p1 + 0.5f);
			if(m < 0) m = 0;
			if(m > SYNTH_NMODES - 1) m = SYNTH_NMODES - 1;
			gSynthMode = m;
		}
		if(slotSet(s->p2))
			gSynthLevel = slotClamp(s->p2, 0.0f, 2.5f);
		break;

	case PAD_VIB:
		if(slotSet(s->p1))
			gVibRateScale = slotClamp(s->p1, 0.2f, 3.0f);
		if(slotSet(s->p2))
			gVibDepthSec = slotClamp(s->p2, 0.0f, 0.005f);
		break;

	case PAD_CHORUS:
		if(slotSet(s->p1))
			gChorRateScale = slotClamp(s->p1, 0.2f, 3.0f);
		if(slotSet(s->p2))
			gChorDepthSec = slotClamp(s->p2, 0.0f, 0.008f);
		break;

	case PAD_ENVF:
		if(slotSet(s->p1))
			gEnvfSens = slotClamp(s->p1, 0.2f, 6.0f);
		if(slotSet(s->p2))
			gEnvfQ = slotClamp(s->p2, 0.3f, 8.0f);
		break;

	case PAD_PLUCKSAG:
		if(slotSet(s->p1))
			gSagDepthSec = slotClamp(s->p1, 0.0f, 0.01f);
		if(slotSet(s->p2))
			gSagSettleSec = slotClamp(s->p2, 0.01f, 0.5f);
		break;

	case PAD_POG:
		if(slotSet(s->p1))
			gPogAttackSec = slotClamp(s->p1, 0.02f, 3.0f);
		if(slotSet(s->p2))
			gPogModDepth = slotClamp(s->p2, -3.0f, 3.0f);
		break;

	case PAD_RES:
		if(slotSet(s->p1))
			gResSens = slotClamp(s->p1, 0.2f, 6.0f);
		if(slotSet(s->p2))
			gResFb = slotClamp(s->p2, 0.80f, 0.998f);
		break;

	case PAD_SUS:
		if(slotSet(s->p1))
			gSusFb = slotClamp(s->p1, 0.90f, 0.9995f);
		if(slotSet(s->p2))
			gSusDamp = slotClamp(s->p2, 0.03f, 0.90f);
		break;

	case PAD_JAWARI:
		if(slotSet(s->p1))
			gJawBuzz = slotClamp(s->p1, 0.0f, 2.0f);
		if(slotSet(s->p2))
			gJawPitch = slotClamp(s->p2, 16.0f, 400.0f);
		break;

	case PAD_SYMP:
		if(slotSet(s->p1))
			gSympCoupling = slotClamp(s->p1, 0.0f, 4.0f);
		if(slotSet(s->p2))
			gSympFb = slotClamp(s->p2, 0.90f, 0.9990f);
		break;

	case PAD_SITAR:
		if(slotSet(s->p1))
			gSitarLevel = slotClamp(s->p1, 0.0f, 1.0f);
		if(slotSet(s->p2))
			gSitarRing = slotClamp(s->p2, 0.9950f, 0.9998f);
		break;

	case PAD_PAN:
		if(slotSet(s->p1))
			gPanSpread = slotClamp(s->p1, 0.0f, 2.0f);
		if(slotSet(s->p2))
			gPanDyn = slotClamp(s->p2, 0.0f, 1.0f);
		break;

	default:
		break;
	}

	if(s->label)
		rt_printf("slot: %s\n", s->label);
	gStateDirty = 1;   /* a voicing is state; survive the next power cycle */
}

/*
 * ===================== RETIRED v5 - DEAD CODE =====================
 *
 * applyPreset()/applyPitchPreset() were PRESET MODE: one-touch vibes,
 * 16 pads each independently toggleable except three families sharing a
 * global parameter set. v6 retired the dual-mode UI - trellis_ui.cpp no
 * longer calls applyPreset() from anywhere, and gPresetMode is pinned 0.
 *
 * These bodies are kept ONLY so the archived v5 sources still build if
 * anyone revives them; the `pad` argument here still means a v5 PHYSICAL
 * pad index, NOT a v6 effect id, so calling this from live code would
 * toggle the wrong effects. Reference copy + rationale:
 *     bela/archive/v5-preset-era/
 * Do not extend these. New behaviour goes in applyEncoders()/trellis_ui.
 */
static void applyPitchPreset(int pad, float semis, float wetPct)
{
	if(gPresetPitchSel == pad) {
		gPitchActive = 0;
		gPresetPitchSel = -1;
	} else {
		gPitchActive = 1;
		gPresetPitchSel = pad;
		gPitchSemitones = semis;
		gPitchCents = 0.0f;
		gPitchCentsMode = 0;
		gPitchWetPct = wetPct;
	}
	applyPitch();
}

/* RETIRED v5 - unreachable from the live UI. See the block above. */
void applyPreset(int pad)
{
	switch(pad) {
	case 0: case 1: case 2: case 3: {
		/*
		 * ROUND 3 - live now. Static, zero tilt bias, zero wobble,
		 * zero sag-linking; ratios pulled in closer to unity, glide
		 * slowed way down for a relaxed, langorous character. See
		 * git log for round 1/round 2 history.
		 */
		static const float FVAL[4]   = { 0.75f, 0.90f, 1.10f, 1.30f };
		static const float FGLIDE[4] = { 0.06f, 0.08f, 0.10f, 0.07f };
		if(gPresetFormantSel == pad) {
			gFormantActive = 0;
			gPresetFormantSel = -1;
			gFormantTiltBias = 0.0f;
			gPresetFormantWobble = 0;
		} else {
			gFormantActive = 1;
			gPresetFormantSel = pad;
			gFormantVal = FVAL[pad];
			gFormantGlide = FGLIDE[pad];
			gFormantTiltBias = 0.0f;
			gPresetFormantWobble = 0;
		}
		applyFormant();
		break;
	}
	case 4: applyPitchPreset(pad, -12.0f, 100.0f); break;  // BASS
	case 5: applyPitchPreset(pad,  -5.0f,  50.0f); break;
	case 6: applyPitchPreset(pad,  -7.0f,  50.0f); break;
	case 7: applyPitchPreset(pad,  12.0f, 100.0f); break;
	case 8:  // VIBRATO (radio w/ chorus)
		if(gPresetVibChorusSel == 0) {
			gVibOn = 0; gPresetVibChorusSel = -1;
		} else {
			gVibOn = 1; gChorusOn = 0; gPresetVibChorusSel = 0;
		}
		break;
	case 9:  // CHORUS (radio w/ vibrato)
		if(gPresetVibChorusSel == 1) {
			gChorusOn = 0; gPresetVibChorusSel = -1;
		} else {
			gChorusOn = 1; gVibOn = 0; gPresetVibChorusSel = 1;
		}
		break;
	case 10: gSusOn = !gSusOn; break;      // SUSTAINER
	case 11: gSineOn = !gSineOn; break;    // SYNTH
	case 12: gSagOn = !gSagOn; break;      // PLUCK SAG ("bends as you play")
	case 13:                               // AUTOWAH, locked down
		gEnvFOn = !gEnvFOn;
		if(gEnvFOn) { gEnvfSens = PRESET_ENVF_SENS; gEnvfQ = PRESET_ENVF_Q; }
		break;
	case 14:                               // FORMANT-WAH (auto), locked down
		gFWahOn = !gFWahOn;
		if(gFWahOn) gFWahSens = PRESET_FWAH_SENS;
		applyMixRouting();
		break;
	case 15: gSympOn = !gSympOn; break;    // SYMPATHETIC RESONANCE
	}
}

/*
 * PERSISTED STATE: remember every toggle/parameter across power cycles.
 *
 * STATE_MAGIC was bumped to 0x48475333 ("HGS3") for v6. That SILENTLY
 * DISCARDS every v5 save file, and that is intended, not an oversight:
 * the pad semantics changed underneath the data. In v5 the persisted
 * presetFormantSel/presetPitchSel were physical pad indices; in v6 the
 * same numbers are effect ids selected through gCatSlot[][]. Reloading a
 * v5 file would restore a coherent-looking but WRONG selection - gold
 * LEDs on pads whose effects are off. A rejected magic just falls back
 * to the compiled-in defaults, which is always safe.
 */
typedef struct {
	uint32_t magic;
	/* presetMode: RETIRED v5 tombstone. Still written/read so the record
	 * layout stays legible against archived dumps, but loadState() forces
	 * gPresetMode = 0 and ignores whatever is in the file. */
	int presetMode, presetFormantSel, presetPitchSel, presetVibChorusSel;
	int presetFormantDynamic, presetFormantWobble;
	int pitchActive, formantActive, fmActive;
	int vibOn, chorusOn, envFOn, fWahOn, sineOn, pogOn, resOn, susOn;
	int jawOn, sitarOn, panOn, sagOn, sympOn;
	float pitchSemitones, pitchCents; int pitchCentsMode; float pitchWetPct;
	float formantVal, formantGlide, formantTiltBias;
	float fmRatioVal, fmIndexVal; int fmQuantized;
	float vibDepthSec, vibRateScale;
	float chorDepthSec, chorRateScale;
	float envfSens, envfQ, fWahSens;
	float pogAttackSec, pogModDepth;
	float resSens, resFb, resGain;
	float susFb, susDamp, susLevel;
	float jawBuzz, jawPitch, jawSens;
	float sitarRing, sitarLevel, sitarBuzz;
	float sagDepthSec, sagSettleSec;
	float sympCoupling, sympFb;
	float panSpread, panDyn;
	int synthMode, synthPoly; float synthLevel, synthGateThr;
	int formantMode;
	int curCategory;     /* v6: which grid page was live at power-down */
} PersistedState;

void saveState(void)
{
	PersistedState st;
	memset(&st, 0, sizeof(st));
	st.magic = STATE_MAGIC;
	st.presetMode = 0;   // retired tombstone; never anything but 0 in v6
	st.presetFormantSel = gPresetFormantSel;
	st.presetPitchSel = gPresetPitchSel;
	st.presetVibChorusSel = gPresetVibChorusSel;
	st.presetFormantDynamic = 0; // retired field, kept for struct layout
	st.presetFormantWobble = gPresetFormantWobble;
	st.pitchActive = gPitchActive; st.formantActive = gFormantActive;
	st.fmActive = gFmActive;
	st.vibOn = gVibOn; st.chorusOn = gChorusOn; st.envFOn = gEnvFOn;
	st.fWahOn = gFWahOn; st.sineOn = gSineOn; st.pogOn = gPogOn;
	st.resOn = gResOn; st.susOn = gSusOn; st.jawOn = gJawOn;
	st.sitarOn = gSitarOn; st.panOn = gPanOn; st.sagOn = gSagOn;
	st.sympOn = gSympOn;
	st.pitchSemitones = gPitchSemitones; st.pitchCents = gPitchCents;
	st.pitchCentsMode = gPitchCentsMode; st.pitchWetPct = gPitchWetPct;
	st.formantVal = gFormantVal; st.formantGlide = gFormantGlide;
	st.formantTiltBias = gFormantTiltBias;
	st.fmRatioVal = gFmRatioVal; st.fmIndexVal = gFmIndexVal;
	st.fmQuantized = gFmQuantized;
	st.vibDepthSec = gVibDepthSec; st.vibRateScale = gVibRateScale;
	st.chorDepthSec = gChorDepthSec; st.chorRateScale = gChorRateScale;
	st.envfSens = gEnvfSens; st.envfQ = gEnvfQ; st.fWahSens = gFWahSens;
	st.pogAttackSec = gPogAttackSec; st.pogModDepth = gPogModDepth;
	st.resSens = gResSens; st.resFb = gResFb; st.resGain = gResGain;
	st.susFb = gSusFb; st.susDamp = gSusDamp; st.susLevel = gSusLevel;
	st.jawBuzz = gJawBuzz; st.jawPitch = gJawPitch; st.jawSens = gJawSens;
	st.sitarRing = gSitarRing; st.sitarLevel = gSitarLevel;
	st.sitarBuzz = gSitarBuzz;
	st.sagDepthSec = gSagDepthSec; st.sagSettleSec = gSagSettleSec;
	st.sympCoupling = gSympCoupling; st.sympFb = gSympFb;
	st.panSpread = gPanSpread; st.panDyn = gPanDyn;
	st.synthMode = gSynthMode; st.synthPoly = gSynthPoly;
	st.synthLevel = gSynthLevel; st.synthGateThr = gSynthGateThr;
	st.formantMode = gFormantMode;
	st.curCategory = gCurCategory;

	FILE *f = fopen(STATE_FILE ".tmp", "wb");
	if(!f)
		return;
	fwrite(&st, sizeof(st), 1, f);
	fclose(f);
	rename(STATE_FILE ".tmp", STATE_FILE);
}

void loadState(void)
{
	FILE *f = fopen(STATE_FILE, "rb");
	if(!f)
		return;                      // first boot ever - keep defaults
	PersistedState st;
	memset(&st, 0, sizeof(st));
	size_t n = fread(&st, sizeof(st), 1, f);
	fclose(f);
	if(n != 1 || st.magic != STATE_MAGIC)
		return;                      // no file, or an old/foreign format

	// RETIRED: st.presetMode is deliberately NOT read back. The dual-mode
	// UI is gone; a stale 1 in an old file would arm a mode with no way
	// out (the both-buttons escape combo was removed from render.cpp).
	gPresetMode = 0;
	gPresetFormantSel = st.presetFormantSel;
	gPresetPitchSel = st.presetPitchSel;
	gPresetVibChorusSel = st.presetVibChorusSel;
	gPresetFormantWobble = st.presetFormantWobble;
	gPitchActive = st.pitchActive; gFormantActive = st.formantActive;
	gFmActive = st.fmActive;
	gVibOn = st.vibOn; gChorusOn = st.chorusOn; gEnvFOn = st.envFOn;
	gFWahOn = st.fWahOn; gSineOn = st.sineOn; gPogOn = st.pogOn;
	gResOn = st.resOn; gSusOn = st.susOn; gJawOn = st.jawOn;
	gSitarOn = st.sitarOn; gSagOn = st.sagOn;
	// Stereo spread is ALWAYS ON - st.panOn is deliberately ignored (old
	// state files may carry a 0 from when the pad could toggle it off).
	// panSpread/panDyn still restore: they are the voicing, not the switch.
	gPanOn = 1;
	gSympOn = st.sympOn;
	gPitchSemitones = st.pitchSemitones; gPitchCents = st.pitchCents;
	gPitchCentsMode = st.pitchCentsMode; gPitchWetPct = st.pitchWetPct;
	gFormantVal = st.formantVal; gFormantGlide = st.formantGlide;
	gFormantTiltBias = st.formantTiltBias;
	gFmRatioVal = st.fmRatioVal; gFmIndexVal = st.fmIndexVal;
	gFmQuantized = st.fmQuantized;
	gVibDepthSec = st.vibDepthSec; gVibRateScale = st.vibRateScale;
	gChorDepthSec = st.chorDepthSec; gChorRateScale = st.chorRateScale;
	gEnvfSens = st.envfSens; gEnvfQ = st.envfQ; gFWahSens = st.fWahSens;
	gPogAttackSec = st.pogAttackSec; gPogModDepth = st.pogModDepth;
	gResSens = st.resSens; gResFb = st.resFb; gResGain = st.resGain;
	gSusFb = st.susFb; gSusDamp = st.susDamp; gSusLevel = st.susLevel;
	gJawBuzz = st.jawBuzz; gJawPitch = st.jawPitch; gJawSens = st.jawSens;
	gSitarRing = st.sitarRing; gSitarLevel = st.sitarLevel;
	gSitarBuzz = st.sitarBuzz;
	gSagDepthSec = st.sagDepthSec; gSagSettleSec = st.sagSettleSec;
	gSympCoupling = st.sympCoupling; gSympFb = st.sympFb;
	gPanSpread = st.panSpread; gPanDyn = st.panDyn;
	gSynthMode = st.synthMode; gSynthPoly = st.synthPoly;
	gSynthLevel = st.synthLevel; gSynthGateThr = st.synthGateThr;
	gFormantMode = st.formantMode;
	// Range-check the page: a corrupt or truncated record must not leave
	// gCurCategory indexing gCatSlot[][] out of bounds.
	gCurCategory = (st.curCategory >= 0 && st.curCategory < NUM_CATEGORIES)
			? st.curCategory : CAT_SPECTRAL;
}

/*
 * applyEncoders: routes live main/aux encoder deltas + click flags to
 * whichever effect is currently "armed" (gArmedPad, set by the last pad
 * press in trellisTick).
 *
 * v6: `pad` is an EFFECT id, not a physical pad position - trellis_ui
 * resolves the pressed pad through gCatSlot[gCurCategory][slot] before
 * calling us. The PAD_* values are unchanged, so every case below still
 * matches; only the meaning of the argument moved.
 *
 * CLICK CONVENTION (v6, was previously inconsistent):
 *   mainClick - primary MODE switch (what the main knob means, or the
 *               effect's engine choice). Already wired on PITCH, FM,
 *               FORMANT, SINE, SITAR.
 *   auxClick  - SECONDARY RATCHET: steps a third parameter through a
 *               few musically-chosen discrete values and wraps. It was
 *               plumbed all the way from render.cpp but never read by
 *               anything, so half the >2-param effects (RES gain, SUS
 *               level, JAWARI sens, SYNTH gate) had knobs the UI simply
 *               could not reach. Ratchet, not continuous, because a
 *               click has no direction - wrapping is the only sane way
 *               to make one button walk a range in both directions.
 *   SITAR is the one effect with no auxClick: its ratchet (buzz) is
 *   already on mainClick, and moving it would break muscle memory.
 */
void applyEncoders(int pad, int mainDelta, int auxDelta, int mainClick, int auxClick)
{
	switch(pad) {
	case PAD_ENVF:
		gEnvfSens += mainDelta * 0.05f;
		if(gEnvfSens < 0.2f) gEnvfSens = 0.2f;
		if(gEnvfSens > 6.0f) gEnvfSens = 6.0f;
		gEnvfQ += auxDelta * 0.1f;
		if(gEnvfQ < 0.3f) gEnvfQ = 0.3f;
		if(gEnvfQ > 8.0f) gEnvfQ = 8.0f;
		break;
	case PAD_RES:
		gResSens += mainDelta * 0.06f;    // MAIN: how far dynamics push the harmonic
		if(gResSens < 0.2f) gResSens = 0.2f;
		if(gResSens > 6.0f) gResSens = 6.0f;
		gResFb += auxDelta * 0.004f;      // AUX: how long the overtone rings
		if(gResFb < 0.80f) gResFb = 0.80f;
		if(gResFb > 0.998f) gResFb = 0.998f;
		if(auxClick) {                    // AUX CLICK: resonator output gain
			gResGain = (gResGain > 3.0f) ? 1.0f
				 : (gResGain > 1.6f) ? 3.5f : 2.2f;
			rt_printf("RES gain %.2f\n", gResGain);
		}
		break;
	case PAD_VIB:
		gVibRateScale += mainDelta * 0.02f;
		if(gVibRateScale < 0.2f) gVibRateScale = 0.2f;
		if(gVibRateScale > 3.0f) gVibRateScale = 3.0f;
		gVibDepthSec += auxDelta * 0.0002f;
		if(gVibDepthSec < 0.0f) gVibDepthSec = 0.0f;
		if(gVibDepthSec > 0.005f) gVibDepthSec = 0.005f;
		break;
	case PAD_CHORUS:
		gChorRateScale += mainDelta * 0.02f;
		if(gChorRateScale < 0.2f) gChorRateScale = 0.2f;
		if(gChorRateScale > 3.0f) gChorRateScale = 3.0f;
		gChorDepthSec += auxDelta * 0.0003f;
		if(gChorDepthSec < 0.0f) gChorDepthSec = 0.0f;
		if(gChorDepthSec > 0.008f) gChorDepthSec = 0.008f;
		break;
	case PAD_POG:
		gPogAttackSec += mainDelta * 0.05f;
		if(gPogAttackSec < 0.02f) gPogAttackSec = 0.02f;
		if(gPogAttackSec > 3.0f) gPogAttackSec = 3.0f;
		gPogModDepth += auxDelta * 0.1f;
		if(gPogModDepth < -3.0f) gPogModDepth = -3.0f;
		if(gPogModDepth > 3.0f) gPogModDepth = 3.0f;
		break;
	case PAD_PLUCKSAG:
		gSagDepthSec += mainDelta * 0.0002f;
		if(gSagDepthSec < 0.0f) gSagDepthSec = 0.0f;
		if(gSagDepthSec > 0.01f) gSagDepthSec = 0.01f;
		gSagSettleSec += auxDelta * 0.01f;
		if(gSagSettleSec < 0.01f) gSagSettleSec = 0.01f;
		if(gSagSettleSec > 0.5f) gSagSettleSec = 0.5f;
		break;
	case PAD_SYMP:
		gSympCoupling += mainDelta * 0.06f;  // MAIN: how hard the others ring it
		if(gSympCoupling < 0.0f) gSympCoupling = 0.0f;
		if(gSympCoupling > 4.0f) gSympCoupling = 4.0f;
		gSympFb += auxDelta * 0.003f;        // AUX: ring length
		if(gSympFb < 0.90f) gSympFb = 0.90f;
		if(gSympFb > 0.9990f) gSympFb = 0.9990f;
		break;
	case PAD_SUS:
		gSusFb += mainDelta * 0.002f;     // MAIN: hold time (top = infinite)
		if(gSusFb < 0.90f) gSusFb = 0.90f;
		if(gSusFb > 0.9995f) gSusFb = 0.9995f;
		gSusDamp += auxDelta * 0.02f;     // AUX: tone of the held note
		if(gSusDamp < 0.03f) gSusDamp = 0.03f;
		if(gSusDamp > 0.90f) gSusDamp = 0.90f;
		if(auxClick) {                    // AUX CLICK: how loud the hold sits
			gSusLevel = (gSusLevel > 2.5f) ? 0.9f
				  : (gSusLevel > 1.4f) ? 3.0f : 1.8f;
			rt_printf("SUS level %.2f\n", gSusLevel);
		}
		break;
	case PAD_JAWARI:
		gJawBuzz += mainDelta * 0.05f;    // MAIN: buzz amount
		if(gJawBuzz < 0.0f) gJawBuzz = 0.0f;
		if(gJawBuzz > 2.0f) gJawBuzz = 2.0f;
		gJawPitch += auxDelta * 6.0f;     // AUX: metallic pitch of the rattle
		if(gJawPitch < 16.0f) gJawPitch = 16.0f;
		if(gJawPitch > 400.0f) gJawPitch = 400.0f;
		if(auxClick) {                    // AUX CLICK: dynamic sensitivity
			gJawSens = (gJawSens > 3.0f) ? 1.0f
				 : (gJawSens > 1.5f) ? 4.0f : 2.0f;
			rt_printf("JAWARI sens %.2f\n", gJawSens);
		}
		break;
	case PAD_SITAR:
		gSitarLevel += mainDelta * 0.02f;   // MAIN: wet fraction (dry stays 100%)
		if(gSitarLevel < 0.0f) gSitarLevel = 0.0f;
		if(gSitarLevel > 1.0f) gSitarLevel = 1.0f;
		gSitarRing += auxDelta * 0.0002f;   // AUX: tarab ring length
		if(gSitarRing < 0.9950f) gSitarRing = 0.9950f;
		if(gSitarRing > 0.9998f) gSitarRing = 0.9998f;
		if(mainClick) {                      // click: more/less jawari buzz
			gSitarBuzz = (gSitarBuzz > 0.9f) ? 0.2f : gSitarBuzz + 0.35f;
			rt_printf("SITAR buzz %.2f\n", gSitarBuzz);
		}
		break;
	case PAD_FWAH:
		gFWahSens += mainDelta * 0.08f;       // MAIN: how hard the vowel swings
		if(gFWahSens < 0.2f) gFWahSens = 0.2f;
		if(gFWahSens > 8.0f) gFWahSens = 8.0f;
		/*
		 * AUX: formant-driven EQ tilt bias. FORMANT-WAH was the one
		 * effect with a dead second encoder - gFWahSens was its only
		 * global, so the aux knob did nothing at all.
		 *
		 * gFormantTiltBias is the right second axis rather than an
		 * arbitrary one: dsp_core.cpp computes the wet-leg tilt as
		 * log2(formant) * gFormantTiltBias, and FORMANT-WAH is exactly
		 * the thing that sweeps `formant` from the envelope. So this
		 * knob decides whether an env-driven vowel sweep also BRIGHTENS
		 * on the way up (+, the classic wah "opening" shine), stays
		 * spectrally flat (0, pure vowel colour), or darkens (-, a
		 * cocked-wah/vocal-tract-closing feel). Same engine, genuinely
		 * orthogonal control. Range [-1,+1], 0.05/detent.
		 */
		gFormantTiltBias += auxDelta * 0.05f;
		if(gFormantTiltBias < -1.0f) gFormantTiltBias = -1.0f;
		if(gFormantTiltBias > 1.0f) gFormantTiltBias = 1.0f;
		break;
	case PAD_PAN:
		gPanSpread += mainDelta * 0.05f;      // MAIN: spread (0 = mono)
		if(gPanSpread < 0.0f) gPanSpread = 0.0f;
		if(gPanSpread > 2.0f) gPanSpread = 2.0f;
		gPanDyn += auxDelta * 0.04f;          // AUX: dynamic fly-out
		if(gPanDyn < 0.0f) gPanDyn = 0.0f;
		if(gPanDyn > 1.0f) gPanDyn = 1.0f;
		break;
	case PAD_PITCH:
		if(mainClick) {
			gPitchCentsMode = !gPitchCentsMode;
			rt_printf("pitch mode: %s\n", gPitchCentsMode ? "cents" : "semitone");
		}
		if(gPitchCentsMode) {
			gPitchCents += (float)mainDelta * 2.0f; // 2 cents/detent, +-50
			if(gPitchCents < -50.0f) gPitchCents = -50.0f;
			if(gPitchCents > 50.0f) gPitchCents = 50.0f;
		} else {
			gPitchSemitones += (float)mainDelta; // 1 semitone/detent
			if(gPitchSemitones < -12.0f) gPitchSemitones = -12.0f;
			if(gPitchSemitones > 12.0f) gPitchSemitones = 12.0f;
		}
		gPitchWetPct += (float)auxDelta * 10.0f; // 10%/detent
		if(gPitchWetPct < 0.0f) gPitchWetPct = 0.0f;
		if(gPitchWetPct > 100.0f) gPitchWetPct = 100.0f;
		applyPitch();
		break;
	case PAD_FM:
		if(mainClick) {
			gFmQuantized = !gFmQuantized;
			rt_printf("fm mode: %s\n", gFmQuantized ? "quantized" : "free");
		}
		if(gFmQuantized) {
			static const float qSteps[6] = { 0.25f, 0.5f, 1.0f, 2.0f, 3.0f, 4.0f };
			int idx = 0;
			float best = 1e9f;
			for(int k = 0; k < 6; k++) {
				float d = fabsf(qSteps[k] - gFmRatioVal);
				if(d < best) { best = d; idx = k; }
			}
			idx += mainDelta;
			if(idx < 0) idx = 0;
			if(idx > 5) idx = 5;
			gFmRatioVal = qSteps[idx];
		} else {
			gFmRatioVal += (float)mainDelta * 0.05f;
			if(gFmRatioVal < 0.1f) gFmRatioVal = 0.1f;
			if(gFmRatioVal > 8.0f) gFmRatioVal = 8.0f;
		}
		gFmIndexVal += (float)auxDelta * 0.05f;
		if(gFmIndexVal < 0.02f) gFmIndexVal = 0.02f;
		if(gFmIndexVal > 3.0f) gFmIndexVal = 3.0f;
		break;
	case PAD_FORMANT:
		if(mainClick) {                            // click: GRAIN <-> LPC engine
			gFormantMode = !gFormantMode;
			rt_printf("FORMANT mode: %s\n", gFormantMode ? "LPC" : "GRAIN");
		}
		gFormantVal += (float)mainDelta * 0.02f;   // MAIN: vowel/filter position
		if(gFormantVal < 0.4f) gFormantVal = 0.4f;
		if(gFormantVal > 2.2f) gFormantVal = 2.2f;
		applyFormant();
		gFormantGlide += (float)auxDelta * -0.01f;  // AUX: glide speed
		if(gFormantGlide < 0.01f) gFormantGlide = 0.01f;  // ~290ms, silkiest
		if(gFormantGlide > 0.35f) gFormantGlide = 0.35f;  // ~7ms, snappiest
		break;
	case PAD_SINE:
		if(mainClick) {
			gSynthPoly = !gSynthPoly;
			rt_printf("SYNTH: %s\n", gSynthPoly ? "POLY (per-string)" : "MONO (loudest string)");
		}
		if(mainDelta) {
			int m = gSynthMode + mainDelta;
			while(m < 0) m += SYNTH_NMODES;
			m %= SYNTH_NMODES;
			gSynthMode = m;
			rt_printf("SYNTH mode %d = %s\n", m, SYNTH_NAMES[m]);
		}
		gSynthLevel += (float)auxDelta * 0.05f;
		if(gSynthLevel < 0.0f) gSynthLevel = 0.0f;
		if(gSynthLevel > 2.5f) gSynthLevel = 2.5f;
		if(auxClick) {                    // AUX CLICK: note-off gate threshold
			// low = long tails but chases room noise; high = tight but
			// chokes quiet notes. Three usable points, not a knob.
			gSynthGateThr = (gSynthGateThr > 0.018f) ? 0.004f
				      : (gSynthGateThr > 0.007f) ? 0.025f : 0.010f;
			rt_printf("SYNTH gate %.3f\n", gSynthGateThr);
		}
		// (release-time control removed - amplitude now follows the real
		// string envelope, so there's no artificial release to adjust)
		break;
	default:
		break;
	}
	gStateDirty = 1;
}
