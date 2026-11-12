// boot.cpp — hwBoot(): the exact boot order of PMOR_V2.cpp main() L3883–4318
// (audit §2.12 items 1–14). Order matters — every step below was placed by a
// measured USB-enumeration failure. VERBATIM comments kept.
#include <string.h>
#include "studio.h"
#include "hw.h"
#include "hw_priv.h"
#include "../config.h"
#include "../pmor_core/shared_state.h"
#include "../pmor_core/clock.h"
#include "../pmor_core/sequencer.h"
#include "../pmor_core/scale.h"
#include "../pmor_core/synth.h"
#include "../pmor_core/drums.h"
#include "../pmor_core/sampler.h"
#include "../pmor_core/fx.h"
#include "../pmor_core/output.h"
#include "../pmor_core/metro.h"
#include "../pmor_core/params.h"
#include "../store/patch.h"
#include "daisy_seed.h"
extern "C" {
#include "usb/board_stm32h7.h"
#include "tusb.h"
#include "usb/clock_engine.h"
#include "usb/usb_audio_stream.h"
}
using namespace daisy;

DaisySeed hw;

// Boot timing marks (ms since reset) -- printed once over CDC so a slow boot
// can be attributed instead of guessed at.
volatile uint32_t g_t_usb=0,g_t_oled=0,g_t_sens=0,g_t_lidar=0,g_t_sd=0,g_t_boot=0;

// libDaisy hands the ISR `const float* const*` in-buffers; hw.h's AudioCb takes
// float** for both, so a one-line trampoline keeps the engine header libDaisy-free.
static AudioCb s_cb = nullptr;
static void audioTrampoline(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t n) {
    s_cb((float**)in, (float**)out, n);
}

// CDC "AUDIO 1" re-arms the engine after "AUDIO 0" (hw/cdc_hooks.cpp)
void hwRestartAudio() { if(s_cb) hw.StartAudio(audioTrampoline); }

void hwBoot(AudioCb cb)
{
    // 1 ─────────────────────────────────────────────────────────────────
    hw.Configure(); hw.Init(true);   // 480MHz boost — full tilt
                                     // (ruled OUT as a USB-enumeration cause
                                     //  2026-08-14: non-boost behaved identically)

    // 2 ── HARDWARE DFU ESCAPE HATCH: hold BOTH buttons at power-on ──────
    dfuBootGesture();

    // 3 ── USB bring-up — BEFORE the OLED, with a settle delay before
    // tusb_init() (ported ordering discipline from firmware-tinyusb/: on
    // that project's hardware, initializing the OLED before USB silently
    // killed enumeration; this project's OLED is a different SPI
    // peripheral, but paying the same ordering cost here is free insurance
    // against the same class of bug). usb_audio_stream_init()/
    // clock_engine_init() first (plain state init, no hardware side
    // effects yet) — same order firmware-tinyusb/'s main.cpp uses.
    usb_audio_stream_init();
    clock_engine_init();
    // v3: clock_engine_process_audio() is now wired into the ISR (PO-sync
    // click + sync-in). clock_engine_init() turns its built-in test beat ON
    // by default — that would mix a kick/snare/hat pattern into the
    // instrument's output. Off here; "BEAT ON" over CDC still re-enables it.
    clock_engine_set_beat_enabled(false);

    board_usb_init();
    System::Delay(500);   // USB PHY/VBUS-sense settle, mirrors the source project's own delay
    tusb_init();

    // ── FORCE A BUS-VISIBLE DISCONNECT/RECONNECT ─────────────────────────
    // The Daisy bootloader enumerated as a DFU device moments ago. When it
    // jumps here, the host still believes an already-configured device is
    // sitting at that address, so it never starts a fresh enumeration — our
    // descriptors are never requested and nothing appears on the Mac.
    // This matches the measured evidence exactly: the OTG interrupt fires
    // continuously (~320/s, counter climbed past 1900) so the core is alive
    // and the host IS driving the bus, yet the device never mounts.
    // Asserting soft-disconnect (D+ pull-up off) for 150ms makes the host
    // see a real unplug, drop the stale address, and re-enumerate us cleanly.
    tud_disconnect();
    System::Delay(150);
    tud_connect();
    g_t_usb = System::GetNow();

    // 4 ── Hold the OLED off until the host has FINISHED enumerating ─────
    // Ordering alone was not enough. Measured on hardware 2026-08-14: this
    // exact firmware WITHOUT the OLED enumerates every time (audio + MIDI +
    // CDC all appear); WITH the OLED initialised here, nothing enumerates at
    // all — no audio, no MIDI, no serial — even though the app itself runs.
    // Cause is current, not sequence: the OLED module's onboard 12V boost
    // pushes total draw past what a bus/dock port supplies, and the brownout
    // lands squarely in the enumeration window (PocketPassthru-FLASHING.md
    // §6 documented the same failure for OLED + CDC alone).
    //
    // So: pump tud_task() until tud_mounted(), THEN power the display. The
    // 3s cap is mandatory — standalone on battery there is no host and
    // tud_mounted() never becomes true; the instrument must still boot with
    // a working screen. Do not "simplify" this into a bare wait.
    {
        uint32_t t0 = System::GetNow();
        while(!tud_mounted() && (System::GetNow() - t0) < 3000)
            tud_task();
    }

    // ── HOLD EVERYTHING UNTIL USB HAS ENUMERATED ─────────────────────────
    // Every build that touches the OLED before the host finishes enumerating
    // fails (irq climbing past 60000, tud_mounted() stuck at 0); every build
    // that leaves the display alone enumerates fine. Per-page tud_task()
    // yields inside Update() did NOT fix it (verified compiled in and called
    // via objdump), and battery power did NOT fix it — so this is not
    // task-starvation and not a brownout: enumeration itself is the fragile
    // window, and display activity during it is what breaks it.
    //
    // So: do nothing but pump USB until the host has us mounted. Only then
    // bring the screen up and run the instrument normally. The 6s cap keeps
    // the device fully usable standalone, where no host will ever mount it.
    {
        uint32_t t0 = System::GetNow();
        bool led = false;
        while(!tud_mounted() && (System::GetNow() - t0) < 6000) {
            tud_task();
            if(((System::GetNow() - t0) % 250) < 2) { led = !led; hw.SetLed(led); }
        }
    }

    // 5 ── OLED init + SPI1 IRQ off + dark first frame ────────────────────
    oledInit();

    // 6 ── buses ──────────────────────────────────────────────────────────
    sensorsBusInit();

    // 7 ── IMU reset/wake/ID (no mag probe on the 6050) ───────────────────
    imuInit();

    // 8 ── VL53L1X lidar (0x29) + continuous 100 ms ───────────────────────
    lidarInit();

    // 9 ── SD card — SPI bit-bang, link + mount ───────────────────────────
    sdMount();
    g_t_sd = System::GetNow();

    // 10 ── enc1 + joystick + ADC ─────────────────────────────────────────
    controlsInit();

    // 12 ── zero every SDRAM buffer — SDRAM isn't cleared at reset, so
    // un-cleared buffers read as garbage = loud static the moment an FX
    // engages. In v3 each module's init() owns its own buffers/defaults.
    // Runs BEFORE patchInit() (v2 item 11) so a loaded patch — including the
    // sampler audio sidecar in smp_buf — is not wiped by the inits.
    memset(&g, 0, sizeof(g));
    clockInit(120.f);
    seqInit();
    scaleInit();
    synthInit();
    drumsInit();
    samplerInit();
    fxInit();
    outputInit();
    metroInit();
    pmorSchemaInit();   // param schema defaults = the freshly-initialised engines (before any patch loads)

    // 11 ── restore patches: globals from QSPI, last patch from SD ─────────
    patchInit();
    g_t_boot = System::GetNow();

    // 13 ── audio ─────────────────────────────────────────────────────────
    s_cb = cb;
    hw.SetAudioBlockSize(BLOCK);
    hw.StartAudio(audioTrampoline);

    // 14 ── NeoTrellis init LAST — prevents SDMMC slowness from resetting
    // freshly-inited seesaw
    trellisBootInit();
    g.trellis_ok = trellisOk();

    // (v2 15: the tunnel-dive intro animation is gone — one static frame.)
    oledClear();
    oledRoundRect(20, 18, 107, 45, 5, true, false);
    oledTextBig(36, 26, "PMOR v3", true);
    oledFlush();
    tud_task();
}
