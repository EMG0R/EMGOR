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
#define SYNTH_NMODES 23   /* 0-7 = analog pads (2026-11-05), 8-22 = the old 15 (old index + 8) */
#define SYNTH_STATE_VER 1 /* persisted-state synth index version: 0 = pre-analog (15 modes), 1 = 23 modes */
#define SYNTH_OLD_SHIFT 8 /* old mode index + this = new index */

/* sitar / sympathetic */
#define TARAB_N 14

/* comb resonator */
#define COMB_SIZE 1024
#define COMB_MASK (COMB_SIZE - 1)

/* GPIO rotary encoder. v8: there is only ONE - pins 3/4/5 (the old ENC2
 * CLK/DT/SW) are unused; that encoder was never fitted. */
#define PIN_ENC1_CLK 0
#define PIN_ENC1_DT  1
#define PIN_ENC1_SW  2
/*
 * Quadrature transitions per physical detent. gEnc*.setup(..., Encoder::ANY)
 * counts EVERY transition, and a KY-040-style detented encoder produces FOUR
 * per click - so this must be 4. At 2 every detent moved TWO logical steps,
 * which is the "it does 2 at a time / flips through the menu" behaviour.
 */
#define ENC_DETENT 4

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
/*
 * v8 NEW EFFECTS. Effect-id space now exceeds NPIX (16 pads) on purpose -
 * NPIX still means "physical LEDs/pads", NUM_EFFECTS means "rows in gFx[]".
 * These two ids are never a pad's PAD_SLOT (no such macro any more) and
 * never collide with a physical pad index; gFx[] is sized NUM_EFFECTS, not
 * NPIX, specifically so this is safe. See trellis_ui.cpp.
 */
#define PAD_XFADE    16   /* dry/hex LFO crossfade, equal-power, speed+depth */
#define PAD_BEND     17   /* attack pitch-envelope: bends into the note */
#define NUM_EFFECTS  18

#define PRESET_ENVF_SENS 1.2f
#define PRESET_ENVF_Q    1.4f
#define PRESET_FWAH_SENS 1.2f

/*
 * ===================== v8 FLAT GRID UI (2026-10-27) =====================
 *
 * NO SUBMENUS. Every physical pad is ONE fixed function, always - no page
 * switching, no category select, no PAD_SLOT indirection. The 4x4 grid is
 * row-major, top-left origin: pad index i sits at row i/4, column i%4
 * (trellis_ui.cpp key decode). See presets.cpp's gPadFx[NPIX] for the table -
 * pad i IS effect gPadFx[i].fx, directly, with no geometry math at all.
 *
 *     [ 0 TIMBRE  ][ 1 PITCH    ][ 2 AUTOWAH  ][ 3 SYNTH    ]
 *     [ 4 FM GROWL][ 5 XFADE    ][ 6 FWAH     ][ 7 BEND     ]
 *     [ 8 PAN     ][ 9 TRS DRY  ][10 open     ][11 open     ]
 *     [12 open    ][13 open     ][14 open     ][15 USB AUDIO]
 *
 * Pads 10/11/12/13/14 are deliberately unassigned (EMGOR: "the other stuff I
 * don't like yet, I'll come back to them") - lava background only, EXCEPT
 * that 10/11/12 double as the live PITCH-interval readout (see trellis_ui.cpp)
 * while PAD_PITCH is armed.
 *
 * Replaces the v6/v7 4-category x 12-slot menu (archived in
 * bela/archive/v8-category-menu-pre-2026-10-27/) - that table and all its
 * MENU_PAD/IS_MENU_PAD/PAD_SLOT geometry are gone, not reused.
 */
#define FX_NONE (-1)
/* Not an effect: the USB audio on/off pad. Negative so it can never index
 * gFx[]; every gPadFx consumer must check for it.
 * no parens, no trailing comment: hgweb.py parses PAD_ defines as bare integers */
#define PAD_USB -2
/* TRS DRY PASSTHROUGH (pad 9, v8): same pattern as PAD_USB - a hard toggle,
 * not a voiced effect, handled directly in trellis_ui.cpp. */
#define PAD_TRS_DRY -3

/*
 * A slot is an effect AND a VOICING, not just an effect.
 *
 * v5's preset pads were four distinct formant voices (0.75/0.90/1.10/
 * 1.30) you could hit instantly. v6 collapsed them to one FORMANT pad
 * sitting at gFormantVal = 1.0 - and a formant ratio of 1.0 IS the dry
 * signal, so the pad audibly did nothing. That regression is why slots
 * carry parameters: pressing a slot sets the voicing, then the encoders
 * take over from there.
 *
 * p1/p2 are the effect's primary/secondary parameters, same axes the
 * encoders drive (see applySlotPreset() in presets.cpp for the per-
 * effect meaning). SLOT_KEEP leaves a parameter at its current value.
 */
#define SLOT_KEEP (-1e9f)

typedef struct {
	signed char fx;        /* effect id, or FX_NONE for an empty slot */
	float p1, p2;          /* voicing applied on press; SLOT_KEEP = leave */
	const char *label;     /* short name, for the boot/press log */
} FxSlot;

/* pad i -> voiced effect, DIRECTLY, no category/slot indirection.
 * Defined in presets.cpp. */
extern const FxSlot gPadFx[NPIX];

/* apply a slot's voicing to the live globals; no-op for SLOT_KEEP */
void applySlotPreset(const FxSlot *s);

/*
 * LED scheme. Values are pre-gamma linear 0-255 RGB; trellis_ui.cpp
 * applies the gamma LUT and LED_BRIGHTNESS on the way out.
 *   PURPLE - background: every unlit pad, the resting grid
 *   BLUE   - the four menu pads (bright = selected, dim = not)
 *   GOLD   - an FX pad whose effect is ON
 */
#define COL_BG_R        58
#define COL_BG_G         0
#define COL_BG_B       120
/* COL_MENU_* retired with the v6/v7 menu row - there is no menu any more
 * (v8 flat grid). No replacement needed. */
/* ON colour. Was gold; light blue by request 2026-09-22. The name is kept as
 * COL_GOLD_* because it is referenced in several places and renaming it would
 * touch code this change has no business touching - it means "the ON colour". */
#define COL_GOLD_R      70
#define COL_GOLD_G     200
#define COL_GOLD_B     255
#define COL_SLOT_DIM  0.45f   /* populated-but-off FX slot, purple at this */

/* persisted state */
#define STATE_FILE "/root/Bela/projects/hyperGuitar6chPan/hyperguitar-state.bin"
#define STATE_MAGIC 0x48475335u /* "HGS5" - 2026-10-27 flat-grid redo, no submenus.
                                  * Bumped for the same reason v6's bump was: the whole
                                  * pad map changed shape, so an old file's restored
                                  * category/slot state would mean nothing here and a
                                  * stale "on" flag could come back with no pad left to
                                  * switch it off. */

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
#ifndef LPC_ORDER
#define LPC_ORDER 32   /* MAX order (lpc.cpp: 28 on strings 0-1, 24 above; 32 allowed) */
#endif
#ifndef LPC_WIN
#define LPC_WIN 1024   /* MAX analysis window (lpc.cpp: 1024 on strings 0-1, 512 above) */
#endif
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
	float lvl;               /* slow |y| envelope: RELATIVE mark gate */
} MarkPicker;

/* one mark-synchronous COLA grain stream (window = 2x launch spacing) */
typedef struct {
	ShGrain gr[SH_NGRAINS];
	float launchCd, srcDelay;
} GrainLeg;

typedef struct {
	MarkPicker mkFx;         /* marks on the pitch-stage output (formant src) */
	float Tsm;               /* tracker period, hop-snapped (never re-smoothed) */
	float Tres;              /* period the resampler splices with (live once the
	                          * note is locked, else periodGood/p0) */
	/* pitch: ONE SOLAD resampler, both directions. sdDelay is the read delay
	 * behind the write head; it drifts at 1-rho per sample and jumps back by
	 * one period (down, at sdHiThresh) or sdN periods (up, at sdLo). */
	float sdDelay, sdOldDelay, sdFadePh, sdFadeStep, sdHiThresh;
	float sdLo;              /* up: delay floor (fade room incl.) */
	float sdMid, sdOnsPh, sdOnsStep;    /* attack overlay: outgoing read + phase */
	int sdFading, sdEq, sdUp, sdN, sdOnsLive;
	float downAmt, upAmt;    /* leg blend, slewed per sample */
	/* formant: series grain leg sourcing the pitch-stage output */
	GrainLeg fm;
	float formantAmt;        /* engage blend, slewed per sample */
	/* transient path: mode 0=RUN 1=TRANS (dry tap carries the attack) */
	int mode;
	float grainMix;          /* 0 = dry tap, 1 = shifter chain (equal-power) */
	int fxGate;              /* formant-only: picker stale -> output dry */
	int noteLocked;          /* tracker locked the current note (since onset) */
	float envF, envS;
	float onsCF, onsCS;      /* period-scaled follower coefficients */
	int onsetRefract, relockHops;
	int fxAliveHops, fxDeadHops; /* mkFx freshness hysteresis (mode gate) */
	int transAge;            /* hops since entering TRANS */
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
	/* LPC v2 (lpc.cpp only): onset snap, history for lattice priming */
	unsigned int lpcOnsetSeen, lpcFreshStamp;
	int lpcSnap, lpcHistPos;
	float lpcHist[128];
	float lpcPin, lpcPout, lpcCorr;  /* slow level trim: out power tracks in power */
	/* 2026-11-01 lpc-q: per-sample coefficient ramp (H1/H5), idle fade (H3) */
	float lpcDK[LPC_ORDER], lpcDKr[LPC_ORDER], lpcDG;   /* per-sample deltas toward lpcK/lpcKr/lpcGain */
	int lpcRamp;                 /* samples of ramp left */
	int lpcRampSnap;             /* 1 while the long snap ramp runs (hop ramps are held off) */
	unsigned int lpcRampStamp;   /* last lpcFreshStamp a ramp was started for */
	int lpcSilentN;              /* consecutive samples with dbgGate==2 (capped) */
	int lpcIdle;                 /* 1 = faded out for silence; cleared by the wake snap */
	unsigned int lpcWakeStamp;   /* sample stamp the string stopped being silent */
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
	int dbgGate;        // TEMP DIAG: which tracker early-out fired last hop
	float dbgE0;        // TEMP DIAG: window mean-square vs GATE_RMS^2
	// JAWARI BUZZ has no extra per-voice state (uses env directly)
	// SHAKUHACHI/NEY
	// PLUCK SAG
	float sagBuf[VBUF_SIZE];
	unsigned int sagWrite;
	float sagEnv, sagEnvHold, sagDecayCoef;
	int sagRefract;
	// BEND: attack pitch-envelope (v8). Same onset-detect shape as SAG
	// (env-spike heuristic, hop-rate refractory), but feeds the REAL
	// shifter's beta rather than a delay tap - see dsp_core.cpp.
	float bendEnv, bendEnvHold, bendDecayCoef;
	int bendRefract;
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

// ---- v8 new effects ----
/* XFADE: dry/hex equal-power LFO crossfade. Rate = Hz, depth 0..1 = how far
 * the sweep swings either side of an even 50/50 blend. See xfadeTick() in
 * presets.cpp, called once per block from render(). */
extern volatile int gXfadeOn;
extern float gXfadeRate;   /* Hz */
extern float gXfadeDepth;  /* 0..1 */

/* BEND: attack pitch-envelope, riding the real shifter (see dsp_core.cpp's
 * updateModulation()). gBendDepthSemis is signed - positive bends DOWN into
 * the note (starts sharp), negative bends UP into it (starts flat), matching
 * "bends my notes as I play them into the note." gBendSettleSec is the pot
 * job (pickup, like chorus/vib depth); gBendDepthSemis is the main encoder. */
extern volatile int gBendOn;
extern float gBendDepthSemis;  /* signed, semitones at attack */
extern float gBendSettleSec;

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
/* TRS DRY PASSTHROUGH (v8): hard bypass, pad 9. When on, every string's
 * output is the raw TRS/hex dry signal - the whole effects chain (FM,
 * autowah, resonator, jawari, sustainer, sitar, symp, sag, vibrato, pog,
 * chorus, synth) is skipped outright for that sample, not just dry/wet
 * balanced. Volume slider and stereo pan still apply downstream. */
extern volatile int gTrsDryOn;

/* AUTO-LEVEL (2026-10-27): slow (tau ~5s), bounded (+/-6dB) makeup trim
 * that holds the long-term loudness steady across PITCH/FORMANT/FM/BEND/
 * XFADE/SYNTH/TRS-dry toggles, without reacting to playing dynamics - NOT
 * a compressor. On by default. See the design note and AUTOLVL_* constants
 * above the master limiter in render.cpp for the mechanism. gAutoLevelOn
 * is the single flag to flip to 0 for an A/B; gAutoLevelTrimDb is a
 * readout only (current trim in dB, for hg_mon.cpp telemetry). */
extern volatile int gAutoLevelOn;
extern float gAutoLevelTrimDb;

/*
 * v8 RESOURCE GATE (whammy + general idle-cost fix): 1 whenever the wet leg
 * is wanted (gWetTarget) OR still audibly decaying (gWetS not yet silent) -
 * computed once per block in render.cpp, consumed in dsp_core.cpp to skip
 * the shifter's per-sample and per-hop work entirely when idle. See
 * voiceProcess()/updateModulation().
 */
extern volatile int gWetEngineActive;

/* JOYSTICK WHAMMY (v8): continuous ±1 octave pitch bend, independent of
 * which pad is pressed - compounds on top of PITCH/BEND/whatever else is
 * live, exactly like BEND compounds on top of PITCH. Smoothed semitone
 * offset, written every block by hg_ctrl.cpp, read by updateModulation(). */
extern float gJoyWhammySemis;
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

// ---- GPIO rotary encoder (v8: just the one - gEnc2/ENC2 retired) ----
extern Encoder gEnc1;
extern int gEnc1LastPos;
/* Samples of SOLID contact before the button counts as pressed (~10 ms at
 * 44.1 kHz), and the refractory period after a click during which a second
 * one cannot fire (~200 ms - far longer than any switch bounces, far shorter
 * than a human double-press). See the integrator block in render.cpp. */
#define ENC_BTN_INTEG   441
#define ENC_BTN_REFRACT 8820
extern int gEnc1BtnStable;   /* 1 = PRESSED (debounced) */
extern int gEnc1BtnHold;
extern volatile int gEnc1Click;
struct SensorState;
extern struct SensorState gHgSens;   /* MPU6050 + VL53L1X, see i2c_sensors.h */
extern int gTrellisDisabled;
extern int gHgSensorDiv;

/* GRID FLASH. A whole-grid colour pulse for a hard toggle (USB audio, TRS
 * dry passthrough): green = on, red = off. The old "which encoder is this"
 * identification meaning (values 1/2) is RETIRED - v8 has one encoder, no
 * ambiguity to resolve. Set by trellis_ui.cpp, consumed and decremented by
 * the trellis render loop. */
extern volatile int gEncFlashWho;    /* 0 none, 3 = green (on), 4 = red (off) */
extern volatile int gEncFlashCount;  /* trellis frames remaining */

/* THE POT - see potTick() in presets.cpp */
extern volatile float gHgPot;      /* 0..1 calibrated, clockwise = up (hg_ctrl.cpp) */
extern float gPitchWetByStop[6];   /* 2026-11-01 pwet: per-stop PITCH wet 0..1 (-12,-7,-5,+5,+7,+12) */
void applyPitchWet(void);          /* 2026-11-01 pwet: re-apply wet of current stop into gPitchBal */
extern volatile float gPitchBal;   /* pitch dry/wet: 0 = TRS guitar, 1 = shifted hex */
void potArm(int fx);               /* call on every pad press that arms fx */
void potTick(float pot);           /* once per audio block */
extern volatile int gArmedPad;   /* EFFECT id the encoders drive, FX_NONE if none */
extern volatile int gRawDiag;
extern volatile int gStateDirty;
/*
 * RETIRED in v6. The dual-mode UI is gone: pads and encoders now behave
 * the same way at all times. gPresetMode is pinned to 0 and kept only so
 * the archived v5 preset code still compiles if it is ever revived.
 * See bela/archive/v5-preset-era/.
 */
extern volatile int gPresetMode;
extern volatile int gModeFlash;
extern int gPresetFormantSel;
extern int gPresetPitchSel;
extern int gPresetVibChorusSel;
extern int gSwInit, gEnc1Idle;
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
/* 2026-11-01 lpc-w: LPC analysis on a worker thread (HG_LPC_WORKER 0 = direct call only) */
#ifndef HG_LPC_WORKER
#define HG_LPC_WORKER 1
#endif
void lpcSubmit(Voice *v);          /* shifter.cpp call site: snapshot + publish (or direct analysis if no worker) */
void lpcCancel(int s);             /* drop in-flight results for string s (formant stage went inactive) */
void lpcAdoptAll(void);            /* start of every render block: adopt finished results */
int  lpcWorkerPoll(void);          /* worker thread: run pending jobs, returns jobs run */
void lpcWorkerEnable(int on);      /* hg_mc.cpp, once the worker thread is up / before it stops */
void lpcInitOnce(void);   /* called from hg_mc.cpp's worker pool init */
float lpcProcess(Voice *v, float x);

/* pitch_tracker.cpp */
void trackPitch(Voice *v, int s);

/* 2026-11-05 synth dynamics: tracker-published band-limited level (pitch-
 * synchronous RMS, once per hop) + a 16-hop history ring (newest = idx-1). */
#define BAND_HIST_N 16
extern float gBandLvl[NUM_STRINGS];
extern float gBandHist[NUM_STRINGS][BAND_HIST_N];
extern int   gBandHistIdx[NUM_STRINGS];
extern float gSynSustain;            /* persisted mix param syn_sustain, 0..1 */
extern float gSynFmod;               /* persisted mix param syn_fmod, 0..1 = 0..4 octaves level->cutoff (pot while SYNTH armed) */

/* saved synth index -> current index. ver 0 = state written before the 8 analog
 * patches were put first: the same SOUND is now 8 higher. Out-of-range -> 0. */
static inline int synthModeMigrate(int saved, int ver)
{
	if(ver < SYNTH_STATE_VER) {
		if(saved < 0 || saved >= SYNTH_NMODES - SYNTH_OLD_SHIFT) return 0;   /* not a valid old index */
		return saved + SYNTH_OLD_SHIFT;
	}
	return (saved < 0 || saved >= SYNTH_NMODES) ? 0 : saved;
}
const char *synWhyName(int why);

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
void applyEncoders(int pad, int mainDelta, int mainClick);
/* v8: per-block tick for the XFADE effect (dry/hex LFO), called from render()
 * right next to hgGlideTick(). No-op while gXfadeOn is 0. */
void xfadeTick(float blockSeconds);
/* v8: continuous pot -> formant tone mapping for the TIMBRE pad (darkest ..
 * brightest), with the same near-unity silent-ratio guard applySlotPreset()
 * already uses for PAD_FORMANT. */
float timbrePotToFormant(float pot);

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

/* 2026-11-01 tier-A: audio-thread -> aux-task diagnostics. The audio thread
 * pushes a compact struct (no formatting); trellisTick does snprintf+rt_printf.
 * Single producer (audio thread), single consumer (trellisTick). Defined in
 * shifter.cpp so the host test links it. */
enum { SHDIAG_MODE = 1, SHDIAG_STATUS = 2, SHDIAG_ONSET = 3,
	SHDIAG_SYN_ON = 4,    /* 2026-11-05: synth onset verdict (accepted or rejected) */
	SHDIAG_SYN_OFF = 5 }; /* 2026-11-05: synth note end */
struct ShDiagEv {
	int code, s;
	int i[4];
	float f[6];
};
void shDiagPush(const ShDiagEv *e);
int  shDiagPop(ShDiagEv *e);        /* 1 = got one */

/* 2026-11-01 tier-A: telemetry counters, plain volatile, written by the audio
 * thread (cumulative since boot; hg_mon.cpp diffs them per report). */
extern volatile unsigned int gTelTrans[NUM_STRINGS];   /* TRANS-mode entries */
extern volatile unsigned int gTelFxg[NUM_STRINGS];     /* fxGate flips */
extern volatile unsigned int gTelLoopCap;              /* dc loop cap hit -> re-anchor (all strings) */
extern volatile unsigned int gTelLpcMaxTk;             /* max lpcAnalyze ticks; monitor zeroes */
extern volatile unsigned int gTelLpcLate;              /* 2026-11-01 lpc-w: LPC jobs not finished when due (cumulative) */
extern volatile unsigned int gTelLpcWorkerUs;          /* 2026-11-01 lpc-w: max us of one worker run; monitor zeroes */
extern volatile uint64_t gTelCpuSumTk, gTelCpuMaxTk;   /* render() block time (max: monitor zeroes) */
extern volatile unsigned int gTelCpuN;                 /* blocks measured (cumulative) */

#endif /* HYPERGUITAR_H */
