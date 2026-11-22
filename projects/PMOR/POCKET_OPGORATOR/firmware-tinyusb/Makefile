# Pocket OpGorator — TinyUSB composite firmware
#
# Builds against libDaisy (for MCU init / audio DMA / codec) and TinyUSB
# (for the USB device stack), both expected as sibling checkouts under
# lib/ -- see README.md "getting the dependencies" for how to fetch them.
# This Makefile follows the standard libDaisy app convention (set TARGET +
# sources, include core/Makefile, which does the actual compile/link/flash
# rules) and layers TinyUSB's sources/includes on top via C_INCLUDES/
# C_SOURCES before that include.

# ------------------------------------------------------------------------
# USB bring-up isolation ladder (see README.md for the full writeup).
# USB-MIDI has never been proven to enumerate on this hardware -- an
# earlier attempt on a different stack (libDaisy's own MidiUsbHandler,
# see ../PocketPassthru-FLASHING.md and its main_midi_trigger.cpp.bak)
# never enumerated. Before trusting the full composite (audio+MIDI+CDC,
# three interface classes at once), these smaller variants isolate the
# question:
#
#   make              -- full composite (default)
#   make midi-only    -- USB-MIDI ONLY. No audio, no CDC. Answers "does
#                         USB-MIDI enumerate on this hardware at all".
#   make midi-audio   -- USB-MIDI + UAC2 audio, still no CDC. Bisection
#                         point if the full composite misbehaves.
#
# ISOLATION= can also be set directly (empty / midi / midi_audio); the
# two convenience targets below just set it. Each variant gets its own
# TARGET name so binaries don't collide, but they DO share BUILD_DIR --
# run `make clean` when switching between variants (also true when
# switching AUDIO_BITS; the object filenames don't encode the build
# flags, only the source list does).
# ------------------------------------------------------------------------
ISOLATION ?=

# core/Makefile (included at the bottom) defines the real `all` target;
# without this, plain `make` would default to whichever target is defined
# FIRST in the concatenated file, which -- since midi-only/midi-audio are
# declared below, before the include -- would otherwise be `midi-only`,
# not `all`. Pin it explicitly.
.DEFAULT_GOAL := all

ifeq ($(ISOLATION),midi)
TARGET = pocket_opgorator_tinyusb_midionly
C_DEFS += -DPOCKET_OPGORATOR_ISOLATION_MIDI
else ifeq ($(ISOLATION),midi_audio)
TARGET = pocket_opgorator_tinyusb_midiaudio
C_DEFS += -DPOCKET_OPGORATOR_ISOLATION_MIDI_AUDIO
else
TARGET = pocket_opgorator_tinyusb
endif

.PHONY: midi-only midi-audio
midi-only:
	$(MAKE) ISOLATION=midi
midi-audio:
	$(MAKE) ISOLATION=midi_audio

# ------------------------------------------------------------------------
# Dependency locations (override on the command line if you keep them
# elsewhere, e.g. `make LIBDAISY_DIR=~/libDaisy TINYUSB_DIR=~/tinyusb`)
# ------------------------------------------------------------------------
LIBDAISY_DIR ?= lib/libDaisy
TINYUSB_DIR  ?= lib/tinyusb

# ------------------------------------------------------------------------
# Audio bit depth: 24 (default, primary format) or 16 (community-tested
# fallback -- see README.md "16-bit fallback"). Usage: `make AUDIO_BITS=16`
# Irrelevant (but harmless) for `midi-only`, which never touches audio.
# ------------------------------------------------------------------------
AUDIO_BITS ?= 24
ifeq ($(AUDIO_BITS),16)
C_DEFS += -DPOCKET_OPGORATOR_AUDIO_16BIT
endif

# ------------------------------------------------------------------------
# App sources
# ------------------------------------------------------------------------
CPP_SOURCES = src/main.cpp

C_SOURCES += \
src/board_stm32h7.c \
src/usb_descriptors.c

# usb_audio_stream.c calls tud_audio_write()/tud_audio_mounted(), which
# are only DECLARED (via tusb.h's own CFG_TUD_AUDIO-gated include of
# audio_device.h) when audio is enabled -- so unlike usb_descriptors.c
# and clock_engine.c (both internally #if-guarded and safe to always
# compile), this file must be excluded from the source list entirely for
# midi-only, or it fails to compile rather than compiling to nothing.
ifneq ($(ISOLATION),midi)
C_SOURCES += src/usb_audio_stream.c
endif

# clock_engine.c is the clock master/follower + PO-sync engine -- only
# meaningful (and only wired up in main.cpp) for the full composite, but
# it's cheap and internally CFG_TUD_CDC-guarded, so no harm building it
# for midi-audio too if you want to experiment; main.cpp simply doesn't
# call into it outside the full-composite branch. midi-only skips it to
# stay as close to "just the TinyUSB MIDI class driver" as possible.
ifeq ($(ISOLATION),)
C_SOURCES += src/clock_engine.c
endif

# TinyUSB core + the 3 class drivers we use + the STM32 DWC2 device
# controller driver. Deliberately NOT pulling in TinyUSB's own hw/bsp
# board-support layer or its host-mode files -- see board_stm32h7.h for why.
# audio_device.c / cdc_device.c are TinyUSB's own class drivers, and both
# wrap their ENTIRE contents in `#if CFG_TUD_ENABLED && CFG_TUD_xxx` --
# compiling them unconditionally (their normal upstream convention) is
# correct and harmless even when tusb_config.h sets CFG_TUD_AUDIO/
# CFG_TUD_CDC to 0 for an isolation build: they just compile to an empty
# translation unit in that case, no exclusion needed here.
C_SOURCES += \
$(TINYUSB_DIR)/src/tusb.c \
$(TINYUSB_DIR)/src/common/tusb_fifo.c \
$(TINYUSB_DIR)/src/device/usbd.c \
$(TINYUSB_DIR)/src/class/audio/audio_device.c \
$(TINYUSB_DIR)/src/class/midi/midi_device.c \
$(TINYUSB_DIR)/src/class/cdc/cdc_device.c \
$(TINYUSB_DIR)/src/portable/synopsys/dwc2/dcd_dwc2.c \
$(TINYUSB_DIR)/src/portable/synopsys/dwc2/dwc2_common.c

C_INCLUDES += \
-Isrc \
-I$(TINYUSB_DIR)/src \
-I$(TINYUSB_DIR)/src/portable/synopsys/dwc2

# App-level TinyUSB config lives in src/tusb_config.h; TinyUSB's own
# sources #include "tusb_config.h" and expect it on the include path (this
# is the standard TinyUSB integration convention, not libDaisy-specific).

# ------------------------------------------------------------------------
# Boot type: BOOT_NONE flashes straight to internal flash at 0x08000000
# and enumerates with ST's DFU PID (0483:df11) for `dfu-util` -- no Daisy
# bootloader involved, matching the "un-brickable via mask-ROM DFU" design
# (README.md). Override to BOOT_QSPI/BOOT_SRAM if you later want this
# chainloaded from the Daisy bootloader instead.
# ------------------------------------------------------------------------
APP_TYPE ?= BOOT_NONE

# ------------------------------------------------------------------------
# Core location, and generic libDaisy app Makefile (compiles/links/flashes)
# ------------------------------------------------------------------------
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
