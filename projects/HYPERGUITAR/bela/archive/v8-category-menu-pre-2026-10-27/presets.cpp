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
	 * MENU REDO 2026-09-24 (owner spec). Previous table archived in
	 * bela/archive/v7-menus-pre-2026-09-24/. Slot 0 is the pad where OCT
	 * DOWN has always been (bottom-right as you look at the grid).
	 *
	 * CAT 0 TONE - ten smooth formant shifts, deep -> high, all on the same
	 * slow glide so every change melts rather than steps. Spot 9 (slot 8)
	 * is SYNTHESIS (the synth voice, default mode 5 WARM TRI (modes ordered dark->bright 2026-10-01) - ENC1 walks
	 * all 15 modes, ENC2 = level), last slot FM GROWL. Pot and encoders do
	 * NOTHING on formant.
	 */
	{  { PAD_FORMANT, 0.55f,  0.025f, "FORM 0.55"  },
	  { PAD_FORMANT, 0.65f,  0.025f, "FORM 0.65"  },
	  { PAD_FORMANT, 0.75f,  0.025f, "FORM 0.75"  },
	  { PAD_FORMANT, 0.82f,  0.025f, "FORM 0.82"  },
	  { PAD_FORMANT, 0.90f,  0.025f, "FORM 0.90"  },
	  { PAD_FORMANT, 1.06f,  0.025f, "FORM 1.06"  },
	  { PAD_FORMANT, 1.12f,  0.025f, "FORM 1.12"  },
	  { PAD_FORMANT, 1.20f,  0.025f, "FORM 1.20"  },
	  { PAD_SINE,    5.0f,   1.00f,  "SYNTHESIS"  },
	  { PAD_FORMANT, 1.32f,  0.025f, "FORM 1.32"  },
	  { PAD_FORMANT, 1.50f,  0.025f, "FORM 1.50"  },
	  { PAD_FM,      2.00f,  1.20f,  "FM GROWL"   } },
	/*
	 * CAT 1 PITCH - six intervals, nothing else. Always 100% wet on the hex
	 * pickup; the POT is the dry/wet, balancing the guitar's own TRS pickup
	 * (dry) against the shifted hex (wet). p2 is unused (no wet preset).
	 * ENC1 moves the interval by semitones; re-pressing a pad restores it.
	 */
	{ { PAD_PITCH, -12.0f, 100.0f, "OCT DOWN"  },
	  { PAD_PITCH,  12.0f, 100.0f, "OCT UP"    },
	  { PAD_PITCH,  -7.0f, 100.0f, "5TH DOWN"  },
	  { PAD_PITCH,  -5.0f, 100.0f, "4TH DOWN"  },
	  { PAD_PITCH,   5.0f, 100.0f, "4TH UP"    },
	  { PAD_PITCH,   7.0f, 100.0f, "5TH UP"    },
	  { FX_NONE,     0.0f,   0.0f,   ""          },
	  { FX_NONE,     0.0f,   0.0f,   ""          },
	  { FX_NONE,     0.0f,   0.0f,   ""          },
	  { FX_NONE,     0.0f,   0.0f,   ""          },
	  { FX_NONE,     0.0f,   0.0f,   ""          },
	  { FX_NONE,     0.0f,   0.0f,   ""          } },
	/*
	 * CAT 2 MOD - one chorus, one vibrato, one auto-wah. POG SWELL sits in
	 * spot 9 (slot 8) - owner's note: it sucks as it stands, needs work. POT = amount macro
	 * with a 10% pickup (see potTick), ENC1 = rate (chorus/vib) or Q (wah).
	 * Every press restores these defaults.
	 */
	{ { PAD_CHORUS,  1.00f, 0.0030f, "CHORUS"   },
	  { PAD_VIB,     1.20f, 0.0018f, "VIBRATO"  },
	  { FX_NONE,     0.0f,   0.0f,   ""          },
	  { PAD_ENVF,    3.20f, 3.50f,   "AUTO-WAH" },
	  { FX_NONE,     0.0f,   0.0f,   ""          },
	  { FX_NONE,     0.0f,   0.0f,   ""          },
	  { FX_NONE,     0.0f,   0.0f,   ""          },
	  { FX_NONE,     0.0f,   0.0f,   ""          },
	  { PAD_POG,     0.45f, 0.00f,   "POG SWELL" },
	  { FX_NONE,     0.0f,   0.0f,   ""          },
	  { FX_NONE,     0.0f,   0.0f,   ""          },
	  { FX_NONE,     0.0f,   0.0f,   ""          } },
	/*
	 * CAT 3 UTILITY - stereo spread first. Always on; the pad arms the pot,
	 * which sets the spread amount (10% pickup). LAST slot = USB AUDIO
	 * on/off (not an effect - handled directly in trellis_ui.cpp). Off =
	 * the firmware stops feeding USB and the bridge closes its output.
	 */
	{ { PAD_PAN,     1.20f, 0.25f,   "SPREAD"   },
	  { FX_NONE,     0.0f,   0.0f,   ""          },
	  { FX_NONE,     0.0f,   0.0f,   ""          },
	  { FX_NONE,     0.0f,   0.0f,   ""          },
	  { FX_NONE,     0.0f,   0.0f,   ""          },
	  { FX_NONE,     0.0f,   0.0f,   ""          },
	  { FX_NONE,     0.0f,   0.0f,   ""          },
	  { FX_NONE,     0.0f,   0.0f,   ""          },
	  { FX_NONE,     0.0f,   0.0f,   ""          },
	  { FX_NONE,     0.0f,   0.0f,   ""          },
	  { FX_NONE,     0.0f,   0.0f,   ""          },
	  { PAD_USB,     0.0f,  0.0f,    "USB AUDIO" } }
};


/* Short enough to survive any future 4-char OLED/log field. Index order
 * must match the CAT_* defines exactly - they index this array directly. */
const char *const gCatName[NUM_CATEGORIES] = {
	"TONE", "PITCH", "MOD", "UTILITY"
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
		wet = 1.0f;   /* always fully wet; the pot balances TRS (dry) against it */
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
		/* p1 = whole-semitone interval. No cents, no wet: the shifter is
		 * always 100% wet and the pot is the dry/wet (see potTick). */
		if(slotSet(s->p1))
			gPitchSemitones = slotClamp(roundf(s->p1), -12.0f, 12.0f);
		gPitchCents = 0.0f;
		gPitchCentsMode = 0;
		gPitchWetPct = 100.0f;
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
	gSynthMode = st.synthMode;
	gSynthPoly = 1;   /* always boot POLY (owner); the click switches to mono */
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
/*
 * THE POT (2026-09-24). Its job follows the ARMED effect (the last pad you
 * pressed):
 *
 *   CHORUS / VIBRATO  depth       AUTO-WAH  sensitivity     SPREAD  amount
 *   PITCH (or formant/FM armed while pitch is on): dry/wet, DIRECT
 *
 * For the depth/sensitivity/spread jobs the pot uses a PICKUP: pressing
 * the pad restores the preset's default, and the pot is ignored until it
 * has moved more than POT_PICKUP (10% of travel) from where it sat at the
 * press. So a press never jumps to wherever the knob happened to be.
 *
 * The pitch dry/wet has no pickup, by request: it always IS the pot.
 * gPitchBal 0 = all TRS guitar (dry), 1 = all shifted hex (wet). While the
 * pot is busy with another effect the balance holds where it was.
 */
#define POT_PICKUP 0.10f
volatile float gPitchBal = 1.0f;
static int   sPotFx = FX_NONE;
static float sPotAtArm = 0.0f;
static int   sPotOwned = 0;

static int potPickupFx(int fx)
{
	return fx == PAD_CHORUS || fx == PAD_VIB || fx == PAD_ENVF || fx == PAD_PAN;
}

void potArm(int fx)
{
	sPotFx = fx;
	sPotAtArm = gHgPot;
	sPotOwned = 0;
}

void potTick(float pot)
{
	int fx = gArmedPad;
	if(potPickupFx(fx)) {
		if(fx != sPotFx)
			potArm(fx);                    /* armed from elsewhere (NEPTR) */
		if(!sPotOwned && fabsf(pot - sPotAtArm) > POT_PICKUP) {
			sPotOwned = 1;
			rt_printf("pot: picked up %s\n", fx == PAD_CHORUS ? "chorus depth"
					: fx == PAD_VIB ? "vibrato depth"
					: fx == PAD_ENVF ? "auto-wah sensitivity" : "spread");
		}
		if(sPotOwned) {
			switch(fx) {
			case PAD_CHORUS: gChorDepthSec = 0.0065f * pot; break;
			case PAD_VIB:    gVibDepthSec  = 0.0045f * pot; break;
			case PAD_ENVF:   gEnvfSens     = 0.3f + 5.7f * pot; break;
			case PAD_PAN:    gPanSpread    = 2.0f * pot; break;
			}
		}
		return;
	}
	if(gPitchActive)
		gPitchBal = pot;
}

void applyEncoders(int pad, int mainDelta, int auxDelta, int mainClick, int auxClick)
{
	switch(pad) {
	case PAD_ENVF:
		/* ENC1 = Q (resonance). Sensitivity is the pot. */
		gEnvfQ += mainDelta * 0.1f;
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
		/* ENC1 = rate, fine: 1.5% per detent, multiplicative so it feels
		 * even across the range (~180 detents from slowest to fastest).
		 * Depth is the pot. */
		if(mainDelta) {
			gVibRateScale *= powf(1.015f, (float)mainDelta);
			if(gVibRateScale < 0.2f) gVibRateScale = 0.2f;
			if(gVibRateScale > 3.0f) gVibRateScale = 3.0f;
		}
		break;
	case PAD_CHORUS:
		/* ENC1 = rate, fine (same law as vibrato). Depth is the pot. */
		if(mainDelta) {
			gChorRateScale *= powf(1.015f, (float)mainDelta);
			if(gChorRateScale < 0.2f) gChorRateScale = 0.2f;
			if(gChorRateScale > 3.0f) gChorRateScale = 3.0f;
		}
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
		/* spread is the pot; encoders do nothing here */
		break;
	case PAD_PITCH:
		/* ENC1 = interval in semitones. The pot owns dry/wet. */
		if(mainDelta) {
			gPitchSemitones += (float)mainDelta;
			if(gPitchSemitones < -12.0f) gPitchSemitones = -12.0f;
			if(gPitchSemitones > 12.0f) gPitchSemitones = 12.0f;
			applyPitch();
		}
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
		/* TONE menu: the formant presets are fixed. Encoders do nothing. */
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
