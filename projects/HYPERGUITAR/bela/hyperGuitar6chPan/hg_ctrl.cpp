/*
 * hg_ctrl.cpp - TRS expression pedal, joystick whammy, encoder diagnostic.
 * See hg_ctrl.h for the pin map and the reasoning behind the arm-at-zero and
 * the offset-not-value joystick.
 */
#include "hyperguitar.h"
#include "hg_ctrl.h"
#include <cmath>

volatile unsigned gTelEncInvalid = 0;  /* 2026-11-01 enc: invalid quadrature transitions seen */
volatile unsigned gTelEncSteps = 0;    /* 2026-11-01 enc: UI steps emitted */
int gHgEncDiag = 1;   /* on by default; it only prints on real movement */

/* ---------------- joystick button (see hg_ctrl.h) ---------------- */
volatile float gHgExpr = 0.0f;
volatile int   gHgExprArmed = 0;
volatile float gHgJoyXv = 0.0f;
volatile float gHgJoyYv = 0.0f;
volatile float gHgPot = 0.0f;
volatile float gHgExprRaw = 0.0f;

volatile int gHgJoyClick = 0;   /* one-shot on press; consumer clears it */
volatile int gHgJoyHeld  = 0;   /* level, 1 while held */
static int gJoySwInit = 0, gJoySwIdle = 1, gJoySwStable = 1, gJoySwHold = 0;

/* ---------------- expression pedal ---------------- */
static int   gExprArmed = 0;
static float gExprVal   = 0.0f;   /* 0..1, only meaningful once armed */
static float gExprSmooth = 0.0f;

/* ---------------- joystick ---------------- */
static int   gJoyInit = 0;
static float gJoyCenX = 0.5f, gJoyCenY = 0.5f;
static float gJoyX = 0.0f, gJoyY = 0.0f;   /* -1..+1, 0 inside the deadzone */

/*
 * Whammy base (FORMANT only, v8). While the stick is centred this TRACKS the
 * live value, so the pads and encoders stay in charge; while it is pushed it
 * holds, and the live value becomes base + offset. Releasing therefore
 * restores exactly the sound that was there before, with no memory of how it
 * got there. The old gHgPitchBase/gJoyHeldPitch pair is RETIRED: the v8
 * universal whammy (below) replaces it with a multiplicative offset that
 * compounds on top of PITCH/BEND rather than overwriting gPitchSemitones -
 * see hyperguitar.h's gJoyWhammySemis and dsp_core.cpp's updateModulation().
 */
/* NOTE the gHg prefix: hyperguitar.h already declares a per-string
 * `volatile float gFormantBase[NUM_STRINGS]` — a completely different thing
 * (the tracker's per-string formant reference). Shadowing it was a
 * redefinition error, and had it compiled it would have been miserable to
 * read. Same reason gHgFormantBase is prefixed. */
static float gHgFormantBase = 1.0f;
static int   gJoyHeldFormant = 0;

void hgCtrlSetup(void)
{
	gExprArmed = 0;
	gExprVal = gExprSmooth = 0.0f;
	gJoyInit = 0;
	gJoyHeldFormant = 0;
	gJoySwInit = 0;
	gHgJoyClick = 0;
	gHgJoyHeld = 0;
	rt_printf("hg-ctrl: expression pedal DISARMED - sweep it to heel once to "
			"arm it (an unplugged jack floats mid-scale and never will)\n");
}

/* deadzone + rescale so the usable travel still spans the full -1..+1 */
static inline float joyAxis(float raw, float centre, float deadzone)
{
	float d = raw - centre;
	float span = (d >= 0.0f) ? (1.0f - centre) : centre;
	if(span < 1e-3f)
		return 0.0f;
	float n = d / span;                       /* -1..+1 before the deadzone */
	if(fabsf(n) < deadzone)
		return 0.0f;                          /* EXACTLY zero, see header */
	float s = (n > 0.0f) ? 1.0f : -1.0f;
	return s * (fabsf(n) - deadzone) / (1.0f - deadzone);
}

void hgCtrlTick(BelaContext *context)
{
	if(context->analogFrames == 0)
		return;

	/* one analog frame per block is plenty: these are hands, not audio */
	float ain0 = analogRead(context, 0, AIN_EXPR);
	float ainX = analogRead(context, 0, AIN_JOY_X);
	float ainY = analogRead(context, 0, AIN_JOY_Y);
	float ainP = analogRead(context, 0, AIN_POT);
	/* POT CALIBRATION (2026-09-24). Bela's analog inputs are 0-4.096 V
	 * full scale and the pot sits across 3.3 V, so raw tops out at ~0.806
	 * (the dashboard's "max is 81"). It was also wired backwards. Rescale
	 * to 0..1, flip it so clockwise = up, and smooth off the ADC rattle. */
	{
		static float sPot = -1.0f;
		const float POT_RAW_LO = 0.004f, POT_RAW_HI = 0.800f;
		if(sPot < 0.0f) sPot = ainP;
		sPot += (ainP - sPot) * 0.15f;
		float n = (sPot - POT_RAW_LO) / (POT_RAW_HI - POT_RAW_LO);
		n = n < 0.0f ? 0.0f : (n > 1.0f ? 1.0f : n);
		gHgPot = 1.0f - n;
	}
	gHgExprRaw = ain0;

	/* ---------------- expression pedal ---------------- */
	/*
	 * ARM-AT-ZERO. Until the pedal has been seen at heel it contributes
	 * nothing, which is exactly what we want from a jack with no pedal in it.
	 */
	if(!gExprArmed) {
		if(ain0 < HG_EXPR_ARM_BELOW) {
			gExprArmed = 1;
			gExprSmooth = ain0;
			rt_printf("hg-ctrl: expression pedal ARMED\n");
			gHgExprArmed = 1;
		}
	} else {
		/* light smoothing: a pedal is a hand, and the ADC's last bits rattle */
		gExprSmooth += (ain0 - gExprSmooth) * 0.25f;
		gExprVal = gExprSmooth < 0.0f ? 0.0f : (gExprSmooth > 1.0f ? 1.0f : gExprSmooth);
		gHgExpr = gExprVal;

		/*
		 * PEDAL-DRIVES-PITCH/FORMANT: RETIRED (v8.2, 2026-10-27). This used
		 * to hard-OVERWRITE gPitchSemitones/gFormantVal every block whenever
		 * PITCH/TIMBRE was on - the only continuous control either effect
		 * had, back before the flat grid gave PITCH its own 6-stop encoder
		 * and TIMBRE its own continuous pot. Left live, it fights BOTH of
		 * those every block (pedal position wins, unconditionally,
		 * regardless of what the encoder/pot just set) - confirmed bug,
		 * EMGOR 2026-10-27: "it only has down and octave and down a fifth"
		 * for PITCH, because the pedal kept dragging gPitchSemitones back
		 * toward wherever it physically rests, undoing every encoder step.
		 * gExprVal/gHgExpr are still read and smoothed above, available for
		 * a future control that actually wants the pedal.
		 */
	}

	/* ---------------- joystick button ---------------- */
	/*
	 * Debounced the same way the encoder switches are, and with the idle level
	 * MEASURED rather than assumed — a module that pulls the other way would
	 * otherwise read as permanently held.
	 */
	{
		bool sw = digitalRead(context, 0, PIN_JOY_SW);
		if(!gJoySwInit) {
			gJoySwIdle = sw;
			gJoySwStable = sw;
			gJoySwInit = 1;
		}
		if(gJoySwHold > 0) {
			gJoySwHold--;
		} else if(sw != gJoySwStable) {
			gJoySwStable = sw;
			gJoySwHold = 30;              /* ~30 blocks of debounce */
			if(sw != gJoySwIdle) {
				gHgJoyClick = 1;          /* press edge */
				gHgJoyHeld = 1;
				if(gHgEncDiag)
					rt_printf("ENC-DIAG: JOYSTICK BUTTON (pin %d) PRESS\n",
							PIN_JOY_SW);
			} else {
				gHgJoyHeld = 0;
				if(gHgEncDiag)
					rt_printf("ENC-DIAG: JOYSTICK BUTTON release\n");
			}
		}
	}

	/* ---------------- joystick ---------------- */
	if(!gJoyInit) {
		/*
		 * The centre is MEASURED at boot, not assumed to be 0.5. A thumbstick's
		 * mechanical centre is never electrically centred, and assuming it is
		 * leaves a permanent bend in the note the moment the effect is engaged.
		 */
		gJoyCenX = ainX;
		gJoyCenY = ainY;
		gJoyInit = 1;
		rt_printf("hg-ctrl: joystick centre measured X=%.3f Y=%.3f\n",
				gJoyCenX, gJoyCenY);
	}
	/* Raw axes first, then re-orient to the PLAYER's frame - see hg_ctrl.h.
	 * Everything downstream thinks in up/down/left/right as the hand feels
	 * them, so the rotation is resolved once, here, and never again.
	 *
	 * TWO deadzones computed from the SAME raw channels: the wide one
	 * (HG_JOY_DEADZONE) for X (formant bend while active) and the legacy
	 * gJoyY telemetry mirror, and the small one (HG_WHAMMY_DEADZONE) for the
	 * v8 universal whammy specifically - see hg_ctrl.h. Both go through the
	 * identical swap/invert reorientation so "which raw channel is Y" stays
	 * correct regardless of JOY_SWAP_XY/JOY_INVERT_Y. */
	float rawX = joyAxis(ainX, gJoyCenX, HG_JOY_DEADZONE);
	float rawY = joyAxis(ainY, gJoyCenY, HG_JOY_DEADZONE);
	float rawYw = joyAxis(ainY, gJoyCenY, HG_WHAMMY_DEADZONE);
	float rawXw = joyAxis(ainX, gJoyCenX, HG_WHAMMY_DEADZONE);
	if(JOY_SWAP_XY) {
		float t;
		t = rawX; rawX = rawY; rawY = t;
		t = rawXw; rawXw = rawYw; rawYw = t;
	}
	gJoyX = JOY_INVERT_X ? -rawX : rawX;
	gJoyY = JOY_INVERT_Y ? -rawY : rawY;
	float whammyY = JOY_INVERT_Y ? -rawYw : rawYw;
	gHgJoyXv = gJoyX;
	gHgJoyYv = gJoyY;

	/*
	 * UNIVERSAL PITCH WHAMMY (v8, 2026-10-27). UP/DOWN always bends pitch,
	 * regardless of what's engaged - "a holistic whammy bar across
	 * everything" (EMGOR). This is a MULTIPLICATIVE offset computed in
	 * dsp_core.cpp's updateModulation() (gJoyWhammySemis), exactly like BEND
	 * - it compounds on top of PITCH's own selected interval instead of
	 * overwriting gPitchSemitones, so the two never fight. Smoothed (slew,
	 * not stepped) for "high quality... smooth ramp"; EXACTLY 0 inside the
	 * small whammy deadzone, so a centred stick costs nothing - see
	 * gWetEngineActive in render.cpp/dsp_core.cpp for the actual resource
	 * gate this drives.
	 */
	{
		static float sWhammySmooth = 0.0f;
		/* headroom (v8.3): SATURATE of full throw already reads as full
		 * deflection - see HG_WHAMMY_SATURATE in hg_ctrl.h for why. */
		float sat = whammyY / HG_WHAMMY_SATURATE;
		if(sat > 1.0f) sat = 1.0f;
		if(sat < -1.0f) sat = -1.0f;
		/* response curve (v8.1): gentle near centre, full range only at
		 * full throw - see HG_WHAMMY_CURVE in hg_ctrl.h for why. */
		float shaped = (sat < 0.0f ? -1.0f : 1.0f)
				* powf(fabsf(sat), HG_WHAMMY_CURVE);
		float target = shaped * HG_JOY_PITCH_SEMIS;   /* +/-1 octave at full throw */
		sWhammySmooth += (target - sWhammySmooth) * 0.08f;
		/* snap the tail end of the ramp to exact 0 - an asymptotic decay
		 * never quite reaches it, which would pin the wet engine open
		 * forever at an inaudible residual offset (see gWetEngineActive). */
		if(whammyY == 0.0f && fabsf(sWhammySmooth) < 0.01f)
			sWhammySmooth = 0.0f;
		gJoyWhammySemis = sWhammySmooth;
	}

	/* LEFT/RIGHT = FORMANT, same contract. Together they are the matrix: a
	 * diagonal push bends pitch and vowel at once, which is the whole point. */
	if(gFormantActive) {
		if(gJoyX == 0.0f) {
			gHgFormantBase = gFormantVal;
			gJoyHeldFormant = 0;
		} else {
			if(!gJoyHeldFormant) {
				gHgFormantBase = gFormantVal;
				gJoyHeldFormant = 1;
			}
			float v = gHgFormantBase + gJoyX * HG_JOY_FORMANT_RANGE;
			/* keep inside the range the pads themselves use */
			gFormantVal = v < 0.45f ? 0.45f : (v > 1.90f ? 1.90f : v);
		}
	} else {
		gJoyHeldFormant = 0;
	}
}

/*
 * v8: there is ONE physical encoder (pins 0/1/2). The old ENC1-vs-ENC2
 * "which one is fitted" identification problem this used to solve doesn't
 * exist any more - there is no second encoder to confuse it with - so this
 * is now just a plain rotation/click activity log, not a diagnostic.
 */
void hgEncDiag(int mainDelta, int mainClick)
{
	if(!gHgEncDiag)
		return;
	if(mainDelta)
		rt_printf("encoder rotated %+d\n", mainDelta);
	if(mainClick)
		rt_printf("encoder CLICK\n");
}
