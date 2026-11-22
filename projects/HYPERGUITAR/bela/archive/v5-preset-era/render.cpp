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

extern const float OPEN_HZ[NUM_STRINGS] =
	{ 69.30f, 92.50f, 123.47f, 164.81f, 207.65f, 277.18f };
extern const int STRING_CHANNEL[NUM_STRINGS] = { 9, 7, 6, 8, 4, 5 };
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
volatile int gSynthPoly = 0; // 0 = mono, 1 = poly
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

// STEREO SPREAD (pad 16)
float gPanSpread = 1.0f;
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
int gEnc1LastPos = 0, gEnc2LastPos = 0;
int gEnc1BtnStable = 1, gEnc2BtnStable = 1; // debounce state, 1=released
int gEnc1BtnHold = 0, gEnc2BtnHold = 0;     // debounce countdown
volatile int gEnc1Click = 0, gEnc2Click = 0; // one-shot flags, consumed then cleared
volatile int gArmedPad = -1; // which pad currently owns the encoders
volatile int gRawDiag = 0;   // set 1 to log raw input levels every second
volatile int gStateDirty = 0; // set on any change, autosaved & cleared in trellisTick
// PRESET <-> MANUAL mode (both-encoder-click toggle). Starts in PRESET.
volatile int gPresetMode = 1;
int gComboLatch1 = 0, gComboLatch2 = 0; // both-click combo detect
volatile int gModeFlash = 0;            // white-flash countdown (LED, ~30Hz)
int gPresetFormantSel = -1;   // pads 0-3
int gPresetPitchSel = -1;     // pads 4-7
int gPresetVibChorusSel = -1; // 0=vib(pad8), 1=chorus(pad9), -1=off
int gSwInit = 0, gEnc1Idle = 1, gEnc2Idle = 1;
float gLimEnv = 0.0f;
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

		// 15 SITAR - part 2: the tarab bank itself.
		runSitarTarab(dryIn, &outL, &outR);

		// SYNTH MONO
		runSynthMono(&outL, &outR, sr);

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
		gSampleStamp++;
	}
	gTimeSec += (double)context->audioFrames / context->audioSampleRate;
}

void cleanup(BelaContext *context, void *userData)
{
	if(gI2cFd >= 0)
		close(gI2cFd);
}
