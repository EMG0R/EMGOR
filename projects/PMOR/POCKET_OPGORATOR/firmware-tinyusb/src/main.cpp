// main.cpp — Pocket OpGorator TinyUSB composite firmware entry point.
//
// Three build variants live in this one file, selected by the Makefile
// (see README.md "USB bring-up isolation ladder" for the full rationale):
//
//   POCKET_OPGORATOR_ISOLATION_MIDI        `make midi-only`
//     USB-MIDI ONLY. No audio, no CDC, no OLED (this project never had
//     one). Answers exactly one question with zero confounds: does
//     USB-MIDI enumerate on this hardware at all? Prior art
//     (main_midi_trigger.cpp.bak in the PocketPassthru project) tried
//     MIDI once before, on a different stack (libDaisy's own
//     MidiUsbHandler), and it never enumerated on that host -- this is
//     the first attempt on TinyUSB, and this build exists so a failure
//     here is unambiguous instead of buried under audio+CDC.
//
//   POCKET_OPGORATOR_ISOLATION_MIDI_AUDIO  `make midi-audio`
//     USB-MIDI + UAC2 capture audio, still no CDC. Bisection point
//     between "MIDI alone works" and "the full composite works" -- if
//     the full build misbehaves, flash this one to find out whether
//     audio-on-top-of-MIDI is the breaking addition or whether CDC is.
//
//   (neither defined, the default)         `make`
//     Full composite: UAC2 capture-only + USB-MIDI + CDC-ACM, wired into
//     libDaisy's audio callback + the clock/PO-sync engine
//     (clock_engine.c) + the buttonless-DFU CDC trigger ported from
//     PocketPassthru (see "buttonless DFU" below).
//
// All three keep the same ordering discipline PocketPassthru's WORKING
// build established (its .cpp comment: "USB is initialized FIRST...
// BEFORE the OLED SPI and audio. Initializing the OLED before USB
// prevented enumeration.") and its documented OLED+USB co-enumeration
// brownout finding (PocketPassthru-FLASHING.md §6: OLED's 12V boost +
// audio + USB exceeds a bus/dock port's current budget). This project
// has no OLED, but adds a THIRD USB interface class (UAC2 audio) on top
// of what PocketPassthru proved works (CDC + audio), which is new
// current-draw/enumeration-complexity territory -- hence the isolation
// ladder above instead of trusting the full composite blind. USB is
// brought up first in every variant, before hw.StartAudio(), with a
// settle delay after board_usb_init() before tusb_init() -- mirroring
// PocketPassthru's own `System::Delay(500)` between USB init and
// arming its receive callback, on the theory that whatever settling
// that delay was covering (the FS PHY / VBUS sense, most likely) is a
// property of the hardware, not of which USB stack sits on top of it.
#include "daisy_seed.h"

extern "C" {
#include "board_stm32h7.h"
#include "tusb.h"
#include "clock_engine.h"
}

#if CFG_TUD_AUDIO
extern "C" {
#include "usb_audio_stream.h"
}
#endif

using namespace daisy;

static DaisySeed hw;

// USB PHY settle delay after board_usb_init(), before tusb_init(). See the
// file header block above for why this mirrors PocketPassthru's
// System::Delay(500) between hw.usb_handle.Init() and arming its RX
// callback -- unverified on THIS stack, but cheap insurance and easy to
// remove later if hardware bring-up shows it's unnecessary.
static constexpr uint32_t kUsbSettleDelayMs = 500;

#if defined(POCKET_OPGORATOR_ISOLATION_MIDI)
//====================================================================
// ISOLATION BUILD: USB-MIDI ONLY
//====================================================================
// No audio callback at all -- StartAudio() is never called. Nothing here
// but proving the MIDI interface enumerates and can move bytes. LED blink
// is the same proof-of-life signal PocketPassthru used (its onboard-LED
// blink "past OLED + audio init"); here it just means "past USB init and
// still alive in the main loop".
int main(void) {
  hw.Configure();
  hw.Init();

  board_usb_init();
  System::Delay(kUsbSettleDelayMs);
  tusb_init();

  bool led = false;
  uint32_t last_blink = System::GetNow();
  for (;;) {
    tud_task();

    // Minimal MIDI loopback proof-of-life: echo any received byte back
    // out, so a host-side MIDI monitor can confirm both RX and TX paths
    // without needing the full clock engine. Deliberately NOT using
    // clock_engine.c here -- keeping this build as small and as close to
    // "just the TinyUSB MIDI class driver" as possible is the point.
    uint8_t b;
    while (tud_midi_stream_read(&b, 1) == 1) {
      tud_midi_stream_write(0, &b, 1);
    }

    uint32_t now = System::GetNow();
    if (now - last_blink >= 250) {
      led = !led;
      hw.SetLed(led);
      last_blink = now;
    }
  }
}

#elif defined(POCKET_OPGORATOR_ISOLATION_MIDI_AUDIO)
//====================================================================
// ISOLATION BUILD: USB-MIDI + UAC2 CAPTURE AUDIO, NO CDC
//====================================================================
static void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
  usb_audio_stream_push_block(in, /*n_channels=*/2, size);
  // Raw passthrough -- deliberately not routed through clock_engine's
  // PO-sync/SY-mode logic here, so a misbehavior in THIS build isolates
  // to "MIDI + audio" rather than also implicating the clock engine.
  for (size_t ch = 0; ch < 2; ch++) {
    for (size_t i = 0; i < size; i++) {
      out[ch][i] = in[ch][i];
    }
  }
}

int main(void) {
  hw.Configure();
  hw.Init();
  hw.SetAudioBlockSize(48);
  hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

  usb_audio_stream_init();

  board_usb_init();
  System::Delay(kUsbSettleDelayMs);
  tusb_init();

  hw.StartAudio(AudioCallback);

  bool led = false;
  uint32_t last_blink = System::GetNow();
  for (;;) {
    tud_task();

    uint8_t b;
    while (tud_midi_stream_read(&b, 1) == 1) {
      tud_midi_stream_write(0, &b, 1);
    }

    uint32_t now = System::GetNow();
    if (now - last_blink >= 250) {
      led = !led;
      hw.SetLed(led);
      last_blink = now;
    }
  }
}

#else
//====================================================================
// FULL COMPOSITE: UAC2 capture-only + USB-MIDI + CDC-ACM + clock/PO-sync
//====================================================================
//--------------------------------------------------------------------+
// Audio callback: Daisy ADC -> USB UAC2 capture FIFO, and (separately)
// -> clock_engine's SY-mode routing/PO-sync render -> Daisy DAC.
//--------------------------------------------------------------------+
// AUDIO_BLOCK_SAMPLES (48, see audio_format.h) is set as the Daisy audio
// block size below specifically so one callback == one nominal 1ms USB
// full-speed frame's worth of samples. That 1:1 relationship is what makes
// "occasionally send 47 or 49 instead of 48" (TinyUSB's own EP_IN flow
// control, driven off FIFO fill level) sufficient to track drift, without
// this callback itself having to know or care about USB framing.
//
// USB capture gets the RAW ADC input, unmodified by whatever SY mode is
// active -- clock_engine_process_audio() only affects what reaches the
// Daisy's own analog outputs (headphone/speaker + PO sync jacks). This is
// a deliberate judgment call (not specified anywhere): a host recording
// over USB should hear the true input, not a sync-mode-mangled monitor mix
// (e.g. SY3/SY4 rendering mono-only to the local outputs). See README.
// The ONE exception (2026-08-12, default test beat): the beat itself IS
// mixed into the USB capture too (see clock_engine_mix_beat_into_capture()
// -- not gated by SY-mode click-channel protection, since USB capture has
// no analog sync-click channel to protect), so the built-in beat can be
// verified by recording over USB, not just by ear at the analog outputs.
static void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
  clock_engine_note_audio_block(size); // diag instrumentation -- see clock_engine.h
  // clock_engine_process_audio() must run BEFORE the capture mix below --
  // it's what renders this block's beat samples that
  // clock_engine_mix_beat_into_capture() then reads.
  clock_engine_process_audio(in, out, size);

  static float cap_l[AUDIO_BLOCK_SAMPLES];
  static float cap_r[AUDIO_BLOCK_SAMPLES];
  clock_engine_mix_beat_into_capture(in[0], in[1], cap_l, cap_r, size);
  float *capture[2] = { cap_l, cap_r };
  usb_audio_stream_push_block(capture, /*n_channels=*/2, size);
}

//--------------------------------------------------------------------+
// CDC console: line-buffered, feeds complete lines to clock_engine's
// command handler (SY mode / click wave / status / buttonless DFU).
//--------------------------------------------------------------------+
static void PollCdc(void) {
  if (!tud_cdc_connected() || !tud_cdc_available()) return;

  static char line[64];
  static uint8_t len = 0;

  uint8_t buf[64];
  uint32_t n = tud_cdc_read(buf, sizeof(buf));
  for (uint32_t i = 0; i < n; i++) {
    char c = (char) buf[i];
    if (c == '\r') continue;
    if (c == '\n') {
      line[len] = '\0';
      clock_engine_handle_cdc_line(line);
      len = 0;
      continue;
    }
    if (len < sizeof(line) - 1) line[len++] = c;
    // else: silently drop overlong-line overflow rather than corrupt
    // adjacent memory -- a malformed/too-long line just never completes.
  }
}

int main(void) {
  hw.Init();

  // 48 samples/block @ 48kHz == exactly 1ms == one nominal USB FS frame.
  // See audio_format.h AUDIO_BLOCK_SAMPLES / AudioCallback comment above.
  hw.SetAudioBlockSize(48);
  hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

  usb_audio_stream_init();
  clock_engine_init();

  board_usb_init();
  System::Delay(kUsbSettleDelayMs);
  tusb_init();

  // (No tud_sof_cb_enable() here -- the SOF-based instrumentation/
  // calibration this project tried was deleted 2026-08-12, third
  // debugging round: sof_count read 0 for the device's entire uptime
  // across two hardware rounds including a 31-minute run, and SOF has no
  // bearing on the actual bug (USB-MIDI transmit duplication, not a rate
  // problem) -- see clock_engine.h's retraction comment.)
  hw.StartAudio(AudioCallback);

  for (;;) {
    tud_task();
    clock_engine_poll();
    PollCdc();

    // Buttonless DFU (MUST PRESERVE -- ported from PocketPassthru, see
    // PocketPassthru-FLASHING.md and clock_engine.c's dfu-trigger
    // comment). Checked every loop iteration, same as PocketPassthru's
    // own `if (g_dfu_request)` check in its main while(1).
    if (clock_engine_dfu_requested()) {
      System::Delay(50);
      System::ResetToBootloader(System::BootloaderMode::STM);
    }
  }
}
#endif
