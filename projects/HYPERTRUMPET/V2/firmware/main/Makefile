# HYPERTRUMPET V2 main instrument firmware (skeleton).
#   make   -> build/hx_main.bin, APP_TYPE=BOOT_SRAM: app in QSPI 0x90040000 via the Daisy bootloader, runs from AXI SRAM (does NOT fit the 128 KB internal flash)
#   flash (bootloader already installed): dfu-util -a 0 -s 0x90040000:leave -D build/hx_main.bin -d ,0483:df11   (the app's `dfu` command enters the bootloader)
# Options: HX_HALLS=1 (Hall valves on A0-A2), HX_JOY=1 (joystick A9/A10), HX_BOOST=0 (400 MHz)
TARGET    = hx_main
APP_TYPE ?= BOOT_SRAM
CORE      = ../../../HYPER_CORE
LIBDAISY_DIR ?= $(CORE)/third_party/libDaisy
OPT       = -O2
CPP_STANDARD = -std=gnu++17
FX        = $(CORE)/src/dsp/fx

CPP_SOURCES = main.cpp drivers/VL53L1X.cpp $(CORE)/src/hal/dfu.cpp $(FX)/fx_engine.cpp $(FX)/fx_registry.cpp $(FX)/mapping/fx_mapping.cpp
C_INCLUDES += -I. -I../common -I$(CORE)/src/hal -I$(CORE)/src/drivers -I$(FX) -I$(FX)/mapping
C_DEFS     += -DHX_TARGET_SEED -DHX_LOOP_SECONDS=60 -DHX_LOOP_LAYERS=4
ifeq ($(HX_HALLS),1)
C_DEFS += -DHX_HALLS=1
endif
ifeq ($(HX_JOY),1)
C_DEFS += -DHX_JOY=1
endif
ifeq ($(HX_BOOST),0)
C_DEFS += -DHX_BOOST=0
endif
ifeq ($(APP_TYPE),BOOT_NONE)
C_DEFS += -DPMOR_DFU_MODE=daisy::System::BootloaderMode::STM
endif
C_DEFS += -DHYPER_DFU_PIN_A=daisy::seed::D2 -DHYPER_DFU_PIN_B=daisy::seed::D6

# libDaisy defines its own (empty-loop) HardFault_Handler in libdaisy.a; ours (common/hx_fault.h) is linked first and must win
LIBS += -Wl,--allow-multiple-definition
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
size: ; arm-none-eabi-size build/$(TARGET).elf
