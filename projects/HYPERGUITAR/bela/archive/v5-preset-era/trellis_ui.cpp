/*
 * trellis_ui.cpp - NeoTrellis 4x4 pad grid over /dev/i2c-1: seesaw
 * register I/O, LED palettes, pad-press dispatch, autosave throttle.
 * Split out of the former monolithic render.cpp. Bodies are
 * byte-for-byte identical to the original.
 */
#include "hyperguitar.h"
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

static void lavaPalette(float t, uint8_t *r, uint8_t *g, uint8_t *b)
{
	if(t < 0.0f) t = 0.0f;
	if(t > 1.0f) t = 1.0f;
	float R, G, B;
	if(t < 0.5f) {
		float u = t / 0.5f;
		R = 0.30f + 0.32f * u;
		G = 0.0f;
		B = 0.55f + 0.35f * u;
	} else {
		float u = (t - 0.5f) / 0.5f;
		R = 0.62f + 0.38f * u;
		G = 0.10f * u * u;
		B = 0.90f + 0.10f * u;
	}
	float bright = 0.40f + 0.60f * t;
	*r = gGammaLUT[(int)(R * bright * 255.0f)];
	*g = gGammaLUT[(int)(G * bright * 255.0f)];
	*b = gGammaLUT[(int)(B * bright * 255.0f)];
}

// PRESET MODE palette: every pad its own shade, but R always well below B
// and G always suppressed so the whole grid reads as one cohesive family
// of purples rather than drifting toward blue/pink/red on any pad.
static void purplePalette(int i, float active, uint8_t *r, uint8_t *g, uint8_t *b)
{
	float t = (float)i / (float)(NPIX - 1);
	float R = 0.35f + 0.30f * sinf(t * 6.2831853f * 1.7f + 0.6f);
	float B = 0.65f + 0.30f * sinf(t * 6.2831853f * 1.3f + 2.1f);
	float G = 0.06f + 0.05f * sinf(t * 6.2831853f * 2.3f + 4.0f);
	if(R < 0.20f) R = 0.20f; if(R > 0.75f) R = 0.75f;
	if(B < 0.55f) B = 0.55f; if(B > 1.00f) B = 1.00f;
	if(G < 0.00f) G = 0.00f; if(G > 0.14f) G = 0.14f;
	float bright = 0.42f + 0.40f * active;  // brighter when this preset is on
	int ri = (int)(R * bright * 255.0f), gi = (int)(G * bright * 255.0f),
	    bi = (int)(B * bright * 255.0f);
	if(ri < 0) ri = 0; if(ri > 255) ri = 255;
	if(gi < 0) gi = 0; if(gi > 255) gi = 255;
	if(bi < 0) bi = 0; if(bi > 255) bi = 255;
	*r = gGammaLUT[ri];
	*g = gGammaLUT[gi];
	*b = gGammaLUT[bi];
}

void trellisTick(void *)
{
	if(!gTrellisOk)
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
				gPadFlash[pad] = 1.0f;
				gStateDirty = 1;
				if(gPresetMode) {
					applyPreset(pad);
					continue;
				}
				if(pad == PAD_PITCH) {
					gArmedPad = pad;
					gPitchActive = !gPitchActive;
					applyPitch();
					rt_printf("pitch %d semi=%.1f cents=%.0f mode=%s wet=%.0f%%\n",
							gPitchActive, gPitchSemitones, gPitchCents,
							gPitchCentsMode ? "cents" : "semi", gPitchWetPct);
				} else if(pad == PAD_FM) {
					gArmedPad = pad;
					gFmActive = !gFmActive;
					rt_printf("fm %d ratio=%.2f idx=%.2f mode=%s\n",
							gFmActive, gFmRatioVal, gFmIndexVal,
							gFmQuantized ? "quant" : "free");
				} else if(pad == PAD_FORMANT) {
					gArmedPad = pad;
					gFormantActive = !gFormantActive;
					applyFormant();
					rt_printf("formant %d val=%.2f glide=%.3f\n",
							gFormantActive, gFormantVal, gFormantGlide);
				} else if(pad == PAD_SINE) {
					gArmedPad = pad;
					gSineOn = !gSineOn;
					rt_printf("synth %d [%s] %s (main=timbre, click=mono/poly, aux=level)\n",
							gSineOn, SYNTH_NAMES[gSynthMode],
							gSynthPoly ? "POLY" : "MONO");
				} else if(pad == PAD_VIB) {
					gArmedPad = pad;
					gVibOn = !gVibOn;
					rt_printf("vibrato %d\n", gVibOn);
				} else if(pad == PAD_CHORUS) {
					gArmedPad = pad;
					gChorusOn = !gChorusOn;
					rt_printf("chorus %d\n", gChorusOn);
				} else if(pad == PAD_ENVF) {
					gArmedPad = pad;
					gEnvFOn = !gEnvFOn;
					rt_printf("autowah %d\n", gEnvFOn);
				} else if(pad == PAD_POG) {
					gArmedPad = pad;
					gPogOn = !gPogOn;
					rt_printf("pog %d\n", gPogOn);
				} else if(pad == PAD_RES) {
					gArmedPad = pad;
					gResOn = !gResOn;
					rt_printf("resonator %d\n", gResOn);
				} else if(pad == PAD_SUS) {
					gArmedPad = pad;
					gSusOn = !gSusOn;
					rt_printf("sustainer %d\n", gSusOn);
				} else if(pad == PAD_FWAH) {
					gArmedPad = pad;
					gFWahOn = !gFWahOn;
					applyMixRouting();
					rt_printf("formantwah %d\n", gFWahOn);
				} else if(pad == PAD_JAWARI) {
					gArmedPad = pad;
					gJawOn = !gJawOn;
					rt_printf("jawari %d\n", gJawOn);

				} else if(pad == PAD_SITAR) {
					gArmedPad = pad;
					gSitarOn = !gSitarOn;
					rt_printf("SITAR TONE %d (buzz=%.2f sus=%.4f)\n",
							gSitarOn, gSitarBuzz, gSitarRing);
				} else if(pad == PAD_PAN) {
					gArmedPad = pad;
					gPanOn = !gPanOn;
					rt_printf("stereo-spread %d (spread=%.2f dyn=%.2f)\n",
							gPanOn, gPanSpread, gPanDyn);
				} else if(pad == PAD_PLUCKSAG) {
					gArmedPad = pad;
					gSagOn = !gSagOn;
					rt_printf("plucksag %d\n", gSagOn);
				} else if(pad == PAD_SYMP) {
					gArmedPad = pad;
					gSympOn = !gSympOn;
					rt_printf("sympathetic %d\n", gSympOn);
				}
			}
		}
	}

	gLavaT += 1.0f / 30.0f;
	float t = gLavaT;
	float field[NPIX] = { 0 };
	static const float bx[3] = { 0.041f, 0.023f, 0.031f };
	static const float by[3] = { 0.027f, 0.047f, 0.017f };
	static const float ph[3] = { 0.0f, 2.1f, 4.4f };
	for(int bi = 0; bi < 3; bi++) {
		float cx = 1.5f + 1.6f * sinf(2.0f * (float)M_PI * bx[bi] * t + ph[bi]);
		float cy = 1.5f + 1.6f * sinf(2.0f * (float)M_PI * by[bi] * t + ph[bi] * 1.7f);
		float rad = 1.25f + 0.35f * sinf(2.0f * (float)M_PI * 0.05f * t + ph[bi]);
		for(int y = 0; y < 4; y++)
			for(int x = 0; x < 4; x++) {
				float dx = x - cx, dy = y - cy;
				field[y * 4 + x] += expf(-(dx * dx + dy * dy) / (rad * rad));
			}
	}

	uint8_t frame[NPIX * 3];
	int flashing = gModeFlash > 0;
	if(flashing) gModeFlash--;
	for(int i = 0; i < NPIX; i++) {
		uint8_t r, g, b;
		int on;
		if(gPresetMode) {
			on = (i <= 3 && gPresetFormantSel == i)
			  || (i >= 4 && i <= 7 && gPresetPitchSel == i)
			  || (i == 8 && gPresetVibChorusSel == 0)
			  || (i == 9 && gPresetVibChorusSel == 1)
			  || (i == 10 && gSusOn)
			  || (i == 11 && gSineOn)
			  || (i == 12 && gSagOn)
			  || (i == 13 && gEnvFOn)
			  || (i == 14 && gFWahOn)
			  || (i == 15 && gSympOn);
			purplePalette(i, on ? 1.0f : 0.0f, &r, &g, &b);
		} else {
			float v = 0.35f + field[i] * 0.45f;
			v = v / (1.0f + v * 0.35f);
			if(v > 0.92f) v = 0.92f;
			lavaPalette(v, &r, &g, &b);
			on = (gPitchActive && i == PAD_PITCH)
			  || (gFmActive && i == PAD_FM)
			  || (gFormantActive && i == PAD_FORMANT)
			  || (gSineOn && i == PAD_SINE)
			  || (gVibOn && i == PAD_VIB)
			  || (gChorusOn && i == PAD_CHORUS)
			  || (gEnvFOn && i == PAD_ENVF)
			  || (gPogOn && i == PAD_POG)
			  || (gResOn && i == PAD_RES)
			  || (gSusOn && i == PAD_SUS)
			  || (gFWahOn && i == PAD_FWAH)
			  || (gJawOn && i == PAD_JAWARI)
			  || (gSitarOn && i == PAD_SITAR)
			  || (gPanOn && i == PAD_PAN)
			  || (gSagOn && i == PAD_PLUCKSAG)
			  || (gSympOn && i == PAD_SYMP);
		}

		float fl = gPadFlash[i];
		if(on)
			fl = 1.0f;
		if(fl > 0.003f) {
			float w = fl * fl;
			r = (uint8_t)(r + (255 - r) * w);
			g = (uint8_t)(g + (255 - g) * w);
			b = (uint8_t)(b + (255 - b) * w);
		}
		if(!on)
			gPadFlash[i] *= 0.86f;

		// MODE-SWITCH FLASH: briefly slam the whole grid to white,
		// overriding everything else, so the mode change is unmissable.
		if(flashing)
			r = g = b = 255;

		// GLOBAL BRIGHTNESS: one knob, applied last, after every other
		// effect (palette, pad flash, mode-flash white) - so "dim the
		// whole grid" always means exactly that, not "dim some things."
		r = (uint8_t)(r * LED_BRIGHTNESS);
		g = (uint8_t)(g * LED_BRIGHTNESS);
		b = (uint8_t)(b * LED_BRIGHTNESS);

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
