TARGET = PMOR_V2
CPP_SOURCES = PMOR_V2.cpp VL53L1X_mini.cpp
OPT = -Os
# Run from SRAM via the Daisy bootloader — lifts the 128KB internal-flash
# ceiling to ~480KB (app stored in QSPI, copied to SRAM at boot).
# Flash flow: `make program-boot` once to install the bootloader, then
# `make program-dfu` for the app from then on.
APP_TYPE = BOOT_SRAM
# Bisect switch: `make USB_ONLY=1` builds a USB-only firmware (no instrument
# hardware init) to isolate whether BOOT_SRAM or the instrument breaks USB.
ifdef MOSI_OD
C_DEFS += -DPMOR_MOSI_OPEN_DRAIN
endif
ifdef OLED_USB_ONLY
C_DEFS += -DPMOR_OLED_USB_ONLY
endif
ifdef NO_USB
C_DEFS += -DPMOR_NO_USB
endif
ifdef CHUNK
C_DEFS += -DPMOR_CHUNK=$(CHUNK)
endif
ifdef NO_LIDAR
C_DEFS += -DPMOR_NO_LIDAR
endif
ifdef NO_SD
C_DEFS += -DPMOR_NO_SD
endif
ifdef QUIET_BOOT
C_DEFS += -DPMOR_QUIET_BOOT
endif
USE_FATFS = 1
#CPP_SOURCES = SDTest.cpp
LIBDAISY_DIR = ../../libDaisy
DAISYSP_DIR = ../../DaisySP

# ------------------------------------------------------------------------
# TinyUSB composite (UAC2 capture + USB-MIDI + CDC-ACM) + clock/PO-sync
# engine, ported from firmware-tinyusb/ (proven on hardware there). Sources
# live in usb/ (copied+integrated, not the original firmware-tinyusb/src —
# that project is read-only reference here). TinyUSB itself is NOT
# vendored into this repo; point TINYUSB_DIR at the checkout firmware-
# tinyusb/ already uses (override on the command line if you keep it
# elsewhere, e.g. `make TINYUSB_DIR=~/tinyusb`).
# ------------------------------------------------------------------------
TINYUSB_DIR ?= /Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/Pocket-OpGorator/firmware-tinyusb/lib/tinyusb

C_SOURCES += \
usb/board_stm32h7.c \
usb/usb_descriptors.c \
usb/usb_audio_stream.c \
usb/clock_engine.c \
$(TINYUSB_DIR)/src/tusb.c \
$(TINYUSB_DIR)/src/common/tusb_fifo.c \
$(TINYUSB_DIR)/src/device/usbd.c \
$(TINYUSB_DIR)/src/class/audio/audio_device.c \
$(TINYUSB_DIR)/src/class/midi/midi_device.c \
$(TINYUSB_DIR)/src/class/cdc/cdc_device.c \
$(TINYUSB_DIR)/src/portable/synopsys/dwc2/dcd_dwc2.c \
$(TINYUSB_DIR)/src/portable/synopsys/dwc2/dwc2_common.c

C_INCLUDES += \
-Iusb \
-I$(TINYUSB_DIR)/src \
-I$(TINYUSB_DIR)/src/portable/synopsys/dwc2

SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
