# Host tests + bfin cross-compile for the OP FREAK generative sequencer.
ROOT   := $(abspath ../..)
SRCS   := seq_host.c seq_mode_turing.c seq_mode_euclid.c seq_mode_markov.c seq_mode_gamelan.c \
          seq_mode_cellular.c seq_mode_total.c
BUILD  := $(ROOT)/build/seq
DEMOS  := $(ROOT)/build/demos
CC     ?= cc
BFIN   := $(ROOT)/toolchain/bfin/bin/bfin-elf-gcc
BFLAGS := -mcpu=bf524 -Os -ffreestanding -fno-builtin -fno-common -ffunction-sections \
          -fdata-sections -Wall -Wextra -I $(ROOT)/vendor/op1-sdk/include -I .

.PHONY: test bfin clean
test: $(BUILD)/test_seq
	mkdir -p $(DEMOS)
	$(BUILD)/test_seq $(DEMOS)

$(BUILD)/test_seq: test/test_seq.c $(SRCS) seq.h seq_mod.h
	mkdir -p $(BUILD)
	$(CC) -std=c99 -O1 -g -Wall -Wextra -I . -o $@ test/test_seq.c $(SRCS)

bfin: $(SRCS:%.c=$(BUILD)/bfin/%.o) $(BUILD)/bfin/sdk_adapter.o
	@echo "bfin objects OK:"; ls -l $^ | awk '{print $$5, $$9}'
	$(ROOT)/toolchain/bfin/bin/bfin-elf-size $^
	@echo "undefined symbols (should be only te/gfx/adapter hooks):"; \
	$(ROOT)/toolchain/bfin/bin/bfin-elf-nm -u $^ | sort -u | grep -v ':' || true

$(BUILD)/bfin/%.o: %.c seq.h seq_mod.h
	mkdir -p $(BUILD)/bfin
	$(BFIN) $(BFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)
