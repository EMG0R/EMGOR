// drum_sounds.cpp — the 64 synthesized recipes (research/drum-synthesis.md),
// expressed as data (SoundDef, one row per sound) driving ~10 shared engines
// (VKind, see drums_priv.h). Owns the DRUM_DEFS / DRUM_SOUND_NAMES tables and the SD preset override. (The original per-kind voice that
// used to live here is tests/ref/drum_voice_ref.cpp, the reference the synth engine is tested against; the engine is drum_super.cpp.) See drums.cpp for voice management,
// param pages and the public drums* API.
#include "drums_priv.h"
#include <string.h>
static inline float fxTanhL(float z) { if (z > 3.f) return 1.f; if (z < -3.f) return -1.f; float z2 = z * z; return z * (27.f + z2) / (27.f + 9.f * z2); }

// ==================================================================== names
// <=5 chars, upper-case (AGENT_RULES rule 12).
extern const char* const DRUM_SOUND_NAMES[DG_COUNT][DRUM_SOUNDS] = {
    // DG_KICK
    { "KSUB","KCLUB","808","909","KCK1","KCK2","KCK3","K808","K909","KHARD","PHONE","SUB","BOOM","DIST","RUMBL","GLICH",
      "DEEP","PUNCH","TRAP","TAPE","CLICK","THUMP","FOLD","DUST" },
    // DG_SNARE
    { "SNP1","808","GATE","CLAP9","SNP2","SNRF","CHOP","CLAP","ESNR","RIM","909","CLAP8","TIGHT","MODAL","FM","ZAPN",
      "SNAP","ROOM","RIMS","FLAM","CRSH","TRAPS","BRUSH","GLASS" },
    // DG_HAT
    { "HCL","CH808","CRASH","HOP","HMD","WASH","EHAT1","EHAT2","EHAT3","ECYM","OH808","CH909","OH909","RIDE","NOH","FMCYM",
      "TICK","SIZZL","TRSH","PEDAL","SHAKE","OPEN2","BELLH","RING2" },
    // DG_PERC
    { "TOML","CHIRP","COWB","CLAVE","TOMM","TOMH","BELL","BLIP","LZTOM","FMBEL","MTL","COSMO","TOM9","CONGA","LASER","BONGO",
      "GEN1","GEN2","GEN3","TOMLO","TOMHI","FUNK","WOOD","CONG2" },
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
    // ---- stage-2 presets (16..23): use the depth stage (x*) and are level-matched by tools/ measure pass
    /*16 deep*/ { .kind = VK_RESON, .pmode = PM_KEYROOT, .t60_0 = 1.00f,
                    .clickMs = 10.f, .clickBump = 1.5f, .selfFm = 0.2f, .gain = 1.542f, .satDrive = 3.f, .xBody = 0.35f },
    /*17 punch*/ { .kind = VK_DUAL, .pmode = PM_KEYROOT, .t60_0 = 0.12f,
                    .clickMs = 60.f, .clickBump = 5.f, .useOsc = true,
                    .nF = 1500.f, .nT60 = 0.015f, .nAtkMs = 0.3f, .nLvl = 0.4f, .gain = 1.682f, .satDrive = 4.f, .xSnap = 0.5f },
    /*18 trap*/ { .kind = VK_SWEEP, .pmode = PM_KEYROOT, .t60_0 = 1.40f,
                    .sweepStartR = 1.25f, .sweepMs = 25.f, .drive = 0.2f, .gain = 1.221f, .satDrive = 4.f, .xBody = 0.2f },
    /*19 tape*/ { .kind = VK_LEGACY, .pmode = PM_KEYROOT, .f0 = 54.f, .lAmp = 1.f, .lDec = 0.99990f, .lNoiz = 0.06f, .lDrop = 5.f, .lPart = 0.f,
                    .gain = 1.284f, .xDrive = 0.4f, .xCrush = 0.15f },
    /*20 click*/ { .kind = VK_RESON, .pmode = PM_KEYROOT, .t60_0 = 0.22f,
                    .clickMs = 3.f, .clickBump = 2.2f, .gain = 1.667f, .satDrive = 5.f, .xSnap = 0.9f },
    /*21 thump*/ { .kind = VK_RESON, .pmode = PM_KEYROOT, .f1r = 2.f,
                    .t60_0 = 0.20f, .t60_1 = 0.08f, .g0 = 1.f, .g1 = 0.4f, .gain = 4.557f, .xBody = 0.5f, .xDrive = 0.3f },
    /*22 fold*/ { .kind = VK_SWEEP, .pmode = PM_KEYROOT, .t60_0 = 0.40f,
                    .sweepStartR = 2.5f, .sweepMs = 30.f, .drive = 1.2f, .fold = true, .gain = 1.465f, .satDrive = 3.f, .xDrive = 0.2f },
    /*23 dust*/ { .kind = VK_GLITCH, .pmode = PM_KEYROOT, .t60_0 = 0.10f,
                    .retrigN = 3, .retrigGapMs = 18.f, .retrigMul = 0.6f, .gain = 1.884f, .satDrive = 5.f, .xCrush = 0.35f },
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
    // ---- stage-2 presets (16..23)
    /*16 snap*/ { .kind = VK_DUAL, .f0 = 190.f, .f1r = 1.6f,
                    .t60_0 = 0.10f, .t60_1 = 0.06f, .g0 = 0.6f, .g1 = 0.5f,
                    .clickMs = 4.f, .clickBump = 1.3f,
                    .nF = 2200.f, .nT60 = 0.07f, .nAtkMs = 0.2f, .nLvl = 0.5f, .gain = 2.350f, .xSnap = 0.7f },
    /*17 room*/ { .kind = VK_DUAL, .f0 = 170.f, .f1r = 1.7f,
                    .t60_0 = 0.30f, .t60_1 = 0.20f, .g0 = 0.5f, .g1 = 0.4f,
                    .nF = 1400.f, .nT60 = 0.35f, .nHoldMs = 30.f, .nAtkMs = 0.6f, .nLvl = 0.7f, .gain = 1.598f, .xBody = 0.3f },
    /*18 rims*/ { .kind = VK_RESON, .f0 = 450.f, .f1r = 1.6f,
                    .t60_0 = 0.04f, .t60_1 = 0.03f, .g0 = 1.f, .g1 = 0.6f,
                    .nF = 3000.f, .nT60 = 0.02f, .nAtkMs = 0.2f, .nLvl = 0.3f, .gain = 1.722f, .xSnap = 0.5f },
    /*19 flam*/ { .kind = VK_CLAP, .nF = 1300.f, .nQ = 1.6f,
                    .bN = 2, .bGapMs = 25.f, .bDurMs = 9.f,
                    .tailT60 = 0.12f, .tailAtkMs = 6.f, .tailLvl = 0.6f, .gain = 5.402f },
    /*20 crsh*/ { .kind = VK_DUAL, .f0 = 200.f, .f1r = 1.7f,
                    .t60_0 = 0.12f, .t60_1 = 0.08f, .g0 = 0.6f, .g1 = 0.5f,
                    .nF = 1800.f, .nT60 = 0.09f, .nAtkMs = 0.2f, .nLvl = 0.6f, .gain = 2.192f, .xCrush = 0.5f },
    /*21 trap*/ { .kind = VK_DUAL, .f0 = 230.f, .f1r = 1.5f,
                    .t60_0 = 0.09f, .t60_1 = 0.05f, .g0 = 0.6f, .g1 = 0.4f,
                    .nF = 4500.f, .nT60 = 0.08f, .nAtkMs = 0.2f, .nLvl = 0.6f, .gain = 1.798f, .xDrive = 0.4f },
    /*22 brush*/ { .kind = VK_NOISE, .nBp = true, .nF = 3500.f, .nQ = 0.6f, .nT60 = 0.25f,
                    .nAtkMs = 15.f, .nLvl = 1.f, .gain = 12.363f },
    /*23 glass*/ { .kind = VK_FM, .f0 = 420.f, .t60_0 = 0.14f,
                    .clickMs = 10.f, .clickBump = 1.4f,
                    .fmRatio = 3.5f, .fmIdx0 = 9.f, .fmIdx1 = 1.f, .fmMs = 35.f, .gain = 1.984f, .xSnap = 0.4f },
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
    // ---- stage-2 presets (16..23)
    /*16 tick*/ { .kind = VK_METAL, .t60_0 = 0.03f, .g0 = 1.f,
                    .bp1 = 3440.f, .bp1Q = 2.5f, .bp2 = 7100.f, .bp2Q = 2.5f, .hpF = 8000.f, .hpQ = 0.7f, .gain = 9.239f, .xSnap = 0.3f },
    /*17 sizzl*/ { .kind = VK_METAL, .t60_0 = 0.90f, .t60_1 = 0.50f, .g0 = 1.f, .g1 = 0.6f, .twoBand = true,
                    .bp1 = 4200.f, .bp1Q = 2.f, .bp2 = 8800.f, .bp2Q = 2.f, .hpF = 9000.f, .hpQ = 1.f, .noiseMix = 0.3f, .gain = 0.980f },
    /*18 trsh*/ { .kind = VK_METAL, .t60_0 = 0.15f, .g0 = 1.f,
                    .crush = true, .crushBits = 4.f, .crushHz = 16000.f,
                    .bp1 = 3440.f, .bp1Q = 1.5f, .bp2 = 9500.f, .bp2Q = 1.5f, .hpF = 6500.f, .hpQ = 1.f, .noiseMix = 0.5f, .gain = 1.538f, .xCrush = 0.2f },
    /*19 pedal*/ { .kind = VK_METAL, .t60_0 = 0.12f, .g0 = 1.f,
                    .bp1 = 3440.f, .bp1Q = 2.5f, .bp2 = 7100.f, .bp2Q = 2.5f, .hpF = 6000.f, .hpQ = 0.7f,
                    .delay2Ms = 40.f, .delay2Lvl = 0.5f, .gain = 2.887f },
    /*20 shake*/ { .kind = VK_NOISE, .nBp = true, .nF = 9000.f, .nQ = 0.8f, .nT60 = 0.09f,
                    .nAtkMs = 12.f, .nLvl = 1.f, .gain = 2.481f },
    /*21 open2*/ { .kind = VK_METAL, .t60_0 = 0.50f, .g0 = 1.f,
                    .bp1 = 3440.f, .bp1Q = 1.8f, .bp2 = 8000.f, .bp2Q = 1.8f, .hpF = 7000.f, .hpQ = 0.9f, .noiseMix = 0.6f, .gain = 0.389f, .xDrive = 0.2f },
    /*22 bellh*/ { .kind = VK_METAL, .t60_0 = 1.20f, .t60_1 = 0.40f, .g0 = 1.f, .g1 = 0.4f, .twoBand = true,
                    .bp1 = 5000.f, .bp1Q = 1.5f, .bp2 = 7100.f, .bp2Q = 1.5f, .hpF = 9000.f, .hpQ = 1.f,
                    .pingF0 = 3300.f, .pingF1 = 5200.f, .pingT60 = 1.0f, .pingLvl = 0.5f, .gain = 0.460f },
    /*23 ring2*/ { .kind = VK_METAL, .t60_0 = 0.25f, .g0 = 1.f,
                    .bp1 = 2800.f, .bp1Q = 4.f, .bp2 = 6200.f, .bp2Q = 4.f, .hpF = 5000.f, .hpQ = 1.5f,
                    .pingF0 = 4100.f, .pingF1 = 0.f, .pingT60 = 0.4f, .pingLvl = 0.4f, .gain = 0.831f, .xBody = 0.25f },
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
    // ---- stage-2 presets (16..23). 16-18 = generative FM (VK_GENFM): every hit re-rolls
    /*16 gen1*/ { .kind = VK_GENFM, .pmode = PM_TOMDEG, .tomOct = 1, .t60_0 = 0.25f, .g0 = 1.f,
                    .fmIdx0 = 6.f, .gain = 0.935f, .gfChaos = 0.35f },
    /*17 gen2*/ { .kind = VK_GENFM, .pmode = PM_TOMDEG, .tomOct = 2, .t60_0 = 0.18f, .g0 = 1.f,
                    .fmIdx0 = 10.f, .gain = 2.325f, .gfChaos = 0.7f },
    /*18 gen3*/ { .kind = VK_GENFM, .pmode = PM_TOMDEG, .tomOct = 0, .t60_0 = 0.50f, .g0 = 1.f,
                    .fmIdx0 = 4.f, .gain = 0.920f, .gfChaos = 1.f },
    /*19 tomlo*/ { .kind = VK_RESON, .pmode = PM_TOMDEG, .tomOct = 0, .t60_0 = 0.45f,
                    .clickMs = 8.f, .clickBump = 1.6f, .gain = 0.921f, .xBody = 0.3f },
    /*20 tomhi*/ { .kind = VK_RESON, .pmode = PM_TOMDEG, .tomOct = 2, .f1r = 1.6f, .t60_0 = 0.18f, .t60_1 = 0.10f, .g0 = 1.f, .g1 = 0.4f,
                    .clickMs = 5.f, .clickBump = 1.4f, .gain = 5.053f },
    /*21 funk*/ { .kind = VK_RESON, .pmode = PM_TOMDEG, .tomOct = 2, .t60_0 = 0.05f,
                    .clickMs = 2.f, .clickBump = 1.5f, .drive = 1.f, .gain = 3.533f, .xSnap = 0.6f },
    /*22 wood*/ { .kind = VK_RESON, .f0 = 900.f, .f1r = 1.5f, .t60_0 = 0.03f, .t60_1 = 0.02f, .g0 = 1.f, .g1 = 0.5f, .gain = 2.328f },
    /*23 cong2*/ { .kind = VK_RESON, .pmode = PM_TOMDEG, .tomOct = 1, .f1r = 1.9f, .t60_0 = 0.10f, .t60_1 = 0.06f, .g0 = 1.f, .g1 = 0.5f,
                    .clickMs = 4.f, .clickBump = 1.3f, .drive = 1.2f, .gain = 1.215f },
},
};

// ==================================================================== SD preset override (control plane writes, audio reads)
// A recipe installed from PRESETS/<ENGINE>.TXT replaces the table row for that sound. Install happens once per row before the
// flag is raised (write struct, then flag): a trigger that races the install simply sees the table row. No runtime un-install.
// SDRAM storage as RAW BYTES: SoundDef has default member initialisers, so a `SoundDef SDRAM_BSS s_ovr[..]` got a static CONSTRUCTOR that
// ran from __libc_init_array BEFORE hw.Init() had started the SDRAM controller -> bus fault at startup, no USB (4052811/f7a6fe1 never booted).
// tools/fwcheck.sh check 3 now fails the build if any static initialiser references SDRAM.
alignas(8) static uint8_t SDRAM_BSS s_ovrRaw[DG_COUNT][DRUM_SOUNDS][(sizeof(SoundDef) + 7) & ~7u];   // only meaningful where s_ovrOn is set
#define S_OVR(g, s) (*reinterpret_cast<SoundDef*>(s_ovrRaw[g][s]))
static volatile bool s_ovrOn[DG_COUNT][DRUM_SOUNDS];
const SoundDef& drumDef(uint8_t g, uint8_t s) { return s_ovrOn[g][s] ? S_OVR(g, s) : DRUM_DEFS[g][s]; }
size_t      drumsDefSize() { return sizeof(SoundDef); }
const void* drumsDefBlob(uint8_t g, uint8_t s) { return (g < DG_COUNT && s < DRUM_SOUNDS) ? (const void*)&DRUM_DEFS[g][s] : nullptr; }
bool drumsDefInstall(uint8_t g, uint8_t s, const void* blob, size_t n) {
    if (g >= DG_COUNT || s >= DRUM_SOUNDS || n != sizeof(SoundDef) || s_ovrOn[g][s]) return false;
    memcpy(&S_OVR(g, s), blob, n); __sync_synchronize(); s_ovrOn[g][s] = true; return true;
}
bool drumsDefInstalled(uint8_t g, uint8_t s) { return g < DG_COUNT && s < DRUM_SOUNDS && s_ovrOn[g][s]; }

static DrumUserEntry SDRAM_BSS s_usr[DG_COUNT][DRUM_USER_SLOTS];   // SDRAM, valid where s_usrOn
static volatile bool s_usrOn[DG_COUNT][DRUM_USER_SLOTS];
void drumsUserSet(uint8_t g, uint8_t n, uint8_t base, const float k[4], const char* name) {
    if (g >= DG_COUNT || n >= DRUM_USER_SLOTS) return;
    DrumUserEntry& e = s_usr[g][n]; e.base = base < DRUM_SOUNDS ? base : 0; for (int i = 0; i < 4; i++) e.k[i] = k[i];
    strncpy(e.name, name ? name : "", 9); e.name[9] = 0; __sync_synchronize(); s_usrOn[g][n] = true;
}
bool drumsUserGet(uint8_t g, uint8_t n, DrumUserEntry* o) { if (g >= DG_COUNT || n >= DRUM_USER_SLOTS || !s_usrOn[g][n]) return false; *o = s_usr[g][n]; return true; }
int  drumsUserNextFree(uint8_t g) { if (g >= DG_COUNT) return -1; for (int n = 0; n < DRUM_USER_SLOTS; n++) if (!s_usrOn[g][n]) return n; return -1; }
