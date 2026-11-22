/*
 * trellis_ui.cpp - NeoTrellis 4x4 pad grid over /dev/i2c-1: seesaw
 * register I/O, LED rendering, pad-press dispatch, autosave throttle.
 *
 * v6 CATEGORY MENU. Column 0 (pads 0/4/8/12) is a four-entry menu, one
 * pad per category; the other 12 pads are the FX slots of whichever
 * category is live. See the geometry comment in hyperguitar.h.
 *
 * Two things drove this rewrite, both of them playability bugs in v5:
 *   1. The pad->effect map was hard-coded in FOUR places (press dispatch,
 *      lit-pad test, preset render, encoder dispatch). Adding an effect
 *      meant editing all four and they drifted. There is now exactly ONE
 *      map - gCatSlot[][] in presets.cpp - and one table, gFx[] below.
 *   2. gArmedPad was INVISIBLE. You could not tell which effect the
 *      encoders were driving without turning a knob and listening. The
 *      armed pad now breathes; see the render loop.
 *
 * A slot carries a VOICING as well as an effect, so several slots on a
 * page can share one effect id. Presses are therefore radio-group-ish,
 * not plain toggles, and gold marks the live voicing rather than the
 * live effect - see gActiveSlot[] below for both halves of that.
 */
#include "hyperguitar.h"
#include "hg_link.h"
#include "i2c_sensors.h"
#include "hg_mon.h"
#include <cmath>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

static int ssWrite(uint8_t mod, uint8_t fn, const uint8_t *data, int len)
{
	uint8_t buf[64] = { mod, fn };
	if(len > 60) return -1;
	if(len) memcpy(buf + 2, data, len);
	return write(gI2cFd, buf, 2 + len) == 2 + len ? 0 : -1;
}

static int ssRead(uint8_t mod, uint8_t fn, uint8_t *out, int len)
{
	uint8_t reg[2] = { mod, fn };
	if(write(gI2cFd, reg, 2) != 2) return -1;
	usleep(1000);
	return read(gI2cFd, out, len) == len ? 0 : -1;
}

/* ===================== EFFECT DISPATCH TABLE =====================
 *
 * One row per effect id, indexed BY the id - gFx[PAD_VIB] is vibrato.
 * A press resolves pad -> slot -> effect id through gCatSlot[][], then
 * does the same two things for every effect: set the on/off flag and run
 * the apply hook if it has one. No per-pad branching anywhere.
 *
 * The PARAMETER side is not this table's job - applySlotPreset() owns it,
 * because the voicing lives in the slot, not in the effect. This table
 * only knows "is it on" and "who needs to recompute".
 *
 * `log` exists only because a few effects print their current parameter
 * values on toggle, which is genuinely useful at the bench when you are
 * trying to remember what the encoders left an effect set to. NULL means
 * the slot label line is the whole story.
 */
typedef struct {
	volatile int *flag;     /* the effect's global on/off */
	void (*apply)(void);    /* recompute hook, NULL if none */
	const char *name;       /* console label */
	void (*log)(void);      /* optional detailed console line */
} FxEntry;

static void logPitch(void)
{
	rt_printf("pitch %d semi=%.1f cents=%.0f mode=%s wet=%.0f%%\n",
			gPitchActive, gPitchSemitones, gPitchCents,
			gPitchCentsMode ? "cents" : "semi", gPitchWetPct);
}

static void logFm(void)
{
	rt_printf("fm %d ratio=%.2f idx=%.2f mode=%s\n",
			gFmActive, gFmRatioVal, gFmIndexVal,
			gFmQuantized ? "quant" : "free");
}

static void logFormant(void)
{
	rt_printf("formant %d val=%.2f glide=%.3f\n",
			gFormantActive, gFormantVal, gFormantGlide);
}

static void logSine(void)
{
	rt_printf("synth %d [%s] %s (main=timbre, click=mono/poly, aux=level)\n",
			gSineOn, SYNTH_NAMES[gSynthMode],
			gSynthPoly ? "POLY" : "MONO");
}

static void logSitar(void)
{
	rt_printf("SITAR TONE %d (buzz=%.2f sus=%.4f)\n",
			gSitarOn, gSitarBuzz, gSitarRing);
}

static void logPan(void)
{
	rt_printf("stereo-spread %d (spread=%.2f dyn=%.2f)\n",
			gPanOn, gPanSpread, gPanDyn);
}

/*
 * Rows are POSITIONAL - the order here must match the PAD_* ids in
 * hyperguitar.h exactly, because the table is indexed by id. The
 * static_asserts below pin the two ends of that range so a reordered
 * or inserted id fails the build instead of silently toggling the
 * wrong effect mid-gig.
 */
static const FxEntry gFx[NPIX] = {
	{ &gPitchActive,   applyPitch,      "pitch",        logPitch   }, /*  0 PAD_PITCH    */
	{ &gFmActive,      NULL,            "fm",           logFm      }, /*  1 PAD_FM       */
	{ &gFormantActive, applyFormant,    "formant",      logFormant }, /*  2 PAD_FORMANT  */
	{ &gSineOn,        NULL,            "synth",        logSine    }, /*  3 PAD_SINE     */
	{ &gVibOn,         NULL,            "vibrato",      NULL       }, /*  4 PAD_VIB      */
	{ &gChorusOn,      NULL,            "chorus",       NULL       }, /*  5 PAD_CHORUS   */
	{ &gEnvFOn,        NULL,            "autowah",      NULL       }, /*  6 PAD_ENVF     */
	/* formant-wah re-routes the wet/dry legs, so it MUST re-run mix routing */
	{ &gFWahOn,        applyMixRouting, "formantwah",   NULL       }, /*  7 PAD_FWAH     */
	{ &gSagOn,         NULL,            "plucksag",     NULL       }, /*  8 PAD_PLUCKSAG */
	{ &gPogOn,         NULL,            "pog",          NULL       }, /*  9 PAD_POG      */
	{ &gResOn,         NULL,            "resonator",    NULL       }, /* 10 PAD_RES      */
	{ &gSusOn,         NULL,            "sustainer",    NULL       }, /* 11 PAD_SUS      */
	{ &gJawOn,         NULL,            "jawari",       NULL       }, /* 12 PAD_JAWARI   */
	{ &gSympOn,        NULL,            "sympathetic",  NULL       }, /* 13 PAD_SYMP     */
	{ &gSitarOn,       NULL,            "sitar",        logSitar   }, /* 14 PAD_SITAR    */
	{ &gPanOn,         NULL,            "stereo-spread",logPan     }, /* 15 PAD_PAN      */
};

static_assert(PAD_PITCH == 0, "gFx[] is indexed by effect id");
static_assert(PAD_FWAH == 7,  "gFx[] row order drifted from the PAD_* ids");
static_assert(PAD_PAN == 15,  "gFx[] must cover every effect id, 0..NPIX-1");


/* ===================== ACTIVE VOICING PER EFFECT =====================
 *
 * A slot is an effect AND a voicing, so several slots in a category can
 * carry the SAME effect id (four FORMANT ratios, say). A plain toggle
 * would then be unplayable: hitting FORMANT 1.30 while FORMANT 0.75 was
 * running would switch the effect OFF instead of changing the vowel.
 * v5's preset pads behaved as RADIO GROUPS and that is the feel we want
 * back, so we remember which slot's voicing is live for each effect:
 *
 *   different slot, same effect, effect on -> re-voice (no toggle)
 *   same slot, effect on                   -> off
 *   effect off                             -> on, with this slot's voicing
 *
 * The value is the slot's GLOBAL key, cat*SLOTS_PER_CAT + slot, so an
 * effect that appears on two pages still lights exactly one pad. -1 means
 * "no voicing chosen yet" - the state after boot, or after loading a
 * state file that had the effect on. In that case the first press adopts
 * that slot's voicing rather than toggling off, which is the only
 * sensible reading of "I pressed a voicing I have not selected yet".
 *
 * Deliberately NOT persisted: it is transient UI state, derivable by
 * pressing a pad, and saving it would only create a way for the LEDs to
 * disagree with the audio after a restart.
 */
static short gActiveSlot[NPIX] = {
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
};

#define SLOT_KEY(cat, slot) ((short)((cat) * SLOTS_PER_CAT + (slot)))

/*
 * IS THIS SLOT THE ONE YOU ARE HEARING?
 *
 * ONE predicate, used by BOTH the press dispatch and the LED render. They used
 * to have their own copies and they disagreed: the LEDs treated act < 0
 * ("effect on, voicing unknown") as "light every slot", while the press
 * handler treated it as "match nothing". The result was a grid full of lit
 * pads where pressing a lit pad did NOT switch the effect off - it re-voiced
 * instead. act < 0 happens after a state restore and after any change that
 * came from NEPTR rather than a pad, so it was common and looked random.
 *
 * The rule now: a pad that is LIT can always be pressed to turn its effect
 * off. What you see is what pressing it does.
 */
#define SLOT_IS_LIVE(fx, key) \
	(*gFx[fx].flag && (gActiveSlot[fx] == (key) || gActiveSlot[fx] < 0))

/*
 * The NEPTR link's way in to gFx[]. It lives HERE, next to the table, rather
 * than in hg_lanes.cpp, for the reason this file's header comment gives: the
 * pad->effect map existed in four places in v5 and they drifted. gFx[] stays
 * static and there is still exactly one place that knows how to switch an
 * effect on - so formant-wah's applyMixRouting() hook fires for a NEPTR
 * encoder exactly as it does for a pad press, with nothing to keep in step.
 *
 * PAD_PAN is NOT exempt from an explicit off here, unlike in the pad dispatch.
 * That exemption exists because a PAN pad press is a re-voice gesture and
 * toggling the stereo spread off by accident is never what the player meant;
 * an explicit fxSetActive(PAD_PAN, 0) from a lane is not an accident. No lane
 * currently carries PAD_PAN, so this is a note for whoever adds one.
 */
int fxIsOn(int fx)
{
	if(fx < 0 || fx >= NPIX)
		return 0;
	return *gFx[fx].flag ? 1 : 0;
}

/*
 * What the LED of one slot is showing, for the web dashboard: bit 0 = lit
 * (this is the live voicing), bit 1 = breathing (the encoders are on it).
 * Same SLOT_IS_LIVE rule and same armed test as the LED render in
 * trellisTick(), so the dashboard can never light a pad the grid does not.
 * fxIsOn() alone was per-EFFECT: pressing FORM 0.75 lit all seven FORMANT
 * pads on the dashboard while the grid lit one.
 */
int fxSlotLed(int cat, int slot)
{
	if(cat < 0 || cat >= NUM_CATEGORIES || slot < 0 || slot >= SLOTS_PER_CAT)
		return 0;
	const FxSlot *sl = &gCatSlot[cat][slot];
	if(sl->fx == FX_NONE)
		return 0;
	if(sl->fx == PAD_USB)
		return gUsbAudio ? 1 : 0;
	short key = SLOT_KEY(cat, slot);
	short act = gActiveSlot[sl->fx];
	int st = SLOT_IS_LIVE(sl->fx, key) ? 1 : 0;
	if(sl->fx == gArmedPad && (act == key || act < 0))
		st |= 2;
	return st;
}

void fxSetActive(int fx, int on)
{
	if(fx < 0 || fx >= NPIX)
		return;
	const FxEntry *e = &gFx[fx];
	*e->flag = on ? 1 : 0;
	if(e->apply)
		e->apply();
	/*
	 * The armed-voicing marker is cleared, not guessed. A lane entry is not a
	 * slot, so there is no SLOT_KEY to record; leaving a stale one would light
	 * the wrong pad gold when the Trellis comes back. -1 is the honest answer
	 * ("on, via no pad"), and is exactly the state gActiveSlot documents for a
	 * restored-but-unpressed effect.
	 */
	if(on)
		gActiveSlot[fx] = -1;
}

/* ========================= LED HELPERS ========================= */

/*
 * Pre-gamma linear RGB in, gamma-corrected bytes out. Everything on the
 * grid goes through here so there is one place where the gamma curve is
 * applied - the v5 code had it duplicated in two palette functions with
 * subtly different clamping.
 */
static void padColor(float R, float G, float B, uint8_t *r, uint8_t *g, uint8_t *b)
{
	int ri = (int)(R + 0.5f), gi = (int)(G + 0.5f), bi = (int)(B + 0.5f);
	if(ri < 0) ri = 0; if(ri > 255) ri = 255;
	if(gi < 0) gi = 0; if(gi > 255) gi = 255;
	if(bi < 0) bi = 0; if(bi > 255) bi = 255;
	*r = gGammaLUT[ri];
	*g = gGammaLUT[gi];
	*b = gGammaLUT[bi];
}

/*
 * An EMPTY slot (slot fx == FX_NONE) is background purple knocked right
 * down. Not black: a dead-black pad reads as "the grid crashed", whereas
 * a barely-there purple reads as "nothing assigned here", which is the
 * truth. Categories are not all full and never will be.
 */
#define COL_EMPTY_DIM 0.10f

/* armed-pad breathe: ~0.9Hz, deliberately slower than any tremolo you
 * would be playing over, so it never fights the music visually */
#define ARM_PULSE_HZ  0.9f
#define ARM_LIFT      0.40f   /* peak multiplicative brightness lift */
#define ARM_WHITE     0.16f   /* peak lerp toward white, keeps gold distinct */

/* Sensors live on this task too. They share /dev/i2c-1 with the pad grid, and
 * two aux tasks doing ioctl(I2C_SLAVE)+read/write on one bus WILL eventually
 * interleave and corrupt each other's transactions. Running both from a single
 * task serialises them by construction - no lock, nothing to get wrong.
 *
 * It also means the sensors keep working when the pad grid is switched off:
 * the early return below is deliberately AFTER the sensor read. */
/*
 * LAVA LAMP BACKGROUND (2026-09-23). Every pad that is not showing state -
 * populated-but-off slots and empty slots - glows with a slow drifting lava
 * field instead of flat purple. Three soft blobs wander the 4x4 grid on
 * incommensurate Lissajous paths, so the pattern never visibly repeats, and
 * the palette drifts between violet, magenta and indigo.
 *
 * Deliberately always moving: the LED current is never constant, which makes
 * any supply/ground coupling into the audio audible straight away. It is a
 * test signal as much as decoration.
 *
 * Live voicings (solid blue), the armed breathe and the blue menu column are
 * drawn on top unchanged, so the grid still reads exactly as before.
 */
#define LAVA_LEVEL      0.50f   /* peak lava; lit pads stay clearly brighter */
#define LAVA_FLOOR      0.30f   /* never dark between blobs: the whole grid glows */

static void lavaColor(int pad, float t, float *R, float *G, float *B)
{
	float x = (float)(pad % 4), y = (float)(pad / 4);
	float field = 0.0f;
	for(int k = 0; k < 3; k++) {
		float cx = 1.5f + 1.7f * sinf(t * (0.19f + 0.05f * k) + k * 2.1f);
		float cy = 1.5f + 1.7f * sinf(t * (0.27f - 0.04f * k) + k * 1.3f + 0.7f);
		float dx = x - cx, dy = y - cy;
		field += 1.0f / (1.0f + 0.9f * (dx * dx + dy * dy));
	}
	float v = field * 0.75f;
	if(v > 1.0f) v = 1.0f;
	v = v * v * (3.0f - 2.0f * v);               /* smoothstep: soft blob edges */
	v = LAVA_FLOOR + (1.0f - LAVA_FLOOR) * v;

	/* palette: light purple <-> light blue, with a violet undertow.
	 * h drifts slowly across the grid, m slower still. */
	float h = 0.5f + 0.5f * sinf(t * 0.07f + x * 0.45f - y * 0.30f);
	float m = 0.5f + 0.5f * sinf(t * 0.045f + 2.0f + y * 0.35f - x * 0.2f);
	/* light purple (200,130,255) -> light blue (110,180,255) */
	float r = 200.0f - 90.0f * h, g = 130.0f + 50.0f * h, b = 255.0f;
	/* violet undertow (150,60,255) */
	r += (150.0f - r) * 0.45f * m;
	g += ( 60.0f - g) * 0.45f * m;
	*R = r * v * LAVA_LEVEL;
	*G = g * v * LAVA_LEVEL;
	*B = b * v * LAVA_LEVEL;
}

void trellisTick(void *)
{
	/* The task runs at ~30 Hz for the LEDs; gHgSensorDiv divides that down to
	 * the configured sensor rate. Polling slowly is a NOISE decision, not a
	 * performance one - each I2C burst is audible on this rig. */
	/* Web-UI telemetry rides this task too: monTick() does UDP socket I/O,
	 * which has no business on the audio thread. render() only accumulates
	 * the running means; the conversion to dB and the send happen here. */
	{
		float inDb[10], outDb[2];
		for(int i = 0; i < 10; i++)
			inDb[i] = 20.0f * logf(sqrtf(gMonInRms[i]) + 1e-9f) / logf(10.0f) * 1.0f;
		for(int i = 0; i < 2; i++)
			outDb[i] = 20.0f * logf(sqrtf(gMonOutRms[i]) + 1e-9f) / logf(10.0f);
		monTick(inDb, outDb);
	}

	static int sSensDiv = 0;
	if(++sSensDiv >= gHgSensorDiv) {
		sSensDiv = 0;
		sensorsRead(&gHgSens);
	}

	if(!gTrellisOk || gTrellisDisabled)
		return;

	uint8_t count = 0;
	if(ssRead(SS_KEYPAD, SS_KEY_COUNT, &count, 1) == 0 && count > 0) {
		if(count > 16) count = 16;
		uint8_t ev[16];
		if(ssRead(SS_KEYPAD, SS_KEY_FIFO, ev, count) == 0) {
			for(int i = 0; i < count; i++) {
				int edge = ev[i] & 0x03;
				int k = ev[i] >> 2;
				int pad = (k / 8) * 4 + (k % 8);
				if(pad < 0 || pad >= NPIX || edge != 3)
					continue;

				if(IS_MENU_PAD(pad)) {
					// PAGE SWITCH ONLY. A menu press must never touch
					// audio: no toggle, no re-arm. You have to be able
					// to look for an effect mid-song without changing
					// the sound or stealing the encoders from whatever
					// they were driving.
					gCurCategory = MENU_CATEGORY(pad);
					gPadFlash[pad] = 1.0f;
					gStateDirty = 1;
					continue;
				}

				const FxSlot *sl = &gCatSlot[gCurCategory][PAD_SLOT(pad)];
				if(sl->fx == FX_NONE)
					continue;   // empty slot: dead to the touch, no flash

				/* USB AUDIO pad: a switch, not an effect. Whole grid flashes
				 * green = on, red = off. Never arms anything. */
				if(sl->fx == PAD_USB) {
					int on = monUsbToggle();
					gEncFlashWho = on ? 3 : 4;
					gEncFlashCount = 15;
					gPadFlash[pad] = 1.0f;
					continue;
				}

				int fx = sl->fx;
				const FxEntry *e = &gFx[fx];
				short key = SLOT_KEY(gCurCategory, PAD_SLOT(pad));
				// The slot LABEL is the useful log line - it names the
				// voicing, not just the effect ("FORMANT 0.75", not
				// "formant"). e->name is only the fallback for a slot
				// table entry that forgot one.
				const char *lbl = sl->label ? sl->label : e->name;

				// gArmedPad is an EFFECT ID in v6, not a pad index - the
				// same effect can sit at different pads as pages change,
				// and the encoders must follow the effect, not the hole.
				// Armed on EVERY press, the off press included: you often
				// flick something off and immediately want to re-dial it
				// before bringing it back.
				gArmedPad = fx;
				potArm(fx);      /* pot pickup restarts from here */

				// PAD_PAN is exempt from toggle-off: the stereo spread is
				// part of the instrument's identity, always on. Its slots
				// still re-voice (narrow/wide/flyout select a spread), a
				// same-slot press just re-applies the voicing.
				if(SLOT_IS_LIVE(fx, key) && fx != PAD_PAN) {
					// The SAME voicing pressed again - the only path that
					// turns an effect off. gActiveSlot is left alone so the
					// next press comes back with the voicing you were last
					// using rather than dumping you at slot defaults.
					*e->flag = 0;
					if(e->apply)
						e->apply();
					/* tell NEPTR, so its screen does not keep showing this
					 * lane lit after the player switched it off on the grid */
					hgReportFx(fx, sl->p1, sl->p2, 0);
					rt_printf("%s/%s off\n", gCatName[gCurCategory], lbl);
				} else {
					// ON, or a re-voice of an already-on effect. Both do the
					// same work - the flag write is simply a no-op in the
					// re-voice case - which is exactly why they share a path.
					int wasOn = *e->flag;
					*e->flag = 1;
					gActiveSlot[fx] = key;
					// Voicing FIRST, then the effect's recompute hook, so the
					// hook sees this slot's p1/p2 and not the previous ones.
					applySlotPreset(sl);
					if(e->apply)
						e->apply();
					/* same, for an engage or a re-voice: NEPTR mirrors
					 * the guitar rather than only its own menu */
					hgReportFx(fx, sl->p1, sl->p2, 1);
					rt_printf("%s/%s %s\n", gCatName[gCurCategory], lbl,
							wasOn ? "voicing" : "on");
					// The per-effect detail line still earns its keep: the
					// label says which voicing was picked, the log says what
					// the encoders have since done to it.
					if(e->log)
						e->log();
				}

				gPadFlash[pad] = 1.0f;
				gStateDirty = 1;
			}
		}
	}

	gLavaT += 1.0f / 30.0f;   // 30Hz tick: gLavaT is the animation clock, in seconds
	float pulse = 0.5f + 0.5f * sinf(2.0f * (float)M_PI * ARM_PULSE_HZ * gLavaT);

	/* WHOLE-GRID FLASH (encoder axis, synth mono/poly, USB on/off). It used
	 * to bypass the normal render and write all 16 LEDs at FULL brightness
	 * in one 50-byte I2C burst: ~4x the grid's normal current in a single
	 * step (on the Bela's USB-fed 5 V - the board browned out and rebooted
	 * on an encoder click, 2026-09-25) and a write past the seesaw's ~32-byte
	 * buffer. Now it only picks the colour; the pad loop below paints it
	 * through the same brightness scaling and 24-byte chunking as
	 * everything else, so a flash never draws more than the normal grid. */
	int encFlash = 0;
	float flR = 0.0f, flG = 0.0f, flB = 0.0f;
	if(gEncFlashCount > 0) {
		gEncFlashCount--;
		encFlash = 1;
		if(gEncFlashWho == 3)      { flR = 0;   flG = 230; flB = 60; }  /* USB on */
		else if(gEncFlashWho == 4) { flR = 255; flG = 20;  flB = 20; }  /* USB off */
		else if(gEncFlashWho == 1) { flR = 0;   flG = 220; flB = 40; }  /* green */
		else                       { flR = 255; flG = 200; flB = 0;  }  /* yellow */
	}

	uint8_t frame[NPIX * 3];
	int flashing = gModeFlash > 0;
	if(flashing) gModeFlash--;
	for(int i = 0; i < NPIX; i++) {
		// Every pad starts from background purple; the cases below either
		// scale it or replace it outright.
		/* LAVA UNDER EVERYTHING: every pad starts from the lava field; the
		 * cases below either keep it (nothing to show) or draw state on top. */
		float LR, LG, LB;
		lavaColor(i, gLavaT, &LR, &LG, &LB);
		float R = LR, G = LG, B = LB;
		int armed = 0;
		int lava = 1;   /* 1 = this pad is showing only the lava background */

		if(IS_MENU_PAD(i)) {
			// BLUE = navigation. Never gold, never purple, so the menu
			// column is unmistakable at a glance on a dark stage.
			if(MENU_CATEGORY(i) == gCurCategory) {
				R = COL_MENU_R; G = COL_MENU_G; B = COL_MENU_B;
				lava = 0;
			} else {
				/* other pages: lava with a blue tint over it */
				R = fmaxf(LR, COL_MENU_R * COL_MENU_DIM);
				G = fmaxf(LG, COL_MENU_G * COL_MENU_DIM);
				B = fmaxf(LB, COL_MENU_B * COL_MENU_DIM);
			}
		} else {
			const FxSlot *sl = &gCatSlot[gCurCategory][PAD_SLOT(i)];
			if(sl->fx == FX_NONE) {
				/* empty slot: lava only */
			} else if(sl->fx == PAD_USB) {
				if(gUsbAudio) {             /* lit exactly like an ON effect */
					R = COL_GOLD_R; G = COL_GOLD_G; B = COL_GOLD_B;
					lava = 0;
				}
			} else {
				// GOLD IS PER-VOICING, NOT PER-EFFECT. With four FORMANT
				// slots on one page, lighting all four whenever formant is
				// on would tell you the effect is running but hide WHICH
				// vowel - the one thing you actually need to see. The other
				// slots of a running effect stay populated-but-off purple,
				// i.e. "available, not the one you are hearing".
				short key = SLOT_KEY(gCurCategory, PAD_SLOT(i));
				short act = gActiveSlot[sl->fx];
				// act < 0 = effect on but no voicing chosen yet (restored
				// from a state file). We cannot know which slot is live, so
				// fall back to lighting them all rather than lying with a
				// dark grid over an audibly running effect.
				int live = SLOT_IS_LIVE(sl->fx, key);
				if(live) {
					R = COL_GOLD_R; G = COL_GOLD_G; B = COL_GOLD_B;
					lava = 0;
				}
				// Breathe follows the same rule: the encoders drive one
				// effect, and its live voicing is where that lands.
				armed = (sl->fx == gArmedPad) && (act == key || act < 0);
			}
		}

		// ARMED = the effect the encoders are driving. A slow breathe,
		// not a colour change: colour already means on/off, and an
		// armed-but-OFF effect is a real and common state (you arm it,
		// dial it in, then turn it on). Motion is the only channel left
		// that does not collide with either meaning.
		if(armed) {
			float lift = 1.0f + ARM_LIFT * pulse;
			float w = ARM_WHITE * pulse;
			R = R * lift; G = G * lift; B = B * lift;
			R += (255.0f - R) * w;
			G += (255.0f - G) * w;
			B += (255.0f - B) * w;
		}

		// see gTrellisBright in hg_mon.cpp: scales LED current only, leaves
		// the I2C traffic identical - the A/B that separates the two theories
		/*
		 * LOW-CURRENT RENDER. Nearly all the LED current is the background
		 * purple burning on all 16 pads at once, not the one or two pads that
		 * are actually ON - and LED current is the hum (proven: hot-unplug
		 * silenced it with the I2C writes still running; 3.3 V -> 5 V made it
		 * far worse). So the saving is taken out of the BACKGROUND.
		 *
		 * Blacking those pads out entirely (gTrellisDark) does work, and cut
		 * the noise - but it also hides populated-but-off slots, which is the
		 * one thing the grid exists to show. Dimming them instead keeps every
		 * state distinguishable while drawing a fraction of the current, and
		 * actually RAISES contrast between an off slot and a live one.
		 */
		if(!armed) {
			int meaningful = IS_MENU_PAD(i)
					? (MENU_CATEGORY(i) == gCurCategory)
					: !lava;
			if(!meaningful) {
				float d = gTrellisDark ? 0.0f : gTrellisOffDim;
				R *= d; G *= d; B *= d;
			}
		}
		if(encFlash) { R = flR; G = flG; B = flB; }
		uint8_t r, g, b;
		padColor(R, G, B, &r, &g, &b);

		// PRESS FEEDBACK: a decaying white pop under the finger. In v5
		// this was also pinned to 1.0 for every lit pad, which slammed
		// every ON pad to pure white and made the palette pointless -
		// gold could never actually be seen. Press flash only, now.
		float fl = gPadFlash[i];
		if(fl > 0.003f) {
			float w = fl * fl;
			r = (uint8_t)(r + (255 - r) * w);
			g = (uint8_t)(g + (255 - g) * w);
			b = (uint8_t)(b + (255 - b) * w);
		}
		gPadFlash[i] *= 0.86f;

		// FULL-GRID WHITE FLASH: overrides everything else, so whatever
		// raises gModeFlash is unmissable. Nothing in this file sets it
		// any more (v6 has no modes to switch between) - kept live for
		// other callers.
		if(flashing)
			r = g = b = 255;

		// GLOBAL BRIGHTNESS: one knob, applied last, after every other
		// effect (palette, pad flash, mode-flash white) - so "dim the
		// whole grid" always means exactly that, not "dim some things."
		// gTrellisBright rides WITH LED_BRIGHTNESS at the very end, after the
		// press flash and the full-grid flash. It was applied earlier before,
		// which meant a pad press still lit at full current with the grid
		// "off" - audible on every click. Last stage is the only place a
		// current control can honestly be called global. bright 0 = dark.
		float lb = LED_BRIGHTNESS * gTrellisBright;
		r = (uint8_t)(r * lb);
		g = (uint8_t)(g * lb);
		b = (uint8_t)(b * lb);

		frame[i * 3 + 0] = g;
		frame[i * 3 + 1] = r;
		frame[i * 3 + 2] = b;
	}
	for(int off = 0; off < NPIX * 3; off += 24) {
		int chunk = NPIX * 3 - off;
		if(chunk > 24) chunk = 24;
		uint8_t pkt[26] = { (uint8_t)(off >> 8), (uint8_t)off };
		memcpy(pkt + 2, frame + off, chunk);
		ssWrite(SS_NEOPIX, 0x04, pkt, 2 + chunk);
	}
	ssWrite(SS_NEOPIX, 0x05, NULL, 0);

	// AUTOSAVE: throttled to ~once/3s and only when something actually
	// changed - trellisTick runs at ~30Hz, and this is a real flash
	// write, so don't do it every tick even though it's cheap per-call.
	static int saveCd = 0;
	if(++saveCd >= 90) {
		saveCd = 0;
		if(gStateDirty) {
			gStateDirty = 0;
			saveState();
		}
	}
}

int trellisInit(void)
{
	gI2cFd = open(TRELLIS_BUS, O_RDWR);
	if(gI2cFd < 0) return -1;
	if(ioctl(gI2cFd, I2C_SLAVE, TRELLIS_ADDR) < 0) return -1;

	uint8_t one = 0xFF;
	ssWrite(SS_STATUS, SS_STATUS_SWRST, &one, 1);
	usleep(150000);
	uint8_t id = 0;
	if(ssRead(SS_STATUS, SS_STATUS_HW_ID, &id, 1) < 0)
		return -1;

	for(int i = 0; i < 256; i++)
		gGammaLUT[i] = (uint8_t)(powf(i / 255.0f, 2.5f) * 255.0f + 0.5f);

	uint8_t pin = 3, speed = 1, len[2] = { 0, NPIX * 3 };
	ssWrite(SS_NEOPIX, 0x01, &pin, 1);
	ssWrite(SS_NEOPIX, 0x02, &speed, 1);
	ssWrite(SS_NEOPIX, 0x03, len, 2);

	for(int i = 0; i < NPIX; i++) {
		uint8_t k = (uint8_t)((i / 4) * 8 + (i % 4));
		uint8_t cfg[2] = { k, (uint8_t)((1 << 4) | 0x01) };
		ssWrite(SS_KEYPAD, SS_KEY_EVENT, cfg, 2);
	}
	return 0;
}
