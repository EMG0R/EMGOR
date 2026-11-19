# HYPERTRUMPET V2 bring-up firmware. One libDaisy app, compile-time test selector.
#   make                      -> TEST=01_faders, APP_TYPE=BOOT_NONE (default): internal flash 0x08000000 via ST ROM DFU, no Daisy bootloader, <=128 KB
#   make TEST=02_pots         -> another test (see tests/)
#   make APP_TYPE=BOOT_SRAM   -> app in QSPI 0x90040000 via the Daisy bootloader (needs it installed)
# Output: build/bringup_<TEST>.bin
TEST     ?= 01_faders
# test 06 onwards: the Seed has the Daisy bootloader, build with APP_TYPE=BOOT_SRAM (INTERNAL=0 ./flash.sh does that)
APP_TYPE ?= BOOT_NONE
TARGET    = bringup_$(TEST)
# bisect builds 07a / 07b / 07c are named bringup_07a.bin etc.
ifneq (,$(filter 07a% 07b% 07c% 07d%,$(TEST)))
TARGET    = bringup_$(word 1,$(subst _, ,$(TEST)))
endif
CORE      = ../../../HYPER_CORE
LIBDAISY_DIR ?= $(CORE)/third_party/libDaisy
OPT       = -Os
ifneq (,$(filter 10% 11% 12% 13% 14%,$(TEST)))
OPT       = -O2      # effects test only: the DSP loops are far slower at -Os; every other test keeps -Os so 08_mic stays byte-identical
LDSCRIPT  = axi.lds   # adds .axi_bss (NOLOAD, AXI SRAM, cached): the drone hot arrays
endif

CPP_SOURCES = main.cpp $(CORE)/src/hal/dfu.cpp
# repro 09 variants: a = adc then audio, b = audio then adc, c = no oversampling, d = no audio (control)
ifneq (,$(filter 09%,$(TEST)))
CPP_SOURCES = repro09.cpp $(CORE)/src/hal/dfu.cpp
C_DEFS += $(if $(filter 09a%,$(TEST)),-DREPRO_ORDER=0 -DREPRO_OVS=32 -DREPRO_AUDIO=1,)$(if $(filter 09b%,$(TEST)),-DREPRO_ORDER=1 -DREPRO_OVS=32 -DREPRO_AUDIO=1,)$(if $(filter 09c%,$(TEST)),-DREPRO_ORDER=0 -DREPRO_OVS=0 -DREPRO_AUDIO=1,)$(if $(filter 09d%,$(TEST)),-DREPRO_ORDER=0 -DREPRO_OVS=32 -DREPRO_AUDIO=0,)
TARGET = bringup_$(word 1,$(subst _, ,$(TEST)))
endif
ifneq (,$(filter 05% 06% 07% 08% 10% 12% 13% 14%,$(TEST)))
CPP_SOURCES += VL53L1X.cpp
endif
# test 11: spare-Seed effect benchmark: its own main, no dfu.cpp (no boot-gesture pins read), no sensors
ifneq (,$(filter 11%,$(TEST)))
CPP_SOURCES = bench.cpp
endif
C_INCLUDES += -I. -I$(CORE)/src/hal
C_DEFS     += $(EXTRA_DEFS)
C_DEFS     += -DTEST_ID_STR=\"$(TEST)\" -DTEST_SEL_$(TEST)=1
# dfu.cpp must jump to the bootloader that matches how the app is linked
ifeq ($(APP_TYPE),BOOT_NONE)
C_DEFS += -DPMOR_DFU_MODE=daisy::System::BootloaderMode::STM
endif
# the two hardware-DFU-escape keys (both low at power-on): encoder 1 SW = D3 (pin 4), encoder 2 SW = D6 (pin 7)
C_DEFS += -DHYPER_DFU_PIN_A=daisy::seed::D3 -DHYPER_DFU_PIN_B=daisy::seed::D6

SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
