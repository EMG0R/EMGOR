TARGET = PMOR_V3
# pmor_core/ is the hardware-free core (engines, FX, sequencer, patch format, param schema); hw/ ui/ store/
# are the thin hardware layer over it. pmor_core/host/ (host-only glue) is deliberately NOT globbed here.
# LIBDAISY_DIR/DAISYSP_DIR/TINYUSB_DIR are overridable (?=); check.sh passes them.
# Every module that exists (or will exist) is picked up by wildcard, so a
# module another agent has not written yet does not break the build — its
# symbols come from pmor_core/stubs.cpp (weak) until the strong file lands.
# NOTE: libDaisy's core Makefile puts object files FLAT in build/ by basename,
# so every .cpp basename across hw/ pmor_core/ ui/ store/ MUST be unique
# (e.g. ui/render_oled.cpp + ui/render_trellis.cpp, never two render.cpp).
CPP_SOURCES = main.cpp $(wildcard hw/*.cpp) $(wildcard pmor_core/*.cpp) $(wildcard ui/*.cpp) $(wildcard store/*.cpp)
OPT = -Os
LDFLAGS += -Wl,--allow-multiple-definition   # hw/bootguard.cpp overrides libDaisy's HardFault_Handler (reset instead of an endless spin); our object links first
ifdef DRUMBENCH
C_DEFS += -DPMOR_DRUMBENCH=1
endif
C_DEFS += -DPMOR_FW_VERSION=\"$(shell git rev-parse --short HEAD 2>/dev/null || echo dev)\"
# Run from SRAM via the Daisy bootloader — lifts the 128KB internal-flash
# ceiling to ~480KB (app stored in QSPI, copied to SRAM at boot).
# Flash flow: `make program-boot` once to install the bootloader, then
# `make program-dfu` for the app from then on.
APP_TYPE = BOOT_SRAM
USE_FATFS = 1
LIBDAISY_DIR ?= ../../libDaisy
DAISYSP_DIR ?= ../../DaisySP

C_INCLUDES += -I. -Ihw -Ipmor_core -Iui -Istore

# ------------------------------------------------------------------------
# Tap-tempo analyzer + 14-bit tempo wire format: ONE header, shared with the Pi
# transport (demiurge-clock). Standalone PMOR and the Pi run literally the same
# tap code. Not copied here on purpose (one mechanism, not two that must stay
# identical) -- now VENDORED in pmor_core/third_party/ (see README.txt there).
# ------------------------------------------------------------------------
C_INCLUDES += -Ipmor_core/third_party   # vendored demiurge-transport.h (shared tap analyzer + 14-bit tempo wire)

# ------------------------------------------------------------------------
# TinyUSB composite (UAC2 capture + USB-MIDI + CDC-ACM) + clock/PO-sync
# engine, ported from firmware-tinyusb/ (proven on hardware there). Sources
# live in usb/ (copied+integrated, not the original firmware-tinyusb/src —
# that project is read-only reference here). TinyUSB itself is NOT
# vendored into this repo; point TINYUSB_DIR at the checkout firmware-
# tinyusb/ already uses (override on the command line if you keep it
# elsewhere, e.g. `make TINYUSB_DIR=~/tinyusb`).
# Mac (EMGOR's machine):
#   TINYUSB_DIR ?= /Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/Pocket-OpGorator/firmware-tinyusb/lib/tinyusb
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
