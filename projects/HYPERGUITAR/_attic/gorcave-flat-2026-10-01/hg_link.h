/*
 * hg_link.h - the NEPTR control link: lane model + OSC transport.
 *
 * WHY THIS EXISTS: the NeoTrellis came out of this instrument and went into
 * the PMOR v2 rebuild, so hyperGuitar has no control surface of its own. NEPTR
 * drives it instead, over the USB network link that the Bela's composite gadget
 * already provides (Pi 192.168.7.1 <-> Bela 192.168.7.2). This is a CONTROL
 * link only - audio still reaches NEPTR over the UAC2 bridge as before.
 *
 * ===================== THE LANE MODEL =====================
 *
 * NEPTR has five encoders. A LANE is what one encoder drives: an ordered list
 * of entries, a current index, and an on/off flag. Button = toggle the lane,
 * rotation = accumulate through its entries.
 *
 * An ENTRY is an (effect id, voicing) pair, and the voicings are lifted from
 * gCatSlot[][] in presets.cpp UNCHANGED - the v5-restored formant and pitch
 * pads are explicitly marked "do not retune" there, and this file is not the
 * place to second-guess that.
 *
 * Two exclusivity rules, which between them produce every behaviour asked for:
 *
 *   1. WITHIN a lane, at most one entry is active.
 *   2. ACROSS lanes, activating an entry deactivates any entry in another lane
 *      carrying the SAME effect id.
 *
 * Rule 2 is what makes GLIDE and STEPS a radio pair without special-casing
 * them - both drive PAD_PITCH, so one switches the other off. Rule 1 is what
 * lets DYNAMICS hold entries spanning four DIFFERENT effect ids (autowah,
 * formant-wah, pluck-sag, pog) and still behave as a single knob.
 *
 * ===================== WHY apply() ECHOES =====================
 *
 * hgLaneApply() is the ONLY way a lane changes, and it emits the state echo
 * itself rather than leaving that to the OSC handler. So a press arriving over
 * OSC and a press arriving from a future re-attached Trellis are
 * indistinguishable downstream, and the Trellis gets two-way sync for free
 * with no second code path to keep in step. This is the same "exactly ONE map"
 * discipline trellis_ui.cpp already enforces for pad dispatch.
 */
#ifndef HG_LINK_H
#define HG_LINK_H

#include "hyperguitar.h"

#define HG_NLANES 5

/* Network. The Bela listens here; the Pi is where state echoes go. */
#define HG_LISTEN_PORT 7772
#define HG_PI_HOST     "192.168.7.1"
#define HG_PI_PORT     7770    /* NEPTR's EXISTING control-OSC port (giCtlOSC) */

/*
 * The OSC aux task runs at a FIXED rate, derived in render() from the sample
 * rate the same way the trellis LED divider is. Fixed rather than "every N
 * blocks" because the buffer size is a tuning knob here (-p 64 today, -p 128
 * if underruns come back) and the pedal's feel and the heartbeat's period must
 * not move when it is turned.
 *
 * 200 Hz comfortably outruns NEPTR's pedal, which arrives as 7-bit CCs at
 * ~100 Hz, so the link is never the thing that makes the glide feel steppy.
 */
#define HG_OSC_HZ      200
#define HG_HELLO_TICKS HG_OSC_HZ   /* one heartbeat a second; NEPTR times out at 3 */

/*
 * How a lane's entries are interpreted. p1/p2 mean different things per kind,
 * which is the one piece of per-lane branching the model keeps - collapsing it
 * further would mean three parallel entry tables instead of one.
 */
typedef enum {
	HG_FX = 0,    /* p1/p2 = the effect's voicing, straight to applySlotPreset */
	HG_GLIDE,     /* p1/p2 = pedal range lo/hi in semitones */
	HG_SYNTH,     /* p1 = SYNTH_NAMES index, p2 = level */
} HgLaneKind;

typedef struct {
	signed char fx;        /* effect id this entry drives */
	float p1, p2;          /* per-kind, see HgLaneKind */
	const char *label;     /* what NEPTR's UI shows */
} HgEntry;

typedef struct {
	const char *name;
	HgLaneKind kind;
	const HgEntry *entries;
	int nEntries;
} HgLane;

extern const HgLane gHgLane[HG_NLANES];

/* live lane state - index is always valid, `on` is the toggle */
extern int gHgLaneOn[HG_NLANES];
extern int gHgLaneIdx[HG_NLANES];

/*
 * PEDAL GLIDE (lane 0). The one genuinely new DSP behaviour: pitch shift at
 * 100% wet with gPitchSemitones driven continuously by NEPTR's expression
 * pedal instead of parked at a fixed interval.
 *
 * gHgPedal is written by the OSC task (0..1) and read by render(). It is a
 * plain float rather than anything lock-free on purpose: a torn read of a
 * pedal position is one control block of a slightly wrong semitone value on a
 * signal that is already being slewed, which is inaudible - and the
 * alternative costs a lock on the audio thread, which is not.
 */
extern float gHgPedal;
extern int   gHgGlideOn;

/*
 * THE single mutation point. `echo` is 0 only for the initial boot apply,
 * where NEPTR has not said hello yet and there is nobody to tell.
 */
void hgLaneApply(int lane, int on, int idx, int echo);

/* emit one lane's state echo; the single point every surface funnels into */
void hgLaneEmit(int lane);

/*
 * REPORT A CHANGE THAT DID NOT COME FROM A LANE.
 *
 * The Trellis dispatch in trellis_ui.cpp predates the NEPTR link and writes
 * effect flags directly, so a pad press never reached hgLaneApply() and NEPTR's
 * screen went stale the moment the player touched the guitar. This is the way
 * back in: given the effect id and the voicing that was just applied, find the
 * lane entry that matches and echo it, so the two surfaces agree no matter
 * which one moved.
 *
 * Matching is by effect id AND voicing, because several lane entries share an
 * effect id (nine PAD_PITCH intervals, seven PAD_FORMANT vowels) and echoing
 * the wrong index would put NEPTR's readout confidently on the wrong preset.
 * An effect that no lane carries (JAWARI, SITAR, VIB, CHORUS, PAN, RES, SUS,
 * SYMP) simply is not reported: NEPTR's page shows five lanes and cannot
 * represent it, and inventing a row for it would be a lie.
 */
void hgReportFx(int fx, float p1, float p2, int on);

/* re-emit all five lanes; answers NEPTR's /hg/sync after a presence gap */
void hgEmitAll(void);

/* per-block: drive gPitchSemitones from the pedal when the glide lane is on */
void hgGlideTick(float blockSeconds);

/* transport - setup() creates the socket, the aux task drains it */
bool hgOscSetup(void);
void hgOscTick(void *);
void hgOscClose(void);

/*
 * Set an effect's on/off flag and run its recompute hook, going through
 * trellis_ui.cpp's gFx[] table rather than duplicating it. Defined there for
 * exactly that reason - gFx[] stays static and there is still only one map.
 */
void fxSetActive(int fx, int on);

#endif /* HG_LINK_H */
