/*
 * presets.cpp - preset application, encoder-driven manual param edits,
 * and persisted state save/load. Split out of the former monolithic
 * render.cpp. Bodies are byte-for-byte identical to the original.
 */
#include "hyperguitar.h"
#include "hg_mon.h"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

/*
 * ==================== v8 FLAT PAD TABLE (2026-10-27) ====================
 *
 * pad i -> EFFECT id + VOICING, DIRECTLY. No categories, no pages, no
 * PAD_SLOT geometry. trellis_ui.cpp renders through this table and
 * dispatches through it; applyEncoders() takes the effect id that comes
 * back out, exactly as before. See hyperguitar.h for the physical layout
 * diagram. Previous (category/slot) table archived in
 * bela/archive/v8-category-menu-pre-2026-10-27/.
 *
 * Pads 10/11/12/13/14 are FX_NONE on purpose (EMGOR, 2026-10-27: "the other
 * stuff I just don't like them yet but I'll come back to them") - lava
 * background only. 10/11/12 are also the live PITCH-interval readout pads
 * while PAD_PITCH is armed - see trellis_ui.cpp.
 *
 * THE RULE IS UNCHANGED: every non-empty slot must be audible the instant
 * it is pressed. No unity ratios, no zero depths, no zero wet.
 */
const FxSlot gPadFx[NPIX] = {
	/* row 0 */
	{ PAD_FORMANT, 1.0f,    0.08f,  "TIMBRE"    }, /*  0 - p1/p2 unused: the
	                                                 * pot drives gFormantVal
	                                                 * continuously, see
	                                                 * timbrePotToFormant() */
	{ PAD_PITCH,  -12.0f,  100.0f,  "PITCH"     }, /*  1 - 6-stop interval,
	                                                 * see applyEncoders() */
	{ PAD_ENVF,     3.20f,   3.50f, "AUTO-WAH"  }, /*  2 */
	{ PAD_SINE,     0.0f,    1.00f, "SYNTH"     }, /*  3 - p1 0 = JUNO PAD, the new default
                                                 * (was 5 = WARM TRI, which is 13 now) */
	/* row 1 - companion to the pad directly above */
	{ PAD_FM,       2.00f,   1.20f, "FM GROWL"  }, /*  4 - under TIMBRE */
	{ PAD_XFADE,    0.6f,    0.6f,  "XFADE"     }, /*  5 - under PITCH:
	                                                 * p1=rate Hz, p2=depth */
	{ PAD_FWAH,     2.20f,   0.35f, "FORM-WAH"  }, /*  6 - under AUTO-WAH */
	{ PAD_BEND,     5.0f,    0.12f, "BEND"      }, /*  7 - under SYNTH:
	                                                 * p1=depth semis,
	                                                 * p2=settle sec */
	/* row 2 */
	{ PAD_PAN,      1.20f,   0.25f, "SPREAD"    }, /*  8 */
	{ PAD_TRS_DRY,  0.0f,    0.0f,  "TRS DRY"   }, /*  9 - handled directly in
	                                                 * trellis_ui.cpp, like
	                                                 * PAD_USB: a hard bypass
	                                                 * toggle, not a voiced fx */
	{ FX_NONE,      0.0f,    0.0f,  ""          }, /* 10 - open (pitch readout) */
	{ FX_NONE,      0.0f,    0.0f,  ""          }, /* 11 - open (pitch readout) */
	/* row 3 */
	{ FX_NONE,      0.0f,    0.0f,  ""          }, /* 12 - open (pitch readout) */
	{ FX_NONE,      0.0f,    0.0f,  ""          }, /* 13 - open */
	{ FX_NONE,      0.0f,    0.0f,  ""          }, /* 14 - open */
	{ PAD_USB,      0.0f,    0.0f,  "USB AUDIO" }, /* 15 */
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
	else if(gFormantActive || gFWahOn || gBendOn || gJoyWhammySemis != 0.0f)
		wet = 1.0f;   /* BEND/whammy ride the shifter too - silent without the wet leg open */
	gWetTarget = wet;
	gDryTarget = 1.0f - wet;
	/*
	 * XFADE overrides both targets every block (see xfadeTick()) while it is
	 * on, so it does not matter what this function just computed - but it
	 * must still run on every toggle so turning XFADE off hands control back
	 * to whatever wet/dry state the other effects actually want.
	 */
}

/* 2026-11-01 pwet: persisted per-interval PITCH wet (0..1), one per fixed stop
 * {-12,-7,-5,+5,+7,+12}. Set from the web UI (HGSET pwet_*, hg_mon.cpp). The pot no
 * longer touches PITCH dry/wet. Defaults = the retired v5 table's wet % (-12:100,
 * -5:50, -7:50, +12:100); +5/+7 had no entry -> 1.0. gPitchBal is what render() reads.
 * 2026-11-02 EMGOR: boot defaults = the values he dialled in on the web UI
 * ("make sure it inits w the values i have now"); the mix.conf overrides these when present. */
float gPitchWetByStop[6] = { 1.0f, 0.28f, 0.28f, 0.12f, 0.11f, 1.0f };

/* 2026-11-01 pwet: push the wet of the stop gPitchSemitones sits on into gPitchBal.
 * Off-stop values (glide lane, slot p1) leave gPitchBal alone. */
void applyPitchWet(void)
{
	static const float steps[6] = { -12.0f, -7.0f, -5.0f, 5.0f, 7.0f, 12.0f };
	for(int k = 0; k < 6; k++) {
		if(fabsf(gPitchSemitones - steps[k]) < 0.01f) {
			float w = gPitchWetByStop[k];
			gPitchBal = w < 0.0f ? 0.0f : (w > 1.0f ? 1.0f : w);
			return;
		}
	}
}

// Recomputes gBetaBase[]/dry-wet from the live encoder-set pitch state.
// Called on pad press (on/off) and on every encoder move while armed.
void applyPitch(void)
{
	applyPitchWet();   /* 2026-11-01 pwet */
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

	case PAD_XFADE:
		if(slotSet(s->p1))
			gXfadeRate = slotClamp(s->p1, 0.05f, 8.0f);
		if(slotSet(s->p2))
			gXfadeDepth = slotClamp(s->p2, 0.0f, 1.0f);
		break;

	case PAD_BEND:
		if(slotSet(s->p1))
			gBendDepthSemis = slotClamp(s->p1, -24.0f, 24.0f);
		if(slotSet(s->p2))
			gBendSettleSec = slotClamp(s->p2, 0.01f, 1.0f);
		applyMixRouting();   /* opens the wet leg, same contract as FORMANT/FWAH */
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
	/* v8: no pages any more - replaces the v6/v7 curCategory field (removed,
	 * not kept as a tombstone: a bumped magic already discards old files). */
	int xfadeOn; float xfadeRate, xfadeDepth;
	int bendOn; float bendDepthSemis, bendSettleSec;
	int trsDryOn;
	/* 2026-11-05: synth index version (SYNTH_STATE_VER). APPENDED, magic NOT bumped:
	 * a bump would discard every other saved setting. A file written before this field
	 * existed is shorter; loadState() zero-fills the tail, so synthVer reads 0 and the
	 * saved synth index is migrated (+8: the 8 analog patches were put first). */
	int synthVer;
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
	st.xfadeOn = gXfadeOn; st.xfadeRate = gXfadeRate; st.xfadeDepth = gXfadeDepth;
	st.bendOn = gBendOn; st.bendDepthSemis = gBendDepthSemis;
	st.bendSettleSec = gBendSettleSec;
	st.trsDryOn = gTrsDryOn;
	st.synthVer = SYNTH_STATE_VER;

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
	/* accept the pre-synthVer (shorter) layout too: the tail stays zeroed => synthVer 0 */
	size_t n = fread(&st, 1, sizeof(st), f);
	fclose(f);
	if(n < offsetof(PersistedState, synthVer) || st.magic != STATE_MAGIC)
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
	gSynthMode = synthModeMigrate(st.synthMode, st.synthVer);   /* same SOUND after the 8 analog patches were put first */
	gSynthPoly = 1;   /* always boot POLY (owner); the click switches to mono */
	gSynthLevel = st.synthLevel; gSynthGateThr = st.synthGateThr;
	(void)st.formantMode;
	gFormantMode = 1;   /* 2026-11-01 A/B: always boot WARP (LPC), like gSynthPoly above; saved SMOOTH must not undo the test. Encoder still cycles. */
	gXfadeOn = st.xfadeOn; gXfadeRate = st.xfadeRate; gXfadeDepth = st.xfadeDepth;
	gBendOn = st.bendOn; gBendDepthSemis = st.bendDepthSemis;
	gBendSettleSec = st.bendSettleSec;
	gTrsDryOn = st.trsDryOn;
}

/*
 * applyEncoders: routes the live encoder delta + click flag to whichever
 * effect is currently "armed" (gArmedPad, set by the last pad press in
 * trellisTick).
 *
 * v8: ONE physical encoder, period - ENC2 was never fitted, and the old
 * "aux" axis/click (rotation on a second parameter, a secondary ratchet on
 * a second click) is RETIRED, not emulated via a mode-swap gesture any
 * more (that workaround lived in render.cpp and was ALSO the source of a
 * real bug: a click could fire the axis-swap AND an effect's own click
 * behaviour in the same gesture - see render.cpp's render() for the fix).
 * `pad` is an EFFECT id, not a physical pad position - trellis_ui resolves
 * the pressed pad through gPadFx[] before calling us.
 *
 * CLICK CONVENTION: mainClick is the effect's ONE discrete action - a mode
 * switch (PITCH cents removed in v8; FM quantized/free; FORMANT shape has
 * its own dedicated rotation, no click), or a ratchet through a few
 * musically-chosen values (SITAR buzz, and RES gain/SUS level/JAWARI sens
 * moved here from the old dead auxClick - they had no OTHER click user, so
 * nothing was given up moving them). Every effect that used to have BOTH a
 * second rotation axis AND/OR a second click (POG mod depth, PLUCKSAG
 * settle, SYMP ring length, SUS damp, JAWARI pitch, SITAR ring, FWAH tilt
 * bias, FM index, SYNTH level/gate, XFADE depth, BEND direction-flip) now
 * has that axis frozen at whatever the slot preset or save file set it to -
 * still a real, audible parameter, just not live-tweakable from the grid
 * with one encoder. None of those effects are reachable from the current
 * grid anyway except XFADE/BEND/FM/SYNTH, whose main axis is unaffected.
 */
/*
 * THE POT (2026-09-24). Its job follows the ARMED effect (the last pad you
 * pressed):
 *
 *   CHORUS / VIBRATO  depth       AUTO-WAH  sensitivity     SPREAD  amount
 *   SYNTH  filter mod (gSynFmod 0..1 = 0..4 octaves of level->cutoff; was: nothing,
 *          the pot did not touch SYNTH. synth wet gSineAmtS is just the on/off
 *          fade of the pad and was never pot-driven)
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
	return fx == PAD_CHORUS || fx == PAD_VIB || fx == PAD_ENVF || fx == PAD_PAN
			|| fx == PAD_BEND || fx == PAD_SINE;
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
					: fx == PAD_ENVF ? "auto-wah sensitivity"
					: fx == PAD_BEND ? "bend settle time"
					: fx == PAD_SINE ? "synth filter mod" : "spread");
		}
		if(sPotOwned) {
			switch(fx) {
			case PAD_CHORUS: gChorDepthSec = 0.0065f * pot; break;
			case PAD_VIB:    gVibDepthSec  = 0.0045f * pot; break;
			case PAD_ENVF:   gEnvfSens     = 0.3f + 5.7f * pot; break;
			case PAD_PAN:    gPanSpread    = 2.0f * pot; break;
			case PAD_BEND:   gBendSettleSec = 0.01f + 0.99f * pot; break;
			case PAD_SINE:   /* 2026-11-05: SYNTH filter mod = level->cutoff octaves, 0..4 */
				if(fabsf(pot - gSynFmod) > 0.004f) {
					gSynFmod = pot < 0.0f ? 0.0f : (pot > 1.0f ? 1.0f : pot);
					monMixDirty();            /* persisted as mix param syn_fmod */
				}
				break;
			}
		}
		return;
	}
	/*
	 * TIMBRE: direct, no pickup - the pot IS the tone knob continuously
	 * whenever TIMBRE was the last pad pressed, same contract as pitch
	 * dry/wet below. No pickup because "darkest..brightest" has no sane
	 * default to pick up FROM; it is the control, not a macro on top of one.
	 */
	if(fx == PAD_FORMANT) {
		gFormantVal = timbrePotToFormant(pot);
		applyFormant();
		return;
	}
	/* 2026-11-01 pwet: pot no longer sets PITCH dry/wet; per-stop wet from the web UI.
	if(gPitchActive)
		gPitchBal = pot;
	*/
}

void applyEncoders(int pad, int mainDelta, int mainClick)
{
	switch(pad) {
	case PAD_ENVF:
		/* ENC = Q (resonance). Sensitivity is the pot. */
		gEnvfQ += mainDelta * 0.1f;
		if(gEnvfQ < 0.3f) gEnvfQ = 0.3f;
		if(gEnvfQ > 8.0f) gEnvfQ = 8.0f;
		break;
	case PAD_RES:
		gResSens += mainDelta * 0.06f;    // how far dynamics push the harmonic
		if(gResSens < 0.2f) gResSens = 0.2f;
		if(gResSens > 6.0f) gResSens = 6.0f;
		/* feedback (overtone ring length) lost its aux rotation in v8 -
		 * frozen at the slot/save-file value. */
		if(mainClick) {                    // CLICK: resonator output gain
			gResGain = (gResGain > 3.0f) ? 1.0f
				 : (gResGain > 1.6f) ? 3.5f : 2.2f;
			rt_printf("RES gain %.2f\n", gResGain);
		}
		break;
	case PAD_VIB:
		/* ENC = rate, fine: 1.5% per detent, multiplicative so it feels
		 * even across the range (~180 detents from slowest to fastest).
		 * Depth is the pot. */
		if(mainDelta) {
			gVibRateScale *= powf(1.015f, (float)mainDelta);
			if(gVibRateScale < 0.2f) gVibRateScale = 0.2f;
			if(gVibRateScale > 3.0f) gVibRateScale = 3.0f;
		}
		break;
	case PAD_CHORUS:
		/* ENC = rate, fine (same law as vibrato). Depth is the pot. */
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
		/* mod depth lost its aux rotation in v8 - frozen at slot/save value. */
		break;
	case PAD_PLUCKSAG:
		gSagDepthSec += mainDelta * 0.0002f;
		if(gSagDepthSec < 0.0f) gSagDepthSec = 0.0f;
		if(gSagDepthSec > 0.01f) gSagDepthSec = 0.01f;
		/* settle time lost its aux rotation in v8 - frozen at slot/save value. */
		break;
	case PAD_SYMP:
		gSympCoupling += mainDelta * 0.06f;  // how hard the others ring it
		if(gSympCoupling < 0.0f) gSympCoupling = 0.0f;
		if(gSympCoupling > 4.0f) gSympCoupling = 4.0f;
		/* ring length lost its aux rotation in v8 - frozen at slot/save value. */
		break;
	case PAD_SUS:
		gSusFb += mainDelta * 0.002f;     // hold time (top = infinite)
		if(gSusFb < 0.90f) gSusFb = 0.90f;
		if(gSusFb > 0.9995f) gSusFb = 0.9995f;
		/* damp (held-note tone) lost its aux rotation in v8 - frozen at
		 * slot/save value. The level ratchet moves here from the old dead
		 * auxClick - nothing else wanted this click. */
		if(mainClick) {
			gSusLevel = (gSusLevel > 2.5f) ? 0.9f
				  : (gSusLevel > 1.4f) ? 3.0f : 1.8f;
			rt_printf("SUS level %.2f\n", gSusLevel);
		}
		break;
	case PAD_JAWARI:
		gJawBuzz += mainDelta * 0.05f;    // buzz amount
		if(gJawBuzz < 0.0f) gJawBuzz = 0.0f;
		if(gJawBuzz > 2.0f) gJawBuzz = 2.0f;
		/* rattle pitch lost its aux rotation in v8 - frozen at slot/save
		 * value. The sens ratchet moves here from the old dead auxClick. */
		if(mainClick) {
			gJawSens = (gJawSens > 3.0f) ? 1.0f
				 : (gJawSens > 1.5f) ? 4.0f : 2.0f;
			rt_printf("JAWARI sens %.2f\n", gJawSens);
		}
		break;
	case PAD_SITAR:
		gSitarLevel += mainDelta * 0.02f;   // wet fraction (dry stays 100%)
		if(gSitarLevel < 0.0f) gSitarLevel = 0.0f;
		if(gSitarLevel > 1.0f) gSitarLevel = 1.0f;
		/* tarab ring length lost its aux rotation in v8 - frozen at
		 * slot/save value. */
		if(mainClick) {                      // click: more/less jawari buzz
			gSitarBuzz = (gSitarBuzz > 0.9f) ? 0.2f : gSitarBuzz + 0.35f;
			rt_printf("SITAR buzz %.2f\n", gSitarBuzz);
		}
		break;
	case PAD_FWAH:
		gFWahSens += mainDelta * 0.08f;       // how hard the vowel swings
		if(gFWahSens < 0.2f) gFWahSens = 0.2f;
		if(gFWahSens > 8.0f) gFWahSens = 8.0f;
		/* EQ tilt bias lost its aux rotation in v8 - frozen at the slot
		 * default (0.35, see gPadFx[]). */
		break;
	case PAD_PAN:
		/* spread is the pot; the encoder does nothing here */
		break;
	case PAD_PITCH: {
		/*
		 * v8: six FIXED stops only - octave down, 5th down, 4th down, 4th
		 * up, 5th up, octave up (EMGOR, 2026-10-27) - not continuous
		 * semitone creep. Same nearest-index-then-step pattern as FM's
		 * qSteps[]. The pot still owns dry/wet, unchanged.
		 */
		static const float pitchSteps[6] = { -12.0f, -7.0f, -5.0f, 5.0f, 7.0f, 12.0f };
		if(mainDelta) {
			int idx = 0;
			float best = 1e9f;
			for(int k = 0; k < 6; k++) {
				float d = fabsf(pitchSteps[k] - gPitchSemitones);
				if(d < best) { best = d; idx = k; }
			}
			idx += mainDelta;
			if(idx < 0) idx = 0;
			if(idx > 5) idx = 5;
			gPitchSemitones = pitchSteps[idx];
			applyPitch();
		}
		break;
	}
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
		/* FM index lost its aux rotation in v8 - frozen at the slot
		 * default (1.20, see gPadFx[]). */
		break;
	case PAD_FORMANT: {
		/*
		 * v8 TIMBRE pad: the pot is continuous tone (see
		 * timbrePotToFormant()), not a fixed voicing any more. ENC1 walks
		 * "shape" - which processing character the TIMBRE knob rides on -
		 * four positions, position 0 the smoothest/most normal (EMGOR,
		 * 2026-10-27: "first being most smooth / normal"). Built entirely
		 * from engine knobs that already exist (gFormantMode, the TALKBOX
		 * wobble flag, glide speed) rather than new DSP.
		 */
		/*
		 * Derived, not a disconnected static counter: loadState() restores
		 * gFormantMode/gPresetFormantWobble/gFormantGlide from whatever
		 * shape was live at last power-down, and a plain "static int shape
		 * = 0" would ignore that and start the encoder back at SMOOTH every
		 * boot regardless. Reading the current state back into an index
		 * keeps the two in sync with no persisted field of their own.
		 */
		int shape = (gFormantMode == 1) ? 1
				: gPresetFormantWobble ? 2
				: (gFormantGlide > 0.15f) ? 3 : 0;
		if(mainDelta) {
			shape += mainDelta;
			while(shape < 0) shape += 4;
			shape %= 4;
			switch(shape) {
			/* 2026-11-01 EMGOR: "make sure it never uses that bad algo again".
			 * The grain formant engine (gFormantMode=0) is retired from every
			 * shape; all four are LPC (mode 1) and differ only in wobble/glide.
			 * Grain code stays in shifter.cpp but is unreachable from the UI. */
			case 0:  /* SMOOTH: LPC, no wobble, normal glide */
				gFormantMode = 1;
				gPresetFormantWobble = 0;
				gFormantGlide = 0.08f;
				rt_printf("TIMBRE shape: SMOOTH\n");
				break;
			case 1:  /* WARP: LPC, no wobble, normal glide (same as SMOOTH until LPC variants exist) */
				gFormantMode = 1;
				gPresetFormantWobble = 0;
				gFormantGlide = 0.08f;
				rt_printf("TIMBRE shape: WARP\n");
				break;
			case 2:  /* BREATHY: LPC + slow TALKBOX vowel-breathing */
				gFormantMode = 1;
				gPresetFormantWobble = 1;
				gFormantGlide = 0.08f;
				rt_printf("TIMBRE shape: BREATHY\n");
				break;
			case 3:  /* SNAPPY: LPC, fast glide, no wobble */
				gFormantMode = 1;
				gPresetFormantWobble = 0;
				gFormantGlide = 0.25f;
				rt_printf("TIMBRE shape: SNAPPY\n");
				break;
			}
		}
		break;
	}
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
		/* level and the gate-threshold ratchet lost their aux axis/click in
		 * v8 - frozen at the slot/save value. mainClick is mono/poly's, and
		 * that was already the one click this effect gets. */
		break;
	case PAD_XFADE:
		/* ENC = rate (Hz), multiplicative like vib/chorus rate. */
		if(mainDelta) {
			gXfadeRate *= powf(1.04f, (float)mainDelta);
			if(gXfadeRate < 0.05f) gXfadeRate = 0.05f;
			if(gXfadeRate > 8.0f) gXfadeRate = 8.0f;
		}
		/* depth lost its aux rotation in v8 - frozen at the slot default
		 * (0.6, see gPadFx[]). */
		break;
	case PAD_BEND:
		/* ENC = depth, signed semitones - rotate past 0 to flip which way
		 * it bends (the old auxClick direction-flip shortcut is gone, but
		 * nothing is actually lost: this was always reachable by rotation
		 * alone). Settle time is the pot (pickup - see potTick()). */
		gBendDepthSemis += (float)mainDelta * 0.5f;
		if(gBendDepthSemis < -24.0f) gBendDepthSemis = -24.0f;
		if(gBendDepthSemis > 24.0f) gBendDepthSemis = 24.0f;
		break;
	default:
		break;
	}
	gStateDirty = 1;
}

/* ==================== v8: TIMBRE pot + XFADE tick ==================== */

/*
 * Curated range, not the engine's full clamp (0.4-2.2): these ten values are
 * the ones the old TONE category already proved sound good by ear (FORM 0.55
 * .. FORM 1.50). "Darkest .. brightest" maps straight across it, clockwise =
 * up = brighter (gHgPot is already flipped so clockwise = up, see hg_ctrl.cpp).
 */
#define TIMBRE_LO 0.55f
#define TIMBRE_HI 1.50f

float timbrePotToFormant(float pot)
{
	if(pot < 0.0f) pot = 0.0f;
	if(pot > 1.0f) pot = 1.0f;
	float f = TIMBRE_LO + pot * (TIMBRE_HI - TIMBRE_LO);
	/* same unity-silence guard applySlotPreset() uses for PAD_FORMANT: a
	 * formant ratio of 1.0 is bit-identical to the dry signal, and TIMBRE_LO/
	 * TIMBRE_HI straddle it, so the middle of the pot's travel would
	 * otherwise go briefly silent. */
	if(fabsf(f - 1.0f) < 0.03f)
		f = (f < 1.0f) ? 0.97f : 1.03f;
	return f;
}

/*
 * XFADE: equal-power crossfade between dry (TRS) and wet (hex/processed),
 * centred on a 50/50 blend so loudness holds steady while the dry/wet
 * BALANCE sweeps - depth scales how far either side of centre it swings.
 * Overrides gDryTarget/gWetTarget every block while on; applyMixRouting()
 * takes control back the instant it is switched off (see its own comment).
 * Per-block like hgGlideTick(), not per-sample: XFADE's useful rate range
 * (0.05-8 Hz) is far below the sample-accurate gDryS/gWetS slew already
 * smoothing these targets every sample, so a block-rate update is inaudibly
 * steppy at any -p this project runs.
 */
static float sXfadePhase = 0.0f;
void xfadeTick(float blockSeconds)
{
	if(!gXfadeOn)
		return;
	sXfadePhase += 2.0f * (float)M_PI * gXfadeRate * blockSeconds;
	if(sXfadePhase > 2.0f * (float)M_PI)
		sXfadePhase -= 2.0f * (float)M_PI;
	/* x in [-depth, +depth] around 0, then mapped to an equal-power pair
	 * around the 0.5/0.5 centre so the two gains always sum in power to 1. */
	float x = gXfadeDepth * sinf(sXfadePhase);         /* -depth..+depth */
	float angle = (0.5f + 0.5f * x) * 0.5f * (float)M_PI; /* 0..pi/2 */
	gWetTarget = sinf(angle);
	gDryTarget = cosf(angle);
}
