#include "DaisyDuino.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Set Arduino IDE  ->  Tools > USB Type: "Serial + MIDI"  (or "MIDI")
// so the board enumerates as a USB-MIDI device the computer can talk to.

#define OLED_DC   D3
#define OLED_RST  D4
#define OLED_CS   D7

Adafruit_SSD1306 display(128, 64, &SPI, OLED_DC, OLED_RST, OLED_CS);

DaisyHardware    hw;
MidiUsbHandler   midi;

// ================================================================
// AUDIO — RAW PASSTHROUGH (no DSP)
// ================================================================
void AudioCallback(float **in, float **out, size_t size) {
  // Direct input -> output. No gain, no filtering, no synthesis.
  for (size_t i = 0; i < size; i++) {
    out[0][i] = in[0][i];
    out[1][i] = in[1][i];
  }

  // --- SYNTHESIS DISABLED ---
  // for (size_t i = 0; i < size; i++) {
  //   float sig = osc.Process();   // oscillator / sample playback / effects
  //   out[0][i] = sig;
  //   out[1][i] = sig;
  // }
}

// ================================================================
// BUTTONLESS USB FLASHING
// ----------------------------------------------------------------
// Send MIDI CC 119 value 127 from the computer and the board reboots
// itself into DFU. Then upload as usual -- no BOOT/RESET button.
// One-time setup: do the manual button-DFU ONCE to flash this sketch;
// after that every upload is buttonless.
// ================================================================
void setup() {
  // ---- Daisy audio: in -> out passthrough ----
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  DAISY.begin(AudioCallback);

  // ---- USB MIDI (carries the reboot-to-DFU command) ----
  MidiUsbHandler::Config mcfg;
  midi.Init(mcfg);
  midi.StartReceive();

  // ---- OLED hello world (same stuff in background) ----
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x00)) {
    while (true); // halt if OLED init fails
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 24);
  display.println("HELLO");
  display.setCursor(20, 44);
  display.println("WORLD");
  display.display();
}

void loop() {
  midi.Listen();
  while (midi.HasEvents()) {
    MidiEvent ev = midi.PopEvent();
    if (ev.type == ControlChange) {
      ControlChangeEvent cc = ev.AsControlChange();
      if (cc.control_number == 119 && cc.value == 127) {
        // Reboot into the STM ROM DFU bootloader -> ready to flash.
        System::ResetToBootloader(System::BootloaderMode::STM);
      }
    }
  }
}
