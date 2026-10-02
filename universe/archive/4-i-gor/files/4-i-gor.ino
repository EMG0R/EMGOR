// 4 i Got — Quad Audio Interface
// Teensy 4.0 | Teensyduino required
// Tools > USB Type: "MIDI + Audio"
//
// USB Audio: 4 in / 4 out (two stereo pairs)
//   USB Out ch 0-1 ← ADC 1 (PCM1808, SAI1 RX, pin 8)
//   USB Out ch 2-3 ← ADC 2 (PCM1808, SAI2 RX, pin 3)
//   USB In  ch 0-1 → DAC 1 (PCM5102, SAI1 TX, pin 7)
//   USB In  ch 2-3 → DAC 2 (PCM5102, SAI2 TX, pin 2)
//
// MIDI:
//   Serial1 RX (pin 0) ← MIDI IN (6N138)
//   Serial1 TX (pin 1) → MIDI OUT
//   pin 4              ← Sustain pedal (INPUT_PULLUP, LOW = pressed)
//
// Teensy USB Audio does 2-channel stereo natively.
// For 4-channel (quad), the host DAW sees two stereo USB devices
// sharing one Teensy USB connection. AudioInputUSB and AudioOutputUSB
// handle channels 0-1. Channels 2-3 use the second I2S bus (SAI2)
// with direct hardware passthrough — the computer sends/receives
// all 4 channels over USB, Teensy routes them to the correct I2S bus.
//
// Note: Teensy Audio Library USB audio is 2-ch only. To get true
// 4-ch USB, you'd need custom USB descriptors. This firmware uses
// USB for ch 0-1 and treats ch 2-3 as hardware direct monitoring
// (ADC2 → DAC2 with gain control). For full 4-ch USB see Teensy
// forums on custom audio USB descriptors.

#include <Audio.h>
#include <MIDI.h>

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI_HW);

// ── Audio objects ─────────────────────────────────────────

// Hardware I2S
AudioInputI2S       adcIn1;     // ADC 1 — SAI1 (pin 8)
AudioInputI2S2      adcIn2;     // ADC 2 — SAI2 (pin 3)
AudioOutputI2S      dacOut1;    // DAC 1 — SAI1 (pin 7)
AudioOutputI2S2     dacOut2;    // DAC 2 — SAI2 (pin 2)

// USB I/O (stereo — channels 0-1)
AudioInputUSB       usbIn;      // computer → Teensy (ch 0-1)
AudioOutputUSB      usbOut;     // Teensy → computer (ch 0-1)

// Output mixers — blend USB playback + direct monitor per channel
AudioMixer4         mixL1;      // DAC 1 left
AudioMixer4         mixR1;      // DAC 1 right
AudioMixer4         mixL2;      // DAC 2 left
AudioMixer4         mixR2;      // DAC 2 right

// ── Connections ──────────────────────────────────────────

// ADC 1 → USB Out (recording channels 0-1)
AudioConnection     c0(adcIn1, 0, usbOut,  0);
AudioConnection     c1(adcIn1, 1, usbOut,  1);

// USB In → output mixer ch0 (playback from computer → DAC 1)
AudioConnection     c2(usbIn,  0, mixL1,   0);
AudioConnection     c3(usbIn,  1, mixR1,   0);

// ADC 1 → output mixer ch1 (direct monitoring → DAC 1)
AudioConnection     c4(adcIn1, 0, mixL1,   1);
AudioConnection     c5(adcIn1, 1, mixR1,   1);

// ADC 2 → DAC 2 (direct hardware monitoring, ch 2-3)
AudioConnection     c6(adcIn2, 0, mixL2,   0);
AudioConnection     c7(adcIn2, 1, mixR2,   0);

// Mixers → DACs
AudioConnection     c8(mixL1,  0, dacOut1, 0);
AudioConnection     c9(mixR1,  0, dacOut1, 1);
AudioConnection    c10(mixL2,  0, dacOut2, 0);
AudioConnection    c11(mixR2,  0, dacOut2, 1);

// ── Pins ─────────────────────────────────────────────────
const int PIN_SUSTAIN = 4;

// ── State ─────────────────────────────────────────────────
float masterGain   = 0.8f;
float monitorGain  = 0.0f;   // direct monitoring off by default (avoid feedback)
bool  sustainHeld  = false;
uint8_t midiChannel = 1;

// ── Prototypes ────────────────────────────────────────────
void setPlaybackGain(float g);
void setMonitorGain(float g);

// Hardware MIDI → USB MIDI (forward only, no echo back to hardware)
void hwNoteOn(byte ch, byte note, byte vel)  { usbMIDI.sendNoteOn(note, vel, ch); }
void hwNoteOff(byte ch, byte note, byte vel) { usbMIDI.sendNoteOff(note, vel, ch); }
void hwCC(byte ch, byte num, byte val) {
  if (num == 7) { masterGain = val / 127.0f; setPlaybackGain(masterGain); }
  usbMIDI.sendControlChange(num, val, ch);
}

// USB MIDI → Hardware MIDI (forward only, no echo back to USB)
void usbNoteOn(byte ch, byte note, byte vel)  { MIDI_HW.sendNoteOn(note, vel, ch); }
void usbNoteOff(byte ch, byte note, byte vel) { MIDI_HW.sendNoteOff(note, vel, ch); }
void usbCC(byte ch, byte num, byte val) {
  if (num == 7) { masterGain = val / 127.0f; setPlaybackGain(masterGain); }
  MIDI_HW.sendControlChange(num, val, ch);
}

// ─────────────────────────────────────────────────────────
void setup() {
  AudioMemory(32);

  setPlaybackGain(masterGain);
  setMonitorGain(monitorGain);

  // Hardware MIDI (Serial1 = pins 0/1) at 31250 baud
  MIDI_HW.begin(MIDI_CHANNEL_OMNI);
  MIDI_HW.setHandleNoteOn(hwNoteOn);
  MIDI_HW.setHandleNoteOff(hwNoteOff);
  MIDI_HW.setHandleControlChange(hwCC);

  // USB MIDI — separate handlers, forward to hardware only
  usbMIDI.setHandleNoteOn(usbNoteOn);
  usbMIDI.setHandleNoteOff(usbNoteOff);
  usbMIDI.setHandleControlChange(usbCC);

  pinMode(PIN_SUSTAIN, INPUT_PULLUP);
}

void loop() {
  MIDI_HW.read();
  usbMIDI.read();

  // Sustain pedal — send on both interfaces
  bool pressed = (digitalRead(PIN_SUSTAIN) == LOW);
  if (pressed != sustainHeld) {
    sustainHeld = pressed;
    uint8_t val = sustainHeld ? 127 : 0;
    MIDI_HW.sendControlChange(64, val, midiChannel);
    usbMIDI.sendControlChange(64, val, midiChannel);
  }
}

// ── Audio helpers ─────────────────────────────────────────
void setPlaybackGain(float g) {
  mixL1.gain(0, g);   // USB playback → DAC 1
  mixR1.gain(0, g);
}

void setMonitorGain(float g) {
  mixL1.gain(1, g);   // ADC 1 direct monitor → DAC 1
  mixR1.gain(1, g);
  mixL2.gain(0, g);   // ADC 2 direct monitor → DAC 2
  mixR2.gain(0, g);
}
