# HYPER_CORE import
# origin : ~/Documents/_EMGOR_SYNTH/______2026NEW/hyperGuitar/bela/hyperGuitar6chPan/hosttest/Makefile
# commit : hyperGuitar 6088064
# changes: none (verbatim). names.inc rule depends on render.cpp, which is not imported; names.inc is checked in
# licence: Emory (own work)
CXX ?= c++
CXXFLAGS = -O2 -std=c++11 -Wall -I stub -I ..

SRCS = test_main.cpp ../dsp_core.cpp ../pitch_tracker.cpp ../shifter.cpp ../lpc.cpp

run_tests: $(SRCS) ../hyperguitar.h stub/Bela.h ../synth.cpp test_synth.inc names.inc
	$(CXX) $(CXXFLAGS) $(SRCS) -o run_tests -lm

names.inc: ../render.cpp
	sed -n '/^const char \*SYNTH_NAMES\[SYNTH_NMODES\] = {/,/^};/p' ../render.cpp > names.inc

test: run_tests
	./run_tests

clean:
	rm -f run_tests names.inc

.PHONY: test clean
