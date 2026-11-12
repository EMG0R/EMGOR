/*
 * trellis_ui.cpp - NeoTrellis 4x4 pad grid over /dev/i2c-1: seesaw
 * register I/O, LED rendering, pad-press dispatch, autosave throttle.
 *
 * v8 FLAT GRID, no submenus (2026-10-27). pad i -> gPadFx[i], one fixed
 * effect per pad, always - see the geometry diagram in hyperguitar.h. This
 * replaces the v6/v7 4-category menu (archived in
 * bela/archive/v8-category-menu-pre-2026-10-27/).
 *
 * Because pad<->effect is now a BIJECTION (every effect id appears on at
 * most one pad - unlike v6/v7 where several slots could share an effect
 * id with different voicings), the whole gActiveSlot[]/SLOT_KEY/
 * SLOT_IS_LIVE "which voicing is live" machinery that version needed is
 * gone too: "is this pad lit" is simply "is this pad's effect on", full
 * stop. A press is a plain toggle, not a radio group.
 *
 * gFx[] is still the one place that knows how to switch an effect on/off
 * and run its recompute hook - same reason as before: the NEPTR link
 * (hg_lanes.cpp) drives it through fxSetActive() too, so a pad press and
 * an OSC lane change can never disagree about what "effect N, on" means.
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
 * A press resolves pad -> effect id through gPadFx[] directly (v8: no
 * category/slot step any more), then does the same two things for every
 * effect: set the on/off flag and run the apply hook if it has one. No
 * per-pad branching anywhere.
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

static void logXfade(void)
{
	rt_printf("xfade %d rate=%.2fHz depth=%.2f\n", gXfadeOn, gXfadeRate, gXfadeDepth);
}

static void logBend(void)
{
	rt_printf("bend %d depth=%.1fsemi settle=%.2fs\n",
			gBendOn, gBendDepthSemis, gBendSettleSec);
}

/*
 * Rows are POSITIONAL - the order here must match the PAD_* ids in
 * hyperguitar.h exactly, because the table is indexed by id. v8: effect-id
 * space (NUM_EFFECTS=18) now exceeds the physical pad count (NPIX=16) on
 * purpose - PAD_XFADE/PAD_BEND are real rows here with no corresponding
 * "pad == effect id" coincidence any more. The static_asserts pin the two
 * ends of the range so a reordered or inserted id fails the build instead
 * of silently toggling the wrong effect mid-gig.
 */
static const FxEntry gFx[NUM_EFFECTS] = {
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
	/* xfade re-takes the wet/dry targets every block itself (xfadeTick) -
	 * no apply hook needed on toggle ON; toggling OFF needs applyMixRouting
	 * to hand control back, so that's the hook. */
	{ &gXfadeOn,       applyMixRouting, "xfade",        logXfade   }, /* 16 PAD_XFADE    */
	/* bend rides the shifter's wet leg like formant/fwah - must reopen it */
	{ &gBendOn,        applyMixRouting, "bend",         logBend    }, /* 17 PAD_BEND     */
};

static_assert(PAD_PITCH == 0, "gFx[] is indexed by effect id");
static_assert(PAD_FWAH == 7,  "gFx[] row order drifted from the PAD_* ids");
static_assert(PAD_BEND == NUM_EFFECTS - 1, "gFx[] must cover every effect id, 0..NUM_EFFECTS-1");


/* ===================== ON/OFF, NO VOICING TRACKING =====================
 *
 * v8: pad<->effect is a BIJECTION - every effect id in gPadFx[] appears on
 * exactly one pad, unlike v6/v7 where several slots on a page could share
 * one effect id at different voicings (four FORMANT ratios, say) and
 * needed a radio-group so pressing a different voicing didn't just toggle
 * the effect off. That whole problem doesn't exist here: "is this pad's
 * effect on" IS "is this pad lit", so a press is a plain toggle and the
 * gActiveSlot/SLOT_KEY/SLOT_IS_LIVE bookkeeping that used to live here is
 * gone, not reused.
 *
 * gFx[] stays static and is still the one place that knows how to switch
 * an effect on - the NEPTR link (hg_lanes.cpp) drives it through
 * fxSetActive() too, so a pad press and an OSC lane change can never
 * disagree about what "effect N, on" means.
 */
int fxIsOn(int fx)
{
	if(fx < 0 || fx >= NUM_EFFECTS)
		return 0;
	return *gFx[fx].flag ? 1 : 0;
}

/*
 * What pad i's LED is showing, for the web dashboard: bit 0 = lit, bit 1 =
 * breathing (the encoders are armed on it). Same rule as the LED render in
 * trellisTick(), so the dashboard can never show a pad the grid does not.
 */
int fxPadLed(int pad)
{
	if(pad < 0 || pad >= NPIX)
		return 0;
	const FxSlot *sl = &gPadFx[pad];
	if(sl->fx == FX_NONE)
		return 0;
	if(sl->fx == PAD_USB)
		return gUsbAudio ? 1 : 0;
	if(sl->fx == PAD_TRS_DRY)
		return gTrsDryOn ? 1 : 0;
	int st = fxIsOn(sl->fx) ? 1 : 0;
	if(sl->fx == gArmedPad)
		st |= 2;
	return st;
}

void fxSetActive(int fx, int on)
{
	if(fx < 0 || fx >= NUM_EFFECTS)
		return;
	const FxEntry *e = &gFx[fx];
	*e->flag = on ? 1 : 0;
	if(e->apply)
		e->apply();
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

	/* 2026-11-01 tier-A: shifter diagnostics pushed by the audio thread as
	 * compact structs; formatted + printed here, off the audio thread. Text
	 * is identical to the former in-shifter rt_printf lines. Bounded per tick. */
	{
		ShDiagEv e;
		for(int k = 0; k < 16 && shDiagPop(&e); k++) {
			if(e.code == SHDIAG_MODE)
				rt_printf("md s%d %d->%d T%.0f c%.2f nl%d rl%d g%d E%.1e\n", e.s,
						e.i[0], e.i[1], e.f[0], e.f[1], e.i[2], e.i[3], (int)e.f[2], e.f[3]);
			else if(e.code == SHDIAG_STATUS)
				rt_printf("st s%d T%.0f c%.2f m%d e%.3f fA%.2f g%d E%.1e\n", e.s,
						e.f[0], e.f[1], e.i[0], e.f[2], e.f[3], e.i[1], e.f[4]);
			else if(e.code == SHDIAG_ONSET)
				rt_printf("on s%d t%u age%u envF%.4f envS%.4f env%.4f oth%.4f T%.0f c%.2f\n", e.s,
						(unsigned int)e.i[0], (unsigned int)e.i[1], e.f[0], e.f[1], e.f[2],
						e.f[3], e.f[4], e.f[5]);
			else if(e.code == SHDIAG_SYN_ON)
				rt_printf("syn s%d ON T%.0f c%.2f lvl%.4f oth%.4f dtN%d why%s\n", e.s,
						e.f[0], e.f[1], e.f[2], e.f[3], e.i[1], synWhyName(e.i[0]));
			else if(e.code == SHDIAG_SYN_OFF)
				rt_printf("syn s%d OFF why%s age%d\n", e.s, synWhyName(e.i[0]), e.i[1]);
		}
		/* perf summary into the live log, once per ~2 s (tick is ~30 Hz) */
		static int sTelCd = 0;
		if(++sTelCd >= 60) {
			sTelCd = 0;
			char tl[200];
			monTelSummary(tl, sizeof(tl));
			rt_printf("%s\n", tl);
		}
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
				pad = 15 - pad;   /* GRID 180, see the render-loop comment below */
				if(pad < 0 || pad >= NPIX || edge != 3)
					continue;

				/* v8: pad -> effect, DIRECTLY. No page, no menu pads. */
				const FxSlot *sl = &gPadFx[pad];
				if(sl->fx == FX_NONE)
					continue;   // open pad: dead to the touch, no flash

				/* USB AUDIO and TRS DRY: hard toggles, not effects. Whole
				 * grid flashes green = on, red = off. Never arm anything. */
				if(sl->fx == PAD_USB) {
					int on = monUsbToggle();
					gEncFlashWho = on ? 3 : 4;
					gEncFlashCount = 15;
					gPadFlash[pad] = 1.0f;
					continue;
				}
				if(sl->fx == PAD_TRS_DRY) {
					gTrsDryOn = !gTrsDryOn;
					gEncFlashWho = gTrsDryOn ? 3 : 4;
					gEncFlashCount = 15;
					gPadFlash[pad] = 1.0f;
					gStateDirty = 1;
					rt_printf("TRS DRY %s\n", gTrsDryOn ? "on" : "off");
					continue;
				}

				int fx = sl->fx;
				const FxEntry *e = &gFx[fx];
				const char *lbl = sl->label ? sl->label : e->name;

				// Armed on EVERY press, the off press included: you often
				// flick something off and immediately want to re-dial it
				// before bringing it back.
				gArmedPad = fx;
				potArm(fx);      /* pot pickup restarts from here */

				// PAD_PAN is exempt from toggle-off: the stereo spread is
				// part of the instrument's identity, always on - pressing
				// it just re-applies its voicing.
				if(fxIsOn(fx) && fx != PAD_PAN) {
					*e->flag = 0;
					if(e->apply)
						e->apply();
					/* tell NEPTR, so its screen does not keep showing this
					 * lane lit after the player switched it off on the grid */
					hgReportFx(fx, sl->p1, sl->p2, 0);
					rt_printf("%s off\n", lbl);
				} else {
					int wasOn = *e->flag;
					*e->flag = 1;
					// Voicing FIRST, then the effect's recompute hook, so the
					// hook sees this slot's p1/p2 and not the previous ones.
					// TWO EXCEPTIONS keep their own live value instead of the
					// table default on re-engage:
					//   TIMBRE - p1/p2 are placeholders (the pot drives
					//   gFormantVal continuously, see timbrePotToFormant()),
					//   so apply the CURRENT pot reading - otherwise a press
					//   could silently land near-unity and look dead.
					//   PITCH - gPitchSemitones should PERSIST across
					//   on/off toggles (EMGOR, 2026-10-27: "it forgets which
					//   ps value i used last"); the table default (-12) is
					//   only for the very first press ever, when
					//   gPitchSemitones is already -12 from boot anyway, so
					//   skipping the reset costs nothing there and fixes
					//   every later re-engage. Never silent either way -
					//   none of the 6 stops is unity.
					if(fx == PAD_FORMANT) {
						gFormantVal = timbrePotToFormant(gHgPot);
						applyFormant();
					} else if(fx == PAD_PITCH) {
						applyPitch();
					} else {
						applySlotPreset(sl);
					}
					if(e->apply)
						e->apply();
					/* same, for an engage: NEPTR mirrors the guitar rather
					 * than only its own menu */
					hgReportFx(fx, sl->p1, sl->p2, 1);
					rt_printf("%s %s\n", lbl, wasOn ? "re-armed" : "on");
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
	for(int phys = 0; phys < NPIX; phys++) {
		/*
		 * GRID 180 (EMGOR, 2026-10-27, "it feels like 180 degrees wrong" -
		 * locked in as a straight 180, no further re-derivation). `phys` is
		 * the physical LED/key position on the seesaw; `i` is the LOGICAL
		 * pad everything else in this file already means by "i" (gPadFx[],
		 * the PITCH readout pads, gPadFlash[], gArmedPad comparisons). The
		 * SAME transform, 15-x, is applied at the only other place physical
		 * and logical meet - the key-press decode above - so a press and
		 * its own LED always agree on which physical key they're talking
		 * about. Self-inverse (15-(15-x)=x), so one constant does both
		 * directions.
		 */
		int i = 15 - phys;
		// Every pad starts from background purple; the cases below either
		// scale it or replace it outright.
		/* LAVA UNDER EVERYTHING: every pad starts from the lava field; the
		 * cases below either keep it (nothing to show) or draw state on top. */
		float LR, LG, LB;
		lavaColor(i, gLavaT, &LR, &LG, &LB);
		float R = LR, G = LG, B = LB;
		int armed = 0;
		int lava = 1;   /* 1 = this pad is showing only the lava background */

		/*
		 * PITCH INTERVAL READOUT (v8): pads 10/11/12 double as a live
		 * meter whenever PITCH is armed - direction by colour (warm
		 * red/orange = a DOWN interval, cool blue/cyan = UP), magnitude by
		 * how many of the three are lit (4th = 1, 5th = 2, octave = 3).
		 * Pads 13/14 stay pure lava always - EMGOR, 2026-10-27: "the other
		 * stuff I'll come back to", kept visually quiet on purpose.
		 */
		if(gArmedPad == PAD_PITCH && i >= 10 && i <= 12) {
			float absSemi = fabsf(gPitchSemitones);
			int rank = (absSemi > 9.0f) ? 3 : (absSemi > 6.0f) ? 2 : 1;
			int lit = (i - 10) < rank;   /* pads 10,11,12 fill left to right */
			if(lit) {
				if(gPitchSemitones < 0.0f) { R = 255; G = 70; B = 15; }  /* down: warm */
				else                       { R = 30;  G = 160; B = 255; } /* up: cool */
				lava = 0;
			}
		} else if(i == 10 || i == 11 || i == 12 || i == 13 || i == 14) {
			/* open pads: lava only, nothing else to show */
		} else {
			const FxSlot *sl = &gPadFx[i];
			if(sl->fx == FX_NONE) {
				/* open pad: lava only */
			} else if(sl->fx == PAD_USB) {
				if(gUsbAudio) {
					R = COL_GOLD_R; G = COL_GOLD_G; B = COL_GOLD_B;
					lava = 0;
				}
			} else if(sl->fx == PAD_TRS_DRY) {
				if(gTrsDryOn) {
					R = COL_GOLD_R; G = COL_GOLD_G; B = COL_GOLD_B;
					lava = 0;
				}
			} else {
				// v8: pad<->effect is a bijection, so "lit" is simply
				// "this pad's effect is on" - no voicing bookkeeping needed.
				if(fxIsOn(sl->fx)) {
					R = COL_GOLD_R; G = COL_GOLD_G; B = COL_GOLD_B;
					lava = 0;
				}
				armed = (sl->fx == gArmedPad);
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
			int meaningful = !lava;
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

		frame[phys * 3 + 0] = g;
		frame[phys * 3 + 1] = r;
		frame[phys * 3 + 2] = b;
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
	/* 2026-11-01 tier-A: bound a hung transfer to 20 ms (units of 10 ms), 1 retry */
	ioctl(gI2cFd, I2C_TIMEOUT, 2);
	ioctl(gI2cFd, I2C_RETRIES, 1);
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
