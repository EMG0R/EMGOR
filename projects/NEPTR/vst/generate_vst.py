#!/usr/bin/env python3
"""Generate vst/NEPTR_VST.csd — the Cabbage plugin version of NEPTR.

Single source of truth for the VST GUI + control mapping. The DSP itself is
#included from ../csound/*.orc (the same files the Pi runs). Rerun after
changing the tables below:

    python3 vst/generate_vst.py            # rewrite the csd
    python3 vst/generate_vst.py --install  # + update installed .vst3/.component
    python3 vst/generate_vst.py --check    # CI: csd matches generator

Layout: one screen, every effect a toggle button in a category grid; clicking
an effect exposes its parameter knobs in the panel below (Cabbage queue mode,
cabbageSet). Presets = Cabbage snapshot system (dropdown + SAVE button).
Knob defaults come from globals.orc `init` lines automatically.
"""
import os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
CSOUND_DIR = os.path.join(ROOT, "csound")
OUT = os.path.join(HERE, "NEPTR_VST.csd")

# ---------------------------------------------------------------- palette
BG = "28, 0, 30"
SLOT_BG = "40, 10, 44"
OFF_OUTLINE = "113, 64, 117"
OFF_TEXT = "155, 114, 158"
EMPTY_OUTLINE = "55, 25, 62"
ACTIVE = "220, 186, 222"
V1 = "202, 42, 235"   # violet-magenta
V2 = "138, 44, 245"   # electric violet
V3 = "245, 44, 218"   # vivid magenta
V4 = "145, 89, 255"   # bright lavender
WHITE = "255, 255, 255"

# ---------------------------------------------------------------- effects
# P: (chan, label, min, max, glob, opts)
#   opts: df=explicit default; scale=global = value*scale; skew=widget skew
#         (0.5 ~ exp feel); inc=increment; combo=[items] with step=per-index
#         multiplier (global = (value-1)*step + off), dfc=default combo value
def P(chan, label, mn, mx, glob, **opts):
    return dict(chan=chan, label=label, mn=mn, mx=mx, glob=glob, **opts)

FX = {
    # ---- VOLUME
    "vol":  dict(label="VOLUME", byp="gkVOLpedalBypass", id=1, claim=1,
                 params=[P("level", "LEVEL", 0, 1, "gkVOLpedal")]),
    "atk":  dict(label="ATTACK", byp="gkATTK_bypass", id=2, claim=2,
                 params=[P("mix", "MIX", 0, 1, "gkATTK_mix", df=1),
                         P("time", "ATK MS", 10, 2000, "gkATTK_atkMs", df=150, skew=0.5),
                         P("sens", "SENS", 0, 1, "gkATTK_sens", df=0.5),
                         P("shape", "SHAPE", 0, 1, "gkATTK_shape", df=0.5),
                         P("range", "RANGE", 0, 1, "gkATTK_range", df=0.7),
                         P("retrig", "RETRIG", 20, 150, "gkATTK_retrigMs", df=45)]),
    "les":  dict(label="LESLIE", byp="gkLeslieBypass", id=10, claim=10,
                 params=[P("speed", "SPEED HZ", 0.1, 10, "gkLeslieSpeed", skew=0.5),
                         P("depth", "MIC DIST", 0, 1, "gkLeslieDepth")]),
    "rm":   dict(label="RINGMOD", byp="gkBypassRM", id=11, claim=11,
                 params=[P("depth", "DEPTH", 0, 1, "gkARModDepth"),
                         P("freq", "FREQ HZ", 0.05, 250, "gkARModFreq", skew=0.25)]),
    "tape": dict(label="TAPE", byp="gkTapeBypass", id=21, claim=21,
                 params=[P("amt", "AMOUNT", 0, 1, "gkTapeAmount", df=0.4),
                         P("fail", "FAIL", 0, 1, "gkTapeFail", df=0.25),
                         P("model", "MODEL", 0, 1, "gkTapeModel", df=0.35)]),
    # ---- SPECTRAL
    "fms":  dict(label="FORMS", byp="gkFormsBypass", id=28, claim=28,
                 params=[P("mix", "MIX", 0, 1, "gkFormsMix"),
                         P("shift", "SHIFT", -12, 12, "gkFormsShift", inc=0.5)]),
    "fmt":  dict(label="FORMANT", byp="gkFormantBypass", id=5, claim=5,
                 params=[P("mix", "MIX", 0, 1, "gkFormantMix"),
                         P("shift", "SHIFT", -12, 12, "gkFormantShift", inc=0.5)]),
    "sdl":  dict(label="SPECDLY", byp="gkSpecDelBypass", id=16, claim=None,
                 params=[P("mix", "MIX", 0, 1, "gkSpecDelMix"),
                         P("maxdel", "MAX DEL", 0, 1, "gkSpecDelMaxDel"),
                         P("fback", "FEEDBK", 0, 1, "gkSpecDelFB")]),
    "bin":  dict(label="BIN REDU", byp="gkBinBypass", id=38, claim=38,
                 params=[P("pedal", "REDUCE", 0, 1, "gkBinPedal", df=1)]),
    "bc":   dict(label="BCRUSHER", byp="gkBcBypass", id=44, claim=44,
                 params=[P("pedal", "COARSE", 0, 1, "gkBcPedal", df=0)]),
    "bsc":  dict(label="BINSCATTER", byp="gkBscBypass", id=67, claim=None,
                 params=[P("mix", "MIX", 0, 1, "gkBscMix", df=0.5),
                         P("scatter", "SCATTER", 0, 1, "gkBscScatter", df=0.7),
                         P("fback", "FEEDBK", 0.5, 1.1, "gkBscFB", df=0.95),
                         P("window", "WINDOW", 0.02, 1.0, "gkBscMaxDel", df=0.15)]),
    # ---- PITCH
    "h90":  dict(label="H90", byp="gkBypassH90", id=26, claim=26,
                 params=[P("mix", "MIX", 0, 1, "gkH90mix"),
                         P("semi", "SEMI", -12, 12, "gkSemiH90", inc=0.5)]),
    "ps":   dict(label="OLD PS", byp="gkBypassPS", id=3, claim=3,
                 params=[P("mix", "DRY %", 0, 100, "gkDryPctPS", inc=5),
                         P("semi", "SEMI", -12, 12, "gkSemi", inc=0.5)]),
    "gps":  dict(label="GPS", byp="gkBypassGPS", id=4, claim=4,
                 params=[P("mix", "MIX", 0, 1, "gkGPSmix"),
                         P("semi", "SEMI", -12, 12, "gkGPSsemis", inc=0.5),
                         P("fback", "FEEDBK", 0, 0.99, "gkGPSfback"),
                         P("dly", "DELAY", 0, 0.1, "gkGPSdly")]),
    "pf":   dict(label="P-FORK", byp="gkBypassPF", id=27, claim=27,
                 params=[P("mix", "MIX", 0, 1, "gkPFmix"),
                         P("semi", "SEMI", -12, 12, "gkSemiPF", inc=2.5)]),
    # ---- FILTERING
    "lsy":  dict(label="LOSSY", byp="gkLsyBypass", id=46, claim=None,
                 params=[P("mix", "MIX", 0, 1, "gkLSY_mix", df=1),
                         P("loss", "LOSS", 0, 1, "gkLSY_loss", df=0.35),
                         P("bw", "BANDW", 0, 1, "gkLSY_bw", df=0.8),
                         P("pkt", "PACKETS", 0, 1, "gkLSY_pkt", df=0)]),
    "res":  dict(label="RESONATR", byp="gkRES_bypass", id=47, claim=None,
                 params=[P("mix", "MIX", 0, 1, "gkRES_mix", df=0.4),
                         P("onset", "ONSET", 0, 1, "gkRES_onset", df=0.5),
                         P("fb", "FEEDBK", 0, 1, "gkRES_fb", df=0.6),
                         P("oct", "OCTAVE", -1, 1, "gkRES_oct", df=0),
                         P("frz", "FREEZE", 0, 1, "gkRES_frz", combo=["RUN", "FREEZE"], step=1)]),
    "wah":  dict(label="WAH", byp="gkWAH_bypass", id=22, claim=22,
                 params=[P("freq", "FREQ", 0, 1, "gkWAH_freq"),
                         P("res", "RES", 0.5, 0.97, "gkWAH_res")]),
    "mog":  dict(label="AUTOMOOG", byp="gkAUTOMOOG_bypass", id=14, claim=None,
                 params=[P("sens", "SENS", 0, 1, "gkAUTOMOOG_sens"),
                         P("res", "RES", 0, 0.95, "gkAUTOMOOG_res"),
                         P("type", "TYPE", 1, 4, "gkAUTOMOOG_type",
                           combo=["TYPE 1", "TYPE 2", "TYPE 3", "TYPE 4"], step=1, off=1),
                         P("level", "LEVEL", 0, 2, "gkAUTOMOOG_level")]),
    # ---- MOD
    "vib":  dict(label="VIBRATO", byp="gkVIB_bypass", id=6, claim=None,
                 params=[P("depth", "DEPTH", 0, 1, "gkVIB_depth", scale=0.005),
                         P("rate", "RATE HZ", 0.05, 12, "gkVIB_freq", skew=0.5),
                         P("fback", "FEEDBK", 0, 0.7, "gkVIB_fback")]),
    "cho":  dict(label="CHORUS", byp="gkBypassChorus", id=7, claim=None,
                 params=[P("mix", "MIX", 0, 1, "gkChorusMix"),
                         P("rate", "RATE HZ", 0.05, 8, "gkChorusRate", skew=0.5),
                         P("depth", "DEPTH", 0, 1, "gkChorusDepth")]),
    "pha":  dict(label="PHASER", byp="gkPHA_bypass", id=8, claim=8,
                 params=[P("depth", "DEPTH", 0, 1, "gkPHA_depth"),
                         P("rate", "RATE HZ", 0.05, 12, "gkPHA_rate", skew=0.5),
                         P("fback", "FEEDBK", 0, 0.9, "gkPHA_fback"),
                         P("stages", "STAGES", 1, 24, "gkPHA_stages", inc=1)]),
    "trm":  dict(label="TREMOLO", byp="gkTremBypass", id=43, claim=None,
                 params=[P("depth", "DEPTH", 0, 1, "gkTremDepth", df=0.7),
                         P("mode", "STYLE", 0, 3, "gkTremMode",
                           combo=["SINE", "OPTO", "BIAS", "HARM"], step=1),
                         P("rate", "RATE HZ", 0.5, 20, "gkTremRate", df=7.38, skew=0.5)]),
    "fla":  dict(label="FLANGER", byp="gkFLA_bypass", id=9, claim=None,
                 params=[P("depth", "DEPTH", 0, 1, "gkFLA_depth", scale=0.01),
                         P("rate", "RATE HZ", 0.05, 12, "gkFLA_rate", skew=0.5),
                         P("fback", "FEEDBK", -1, 1, "gkFLA_fback")]),
    # ---- DRIVE
    "od":   dict(label="TUBESCREAM", byp="gkOD_bypass", id=31, claim=31,
                 params=[P("drive", "DRIVE", 0, 1, "gkOD_drive", df=0.4),
                         P("tone", "TONE", 0, 1, "gkOD_tone"),
                         P("morph", "BODY", 0, 1, "gkOD_wc1"),
                         P("blend", "CLEAN", 0, 1, "gkOD_wc2")]),
    "mar":  dict(label="MARSHALL", byp="gkMAR_bypass", id=32, claim=32,
                 params=[P("drive", "DRIVE", 0, 1, "gkMAR_drive", df=0.75),
                         P("tone", "TONE", 0, 1, "gkMAR_tone"),
                         P("pres", "PRESENCE", 0, 1, "gkMAR_wc1"),
                         P("tight", "TIGHT", 0, 1, "gkMAR_wc2")]),
    "rmz":  dict(label="RAT-MZ", byp="gkRMZ_bypass", id=34, claim=34,
                 params=[P("drive", "DRIVE", 0, 1, "gkRMZ_drive", df=0.75),
                         P("tone", "FILTER", 0, 1, "gkRMZ_tone"),
                         P("midf", "MID FRQ", 0, 1, "gkRMZ_wc1"),
                         P("midg", "MID GAIN", 0, 1, "gkRMZ_wc2")]),
    "dhg":  dict(label="DS-1K", byp="gkDHG_bypass", id=33, claim=33,
                 params=[P("drive", "DRIVE", 0, 1, "gkDHG_drive", df=0.75),
                         P("tone", "TONE", 0, 1, "gkDHG_tone"),
                         P("blend", "FAT", 0, 1, "gkDHG_wc1"),
                         P("tight", "EDGE", 0, 1, "gkDHG_wc2")]),
    "muf":  dict(label="BIGMUFF", byp="gkMUF_bypass", id=35, claim=35,
                 params=[P("drive", "SUSTAIN", 0, 1, "gkMUF_drive", df=0.75),
                         P("tone", "TONE", 0, 1, "gkMUF_tone"),
                         P("mids", "MIDS", 0, 1, "gkMUF_wc1"),
                         P("voice", "VOICING", 0, 1, "gkMUF_wc2")]),
    # ---- PREAMP
    "pre":  dict(label="MAIN PRE", byp="gkPreamp_bypass", id=39, claim=None, params=[]),
    "bpl":  dict(label="BASS PRE L", byp="gkBPL_bypass", id=40, claim=None, radio="bph",
                 params=[P("drive", "DRIVE", 0, 1, "gkBPL_drive")]),
    "bph":  dict(label="BASS PRE H", byp="gkBPH_bypass", id=41, claim=None, radio="bpl",
                 params=[P("drive", "DRIVE", 0, 1, "gkBPH_drive")]),
    "bds":  dict(label="BASS DIST", byp="gkBDS_bypass", id=42, claim=42,
                 params=[P("drive", "DRIVE", 0, 1, "gkBDS_drive"),
                         P("blend", "CLEAN LOW", 0, 1, "gkBDS_blend"),
                         P("tone", "TONE", 0, 1, "gkBDS_tone")]),
    "cmp":  dict(label="COMPRESS", byp="gkCMP_bypass", id=57, claim=None,
                 params=[P("amt", "AMT", 0, 1, "gkCMP_amt"),
                         P("speed", "SPEED", 0, 1, "gkCMP_speed"),
                         P("makeup", "MAKEUP", 0, 1, "gkCMP_makeup"),
                         P("blend", "BLEND", 0, 1, "gkCMP_blend"),
                         P("gate", "GATE", 0, 1, "gkCMP_gate")]),
    # ---- DELAY
    "dly":  dict(label="DELAY", byp="gkBypassDelay", id=15, claim=15,
                 params=[P("mix", "MIX", 0, 1, "gkDelMix"),
                         P("time", "TIME MS", 0, 700, "gkDelayMS", df=350, scale=1000),
                         P("fback", "FEEDBK", 0, 1.2, "gkFeedback"),
                         P("semi", "SEMI", -5, 12, "gkSemiDel", inc=1),
                         P("mode", "REPEATS", 0, 1, "gkDlyMode",
                           combo=["NORMAL", "REVERSE"], step=1)]),
    "tdl":  dict(label="TAPE DELAY", byp="gkTdlBypass", id=68, claim=None,
                 params=[P("mix", "MIX", 0, 1, "gkTdlMix", df=0.35),
                         P("time", "TIME MS", 60, 1200, "gkTdlTimeMs", df=300, skew=0.5),
                         P("fback", "FEEDBK", 0, 1.15, "gkTdlFeedback", df=0.35),
                         P("fctr", "FILT CTR", 250, 5000, "gkTdlFiltCtr", df=1200, skew=0.5),
                         P("fq", "FILT Q", 0.5, 6, "gkTdlFiltQ", df=0.7),
                         P("head", "HEAD MODE", 1, 7, "gkTdlHeadMode",
                           combo=["1", "2", "3", "2+3", "1+2", "1+3", "1+2+3"],
                           step=1, off=1, dfc=1)]),
    "mvd":  dict(label="POLY DLY", byp="gkMvdBypass", id=69, claim=69,
                 params=[P("numl", "NUM L", 1, 16, "gkMvdNumL", df=1),
                         P("denl", "DEN L", 1, 16, "gkMvdDenL", df=2),
                         P("numr", "NUM R", 1, 16, "gkMvdNumR", df=1),
                         P("denr", "DEN R", 1, 16, "gkMvdDenR", df=3),
                         P("att", "ATTACK", 0, 1, "gkMvdAtt", df=0),
                         P("dec", "DECAY", 0, 1, "gkMvdDec", df=0),
                         P("reps", "REPEATS", 1, 8, "gkMvdReps", df=4),
                         P("intv", "INTERVAL", -12, 12, "gkMvdIntv", df=7)]),
    # ---- DIGIVERB
    "neb":  dict(label="NEBULA", byp="gkNebBypass", id=70, claim=70,
                 params=[P("mix", "MIX", 0, 1, "gkNebMix", df=1),
                         P("dens", "DENSITY", 0, 1, "gkNebDensity", df=0.4),
                         P("size", "SIZE", 0, 1, "gkNebSize", df=0.6)]),
    "shm":  dict(label="SHIMMER", byp="gkShmBypass", id=71, claim=None,
                 params=[P("mix", "MIX", 0, 1, "gkShmMix", df=1),
                         P("intv", "INTERVAL", 0, 3, "gkShmInterval",
                           combo=["+12", "+7", "+5", "-12"], step=1, off=0, dfc=1),
                         P("bloom", "BLOOM", 0, 1, "gkShmFbAmt", df=0.5)]),
    "frz":  dict(label="FREEZE", byp="gkFrzBypass", id=72, claim=72,
                 params=[P("bed", "BED LVL", 0, 1, "gkFrzBedLevel", df=0.8),
                         P("damp", "DAMP", 0, 1, "gkFrzDamp", df=0.5),
                         P("listen", "LISTEN", 0, 1, "gkFrzListenGain", df=0.5)]),
    "gvb":  dict(label="GRAINVERB", byp="gkGvbBypass", id=73, claim=None,
                 params=[P("mix", "MIX", 0, 1, "gkGvbMix", df=1),
                         P("spacing", "SPACING", 0, 1, "gkGvbSpacing", df=0.5),
                         P("decay", "DECAY", 0, 1, "gkGvbDecay", df=0.5)]),
    "vrb":  dict(label="REVERB", byp="gkverbBypass", id=19, claim=None,
                 params=[P("mix", "MIX", 0, 1, "gkverbMix"),
                         P("room", "ROOM", 0, 0.999, "gkReverbFB", df=0.92),
                         P("damp", "DAMP", 0, 1, "gkReverbDamp")]),
    # ---- GRANULAR
    "grn":  dict(label="GRANULAR", byp="gkGranBypass", id=18, claim=None,
                 params=[P("mix", "MIX", 0, 1, "gkGranMix"),
                         P("dens", "DENSITY", 10, 400, "gkGranDens", inc=10),
                         P("size", "SIZE", 0.05, 1.2, "gkGranSize"),
                         P("pitch", "PITCH", 0, 24, "gkGranPitch", inc=1)]),
    "mod":  dict(label="MOOD", byp="gkMOOD_Bypass", id=17, claim=17,
                 params=[P("mix", "MIX", 0, 1, "gkMOOD_Mix"),
                         P("clock", "PITCH", 0, 1, "gkMOOD_Clock"),
                         P("dens", "DENSITY", 0, 1400, "gkMOOD_Dens"),
                         P("dly", "MAX DLY", 0, 1, "gkMOOD_Dly")]),
    # ---- BASS
    "oct":  dict(label="OCTAVER", byp="gkOCT_bypass", id=54, claim=None,
                 params=[P("sub", "SUB", 0, 1, "gkOCT_sub"),
                         P("tone", "TONE", 0, 1, "gkOCT_tone"),
                         P("char", "CHAR", 0, 1, "gkOCT_char")]),
    "env":  dict(label="ENV FILTER", byp="gkENV_bypass", id=55, claim=None,
                 params=[P("wet", "WET", 0, 1, "gkENV_wet"),
                         P("sens", "SENS", 0, 1, "gkENV_sens"),
                         P("mode", "MODE", 0, 1, "gkENV_mode"),
                         P("att", "ATTACK", 0, 1, "gkENV_att"),
                         P("dec", "DECAY", 0, 1, "gkENV_dec")]),
    # ---- SYNTH
    "tsy":  dict(label="TRACKSYN", byp="gkTSY_bypass", id=61, claim=61,
                 params=[P("mix", "BLEND", 0, 1, "gkTSY_mix"),
                         P("cut", "CUTOFF", 0, 1, "gkTSY_cut"),
                         P("res", "RES", 0, 1, "gkTSY_res"),
                         P("glide", "GLIDE", 0, 1, "gkTSY_glide"),
                         P("sub", "SUB", 0, 1, "gkTSY_sub"),
                         P("det", "DETUNE", 0, 1, "gkTSY_det")]),
    "pll":  dict(label="PLLFUZZ", byp="gkPLL_bypass", id=62, claim=None,
                 params=[P("mix", "DRY/WET", 0, 1, "gkPLL_mix"),
                         P("intv", "INTERVAL", 0, 4, "gkPLL_intv",
                           combo=["/4", "/2", "x1", "x2", "x3"], step=1),
                         P("lock", "LOCK SPD", 0, 1, "gkPLL_lock"),
                         P("vmix", "VOICE", 0, 1, "gkPLL_vmix")]),
    "teq":  dict(label="TONE EQ", byp="gkTEQ_bypass", id=58, claim=None,
                 params=[P("mid", "MID", 0, 1, "gkTEQ_mid", df=0.5),
                         P("freq", "FREQ", 0, 1, "gkTEQ_freq", df=0.5),
                         P("tilt", "TILT", 0, 1, "gkTEQ_tilt", df=0.5),
                         P("low", "LOW", 0, 1, "gkTEQ_low", df=0.5),
                         P("high", "HIGH", 0, 1, "gkTEQ_high", df=0.5)]),
    # ---- AIR
    "fbk":  dict(label="FEEDBACKER", byp="gkFBK_bypass", id=63, claim=63,
                 params=[P("level", "LEVEL", 0, 1, "gkFBK_level"),
                         P("harm", "HARMONIC", 0, 0.75, "gkFBK_harm",
                           combo=["FUND", "+OCT", "+OCT5", "+2OCT"], step=0.25),
                         P("bloom", "BLOOM", 0, 1, "gkFBK_bloom"),
                         P("vib", "VIBRATO", 0, 1, "gkFBK_vib")]),
    "dif":  dict(label="DIFFUSION", byp="gkDIF_bypass", id=64, claim=None,
                 params=[P("wet", "DRY/WET", 0, 1, "gkDIF_wet"),
                         P("depth", "DEPTH", 0, 1, "gkDIF_depth"),
                         P("rate", "MOD RATE", 0, 1, "gkDIF_rate"),
                         P("duck", "DUCK", 0, 1, "gkDIF_duck")]),
    "cfb":  dict(label="CHAIN FB", byp="gkCFB_bypass", id=60, claim=None,
                 params=[P("ret", "RETURN", 0, 1, "gkCFB_ret", df=0.4),
                         P("gain", "FEEDBK", 0, 1, "gkCFB_gain", df=0.5),
                         P("tone", "TONE", 0, 1, "gkCFB_tone", df=0.5),
                         P("ceil", "CEILING", 0.1, 0.95, "gkCFB_ceil", df=0.8)]),
    # ---- CHASE BLISS
    "blp":  dict(label="BLOOPER", byp="gkBLP_bypass", id=56, claim=56,
                 params=[P("motion", "MOTION", 0, 1, "gkBLP_Motion", df=0.45),
                         P("scat", "MOD AMT", 0, 1, "gkBLP_Scat", df=0.45),
                         P("size", "GRAIN", 0, 1, "gkBLP_Size", df=0.5),
                         P("char", "THROW", 0, 2, "gkBLP_Char",
                           combo=["NEAR", "THROW", "FAR"], step=1, dfc=2)]),
    "onw":  dict(label="ONWARD", byp="gkONW_bypass", id=45, claim=45,
                 params=[P("mix", "MIX", 0, 1, "gkONW_mix", df=0.6),
                         P("blend", "BLEND", 0, 1, "gkONW_blend", df=0.75),
                         P("size", "SIZE", 0, 1, "gkONW_size", df=0.3),
                         P("follow", "FOLLOW", 0, 1, "gkONW_follow", df=0.6),
                         P("density", "DENSITY", 0, 1, "gkONW_density", df=0.15),
                         P("err", "ERROR", 0, 1, "gkONW_err", df=0.35),
                         P("oct", "OCTAVE", -1, 1, "gkONW_oct", df=0, inc=1)]),
    "hab":  dict(label="HABIT", byp="gkHabitBypass", id=36, claim=36,
                 params=[P("mix", "MIX", 0, 1, "gkHabitMix"),
                         P("size", "SIZE", 0, 1, "gkHabitSize"),
                         P("repeats", "REPEATS", 0, 1, "gkHabitRepeats"),
                         P("blend", "BLEND", 0, 1, "gkHabitBlend"),
                         P("rec", "BUFFER", 0, 1, "gkHab_rec",
                           combo=["FROZEN", "REC"], step=1, dfc=2)]),
    "md2":  dict(label="MOOD II", byp="gkMD2_bypass", id=66, claim=66,
                 params=[P("mix", "MIX", 0, 1, "gkMD2_Mix"),
                         P("clock", "CLOCK", 0, 1, "gkMD2_Clock"),
                         P("length", "LENGTH", 0, 1, "gkMD2_Length"),
                         P("gen", "GENERATE", 0, 1, "gkMD2_Gen")]),
    # ---- GLITCH
    "imp":  dict(label="IMPULSE", byp="gkIMP_bypass", id=48, claim=48,
                 params=[P("mix", "MIX", 0, 1, "gkIMP_mix", df=0.5),
                         P("prob", "PROB", 0, 1, "gkIMP_prob", df=0.5),
                         P("dec", "DECAY", 0, 1, "gkIMP_dec", df=0.25),
                         P("tone", "TONE", 0, 1, "gkIMP_tone", df=0.5)]),
    "bbc":  dict(label="BBCUT", byp="gkBBcut_bypass", id=20, claim=20,
                 params=[P("mix", "MIX", 0, 1, "gkBBcut_drywet"),
                         P("prob", "PROB", 0, 1, "gkBBcut_prob", df=0.5),
                         P("flt", "FILTER", 0, 1, "gkBBcut_fltMix")]),
    "rpt":  dict(label="REPEAT", byp="gkWGuide_bypass", id=23, claim=23,
                 params=[P("mix", "MIX", 0, 1, "gkWGuide_drywet"),
                         P("prob", "PROB", 0, 1, "gkRPT_prob", df=0.5),
                         P("char", "CHAR", 0, 1, "gkRPT_char"),
                         P("len", "LEN", 0, 2, "gkRPT_len",
                           combo=["1/2 TICK", "1 TICK", "2 TICK"], step=1, dfc=2)]),
    "chp":  dict(label="CHOP", byp="gkSqMod_bypass", id=24, claim=24,
                 params=[P("depth", "DEPTH", 0, 1, "gkSqMod_drywet"),
                         P("prob", "PROB", 0, 1, "gkCHP_prob", df=0.5),
                         P("rate", "RATE", 0, 1, "gkCHP_rate"),
                         P("shape", "SHAPE", 0, 1, "gkCHP_shape")]),
    "rvs":  dict(label="RVRS", byp="gkFShift_bypass", id=25, claim=25,
                 params=[P("mix", "MIX", 0, 1, "gkFShift_drywet"),
                         P("prob", "PROB", 0, 1, "gkRVS_prob", df=0.5),
                         P("len", "LEN", 0, 1, "gkRVS_len")]),
}

# CC29 expression dispatch: claim id -> csound statement using kEx (0..1)
EXPR = {
    1:  'gkVOLpedal = kEx',
    2:  'gkATTK_sens = kEx',
    3:  'gkSemi = kEx*24 - 12',
    4:  'gkGPSsemis = kEx*24 - 12',
    5:  'gkFormantShift = kEx*24 - 12',
    8:  'gkPHA_fback = kEx*0.9',
    10: 'gkLeslieSpeed = 0.1 * exp(log(100) * kEx)',
    11: 'gkARModFreq = 0.05 * exp(log(5000) * kEx)',
    15: 'gkDelayMS = kEx*700000',
    17: 'gkMOOD_Clock = 1 - kEx',
    20: 'gkBBcut_expr = kEx',
    21: 'gkTapeExpr = kEx',
    22: 'gkWAH_freq = kEx',
    23: 'gkRPT_expr = kEx',
    24: 'gkCHP_expr = kEx',
    25: 'gkRVS_expr = kEx',
    26: 'gkSemiH90 = kEx*24 - 12',
    27: 'gkSemiPF = kEx*24 - 12',
    28: 'gkFormsShift = kEx*24 - 12',
    30: 'gkMasterVol = kEx',
    31: 'gkOD_drive = kEx',
    32: 'gkMAR_drive = kEx',
    33: 'gkDHG_drive = kEx',
    34: 'gkRMZ_drive = kEx',
    35: 'gkMUF_drive = kEx',
    36: 'gkHabitScan = kEx',
    38: 'gkBinPedal = kEx',
    42: 'gkBDS_drive = kEx',
    44: 'gkBcPedal = kEx',
    45: 'gkONW_follow = kEx',
    48: 'gkIMP_expr = kEx',
    56: 'gkBLP_expr = kEx',
    61: 'gkTSY_cut = kEx',
    63: 'gkFBK_expr = kEx',
    66: 'gkMD2_Clock = kEx',
    69: 'gkMvdExpr = kEx',
    70: 'gkNebWarp = kEx',
    72: 'gkFrzExpr = kEx',
}
CLAIM_NAMES = {0: "-", 30: "MASTER VOL"}
for c, fx in FX.items():
    if fx.get("claim"):
        CLAIM_NAMES[fx["claim"]] = fx["label"]

# Category grid: every effect on screen at once. "NAM"/"IRVERB" are the
# row-button groups (shared param panels).
CATS = [
    ("VOLUME",   ["vol", "atk", "les", "rm", "tape"]),
    ("SPECTRAL", ["fms", "fmt", "bsc", "bin", "bc"]),
    ("PITCH",    ["h90", "ps", "gps", "pf"]),
    ("FILTER",   ["lsy", "res", "wah", "mog"]),
    ("MOD",      ["vib", "cho", "pha", "trm", "fla"]),
    ("DRIVE",    ["od", "mar", "rmz", "dhg", "muf"]),
    ("PREAMP",   ["pre", "bpl", "bph", "bds", "cmp"]),
    ("GRANULAR", ["grn", "mod"]),
    # DIGIVERB target order (phase 2, user mandate): NEBULA, SHIMMER, FREEZE,
    # GRAINVERB, then REVERB LAST.
    ("DIGIVERB", ["neb", "shm", "frz", "gvb", "vrb"]),
    # DELAY: SPECDLY (sdl) moved here; BINSCATTER (bsc, split out of SPECDLY)
    # lives in SPECTRAL instead.
    ("DELAY",    ["dly", "sdl", "tdl", "mvd"]),
    ("BASS",     ["oct", "env"]),
    ("SYNTH",    ["tsy", "pll", "teq"]),
    ("AIR",      ["fbk", "dif", "cfb"]),
    ("CH BLISS", ["blp", "onw", "hab", "md2"]),
    ("GLITCH",   ["imp", "bbc", "rpt", "chp", "rvs"]),
    ("NAM",      "NAM"),
    ("IR VERB",  "IRVERB"),
]
NAM_ROWS = ["AC30", "DELUXE", "FENDER", "TWEED", "CRUNCH"]      # row 0..4
IR_ROWS = ["SPRING", "PLATE", "HALL", "CHAMBER", "WOOD RM"]     # row 0..4

# LOOPER speed combo labels, index-aligned with giLpSpeeds (globals.orc):
# -2, -1, -0.5, -0.25, 0.25, 0.5, 1, 2 -> index 6 ("+1x") is the boot default.
LOOPER_SPEED_LABELS = ["-2x", "-1x", "-1/2", "-1/4", "+1/4", "+1/2", "+1x", "+2x"]
LOOPER_STATE_LABELS = {0: "IDLE", 1: "REC", 2: "PLAY", 3: "OVD", 4: "STOP", 5: "SYNC",
                        6: "TRIM"}

# selection indices for the shared panels

# Intentional short button names (grid only; panels keep the full label).
SHORT = {
    "rm": "RINGMOD", "fmt": "FORMANT", "sdl": "SPECDLY", "bin": "BINRED",
    "bc": "BCRUSH", "res": "RESON", "mog": "MOOG", "vib": "VIBRATO",
    "trm": "TREMOLO", "fla": "FLANGER", "od": "OD", "mar": "MARSH",
    "muf": "BIGMUFF", "pre": "MAINPRE", "bpl": "BASS L", "bph": "BASS H",
    "bds": "BASSDST", "cmp": "COMP", "grn": "GRAIN", "oct": "OCTAVER",
    "env": "ENVFLT", "tsy": "TRKSYN", "pll": "PLLFUZZ", "teq": "TONE EQ",
    "fbk": "FEEDBK", "dif": "DIFFUS", "cfb": "CHAINFB", "blp": "BLOOPER",
    "imp": "IMPULSE",
}
def grid_name(code):
    return SHORT.get(code, FX[code]["label"])

FX_ORDER = list(FX.keys())
SEL = {code: i for i, code in enumerate(FX_ORDER)}
SEL_NAM = len(FX_ORDER)
SEL_IR = len(FX_ORDER) + 1

# ---------------------------------------------------------------- defaults
def load_inits():
    inits = {}
    with open(os.path.join(CSOUND_DIR, "globals.orc")) as f:
        for line in f:
            m = re.match(r"\s*(g[ki]\w+)\s+init\s+(-?[\d.]+)", line)
            if m:
                inits[m.group(1)] = float(m.group(2))
    return inits

INITS = load_inits()

# NAM: same plugin opcode NAMDrop uses (proven to load inside Cabbage) +
# the repo's 25-profile live set. IR names come from the wav filenames.
NAM_LIB = os.path.expanduser("~/Library/CsoundPlugins/csound-nam.so")
NAM_PROFILE_DIR = os.path.join(CSOUND_DIR, "nam", "profiles", "live")
IR_DIR_PATH = os.path.join(CSOUND_DIR, "ir")

def variant_names(dirpath, ext):
    """Row-major variant display names from <row+1><variant+1>_name.<ext>."""
    names = {}
    if os.path.isdir(dirpath):
        for f in sorted(os.listdir(dirpath)):
            m = re.match(r"([1-5])([1-5])_(.+)\." + ext + r"$", f)
            if m:
                r, v = int(m.group(1)) - 1, int(m.group(2)) - 1
                names.setdefault(r, {})[v] = m.group(3).replace("_", " ").upper()
    return [[names.get(r, {}).get(v, f"V{v + 1}") for v in range(5)]
            for r in range(5)]

NAM_VARIANTS = variant_names(NAM_PROFILE_DIR, "nam")
IR_VARIANTS = variant_names(IR_DIR_PATH, "wav")

# ------------------------------------------------------------ state registry
# One flat table of every persistable value, in GLOBAL domain. Both platforms
# include the generated csound/state_persist.orc built from this list, so the
# Pi and the VST stay locked together by construction. Widget-domain
# conversions (for the standalone app's GUI restore) are derived per entry.
MAC_STATE_DIR = os.path.expanduser("~/Library/Application Support/NEPTR")

def build_state_entries():
    E = []  # dicts: glob, chan (widget or None), kind, + conversion fields
    for code, fx in FX.items():
        E.append(dict(glob=fx["byp"], chan=f"on_{code}", kind="toggle"))
        for p in fx["params"]:
            ch = f'{code}_{p["chan"]}'
            if "combo" in p:
                E.append(dict(glob=p["glob"], chan=ch, kind="combo",
                              step=p.get("step", 1), off=p.get("off", 0)))
            else:
                E.append(dict(glob=p["glob"], chan=ch, kind="knob",
                              scale=p.get("scale", 1)))
    # NAM / IR row machinery + misc (widget restore handled specially)
    E.append(dict(glob="gkNAMActiveRow", chan=None, kind="raw"))
    E.append(dict(glob="gkNAMBypass", chan=None, kind="raw"))
    E.append(dict(glob="gkNAMInGain", chan="nam_in_gain", kind="knob", scale=1))
    E.append(dict(glob="gkNAMOutGain", chan="nam_out_gain", kind="knob", scale=1))
    for r in range(5):
        E.append(dict(glob=f"gkNAMRowSel[{r}]", chan=f"nam_sel{r}",
                      kind="combo", step=1, off=0))
    E.append(dict(glob="gkIRActiveRow", chan=None, kind="raw"))
    E.append(dict(glob="gkIRBypass", chan=None, kind="raw"))
    E.append(dict(glob="gkIRDamp", chan="ir_damp", kind="knob", scale=1))
    E.append(dict(glob="gkIRLevel", chan="ir_level", kind="knob", scale=1))
    for r in range(5):
        E.append(dict(glob=f"gkIRSel[{r}]", chan=f"ir_sel{r}",
                      kind="combo", step=1, off=0))
        E.append(dict(glob=f"gkIRRowMix[{r}]", chan=f"ir_mix{r}",
                      kind="knob", scale=1))
    E.append(dict(glob="gkStereoIn", chan="stereo_in", kind="toggle_direct"))
    E.append(dict(glob="gkMasterVol", chan=None, kind="raw"))
    # LOOPER speed combos persist (per-channel varispeed selection); the
    # gesture buttons themselves are momentary and intentionally NOT
    # persisted (a saved "held" state would make no sense on reload).
    for n in range(1, 6):
        E.append(dict(glob=f"gkLpSpIdx{n}", chan=f"lp{n}_speed",
                      kind="combo", step=1, off=0))
    for i, e in enumerate(E):
        e["idx"] = i
    return E

STATE = build_state_entries()
STATE_ORC = os.path.join(CSOUND_DIR, "state_persist.orc")

def emit_state_orc():
    n = len(STATE)
    size = 1
    while size < n:
        size *= 2
    # Keep at least one slot ABOVE the data for the layout stamp below. If the
    # entry count lands exactly on a power of two the top index is real data,
    # so grow the table rather than overwrite a setting with the stamp.
    if size == n:
        size *= 2
    stamp_idx = size - 1
    # The stamp is the ENTRY COUNT, not a fixed magic, because the failure this
    # guards against is a LAYOUT SHIFT: every global is addressed by position,
    # so adding or removing one effect slides every index after it. A file
    # written by a different layout is not merely stale, it is actively wrong —
    # it restores one effect's value into another effect's global.
    stamp_val = 77000000 + n
    L = []
    A = L.append
    A(";==============================================================")
    A("; NEPTR SETTINGS PERSISTENCE — GENERATED by vst/generate_vst.py")
    A("; (do not hand-edit; the registry lives in the generator's FX")
    A(";  table, which also builds the VST GUI — Pi and VST stay in")
    A(";  lockstep by construction).")
    A(";")
    A("; instr 97, always-on: every 2 s compares each registered global")
    A("; against the state table; on any change rewrites the table and")
    A("; ftsavek's it to $STATE_DIR/neptr_settings.ftab. At startup the")
    A("; file (if present) is loaded and, on non-VST builds (the Pi),")
    A("; every global is restored on the first k-cycle. VST builds")
    A("; define STATE_VST and restore through the WIDGETS instead (see")
    A("; the generated csd), so GUI and engine stay consistent.")
    A(";")
    A("; Include AFTER globals.orc (any position after it is fine).")
    A("; No-op unless STATE_PERSIST is defined.")
    A(";==============================================================")
    A("")
    A("#ifdef STATE_PERSIST")
    A("")
    A("#ifndef STATE_DIR")
    A("#define STATE_DIR #/home/gorpi7/demiurge/state#")
    A("#endif")
    A("")
    A(f"giNeptrState ftgen 0, 0, -{size}, -2, 0")
    A("gkNeptrStLoaded init 0   ; 0 pending, 1 loaded, -1 no file")
    A("")
    A("instr 97  ; SETTINGS STATE ENGINE")
    A('    SDir = "$STATE_DIR"')
    A('    SCmd sprintf "mkdir -p \\"%s\\"", SDir')
    A("    iRet system_i 1, SCmd")
    A('    SFile sprintf "%s/neptr_settings.ftab", SDir')
    A("    iOk filevalid SFile")
    A("    iStamp = 0")
    A("    if (iOk == 1) then")
    A("        ftload SFile, 0, giNeptrState")
    A(f"        iStamp tab_i {stamp_idx}, giNeptrState")
    A("    endif")
    A("    ; A file whose stamp does not match THIS layout is not restored. It")
    A("    ; is not enough that the file exists and parses: `filevalid` says")
    A("    ; nothing about whether index 220 still means what it meant when the")
    A("    ; file was written. Restoring an unstamped file is how every effect")
    A("    ; came back ON at boot — a bypass global reads 0 for 'active', so an")
    A("    ; unwritten or shifted region silently ENABLES the whole chain, and")
    A("    ; on 2026-08-11 that switched on CHAINFB (a deliberately")
    A("    ; self-oscillating feedback loop) at every single boot. Declining to")
    A("    ; restore costs the user their saved settings once; restoring")
    A("    ; garbage costs them an instrument that screams.")
    A(f"    iGood = ((iOk == 1 && iStamp == {stamp_val}) ? 1 : 0)")
    A("    ; Stamp the live table so every save from here on identifies itself.")
    A(f"    tableiw {stamp_val}, {stamp_idx}, giNeptrState")
    A("    kFlag init 0")
    A("    if (kFlag == 0) then")
    A("        kFlag = 1")
    A("        gkNeptrStLoaded = (iGood == 1 ? 1 : -1)")
    A("        if (iOk == 1 && iGood == 0) then")
    A('            prints "[state] %s ignored: layout stamp mismatch '
      '(want %d) — defaults kept, effects stay bypassed\\n", SFile, '
      f"{stamp_val}")
    A("        endif")
    A("#ifndef STATE_VST")
    A("        ; ---- direct global restore (Pi build) ----")
    A("        if (iGood == 1) then")
    for e in STATE:
        A(f"            {e['glob']} = tab:k({e['idx']}, giNeptrState)")
    A("        endif")
    A("#endif")
    A("    endif")
    A("    ; ---- change-scan + save (2 s cadence) ----")
    A("    kMet metro 0.5")
    A("    kDirty = 0")
    A("    if (kMet == 1 && gkNeptrStLoaded != 0) then")
    for e in STATE:
        A(f"        if ({e['glob']} != tab:k({e['idx']}, giNeptrState)) then")
        A(f"            tablew {e['glob']}, {e['idx']}, giNeptrState")
        A("            kDirty = 1")
        A("        endif")
    A("    endif")
    A("    ftsavek SFile, kDirty, 0, giNeptrState")
    A("endin")
    A("")
    A("schedule 97, 0, -1")
    A("")
    A("#endif  ; STATE_PERSIST")
    A("")
    content = "\n".join(L)
    with open(STATE_ORC, "w") as f:
        f.write(content)
    return content

def knob_default(p):
    if "df" in p:
        return p["df"]
    scale = p.get("scale", 1)
    v = INITS.get(p["glob"])
    if v is None:
        return p["mn"]
    v = v / scale
    return min(max(v, p["mn"]), p["mx"])

def combo_default(p):
    if "dfc" in p:
        return p["dfc"]
    v = INITS.get(p["glob"])
    if v is None:
        return 1
    step, off = p.get("step", 1), p.get("off", 0)
    idx = round((v - off) / step)
    return int(min(max(idx + 1, 1), len(p["combo"])))

# ---------------------------------------------------------------- cabbage
# Compact layout (2026-07-26 user call): small click boxes, minimal negative
# space; knobs keep their size. Categories ordered like the hardware menus.
#
# 2026-07-27 layout-math rewrite: the CATS table grew from 15 to 17 rows
# (NAM + IR VERB appended) but the grid geometry below was still keyed off
# the fixed COL_SPLIT (left-column row count) instead of the taller of the
# two columns, so the panel/footer/drum stack started 1 row too high and
# the IR VERB row (right column's 9th row) rendered underneath the param
# panel — only a sliver visible. Every section below is now derived from
# len(CATS) so future CATS growth can't reintroduce the clip.
GRID_Y = 34
ROW_H = 29
BTN_W, BTN_H, BTN_GAP = 60, 26, 3      # small, near-square click boxes
CAT_LBL_W = 46
COL_SPLIT = 8          # first 8 categories in the left column
COL_W = CAT_LBL_W + 4 + 5 * (BTN_W + BTN_GAP) - BTN_GAP   # 362
LEFT_X = 8
RIGHT_X = LEFT_X + COL_W + 10
W = RIGHT_X + COL_W + 8
GRID_ROWS = max(COL_SPLIT, len(CATS) - COL_SPLIT)   # tallest of the 2 columns
GRID_BOT = GRID_Y + GRID_ROWS * ROW_H               # bottom edge of the grid
MARGIN = 12                                          # standard section gap

PANEL_Y = GRID_BOT + MARGIN                    # param panel below the FULL grid
# PANEL_H must clear the tallest panel's own content: ir_panel's per-row MIX
# knobs sit at rel-y 50 with h=48 -> rel-bottom 98. 114 gives that content a
# clean ~16px margin without touching any panel's internal layout.
PANEL_H = 114
FOOT_Y = PANEL_Y + PANEL_H + MARGIN
FOOT_H = 12 + 28 + 10                          # label + expr slider + margin
LOOPER_Y = FOOT_Y + FOOT_H + MARGIN
# LOOPER strip content stack (see build_looper_strip): title + button +
# state label + speed combo, each separated by a small gap.
LOOPER_TITLE_H, LOOPER_BTN_H, LOOPER_LBL_H, LOOPER_COMBO_H = 14, 24, 13, 22
LOOPER_CONTENT_H = (LOOPER_TITLE_H + 6 + LOOPER_BTN_H + 3 + LOOPER_LBL_H + 3
                     + LOOPER_COMBO_H)
LOOPER_H = LOOPER_CONTENT_H + 10
DRUM_Y = LOOPER_Y + LOOPER_H + MARGIN
# DRUM strip buttons are BTN_H=26 tall; +44 gives ~18px of comfortable
# margin under the last row.
H = DRUM_Y + 44

def esc(s):
    return s.replace('"', '\\"')

def widget_button(chan, label, x, y, w=BTN_W, h=BTN_H, momentary=False, fs=11,
                  val=0):
    lat = ", latched(0)" if momentary else ""
    return (f'button bounds({x}, {y}, {w}, {h}), channel("{chan}"), '
            f'text("{esc(label)}"), corners(3), fontSize({fs}), value({val}){lat}, '
            f'colour:0({SLOT_BG}), colour:1({V1}), '
            f'fontColour:0({OFF_TEXT}), fontColour:1({WHITE}), '
            f'outlineColour({OFF_OUTLINE}), outlineThickness(1)')

def widget_knob(chan, label, mn, mx, df, x, y, skew=1, inc=0.001, col=V2, w=58, h=76):
    return (f'rslider bounds({x}, {y}, {w}, {h}), channel("{chan}"), '
            f'range({mn}, {mx}, {df}, {skew}, {inc}), text("{esc(label)}"), '
            f'fontSize(10), textColour({OFF_TEXT}), colour({col}), '
            f'trackerColour({ACTIVE}), outlineColour({EMPTY_OUTLINE}), '
            f'markerColour({WHITE})')

def widget_combo(chan, label, items, df, x, y, w=104):
    it = ", ".join(f'"{esc(i)}"' for i in items)
    return (f'label bounds({x}, {y}, {w}, 12), text("{esc(label)}"), fontSize(10), '
            f'align("centre"), fontColour({OFF_TEXT})\n'
            f'combobox bounds({x}, {y + 13}, {w}, 24), channel("{chan}"), '
            f'items({it}), value({df}), colour({SLOT_BG}), fontColour({ACTIVE})')

def fx_panel(code):
    """Hidden-until-selected parameter panel for one effect."""
    fx = FX[code]
    out = [f'image bounds(8, {PANEL_Y}, {W - 16}, {PANEL_H}), colour({SLOT_BG}), '
           f'outlineColour({OFF_OUTLINE}), outlineThickness(1), corners(6), '
           f'channel("pnl_{code}"), visible(0) {{']
    out.append(f'label bounds(12, 4, 300, 16), text("{esc(fx["label"])}"), '
               f'fontSize(14), align("left"), fontColour({ACTIVE})')
    if not fx["params"]:
        out.append(f'label bounds(12, 56, 500, 16), fontSize(12), align("left"), '
                   f'fontColour({OFF_TEXT}), text("No parameters — just the toggle.")')
    n = 0
    for p in fx["params"]:
        px = 12 + n * 63
        py = 24
        if "combo" in p:
            out.append(widget_combo(f'{code}_{p["chan"]}', p["label"], p["combo"],
                                    combo_default(p), px, py + 18, w=104))
            n += 2
        else:
            out.append(widget_knob(f'{code}_{p["chan"]}', p["label"], p["mn"], p["mx"],
                                   round(knob_default(p), 4), px, py, h=70,
                                   skew=p.get("skew", 1), inc=p.get("inc", 0.001)))
            n += 1
    if code == "frz":
        # FREEZE has no double-tap gesture in the VST GUI, so a dedicated
        # momentary CLEAR button substitutes for "single click while
        # engaged" (clears the bed, stays engaged) — see build_control().
        out.append(widget_button("frz_clear", "CLEAR", 12 + n * 63, 24,
                                 w=58, h=70, momentary=True, fs=10))
    out.append("}")
    return "\n".join(out)

def nam_panel():
    out = [f'image bounds(8, {PANEL_Y}, {W - 16}, {PANEL_H}), colour({SLOT_BG}), '
           f'outlineColour({OFF_OUTLINE}), outlineThickness(1), corners(6), '
           f'channel("pnl_nam"), visible(0) {{']
    out.append(f'label bounds(12, 5, 120, 16), text("NAM AMP"), fontSize(14), '
               f'align("left"), fontColour({ACTIVE})')
    for r in range(5):
        out.append(widget_combo(f"nam_sel{r}", NAM_ROWS[r], NAM_VARIANTS[r],
                                1, 12 + r * 116, 24, w=108))
    # Amp sim is 100% wet, always — no mix knob (removed 2026-07-27). IN/OUT
    # gain knobs enlarged + moved right (2026-07-27 tighten pass) so they fill
    # the panel's dead right-hand space instead of floating small in a sea
    # of empty background.
    out.append(widget_knob("nam_in_gain", "IN", -12, 18,
                           INITS.get("gkNAMInGain", 0), 600, 12, w=58, h=90))
    out.append(widget_knob("nam_out_gain", "OUT", -18, 18,
                           INITS.get("gkNAMOutGain", 0), 666, 12, w=58, h=90))
    out.append("}")
    return "\n".join(out)

def ir_panel():
    out = [f'image bounds(8, {PANEL_Y}, {W - 16}, {PANEL_H}), colour({SLOT_BG}), '
           f'outlineColour({OFF_OUTLINE}), outlineThickness(1), corners(6), '
           f'channel("pnl_ir"), visible(0) {{']
    out.append(f'label bounds(12, 5, 120, 16), text("IR VERB"), fontSize(14), '
               f'align("left"), fontColour({ACTIVE})')
    for r in range(5):
        x = 12 + r * 116
        out.append(widget_combo(f"ir_sel{r}", IR_ROWS[r], IR_VARIANTS[r],
                                1, x, 16, w=108))
        out.append(widget_knob(f"ir_mix{r}", "MIX", 0, 1, 0.35,
                               x + 30, 50, w=46, h=48))
    out.append(widget_knob("ir_damp", "DAMP", 0, 1,
                           INITS.get("gkIRDamp", 0.5), 624, 18, w=52, h=68))
    out.append(widget_knob("ir_level", "LEVEL", 0, 1,
                           INITS.get("gkIRLevel", 0.8), 684, 18, w=52, h=68))
    out.append("}")
    return "\n".join(out)

def build_looper_strip():
    """5-channel LOOPER strip: one main gesture button + state/speed readout
    + speed combo per channel. Sits directly above the DRUMS strip (looper-
    adjacent) as its own bottom section; see build_control()'s looper block
    for the instr98 logic driving these widgets."""
    out = [f'image bounds(8, {LOOPER_Y}, {W - 16}, {LOOPER_H}), '
           f'colour({SLOT_BG}), outlineColour({OFF_OUTLINE}), '
           f'outlineThickness(1), corners(6)']
    out.append(f'label bounds(14, {LOOPER_Y + 6}, 70, 12), text("LOOPER"), '
               f'fontSize(10), align("left"), fontColour({OFF_TEXT})')
    row_y = LOOPER_Y + LOOPER_TITLE_H + 6
    ch_w, ch_gap, x0 = 70, 8, 14
    it = ", ".join(f'"{esc(s)}"' for s in LOOPER_SPEED_LABELS)
    for n in range(1, 6):
        x = x0 + (n - 1) * (ch_w + ch_gap)
        out.append(widget_button(f"lp{n}_btn", f"LOOP {n}", x, row_y,
                                 w=ch_w, h=LOOPER_BTN_H, momentary=True, fs=10))
        lbl_y = row_y + LOOPER_BTN_H + 3
        out.append(f'label bounds({x}, {lbl_y}, {ch_w}, {LOOPER_LBL_H}), '
                   f'text("IDLE +1x"), fontSize(9), align("centre"), '
                   f'fontColour({OFF_TEXT}), channel("lp{n}_state")')
        combo_y = lbl_y + LOOPER_LBL_H + 3
        out.append(f'combobox bounds({x}, {combo_y}, {ch_w}, {LOOPER_COMBO_H}), '
                   f'channel("lp{n}_speed"), items({it}), value(7), '
                   f'colour({SLOT_BG}), fontColour({ACTIVE}), fontSize(9)')
    return "\n".join(out)

def build_cabbage():
    L = []
    L.append(f'form caption("NEPTR PHASE 4"), size({W}, {H}), colour({BG}), '
             f'pluginid("NPTR"), guiMode("queue")')
    # top bar: title + preset system (Cabbage snapshots)
    L.append(f'label bounds(10, 10, 140, 20), text("NEPTR PHASE 4"), fontSize(15), '
             f'align("left"), fontColour({ACTIVE})')
    L.append(f'label bounds(160, 13, 18, 12), text("IN"), fontSize(10), '
             f'align("left"), fontColour({OFF_TEXT})')
    L.append(f'hslider bounds(182, 10, 104, 22), channel("meter_in"), '
             f'range(0, 1, 0), colour({V1}), trackerColour({V1}), active(0)')
    L.append(f'label bounds({W - 412}, 13, 54, 13), text("PRESET"), fontSize(10), '
             f'align("right"), fontColour({OFF_TEXT})')
    L.append(f'combobox bounds({W - 352}, 8, 230, 24), channel("presetCombo"), '
             f'populate("*.snaps"), channelType("string"), '
             f'colour({SLOT_BG}), fontColour({ACTIVE})')
    L.append(f'filebutton bounds({W - 118}, 8, 110, 24), text("SAVE PRESET", '
             f'"SAVE PRESET"), mode("named snapshot"), populate("*.snaps"), '
             f'corners(3), fontSize(11), colour:0({V3}), colour:1({V3}), '
             f'fontColour:0({WHITE}), fontColour:1({WHITE})')
    # effect grid
    for ci, (cat, codes) in enumerate(CATS):
        col = 0 if ci < COL_SPLIT else 1
        row = ci if ci < COL_SPLIT else ci - COL_SPLIT
        x0 = LEFT_X if col == 0 else RIGHT_X
        y = GRID_Y + row * ROW_H
        L.append(f'label bounds({x0}, {y + 6}, {CAT_LBL_W}, 12), text("{cat}"), '
                 f'fontSize(9), align("left"), fontColour({OFF_TEXT})')
        bx = x0 + CAT_LBL_W + 4
        if codes == "NAM":
            for r in range(5):
                # DELUXE (row 1) lit at boot = hardware default amp
                L.append(widget_button(f"nam_row{r}", NAM_ROWS[r],
                                       bx + r * (BTN_W + BTN_GAP), y,
                                       val=1 if r == 1 else 0))
        elif codes == "IRVERB":
            for r in range(5):
                L.append(widget_button(f"ir_row{r}", IR_ROWS[r],
                                       bx + r * (BTN_W + BTN_GAP), y))
        else:
            for i, code in enumerate(codes):
                L.append(widget_button(f"on_{code}", grid_name(code),
                                       bx + i * (BTN_W + BTN_GAP), y, fs=9))
    # welcome panel (visible until something is selected)
    L.append(f'image bounds(8, {PANEL_Y}, {W - 16}, {PANEL_H}), colour({SLOT_BG}), '
             f'outlineColour({EMPTY_OUTLINE}), outlineThickness(1), corners(6), '
             f'channel("pnl_none"), visible(1) {{')
    L.append(f'label bounds(0, 46, {W - 16}, 18), text("CLICK AN EFFECT TO EDIT ITS '
             f'CONTROLS"), fontSize(12), align("centre"), '
             f'fontColour({OFF_TEXT})')
    L.append("}")
    # parameter panels (all hidden; selection shows one)
    for code in FX_ORDER:
        L.append(fx_panel(code))
    L.append(nam_panel())
    L.append(ir_panel())
    # footer: expression pedal + stereo switch
    L.append(f'label bounds(10, {FOOT_Y}, 90, 12), text("EXPR PEDAL"), '
             f'fontSize(10), fontColour({OFF_TEXT})')
    L.append(f'hslider bounds(10, {FOOT_Y + 12}, {W - 428}, 28), channel("expr"), '
             f'range(0, 1, 0, 1, 0.001), colour({V4}), trackerColour({V4})')
    L.append(f'label bounds({W - 408}, {FOOT_Y + 18}, 260, 14), '
             f'text("PEDAL TARGET: -"), fontSize(11), align("left"), '
             f'fontColour({ACTIVE}), channel("exprLbl")')
    L.append(f'checkbox bounds({W - 136}, {FOOT_Y + 16}, 126, 20), '
             f'channel("stereo_in"), text("STEREO IN"), value(1), colour:1({V1}), '
             f'fontColour:0({OFF_TEXT}), fontColour:1({ACTIVE})')
    # looper strip: 5-channel gesture buttons (see build_looper_strip)
    L.append(build_looper_strip())
    # drums strip: looper ch1 sample mode (prev/next/play/clear + filename)
    L.append(f'label bounds(10, {DRUM_Y + 7}, 46, 12), text("DRUMS"), '
             f'fontSize(10), align("left"), fontColour({OFF_TEXT})')
    L.append(widget_button("drum_prev", "◀", 60, DRUM_Y, w=30,
                           momentary=True))
    L.append(widget_button("drum_next", "▶", 93, DRUM_Y, w=30,
                           momentary=True))
    L.append(widget_button("drum_play", "PLAY/STOP", 126, DRUM_Y, w=80,
                           momentary=True, fs=10))
    L.append(widget_button("drum_clear", "CLEAR", 209, DRUM_Y, w=56,
                           momentary=True, fs=10))
    L.append(f'label bounds(275, {DRUM_Y + 7}, {W - 285}, 13), text("-"), '
             f'fontSize(11), align("left"), fontColour({ACTIVE}), '
             f'channel("drum_lbl")')
    # NOTE: no full-window overlay here — a transparent image spanning the whole
    # window intercepts every mouse click before it reaches the buttons beneath
    # (this was the "nothing happens when I click" bug). The old hot-reload glow
    # widget was removed for exactly that reason.
    return "\n".join(L)

# ---------------------------------------------------------------- control orc
def emit_param_write(A, code, p):
    ch = f'{code}_{p["chan"]}'
    kv = f"kP_{ch}"
    A(f'{kv} chnget "{ch}"')
    A(f"kC_{ch} changed2 {kv}")
    A(f"if (kC_{ch} == 1 || kInit == 1) then")
    if "combo" in p:
        step, off = p.get("step", 1), p.get("off", 0)
        expr = f"({kv} - 1) * {step}" + (f" + {off}" if off else "")
        A(f"  {p['glob']} = {expr}")
    elif p.get("scale", 1) != 1:
        A(f"  {p['glob']} = {kv} * {p['scale']}")
    else:
        A(f"  {p['glob']} = {kv}")
    A("endif")

STATE_IDX = {e["glob"]: e["idx"] for e in STATE}

def emit_widget_restore(A):
    """Standalone app only: push loaded state into the widgets, which then
    flow back into the globals through the normal instr-98 paths."""
    A("#ifdef STATE_RESTORE_WIDGETS")
    A("; ---------- settings restore -> widgets (standalone app) ----------")
    A("kWRst init 0")
    A("if (kWRst == 0 && kInit == 0 && gkNeptrStLoaded == 1) then")
    A("  kWRst = 1")
    for e in STATE:
        if not e.get("chan"):
            continue
        idx = e["idx"]
        if e["kind"] == "toggle":
            expr = f"(tab:k({idx}, giNeptrState) < 0.5 ? 1 : 0)"
        elif e["kind"] == "toggle_direct":
            expr = f"tab:k({idx}, giNeptrState)"
        elif e["kind"] == "combo":
            step, off = e["step"], e["off"]
            base = f"tab:k({idx}, giNeptrState)"
            if off:
                base = f"({base} - {off})"
            expr = f"{base} / {step} + 1"
        elif e.get("scale", 1) != 1:
            expr = f"tab:k({idx}, giNeptrState) / {e['scale']}"
        else:
            expr = f"tab:k({idx}, giNeptrState)"
        A(f'  cabbageSetValue "{e["chan"]}", {expr}, k(1)')
    iNAR, iNBP = STATE_IDX["gkNAMActiveRow"], STATE_IDX["gkNAMBypass"]
    iIAR, iIBP = STATE_IDX["gkIRActiveRow"], STATE_IDX["gkIRBypass"]
    for r in range(5):
        A(f'  cabbageSetValue "nam_row{r}", (tab:k({iNAR}, giNeptrState) == {r} '
          f'&& tab:k({iNBP}, giNeptrState) < 0.5 ? 1 : 0), k(1)')
        A(f'  cabbageSetValue "ir_row{r}", (tab:k({iIAR}, giNeptrState) == {r} '
          f'&& tab:k({iIBP}, giNeptrState) < 0.5 ? 1 : 0), k(1)')
    A("endif")
    A("#endif")

def emit_panel_switch(A):
    """Show the selected panel, hide the rest (runs when kSel changes)."""
    A("if (kSelChg == 1) then")
    A('  cabbageSet k(1), "pnl_none", "visible(0)"')
    for code in FX_ORDER:
        A(f'  cabbageSet k(1), "pnl_{code}", sprintfk("visible(%d)", '
          f'kSel == {SEL[code]} ? 1 : 0)')
    A(f'  cabbageSet k(1), "pnl_nam", sprintfk("visible(%d)", '
      f'kSel == {SEL_NAM} ? 1 : 0)')
    A(f'  cabbageSet k(1), "pnl_ir", sprintfk("visible(%d)", '
      f'kSel == {SEL_IR} ? 1 : 0)')
    A("endif")

def emit_looper_control(A):
    """LOOPER 5-channel gesture surface, driven by the lpN_btn momentary
    buttons + lpN_speed combos. Faithfully mirrors the hardware press/
    release/hold-watcher semantics in csound/handler/handler_looper.orc
    ($LP_CHn_PRESS / $LP_RELEASE_ALL / $LP_HOLD_WATCHER), which are MIDI-CC
    driven and not #included in this VST build (MIDI is disabled: -M0
    -m0d) — so there is no collision reusing the same gkLp* globals here.
    Grid-snap-close logic is the $LP_QCLOSE macro, inlined per channel."""
    A("; ---------- LOOPER (5-channel gesture surface) ----------")
    A("; Mirrors handler_looper.orc's $LP_CHn_PRESS / $LP_RELEASE_ALL /")
    A("; $LP_HOLD_WATCHER macros verbatim, but button-driven instead of")
    A("; MIDI-CC-driven (this build runs with MIDI disabled).")
    for n in range(1, 6):
        buf = f"giLpBuf{n}L"
        others = [m for m in range(1, 6) if m != n]
        clear_cond = " && ".join(f"gkLpLen{m} == 0" for m in others)
        A(f"; -- LOOPER ch{n} --")
        A(f'kLpBtn{n} chnget "lp{n}_btn"')
        A(f"kLpPrv{n} init 0")
        A(f"if (kLpBtn{n} > 0.5 && kLpPrv{n} < 0.5) then")
        A(f"  kLpNow{n} times")
        A(f"  if (kLpNow{n} - gkLpLastTap{n} < 0.5) then")
        A(f"    ; double-tap -> clear")
        A(f"    gkLpDblFired{n} = 1")
        A(f"    gkLpState{n} = 0")
        A(f"    gkLpLen{n} = 0")
        A(f"    gkLpWrPos{n} = 0")
        A(f"    gkLpRdPos{n} = 0")
        A(f"    gkLpSpIdx{n} = 6")
        A(f"    if (gkDrumChan == {n}) then")
        A(f"      gkDrumChan = -1")
        A(f"      gkDrumIdx = -1")
        A(f"    endif")
        A(f"    if ({clear_cond}) then")
        A(f"      gkLpMasterLen = 0")
        A(f"    endif")
        A(f"  elseif (gkLpState{n} == 0) then")
        A(f"    ; idle -> record (speed reset + transport anchor)")
        A(f"    gkLpState{n} = 1")
        A(f"    gkLpWrPos{n} = 0")
        A(f"    gkLpRdPos{n} = 0")
        A(f"    gkLpSpIdx{n} = 6")
        A(f"    gkLpAnchor{n} = gkLpClock")
        A(f"    gkLpConsumed{n} = 1")
        A(f"  elseif (gkLpState{n} == 1) then")
        A(f"    ; recording -> close (grid-snap, $LP_QCLOSE inlined; see")
        A(f"    ; handler/handler_looper.orc for the canonical macro — kept")
        A(f"    ; in lockstep here, 2026-07-29 Pocket OpGorator spec §4)")
        A(f"    if (gkLpMasterLen == 0) then")
        A(f"      ; first loop closed anywhere = master transport")
        A(f"      if (gkLpGridAvail == 1 && gkLpBeatSamples > 0) then")
        A(f"        ; CLOCKED -- quantize to the nearest whole-beat grid line")
        A(f"        kLpBeats{n} = round(gkLpWrPos{n} / gkLpBeatSamples)")
        A(f"        if (kLpBeats{n} < 1) then")
        A(f"          kLpBeats{n} = 1")
        A(f"        endif")
        A(f"        kLpMTgt{n} = kLpBeats{n} * gkLpBeatSamples")
        A(f"        kLpMBufSz{n} = ftlen({buf})")
        A(f"        if (kLpMTgt{n} > kLpMBufSz{n}) then")
        A(f"          kLpMTgt{n} = kLpMBufSz{n}")
        A(f"        endif")
        A(f"        if (gkLpWrPos{n} >= kLpMTgt{n}) then")
        A(f"          ; late/exact -- round back, finalize now")
        A(f"          gkLpLen{n}      = kLpMTgt{n}")
        A(f"          gkLpMasterLen   = kLpMTgt{n}")
        A(f"          gkLpState{n}    = 2")
        A(f"          gkLpRdPos{n}    = gkLpWrPos{n} % kLpMTgt{n}")
        A(f"        else")
        A(f"          ; early -- keep recording until the boundary")
        A(f"          gkLpTarget{n} = kLpMTgt{n}")
        A(f"          gkLpState{n}  = 5")
        A(f"        endif")
        A(f"      else")
        A(f"        ; UNCLOCKED -- free capture, trim trailing silence")
        A(f"        gkLpTrimPos{n} = gkLpWrPos{n}")
        A(f"        gkLpTarget{n}  = gkLpWrPos{n}   ; fallback if all-silent")
        A(f"        gkLpState{n}   = 6")
        A(f"      endif")
        A(f"    else")
        A(f"      kLpM{n} = gkLpMasterLen")
        A(f"      kLpC0_{n} = kLpM{n} / 4")
        A(f"      kLpC1_{n} = kLpM{n} / 2")
        A(f"      kLpC2_{n} = kLpM{n}")
        A(f"      kLpC3_{n} = kLpM{n} * 2")
        A(f"      kLpC4_{n} = kLpM{n} * 4")
        A(f"      kLpBst{n} = kLpC0_{n}")
        A(f"      kLpBsD{n} = abs(gkLpWrPos{n} - kLpC0_{n})")
        A(f"      if (abs(gkLpWrPos{n} - kLpC1_{n}) < kLpBsD{n}) then")
        A(f"        kLpBst{n} = kLpC1_{n}")
        A(f"        kLpBsD{n} = abs(gkLpWrPos{n} - kLpC1_{n})")
        A(f"      endif")
        A(f"      if (abs(gkLpWrPos{n} - kLpC2_{n}) < kLpBsD{n}) then")
        A(f"        kLpBst{n} = kLpC2_{n}")
        A(f"        kLpBsD{n} = abs(gkLpWrPos{n} - kLpC2_{n})")
        A(f"      endif")
        A(f"      if (abs(gkLpWrPos{n} - kLpC3_{n}) < kLpBsD{n}) then")
        A(f"        kLpBst{n} = kLpC3_{n}")
        A(f"        kLpBsD{n} = abs(gkLpWrPos{n} - kLpC3_{n})")
        A(f"      endif")
        A(f"      if (abs(gkLpWrPos{n} - kLpC4_{n}) < kLpBsD{n}) then")
        A(f"        kLpBst{n} = kLpC4_{n}")
        A(f"      endif")
        A(f"      kLpBufSz{n} = ftlen({buf})")
        A(f"      if (kLpBst{n} > kLpBufSz{n}) then")
        A(f"        kLpBst{n} = kLpBufSz{n}")
        A(f"      endif")
        A(f"      if (gkLpWrPos{n} >= kLpBst{n}) then")
        A(f"        gkLpLen{n}   = kLpBst{n}")
        A(f"        gkLpState{n} = 2")
        A(f"        gkLpRdPos{n} = gkLpWrPos{n} % kLpBst{n}")
        A(f"      else")
        A(f"        gkLpTarget{n} = kLpBst{n}")
        A(f"        gkLpState{n}  = 5")
        A(f"      endif")
        A(f"    endif")
        A(f"    gkLpConsumed{n} = 1")
        A(f"  elseif (gkLpState{n} == 4) then")
        A(f"    ; stopped -> resume, transport relock")
        A(f"    gkLpState{n} = 2")
        A(f"    gkLpRdPos{n} = (gkLpClock - gkLpAnchor{n}) % gkLpLen{n}")
        A(f"    gkLpConsumed{n} = 1")
        A(f"  endif")
        A(f"  gkLpLastTap{n} = kLpNow{n}")
        A(f"  gkLpPressT{n}  = kLpNow{n}")
        A(f"elseif (kLpBtn{n} < 0.5 && kLpPrv{n} > 0.5) then")
        A(f"  ; release: classify tap-vs-hold for play/overdub only")
        A(f"  kLpRel{n} times")
        A(f"  gkLpPressT{n} = -1")
        A(f"  if (gkLpDblFired{n} >= 0.5) then")
        A(f"    gkLpDblFired{n} = 0")
        A(f"  elseif (gkLpConsumed{n} >= 0.5) then")
        A(f"    gkLpConsumed{n} = 0")
        A(f"  else")
        A(f"    if (gkLpState{n} == 2) then")
        A(f"      gkLpState{n} = 4     ; play -> stop")
        A(f"    elseif (gkLpState{n} == 3) then")
        A(f"      gkLpState{n} = 2     ; overdub -> play")
        A(f"    endif")
        A(f"  endif")
        A(f"endif")
        A(f"kLpPrv{n} = kLpBtn{n}")
        A("")
    A("; hold watcher: play->overdub / overdub->play at the 0.4s threshold,")
    A("; independent of button events (fires the instant the hold matures")
    A("; while the button is still down, same as the hardware).")
    A("kLpNowW times")
    for n in range(1, 6):
        A(f"if (gkLpPressT{n} >= 0 && gkLpConsumed{n} < 0.5 && "
          f"gkLpDblFired{n} < 0.5 && kLpNowW - gkLpPressT{n} >= 0.4) then")
        A(f"  if (gkLpState{n} == 2) then")
        A(f"    gkLpState{n} = 3")
        A(f"    gkLpConsumed{n} = 1")
        A(f"  elseif (gkLpState{n} == 3) then")
        A(f"    gkLpState{n} = 2")
        A(f"    gkLpConsumed{n} = 1")
        A(f"  endif")
        A(f"endif")
    A("")
    A("; speed combo <-> gkLpSpIdxN, bidirectional (widget writes the")
    A("; global; any global change — incl. the reset-to-6 gestures above —")
    A("; syncs the widget back so the combo never goes stale).")
    for n in range(1, 6):
        A(f'kLpSpW{n} chnget "lp{n}_speed"')
        A(f"kLpSpWC{n} changed2 kLpSpW{n}")
        A(f"if (kLpSpWC{n} == 1) then")
        A(f"  gkLpSpIdx{n} = kLpSpW{n} - 1")
        A(f"endif")
        A(f"kLpSpGC{n} changed2 gkLpSpIdx{n}")
        A(f"if (kLpSpGC{n} == 1) then")
        A(f'  cabbageSetValue "lp{n}_speed", gkLpSpIdx{n} + 1, k(1)')
        A(f"endif")
    A("")
    A("; per-channel state+speed label, ~10 Hz")
    A("kLpUiTick metro 10")
    A("if (kLpUiTick == 1) then")
    for n in range(1, 6):
        A(f"  if (gkLpState{n} == 0) then")
        A(f'    Slp{n}st strcpy "IDLE"')
        for code, name in sorted(LOOPER_STATE_LABELS.items())[1:]:
            A(f"  elseif (gkLpState{n} == {code}) then")
            A(f'    Slp{n}st strcpy "{name}"')
        A("  endif")
        A(f"  if (gkLpSpIdx{n} == 0) then")
        A(f'    Slp{n}sp strcpy "{LOOPER_SPEED_LABELS[0]}"')
        for i, lbl in enumerate(LOOPER_SPEED_LABELS[1:], start=1):
            A(f"  elseif (gkLpSpIdx{n} == {i}) then")
            A(f'    Slp{n}sp strcpy "{lbl}"')
        A("  endif")
        A(f'  cabbageSet k(1), "lp{n}_state", sprintfk("text(\\"%s %s\\")", '
          f"Slp{n}st, Slp{n}sp)")
    A("endif")

def build_control():
    L = []
    A = L.append
    A("instr 98  ; ============ VST CONTROL SURFACE (generated) ============")
    A("kInit init 1")
    A("kExprTgt init 0")
    A("kSel init %d  ; boot on H90 so controls show immediately" % SEL["h90"])
    A("kSelChg = 0")
    A("kSelBoot init 1")
    A("if (kSelBoot == 1) then")
    A("  kSelBoot = 0")
    A("  kSelChg = 1")
    A("endif")
    A("")
    A("; ---------- input level meter + audio debug ----------")
    A("aMtL, aMtR ins")
    A("kRmsL rms aMtL, 8")
    A("kRmsR rms aMtR, 8")
    A("kInRms = (kRmsL + kRmsR) * 0.5")
    A("kMetT metro 12")
    A("if (kMetT == 1) then")
    A('  cabbageSetValue "meter_in", limit:k(kInRms * 4, 0, 1), k(1)')
    A("endif")
    A("#ifdef AUDIO_DEBUG")
    A('  SDbg sprintf "%s/audio_debug.log", "$STATE_DIR"')
    A("  kDbgInit init 1")
    A("  if (kDbgInit == 1) then")
    A("    kDbgInit = 0")
    A('    prints "NEPTR-AUDIO sr=%d ksmps=%d nchnls_i=%d nchnls=%d\\n", sr, ksmps, nchnls_i, nchnls')
    A("  endif")
    A("  ; running window peak, reset every log tick")
    A("  kPk init 0")
    A("  kAbs = abs(k(aMtL)) > abs(k(aMtR)) ? abs(k(aMtL)) : abs(k(aMtR))")
    A("  kPk = kAbs > kPk ? kAbs : kPk")
    A("  aMonL, aMonR monitor")
    A("  kOutL rms aMonL, 8")
    A("  kOutR rms aMonR, 8")
    A("  kDbgT metro 2")
    A("  if (kDbgT == 1) then")
    A('    fprintks SDbg, "sr=%d nch_i=%d nch_o=%d IN(L=%.6f R=%.6f pk=%.6f) OUT(L=%.6f R=%.6f)\\n", sr, nchnls_i, nchnls, kRmsL, kRmsR, kPk, kOutL, kOutR')
    A("    kPk = 0")
    A("  endif")
    A("#endif")
    A("")
    A("; ---------- stereo input switch ----------")
    A('kStIn chnget "stereo_in"')
    A("gkStereoIn = kStIn")
    A("")
    A("; ---------- effect toggles + params + selection ----------")
    for code, fx in FX.items():
        A(f"; -- {fx['label']} (toggle {fx['id']})")
        A(f'kOn_{code} chnget "on_{code}"')
        A(f"kClk_{code} changed2 kOn_{code}")
        A(f"if (kClk_{code} == 1) then")
        A(f"  kSel = {SEL[code]}")
        A("  kSelChg = 1")
        A("endif")
        A(f"kPv_{code} init 0")
        A(f"if (kOn_{code} > 0.5 && kPv_{code} < 0.5) then")
        if fx.get("claim"):
            A(f"  kExprTgt = {fx['claim']}")
            A(f'  cabbageSet k(1), "exprLbl", "text(\\"PEDAL TARGET: '
              f'{esc(CLAIM_NAMES[fx["claim"]])}\\")"')
        if fx.get("radio"):
            A(f'  cabbageSetValue "on_{fx["radio"]}", k(0), k(1)')
        if code == "hab":
            A("  gkHab_rec = 1")
            A('  cabbageSetValue "hab_rec", k(2), k(1)')
        A("endif")
        A(f"kPv_{code} = kOn_{code}")
        A(f"{fx['byp']} = 1 - kOn_{code}")
        for p in fx["params"]:
            emit_param_write(A, code, p)
        A("")
    # FREEZE clear button (momentary pulse; rising-edge pattern, same as the
    # drum transport buttons below) — no double-tap gesture in the VST GUI,
    # so this substitutes for "single click while engaged" (clear, stay on).
    A("; -- FREEZE clear (momentary) --")
    A('kB_frz_clear chnget "frz_clear"')
    A("kP_frz_clear init 0")
    A("if (kB_frz_clear > 0.5 && kP_frz_clear < 0.5) then")
    A("  gkFrzClear = 1")
    A("endif")
    A("kP_frz_clear = kB_frz_clear")
    A("")
    # IR VERB rows
    A("; ---------- IR VERB rows ----------")
    for r in range(5):
        A(f'kIrB{r} chnget "ir_row{r}"')
        A(f"kIrC{r} changed2 kIrB{r}")
        A(f"if (kIrC{r} == 1) then")
        A(f"  kSel = {SEL_IR}")
        A("  kSelChg = 1")
        A("endif")
        A(f"kIrP{r} init 0")
        A(f"if (kIrB{r} > 0.5 && kIrP{r} < 0.5) then")
        A(f"  gkIRActiveRow = {r}")
        A("  gkIRBypass = 0")
        for o in range(5):
            if o != r:
                A(f'  cabbageSetValue "ir_row{o}", k(0), k(1)')
        A(f"elseif (kIrB{r} < 0.5 && kIrP{r} > 0.5 && gkIRActiveRow == {r}) then")
        A("  gkIRBypass = 1")
        A("endif")
        A(f"kIrP{r} = kIrB{r}")
        A(f'kIrS{r} chnget "ir_sel{r}"')
        A(f"kIrSc{r} changed2 kIrS{r}")
        A(f"if (kIrSc{r} == 1 || kInit == 1) then")
        A(f"  gkIRSel[{r}] = kIrS{r} - 1")
        A("endif")
        A(f'kIrM{r} chnget "ir_mix{r}"')
        A(f"kIrMc{r} changed2 kIrM{r}")
        A(f"if (kIrMc{r} == 1 || kInit == 1) then")
        A(f"  gkIRRowMix[{r}] = kIrM{r}")
        A("endif")
    A('kIrDamp chnget "ir_damp"')
    A("kIrDampC changed2 kIrDamp")
    A("if (kIrDampC == 1 || kInit == 1) then")
    A("  gkIRDamp = kIrDamp")
    A("endif")
    A('kIrLev chnget "ir_level"')
    A("kIrLevC changed2 kIrLev")
    A("if (kIrLevC == 1 || kInit == 1) then")
    A("  gkIRLevel = kIrLev")
    A("endif")
    A("")
    # NAM rows — selection always works; DSP wiring only with VST_NAM
    A("; ---------- NAM rows ----------")
    for r in range(5):
        A(f'kNmB{r} chnget "nam_row{r}"')
        A(f"kNmC{r} changed2 kNmB{r}")
        A(f"if (kNmC{r} == 1) then")
        A(f"  kSel = {SEL_NAM}")
        A("  kSelChg = 1")
        A("endif")
    A("#ifdef VST_NAM")
    for r in range(5):
        A(f"kNmP{r} init 0")
        A(f"if (kNmB{r} > 0.5 && kNmP{r} < 0.5) then")
        A(f"  gkNAMActiveRow = {r}")
        A("  gkNAMBypass = 0")
        A(f"  gkNAMProfileIdx = giNAMRowBase[{r}] + gkNAMRowSel[{r}]")
        for o in range(5):
            if o != r:
                A(f'  cabbageSetValue "nam_row{o}", k(0), k(1)')
        A(f"elseif (kNmB{r} < 0.5 && kNmP{r} > 0.5 && gkNAMActiveRow == {r}) then")
        A("  gkNAMBypass = 1")
        A("endif")
        A(f"kNmP{r} = kNmB{r}")
        A(f'kNmS{r} chnget "nam_sel{r}"')
        A(f"kNmSc{r} changed2 kNmS{r}")
        A(f"if (kNmSc{r} == 1) then")
        A(f"  gkNAMRowSel[{r}] = kNmS{r} - 1")
        A(f"  if (gkNAMActiveRow == {r}) then")
        A(f"    gkNAMProfileIdx = giNAMRowBase[{r}] + gkNAMRowSel[{r}]")
        A("  endif")
        A("endif")
    A('kNamInG chnget "nam_in_gain"')
    A("kNamInGC changed2 kNamInG")
    A("if (kNamInGC == 1 || kInit == 1) then")
    A("  gkNAMInGain = kNamInG")
    A("endif")
    A('kNamOutG chnget "nam_out_gain"')
    A("kNamOutGC changed2 kNamOutG")
    A("if (kNamOutGC == 1 || kInit == 1) then")
    A("  gkNAMOutGain = kNamOutG")
    A("endif")
    A("#else")
    A("; NAM disabled in this build (see vst/README.md); keep it hard-bypassed.")
    A("gkNAMBypass = 1")
    A("#endif")
    A("")
    emit_looper_control(A)
    A("")
    A("; ---------- drums (looper sample mode; browse targets the drum")
    A("; channel, or ch1 while none is loaded) ----------")
    A("gkDrumReqCh = (gkDrumChan >= 1 ? gkDrumChan : 1)")
    for chan, act in (("drum_prev", "gkDrumStep = gkDrumStep + 1"),
                      ("drum_next", "gkDrumStep = gkDrumStep + 1"),
                      ("drum_play", "gkDrumTap = 1"),
                      ("drum_clear", "gkDrumClear = 1")):
        A(f'kB_{chan} chnget "{chan}"')
        A(f"kP_{chan} init 0")
        A(f"if (kB_{chan} > 0.5 && kP_{chan} < 0.5) then")
        A(f"  {act}")
        A("endif")
        A(f"kP_{chan} = kB_{chan}")
    A("kDrTick metro 4")
    A("if (kDrTick == 1) then")
    A('  SDrNm chngetks "drum_file"')
    A("  if (gkDrumIdx >= 0) then")
    A('    cabbageSet k(1), "drum_lbl", sprintfk("text(\\"%d/%d %s\\")", '
      "gkDrumIdx + 1, gkDrumCount, SDrNm)")
    A("  else")
    A('    cabbageSet k(1), "drum_lbl", "text(\\"-\\")"')
    A("  endif")
    A("endif")
    A("")
    A("; ---------- parameter panel switching ----------")
    emit_panel_switch(A)
    A("")
    emit_widget_restore(A)
    A("")
    A("#ifdef STATE_RESTORE_WIDGETS")
    A("; ---------- hot-reload blue edge-glow ----------")
    A('SMark sprintf "%s/.updated", "$STATE_DIR"')
    A("iUpd filevalid SMark")
    A("if (iUpd == 1) then")
    A('  SRmk sprintf "rm -f \\"%s\\"", SMark')
    A("  iRmk system_i 1, SRmk")
    A("endif")
    A("kGlow init iUpd")
    A("kGlowFps metro 60")
    A("if (kGlow > 0) then")
    A("  kGlow = kGlow - 1.6/kr")
    A("  kGlow = kGlow < 0 ? 0 : kGlow")
    A("  if (kGlowFps == 1 || kGlow == 0) then")
    A('    cabbageSet 1, "updGlow", sprintfk("outlineColour(80, 180, 255, %d)", int(255*kGlow))')
    A("  endif")
    A("endif")
    A("#endif")
    A("")
    A("; ---------- expression pedal (last-toggled-wins) ----------")
    A('kEx chnget "expr"')
    A("kExC changed2 kEx")
    A("if (kExC == 1) then")
    first = True
    for tid in sorted(EXPR):
        cond = "if" if first else "elseif"
        first = False
        A(f"  {cond} (kExprTgt == {tid}) then")
        A(f"    {EXPR[tid]}")
    A("  endif")
    A("endif")
    A("")
    A("kInit = 0")
    A("endin")
    return "\n".join(L)

# ---------------------------------------------------------------- includes
INCLUDES = [
    "globals.orc",
    "effects/preamp.orc", "effects/attack.orc", "effects/h90.orc",
    "effects/pitchfork.orc", "effects/pitchshifter.orc", "effects/gps.orc",
    "effects/phaser.orc", "effects/flanger.orc", "effects/chorus.orc",
    "effects/vibrato.orc", "effects/leslie.orc", "effects/overdrive.orc",
    "effects/marshall.orc", "effects/dualhigain.orc", "effects/ratmz.orc",
    "effects/bigmuff.orc", "effects/bass_preamp.orc", "effects/bass_dist.orc",
    "effects/tremolo.orc", "effects/forms.orc", "effects/formant.orc",
    "effects/bincrush.orc", "effects/bincrusher.orc", "effects/lossy.orc",
    "effects/tape.orc", "effects/ringmod.orc", "effects/automoog.orc",
    "effects/wah.orc",
    # NAM include is conditional — see below
    "effects/spectraldelay.orc", "effects/habit.orc", "effects/mood.orc",
    "effects/granular.orc", "effects/delay.orc", "effects/reverb.orc",
    "effects/stutter.orc", "effects/looper.orc", "effects/drumloops.orc",
    "looper_persist.orc",
    "chase_bliss_clock.orc",
    # Delay/verb expansion (2026-07-26 spec): all four are tempo-lock
    # consumers so they're placed after chase_bliss_clock.orc. movedelay.orc is
    # POLY DLY v5 (2026-10-22), self-contained (movedelay_modes.orc removed).
    "effects/movedelay.orc",
    "effects/tapedelay.orc", "effects/nebula.orc", "effects/shimmerverb.orc",
    "effects/freezeverb.orc", "effects/grainverb.orc",
    "onset_pick.orc", "effects/onward.orc", "effects/impulse_synth.orc",
    "effects/resonator.orc", "effects/ir_reverb.orc", "effects/octaver.orc",
    "effects/envfilter.orc", "effects/blooper.orc", "effects/mood2.orc",
    "effects/compressor.orc", "effects/toneeq.orc", "effects/chainfb.orc",
    "effects/tracksynth.orc", "effects/pllfuzz.orc", "effects/feedbacker.orc",
    "effects/diffusion.orc",
    "signal_chain.orc",
    "state_persist.orc",
]

# Stubs so `csound --syntax-check-only --omacro:CLI_CHECK=1` works without
# Cabbage's built-in opcodes. Real Cabbage never defines CLI_CHECK.
CLI_STUBS = """#ifdef CLI_CHECK
opcode cabbageSet, 0, kSS
kTrig, SChan, SProp xin
endop
opcode cabbageSetValue, 0, Skk
SChan, kVal, kTrig xin
endop
#endif"""

def build_csd(nam=True, standalone=False):
    inc = "\n".join(f'#include "{CSOUND_DIR}/{f}"' for f in INCLUDES)
    nam_inc = f'#include "{CSOUND_DIR}/effects/nam.orc"' if nam else ""
    nam_def = (f"#define VST_NAM ##\n#define NAM_DIR #{NAM_PROFILE_DIR}#"
               if nam else "#define VST_NO_NAM ##")
    if standalone:
        # standalone app: settings + looper audio persist to a state dir;
        # DAW plugin builds leave persistence off (the DAW owns session state)
        nam_def += ("\n#define STATE_PERSIST ##"
                    "\n#define STATE_VST ##"
                    "\n#define STATE_RESTORE_WIDGETS ##"
                    "\n#define AUDIO_DEBUG ##"
                    f"\n#define STATE_DIR #{MAC_STATE_DIR}#")
    nam_opt = f" --opcode-lib={NAM_LIB}" if nam else ""
    ir_dir = os.path.join(CSOUND_DIR, "ir")
    drum_dir = os.path.join(ROOT, "samples", "drumloops")
    return f"""<Cabbage>
{build_cabbage()}
</Cabbage>
<CsoundSynthesizer>
<CsOptions>
-n -d -+rtmidi=NULL -M0 -m0d{nam_opt}
</CsOptions>
<CsInstruments>

; ==========================================================================
; NEPTR VST — GENERATED FILE. Do not hand-edit; run vst/generate_vst.py.
; DSP = the same .orc files the hardware runs (absolute-path includes).
; NAM: same csound-nam.so the NAMDrop plugin loads, profiles from the repo.
; ==========================================================================

#define PRESET_BANK #0#
#define STEREO_IN #1#
#define KSMPS #64#
#define IR_DIR #{ir_dir}#
#define DRUMLOOP_DIR #{drum_dir}#
{nam_def}

{CLI_STUBS}

{inc}
{nam_inc}

{build_control()}

</CsInstruments>
<CsScore>
i98  0 z
i1   0 z
i209 0 z
i210 0 z
i211 0 z
i212 0 z
i213 0 z
i214 0 z
i216 0 z
i190 0 z
i193 0 z
i194 0 z
i195 0 z
</CsScore>
</CsoundSynthesizer>
"""

APP_OUT = os.path.join(HERE, "NEPTR_APP.csd")

def main():
    nam = "--no-nam" not in sys.argv
    if nam and not os.path.isfile(NAM_LIB):
        sys.exit(f"missing {NAM_LIB} — build/copy csound-nam.so or use --no-nam")
    state_orc = emit_state_orc()
    csd = build_csd(nam=nam)
    app_csd = build_csd(nam=nam, standalone=True)
    if "--check" in sys.argv:
        stale = []
        for path, want in ((OUT, csd), (APP_OUT, app_csd), (STATE_ORC, state_orc)):
            try:
                cur = open(path).read()
            except FileNotFoundError:
                cur = None
            if cur != want:
                stale.append(path)
        if stale:
            print("STALE: " + ", ".join(stale))
            sys.exit(1)
        print("OK: generated files are up to date")
        return
    with open(OUT, "w") as f:
        f.write(csd)
    with open(APP_OUT, "w") as f:
        f.write(app_csd)
    print(f"wrote {OUT} + NEPTR_APP.csd + state_persist.orc (NAM {'ON' if nam else 'off'})")
    if "--install" in sys.argv:
        import shutil, subprocess, time
        home = os.path.expanduser("~")
        app = os.path.join(home, "Applications/NEPTR.app")
        app_bin = os.path.join(app, "Contents/MacOS/NEPTR")
        # was the standalone app running? if so we hot-reload it after install
        running = subprocess.run(["pgrep", "-f", app_bin],
                                 capture_output=True).returncode == 0
        if running:
            subprocess.run(["pkill", "-9", "-f", app_bin])
            for _ in range(20):   # wait until it's actually gone (<=2s)
                if subprocess.run(["pgrep", "-f", app_bin],
                                  capture_output=True).returncode != 0:
                    break
                time.sleep(0.1)
        # App runs the SAME plain VST csd as the VST3/AU (identical to Ableton).
        # The standalone-only NEPTR_APP.csd (persistence/debug/glow) is still
        # generated for reference but no longer installed — its widget-restore
        # was breaking the standalone GUI.
        targets = [
            (os.path.join(home, "Library/Audio/Plug-Ins/VST3/NEPTR.vst3"), OUT),
            (os.path.join(home, "Library/Audio/Plug-Ins/Components/NEPTR.component"), OUT),
            (app, OUT),
        ]
        for t, src in targets:
            dst = os.path.join(t, "Contents", "NEPTR.csd")
            if os.path.isdir(os.path.dirname(dst)):
                shutil.copyfile(src, dst)
                subprocess.run(["codesign", "--force", "--deep", "-s", "-", t],
                               check=True, capture_output=True)
                print(f"updated {dst}")
            else:
                print(f"skip (not installed): {t}")
        if running:
            # drop the update marker so the fresh launch flashes the blue glow;
            # state_persist recalls everything so it reopens exactly as it was
            os.makedirs(MAC_STATE_DIR, exist_ok=True)
            open(os.path.join(MAC_STATE_DIR, ".updated"), "w").close()
            subprocess.run(["open", app])
            print("hot-reloaded running NEPTR.app (state recalled, blue-glow flag set)")

if __name__ == "__main__":
    main()
