# pitchbench - offline tracker latency/accuracy/crosstalk harness.
#   make            -> ./pb_new  (current ../../pitch_tracker.cpp)
#   make old        -> ./pb_old  (archived v1: ../../../archive/pitch_tracker_v1.cpp.txt)
#   ./pb_new all -23 add | grep -v '^md \|^st '     (shifter diag lines filtered)
#   ./pb_new steady -23 add 25                      (strings tuned 25 c flat)
CXX ?= c++
CXXFLAGS = -O2 -std=c++14 -Wall -I ../stub -I ../..
CORE = ../../dsp_core.cpp ../../shifter.cpp ../../lpc.cpp

pb_new: pitchbench.cpp ../../pitch_tracker.cpp $(CORE) ../../hyperguitar.h
	$(CXX) $(CXXFLAGS) pitchbench.cpp $(CORE) ../../pitch_tracker.cpp -o pb_new -lm

old: pb_old
pb_old: pitchbench.cpp ../../../archive/pitch_tracker_v1.cpp.txt $(CORE)
	$(CXX) $(CXXFLAGS) pitchbench.cpp $(CORE) -x c++ ../../../archive/pitch_tracker_v1.cpp.txt -o pb_old -lm

clean:
	rm -f pb_new pb_old pb_new_ops pb_old_ops dump.f32
.PHONY: clean old

# op-count builds (multiply-accumulates in the tracker's correlation loops)
ops: pitchbench.cpp ../../pitch_tracker.cpp $(CORE)
	$(CXX) $(CXXFLAGS) -DPT_OPCOUNT pitchbench.cpp $(CORE) ../../pitch_tracker.cpp -o pb_new_ops -lm
.PHONY: ops
