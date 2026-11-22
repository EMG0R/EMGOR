/*
 * hg_ctrl.cpp - TRS expression pedal, joystick whammy, encoder diagnostic.
 * See hg_ctrl.h for the pin map and the reasoning behind the arm-at-zero and
 * the offset-not-value joystick.
 */
#include "hyperguitar.h"
#include "hg_ctrl.h"
#include <cmath>

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
 * Whammy bases. While the stick is centred these TRACK the live value, so the
 * pads and encoders stay in charge; while it is pushed they hold, and the live
 * value becomes base + offset. Releasing therefore restores exactly the sound
 * that was there before, with no memory of how it got there.
 */
static float gHgPitchBase   = 0.0f;
/* NOTE the gHg prefix: hyperguitar.h already declares a per-string
 * `volatile float gFormantBase[NUM_STRINGS]` — a completely different thing
 * (the tracker's per-string formant reference). Shadowing it was a
 * redefinition error, and had it compiled it would have been miserable to
 * read. Same reason gHgPitchBase is prefixed. */
static float gHgFormantBase = 1.0f;
static int   gJoyHeldPitch = 0, gJoyHeldFormant = 0;

void hgCtrlSetup(void)
{
	gExprArmed = 0;
	gExprVal = gExprSmooth = 0.0f;
	gJoyInit = 0;
	gJoyHeldPitch = gJoyHeldFormant = 0;
	gJoySwInit = 0;
	gHgJoyClick = 0;
	gHgJoyHeld = 0;
	rt_printf("hg-ctrl: expression pedal DISARMED - sweep it to heel once to "
			"arm it (an unplugged jack floats mid-scale and never will)\n");
}

/* deadzone + rescale so the usable travel still spans the full -1..+1 */
static inline float joyAxis(float raw, float centre)
{
	float d = raw - centre;
	float span = (d >= 0.0f) ? (1.0f - centre) : centre;
	if(span < 1e-3f)
		return 0.0f;
	float n = d / span;                       /* -1..+1 before the deadzone */
	if(fabsf(n) < HG_JOY_DEADZONE)
		return 0.0f;                          /* EXACTLY zero, see header */
	float s = (n > 0.0f) ? 1.0f : -1.0f;
	return s * (fabsf(n) - HG_JOY_DEADZONE) / (1.0f - HG_JOY_DEADZONE);
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
		 * WHERE THE PEDAL GOES depends on what is switched on, which is the
		 * "whammy or formant" behaviour asked for. Pitch wins when both are
		 * live: a pitch whammy is the louder gesture and having the pedal do
		 * two things at once is not playable.
		 */
		if(gPitchActive) {
			gPitchSemitones = -HG_JOY_PITCH_SEMIS
					+ gExprVal * (2.0f * HG_JOY_PITCH_SEMIS);
			gPitchWetPct = 100.0f;
		} else if(gFormantActive) {
			gFormantVal = 0.55f + gExprVal * (1.60f - 0.55f);
		}
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
	 * them, so the rotation is resolved once, here, and never again. */
	float rawX = joyAxis(ainX, gJoyCenX);
	float rawY = joyAxis(ainY, gJoyCenY);
	if(JOY_SWAP_XY) { float t = rawX; rawX = rawY; rawY = t; }
	gJoyX = JOY_INVERT_X ? -rawX : rawX;
	gJoyY = JOY_INVERT_Y ? -rawY : rawY;
	gHgJoyXv = gJoyX;
	gHgJoyYv = gJoyY;

	/*
	 * UP/DOWN = PITCH, but only while the pitch shifter is on. With it off the
	 * stick is inert rather than silently arming something the player cannot
	 * hear - "just for when the pitch and/or formant shifter is on".
	 */
	/* UP = +1 octave, DOWN = -1 octave, centre = exactly the preset value.
	 * HG_JOY_PITCH_SEMIS is 12, so full throw is an octave either way. */
	if(gPitchActive) {
		if(gJoyY == 0.0f) {
			gHgPitchBase = gPitchSemitones;   /* centred: follow the pads */
			gJoyHeldPitch = 0;
		} else {
			if(!gJoyHeldPitch) {
				gHgPitchBase = gPitchSemitones;   /* latch on leaving centre */
				gJoyHeldPitch = 1;
			}
			gPitchSemitones = gHgPitchBase + gJoyY * HG_JOY_PITCH_SEMIS;
		}
	} else {
		gJoyHeldPitch = 0;
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

void hgEncDiag(int mainDelta, int auxDelta, int mainClick, int auxClick)
{
	if(!gHgEncDiag)
		return;
	if(mainDelta)
		rt_printf("ENC-DIAG: ENC1 (main, pins 0/1/2) rotated %+d\n", mainDelta);
	if(auxDelta)
		rt_printf("ENC-DIAG: ENC2 (aux,  pins 3/4/5) rotated %+d\n", auxDelta);
	if(mainClick)
		rt_printf("ENC-DIAG: ENC1 (main, pin 2) CLICK\n");
	if(auxClick)
		rt_printf("ENC-DIAG: ENC2 (aux,  pin 5) CLICK\n");
}
