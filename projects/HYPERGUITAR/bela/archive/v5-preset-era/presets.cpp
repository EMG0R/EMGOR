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

// PRESET MODE: one-touch vibes. 16 pads, each independently toggleable
// EXCEPT three families that share a single global parameter set.
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
 */
typedef struct {
	uint32_t magic;
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
} PersistedState;

void saveState(void)
{
	PersistedState st;
	memset(&st, 0, sizeof(st));
	st.magic = STATE_MAGIC;
	st.presetMode = gPresetMode;
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

	gPresetMode = st.presetMode;
	gPresetFormantSel = st.presetFormantSel;
	gPresetPitchSel = st.presetPitchSel;
	gPresetVibChorusSel = st.presetVibChorusSel;
	gPresetFormantWobble = st.presetFormantWobble;
	gPitchActive = st.pitchActive; gFormantActive = st.formantActive;
	gFmActive = st.fmActive;
	gVibOn = st.vibOn; gChorusOn = st.chorusOn; gEnvFOn = st.envFOn;
	gFWahOn = st.fWahOn; gSineOn = st.sineOn; gPogOn = st.pogOn;
	gResOn = st.resOn; gSusOn = st.susOn; gJawOn = st.jawOn;
	gSitarOn = st.sitarOn; gPanOn = st.panOn; gSagOn = st.sagOn;
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
}

/*
 * applyEncoders: routes live main/aux encoder deltas + click flags to
 * whichever pad is currently "armed" (gArmedPad, set by the last pad
 * press in trellisTick).
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
		break;
	case PAD_JAWARI:
		gJawBuzz += mainDelta * 0.05f;    // MAIN: buzz amount
		if(gJawBuzz < 0.0f) gJawBuzz = 0.0f;
		if(gJawBuzz > 2.0f) gJawBuzz = 2.0f;
		gJawPitch += auxDelta * 6.0f;     // AUX: metallic pitch of the rattle
		if(gJawPitch < 16.0f) gJawPitch = 16.0f;
		if(gJawPitch > 400.0f) gJawPitch = 400.0f;
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
		// (release-time control removed - amplitude now follows the real
		// string envelope, so there's no artificial release to adjust)
		break;
	default:
		break;
	}
	gStateDirty = 1;
}
