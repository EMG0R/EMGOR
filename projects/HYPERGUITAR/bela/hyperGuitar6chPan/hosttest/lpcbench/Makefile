# lpcbench - host harness for ../../lpc.cpp
#   make            -> ./lb_new  (current lpc.cpp, with the LPC_BENCH debug hook)
#   make old        -> ./lb_old  (lpc_old.cpp.txt = e4b7844)
#   make sweep OPTS="-DLPC_PREEMPH=0.9f"  -> ./lb_sweep with tunable overrides
CXX ?= c++
CXXFLAGS = -O2 -std=c++14 -Wall -Wextra -I ../stub -I ../..
NEWSRC = -DLPC_BENCH -DLPC_SRC='"../../lpc.cpp"'
lb_new: lpcbench.cpp ../../lpc.cpp ../../hyperguitar.h
	$(CXX) $(CXXFLAGS) $(NEWSRC) lpcbench.cpp -o lb_new -lm
old: lb_old
lb_old: lpcbench.cpp lpc_old.cpp.txt ../../hyperguitar.h
	$(CXX) $(CXXFLAGS) -DLPC_SRC='"lpc_old.cpp.txt"' lpcbench.cpp -o lb_old -lm
sweep: lpcbench.cpp ../../lpc.cpp
	$(CXX) $(CXXFLAGS) $(NEWSRC) $(OPTS) lpcbench.cpp -o lb_sweep -lm
clean:
	rm -f lb_new lb_old lb_sweep
.PHONY: old sweep clean
