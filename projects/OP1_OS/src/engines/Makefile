# OP FREAK sound engines: host tests / demos / bench and bfin cross-compile.
# make test | demos | bench | bfin | sim | all      (run from src/engines; outputs under ../../build)
ROOT   := $(abspath ../..)
BUILD  := $(ROOT)/build
CC     ?= cc
COMMON := ../common/op_common.c
PURP   := purp/purp.c
LD     := lildrummer/lildrummer.c
EHINC  := $(ROOT)/vendor/op1-sdk/features/enginehost
SDKINC := $(ROOT)/vendor/op1-sdk/include
BFIN   := $(ROOT)/toolchain/bfin/bin/bfin-elf-gcc
BFLAGS := -mcpu=bf524 -O2 -ffreestanding -fno-builtin -fno-tree-loop-distribute-patterns -Wall -Wextra \
          -I../common -Ipurp -Ilildrummer -I$(EHINC) -I$(SDKINC)
PY     := $(ROOT)/.venv/bin/python
ITER   ?= 300

all: test demos bench bfin

$(BUILD)/eng:
	mkdir -p $@ $(BUILD)/demos

# fuzz + unit tests under UBSan: any signed overflow / bad shift aborts the run
test: $(BUILD)/eng
	$(CC) -O1 -g -Wall -Wextra -fsanitize=undefined,address -fno-sanitize-recover=all -o $(BUILD)/eng/fuzz \
	  test/fuzz.c $(PURP) $(LD) $(COMMON) -lm
	$(BUILD)/eng/fuzz $(ITER)
	$(CC) -O2 -Wall -DLD_SDK_TAG=7 -I$(EHINC) -o $(BUILD)/eng/adapter_test test/adapter_test.c purp/sdk_adapter.c \
	  lildrummer/sdk_adapter.c $(PURP) $(LD) $(COMMON) -lm
	$(BUILD)/eng/adapter_test

demos: $(BUILD)/eng
	$(CC) -O2 -Wall -o $(BUILD)/eng/demo_purp test/demo_purp.c $(PURP) $(COMMON) -lm
	$(CC) -O2 -Wall -o $(BUILD)/eng/demo_ld test/demo_lildrummer.c $(LD) $(COMMON) -lm
	$(BUILD)/eng/demo_purp $(BUILD)/demos
	$(BUILD)/eng/demo_ld $(BUILD)/demos
	$(PY) test/analyze.py $(BUILD)/demos/purp_*.wav $(BUILD)/demos/lildrummer_*.wav

bench: $(BUILD)/eng
	$(CC) -O2 -o $(BUILD)/eng/bench test/bench.c $(PURP) $(LD) $(COMMON) -lm
	$(BUILD)/eng/bench

bfin: $(BUILD)/eng
	for f in $(COMMON) $(PURP) $(LD) purp/sdk_adapter.c; do \
	  $(BFIN) $(BFLAGS) -c $$f -o $(BUILD)/eng/`basename $$f`.bfin.o || exit 1; echo "bfin ok: $$f"; done
	$(BFIN) $(BFLAGS) -DLD_SDK_TAG=7 -c lildrummer/sdk_adapter.c -o $(BUILD)/eng/ld_adapter.bfin.o && echo "bfin ok: lildrummer/sdk_adapter.c"

sim:
	$(PY) sim/run_bench.py

.PHONY: all test demos bench bfin sim
