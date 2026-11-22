# FULL-CHAIN host benchmark.
#
# Unlike hosttest/, this links ALL NINE translation units - including
# render.cpp, effects.cpp and synth.cpp - and drives the REAL render()
# block by block. Every earlier bench in this project covered only
# voiceProcess + shifter + tracker + lpc and excluded the effects chain,
# the mix and the limiter; that partial view gave the wrong headroom
# answer twice, most recently in Aug 2026 when the whole regression turned
# out to be in pitch_tracker.cpp and was invisible to a DSP-core bench.
#
#   make                      build ./bench
#   ./bench <scene> <secs> [dumpfile] [reps]
#     scene 0 = everything off      1 = FORMANT only
#           2 = several fx on       3 = FORMANT in LPC mode
#           10+n = effects-chain-only microbench for scene n
#     dumpfile: writes stereo float32 (all L, then all R) AND enables the
#     scheduled slot-press / encoder-detent events, for A/B artifact runs:
#         ./bench 1 7 a.f32 1 ; ./bench 1 7 b.f32 1 ; python3 disc.py a.f32 b.f32
#           4 = pitch up +7           5 = pitch up +12
#           6 = up12 + formant        7 = up7 + formant down
#
# LEVEL REALISM (2026-09-15). The generator's plucks are amp 0.10..0.32 on
# all six channels; the instrument is ~10x quieter than that and its two
# plain top strings are several dB below the four wound ones. Three env
# knobs express that, all no-ops when unset so every historic number from
# this bench still reproduces:
#
#   HG_BENCH_PICKUP=0.1   device level + the per-string pickup taper
#   HG_BENCH_SOLO=s       material on string s only (9 = none: silence)
#   HG_BENCH_NOISE=3e-4   white noise on EVERY channel, incl. silent ones
#
# Loud, equal-level material is what hid the onset-detector calibration
# bug for a year: at amp 0.2 an absolute 0.004 floor is 3% of the signal
# and invisible, at device level on the high e it is 90% of it. Run any
# threshold change BOTH ways, and run it against HG_BENCH_NOISE with no
# plucks to check the false-positive side.
#
# THE HG_PROBE BUILD is the tool for TEXTURE problems, which disc.py
# cannot see - it is a click detector and reads 0 hits at device levels.
# Probe events, per string: O = onset, M = mode change, P = per-hop state
# (now including env and the tracker's gate verdict, so mode RESIDENCY can
# be weighted by whether the string was audible), L = grain launch,
# T/S = tap and SOLAD splices, A = re-anchor.
#
# NOTE ON UNITS: hg_osc.cpp is NOT linked - it is a UDP socket plus the
# oscpkt header-only library, neither of which exists here. stub/
# hg_osc_stub.cpp stands in for it ("nobody ever pressed anything").
# hg_lanes.cpp IS linked for real, so the lane->effect map is the device's.
#
# COMPARING AGAINST THE ARCHIVED BUILD: point SRC at the other tree, e.g.
#   make SRC=../../../archive/v5-preset-era OUT=bench_arc
#
# CAUTION ON WALL TIME. This host (Apple arm64) has wide vector units and
# cheap transcendentals; the tracker's correlation loops that dominate the
# Cortex-A53 are nearly free here. Read the OP COUNTS as the transferable
# number and treat host microseconds as a weak secondary signal. Op counts
# come from coverage instrumentation:
#   clang++ -O1 -fprofile-instr-generate -fcoverage-mapping ... -o cov
#   LLVM_PROFILE_FILE=p.profraw ./cov 1 6 ; xcrun llvm-profdata merge -sparse p.profraw -o p.profdata
#   xcrun llvm-cov show ./cov -instr-profile=p.profdata --show-line-counts ../../pitch_tracker.cpp
# Note that llvm-cov line counts for small `static inline` helpers depend
# on inlining decisions, so compare LOOP BODIES, not whole-file totals.

CXX ?= c++
SRC ?= ../..
OUT ?= bench
CXXFLAGS = -O2 -std=c++11 -I stub -I $(SRC)

UNITS = render effects synth dsp_core pitch_tracker shifter lpc presets trellis_ui hg_lanes
SRCS = bench_main.cpp stub/hg_osc_stub.cpp $(addprefix $(SRC)/,$(addsuffix .cpp,$(UNITS)))

$(OUT): $(SRCS) stub/Bela.h
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(OUT) -lm

clean:
	rm -f bench bench_arc *.f32 *.profraw *.profdata

.PHONY: clean
