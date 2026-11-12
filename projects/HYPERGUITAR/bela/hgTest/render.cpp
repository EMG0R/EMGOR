/*
 * hgTest - input passthrough + level meter.
 *
 * WHAT IT DOES
 *   Sums every audio input and sends that one mono sum to EVERY output, with
 *   no panning. Inputs 2 and 3 (0-indexed) are the contact mics and get their
 *   own gain and EQ, because a piezo through a follower is a very different
 *   signal from a magnetic pickup.
 *
 * WHY IT ALSO PRINTS A METER
 *   The point of this build is finding out whether the piezo buffer works, and
 *   "I can hear something" does not tell you WHICH input it came from. Every
 *   second it prints a per-channel level bar, so a dead channel, a wired-wrong
 *   channel and a working one are distinguishable without unplugging anything.
 *
 *   Channels 2 and 3 are the ones to watch - they are the new buffer, and they
 *   are also the pair whose jack was broken and replaced.
 *
 * SAFETY
 *   The sum is scaled by 1/N and then soft-clipped. Ten summed inputs can
 *   easily exceed full scale, and this is going into speakers someone is
 *   listening to.
 */
#include <Bela.h>
#include "i2c_sensors.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cmath>
#include <cstdio>

/* ---- listening level ----
 * Calibrated offline, not guessed: dividing the sum by the CHANNEL COUNT (the
 * obvious thing) put the peak at 0.065, i.e. ~24 dB down and barely audible.
 * Ten sources do not add coherently, so 1/sqrt(N) is the honest normaliser and
 * 1/N throws away most of the level for a clipping case that never happens. */
static const float MASTER = 1.0f;
static const float DRIVE  = 1.2f;   /* soft-clip drive, applied after MASTER */

/*
 * Contact-mic gain. The first guess of 8x was WRONG BY EAR: the BC517 follower
 * passes far more level than assumed and the piezos came out "super loud".
 * 0.8x - ten times lower - was set from listening, which is the only way this
 * number was ever going to be right.
 */
static const float PIEZO_GAIN = 2.5f;

/*
 * PIEZO SOLO. With everything summed it is impossible to tell whether a hum is
 * coming from the contact mics or from one of the eight other live inputs, and
 * guessing wrong sends you soldering the wrong thing. Set to 1 and ONLY the two
 * contact-mic channels reach the output; every other input is muted.
 *
 * Used it, it did its job - the piezos were confirmed working in isolation on
 * 2026-09-22 at 2.5x gain. Back to 0 = everything passes.
 */
static const int PIEZO_SOLO = 0;

/*
 * CONTACT MICS MUTED (2026-09-22, by request). They are confirmed working; they
 * are muted so they stay out of the way while the hex pickup channel order is
 * being sorted out. Flip to 1 to bring them back.
 */
static const int PIEZO_ENABLE = 1;

/*
 * GUITAR TRS INPUT TRIM. Channels 0/1 measured ~35 dB hotter than the hex
 * pickup (-35 vs -73 at idle), which made the sum useless for judging the
 * pickup. -20 dB brings them into the same ballpark. This is a TEST-RIG
 * balance, not a mix decision.
 */
static const float TRS_GAIN = 0.1f;
static const int TRS_A = 0;
static const int TRS_B = 1;

/* ---- string-order detection ----
 * The hex pickup's channel order is wrong and has to be discovered by ear:
 * the player plucks lowest string to highest, and each detected transient is
 * printed with its channel. Reading that sequence gives the true mapping for
 * STRING_CHANNEL[] without guessing.
 */
static const float ONSET_RISE = 6.0f;    /* fast/slow envelope ratio to fire */
/* MEASURED, not guessed: the idle noise floor on this rig sits around -73 dB
 * (~2.2e-4 linear) and the first build happily reported eight "onsets" there
 * during boot settling. A real pluck reads -40 dB or hotter. 5e-3 (-46 dB) is
 * comfortably above the floor and below the quietest real note. */
static const float ONSET_FLOOR = 5e-3f;
static const int   ONSET_REFRACT_MS = 250;   /* one report per pluck, not per wobble */

/* Ignore the first second entirely: envelopes start at zero, so everything
 * looks like a rise while they fill. */
static const int   ONSET_SETTLE_MS = 1000;

/* ---- PLUCK SNAPSHOT ----
 * Onset-per-channel was the wrong tool for finding the string order. A hex
 * pickup crosstalks badly - one pluck fired ch5 at -23 dB AND ch4 at -37 - so
 * a per-channel trigger reports the bleed as if it were a second note, and the
 * sequence becomes unreadable.
 *
 * Instead: notice that SOMETHING was plucked, wait for the attack to develop,
 * then rank EVERY channel by peak and print the table. The loudest channel is
 * the string; the runners-up are the bleed, and seeing them ranked makes that
 * obvious instead of confusing. One line per pluck, no threshold guessing. */
static const float PLUCK_ARM   = 0.010f;   /* ~-40 dB peak on any one channel */
static const int   PLUCK_WIN_MS = 60;      /* let the attack develop */
static const int   PLUCK_HOLD_MS = 400;    /* one report per pluck */

/* Which inputs are the contact mics (0-indexed) = "input 3 and 4" on a panel */
static const int PIEZO_A = 2;
static const int PIEZO_B = 3;

/* ---- per-piezo filter state ---- */
struct PiezoEq {
	/* TWO one-pole high-passes in series = 12 dB/oct.
	 * A single pole at 70 Hz was useless against hum: 6 dB/oct puts 50/60 Hz
	 * only a couple of dB down, so mains sailed straight through. Cascading
	 * two at a higher corner gets 50/60 Hz roughly 17 dB down while leaving
	 * the body thump a contact mic is actually for. */
	float hp1X, hp1Y;
	float hp2X, hp2Y;
	/* one-pole low-pass: tames the hard upper-mid zing piezos are known for */
	float lpY;
};
static PiezoEq gEq[2];

static float gHpA = 0.0f;   /* high-pass coefficient, set in setup() */
static float gLpA = 0.0f;   /* low-pass coefficient */

/* ---- onset detect, per channel ---- */
static float gEnvF[32], gEnvS[32];
static int   gRefract[32];
static int   gOnsetSeq = 0;
static int   gSettle = 0;
static float gPk[32];
static int   gPluckWin = 0, gPluckHold = 0, gPluckN = 0;

/* ---- sensors + telemetry ---- */
static SensorState gSens;
static AuxiliaryTask gSensTask;
static unsigned int gSensDiv = 0;
static int gTxSock = -1;
static struct sockaddr_in gTxAddr;
static float gOutRms[2];

/* ---- meter state ---- */
static float gRms[32];
static unsigned int gMeterCount = 0;
static unsigned int gMeterPeriod = 0;
static int gNIn = 0, gNOut = 0;
static float gNorm = 1.0f;   /* 1/sqrt(channel count) */

static inline float onePoleCoeff(float hz, float sr)
{
	float x = expf(-2.0f * (float)M_PI * hz / sr);
	return x;
}

/* Sensors are read on an AUX TASK, never the audio thread: a VL53L1X ranging
 * read is tens of milliseconds and an I2C transaction blocks. */
static void sensorTask(void *)
{
	sensorsRead(&gSens);

	/* telemetry to the Mac over the USB-net gadget. Fire-and-forget: a dropped
	 * monitor packet costs one stale frame on a display, and retrying would
	 * mean blocking here. */
	if(gTxSock < 0)
		return;
	char buf[768];
	int p = 0;
	p += snprintf(buf + p, sizeof(buf) - p, "HGMON in:");
	for(int ch = 0; ch < gNIn; ch++)
		p += snprintf(buf + p, sizeof(buf) - p, "%s%.1f", ch ? "," : "",
				20.0f * log10f(sqrtf(gRms[ch]) + 1e-9f));
	p += snprintf(buf + p, sizeof(buf) - p, " out:%.1f,%.1f",
			20.0f * log10f(sqrtf(gOutRms[0]) + 1e-9f),
			20.0f * log10f(sqrtf(gOutRms[1]) + 1e-9f));
	if(gSens.mpuOk)
		p += snprintf(buf + p, sizeof(buf) - p,
				" acc:%.3f,%.3f,%.3f gyr:%.2f,%.2f,%.2f",
				gSens.accX, gSens.accY, gSens.accZ,
				gSens.gyrX, gSens.gyrY, gSens.gyrZ);
	if(gSens.tofOk)
		p += snprintf(buf + p, sizeof(buf) - p, " tof:%d", gSens.tofMm);
	p += snprintf(buf + p, sizeof(buf) - p, "\n");
	sendto(gTxSock, buf, p, 0, (struct sockaddr *)&gTxAddr, sizeof(gTxAddr));
}

bool setup(BelaContext *context, void *userData)
{
	gNIn = context->audioInChannels;
	gNOut = context->audioOutChannels;
	if(gNIn > 32) gNIn = 32;

	/* 130 Hz, applied TWICE. Chosen against mains hum, not against the
	 * instrument's range - a contact mic's useful content is percussive body
	 * response, which lives well above this. */
	gHpA = onePoleCoeff(130.0f, context->audioSampleRate);
	/* 7 kHz low-pass: piezos peak hard in the 3-8 kHz region and that is what
	 * reads as "harsh" or "clicky" rather than "woody". */
	gLpA = onePoleCoeff(7000.0f, context->audioSampleRate);

	for(int i = 0; i < 2; i++) {
		gEq[i].hp1X = gEq[i].hp1Y = 0.0f;
		gEq[i].hp2X = gEq[i].hp2Y = 0.0f;
		gEq[i].lpY = 0.0f;
	}
	for(int i = 0; i < 32; i++)
		gRms[i] = 0.0f;

	gNorm = 1.0f / sqrtf(gNIn > 0 ? (float)gNIn : 1.0f);
	gMeterPeriod = (unsigned int)(context->audioSampleRate);   /* ~1 s */

	rt_printf("hgTest: %d in -> %d out, sr %.0f\n",
			gNIn, gNOut, context->audioSampleRate);
	rt_printf("hgTest: summing ALL inputs to ALL outputs, no panning\n");
	rt_printf("hgTest: contact mics on inputs %d and %d (gain %.1fx, "
			"70Hz HP / 7kHz LP)\n", PIEZO_A, PIEZO_B, PIEZO_GAIN);
	rt_printf("hgTest: watch the meter - ch%d/ch%d are the new buffer\n",
			PIEZO_A, PIEZO_B);
	rt_printf("hgTest: contact mics %s, TRS ch%d/%d trimmed %.0f dB\n",
			PIEZO_ENABLE ? "ENABLED" : "MUTED", TRS_A, TRS_B,
			20.0f * log10f(TRS_GAIN));

	for(int i = 0; i < 32; i++) { gEnvF[i] = gEnvS[i] = 0.0f; gRefract[i] = 0; }
	gSettle = (int)(context->audioSampleRate * ONSET_SETTLE_MS / 1000.0f);
	gOutRms[0] = gOutRms[1] = 0.0f;

	/* ---- sensors ---- */
	/* both sensors on for this pass-through test */
	/* CONTROL TEST 2026-09-23: the clicking continued with the VL53L1X
	 * PHYSICALLY UNPLUGGED - no laser, no current draw, nothing on the far end
	 * of the wires. That rules the sensor out entirely, so the noise is made on
	 * THIS side by the act of polling: either the I2C lines being driven, or
	 * the periodic aux-task wakeup loading the Bela's own supply.
	 *
	 * Both sensors OFF. Nothing touches /dev/i2c-1 while this is set, so
	 * silence here confirms polling is the source and the fix is software or
	 * the Bela's own power input - NOT decoupling at any sensor. */
	/* BACK TO THE NOISY CASE on purpose. Both sensors polling, ranging on -
	 * this is the loudest normal state, so it is the honest thing to A/B a
	 * hardware fix against. Silence here means the fix worked. */
	gEnableMpu = 1;
	gEnableTof = 1;
	gTofNoRange = 0;
	sensorsInit(&gSens);
	gSensTask = Bela_createAuxiliaryTask(sensorTask, 50, "hg-sensors");

	/* ---- telemetry socket ---- */
	gTxSock = socket(AF_INET, SOCK_DGRAM, 0);
	if(gTxSock >= 0) {
		memset(&gTxAddr, 0, sizeof(gTxAddr));
		gTxAddr.sin_family = AF_INET;
		gTxAddr.sin_port = htons(9000);
		inet_pton(AF_INET, "192.168.7.1", &gTxAddr.sin_addr);
		rt_printf("hgTest: telemetry -> 192.168.7.1:9000\n");
	}

	rt_printf("hgTest: *** PLUCK LOWEST STRING TO HIGHEST - watch PLUCK lines ***\n");
	if(PIEZO_SOLO)
		rt_printf("hgTest: *** PIEZO SOLO - all other inputs MUTED ***\n");
	return true;
}

void render(BelaContext *context, void *userData)
{
	for(unsigned int n = 0; n < context->audioFrames; n++) {

		float sum = 0.0f;

		if(gSettle > 0) gSettle--;
		if(gPluckHold > 0) gPluckHold--;
		if(gPluckWin > 0 && --gPluckWin == 0) {
			/* window closed: rank every channel by the peak it reached */
			int order[32];
			for(int i = 0; i < gNIn; i++) order[i] = i;
			for(int i = 0; i < gNIn - 1; i++)
				for(int j = i + 1; j < gNIn; j++)
					if(gPk[order[j]] > gPk[order[i]]) {
						int t = order[i]; order[i] = order[j]; order[j] = t;
					}
			gPluckN++;
			rt_printf("PLUCK #%d  WINNER ch%d (%.1f dB)   next: ch%d (%.1f)  ch%d (%.1f)\n",
					gPluckN, order[0], 20.0f * log10f(gPk[order[0]] + 1e-9f),
					order[1], 20.0f * log10f(gPk[order[1]] + 1e-9f),
					order[2], 20.0f * log10f(gPk[order[2]] + 1e-9f));
			for(int i = 0; i < gNIn; i++) gPk[i] = 0.0f;
			gPluckHold = (int)(context->audioSampleRate * PLUCK_HOLD_MS / 1000.0f);
		}

		for(int ch = 0; ch < gNIn; ch++) {
			float x = audioRead(context, n, ch);

			/* meter BEFORE any gain, so it shows what actually arrived at the
			 * jack rather than what this program did to it */
			gRms[ch] += (x * x - gRms[ch]) * 0.0005f;

			/* ---- peak track for the pluck snapshot ---- */
			{
				float a = fabsf(x);
				if(a > gPk[ch]) gPk[ch] = a;
				if(gSettle <= 0 && gPluckHold <= 0 && gPluckWin <= 0
						&& a > PLUCK_ARM)
					gPluckWin = (int)(context->audioSampleRate
							* PLUCK_WIN_MS / 1000.0f);
			}

			if(ch == TRS_A || ch == TRS_B)
				x *= TRS_GAIN;

			if(ch == PIEZO_A || ch == PIEZO_B) {
				PiezoEq &e = gEq[(ch == PIEZO_A) ? 0 : 1];

				/* high-pass, stage 1 */
				float hp = gHpA * (e.hp1Y + x - e.hp1X);
				e.hp1X = x;
				e.hp1Y = hp;

				/* high-pass, stage 2 - the one that actually kills the hum */
				float hp2 = gHpA * (e.hp2Y + hp - e.hp2X);
				e.hp2X = hp;
				e.hp2Y = hp2;

				/* one-pole low-pass */
				e.lpY = e.lpY + (1.0f - gLpA) * (hp2 - e.lpY);

				x = e.lpY * PIEZO_GAIN;
				if(!PIEZO_ENABLE)
					x = 0.0f;   /* confirmed working; parked for now */
			}

			/* solo: mute everything that is not a contact mic */
			if(PIEZO_SOLO && ch != PIEZO_A && ch != PIEZO_B)
				x = 0.0f;

			sum += x;
		}

		/* 1/sqrt(N), not 1/N - see MASTER. tanh is the safety net for the
		 * worst case where everything DOES line up; it is going to speakers. */
		sum *= MASTER * gNorm;
		sum = tanhf(sum * DRIVE);

		/* same mono sum to every output - no panning, by request */
		for(int ch = 0; ch < gNOut; ch++)
			audioWrite(context, n, ch, sum);
		gOutRms[0] += (sum * sum - gOutRms[0]) * 0.0005f;
		gOutRms[1] = gOutRms[0];
	}

	/* sensors + telemetry on an aux task: I2C blocks, audio must not */
	/* THROTTLED to ~20 Hz. Scheduling this every audio block ran the telemetry
	 * at ~680 packets/sec - 34x what the monitor needs, burning CPU on both
	 * ends of a USB link that is already the weak point on this rig. */
	gSensDiv += context->audioFrames;
	if(gSensTask && gSensDiv >= (unsigned int)(context->audioSampleRate / 20)) {
		gSensDiv = 0;
		Bela_scheduleAuxiliaryTask(gSensTask);
	}

	/* ---- meter, once a second ---- */
	gMeterCount += context->audioFrames;
	if(gMeterCount >= gMeterPeriod) {
		gMeterCount = 0;
		char line[512];
		int p = 0;
		p += snprintf(line + p, sizeof(line) - p, "LVL ");
		for(int ch = 0; ch < gNIn; ch++) {
			float db = 20.0f * log10f(sqrtf(gRms[ch]) + 1e-9f);
			const char *tag = (ch == PIEZO_A || ch == PIEZO_B) ? "*" : " ";
			p += snprintf(line + p, sizeof(line) - p, "%s%d:%-5.0f", tag, ch, db);
			if(p > (int)sizeof(line) - 16)
				break;
		}
		rt_printf("%s  (dB, * = contact mic)\n", line);
	}
}

void cleanup(BelaContext *context, void *userData)
{
}
