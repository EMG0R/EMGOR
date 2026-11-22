/*
 * usbaudio-bridge: standalone, ordinary (non-real-time) process that
 * drains the lock-free ring buffer hyperGuitar6chPan writes into shared
 * memory and feeds it to the UAC2 USB gadget's ALSA playback device
 * (which appears as a capture/input device on the connected host).
 *
 * Deliberately kept OUT of the real-time Xenomai/EVL audio process -
 * ALSA writes here are normal blocking calls, which is fine for this
 * process but would be unacceptable inside the RT audio callback. If
 * this process crashes or the ALSA device misbehaves, the guitar
 * instrument itself is completely unaffected - it only ever READS the
 * shared ring, never signals back to the producer.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <alsa/asoundlib.h>
#include <sys/stat.h>

/* Written by hyperGuitar6chPan (HGSET usb 0 / grid double-tap). While it
 * exists the bridge closes the USB sound output and idles; removed = reopen.
 * Lives in /run so it never survives a reboot. */
#define USB_OFF_FLAG "/run/hyperguitar-usbaudio-off"
static int usbOff(void)
{
	struct stat st;
	return stat(USB_OFF_FLAG, &st) == 0;
}

#define BRIDGE_RING_FRAMES 512    // ~11.6ms ceiling. MUST match render.cpp's BRIDGE_RING_FRAMES -
                                  // shared struct layout, changing one without
                                  // the other corrupts the shared memory.
#define BRIDGE_RING_MASK (BRIDGE_RING_FRAMES - 1)
#define BRIDGE_SHM_NAME "/hyperguitar_usbaudio"
#define TARGET_BACKLOG 64    // ~1.45ms - where we actively steer the backlog to
#define RESYNC_BACKLOG 192   // ~4.35ms - if we ever drift this far behind,
                             // snap forward instead of slowly draining it off

typedef struct {
	volatile uint32_t head;
	volatile uint32_t tail;
	int16_t data[BRIDGE_RING_FRAMES * 2];
} BridgeRing;

static BridgeRing *openRing(void)
{
	for(;;) {
		int fd = shm_open(BRIDGE_SHM_NAME, O_RDWR, 0666);
		if(fd >= 0) {
			void *m = mmap(NULL, sizeof(BridgeRing), PROT_READ | PROT_WRITE,
					MAP_SHARED, fd, 0);
			close(fd);
			if(m != MAP_FAILED)
				return (BridgeRing *)m;
		}
		fprintf(stderr, "usbaudio-bridge: waiting for hyperGuitar6chPan "
				"to create the shared ring...\n");
		sleep(2);
	}
}

static snd_pcm_t *openPcm(void)
{
	snd_pcm_t *pcm;
	int err;
	for(;;) {
		err = snd_pcm_open(&pcm, "plughw:UAC2Gadget,0",
				SND_PCM_STREAM_PLAYBACK, 0);
		if(err == 0)
			break;
		fprintf(stderr, "usbaudio-bridge: snd_pcm_open failed (%s), "
				"retrying...\n", snd_strerror(err));
		sleep(2);
	}

	snd_pcm_hw_params_t *hw;
	snd_pcm_hw_params_alloca(&hw);
	snd_pcm_hw_params_any(pcm, hw);
	snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S16_LE);
	snd_pcm_hw_params_set_channels(pcm, hw, 2);
	unsigned int rate = 44100;
	snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, 0);
	// was period=512/periods=4 (~46ms fixed buffering) - that alone was
	// a meaningful chunk of the reported latency. Tighter, still stable.
	snd_pcm_uframes_t period = 32;
	snd_pcm_hw_params_set_period_size_near(pcm, hw, &period, 0);
	unsigned int periods = 3;
	snd_pcm_hw_params_set_periods_near(pcm, hw, &periods, 0);
	snd_pcm_hw_params(pcm, hw);
	snd_pcm_prepare(pcm);
	fprintf(stderr, "usbaudio-bridge: PCM open, rate=%u period=%lu periods=%u\n",
			rate, (unsigned long)period, periods);
	return pcm;
}

int main(void)
{
	fprintf(stderr, "usbaudio-bridge: starting\n");
	BridgeRing *ring = openRing();
	snd_pcm_t *pcm = usbOff() ? NULL : openPcm();

	int16_t buf[64];   // 32 stereo frames per write, matches period
	unsigned int flagCheck = 0;
	for(;;) {
		/* USB audio switched off: close the output, drain the ring, idle.
		 * Checked every ~256 loop passes (a stat() is cheap, but not free
		 * at this loop rate), and every pass while already off. */
		if(!pcm || (++flagCheck & 255) == 0) {
			if(usbOff()) {
				if(pcm) {
					snd_pcm_drop(pcm);
					snd_pcm_close(pcm);
					pcm = NULL;
					fprintf(stderr, "usbaudio-bridge: USB audio OFF, output closed\n");
				}
				ring->tail = ring->head;
				usleep(200000);
				continue;
			}
			if(!pcm) {
				fprintf(stderr, "usbaudio-bridge: USB audio ON, reopening\n");
				pcm = openPcm();
				ring->tail = ring->head;   /* start fresh, no stale backlog */
			}
		}
		uint32_t head = ring->head, tail = ring->tail;
		uint32_t avail = head - tail;

		if(avail == 0) {
			usleep(300);
			continue;
		}

		/*
		 * ACTIVE RESYNC: the ring is drop-when-full on the producer side,
		 * which passively lets the backlog creep toward that ceiling and
		 * PIN there under any tiny persistent rate mismatch (this was the
		 * real source of the reported latency, not just the ALSA buffer
		 * size). Instead of ever slowly draining a large backlog off,
		 * snap straight back to TARGET_BACKLOG the moment we're
		 * meaningfully behind it - a few dropped/skipped frames here are
		 * inaudible, a growing latency creep is not.
		 */
		if(avail > RESYNC_BACKLOG) {
			tail = head - TARGET_BACKLOG;
			ring->tail = tail;
			avail = TARGET_BACKLOG;
		}

		uint32_t chunk = avail;
		if(chunk > 32) chunk = 32;

		for(uint32_t i = 0; i < chunk; i++) {
			uint32_t idx = ((tail + i) & BRIDGE_RING_MASK) * 2;
			buf[i * 2]     = ring->data[idx];
			buf[i * 2 + 1] = ring->data[idx + 1];
		}
		ring->tail = tail + chunk;

		snd_pcm_sframes_t written = snd_pcm_writei(pcm, buf, chunk);
		if(written < 0) {
			fprintf(stderr, "usbaudio-bridge: xrun/error (%s), recovering\n",
					snd_strerror((int)written));
			int rc = snd_pcm_recover(pcm, (int)written, 1);
			if(rc < 0) {
				/* Recovery itself failed - device likely unplugged, or
				 * (the common case) no host app has this device open
				 * yet so the isochronous stream isn't flowing at all.
				 * Close and reopen from scratch rather than spinning on
				 * a dead handle, with a short backoff so an idle/
				 * unopened device doesn't get hammered with reopen
				 * attempts many times a second. */
				snd_pcm_close(pcm);
				usleep(300000);
				pcm = openPcm();
			}
		}
	}
	return 0;
}
