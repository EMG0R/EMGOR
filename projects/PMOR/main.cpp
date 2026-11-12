// main.cpp — PMOR v3: boot (hw/boot.cpp) + the control-plane scheduler.
// Loop head (USB / CDC / DFU / BPM handover) is PMOR_V2.cpp L4329–4374.
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "hw/hw.h"
#include "hw/hw_priv.h"
#include "pmor_core/shared_state.h"
#include "pmor_core/clock.h"
#include "pmor_core/sequencer.h"
#include "pmor_core/sampler.h"
#include "hw/transport_link.h"
#include "ui/input.h"
#include "ui/nav.h"
#include "hw/studio.h"
#include "hw/bootguard.h"
#include "ui/render.h"
#include "store/patch.h"
#include "daisy_seed.h"
extern "C" {
#include "tusb.h"
#include "usb/clock_engine.h"
#include "usb/usb_audio_stream.h"
}
using namespace daisy;

// Stack high-water: the main stack lives at the top of DTCM (_estack = 0x20020000) and grows down toward _ebss.
// Paint everything between them before anything deep runs; stackFreeMin() = untouched bytes left (reported in STATUS).
extern "C" uint32_t _edtcmram_bss;   // end of DTCM statics (incl. the persistent boot flag); the stack grows down toward it
static void stackPaint() {
    uint32_t* lo = (uint32_t*)&_edtcmram_bss; uint32_t* sp; __asm volatile("mov %0, sp" : "=r"(sp));
    for (uint32_t* p = lo; p < sp - 32; p++) *p = 0xA5A5A5A5u;
}
extern "C" uint32_t stackFreeMin(void) {
    uint32_t* p = (uint32_t*)&_edtcmram_bss; uint32_t n = 0;
    while (p < (uint32_t*)0x20020000 && *p == 0xA5A5A5A5u) { p++; n += 4; }
    return n;
}

// Per-section worst-case time (us) of the control loop, read by studio STATUS ("lp=" field): which section stalls USB service.
volatile uint32_t g_lp_max_us[12];   // 0-7 sections, 8 whole pass, 9 pass head (before the keypad poll), 10 pad event -> nav handled, 11 gap between keypad poll starts
volatile uint8_t g_lp_skip_pass = 0;   // STATUS resets the windows mid-pass; that pass (host request handling) is not loop work
volatile uint8_t  g_lp_where = 99;   // loop section currently running (99 = outside any), read by the USB-service gap tracker
static inline void lpMark(int i, uint32_t t0) { uint32_t d = (uint32_t)(((uint64_t)(System::GetTick() - t0) * 1000000ull) / System::GetTickFreq()); if (d > g_lp_max_us[i]) g_lp_max_us[i] = d; }
static inline uint32_t lpUs(uint32_t t0) { return (uint32_t)(((uint64_t)(System::GetTick() - t0) * 1000000ull) / System::GetTickFreq()); }
static inline void lpMax(int i, uint32_t d) { if (d > g_lp_max_us[i]) g_lp_max_us[i] = d; }
#define LP(i, stmt) do { g_lp_where = (i); uint32_t _t = System::GetTick(); stmt; lpMark(i, _t); g_lp_where = 99; } while(0)

void AudioCallback(float** in, float** out, size_t n);   // hw/audio_callback.cpp

// ── TASK 1: "would the grid currently produce audio" test ─────────────────
// Mirrors engine/sequencer.cpp's seqFire() mode logic (read-only -- no PRNG
// draws, no side effects, this just asks "could this lane possibly fire",
// not "does it fire on this exact step") so clock_engine's Start/Stop
// transport reflects reality instead of free-running from boot forever:
//   - SEQ_MANUAL never auto-fires (seqFire() returns 0 unconditionally for
//     it, sequencer.cpp:241) -- contributes nothing.
//   - SEQ_PLAY fires from programmed step bits OR from pads currently held
//     for that engine (live play) -- either makes it sounding.
//   - SEQ_GEN can fire from density/probability alone even with an "empty"
//     step[] (sequencer.cpp:257's `if (l.probability <= 0.f) return
//     playMask`), so any nonzero probability*density counts as sounding;
//     otherwise it falls back to the same step-bit scan as PLAY.
static bool laneWouldSound(const LaneSet& l, uint16_t held) {
    int mode = (int)(l.mode_f + 0.5f);
    if(mode < 0) mode = 0;
    else if(mode >= SEQ_MODE_COUNT) mode = SEQ_MODE_COUNT - 1;
    if(mode == SEQ_MANUAL) return false;
    if(mode == SEQ_GEN && l.probability > 0.f && l.density > 0.f) return true;
    if(mode == SEQ_PLAY && held) return true; // live pads held = definitely sounding
    uint32_t loopLen = (uint32_t)l.bars * l.steps_per_bar;
    if(!loopLen) loopLen = 1;
    uint32_t n = loopLen < MAXSTEPS ? loopLen : MAXSTEPS;
    for(uint32_t i = 0; i < n; i++) if(l.step[i]) return true;
    return false;
}

// NOTE: this can't see whether a sampler voice is still physically ringing
// out (SmpVoice/the active-voice list is private to engine/sampler.cpp,
// not exposed via sampler.h) -- smp.rec_slot (actively recording, audible
// via monitoring) is the only sampler liveness this file can observe. A
// long one-shot sample's tail after its trigger step won't by itself keep
// `sounding` true. Closed 2026-10: sampler.h now exposes
// samplerActiveVoices(), so a one-shot still ringing (V_FADEOUT included)
// keeps transport running instead of announcing Stop over audible audio.
static bool computeSounding() {
    if(smp.rec_slot >= 0) return true;
    if(samplerActiveVoices() > 0) return true;   // tails count as sounding
    uint16_t held = g.pads_held;
    uint16_t synHeld = g.syn_chord ? g.syn_chord : (g.pad_engine == ENG_SYNTH ? held : 0);
    for(int e = 0; e < ENG_COUNT; e++) {
        uint16_t engHeld = (e == ENG_SYNTH) ? synHeld : (g.pad_engine == e ? held : 0);
        if(laneWouldSound(seq.lane[e], engHeld)) return true;
    }
    return false;
}

// trellis key events → input ring + the pads_held mirror the audio plane reads
// A pad's press instant, for loop tap-in. The NeoTrellis (seesaw) FIFO has no timestamps. It is polled every 2 ms (hw/trellis.cpp), and a press
// is seen by the count read ~0.8 ms into a poll and delivered ~1 ms later, after the FIFO reply: the press happened on average
// 1.0 (half the poll period) + ~1.1 (request/reply chain still in flight at the count read) ~= 2.2 ms before this callback.
// Stamped HERE (at the poll) rather than where the UI event is finally processed, which can be several more ms later.
#define PAD_POLL_COMP_SAMPLES ((uint32_t)(0.0022f * SR))
static void onPad(uint8_t pad, bool pressed) {
    if(pressed) {
        g.pad_press_sample[pad & 15] = clockSampleNow() - PAD_POLL_COMP_SAMPLES;
        g.pads_held |=  (uint16_t)(1u << pad);
    }
    else        g.pads_held &= (uint16_t)~(1u << pad);
    // pad events are derived from g.pads_held inside inputPoll() (single source, no duplicates)
}

// Boot-time marks — printed once, the first time a CDC terminal is attached.
static void printBootMarksOnce() {
    static bool done = false;
    if(done || !tud_cdc_connected()) return;
    done = true;
    char b[128];
    int n = snprintf(b, sizeof b,
        "PMOR v3 boot ms: usb=%lu oled=%lu sens=%lu sd=%lu patch=%lu trellis=%d imu=%d lidar=%d sdok=%d\r\n",
        (unsigned long)g_t_usb, (unsigned long)g_t_oled, (unsigned long)g_t_sens,
        (unsigned long)g_t_sd, (unsigned long)g_t_boot,
        trellisOk()?1:0, imuOk()?1:0, lidarOk()?1:0, sdOk()?1:0);
    if(n > 0) { tud_cdc_write(b, (uint32_t)n); tud_cdc_write_flush(); }
}

// Keypad poll + (when a pad changed) input + nav handling. Cheap when nothing is due (two comparisons), so it is called at the head of the
// pass AND between the sections below: a pad press is turned into a voice trigger / UI event within one section piece (<= ~1 ms) of its
// FIFO reply being readable instead of waiting for the whole pass.
static void padService(uint32_t now) {
    static uint16_t seen = 0;
    uint32_t t0 = System::GetTick();
    trellisPollKeys(now, onPad);
    g.trellis_ok = trellisOk();
    if(g.pads_held != seen) {
        seen = g.pads_held;
        inputPoll(now, encoderDelta(), encoderPressed(), joyPressed(), g.joy_x, g.joy_y, g.pads_held);
        InputEvent ev;
        while(inputPop(ev)) navHandle(ev, now);
        lpMax(10, lpUs(t0));
    }
}

int main(void)
{
    bootguardStart();   // watchdog + persistent boot flag first: a hang below resets instead of bricking
    stackPaint();
    hwBoot(AudioCallback);
    oledIntro();
    navInit();
    studioInit();
    bootguardBootDone();   // boot finished inside the kick budget (hw/bootguard.cpp)

    uint32_t next_oled = 0;
    uint8_t  grb[48];

    while(1)
    {
        bootguardKick();
        uint32_t now = System::GetNow();
        uint32_t _pass0 = System::GetTick();
        static uint32_t s_prev_pass0 = 0;
        if (s_prev_pass0 && !g_lp_skip_pass) lpMax(8, lpUs(s_prev_pass0) );   // pass-to-pass period (whole pass incl. its own USB service)
        s_prev_pass0 = _pass0;
        bool _skip = g_lp_skip_pass != 0; g_lp_skip_pass = 0;

        // ── USB tasks + MIDI-clock/role engine — control plane only, every
        // iteration, unconditionally (tud_task() must never be rate-limited).
        // The audio-plane clock (engine/clock.cpp) remains the ONE tempo
        // authority; this is the sole place clk.bpm and clock_engine's g_bpm
        // are reconciled — see usb/clock_engine.h "Host-instrument tempo
        // integration".
        tud_task();
        usb_audio_stream_task();   // drain the audio ISR's ring into TinyUSB
                                   // (the ONLY writer of the UAC2 FIFO now)
        PollCdc();        // MUST run before anything that can stall, or CDC
                          // commands (including `dfu`) are never parsed
                          // while the loop is busy — exactly when we need them.
        // Buttonless DFU — checked at the head of the loop so the device can
        // always be reflashed over CDC.
        dfuCheck();
        printBootMarksOnce();

        // TASK 1: tell clock_engine whether the grid would currently
        // produce any audio at all -- it debounces this into real
        // Start/Stop transport (see usb/clock_engine.h). Unconditional,
        // every pass, regardless of role (clock_engine only acts on it
        // while OPG_MASTER, same as clock_engine_set_local_bpm() below).
        {   // laneWouldSound() scans every step of every lane (hundreds of SDRAM words): ~0.3 ms. clock_engine debounces this into Start/Stop over
            // hundreds of ms, so 8 ms is plenty; the main loop now passes every ~0.5 ms and this was a fixed tax on each pass (and on pad polls).
            static uint32_t s_snd_t = 0; static bool s_snd = false;
            if((uint32_t)(now - s_snd_t) >= 8u) { s_snd_t = now; s_snd = computeSounding(); }
            clock_engine_set_sounding(s_snd);
        }
        seqSetGridFollowed(clock_engine_studio() || clock_engine_clock_mode() == PMOR_CLKMODE_SLAVE);   // first-hit recording: a followed grid cannot be restarted (sequencer.h)

        // CLOCK LINK (2026-10-05): tap routing and tempo authority live in engine/transport_link.cpp.
        // A tap is handled where it happens (nav -> transportTap(), ISR-stamped); here, every pass, we
        // only (a) hand the local tempo to clock_engine while we drive the bus, (b) feed the RECEIVED
        // bus tempo back into the grid while SLAVED, and (c) publish what the screen should show.
        // Still exactly one clock: clock_engine never runs a second tempo of its own.
        transportTick();
        clock_engine_poll();
        studioPoll(now);          // PMOR STUDIO: protocol pump, MIDI notes/CCs, follower gating (after the clock, so clock goes first)
        PollCdc();
        // Buttonless DFU (MUST PRESERVE — enclosed device, no physical
        // access to BOOT/RESET without disassembly). Checked every loop
        // iteration, same as the source project's own main().
        dfuCheck();

        // ── pads (2 ms FIFO poll state machine) → input events ────────────
        if (!_skip && !g_lp_skip_pass) lpMax(9, lpUs(_pass0));
        uint32_t _pad0 = System::GetTick(); uint16_t _held0 = g.pads_held;
        LP(0, trellisPollKeys(now, onPad));
        g.trellis_ok = trellisOk();

        // ── encoder / joystick / pad-hold → input events → nav ────────────
        inputPoll(now, encoderDelta(), encoderPressed(), joyPressed(), g.joy_x, g.joy_y, g.pads_held);
        InputEvent ev;
        while(inputPop(ev)) navHandle(ev, now);
        if (g.pads_held != _held0) lpMax(10, lpUs(_pad0));   // keypad reply that delivered a pad change -> voice trigger / UI event handled
        LP(7, navTick(now));

        // ── sensors (IMU 50 ms + watchdog, lidar, FSR median) → g ─────────
        LP(1, sensorsTick(now));
        padService(now);

        // ── patch autosave debounce + chunked audio save ──────────────────
        LP(2, patchTick(now));
        padService(now);

        // ── OLED @ ~30 fps ────────────────────────────────────────────────
        // The frame is drawn in one go (0.3 ms) but PUSHED one ~0.35 ms piece per pass (hw/oled_pmor.h), so no OLED work ever blocks
        // the keypad poll for more than a piece. A new frame waits for the previous push to finish.
        if(now >= next_oled && !oledFlushPending()) {
            next_oled = now + 33;
            LP(3, oledRenderFrame(now));
            LP(4, oledFlushBegin());
        }
        if(oledFlushPending()) LP(4, oledFlushStep());
        padService(now);

        // ── NeoTrellis: render → frame → recovery/watchdog + 80 ms push ───
        LP(5, trellisRenderFrame(now, grb));
        trellisSetFrame(grb);
        LP(6, trellisTick(now));
        padService(now);
    }
}
