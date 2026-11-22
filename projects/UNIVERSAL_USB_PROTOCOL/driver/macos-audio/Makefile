# Makefile for the Omniplex Audio HAL plugin.
#
# Builds a CFPlugIn bundle (conventionally given the .driver extension for
# AudioServerPlugIns) directly with clang -- no Xcode project involved,
# since this machine only has the Command Line Tools, not full Xcode.app.
#
# Output: build/OmniplexAudio.driver/Contents/{Info.plist,MacOS/OmniplexAudioPlugIn}

BUNDLE_NAME   := OmniplexAudio.driver
BUILD_DIR     := build
BUNDLE_DIR    := $(BUILD_DIR)/$(BUNDLE_NAME)
CONTENTS_DIR  := $(BUNDLE_DIR)/Contents
MACOS_DIR     := $(CONTENTS_DIR)/MacOS
BINARY_NAME   := OmniplexAudioPlugIn
BINARY        := $(MACOS_DIR)/$(BINARY_NAME)

CC            := clang
SDK           := $(shell xcrun --sdk macosx --show-sdk-path)
CFLAGS        := -isysroot $(SDK) -arch arm64 -arch x86_64 -mmacosx-version-min=11.0 \
                  -fno-common -Wall -Wextra -Wno-unused-parameter -O2 -g \
                  -I include
LDFLAGS       := -isysroot $(SDK) -arch arm64 -arch x86_64 -mmacosx-version-min=11.0 \
                  -bundle -framework CoreFoundation -framework CoreAudio \
                  -Wl,-exported_symbol,_OmniplexAudio_Create

SRC           := src/OmniplexAudioPlugIn.c

.PHONY: all clean install uninstall

all: $(BUNDLE_DIR)

$(BUNDLE_DIR): $(BINARY) Info.plist
	mkdir -p "$(CONTENTS_DIR)"
	cp Info.plist "$(CONTENTS_DIR)/Info.plist"
	@touch "$(BUNDLE_DIR)"

$(BINARY): $(SRC) include/omniplex_shared_ring.h
	mkdir -p "$(MACOS_DIR)"
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o "$(BINARY)"

clean:
	rm -rf "$(BUILD_DIR)"

install: $(BUNDLE_DIR)
	./install.sh

uninstall:
	./uninstall.sh
