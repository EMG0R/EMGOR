/*
 * hyperGuitar: 6ch Subsix pickup, Db standard -> per-string hybrid pitch
 * engine (SOLAD down / TD-PSOLA up) + series formant + per-string FX.
 * Bela Gem Multi + NeoTrellis 4x4. Research: docs/eventide-pitch-shift-research.md
 *
 * PAD MAP v6 - consolidated: each "effect family" is now ONE pad, in a
 * logical layout (1-8 unchanged; 9-16 grouped by family: dynamic
 * filters, per-note lifecycle, texture, resonance). Two GPIO rotary
 * encoders (main=enc1, aux=enc2) are wired: whichever pad was last
 * pressed is "armed" and the encoders live-adjust that effect's two
 * main parameters (see applyEncoders()). Pitch/FM/Formant/Synth remain
 * preset-cycling via repeated press only for now (deferred - same
 * underlying selection state an encoder will eventually scroll).
 *
 *  0 PITCH   cycle: OFF -> BASS(-1oct) -> OCT-UP(+1oct) -> 4TH-UP ->
 *            5TH-UP -> 4TH-DN -> OFF  (50/50 dry/wet on the interval
 *            presets, full wet on octaves)
 *  1 FM      cycle: OFF -> SUB(0.5) -> RATIO-1 -> RATIO-2 -> OFF
 *            (phase-modulates the actual string signal, index 2)
 *  2 FORMANT cycle: OFF -> LOW(0.60) -> HIGH(1.50) -> OFF
 *  3 SYNTH   monophonic tracked-frequency lead
 *  4 VIBRATO NEPTR unipolar-sine delay sweep. main=rate aux=depth
 *  5 CHORUS  subtle per-string stereo-width voice. main=rate aux=depth
 *  6 AUTOWAH classic envelope filter. main=sensitivity aux=Q
 *  7 POG SWELL  per-note attack bloom, 0.02-3s. main=attack time
 *            aux=filter-sweep depth/direction tied to the swell
 *  -- dynamic-filter family (closest to Autowah) --
 *  8 RESONATOR  envelope drives the harmonic ratio the resonance rides
 *            at: quiet picking rests near the octave, digging in
 *            sweeps it up toward ~2.5 octaves higher. main=sensitivity
 *            aux=Q
 *  9 FORMANT-WAH  envelope sweeps the formant ratio
 *  -- per-note lifecycle pair (start of note / end of note) --
 * 10 PLUCK SAG  real string physics: a hard pluck briefly sags pitch
 *            sharp/flat before settling to true pitch. main=depth
 *            aux=settle time
 * 11 SUSTAINER  harder picking extends that note's natural decay via a
 *            decaying period-locked echo, capped at 3s. Bounded/
 *            self-terminating, not open feedback.
 *  -- texture family --
 * 12 JAWARI BUZZ  sitar-style bridge buzz, dynamics-driven asymmetric
 *            soft clip (shares Autowah's sensitivity convention)
 * 13 SHAKUHACHI/NEY  filtered breath noise near the tracked fundamental,
 *            dynamics-scaled, deliberately subtle
 *  -- resonance family, escalating fixed -> cross-string --
 * 14 HELMHOLTZ BODY RESONANCE  fixed ~90Hz acoustic-cavity-style peak
 * 15 SYMPATHETIC RESONANCE  sitar/hardanger physics: each string gets
 *            its own resonator locked to ITS tracked f0, fed a little
 *            of the other five strings' output - harmonically-related
 *            strings ring faintly when another is played. main=coupling
 *            amount aux=tightness (how selective the ringing is)
 *
 * ARCHIVED: RANDOM + DYNAMIC pitch, TREM sweep, SPREAD, DRIFT/WOBBLE/
 * TOUCH formants, RINGMOD (superseded by FM), old 3-pad FM radio,
 * old 2-pad Formant radio, old 5-pad Pitch radio.
 *
 * GAIN: Subsix-native input (1.0), master 1.0, gain-riding limiter at
 * LIM_CEIL guarantees no output distortion. softClip = dormant safety.
 *
 * VERIFIED channel map (solo-tested 2026-07-09):
 *   ch2=D ch3=A ch4=G ch5=lowE ch8=B ch9=e  (0-1 Gem in, 6-7 unused)
 * Pans: E/A +-10%, D/G +-30%, B/e +-50%.
 */

#include <Bela.h>
#include <libraries/Encoder/Encoder.h>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdint.h>

#define NUM_STRINGS 6
#define INPUT_GAIN 1.0f
#define MASTER_GAIN 1.0f
#define LIM_CEIL 0.90f
#define HIGH_MIX 0.25f
#define MONO_OUT 0

#define BUF_SIZE 8192
#define BUF_MASK (BUF_SIZE - 1)
#define FXBUF_SIZE 8192
#define FXBUF_MASK (FXBUF_SIZE - 1)
#define DECIM 4
#define DBUF_SIZE 1024
#define DBUF_MASK (DBUF_SIZE - 1)
#define HOP 128
#define MAX_FRET_RATIO 4.5f  // full 24-fret range + bend margin. Was 2.6
                             // (~16th fret) - above that the true period
                             // fell below the search range, so the tracker
                             // could only see the 2P peak = OCTAVE DOWN.
#define TUNE_MARGIN 1.12f
#define NSDF_CONF 0.45f
#define FIRST_PEAK 0.82f
#define PITCH_CONF_MIN 0.92f   // reject unconfident (transient) pitch reads.
                               // Was 0.80 - a muted/palm-scraped string
                               // still has SOME correlation (damped, buzzy,
                               // but not white noise), enough to clear
                               // 0.80. 0.92 demands a genuinely clean,
                               // strongly periodic waveform - deliberately
                               // biased to reject a real quiet note over
                               // letting a muted scrape through.
#define ATTACK_BLANK_HOPS 6    // ~17ms of tracker blanking after a pick.
                               // Was 10 (~30ms) - shortened because a pick
                               // now also resets tonalHops, so the synth
                               // needs blank + 3 stable hops before it
                               // speaks; 6 keeps total onset ~26ms while
                               // the confidence gate still rejects the
                               // attack garbage itself.
#define GATE_RMS 3e-4f
#define NGRAINS 14  // was 8 - the raised overlap floor above needs more
                    // concurrent grain slots, and CPU headroom on this
                    // board is nowhere near a constraint (confirmed
                    // ~0.2 load average on a 4-core board)
#define HANN_N 1024
#define VBUF_SIZE 2048  // widened for the chorus base-delay bump below -
                        // 1024 didn't have headroom for base+2*depth at
                        // max chorus settings without wrapping
#define VBUF_MASK (VBUF_SIZE - 1)

/*
 * USB AUDIO BRIDGE: the final stereo mix is ALSO pushed into a lock-free
 * SPSC ring buffer in shared memory, read by a separate non-real-time
 * companion process (usbaudio-bridge) that feeds the UAC2 USB gadget.
 * NEVER block or spin here - this runs inside the Xenomai/EVL real-time
 * audio thread. If the reader falls behind, we just drop samples; the
 * ring only exists to decouple the RT thread from any USB/ALSA timing,
 * never to apply backpressure to it.
 */
#define BRIDGE_RING_FRAMES 512    // ~11.6ms ceiling - was 2048/(~46ms)/8192 (~186ms):
                                  // a passive drop-when-full ring has no
                                  // pressure to stay near-empty, so any
                                  // tiny persistent rate mismatch between
                                  // this RT clock and the USB host clock
                                  // let the backlog creep up and PIN near
                                  // the ring's max size - i.e. the ring's
                                  // capacity effectively became the real
                                  // steady-state latency. Capping it here
                                  // bounds that worst case; the bridge
                                  // process also now actively resyncs
                                  // toward a much smaller target instead
                                  // of just draining passively.
#define BRIDGE_RING_MASK (BRIDGE_RING_FRAMES - 1)
#define BRIDGE_SHM_NAME "/hyperguitar_usbaudio"
typedef struct {
	volatile uint32_t head;   // producer (this RT process) writes
	volatile uint32_t tail;   // consumer (bridge process) writes
	int16_t data[BRIDGE_RING_FRAMES * 2]; // interleaved L/R, S16
} BridgeRing;
static BridgeRing *gBridge = NULL;

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

static const float OPEN_HZ[NUM_STRINGS] =
	{ 69.30f, 92.50f, 123.47f, 164.81f, 207.65f, 277.18f };
static const int STRING_CHANNEL[NUM_STRINGS] = { 5, 3, 2, 4, 8, 9 };
static const float STRING_PAN[NUM_STRINGS] =
	{ -0.10f, +0.10f, -0.30f, +0.30f, -0.50f, +0.50f };
static const float HARM_CUT[NUM_STRINGS] = { 6.0f, 6.0f, 8.0f, 8.0f, 10.0f, 10.0f };
static const float LFO_PH[NUM_STRINGS] = { 0.0f, 1.1f, 2.2f, 3.3f, 4.4f, 5.5f };

// NEPTR vibrato (verified vs phase-4 vibrato.orc): unipolar sine sweep
static float gVibDepthSec = 0.00106f;  // -20% from 1.33ms; encoder-adjustable
static float gVibRateScale = 1.0f;     // encoder-adjustable multiplier on VIB_HZ[]
static const float VIB_HZ[NUM_STRINGS] = { 0.72f, 0.75f, 0.78f, 0.81f, 0.84f, 0.87f };

// CHORUS: subtle second voice, mirrored across the stereo image per string
#define CHOR_BASE_SEC  0.018f  // was 12ms - sat in flanger territory;
                               // 15-35ms is the classic analog-chorus
                               // range (Boss CE-2 etc)
static float gChorDepthSec = 0.002f;   // encoder-adjustable
static float gChorRateScale = 1.0f;    // encoder-adjustable multiplier on CHOR_HZ[]
#define CHOR_GAIN 0.35f
static const float CHOR_HZ[NUM_STRINGS] = { 0.31f, 0.41f, 0.53f, 0.67f, 0.83f, 1.01f };

// AUTOWAH (classic env filter): restored pre-quack voicing,
// threshold raised ~15% (sens x0.85), Q a sliver over original
#define ENVF_FMIN 250.0f
#define ENVF_OCTS 3.0f
static float gEnvfQ    = 0.7071f;  // no resonance - straightforward sweep; encoder-adjustable
static float gEnvfSens = 2.125f;   // encoder-adjustable
static float gFWahSens = 2.0f;  // formant wah sensitivity (encoder)

// FM on the actual string signal: delay-line phase modulation.
// Modulator = sine at gFmRatioVal x tracked f0. Index at the modulator:
// delay amplitude A = INDEX * period/(2*pi*ratio) samples, so ratio
// 0.5 throws sidebands at +-f0/2 = adds the octave-down voice.
#define FM_MAX_A 400.0f

// SYNTH LEAD: MONOPHONIC epic lead. One global voice follows the
// loudest string (hysteresis switching). Two detuned polyBLEP saws,
// bright and alias-free. Pitch rule: big changes SNAP instantly to the
// new fundamental (no onset bend-up), sub-0.5% drift is deadbanded
// (rock-solid sustain), only in-between wobble glides.
#define SINE_LEVEL 0.48f
#define SINE_ENV_SENS 2.0f
/*
 * SYNTH GATE: the amplitude IS the tracked string envelope, full stop -
 * no captured/held sustain level, no synthetic attack ramp, no fixed
 * release timer. This used to hold a constant velocity-scaled level and
 * fade out over a fixed ~260ms so the synth "hung on" longer than the
 * guitar naturally would (H90 HotSawz-style). That's exactly the
 * "synth-pad vibe" being deliberately undone here: the note should
 * start and decay AT THE SAME RATE the physical string does, nothing
 * added. v->env already has an effectively-instant attack (it's a
 * peak-hold) and a natural exponential decay, so feeding it straight
 * through (with only enough one-pole smoothing to kill the sample-domain
 * step at gate open/close) reproduces the real string's ADSR exactly.
 */
static float gSynthGateThr = 0.010f;  // note-off level (raised)
#define SYNTH_TONAL_HOPS 14  // ~40ms of STABLE pitch before the synth speaks.
                             // Was 3 (~9ms) - a muted/palm-scraped or
                             // rubbed string can hold SOME periodicity for
                             // a few ms before collapsing into noise; a
                             // real note stays clean far longer than that.
                             // 40ms is a deliberately high bar: normal
                             // fretted/picked notes clear it easily, short
                             // scrapes and taps die out before they do.
#define SYNTH_ENV_SLEW 0.35f // per-sample one-pole toward the tracked env -
                             // just enough to de-click the gate edge, far
                             // too fast to read as an "attack"/"release"
#define LEAD_DETUNE 1.007f

/*
 * SYNTH: main encoder scrolls through timbres, main CLICK toggles
 * MONO (one voice follows the loudest string's fundamental) <-> POLY
 * (every string drives its own independent oscillator). aux = level.
 */
#define SYNTH_NMODES 10
static volatile int gSynthMode = 0;
static volatile int gSynthPoly = 0; // 0 = mono, 1 = poly
static float gSynthLevel = 1.0f;
static const char *SYNTH_NAMES[SYNTH_NMODES] = {
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
};

// POG SWELL: ~100ms per-note attack bloom (EHX Swello-style, per string)
// Full 0-3s range (matching the real Swello's max) is now an encoder param.
static float gPogAttackSec = 0.18f; // quick jazz swell (Metheny-ish)
static float gPogModDepth = 0.0f;  // aux param: filter-sweep depth, +/- octaves

/*
 * RESONATOR: dynamic/dependent, scaled like Autowah - envelope drives
 * the harmonic ratio the resonance rides at instead of driving a
 * cutoff frequency. Quiet picking rests near the octave; digging in
 * sweeps the peak up toward ~2.5 octaves higher. gResSens mirrors
 * gEnvfSens's role; both are now live-adjustable via encoders.
 */
/* 11 RESONATOR: comb tuned to a HARMONIC of the note - it RINGS that
 * overtone out loud. Dynamics pick which harmonic. */
static float gResSens = 2.0f;    // MAIN
static float gResFb   = 0.965f;  // AUX: how long the overtone rings
static float gResGain = 2.2f;

/*
 * SUSTAINER (re-engineered): a real Karplus-Strong style resonant
 * feedback loop tuned to the string's tracked period, not a one-shot
 * echo. The string injects energy into its own delay loop; the loop
 * gain is solved so the ring decays to -60 dB in exactly the window
 * the pick strength asks for (up to gSusMaxSec). This actually RINGS
 * and sustains - the old version was a single decaying tap, which is
 * why it did nothing.  main = sustain length, aux = damping/tone.
 */
static float gSusFb    = 0.995f; // MAIN: hold time (top = infinite)
static float gSusDamp  = 0.28f;  // AUX: tone of the held note
static float gSusLevel = 1.8f;

/* 13 JAWARI BUZZ: a sitar's flat bridge makes the string rattle. A SHORT
 * high-feedback comb rings high and metallic = the buzz. Excited harder
 * the harder you play, and ADDED on top so the dry level is untouched. */
static float gJawBuzz  = 0.6f;   // MAIN: buzz amount
static float gJawPitch = 90.0f;  // AUX: metallic pitch of the rattle
static float gJawSens  = 2.0f;

/* 15 SITAR TONE. How the real pedals do it (Yamaha patent US5468906A;
 * EHX Ravish): the sympathetic strings are a BANK of ~14 tuned
 * Karplus-Strong string resonators QUANTIZED TO A SCALE - the Ravish's
 * "EXOTIC" setting is the Bhairava raga (major with flat 2nd and flat
 * 6th). The played signal is scaled down and injected as excitation into
 * all of them; whichever ones are harmonically near what you play ring
 * back. A whole scale of tarab strings ringing IS the sitar sound - one
 * resonator per string (what I had) was never going to get there.
 * Plus a bright "zing" on the direct voice for the jawari buzz. */
#define TARAB_N 14
/* Bhairava: 1 b2 3 4 5 b6 7 - over two octaves */
static const int BHAIRAVA[TARAB_N] = { 0,1,4,5,7,8,11, 12,13,16,17,19,20,23 };
static float gSitarRing  = 0.9990f; // AUX: how long the tarab strings ring
                                     // (tarab delay is only 8-24 samples -
                                     // way shorter than the other combs'
                                     // ~100-600 - so it needs fb much
                                     // closer to 1 to ring for the same
                                     // real-world time; 0.978 died in <10ms)
static float gSitarLevel = 0.75f;   // MAIN: wet fraction added on top of
                                     // the (always 100%) dry signal
static float gSitarBuzz  = 0.35f;   // jawari zing on the direct voice

/*
 * PLUCK SAG: real string physics - a hard pluck briefly sags the pitch
 * sharp/flat from nonlinear tension at large amplitude, settling to
 * true pitch as the note decays. One-shot per note, not periodic (vs
 * Vibrato). main(depth)/aux(settle time) are live via encoders.
 */
static float gSagDepthSec = 0.006f;
static float gSagSettleSec = 0.08f;

/*
 * SYMPATHETIC RESONANCE: sitar/hardanger-fiddle physics - each string
 * gets its own resonator locked to ITS tracked f0, fed a little of the
 * other five strings' output (one-sample-old, inaudible latency), so
 * harmonically-related strings ring faintly when another is played.
 * main(coupling amount)/aux(tightness - how selective the ringing is).
 */
/* 14 SYMPATHETIC (sitar tarab): each string's comb tuned to ITS OPEN
 * pitch - like real tarab strings, tuned to fixed scale notes - excited
 * by the OTHER five strings. Per-string by construction. */
static float gSympCoupling = 1.3f; // MAIN
static float gSympFb = 0.986f;     // AUX: ring length
static float gLastMixed[NUM_STRINGS];
static float gTarabDelay[TARAB_N];
static float gTarabPan[TARAB_N];

/*
 * STEREO SPREAD (pad 16): OFF = mono (all strings centred). ON = the
 * per-string pan pattern, scaled by MAIN. AUX adds DYNAMIC panning:
 * the harder a string is played, the further out it flies from centre.
 */
static float gPanSpread = 1.0f;
static float gPanDyn = 0.0f;

// ---- radio/toggle state ----
static volatile int gPitchActive = 0;   // simple on/off - value comes from encoders
static volatile int gFormantActive = 0; // simple on/off - value comes from encoders
static volatile int gFmActive = 0;      // simple on/off - value comes from encoders

// PITCH: main=semitones(-24..+24) or click->cents(+-50), aux=wet% 0-100 in 10s
static float gPitchSemitones = -12.0f;
static int gPitchCentsMode = 0;
static float gPitchCents = 0.0f;
static float gPitchWetPct = 100.0f;

// FM: main=ratio (click toggles quantized{0.25,0.5,1,2,3,4}<->free), aux=index
static float gFmRatioVal = 0.5f;
static int gFmQuantized = 1;
static float gFmIndexVal = 0.80f;

// FORMANT: main=ratio 0.4-2.2, aux=glide (how silky the shift eases in,
// no envelope/dynamics reactivity - that's FORMANT-WAH's job)
static float gFormantVal = 1.0f;
static float gFormantGlide = 0.05f;  // per-hop smoothing coeff, ~57ms glide
static float gFormantTiltBias = 0.0f; // extra formant-driven EQ tilt, preset-only
                                       // (0 = no effect - the existing auto-tilt
                                       // is driven by PITCH beta, which is
                                       // normally 1.0/neutral for a formant-only
                                       // patch, so presets that want extra vowel
                                       // character bias this separately)
// Formant presets differ in BEHAVIOR, not just static ratio number:
static volatile int gPresetFormantWobble = 0;  // slow LFO breathes the ratio
static volatile int gVibOn = 0, gChorusOn = 0, gEnvFOn = 0, gFWahOn = 0;
static volatile int gSineOn = 0, gPogOn = 0, gResOn = 0, gSusOn = 0;
static volatile int gJawOn = 0, gSitarOn = 0, gPanOn = 1; // pan starts ON
static volatile int gSagOn = 0, gSympOn = 0;
static volatile float gBetaBase[NUM_STRINGS] = { 1, 1, 1, 1, 1, 1 };
static volatile float gFormantBase[NUM_STRINGS] = { 1, 1, 1, 1, 1, 1 };
static volatile float gDryTarget = 1.0f;
static volatile float gWetTarget = 0.0f;

/*
 * COMB RESONATOR (delay line). The right primitive for anything that must
 * RING. A delay line captures the waveform on its FIRST pass and
 * recirculates it, so it reaches full amplitude immediately. A biquad
 * resonator has to ACCUMULATE energy over many cycles - and a plucked note
 * decays long before that happens, which is exactly why the previous
 * filter-based Resonator / Sustainer / Sympathetic were inaudible. Every
 * real physical string model (Karplus-Strong) uses a delay line.
 */
#define COMB_SIZE 1024
#define COMB_MASK (COMB_SIZE - 1)
typedef struct {
	float buf[COMB_SIZE];
	unsigned int wr;
	float lp;
} Comb;

static Comb gTarab[TARAB_N];   // the sympathetic string bank
static int gTarabLeadStr = 0;  // which string the tarab scale is currently rooted on
static float gOtherMax[NUM_STRINGS]; // loudest OTHER string, for crosstalk rejection
static float gSympW[NUM_STRINGS][NUM_STRINGS]; // harmonic-relatedness coupling weights

typedef struct {
	float delay0, u, L, F;
	int active;
} Grain;

typedef struct {
	Grain gr[NGRAINS];
	float launchCd, sinceLaunch, lastAnchorDelay;
} GrainStream;

typedef struct {
	float buf[BUF_SIZE];
	float fxbuf[FXBUF_SIZE];
	unsigned int writePos;
	// pitch stages
	GrainStream up;
	float sdDelay, sdOldDelay, sdFadePhase, sdFadeStep;
	int sdFading;
	float sdHiThresh;
	float downAmt, upAmt;
	// formant stages
	GrainStream fsWet, fsDry;
	float beta, formant, formantBlend;
	float formantWobblePhase; // TALKBOX-style slow vowel-breathing LFO
	float tiltLp, tiltLoG, tiltHiG; // auto-EQ tilt on the shifted wet leg
	float env;
	// FX
	float vbuf[VBUF_SIZE];
	unsigned int vWrite;
	float vPhase;
	float cbuf[VBUF_SIZE];
	unsigned int cWrite;
	float cPhase;
	float fmbuf[VBUF_SIZE];
	unsigned int fmWrite;
	float fmPhase, fmA;
	float sinePhase, sineEnvSm;
	float sinePer, sineInc, sineAmp; // portamento period + per-sample smoothing
	float pogGain, envHold;
	int pogRefract;
	float pogLpZ; // POG filter-sweep lowpass state
	float svfLow, svfBand, svfF, svfFT;
	float susTime, susEnvHold;
	int susRefract;
	// comb resonators - the effects that must RING
	Comb resC, susC, jawC, sympC;
	float resDelaySm;  // smoothed resonator delay, kills clicks on harmonic jumps
	float sitarHp;
	// PITCH CONFIDENCE: blank the tracker through the pick transient, and
	// only accept periods the autocorrelation is genuinely confident about
	float pitchConf, envPrev;
	int attackBlank;
	float periodGood;
	int tonalHops;      // consecutive hops with a STABLE, confident pitch
	float periodLast;   // for measuring hop-to-hop pitch stability
	float rawLast;      // last RAW accepted read (stability on raw, not smoothed)
	float periodCandidate; // last raw candidate, for re-acquisition consensus
	int candidateAgree; // consecutive raw reads that agree WITH EACH OTHER
	float envHist[4];   // hop-rate env shift register, for note-off edge detect
	int envHistIdx;
	float envLastHop;   // previous hop's env, for single-hop release edge
	int releaseBlank;   // blanks the tracker through a detected note-off edge
	// JAWARI BUZZ has no extra per-voice state (uses env directly)
	// SHAKUHACHI/NEY
	// PLUCK SAG
	float sagBuf[VBUF_SIZE];
	unsigned int sagWrite;
	float sagEnv, sagEnvHold, sagDecayCoef;
	int sagRefract;
	// SYNTH POLY: this string's own oscillator (up to 6 phases)
	float pPh[6], pAmp, pInc;
	float sFilt[4];  // synth voice filter state (reused per-mode)
	float pLfo;      // per-voice slow LFO (PWM etc)
	int   pGateOn;   // note is sounding
	// DC blocker
	float dcX1, dcY1;
	// tracking lowpass
	float lpCoef[2][5], lpZ[2][2];
	float harmCut, srate, fcCur;
	// pitch tracker
	float dbuf[DBUF_SIZE];
	unsigned int dwritePos;
	float decimAcc;
	int decimCount, hopCountdown;
	float period, p0;
	int lagMin, lagMax, corrWin;
	float gainL, gainR;
} Voice;

static Voice gV[NUM_STRINGS];
static float gDryS = 1.0f, gWetS = 0.0f;
static float gVibAmtS = 0, gChorAmtS = 0, gEnvFAmtS = 0;
static float gFmAmtS = 0, gSineAmtS = 0, gPogAmtS = 0;
static float gResAmtS = 0, gSusAmtS = 0;
static float gJawAmtS = 0, gSitarAmtS = 0, gPanAmtS = 0;
static float gSagAmtS = 0, gSympAmtS = 0;

// ---- GPIO rotary encoders (main=enc1, aux=enc2) ----
#define PIN_ENC1_CLK 0
#define PIN_ENC1_DT  1
#define PIN_ENC1_SW  2
#define PIN_ENC2_CLK 3
#define PIN_ENC2_DT  4
#define PIN_ENC2_SW  5
static Encoder gEnc1, gEnc2;
static int gEnc1LastPos = 0, gEnc2LastPos = 0;
static int gEnc1BtnStable = 1, gEnc2BtnStable = 1; // debounce state, 1=released
static int gEnc1BtnHold = 0, gEnc2BtnHold = 0;     // debounce countdown
static volatile int gEnc1Click = 0, gEnc2Click = 0; // one-shot flags, consumed then cleared
static volatile int gArmedPad = -1; // which pad currently owns the encoders
static volatile int gRawDiag = 0;   // set 1 to log raw input levels every second
static volatile int gStateDirty = 0; // set on any change, autosaved & cleared in trellisTick
// PRESET <-> MANUAL mode (both-encoder-click toggle). Starts in PRESET.
static volatile int gPresetMode = 1;
static int gComboLatch1 = 0, gComboLatch2 = 0; // both-click combo detect
static volatile int gModeFlash = 0;            // white-flash countdown (LED, ~30Hz)
// which preset pad is currently selected within each exclusive family
// (-1 = none). Formant/Pitch/Vib-Chorus each share one global parameter
// set, so selecting a new one within the family REPLACES the old one;
// re-selecting the SAME pad turns that family off (radio-with-off).
static int gPresetFormantSel = -1;   // pads 0-3
static int gPresetPitchSel = -1;     // pads 4-7
static int gPresetVibChorusSel = -1; // 0=vib(pad8), 1=chorus(pad9), -1=off
#define ENC_DETENT 2   // quadrature transitions per physical click
static int gSwInit = 0, gEnc1Idle = 1, gEnc2Idle = 1;
static float gLimEnv = 0.0f;
static float gHannT[HANN_N + 1];
static double gTimeSec = 0.0;

// monophonic lead voice state
static int gLeadStr = 0;
static float gLeadInc = 0.002f, gLeadIncTgt = 0.002f; // cycles/sample
static float gLeadAmp = 0.0f;
static float gLeadLPz = 0.0f;

static inline float pblep(float t, float dt)
{
	if(t < dt) {
		float x = t / dt;
		return x + x - x * x - 1.0f;
	}
	if(t > 1.0f - dt) {
		float x = (t - 1.0f) / dt;
		return x * x + x + x + 1.0f;
	}
	return 0.0f;
}

/*
 * synthOsc: one sample of the selected synth timbre. Shared by MONO
 * (one global voice) and POLY (six independent per-string voices) -
 * same code path, so every timbre works identically in both modes.
 * inc = cycles per sample. All saw/square use polyBLEP (alias-free).
 */
/*
 * synthOsc: one sample of the selected timbre. Shared by MONO and POLY.
 * HOTSAWZ follows Eventide's published H90 recipe: 6 sawtooth oscillators
 * following the pitch tracker, arranged as DETUNED PAIRS across three
 * registers (sub-octave / unison / octave-up). That octave-stacked
 * detuned-pair layout is what makes it sound huge rather than thin.
 */
static inline float synthOsc(int mode, float *ph, float inc, float lfo,
		float *filt, float sr, int noteOn)
{
	switch(mode) {
	case 0: { // SAW LEAD - 2 saws, ~7 cents (the one that already works)
		float d1 = inc, d2 = inc * 1.004f;
		ph[0] += d1; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		ph[1] += d2; if(ph[1] >= 1.0f) ph[1] -= 1.0f;
		float s1 = 2.0f * ph[0] - 1.0f - pblep(ph[0], d1);
		float s2 = 2.0f * ph[1] - 1.0f - pblep(ph[1], d2);
		return 0.5f * (s1 + s2);
	}
	case 1: { // MOOG LEAD - one fat saw through a resonant 2-pole
		  // lowpass with a classic fast-decay filter envelope: the
		  // "pew" of movement comes from the FILTER, not detuning.
		float d = inc;
		ph[0] += d; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		float saw = 2.0f * ph[0] - 1.0f - pblep(ph[0], d);
		if(noteOn) filt[2] = 1.0f;
		filt[2] *= 0.9993f;                    // ~1s filter-env decay
		float fc = inc * sr * (2.5f + 9.0f * filt[2]);
		if(fc > 0.45f * sr) fc = 0.45f * sr;
		if(fc < 40.0f) fc = 40.0f;
		float f = 2.0f * sinf((float)M_PI * fc / sr);
		if(f > 1.2f) f = 1.2f;
		filt[0] += f * filt[1];
		float hi = saw - filt[0] - 0.35f * filt[1];
		filt[1] += f * hi;
		return filt[0] * 1.6f;
	}
	case 2: { // ACID SQUELCH - 303-style pulse through a NEAR-SELF-
		  // OSCILLATING lowpass, fast filter-env sweep - the classic
		  // squelchy acid bite.
		float d = inc;
		ph[0] += d; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		float pw = 0.35f;
		float sq = ph[0] < pw ? 1.0f : -1.0f;
		sq += pblep(ph[0], d);
		float tp = ph[0] - pw; if(tp < 0.0f) tp += 1.0f;
		sq -= pblep(tp, d);
		if(noteOn) filt[2] = 1.0f;
		filt[2] *= 0.9985f;                    // snappier decay than Moog
		float fc = inc * sr * (1.5f + 14.0f * filt[2]);
		if(fc > 0.45f * sr) fc = 0.45f * sr;
		if(fc < 40.0f) fc = 40.0f;
		float f = 2.0f * sinf((float)M_PI * fc / sr);
		if(f > 1.2f) f = 1.2f;
		filt[0] += f * filt[1];
		float hi = sq - filt[0] - 0.12f * filt[1];  // very low damping = high resonance
		filt[1] += f * hi;
		return filt[0] * 1.1f;
	}
	case 3: { // WAVEFOLDER - sine through a tanh wavefolder, fold depth
		  // breathing on the slow LFO. West Coast/Buchla character:
		  // evolving harmonics from nonlinear shaping, not filtering.
		ph[0] += inc; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		float s = sinf(2.0f * (float)M_PI * ph[0]);
		float amt = 3.0f + 2.5f * lfo;
		float folded = tanhf(s * amt);
		float fc = inc * sr * 8.0f;
		if(fc > 0.45f * sr) fc = 0.45f * sr;
		float f = 2.0f * sinf((float)M_PI * fc / sr);
		filt[0] += f * filt[1];
		float hi = folded - filt[0] - 0.6f * filt[1];
		filt[1] += f * hi;
		return filt[0] * 0.9f;
	}
	case 4: { // RINGMOD BELL - carrier x irrational-ratio modulator.
		  // Multiplication (not addition) makes it INHARMONIC -
		  // metallic/bell tones, structurally different from any FM
		  // or saw-stack patch here.
		ph[0] += inc; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		ph[1] += inc * 1.41421356f; if(ph[1] >= 1.0f) ph[1] -= 1.0f;
		float c = sinf(2.0f * (float)M_PI * ph[0]);
		float m = sinf(2.0f * (float)M_PI * ph[1]);
		float bell = c * m;
		float fc = inc * sr * 6.0f;
		if(fc > 0.45f * sr) fc = 0.45f * sr;
		float f = 2.0f * sinf((float)M_PI * fc / sr);
		filt[0] += f * filt[1];
		float hi = bell - filt[0] - 0.7f * filt[1];
		filt[1] += f * hi;
		return filt[0] * 1.8f;
	}
	case 5: { // SYNC LEAD - a slave oscillator hard-resets on every
		  // master cycle; the slave:master ratio sweeps via the
		  // filter env for the classic screaming sync sweep.
		float dm = inc;
		float prevPh = ph[0];
		ph[0] += dm; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		int reset = ph[0] < prevPh;
		if(noteOn) filt[2] = 1.0f;
		filt[2] *= 0.999f;
		float ratio = 1.6f + 3.0f * filt[2];
		if(reset) ph[1] = 0.0f;
		float ds = dm * ratio;
		ph[1] += ds; if(ph[1] >= 1.0f) ph[1] -= 1.0f;
		float saw = 2.0f * ph[1] - 1.0f - pblep(ph[1], ds);
		return saw * 0.75f;
	}
	case 6: { // FM GROWL - 2:1 ratio, DYNAMIC index that decays from
		  // pick attack - loud pick = bright snarl, soft = mellow.
		ph[0] += inc; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		ph[1] += inc * 2.0f; if(ph[1] >= 1.0f) ph[1] -= 1.0f;
		if(noteOn) filt[2] = 1.0f;
		filt[2] *= 0.9994f;
		float idx = 1.0f + 5.0f * filt[2];
		float m = sinf(2.0f * (float)M_PI * ph[1]);
		return sinf(2.0f * (float)M_PI * ph[0] + idx * m) * 0.85f;
	}
	case 7: { // CHIP LEAD - stepped duty-cycle pulse, deliberately raw
		  // and unfiltered for an authentic 8-bit character.
		float d = inc;
		ph[0] += d; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		static const float DUTY[4] = { 0.125f, 0.25f, 0.5f, 0.75f };
		int di = (int)((lfo * 0.5f + 0.5f) * 4.0f);
		if(di < 0) di = 0;
		if(di > 3) di = 3;
		float pw = DUTY[di];
		float sq = ph[0] < pw ? 1.0f : -1.0f;
		sq += pblep(ph[0], d);
		float tp = ph[0] - pw; if(tp < 0.0f) tp += 1.0f;
		sq -= pblep(tp, d);
		return sq * 0.55f;
	}
	case 8: { // SUB GROWL - fundamental + sub-octave sine through a
		  // dark lowpass. Deep, huge low end - a totally different
		  // register from every treble-leaning saw stack here.
		float d = inc;
		ph[0] += d; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		ph[1] += inc * 0.5f; if(ph[1] >= 1.0f) ph[1] -= 1.0f;
		float saw = 2.0f * ph[0] - 1.0f - pblep(ph[0], d);
		float sub = sinf(2.0f * (float)M_PI * ph[1]);
		float src = 0.4f * saw + 0.8f * sub;
		float fc = inc * sr * 3.0f;
		if(fc > 0.45f * sr) fc = 0.45f * sr;
		if(fc < 40.0f) fc = 40.0f;
		float f = 2.0f * sinf((float)M_PI * fc / sr);
		filt[0] += f * filt[1];
		float hi = src - filt[0] - 0.6f * filt[1];
		filt[1] += f * hi;
		return filt[0] * 1.1f;
	}
	case 9: { // FORMANT VOWEL - a pulse source through TWO real
		  // resonant bandpass filters at vowel formant frequencies,
		  // morphing "ooh"<->"ah" on the slow LFO. An actual vocal
		  // filterbank, not just a detuned saw pair.
		float d = inc;
		ph[0] += d; if(ph[0] >= 1.0f) ph[0] -= 1.0f;
		float pw = 0.3f;
		float src = ph[0] < pw ? 1.0f : -1.0f;
		src += pblep(ph[0], d);
		float tp = ph[0] - pw; if(tp < 0.0f) tp += 1.0f;
		src -= pblep(tp, d);

		float t = 0.5f + 0.5f * lfo;           // 0 = "ooh", 1 = "ah"
		float f1 = 350.0f + 550.0f * t;
		float f2 = 900.0f - 200.0f * t;

		float w1 = 2.0f * sinf((float)M_PI * f1 / sr);
		filt[0] += w1 * filt[1];
		float hi1 = src - filt[0] - 0.25f * filt[1];
		filt[1] += w1 * hi1;

		float w2 = 2.0f * sinf((float)M_PI * f2 / sr);
		filt[2] += w2 * filt[3];
		float hi2 = src - filt[2] - 0.30f * filt[3];
		filt[3] += w2 * hi2;

		return (filt[1] * 1.3f + filt[3] * 0.9f) * 0.5f;
	}
	}
	return 0.0f;
}

static inline float bufAtM(const float *buf, unsigned int mask,
		unsigned int wr, float delay)
{
	float readPos = (float)(wr & mask) - delay;
	int i1 = (int)floorf(readPos);
	float fr = readPos - (float)i1;
	float y0 = buf[(i1 - 1) & mask];
	float y1 = buf[i1 & mask];
	float y2 = buf[(i1 + 1) & mask];
	float y3 = buf[(i1 + 2) & mask];
	float a = 0.5f * (-y0 + 3.0f * y1 - 3.0f * y2 + y3);
	float b = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
	float c = 0.5f * (y2 - y0);
	return ((a * fr + b) * fr + c) * fr + y1;
}

static inline float combRun(Comb *c, float in, float delay, float fb, float damp)
{
	if(delay < 8.0f) delay = 8.0f;
	if(delay > (float)(COMB_SIZE - 8)) delay = (float)(COMB_SIZE - 8);
	float y = bufAtM(c->buf, COMB_MASK, c->wr, delay);
	c->lp += damp * (y - c->lp);        // damping = string/air losses
	float v = in + c->lp * fb;
	if(v > 4.0f) v = 4.0f;              // bounded: cannot run away
	if(v < -4.0f) v = -4.0f;
	c->buf[c->wr & COMB_MASK] = v;
	c->wr++;
	return c->lp;
}

static inline float hann01(float t)
{
	if(t <= 0.0f || t >= 1.0f) return 0.0f;
	return gHannT[(int)(t * HANN_N)];
}

/* ---------------- PSOLA grain stream ---------------- */

static void launchGrain(GrainStream *gs, unsigned int wr, unsigned int mask,
		float P, float beta, float F)
{
	float S = P / beta;
	if(S < 20.0f) S = 20.0f;
	if(S > 4000.0f) S = 4000.0f;

	float C = 2.0f * P;
	/*
	 * minC is a FLOOR on grain length (window duration L = C/F, overlap
	 * ratio = L/S). At 1.05x it only guaranteed ~5% overlap - for an
	 * UP-shift formant (F>1) this floor actively clamps the window down
	 * to barely more than a single grain, nowhere near enough Hann
	 * crossfade cushion to blend smoothly. That's exactly what a thin,
	 * clicky, "crackly" up-shift (e.g. SPRITE at F=2.2) sounds like:
	 * hard, audible transitions at every grain boundary. 2.2x guarantees
	 * real crossfade overlap in all cases; this only changes anything
	 * for F above ~0.9ish (2P was already comfortably above the old
	 * floor for real down-shifts, so those are untouched).
	 */
	float minC = 2.2f * S * F;
	if(C < minC) C = minC;
	float maxC = 7.5f * S * F;
	if(C > maxC) C = maxC;
	if(C > 5000.0f) C = 5000.0f;
	float L = C / F;

	/*
	 * NOTE: a prior version of this code "snapped" the launch cadence
	 * itself to try to land the overlap ratio on an integer (a COLA-
	 * compliance attempt) - that was a real bug, not a refinement: S is
	 * what LOCKS the perceived pitch to the true period (grains must
	 * repeat at exactly the original pitch period for formant shifting
	 * to leave pitch untouched). Snapping it away from S made every
	 * non-unity formant ratio ALSO shift pitch. Never touch S itself
	 * for anything other than its own intended use below. A follow-up
	 * attempt at a flatter window at high overlap was also reverted -
	 * it leaked in an audible extra partial. Plain Hann for every grain.
	 */
	float needed = 64.0f + (C > L ? C - L : 0.0f);
	float dIdeal = gs->lastAnchorDelay + gs->sinceLaunch;
	float k = roundf((dIdeal - (needed + 0.6f * P)) / P);
	float d = dIdeal - k * P;
	if(d < needed) d += P;
	while(d + C > (float)(mask - 15) && d - P >= needed)
		d -= P;
	if(d + C > (float)(mask - 15)) d = needed;

	Grain *g = &gs->gr[0];
	float bestProg = -1.0f;
	for(int i = 0; i < NGRAINS; i++) {
		if(!gs->gr[i].active) { g = &gs->gr[i]; bestProg = 2.0f; break; }
		float prog = gs->gr[i].u / gs->gr[i].L;
		if(prog > bestProg) { bestProg = prog; g = &gs->gr[i]; }
	}
	g->delay0 = d;
	g->u = 0.0f;
	g->L = L;
	g->F = F;
	g->active = 1;

	gs->lastAnchorDelay = d;
	gs->sinceLaunch = 0.0f;
	gs->launchCd += S;
}

static inline float streamRun(GrainStream *gs, const float *buf,
		unsigned int mask, unsigned int wr, float P, float beta, float F)
{
	if((gs->launchCd -= 1.0f) <= 0.0f)
		launchGrain(gs, wr, mask, P, beta, F);
	gs->sinceLaunch += 1.0f;

	float out = 0.0f;
	for(int i = 0; i < NGRAINS; i++) {
		Grain *g = &gs->gr[i];
		if(!g->active)
			continue;
		float t = g->u / g->L;
		float w = hann01(t);
		out += w * bufAtM(buf, mask, wr, g->delay0 + g->u * (1.0f - g->F));
		g->u += 1.0f;
		if(g->u >= g->L)
			g->active = 0;
	}
	return out;
}

/* -------- SOLAD continuous downshifter (full lows, no AM nulls) -------- */

static inline float soladRun(Voice *v, float beta)
{
	float drift = 1.0f - beta;
	if(drift < 0.01f) {
		v->sdDelay = 0.5f * v->sdHiThresh;
		v->sdFading = 0;
		return bufAtM(v->buf, BUF_MASK, v->writePos, v->sdDelay);
	}

	if(!v->sdFading && v->sdDelay > v->sdHiThresh) {
		float J = v->period;
		v->sdOldDelay = v->sdDelay;
		v->sdDelay -= J;
		if(v->sdDelay < 8.0f) v->sdDelay = 8.0f;
		v->sdFading = 1;
		v->sdFadePhase = 0.0f;
		v->sdFadeStep = 1.0f / (0.5f * J);
	}

	float out;
	if(v->sdFading) {
		float t = v->sdFadePhase;
		float g = t * t * (3.0f - 2.0f * t);
		out = bufAtM(v->buf, BUF_MASK, v->writePos, v->sdOldDelay) * (1.0f - g)
		    + bufAtM(v->buf, BUF_MASK, v->writePos, v->sdDelay) * g;
		v->sdFadePhase += v->sdFadeStep;
		if(v->sdFadePhase >= 1.0f)
			v->sdFading = 0;
		v->sdOldDelay += drift;
	} else {
		out = bufAtM(v->buf, BUF_MASK, v->writePos, v->sdDelay);
	}
	v->sdDelay += drift;
	return out;
}

/* -------------------- per-sample voice processing -------------------- */

static inline void voiceProcess(Voice *v, float in, int dryNeeded,
		float *wetOut, float *dryOut)
{
	float x = in - v->dcX1 + 0.995f * v->dcY1;
	v->dcX1 = in;
	v->dcY1 = x;
	float full = x;

	float ax = fabsf(full);
	v->env = ax > v->env ? ax : v->env * 0.9995f;

	for(int sec = 0; sec < 2; sec++) {
		float *c = v->lpCoef[sec];
		float *z = v->lpZ[sec];
		float y = c[0] * x + z[0];
		z[0] = c[1] * x - c[3] * y + z[1];
		z[1] = c[2] * x - c[4] * y;
		x = y;
	}
	float high = full - x;
	v->buf[v->writePos & BUF_MASK] = x;

	v->decimAcc += x;
	if(++v->decimCount >= DECIM) {
		v->dbuf[v->dwritePos & DBUF_MASK] = v->decimAcc * (1.0f / DECIM);
		v->dwritePos++;
		v->decimAcc = 0.0f;
		v->decimCount = 0;
	}

	// pitch stage (direction-optimized hybrid)
	float dA = v->downAmt, uA = v->upAmt;
	float p = x * (1.0f - dA - uA);
	if(dA > 0.002f)
		p += dA * soladRun(v, v->beta < 1.0f ? v->beta : 1.0f);
	else
		soladRun(v, 1.0f);
	if(uA > 0.002f) {
		float bUp = v->beta > 1.0f ? v->beta : 1.0f;
		p += uA * streamRun(&v->up, v->buf, BUF_MASK, v->writePos,
				v->period, bUp, 1.0f);
	}
	// AUTO EQ tilt on the shifted signal (gains set per-hop; unity when
	// beta=1 so the passthrough path is untouched)
	v->tiltLp += 0.16f * (p - v->tiltLp);
	p = v->tiltLp * v->tiltLoG + (p - v->tiltLp) * v->tiltHiG;

	v->fxbuf[v->writePos & FXBUF_MASK] = p;

	// series formant stage
	float fb = v->formantBlend;
	float wet = p;
	if(fb > 0.003f) {
		float pOut = v->period / v->beta;
		if(pOut < 30.0f) pOut = 30.0f;
		if(pOut > 2400.0f) pOut = 2400.0f;
		float fw = streamRun(&v->fsWet, v->fxbuf, FXBUF_MASK, v->writePos,
				pOut, 1.0f, v->formant);
		wet = p * (1.0f - fb) + fw * fb;
	}
	// duck the untouched high band while formant is active - it was
	// pasting the original brightness over the shift and masking it
	*wetOut = wet + high * HIGH_MIX * (1.0f - 0.65f * fb);

	if(dryNeeded && fb > 0.003f) {
		float df = streamRun(&v->fsDry, v->buf, BUF_MASK, v->writePos,
				v->period, 1.0f, v->formant) + high * 0.35f;
		*dryOut = full * (1.0f - fb) + df * fb;
	} else {
		*dryOut = full;
	}

	v->writePos++;
}

/* ------------------- filters / trackers / modulation ------------------- */

static void updateLP(Voice *v)
{
	static const float Q[2] = { 0.54119610f, 1.30656296f };
	float mult = fabsf(v->formant - 1.0f) > 0.02f ? 2.5f : 1.0f; // wide band for formant work
	float fc = mult * v->harmCut * v->srate / v->period;
	if(fc > 0.40f * v->srate) fc = 0.40f * v->srate;
	if(fc < 60.0f) fc = 60.0f;
	v->fcCur += 0.25f * (fc - v->fcCur);
	fc = v->fcCur;
	float w0 = 2.0f * (float)M_PI * fc / v->srate;
	float cw = cosf(w0), sw = sinf(w0);
	for(int sec = 0; sec < 2; sec++) {
		float alpha = sw / (2.0f * Q[sec]);
		float a0inv = 1.0f / (1.0f + alpha);
		float b1 = (1.0f - cw) * a0inv;
		v->lpCoef[sec][0] = 0.5f * b1;
		v->lpCoef[sec][1] = b1;
		v->lpCoef[sec][2] = 0.5f * b1;
		v->lpCoef[sec][3] = -2.0f * cw * a0inv;
		v->lpCoef[sec][4] = (1.0f - alpha) * a0inv;
	}
}

static inline float nsdfAt(const Voice *v, const float *recent, float e0, int lag)
{
	float acf = 0.0f, el = 0.0f;
	const float *lagged = recent - lag;
	for(int i = 0; i < v->corrWin; i++) {
		acf += recent[i] * lagged[i];
		el += lagged[i] * lagged[i];
	}
	return acf / sqrtf(e0 * el + 1e-12f);
}

static void trackPitch(Voice *v)
{
	/*
	 * NOTE-OFF EDGE DETECTOR: releasing a fretting finger (or palm-
	 * muting) damps the string almost instantly - MUCH faster than a
	 * freely ringing string could ever decay on its own. That abrupt
	 * collapse is exactly what tells a real release apart from natural
	 * decay: a slowly-dying sustained note can (correctly) drift its
	 * "recent peak" reference down over time, so a simple "is env below
	 * X% of some hold" ratio isn't reliable - it depends on how loud
	 * the note already was. An EDGE (env collapsing by >65% within
	 * ~11.6ms, four hops - OR by >50% in a single ~2.9ms hop, for a
	 * hard/instant mute) can't be produced by decay physics; it can
	 * only be a mute/release event. Detect that edge and blank the
	 * tracker through it, exactly like the existing pick-attack blank -
	 * hold the last known-good period rather than accept whatever the
	 * dying buzz/scrape correlates to.
	 *
	 * ONLY ARM WHILE NOT INSIDE A FRESH ATTACK: a pick transient's own
	 * envelope is choppy (broadband click, brief lull, THEN the
	 * fundamental settles in) - that lull looks exactly like a release
	 * edge to a naive hop-to-hop check, and re-triggering releaseBlank
	 * mid-attack stacks ~35ms blank windows back-to-back, turning the
	 * note-on latency into a huge, audible delay. Skip the edge check
	 * entirely while attackBlank is still counting down; there is no
	 * legitimate "release" to detect microseconds into a fresh pick.
	 */
	{
		float envOld = v->envHist[v->envHistIdx];
		v->envHist[v->envHistIdx] = v->env;
		v->envHistIdx = (v->envHistIdx + 1) & 3;
		if(v->attackBlank == 0) {
			if(envOld > 0.003f && v->env < 0.35f * envOld)
				v->releaseBlank = 12;    // ~35ms
			/*
			 * FAST EDGE: the 4-hop lookback above needs ~11.6ms to notice
			 * a release, because it waits for env to fall to 35% of its
			 * value 4 hops back. A hard mute/finger-lift can collapse the
			 * envelope in a SINGLE hop (~2.9ms) - catch that immediately
			 * so no bad reads slip through in the few hops before the
			 * slower check would otherwise fire.
			 */
			if(v->envLastHop > 0.003f && v->env < 0.5f * v->envLastHop)
				v->releaseBlank = 12;
		}
		v->envLastHop = v->env;
	}
	if(v->releaseBlank > 0) {
		v->releaseBlank--;
		v->pitchConf = 0.0f;
		return;
	}

	float x[DBUF_SIZE];
	int need = v->corrWin + v->lagMax + 1;
	for(int i = 0; i < need; i++)
		x[i] = v->dbuf[(v->dwritePos - need + i) & DBUF_MASK];
	float *recent = x + need - v->corrWin;

	float e0 = 0.0f;
	for(int i = 0; i < v->corrWin; i++)
		e0 += recent[i] * recent[i];
	if(e0 < GATE_RMS * GATE_RMS * (float)v->corrWin) {
		v->pitchConf = 0.0f;   // silence: definitely not a pitched note
		return;
	}

	float coarse[256];
	int nC = 0;
	float best = -2.0f;
	for(int lag = v->lagMin; lag <= v->lagMax; lag += 2) {
		float val = nsdfAt(v, recent, e0, lag);
		coarse[nC++] = val;
		if(val > best) best = val;
	}
	if(best < NSDF_CONF) {
		v->pitchConf = best;   // aperiodic (tap/noise): record the low score
		return;
	}

	int bestLag = -1;
	float bar = FIRST_PEAK * best;
	for(int i = 1; i < nC - 1; i++) {
		if(coarse[i] >= bar && coarse[i] >= coarse[i - 1] && coarse[i] >= coarse[i + 1]) {
			bestLag = v->lagMin + 2 * i;
			best = coarse[i];
			break;
		}
	}
	if(bestLag < 0) {
		v->pitchConf = 0.0f;
		return;
	}
	for(int lag = bestLag - 1; lag <= bestLag + 1; lag += 2) {
		if(lag < v->lagMin || lag > v->lagMax)
			continue;
		float val = nsdfAt(v, recent, e0, lag);
		if(val > best) { best = val; bestLag = lag; }
	}

	/*
	 * Anti-octave-low guard: high on the neck the short true-period
	 * peak is under-sampled by the coarse grid and can lose to the
	 * 2P peak. If half this lag correlates nearly as well, the note
	 * is actually an octave HIGHER - take the half lag.
	 *
	 * (A mirrored "check double the lag" guard was tried here and
	 * reverted - a genuinely periodic signal ALWAYS correlates well at
	 * 2x its true period too, that's what periodicity means, so
	 * "double lag also correlates" is not evidence double lag is
	 * correct. It fired on essentially every note and pushed pitch
	 * down an octave constantly. Deliberately not doing that again.)
	 */
	int half = bestLag / 2;
	if(half >= v->lagMin) {
		float hv = -2.0f;
		int hb = half;
		for(int k = half - 1; k <= half + 1; k++) {
			if(k < v->lagMin || k > v->lagMax)
				continue;
			float val = nsdfAt(v, recent, e0, k);
			if(val > hv) { hv = val; hb = k; }
		}
		if(hv > 0.92f * best) {
			bestLag = hb;
			best = hv;
		}
	}

	float lagF = (float)bestLag;
	if(bestLag > v->lagMin && bestLag < v->lagMax) {
		float ym = nsdfAt(v, recent, e0, bestLag - 1);
		float yp = nsdfAt(v, recent, e0, bestLag + 1);
		float denom = ym - 2.0f * best + yp;
		if(fabsf(denom) > 1e-9f)
			lagF += 0.5f * (ym - yp) / denom;
	}

	float p = lagF * (float)DECIM;
	float pMin = v->p0 / MAX_FRET_RATIO;
	float pMax = v->p0 * TUNE_MARGIN;
	if(p < pMin) p = pMin;
	if(p > pMax) p = pMax;

	/*
	 * ATTACK REJECTION + CONFIDENCE GATE.
	 * A pick attack is a broadband transient with NO stable periodicity -
	 * tracking it produces garbage pitch, which is what made the synth
	 * and the resonators lurch around on every note. So:
	 *  1. blank the tracker for ~30 ms after a fresh attack, and
	 *  2. only accept a new period when the normalized autocorrelation
	 *     says it's genuinely confident (best >= PITCH_CONF_MIN).
	 * Otherwise HOLD the last known-good period - a definite fundamental
	 * is always better than a confident-sounding wrong one.
	 */
	v->pitchConf = best;
	if(v->attackBlank > 0) {
		v->attackBlank--;
		return;                       // still inside the transient - ignore
	}
	if(best < PITCH_CONF_MIN)
		return;                       // not confident: keep the last period

	/*
	 * FULL-RATE REFINEMENT: the decimated (x4) search above is only good
	 * to ~1-2 raw samples even after parabolic interp - which is 10-20
	 * CENTS of error on the high strings (period ~160 samples). That's
	 * the "synth is slightly detuned from what I played" artifact. So
	 * re-measure at the FULL sample rate in a +-3 sample window around
	 * the coarse estimate, then parabolic-interp to ~0.1 sample (~1
	 * cent). Continuous, never quantized - bends stay perfectly smooth.
	 */
	{
		int ip = (int)roundf(p);
		int win = (int)(1.2f * p);
		if(win > 620) win = 620;
		const float *b = v->buf;
		unsigned int wp = v->writePos;
		float re0 = 0.0f;
		for(int i = 0; i < win; i++) {
			float sm = b[(wp - 1 - i) & BUF_MASK];
			re0 += sm * sm;
		}
		float vals[7];
		int bi = -1;
		float bv = -2.0f;
		for(int j = 0; j < 7; j++) {
			int l = ip - 3 + j;
			float acf = 0.0f, el = 0.0f;
			for(int i = 0; i < win; i++) {
				float sa = b[(wp - 1 - i) & BUF_MASK];
				float sb = b[(wp - 1 - i - l) & BUF_MASK];
				acf += sa * sb;
				el += sb * sb;
			}
			vals[j] = acf / sqrtf(re0 * el + 1e-12f);
			if(vals[j] > bv) { bv = vals[j]; bi = j; }
		}
		if(bi >= 1 && bi <= 5 && bv > 0.5f) {
			float ym = vals[bi - 1], yp = vals[bi + 1];
			float denom = ym - 2.0f * bv + yp;
			float off = 0.0f;
			if(fabsf(denom) > 1e-9f) {
				off = 0.5f * (ym - yp) / denom;
				if(off > 0.5f) off = 0.5f;
				if(off < -0.5f) off = -0.5f;
			}
			float pf = (float)(ip - 3 + bi) + off;
			if(pf > 0.8f * p && pf < 1.2f * p)
				p = pf;
		}
	}

	v->periodGood = p;

	/*
	 * DECAY-TAIL FREEZE: the biggest remaining bug. Lift a fretting
	 * finger off a ringing note and the string produces a brief buzz/
	 * scrape as it dies - quiet, but still periodic-ish enough to clear
	 * the confidence gate, usually landing on some garbage long lag.
	 * The snap logic below (correctly) treats 2 confident-but-different
	 * reads as a genuinely new note and jumps the shifter there
	 * INSTANTLY - which is exactly right for a fresh pick, but on a
	 * decaying tail it's a real string physically CANNOT change pitch,
	 * it can only ring out. So: only accept ANY period update (snap or
	 * glide) while the signal is still close to its own recent peak.
	 * Once it's fallen well below that (the decay tail), HOLD the last
	 * good period outright - don't glide toward the noise either.
	 */
	if(v->env < 0.25f * v->envPrev) {
		v->candidateAgree = 0;
		return;                       // decaying tail: hold, don't chase noise
	}

	/*
	 * NOTE-CHANGE SNAP vs BEND GLIDE, with RE-ACQUISITION CONSENSUS.
	 * The old fixed 75/25 blend WALKED from the previous note to the
	 * new one over ~8 hops, passing through every wrong pitch in
	 * between - fixed by snapping instantly on a confirmed new note.
	 * But the first version of that fix only required two consecutive
	 * reads to each be >3% away from the OLD period - it never checked
	 * that the two reads AGREED WITH EACH OTHER. Right as attack/release
	 * blanking lifts, transient-influenced correlation can hand back two
	 * DIFFERENT wrong candidates in a row (a genuine read, then an
	 * octave-confused one, or vice versa) and both individually clear
	 * the "far from old note" bar - so the tracker snapped to whichever
	 * landed second. That's the "start/stop briefly goes flat or hits a
	 * high note" bug. Now a snap requires the current raw read to also
	 * agree with the IMMEDIATELY PRIOR raw read (not the smoothed/
	 * committed period) to within 1.5% - two individually-plausible but
	 * mutually-disagreeing transient reads can no longer produce a false
	 * snap; only real, repeatable periodicity can.
	 */
	{
		float devN = fabsf(p - v->period) / (v->period + 1e-9f);
		if(devN > 0.03f) {
			float agreeDev = fabsf(p - v->periodCandidate) / (p + 1e-9f);
			if(v->candidateAgree > 0 && agreeDev < 0.015f) {
				if(++v->candidateAgree >= 2) {
					v->period = p;   // new note, CONFIRMED: arrive NOW
					v->candidateAgree = 0;
				}
			} else {
				v->candidateAgree = 1;
			}
			v->periodCandidate = p;
		} else {
			v->candidateAgree = 0;
			v->period = 0.75f * v->period + 0.25f * p;   // bend: glide
		}
	}
}

static void updateModulation(Voice *v, int s)
{
	// pitch: straightforward static shifters only (RANDOM/DYNAMIC archived)
	float betaT = gPitchActive ? gBetaBase[s] : 1.0f;
	float formT = gFormantActive ? gFormantBase[s] : 1.0f;

	// FORMANT WAH: matched dynamic sensitivity, wider throw
	if(gFWahOn) {
		float e = v->env * gFWahSens;
		if(e > 1.0f) e = 1.0f;
		formT *= 0.50f + 1.55f * e;   // DRAMATIC: huge vowel swing on attack
	}
	/*
	 * REMOVED: SPRITE's envelope-reactive formant tracking. Every grain
	 * LOCKS IN its resampling rate (F) for its whole lifetime at launch.
	 * Continuously modulating F - especially fast, exactly what an
	 * attack-coupled envelope does - means overlapping grains launched
	 * moments apart carry meaningfully different F, so their internal
	 * read positions phase-diverge from each other. That divergence at
	 * the crossfade IS the click/warble. A grain engine like this can
	 * only guarantee smoothness with a STATIC or very-slowly-drifting F -
	 * no exceptions. SPRITE is now static, like GIANT, distinguished by
	 * its own ratio/glide identity instead.
	 */
	// TALKBOX's identity: a slow, gentle vowel-breathing wobble instead
	// of a static ratio - the formant drifts on its own over time. Kept
	// (unlike SPRITE's fast attack-coupled version above) because it's
	// slow enough that consecutive grain launches see only a tiny F
	// delta - extra safety margin added (slower, shallower) regardless.
	if(gPresetFormantWobble) {
		v->formantWobblePhase += 2.0f * (float)M_PI * 0.15f / v->srate;
		if(v->formantWobblePhase > 2.0f * (float)M_PI)
			v->formantWobblePhase -= 2.0f * (float)M_PI;
		formT *= 1.0f + 0.04f * sinf(v->formantWobblePhase);
	}
	if(formT < 0.40f) formT = 0.40f;
	if(formT > 2.40f) formT = 2.40f;

	v->beta += 0.3f * (betaT - v->beta);
	// FORMANT-WAH needs to track the pick attack, not ease into it - the
	// user's own glide setting is right for the static FORMANT pad (an
	// intentional slow settle), but under WAH formT is being driven by
	// the envelope every hop, and the slow glide made the vowel sweep
	// visibly lag the actual dynamics ("smearing" on hard picks).
	float formGlide = gFWahOn ? 0.20f : gFormantGlide;
	v->formant += formGlide * (formT - v->formant);

	/*
	 * AUTO EQ (the H90's trick): a down-shift darkens and smears the
	 * attack, so tilt the shifted leg BRIGHTER; an up-shift thins it,
	 * so tilt it back toward the lows. Pivot ~1.1kHz, scale with how
	 * far the shift is from unity (neutral at beta=1).
	 */
	float tt = log2f(v->beta);
	if(tt > 1.0f) tt = 1.0f;
	if(tt < -1.0f) tt = -1.0f;
	/*
	 * gFormantTiltBias adds an EXTRA tilt term driven by the FORMANT
	 * ratio rather than pitch beta - beta is normally 1.0 (neutral, tt=0)
	 * for a formant-only patch, so a preset wanting vowel-forward EQ
	 * character (e.g. TALKBOX) needs its own driver, not a bias on a
	 * signal that's usually zero here. 0 for every preset that doesn't
	 * opt in, so this changes nothing for anyone else.
	 */
	float tf = log2f(v->formant) * gFormantTiltBias;
	if(tf > 1.0f) tf = 1.0f;
	if(tf < -1.0f) tf = -1.0f;
	v->tiltLoG = 1.0f + 0.20f * tt + 0.20f * tf; // down: lows eased, up: body restored
	v->tiltHiG = 1.0f - 0.35f * tt - 0.35f * tf; // down: highs boosted, up: fizz eased

	float dT = v->beta < 0.995f ? 1.0f : 0.0f;
	float uT = v->beta > 1.005f ? 1.0f : 0.0f;
	v->downAmt += 0.25f * (dT - v->downAmt);
	v->upAmt += 0.25f * (uT - v->upAmt);

	float dev = fabsf(v->formant - 1.0f) * 25.0f;
	if(dev > 1.0f) dev = 1.0f;
	/*
	 * REMOVED: an "onset gate" used to force dev=0 (dry passthrough)
	 * through the whole attackBlank window (~17ms), then crossfade the
	 * formant back in over the following hops. That made EVERY note
	 * start with no formant color at all and swell in afterward - an
	 * envelope-following shape structurally identical to autowah,
	 * regardless of which preset was active. That's exactly why every
	 * formant preset "felt like auto wah / dynamics reactive." It's not
	 * needed anymore: the grain streams now properly re-anchor on every
	 * attack (see the TRANSIENT RE-ANCHOR block) and the overlap floor
	 * was raised, both of which independently keep transients clean
	 * without also hiding the effect at the start of the note. Formant
	 * is present from the first hop now, fast, like it should be.
	 */
	v->formantBlend += 0.3f * (dev - v->formantBlend);

	// AUTOWAH sweep - sensitivity halved per feel
	float e = v->env * gEnvfSens;
	if(e > 1.0f) e = 1.0f;
	float fcEnv = ENVF_FMIN * exp2f(ENVF_OCTS * e);
	v->svfFT = 2.0f * sinf((float)M_PI * fcEnv / v->srate);

	// FM modulator geometry: A = INDEX * period/(2*pi*ratio)
	if(gFmActive) {
		/*
		 * Delay-line phase modulation. Phase-mod index for the
		 * fundamental is beta = 2*pi*A/period, so A = idx*period/(2*pi).
		 * HARD CONSTRAINT: the read pointer must never outrun the write
		 * pointer (through-zero = time-reversed mush, not FM). That
		 * requires A * 2*pi*ratio/period < 1, i.e. idx*ratio < 1.
		 * Clamp to 0.85 for headroom - this is what was broken.
		 */
		float ratio = gFmRatioVal < 0.05f ? 0.05f : gFmRatioVal;
		float idx = gFmIndexVal;
		float idxMax = 0.85f / ratio;
		if(idx > idxMax) idx = idxMax;
		float A = idx * v->period / (2.0f * (float)M_PI);
		if(A > FM_MAX_A) A = FM_MAX_A;
		v->fmA += 0.3f * (A - v->fmA);
	}

	// SYNTH LEAD: fundamental-locked portamento (ignores attack jitter)
	v->sinePer += 0.06f * (v->period - v->sinePer);
	float se = v->env * SINE_ENV_SENS;
	if(se > 1.0f) se = 1.0f;
	v->sineEnvSm += 0.35f * (se - v->sineEnvSm);

	// POG onset detection: retrigger the ~100ms bloom on each new pick
	if(v->pogRefract > 0)
		v->pogRefract--;
	if(v->pogRefract == 0 && v->env > 2.0f * v->envHold + 0.004f) {
		v->pogGain = 0.05f;
		v->pogRefract = 20; // ~58ms at HOP rate
	}
	v->envHold = v->env > v->envHold ? v->env : v->envHold * 0.985f;

	/*
	 * STEREO SPREAD: pad OFF = mono (everything centred). ON = the
	 * per-string pan pattern scaled by MAIN, plus AUX dynamic panning -
	 * the harder a string is hit the further out it flies from centre.
	 */
	{
		float pan = 0.0f;
		if(gPanOn) {
			pan = STRING_PAN[s] * gPanSpread;
			if(gPanDyn > 0.001f) {
				float e = v->env * 3.0f;
				if(e > 1.0f) e = 1.0f;
				float dir = STRING_PAN[s] >= 0.0f ? 1.0f : -1.0f;
				pan += dir * gPanDyn * e;
			}
			if(pan < -1.0f) pan = -1.0f;
			if(pan > 1.0f) pan = 1.0f;
		}
		float angle = (pan + 1.0f) * 0.25f * (float)M_PI;
		v->gainL = cosf(angle);
		v->gainR = sinf(angle);
	}

	/*
	 * TONAL GATE: count consecutive hops where the autocorrelation is
	 * confident this is a genuinely PERIODIC signal. A tap, knock, palm
	 * hit or handling noise is loud but APERIODIC - it never accumulates
	 * tonal hops, so the synth stays silent. Only a real fretted/plucked
	 * note (which IS periodic) opens the gate. This is what stops the
	 * synth firing notes when you tap the strings.
	 *
	 * CROSSTALK REJECTION (the actual fix for tap-triggers + poly mess):
	 * hex pickups bleed roughly -20dB (~0.1 amplitude ratio) into
	 * neighboring channels. That bleed is a perfectly clean, periodic
	 * tone from whichever string IS being played, so a "silent" string
	 * kept tracking its NEIGHBOR's pitch with high confidence in the
	 * background - and a mere touch/tap (which spikes env without adding
	 * any real periodicity of its own) was enough to pass the old
	 * envelope-only checks and open the synth on the bleed pitch. Demand
	 * this string's own level clearly DOMINATE every other string before
	 * its tonal hops even count. 0.35 sits well above the ~0.1 crosstalk
	 * floor with real margin; a genuinely played note is essentially
	 * always far louder in its own channel than any neighbor's bleed.
	 */
	{
		// stability measured on the RAW accepted reads, not the smoothed
		// period (the smoothing itself used to fake instability mid-walk)
		float dev = fabsf(v->periodGood - v->rawLast) / (v->periodGood + 1e-9f);
		v->rawLast = v->periodGood;
		int dominant = v->env > 0.5f * gOtherMax[s];   // was 0.35 - a hand
		// rubbed across all six strings raises every channel's level
		// together (not one clean pluck standing out), so widen the
		// margin further above the ~0.1 crosstalk floor.
		// A real note has a STRONG, STABLE fundamental. A tap is loud but
		// aperiodic; an attack transient is periodic-ish but its period
		// jumps around. Demand ALL THREE: high confidence, a period that
		// isn't moving more than ~2% hop to hop, and this string's own
		// signal dominating (not crosstalk from a neighbor).
		if(v->pitchConf > PITCH_CONF_MIN && dev < 0.02f && dominant) {
			if(v->tonalHops < 200) v->tonalHops++;
		} else {
			v->tonalHops = 0;
		}
	}

	// Fresh pick? Blank the pitch tracker through the transient so no
	// effect ever chases the attack noise instead of the real note.
	if(v->env > 1.9f * v->envPrev + 0.004f) {
		v->attackBlank = ATTACK_BLANK_HOPS;
		/*
		 * STALE-PITCH GUARD: the period is FROZEN through the blank, and
		 * frozen = zero deviation = tonalHops kept counting through the
		 * attack on the OLD note's pitch. On a fast repick/hammer-on the
		 * synth gate reopened instantly and snapped to the PREVIOUS
		 * note's frequency - the sour note-on. A new pick must re-earn
		 * pitch stability before the synth may speak.
		 */
		v->tonalHops = 0;
		/*
		 * TRANSIENT RE-ANCHOR: the mushy-bass fix. By the time a new
		 * pick lands, the downshifter's read tap has drifted up to a
		 * full period behind the write head, so the attack gets served
		 * late and crossfaded into soup. Splice the tap back to fresh
		 * signal RIGHT NOW (clickless - reuses the normal fade), and
		 * make the up-shifter's next grain anchor at the attack too.
		 */
		if(v->sdDelay > 40.0f) {
			v->sdOldDelay = v->sdDelay;
			v->sdDelay = 16.0f;
			v->sdFading = 1;
			v->sdFadePhase = 0.0f;
			v->sdFadeStep = 1.0f / (0.5f * v->period + 32.0f);
		}
		v->up.lastAnchorDelay = 72.0f;
		v->up.launchCd = 0.0f;
		/*
		 * The formant grain streams (fsWet/fsDry) never got this same
		 * re-anchor treatment - they only get initialized once at boot,
		 * so every note picked while formant is engaged played through
		 * grains still anchored to stale, pre-attack buffer content.
		 * The onset gate (formantBlend forced to 0 through attackBlank,
		 * see below) hides most of it, but the stale anchor was still
		 * there the instant formantBlend started crossfading back in -
		 * that mismatch IS the "little bad noise at transient starts."
		 * Same fix, same reasoning as the pitch-shifter above.
		 */
		v->fsWet.lastAnchorDelay = 72.0f;
		v->fsWet.launchCd = 0.0f;
		v->fsDry.lastAnchorDelay = 72.0f;
		v->fsDry.launchCd = 0.0f;
	}
	v->envPrev = v->env > v->envPrev ? v->env : v->envPrev * 0.985f;

	// PLUCK SAG onset detection + decay coefficient (settle time is live)
	v->sagDecayCoef = expf(-1.0f / (gSagSettleSec * v->srate));
	if(v->sagRefract > 0)
		v->sagRefract--;
	if(v->sagRefract == 0 && v->env > 1.8f * v->sagEnvHold + 0.003f) {
		v->sagEnv = 1.0f;
		v->sagRefract = 20;
	}
	v->sagEnvHold = v->env > v->sagEnvHold ? v->env : v->sagEnvHold * 0.985f;

	/*
	 * SUSTAINER feedback gain, period-independent decay time. gSusFb is
	 * the raw per-sample-loop gain AT A REFERENCE PERIOD; the equal-RT60
	 * gain at this string's own period works out to gSusFb^(period/ref)
	 * (derived from the standard RT60 = -3*delay/(sr*log10(fb)) comb
	 * formula - the sr and log10(gSusFb) terms cancel out to that clean
	 * power form). Without this, a fixed raw gain makes short-period
	 * (high) strings decay several times faster than long-period (low)
	 * strings for the "same" sustain setting.
	 */
	{
		const float SUS_REF_PERIOD = 300.0f;
		float fbNote = powf(gSusFb, v->period / SUS_REF_PERIOD);
		if(fbNote > 0.9999f) fbNote = 0.9999f;
		v->susTime = fbNote;
	}
}

static inline float softClip(float x)
{
	if(x > 1.0f) return 2.0f / 3.0f;
	if(x < -1.0f) return -2.0f / 3.0f;
	return x - (x * x * x) / 3.0f;
}

/* ================= NeoTrellis: seesaw over /dev/i2c-1 ================= */

#define SS_STATUS       0x00
#define SS_STATUS_SWRST 0x7F
#define SS_STATUS_HW_ID 0x01
#define SS_NEOPIX       0x0E
#define SS_KEYPAD       0x10
#define SS_KEY_EVENT    0x01
#define SS_KEY_COUNT    0x04
#define SS_KEY_FIFO     0x10
#define NPIX 16
#define LED_BRIGHTNESS 0.25f  // overall grid dimmer - was blinding at full tilt
#define TRELLIS_ADDR 0x2E
#define TRELLIS_BUS "/dev/i2c-1"

#define PAD_PITCH    0   // 1  Pitch
#define PAD_FM       1   // 2  FM
#define PAD_FORMANT  2   // 3  Formant
#define PAD_SINE     3   // 4  Synth
#define PAD_VIB      4   // 5  Vibrato
#define PAD_CHORUS   5   // 6  Chorus
#define PAD_ENVF     6   // 7  Autowah
#define PAD_FWAH     7   // 8  Formant-Wah  (swapped with POG)
#define PAD_PLUCKSAG 8   // 9  Pluck Sag   (swapped with Resonator)
#define PAD_POG      9   // 10 POG Swell    (swapped with Formant-Wah)
#define PAD_RES      10  // 11 Resonator   (swapped with Pluck Sag)
#define PAD_SUS      11  // 12 Sustainer
#define PAD_JAWARI   12  // 13 Jawari Buzz
#define PAD_SYMP     13  // 14 Sympathetic  (moved down; Ney removed)
#define PAD_SITAR    14  // 15 SITAR TONE (jawari waveguide)
#define PAD_PAN      15  // 16 Stereo Spread / dynamic panning

static int gI2cFd = -1;
static int gTrellisOk = 0;
static float gPadFlash[NPIX];
static float gLavaT = 0.0f;
static uint8_t gGammaLUT[256];
static AuxiliaryTask gTrellisTask;

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

// Recomputes gBetaBase[]/dry-wet from the live encoder-set pitch state.
// Called on pad press (on/off) and on every encoder move while armed.
static void applyPitch(void)
{
	if(!gPitchActive) {
		gDryTarget = 1.0f;
		gWetTarget = 0.0f;
		return;
	}
	// cents is always a fine-tune layered ON TOP of the semitone value -
	// clicking into "cents mode" just changes what the main knob nudges
	float semis = gPitchSemitones + gPitchCents / 100.0f;
	float beta = powf(2.0f, semis / 12.0f);
	for(int s = 0; s < NUM_STRINGS; s++)
		gBetaBase[s] = beta;
	float wet = gPitchWetPct / 100.0f;
	gWetTarget = wet;
	gDryTarget = 1.0f - wet;
}

static void applyFormant(void)
{
	float f = gFormantActive ? gFormantVal : 1.0f;
	for(int s = 0; s < NUM_STRINGS; s++)
		gFormantBase[s] = f;
}

// PRESET MODE: one-touch vibes. 16 pads, each independently toggleable
// EXCEPT three families that share a single global parameter set (so
// selecting a new member of the family necessarily replaces the old
// one) - formant showcase (0-3), pitch shift (4-7), and the vib/chorus
// radio pair (8/9) the owner asked for explicitly. Re-selecting the
// SAME pad in a family turns that family off (radio-with-off).
static void applyPitchPreset(int pad, float semis, float wetPct)
{
	if(gPresetPitchSel == pad) {
		gPitchActive = 0;
		gPresetPitchSel = -1;
	} else {
		gPitchActive = 1;
		gPresetPitchSel = pad;
		gPitchSemitones = semis;
		gPitchCents = 0.0f;
		gPitchCentsMode = 0;
		gPitchWetPct = wetPct;
	}
	applyPitch();
}

// LOCKED-DOWN starting sensitivity for the auto-dynamics presets (14/15)
// - out of the box these were noise-triggery (too sensitive) with a low,
// mushy Q; a preset should sound great immediately. Still fully
// encoder-adjustable afterward, same as manual mode.
#define PRESET_ENVF_SENS 1.2f
#define PRESET_ENVF_Q    1.4f
#define PRESET_FWAH_SENS 1.2f

static void applyPreset(int pad)
{
	switch(pad) {
	case 0: case 1: case 2: case 3: {
		/*
		 * ============ ROUND 1 - DO NOT DELETE, KEPT FOR REFERENCE ============
		 * GIANT, SPRITE, TALKBOX, RUBBER. Root cause of "feels like auto
		 * wah / dynamics reactive" turned out to be a SHARED mechanism,
		 * not these presets themselves: an onset gate elsewhere in
		 * updateModulation forced formantBlend to 0 through every pick's
		 * attack window, then crossfaded it back in - every preset swelled
		 * in after the attack instead of being present at the pick, which
		 * reads exactly like an envelope-follower effect. That gate has
		 * been removed (see the formantBlend block above). Values below
		 * preserved in case round 1's per-preset behaviors (dynamics-
		 * reactive SPRITE attempt, TALKBOX wobble, RUBBER sag-link) are
		 * wanted again later - do not delete, just inactive.
		 *
		 * static const float FVAL[4]    = { 0.62f, 1.55f, 0.85f, 0.70f };
		 * static const float FGLIDE[4]  = { 0.025f, 0.12f, 0.05f, 0.035f };
		 * static const float TILTBIAS[4] = { 0.4f, 0.0f, 1.0f, 0.5f };
		 * if(gPresetFormantSel == pad) {
		 *     gFormantActive = 0;
		 *     gPresetFormantSel = -1;
		 *     gFormantTiltBias = 0.0f;
		 *     gPresetFormantWobble = 0;
		 * } else {
		 *     gFormantActive = 1;
		 *     gPresetFormantSel = pad;
		 *     gFormantVal = FVAL[pad];
		 *     gFormantGlide = FGLIDE[pad];
		 *     gFormantTiltBias = TILTBIAS[pad];
		 *     gPresetFormantWobble = (pad == 2);  // TALKBOX
		 *     if(gSagOn && pad == 0) {        // GIANT: slow, deep sag
		 *         gSagDepthSec = 0.008f;
		 *         gSagSettleSec = 0.15f;
		 *     } else if(gSagOn && pad == 3) { // RUBBER: quick, springy sag
		 *         gSagDepthSec = 0.005f;
		 *         gSagSettleSec = 0.05f;
		 *     }
		 * }
		 * ======================================================================
		 *
		 * ============ ROUND 2 - CONFIRMED GOOD, DO NOT DELETE ============
		 * Deliberately EVEN: same fast glide, zero tilt bias, zero
		 * wobble, zero sag-linking for all four - no per-preset special-
		 * cased behavior, because that behavior (plus the onset gate,
		 * now removed above) is what made round 1 read as dynamics-
		 * reactive. Four clean, distinct, static formant colors, present
		 * immediately on the pick - H90-style, not auto-wah-style.
		 * Moderate spread (0.65-1.50), fast snap-in glide (0.20). User-
		 * confirmed good - keep this exact block if reverting is ever
		 * needed.
		 *
		 * static const float FVAL[4]   = { 0.65f, 0.85f, 1.20f, 1.50f };
		 * static const float FGLIDE[4] = { 0.20f, 0.20f, 0.20f, 0.20f };
		 * (same if/else structure as round 3 below, just these values)
		 * ===================================================================
		 *
		 * ROUND 3 - live now. Same proven discipline as round 2 (static,
		 * zero tilt bias, zero wobble, zero sag-linking) but chillier:
		 * ratios pulled in closer to unity (0.75-1.30, a gentler spread
		 * than round 2's 0.65-1.50) and glide slowed way down (silky
		 * ~90-160ms settle instead of round 2's snappy ~15ms) for a more
		 * relaxed, langorous character. Still fully static - no fade-in,
		 * no dynamics coupling, present at the pick, just eased in softer.
		 */
		static const float FVAL[4]   = { 0.75f, 0.90f, 1.10f, 1.30f };
		static const float FGLIDE[4] = { 0.06f, 0.08f, 0.10f, 0.07f };
		if(gPresetFormantSel == pad) {
			gFormantActive = 0;
			gPresetFormantSel = -1;
			gFormantTiltBias = 0.0f;
			gPresetFormantWobble = 0;
		} else {
			gFormantActive = 1;
			gPresetFormantSel = pad;
			gFormantVal = FVAL[pad];
			gFormantGlide = FGLIDE[pad];
			gFormantTiltBias = 0.0f;
			gPresetFormantWobble = 0;
		}
		applyFormant();
		break;
	}
	case 4: applyPitchPreset(pad, -12.0f, 100.0f); break;  // BASS
	case 5: applyPitchPreset(pad,  -5.0f,  50.0f); break;
	case 6: applyPitchPreset(pad,  -7.0f,  50.0f); break;
	case 7: applyPitchPreset(pad,  12.0f, 100.0f); break;
	case 8:  // VIBRATO (radio w/ chorus)
		if(gPresetVibChorusSel == 0) {
			gVibOn = 0; gPresetVibChorusSel = -1;
		} else {
			gVibOn = 1; gChorusOn = 0; gPresetVibChorusSel = 0;
		}
		break;
	case 9:  // CHORUS (radio w/ vibrato)
		if(gPresetVibChorusSel == 1) {
			gChorusOn = 0; gPresetVibChorusSel = -1;
		} else {
			gChorusOn = 1; gVibOn = 0; gPresetVibChorusSel = 1;
		}
		break;
	case 10: gSusOn = !gSusOn; break;      // SUSTAINER
	case 11: gSineOn = !gSineOn; break;    // SYNTH
	case 12: gSagOn = !gSagOn; break;      // PLUCK SAG ("bends as you play")
	case 13:                               // AUTOWAH, locked down
		gEnvFOn = !gEnvFOn;
		if(gEnvFOn) { gEnvfSens = PRESET_ENVF_SENS; gEnvfQ = PRESET_ENVF_Q; }
		break;
	case 14:                               // FORMANT-WAH (auto), locked down
		gFWahOn = !gFWahOn;
		if(gFWahOn) gFWahSens = PRESET_FWAH_SENS;
		break;
	case 15: gSympOn = !gSympOn; break;    // SYMPATHETIC RESONANCE
	}
}

/*
 * PERSISTED STATE: remember every toggle/parameter across power cycles.
 * The board has no clean-shutdown signal (it's USB-powered - the cable
 * just gets yanked), so there's no "save on shutdown" hook to catch.
 * Instead: autosave a few seconds after anything actually changes
 * (trellisTick, throttled - see gStateDirty), and load it back at boot.
 * Write-to-temp-then-rename makes each save atomic, so a power cut
 * mid-write can never leave a half-written, corrupt state file behind -
 * worst case you lose only the last few seconds of tweaking.
 */
#define STATE_FILE "/root/Bela/projects/hyperGuitar6chPan/hyperguitar-state.bin"
#define STATE_MAGIC 0x48475331u /* "HGS1" */

typedef struct {
	uint32_t magic;
	int presetMode, presetFormantSel, presetPitchSel, presetVibChorusSel;
	int presetFormantDynamic, presetFormantWobble;
	int pitchActive, formantActive, fmActive;
	int vibOn, chorusOn, envFOn, fWahOn, sineOn, pogOn, resOn, susOn;
	int jawOn, sitarOn, panOn, sagOn, sympOn;
	float pitchSemitones, pitchCents; int pitchCentsMode; float pitchWetPct;
	float formantVal, formantGlide, formantTiltBias;
	float fmRatioVal, fmIndexVal; int fmQuantized;
	float vibDepthSec, vibRateScale;
	float chorDepthSec, chorRateScale;
	float envfSens, envfQ, fWahSens;
	float pogAttackSec, pogModDepth;
	float resSens, resFb, resGain;
	float susFb, susDamp, susLevel;
	float jawBuzz, jawPitch, jawSens;
	float sitarRing, sitarLevel, sitarBuzz;
	float sagDepthSec, sagSettleSec;
	float sympCoupling, sympFb;
	float panSpread, panDyn;
	int synthMode, synthPoly; float synthLevel, synthGateThr;
} PersistedState;

static void saveState(void)
{
	PersistedState st;
	memset(&st, 0, sizeof(st));
	st.magic = STATE_MAGIC;
	st.presetMode = gPresetMode;
	st.presetFormantSel = gPresetFormantSel;
	st.presetPitchSel = gPresetPitchSel;
	st.presetVibChorusSel = gPresetVibChorusSel;
	st.presetFormantDynamic = 0; // retired field, kept for struct layout
	st.presetFormantWobble = gPresetFormantWobble;
	st.pitchActive = gPitchActive; st.formantActive = gFormantActive;
	st.fmActive = gFmActive;
	st.vibOn = gVibOn; st.chorusOn = gChorusOn; st.envFOn = gEnvFOn;
	st.fWahOn = gFWahOn; st.sineOn = gSineOn; st.pogOn = gPogOn;
	st.resOn = gResOn; st.susOn = gSusOn; st.jawOn = gJawOn;
	st.sitarOn = gSitarOn; st.panOn = gPanOn; st.sagOn = gSagOn;
	st.sympOn = gSympOn;
	st.pitchSemitones = gPitchSemitones; st.pitchCents = gPitchCents;
	st.pitchCentsMode = gPitchCentsMode; st.pitchWetPct = gPitchWetPct;
	st.formantVal = gFormantVal; st.formantGlide = gFormantGlide;
	st.formantTiltBias = gFormantTiltBias;
	st.fmRatioVal = gFmRatioVal; st.fmIndexVal = gFmIndexVal;
	st.fmQuantized = gFmQuantized;
	st.vibDepthSec = gVibDepthSec; st.vibRateScale = gVibRateScale;
	st.chorDepthSec = gChorDepthSec; st.chorRateScale = gChorRateScale;
	st.envfSens = gEnvfSens; st.envfQ = gEnvfQ; st.fWahSens = gFWahSens;
	st.pogAttackSec = gPogAttackSec; st.pogModDepth = gPogModDepth;
	st.resSens = gResSens; st.resFb = gResFb; st.resGain = gResGain;
	st.susFb = gSusFb; st.susDamp = gSusDamp; st.susLevel = gSusLevel;
	st.jawBuzz = gJawBuzz; st.jawPitch = gJawPitch; st.jawSens = gJawSens;
	st.sitarRing = gSitarRing; st.sitarLevel = gSitarLevel;
	st.sitarBuzz = gSitarBuzz;
	st.sagDepthSec = gSagDepthSec; st.sagSettleSec = gSagSettleSec;
	st.sympCoupling = gSympCoupling; st.sympFb = gSympFb;
	st.panSpread = gPanSpread; st.panDyn = gPanDyn;
	st.synthMode = gSynthMode; st.synthPoly = gSynthPoly;
	st.synthLevel = gSynthLevel; st.synthGateThr = gSynthGateThr;

	FILE *f = fopen(STATE_FILE ".tmp", "wb");
	if(!f)
		return;
	fwrite(&st, sizeof(st), 1, f);
	fclose(f);
	rename(STATE_FILE ".tmp", STATE_FILE);
}

static void loadState(void)
{
	FILE *f = fopen(STATE_FILE, "rb");
	if(!f)
		return;                      // first boot ever - keep defaults
	PersistedState st;
	memset(&st, 0, sizeof(st));
	size_t n = fread(&st, sizeof(st), 1, f);
	fclose(f);
	if(n != 1 || st.magic != STATE_MAGIC)
		return;                      // no file, or an old/foreign format

	gPresetMode = st.presetMode;
	gPresetFormantSel = st.presetFormantSel;
	gPresetPitchSel = st.presetPitchSel;
	gPresetVibChorusSel = st.presetVibChorusSel;
	gPresetFormantWobble = st.presetFormantWobble;
	gPitchActive = st.pitchActive; gFormantActive = st.formantActive;
	gFmActive = st.fmActive;
	gVibOn = st.vibOn; gChorusOn = st.chorusOn; gEnvFOn = st.envFOn;
	gFWahOn = st.fWahOn; gSineOn = st.sineOn; gPogOn = st.pogOn;
	gResOn = st.resOn; gSusOn = st.susOn; gJawOn = st.jawOn;
	gSitarOn = st.sitarOn; gPanOn = st.panOn; gSagOn = st.sagOn;
	gSympOn = st.sympOn;
	gPitchSemitones = st.pitchSemitones; gPitchCents = st.pitchCents;
	gPitchCentsMode = st.pitchCentsMode; gPitchWetPct = st.pitchWetPct;
	gFormantVal = st.formantVal; gFormantGlide = st.formantGlide;
	gFormantTiltBias = st.formantTiltBias;
	gFmRatioVal = st.fmRatioVal; gFmIndexVal = st.fmIndexVal;
	gFmQuantized = st.fmQuantized;
	gVibDepthSec = st.vibDepthSec; gVibRateScale = st.vibRateScale;
	gChorDepthSec = st.chorDepthSec; gChorRateScale = st.chorRateScale;
	gEnvfSens = st.envfSens; gEnvfQ = st.envfQ; gFWahSens = st.fWahSens;
	gPogAttackSec = st.pogAttackSec; gPogModDepth = st.pogModDepth;
	gResSens = st.resSens; gResFb = st.resFb; gResGain = st.resGain;
	gSusFb = st.susFb; gSusDamp = st.susDamp; gSusLevel = st.susLevel;
	gJawBuzz = st.jawBuzz; gJawPitch = st.jawPitch; gJawSens = st.jawSens;
	gSitarRing = st.sitarRing; gSitarLevel = st.sitarLevel;
	gSitarBuzz = st.sitarBuzz;
	gSagDepthSec = st.sagDepthSec; gSagSettleSec = st.sagSettleSec;
	gSympCoupling = st.sympCoupling; gSympFb = st.sympFb;
	gPanSpread = st.panSpread; gPanDyn = st.panDyn;
	gSynthMode = st.synthMode; gSynthPoly = st.synthPoly;
	gSynthLevel = st.synthLevel; gSynthGateThr = st.synthGateThr;
}

/*
 * applyEncoders: routes live main/aux encoder deltas + click flags to
 * whichever pad is currently "armed" (gArmedPad, set by the last pad
 * press in trellisTick). Synth (mono/poly + timbre) is the only
 * deliberately-deferred pad left - its redesign is out of scope here.
 */
static void applyEncoders(int pad, int mainDelta, int auxDelta, int mainClick, int auxClick)
{
	switch(pad) {
	case PAD_ENVF:
		gEnvfSens += mainDelta * 0.05f;
		if(gEnvfSens < 0.2f) gEnvfSens = 0.2f;
		if(gEnvfSens > 6.0f) gEnvfSens = 6.0f;
		gEnvfQ += auxDelta * 0.1f;
		if(gEnvfQ < 0.3f) gEnvfQ = 0.3f;
		if(gEnvfQ > 8.0f) gEnvfQ = 8.0f;
		break;
	case PAD_RES:
		gResSens += mainDelta * 0.06f;    // MAIN: how far dynamics push the harmonic
		if(gResSens < 0.2f) gResSens = 0.2f;
		if(gResSens > 6.0f) gResSens = 6.0f;
		gResFb += auxDelta * 0.004f;      // AUX: how long the overtone rings
		if(gResFb < 0.80f) gResFb = 0.80f;
		if(gResFb > 0.998f) gResFb = 0.998f;
		break;
	case PAD_VIB:
		gVibRateScale += mainDelta * 0.02f;
		if(gVibRateScale < 0.2f) gVibRateScale = 0.2f;
		if(gVibRateScale > 3.0f) gVibRateScale = 3.0f;
		gVibDepthSec += auxDelta * 0.0002f;
		if(gVibDepthSec < 0.0f) gVibDepthSec = 0.0f;
		if(gVibDepthSec > 0.005f) gVibDepthSec = 0.005f;
		break;
	case PAD_CHORUS:
		gChorRateScale += mainDelta * 0.02f;
		if(gChorRateScale < 0.2f) gChorRateScale = 0.2f;
		if(gChorRateScale > 3.0f) gChorRateScale = 3.0f;
		gChorDepthSec += auxDelta * 0.0003f;
		if(gChorDepthSec < 0.0f) gChorDepthSec = 0.0f;
		if(gChorDepthSec > 0.008f) gChorDepthSec = 0.008f;
		break;
	case PAD_POG:
		gPogAttackSec += mainDelta * 0.05f;
		if(gPogAttackSec < 0.02f) gPogAttackSec = 0.02f;
		if(gPogAttackSec > 3.0f) gPogAttackSec = 3.0f;
		gPogModDepth += auxDelta * 0.1f;
		if(gPogModDepth < -3.0f) gPogModDepth = -3.0f;
		if(gPogModDepth > 3.0f) gPogModDepth = 3.0f;
		break;
	case PAD_PLUCKSAG:
		gSagDepthSec += mainDelta * 0.0002f;
		if(gSagDepthSec < 0.0f) gSagDepthSec = 0.0f;
		if(gSagDepthSec > 0.01f) gSagDepthSec = 0.01f;
		gSagSettleSec += auxDelta * 0.01f;
		if(gSagSettleSec < 0.01f) gSagSettleSec = 0.01f;
		if(gSagSettleSec > 0.5f) gSagSettleSec = 0.5f;
		break;
	case PAD_SYMP:
		gSympCoupling += mainDelta * 0.06f;  // MAIN: how hard the others ring it
		if(gSympCoupling < 0.0f) gSympCoupling = 0.0f;
		if(gSympCoupling > 4.0f) gSympCoupling = 4.0f;
		gSympFb += auxDelta * 0.003f;        // AUX: ring length
		if(gSympFb < 0.90f) gSympFb = 0.90f;
		if(gSympFb > 0.9990f) gSympFb = 0.9990f;
		break;
	case PAD_SUS:
		gSusFb += mainDelta * 0.002f;     // MAIN: hold time (top = infinite)
		if(gSusFb < 0.90f) gSusFb = 0.90f;
		if(gSusFb > 0.9995f) gSusFb = 0.9995f;
		gSusDamp += auxDelta * 0.02f;     // AUX: tone of the held note
		if(gSusDamp < 0.03f) gSusDamp = 0.03f;
		if(gSusDamp > 0.90f) gSusDamp = 0.90f;
		break;
	case PAD_JAWARI:
		gJawBuzz += mainDelta * 0.05f;    // MAIN: buzz amount
		if(gJawBuzz < 0.0f) gJawBuzz = 0.0f;
		if(gJawBuzz > 2.0f) gJawBuzz = 2.0f;
		gJawPitch += auxDelta * 6.0f;     // AUX: metallic pitch of the rattle
		if(gJawPitch < 16.0f) gJawPitch = 16.0f;
		if(gJawPitch > 400.0f) gJawPitch = 400.0f;
		break;
	case PAD_SITAR:
		gSitarLevel += mainDelta * 0.02f;   // MAIN: wet fraction (dry stays 100%)
		if(gSitarLevel < 0.0f) gSitarLevel = 0.0f;
		if(gSitarLevel > 1.0f) gSitarLevel = 1.0f;
		gSitarRing += auxDelta * 0.0002f;   // AUX: tarab ring length
		if(gSitarRing < 0.9950f) gSitarRing = 0.9950f;
		if(gSitarRing > 0.9998f) gSitarRing = 0.9998f;
		if(mainClick) {                      // click: more/less jawari buzz
			gSitarBuzz = (gSitarBuzz > 0.9f) ? 0.2f : gSitarBuzz + 0.35f;
			rt_printf("SITAR buzz %.2f\n", gSitarBuzz);
		}
		break;
	case PAD_FWAH:
		gFWahSens += mainDelta * 0.08f;       // MAIN: how hard the vowel swings
		if(gFWahSens < 0.2f) gFWahSens = 0.2f;
		if(gFWahSens > 8.0f) gFWahSens = 8.0f;
		break;
	case PAD_PAN:
		gPanSpread += mainDelta * 0.05f;      // MAIN: spread (0 = mono)
		if(gPanSpread < 0.0f) gPanSpread = 0.0f;
		if(gPanSpread > 2.0f) gPanSpread = 2.0f;
		gPanDyn += auxDelta * 0.04f;          // AUX: dynamic fly-out
		if(gPanDyn < 0.0f) gPanDyn = 0.0f;
		if(gPanDyn > 1.0f) gPanDyn = 1.0f;
		break;
	case PAD_PITCH:
		if(mainClick) {
			gPitchCentsMode = !gPitchCentsMode;
			rt_printf("pitch mode: %s\n", gPitchCentsMode ? "cents" : "semitone");
		}
		if(gPitchCentsMode) {
			gPitchCents += (float)mainDelta * 2.0f; // 2 cents/detent, +-50
			if(gPitchCents < -50.0f) gPitchCents = -50.0f;
			if(gPitchCents > 50.0f) gPitchCents = 50.0f;
		} else {
			gPitchSemitones += (float)mainDelta; // 1 semitone/detent
			if(gPitchSemitones < -12.0f) gPitchSemitones = -12.0f;
			if(gPitchSemitones > 12.0f) gPitchSemitones = 12.0f;
		}
		gPitchWetPct += (float)auxDelta * 10.0f; // 10%/detent
		if(gPitchWetPct < 0.0f) gPitchWetPct = 0.0f;
		if(gPitchWetPct > 100.0f) gPitchWetPct = 100.0f;
		applyPitch();
		break;
	case PAD_FM:
		if(mainClick) {
			gFmQuantized = !gFmQuantized;
			rt_printf("fm mode: %s\n", gFmQuantized ? "quantized" : "free");
		}
		if(gFmQuantized) {
			static const float qSteps[6] = { 0.25f, 0.5f, 1.0f, 2.0f, 3.0f, 4.0f };
			int idx = 0;
			float best = 1e9f;
			for(int k = 0; k < 6; k++) {
				float d = fabsf(qSteps[k] - gFmRatioVal);
				if(d < best) { best = d; idx = k; }
			}
			idx += mainDelta;
			if(idx < 0) idx = 0;
			if(idx > 5) idx = 5;
			gFmRatioVal = qSteps[idx];
		} else {
			gFmRatioVal += (float)mainDelta * 0.05f;
			if(gFmRatioVal < 0.1f) gFmRatioVal = 0.1f;
			if(gFmRatioVal > 8.0f) gFmRatioVal = 8.0f;
		}
		gFmIndexVal += (float)auxDelta * 0.05f;
		if(gFmIndexVal < 0.02f) gFmIndexVal = 0.02f;
		if(gFmIndexVal > 3.0f) gFmIndexVal = 3.0f;
		break;
	case PAD_FORMANT:
		gFormantVal += (float)mainDelta * 0.02f;   // MAIN: vowel/filter position
		if(gFormantVal < 0.4f) gFormantVal = 0.4f;
		if(gFormantVal > 2.2f) gFormantVal = 2.2f;
		applyFormant();
		gFormantGlide += (float)auxDelta * -0.01f;  // AUX: glide speed
		if(gFormantGlide < 0.01f) gFormantGlide = 0.01f;  // ~290ms, silkiest
		if(gFormantGlide > 0.35f) gFormantGlide = 0.35f;  // ~7ms, snappiest
		break;
	case PAD_SINE:
		if(mainClick) {
			gSynthPoly = !gSynthPoly;
			rt_printf("SYNTH: %s\n", gSynthPoly ? "POLY (per-string)" : "MONO (loudest string)");
		}
		if(mainDelta) {
			int m = gSynthMode + mainDelta;
			while(m < 0) m += SYNTH_NMODES;
			m %= SYNTH_NMODES;
			gSynthMode = m;
			rt_printf("SYNTH mode %d = %s\n", m, SYNTH_NAMES[m]);
		}
		gSynthLevel += (float)auxDelta * 0.05f;
		if(gSynthLevel < 0.0f) gSynthLevel = 0.0f;
		if(gSynthLevel > 2.5f) gSynthLevel = 2.5f;
		// (release-time control removed - amplitude now follows the real
		// string envelope, so there's no artificial release to adjust)
		break;
	default:
		break;
	}
	gStateDirty = 1;
}

static void trellisTick(void *)
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

static int trellisInit(void)
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

/* ======================= Bela lifecycle ======================= */

bool setup(BelaContext *context, void *userData)
{
	float sr = context->audioSampleRate;
	rt_printf("hyperGuitar v5: in %d ch, sr %.0f, block %d\n",
			context->audioInChannels, sr, context->audioFrames);

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

		v->sdHiThresh = 1.15f * v->p0 + 128.0f;
		v->sdDelay = 0.5f * v->sdHiThresh;
		v->up.lastAnchorDelay = 128.0f;
		v->up.launchCd = (float)(s * 7);
		v->fsWet.lastAnchorDelay = 128.0f;
		v->fsWet.launchCd = (float)(s * 7 + 2);
		v->fsDry.lastAnchorDelay = 128.0f;
		v->fsDry.launchCd = (float)(s * 7 + 4);

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
		float root = 26.0f * OPEN_HZ[0];  // ~1802 Hz on a Db low string -
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

	if(trellisInit() == 0) {
		gTrellisOk = 1;
		rt_printf("NeoTrellis v6: 0 pitch|1 fm|2 formant|3 sine|4 vib|5 chor|"
				"6 wah|7 pog|8 res|9 fwah|10 plucksag|11 sus|12 jawari|13 ney|"
				"14 helm|15 sympathetic + 2 GPIO encoders (arm-on-press)\n");
	} else {
		rt_printf("NeoTrellis not found - audio runs, no UI\n");
	}
	gTrellisTask = Bela_createAuxiliaryTask(trellisTick, 50, "trellis");
	srand(12345);
	return true;
}

void render(BelaContext *context, void *userData)
{
	static unsigned int ledDivider = 0;
	unsigned int ledPeriod =
		(unsigned int)(context->audioSampleRate / 30.0f / context->audioFrames);
	if(gTrellisOk && ++ledDivider >= ledPeriod) {
		ledDivider = 0;
		Bela_scheduleAuxiliaryTask(gTrellisTask);
	}

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
			gEnc1Idle = sw1; gEnc1BtnStable = sw1;
			gEnc2Idle = sw2; gEnc2BtnStable = sw2;
			gSwInit = 1;
		}
		if(gEnc1BtnHold > 0) gEnc1BtnHold--;
		else if(sw1 != gEnc1BtnStable) {
			gEnc1BtnStable = sw1;
			gEnc1BtnHold = 300;
			if(sw1 != gEnc1Idle) gEnc1Click = 1; // moved AWAY from idle = press
		}
		if(gEnc2BtnHold > 0) gEnc2BtnHold--;
		else if(sw2 != gEnc2BtnStable) {
			gEnc2BtnStable = sw2;
			gEnc2BtnHold = 300;
			if(sw2 != gEnc2Idle) gEnc2Click = 1;
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
	 * BOTH-ENCODER CLICK: toggle PRESET <-> MANUAL mode. Doesn't require
	 * true same-block simultaneity (near-impossible for a human) - each
	 * click arms a short latch (~330ms at this block rate); if the OTHER
	 * button's click lands while that latch is live, it's a combo. Single
	 * clicks pass through to applyEncoders exactly as before (zero added
	 * latency) - the combo is purely an ADDITIONAL check layered on top.
	 */
	if(gComboLatch1 > 0) gComboLatch1--;
	if(gComboLatch2 > 0) gComboLatch2--;
	int comboFire = (mainClick && auxClick)
	             || (mainClick && gComboLatch2 > 0)
	             || (auxClick && gComboLatch1 > 0);
	if(mainClick) gComboLatch1 = 15;
	if(auxClick)  gComboLatch2 = 15;
	if(comboFire) {
		gPresetMode = !gPresetMode;
		gModeFlash = 8;             // ~270ms white flash at ~30Hz LED rate
		gComboLatch1 = gComboLatch2 = 0;
		rt_printf("MODE: %s\n", gPresetMode ? "PRESET" : "MANUAL");
	}

	if(!gPresetMode && gArmedPad >= 0 && (mainDelta || auxDelta || mainClick || auxClick))
		applyEncoders(gArmedPad, mainDelta, auxDelta, mainClick, auxClick);

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
	 * SITAR TARAB RETUNE: was rooted at a FIXED ~1800Hz (26x the open low
	 * string) no matter what's played - completely unrelated to the
	 * note, so it read as generic high hiss instead of sitar sympathetic
	 * ring. Now tracks the loudest STABLE string once per block and
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
	 * SYMPATHETIC COUPLING WEIGHTS (per block): real sitar physics. Each
	 * string s has a resonator tuned to its OPEN pitch (harmonics k*f0s).
	 * A played string o only rings it when one of o's partials m*fo lands
	 * near one of those harmonics - i.e. m*(fo/f0s) is close to an
	 * integer. Weight each o->s injection by that coincidence instead of
	 * the old flat sum: consonant notes bloom, unrelated notes stay out
	 * of the mud. Small floor keeps the pluck click faintly exciting
	 * everything, like real tarab strings.
	 */
	/*
	 * CROSSTALK REFERENCE (per block): hex pickups bleed ~-20dB between
	 * channels. When a string's own note has decayed, the loudest thing
	 * left in its channel is often ANOTHER string - the tracker locks
	 * onto the bleed and the synth warbles to a wrong note as it dies.
	 * The synth gate refuses to OPEN a string whose level is dwarfed by
	 * the rest of the guitar (already-sounding notes are never cut).
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

		for(int s = 0; s < NUM_STRINGS; s++) {
			Voice *v = &gV[s];
			float dry = dryIn[s];

			if(--v->hopCountdown <= 0) {
				trackPitch(v);
				updateModulation(v, s);
				updateLP(v);
				v->hopCountdown = HOP;
			}

			float wet, dryLeg;
			voiceProcess(v, dry, gDryS > 0.01f, &wet, &dryLeg);
			float mixed = dryLeg * gDryS + wet * gWetS;

			// FM: phase-modulate the ACTUAL string signal (index 2,
			// modulator = sine at ratio x tracked f0)
			v->fmbuf[v->fmWrite & VBUF_MASK] = mixed;
			if(gFmAmtS > 0.002f && gFmActive) {
				float ratio = gFmRatioVal;
				v->fmPhase += 2.0f * (float)M_PI * ratio / v->period;
				if(v->fmPhase > 2.0f * (float)M_PI)
					v->fmPhase -= 2.0f * (float)M_PI;
				float d = (v->fmA + 8.0f) + v->fmA * sinf(v->fmPhase);
				float fmOut = bufAtM(v->fmbuf, VBUF_MASK, v->fmWrite, d);
				mixed = mixed * (1.0f - gFmAmtS) + fmOut * gFmAmtS;
			}
			v->fmWrite++;

			// AUTOWAH
			if(gEnvFAmtS > 0.002f) {
				v->svfF += 0.01f * (v->svfFT - v->svfF);
				// dynamics-driven bite (Mu-Tron/Q-Tron style): a harder
				// pick tightens the resonance for more "quack", a quiet
				// note stays smoother - real envelope filters do this,
				// a static Q doesn't.
				float qDyn = gEnvfQ * (0.6f + 1.2f * fminf(v->env * 3.0f, 1.0f));
				float f = v->svfF, q = 1.0f / qDyn;
				v->svfLow += f * v->svfBand;
				float hi = mixed - v->svfLow - q * v->svfBand;
				v->svfBand += f * hi;
				mixed += gEnvFAmtS * (1.1f * v->svfLow - mixed);
			}

			// 11 RESONATOR: rings a harmonic of the note; dynamics choose which
			if(gResAmtS > 0.002f) {
				float re = v->env * gResSens;
				if(re > 1.0f) re = 1.0f;
				int harm = 2 + (int)(4.0f * re);      // 2nd .. 6th harmonic
				float dTarget = v->period / (float)harm;
				// dynamics crossing a harmonic boundary jumps this delay
				// by a large discrete step (e.g. 2nd->3rd harmonic) -
				// feeding that straight into a feedback comb is an
				// instant delay-length jump = an audible click/zip.
				// Smooth it; fast enough to feel immediate, slow enough
				// to not click.
				v->resDelaySm += 0.25f * (dTarget - v->resDelaySm);
				float ring = combRun(&v->resC, mixed * 0.35f, v->resDelaySm,
						gResFb, 0.12f);
				mixed += gResAmtS * gResGain * ring;
			} else {
				v->resC.buf[v->resC.wr & COMB_MASK] = 0.0f;
				v->resC.wr++; v->resC.lp = 0.0f;
			}

			// 13 JAWARI BUZZ: short metallic comb rattle, harder = buzzier.
			// ADDED on top, so the dry level is never touched or clipped.
			// The real thing is a NONLINEAR bridge contact, not a fixed
			// linear comb: soft-clip the excitation for the asymmetric
			// buzz character, and let feedback tighten on a hard pick
			// and loosen as the note settles (contact gets shallower as
			// the string calms down), instead of one constant feedback
			// regardless of dynamics.
			if(gJawAmtS > 0.002f) {
				float je = v->env * gJawSens;
				if(je > 1.0f) je = 1.0f;
				float exc = tanhf(mixed * (0.15f + 0.55f * je) * 1.3f) * (1.0f / 1.3f);
				float jfb = 0.86f + 0.08f * je;
				float ring = combRun(&v->jawC, exc, gJawPitch, jfb, 0.55f);
				mixed += gJawAmtS * gJawBuzz * 2.0f * je * ring;
			} else {
				v->jawC.buf[v->jawC.wr & COMB_MASK] = 0.0f;
				v->jawC.wr++; v->jawC.lp = 0.0f;
			}

			// 12 SUSTAINER: comb at the string's own period, near-unity
			// feedback - grabs the fundamental instantly and holds it.
			// v->susTime is the per-string feedback gain computed in
			// updateModulation() so every string decays over the SAME
			// wall-clock time for a given knob setting (a fixed raw gain
			// makes high strings - more loop iterations/sec at a shorter
			// period - decay several times faster than low strings for
			// the "same" sustain setting). Injection also scales with
			// pick strength so a hard dig-in rings harder than a scrape.
			if(gSusAmtS > 0.002f) {
				float inputLevel = 0.15f + 0.35f * fminf(v->env / 0.5f, 1.0f);
				float ring = combRun(&v->susC, mixed * inputLevel, v->period,
						v->susTime, gSusDamp);
				mixed += gSusAmtS * gSusLevel * ring;
			} else {
				v->susC.buf[v->susC.wr & COMB_MASK] = 0.0f;
				v->susC.wr++; v->susC.lp = 0.0f;
			}

			// 15 SITAR - part 1: the jawari "zing" on the played note. The
			// wide flat bridge throws energy into the high partials.
			if(gSitarAmtS > 0.002f) {
				v->sitarHp += 0.22f * (mixed - v->sitarHp);
				float bright = mixed - v->sitarHp;      // high band
				float zing = tanhf(bright * (1.0f + 6.0f * gSitarBuzz));
				// gated by gSitarLevel too - MAIN is now the ONE wet
				// control for the whole pad, zing included
				mixed += gSitarAmtS * gSitarLevel * gSitarBuzz * 1.3f * zing;
			}

			// 14 SYMPATHETIC: this string's comb is tuned to ITS OPEN pitch
			// (a sitar's tarab strings are tuned to fixed scale notes) and
			// is rung by the OTHER five strings' RAW signal - no feedback.
			if(gSympAmtS > 0.002f) {
				float inject = 0.0f;
				for(int o = 0; o < NUM_STRINGS; o++)
					if(o != s) inject += dryIn[o] * gSympW[s][o];
				// x2: the relatedness weights cut the average injection,
				// so consonant pairs now ring HARDER than the old flat
				// sum while unrelated ones nearly vanish
				inject *= 2.0f * gSympCoupling / (float)(NUM_STRINGS - 1);
				if(inject > 1.5f) inject = 1.5f;
				if(inject < -1.5f) inject = -1.5f;
				float ring = combRun(&v->sympC, inject * 0.5f, v->p0,
						gSympFb, 0.20f);
				mixed += gSympAmtS * 3.0f * ring;
			} else {
				v->sympC.buf[v->sympC.wr & COMB_MASK] = 0.0f;
				v->sympC.wr++; v->sympC.lp = 0.0f;
			}

			// PLUCK SAG: transient pitch spike-then-settle on note attack
			v->sagBuf[v->sagWrite & VBUF_MASK] = mixed;
			if(gSagAmtS > 0.002f) {
				float sagDepthSmp = gSagDepthSec * sr;
				float dly = 4.0f + sagDepthSmp * v->sagEnv;
				float sagged = bufAtM(v->sagBuf, VBUF_MASK, v->sagWrite, dly);
				mixed = mixed * (1.0f - gSagAmtS) + sagged * gSagAmtS;
			}
			v->sagWrite++;
			v->sagEnv *= v->sagDecayCoef;

			// VIBRATO (NEPTR unipolar sine sweep). Depth follows the
			// string's own envelope (floored, never fully off) so it
			// naturally eases as a note decays instead of wobbling a
			// dead/dying string at full throw - real player vibrato
			// tapers with the note too.
			v->vbuf[v->vWrite & VBUF_MASK] = mixed;
			float A = gVibAmtS * vibDepthSmp
					* (0.4f + 0.6f * fminf(v->env * 3.0f, 1.0f));
			float dly = 4.0f + A * (1.0f + sinf(v->vPhase));
			mixed = bufAtM(v->vbuf, VBUF_MASK, v->vWrite, dly);
			v->vWrite++;
			v->vPhase += 2.0f * (float)M_PI * VIB_HZ[s] * gVibRateScale / sr;
			if(v->vPhase > 2.0f * (float)M_PI)
				v->vPhase -= 2.0f * (float)M_PI;

			// POG SWELL: per-note ~100ms attack bloom
			if(v->pogGain < 1.0f) {
				v->pogGain += pogStep;
				if(v->pogGain > 1.0f) v->pogGain = 1.0f;
			}
			if(gPogAmtS > 0.002f) {
				// THE SWELL: volume envelope fading in over gPogAttackSec.
				// (This is the whole point of the effect - it got lost in
				// an earlier filter rewrite, which is why it was inaudible.)
				float pg = v->pogGain * v->pogGain; // perceptual curve
				float voiced;
				if(fabsf(gPogModDepth) > 0.05f) {
					// optional filter sweep riding the swell's timing
					float cutoffHz = 1200.0f * exp2f(gPogModDepth * v->pogGain);
					if(cutoffHz < 200.0f) cutoffHz = 200.0f;
					if(cutoffHz > 8000.0f) cutoffHz = 8000.0f;
					float fcoef = 2.0f * sinf((float)M_PI * cutoffHz / sr);
					if(fcoef > 1.0f) fcoef = 1.0f;
					v->pogLpZ += fcoef * (mixed - v->pogLpZ);
					voiced = v->pogLpZ * pg;
				} else {
					voiced = mixed * pg; // clean swell, no tone change
				}
				mixed = mixed * (1.0f - gPogAmtS) + voiced * gPogAmtS;
			}

			// CHORUS: subtle voice mirrored across the image (width).
			// Triangle LFO (not sine) - the classic analog chorus (Boss
			// CE-2/Roland CE-1) sweeps its delay with an integrator-
			// derived triangle, which throws pitch faster/more audibly
			// than a sine's slow-at-the-ends glide.
			v->cbuf[v->cWrite & VBUF_MASK] = mixed;
			float wide = 0.0f;
			if(gChorAmtS > 0.002f) {
				float ct = v->cPhase * (0.5f / (float)M_PI); // 0..1
				float tri = ct < 0.5f ? (4.0f * ct - 1.0f) : (3.0f - 4.0f * ct);
				float cd = chorBaseSmp + chorDepthSmp * (1.0f + tri);
				wide = bufAtM(v->cbuf, VBUF_MASK, v->cWrite, cd);
			}
			v->cWrite++;
			v->cPhase += 2.0f * (float)M_PI * CHOR_HZ[s] * gChorRateScale / sr;
			if(v->cPhase > 2.0f * (float)M_PI)
				v->cPhase -= 2.0f * (float)M_PI;

			// SYNTH POLY: this string drives its OWN oscillator at its own
			// tracked fundamental - six independent voices, true polyphony
			if(gSineAmtS > 0.002f && gSynthPoly) {
				/*
				 * GATE with HYSTERESIS: opening demands enough level, a
				 * confidently PITCHED signal, AND not being buried under
				 * bleed from the other strings. But once OPEN it only
				 * closes on LEVEL (at half the on-threshold) - requiring
				 * fresh confidence every block made one mid-note tracker
				 * dropout close and re-open the gate, and every re-open
				 * re-snapped pitch+velocity = the note-off blips.
				 */
				int on;
				if(v->pGateOn)
					on = v->env > 0.5f * gSynthGateThr;
				else
					on = (v->env > gSynthGateThr)
					  && (v->env > 0.35f * gOtherMax[s])
					  && (v->tonalHops >= SYNTH_TONAL_HOPS);
				int noteOn = on && !v->pGateOn;
				if(noteOn) {
					// NOTE-ON: SNAP straight to pitch. Gliding in from the
					// previous note is what made every entrance sound detuned.
					v->pInc = v->beta / v->period;
				}
				v->pGateOn = on;
				// AMPLITUDE = THE STRING'S OWN ENVELOPE. No held sustain
				// level, no fixed release fade - whatever the real string
				// is doing right now is what the synth does.
				float et = on ? v->env * SINE_ENV_SENS : 0.0f;
				if(et > 1.3f) et = 1.3f;
				v->pAmp += SYNTH_ENV_SLEW * (et - v->pAmp);
				if(on) {
					// held note: glide smoothly so BENDS work
					float targInc = v->beta / v->period;
					v->pInc += 0.02f * (targInc - v->pInc);
				}
				// gate closed: FREEZE the pitch - don't chase the garbage
				// the tracker reports while the note dies away.
				v->pLfo += 0.35f / v->srate;                 // ~0.35 Hz PWM sweep
				if(v->pLfo >= 1.0f) v->pLfo -= 1.0f;
				float lfo = sinf(2.0f * (float)M_PI * v->pLfo);
				float syn = synthOsc(gSynthMode, v->pPh, v->pInc, lfo,
						v->sFilt, v->srate, noteOn)
						* v->pAmp * gSynthLevel * SINE_LEVEL;
				mixed = mixed * (1.0f - gSineAmtS) + syn * gSineAmtS;
			}

			/*
			 * SAFETY SCRUB: recursive filters (and the sympathetic
			 * cross-string feedback path) can blow up or take on a NaN.
			 * A NaN in a biquad state is PERMANENT - it poisons every
			 * future sample, which is exactly the "fucked up noise until
			 * I restart" failure. Detect it and reset that string's
			 * filter states in place. Pure arithmetic: no added latency.
			 * (!(|x| < 16) is true for NaN as well as for runaway values.)
			 */
			if(!(fabsf(mixed) < 16.0f)) {
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
			}

			// feeds next sample's Sympathetic Resonance injection on the
			// OTHER five strings - captured after this string's full chain
			gLastMixed[s] = mixed;

#if MONO_OUT
			outL += mixed + gChorAmtS * CHOR_GAIN * wide;
			outR += mixed + gChorAmtS * CHOR_GAIN * wide;
#else
			// chorus voice takes the MIRRORED pan: per-string width
			outL += mixed * v->gainL + gChorAmtS * CHOR_GAIN * wide * v->gainR;
			outR += mixed * v->gainR + gChorAmtS * CHOR_GAIN * wide * v->gainL;
#endif
		}

		// 15 SITAR - part 2: the tarab bank itself. All 14 scale-tuned
		// resonators excited once per sample by the summed dry signal;
		// short damped rings so it pings bright and fast (real tarab
		// strings are thin/high and speak instantly, not a wash/echo).
		if(gSitarAmtS > 0.002f) {
			float exc = 0.0f;
			for(int s = 0; s < NUM_STRINGS; s++)
				exc += dryIn[s];
			exc *= 0.32f;
			if(exc > 1.5f) exc = 1.5f;
			if(exc < -1.5f) exc = -1.5f;
			float tl = 0.0f, tr = 0.0f;
			for(int k = 0; k < TARAB_N; k++) {
				float ring = combRun(&gTarab[k], exc, gTarabDelay[k],
						gSitarRing, 0.5f);
				tl += ring * (1.0f - gTarabPan[k]);
				tr += ring * gTarabPan[k];
			}
			// strip anything that drifted into the mid band ("wuuuhh") -
			// a 1-pole highpass around ~900 Hz so only the high vowelly
			// shimmer ("eeaaaiiiooowww") survives into the mix
			static float hpL = 0.0f, hpR = 0.0f;
			float hpCoef = 0.05f;  // lowpass tracker @ ~350 Hz; hi = tl - lp
			hpL += hpCoef * (tl - hpL);
			hpR += hpCoef * (tr - hpR);
			float hiL = tl - hpL;
			float hiR = tr - hpR;
			// fixed wet fraction ON TOP of the untouched dry signal -
			// dry is never attenuated, wet is just added in at gSitarLevel
			float g = gSitarAmtS * gSitarLevel;
			outL += g * hiL;
			outR += g * hiR;

			static float dbgPk = 0.0f, dbgExcPk = 0.0f;
			static int dbgCtr = 0;
			float wetPk = fabsf(g * hiL) > fabsf(g * hiR) ? fabsf(g * hiL) : fabsf(g * hiR);
			if(wetPk > dbgPk) dbgPk = wetPk;
			if(fabsf(exc) > dbgExcPk) dbgExcPk = fabsf(exc);
			if(++dbgCtr >= 22050) {
				rt_printf("SITAR-DBG amtS=%.3f exc_pk=%.4f wet_pk=%.4f\n",
						gSitarAmtS, dbgExcPk, dbgPk);
				dbgCtr = 0; dbgPk = 0.0f; dbgExcPk = 0.0f;
			}
		}

		// SYNTH MONO: one voice tracking the loudest string's fundamental,
		// snap-pitched and rock-solid. Same 5 timbres as poly (shared osc).
		if(gSineAmtS > 0.002f && !gSynthPoly) {
			Voice *lv = &gV[gLeadStr];
			// GATE: amplitude IS the tracked string envelope - see the
			// POLY voice above for why (no held sustain, no release fade).
			static int sLeadGate = 0;
			// same hysteresis as poly: confidence + bleed check only to
			// OPEN; once sounding, only a real level drop closes it
			int on;
			if(sLeadGate)
				on = lv->env > 0.5f * gSynthGateThr;
			else
				on = (lv->env > gSynthGateThr)
				  && (lv->env > 0.35f * gOtherMax[gLeadStr])
				  && (lv->tonalHops >= SYNTH_TONAL_HOPS);
			int noteOn = on && !sLeadGate;
			if(noteOn)
				gLeadInc = gLeadIncTgt;   // NOTE-ON: snap, never glide in
			sLeadGate = on;
			float et = on ? lv->env * SINE_ENV_SENS : 0.0f;
			if(et > 1.3f) et = 1.3f;
			gLeadAmp += SYNTH_ENV_SLEW * (et - gLeadAmp);
			if(on)
				gLeadInc += 0.02f * (gLeadIncTgt - gLeadInc);  // bends glide

			static float gLeadPh[6] = { 0, 0, 0, 0, 0, 0 };
			static float gLeadFilt[4] = { 0, 0, 0, 0 };
			static float gLeadLfo = 0.0f;
			gLeadLfo += 0.35f / sr;
			if(gLeadLfo >= 1.0f) gLeadLfo -= 1.0f;
			float lead = synthOsc(gSynthMode, gLeadPh, gLeadInc,
					sinf(2.0f * (float)M_PI * gLeadLfo),
					gLeadFilt, sr, noteOn);
			gLeadLPz += 0.55f * (lead - gLeadLPz); // bright tilt, ~6k
			lead = gLeadLPz * gLeadAmp * gSynthLevel * SINE_LEVEL;

			outL = outL * (1.0f - gSineAmtS) + lead * gSineAmtS;
			outR = outR * (1.0f - gSineAmtS) + lead * gSineAmtS;
		}

		outL *= MASTER_GAIN;
		outR *= MASTER_GAIN;

		float pk = fabsf(outL) > fabsf(outR) ? fabsf(outL) : fabsf(outR);
		gLimEnv = pk > gLimEnv ? pk : gLimEnv * 0.9998f;
		if(gLimEnv > LIM_CEIL) {
			float g = LIM_CEIL / gLimEnv;
			outL *= g;
			outR *= g;
		}

		float wL = softClip(outL), wR = softClip(outR);
		audioWrite(context, n, 0, wL);
		audioWrite(context, n, 1, wR);
		bridgePush(wL, wR);
	}
	gTimeSec += (double)context->audioFrames / context->audioSampleRate;
}

void cleanup(BelaContext *context, void *userData)
{
	if(gI2cFd >= 0)
		close(gI2cFd);
}
