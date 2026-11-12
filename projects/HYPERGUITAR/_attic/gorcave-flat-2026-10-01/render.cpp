/*
 * hyperGuitar: 6ch Subsix pickup, Db standard -> per-string hybrid pitch
 * engine (SOLAD down / TD-PSOLA up) + series formant + per-string FX.
 * Bela Gem Multi + NeoTrellis 4x4. Research: docs/eventide-pitch-shift-research.md
 *
 * PAD MAP v8 - 2026-09-24 MENU REDO. The top row of the 4x4 grid picks
 * the page; the other 12 pads are that page's slots. gCatSlot[][] in
 * presets.cpp is the single source of truth. Old table archived in
 * bela/archive/v7-menus-pre-2026-09-24/.
 *
 *     TONE     11 smooth formant ratios + FM GROWL (no pot/encoder)
 *     PITCH    OCT DN, OCT UP, 5TH DN, 4TH DN, 4TH UP, 5TH UP
 *              pot = dry/wet (TRS guitar vs shifted hex), enc1 = semitones
 *     MOD      CHORUS, VIBRATO, -, AUTO-WAH
 *              pot = depth / sensitivity (10% pickup), enc1 = rate / Q
 *     UTILITY  SPREAD (pot = amount, 10% pickup)
 *
 * LEDs: lava lamp under everything, solid blue = effect ON, breathe = the
 * armed effect the pot and encoders drive. USB audio: double-tap the
 * current page's menu pad.
 *
 * ENCODERS: enc1 = primary, enc2 = secondary, per effect; the clicks are
 * each effect's discrete toggles. See applyEncoders() for the full map.
 *
 * RETIRED in v7: the PRESET/MANUAL dual mode and its both-encoder-click
 * toggle. v5 booted into PRESET, which gated the encoders off entirely;
 * escaping it needed BOTH encoder buttons, so one unplugged switch made
 * every knob appear dead. Snapshot of that version, and the old flat pad
 * map it documented, is in bela/archive/v5-preset-era/.
 *
 * GAIN: Subsix-native input (1.0), master 1.0, smoothed-gain limiter at
 * LIM_CEIL guarantees no output distortion (1 ms attack, program-
 * dependent release, no lookahead). softClip catches the small
 * transient overshoot the attack lets past, and is otherwise dormant.
 *
 * VERIFIED channel map (solo-tested 2026-07-09, REMAPPED 2026-07-28
 * after the 2/3 jack broke off - each string kept its own tuning/pan/
 * effects identity, only the physical channel it reads from moved):
 *   ch6=D ch7=A ch8=G ch9=lowE ch4=B ch5=e  (0-1 Gem in, 2-3 broken/unused)
 *   (was ch2=D ch3=A ch4=G ch5=lowE ch8=B ch9=e)
 * Pans: E/A +-10%, D/G +-30%, B/e +-50%.
 *
 * SOURCE SPLIT: this file used to be the ~3200-line monolith containing
 * everything below. It's now split into pitch_tracker.cpp, dsp_core.cpp,
 * synth.cpp, presets.cpp, trellis_ui.cpp and effects.cpp (see
 * hyperguitar.h for the shared declarations) - Bela's build compiles
 * every .cpp it finds in the project dir and links them together, so
 * this is a pure reorganization, not a behavior change. render.cpp keeps
 * only the actual global variable definitions and the three Bela
 * lifecycle entry points (setup/render/cleanup).
 */

#include "hyperguitar.h"
#include "hg_link.h"
#include "hg_ctrl.h"
#include "i2c_sensors.h"
#include "hg_mon.h"

/*
 * TRELLIS KILL SWITCH - diagnostic, 2026-09-22.
 * Set to 1 and the trellis aux task is never scheduled: no I2C traffic, no
 * NeoPixel refresh, no LED current. Pads stop working, audio is untouched.
 * This is the A/B for "is the hum the Trellis or not" - if the hum survives
 * with this at 1, the Trellis is exonerated and the cause is elsewhere
 * (shared supply, the new sensors on the bus, or the buffer itself).
 */
#define TRELLIS_DISABLE 0

/* ===================== AUX INPUTS INTO THE MIX (2026-09-22) =====================
 * The hex pickup is the instrument, but three other sources are wired and were
 * never heard: the guitar's own TRS output and the two contact mics. Summed in
 * BEFORE the limiter so they are protected by it like everything else.
 *
 * Levels measured on this rig, not guessed: the TRS output ran ~35 dB hotter
 * than the hex pickup (-35 dB vs -73 at idle), so it needs a big trim to sit
 * with the strings rather than bury them. The piezos ran quiet through their
 * BC517 followers and need a modest lift.
 *
 * Set either gain to 0 to mute that source.
 */
static const float AUX_TRS_GAIN   = 0.10f;   /* ch0/1 guitar TRS (-20 dB) */
static const float AUX_PIEZO_GAIN = 2.50f;   /* ch2/3, contact mics */
static const int   AUX_TRS_A = 0, AUX_TRS_B = 1;
static const int   AUX_PZ_A  = 2, AUX_PZ_B  = 3;

/*
 * CONTACT-MIC SOLO - diagnostic. 1 mutes the entire instrument (hex pickup and
 * everything downstream of it) plus the TRS input, leaving ONLY the two piezos.
 * With the Trellis already disabled this isolates the buffer completely: any
 * buzz still audible is coming from the piezo circuit itself and nothing else.
 */
/* Was 1 for the isolation test on 2026-09-22 - the buffer came back COMPLETELY
 * QUIET, which cleared the piezo circuit of the residual buzz. Back to 0. */
#define AUX_PIEZO_SOLO 0

/* TRS SOLO - hear the guitar's own built-in pickup alone: hex pickup muted,
 * contact mics muted, no I2C at all. */
#define AUX_TRS_SOLO 0

/* ===================== MULTI-OUTPUT ROUTING (2026-09-23) =====================
 * Outputs 1-2 (indices 0,1) are the STEREO MIX and always carry it. The two
 * contact mics are hard-panned into that pair - piezo A hard left, piezo B
 * hard right - instead of the old both-mics-to-both-sides 0.5/0.5 sum, so the
 * body sensors have a stereo position of their own.
 *
 * Outputs 3 and up (indices 2+) are DIRECT PER-STRING FEEDS, one string each,
 * assigned lowest string to highest:
 *
 *   out 3 = low E    out 4 = A    out 5 = D    out 6 = G    out 7 = B ...
 *
 * The signal is that string's FULL processed chain (post pitch/formant/FX,
 * pre-pan), not its raw pickup - panning is a stereo-mix concern and a direct
 * out wants the whole voice, undivided.
 *
 * This is a mapping over whatever the hardware actually gives us, so it does
 * not hardcode an output count. setup() logs the real map, and says out loud
 * when there are fewer outputs than strings rather than silently dropping the
 * top string(s) - that would be a missing instrument nobody could explain.
 */

/*
 * TEST MODE - set to 1 to also send the hard-panned piezos across EVERY aux
 * output (odd outputs get the right mic, even ones the left), on top of the
 * per-string feed. Purpose: confirm every physical output is alive and
 * correctly ordered by tapping the guitar body, with no playing needed.
 *
 * SET THIS BACK TO 0 once the outputs are confirmed. The piezos then live
 * only on outputs 1-2, which is the intended end state.
 */
#define PIEZO_ON_ALL_OUTS 0

/* number of outputs above the stereo pair; filled in by setup() */
static int gNumAuxOut = 0;

/* Contact mics get a 2-pole 130 Hz high-pass. One pole at 70 Hz was tried and
 * was useless against mains hum - 6 dB/oct leaves 50/60 Hz barely touched. */
static float gAuxHpA = 0.0f;
static float gPzHp1X[2], gPzHp1Y[2], gPzHp2X[2], gPzHp2Y[2];
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

/*
 * USB AUDIO BRIDGE: the final stereo mix is ALSO pushed into a lock-free
 * SPSC ring buffer in shared memory, read by a separate non-real-time
 * companion process (usbaudio-bridge) that feeds the UAC2 USB gadget.
 * NEVER block or spin here - this runs inside the Xenomai/EVL real-time
 * audio thread. If the reader falls behind, we just drop samples; the
 * ring only exists to decouple the RT thread from any USB/ALSA timing,
 * never to apply backpressure to it.
 */
BridgeRing *gBridge = NULL;

static inline void bridgePush(float l, float r)
{
	if(!gBridge)
		return;
	uint32_t head = gBridge->head, tail = gBridge->tail;
	if(head - tail >= BRIDGE_RING_FRAMES)
		return;                     // reader behind - drop, never block
	if(l > 1.0f) l = 1.0f; if(l < -1.0f) l = -1.0f;
	if(r > 1.0f) r = 1.0f; if(r < -1.0f) r = -1.0f;
	uint32_t i = (head & BRIDGE_RING_MASK) * 2;
	gBridge->data[i]     = (int16_t)(l * 32767.0f);
	gBridge->data[i + 1] = (int16_t)(r * 32767.0f);
	gBridge->head = head + 1;
}

/* ============================ GLOBAL STATE ============================ */

/* C STANDARD (C-F-Bb-Eb-G-C), retuned from Db standard 2026-09-01: the
 * player tunes slightly FLAT of this by taste, which TUNE_MARGIN (1.12 =
 * ~196 cents of flat headroom on every per-string range) absorbs. The
 * old Db table left open strings a semitone below every assumed open -
 * the high C sat within 1 semitone of its tracker range ceiling, which
 * is where "any note below G tweaks" lived. */
extern const float OPEN_HZ[NUM_STRINGS] =
	{ 65.41f, 87.31f, 116.54f, 155.56f, 196.00f, 261.63f };
extern const int STRING_CHANNEL[NUM_STRINGS] = { 5, 4, 6, 8, 9, 7 };
// REMAPPED 2026-09-22, MEASURED not guessed. Was { 9, 7, 6, 8, 4, 5 }; only
// ch6 and ch8 were already correct. The player plucked lowest string to
// highest while hgTest ranked every channel by peak on each pluck, and each
// string held its winner for 4-6 consecutive plucks with 12-17 dB of margin
// over the crosstalk:
//     lowE->ch5  A->ch4  D->ch6  G->ch8  B->ch9  e->ch7
//
// Ranking ALL channels is what made this reliable. A per-channel onset
// detector was tried first and its output was unusable: this pickup
// crosstalks hard (one pluck lit ch5 at -23 dB AND ch4 at -37), so the bleed
// registered as extra notes and 6 strings produced 23 scrambled triggers.
// was { 5, 3, 2, 4, 8, 9 } - remapped 2026-07-28 after the 2/3 jack broke:
// same [lowE, A, D, G, B, e] index order, each string keeps its own
// tuning/pan/effects identity (OPEN_HZ/STRING_PAN/etc are untouched,
// keyed by string index not channel number) - only which physical
// input feeds that string moved: old 2->6, 3->7, 4->8, 5->9, 8->4, 9->5.
extern const float STRING_PAN[NUM_STRINGS] =
	{ -0.10f, +0.10f, -0.30f, +0.30f, -0.50f, +0.50f };
extern const float HARM_CUT[NUM_STRINGS] = { 6.0f, 6.0f, 8.0f, 8.0f, 10.0f, 10.0f };
extern const float LFO_PH[NUM_STRINGS] = { 0.0f, 1.1f, 2.2f, 3.3f, 4.4f, 5.5f };

// NEPTR vibrato (verified vs phase-4 vibrato.orc): unipolar sine sweep
float gVibDepthSec = 0.00106f;  // -20% from 1.33ms; encoder-adjustable
float gVibRateScale = 1.0f;     // encoder-adjustable multiplier on VIB_HZ[]
extern const float VIB_HZ[NUM_STRINGS] = { 0.72f, 0.75f, 0.78f, 0.81f, 0.84f, 0.87f };

// CHORUS: subtle second voice, mirrored across the stereo image per string
float gChorDepthSec = 0.002f;   // encoder-adjustable
float gChorRateScale = 1.0f;    // encoder-adjustable multiplier on CHOR_HZ[]
extern const float CHOR_HZ[NUM_STRINGS] = { 0.31f, 0.41f, 0.53f, 0.67f, 0.83f, 1.01f };

// AUTOWAH (classic env filter): restored pre-quack voicing,
// threshold raised ~15% (sens x0.85), Q a sliver over original
float gEnvfQ    = 0.7071f;  // no resonance - straightforward sweep; encoder-adjustable
float gEnvfSens = 2.125f;   // encoder-adjustable
float gFWahSens = 2.0f;  // formant wah sensitivity (encoder)

// SYNTH LEAD: MONOPHONIC epic lead.
volatile int gSynthMode = 0;
volatile int gSynthPoly = 1; // 0 = mono, 1 = poly - starts POLY (owner, 2026-09-24)
float gSynthLevel = 1.0f;
float gSynthGateThr = 0.010f;  // note-off level (raised)
const char *SYNTH_NAMES[SYNTH_NMODES] = {
	"SAW LEAD",     // 0 the one that works - untouched
	"MOOG LEAD",    // 1 saw through a resonant lowpass + snap filter env
	"ACID SQUELCH", // 2 303-style pulse, near-self-osc filter sweep
	"WAVEFOLDER",   // 3 sine through a tanh wavefolder, breathing fold depth
	"RINGMOD BELL", // 4 carrier x irrational-ratio modulator, inharmonic
	"SYNC LEAD",    // 5 hard-synced slave osc, sweeping ratio
	"FM GROWL",     // 6 2:1 FM, dynamic pick-driven index
	"CHIP LEAD",    // 7 stepped-duty pulse, raw 8-bit character
	"SUB GROWL",    // 8 fundamental + sub-octave through a dark lowpass
	"FORMANT VOWEL",// 9 real 2-band resonant filterbank, ooh<->ah morph
	"WARM TRI",     // 10 two soft triangles 4c apart - SYNTHESIS default
	"GLASS",        // 11 sine + a whisper of 2nd/3rd harmonic
	"SOFT SQUARE",  // 12 band-limited square, double lowpass
	"SUPERSAW",     // 13 six saws +-19c, softened top
	"DRIFT PAD",    // 14 three sines, one drifting, octave air
};

// POG SWELL: ~100ms per-note attack bloom (EHX Swello-style, per string)
float gPogAttackSec = 0.18f; // quick jazz swell (Metheny-ish)
float gPogModDepth = 0.0f;  // aux param: filter-sweep depth, +/- octaves

// RESONATOR
float gResSens = 2.0f;    // MAIN
float gResFb   = 0.965f;  // AUX: how long the overtone rings
float gResGain = 2.2f;

// SUSTAINER
float gSusFb    = 0.995f; // MAIN: hold time (top = infinite)
float gSusDamp  = 0.28f;  // AUX: tone of the held note
float gSusLevel = 1.8f;

// JAWARI BUZZ
float gJawBuzz  = 0.6f;   // MAIN: buzz amount
float gJawPitch = 90.0f;  // AUX: metallic pitch of the rattle
float gJawSens  = 2.0f;

// SITAR TONE
extern const int BHAIRAVA[TARAB_N] = { 0,1,4,5,7,8,11, 12,13,16,17,19,20,23 };
float gSitarRing  = 0.9990f; // AUX: how long the tarab strings ring
float gSitarLevel = 0.75f;   // MAIN: wet fraction added on top of
                              // the (always 100%) dry signal
float gSitarBuzz  = 0.35f;   // jawari zing on the direct voice

// PLUCK SAG
float gSagDepthSec = 0.006f;
float gSagSettleSec = 0.08f;

// SYMPATHETIC RESONANCE
float gSympCoupling = 1.3f; // MAIN
float gSympFb = 0.986f;     // AUX: ring length
float gLastMixed[NUM_STRINGS];
float gTarabDelay[TARAB_N];
float gTarabPan[TARAB_N];

// STEREO SPREAD (pad 16). Default spread 0.5: the STRING_PAN pattern
// stays the canonical +-10/30/50%, the spread scales it so the top pair
// sits half-panned (+-25%) - full pattern felt hard-panned by ear.
float gPanSpread = 0.5f;
float gPanDyn = 0.0f;

// ---- radio/toggle state ----
volatile int gPitchActive = 0;   // simple on/off - value comes from encoders
volatile int gFormantActive = 0; // simple on/off - value comes from encoders
volatile int gFormantMode = 0;   // 0 = GRAIN (IVL-style), 1 = LPC envelope warp
unsigned int gSampleStamp = 0;
unsigned int gOnsetStamp[NUM_STRINGS] = { 0 };
volatile int gFmActive = 0;      // simple on/off - value comes from encoders

// PITCH: main=semitones(-24..+24) or click->cents(+-50), aux=wet% 0-100 in 10s
float gPitchSemitones = -12.0f;
int gPitchCentsMode = 0;
float gPitchCents = 0.0f;
float gPitchWetPct = 100.0f;

// FM: main=ratio (click toggles quantized{0.25,0.5,1,2,3,4}<->free), aux=index
float gFmRatioVal = 0.5f;
int gFmQuantized = 1;
float gFmIndexVal = 0.80f;

// FORMANT: main=ratio 0.4-2.2, aux=glide
float gFormantVal = 1.0f;
float gFormantGlide = 0.05f;  // per-hop smoothing coeff, ~57ms glide
float gFormantTiltBias = 0.0f; // extra formant-driven EQ tilt, preset-only
volatile int gPresetFormantWobble = 0;  // slow LFO breathes the ratio
volatile int gVibOn = 0, gChorusOn = 0, gEnvFOn = 0, gFWahOn = 0;
volatile int gSineOn = 0, gPogOn = 0, gResOn = 0, gSusOn = 0;
volatile int gJawOn = 0, gSitarOn = 0, gPanOn = 1; // pan starts ON
volatile int gSagOn = 0, gSympOn = 0;
volatile float gBetaBase[NUM_STRINGS] = { 1, 1, 1, 1, 1, 1 };
volatile float gFormantBase[NUM_STRINGS] = { 1, 1, 1, 1, 1, 1 };
volatile float gDryTarget = 1.0f;
volatile float gWetTarget = 0.0f;

Comb gTarab[TARAB_N];   // the sympathetic string bank
int gTarabLeadStr = 0;  // which string the tarab scale is currently rooted on
float gOtherMax[NUM_STRINGS]; // loudest OTHER string, for crosstalk rejection
float gSympW[NUM_STRINGS][NUM_STRINGS]; // harmonic-relatedness coupling weights

Voice gV[NUM_STRINGS];
float gDryS = 1.0f, gWetS = 0.0f;
float gVibAmtS = 0, gChorAmtS = 0, gEnvFAmtS = 0;
float gFmAmtS = 0, gSineAmtS = 0, gPogAmtS = 0;
float gResAmtS = 0, gSusAmtS = 0;
float gJawAmtS = 0, gSitarAmtS = 0, gPanAmtS = 0;
float gSagAmtS = 0, gSympAmtS = 0;

// ---- GPIO rotary encoders (main=enc1, aux=enc2) ----
Encoder gEnc1, gEnc2;
/* button debounce integrators - see the block in render() */
static int sEnc1Int = 0, sEnc2Int = 0;
int gEnc1LastPos = 0, gEnc2LastPos = 0;
int gEnc1BtnStable = 1, gEnc2BtnStable = 1; // debounce state, 1=released
int gEnc1BtnHold = 0, gEnc2BtnHold = 0;     // debounce countdown
volatile int gEnc1Click = 0, gEnc2Click = 0; // one-shot flags, consumed then cleared
volatile int gArmedPad = FX_NONE; // EFFECT id the encoders drive (not a pad index)
volatile int gRawDiag = 0;   // set 1 to log raw input levels every second
volatile int gStateDirty = 0; // set on any change, autosaved & cleared in trellisTick
volatile int gCurCategory = CAT_SPECTRAL; // which page the grid is showing
/*
 * RETIRED in v6. v5 booted into PRESET and gated the encoders behind
 * `!gPresetMode`, escapable only by clicking BOTH encoder buttons. One
 * dead encoder switch therefore made every knob look broken - which is
 * exactly the fault that prompted this redesign. There is no mode now;
 * pads and encoders behave identically at all times. Pinned to 0 so the
 * archived v5 code in bela/archive/v5-preset-era/ still compiles.
 */
volatile int gPresetMode = 0;
volatile int gModeFlash = 0;            // white-flash countdown (LED, ~30Hz)
int gPresetFormantSel = -1;   // pads 0-3
int gPresetPitchSel = -1;     // pads 4-7
int gPresetVibChorusSel = -1; // 0=vib(pad8), 1=chorus(pad9), -1=off
int gSwInit = 0, gEnc1Idle = 1, gEnc2Idle = 1;
float gLimEnv = 0.0f;

/*
 * ===================== MASTER OUTPUT STAGE STATE =====================
 *
 * THE CLICK BUG (2026-08-12). The master limiter used to be a
 * zero-attack gain rider:
 *     gLimEnv = pk > gLimEnv ? pk : gLimEnv * 0.9998f;
 *     if(gLimEnv > LIM_CEIL) { g = LIM_CEIL/gLimEnv; outL *= g; ... }
 * gLimEnv jumped to a new peak in ONE sample, so the applied gain g
 * STEPPED discontinuously the instant a peak crossed the ceiling. Every
 * pick attack that engaged it put a gain step - i.e. a click - straight
 * onto the master bus, which is why the clicks were audible in
 * everything regardless of which effect was on.
 *
 * The fix is the standard smoothed-GAIN limiter: the detector may still
 * react instantly (it only has to KNOW we are over), but the gain that
 * multiplies the audio is itself one-poled, so it can never step.
 *
 * Coefficients are one-pole c = 1 - exp(-1/(tau*sr)) at sr = 44100 (the
 * rate this rig runs at; the pre-existing 0.9998/0.9995-style constants
 * elsewhere in the file make the same assumption).
 */
static float sLimGain = 1.0f;   // the gain actually applied to the audio

// 1.0 ms attack. A gain change spread over ~1 ms is below the ~2 ms
// window in which the ear hears a gain move as a discrete click, yet
// short enough that the overshoot it lets past stays inside softClip's
// smooth knee (see the softClip note below) instead of reaching the
// converter. Slower than this and the flat-topped patch on a hard strum
// starts to be audible as distortion instead of inaudible as limiting.
#define LIM_ATK_C 0.02243f

// Program-dependent release. A short isolated transient only pulls the
// gain down a little, and wants to be given back quickly (60 ms) so the
// note behind it is not ducked. A sustained loud passage - a full six
// string strum - pulls it down hard, and wants a slow (400 ms) recovery
// or the limiter pumps in time with the strum. Interpolating between the
// two on the CURRENT amount of gain reduction keeps the coefficient a
// continuous function of state, so the release itself never steps.
#define LIM_REL_FAST 3.780e-4f  // tau 60 ms  - shallow reduction
#define LIM_REL_SLOW 5.670e-5f  // tau 400 ms - deep reduction
#define LIM_REL_KNEE 0.30f      // reduction depth that reaches full slow

// Detector decay, tau ~45 ms. Was 0.9998 (tau 113 ms), which used to be
// the only release in the system; now that the smoothed gain owns the
// audible recovery, the detector is deliberately faster so the
// program-dependent release above is what you actually hear rather than
// being masked by the peak follower's own hold.
#define LIM_ENV_REL 0.9995f

/*
 * SAFETY SCRUB, tier 1: a C1 soft saturator for finite-but-huge values.
 * Identity below SCRUB_SOFT (nothing on a normal signal is touched),
 * asymptotic to SCRUB_MAX above it, derivative exactly 1 on both sides
 * of the knee. Keeps every feedback path that is fed from gLastMixed[]
 * bounded WITHOUT the step that hard-zeroing produced.
 */
#define SCRUB_SOFT 8.0f
#define SCRUB_MAX  32.0f
// tier 2 recovery: NaN/Inf really does have to be replaced with silence,
// but coming BACK from that silence in one sample is a second click.
// ~10 ms fade-in (1/(0.010*44100)) makes the re-entry inaudible.
#define SCRUB_FADE_STEP 0.00227f
static float sScrubFade[NUM_STRINGS] = { 1, 1, 1, 1, 1, 1 };

/*
 * PAN SMOOTHING. v->gainL/gainR are recomputed once per HOP (128
 * samples, ~2.9 ms) inside updateModulation(), from the pad's on/off
 * state and from the per-string envelope when dynamic pan is up. Both
 * therefore land on the output as a STEP at the hop boundary - a tick on
 * every attack with dynamic pan engaged, and a bang when the pan pad is
 * toggled (gPanAmtS is smoothed but nothing ever uses it). One-poling
 * the gains here, at the point of use, turns every one of those steps
 * into a ~5 ms slew: fast enough that the image still tracks the pick,
 * slow enough that nothing is heard as an edge.
 */
#define PAN_SM_C 0.0045f        // tau ~5 ms
static float sPanL[NUM_STRINGS], sPanR[NUM_STRINGS];
float gHannT[HANN_N + 1];
double gTimeSec = 0.0;

// monophonic lead voice state
int gLeadStr = 0;
float gLeadInc = 0.002f, gLeadIncTgt = 0.002f; // cycles/sample
float gLeadAmp = 0.0f;
float gLeadLPz = 0.0f;

// NeoTrellis
int gI2cFd = -1;
int gTrellisOk = 0;
float gPadFlash[NPIX];
float gLavaT = 0.0f;
uint8_t gGammaLUT[256];
AuxiliaryTask gTrellisTask;

/* see hyperguitar.h - which physical encoder is fitted, told by LED colour */
/* Input/output levels for the web UI. Written by render(), READ by the
 * trellis aux task - monTick() does socket I/O and must never run on the
 * audio thread. */
float gMonInRms[10];
float gMonOutRms[2];

volatile int gEncFlashWho = 0;
volatile int gEncFlashCount = 0;

/* MPU6050 + VL53L1X, read on the trellis task (see trellis_ui.cpp for why).
 * Global so the rest of the instrument can use them as modulation sources. */
/* NO I2C AT ALL (diagnostic, 2026-09-22). With this set, sensorsInit() is never
 * called and the trellis task is never scheduled, so nothing touches
 * /dev/i2c-1: no pad grid, no LED refresh, no MPU6050, no VL53L1X. Combined
 * with both aux gains at 0 this is the bare hex pickup and nothing else - the
 * quietest the instrument can possibly be, and the reference point to measure
 * every noise source against. */
#define I2C_DISABLE_ALL 0

/* ---- GRANULAR I2C, because the bus is audible on this rig ----
 * Each source is switchable on its own so the noise can be attributed instead
 * of guessed at.
 *
 * RESULT SO FAR: the VL53L1X alone at 4 Hz produced clearly audible clicks at
 * exactly 4 per second - the noise tracks the poll rate one-for-one, so it is
 * the ranging EVENT, not anything continuous.
 *
 * ANSWERED 2026-09-22: the MPU6050 at the SAME 4 Hz was completely SILENT.
 * That settles it. The MPU's read is 14 bytes against the ToF's 4, so if the
 * I2C bus itself were coupling the MPU would have been the worse case - it was
 * not. The noise is the VL53L1X's VCSEL laser current pulse, one per ranging
 * event, on the shared supply.
 *
 *   => FIX: decoupling (0.1uF ceramic + 10uF) at the VL53L1X's own power pins,
 *      within ~5mm. Not at the star point, not midway down the cable.
 *   => The MPU6050 can be polled freely; it costs nothing audible.
 */
/* 2026-09-23: Trellis OFF, sensors ON. The 100R series resistors went on
 * SDA/SCL and made NO difference to the Trellis hum - and the hum's pitch
 * tracks LED colour and two lit pads beat against each other, which is
 * per-LED PWM current, not a bus. So the bus theory is only still open for
 * the LIDAR's click, which is a different noise. This build tests exactly
 * that: sensors polled at the same 15 Hz, no LED current anywhere. */
#define ENABLE_TRELLIS 1
#define ENABLE_MPU     1   /* measured silent - free to poll */
#define ENABLE_TOF     1   /* ON - under test against the 100R resistors */

/* Poll rate for the sensors, in Hz. LOW ON PURPOSE: every I2C transaction is a
 * burst of switching right next to the audio front end, so the fewer per
 * second the better. A distance sensor used as a modulation source does not
 * need to be fast - a hand moves at human speed, and 4 Hz is already ahead of
 * it. If it is still audible at 4 Hz, the bus is the problem, not the rate. */
#define SENSOR_POLL_HZ 15  /* fast on purpose: worst case for the A/B */

SensorState gHgSens;
int gHgSensorDiv = 8;   /* set from SENSOR_POLL_HZ in setup() */
int gTrellisDisabled = !ENABLE_TRELLIS;
AuxiliaryTask gHgOscTask;

/* ======================= Bela lifecycle ======================= */

bool setup(BelaContext *context, void *userData)
{
	float sr = context->audioSampleRate;
	rt_printf("hyperGuitar v5: in %d ch, OUT %d ch, sr %.0f, block %d\n",
			context->audioInChannels, context->audioOutChannels, sr,
			context->audioFrames);

	/* ---- output map, printed so the wiring can be checked against it ---- */
	gNumAuxOut = (int)context->audioOutChannels - 2;
	if(gNumAuxOut < 0) gNumAuxOut = 0;
	rt_printf("out 1/2: STEREO MIX + piezo A hard L / piezo B hard R\n");
	{
		static const char *SNAME[NUM_STRINGS] =
			{ "lowE", "A", "D", "G", "B", "highE" };
		for(int a = 0; a < gNumAuxOut && a < NUM_STRINGS; a++)
			rt_printf("out %d: string %s (audio in ch %d)\n",
					3 + a, SNAME[a], STRING_CHANNEL[a]);
		for(int a = NUM_STRINGS; a < gNumAuxOut; a++)
			rt_printf("out %d: unused\n", 3 + a);
		if(gNumAuxOut < NUM_STRINGS)
			rt_printf("*** WARNING: only %d aux output(s) for %d strings - "
					"%s has NO direct out (it is still in the stereo mix) ***\n",
					gNumAuxOut, NUM_STRINGS,
					SNAME[gNumAuxOut < 0 ? 0 : gNumAuxOut]);
	}
	if(PIEZO_ON_ALL_OUTS)
		rt_printf("*** TEST MODE: piezos ALSO on every aux output - "
				"set PIEZO_ON_ALL_OUTS 0 once outputs are confirmed ***\n");

	// Restore whatever was active last time the board was powered -
	// applyPitch()/applyFormant() push the loaded values into
	// gBetaBase[]/gFormantBase[]; per-voice beta/formant glide toward
	// them over the first few hops exactly like any live parameter
	// change already does, so no special pre-seeding is needed.
	loadState();
	applyPitch();
	applyFormant();

	/*
	 * USB AUDIO BRIDGE shared memory: fresh segment every run (unlink
	 * first so a stale head/tail from a previous run can never cause
	 * the bridge process to think there's a backlog to catch up on).
	 * If this fails for any reason, gBridge stays NULL and bridgePush()
	 * is just a no-op - the instrument runs exactly as before, USB
	 * audio is the ONE thing that's degraded, never anything else.
	 */
	{
		shm_unlink(BRIDGE_SHM_NAME);
		int fd = shm_open(BRIDGE_SHM_NAME, O_CREAT | O_RDWR, 0666);
		if(fd >= 0) {
			if(ftruncate(fd, sizeof(BridgeRing)) == 0) {
				void *m = mmap(NULL, sizeof(BridgeRing), PROT_READ | PROT_WRITE,
						MAP_SHARED, fd, 0);
				if(m != MAP_FAILED) {
					gBridge = (BridgeRing *)m;
					memset(gBridge, 0, sizeof(BridgeRing));
					rt_printf("USB audio bridge: shared ring ready\n");
				}
			}
			close(fd);
		}
		if(!gBridge)
			rt_printf("USB audio bridge: shm setup failed - USB audio disabled, guitar unaffected\n");
	}

	for(int i = 0; i <= HANN_N; i++)
		gHannT[i] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * i / (float)HANN_N));

	for(int s = 0; s < NUM_STRINGS; s++) {
		Voice *v = &gV[s];
		if(STRING_CHANNEL[s] >= (int)context->audioInChannels) {
			rt_printf("ERROR: channel %d not available\n", STRING_CHANNEL[s]);
			return false;
		}
		memset(v, 0, sizeof(Voice));
		v->p0 = sr / OPEN_HZ[s];
		v->period = v->p0;
		v->beta = 1.0f;
		v->formant = 1.0f;
		v->pogGain = 1.0f;
		v->harmCut = HARM_CUT[s];
		v->srate = sr;
		v->fcCur = v->harmCut * OPEN_HZ[s];
		updateLP(v);

		shifterInit(v);

		v->vPhase = LFO_PH[s];
		v->cPhase = LFO_PH[s] * 0.7f;
		v->formantWobblePhase = LFO_PH[s]; // stagger so strings don't wobble in unison
		v->svfF = v->svfFT = 2.0f * sinf((float)M_PI * ENVF_FMIN / sr);
		v->fmA = gFmIndexVal * v->p0 / (2.0f * (float)M_PI);
		v->sinePer = v->p0;
		v->sineInc = 2.0f * (float)M_PI / v->p0;

		v->lagMax = (int)(v->p0 * TUNE_MARGIN / (float)DECIM);
		v->lagMin = (int)(v->p0 / (MAX_FRET_RATIO * (float)DECIM));
		if(v->lagMin < 4) v->lagMin = 4;
		v->corrWin = (int)(2.0f * v->p0 / (float)DECIM);
		v->hopCountdown = (s * HOP) / NUM_STRINGS;

		float angle = (STRING_PAN[s] + 1.0f) * 0.25f * (float)M_PI;
		v->gainL = cosf(angle);
		v->gainR = sinf(angle);
		// seed the per-sample pan smoother so the first block starts at
		// the right image instead of sliding in from hard left
		sPanL[s] = v->gainL;
		sPanR[s] = v->gainR;

		// SHAKUHACHI/NEY: per-string noise generator seed

		v->susTime = 1.0f;
		v->tiltLoG = 1.0f;   // auto-EQ neutral until first hop computes it
		v->tiltHiG = 1.0f;
		memset(&v->resC, 0, sizeof(Comb));
		memset(&v->susC, 0, sizeof(Comb));
		memset(&v->jawC, 0, sizeof(Comb));
		memset(&v->sympC, 0, sizeof(Comb));

		// SYMPATHETIC RESONANCE: starts locked to this string's own open note

		// SYNTH POLY: per-string oscillator starts at its open-string pitch,
		// phases staggered so the six voices don't start phase-locked
		v->pInc = 1.0f / v->p0;
		for(int k = 0; k < 6; k++)
			v->pPh[k] = (float)((s * 7 + k * 13) % 100) * 0.01f;
		v->pLfo = (float)s * 0.15f;
	}

	// tune the tarab bank: Bhairava raga rooted WAY above the guitar's
	// low string - real tarab strings are thin steel, tuned high above
	// the main strings so they shimmer as a separate high scale, not
	// unison with what's played (unison-pitch ringing reads as echo).
	{
		float root = 26.0f * OPEN_HZ[0];  // ~1700 Hz on a C low string -
		                                   // deliberately way up top, real
		                                   // sitar tarab register
		for(int k = 0; k < TARAB_N; k++) {
			float f = root * powf(2.0f, (float)BHAIRAVA[k] / 12.0f);
			gTarabDelay[k] = sr / f;
			gTarabPan[k] = 0.5f + 0.42f * sinf((float)k * 1.7f); // spread them
			memset(&gTarab[k], 0, sizeof(Comb));
		}
	}

	gEnc1.setup(15, Encoder::ANY);
	gEnc2.setup(15, Encoder::ANY);
	pinMode(context, 0, PIN_ENC1_CLK, INPUT);
	pinMode(context, 0, PIN_ENC1_DT, INPUT);
	pinMode(context, 0, PIN_ENC1_SW, INPUT);
	pinMode(context, 0, PIN_ENC2_CLK, INPUT);
	pinMode(context, 0, PIN_ENC2_DT, INPUT);
	pinMode(context, 0, PIN_ENC2_SW, INPUT);
	pinMode(context, 0, PIN_JOY_SW, INPUT);   /* joystick push-button */
	hgCtrlSetup();

	if(trellisInit() == 0) {
		gTrellisOk = 1;
		rt_printf("NeoTrellis v7 CATEGORY MENU: menu column pads 0/4/8/12 = "
				"%s|%s|%s|%s, 12 FX slots each. "
				"Encoders always live on the armed effect (no modes).\n",
				gCatName[CAT_SPECTRAL], gCatName[CAT_PITCH],
				gCatName[CAT_MOD], gCatName[CAT_FILTER]);
	} else {
		rt_printf("NeoTrellis not found - audio runs, no UI\n");
	}
	gHgSensorDiv = 30 / (SENSOR_POLL_HZ > 0 ? SENSOR_POLL_HZ : 1);
	if(gHgSensorDiv < 1) gHgSensorDiv = 1;
	gEnableMpu = ENABLE_MPU;
	gEnableTof = ENABLE_TOF;
	if(!I2C_DISABLE_ALL && (ENABLE_MPU || ENABLE_TOF))
		sensorsInit(&gHgSens);
	gTrellisTask = Bela_createAuxiliaryTask(trellisTick, 50, "trellis");

	/*
	 * NEPTR CONTROL LINK. The NeoTrellis went into the PMOR v2 rebuild, so
	 * NEPTR's five encoders are this instrument's control surface now - see
	 * hg_link.h. A failure here is NOT fatal: the instrument must still make
	 * sound with no Pi attached, exactly as it does with no Trellis attached.
	 */
	if(hgOscSetup()) {
		gHgOscTask = Bela_createAuxiliaryTask(hgOscTick, 50, "hg-osc");
		/* echo 0: nobody has said hello yet, there is no one to tell */
		for(int L = 0; L < HG_NLANES; L++)
			hgLaneApply(L, 0, 0, 0);
	} else {
		gHgOscTask = 0;
		rt_printf("hg-osc: link down - audio runs, no NEPTR control\n");
	}
	gAuxHpA = expf(-2.0f * (float)M_PI * 130.0f / context->audioSampleRate);
	for(int i = 0; i < 2; i++) {
		gPzHp1X[i] = gPzHp1Y[i] = gPzHp2X[i] = gPzHp2Y[i] = 0.0f;
	}
	rt_printf("*** I2C: trellis=%d mpu=%d tof=%d @ %d Hz ***\n",
			ENABLE_TRELLIS, ENABLE_MPU, ENABLE_TOF, SENSOR_POLL_HZ);
	if(AUX_TRS_SOLO)
		rt_printf("*** TRS SOLO - built-in pickup only, hex+mics muted, no I2C ***\n");
	if(AUX_PIEZO_SOLO)
		rt_printf("*** CONTACT MIC SOLO - pickup and TRS muted ***\n");
	rt_printf("aux mix: TRS ch%d/%d x%.2f, contact mics ch%d/%d x%.2f\n",
			AUX_TRS_A, AUX_TRS_B, AUX_TRS_GAIN,
			AUX_PZ_A, AUX_PZ_B, AUX_PIEZO_GAIN);

	monSetup();

	srand(12345);
	return true;
}

void render(BelaContext *context, void *userData)
{
	static unsigned int ledDivider = 0;
	unsigned int ledPeriod =
		(unsigned int)(context->audioSampleRate / 30.0f / context->audioFrames);
	/* Scheduled even when the pad grid is disabled - the task also carries the
	 * sensor poll, and gTrellisDisabled makes it skip only the grid half. */
	if(!I2C_DISABLE_ALL && (ENABLE_TRELLIS || ENABLE_MPU || ENABLE_TOF)
			&& gTrellisOk && ++ledDivider >= ledPeriod) {
		ledDivider = 0;
		Bela_scheduleAuxiliaryTask(gTrellisTask);
	}

	/*
	 * NEPTR link, on the SAME divider pattern as the LEDs above. The rate is
	 * pinned to HG_OSC_HZ rather than to a block count so that changing -p
	 * does not change how the expression pedal feels or how often the
	 * heartbeat that gates NEPTR's menu goes out.
	 */
	static unsigned int hgDivider = 0;
	unsigned int hgPeriod =
		(unsigned int)(context->audioSampleRate / (float)HG_OSC_HZ / context->audioFrames);
	if(hgPeriod < 1) hgPeriod = 1;
	if(gHgOscTask && ++hgDivider >= hgPeriod) {
		hgDivider = 0;
		Bela_scheduleAuxiliaryTask(gHgOscTask);
	}
	/* pedal -> pitch, every block: this is the only part of the link that
	 * touches the audio path, and it is a single smoothed float write. */
	hgGlideTick((float)context->audioFrames / context->audioSampleRate);

	/*
	 * RAW-INPUT WATCHDOG. Measures the ADC signal BEFORE any DSP touches
	 * it, so we can prove whether the "horrible noise until reboot" is
	 * arriving from the codec (analog/grounding/ESD - the input latches
	 * into a bad state and only a codec re-init clears it) or is being
	 * generated by the DSP. Also auto-recovers: if the raw input stays
	 * pathological (railed / huge DC) for ~2 s, exit so the boot service
	 * relaunches us - a codec re-init without you touching anything.
	 */
	{
		static unsigned int diagCtr = 0;
		static unsigned int badBlocks = 0;
		float pk[NUM_STRINGS] = {0}, dc[NUM_STRINGS] = {0};
		for(int s = 0; s < NUM_STRINGS; s++) {
			for(unsigned int n = 0; n < context->audioFrames; n++) {
				float x = audioRead(context, n, STRING_CHANNEL[s]);
				float a = fabsf(x);
				if(a > pk[s]) pk[s] = a;
				dc[s] += x;
			}
			dc[s] /= (float)context->audioFrames;
		}
		int bad = 0;
		for(int s = 0; s < NUM_STRINGS; s++)
			if(pk[s] > 0.95f || fabsf(dc[s]) > 0.18f)
				bad = 1;   // railed or big DC = codec is sick. 0.08 was
				           // WRONG - live logs show normal picking/bends
				           // swing a single 64-sample block-mean up to
				           // ~0.13 by itself, so that threshold false-
				           // triggered mid-play and restarted the whole
				           // process while playing. The true ESD latch
				           // sits at a rock-steady ~0.25. 0.18 clears
				           // real playing's observed ceiling with margin
				           // while staying below the fault signature.
		if(bad) {
			if(badBlocks < 0x7fffffff) badBlocks++;
		} else {
			badBlocks = (badBlocks > 4) ? badBlocks - 4 : 0;
			// decay, don't instantly reset - one clean block mid-fault
			// shouldn't un-arm the 2s counter (unsigned: guard vs wrap)
		}

		if(++diagCtr >= (unsigned int)(context->audioSampleRate / (float)context->audioFrames)) {
			diagCtr = 0;
			if(bad || gRawDiag)
				rt_printf("RAW-IN pk %.2f %.2f %.2f %.2f %.2f %.2f | dc %.3f %.3f %.3f %.3f %.3f %.3f%s\n",
						pk[0],pk[1],pk[2],pk[3],pk[4],pk[5],
						dc[0],dc[1],dc[2],dc[3],dc[4],dc[5],
						bad ? "  <<< CODEC INPUT IS SICK (analog/ESD, not DSP)" : "");
		}
		// ~2 s of pathological input: bail out so the boot service restarts
		// us and the codec gets re-initialised. Beats rebooting by hand.
		if(badBlocks > (unsigned int)(2.0f * context->audioSampleRate / context->audioFrames)) {
			rt_printf("RAW-IN stuck pathological for 2s - restarting to re-init the codec\n");
			Bela_requestStop();
		}
	}

	float sr = context->audioSampleRate;
	float vibDepthSmp = gVibDepthSec * sr;
	float chorBaseSmp = CHOR_BASE_SEC * sr;
	float chorDepthSmp = gChorDepthSec * sr;
	float pogStep = 1.0f / (gPogAttackSec * sr);

	// GPIO rotary encoders: read every digital frame this block
	for(unsigned int n = 0; n < context->digitalFrames; n++) {
		bool a1 = digitalRead(context, n, PIN_ENC1_CLK);
		bool b1 = digitalRead(context, n, PIN_ENC1_DT);
		Encoder::Rotation r1 = gEnc1.process(a1, b1);
		if(r1 == Encoder::CW) gEnc1LastPos++;
		else if(r1 == Encoder::CCW) gEnc1LastPos--;

		bool a2 = digitalRead(context, n, PIN_ENC2_CLK);
		bool b2 = digitalRead(context, n, PIN_ENC2_DT);
		Encoder::Rotation r2 = gEnc2.process(a2, b2);
		if(r2 == Encoder::CW) gEnc2LastPos++;
		else if(r2 == Encoder::CCW) gEnc2LastPos--;

		/*
		 * Buttons: LEARN the idle level on the first frame instead of
		 * assuming active-low. The two modules idle at OPPOSITE levels
		 * (diagnostics showed enc1 sw=1, enc2 sw=0), so a hardcoded
		 * polarity makes one of them never fire - which is exactly why
		 * the mono/poly click was unreliable.
		 */
		bool sw1 = digitalRead(context, n, PIN_ENC1_SW);
		bool sw2 = digitalRead(context, n, PIN_ENC2_SW);
		if(!gSwInit) {
			gEnc1Idle = sw1; gEnc1BtnStable = 0;   // 0 = not pressed
			gEnc2Idle = sw2; gEnc2BtnStable = 0;
			gSwInit = 1;
		}
		/*
		 * BUTTON DEBOUNCE - INTEGRATOR + REFRACTORY (2026-09-23, v2).
		 *
		 * v1 was edge detection with a blanking window, and it still double
		 * fired. Blanking only hides bounce for a fixed time: the press edge
		 * fires, the window closes, and a LATE bounce on RELEASE then reads
		 * as a fresh press. Making the window longer just moves the failure.
		 *
		 * This does not look at edges at all. An integrator counts up while
		 * the contact reads pressed and down while it reads released, so
		 * chatter averages out instead of triggering. The state only flips to
		 * pressed after ENC_BTN_INTEG samples of SOLID contact, and only back
		 * to released after the counter has drained fully to zero - so a
		 * bouncing contact can never complete a round trip.
		 *
		 * On top of that a refractory period after each click makes a second
		 * click physically impossible for ENC_BTN_REFRACT samples. A human
		 * cannot press faster than that; a bouncing switch can. That is the
		 * whole difference, and it is why this cannot fail the way v1 did.
		 *
		 * gEnc*BtnHold is reused as the refractory counter rather than adding
		 * new globals, since its old blanking role is gone.
		 */
		int p1 = (sw1 != gEnc1Idle);
		int p2 = (sw2 != gEnc2Idle);

		if(p1) { if(sEnc1Int < ENC_BTN_INTEG) sEnc1Int++; }
		else   { if(sEnc1Int > 0) sEnc1Int--; }
		if(gEnc1BtnHold > 0) gEnc1BtnHold--;
		if(sEnc1Int >= ENC_BTN_INTEG && !gEnc1BtnStable) {
			gEnc1BtnStable = 1;
			if(gEnc1BtnHold == 0) {
				gEnc1Click = 1;
				gEnc1BtnHold = ENC_BTN_REFRACT;
			}
		} else if(sEnc1Int == 0 && gEnc1BtnStable) {
			gEnc1BtnStable = 0;
		}

		if(p2) { if(sEnc2Int < ENC_BTN_INTEG) sEnc2Int++; }
		else   { if(sEnc2Int > 0) sEnc2Int--; }
		if(gEnc2BtnHold > 0) gEnc2BtnHold--;
		if(sEnc2Int >= ENC_BTN_INTEG && !gEnc2BtnStable) {
			gEnc2BtnStable = 1;
			if(gEnc2BtnHold == 0) {
				gEnc2Click = 1;
				gEnc2BtnHold = ENC_BTN_REFRACT;
			}
		} else if(sEnc2Int == 0 && gEnc2BtnStable) {
			gEnc2BtnStable = 0;
		}
	}

	/*
	 * Once per audio block: compute deltas and dispatch to the armed pad.
	 * DETENT DIVIDER: a mechanical KY-040-style encoder emits ENC_DETENT
	 * quadrature transitions per physical click, and Encoder::ANY counts
	 * every one. Undivided, one click moved pitch by TWO semitones (the
	 * "it skips notes / goes in whole tones" bug). Accumulate the raw
	 * counts and emit exactly one logical step per detent, so every
	 * semitone is reachable.
	 */
	static int sEnc1Prev = 0, sEnc2Prev = 0;
	static int sMainAcc = 0, sAuxAcc = 0;
	sMainAcc += gEnc1LastPos - sEnc1Prev; sEnc1Prev = gEnc1LastPos;
	sAuxAcc  += gEnc2LastPos - sEnc2Prev; sEnc2Prev = gEnc2LastPos;
	int mainDelta = sMainAcc / ENC_DETENT; sMainAcc -= mainDelta * ENC_DETENT;
	int auxDelta  = sAuxAcc / ENC_DETENT;  sAuxAcc  -= auxDelta * ENC_DETENT;
	int mainClick = gEnc1Click; gEnc1Click = 0;
	int auxClick  = gEnc2Click; gEnc2Click = 0;

	/*
	 * v5's both-encoder-click mode toggle is GONE along with the modes.
	 * Both clicks now belong to the armed effect: mainClick is its
	 * discrete toggle (cents/semitone, GRAIN/LPC, mono/poly...) and
	 * auxClick its secondary ratchet. Freeing the combo also removes the
	 * single point of failure that hid a working pair of encoders behind
	 * one unplugged switch.
	 *
	 * gArmedPad is an EFFECT id now, resolved by trellis_ui.cpp through
	 * gCatSlot[][], so the encoders follow whatever you last pressed on
	 * whichever page you were on - no re-arming when you change category.
	 */
	/* Name which physical encoder moved, so an unplugged one is findable
	 * from the log without opening the box. Prints only on real movement. */
	hgEncDiag(mainDelta, auxDelta, mainClick, auxClick);

	/* Light the grid so the fitted encoder identifies itself: ENC1 green,
	 * ENC2 yellow. ~20 trellis frames at 30 Hz is about two thirds of a
	 * second - long enough to see, short enough not to be in the way. */
	(void)auxDelta; (void)auxClick;   /* ENC2 not fitted */

	/* ONE ENCODER FITTED (ENC1, identified by the green flash 2026-09-22).
	 * ENC2 is not present, so its delta and click are always zero and every
	 * effect's AUX axis would be unreachable. ENC1's CLICK therefore toggles
	 * which axis it drives: rotation goes to MAIN or to AUX, and the grid
	 * flashes to say which mode you just entered. That keeps both parameter
	 * axes of every effect reachable from a single knob. */
	static int sAuxMode = 0;
	/* SYNTHESIS armed: the click is MONO <-> POLY instead of the axis swap,
	 * and rotation always walks the synth modes. Green = poly, yellow = mono. */
	if(mainClick && gArmedPad == PAD_SINE) {
		gSynthPoly = !gSynthPoly;
		gEncFlashWho = gSynthPoly ? 1 : 2;
		gEncFlashCount = 12;
		gStateDirty = 1;
		rt_printf("SYNTH: %s\n", gSynthPoly ? "POLY (per-string)" : "MONO (loudest string)");
		mainClick = 0;
		sAuxMode = 0;
	}
	if(mainClick) {
		sAuxMode = !sAuxMode;
		gEncFlashWho = sAuxMode ? 2 : 1;   /* yellow = AUX, green = MAIN */
		gEncFlashCount = 12;
		rt_printf("encoder: %s axis\n", sAuxMode ? "AUX" : "MAIN");
	}
	int mD = sAuxMode ? 0 : mainDelta;
	int aD = sAuxMode ? mainDelta : 0;
	if(gArmedPad != FX_NONE && (mD || aD))
		applyEncoders(gArmedPad, mD, aD, 0, 0);

	/* TRS pedal + joystick whammy. AFTER the encoders on purpose: the stick is
	 * an offset on top of whatever the encoders just set, not a competitor. */
	hgCtrlTick(context);
	potTick(gHgPot);

	// MONO LEAD: pick the loudest string (hysteresis), snap/deadband pitch
	if(gSineOn) {
		int sel = gLeadStr;
		float be = gV[sel].env;
		for(int s = 0; s < NUM_STRINGS; s++) {
			if(gV[s].tonalHops < SYNTH_TONAL_HOPS)
				continue;   // ignore strings that are just being tapped/knocked
			if(gV[s].env > 1.25f * be + 0.002f) { sel = s; be = gV[s].env; }
		}
		gLeadStr = sel;
		Voice *lv = &gV[sel];
		// only take a new pitch target from a string with a STABLE note -
		// never from a transient or a tap
		if(lv->tonalHops >= SYNTH_TONAL_HOPS)
			gLeadIncTgt = lv->beta / lv->period;
	}

	/*
	 * SITAR TARAB RETUNE: was rooted at a FIXED ~1800Hz no matter what's
	 * played. Now tracks the loudest STABLE string once per block and
	 * rebuilds the 14-comb Bhairava scale rooted an octave+fifth above
	 * IT, so the shimmer is always harmonically tied to what you're
	 * playing (real tarab strings are tuned relative to the raga/note).
	 */
	if(gSitarAmtS > 0.002f) {
		int sel = gTarabLeadStr;
		float be = gV[sel].env;
		for(int s = 0; s < NUM_STRINGS; s++) {
			if(gV[s].tonalHops < SYNTH_TONAL_HOPS)
				continue;
			if(gV[s].env > 1.2f * be + 0.002f) { sel = s; be = gV[s].env; }
		}
		gTarabLeadStr = sel;
		Voice *tv = &gV[sel];
		if(tv->tonalHops >= SYNTH_TONAL_HOPS && tv->period > 1.0f) {
			float domFreq = sr / tv->period;
			float root = domFreq * 3.0f;
			for(int k = 0; k < TARAB_N; k++) {
				float f = root * powf(2.0f, (float)BHAIRAVA[k] / 12.0f);
				gTarabDelay[k] = sr / f;
			}
		}
	}

	/*
	 * SYMPATHETIC COUPLING WEIGHTS (per block) + CROSSTALK REFERENCE.
	 * See git log for the full rationale (unchanged, moved verbatim).
	 */
	for(int s = 0; s < NUM_STRINGS; s++) {
		float m = 0.0f;
		for(int o = 0; o < NUM_STRINGS; o++)
			if(o != s && gV[o].env > m) m = gV[o].env;
		gOtherMax[s] = m;
	}

	if(gSympAmtS > 0.002f) {
		for(int s = 0; s < NUM_STRINGS; s++) {
			for(int o = 0; o < NUM_STRINGS; o++) {
				if(o == s) continue;
				float target = 0.15f;
				Voice *vo = &gV[o];
				if(vo->tonalHops >= 3 && vo->period > 1.0f) {
					float q = gV[s].p0 / vo->period;  // = fo / f0s
					float bestRel = 0.0f;
					for(int m = 1; m <= 4; m++) {
						float xm = (float)m * q;
						float k = roundf(xm);
						if(k < 1.0f) continue;
						float d = fabsf(xm - k) / k;      // relative detune
						float rel = 1.0f - d / 0.02f;      // zero credit at 2% off
						if(rel < 0.0f) rel = 0.0f;
						rel /= 0.6f + 0.4f * (float)m;     // high-order = weaker
						if(rel > bestRel) bestRel = rel;
					}
					target = 0.15f + 0.85f * bestRel;
				}
				gSympW[s][o] += 0.05f * (target - gSympW[s][o]);
			}
		}
	}

	for(unsigned int n = 0; n < context->audioFrames; n++) {
		float outL = 0.0f;
		float outR = 0.0f;

		gDryS += 0.005f * (gDryTarget - gDryS);
		gWetS += 0.005f * (gWetTarget - gWetS);
		gVibAmtS += 0.001f * ((gVibOn ? 1.0f : 0.0f) - gVibAmtS);
		gChorAmtS += 0.001f * ((gChorusOn ? 1.0f : 0.0f) - gChorAmtS);
		gEnvFAmtS += 0.001f * ((gEnvFOn ? 1.0f : 0.0f) - gEnvFAmtS);
		gFmAmtS += 0.001f * ((gFmActive ? 1.0f : 0.0f) - gFmAmtS);
		gSineAmtS += 0.001f * ((gSineOn ? 1.0f : 0.0f) - gSineAmtS);
		gPogAmtS += 0.001f * ((gPogOn ? 1.0f : 0.0f) - gPogAmtS);
		gResAmtS += 0.001f * ((gResOn ? 1.0f : 0.0f) - gResAmtS);
		gSusAmtS += 0.001f * ((gSusOn ? 1.0f : 0.0f) - gSusAmtS);
		gJawAmtS += 0.001f * ((gJawOn ? 1.0f : 0.0f) - gJawAmtS);
		gSitarAmtS += 0.001f * ((gSitarOn ? 1.0f : 0.0f) - gSitarAmtS);
		gPanAmtS += 0.001f * ((gPanOn ? 1.0f : 0.0f) - gPanAmtS);
		gSagAmtS += 0.001f * ((gSagOn ? 1.0f : 0.0f) - gSagAmtS);
		gSympAmtS += 0.001f * ((gSympOn ? 1.0f : 0.0f) - gSympAmtS);

		// all six DRY inputs for this sample - the sympathetic strings are
		// excited by the STRINGS themselves, never by processed output
		// (feeding them the wet mix created a cross-string feedback loop
		// that ran away and tripped the safety mute = total silence)
		float dryIn[NUM_STRINGS];
		for(int s = 0; s < NUM_STRINGS; s++)
			dryIn[s] = audioRead(context, n, STRING_CHANNEL[s]) * INPUT_GAIN;

		/* each string's finished voice, tapped for its direct output */
		float strOut[NUM_STRINGS] = { 0, 0, 0, 0, 0, 0 };

		for(int s = 0; s < NUM_STRINGS; s++) {
			Voice *v = &gV[s];
			float dry = dryIn[s];

			if(--v->hopCountdown <= 0) {
				trackPitch(v, s);
				updateModulation(v, s);
				updateLP(v);
				v->hopCountdown = HOP;
			}

			float wet, dryLeg;
			voiceProcess(v, dry, gDryS > 0.01f, &wet, &dryLeg);
			float mixed = dryLeg * gDryS + wet * gWetS;

			runFM(v, &mixed);
			runAutowah(v, &mixed);
			runResonator(v, &mixed);
			runJawari(v, &mixed);
			runSustainer(v, &mixed);
			runSitarZing(v, &mixed);
			runSympathetic(v, s, dryIn, &mixed);
			runPluckSag(v, &mixed, sr);
			runVibrato(v, s, &mixed, vibDepthSmp, sr);
			runPogSwell(v, &mixed, pogStep);

			float wide = 0.0f;
			runChorus(v, s, &mixed, chorBaseSmp, chorDepthSmp, &wide, sr);

			runSynthPoly(v, s, &mixed);

			/*
			 * SAFETY SCRUB, now in two tiers. Recursive filters (and the
			 * sympathetic cross-string feedback path) can blow up or take
			 * on a NaN. A NaN in a biquad state is PERMANENT - it poisons
			 * every future sample, which is exactly the "fucked up noise
			 * until I restart" failure.
			 *
			 * The old single test was `!(|mixed| < 16)` -> force 0 and
			 * wipe every state. That is TOO LOOSE and its recovery is
			 * itself a click: the sustainer comb runs fb up to 0.9999 and
			 * the resonator/sitar/symp voices stack on top of it, so a
			 * hot sustained hold reaches double digits while remaining
			 * perfectly finite and healthy - and each time it did, one
			 * sample was slammed to 0.0 (a click) and four combs plus the
			 * tracker biquads were wiped (a dropout) on LOUD PLAYING.
			 *
			 * Tier 1, finite but huge: fold through the C1 saturator
			 * above. Value and slope are continuous at the knee, the
			 * result is bounded by SCRUB_MAX, and because gLastMixed[] is
			 * what feeds the cross-string path, bounding it here is what
			 * actually stops a runaway - no state wipe needed, and no
			 * step in the audio.
			 *
			 * Tier 2, NaN/Inf ONLY: `!(|x| < 1e6)` is true for NaN, for
			 * +/-Inf, and for nothing a working signal chain produces.
			 * That state is unrecoverable, so reset the recursive states
			 * in place - but ramp the string back in over SCRUB_FADE_STEP
			 * instead of stepping from silence to full. Pure arithmetic:
			 * no added latency.
			 */
			if(!(fabsf(mixed) < 1e6f)) {
				mixed = 0.0f;
				memset(&v->resC, 0, sizeof(Comb));
				memset(&v->susC, 0, sizeof(Comb));
				memset(&v->jawC, 0, sizeof(Comb));
				memset(&v->sympC, 0, sizeof(Comb));
				memset(v->lpZ, 0, sizeof(v->lpZ));
				v->svfLow = v->svfBand = 0.0f;
				v->pogLpZ = 0.0f;
				v->dcX1 = v->dcY1 = 0.0f;
				v->sagEnv = 0.0f;
				sScrubFade[s] = 0.0f;
			} else {
				float a = fabsf(mixed);
				if(a > SCRUB_SOFT) {
					// u/(1+u) knee: f(SOFT)=SOFT, f'(SOFT)=1, f(inf)=MAX
					float u = (a - SCRUB_SOFT) * (1.0f / (SCRUB_MAX - SCRUB_SOFT));
					float sat = SCRUB_SOFT + (SCRUB_MAX - SCRUB_SOFT) * u / (1.0f + u);
					mixed = mixed < 0.0f ? -sat : sat;
				}
			}
			if(sScrubFade[s] < 1.0f) {
				sScrubFade[s] += SCRUB_FADE_STEP;
				if(sScrubFade[s] > 1.0f) sScrubFade[s] = 1.0f;
				mixed *= sScrubFade[s];
			}

			// feeds next sample's Sympathetic Resonance injection on the
			// OTHER five strings - captured after this string's full chain
			gLastMixed[s] = mixed;
			/* STRING VOLUME (hex sliders) - applied AFTER the whole chain,
			 * not at the input: the tracker, gates and synth thresholds are
			 * absolute levels, and a quiet slider must not starve them. */
			mixed *= gVolSub[s];
			wide  *= gVolSub[s];
			strOut[s] = mixed;

#if MONO_OUT
			outL += mixed + gChorAmtS * CHOR_GAIN * wide;
			outR += mixed + gChorAmtS * CHOR_GAIN * wide;
#else
			// see PAN SMOOTHING above: the hop-rate pan gains are slewed
			// here, at the point of use, so a pan change is a move and
			// not an edge
			sPanL[s] += PAN_SM_C * (v->gainL - sPanL[s]);
			sPanR[s] += PAN_SM_C * (v->gainR - sPanR[s]);
			// chorus voice takes the MIRRORED pan: per-string width
			outL += mixed * sPanL[s] + gChorAmtS * CHOR_GAIN * wide * sPanR[s];
			outR += mixed * sPanR[s] + gChorAmtS * CHOR_GAIN * wide * sPanL[s];
#endif
		}

		// 15 SITAR - part 2: the tarab bank itself.
		runSitarTarab(dryIn, &outL, &outR);

		// SYNTH MONO
		runSynthMono(&outL, &outR, sr);

		outL *= MASTER_GAIN;
		outR *= MASTER_GAIN;

		/*
		 * MASTER LIMITER (smoothed gain - see the block comment at the
		 * top of this file for what was wrong with the old one). The
		 * detector still attacks instantly, because it only decides the
		 * TARGET; the gain that touches the audio is one-poled, so the
		 * waveform stays continuous through every transient.
		 *
		 * NO LOOKAHEAD, by design - this instrument is fighting for
		 * milliseconds and a delay line here would cost more than the
		 * problem. The 1 ms attack therefore lets a small, brief
		 * overshoot past, which is exactly what softClip() behind us is
		 * for: it is monotone and C1 (value 2/3 and slope 0 on both
		 * sides of |x| = 1), so it absorbs the overshoot as a smooth
		 * knee, not an edge. Measured worst case at the converter is
		 * +0.13 dB over the nominal softClip(LIM_CEIL) level even for an
		 * instantaneous silence -> full six-string slam.
		 */
		/* ---- aux sources in, before the limiter ---- */
		if(AUX_PIEZO_SOLO || AUX_TRS_SOLO) {
			outL = 0.0f;      /* silence the instrument itself */
			outR = 0.0f;
		}
		/* PITCH DRY/WET (2026-09-24). While pitch is on, the pot balances
		 * the guitar's own TRS pickup (dry) against the shifted hex (wet),
		 * equal-power. Pitch off = both at full, exactly as before. Gains
		 * glide (~20 ms) so the pot and pitch on/off never click. */
		{
			static float sHexG = 1.0f, sTrsG = 1.0f;
			static float tHex = 1.0f, tTrs = 1.0f;
			if(n == 0) {
				if(gSineOn) {
					/* SYNTHESIS is 100% wet: the synth already replaces the
					 * hex strings, so the dry guitar (TRS) goes too. The
					 * contact mics are faded below the same way. */
					tHex = 1.0f; tTrs = 0.0f;
				} else if(gPitchActive) {
					float b = gPitchBal < 0.0f ? 0.0f : (gPitchBal > 1.0f ? 1.0f : gPitchBal);
					tHex = sinf(b * 1.5707963f);
					tTrs = cosf(b * 1.5707963f);
				} else {
					tHex = 1.0f; tTrs = 1.0f;
				}
			}
			sHexG += (tHex - sHexG) * 0.0012f;
			sTrsG += (tTrs - sTrsG) * 0.0012f;
			outL *= sHexG;
			outR *= sHexG;
			if(!AUX_PIEZO_SOLO && gVolTrs > 0.0f) {
				/* MONO-SAFE TRS (2026-09-30). The guitar jack is wired TRS
				 * (tip -> in 0, ring -> in 1) but the guitar is mono: a TS
				 * plug GROUNDS the ring, so in 1 is silence. Feeding tip to
				 * the left output and ring to the right put the whole guitar
				 * on output 1 and nothing on output 2 - the "one output is
				 * quieter" gig bug. Sum both contacts to BOTH sides: with a
				 * TS plug that is tip on both (balanced L/R), and a stereo
				 * plug later still arrives intact (mono-summed). */
				float trs = (audioRead(context, n, AUX_TRS_A)
				           + audioRead(context, n, AUX_TRS_B)) * gVolTrs * sTrsG;
				outL += trs;
				outR += trs;
			}
		}
		/* HARD-PANNED contact mics: A -> left, B -> right. Kept in scope
		 * past the limiter so the aux-output test mode can reuse the exact
		 * same filtered, gained signal rather than re-deriving it. */
		float pzL = 0.0f, pzR = 0.0f;
		if(gVolPiezo > 0.0f) {
			for(int i = 0; i < 2; i++) {
				float x = audioRead(context, n, i ? AUX_PZ_B : AUX_PZ_A);
				float h1 = gAuxHpA * (gPzHp1Y[i] + x - gPzHp1X[i]);
				gPzHp1X[i] = x; gPzHp1Y[i] = h1;
				float h2 = gAuxHpA * (gPzHp2Y[i] + h1 - gPzHp2X[i]);
				gPzHp2X[i] = h1; gPzHp2Y[i] = h2;
				float v = h2 * gVolPiezo;
				if(i) pzR = v; else pzL = v;
			}
			/* contact mics are dry too: out while SYNTHESIS is on */
			static float sPzG = 1.0f;
			sPzG += ((gSineOn ? 0.0f : 1.0f) - sPzG) * 0.0012f;
			outL += pzL * sPzG;
			outR += pzR * sPzG;
		}

		float pk = fabsf(outL) > fabsf(outR) ? fabsf(outL) : fabsf(outR);
		gLimEnv = pk > gLimEnv ? pk : gLimEnv * LIM_ENV_REL;
		float gTgt = gLimEnv > LIM_CEIL ? LIM_CEIL / gLimEnv : 1.0f;
		float limC;
		if(gTgt < sLimGain) {
			limC = LIM_ATK_C;
		} else {
			float gr = (1.0f - sLimGain) * (1.0f / LIM_REL_KNEE);
			if(gr > 1.0f) gr = 1.0f;
			limC = LIM_REL_FAST + (LIM_REL_SLOW - LIM_REL_FAST) * gr;
		}
		sLimGain += limC * (gTgt - sLimGain);
		outL *= sLimGain;
		outR *= sLimGain;

		/* Independent level for the first pair and everything above it, so a
		 * monitor feed and a front-of-house feed can differ. */
		for(int mc = 0; mc < 10 && mc < (int)context->audioInChannels; mc++) {
			float mv = audioRead(context, n, mc);
			gMonInRms[mc] += (mv * mv - gMonInRms[mc]) * 0.0005f;
			/* CLIP AUDIT: raw converter peak per input, and a running count
			 * of samples at the converter's ceiling - no software gain can
			 * undo a clip that happened here. */
			float am = fabsf(mv);
			if(am > gMonInPk[mc]) gMonInPk[mc] = am;
			if(am >= 0.98f) gMonInClip[mc]++;
		}

		/* ...and the output: before the soft clipper (anything past 1.0 is
		 * being squashed) and after it (what actually leaves). */
		{
			float pa = fabsf(outL * gVolOutA), pb = fabsf(outR * gVolOutA);
			if(pa > gMonPrePk[0]) gMonPrePk[0] = pa;
			if(pb > gMonPrePk[1]) gMonPrePk[1] = pb;
			if(pa > 1.0f || pb > 1.0f) gMonPreClip++;
		}
		float wL = softClip(outL * gVolOutA), wR = softClip(outR * gVolOutA);
		if(fabsf(wL) > gMonOutPk[0]) gMonOutPk[0] = fabsf(wL);
		if(fabsf(wR) > gMonOutPk[1]) gMonOutPk[1] = fabsf(wR);
		audioWrite(context, n, 0, wL);
		audioWrite(context, n, 1, wR);

		/*
		 * AUX OUTPUTS - one string each, lowest to highest. strOut[] is taken
		 * before the stereo pan and before MASTER_GAIN, so both are applied
		 * here; sLimGain rides along too, so a direct out ducks with the mix
		 * instead of clipping on its own while the stereo pair stays clean.
		 */
		const float auxG = gVolOutB * MASTER_GAIN * sLimGain;
		for(int a = 0; a < gNumAuxOut; a++) {
			float v = (a < NUM_STRINGS) ? strOut[a] * auxG : 0.0f;
			if(PIEZO_ON_ALL_OUTS)
				v += ((a & 1) ? pzR : pzL) * gVolOutB * sLimGain;
			audioWrite(context, n, 2 + a, softClip(v));
		}
		gMonOutRms[0] += (wL * wL - gMonOutRms[0]) * 0.0005f;
		gMonOutRms[1] += (wR * wR - gMonOutRms[1]) * 0.0005f;
		/* USB audio switch: ~10 ms fade either way, then stop pushing
		 * entirely while off so the bridge has nothing to do. */
		{
			static float sUsbG = 1.0f;
			sUsbG += ((gUsbAudio ? 1.0f : 0.0f) - sUsbG) * 0.0025f;
			if(gUsbAudio || sUsbG > 0.0005f)
				bridgePush(wL * sUsbG, wR * sUsbG);
		}
		gSampleStamp++;
	}
	gTimeSec += (double)context->audioFrames / context->audioSampleRate;
}

void cleanup(BelaContext *context, void *userData)
{
	if(gI2cFd >= 0)
		close(gI2cFd);
	hgOscClose();
}
