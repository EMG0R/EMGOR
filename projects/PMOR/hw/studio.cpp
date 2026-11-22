// studio.cpp -- see studio.h.
#include "../pmor_core/note_pend.h"
#include "studio.h"
#include <stdio.h>
#include <string.h>
#include "hw.h"
#include "../config.h"
#include "../pmor_core/proto.h"
#include "../pmor_core/fx_midi.h"
#include "../pmor_core/events.h"
#include "../pmor_core/render.h"
#include "../pmor_core/shared_state.h"
#include "../pmor_core/synth.h"
#include "../pmor_core/drums.h"
#include "../pmor_core/sampler.h"
#include "../pmor_core/fx.h"
#include "../pmor_core/output.h"
#include "../pmor_core/clock.h"
#include "../pmor_core/params.h"
#include "../pmor_core/patch_format.h"
#include "../store/patch.h"
#include "../store/files.h"
#include "bootguard.h"
#include "daisy_seed.h"
extern "C" {
#include "tusb.h"
#include "usb/clock_engine.h"
}
using namespace daisy;

#ifndef PMOR_FW_VERSION
#define PMOR_FW_VERSION "dev"
#endif

// ------------------------------------------------------------------ settings (card file PMOR3/STUDIO.CFG, written deferred)
// Not QSPI: a QSPI sector erase blocks the whole main loop for ~250 ms (measured: MIDI clock hiccup). A small card file
// goes through fileWriteAtomic, which keeps USB/MIDI serviced, and is only written ~1 s after the last change.
struct StudioRec { uint32_t magic; uint8_t clockmode; uint8_t pad[3]; };
#define STUDIO_MAGIC 0x53545543u    // 'STUC'
#define STUDIO_CFG "PMOR3/STUDIO.CFG"
static void svcTick(uint32_t now);
static uint8_t  s_clockmode = 0;     // 0 AUTO 1 INTERNAL 2 FOLLOW 3 MASTER
static bool     s_cfg_dirty = false;
static uint32_t s_cfg_at = 0;

// ------------------------------------------------------------------ mute / audition (audio plane reads)
static volatile bool     s_mute = false;
static volatile uint32_t s_audition_until = 0;        // sample time
static bool studioFollow() { return protoActive() && (s_clockmode == 0 || s_clockmode == 2); }
bool studioMuted(uint32_t sample_now) { return s_mute && (int32_t)(sample_now - s_audition_until) >= 0; }

// ------------------------------------------------------------------ ops
static size_t opTx(const uint8_t* d, size_t n) { size_t w = tud_cdc_write(d, (uint32_t)n); tud_cdc_write_flush(); return w; }
extern "C" void pmor_usb_service(void);
static void opService() { pmor_usb_service(); }
static void opTempoSeed(uint32_t m) { clock_engine_tempo_seed((float) m / 1000.0f); }
// NEVER clear the CDC TX FIFO. 0c48eaa called tud_cdc_write_clear() on every session reset (DTR rise/fall, mount edge, HELLO flush); on a fresh
// open that lands while the boot banner's IN transfer is in flight (DWC2 transfers straight out of the FIFO), the FIFO accounting no longer
// matches the transfer, the endpoint never completes and stays busy: every later write (HELLO replies, legacy console) queues forever while RX
// keeps working (hardware: TX-silent 60 s+, 'dfu' still obeyed). Stale bytes already in the FIFO are harmless (<= 1 FIFO of old replies); only
// PMOR's own queue (s_txq) is dropped. tools/fwcheck.sh rule 6 forbids any reference to tud_cdc_write_clear.
static void opTxReset() {}
static size_t opTxSpace() { return tud_cdc_write_available(); }
static bool   opBusIdle() { return true; }
static uint32_t opNow() { return System::GetNow(); }
static uint8_t  opSlot() { return patchCurrent(); }
static uint32_t opEpoch() { return patchApplyCount(); }
static void     opParamChanged() { /* autosave is CRC-scan driven (patchTick) */ }
static bool     opFollowLocked() { return clock_engine_studio(); }
extern volatile uint32_t g_audio_overruns, g_cpu_max_permille;
static uint32_t s_midi_drops = 0;
extern volatile uint32_t g_lp_max_us[12]; extern volatile uint8_t g_lp_skip_pass, g_kp_skip_gap;
extern volatile uint32_t g_svc_calls, g_svc_us_sum, g_svc_us_max, g_kp_polls, g_kp_stretch, g_kp_down;
extern volatile uint32_t g_led_lat_us_max, g_led_lat_us_last;   // hw/trellis.cpp: pad event -> LED shown (priority path)
extern volatile uint32_t g_sns_us[4]; extern volatile uint32_t g_sns_err[3];
extern "C" uint32_t stackFreeMin(void);
extern volatile uint32_t g_ff_why[4];
extern volatile uint32_t g_sd_rd_sectors, g_sd_rd_spi_us, g_sd_rd_svc_us, g_sd_rd_calls, g_sd_rd_sector0, g_ff_opens, g_ff_reads;
uint32_t studioSvcGapMaxMs(void); uint8_t studioSvcGapWhere(void);
static void opStatus(char* b, size_t n) {
    uint32_t fm = 0, fp = 0, fx = 0, fn = clock_engine_ferr_stats(&fm, &fp, &fx);
    uint32_t ri, rc, rp, rn; int32_t lmx, lmn; clock_engine_rx_stats(&ri, &rc, &rp, &rn, &lmx, &lmn);
    uint32_t ivn = 0, ivx = 0; clock_engine_iv_stats(&ivn, &ivx);
    PatchStoreStatus ps; patchStoreStatus(&ps);
    snprintf(b, n, "role=%s tx_stalls=%lu resets=%lu/%lu/%lu/%lu/%lu/%lu/%lu cm=%u/%u sd=%lu/%lu/%lu calls=%lu/%lu ffo=%lu/%lu why=%lu/%lu/%lu/%lu job=%lu/%lu boot=%u/%lu stack_free=%lu store=%u/%u/%u/%u/%u/%u/%u pool=%u ferr_us=%lu/%lu/%lu ferr_n=%lu rx=%lu/%lu/%lu/%lu late_us=%ld/%ld iv_us=%lu/%lu clk=%s running=%d bpm=%d svc_gap_ms=%lu gap_in=%d cpu=%d cpu_max=%d audio_overruns=%lu midi_drops=%lu ev_drops=%lu chunks=%lu lp_us=%lu/%lu/%lu/%lu/%lu/%lu/%lu/%lu sens=%d/%d/%d sns_us=%lu/%lu/%lu",
             clock_engine_get_role() == CLOCK_ROLE_DEMIURGE_MASTER ? "follow" : "master", (unsigned long)protoStatTxStalls(), (unsigned long)protoStatResets(0), (unsigned long)protoStatResets(1), (unsigned long)protoStatResets(2), (unsigned long)protoStatResets(3), (unsigned long)protoStatResets(4), (unsigned long)protoStatResets(5), (unsigned long)protoStatResets(6), (unsigned)s_clockmode, (unsigned)clock_engine_policy(), (unsigned long)g_sd_rd_sectors, (unsigned long)g_sd_rd_spi_us, (unsigned long)g_sd_rd_svc_us, (unsigned long)g_sd_rd_calls, (unsigned long)g_sd_rd_sector0, (unsigned long)g_ff_opens, (unsigned long)g_ff_reads, (unsigned long)g_ff_why[0], (unsigned long)g_ff_why[1], (unsigned long)g_ff_why[2], (unsigned long)g_ff_why[3], (unsigned long)(protoDebugJob() >> 24), (unsigned long)(protoDebugJob() & 0xFFFFFF), (unsigned)bootguardState(), (unsigned long)bootguardBootCount(), (unsigned long)stackFreeMin(), (unsigned)ps.state, (unsigned)ps.pending, (unsigned)ps.exported, (unsigned)ps.written, (unsigned)ps.verified, (unsigned)ps.skipped, (unsigned)ps.errors, (unsigned)ps.samples, (unsigned long)fm, (unsigned long)fp, (unsigned long)fx, (unsigned long)fn, (unsigned long)ri, (unsigned long)rc, (unsigned long)rp, (unsigned long)rn, (long)lmn, (long)lmx, (unsigned long)ivn, (unsigned long)ivx, clock_engine_studio() ? "follow" : clock_engine_clock_mode_name(), clock_engine_is_running() ? 1 : 0,
             (int)(clk.bpm + 0.5f), (unsigned long)studioSvcGapMaxMs(), (int)studioSvcGapWhere(), (int)(g.cpu_load * 100.f), (int)(g_cpu_max_permille / 10), (unsigned long)g_audio_overruns, (unsigned long)s_midi_drops, (unsigned long)pmorEventDrops(), (unsigned long)protoStatBulkChunks(),
             (unsigned long)g_lp_max_us[0], (unsigned long)g_lp_max_us[1], (unsigned long)g_lp_max_us[2], (unsigned long)g_lp_max_us[3], (unsigned long)g_lp_max_us[4], (unsigned long)g_lp_max_us[5], (unsigned long)g_lp_max_us[6], (unsigned long)g_lp_max_us[7], trellisOk() ? 1 : 0, imuOk() ? 1 : 0, lidarOk() ? 1 : 0, (unsigned long)g_sns_us[0], (unsigned long)g_sns_us[1], (unsigned long)g_sns_us[2]);
    { size_t l = strlen(b); if (l < n) snprintf(b + l, n - l, " snserr=%lu/%lu lp2_us=%lu/%lu/%lu/%lu svc_us=%lu/%lu kp=%lu/%lu/%lu led_us=%lu/%lu",
        (unsigned long)g_sns_err[0],(unsigned long)g_sns_err[1],
        (unsigned long)g_lp_max_us[8], (unsigned long)g_lp_max_us[9], (unsigned long)g_lp_max_us[10], (unsigned long)g_lp_max_us[11],
        (unsigned long)(g_svc_calls ? g_svc_us_sum / g_svc_calls : 0), (unsigned long)g_svc_us_max, (unsigned long)g_kp_polls, (unsigned long)g_kp_stretch, (unsigned long)g_kp_down, (unsigned long)g_led_lat_us_max, (unsigned long)g_led_lat_us_last); }
    { uint32_t cm, cv, cb, cs; drumsCycStats(&cm, &cv, &cb, &cs); size_t l = strlen(b); if (l < n) snprintf(b + l, n - l, " dcyc=%lu/%lu/%lu/%lu", (unsigned long)cm, (unsigned long)cv, (unsigned long)cb, (unsigned long)cs); }   // drum voice loop: max cycles per block / cycles per voice-sample / blocks / voice-samples (window = since the previous STATUS)
    g_led_lat_us_max = 0;
    g_svc_calls = 0; g_svc_us_sum = 0; g_svc_us_max = 0; g_kp_polls = 0;
    for (int i = 0; i < 12; i++) g_lp_max_us[i] = 0;
    g_lp_skip_pass = 1; g_kp_skip_gap = 1;
    for (int i = 0; i < 4; i++) g_sns_us[i] = 0;   // lp_us = per-section max since the previous STATUS (windowed)
}
static int      opManCount() { return patchStoreSampleCount(); }
static bool     opManAt(int i, uint16_t* id, uint32_t* len, uint32_t* rate, uint8_t* ch, uint32_t* ver, char* name) {
    if (!patchStoreSampleAt(i, id, len, rate, ch, ver)) return false; snprintf(name, 16, "S%04u", (unsigned)*id); return true;
}
static int      opScan(uint8_t p) { return patchPoolScan(p); }
static uint32_t opLen(uint8_t p, uint8_t s) { return patchPoolLen(p, s); }
static uint32_t opVer(uint8_t p, uint8_t s) { return patchPoolVer(p, s); }
static int32_t  opRead(uint16_t id, uint32_t off, uint8_t* b, uint32_t m) { return patchPoolRead(id, off, b, m); }
static int      opAssign(uint8_t s, uint16_t id) { return patchAssignSample(s, id); }
static int      opDel(uint8_t s) { samplerDelete(s); smp.audio_dirty = true; return 0; }
static int      opClear() { patchClearLive(); return 0; }

static uint32_t s_syn_off_at = 0; static uint8_t s_syn_off_pad = 0xFF;
static void opAudition(char k, uint8_t a, uint8_t vel) {
    s_audition_until = pmorSampleNow() + (uint32_t)(1.8f * SR);
    float v = (float)vel / 127.f;
    if (k == 'D') drumsTrigger(a, v);
    else if (k == 'M') samplerPlay(a, v);
    else { synthNoteOn(a, v); s_syn_off_pad = a; s_syn_off_at = System::GetNow() + 600; }
}
static void opLegacy(const char* line) { clock_engine_handle_cdc_line(line); }

static void applyMode() {
    clock_engine_set_policy(s_clockmode);
    bool studio = protoActive();
    s_mute = studio;
    clock_engine_set_studio(studioFollow());
    if (!studio) pmorSetTransportRun(true);
}
static void opMode(ProtoMode m) { if (m < PM_STUDIO) patchSetNoSave(false); applyMode(); }   // a session ending always re-enables autosave
// Followed transport -> the engine, at the moment the 0xFA/0xFB/0xFC is parsed (not a main-loop pass later: step 0 / the first click sit on the Start tick).
// STUDIO/EDIT gate sequencer + metronome; AUTO-follow outside a session gates the metronome only.
static void opTransportCb(bool run) { if (clock_engine_studio()) pmorSetTransportRun(run); pmorSetMetroGate(run); }
static void opCcIn(uint8_t ch, uint8_t c, uint8_t v) { if (protoActive()) fxMidiIn(System::GetNow(), ch, c, v); }   // FX over MIDI, in: STUDIO/EDIT sessions only
static void opSetClockMode(uint8_t m) { s_clockmode = m & 3; s_cfg_dirty = true; s_cfg_at = System::GetNow() + 1000; applyMode(); }

static const ProtoOps OPS = { opTx, opTxSpace, opBusIdle, opNow, PMOR_FW_VERSION, opSlot, opEpoch, opParamChanged, opFollowLocked, opStatus,
                              opScan, opLen, opVer, opRead, opAssign, opDel, opClear, opAudition, opSetClockMode, opLegacy, opMode, patchFileRead, patchSaveScratch, opManCount, opManAt, patchStoreSampleLen, patchStoreAlias, patchStoreRetry, opService, clock_engine_join_begin, clock_engine_join_poll, patchSetNoSave, opTxReset, opTempoSeed, patchSavePresetAt, patchRefsScan, patchBinDelete, patchBinCount, patchBinAt, patchBinToken, patchBinRestore, patchBinEmpty, patchArchiveScan, patchArchiveBegin, patchArchiveList, patchArchiveState };

// ------------------------------------------------------------------ MIDI out (studio/edit only)
static NotePend s_pend;
static HeldNotes s_held;
static uint16_t s_last_bend = 8192;

static void midiRaw(uint8_t cin, uint8_t b0, uint8_t b1, uint8_t b2) {
    uint8_t p[4] = { cin, b0, b1, b2 };
    if (!tud_midi_packet_write(p)) s_midi_drops++;
}
static void noteOn(uint8_t ch, uint8_t n, uint8_t v) { midiRaw(0x09, (uint8_t)(0x90 | ch), n, v); }
static void noteOff(uint8_t ch, uint8_t n) { midiRaw(0x08, (uint8_t)(0x80 | ch), n, 0); }
static void cc(uint8_t ch, uint8_t c, uint8_t v) { midiRaw(0x0B, (uint8_t)(0xB0 | ch), c, v); }
static void bend(uint8_t ch, uint16_t v) { midiRaw(0x0E, (uint8_t)(0xE0 | ch), (uint8_t)(v & 0x7F), (uint8_t)(v >> 7)); }
static uint8_t s_cc_last[8]; static uint32_t s_cc_t = 0; 

static void midiOutPoll(uint32_t now, bool active) {
    PmorEvent e;
    while (pmorEventPop(&e)) {
        if (!active) continue;
        switch (e.kind) {
            case EV_DRUM_ON: s_pend.hit(now + 30, 0, 36 + e.a, e.b, noteOn, noteOff); break;
            case EV_SMP_ON:  s_pend.hit(now + 100, 2, 36 + e.a, e.b, noteOn, noteOff); break;
            case EV_SYN_ON:
                if (e.c != s_last_bend) { bend(1, e.c); s_last_bend = e.c; }
                if (s_held.on[e.a & 127]) noteOff(1, e.a);
                noteOn(1, e.a, e.b); s_held.on[e.a & 127] = true; break;
            case EV_SYN_OFF: if (s_held.on[e.a & 127]) { noteOff(1, e.a); s_held.on[e.a & 127] = false; } break;
            case EV_SEED:
                cc(3, 110, (uint8_t)((e.seed >> 21) & 127)); cc(3, 111, (uint8_t)((e.seed >> 14) & 127));
                cc(3, 112, (uint8_t)((e.seed >> 7) & 127));  cc(3, 113, (uint8_t)(e.seed & 127)); break;
        }
    }
    static bool s_was_active = false;   // (session end mid-note SENDS the pending + held offs, then resets) clear/reset only on the active -> inactive edge: this runs once per I2C byte, and fxMidiReset writes ~180 SDRAM words
    if (!active) { if (s_was_active) { s_pend.flush(noteOff); s_held.flush(1, noteOff); fxMidiReset(); s_was_active = false; } return; }
    s_was_active = true;
    { static uint32_t s_fx_t = 0; if (now - s_fx_t >= 5) { s_fx_t = now; fxMidiPoll(now, cc); } }   // FX engage (ch5) + amount/params (ch6): changes only, <= 200/s. Every 5 ms, NOT every service call: pmor_usb_service runs per I2C byte, and a full poll (~23 param lookups + all banks) per byte made every sensor/trellis transaction ~10x slower (laggy pads in a session)
    s_pend.service(now, noteOff);
    if (now - s_cc_t >= 10) {      // sensors/fader on ch4 (<=100 Hz), FX engage on ch5
        s_cc_t = now;
        auto q = [](float v) { int x = (int)(v * 127.f + 0.5f); return (uint8_t)(x < 0 ? 0 : x > 127 ? 127 : x); };
        auto qb = [&](float v) { return q(0.5f + 0.5f * v); };
        uint8_t cur[8] = { q(g.fsr), q(g.lidar), qb(g.accel_x), qb(g.accel_y), qb(g.joy_x), qb(g.joy_y), q(outp.fader * 0.75f), 0 };
        for (int i = 0; i < 7; i++) {      // deadband 2 (ADC wobble), exact at the ends
            int d = (int)cur[i] - (int)s_cc_last[i];
            if (d >= 2 || d <= -2 || (d != 0 && (cur[i] == 0 || cur[i] == 127))) { s_cc_last[i] = cur[i]; cc(3, (uint8_t)(1 + i), cur[i]); }
        }
    }
}

// ------------------------------------------------------------------ transport follower gating
static bool s_was_running = false;
static uint32_t s_start_seen = 0;

void studioInit(void) {
    StudioRec r{}; 
    if (sdOk() && fileRead(STUDIO_CFG, &r, sizeof r) == (int)sizeof r && r.magic == STUDIO_MAGIC) s_clockmode = r.clockmode & 3;
    g_audio_overruns = 0; g_cpu_max_permille = 0;   // drop the boot-time outlier
    clock_engine_set_policy(s_clockmode);   // persisted choice (AUTO=0 unless the user picked one)
    protoInit(&OPS);
    clock_engine_set_transport_cb(opTransportCb);
    clock_engine_set_cc_cb(opCcIn);
}
void studioCdcRx(const uint8_t* d, uint32_t n) { protoRx(d, n); }

void studioPoll(uint32_t now) {
    svcTick(now);
    if (s_cfg_dirty && (int32_t)(now - s_cfg_at) >= 0) {       // deferred settings write (card, USB kept serviced)
        s_cfg_dirty = false;
        StudioRec r{ STUDIO_MAGIC, s_clockmode, {0, 0, 0} };
        if (sdOk()) fileWriteAtomic(STUDIO_CFG, &r, sizeof r);
    }
    static bool s_usb_up = false;
    bool up = tud_mounted() && !tud_suspended();
    if (up != s_usb_up) { s_usb_up = up; protoUsbReset(); }       // mount / unmount / suspend / resume: a clean session every time
    bool conn = tud_cdc_connected() && up;
    protoConnected(conn);
    if (conn) {
        protoPoll();
        tud_cdc_write_flush();
    }
    uint8_t slot; int err;
    if (patchAssignPoll(&slot, &err)) protoSampleDone(slot, err);
    if (s_syn_off_pad != 0xFF && (int32_t)(now - s_syn_off_at) >= 0) { synthNoteOff(s_syn_off_pad); s_syn_off_pad = 0xFF; }

    bool follow = clock_engine_studio();
    if (follow) {
        bool run = clock_engine_is_running();
        uint32_t sc = clock_engine_start_count(); bool started = sc != s_start_seen; s_start_seen = sc;
        if ((run && !s_was_running) || started) pmorReseed((now * 2654435761u >> 4) | 1u);     // transport Start: reseed + publish (ch4 CC110-113)
        if (!run && s_was_running) synthAllOff();
        s_was_running = run;
        pmorSetTransportRun(run);
    } else { s_start_seen = clock_engine_start_count(); if (s_was_running) { s_was_running = false; pmorSetTransportRun(true); } }

    midiOutPoll(now, protoActive());
}

uint8_t studioClockMode(void) { return s_clockmode; }
void studioSetClockMode(uint8_t m) { opSetClockMode(m); }
const char* studioClockModeName(uint8_t m) { static const char* const n[4] = { "AUTO", "INTERNAL", "FOLLOW", "MASTER" }; return n[m & 3]; }

// Light service called from pmor_usb_service() (every main-loop pass AND between OLED page blits): drain the sound
// events to USB-MIDI within ~0.4 ms instead of waiting for a whole loop pass (which can be 10+ ms during an OLED frame).
extern volatile uint8_t g_lp_where;
static uint32_t s_svc_last = 0, s_svc_gap_max = 0; static uint8_t s_svc_gap_where = 99;
static void svcTick(uint32_t now) {
    if (s_svc_last) { uint32_t gap = now - s_svc_last; if (gap > s_svc_gap_max && gap < 100000u) { s_svc_gap_max = gap; s_svc_gap_where = g_lp_where; } }
    s_svc_last = now;
}
void studioMidiService(uint32_t now) { svcTick(now); midiOutPoll(now, protoActive()); }
uint32_t studioSvcGapMaxMs(void) { uint32_t m = s_svc_gap_max; s_svc_gap_max = 0; return m; }
uint8_t studioSvcGapWhere(void) { return s_svc_gap_where; }   // longest USB-service gap since last read
