# HYPER_CORE/src/dsp/fx -- host tests + bench. `make test` runs every tests/test_fx_*.cpp; `make bench` times all effects.
CXX ?= c++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
TESTS := $(patsubst tests/test_fx_%.cpp,out/test_%,$(wildcard tests/test_fx_*.cpp))
.PHONY: test bench engine clean
test: $(TESTS)
	@fail=0; for t in $(TESTS); do ./$$t || fail=1; done; exit $$fail
out/test_%: tests/test_fx_%.cpp $(wildcard fx_*.h) $(wildcard tests/*.h) | out
	$(CXX) $(CXXFLAGS) $< -o $@
out:
	mkdir -p out
engine: out/test_engine
out/test_engine: tests/test_engine.cpp fx_engine.cpp fx_registry.cpp $(wildcard fx_*.h) | out
	$(CXX) $(CXXFLAGS) tests/test_engine.cpp fx_engine.cpp fx_registry.cpp -o $@ && ./$@
bench: out/bench
	./out/bench
out/bench: tests/bench.cpp fx_registry.cpp $(wildcard fx_*.h) | out
	$(CXX) -O2 -std=c++17 tests/bench.cpp fx_registry.cpp -o $@
clean:
	rm -rf out
