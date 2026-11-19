# HYPERTRUMPET V2 CPU benchmark firmware.
#   make                        -> build/hx_bench.bin, APP_TYPE=BOOT_SRAM (app in QSPI 0x90040000 via the Daisy bootloader, runs from AXI SRAM)
#   make APP_TYPE=BOOT_NONE     -> internal flash 0x08000000 via ST ROM DFU (only if it fits 128 KB; it does NOT with 14 effects, see size)
#   make size
TARGET    = hx_bench
APP_TYPE ?= BOOT_SRAM
CORE      = ../../../HYPER_CORE
LIBDAISY_DIR ?= $(CORE)/third_party/libDaisy
OPT       = -O2
CPP_STANDARD = -std=gnu++17
FX        = $(CORE)/src/dsp/fx

CPP_SOURCES = main.cpp $(CORE)/src/hal/dfu.cpp $(FX)/fx_engine.cpp $(FX)/fx_registry.cpp $(FX)/mapping/fx_mapping.cpp
C_INCLUDES += -I. -I../common -I$(CORE)/src/hal -I$(FX) -I$(FX)/mapping
C_DEFS     += -DHX_TARGET_SEED -DHX_LOOP_SECONDS=60 -DHX_LOOP_LAYERS=4
ifeq ($(APP_TYPE),BOOT_NONE)
C_DEFS += -DPMOR_DFU_MODE=daisy::System::BootloaderMode::STM
endif
C_DEFS += -DHYPER_DFU_PIN_A=daisy::seed::D2 -DHYPER_DFU_PIN_B=daisy::seed::D6

# libDaisy defines its own (empty-loop) HardFault_Handler in libdaisy.a; ours (common/hx_fault.h) is linked first and must win
LIBS += -Wl,--allow-multiple-definition
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
size: ; arm-none-eabi-size build/$(TARGET).elf
