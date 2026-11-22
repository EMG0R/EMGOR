TARGET = SDTEST
CPP_SOURCES = SDTEST.cpp
OPT = -Os
USE_FATFS = 1
# BOOT_SRAM so the Daisy bootloader survives and PMOR_V2 can go straight back.
APP_TYPE = BOOT_SRAM
# DFU mode MUST match APP_TYPE: QSPI app -> DAISY bootloader, not the ST ROM.
C_DEFS += -DBRINGUP_DFU_MODE=daisy::System::BootloaderMode::DAISY_INFINITE_TIMEOUT
LIBDAISY_DIR = ../../libDaisy
DAISYSP_DIR  = ../../DaisySP
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
