// Fader pipeline for test 10, one path for all three faders:  raw ADC -> 20 ms one-pole -> linearize() (the A-taper curve the web UI shows) -> per-fader endpoint
// calibration n = (lin - lin(lo)) / (lin(hi) - lin(lo)) -> bottom/top dead zones with hysteresis -> g_fs[i] in 0..1.
//   Bottom: n <= FADER_ON (3 %) reads EXACTLY 0.0 (this triggers the CPU gate); once live it snaps back below FADER_OFF (1.5 %).
//   Top:    n >= 1 - FADER_TOP (3 %) reads EXACTLY 1.0; it leaves the top zone again below 1 - 1.5 * FADER_TOP.
// lo / hi are raw ADC endpoints, one pair per fader (the three faders do not have the same raw range). They are learned (auto while no stored calibration exists,
// or on "fcal learn"), set by hand ("fcal set <i> <lo> <hi>"), and persisted in the same QSPI record as the effect parameters (prec.h flo/fhi/fvalid).
//   fcal learn          forget the stored endpoints and learn again: move every fader to the bottom and to the top
//   fcal save           commit the endpoints seen so far
//   fcal set i lo hi    manual
//   fcal reset          defaults (0..65535), learning on
#pragma once
// Measured at rest on the instrument (after.txt 2026-11-17): raw 24..42 at the bottom stop, which linearize() turns into 1.2..2.3 % (the A-taper inverse stretches the bottom), and up to
// 14 % for a few ms at boot (ADC settling). So the bottom zone is generous (on at 7 %, off below 4.5 %), needs 40 ms above the threshold, and nothing is live in the first 1.5 s.
static const float FADER_ON = 0.07f, FADER_OFF = 0.045f, FADER_TOP = 0.03f;   // dead zones, tunable
static const uint32_t FADER_BOOT_MS = 1500; static const int FADER_DEBOUNCE = 8;   // frames of 5 ms
static const uint16_t FADER_MIN_SPAN = 50000;                                  // a fader must have moved at least this far (raw counts) before its endpoints are trusted
static const uint16_t FADER_DEF_LO = 0, FADER_DEF_HI = 63500;                  // uncalibrated defaults from the measured stops: bottom raw 4..42, top raw 63640..63770
static uint16_t g_flo[3] = {FADER_DEF_LO, FADER_DEF_LO, FADER_DEF_LO}, g_fhi[3] = {FADER_DEF_HI, FADER_DEF_HI, FADER_DEF_HI};
static uint32_t g_fvalid = 0;                      // bit i: fader i has stored endpoints
static bool g_flearn = true;                       // auto-learning while any fader has no stored endpoints
static float g_fsm[3] = {-1, -1, -1}, g_fmn[3] = {65535, 65535, 65535}, g_fmx[3] = {0, 0, 0};
static uint32_t g_fchg[3] = {0, 0, 0}; static bool g_fdirty = false;
static bool g_flive[3] = {false, false, false}, g_ftop[3] = {false, false, false}; static int g_fup[3] = {0, 0, 0};
static float g_fs[3] = {0, 0, 0};                  // calibrated, 0..1, exactly 0 / exactly 1 in the dead zones
static volatile bool g_fcal_hello = false;
static void faderReset(bool learn) {
    for(int i = 0; i < 3; i++) { g_flo[i] = FADER_DEF_LO; g_fhi[i] = FADER_DEF_HI; g_fmn[i] = 65535; g_fmx[i] = 0; g_fchg[i] = 0; }
    g_fvalid = 0; g_flearn = learn; g_fdirty = true; g_fcal_hello = true;
}
static void faderStep(const uint16_t* v, uint32_t now) {
    for(int i = 0; i < 3 && i < NCH; i++) {
        float r = (float)v[i];
        g_fsm[i] = g_fsm[i] < 0 ? r : g_fsm[i] + 0.22f * (r - g_fsm[i]);               // ~20 ms at 200 Hz: noise must not chatter the snap
        if(g_flearn && !(g_fvalid & (1u << i))) {
            bool chg = false;
            if(g_fsm[i] < g_fmn[i] - 40.0f) { g_fmn[i] = g_fsm[i]; chg = true; }
            if(g_fsm[i] > g_fmx[i] + 40.0f) { g_fmx[i] = g_fsm[i]; chg = true; }
            if(g_fmn[i] > g_fsm[i] && g_fmx[i] < g_fsm[i]) { g_fmn[i] = g_fmx[i] = g_fsm[i]; }
            if(chg) g_fchg[i] = now;
            if(g_fmx[i] - g_fmn[i] >= FADER_MIN_SPAN && now - g_fchg[i] > 3000) {       // moved far enough and the extremes have settled for 3 s: store
                g_flo[i] = (uint16_t)g_fmn[i]; g_fhi[i] = (uint16_t)g_fmx[i]; g_fvalid |= 1u << i; g_fdirty = true; g_fcal_hello = true;
                if(g_fvalid == 7) g_flearn = false;
            }
        }
        uint16_t lo = g_flo[i], hi = g_fhi[i];
        if(g_flearn && !(g_fvalid & (1u << i)) && g_fmx[i] - g_fmn[i] >= FADER_MIN_SPAN) { lo = (uint16_t)g_fmn[i]; hi = (uint16_t)g_fmx[i]; }   // provisional, already usable
        float la = (float)linearize(i, (uint16_t)(g_fsm[i] + 0.5f)), l0 = (float)linearize(i, lo), l1 = (float)linearize(i, hi);
        float den = l1 - l0, n = fabsf(den) > 1000.0f ? (la - l0) / den : (float)((int)(g_fsm[i]) - lo) / (float)(hi > lo + 100 ? hi - lo : 1);
        if(g_simFader[i] >= 0.0f) n = g_simFader[i] * 0.001f;                    // sim fader: enters here, so the dead zones, snaps and debounce are the real ones
        n = n < 0 ? 0 : n > 1 ? 1 : n;
        if(g_flive[i]) { if(n < FADER_OFF) { g_flive[i] = false; g_fup[i] = 0; } }
        else if(n > FADER_ON && now > FADER_BOOT_MS) { if(++g_fup[i] >= FADER_DEBOUNCE) g_flive[i] = true; } else g_fup[i] = 0;
        if(g_ftop[i]) { if(n < 1.0f - 1.5f * FADER_TOP) g_ftop[i] = false; } else if(n >= 1.0f - FADER_TOP) g_ftop[i] = true;
        g_fs[i] = g_ftop[i] ? 1.0f : g_flive[i] ? n : 0.0f;
    }
}
static void faderCmd(char* const* t, int nt) {          // t[0] == "fcal"
    if(nt < 2) return;
    if(!strcmp(t[1], "learn")) faderReset(true);
    else if(!strcmp(t[1], "reset")) faderReset(true);
    else if(!strcmp(t[1], "save")) { for(int i = 0; i < 3; i++) if(g_fmx[i] - g_fmn[i] >= 2000) { g_flo[i] = (uint16_t)g_fmn[i]; g_fhi[i] = (uint16_t)g_fmx[i]; g_fvalid |= 1u << i; } g_flearn = (g_fvalid != 7); g_fdirty = true; g_fcal_hello = true; }
    else if(!strcmp(t[1], "set") && nt >= 5) { int i = atoi(t[2]); int lo = atoi(t[3]), hi = atoi(t[4]); if(i >= 0 && i < 3 && lo >= 0 && hi <= 65535 && hi - lo >= 1000) { g_flo[i] = (uint16_t)lo; g_fhi[i] = (uint16_t)hi; g_fvalid |= 1u << i; g_flearn = (g_fvalid != 7); g_fdirty = true; g_fcal_hello = true; } }
}
static void faderHello() {
    puts_(",\"fcal\":{\"v\":"); putu(g_fvalid); puts_(",\"learn\":"); put(g_flearn ? '1' : '0'); puts_(",\"e\":[");
    for(int i = 0; i < 3; i++) { if(i) put(','); put('['); putu(g_flo[i]); put(','); putu(g_fhi[i]); put(']'); }
    puts_("],\"seen\":["); for(int i = 0; i < 3; i++) { if(i) put(','); put('['); putu((uint32_t)(g_fmn[i] > g_fmx[i] ? 0 : g_fmn[i])); put(','); putu((uint32_t)g_fmx[i]); put(']'); } puts_("]}");
}
