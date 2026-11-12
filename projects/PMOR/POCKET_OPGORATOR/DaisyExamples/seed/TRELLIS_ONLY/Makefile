TARGET = TRELLIS_ONLY
CPP_SOURCES = TRELLIS_ONLY.cpp
OPT = -Os

# Flash straight to internal flash (0x08000000) — no Daisy bootloader needed.
# This build is tiny, so the 128KB internal ceiling is irrelevant and we avoid
# the BOOT_SRAM bootloader-install step PMOR needs.
APP_TYPE = BOOT_NONE

LIBDAISY_DIR = ../../libDaisy
DAISYSP_DIR  = ../../DaisySP

SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
