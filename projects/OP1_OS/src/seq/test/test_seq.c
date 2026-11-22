/* Host tests + demo renderer for the OP FREAK generative sequencer.
 * usage: test_seq <demo_dir>     (writes <demo_dir>/seq_<mode>.mid, prints ASCII piano rolls) */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../seq.h"

static int g_fail, g_pass;
#define CHECK(c, ...) do { if (c) { g_pass++; } else { g_fail++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define SECTION(n) printf("[%s]\n", n)

static seq_host H, H2;
#define CAP 20000
static seq_event EV[CAP], EV2[CAP];

static int run_ticks(seq_host *h, int ticks, seq_event *buf, int cap)
{
    int n = 0, i;
    seq_event e;
    for (i = 0; i < ticks; i++) {
        seq_tick_one(h);
        while (seq_event_pop(h, &e)) if (n < cap) buf[n++] = e;
    }
    return n;
}

static void train_phrase(seq_host *h)
{
    static const int ph[] = { 60, 62, 64, 67, 64, 62, 60, 62, 64, 69, 67, 64, 62, 60 };
    unsigned i;
    for (i = 0; i < sizeof ph / sizeof *ph; i++) seq_note_in(h, ph[i], 100, 1);
}

static void setup(seq_host *h, int mode, uint32_t seed)
{
    seq_init(h, seed);
    seq_set_mode(h, mode);
    h->up[SEQ_U_PROB] = 80; h->up[SEQ_U_RATCHET] = 30; h->up[SEQ_U_SWING] = 20; h->up[SEQ_U_SPREAD] = 40;
    if (mode == SEQ_MARKOV) train_phrase(h);
    seq_start(h);
}

static int same(const seq_event *a, const seq_event *b, int n)
{
    int i;
    for (i = 0; i < n; i++)
        if (a[i].tick != b[i].tick || a[i].type != b[i].type || a[i].note != b[i].note || a[i].vel != b[i].vel) return 0;
    return 1;
}

/* ------------------------------------------------------------------ MIDI + roll */
static int vlq(uint8_t *p, uint32_t v)
{
    uint8_t t[5]; int n = 0, i;
    t[n++] = v & 0x7F;
    while (v >>= 7) t[n++] = (uint8_t)((v & 0x7F) | 0x80);
    for (i = 0; i < n; i++) p[i] = t[n - 1 - i];
    return n;
}
static int write_mid(const char *path, const char *name, const seq_event *ev, int n, int bpm)
{
    static uint8_t trk[CAP * 8 + 256];
    int len = 0, i, nl = (int)strlen(name);
    uint32_t last = 0, us = 60000000u / (uint32_t)bpm;
    FILE *f;
    uint8_t hdr[14] = { 'M','T','h','d',0,0,0,6,0,0,0,1,0x01,0xE0 };   /* fmt 0, 1 track, 480 ppq */
    len += vlq(trk + len, 0); trk[len++] = 0xFF; trk[len++] = 0x03; trk[len++] = (uint8_t)nl;
    memcpy(trk + len, name, (size_t)nl); len += nl;
    len += vlq(trk + len, 0); trk[len++] = 0xFF; trk[len++] = 0x51; trk[len++] = 3;
    trk[len++] = (uint8_t)(us >> 16); trk[len++] = (uint8_t)(us >> 8); trk[len++] = (uint8_t)us;
    for (i = 0; i < n; i++) {
        uint32_t t = ev[i].tick * 5u;                      /* 96 -> 480 ppq */
        len += vlq(trk + len, t - last); last = t;
        trk[len++] = ev[i].type ? 0x90 : 0x80; trk[len++] = ev[i].note; trk[len++] = ev[i].type ? ev[i].vel : 0;
    }
    len += vlq(trk + len, 0); trk[len++] = 0xFF; trk[len++] = 0x2F; trk[len++] = 0;
    f = fopen(path, "wb");
    if (!f) return 0;
    fwrite(hdr, 1, 14, f);
    fwrite("MTrk", 1, 4, f);
    { uint8_t l[4] = { (uint8_t)(len >> 24), (uint8_t)(len >> 16), (uint8_t)(len >> 8), (uint8_t)len }; fwrite(l, 1, 4, f); }
    fwrite(trk, 1, (size_t)len, f);
    fclose(f);
    return 1;
}

static const char *NN[12] = { "C ","C#","D ","D#","E ","F ","F#","G ","G#","A ","A#","B " };
static void roll(const char *title, const seq_event *ev, int n, int steps)
{
    int cols = steps, lo = 127, hi = 0, i, p, c;
    static char g[128][130];
    memset(g, ' ', sizeof g);
    for (i = 0; i < n; i++) if (ev[i].type && (int)(ev[i].tick / 24) < cols) {
        if (ev[i].note < lo) lo = ev[i].note; if (ev[i].note > hi) hi = ev[i].note;
    }
    printf("--- %s : first %d steps (1 col = 1/16, '#' onset, '=' held) ---\n", title, steps);
    if (lo > hi) { printf("(no notes)\n"); return; }
    for (i = 0; i < n; i++) {
        int t0 = (int)(ev[i].tick / 24);
        if (ev[i].type && t0 < cols) {
            int t1 = cols, j;
            for (j = i + 1; j < n; j++) if (!ev[j].type && ev[j].note == ev[i].note) { t1 = (int)((ev[j].tick + 23) / 24); break; }
            for (c = t0 + 1; c < t1 && c < cols; c++) if (g[ev[i].note][c] == ' ') g[ev[i].note][c] = '=';
            g[ev[i].note][t0] = '#';
        }
    }
    for (p = hi; p >= lo; p--) {
        int used = 0;
        for (c = 0; c < cols; c++) if (g[p][c] != ' ') used = 1;
        if (!used && hi - lo > 26) continue;
        printf("%s%d |", NN[p % 12], p / 12 - 1);
        for (c = 0; c < cols; c++) { putchar(g[p][c]); }
        printf("|\n");
    }
}

/* ------------------------------------------------------------------ tests */
static const char *const euc_name[] = { "E(2,5)", "E(3,4)", "E(3,5)", "E(3,7)", "E(3,8)", "E(4,7)", "E(5,6)", "E(5,8)", "E(5,12)", "E(5,16)", "E(7,8)", "E(7,12)", "E(7,16)", "E(9,16)" };
static const struct { int k, n; const char *p; } euc[] = {
    {2,5,"x.x.."}, {3,4,"x.xx"}, {3,5,"x.x.x"}, {3,7,"x.x.x.."}, {3,8,"x..x..x."}, {4,7,"x.x.x.x"},
    {5,6,"x.xxxx"}, {5,8,"x.xx.xx."}, {5,12,"x..x.x..x.x."}, {5,16,"x..x..x..x..x..."},
    {7,8,"x.xxxxxx"}, {7,12,"x.xx.x.xx.x."}, {7,16,"x..x.x.x..x.x.x."}, {9,16,"x.xx.x.x.xx.x.x."} };
static void bits_str(uint32_t b, int n, char *s) { int i; for (i = 0; i < n; i++) s[i] = (b >> i & 1) ? 'x' : '.'; s[n] = 0; }

static int is_rotation(const char *a, const char *b)
{
    int n = (int)strlen(a), r, i;
    if ((int)strlen(b) != n) return 0;
    for (r = 0; r < n; r++) { for (i = 0; i < n; i++) if (a[(i + r) % n] != b[i]) break; if (i == n) return 1; }
    return 0;
}

static void test_euclid(void)
{
    unsigned i; char s[40];
    SECTION("euclid vs Toussaint table");
    for (i = 0; i < sizeof euc / sizeof *euc; i++) {
        bits_str(seq_euclid_bits(euc[i].k, euc[i].n, 0), euc[i].n, s);
        CHECK(is_rotation(s, euc[i].p), "%s got %s, not a rotation of %s", euc_name[i], s, euc[i].p);
    }
    { int n, k, bad = 0, exact = 0;                 /* maximal evenness for every k <= n <= 32 */
      for (n = 1; n <= 32; n++) for (k = 1; k <= n; k++) {
          uint32_t b = seq_euclid_bits(k, n, 0); int i, pc = 0, first = -1, prev = -1, mn = 99, mx = 0, gap;
          for (i = 0; i < n; i++) if (b >> i & 1) { pc++; if (first < 0) first = i; if (prev >= 0) { gap = i - prev; if (gap < mn) mn = gap; if (gap > mx) mx = gap; } prev = i; }
          gap = first + n - prev; if (gap < mn) mn = gap; if (gap > mx) mx = gap;
          if (pc != k || mx - mn > 1) bad++;
      }
      CHECK(bad == 0, "%d (k,n) pairs with wrong count or uneven gaps", bad);
      for (i = 0; i < (int)(sizeof euc / sizeof *euc); i++) { bits_str(seq_euclid_bits(euc[i].k, euc[i].n, 0), euc[i].n, s); exact += !strcmp(s, euc[i].p); }
      printf("  %d/%d table entries identical in rotation too; all %d (k,n) pairs maximally even\n", exact, (int)(sizeof euc / sizeof *euc), 32 * 33 / 2); }
    bits_str(seq_euclid_bits(3, 8, 1), 8, s); CHECK(!strcmp(s, ".x..x..x"), "rot1 got %s", s);
    bits_str(seq_euclid_bits(3, 8, 2), 8, s); CHECK(!strcmp(s, "x.x..x.."), "rot2 got %s", s);
    bits_str(seq_euclid_bits(3, 8, 8), 8, s); CHECK(!strcmp(s, "x..x..x."), "rot8 wraps got %s", s);
    CHECK(seq_euclid_bits(0, 8, 0) == 0, "0 hits");
    CHECK(seq_euclid_bits(8, 8, 0) == 0xFF, "full");
    /* the pattern as actually played by the mode: layer 1 E(5,8) at 1/16, ratchet/prob off */
    setup(&H, SEQ_EUCLID, 5);
    H.up[SEQ_U_PROB] = 100; H.up[SEQ_U_RATCHET] = 0; H.up[SEQ_U_SWING] = 0;
    H.mp[1] = 5; H.mp[2] = 8; H.mp[3] = 0;
    { int n = run_ticks(&H, 24 * 8, EV, CAP), k, cnt = 0, pos[8] = {0};
      for (k = 0; k < n; k++) if (EV[k].type) { pos[EV[k].tick / 24] = 1; cnt++; }
      for (k = 0; k < 8; k++) s[k] = pos[k] ? 'x' : '.'; s[8] = 0;
      CHECK(!strcmp(s, "x.xx.xx."), "played E(5,8) got %s", s); CHECK(cnt == 5, "5 onsets got %d", cnt); }
    /* stacked layers: 3 layers, per-bar onset counts */
    setup(&H, SEQ_EUCLID, 5);
    H.up[SEQ_U_PROB] = 100; H.up[SEQ_U_RATCHET] = 0; H.up[SEQ_U_SWING] = 0; H.up[SEQ_U_GATE] = 20;
    H.mp[0] = 3; H.mp[1] = 4; H.mp[2] = 16; H.mp[4] = 3; H.mp[5] = 8; H.mp[7] = 5; H.mp[8] = 16; H.mp[10] = 0;
    { int n = run_ticks(&H, 24 * 16, EV, CAP), k, cnt = 0;
      for (k = 0; k < n; k++) if (EV[k].type) cnt++;
      CHECK(cnt == 4 + 6 + 5, "stacked layers onsets per 16 steps = 15, got %d", cnt); }
}

static void test_determinism(void)
{
    int m, n1, n2;
    SECTION("determinism (same seed + params = same event stream)");
    for (m = 0; m < SEQ_NMODES; m++) {
        setup(&H, m, 4242); n1 = run_ticks(&H, 96 * 16, EV, CAP);
        setup(&H2, m, 4242); n2 = run_ticks(&H2, 96 * 16, EV2, CAP);
        CHECK(n1 > 0 && n1 == n2 && same(EV, EV2, n1), "%s: runs differ (%d vs %d events)", seq_mode_name(m), n1, n2);
        setup(&H2, m, 4243); n2 = run_ticks(&H2, 96 * 16, EV2, CAP);
        CHECK(!(n1 == n2 && same(EV, EV2, n1)), "%s: seed 4243 gave the same stream as 4242", seq_mode_name(m));
        /* restart on the same host: seq_start rewinds + reseeds */
        seq_stop(&H); { seq_event e; while (seq_event_pop(&H, &e)) ; }
        seq_start(&H); n2 = run_ticks(&H, 96 * 16, EV2, CAP);
        CHECK(n1 == n2 && same(EV, EV2, n1), "%s: restart not identical", seq_mode_name(m));
        printf("  %-8s %5d events / 16 beats\n", seq_mode_name(m), n1);
    }
    /* clock paths agree: 64-frame blocks == tick loop; external pulses == ticks */
    setup(&H, SEQ_TURING, 9); n1 = run_ticks(&H, 96 * 8, EV, CAP);
    setup(&H2, SEQ_TURING, 9);
    { int f, n = 0; seq_event e; uint32_t frames = 0;
      while (H2.tick < 96 * 8) { seq_run_frames(&H2, 64, 44100); frames += 64; while (seq_event_pop(&H2, &e)) EV2[n++] = e; }
      CHECK(same(EV, EV2, n1 < n ? n1 : n) && n >= n1, "block-driven clock differs from tick loop (%d vs %d)", n1, n);
      /* 120 bpm: 8 beats = 4 s = 176400 frames; allow one block of slack */
      f = (int)frames; CHECK(f > 176400 - 64 && f < 176400 + 256, "8 beats @120bpm took %d frames", f); }
    setup(&H, SEQ_TURING, 9); H.up[SEQ_U_CLKSRC] = SEQ_CLK_EXTERNAL;
    seq_ext_stop(&H); seq_ext_start(&H); { int i, n = 0; seq_event e; for (i = 0; i < 24 * 8; i++) { seq_ext_pulse(&H); while (seq_event_pop(&H, &e)) EV2[n++] = e; }
      CHECK(n >= n1 && same(EV, EV2, n1), "external-clock stream differs from internal (%d vs %d)", n1, n); }
    seq_run_frames(&H, 44100, 44100); CHECK(H.tick == 24 * 8 * 4, "internal clock must be inert while EXTERNAL (tick %u)", H.tick);
}

static void test_markov(void)
{
    int a, b, i, n, ce = 0, cc = 0; seq_host *h = &H;
    SECTION("markov weighting");
    seq_init(h, 77); seq_set_mode(h, SEQ_MARKOV);
    h->mp[0] = 0; h->mp[1] = 0;                       /* no forget, no mutate */
    h->up[SEQ_U_SCALE] = SEQ_SC_CHROMATIC; h->up[SEQ_U_PROB] = 100; h->up[SEQ_U_RATCHET] = 0;
    h->up[SEQ_U_SWING] = 0; h->up[SEQ_U_SPREAD] = 0; h->up[SEQ_U_GATE] = 30;
    for (i = 0; i < 10; i++) { seq_note_in(h, 60, 100, 1); seq_note_in(h, 62, 100, 1); }
    seq_note_in(h, 60, 100, 1); seq_note_in(h, 64, 100, 1); seq_note_in(h, 60, 100, 1);
    CHECK(seq_markov_weight(h, 0, 2) == 160, "w[C][D] = %d want 160 (10 plays x INC 16)", seq_markov_weight(h, 0, 2));
    CHECK(seq_markov_weight(h, 0, 4) == 16, "w[C][E] = %d want 16", seq_markov_weight(h, 0, 4));
    CHECK(seq_markov_weight(h, 2, 0) == 160 - 0 || seq_markov_weight(h, 2, 0) == 160, "w[D][C] = %d", seq_markov_weight(h, 2, 0));
    CHECK(seq_markov_weight(h, 0, 7) == 0, "unplayed transition must stay 0");
    seq_start(h); n = run_ticks(h, 24 * 8000, EV, CAP);
    for (i = 0, a = -1; i < n; i++) if (EV[i].type) {
        b = EV[i].note % 12;
        if (a == 0) { if (b == 2) cc++; else if (b == 4) ce++; else CHECK(0, "C -> %d never played", b); }
        a = b;
    }
    { int tot = cc + ce; double fe = tot ? (double)ce / tot : 0;
      printf("  C->D %d, C->E %d : E share %.3f (expected %.3f)\n", cc, ce, fe, 16.0 / 176.0);
      CHECK(tot > 500 && fe > 0.06 && fe < 0.12, "E share %.3f outside 0.06..0.12", fe); }
    /* more plays = more likely */
    seq_note_in(h, 60, 100, 1); for (i = 0; i < 9; i++) { seq_note_in(h, 64, 100, 1); seq_note_in(h, 60, 100, 1); }
    CHECK(seq_markov_weight(h, 0, 4) == 16 * 10, "w[C][E] after 10 plays = %d", seq_markov_weight(h, 0, 4));
    seq_start(h); n = run_ticks(h, 24 * 8000, EV, CAP);
    cc = ce = 0;
    for (i = 0, a = -1; i < n; i++) if (EV[i].type) { b = EV[i].note % 12; if (a == 0) { if (b == 2) cc++; else if (b == 4) ce++; } a = b; }
    { double fe = (double)ce / (cc + ce); printf("  after 9 more C->E: E share %.3f (expected 0.500)\n", fe);
      CHECK(fe > 0.44 && fe < 0.56, "E share %.3f outside 0.44..0.56", fe); }
    /* forget: weights shrink bar over bar, monotonic, reach 0 */
    h->mp[0] = 40; { int w0 = seq_markov_weight(h, 0, 2), w1, prev = w0, mono = 1, k;
      seq_start(h);
      for (k = 0; k < 40; k++) { run_ticks(h, 24 * 16, EV, CAP); w1 = seq_markov_weight(h, 0, 2); if (w1 > prev) mono = 0; prev = w1; }
      CHECK(mono && w1 < w0 / 4, "forget 40: w %d -> %d after 40 bars", w0, w1); }
    /* clear action */
    { seq_ui_cell c[4]; int k; seq_ui_page(h, 0, 1, c); CHECK(c[0].kind == SEQ_CELL_CLEAR, "CLEAR cell on page 0.1");
      k = seq_encoder(h, 0, 1, 0, 1, 0); CHECK(k, "CLEAR trigger");
      CHECK(seq_markov_weight(h, 0, 2) == 0 && seq_markov_weight(h, 0, 4) == 0, "matrix cleared");
      seq_start(h); n = run_ticks(h, 24 * 32, EV, CAP); CHECK(n == 0, "cleared markov must be silent (%d events)", n); }
    /* matrix survives a mode switch */
    seq_init(h, 3); seq_set_mode(h, SEQ_MARKOV); seq_note_in(h, 60, 1, 1); seq_note_in(h, 62, 1, 1);
    seq_set_mode(h, SEQ_TOTAL); seq_set_mode(h, SEQ_MARKOV);
    CHECK(seq_markov_weight(h, 0, 2) == 16, "learned weights survive mode switch");
    /* learn off */
    h->mp[2] = 0; seq_note_in(h, 62, 1, 1); seq_note_in(h, 64, 1, 1); CHECK(seq_markov_weight(h, 2, 4) == 0, "LEARN=0 learns nothing");
}

static int in_scale(int pitch, int root, const seq_scale *sc)
{
    int rel = ((pitch - root) % 12 + 12) % 12, i;
    for (i = 0; i < sc->n; i++) if (sc->iv[i] == rel) return 1;
    return 0;
}

static void test_scale_and_range(void)
{
    int m, sc, root, bad = 0, total = 0, oor = 0, k, n;
    SECTION("scale lock + octave spread");
    for (m = 0; m < SEQ_NMODES; m++)
        for (sc = 0; sc < SEQ_SC_COUNT; sc++)
            for (root = 0; root < 12; root += 5) {
                setup(&H, m, 100 + (uint32_t)sc);
                H.up[SEQ_U_SCALE] = (int16_t)sc; H.up[SEQ_U_ROOT] = (int16_t)root; H.up[SEQ_U_SPREAD] = 100;
                H.up[SEQ_U_PROB] = 100; H.up[SEQ_U_OCT] = (int16_t)(1 + (sc % 4));
                if (m == SEQ_MARKOV) { H.mp[1] = 30; seq_start(&H); }       /* mutate path too */
                n = run_ticks(&H, 24 * 300, EV, CAP);
                for (k = 0; k < n; k++) if (EV[k].type) {
                    total++;
                    if (!in_scale(EV[k].note, root, &seq_scales[sc])) bad++;
                    if (EV[k].note > 127) oor++;
                }
            }
    printf("  %d note-ons over 6 modes x 14 scales x 3 roots\n", total);
    CHECK(total > 1000 && bad == 0, "out-of-scale notes: %d of %d", bad, total);
    CHECK(oor == 0, "notes > 127: %d", oor);

    /* spread bounds + linearity */
    { int maxp[5] = {0}; static const int sp[5] = { 0, 20, 40, 70, 100 }; int ok = 1, si;
      for (si = 0; si < 5; si++) {
          int lo, hi_allowed, maxo = 0, mino = 999;
          for (m = 0; m < SEQ_NMODES; m++) {
              setup(&H, m, 31); H.up[SEQ_U_SPREAD] = (int16_t)sp[si]; H.up[SEQ_U_PROB] = 100; H.up[SEQ_U_OCT] = 2;
              H.up[SEQ_U_ROOT] = 3; H.up[SEQ_U_SCALE] = SEQ_SC_LYDIAN; if (m == SEQ_MARKOV) seq_start(&H);
              n = run_ticks(&H, 24 * 1500, EV, CAP);
              lo = 12 * 2 + 3;
              hi_allowed = lo + 11 + 12 * ((sp[si] * 5 + 99) / 100);
              for (k = 0; k < n; k++) if (EV[k].type) {
                  if (EV[k].note < lo || EV[k].note > hi_allowed) { ok = 0; printf("  %s spread %d: note %d outside %d..%d\n", seq_mode_name(m), sp[si], EV[k].note, lo, hi_allowed); }
                  if (EV[k].note - lo > maxo) maxo = EV[k].note - lo;
                  if (EV[k].note - lo < mino) mino = EV[k].note - lo;
              }
          }
          maxp[si] = maxo;
          printf("  spread %3d%% (%.2f oct): highest note +%d semitones over root\n", sp[si], sp[si] * 0.05, maxo);
      }
      CHECK(ok, "note outside allowed window");
      CHECK(maxp[0] < 12, "spread 0 must stay inside one octave, got +%d", maxp[0]);
      CHECK(maxp[1] < maxp[2] && maxp[2] < maxp[3] && maxp[3] < maxp[4], "highest note must grow with spread");
      CHECK(maxp[4] >= 12 * 4 && maxp[4] <= 12 * 5 + 11, "spread 100 reaches ~5 octaves, got +%d", maxp[4]); }
}

static void test_universal(void)
{
    int n, k, on = 0, off = 0, bal;
    SECTION("probability / ratchet / swing / gate / note balance");
    setup(&H, SEQ_TURING, 8); H.up[SEQ_U_PROB] = 0; n = run_ticks(&H, 24 * 200, EV, CAP);
    CHECK(n == 0, "probability 0 must be silent (%d)", n);
    setup(&H, SEQ_TURING, 8); H.mp[3] = 100; H.up[SEQ_U_PROB] = 100; H.up[SEQ_U_RATCHET] = 0; n = run_ticks(&H, 24 * 200, EV, CAP);
    for (k = 0; k < n; k++) on += EV[k].type;
    CHECK(on == 200, "prob 100 density 100 = one onset per step, got %d", on);
    setup(&H, SEQ_TURING, 8); H.mp[3] = 100; H.up[SEQ_U_PROB] = 50; H.up[SEQ_U_RATCHET] = 0; n = run_ticks(&H, 24 * 4000, EV, CAP);
    for (on = 0, k = 0; k < n; k++) on += EV[k].type;
    CHECK(on > 1800 && on < 2200, "prob 50 gave %d of 4000", on);
    setup(&H, SEQ_TURING, 8); H.mp[3] = 100; H.up[SEQ_U_PROB] = 100; H.up[SEQ_U_RATCHET] = 100; n = run_ticks(&H, 24 * 200, EV, CAP);
    for (on = 0, k = 0; k < n; k++) on += EV[k].type;
    CHECK(on >= 400 && on <= 800, "ratchet 100 gave %d onsets for 200 steps", on);
    setup(&H, SEQ_TURING, 8); H.mp[3] = 100; H.up[SEQ_U_PROB] = 100; H.up[SEQ_U_RATCHET] = 0; H.up[SEQ_U_SWING] = 100;
    n = run_ticks(&H, 24 * 8, EV, CAP);
    { int ok = 1;
      for (k = 0, on = 0; k < n; k++) if (EV[k].type) { if ((EV[k].tick % 48) != 0 && (EV[k].tick % 48) != 36) ok = 0; on++; }
      CHECK(ok && on == 8, "swing 100: onsets at 0 and +36 of each 48-tick pair (ok=%d on=%d)", ok, on); }
    setup(&H, SEQ_TURING, 8); H.mp[3] = 100; H.up[SEQ_U_PROB] = 100; H.up[SEQ_U_SWING] = 0; H.up[SEQ_U_RATCHET] = 0; H.up[SEQ_U_GATE] = 25;
    n = run_ticks(&H, 24 * 16, EV, CAP);
    { int ok = 1; for (k = 0; k + 1 < n; k++) if (EV[k].type && !EV[k + 1].type && EV[k + 1].tick - EV[k].tick != 6) ok = 0;
      CHECK(ok, "gate 25%% = 6 ticks of a 24 tick step"); }
    /* balance across heavy settings, then stop flushes */
    for (k = 0; k < SEQ_NMODES; k++) {
        setup(&H, k, 5 + (uint32_t)k); H.up[SEQ_U_GATE] = 200; H.up[SEQ_U_RATCHET] = 100; H.up[SEQ_U_SWING] = 80; H.up[SEQ_U_PROB] = 90;
        n = run_ticks(&H, 24 * 600, EV, CAP);
        seq_stop(&H); { seq_event e; while (seq_event_pop(&H, &e)) if (n < CAP) EV[n++] = e; }
        { int held[128] = {0}, i, errs = 0;
          for (i = 0; i < n; i++) { if (EV[i].type) held[EV[i].note]++; else { if (!held[EV[i].note]) errs++; else held[EV[i].note]--; } }
          for (i = 0; i < 128; i++) if (held[i]) errs++;
          CHECK(errs == 0 && H.dropped == 0, "%s: on/off mismatch %d, dropped %u", seq_mode_name(k), errs, H.dropped); }
    }
    (void)off; (void)bal;
}

static uint32_t mk_cb_n;
static void cb(void *u, const seq_mod *m) { (void)u; mk_cb_n++; if (m->pitch > 65535) mk_cb_n += 1000000; }

static void test_mod_and_ui(void)
{
    int n, k; seq_ui_cell c[4];
    SECTION("mod stream + UI model");
    setup(&H, SEQ_EUCLID, 3); H.mod_cb = cb; mk_cb_n = 0;
    n = run_ticks(&H, 24 * 64, EV, CAP);
    CHECK(mk_cb_n == 64, "mod callback once per step: %u", mk_cb_n);
    CHECK(seq_mod_get(&H)->step == 63, "mod.step = %u", seq_mod_get(&H)->step);
    CHECK(seq_mod_get(&H)->accent == 0 && seq_mod_get(&H)->phase == 65535 / 16 * 15 + 15 / 1 * 0 + (65535 * 15 / 16 - 65535 / 16 * 15), "phase on step 63 (%u)", seq_mod_get(&H)->phase);
    (void)n;
    /* page model */
    seq_init(&H, 1);
    CHECK(seq_ui_subpages(&H, 0) == 1 && seq_ui_subpages(&H, 1) == 2, "subpage counts");
    seq_set_mode(&H, SEQ_EUCLID); CHECK(seq_ui_subpages(&H, 0) == 3, "euclid mode pages = 3");
    seq_ui_page(&H, 0, 2, c); CHECK(!strcmp(c[0].name, "STEPS 3") && !strcmp(c[2].name, "MELODY"), "euclid page 3 names");
    seq_ui_page(&H, 1, 0, c); CHECK(!strcmp(c[0].name, "SCALE") && c[0].text && !strcmp(c[0].text, "LYDIAN"), "default scale lydian");
    CHECK(seq_encoder(&H, 1, 0, 0, 2, 0) && H.up[SEQ_U_SCALE] == 2, "scale encoder");
    CHECK(seq_encoder(&H, 1, 0, 3, 50, 0) && H.up[SEQ_U_SPREAD] == 50, "spread encoder clamps to range");
    seq_encoder(&H, 1, 0, 3, 99, 1); CHECK(H.up[SEQ_U_SPREAD] == 100, "spread clamp 100");
    seq_push(&H, 1, 0, 3); CHECK(H.up[SEQ_U_SPREAD] == 0, "push = default");
    { uint32_t s0 = H.seed; seq_push(&H, 3, 0, 2); CHECK(H.seed != s0, "RESEED changes seed"); }
    seq_encoder(&H, 3, 0, 0, 1, 0); CHECK(H.mode == SEQ_MARKOV, "mode knob");
    seq_encoder(&H, 3, 0, 3, 1, 0); CHECK(H.running == 1, "RUN knob");
    seq_encoder(&H, 2, 0, 0, 10, 0); CHECK(H.bpm_x100 == 13000, "bpm %u", H.bpm_x100);
    for (k = 0; k < SEQ_NMODES; k++) { seq_viz v; seq_set_mode(&H, k); seq_viz_get(&H, &v); CHECK(v.w > 0 && v.h > 0 || k == SEQ_MARKOV, "%s viz size", seq_mode_name(k)); }
    /* clock out stub */
    { static int pulses, starts, stops; seq_clock_io io;
      pulses = starts = stops = 0;
      { /* local functions via static */ }
      (void)io; (void)pulses; (void)starts; (void)stops; }
}

static int g_pulses, g_starts, g_stops;
static void io_pulse(void *u) { (void)u; g_pulses++; }
static void io_start(void *u) { (void)u; g_starts++; }
static void io_stop(void *u) { (void)u; g_stops++; }
static void test_clock_out(void)
{
    seq_clock_io io; io.user = 0; io.pulse = io_pulse; io.start = io_start; io.stop = io_stop;
    SECTION("clock out (master) stub");
    seq_init(&H, 1); H.clock_io = &io; H.up[SEQ_U_CLKSRC] = SEQ_CLK_MASTER;
    seq_start(&H); run_ticks(&H, 96 * 4, EV, CAP); seq_stop(&H);
    CHECK(g_pulses == 96 && g_starts == 1 && g_stops == 1, "pulses %d starts %d stops %d (want 96/1/1)", g_pulses, g_starts, g_stops);
}

/* total: slow evolution */
static void test_total(void)
{
    static uint8_t grid[160][16]; int bar, s, n, k;
    double cons = 0, far_eq = 0; int nc = 0;
    SECTION("TOTAL evolves slowly, needs no input");
    seq_init(&H, 2026); seq_set_mode(&H, SEQ_TOTAL);
    H.up[SEQ_U_PROB] = 100; H.up[SEQ_U_RATCHET] = 0; H.up[SEQ_U_SWING] = 0; H.up[SEQ_U_SPREAD] = 0; H.up[SEQ_U_GATE] = 40;
    seq_start(&H);
    n = run_ticks(&H, 24 * 16 * 160, EV, CAP);
    memset(grid, 0, sizeof grid);
    for (k = 0; k < n; k++) if (EV[k].type) { int st = (int)(EV[k].tick / 24); grid[st / 16][st % 16] = EV[k].note; }
    for (bar = 1; bar < 160; bar++) { int eq = 0; for (s = 0; s < 16; s++) eq += grid[bar][s] == grid[bar - 1][s]; cons += eq / 16.0; nc++; }
    cons /= nc;
    { int eq = 0; for (s = 0; s < 16; s++) eq += grid[128][s] == grid[0][s]; far_eq = eq / 16.0; }
    { int eq = 0; for (s = 0; s < 16; s++) eq += grid[150][s] == grid[22][s]; far_eq = (far_eq + eq / 16.0) / 2; }
    printf("  160 bars @120 bpm (%.1f min): adjacent-bar match %.2f, match after ~4 min %.2f\n", 160 * 2 / 60.0, cons, far_eq);
    CHECK(cons > 0.75, "adjacent bars too different (%.2f)", cons);
    CHECK(far_eq < cons - 0.15, "material did not evolve (far %.2f vs adjacent %.2f)", far_eq, cons);
    CHECK(n > 500, "TOTAL produced only %d events without input", n);
}

/* ------------------------------------------------------------------ demos */
int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    int m;
    printf("OP FREAK sequencer host tests (sizeof seq_host = %u bytes)\n", (unsigned)sizeof(seq_host));
    test_euclid();
    test_determinism();
    test_markov();
    test_scale_and_range();
    test_universal();
    test_mod_and_ui();
    test_clock_out();
    test_total();

    printf("\n==== demos -> %s ====\n", dir);
    for (m = 0; m < SEQ_NMODES; m++) {
        char path[512]; int n, bars = (m == SEQ_TOTAL) ? 48 : 8;
        seq_init(&H, 20261104u + (uint32_t)m);
        seq_set_mode(&H, m);
        H.up[SEQ_U_SPREAD] = 30; H.up[SEQ_U_SWING] = 18; H.up[SEQ_U_GATE] = 60; H.up[SEQ_U_RATCHET] = 10;
        H.up[SEQ_U_ROOT] = 2; H.up[SEQ_U_OCT] = 3;
        if (m == SEQ_EUCLID) { H.mp[0] = 3; H.mp[10] = 35; }
        if (m == SEQ_GAMELAN) { H.up[SEQ_U_SCALE] = SEQ_SC_SLENDRO; H.up[SEQ_U_SPREAD] = 12; H.up[SEQ_U_SWING] = 0; H.up[SEQ_U_RATCHET] = 0; }
        if (m == SEQ_CELLULAR) H.mp[0] = 1;
        if (m == SEQ_MARKOV) { int r; for (r = 0; r < 3; r++) train_phrase(&H); H.mp[1] = 8; }
        seq_start(&H);
        n = run_ticks(&H, 96 * 4 * bars, EV, CAP);
        seq_stop(&H); { seq_event e; while (seq_event_pop(&H, &e)) if (n < CAP) EV[n++] = e; }
        snprintf(path, sizeof path, "%s/seq_%s.mid", dir, seq_modes[m]->name);
        for (char *p = strrchr(path, '/'); p && *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
        CHECK(write_mid(path, seq_modes[m]->name, EV, n, 120), "write %s", path);
        printf("%s  (%d events, %d bars)\n", path, n, bars);
        roll(seq_modes[m]->name, EV, n, 64);
    }
    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
