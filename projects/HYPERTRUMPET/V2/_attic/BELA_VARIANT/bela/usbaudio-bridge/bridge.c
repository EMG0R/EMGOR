/*
 * usbaudio-bridge (hyperTrumpet): standalone, ordinary (non-real-time)
 * process that drains the lock-free ring the hyperTrumpet Bela project
 * writes into shared memory and feeds the UAC2 USB gadget's ALSA playback
 * device (which appears as a capture/input device on the connected host).
 *
 * Ported from hyperGuitar's bridge.c. Changes:
 *  - ring layout lives in hypertrumpet_usbring.h (shared with render.cpp)
 *  - 4 lanes in the ring (FX L/R + dry mic 1/2); the number of channels sent
 *    to USB follows the gadget's p_chmask, read from configfs at open time
 *    (2 = FX only, 4 = FX + dry). Env HT_USB_CH overrides; fallback 2.
 *  - sample rate follows the gadget's p_srate (env HT_USB_RATE overrides)
 *  - hw_params failures are checked and retried (hyperGuitar ignored them)
 *  - acquire/release ordering on the ring indices
 *
 * Kept OUT of the real-time audio process on purpose: ALSA writes here are
 * normal blocking calls. If this process dies or ALSA misbehaves the
 * instrument is unaffected - it only ever pushes to the ring.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <alsa/asoundlib.h>
#include "hypertrumpet_usbring.h"

#define UAC2_CFG "/sys/kernel/config/usb_gadget/g_multi/functions/uac2.usb0/"
#define TARGET_BACKLOG 64    /* ~1.45 ms @44.1k - where we steer the backlog */
#define RESYNC_BACKLOG 192   /* ~4.35 ms - if this far behind, snap forward */
#define CHUNK 32             /* frames per write == ALSA period */
#define MAX_PCM_CH 8

static unsigned g_ch = 2;
static unsigned g_rate = 44100;

static int usbOff(void)
{
	struct stat st;
	return stat(HT_USB_OFF_FLAG, &st) == 0;
}

static long readCfgNum(const char *file)
{
	char path[256], buf[64];
	snprintf(path, sizeof path, UAC2_CFG "%s", file);
	FILE *f = fopen(path, "r");
	if(!f) return -1;
	long v = -1;
	if(fgets(buf, sizeof buf, f))
		v = strtol(buf, NULL, 0);   /* srate may be a comma list: first value */
	fclose(f);
	return v;
}

/* Decide channel count + rate from the gadget config (or env). */
static void configure(void)
{
	const char *e;
	long m = readCfgNum("p_chmask");
	g_ch = (m > 0) ? (unsigned)__builtin_popcountl(m) : 2;
	if((e = getenv("HT_USB_CH")) && atoi(e) > 0) g_ch = (unsigned)atoi(e);
	if(g_ch > MAX_PCM_CH) g_ch = MAX_PCM_CH;
	long r = readCfgNum("p_srate");
	g_rate = (r > 0) ? (unsigned)r : 44100;
	if((e = getenv("HT_USB_RATE")) && atoi(e) > 0) g_rate = (unsigned)atoi(e);
	fprintf(stderr, "usbaudio-bridge: p_chmask=%ld -> %u ch, rate %u\n", m, g_ch, g_rate);
}

static HtRing *openRing(void)
{
	for(;;) {
		int fd = shm_open(HT_SHM_NAME, O_RDWR, 0666);
		if(fd >= 0) {
			void *m = mmap(NULL, sizeof(HtRing), PROT_READ | PROT_WRITE,
					MAP_SHARED, fd, 0);
			close(fd);
			if(m != MAP_FAILED)
				return (HtRing *)m;
		}
		fprintf(stderr, "usbaudio-bridge: waiting for hyperTrumpet to create the shared ring...\n");
		sleep(2);
	}
}

static snd_pcm_t *openPcm(void)
{
	for(;;) {
		snd_pcm_t *pcm;
		int err = snd_pcm_open(&pcm, "plughw:UAC2Gadget,0", SND_PCM_STREAM_PLAYBACK, 0);
		if(err < 0) {
			fprintf(stderr, "usbaudio-bridge: snd_pcm_open failed (%s), retrying...\n", snd_strerror(err));
			sleep(2);
			continue;
		}
		configure();   /* re-read each (re)open: gadget may have changed */
		snd_pcm_hw_params_t *hw;
		snd_pcm_hw_params_alloca(&hw);
		snd_pcm_hw_params_any(pcm, hw);
		snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
		snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S16_LE);
		unsigned int rate = g_rate;
		snd_pcm_uframes_t period = CHUNK;
		unsigned int periods = 3;   /* 96 frames ~2.2 ms ALSA buffer @44.1k */
		if((err = snd_pcm_hw_params_set_channels(pcm, hw, g_ch)) < 0 ||
		   (err = snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, 0)) < 0 ||
		   (err = snd_pcm_hw_params_set_period_size_near(pcm, hw, &period, 0)) < 0 ||
		   (err = snd_pcm_hw_params_set_periods_near(pcm, hw, &periods, 0)) < 0 ||
		   (err = snd_pcm_hw_params(pcm, hw)) < 0) {
			fprintf(stderr, "usbaudio-bridge: hw_params failed (%s), retrying...\n", snd_strerror(err));
			snd_pcm_close(pcm);
			sleep(2);
			continue;
		}
		snd_pcm_prepare(pcm);
		fprintf(stderr, "usbaudio-bridge: PCM open, ch=%u rate=%u(req %u) period=%lu periods=%u\n",
				g_ch, rate, g_rate, (unsigned long)period, periods);
		if(rate != g_rate)
			fprintf(stderr, "usbaudio-bridge: WARNING rate mismatch - ring is at the Bela project rate; "
					"they must equal HT_USB_RATE or latency drifts/resyncs\n");
		return pcm;
	}
}

int main(void)
{
	fprintf(stderr, "usbaudio-bridge: starting\n");
	HtRing *ring = openRing();
	snd_pcm_t *pcm = usbOff() ? NULL : openPcm();
	if(!pcm) configure();

	int16_t buf[CHUNK * MAX_PCM_CH];
	unsigned int flagCheck = 0;
	for(;;) {
		/* USB audio switched off: close the output, drain the ring, idle. */
		if(!pcm || (++flagCheck & 255) == 0) {
			if(usbOff()) {
				if(pcm) {
					snd_pcm_drop(pcm);
					snd_pcm_close(pcm);
					pcm = NULL;
					fprintf(stderr, "usbaudio-bridge: USB audio OFF, output closed\n");
				}
				__atomic_store_n(&ring->tail, __atomic_load_n(&ring->head, __ATOMIC_ACQUIRE), __ATOMIC_RELEASE);
				usleep(200000);
				continue;
			}
			if(!pcm) {
				fprintf(stderr, "usbaudio-bridge: USB audio ON, reopening\n");
				pcm = openPcm();
				__atomic_store_n(&ring->tail, __atomic_load_n(&ring->head, __ATOMIC_ACQUIRE), __ATOMIC_RELEASE);
			}
		}
		uint32_t head = __atomic_load_n(&ring->head, __ATOMIC_ACQUIRE);
		uint32_t tail = ring->tail;
		uint32_t avail = head - tail;
		if(avail == 0) { usleep(300); continue; }

		/* ACTIVE RESYNC (from hyperGuitar): the producer drops when full, so the
		 * backlog creeps to the ceiling under any tiny rate mismatch. Snap back
		 * to TARGET_BACKLOG instead of slowly draining. */
		if(avail > RESYNC_BACKLOG) {
			tail = head - TARGET_BACKLOG;
			avail = TARGET_BACKLOG;
		}
		uint32_t chunk = avail > CHUNK ? CHUNK : avail;

		for(uint32_t i = 0; i < chunk; i++) {
			const int16_t *src = &ring->data[((tail + i) & HT_RING_MASK) * HT_RING_LANES];
			int16_t *dst = &buf[i * g_ch];
			for(unsigned c = 0; c < g_ch; c++)
				dst[c] = (c < HT_RING_LANES) ? src[c] : 0;
		}
		__atomic_store_n(&ring->tail, tail + chunk, __ATOMIC_RELEASE);

		snd_pcm_sframes_t written = snd_pcm_writei(pcm, buf, chunk);
		if(written < 0) {
			fprintf(stderr, "usbaudio-bridge: xrun/error (%s), recovering\n", snd_strerror((int)written));
			if(snd_pcm_recover(pcm, (int)written, 1) < 0) {
				/* Dead handle, or (common) no host app has the device open so the
				 * isochronous stream is not flowing. Reopen with a backoff. */
				snd_pcm_close(pcm);
				usleep(300000);
				pcm = openPcm();
			}
		}
	}
	return 0;
}
