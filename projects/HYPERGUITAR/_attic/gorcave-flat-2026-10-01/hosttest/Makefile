CXX ?= c++
CXXFLAGS = -O2 -std=c++11 -Wall -I stub -I ..

SRCS = test_main.cpp ../dsp_core.cpp ../pitch_tracker.cpp ../shifter.cpp ../lpc.cpp

run_tests: $(SRCS) ../hyperguitar.h stub/Bela.h
	$(CXX) $(CXXFLAGS) $(SRCS) -o run_tests -lm

test: run_tests
	./run_tests

clean:
	rm -f run_tests

.PHONY: test clean
