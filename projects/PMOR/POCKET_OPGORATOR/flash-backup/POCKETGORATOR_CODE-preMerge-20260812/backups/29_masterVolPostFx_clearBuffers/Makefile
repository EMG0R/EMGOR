TARGET = POCKETGORATOR_CODE
CPP_SOURCES = POCKETGORATOR_CODE.cpp VL53L1X_mini.cpp
OPT = -Os
# Run from SRAM via the Daisy bootloader — lifts the 128KB internal-flash
# ceiling to ~480KB (app stored in QSPI, copied to SRAM at boot).
# Flash flow: `make program-boot` once to install the bootloader, then
# `make program-dfu` for the app from then on.
APP_TYPE = BOOT_SRAM
USE_FATFS = 1
#CPP_SOURCES = SDTest.cpp
LIBDAISY_DIR = ../../libDaisy
DAISYSP_DIR = ../../DaisySP
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
