/*
 * hg_mon.cpp - telemetry out + control in, for the web UI.
 *
 * Plain-text UDP, newline framed. Deliberately not OSC: the NEPTR link already
 * owns OSC on this box and a second, different OSC consumer would be one more
 * thing to keep in step. A monitor stream that a human can read with netcat is
 * worth more here than a tidy binary format.
 *
 * OUT  -> 192.168.7.1:9000, ~20 Hz
 *   HGMON in:<10 dB> out:<2 dB> acc:x,y,z gyr:x,y,z tof:mm
 *         menu:<0-3> armed:<fx id or -1> slots:<12 of 0-3, bit0 lit bit1 armed>
 *         fx:<48 of 0-3, all four pages, category-major>
 *         vol:<8 gains>
 *
 *   `slots` is the on/off state of the TWELVE SLOTS OF THE CURRENT MENU, in
 *   SLOT order (0..11), not pad order. The UI does the pad mapping, because
 *   the physical rotation is a display concern and has already been changed
 *   three times - keeping it out of the wire format means the next rotation is
 *   a UI edit, not a firmware flash.
 *
 * IN   <- port 9001
 *   HGSET <name> <float>
 *   names: sub0..sub5, piezo, trs, outA, outB
 *
 * Every gain is applied where that source enters the mix, so a change is
 * audible immediately and survives nothing - these are live controls, not
 * persisted settings.
 */
#include "hyperguitar.h"
#include "hg_mon.h"
#include "i2c_sensors.h"
#include "hg_ctrl.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>
#include <cmath>

#define MON_TX_HOST "192.168.7.1"
#define MON_TX_PORT 9000
#define MON_RX_PORT 9001

/* live gains, all default unity except where the rig needed a trim */
float gVolSub[NUM_STRINGS] = { 1, 1, 1, 1, 1, 1 };
/* Contact mics, back in (2026-09-23). The noise that forced this to 0 was the
 * NeoTrellis sharing a ground pin with the audio side - moving the Trellis to
 * a different ground pin on the board fixed it at the source. The buffer was
 * never the fault; it was faithfully reproducing a ground it shared. */
float gVolPiezo = 2.50f;   /* contact mics: the quiet body layer (restored 2026-09-30) */
float gVolTrs   = 1.0f;    /* guitar TRS at unity - raw converter level, same as the hex strings (INPUT_GAIN 1.0). Was 0.10: -20 dB, the "hella quiet" guitar */
float gVolOutA  = 1.0f;   /* outputs 1+2: unity; the limiter (LIM_CEIL 0.90) already sits at the converter ceiling */
float gVolOutB  = 1.0f;   /* outputs 3+ : unity, same staging as the pair */

float gMonInPk[10], gMonPrePk[2], gMonOutPk[2];
unsigned int gMonInClip[10], gMonPreClip;

/*
 * NEOPIXEL BRIGHTNESS - a live control, because it is a DIAGNOSTIC as much as
 * a preference. The hum's pitch and character track the LED colour and the
 * number of lit pads, and two lit pads beat against each other like two
 * detuned oscillators. That is the signature of the WS2812s' own per-LED PWM
 * current, not of I2C: the bus runs one clock at one rate and sends the same
 * bytes whatever colour they encode.
 *
 * Scaling every pad here changes the LED CURRENT while leaving the I2C traffic
 * byte-for-byte identical - same registers, same length, same rate. So:
 *   noise scales with this  -> it is LED current. Fix is power/ground wiring.
 *   noise ignores this      -> it is the bus after all.
 * And if a low setting is quiet AND readable, that is a fix with no soldering.
 */
/* DEFAULT 0.25, not 1.0, and deliberately so. LED current IS the noise -
 * proven twice over: hot-unplugging the board silenced it while the I2C
 * writes carried on, and moving it from 3.3 V to 5 V (which RAISES WS2812
 * current, ~60 mA/LED at full white) made it dramatically worse. Booting at
 * full brightness is therefore booting into the loudest possible state, which
 * is what the "evil sounds while booting" was. Start quiet, raise by ear. */
/* LEDs DARK at boot. LED current is the entire Trellis hum - at bright 0 the
 * grid is silent, and the Bela cannot source what 16 WS2812s want anyway
 * (touching the board to 3.3 V browns it out). Pads still work; raise with
 * HGSET bright 0..1 once the Trellis has a supply of its own. */
float gTrellisBright = 1.0f;   /* NORMAL - grid looks exactly as it always did */

/* OFF by default. Blacking out unlit pads does cut LED current, but it hides
 * populated-but-off slots - and "what lives here, and what is on" is the whole
 * job of the grid. A current saving that costs you that is not a saving.
 * Reachable via HGSET dark 1 for a measurement only, never as a default. */
int gTrellisDark = 0;

/* how much of normal brightness an unlit pad keeps. Low enough to save most of
 * the current, high enough to still read as "populated but off" in a dark room.
 * HGSET offdim <0..1>. */
float gTrellisOffDim = 1.0f;   /* 1.0 = unlit pads untouched, stock look */

static void mixLoad(void);   /* below, next to applySet() */

/* USB audio: 1 = on. The bridge process watches USB_OFF_FLAG (a file in
 * /run, so a reboot always starts from what the mix file says, not from a
 * stale flag) and closes its USB sound output while it exists. */
volatile int gUsbAudio = 1;
#define USB_OFF_FLAG "/run/hyperguitar-usbaudio-off"
static int gTx = -1, gRx = -1;
static struct sockaddr_in gTxAddr;

void monSetup(void)
{
	gTx = socket(AF_INET, SOCK_DGRAM, 0);
	if(gTx >= 0) {
		memset(&gTxAddr, 0, sizeof(gTxAddr));
		gTxAddr.sin_family = AF_INET;
		gTxAddr.sin_port = htons(MON_TX_PORT);
		inet_pton(AF_INET, MON_TX_HOST, &gTxAddr.sin_addr);
	}

	gRx = socket(AF_INET, SOCK_DGRAM, 0);
	if(gRx >= 0) {
		/* NON-BLOCKING: this is drained from an aux task that must never park
		 * on an empty socket - the tasks behind it are real-time scheduled. */
		int fl = fcntl(gRx, F_GETFL, 0);
		fcntl(gRx, F_SETFL, fl | O_NONBLOCK);
		struct sockaddr_in me;
		memset(&me, 0, sizeof(me));
		me.sin_family = AF_INET;
		me.sin_addr.s_addr = htonl(INADDR_ANY);
		me.sin_port = htons(MON_RX_PORT);
		if(bind(gRx, (struct sockaddr *)&me, sizeof(me)) < 0) {
			close(gRx);
			gRx = -1;
		}
	}
	rt_printf("hg-mon: tx %s:%d  rx :%d\n", MON_TX_HOST, MON_TX_PORT, MON_RX_PORT);
	mixLoad();
}

/*
 * MIX + GRID SETTINGS PERSIST (2026-09-24). Gains and the NeoTrellis look
 * survive a reboot. Kept in their OWN text file, not in PersistedState:
 * that record is a fixed binary layout guarded by STATE_MAGIC, and growing it
 * would silently discard every saved effect. A text file of "name value"
 * lines is also readable and hand-editable over ssh.
 *
 * Saved from the aux task (never the audio thread) whenever an HGSET changes
 * one of these names; loaded once at boot through applySet() itself, so the
 * same clamping applies to a hand-edited file as to a live command.
 */
#define MIX_FILE "/root/Bela/projects/hyperGuitar6chPan/hyperguitar-mix.conf"
static int gMixDirty = 0;
static int gMixLoading = 0;

static int isMixName(const char *n)
{
	if(!strncmp(n, "sub", 3) && n[3] >= '0' && n[3] <= '5' && !n[4]) return 1;
	return !strcmp(n, "piezo") || !strcmp(n, "trs") || !strcmp(n, "outA") || !strcmp(n, "usb")
		|| !strcmp(n, "outB") || !strcmp(n, "bright") || !strcmp(n, "offdim")
		|| !strcmp(n, "dark");
}

static void mixSave(void)
{
	FILE *f = fopen(MIX_FILE ".tmp", "w");
	if(!f) return;
	for(int i = 0; i < NUM_STRINGS; i++)
		fprintf(f, "sub%d %.4f\n", i, gVolSub[i]);
	fprintf(f, "piezo %.4f\ntrs %.4f\noutA %.4f\noutB %.4f\n",
			gVolPiezo, gVolTrs, gVolOutA, gVolOutB);
	fprintf(f, "bright %.4f\noffdim %.4f\ndark %d\nusb %d\n",
			gTrellisBright, gTrellisOffDim, gTrellisDark, gUsbAudio);
	fflush(f);
	fsync(fileno(f));
	fclose(f);
	rename(MIX_FILE ".tmp", MIX_FILE);
}

static void applySet(const char *name, float v);

static void mixLoad(void)
{
	FILE *f = fopen(MIX_FILE, "r");
	if(!f) {
		rt_printf("hg-mon: no %s yet - using compiled-in gains\n", MIX_FILE);
		return;
	}
	char line[96], name[32];
	float v;
	int n = 0;
	gMixLoading = 1;
	while(fgets(line, sizeof(line), f)) {
		if(sscanf(line, "%31s %f", name, &v) == 2 && isMixName(name)) {
			applySet(name, v);
			n++;
		}
	}
	gMixLoading = 0;
	fclose(f);
	rt_printf("hg-mon: restored %d mix/grid settings from %s\n", n, MIX_FILE);
}

static void applySet(const char *name, float v)
{
	if(v < 0.0f) v = 0.0f;
	if(v > 4.0f) v = 4.0f;       /* generous ceiling, but not unbounded */

	if(!strncmp(name, "sub", 3) && name[3] >= '0' && name[3] <= '5')
		gVolSub[name[3] - '0'] = v;
	else if(!strcmp(name, "piezo")) gVolPiezo = v;
	else if(!strcmp(name, "trs"))   gVolTrs = v;
	else if(!strcmp(name, "outA"))  gVolOutA = v;
	else if(!strcmp(name, "outB"))  gVolOutB = v;
	/*
	 * LIVE NOISE BISECT (2026-09-23). Each sensor's polling, and the poll rate
	 * itself, switchable from the monitor channel - so the A/B is a few
	 * seconds by ear instead of a rebuild-and-reboot per data point. The old
	 * cycle is why this hunt produced so many invalid tests: state changed
	 * between listening and measuring.
	 *   HGSET mpu 0|1     stop/start MPU6050 polling
	 *   HGSET tof 0|1     stop/start VL53L1X polling
	 *   HGSET polldiv N   1 = 30 Hz, 2 = 15 Hz, 30 = 1 Hz
	 * If the click's RATE follows polldiv, it is caused by the poll. If it is
	 * unaffected, the poll is innocent and something else in the box ticks.
	 */
	else if(!strcmp(name, "mpu"))   gEnableMpu = v > 0.5f;
	else if(!strcmp(name, "tof"))   gEnableTof = v > 0.5f;
	/*
	 * HGSET tofnorange 1 - THE test that splits the lidar's two suspects.
	 * Ranging is STOPPED (laser dark) but the per-tick poll carries on
	 * unchanged: same register, same rate, same bus traffic.
	 *   silent      -> it is the LASER's current. Fix is power/decoupling.
	 *   still noisy -> it is the TRANSACTION. Fix is software.
	 * Nothing else we have run separates these two, because every previous
	 * test changed both at once.
	 */
	else if(!strcmp(name, "tofnorange")) tofSetRanging(!(v > 0.5f));
	/* HGSET trellis 0|1 - stops/starts the LED+pad half only. Pair it with a
	 * HOT UNPLUG of the board to split power from bus: with the software
	 * still writing, unplugging kills the LED current but keeps the I2C
	 * transactions. Silent on unplug = LED current. Still noisy = the bus. */
	else if(!strcmp(name, "trellis")) gTrellisDisabled = !(v > 0.5f);
	else if(!strcmp(name, "polldiv")) {
		int d = (int)(v + 0.5f);
		gHgSensorDiv = d < 1 ? 1 : d;
	}
	else if(!strcmp(name, "bright")) gTrellisBright = v > 1.0f ? 1.0f : v;
	/*
	 * HGSET dark 1 - the BIGGER current lever of the two. Every pad currently
	 * sits on background purple, so all 16 LEDs draw current all the time even
	 * when nothing is on. Rendering unlit pads fully black leaves only the
	 * menu pad and any live slots drawing at all: typically 1-3 LEDs instead
	 * of 16. Brightness scales what is lit; this changes HOW MANY are lit, and
	 * the two multiply.
	 */
	else if(!strcmp(name, "dark"))  gTrellisDark = v > 0.5f;
	else if(!strcmp(name, "offdim")) gTrellisOffDim = v > 1.0f ? 1.0f : v;
	else if(!strcmp(name, "usb"))   gUsbAudio = v > 0.5f;
	else return;
	if(!gMixLoading && isMixName(name))
		gMixDirty = 1;
	rt_printf("hg-mon: %s = %.3f\n", name, v);
}

int monUsbToggle(void)
{
	gUsbAudio = !gUsbAudio;
	gMixDirty = 1;
	rt_printf("hg-mon: usb audio %s (grid double-tap)\n", gUsbAudio ? "ON" : "OFF");
	return gUsbAudio;
}

void monTick(const float *inDb, const float *outDb)
{
	/* ---- control in ---- */
	if(gRx >= 0) {
		char rb[256];
		ssize_t n;
		while((n = recv(gRx, rb, sizeof(rb) - 1, 0)) > 0) {
			rb[n] = 0;
			char name[32];
			float v;
			if(sscanf(rb, "HGSET %31s %f", name, &v) == 2)
				applySet(name, v);
		}
	}
	/* one write per burst of changes, not one per slider step */
	if(gMixDirty) {
		gMixDirty = 0;
		mixSave();
	}
	/* tell the bridge process: flag file present = USB audio off */
	{
		static int sUsbApplied = -1;
		if(sUsbApplied != gUsbAudio) {
			sUsbApplied = gUsbAudio;
			if(gUsbAudio) {
				unlink(USB_OFF_FLAG);
			} else {
				FILE *uf = fopen(USB_OFF_FLAG, "w");
				if(uf) fclose(uf);
			}
		}
	}

	/* ---- telemetry out ---- */
	if(gTx < 0)
		return;
	char b[1024];
	int p = 0;
	p += snprintf(b + p, sizeof(b) - p, "HGMON in:");
	for(int i = 0; i < 10; i++)
		p += snprintf(b + p, sizeof(b) - p, "%s%.1f", i ? "," : "", inDb[i]);
	p += snprintf(b + p, sizeof(b) - p, " out:%.1f,%.1f", outDb[0], outDb[1]);

	if(gHgSens.mpuOk)
		p += snprintf(b + p, sizeof(b) - p,
				" acc:%.3f,%.3f,%.3f gyr:%.2f,%.2f,%.2f",
				gHgSens.accX, gHgSens.accY, gHgSens.accZ,
				gHgSens.gyrX, gHgSens.gyrY, gHgSens.gyrZ);
	if(gHgSens.tofOk)
		p += snprintf(b + p, sizeof(b) - p, " tof:%d", gHgSens.tofMm);

	/* CONTROLS. Everything on the panel, so the web UI can show the physical
	 * state of the instrument and not just its audio. expr carries its armed
	 * flag because an unarmed pedal reads mid-scale and looks live otherwise -
	 * that distinction is the whole point of the arm-at-zero behaviour. */
	p += snprintf(b + p, sizeof(b) - p,
			" expr:%.3f exprarm:%d exprraw:%.3f joy:%.3f,%.3f joysw:%d"
			" pot:%.3f enc:%d,%d encbtn:%d,%d",
			gHgExpr, gHgExprArmed, gHgExprRaw, gHgJoyXv, gHgJoyYv, gHgJoyHeld,
			gHgPot, gEnc1LastPos, gEnc2LastPos,
			gEnc1BtnStable, gEnc2BtnStable);

	p += snprintf(b + p, sizeof(b) - p, " menu:%d armed:%d slots:",
			gCurCategory, gArmedPad);
	for(int sl = 0; sl < SLOTS_PER_CAT; sl++) {
		/* per-SLOT, exactly what that pad's LED shows: 1 = lit, 2 = armed */
		p += snprintf(b + p, sizeof(b) - p, "%s%d", sl ? "," : "",
				fxSlotLed(gCurCategory, sl));
	}
	/* EVERY page, not just the live one: 4 x 12 slot LED states in
	 * category-major order (cat 0 slot 0..11, cat 1 ...), same 0-3 bits as
	 * slots:. Lets the dashboard show the whole effect map accurately
	 * without the player having to page through the grid. */
	p += snprintf(b + p, sizeof(b) - p, " fx:");
	for(int c = 0; c < NUM_CATEGORIES; c++)
		for(int sl = 0; sl < SLOTS_PER_CAT; sl++)
			p += snprintf(b + p, sizeof(b) - p, "%s%d",
					(c || sl) ? "," : "", fxSlotLed(c, sl));

	/* PER-STRING TRACKER, string order (0 = lowest). hz = what the tracker
	 * currently believes (0 when not confidently tonal), cf = its
	 * confidence. Lets the dashboard show expected vs actual per string, so
	 * a wrong channel map or a bad range is visible while playing. */
	p += snprintf(b + p, sizeof(b) - p, " hz:");
	for(int s2 = 0; s2 < NUM_STRINGS; s2++) {
		const Voice *v = &gV[s2];
		float hz = (v->tonalHops > 0 && v->period > 1.0f) ? v->srate / v->period : 0.0f;
		p += snprintf(b + p, sizeof(b) - p, "%s%.1f", s2 ? "," : "", hz);
	}
	p += snprintf(b + p, sizeof(b) - p, " cf:");
	for(int s2 = 0; s2 < NUM_STRINGS; s2++)
		p += snprintf(b + p, sizeof(b) - p, "%s%.2f", s2 ? "," : "", gV[s2].pitchConf);
	p += snprintf(b + p, sizeof(b) - p, " syn:%d,%d", (int)gSynthMode, (int)gSynthPoly);
	/* CLIP AUDIT: pk = raw input peak per channel (0..1 of converter full
	 * scale), ic = input samples at the ceiling since boot, op = output peak
	 * pre-soft-clip A,B then post A,B, oc = pre-clip overs since boot. */
	p += snprintf(b + p, sizeof(b) - p, " pk:");
	for(int c = 0; c < 10; c++) {
		p += snprintf(b + p, sizeof(b) - p, "%s%.3f", c ? "," : "", gMonInPk[c]);
		gMonInPk[c] = 0.0f;
	}
	p += snprintf(b + p, sizeof(b) - p, " ic:");
	for(int c = 0; c < 10; c++)
		p += snprintf(b + p, sizeof(b) - p, "%s%u", c ? "," : "", gMonInClip[c]);
	p += snprintf(b + p, sizeof(b) - p, " op:%.3f,%.3f,%.3f,%.3f oc:%u",
			gMonPrePk[0], gMonPrePk[1], gMonOutPk[0], gMonOutPk[1], gMonPreClip);
	gMonPrePk[0] = gMonPrePk[1] = gMonOutPk[0] = gMonOutPk[1] = 0.0f;
	p += snprintf(b + p, sizeof(b) - p,
			" usb:%d vol:%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n", (int)gUsbAudio,
			gVolSub[0], gVolSub[1], gVolSub[2], gVolSub[3], gVolSub[4],
			gVolSub[5], gVolPiezo, gVolTrs, gVolOutA, gVolOutB);

	sendto(gTx, b, p, 0, (struct sockaddr *)&gTxAddr, sizeof(gTxAddr));
}
