TARGET = INPUT_TEST
CPP_SOURCES = INPUT_TEST.cpp ../POCKETGORATOR_CODE/VL53L1X_mini.cpp
OPT = -Os
USE_FATFS = 1

# Flash straight to internal flash (0x08000000) — no Daisy bootloader needed.
# This build is small, so the 128KB internal ceiling is irrelevant and we avoid
# the BOOT_SRAM bootloader-install step PMOR itself needs.
APP_TYPE = BOOT_NONE

LIBDAISY_DIR = ../../libDaisy
DAISYSP_DIR  = ../../DaisySP

# oled_pmor.h #includes "tusb.h". PMOR resolves that against the real TinyUSB
# because PMOR compiles the full composite stack. These tests use libDaisy's
# CDC instead, so we ship a local empty tusb.h and put -I. FIRST so it wins.
C_INCLUDES += \
-I. \
-I../POCKETGORATOR_CODE

SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
