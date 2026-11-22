// drum_sounds.cpp — the 64 synthesized recipes (research/drum-synthesis.md),
// expressed as data (SoundDef, one row per sound) driving ~10 shared engines
// (VKind, see drums_priv.h). Owns DrumVoice::trigger()/process() and the
// DRUM_DEFS / DRUM_SOUND_NAMES tables. See drums.cpp for voice management,
// param pages and the public drums* API.
#include "drums_priv.h"
static inline float fxTanhL(float z) { if (z > 3.f) return 1.f; if (z < -3.f) return -1.f; float z2 = z * z; return z * (27.f + z2) / (27.f + 9.f * z2); }

// ==================================================================== names
// <=5 chars, upper-case (AGENT_RULES rule 12).
extern const char* const DRUM_SOUND_NAMES[DG_COUNT][DRUM_SOUNDS] = {
    // DG_KICK
    { "KSUB","KCLUB","808","909","KCK1","KCK2","KCK3","K808","K909","KHARD","PHONE","SUB","BOOM","DIST","RUMBL","GLICH" },
    // DG_SNARE
    { "SNP1","808","GATE","CLAP9","SNP2","SNRF","CHOP","CLAP","ESNR","RIM","909","CLAP8","TIGHT","MODAL","FM","ZAPN" },
    // DG_HAT
    { "HCL","CH808","CRASH","HOP","HMD","WASH","EHAT1","EHAT2","EHAT3","ECYM","OH808","CH909","OH909","RIDE","NOH","FMCYM" },
    // DG_PERC
    { "TOML","CHIRP","COWB","CLAVE","TOMM","TOMH","BELL","BLIP","LZTOM","FMBEL","MTL","COSMO","TOM9","CONGA","LASER","BONGO" },
};

// ==================================================================== sound tables
// gain (level-match trim) is left at a flat 1.0 everywhere: real trims need a
// render-and-measure pass per §6.2 (bounce each voice at length=0.5/pitch=0/
// tone=0.5/accent=1, measure peak, gain = 0.891/peak) which needs an offline
// tool this module doesn't have; the mix is still listenable because every
// engine's own g0/g1/g2 weights were chosen to roughly balance already. To
// re-tune: fill in `/peak` per row once that measurement exists.
extern const SoundDef DRUM_DEFS[DG_COUNT][DRUM_SOUNDS] = {

// -------------------------------------------------------------- DG_KICK
{
    /*0 ksub*/ { .kind = VK_LEGACY, .pmode = PM_KEYROOT, .f0 = 50.f, .lAmp = 1.f, .lDec = 0.99996f, .lNoiz = 0.03f, .lDrop = 3.f, .lPart = 0.f, .gain = 1.520f },
    /*1 kclub*/ { .kind = VK_LEGACY, .pmode = PM_KEYROOT, .f0 = 52.f, .lAmp = 1.f, .lDec = 0.99992f, .lNoiz = 0.12f, .lDrop = 5.5f, .lPart = 0.f, .gain = 0.892f },
    /*2 808*/ { .kind = VK_RESON, .pmode = PM_KEYROOT, .t60_0 = 0.30f,
                    .clickMs = 6.f, .clickBump = 1.7f, .selfFm = 0.3f, .gain = 2.239f, .satDrive = 6.434f },
    /*3 909*/ { .kind = VK_DUAL, .pmode = PM_KEYROOT, .t60_0 = 0.16f,
                    .clickMs = 100.f, .clickBump = 4.5f, .useOsc = true,
                    .nF = 1200.f, .nT60 = 0.02f, .nAtkMs = 0.5f, .nLvl = 0.35f, .gain = 1.311f, .satDrive = 3.104f },
    /*4 kck1*/ { .kind = VK_LEGACY, .pmode = PM_KEYROOT, .f0 = 56.f, .lAmp = 0.97f, .lDec = 0.99984f, .lNoiz = 0.06f, .lDrop = 4.2f, .lPart = 0.f, .gain = 1.420f },
    /*5 kck2*/ { .kind = VK_LEGACY, .pmode = PM_KEYROOT, .f0 = 52.f, .lAmp = 1.f, .lDec = 0.99993f, .lNoiz = 0.05f, .lDrop = 6.f, .lPart = 0.f, .gain = 0.964f },
    /*6 kck3*/ { .kind = VK_LEGACY, .pmode = PM_KEYROOT, .f0 = 60.f, .lAmp = 0.97f, .lDec = 0.99980f, .lNoiz = 0.1f, .lDrop = 4.8f, .lPart = 0.f, .gain = 1.092f },
    /*7 k808*/ { .kind = VK_LEGACY, .pmode = PM_KEYROOT, .f0 = 52.f, .lAmp = 1.f, .lDec = 0.99995f, .lNoiz = 0.05f, .lDrop = 7.f, .lPart = 0.f, .gain = 0.852f },
    /*8 k909*/ { .kind = VK_LEGACY, .pmode = PM_KEYROOT, .f0 = 55.f, .lAmp = 1.f, .lDec = 0.99988f, .lNoiz = 0.08f, .lDrop = 4.f, .lPart = 0.f, .gain = 1.290f },
    /*9 khard*/ { .kind = VK_LEGACY, .pmode = PM_KEYROOT, .f0 = 58.f, .lAmp = 1.f, .lDec = 0.99985f, .lNoiz = 0.2f, .lDrop = 8.f, .lPart = 0.f, .gain = 0.695f },
    /*10 phone*/ { .kind = VK_SWEEP, .pmode = PM_KEYROOT, .f1r = 1.85f,
                    .t60_0 = 0.25f, .t60_1 = 0.06f, .g0 = 1.f, .g1 = 0.5f,
                    .sweepStartR = 3.4f, .sweepMs = 40.f,
                    .nBp = true, .nF = 2500.f, .nQ = 2.f, .nT60 = 0.012f,
                    .nAtkMs = 0.2f, .nLvl = 0.4f, .fold = true, .gain = 1.990f, .satDrive = 2.867f },
    /*11 sub*/ { .kind = VK_SWEEP, .pmode = PM_KEYROOT, .t60_0 = 0.60f,
                    .sweepStartR = 1.5f, .sweepMs = 15.f, .drive = 0.3f, .gain = 1.726f, .satDrive = 5.506f },
    /*12 boom*/ { .kind = VK_RESON, .pmode = PM_KEYROOT, .t60_0 = 1.50f,
                    .clickMs = 8.f, .clickBump = 1.6f, .selfFm = 0.6f, .drive = 1.5f, .gain = 1.649f, .satDrive = 3.099f },
    /*13 dist*/ { .kind = VK_DUAL, .pmode = PM_KEYROOT, .t60_0 = 0.35f,
                    .clickMs = 100.f, .clickBump = 4.5f, .useOsc = true,
                    .nF = 1200.f, .nT60 = 0.02f, .nAtkMs = 0.5f, .nLvl = 0.3f,
                    .drive = 8.f, .fold = true, .gain = 0.813f, .satDrive = 1.818f },
    /*14 rumbl*/ { .kind = VK_RESON, .pmode = PM_KEYROOT, .f1r = 1.5f,
                    .t60_0 = 0.40f, .t60_1 = 0.15f, .g0 = 1.f, .g1 = 0.5f, .gain = 2.364f, .satDrive = 4.403f },
    /*15 glich*/ { .kind = VK_GLITCH, .pmode = PM_KEYROOT, .t60_0 = 0.15f,
                    .retrigN = 5, .retrigGapMs = 12.f, .retrigMul = 0.7f, .gain = 1.695f, .satDrive = 7.235f },
},

// -------------------------------------------------------------- DG_SNARE
{
    /*0 snp1*/ { .kind = VK_LEGACY, .f0 = 210.f, .lAmp = 0.8f, .lDec = 0.99950f, .lNoiz = 0.85f, .lDrop = 1.5f, .lPart = 0.f, .gain = 0.468f },
    /*1 808*/ { .kind = VK_DUAL, .f0 = 180.f, .f1r = 1.833f,
                    .t60_0 = 0.16f, .t60_1 = 0.09f, .g0 = 0.6f, .g1 = 0.6f,
                    .clickMs = 2.f, .clickBump = 1.15f,
                    .nF = 1700.f, .nT60 = 0.06f, .nAtkMs = 0.3f, .nLvl = 0.4f, .gain = 3.175f },
    /*2 gate*/ { .kind = VK_DUAL, .f0 = 160.f, .f1r = 1.875f,
                    .t60_0 = 0.25f, .t60_1 = 0.2f, .g0 = 0.5f, .g1 = 0.5f,
                    .nF = 800.f, .nT60 = 0.02f, .nHoldMs = 120.f,
                    .nAtkMs = 1.f, .nLvl = 0.8f, .gain = 0.998f },
    /*3 clap9*/ { .kind = VK_CLAP, .nF = 1140.f, .nQ = 1.95f,
                    .bN = 4, .bGapMs = 11.f, .bDurMs = 7.f,
                    .tailT60 = 0.15f, .tailAtkMs = 10.f, .tailLvl = 0.6f, .gain = 4.527f },
    /*4 snp2*/ { .kind = VK_LEGACY, .f0 = 240.f, .lAmp = 0.78f, .lDec = 0.99938f, .lNoiz = 0.9f, .lDrop = 1.6f, .lPart = 0.f, .gain = 0.591f },
    /*5 snrf*/ { .kind = VK_LEGACY, .f0 = 175.f, .lAmp = 0.8f, .lDec = 0.99968f, .lNoiz = 0.78f, .lDrop = 1.4f, .lPart = 0.f, .gain = 0.465f },
    /*6 chop*/ { .kind = VK_LEGACY, .f0 = 300.f, .lAmp = 0.72f, .lDec = 0.99930f, .lNoiz = 0.95f, .lDrop = 1.2f, .lPart = 0.f, .gain = 0.686f },
    /*7 clap*/ { .kind = VK_LEGACY, .f0 = 1100.f, .lAmp = 0.7f, .lDec = 0.99955f, .lNoiz = 0.95f, .lDrop = 1.f, .lPart = 0.f, .gain = 0.486f },
    /*8 esnr*/ { .kind = VK_LEGACY, .f0 = 320.f, .lAmp = 0.72f, .lDec = 0.99940f, .lNoiz = 0.8f, .lDrop = 1.4f, .lPart = 0.f, .gain = 0.704f },
    /*9 rim*/ { .kind = VK_LEGACY, .f0 = 180.f, .lAmp = 0.78f, .lDec = 0.99965f, .lNoiz = 0.7f, .lDrop = 1.5f, .lPart = 0.f, .gain = 0.679f },
    /*10 909*/ { .kind = VK_DUAL, .f0 = 185.f, .f1r = 1.47f,
                    .t60_0 = 0.12f, .t60_1 = 0.08f, .g0 = 0.6f, .g1 = 0.5f,
                    .clickMs = 35.f, .clickBump = 2.f, .useOsc = true,
                    .nF = 1900.f, .nT60 = 0.05f, .nHoldMs = 24.f,
                    .nAtkMs = 0.2f, .nLvl = 0.5f, .gain = 2.225f },
    /*11 clap8*/ { .kind = VK_CLAP, .nF = 1000.f, .nQ = 2.f,
                    .bN = 4, .bGapMs = 10.f, .bDurMs = 8.f,
                    .tailT60 = 0.10f, .tailAtkMs = 5.f, .tailLvl = 0.5f, .drive = 0.6f, .gain = 4.368f },
    /*12 tight*/ { .kind = VK_DUAL, .f0 = 200.f, .f1r = 1.8f,
                    .t60_0 = 0.05f, .t60_1 = 0.035f, .g0 = 0.5f, .g1 = 0.5f,
                    .clickMs = 3.f, .clickBump = 1.4f,
                    .nBp = true, .nF = 3000.f, .nQ = 0.7f, .nT60 = 0.045f,
                    .nAtkMs = 0.2f, .nLvl = 0.5f, .drive = 1.5f, .gain = 2.591f },
    /*13 modal*/ { .kind = VK_RESON, .f0 = 220.f, .f1r = 2.0f, .f2r = 3.18f,
                    .t60_0 = 0.30f, .t60_1 = 0.25f, .t60_2 = 0.20f,
                    .g0 = 1.f, .g1 = 0.5f, .g2 = 0.3f,
                    .nF = 1900.f, .nT60 = 0.1f, .nAtkMs = 0.3f, .nLvl = 0.25f, .gain = 1.536f },
    /*14 fm*/ { .kind = VK_FM, .f0 = 200.f, .t60_0 = 0.12f,
                    .clickMs = 15.f, .clickBump = 1.5f,
                    .nBp = true, .nF = 2200.f, .nQ = 1.2f, .nT60 = 0.09f,
                    .nAtkMs = 0.3f, .nLvl = 0.35f,
                    .fmRatio = 2.31f, .fmIdx0 = 8.f, .fmIdx1 = 1.f, .fmMs = 40.f, .gain = 1.730f },
    /*15 zapn*/ { .kind = VK_LEGACY, .f0 = 2200.f, .lAmp = 0.6f, .lDec = 0.99935f, .lNoiz = 0.92f, .lDrop = 1.f, .lPart = 0.f, .gain = 0.789f },
},

// -------------------------------------------------------------- DG_HAT
{
    /*0 hcl*/ { .kind = VK_LEGACY, .f0 = 6000.f, .lAmp = 0.42f, .lDec = 0.99800f, .lNoiz = 1.f, .lDrop = 1.f, .lPart = 0.f, .gain = 0.777f },
    /*1 ch808*/ { .kind = VK_METAL, .t60_0 = 0.05f, .g0 = 1.f,
                    .bp1 = 3440.f, .bp1Q = 2.5f, .bp2 = 7100.f, .bp2Q = 2.5f,
                    .hpF = 6000.f, .hpQ = 0.7f, .gain = 5.943f },
    /*2 crash*/ { .kind = VK_METAL, .t60_0 = 0.80f, .t60_1 = 0.48f,
                    .g0 = 1.f, .g1 = 0.7f, .twoBand = true,
                    .bp1 = 3440.f, .bp1Q = 2.5f, .bp2 = 7100.f, .bp2Q = 2.5f,
                    .hpF = 10500.f, .hpQ = 2.f, .gain = 0.812f },
    /*3 hop*/ { .kind = VK_LEGACY, .f0 = 6000.f, .lAmp = 0.38f, .lDec = 0.99965f, .lNoiz = 1.f, .lDrop = 1.f, .lPart = 0.f, .gain = 0.360f },
    /*4 hmd*/ { .kind = VK_LEGACY, .f0 = 6000.f, .lAmp = 0.4f, .lDec = 0.99900f, .lNoiz = 1.f, .lDrop = 1.f, .lPart = 0.f, .gain = 0.534f },
    /*5 wash*/ { .kind = VK_LEGACY, .f0 = 6000.f, .lAmp = 0.33f, .lDec = 0.99988f, .lNoiz = 1.f, .lDrop = 1.f, .lPart = 0.f, .gain = 0.237f },
    /*6 ehat1*/ { .kind = VK_LEGACY, .f0 = 9000.f, .lAmp = 0.4f, .lDec = 0.99770f, .lNoiz = 1.f, .lDrop = 1.f, .lPart = 0.f, .gain = 0.816f },
    /*7 ehat2*/ { .kind = VK_LEGACY, .f0 = 9000.f, .lAmp = 0.38f, .lDec = 0.99880f, .lNoiz = 1.f, .lDrop = 1.f, .lPart = 0.f, .gain = 0.603f },
    /*8 ehat3*/ { .kind = VK_LEGACY, .f0 = 9000.f, .lAmp = 0.36f, .lDec = 0.99955f, .lNoiz = 1.f, .lDrop = 1.f, .lPart = 0.f, .gain = 0.384f },
    /*9 ecym*/ { .kind = VK_LEGACY, .f0 = 7200.f, .lAmp = 0.34f, .lDec = 0.99986f, .lNoiz = 1.f, .lDrop = 1.f, .lPart = 0.f, .gain = 0.251f },
    /*10 oh808*/ { .kind = VK_METAL, .t60_0 = 0.35f, .g0 = 1.f,
                    .bp1 = 3440.f, .bp1Q = 2.5f, .bp2 = 7100.f, .bp2Q = 2.5f,
                    .hpF = 6000.f, .hpQ = 0.7f, .gain = 2.228f },
    /*11 ch909*/ { .kind = VK_METAL, .t60_0 = 0.06f, .g0 = 1.f,
                    .crush = true, .crushBits = 6.f, .crushHz = 32000.f,
                    .bp1 = 3440.f, .bp1Q = 1.2f, .bp2 = 9500.f, .bp2Q = 1.2f,
                    .hpF = 7000.f, .hpQ = 1.f, .noiseMix = 0.4f, .gain = 3.196f },
    /*12 oh909*/ { .kind = VK_METAL, .t60_0 = 0.40f, .g0 = 1.f,
                    .crush = true, .crushBits = 6.f, .crushHz = 32000.f,
                    .bp1 = 3440.f, .bp1Q = 1.2f, .bp2 = 9500.f, .bp2Q = 1.2f,
                    .hpF = 7000.f, .hpQ = 1.f, .noiseMix = 0.4f, .gain = 1.294f },
    /*13 ride*/ { .kind = VK_METAL, .t60_0 = 2.0f, .t60_1 = 0.3f,
                    .g0 = 1.f, .g1 = 0.4f, .twoBand = true,
                    .bp1 = 5000.f, .bp1Q = 1.5f, .bp2 = 7100.f, .bp2Q = 1.5f,
                    .hpF = 9000.f, .hpQ = 1.f,
                    .pingF0 = 2900.f, .pingF1 = 4300.f, .pingT60 = 1.8f, .pingLvl = 0.35f, .gain = 0.462f },
    /*14 noh*/ { .kind = VK_NOISE, .nF = 8000.f, .nQ = 1.5f, .nT60 = 0.6f,
                    .nAtkMs = 0.3f, .nLvl = 1.f, .gain = 0.270f },
    /*15 fmcym*/ { .kind = VK_FM, .f0 = 1100.f, .t60_0 = 0.90f,
                    .fmRatio = 1.414f, .fmIdx0 = 20.f, .fmIdx1 = 20.f, .fmMs = 1.f, .gain = 0.199f },
},

// -------------------------------------------------------------- DG_PERC
{
    /*0 toml*/ { .kind = VK_RESON, .pmode = PM_TOMDEG, .tomOct = 0, .t60_0 = 0.20f,
                    .clickMs = 6.f, .clickBump = 1.5f,
                    .nF = 800.f, .nT60 = 0.24f, .nAtkMs = 0.5f, .nLvl = 0.15f, .gain = 1.693f },
    /*1 chirp*/ { .kind = VK_SWEEP, .pmode = PM_TOMDEG, .tomOct = 2, .t60_0 = 0.06f,
                    .sweepStartR = 4.f, .sweepMs = 30.f, .gain = 3.601f },
    /*2 cowb*/ { .kind = VK_COWBELL, .f0 = 540.f, .f1r = 1.481f,
                    .t60_0 = 0.015f, .t60_1 = 0.60f, .g0 = 1.f, .nQ = 5.f, .gain = 1.650f },
    /*3 clave*/ { .kind = VK_RESON, .f0 = 2500.f, .t60_0 = 0.025f, .drive = 2.f, .gain = 3.751f },
    /*4 tomm*/ { .kind = VK_LEGACY, .f0 = 130.f, .lAmp = 0.92f, .lDec = 0.99982f, .lNoiz = 0.04f, .lDrop = 2.2f, .lPart = 0.f, .gain = 0.525f },
    /*5 tomh*/ { .kind = VK_LEGACY, .f0 = 190.f, .lAmp = 0.9f, .lDec = 0.99978f, .lNoiz = 0.04f, .lDrop = 2.2f, .lPart = 0.f, .gain = 0.423f },
    /*6 bell*/ { .kind = VK_LEGACY, .f0 = 540.f, .lAmp = 0.9f, .lDec = 0.99945f, .lNoiz = 0.04f, .lDrop = 1.f, .lPart = 1.48f, .gain = 0.490f },
    /*7 blip*/ { .kind = VK_LEGACY, .f0 = 0.f, .lAmp = 0.55f, .lDec = 0.99940f, .lNoiz = 0.06f, .lDrop = 1.8f, .lPart = 2.717f, .gain = 0.655f },
    /*8 lztom*/ { .kind = VK_LEGACY, .f0 = 220.f, .lAmp = 0.8f, .lDec = 0.99955f, .lNoiz = 0.04f, .lDrop = 3.f, .lPart = 1.5f, .gain = 0.630f },
    /*9 fmbel*/ { .kind = VK_LEGACY, .f0 = 330.f, .lAmp = 0.78f, .lDec = 0.99950f, .lNoiz = 0.04f, .lDrop = 3.f, .lPart = 2.01f, .gain = 0.538f },
    /*10 mtl*/ { .kind = VK_LEGACY, .f0 = 660.f, .lAmp = 0.74f, .lDec = 0.99930f, .lNoiz = 0.05f, .lDrop = 1.f, .lPart = 3.46f, .gain = 0.597f },
    /*11 cosmo*/ { .kind = VK_LEGACY, .f0 = 0.f, .lAmp = 0.55f, .lDec = 0.99935f, .lNoiz = 0.05f, .lDrop = 2.2f, .lPart = 4.21f, .gain = 0.660f },
    /*12 tom9*/ { .kind = VK_DUAL, .pmode = PM_TOMDEG, .tomOct = 1,
                    .f1r = 1.5f, .t60_0 = 0.30f, .t60_1 = 0.15f, .g0 = 1.f, .g1 = 0.5f,
                    .clickMs = 150.f, .clickBump = 2.f, .useOsc = true,
                    .nF = 2000.f, .nT60 = 0.03f, .nAtkMs = 0.2f, .nLvl = 0.2f, .gain = 0.637f },
    /*13 conga*/ { .kind = VK_RESON, .pmode = PM_TOMDEG, .tomOct = 1, .t60_0 = 0.08f,
                    .clickMs = 4.f, .clickBump = 1.3f, .drive = 1.5f, .gain = 1.583f },
    /*14 laser*/ { .kind = VK_SWEEP, .f0 = 300.f, .t60_0 = 0.25f,
                    .sweepLog = true, .sweepStartR = 20.f, .sweepMs = 250.f, .useSquare = true,
                    .amHz = 40.f, .amDepth = 0.2f, .lpTrackMult = 2.f, .gain = 1.782f },
    /*15 bongo*/ { .kind = VK_RESON, .pmode = PM_TOMDEG, .tomOct = 2,
                    .f1r = 1.59f, .f2r = 2.14f,
                    .t60_0 = 0.15f, .t60_1 = 0.09f, .t60_2 = 0.06f,
                    .g0 = 1.f, .g1 = 0.5f, .g2 = 0.3f,
                    .clickMs = 8.f, .clickBump = 1.25f,
                    .nF = 2000.f, .nT60 = 0.01f, .nAtkMs = 0.2f, .nLvl = 0.15f, .gain = 4.802f },
},
};

// ==================================================================== DrumVoice

void DrumVoice::trigger(const SoundDef& d, const DrumPadCfg& cfg, float vel,
                         uint8_t grp, uint8_t idxGrp, uint8_t sIdx, uint32_t& seed) {
    def = &d;
    group = grp; idxInGrp = idxGrp; soundIdx = sIdx;
    age = 0;
    lenRatio = fastPow2((clampf(cfg.length, 0.f, 1.f) - 0.5f) * 4.f);
    reverse  = cfg.reverse || d.forceReverse;
    accent   = 0.5f + 0.5f * clampf(vel, 0.f, 1.f);
    tone     = clampf(cfg.tone, 0.f, 1.f);

    float pRatio = fastPow2(clampf(cfg.pitch, -1.f, 1.f));
    if (d.pmode == PM_KEYROOT) {
        // KICK ROOT = F1 (43.65 Hz) shifted by the global key — exactly v2's drumPadHit().
        // (scaleHz(0,0) is the synth's F3 root: using it put every kick at 175 Hz = a tom.)
        fBase = scaleHz(0, 0) * 0.25f * pRatio;
        if (d.kind == VK_LEGACY) fBase *= (d.f0 / 52.f);   // v2: each kick's nominal/52 ratio
    } else if (d.pmode == PM_TOMDEG) {
        int degree = scaleTomDegree(lcg01(seed));
        int octJit = (int)(lcg(seed) & 1u);
        fBase = scaleHz(degree, (int)d.tomOct + octJit) * pRatio;
    } else {
        fBase = d.f0 * pRatio;
        if (d.kind == VK_LEGACY && d.f0 <= 1.f)               // v2 "modular blip": random pitch every hit
            fBase = (300.f + (float)(lcg(seed) % 2100u)) * pRatio;
    }
    if (fBase < 10.f) fBase = 10.f;

    // universal TONE colour sweep target (see drums_priv.h comment)
    toneHp = (grp == DG_HAT);
    static constexpr float kLoF = 400.f;          // ..14000 Hz sweep (log2(14000/400) ~= 5.129283)
    toneHz = kLoF * fastPow2(tone * 5.129283f);

    // nominal total length (pre length-ratio) per kind, then scale
    float nomLen;
    switch (d.kind) {
        case VK_RESON: {
            nomLen = d.t60_0;
            if (d.f1r > 0.f && d.t60_1 > nomLen) nomLen = d.t60_1;
            if (d.f2r > 0.f && d.t60_2 > nomLen) nomLen = d.t60_2;
            if (d.nLvl > 0.f && d.nT60 > nomLen) nomLen = d.nT60;
            break;
        }
        case VK_DUAL: {
            nomLen = d.t60_0;
            if (d.t60_1 > nomLen) nomLen = d.t60_1;
            float nl = d.nT60 + d.nHoldMs * 0.001f;
            if (nl > nomLen) nomLen = nl;
            break;
        }
        case VK_SWEEP: {
            nomLen = d.t60_0;
            float sw = d.sweepMs * 0.001f;
            if (sw > nomLen) nomLen = sw;
            break;
        }
        case VK_CLAP:    nomLen = d.tailT60 + d.bN * d.bGapMs * 0.001f; break;
        case VK_NOISE:   nomLen = d.nT60 + d.nHoldMs * 0.001f; break;
        case VK_METAL:   nomLen = d.t60_0 > d.t60_1 ? d.t60_0 : d.t60_1; break;
        case VK_RING:    nomLen = d.t60_0; break;
        case VK_FM:      nomLen = d.fmMs * 0.001f + d.t60_0; break;
        case VK_COWBELL: nomLen = d.t60_1; break;
        case VK_GLITCH:  nomLen = d.t60_0 + d.retrigN * d.retrigGapMs * 0.001f; break;
        case VK_LEGACY:  nomLen = logf(0.0008f) / logf(d.lDec) * DRUM_FS_INV; break;   // v2's env > 0.0008 cutoff
        default:         nomLen = 0.3f; break;
    }
    if (nomLen < 0.02f) nomLen = 0.02f;
    totalLen = nomLen * lenRatio;

    // pan (§ task spec): kick/snare centre; hats 0/1 hard L/R-ish, 2/3 centre;
    // percussion gets a fresh random pan every hit.
    float p;
    if (grp == DG_KICK || grp == DG_SNARE) {
        p = 0.5f;
    } else if (grp == DG_HAT) {
        p = (idxGrp == 0) ? 0.25f : (idxGrp == 1) ? 0.75f : 0.5f;
    } else {
        p = 0.25f + 0.5f * lcg01(seed);
    }
    panL = cosf(p * 1.57079633f);
    panR = sinf(p * 1.57079633f);

    // fresh DSP state so an old tail can never bleed into the new hit
    for (int i = 0; i < 3; i++) { ry1[i] = ry2[i] = 0.f; rR[i] = 0.f; rCw[i] = 1.f; rNorm[i] = 1.f; }
    quietRun = 0;
    ph0 = ph1 = ph2 = 0.f; iph0 = iph1 = iph2 = 0u;
    ampCache = 0.f;
    svfA = Svf(); svfB = Svf(); svfC = Svf(); svfD = Svf();
    dc = DcBlock();
    crushHold = 0.f; crushPhase = 0.f;
    crushLevels = fastPow2(d.crushBits);

    state = VS_ACTIVE;
}

void DrumVoice::startChoke(uint32_t ms) {
    chokeTotal = (uint32_t)(ms * 0.001f * SR);
    if (chokeTotal < 8u) chokeTotal = 8u;
    chokeLeft = chokeTotal;
    state = VS_CHOKE;
}

// ---- shared per-sample helpers ---------------------------------------------

float DrumVoice::env(float t60, float holdS) const {
    if (t60 < 0.003f) t60 = 0.003f;
    float tau = t60 * (1.f / 6.9078f);
    float tNow = (float)age * DRUM_FS_INV;
    float tEff = reverse ? (totalLen - tNow) : tNow;
    if (tEff < 0.f) tEff = 0.f;
    float e = (tEff <= holdS) ? 1.f : fastExp(-(tEff - holdS) / tau);
    if (!reverse) {
        const float atk = 0.0004f;
        if (tNow < atk) e *= 0.5f - 0.5f * cosf(DRUM_PI * (tNow / atk));
    } else {
        const float rel = 0.005f;
        float remain = totalLen - tNow;
        if (remain < rel) {
            float g = remain > 0.f ? remain / rel : 0.f;
            e *= 0.5f - 0.5f * cosf(DRUM_PI * g);
        }
    }
    return e;
}

float DrumVoice::genExp(float start, float end, float Tms) const {
    if (Tms < 0.5f) Tms = 0.5f;
    float tau = (Tms * 0.001f) / 5.f;
    float tNow = (float)age * DRUM_FS_INV;
    float tEff = reverse ? (totalLen - tNow) : tNow;
    if (tEff < 0.f) tEff = 0.f;
    return end + (start - end) * fastExp(-tEff / tau);
}

float DrumVoice::genLog(float start, float end, float Tms) const {
    if (Tms < 0.5f) Tms = 0.5f;
    float T = Tms * 0.001f;
    float tNow = (float)age * DRUM_FS_INV;
    float tEff = reverse ? (totalLen - tNow) : tNow;
    if (tEff < 0.f) tEff = 0.f;
    if (tEff > T) tEff = T;
    float u = 1.f - tEff / T;
    if (end < 1.f) end = 1.f;
    if (start < 1.f) start = 1.f;
    return end * fastExp(u * logf(start / end));
}

void DrumVoice::setReson(int slot, float freq, float t60) {
    if (age != 0u && (age & 7u) != 0u) return;    // recompute at ~6 kHz control-rate, not every sample
    if (freq < 20.f) freq = 20.f;
    if (freq > SR * 0.45f) freq = SR * 0.45f;
    if (t60 < 0.005f) t60 = 0.005f;
    // The amplitude envelope env(t60) shapes the hit (and makes REVERSE work); the
    // resonator itself rings ~4x longer so it never fights the envelope. Q from
    // t60*4: Q = T60*f/2.2 (§0 "bridged-T resonator").
    float Q = (t60 * 4.f * freq) * 0.4545455f;
    if (Q < 0.5f) Q = 0.5f;
    float w = DRUM_TWOPI * freq / SR;
    rR[slot]  = fastExp(-DRUM_PI * freq / (Q * SR));
    rCw[slot] = cosf(w);
    // 2-pole impulse peak ~= 1/sin(w); the exciter pingExc() sums to ~14.4 impulse-units.
    rNorm[slot] = sinf(w) * (1.f / 14.4f);
}

float DrumVoice::tickReson(int slot, float x) {
    float y = 2.f * rR[slot] * rCw[slot] * ry1[slot] - rR[slot] * rR[slot] * ry2[slot] + x;
    ry2[slot] = flushDenorm(ry1[slot]);
    ry1[slot] = flushDenorm(y);
    return y * rNorm[slot];
}

// short exponential click, stands in for the doc's "1 ms pulse, low-passed"
// exciter (§0) — pings the resonator without a discontinuity.
float DrumVoice::pingExc(float amp) const {
    if (age > 32u) return 0.f;
    float t = (float)age * DRUM_FS_INV;
    return amp * fastExp(-t / 0.0003f);
}

float DrumVoice::shape(float x, const SoundDef* d) const {
    if (d->drive > 0.f) x = drumSwing(x, 1.f + d->drive);
    if (d->fold) {
        float y = x;
        if (y > 1.f) y = 2.f - y; else if (y < -1.f) y = -2.f - y;
        if (y > 1.f) y = 2.f - y; else if (y < -1.f) y = -2.f - y;
        x = 0.7f * y + 0.3f * x;
    }
    return x;
}

float DrumVoice::noiseLayer(float noiseSample, const SoundDef* d) {
    if (d->nLvl <= 0.f) return 0.f;
    float holdS = d->nHoldMs * 0.001f;
    float a = env(d->nT60, holdS);
    float atkS = d->nAtkMs * 0.001f;
    float tNow = (float)age * DRUM_FS_INV;
    if (!reverse && atkS > 0.f && tNow < atkS) a *= (tNow / atkS);

    if (d->nF <= 0.f) return noiseSample * a * d->nLvl;    // unfiltered white noise (S10)

    float fc = d->nF;
    if (d->nSweepTo > 0.f) fc = genExp(d->nF, d->nSweepTo, d->sweepMs > 0.f ? d->sweepMs : 60.f);
    if (age == 0u || (d->nSweepTo > 0.f && (age & 7u) == 0u)) svfB.setFc(fc, d->nQ);
    float lp, bp, hp;
    svfB.tick(noiseSample, lp, bp, hp);
    float band = d->nBp ? bp : hp;
    return band * a * d->nLvl;
}

// ---- per-kind engines --------------------------------------------------

float DrumVoice::procReson(const SoundDef* d, float noise) {
    float freq0 = fBase;
    if (d->clickMs > 0.f) freq0 = genExp(fBase * d->clickBump, fBase, d->clickMs);
    if (d->selfFm > 0.f) freq0 *= (1.f + d->selfFm * 0.08f * ampCache);
    setReson(0, freq0, d->t60_0);
    float exc = pingExc(accent);
    float y0 = tickReson(0, exc);
    float a0 = env(d->t60_0);
    ampCache = a0;
    float out = y0 * a0 * (d->g0 > 0.f ? d->g0 : 1.f);

    if (d->f1r > 0.f) {
        setReson(1, fBase * d->f1r, d->t60_1);
        float y1 = tickReson(1, exc);
        out += y1 * env(d->t60_1) * d->g1;
    }
    if (d->f2r > 0.f) {
        setReson(2, fBase * d->f2r, d->t60_2);
        float y2 = tickReson(2, exc);
        out += y2 * env(d->t60_2) * d->g2;
    }
    out += noiseLayer(noise, d);
    return dc.process(shape(out, d)) * d->gain;
}

float DrumVoice::procDual(const SoundDef* d, float noise) {
    float exc = d->useOsc ? 0.f : pingExc(accent);

    float freq0 = fBase;
    if (d->clickMs > 0.f) freq0 = genExp(fBase * d->clickBump, fBase, d->clickMs);
    float a0 = env(d->t60_0);
    ampCache = a0;
    float y0;
    if (d->useOsc) {
        ph0 += DRUM_TWOPI * freq0 * DRUM_FS_INV;
        if (ph0 > DRUM_TWOPI) ph0 -= DRUM_TWOPI;
        float s = sinf(ph0);
        y0 = 2.f * s / (1.f + fabsf(s));        // "rounded" triangle-ish core
    } else {
        setReson(0, freq0, d->t60_0);
        y0 = tickReson(0, exc);
    }
    float out = y0 * a0 * (d->g0 > 0.f ? d->g0 : 1.f);

    if (d->f1r > 0.f) {
        float freq1 = freq0 * d->f1r;
        float a1 = env(d->t60_1);
        float y1;
        if (d->useOsc) {
            ph1 += DRUM_TWOPI * freq1 * DRUM_FS_INV;
            if (ph1 > DRUM_TWOPI) ph1 -= DRUM_TWOPI;
            float s = sinf(ph1);
            y1 = 2.f * s / (1.f + fabsf(s));
        } else {
            setReson(1, freq1, d->t60_1);
            y1 = tickReson(1, exc);
        }
        out += y1 * a1 * d->g1;
    }

    out += noiseLayer(noise, d);
    return dc.process(shape(out, d)) * d->gain;
}

float DrumVoice::procSweep(const SoundDef* d, float noise) {
    float freq = d->sweepLog ? genLog(fBase * d->sweepStartR, fBase, d->sweepMs)
                              : genExp(fBase * d->sweepStartR, fBase, d->sweepMs);
    if (d->vibHz > 0.f) {
        float tNow = (float)age * DRUM_FS_INV;
        freq *= (1.f + d->vibDepth * sinf(DRUM_TWOPI * d->vibHz * tNow));
    }
    float a = env(d->t60_0);
    ampCache = a;

    float y;
    if (d->useSquare) {
        uint32_t inc = (uint32_t)((freq * DRUM_FS_INV) * 4294967296.f);
        iph0 += inc;
        y = (iph0 & 0x80000000u) ? -1.f : 1.f;
    } else {
        ph0 += DRUM_TWOPI * freq * DRUM_FS_INV;
        if (ph0 > DRUM_TWOPI) ph0 -= DRUM_TWOPI;
        y = sinf(ph0);
    }
    if (d->amHz > 0.f) {
        float tNow = (float)age * DRUM_FS_INV;
        float am = 1.f - d->amDepth * (0.5f - 0.5f * sinf(DRUM_TWOPI * d->amHz * tNow));
        y *= am;
    }

    float out = y * a * (d->g0 > 0.f ? d->g0 : 1.f);

    if (d->f1r > 0.f) {
        float f1 = genExp(fBase * d->sweepStartR * 0.8f, fBase * d->f1r, d->sweepMs * 0.8f);
        ph1 += DRUM_TWOPI * f1 * DRUM_FS_INV;
        if (ph1 > DRUM_TWOPI) ph1 -= DRUM_TWOPI;
        out += sinf(ph1) * env(d->t60_1) * d->g1;
    }

    if (d->vcf) {
        float fc = genExp(d->vcfStartHz, d->vcfEndHz, d->sweepMs);
        if (age == 0u || (age & 7u) == 0u) svfA.setFc(fc, d->vcfQ);
        float lp, bp, hp; svfA.tick(out, lp, bp, hp);
        out = lp;
    }
    if (d->lpTrackMult > 0.f) {
        float fc = freq * d->lpTrackMult;
        if (age == 0u || (age & 7u) == 0u) svfC.setFc(fc, 0.9f);
        float lp, bp, hp; svfC.tick(out, lp, bp, hp);
        out = lp;
    }

    out += noiseLayer(noise, d);

    if (d->crush) {
        crushPhase += d->crushHz * DRUM_FS_INV;
        if (crushPhase >= 1.f) {
            crushPhase -= 1.f;
            crushHold = floorf(out * crushLevels + 0.5f) / crushLevels;
        }
        out = crushHold;
    }
    return dc.process(shape(out, d)) * d->gain;
}

float DrumVoice::procClap(const SoundDef* d, float noise) {
    float tNow = (float)age * DRUM_FS_INV;
    float tRef = reverse ? (totalLen - tNow) : tNow;
    if (tRef < 0.f) tRef = 0.f;

    if (age == 0u) svfB.setFc(d->nF, d->nQ);
    float lp, bp, hp; svfB.tick(noise, lp, bp, hp);
    float band = bp;

    uint8_t n = d->bN > 0 ? d->bN : 1;
    float burstTau = d->bDurMs * 0.001f * 0.35f;
    float burstEnv = 0.f;
    for (uint8_t i = 0; i < n; i++) {
        uint8_t bi = reverse ? (uint8_t)(n - 1 - i) : i;   // reverse: mirror burst order
        float dt = tRef - bi * (d->bGapMs * 0.001f);
        if (dt >= 0.f) {
            float e = fastExp(-dt / burstTau);
            if (e > burstEnv) burstEnv = e;
        }
    }
    float tail = env(d->tailT60);
    float atk = d->tailAtkMs * 0.001f;
    if (!reverse && atk > 0.f && tNow < atk) tail *= (tNow / atk);

    ampCache = burstEnv > tail ? burstEnv : tail;
    float out = band * (burstEnv + tail * d->tailLvl);
    return dc.process(shape(out, d)) * d->gain;
}

float DrumVoice::procNoise(const SoundDef* d, float noise) {
    float out = noiseLayer(noise, d);
    out *= (0.9f + 0.2f * lcg01(rnd));      // "random +-1.5 dB per hit" (§ several perc)
    return dc.process(shape(out, d)) * d->gain;
}

float DrumVoice::procMetal(const SoundDef* d, float metal, float noise) {
    float sig = metal;
    if (d->noiseMix > 0.f) sig = sig * (1.f - d->noiseMix) + noise * d->noiseMix;

    float lp, bp, hp;
    if (age == 0u) svfA.setFc(d->bp1, d->bp1Q);
    svfA.tick(sig, lp, bp, hp);
    float band1 = bp;

    if (age == 0u) svfB.setFc(d->bp2, d->bp2Q);
    svfB.tick(sig, lp, bp, hp);
    float band2 = bp;

    float hpCut = d->hpF * (0.4f + 1.2f * tone);      // TONE brightens the final HP corner
    if (age == 0u || (age & 7u) == 0u) svfC.setFc(hpCut, d->hpQ);
    float src = d->twoBand ? band2 : (band1 + band2);
    svfC.tick(src, lp, bp, hp);
    float hiPart = hp;

    float envA = env(d->t60_0);
    ampCache = envA;
    float out;
    if (d->twoBand) {
        float envB = env(d->t60_1 > 0.f ? d->t60_1 : d->t60_0 * 0.6f);
        out = drumSwing(band1, d->g0) * envA + drumSwing(hiPart, d->g1) * envB;
    } else {
        out = drumSwing(hiPart, d->g0) * envA;
    }

    if (d->pingLvl > 0.f) {
        ph0 += DRUM_TWOPI * d->pingF0 * DRUM_FS_INV; if (ph0 > DRUM_TWOPI) ph0 -= DRUM_TWOPI;
        ph1 += DRUM_TWOPI * d->pingF1 * DRUM_FS_INV; if (ph1 > DRUM_TWOPI) ph1 -= DRUM_TWOPI;
        float pingEnv = env(d->pingT60);
        out += (sinf(ph0) + sinf(ph1)) * 0.5f * pingEnv * d->pingLvl;
    }
    if (d->delay2Ms > 0.f) {
        float tNow = (float)age * DRUM_FS_INV;
        float t0 = d->delay2Ms * 0.001f;
        if (tNow >= t0) {
            float e2 = fastExp(-(tNow - t0) / (d->t60_0 * 0.166096f));
            out += hiPart * e2 * d->delay2Lvl;
        }
    }
    if (d->crush) {
        crushPhase += d->crushHz * DRUM_FS_INV;
        if (crushPhase >= 1.f) {
            crushPhase -= 1.f;
            crushHold = floorf(out * crushLevels + 0.5f) / crushLevels;
        }
        out = crushHold;
    }
    return dc.process(out) * d->gain;
}

float DrumVoice::procRing(const SoundDef* d) {
    ph0 += DRUM_TWOPI * (fBase * 0.667f) * DRUM_FS_INV; if (ph0 > DRUM_TWOPI) ph0 -= DRUM_TWOPI;
    iph0 += (uint32_t)((fBase * 25.1f * DRUM_FS_INV) * 4294967296.f);
    float r0 = sinf(ph0) * ((iph0 & 0x80000000u) ? -1.f : 1.f);

    ph1 += DRUM_TWOPI * (fBase * 1.7f) * DRUM_FS_INV; if (ph1 > DRUM_TWOPI) ph1 -= DRUM_TWOPI;
    iph1 += (uint32_t)((fBase * 26.9f * DRUM_FS_INV) * 4294967296.f);
    float r1 = sinf(ph1) * ((iph1 & 0x80000000u) ? -1.f : 1.f);

    ph2 += DRUM_TWOPI * (fBase * 2.43f) * DRUM_FS_INV; if (ph2 > DRUM_TWOPI) ph2 -= DRUM_TWOPI;
    iph2 += (uint32_t)((fBase * 35.0f * DRUM_FS_INV) * 4294967296.f);
    float r2 = sinf(ph2) * ((iph2 & 0x80000000u) ? -1.f : 1.f);

    float sum = (r0 + r1 + r2) * (1.f / 3.f);

    float fc = d->bp1 * (0.4f + 1.2f * tone);
    if (age == 0u || (age & 7u) == 0u) svfA.setFc(fc, d->bp1Q);
    float lp, bp, hp; svfA.tick(sum, lp, bp, hp);

    float a = env(d->t60_0);
    ampCache = a;
    float out = bp * a * (d->g0 > 0.f ? d->g0 : 1.f);
    return dc.process(shape(out, d)) * d->gain;
}

float DrumVoice::procFm(const SoundDef* d, float noise) {
    float carrier = fBase;
    if (d->clickMs > 0.f) carrier = genExp(fBase * d->clickBump, fBase, d->clickMs);
    float idx = genExp(d->fmIdx0, d->fmIdx1, d->fmMs) * (0.5f + 1.f * tone);

    ph1 += DRUM_TWOPI * (carrier * d->fmRatio) * DRUM_FS_INV;
    if (ph1 > DRUM_TWOPI) ph1 -= DRUM_TWOPI;
    float mod = sinf(ph1);

    ph0 += DRUM_TWOPI * carrier * DRUM_FS_INV;
    if (ph0 > DRUM_TWOPI) ph0 -= DRUM_TWOPI;
    float y = sinf(ph0 + idx * mod);

    float a = env(d->t60_0);
    ampCache = a;
    float out = y * a * (d->g0 > 0.f ? d->g0 : 1.f);
    out += noiseLayer(noise, d);
    return dc.process(shape(out, d)) * d->gain;
}

float DrumVoice::procCowbell(const SoundDef* d) {
    float f1 = fBase;
    float f2 = fBase * (d->f1r > 0.f ? d->f1r : 1.481f);

    iph0 += (uint32_t)((f1 * DRUM_FS_INV) * 4294967296.f);
    iph1 += (uint32_t)((f2 * DRUM_FS_INV) * 4294967296.f);
    float sq1 = (iph0 & 0x80000000u) ? -1.f : 1.f;
    float sq2 = (iph1 & 0x80000000u) ? -1.f : 1.f;
    float sum = (sq1 + sq2) * 0.5f;

    float q = (d->nQ > 0.f ? d->nQ : 5.f) * (0.5f + 1.f * tone);
    if (age == 0u) { svfA.setFc(f1, q); svfB.setFc(f2, q); }
    float lp, bp, hp;
    svfA.tick(sum, lp, bp, hp); float b1 = bp;
    svfB.tick(sum, lp, bp, hp); float b2 = bp;
    float band = (b1 + b2) * 0.5f;

    float fast = env(d->t60_0);
    float tailE = env(d->t60_1);
    float envv = 0.6f * fast + 0.4f * tailE;
    ampCache = envv;

    float out = drumSwing(band, d->g0 > 0.f ? d->g0 : 1.5f) * envv;
    return dc.process(shape(out, d)) * d->gain;
}

float DrumVoice::procGlitch(const SoundDef* d, float noise) {
    float tNow = (float)age * DRUM_FS_INV;
    float gap = d->retrigGapMs * 0.001f * lenRatio;
    uint8_t n = d->retrigN > 0 ? d->retrigN : 1;
    uint8_t idx = (uint8_t)(tNow / gap);
    if (idx >= n) idx = n - 1;
    float tLocal = tNow - idx * gap;

    float mul = 1.f;
    for (uint8_t i = 0; i < idx; i++) mul *= d->retrigMul;

    float out;
    if (d->retrigNoise) {
        float e = fastExp(-tLocal / (0.006f * lenRatio));
        out = noise * e;
    } else {
        setReson(0, fBase, d->t60_0);
        float exc = (tLocal < 0.0008f) ? accent * fastExp(-tLocal / 0.0003f) : 0.f;
        float y = tickReson(0, exc);
        float e = fastExp(-tLocal / (d->t60_0 * lenRatio * 0.166096f));
        out = y * e;
    }
    out *= mul;
    ampCache = mul;
    return dc.process(shape(out, d)) * d->gain;
}

// v2 drum engine (PMOR_V2.cpp L2456-2486), per voice. LENGTH scales the per-sample
// decay exponent; REVERSE plays the envelope and the pitch drop time-reversed.
float DrumVoice::procLegacy(const SoundDef* d, float noise) {
    if (age == 0u) {
        legFt = fBase;
        if (legFt < 5.f) legFt = 5.f;
        if (legFt > 16000.f) legFt = 16000.f;
        legF = legFt * d->lDrop;
        if (legF > 20000.f) legF = 20000.f;
        legHatLp = 0.f;
        ph0 = 0.f;
    }
    float tNow = (float)age * DRUM_FS_INV;
    float tEff = reverse ? (totalLen - tNow) : tNow;
    if (tEff < 0.f) tEff = 0.f;
    // envelope: dec^samples, with the length ratio applied to the exponent
    float envv = fastExp(logf(d->lDec) * (tEff * SR) / lenRatio);
    // pitch drop: one-pole 0.0012/sample from ft*drop toward ft (time-reversed when reversed)
    float f = legFt + (legFt * d->lDrop - legFt) * fastExp(-0.0012f * (tEff * SR) / lenRatio);
    ph0 += f * DRUM_FS_INV; if (ph0 >= 1.f) ph0 -= 1.f;
    float tone = sinf(6.2831853f * ph0);
    if (d->lPart > 0.f) tone = 0.6f * tone + 0.45f * sinf(6.2831853f * ph0 * d->lPart);
    float nz = noise;
    if (d->lNoiz > 0.97f) { legHatLp += (nz - legHatLp) * 0.12f; nz -= legHatLp; }   // hat HP
    float sd = (tone * (1.f - d->lNoiz) + nz * d->lNoiz) * envv * d->lAmp * accent;
    if (!reverse && tNow < 0.0004f) sd *= tNow / 0.0004f;                      // anti-click
    // v2 drum bus: x1.5 into tanh(x*2.0) — that saturation IS the beef
    return fxTanhL(sd * 3.0f) * d->gain;
}

float DrumVoice::render(const SoundDef* d, float metal, float noise) {
    float raw;
    switch (d->kind) {
        case VK_RESON:   raw = procReson(d, noise);        break;
        case VK_DUAL:    raw = procDual(d, noise);          break;
        case VK_SWEEP:   raw = procSweep(d, noise);         break;
        case VK_CLAP:    raw = procClap(d, noise);          break;
        case VK_NOISE:   raw = procNoise(d, noise);         break;
        case VK_METAL:   raw = procMetal(d, metal, noise);  break;
        case VK_RING:    raw = procRing(d);                 break;
        case VK_FM:      raw = procFm(d, noise);            break;
        case VK_COWBELL: raw = procCowbell(d);              break;
        case VK_GLITCH:  raw = procGlitch(d, noise);        break;
        case VK_LEGACY:  raw = procLegacy(d, noise);        break;
        default:         raw = 0.f;                         break;
    }
    if (d->kind != VK_METAL && d->kind != VK_LEGACY) {   // METAL/COWBELL/FM wire `tone` in directly; LEGACY = v2 verbatim (tone unused)
        if (age == 0u) svfD.setFc(toneHz, 0.7f);
        float lp, bp, hp; svfD.tick(raw, lp, bp, hp);
        raw = toneHp ? hp : lp;
        // uniform saturation (the v2 bus tanh that made the kicks beefy): gain is a static
        // trim applied AFTER it, so level-matching never changes the amount of grit.
        if (d->satDrive > 0.f) raw = fxTanhL((raw / d->gain) * d->satDrive) * d->gain;
    }
    return flushBad(raw);
}

float DrumVoice::process(float metal, float noise) {
    if (state == VS_IDLE) return 0.f;

    if (state == VS_CHOKE) {
        float raw = def ? render(def, metal, noise) : 0.f;
        float g = (chokeTotal > 0u) ? (0.5f - 0.5f * cosf(DRUM_PI * ((float)chokeLeft / (float)chokeTotal))) : 0.f;
        float out = raw * g;
        age++;
        if (chokeLeft > 0u) chokeLeft--;
        if (chokeLeft == 0u) state = VS_IDLE;
        return out;
    }

    float raw = render(def, metal, noise);
    age++;
    // §6.4 idle: only after 50 ms of CONTINUOUS silence (a per-sample test would
    // kill a sine at its first zero crossing).
    if (fabsf(raw) < 0.0002f) { if (++quietRun > 2400u) state = VS_IDLE; }
    else quietRun = 0;
    return raw;
}
