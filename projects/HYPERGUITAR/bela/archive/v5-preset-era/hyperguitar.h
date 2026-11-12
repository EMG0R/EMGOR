/*
 * hyperguitar.h - shared declarations for the hyperGuitar6chPan split.
 * Pure mechanical extraction from the former monolithic render.cpp -
 * every constant, struct layout and default value here is byte-for-byte
 * identical to the original. See render.cpp for the top-of-file project
 * description / pad map comment (kept there, not duplicated here).
 */
#ifndef HYPERGUITAR_H
#define HYPERGUITAR_H

#include <Bela.h>
#include <libraries/Encoder/Encoder.h>
#include <stdint.h>

/* ============================ CONSTANTS ============================ */

#define NUM_STRINGS 6
#define INPUT_GAIN 1.0f
#define MASTER_GAIN 1.0f
#define LIM_CEIL 0.90f
#define MONO_OUT 0

#define BUF_SIZE 8192
#define BUF_MASK (BUF_SIZE - 1)
#define FXBUF_SIZE 8192
#define FXBUF_MASK (FXBUF_SIZE - 1)
#define DECIM 4
#define DBUF_SIZE 1024
#define DBUF_MASK (DBUF_SIZE - 1)
#define HOP 128
#define MAX_FRET_RATIO 4.5f
#define TUNE_MARGIN 1.12f
#define NSDF_CONF 0.45f
#define FIRST_PEAK 0.82f
#define PITCH_CONF_MIN 0.92f
#define ATTACK_BLANK_HOPS 6
#define GATE_RMS 3e-4f
#define HANN_N 1024
#define VBUF_SIZE 2048
#define VBUF_MASK (VBUF_SIZE - 1)

/* USB audio bridge shared-memory ring */
#define BRIDGE_RING_FRAMES 512
#define BRIDGE_RING_MASK (BRIDGE_RING_FRAMES - 1)
#define BRIDGE_SHM_NAME "/hyperguitar_usbaudio"

typedef struct {
	volatile uint32_t head;   // producer (this RT process) writes
	volatile uint32_t tail;   // consumer (bridge process) writes
	int16_t data[BRIDGE_RING_FRAMES * 2]; // interleaved L/R, S16
} BridgeRing;

/* vibrato / chorus */
#define CHOR_BASE_SEC  0.018f
#define CHOR_GAIN 0.35f

/* autowah */
#define ENVF_FMIN 250.0f
#define ENVF_OCTS 3.0f

/* FM */
#define FM_MAX_A 400.0f

/* synth lead */
#define SINE_LEVEL 0.48f
#define SINE_ENV_SENS 2.0f
#define SYNTH_TONAL_HOPS 14
#define SYNTH_ENV_SLEW 0.35f
#define LEAD_DETUNE 1.007f
#define SYNTH_NMODES 10

/* sitar / sympathetic */
#define TARAB_N 14

/* comb resonator */
#define COMB_SIZE 1024
#define COMB_MASK (COMB_SIZE - 1)

/* GPIO rotary encoders */
#define PIN_ENC1_CLK 0
#define PIN_ENC1_DT  1
#define PIN_ENC1_SW  2
#define PIN_ENC2_CLK 3
#define PIN_ENC2_DT  4
#define PIN_ENC2_SW  5
#define ENC_DETENT 2

/* NeoTrellis: seesaw over /dev/i2c-1 */
#define SS_STATUS       0x00
#define SS_STATUS_SWRST 0x7F
#define SS_STATUS_HW_ID 0x01
#define SS_NEOPIX       0x0E
#define SS_KEYPAD       0x10
#define SS_KEY_EVENT    0x01
#define SS_KEY_COUNT    0x04
#define SS_KEY_FIFO     0x10
#define NPIX 16
#define LED_BRIGHTNESS 0.25f
#define TRELLIS_ADDR 0x2E
#define TRELLIS_BUS "/dev/i2c-1"

#define PAD_PITCH    0
#define PAD_FM       1
#define PAD_FORMANT  2
#define PAD_SINE     3
#define PAD_VIB      4
#define PAD_CHORUS   5
#define PAD_ENVF     6
#define PAD_FWAH     7
#define PAD_PLUCKSAG 8
#define PAD_POG      9
#define PAD_RES      10
#define PAD_SUS      11
#define PAD_JAWARI   12
#define PAD_SYMP     13
#define PAD_SITAR    14
#define PAD_PAN      15

#define PRESET_ENVF_SENS 1.2f
#define PRESET_ENVF_Q    1.4f
#define PRESET_FWAH_SENS 1.2f

/* persisted state */
#define STATE_FILE "/root/Bela/projects/hyperGuitar6chPan/hyperguitar-state.bin"
#define STATE_MAGIC 0x48475332u /* "HGS2" - layout grew formantMode */

/* ============================= STRUCTS ============================= */

typedef struct {
	float buf[COMB_SIZE];
	unsigned int wr;
	float lp;
} Comb;

/* ---------------- unified pitch+formant shifter ---------------- */
/*
 * One pitch-synchronous grain stream per string: grain LAUNCH SPACING
 * sets the pitch ratio, within-grain READ INCREMENT sets the formant
 * ratio (Lent 1989 / Bristow-Johnson 1995 / IVL US 5,567,901). Window =
 * Hann of exactly 2x the launch spacing, in the OUTPUT domain, so the
 * overlap-add is exact-COLA at every setting - no grain-rate AM.
 */
#define SH_NGRAINS 6
#define LPC_ORDER 16
#define LPC_WIN 512
#define LPC_EPTS 33

typedef struct {
	float d0, u, L, phi;
	int active;
} ShGrain;

/* once-per-period pitch-mark picker: 2nd-order LP at ~1.5*f0 + predicted
 * peak search. One instance per source stream (input, pitch-stage out). */
typedef struct {
	float b0, b1, b2, a1, a2, z1, z2;
	float markDelay;
	int haveMark;
	float pkVal, pkDelay;
	float searchIn, searchLeft;
} MarkPicker;

/* one mark-synchronous COLA grain stream (window = 2x launch spacing) */
typedef struct {
	ShGrain gr[SH_NGRAINS];
	float launchCd, srcDelay;
} GrainLeg;

typedef struct {
	MarkPicker mkIn;         /* marks on the input buffer */
	MarkPicker mkFx;         /* marks on the pitch-stage output (formant src) */
	float Tsm;               /* tracker period, hop-snapped (never re-smoothed) */
	/* pitch: SOLAD resampler down (proven, chirp-free by construction),
	 * mark-based COLA grains up */
	GrainLeg up;
	float sdDelay, sdOldDelay, sdFadePh, sdFadeStep, sdHiThresh;
	int sdFading;
	float downAmt, upAmt;    /* leg blend, slewed at hop rate */
	/* formant: series grain leg sourcing the pitch-stage output */
	GrainLeg fm;
	float formantAmt;        /* engage blend, slewed at hop rate */
	/* transient path + mode machine: 0=GRAINS 1=TAP_TRANSIENT 2=TAP_DRIFT */
	int mode;
	float grainMix;          /* 0 = tap leg, 1 = shifter chain (equal-power) */
	float tapDelay, tapOld;
	int tapFading;
	float tapFadePh, tapFadeStep;
	float envF, envS;
	float onsCF, onsCS;      /* period-scaled follower coefficients */
	int onsetRefract, relockHops, noLockHops;
	int transAge;            /* hops since entering TRANS (dead-zone escape) */
	float betaTPrev;         /* last pitch target, to snap on discrete changes */
	unsigned int transHoldUntil; /* absolute sample stamp (strum coherence) */
	/* anti-alias write filter for phi>1 formant grain reads */
	float aaZ, aaCoef;
	/* LPC formant mode (reflection-coefficient lattices, slewed) */
	float lpcK[LPC_ORDER], lpcKW[LPC_ORDER];
	float lpcKr[LPC_ORDER], lpcKrW[LPC_ORDER];
	float lpcZf[LPC_ORDER], lpcZg[LPC_ORDER];
	float lpcGain, lpcGainW;
	float lpcMix;            /* bypass crossfade while analysis invalid */
	int lpcValid;
} Shifter;

typedef struct {
	float buf[BUF_SIZE];    // full-band input ring (DC-blocked only)
	float abuf[FXBUF_SIZE]; // anti-aliased copy for formant-up grain reads
	float fxbuf[FXBUF_SIZE]; // pitch-stage output ring (formant leg source)
	unsigned int writePos;
	// unified pitch+formant engine
	Shifter sh;
	float beta, formant;
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

/* ============================ GLOBALS ============================ */

extern BridgeRing *gBridge;

extern const float OPEN_HZ[NUM_STRINGS];
extern const int STRING_CHANNEL[NUM_STRINGS];
extern const float STRING_PAN[NUM_STRINGS];
extern const float HARM_CUT[NUM_STRINGS];
extern const float LFO_PH[NUM_STRINGS];

extern float gVibDepthSec;
extern float gVibRateScale;
extern const float VIB_HZ[NUM_STRINGS];

extern float gChorDepthSec;
extern float gChorRateScale;
extern const float CHOR_HZ[NUM_STRINGS];

extern float gEnvfQ;
extern float gEnvfSens;
extern float gFWahSens;

extern volatile int gSynthMode;
extern volatile int gSynthPoly;
extern float gSynthLevel;
extern float gSynthGateThr;
extern const char *SYNTH_NAMES[SYNTH_NMODES];

extern float gPogAttackSec;
extern float gPogModDepth;

extern float gResSens;
extern float gResFb;
extern float gResGain;

extern float gSusFb;
extern float gSusDamp;
extern float gSusLevel;

extern float gJawBuzz;
extern float gJawPitch;
extern float gJawSens;

extern const int BHAIRAVA[TARAB_N];
extern float gSitarRing;
extern float gSitarLevel;
extern float gSitarBuzz;

extern float gSagDepthSec;
extern float gSagSettleSec;

extern float gSympCoupling;
extern float gSympFb;
extern float gLastMixed[NUM_STRINGS];
extern float gTarabDelay[TARAB_N];
extern float gTarabPan[TARAB_N];

extern float gPanSpread;
extern float gPanDyn;

// ---- radio/toggle state ----
extern volatile int gPitchActive;
extern volatile int gFormantActive;
extern volatile int gFmActive;

extern float gPitchSemitones;
extern int gPitchCentsMode;
extern float gPitchCents;
extern float gPitchWetPct;

extern float gFmRatioVal;
extern int gFmQuantized;
extern float gFmIndexVal;

extern float gFormantVal;
extern float gFormantGlide;
extern float gFormantTiltBias;
extern volatile int gFormantMode;      /* 0 = GRAIN, 1 = LPC envelope warp */
extern volatile int gPresetFormantWobble;
extern unsigned int gSampleStamp;      /* absolute sample counter */
extern unsigned int gOnsetStamp[NUM_STRINGS]; /* per-string last onset */
extern volatile int gVibOn, gChorusOn, gEnvFOn, gFWahOn;
extern volatile int gSineOn, gPogOn, gResOn, gSusOn;
extern volatile int gJawOn, gSitarOn, gPanOn;
extern volatile int gSagOn, gSympOn;
extern volatile float gBetaBase[NUM_STRINGS];
extern volatile float gFormantBase[NUM_STRINGS];
extern volatile float gDryTarget;
extern volatile float gWetTarget;

extern Comb gTarab[TARAB_N];
extern int gTarabLeadStr;
extern float gOtherMax[NUM_STRINGS];
extern float gSympW[NUM_STRINGS][NUM_STRINGS];

extern Voice gV[NUM_STRINGS];
extern float gDryS, gWetS;
extern float gVibAmtS, gChorAmtS, gEnvFAmtS;
extern float gFmAmtS, gSineAmtS, gPogAmtS;
extern float gResAmtS, gSusAmtS;
extern float gJawAmtS, gSitarAmtS, gPanAmtS;
extern float gSagAmtS, gSympAmtS;

// ---- GPIO rotary encoders ----
extern Encoder gEnc1, gEnc2;
extern int gEnc1LastPos, gEnc2LastPos;
extern int gEnc1BtnStable, gEnc2BtnStable;
extern int gEnc1BtnHold, gEnc2BtnHold;
extern volatile int gEnc1Click, gEnc2Click;
extern volatile int gArmedPad;
extern volatile int gRawDiag;
extern volatile int gStateDirty;
extern volatile int gPresetMode;
extern int gComboLatch1, gComboLatch2;
extern volatile int gModeFlash;
extern int gPresetFormantSel;
extern int gPresetPitchSel;
extern int gPresetVibChorusSel;
extern int gSwInit, gEnc1Idle, gEnc2Idle;
extern float gLimEnv;
extern float gHannT[HANN_N + 1];
extern double gTimeSec;

// monophonic lead voice state
extern int gLeadStr;
extern float gLeadInc, gLeadIncTgt;
extern float gLeadAmp;
extern float gLeadLPz;

// NeoTrellis
extern int gI2cFd;
extern int gTrellisOk;
extern float gPadFlash[NPIX];
extern float gLavaT;
extern uint8_t gGammaLUT[256];
extern AuxiliaryTask gTrellisTask;

/* ============================ PROTOTYPES ============================ */

/* dsp_core.cpp */
float pblep(float t, float dt);
float bufAtM(const float *buf, unsigned int mask, unsigned int wr, float delay);
float combRun(Comb *c, float in, float delay, float fb, float damp);
void voiceProcess(Voice *v, float in, int dryNeeded, float *wetOut, float *dryOut);
void updateLP(Voice *v);
void updateModulation(Voice *v, int s);
float softClip(float x);

static inline float shHann(float t)
{
	if(t <= 0.0f || t >= 1.0f) return 0.0f;
	return gHannT[(int)(t * HANN_N)];
}

/* shifter.cpp */
void shifterInit(Voice *v);
void shifterHop(Voice *v, int s);
float shifterRun(Voice *v, float full);
void shifterOnset(Voice *v, int s);

/* lpc.cpp */
void lpcAnalyze(Voice *v);
float lpcProcess(Voice *v, float x);

/* pitch_tracker.cpp */
void trackPitch(Voice *v, int s);

/* synth.cpp */
void runSynthPoly(Voice *v, int s, float *mixed);
void runSynthMono(float *outL, float *outR, float sr);

/* presets.cpp */
void applyPitch(void);
void applyFormant(void);
void applyMixRouting(void);
void applyPreset(int pad);
void saveState(void);
void loadState(void);
void applyEncoders(int pad, int mainDelta, int auxDelta, int mainClick, int auxClick);

/* trellis_ui.cpp */
void trellisTick(void *);
int trellisInit(void);

/* effects.cpp */
void runFM(Voice *v, float *mixed);
void runAutowah(Voice *v, float *mixed);
void runResonator(Voice *v, float *mixed);
void runJawari(Voice *v, float *mixed);
void runSustainer(Voice *v, float *mixed);
void runSitarZing(Voice *v, float *mixed);
void runSympathetic(Voice *v, int s, const float *dryIn, float *mixed);
void runPluckSag(Voice *v, float *mixed, float sr);
void runVibrato(Voice *v, int s, float *mixed, float vibDepthSmp, float sr);
void runPogSwell(Voice *v, float *mixed, float pogStep);
void runChorus(Voice *v, int s, float *mixed, float chorBaseSmp, float chorDepthSmp,
		float *wideOut, float sr);
void runSitarTarab(const float *dryIn, float *outL, float *outR);

#endif /* HYPERGUITAR_H */
