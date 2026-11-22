# synthbench - host harness for synth.cpp (velocity engine + timbre centroids).
#   make && ./synthbench            -> scripted scenarios
#   ./synthbench centroid           -> steady-state spectral centroid per mode
CXX ?= c++
CXXFLAGS = -O2 -std=c++11 -Wall -Wextra -I ../stub -I ../..

all: synthbench

names.inc: ../../render.cpp
	sed -n '/^const char \*SYNTH_NAMES\[SYNTH_NMODES\] = {/,/^};/p' ../../render.cpp > names.inc

synthbench: synthbench.cpp ../../synth.cpp ../../hyperguitar.h names.inc
	$(CXX) $(CXXFLAGS) synthbench.cpp -o synthbench -lm

clean:
	rm -f synthbench names.inc
.PHONY: clean
