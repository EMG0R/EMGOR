// demiurge-transport.h -- the ONE tap-tempo analyzer + the ONE tempo wire format.
//
// THERE IS ONE COPY OF THIS FILE (DEMIURGE_OS invariant 4: one mechanism per job).
// Two programs compile it:
//   * demiurge-clock (Pi, src/demiurge-clock.cpp)  -- #include "demiurge-transport.h"
//   * the PMOR firmware (Pocket-OpGorator/DaisyExamples/seed/PMOR_V3)  -- its Makefile adds
//       -I$(DEMIURGE_SRC)   (DEMIURGE_SRC ?= ../../../../DEMIURGE_OS/src, override on the
//       command line if the trees ever live apart)
// so standalone PMOR (nothing plugged in) and the Pi transport run literally the same
// tap code -- same feel, one implementation, not two that must "stay identical".
//
// Header-only, freestanding C99 (also valid C++): no libc beyond stdint, no
// allocation, no I/O. Safe on a Cortex-M7 and on the Pi.
//
// WHAT LIVES HERE
//   1. Tempo wire grid  -- ch16 CC120 (MSB) + CC121 (LSB), 14-bit,
//        w = round((bpm - 40) * 16383 / 360)      40.00 .. 400.00 BPM, 0.022 BPM/step.
//      The WIRE VALUE IS CANONICAL (docs/clock.md "Tempo is a number"): whoever
//      originates a tempo quantises to this grid FIRST and adopts the quantised
//      value, so every device holds the identical number. dt_bpm_canon() is that step.
//   2. Tap analyzer     -- timestamped taps in, tempo out. Timestamps are microseconds
//      on ANY monotonic clock the caller owns (PMOR: audio sample counter converted to
//      us; Pi: CLOCK_MONOTONIC minus the device-reported tap age). Only differences
//      matter. The analyzer never reads a clock itself.
//
// TAP ANALYZER RULES (every one is measured by testing/transport/test_transport.c)
//   * A tap closer than DT_TAP_MIN_GAP_US to the previous one is a bounce / a duplicated
//     USB byte, not a tap: ignored, window untouched. (400 BPM at 1 tap per beat is
//     150 ms, so 120 ms never rejects a real tap.)
//   * A gap longer than DT_TAP_EXPIRE_US starts a fresh window.
//   * Tempo = least-squares slope of tap time vs tap index over the window (up to
//     DT_TAP_MAX taps). The old "mean of consecutive intervals" is algebraically
//     (t_last - t_first)/n -- only the two END taps matter, so one sloppy end tap moves
//     the tempo by its full error. A fit uses every tap.
//   * A new interval more than DT_TAP_OUTLIER (40 %) away from the running estimate is
//     a deliberate tempo change (or a missed beat): the window restarts from the last
//     two taps instead of dragging the estimate through the middle.
//   * Result is clamped to DT_TAP_BPM_MIN..DT_TAP_BPM_MAX and is NOT quantised here --
//     the caller applies dt_bpm_canon() so the number it adopts is the number it sends.
#ifndef DEMIURGE_TRANSPORT_H
#define DEMIURGE_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------- wire grid
#define DT_WIRE_BPM_MIN   40.0
#define DT_WIRE_BPM_MAX   400.0
#define DT_WIRE14_MAX     16383

static inline uint16_t dt_bpm_to_wire14(double bpm) {
    if (!(bpm > DT_WIRE_BPM_MIN)) bpm = DT_WIRE_BPM_MIN;      // also catches NaN
    if (bpm > DT_WIRE_BPM_MAX) bpm = DT_WIRE_BPM_MAX;
    double w = (bpm - DT_WIRE_BPM_MIN) * (double)DT_WIRE14_MAX / (DT_WIRE_BPM_MAX - DT_WIRE_BPM_MIN);
    int32_t r = (int32_t)(w + 0.5);
    if (r < 0) r = 0;
    if (r > DT_WIRE14_MAX) r = DT_WIRE14_MAX;
    return (uint16_t)r;
}
static inline double dt_wire14_to_bpm(uint16_t w) {
    if (w > DT_WIRE14_MAX) w = DT_WIRE14_MAX;
    return DT_WIRE_BPM_MIN + (double)w * (DT_WIRE_BPM_MAX - DT_WIRE_BPM_MIN) / (double)DT_WIRE14_MAX;
}
// Quantise to the wire grid and return what the wire will decode to. Originators adopt THIS.
static inline double dt_bpm_canon(double bpm) { return dt_wire14_to_bpm(dt_bpm_to_wire14(bpm)); }
static inline uint8_t dt_wire14_msb(uint16_t w) { return (uint8_t)((w >> 7) & 0x7F); }
static inline uint8_t dt_wire14_lsb(uint16_t w) { return (uint8_t)(w & 0x7F); }
static inline uint16_t dt_wire14_join(uint8_t msb, uint8_t lsb) {
    return (uint16_t)(((uint16_t)(msb & 0x7F) << 7) | (uint16_t)(lsb & 0x7F));
}

// ---------------------------------------------------------------- tap analyzer
#define DT_TAP_MAX          6            // window: most recent taps kept for the fit
#define DT_TAP_MIN_GAP_US   120000       // closer than this = bounce/duplicate, ignored
#define DT_TAP_EXPIRE_US    2000000      // longer than this since the last tap = new window
#define DT_TAP_OUTLIER      0.40         // |new interval / estimate - 1| above this restarts the window
#define DT_TAP_BPM_MIN      40.0
#define DT_TAP_BPM_MAX      300.0

typedef struct {
    int64_t t_us[DT_TAP_MAX];
    uint8_t n;                           // taps currently in the window (0..DT_TAP_MAX)
} dt_tap_t;

// result codes of dt_tap_feed()/dt_tap_untap()
#define DT_TAP_IGNORED   0               // duplicate/bounce -- nothing changed
#define DT_TAP_ARMED     1               // tap recorded, not enough taps for a tempo yet
#define DT_TAP_TEMPO     2               // *bpm_out holds a fresh tempo

static inline void dt_tap_reset(dt_tap_t *s) { s->n = 0; }

// Least-squares tempo over the current window. Returns 0 if fewer than 2 taps.
static inline int dt_tap_fit(const dt_tap_t *s, double *bpm_out) {
    if (s->n < 2) return 0;
    const double n = (double)s->n;
    const double mean_i = (n - 1.0) * 0.5;
    const int64_t t0 = s->t_us[0];       // subtract t0 first: keeps the sums small and exact
    double mean_t = 0.0;
    for (uint8_t i = 0; i < s->n; i++) mean_t += (double)(s->t_us[i] - t0);
    mean_t /= n;
    double num = 0.0, den = 0.0;
    for (uint8_t i = 0; i < s->n; i++) {
        double di = (double)i - mean_i;
        num += di * ((double)(s->t_us[i] - t0) - mean_t);
        den += di * di;
    }
    if (den <= 0.0 || num <= 0.0) return 0;
    double interval_us = num / den;      // microseconds per tap (= per beat)
    double bpm = 60000000.0 / interval_us;
    if (bpm < DT_TAP_BPM_MIN) bpm = DT_TAP_BPM_MIN;
    if (bpm > DT_TAP_BPM_MAX) bpm = DT_TAP_BPM_MAX;
    *bpm_out = bpm;
    return 1;
}

static inline int dt_tap_feed(dt_tap_t *s, int64_t t_us, double *bpm_out) {
    if (s->n > 0) {
        int64_t gap = t_us - s->t_us[s->n - 1];
        if (gap < DT_TAP_MIN_GAP_US) return DT_TAP_IGNORED;           // bounce / duplicated byte
        if (gap > DT_TAP_EXPIRE_US) s->n = 0;                          // stale window
    }
    // Outlier test against the estimate the window held BEFORE this tap.
    if (s->n >= 3) {
        double est_bpm;
        if (dt_tap_fit(s, &est_bpm)) {
            double est_us = 60000000.0 / est_bpm;
            double new_us = (double)(t_us - s->t_us[s->n - 1]);
            double ratio = new_us / est_us;
            if (ratio < 1.0 - DT_TAP_OUTLIER || ratio > 1.0 + DT_TAP_OUTLIER) {
                s->t_us[0] = s->t_us[s->n - 1];                        // restart from the last two taps
                s->n = 1;
            }
        }
    }
    if (s->n >= DT_TAP_MAX) {                                          // slide the window
        for (uint8_t i = 1; i < DT_TAP_MAX; i++) s->t_us[i - 1] = s->t_us[i];
        s->n = DT_TAP_MAX - 1;
    }
    s->t_us[s->n++] = t_us;
    if (s->n >= 2 && dt_tap_fit(s, bpm_out)) return DT_TAP_TEMPO;
    return DT_TAP_ARMED;
}

// Undo the most recent tap (PMOR's encoder+joystick chord). Returns like dt_tap_feed().
static inline int dt_tap_untap(dt_tap_t *s, double *bpm_out) {
    if (s->n == 0) return DT_TAP_IGNORED;
    s->n--;
    if (s->n >= 2 && dt_tap_fit(s, bpm_out)) return DT_TAP_TEMPO;
    return DT_TAP_ARMED;
}


// ---------------------------------------------------------------- OSC pool wire (/transport/*)
// Minimal OSC 1.0 (no liblo). Addresses on the pool:
//   /transport/tap   ,if  source(0 pmor,1 neptr,2 other), age_ms   in  (UDP 127.0.0.1:9100) and out (9101)
//   /transport/tempo ,f   bpm (wire-canonical)                      out
//   /transport/bar   ,i   bar index since Start                     out
//   /transport/start ,    /transport/stop ,                         out
#define DT_OSC_IN_PORT   9100
#define DT_OSC_OUT_PORT  9101

static inline size_t dt__osc_str(unsigned char *b, size_t o, const char *s) {
    size_t n = 0; while (s[n]) n++;
    n++;                                           // NUL
    for (size_t i = 0; i < n; i++) b[o + i] = (unsigned char)s[i];
    o += n;
    while (o & 3u) b[o++] = 0;
    return o;
}
static inline size_t dt__osc_u32(unsigned char *b, size_t o, uint32_t u) {
    b[o] = (unsigned char)(u >> 24); b[o + 1] = (unsigned char)(u >> 16);
    b[o + 2] = (unsigned char)(u >> 8); b[o + 3] = (unsigned char)u;
    return o + 4;
}
// types: any of "i" "f" in order ("" = no args). Returns bytes written, 0 if it would not fit.
static inline size_t dt_osc_encode(unsigned char *b, size_t cap, const char *addr, const char *types,
                                   int32_t ival, float fval) {
    size_t need = 0; for (const char *a = addr; *a; a++) need++;
    need = (need + 4u) & ~3u;
    size_t nt = 0; for (const char *t = types; *t; t++) nt++;
    need += (nt + 2u + 3u) & ~3u;
    need += 4u * nt;
    if (need > cap) return 0;
    char tt[8]; size_t k = 0; tt[k++] = ',';
    for (size_t i = 0; i < nt && k < 7; i++) tt[k++] = types[i];
    tt[k] = 0;
    size_t o = dt__osc_str(b, 0, addr);
    o = dt__osc_str(b, o, tt);
    for (size_t i = 0; i < nt; i++) {
        if (types[i] == 'i') o = dt__osc_u32(b, o, (uint32_t)ival);
        else { union { float f; uint32_t u; } c; c.f = fval; o = dt__osc_u32(b, o, c.u); }
    }
    return o;
}
// Parse an inbound /transport/tap datagram: ",if" (source, age_ms) | ",i" | "," | ",f" (age only).
// Returns 1 and fills *src (default 2 = other) and *age_ms (default 0) on a valid tap, else 0.
static inline int dt_osc_parse_tap(const unsigned char *b, size_t n, int *src, double *age_ms) {
    static const char addr[] = "/transport/tap";            // 14 chars + NUL = 15 -> padded to 16
    if (n < 20) return 0;
    for (size_t i = 0; i < sizeof addr; i++) if (b[i] != (unsigned char)addr[i]) return 0;
    size_t o = 16;
    if (b[o] != ',') return 0;
    size_t nt = 0; while (o + 1 + nt < n && b[o + 1 + nt]) nt++;
    if (o + 1 + nt >= n) return 0;                           // unterminated type tag
    const unsigned char *tt = b + o + 1;
    o += (nt + 2u + 3u) & ~3u;
    int s = 2; double age = 0.0;
    for (size_t i = 0; i < nt; i++) {
        if (o + 4 > n) return 0;
        uint32_t u = ((uint32_t)b[o] << 24) | ((uint32_t)b[o + 1] << 16) | ((uint32_t)b[o + 2] << 8) | b[o + 3];
        o += 4;
        if (tt[i] == 'i') { if (i == 0) s = (int)(int32_t)u; }
        else if (tt[i] == 'f') { union { float f; uint32_t u; } c; c.u = u; if (i >= 1 || nt == 1) age = (double)c.f; }
    }
    if (s < 0 || s > 2) s = 2;
    *src = s; *age_ms = age;
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif // DEMIURGE_TRANSPORT_H
