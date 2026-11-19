/*
 * hypertrumpet_usbring.h - the ONE definition of the shared-memory ring
 * between the Bela project (producer, real-time thread) and the
 * usbaudio-bridge process (consumer, ordinary thread).
 *
 * hyperGuitar kept two copies of this struct (render.cpp + bridge.c) with a
 * "MUST match" comment. Here both sides #include this header instead.
 * Copy this file next to render.cpp in the Bela project AND leave the
 * original beside bridge.c; keep them byte-identical (install.sh checks
 * by md5 when the project is synced).
 *
 * Layout: fixed 4 lanes of int16, interleaved, per frame:
 *   lane 0 = FX out L     lane 1 = FX out R
 *   lane 2 = DRY in0 (bell mic)   lane 3 = DRY in1 (contact mic 1)
 * The producer may leave lanes 2/3 at zero. The bridge sends only as many
 * lanes as the UAC2 gadget advertises (p_chmask), zero-filling the rest.
 */
#ifndef HYPERTRUMPET_USBRING_H
#define HYPERTRUMPET_USBRING_H
#include <stdint.h>

#define HT_RING_FRAMES 512                 /* power of 2; ~11.6 ms @44.1k */
#define HT_RING_MASK   (HT_RING_FRAMES - 1)
#define HT_RING_LANES  4
#define HT_SHM_NAME    "/hypertrumpet_usbaudio"
/* file present = USB audio OFF (bridge closes the PCM and idles). /run is
 * tmpfs so it never survives a reboot. */
#define HT_USB_OFF_FLAG "/run/hypertrumpet-usbaudio-off"

typedef struct {
	volatile uint32_t head;                /* written by producer only */
	volatile uint32_t tail;                /* written by consumer only */
	int16_t data[HT_RING_FRAMES * HT_RING_LANES];
} HtRing;

static inline int16_t ht_f2s(float v)
{
	if(v > 1.0f) v = 1.0f;
	if(v < -1.0f) v = -1.0f;
	return (int16_t)(v * 32767.0f);
}

/* PRODUCER (RT thread): never blocks. Drops the frame if the reader is behind.
 * Release store on head: on aarch64 the data stores must be visible before
 * the head update (hyperGuitar used plain volatile, which only worked because
 * the reader is slow to notice). */
static inline void ht_ring_push(HtRing *r, float fxL, float fxR, float dryA, float dryB)
{
	if(!r) return;
	uint32_t head = r->head;
	uint32_t tail = __atomic_load_n(&r->tail, __ATOMIC_ACQUIRE);
	if(head - tail >= HT_RING_FRAMES) return;
	int16_t *p = &r->data[(head & HT_RING_MASK) * HT_RING_LANES];
	p[0] = ht_f2s(fxL); p[1] = ht_f2s(fxR);
	p[2] = ht_f2s(dryA); p[3] = ht_f2s(dryB);
	__atomic_store_n(&r->head, head + 1, __ATOMIC_RELEASE);
}
#endif
