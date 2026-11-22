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
