// fx_v2.cpp — THE PMOR_V2 PUNCH-IN FX, VERBATIM (PMOR_V2.cpp L2518-3353, ISR body), wrapped in a
// per-block function. 8 banks x 16 pads exactly as v2's FX1..FX8 menus. Bank-0 host-side
// effects (ST4/8/16/32, BZZ, HALF, DBL, STOP, WASH) are driven by fx.cpp's host logic (the
// v2 gridTick side); everything audible below is the v2 DSP untouched except for the plumbing
// (globals -> FxV2Blk fields, DSY_SDRAM_BSS -> SDRAM_BSS).
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "core_config.h"
#include "fx_v2.h"

// ---- v2 pad enums (PMOR_V2.cpp L833-938) — physical layout, row 0 = top
enum { FX_ST4 = 0,  FX_ST8,   FX_ST16,  FX_ST32,
       FX_REV,      FX_CHOR,  FX_PAN,   FX_CRUSH,
       FX_HALF,     FX_DBL,   FX_BZZ,   FX_MUTE,
       FX_FLT,      FX_WASH,  FX_DIST,  FX_STOP };
enum { F2_DUB=0,  F2_GVRB, F2_TGATE, F2_FLANG,
       F2_RING,   F2_FOLD, F2_COMB,  F2_FORM,
       F2_ISOLO,  F2_ISOMD,F2_ISOHI, F2_PHONE,
       F2_GRAIN,  F2_WOW,  F2_DOWN,  F2_SCAT };
enum { E3_PLATE=0, E3_SHIM,  E3_PONG,  E3_SLAP,
       E3_PHAS,   E3_WAH,   E3_TREM,  E3_HAAS,
       E3_VIBR,   E3_DIM,   E3_SPRG,  E3_SIREN,
       E3_WARM,   E3_SUB,   E3_AIR,   E3_PUMP };
enum { E4_OCTU=0, E4_OCTD,  E4_DSTR,  E4_REPT,
       E4_RGRN,   E4_FRZ,   E4_METL,  E4_FORM,
       E4_FSHF,   E4_FUZZ,  E4_GSEQ,  E4_SRMP,
       E4_VSTP,   E4_RADIO, E4_NGAT,  E4_SHMD };
enum { E5_REPEAT2=0, E5_REPEAT6, E5_NOTESHUF, E5_QUANT68,
       E5_PUNCH,     E5_FILLIN,  E5_BUILDUP,  E5_BOUNCE,
       E5_LPFIX,     E5_HPFIX,   E5_SQUASH,   E5_SCRATCH,
       E5_SCRTFAST,  E5_GLITCH,  E5_BLINDS,   E5_SAMPHLD };
enum { E6_DELAY=0,  E6_PINGPONG, E6_ECHOFADE, E6_FEEDBACK,
       E6_DRONE,    E6_FREEZE,   E6_UNISON,   E6_UNILOW,
       E6_DETUNE,   E6_PITCHLFO, E6_BENDUP,   E6_BENDDN,
       E6_WARBLE,   E6_HARM5,    E6_SUBOCT,   E6_LOOP };
enum { E7_GATE4=0, E7_GATE8,  E7_GATE16, E7_TRANCE,
       E7_TREM,    E7_PAN,     E7_PUMP,   E7_SWELL,
       E7_NOISE,   E7_WIDE,    E7_MONO,   E7_TILTU,
       E7_TILTD,   E7_DRIVE,   E7_SOFTG,  E7_KILL };
enum { E8_RATCH2=0, E8_RATCH3, E8_RATCH4, E8_RATCH8,
       E8_STRUM,    E8_TRANCE2, E8_STAB,  E8_RINGLO,
       E8_RINGHI,   E8_VIBR,   E8_CHOP,   E8_HALFG,
       E8_DBLG,     E8_GLCHG,  E8_REVG,   E8_SILENCE };

const char* const FX_V2_NAMES[8][16] = {
    { "ST4",  "ST8",  "ST16", "ST32", "REV",  "CHOR", "PAN",  "CRUSH", "HALF", "DBL",  "BZZ",  "MUTE", "FLT",  "WASH", "DIST", "STOP" },
    { "DUB",  "GVERB","TGATE","FLANG","RING", "FOLD", "COMB", "VOWEL", "ISO-LO","ISO-MD","ISO-HI","PHONE","GRAIN","WOW",  "DOWN", "SCAT" },
    { "PLATE","SHIMR","PONG", "SLAP", "PHASE","WAH",  "TREM", "HAAS",  "VIBRA","DIMEN","SPRNG","SIREN","WARM", "SUB",  "AIR",  "PUMP" },
    { "OCT+", "OCT-", "DSTRY","REPT", "RGRN", "FREEZ","METAL","FRMNT", "FSHFT","FUZZ", "GSEQ", "SRAMP","VSTOP","RADIO","NGATE","SHMOD" },
    { "RPT2", "RPT6", "SHUF", "Q6/8", "PUNCH","FILL", "BUILD","BALL",  "LPF",  "HPF",  "SQUSH","SCRTC","SCRTF","GLCH", "BLIND","S&H" },
    { "DELAY","PNGPG","FADE", "FBACK","DRONE","FREEZ","UNI",  "UNILO", "DETUN","PLFO", "BND+", "BND-", "WARBL","5TH",  "SUB",  "LOOP" },
    { "GATE4","GATE8","GAT16","TRNCE","TREM", "PAN",  "PUMP", "SWELL", "NOISE","WIDE", "MONO", "BRITE","DARK", "DRIVE","SGATE","KILL" },
    { "RAT2", "RAT3", "RAT4", "RAT8", "STRUM","TRNC2","STAB", "RINGL", "RINGH","VIBR", "CHOP", "HALF", "DBL",  "GLCH", "REVG", "SIL" },
};

// ---- v2 helpers (L692-695, L717)
static inline float psin(float ph){
    float x = ph + ph - 1.f;
    return 4.f * x * (1.f - fabsf(x));
}
static uint32_t lcg_st = 0x2545F491u;
static uint32_t lcg()  { lcg_st=lcg_st*1664525u+1013904223u; return lcg_st; }
// random streams (file scope so pmorReseed() can restart them; defaults = the original fixed seeds)
static uint32_t arng = 0x12345678u;   // audio-rate noise rng
static uint32_t rng4 = 0xC0FFEEu;
static uint32_t rng5 = 0x1234abcdu;
void fxV2Reseed(uint32_t s) {
    lcg_st = 0x2545F491u; arng = 0x12345678u; rng4 = 0xC0FFEEu; rng5 = 0x1234abcdu;
    if (s) { lcg_st ^= s * 2654435761u; arng ^= s * 2246822519u; rng4 ^= s * 3266489917u; rng5 ^= s * 668265263u; }
}

// ---- v2 buffers (L945-995), SDRAM
#define RBUF_N 96000u
static float SDRAM_BSS rbufL[RBUF_N];
static float SDRAM_BSS rbufR[RBUF_N];
#define DUB_N 32768u
#define CMB_N 4096u
static float SDRAM_BSS f2dub[DUB_N];
static float SDRAM_BSS f2cmb[CMB_N];
#define F3PLT_N 24000u
#define F3PNG_N 96000u
#define F3SLP_N 12000u
static float SDRAM_BSS f3plt[F3PLT_N];
static float SDRAM_BSS f3pngL[F3PNG_N];
static float SDRAM_BSS f3pngR[F3PNG_N];
static float SDRAM_BSS f3slap[F3SLP_N];
#define F4REP_N 48000u
#define F4MTL_N 4096u
static float SDRAM_BSS f4rep[F4REP_N];
static float SDRAM_BSS f4mtl[F4MTL_N];
static float SDRAM_BSS f5repL[24000];
static float SDRAM_BSS f5repR[24000];
static float SDRAM_BSS f5scrL[48000];
static float SDRAM_BSS f5scrR[48000];
static float SDRAM_BSS f7dl[2048];
static float SDRAM_BSS f6dL[48000];
static float SDRAM_BSS f6dR[48000];
static float SDRAM_BSS f6ppL[48000];
static float SDRAM_BSS f6ppR[48000];
static float SDRAM_BSS f6echo[24000];
static float SDRAM_BSS f6loopL[96000];
static float SDRAM_BSS f6loopR[96000];
static float SDRAM_BSS f6pchL[12000];
static float SDRAM_BSS f6pchR[12000];
static float SDRAM_BSS f5rpt2[6000];
static float SDRAM_BSS f5fill[4000];
static float SDRAM_BSS f5bnc[8000];
static float SDRAM_BSS f6fb[4096];
static float SDRAM_BSS f6dtL[2048];
static float SDRAM_BSS f6dtR[2048];
static float SDRAM_BSS f6plfo[4096];
static float SDRAM_BSS f6bu[4096];
static float SDRAM_BSS f6bd[4096];
static float SDRAM_BSS f6wrb[4096];
static float SDRAM_BSS xcL[4096], xcR[4096];   // FX1 CHOR (v2: ISR-local statics; SDRAM here, zeroed in fxV2Init)
static float SDRAM_BSS fbuf[1024];             // FX2 FLANG

static uint32_t rb_w = 0;         // capture ring write head (v2 rb_w)
static uint32_t gstep_blk = 0xFFFFFFFFu, gstep_samp = 0;   // v2 grid-locked sub-step position

void fxV2Init() {
    memset(rbufL, 0, sizeof rbufL);  memset(rbufR, 0, sizeof rbufR);
    memset(f2dub, 0, sizeof f2dub);  memset(f2cmb, 0, sizeof f2cmb);
    memset(f3plt, 0, sizeof f3plt);  memset(f3pngL, 0, sizeof f3pngL); memset(f3pngR, 0, sizeof f3pngR);
    memset(f3slap, 0, sizeof f3slap); memset(f4rep, 0, sizeof f4rep);  memset(f4mtl, 0, sizeof f4mtl);
    memset(f5repL, 0, sizeof f5repL); memset(f5repR, 0, sizeof f5repR);
    memset(f5scrL, 0, sizeof f5scrL); memset(f5scrR, 0, sizeof f5scrR);
    memset(f7dl, 0, sizeof f7dl);
    memset(f6dL, 0, sizeof f6dL);    memset(f6dR, 0, sizeof f6dR);
    memset(f6ppL, 0, sizeof f6ppL);  memset(f6ppR, 0, sizeof f6ppR);
    memset(f6echo, 0, sizeof f6echo);
    memset(f6loopL, 0, sizeof f6loopL); memset(f6loopR, 0, sizeof f6loopR);
    memset(f6pchL, 0, sizeof f6pchL); memset(f6pchR, 0, sizeof f6pchR);
    memset(f5rpt2, 0, sizeof f5rpt2); memset(f5fill, 0, sizeof f5fill); memset(f5bnc, 0, sizeof f5bnc);
    memset(f6fb, 0, sizeof f6fb);    memset(f6dtL, 0, sizeof f6dtL);  memset(f6dtR, 0, sizeof f6dtR);
    memset(f6plfo, 0, sizeof f6plfo);
    memset(f6bu, 0, sizeof f6bu); memset(f6bd, 0, sizeof f6bd); memset(f6wrb, 0, sizeof f6wrb);
    memset(xcL, 0, sizeof xcL); memset(xcR, 0, sizeof xcR); memset(fbuf, 0, sizeof fbuf);
    rb_w = 0; gstep_blk = 0xFFFFFFFFu; gstep_samp = 0;
}

uint32_t fxV2RingHead() { return rb_w; }

void fxV2Block(const FxV2Blk& b, float* L, float* R, size_t sz) {
    // ── punch-in FX state (v2 L2237-2253) ──
    static float    crL = 0.f, crR = 0.f;            // CRUSH sample-and-hold
    static uint32_t crn = 0;
    static bool     buf_prev = false;                // buffer-FX edge detect
    static uint32_t fz_anchor = 0;                   // stutter/reverse anchor
    static float    rev_pos = 0.f;                   // reverse read position
    static uint32_t xw = 0;
    static float    xlfo = 0.f, panph = 0.f;

    const uint16_t fh    = b.bank[0];
    const uint16_t fx_eff2 = b.bank[1], fx_eff3 = b.bank[2], fx_eff4 = b.bank[3], fx_eff5 = b.bank[4],
                   fx_eff6 = b.bank[5], fx_eff7 = b.bank[6], fx_eff8 = b.bank[7];
    const float    ffsr  = b.fsr;
    const float    fx_lid = b.lid;
    float fs[8], fl[8];   // per-bank macros: the live FSR / lidar, floored by the bank's own param (0 by default, so identical to v2)
    for(int k = 0; k < 8; k++){ fs[k] = ffsr > b.amt[k] ? ffsr : b.amt[k]; fl[k] = fx_lid > b.lidf[k] ? fx_lid : b.lidf[k]; }
    const uint32_t step_ms = b.step_ms;
    const int      gstep = (int)b.gstep;
    const uint32_t step_rw = b.step_rw;
    const bool     crush = ((fh >> FX_CRUSH) & 1u) != 0;
    const uint32_t cint  = 4u + (uint32_t)(fs[0] * 20.f);   // crush rate ← FSR
    const bool rev   = ((fh >> FX_REV) & 1u) != 0;
    const bool buffx = rev;
    if(buffx && !buf_prev){ fz_anchor = step_rw; rev_pos = 0.f; }
    buf_prev = buffx;
    uint32_t rw = rb_w;   // local ring head for this block
    if((uint32_t)gstep != gstep_blk){ gstep_samp = 0; gstep_blk = (uint32_t)gstep; }
    float gslen = (float)step_ms * 48.f; if(gslen < 1.f) gslen = 1.f;   // samples / 1/16
    for(size_t i=0;i<sz;i++){
        float oL = L[i], oR = R[i];   // (v2: the tanh'd engine mix — v3 hands the engine bus in here)

        // ── punch-in FX chain: reverse → chorus → pan → mute → crush ──
        if(!buffx) {
            // capture ring runs whenever reverse isn't holding it
            rbufL[rw] = oL; rbufR[rw] = oR;
            rw = (rw + 1u) % RBUF_N;
        }
        else {
            // REVERSE: clean backwards loop of the last beat at original
            // pitch; edges faded — no wrap clicks
            uint32_t le = step_ms * 192u;
            if(le > RBUF_N - 64u) le = RBUF_N - 64u;
            rev_pos += 1.f;                       // clean 1:1 reverse, no pitch
            while(rev_pos >= (float)le) rev_pos -= (float)le;
            uint32_t ri = (uint32_t)rev_pos;
            uint32_t rp = (fz_anchor + RBUF_N - 1u - ri) % RBUF_N;
            float g = 1.f;
            if(ri < 480u)            g = (float)ri / 480.f;
            else if(le - ri < 480u)  g = (float)(le - ri) / 480.f;
            oL = rbufL[rp] * g; oR = rbufR[rp] * g;
        }

        // CHOR: full-mix chorus with feedback — deep sweep, FSR = depth+regen
        if((fh >> FX_CHOR) & 1u) {
            xlfo += 0.35f / 48000.f; if(xlfo >= 1.f) xlfo -= 1.f;
            float depth = 0.45f + 0.55f * fs[0];   // deep even untouched
            float xlfo2 = xlfo + 0.25f; if(xlfo2 >= 1.f) xlfo2 -= 1.f;
            float tapL  = 960.f + 740.f * depth * psin(xlfo);
            float tapR  = 960.f + 740.f * depth * psin(xlfo2);
            uint32_t tiL = (uint32_t)tapL; float frL2 = tapL - (float)tiL;
            uint32_t tiR = (uint32_t)tapR; float frR2 = tapR - (float)tiR;
            float a0 = xcL[(xw - tiL) & 4095], a1 = xcL[(xw - tiL - 1u) & 4095];
            float b0 = xcR[(xw - tiR) & 4095], b1 = xcR[(xw - tiR - 1u) & 4095];
            float wxL = a0 + (a1 - a0) * frL2;
            float wxR = b0 + (b1 - b0) * frR2;
            float fb  = 0.55f + 0.42f * fs[0];     // high-feedback scream zone
            xcL[xw & 4095] = oL + wxL * fb;
            xcR[xw & 4095] = oR + wxR * fb;
            xw++;
            oL = oL * 0.5f + wxL * 0.9f;
            oR = oR * 0.5f + wxR * 0.9f;
        } else {
            xcL[xw & 4095] = oL; xcR[xw & 4095] = oR; xw++;   // keep line warm
        }

        // PAN: hard autopan, FSR = speed (0.5Hz → 8Hz)
        if((fh >> FX_PAN) & 1u) {
            panph += (0.5f + 7.5f * fs[0]) / 48000.f; if(panph >= 1.f) panph -= 1.f;
            float px = panph * 2.f - 1.f;
            float ps = 4.f * px * (1.f - fabsf(px));           // -1..1 parabolic sine
            oL *= (0.15f + 0.85f * 0.5f * (1.f + ps)) * 1.4f;
            oR *= (0.15f + 0.85f * 0.5f * (1.f - ps)) * 1.4f;
        }

        // MUT: clean full drop, short ramp so it never clicks
        {
            static float mg = 1.f;
            float mt = ((fh >> FX_MUTE) & 1u) ? 0.f : 1.f;
            mg += (mt - mg) * 0.004f;
            oL *= mg; oR *= mg;
        }
        // (DST / FILTER / CRUSH moved to the ordered master chain below RING)

        // ══ FX2 bank — SP404/OP-1-flavored, all cheap DSP, lidar on a few ══
        if(fx_eff2){
            const float mdL_ = oL, mdR_ = oR;   // bank wet/dry (b.mix[1]; 1 = untouched)
            const uint16_t h2 = fx_eff2;
            const float s = fs[1], lid = fl[1];
            // callback-local beat phase (samples since last grid step) for the
            // tempo-synced FX2 effects — tracks the master gstep
            static uint32_t f2_since = 0; static int f2_lastg = -1;
            if(f2_lastg != gstep){ f2_lastg = gstep; f2_since = 0; }
            // DUB — mono dub echo, FSR=feedback(runaway), lidar=time(80ms..1/4)
            static uint32_t dubw = 0; static float dubt = 9000.f;
            if(h2 & (1u<<F2_DUB)){
                float tt = 3840.f + lid * (float)(DUB_N - 5000u);   // ~80ms..680ms
                dubt += (tt - dubt) * 0.0004f;                       // smear on move
                uint32_t dl = (uint32_t)dubt; if(dl > DUB_N-2u) dl = DUB_N-2u;
                float e = f2dub[(dubw + DUB_N - dl) % DUB_N];
                float fb = 0.45f + s * 0.6f;                         // up to runaway
                float in = (oL + oR) * 0.5f;
                float wr = in + e * fb; wr = tanhf(wr * 0.8f) * 1.25f;
                f2dub[dubw] = wr; dubw = (dubw + 1u) % DUB_N;
                oL += e * 0.9f; oR += e * 0.9f;
            } else { f2dub[dubw] = (oL+oR)*0.5f; dubw = (dubw+1u)%DUB_N; }
            // GVERB — quick comb-bank reverb gated by FSR hold length
            static const uint16_t GVL[4]={1201,1291,1487,1693};
            static uint16_t gvw = 0; static float genv = 0.f;
            if(h2 & (1u<<F2_GVRB)){
                float in = (oL+oR)*0.5f, rv = 0.f;
                for(int c=0;c<4;c++){ float r = f2cmb[(gvw + CMB_N - GVL[c]) % CMB_N];
                    rv += r; }
                rv *= 0.25f;
                f2cmb[gvw] = in + rv * 0.72f; gvw = (gvw+1u)%CMB_N;
                genv = (rv*rv > genv) ? rv*rv : genv * (0.99988f - s*0.0003f); // gate hold←FSR
                float g = (genv > 0.00005f) ? 1.f : 0.f;
                oL = oL*0.4f + rv*g*1.6f; oR = oR*0.4f + rv*g*1.6f;
            }
            // TGATE — tempo trance gate (16 steps), FSR = depth + double-time
            if(h2 & (1u<<F2_TGATE)){
                static const uint16_t PAT = 0xB6D6; // x.xx x.x. xx.x x.x.
                int st = (s > 0.6f) ? ((gstep*2 + (int)(f2_since/(step_ms*24+1))) & 15)
                                    : (gstep & 15);
                float on = ((PAT >> st) & 1) ? 1.f : (0.6f - s*0.6f);
                oL *= on; oR *= on;
            }
            // FLANG — flanger, FSR biased to FAST/short (jet sweep)
            static uint16_t fw = 0; static float flf = 0.f;
            if(h2 & (1u<<F2_FLANG)){
                float ctr = 6.f - 5.4f*s;                       // 6ms..0.6ms (squeeze=short)
                float rate = 0.15f + 6.f * (s*s);               // exp bias to fast
                flf += rate / 48000.f; if(flf>=1.f) flf-=1.f;
                float lfo = 0.5f - 0.5f*cosf(flf*6.2831853f);
                float dms = (ctr + (0.5f+4.5f*s)*lfo) * 48.f;   // samples
                float in = (oL+oR)*0.5f;
                float rd = (float)fw - dms; while(rd<0) rd += 1024.f;
                int ri=(int)rd; float fr=rd-ri;
                float a=fbuf[ri&1023], b=fbuf[(ri+1)&1023];
                float wet=a+(b-a)*fr;
                fbuf[fw]=in + wet*0.7f; fw=(fw+1u)&1023;
                oL = oL*0.5f + wet; oR = oR*0.5f + wet;
            } else { fbuf[fw]=(oL+oR)*0.5f; fw=(fw+1u)&1023; }
            // RING — moved to the ordered master chain (ring → dist → filter → crush)
            // FOLD — wavefolder, FSR=fold gain (bold harmonics, cheap)
            if(h2 & (1u<<F2_FOLD)){
                float g = 1.f + s*6.f;
                oL = sinf(oL*g*1.5707f)*0.8f; oR = sinf(oR*g*1.5707f)*0.8f;
            }
            // COMB — tuned resonator, lidar=pitch(scale), FSR=feedback
            static uint16_t cw2 = 0;
            if(h2 & (1u<<F2_COMB)){
                static const float PENT[8]={130.8f,146.8f,164.8f,196.f,220.f,261.6f,293.7f,329.6f};
                float cf = PENT[(int)(lid*7.99f)];
                uint32_t dl = (uint32_t)(48000.f/cf); if(dl>=CMB_N) dl=CMB_N-1;
                float r = f2cmb[(cw2 + CMB_N - dl) % CMB_N];
                float fb = 0.8f + s*0.19f;
                float in = (oL+oR)*0.5f;
                f2cmb[cw2] = in + r*fb; cw2 = (cw2+1u)%CMB_N;
                oL = oL*0.5f + r*0.7f; oR = oR*0.5f + r*0.7f;
            }
            // VOWEL — 2-bandpass formant morph U→A→I via FSR
            static float v1=0,v2=0,v1b=0,v2b=0;
            if(h2 & (1u<<F2_FORM)){
                float f1 = 300.f + s*700.f;       // 300..1000
                float f2f= 900.f + s*1700.f;      // 900..2600
                float in=(oL+oR)*0.5f;
                float c1=2.f*sinf(3.14159f*f1/48000.f), c2=2.f*sinf(3.14159f*f2f/48000.f);
                v1 += c1*(in - v1 - 0.2f*v1b); v1b += c1*v1;   // resonant bp #1 (state-var)
                v2 += c2*(in - v2 - 0.2f*v2b); v2b += c2*v2;
                float wet = (v1b + v2b)*0.9f;
                oL = oL*0.25f + wet; oR = oR*0.25f + wet;
            }
            // ISO trio — 2 one-pole crossovers → low/mid/high, kill the band
            if(h2 & ((1u<<F2_ISOLO)|(1u<<F2_ISOMD)|(1u<<F2_ISOHI))){
                static float xl=0, xh=0, xlR=0, xhR=0;
                const float a1=0.026f, a2=0.30f;        // ~200Hz, ~3kHz
                xl += a1*(oL-xl); xh += a2*(oL-xh);
                float lo=xl, hi=oL-xh, mid=xh-xl;
                xlR+=a1*(oR-xlR); xhR+=a2*(oR-xhR);
                float loR=xlR, hiR=oR-xhR, midR=xhR-xlR;
                float gl=1,gm=1,gh=1;
                if(h2&(1u<<F2_ISOLO)) gl = (1.f-s);     // squeeze = full kill
                if(h2&(1u<<F2_ISOMD)) gm = (1.f-s);
                if(h2&(1u<<F2_ISOHI)) gh = (1.f-s);
                oL = lo*gl + mid*gm + hi*gh;
                oR = loR*gl + midR*gm + hiR*gh;
            }
            // PHONE — telephone bandpass + grit, FSR tightens/crushes
            static float ph_h=0, ph_l=0, ph_hR=0, ph_lR=0;
            if(h2 & (1u<<F2_PHONE)){
                float la=0.30f - s*0.12f, ha=0.04f + s*0.04f;
                ph_h += ha*(oL-ph_h); float b=oL-ph_h; ph_l += la*(b-ph_l);
                ph_hR+= ha*(oR-ph_hR);float bR=oR-ph_hR;ph_lR+=la*(bR-ph_lR);
                float q=8.f - s*5.f;
                oL = (floorf(ph_l*q)/q)*1.4f; oR=(floorf(ph_lR*q)/q)*1.4f;
            }
            // GRAIN — granular freeze from the capture ring; FSR=size, lidar=scrub
            static uint32_t gpos=0; static uint32_t glen=4800;
            if(h2 & (1u<<F2_GRAIN)){
                glen = (uint32_t)(5760.f - s*5400.f); if(glen<240) glen=240;
                uint32_t base = (uint32_t)(lid * (RBUF_N-glen-2));
                gpos++; if(gpos>=glen) gpos=0;
                uint32_t rp = (base + gpos) % RBUF_N;
                float w = 0.5f-0.5f*cosf(6.2831853f*(float)gpos/(float)glen);
                oL = rbufL[rp]*w + oL*(1.f-w)*0.3f;
                oR = rbufR[rp]*w + oR*(1.f-w)*0.3f;
            }
            f2_since++;
            // WOW — vinyl wow/flutter + crackle, FSR=degradation
            static float wphs=0, wphf=0, wd=0;
            if(h2 & (1u<<F2_WOW)){
                wphs += 0.7f/48000.f; if(wphs>=1.f)wphs-=1.f;
                wphf += 8.f/48000.f;  if(wphf>=1.f)wphf-=1.f;
                float mod=(psin(wphs)*0.6f+psin(wphf)*0.4f)*s*0.4f;
                wd += (mod - wd)*0.5f;
                // tiny crackle
                arng=arng*1664525u+1013904223u;
                float crk = ((arng>>9)*(1.f/4194304.f)-1.f);
                crk = (fabsf(crk)>0.985f) ? crk*s*0.5f : 0.f;
                oL = oL*(1.f+wd)+crk; oR = oR*(1.f+wd)+crk;
            }
            // DOWN — sample-rate reducer (aliasing), FSR=destroy
            static float dhL=0,dhR=0; static uint32_t dhc=0;
            if(h2 & (1u<<F2_DOWN)){
                uint32_t hold = 2u + (uint32_t)(s*40.f);
                if(dhc==0){ dhL=oL; dhR=oR; }
                dhc=(dhc+1u)%hold; oL=dhL; oR=dhR;
            } else dhc=0;
            // SCAT — beat-repeat scatter from the ring (16 slices, FSR=chaos)
            if(h2 & (1u<<F2_SCAT)){
                uint32_t slice = (uint32_t)(step_ms*48.f);   // 16th in samples
                if(slice<480) slice=480; if(slice>RBUF_N/2) slice=RBUF_N/2;
                static uint32_t scbase=0; static int scg=-1;
                if(scg!=gstep){ scg=gstep;
                    if((lcg()%100) < (uint32_t)(20+s*70)) scbase=(lcg()%8)*slice; }
                uint32_t rp = (step_rw + RBUF_N - (scbase + (f2_since % slice))) % RBUF_N;
                float w = (s>0.05f)?1.f:0.f;
                oL = rbufL[rp]*w + oL*(1.f-w);
                oR = rbufR[rp]*w + oR*(1.f-w);
            }
            { const float mx_ = b.mix[1]; if(mx_ < 1.f){ oL = mdL_ + (oL - mdL_) * mx_; oR = mdR_ + (oR - mdR_) * mx_; } }
        }

        // ══ FX3 bank — MUSICAL / SPATIAL ══
        if(fx_eff3){
            const float mdL_ = oL, mdR_ = oR;   // bank wet/dry (b.mix[2]; 1 = untouched)
            const uint16_t h3 = fx_eff3;
            const float s = fs[2], lid = fl[2];
            static uint32_t f3_since = 0; static int f3_lastg = -1;
            if(f3_lastg != gstep){ f3_lastg = gstep; f3_since = 0; }
            static const uint16_t PLC[4]={1789,2143,2647,3119};
            static uint16_t plw=0; static uint16_t paw=0; static float plapbuf[512]={};
            if(h3 & (1u<<E3_PLATE)){
                float in=(oL+oR)*0.5f, rv=0.f; float dec=0.74f + s*0.21f;
                for(int c=0;c<4;c++) rv += f3plt[(plw+F3PLT_N-PLC[c])%F3PLT_N];
                rv*=0.25f; f3plt[plw]=in + rv*dec; plw=(plw+1u)%F3PLT_N;
                float ar=plapbuf[(paw+512-241)&511];
                float aw=rv + ar*0.5f; plapbuf[paw]=aw; paw=(paw+1u)&511;
                float wet=ar - aw*0.5f; oL=oL*0.5f + wet; oR=oR*0.5f + wet;
            }
            static uint16_t shw=0; static float shfb=0.f; static int shsgn=1;
            if(h3 & (1u<<E3_SHIM)){
                float in=(oL+oR)*0.5f;
                float t=f3plt[(shw+F3PLT_N-4799)%F3PLT_N];
                shsgn=-shsgn; float oct=fabsf(t)*shsgn;
                shfb = in + (t*0.6f + oct*(0.4f+s*0.45f));
                f3plt[shw]=tanhf(shfb*0.8f); shw=(shw+1u)%F3PLT_N;
                oL=oL*0.5f + t*0.8f; oR=oR*0.5f + t*0.8f;
            }
            static uint32_t pngw=0;
            if(h3 & (1u<<E3_PONG)){
                uint32_t dl=(uint32_t)(step_ms*48.f*1.5f);
                if(dl>=F3PNG_N) dl=F3PNG_N-1; if(dl<480) dl=480;
                float eL=f3pngR[(pngw+F3PNG_N-dl)%F3PNG_N];
                float eR=f3pngL[(pngw+F3PNG_N-dl)%F3PNG_N];
                float fb=0.35f + lid*0.6f;
                f3pngL[pngw]=tanhf(oL + eL*fb); f3pngR[pngw]=tanhf(oR + eR*fb);
                pngw=(pngw+1u)%F3PNG_N;
                float m=0.3f+s*0.6f; oL=oL*(1.f-m*0.5f)+eL*m; oR=oR*(1.f-m*0.5f)+eR*m;
            } else { f3pngL[pngw]=oL; f3pngR[pngw]=oR; pngw=(pngw+1u)%F3PNG_N; }
            static uint32_t slw=0;
            if(h3 & (1u<<E3_SLAP)){
                uint32_t dl=2880u+(uint32_t)(s*9000.f); if(dl>=F3SLP_N) dl=F3SLP_N-1;
                float e=f3slap[(slw+F3SLP_N-dl)%F3SLP_N]; float in=(oL+oR)*0.5f;
                f3slap[slw]=in + e*0.25f; slw=(slw+1u)%F3SLP_N;
                oL=oL+e*0.55f; oR=oR+e*0.55f;
            } else { f3slap[slw]=(oL+oR)*0.5f; slw=(slw+1u)%F3SLP_N; }
            static float phl[4]={},phr[4]={},php=0.f;
            if(h3 & (1u<<E3_PHAS)){
                float rate=0.08f + s*1.6f; php+=rate/48000.f; if(php>=1.f)php-=1.f;
                float g=0.15f + 0.7f*(0.5f-0.5f*cosf(php*6.2831853f));
                float xL=oL, xR=oR;
                for(int k=0;k<4;k++){ float yL=-g*xL+phl[k]; phl[k]=xL+g*yL; xL=yL;
                                      float yR=-g*xR+phr[k]; phr[k]=xR+g*yR; xR=yR; }
                oL=oL*0.5f+xL*0.5f; oR=oR*0.5f+xR*0.5f;
            }
            static float waf=0,wab=0,wafR=0,wabR=0,wenv=0.f;
            if(h3 & (1u<<E3_WAH)){
                float in=(oL+oR)*0.5f, a=fabsf(in);
                wenv += (a-wenv)*((a>wenv)?0.05f:0.002f);
                float fc=300.f + (wenv*(2.f+s*6.f))*3000.f; if(fc>5500.f)fc=5500.f;
                float f=2.f*sinf(3.14159f*fc/48000.f), q=0.30f;
                waf+=f*(in - waf - q*wab); wab+=f*waf;
                wafR+=f*(oR - wafR - q*wabR); wabR+=f*wafR;
                oL=oL*0.3f + wab*1.4f; oR=oR*0.3f + wabR*1.4f;
            }
            static float trp=0.f;
            if(h3 & (1u<<E3_TREM)){
                float div=(s>0.6f)?8.f:4.f;
                float rate=div/(step_ms*0.001f*16.f);
                trp+=rate/48000.f; if(trp>=1.f)trp-=1.f;
                float depth=0.4f+s*0.6f;
                float g=1.f - depth*(0.5f-0.5f*cosf(trp*6.2831853f));
                oL*=g; oR*=g;
            }
            static float hbuf[256]={}; static uint16_t hbw=0;
            if(h3 & (1u<<E3_HAAS)){
                hbuf[hbw]=oR; uint16_t d=(uint16_t)(8.f+s*22.f);
                float dr=hbuf[(hbw+256-d)&255]; hbw=(hbw+1u)&255;
                float m=(oL+oR)*0.5f;
                oL=m + (oL-m)*(1.f+s*0.6f); oR=m + (dr-m)*(1.f+s*0.6f);
            }
            static float vbuf[1024]={}; static uint16_t vbw=0; static float vbp=0.f;
            if(h3 & (1u<<E3_VIBR)){
                vbuf[vbw]=(oL+oR)*0.5f; vbp+=5.5f/48000.f; if(vbp>=1.f)vbp-=1.f;
                float dms=(12.f + (4.f+s*22.f)*(0.5f-0.5f*cosf(vbp*6.2831853f)));
                float rd=(float)vbw-dms; while(rd<0)rd+=1024.f;
                int ri=(int)rd; float fr=rd-ri;
                float a=vbuf[ri&1023], b=vbuf[(ri+1)&1023];
                vbw=(vbw+1u)&1023; float w=a+(b-a)*fr; oL=w; oR=w;
            } else { vbuf[vbw]=(oL+oR)*0.5f; vbw=(vbw+1u)&1023; }
            static float cbuf[1024]={}; static uint16_t cbw=0; static float cbp=0.f;
            if(h3 & (1u<<E3_DIM)){
                cbuf[cbw]=(oL+oR)*0.5f; cbp+=0.6f/48000.f; if(cbp>=1.f)cbp-=1.f;
                float l1=0.5f-0.5f*cosf(cbp*6.2831853f);
                float l2=0.5f-0.5f*cosf((cbp+0.25f)*6.2831853f);
                float d1=(14.f+(2.f+s*14.f)*l1), d2=(14.f+(2.f+s*14.f)*l2);
                float r1=(float)cbw-d1; while(r1<0)r1+=1024.f; int i1=(int)r1; float f1=r1-i1;
                float r2=(float)cbw-d2; while(r2<0)r2+=1024.f; int i2=(int)r2; float f2c=r2-i2;
                float w1=cbuf[i1&1023]+(cbuf[(i1+1)&1023]-cbuf[i1&1023])*f1;
                float w2=cbuf[i2&1023]+(cbuf[(i2+1)&1023]-cbuf[i2&1023])*f2c;
                cbw=(cbw+1u)&1023; oL=oL*0.6f + w1*0.7f; oR=oR*0.6f + w2*0.7f;
            } else { cbuf[cbw]=(oL+oR)*0.5f; cbw=(cbw+1u)&1023; }
            static float spa[3]={},spaR[3]={}; static uint16_t spw=0;
            if(h3 & (1u<<E3_SPRG)){
                float in=(oL+oR)*0.5f; float t=f3slap[(spw+F3SLP_N-2400)%F3SLP_N];
                float g=0.6f; float xL=in+t*(0.5f+s*0.4f), xR=xL;
                for(int k=0;k<3;k++){ float yL=-g*xL+spa[k]; spa[k]=xL+g*yL; xL=yL;
                                      float yR=-g*xR+spaR[k];spaR[k]=xR+g*yR; xR=yR; }
                f3slap[spw]=tanhf(xL*0.9f); spw=(spw+1u)%F3SLP_N;
                oL=oL*0.5f+xL*0.9f; oR=oR*0.5f+xR*0.9f;
            }
            static float sirp=0.f, sirm=0.f;
            if(h3 & (1u<<E3_SIREN)){
                sirm+=6.f/48000.f; if(sirm>=1.f)sirm-=1.f;
                float f=180.f + lid*900.f + psin(sirm)*60.f*s;
                sirp+=f/48000.f; if(sirp>=1.f)sirp-=1.f;
                float t=tanhf(psin(sirp)*2.2f); float lvl=0.25f+s*0.5f;
                oL=oL*0.7f + t*lvl; oR=oR*0.7f + t*lvl;
            }
            static float wlpL=0,wlpR=0;
            if(h3 & (1u<<E3_WARM)){
                float dr=1.f+s*4.f;
                float yL=tanhf(oL*dr)*(1.f/(1.f+s*1.5f));
                float yR=tanhf(oR*dr)*(1.f/(1.f+s*1.5f));
                wlpL+=0.45f*(yL-wlpL); wlpR+=0.45f*(yR-wlpR);
                oL=yL*0.5f+wlpL*0.5f; oR=yR*0.5f+wlpR*0.5f;
            }
            static float subLP=0.f, subph=0.f, subenv=0.f;
            if(h3 & (1u<<E3_SUB)){
                float in=(oL+oR)*0.5f; subLP+=0.05f*(in-subLP);
                float a=fabsf(subLP); subenv+=(a-subenv)*0.01f;
                subph+=0.0006f; if(subph>=1.f)subph-=1.f;
                float sub=psin(subph)*subenv*(2.f+s*3.f); oL+=sub; oR+=sub;
            }
            static float airL=0,airR=0;
            if(h3 & (1u<<E3_AIR)){
                airL+=0.6f*(oL-airL); float hL=oL-airL;
                airR+=0.6f*(oR-airR); float hR=oR-airR;
                oL+=tanhf(hL*3.f)*0.5f*(0.3f+s*0.9f);
                oR+=tanhf(hR*3.f)*0.5f*(0.3f+s*0.9f);
            }
            static float pumpenv=1.f;
            if(h3 & (1u<<E3_PUMP)){
                uint32_t beat=(uint32_t)(step_ms*48.f*4.f); if(beat<480)beat=480;
                uint32_t pos=(f3_since + (uint32_t)gstep*(uint32_t)(step_ms*48.f))%beat;
                float t=(float)pos/(float)beat;
                float duck=1.f - (0.3f+s*0.65f)*expf(-t*6.f);
                pumpenv+=(duck-pumpenv)*0.05f; oL*=pumpenv; oR*=pumpenv;
            }
            f3_since++;
            { const float mx_ = b.mix[2]; if(mx_ < 1.f){ oL = mdL_ + (oL - mdL_) * mx_; oR = mdR_ + (oR - mdR_) * mx_; } }
        }

        // ══ FX4 bank — EXPERIMENTAL / GLITCH / PITCH ══
        if(fx_eff4){
            const float mdL_ = oL, mdR_ = oR;   // bank wet/dry (b.mix[3]; 1 = untouched)
            const uint16_t h4 = fx_eff4;
            const float s = fs[3];
            static uint32_t f4_since = 0; static int f4_lastg = -1;
            if(f4_lastg != gstep){ f4_lastg = gstep; f4_since = 0; }

            static float ouL=0,ouR=0;
            if(h4 & (1u<<E4_OCTU)){
                float rL=fabsf(oL)*2.f-0.5f, rR=fabsf(oR)*2.f-0.5f;
                ouL+=0.002f*(rL-ouL); ouR+=0.002f*(rR-ouR);
                oL=oL*(1.f-s)+(rL-ouL)*s*1.2f; oR=oR*(1.f-s)+(rR-ouR)*s*1.2f;
            }
            static int odpL=0,odpR=0,odfL=1,odfR=1;
            if(h4 & (1u<<E4_OCTD)){
                int sL=(oL>0.f); if(sL!=odpL){ odpL=sL; if(sL)odfL=-odfL; }
                int sR=(oR>0.f); if(sR!=odpR){ odpR=sR; if(sR)odfR=-odfR; }
                oL=oL*(1.f-s)+fabsf(oL)*odfL*s*1.3f; oR=oR*(1.f-s)+fabsf(oR)*odfR*s*1.3f;
            }
            static float dzL=0,dzR=0; static uint32_t dzc=0;
            if(h4 & (1u<<E4_DSTR)){
                uint32_t hold=1u+(uint32_t)(s*30.f);
                if(dzc==0){ dzL=oL; dzR=oR; } dzc=(dzc+1u)%hold;
                float q=powf(2.f, 8.f - s*7.f); oL=floorf(dzL*q)/q; oR=floorf(dzR*q)/q;
            } else dzc=0;
            static uint32_t rpw=0;
            if(h4 & (1u<<E4_REPT)){
                uint32_t slc=(uint32_t)(step_ms*48.f*0.5f);
                if(slc<240)slc=240; if(slc>F4REP_N/2)slc=F4REP_N/2;
                static uint32_t rpbase=0; static int rpg=-1;
                if(rpg!=gstep){ rpg=gstep; rng4=rng4*1664525u+1013904223u;
                    if((rng4>>24)<(uint32_t)(40+s*200)) rpbase=((rng4>>8)%4u)*slc; }
                uint32_t rd=(rpw+F4REP_N-(rpbase+(f4_since%slc)))%F4REP_N;
                oL=f4rep[rd]; oR=f4rep[rd];
            }
            f4rep[rpw]=(oL+oR)*0.5f; rpw=(rpw+1u)%F4REP_N;
            static uint32_t rgp=0; static uint32_t rglen=4800;
            if(h4 & (1u<<E4_RGRN)){
                rglen=(uint32_t)(6000.f-s*5500.f); if(rglen<300)rglen=300;
                rgp++; if(rgp>=rglen)rgp=0;
                uint32_t rd=(rpw+F4REP_N-rgp)%F4REP_N;
                float w=0.5f-0.5f*cosf(6.2831853f*(float)rgp/(float)rglen);
                oL=f4rep[rd]*w + oL*(1.f-w)*0.3f; oR=f4rep[rd]*w + oR*(1.f-w)*0.3f;
            }
            static uint32_t fzp=0; static uint32_t fzlen=2400; static bool fzcap=false;
            if(h4 & (1u<<E4_FRZ)){
                if(!fzcap){ fzcap=true; fzp=0; }
                fzlen=600u+(uint32_t)((1.f-s)*4800.f);
                uint32_t rd=(rpw+F4REP_N-fzlen+(fzp%fzlen))%F4REP_N; fzp++;
                float w=0.5f-0.5f*cosf(6.2831853f*(float)(fzp%fzlen)/(float)fzlen);
                oL=f4rep[rd]*(0.6f+w*0.4f); oR=f4rep[rd]*(0.6f+w*0.4f);
            } else fzcap=false;
            static const uint16_t MTC[4]={149,179,211,251}; static uint16_t mtw=0;
            if(h4 & (1u<<E4_METL)){
                float in=(oL+oR)*0.5f, rv=0.f; float scale=0.6f+s*0.8f;
                for(int c=0;c<4;c++){ uint16_t d=(uint16_t)(MTC[c]*scale);
                    if(d>=F4MTL_N)d=F4MTL_N-1; rv+=f4mtl[(mtw+F4MTL_N-d)%F4MTL_N]; }
                rv*=0.25f; f4mtl[mtw]=in + rv*0.85f; mtw=(mtw+1u)%F4MTL_N;
                oL=oL*0.3f + rv*1.3f; oR=oR*0.3f + rv*1.3f;
            }
            static float qa=0,qb=0,qaR=0,qbR=0;
            if(h4 & (1u<<E4_FORM)){
                float sh=1.f+s*1.6f; float f1=500.f*sh, f2f=1500.f*sh;
                if(f2f>6000.f)f2f=6000.f; float in=(oL+oR)*0.5f;
                float c1=2.f*sinf(3.14159f*f1/48000.f), c2=2.f*sinf(3.14159f*f2f/48000.f);
                qa+=c1*(in-qa-0.18f*qb); qb+=c1*qa;
                qaR+=c2*(in-qaR-0.18f*qbR); qbR+=c2*qaR;
                float wet=(qb+qbR)*0.9f; oL=oL*0.3f+wet; oR=oR*0.3f+wet;
            }
            static float fsp=0.f;
            if(h4 & (1u<<E4_FSHF)){
                float fc=20.f + s*s*900.f; fsp+=fc/48000.f; if(fsp>=1.f)fsp-=1.f;
                float c=psin(fsp), cq=psin(fsp+0.25f);
                float wL=oL*c - oR*cq, wR=oR*c + oL*cq;
                oL=oL*0.3f + wL*0.9f; oR=oR*0.3f + wR*0.9f;
            }
            if(h4 & (1u<<E4_FUZZ)){
                float g=2.f+s*18.f; float yL=oL*g+0.15f, yR=oR*g+0.15f;
                yL=(yL>0.f)?(1.f-expf(-yL)):(-1.f+expf(yL));
                yR=(yR>0.f)?(1.f-expf(-yR)):(-1.f+expf(yR));
                oL=yL*0.6f; oR=yR*0.6f;
            }
            static uint16_t gpat=0xACE1u; static int gseqg=-1;
            if(h4 & (1u<<E4_GSEQ)){
                if(gseqg!=gstep && gstep==0){ rng4=rng4*1664525u+1013904223u; gpat=(uint16_t)(rng4>>8); }
                gseqg=gstep; int thr=(int)(s*16.f);
                float on=(((gpat>>(gstep&15))&1) || ((gstep&3)<thr)) ? 1.f : 0.f;
                oL*=on; oR*=on;
            }
            static uint32_t srbase=0; static int srg=-1; static float srrd=0.f;
            if(h4 & (1u<<E4_SRMP)){
                uint32_t slc=(uint32_t)(step_ms*48.f*0.5f); if(slc<480)slc=480;
                if(srg!=gstep){ srg=gstep; srbase=(rpw+F4REP_N-slc)%F4REP_N; srrd=0.f; }
                float spd=1.f + s*1.5f*(f4_since/(float)(step_ms*48.f+1.f));
                srrd+=spd; if(srrd>=(float)slc)srrd-=(float)slc;
                uint32_t rd=(srbase+(uint32_t)srrd)%F4REP_N; oL=f4rep[rd]; oR=f4rep[rd];
            }
            static float vsrd=0.f; static bool vsinit=false;
            if(h4 & (1u<<E4_VSTP)){
                if(!vsinit){ vsinit=true; vsrd=0.f; }
                float spd=0.4f + s*2.5f; vsrd+=spd; float back=vsrd;
                if(back>F4REP_N-2)back=F4REP_N-2;
                uint32_t rd=(rpw+F4REP_N-(uint32_t)back)%F4REP_N; float damp=1.f - s*0.5f;
                oL=f4rep[rd]*damp; oR=f4rep[rd]*damp;
            } else vsinit=false;
            static float rdh=0,rdl=0,rdhR=0,rdlR=0,rdcar=0.f;
            if(h4 & (1u<<E4_RADIO)){
                float la=0.35f, ha=0.05f;
                rdh+=ha*(oL-rdh); float bL=oL-rdh; rdl+=la*(bL-rdl);
                rdhR+=ha*(oR-rdhR);float bR=oR-rdhR;rdlR+=la*(bR-rdlR);
                rdcar+=(60.f + s*200.f)/48000.f; if(rdcar>=1.f)rdcar-=1.f;
                float am=0.6f+0.4f*psin(rdcar);
                rng4=rng4*1664525u+1013904223u;
                float hiss=((rng4>>9)*(1.f/4194304.f)-1.f)*s*0.15f;
                oL=rdl*am*1.6f + hiss; oR=rdlR*am*1.6f + hiss;
            }
            static float ngenv=0.f;
            if(h4 & (1u<<E4_NGAT)){
                float a=fabsf(oL)+fabsf(oR);
                ngenv+=(a-ngenv)*((a>ngenv)?0.4f:0.004f);
                float g=(ngenv>(0.02f + s*0.5f))?1.f:0.f; oL*=g; oR*=g;
            }
            static float shval=0.f, shlp=0.f; static uint32_t shc=0;
            if(h4 & (1u<<E4_SHMD)){
                uint32_t rate=2400u-(uint32_t)(s*2200.f); if(rate<60)rate=60;
                if(shc==0){ rng4=rng4*1664525u+1013904223u; shval=(rng4>>9)*(1.f/4194304.f)-1.f; }
                shc=(shc+1u)%rate; shlp+=0.02f*(shval-shlp);
                float mod=0.5f+0.5f*shlp; oL*=mod; oR*=mod;
            } else shc=0;
            f4_since++;
            { const float mx_ = b.mix[3]; if(mx_ < 1.f){ oL = mdL_ + (oL - mdL_) * mx_; oR = mdR_ + (oR - mdR_) * mx_; } }
        }

        // ══ FX5 — POCKET OPERATOR punch-ins ══
        if(fx_eff5){
            const float mdL_ = oL, mdR_ = oR;   // bank wet/dry (b.mix[4]; 1 = untouched)
            const uint16_t h5 = fx_eff5; const float s = fs[4], lid = fl[4];
            static uint32_t f5_since=0; static int f5_lastg=-1;
            if(f5_lastg!=gstep){ f5_lastg=gstep; f5_since=0; }

            if(h5 & (1u<<E5_REPEAT2)){            // 1/2-step beat-slice repeat
                static int wp=0; float* buf=f5rpt2;
                int slc=(int)(step_ms*48*0.5f*(0.4f+0.6f*s)); if(slc<64)slc=64; if(slc>5999)slc=5999;
                buf[wp]=oL; int rp=wp-(int)(f5_since%slc); if(rp<0)rp+=6000;
                oL=oR=buf[rp]; if(++wp>=6000)wp=0;
            }
            if(h5 & (1u<<E5_REPEAT6)){            // ~3/4-step beat repeat
                static int wp=0; uint32_t len=(uint32_t)(step_ms*48*(0.5f+1.0f*s));
                if(len<128)len=128; if(len>23999)len=23999;
                f5repL[wp]=oL; f5repR[wp]=oR; uint32_t ph=f5_since%len; int rp=(int)wp-(int)ph; if(rp<0)rp+=24000;
                oL=f5repL[rp]; oR=f5repR[rp]; if((uint32_t)(++wp)>=24000)wp=0;
            }
            if(h5 & (1u<<E5_NOTESHUF)){           // offbeat swing gate
                uint32_t sw=(uint32_t)(step_ms*48u*0.33f*s);
                float g=(f5_since<sw && (gstep&1))?0.f:1.f;
                static float env=1.f; env+=(g-env)*0.01f; oL*=env; oR*=env;
            }
            if(h5 & (1u<<E5_QUANT68)){            // triplet tremolo-gate
                static float ph=0.f; float rate=3.f/(float)(step_ms*0.001f*2.f);
                ph+=rate/48000.f; if(ph>=1.f)ph-=1.f;
                float g=0.5f+0.5f*psin(ph); g=1.f-s*(1.f-g); oL*=g; oR*=g;
            }
            if(h5 & (1u<<E5_PUNCH)){              // transient pump
                static float env=0.f; float in=fabsf(oL)+fabsf(oR); env+=(in-env)*((in>env)?0.5f:0.002f);
                float g=1.f+s*2.0f*(1.f/(1.f+8.f*env)); oL=tanhf(oL*g); oR=tanhf(oR*g);
            }
            if(h5 & (1u<<E5_FILLIN)){             // accelerating retrigger burst
                static int wp=0; float* buf=f5fill;
                int slc=(int)(step_ms*48*0.25f*(1.0f-0.7f*s)); if(slc<48)slc=48; if(slc>3999)slc=3999;
                buf[wp]=oL; int rp=wp-(int)(f5_since%slc); if(rp<0)rp+=4000; oL=oR=buf[rp]; if(++wp>=4000)wp=0;
            }
            if(h5 & (1u<<E5_BUILDUP)){            // riser, lidar = position
                static float hp=0.f; float fc=0.02f+0.45f*lid; hp+=(oL-hp)*fc; float h=oL-hp;
                oL=oR=h; static float ph=0.f; ph+=(4.f+lid*40.f)/48000.f; if(ph>=1.f)ph-=1.f;
                float g=(ph<(0.5f+0.5f*lid))?1.f:0.2f; oL*=g; oR*=g;
            }
            if(h5 & (1u<<E5_BOUNCE)){             // accelerating decaying retrigger
                static int wp=0; static float t=0.f; static float per=4000.f; float* buf=f5bnc;
                buf[wp]=oL; t+=1.f; if(t>=per){t=0.f; per*=0.7f; if(per<400.f)per=4000.f*(0.5f+0.5f*s);}
                int rp=wp-(int)t; if(rp<0)rp+=8000; float dec=1.f-(t/per)*0.8f; oL=oR=buf[rp]*dec; if(++wp>=8000)wp=0;
            }
            if(h5 & (1u<<E5_LPFIX)){              // fixed lowpass kill
                static float z1=0,z2=0; float fc=0.03f+0.5f*(1.f-s); z1+=fc*(oL-z1); z2+=fc*(z1-z2); oL=z2; oR=z2;
            }
            if(h5 & (1u<<E5_HPFIX)){              // fixed highpass kill
                static float z=0; float fc=0.02f+0.4f*s; z+=fc*(oL-z); oL=oR=oL-z;
            }
            if(h5 & (1u<<E5_SQUASH)){             // limiter/clip
                float g=1.f+s*10.f; float k=0.7f/(0.3f+s); oL=tanhf(oL*g)*k; oR=tanhf(oR*g)*k;
            }
            if(h5 & (1u<<E5_SCRATCH)){            // vinyl jog
                static int wp=0; static float rp=0.f; static float ph=0.f;
                f5scrL[wp]=oL; f5scrR[wp]=oR; ph+=(2.f+s*4.f)/480.f; if(ph>=1.f)ph-=1.f;
                rp+=psin(ph)*60.f*(0.5f+s); float rr=wp-rp; while(rr<0)rr+=48000; while(rr>=48000)rr-=48000;
                int ri=(int)rr; oL=f5scrL[ri]; oR=f5scrR[ri]; if(++wp>=48000)wp=0;
            }
            if(h5 & (1u<<E5_SCRTFAST)){           // faster jog
                static float rp=0.f; static float ph=0.f; static int wp2=0;
                f5scrL[wp2]=oL; ph+=(8.f+s*16.f)/480.f; if(ph>=1.f)ph-=1.f;
                rp+=psin(ph)*40.f; float rr=wp2-rp; while(rr<0)rr+=48000; while(rr>=48000)rr-=48000;
                oL=oR=f5scrL[(int)rr]; if(++wp2>=48000)wp2=0;
            }
            if(h5 & (1u<<E5_GLITCH)){             // random slice repeat
                static int wp=0; static int hold=0; static int len=2000;
                f5scrR[wp]=oL;
                if(--hold<=0){ rng5=rng5*1664525u+1013904223u;
                    if(((rng5>>20)&255)<(int)(s*255)){ len=300+(int)((rng5>>8)&8191); hold=len*2; } else hold=200; }
                if(hold>0 && len){ int rp=wp-(int)(f5_since%len); if(rp<0)rp+=48000; oL=oR=f5scrR[rp]; }
                if(++wp>=48000)wp=0;
            }
            if(h5 & (1u<<E5_BLINDS)){             // hard shutter tremolo
                static float ph=0.f; ph+=(8.f+s*60.f)/48000.f; if(ph>=1.f)ph-=1.f;
                float g=(ph<0.5f)?1.f:0.f; oL*=g; oR*=g;
            }
            if(h5 & (1u<<E5_SAMPHLD)){            // random-rate sample & hold
                static float hL=0,hR=0; static int c=0; static int per=200;
                if(--c<=0){ rng5=rng5*1664525u+1013904223u; per=20+(int)((1.f-s)*1500); c=per; hL=oL; hR=oR; }
                oL=hL; oR=hR;
            }
            f5_since++;
            { const float mx_ = b.mix[4]; if(mx_ < 1.f){ oL = mdL_ + (oL - mdL_) * mx_; oR = mdR_ + (oR - mdR_) * mx_; } }
        }

        // ══ FX6 — DELAY / SPACE / PITCH ══
        if(fx_eff6){
            const float mdL_ = oL, mdR_ = oR;   // bank wet/dry (b.mix[5]; 1 = untouched)
            const uint16_t h6 = fx_eff6; const float s = fs[5], lid = fl[5];
            if(h6 & (1u<<E6_DELAY)){             // tempo 1/8 delay, S=feedback
                static int wp=0; int dt=(int)(step_ms*48*2); if(dt<1)dt=1; if(dt>47999)dt=47999;
                int rp=wp-dt; if(rp<0)rp+=48000; float dl=f6dL[rp], dr=f6dR[rp]; float fb=0.2f+0.55f*s;
                f6dL[wp]=tanhf(oL+dl*fb); f6dR[wp]=tanhf(oR+dr*fb); oL+=dl*0.6f; oR+=dr*0.6f; if(++wp>=48000)wp=0;
            }
            if(h6 & (1u<<E6_PINGPONG)){          // cross-fb stereo delay
                static int wp=0; int dt=(int)(step_ms*48*2); if(dt<1)dt=1; if(dt>47999)dt=47999;
                int rp=wp-dt; if(rp<0)rp+=48000; float dl=f6ppL[rp], dr=f6ppR[rp]; float fb=0.3f+0.55f*s;
                f6ppL[wp]=tanhf(oL+dr*fb); f6ppR[wp]=tanhf(oR+dl*fb); oL+=dl*0.6f; oR+=dr*0.6f; if(++wp>=48000)wp=0;
            }
            if(h6 & (1u<<E6_ECHOFADE)){          // feedback ramps up while held
                static int wp=0; static float fb=0.f; fb+=(0.85f-fb)*0.0008f;
                int dt=(int)(step_ms*48*(1.f+s*2.f)); if(dt<1)dt=1; if(dt>23999)dt=23999;
                int rp=wp-dt; if(rp<0)rp+=24000; float d=f6echo[rp];
                f6echo[wp]=tanhf(oL+d*fb); oL+=d*0.6f; oR+=d*0.6f; if(++wp>=24000)wp=0;
            }
            if(h6 & (1u<<E6_FEEDBACK)){          // resonant short-loop howl
                static int wp=0; float* buf=f6fb; int dt=200+(int)((1.f-s)*1400);
                int rp=wp-dt; if(rp<0)rp+=4096; float d=buf[rp&4095]; float fb=0.85f+0.13f*s;
                buf[wp]=tanhf(oL*0.5f+d*fb); oL=oR=oL*0.3f+d*0.8f; if(++wp>=4096)wp=0;
            }
            if(h6 & (1u<<E6_DRONE)){             // capture & loop a bed, lid=mix
                static int wp=0; static int frozen=0; static int len=12000;
                if(!frozen){ f6loopL[wp]=oL; f6loopR[wp]=oR; if(++wp>=len){frozen=1;wp=0;} }
                else { float gL=f6loopL[wp],gR=f6loopR[wp]; if(++wp>=len)wp=0;
                       oL=oL*(1.f-lid)+gL*lid; oR=oR*(1.f-lid)+gR*lid; }
            }
            if(h6 & (1u<<E6_FREEZE)){            // hold a slice, S=freeze
                static int a=0; int la=8000; int ra=a-la; if(ra<0)ra+=24000;
                float fa=f6echo[ra]; float fb=0.9f+0.099f*s;
                f6echo[a]=tanhf((oL+oR)*0.5f*(1.f-s)+fa*fb);
                oL=oL*(1.f-s)+fa*s; oR=oR*(1.f-s)+fa*s; if(++a>=24000)a=0;
            }
            if(h6 & (1u<<E6_UNISON)){            // detuned doubling up
                static int wp=0; static float rp=0.f; f6pchL[wp]=oL; f6pchR[wp]=oR;
                rp+=1.f+0.012f*s; if(rp>=6000.f)rp-=6000.f; int rd=wp-(int)rp; if(rd<0)rd+=12000;
                oL=oL*0.6f+f6pchL[rd]*0.6f; oR=oR*0.6f+f6pchR[rd]*0.6f; if(++wp>=12000)wp=0;
            }
            if(h6 & (1u<<E6_UNILOW)){            // detuned doubling down
                static int wp=0; static float rp=0.f; f6pchR[wp]=oL;
                rp+=1.f-0.02f*s; if(rp<0.f)rp+=6000.f; if(rp>=6000.f)rp-=6000.f; int rd=wp-(int)rp; if(rd<0)rd+=12000;
                oL=oL*0.6f+f6pchR[rd]*0.6f; oR=oR*0.6f+f6pchR[rd]*0.6f; if(++wp>=12000)wp=0;
            }
            if(h6 & (1u<<E6_DETUNE)){            // 2-voice chorus-detune
                static int wp=0; static float p1=0,p2=0; float* fl=f6dtL; float* fr=f6dtR;
                p1+=0.7f/48000.f; p2+=0.9f/48000.f; if(p1>=1.f)p1-=1.f; if(p2>=1.f)p2-=1.f;
                fl[wp&2047]=oL; fr[wp&2047]=oR; float d1=300.f+250.f*s*psin(p1), d2=300.f+250.f*s*psin(p2);
                int a=(wp-(int)d1)&2047, b=(wp-(int)d2)&2047;
                oL=oL*0.5f+fl[a]*0.5f; oR=oR*0.5f+fr[b]*0.5f; if(++wp>=2048)wp=0;
            }
            if(h6 & (1u<<E6_PITCHLFO)){          // vibrato wobble
                static int wp=0; static float ph=0.f; float* buf=f6plfo;
                ph+=6.f/48000.f; if(ph>=1.f)ph-=1.f; buf[wp&4095]=oL;
                float d=1000.f+900.f*s*psin(ph); int ip=(int)d; float fr=d-ip;
                int r0=(wp-ip)&4095, r1=(wp-ip-1)&4095; oL=oR=buf[r0]+(buf[r1]-buf[r0])*fr; if(++wp>=4096)wp=0;
            }
            if(h6 & (1u<<E6_BENDUP)){            // pitch glide up while held
                static int wp=0; static float spd=1.f; static float rp=0.f; float* buf=f6bu;
                spd+=((1.f+s)-spd)*0.0006f; buf[wp&4095]=oL; rp+=spd; if(rp>=4000.f)rp-=4000.f;
                int rd=(wp-(int)rp)&4095; oL=oR=buf[rd]; if(++wp>=4096)wp=0;
            }
            if(h6 & (1u<<E6_BENDDN)){            // pitch dive while held
                static int wp=0; static float spd=1.f; static float rp=0.f; float* buf=f6bd;
                spd+=((1.f-0.6f*s)-spd)*0.0006f; buf[wp&4095]=oL; rp+=spd; if(rp<0.f)rp+=4000.f; if(rp>=4000.f)rp-=4000.f;
                int rd=(wp-(int)rp)&4095; oL=oR=buf[rd]; if(++wp>=4096)wp=0;
            }
            if(h6 & (1u<<E6_WARBLE)){            // fast tape flutter
                static int wp=0; static float ph=0.f; float* buf=f6wrb;
                ph+=(7.f+s*12.f)/48000.f; if(ph>=1.f)ph-=1.f; buf[wp&4095]=oL;
                float d=800.f+700.f*psin(ph)*s; int ip=(int)d; float fr=d-ip;
                int r0=(wp-ip)&4095,r1=(wp-ip-1)&4095; oL=oR=buf[r0]+(buf[r1]-buf[r0])*fr; if(++wp>=4096)wp=0;
            }
            if(h6 & (1u<<E6_HARM5)){             // +shimmer harmony grain
                static int wp=0; static float rp=0.f; f6pchL[wp&8191]=oL;
                rp+=0.5f; if(rp>=3000.f)rp-=3000.f; int rd=(wp-(int)rp)&8191;
                float fifth=f6pchL[rd]; oL=oL*(1.f-s*0.5f)+fifth*s*0.7f; oR=oL; if(++wp>=8192)wp=0;
            }
            if(h6 & (1u<<E6_SUBOCT)){            // zero-cross sub generator
                static float prev=0.f; static float flip=1.f;
                if(prev<=0.f && oL>0.f) flip=-flip; prev=oL;
                float sub=flip*fabsf(oL); oL+=sub*s; oR+=sub*s;
            }
            if(h6 & (1u<<E6_LOOP)){              // capture & loop a bar, S=length
                static int pp=0; static int cap=1; static int len=48000;
                if(cap){ len=(int)(step_ms*48*16*(0.25f+0.75f*s)); if(len>96000)len=96000; if(len<2400)len=2400; cap=0; }
                f6loopL[pp]=oL; f6loopR[pp]=oR; oL=f6loopL[pp]; oR=f6loopR[pp]; if(++pp>=len)pp=0;
            }
            { const float mx_ = b.mix[5]; if(mx_ < 1.f){ oL = mdL_ + (oL - mdL_) * mx_; oR = mdR_ + (oR - mdR_) * mx_; } }
        }

        // ══ FX7 — LIVE / PERFORMANCE (grid gates, motion, dynamics) ══
        if(fx_eff7){
            const float mdL_ = oL, mdR_ = oR;   // bank wet/dry (b.mix[6]; 1 = untouched)
            const uint16_t h7 = fx_eff7; const float s = fs[6];
            float sl = gslen;                                   // samples per 1/16
            float gpos = (float)gstep*sl + (float)gstep_samp;   // grid-locked position
            // grid gate: N sixteenths per cycle, 50% duty, lightly smoothed. Phase
            // is derived from the playhead so the gate edges sit exactly on grid.
            #define GATE_N(env, N) { float ph=gpos/(sl*(N)); ph-=floorf(ph); \
                float gt=(ph<0.5f)?1.f:0.f; static float env=1.f; env+=(gt-env)*0.02f; oL*=env; oR*=env; }
            if(h7 & (1u<<E7_GATE4))  GATE_N(e7g4, 4.f)
            if(h7 & (1u<<E7_GATE8))  GATE_N(e7g8, 2.f)
            if(h7 & (1u<<E7_GATE16)) GATE_N(e7g16, 1.f)
            if(h7 & (1u<<E7_TRANCE)){            // 16-step trance gate, locked to gstep
                static const uint16_t P=0xB3A6; int st=gstep&15;
                float gt=((P>>st)&1)?1.f:0.f; static float e=1.f; e+=(gt-e)*0.03f; oL*=e; oR*=e;
            }
            if(h7 & (1u<<E7_TREM)){              // sine tremolo at 1/8
                static float ph=0.f; ph+=1.f/(sl*2.f); if(ph>=1.f)ph-=1.f;
                float d=0.4f+0.55f*s; float m=1.f-d*0.5f*(1.f+psin(ph)); oL*=m; oR*=m;
            }
            if(h7 & (1u<<E7_PAN)){               // auto-pan LFO at 1/4
                static float ph=0.f; ph+=1.f/(sl*4.f); if(ph>=1.f)ph-=1.f;
                float p=0.5f+0.5f*psin(ph)*(0.4f+0.6f*s); oL*=(1.f-p)*1.4f; oR*=p*1.4f;
            }
            if(h7 & (1u<<E7_PUMP)){              // sidechain duck on the beat
                float ph=gpos/(sl*4.f); ph-=floorf(ph);
                float duck=0.25f+0.75f*ph; oL*=duck; oR*=duck;
            }
            if(h7 & (1u<<E7_SWELL)){             // reverse swell into the beat
                float ph=gpos/(sl*4.f); ph-=floorf(ph);
                float sw=0.25f+0.75f*(1.f-ph); oL*=sw; oR*=sw;
            }
            if(h7 & (1u<<E7_NOISE)){             // vinyl noise overlay
                float n=((float)(lcg()&0xffff)-32768.f)/32768.f;
                float a=0.02f+0.06f*s; oL+=n*a; oR+=n*a;
            }
            if(h7 & (1u<<E7_WIDE)){              // Haas widener — delay R a touch
                static int wp=0; f7dl[wp&2047]=oR; int rd=(wp-360)&2047;
                oR=oR*0.4f+f7dl[rd]*0.7f; if(++wp>=2048)wp=0;
            }
            if(h7 & (1u<<E7_MONO)){ float m=(oL+oR)*0.5f; oL=m; oR=m; }
            if(h7 & (1u<<E7_TILTU)){             // bright tilt (add highs)
                static float lpL=0.f,lpR=0.f; lpL+=(oL-lpL)*0.25f; lpR+=(oR-lpR)*0.25f;
                oL+= (oL-lpL)*(0.6f+s); oR+=(oR-lpR)*(0.6f+s);
            }
            if(h7 & (1u<<E7_TILTD)){             // dark tilt (low-pass)
                static float lpL=0.f,lpR=0.f; float c=0.35f-0.25f*s;
                lpL+=(oL-lpL)*c; lpR+=(oR-lpR)*c; oL=lpL; oR=lpR;
            }
            if(h7 & (1u<<E7_DRIVE)){ float g=1.f+4.f*s; oL=tanhf(oL*g)*0.8f; oR=tanhf(oR*g)*0.8f; }
            if(h7 & (1u<<E7_SOFTG)){             // smooth pulsing gate at 1/8
                float ph=gpos/(sl*2.f); ph-=floorf(ph); float gt=(ph<0.5f)?1.f:0.f;
                static float e=1.f; e+=(gt-e)*0.006f; oL*=e; oR*=e;
            }
            if(h7 & (1u<<E7_KILL)){ oL=0.f; oR=0.f; }
            #undef GATE_N
            { const float mx_ = b.mix[6]; if(mx_ < 1.f){ oL = mdL_ + (oL - mdL_) * mx_; oR = mdR_ + (oR - mdR_) * mx_; } }
        }

        // ══ FX8 — MIDI / SEQUENCE FX (ratchets, ring, melodic gates) ══
        if(fx_eff8){
            const float mdL_ = oL, mdR_ = oR;   // bank wet/dry (b.mix[7]; 1 = untouched)
            const uint16_t h8 = fx_eff8; const float s = fs[7];
            float sl = gslen;                                   // samples per 1/16
            float gpos = (float)gstep*sl + (float)gstep_samp;   // grid-locked position
            #define RAT_N(env, N) { float ph=gpos/(sl*(N)); ph-=floorf(ph); \
                float gt=(ph<0.5f)?1.f:0.f; static float env=1.f; env+=(gt-env)*0.05f; oL*=env; oR*=env; }
            if(h8 & (1u<<E8_RATCH2)) RAT_N(e8r2, 0.5f)
            if(h8 & (1u<<E8_RATCH3)) RAT_N(e8r3, 0.3333f)
            if(h8 & (1u<<E8_RATCH4)) RAT_N(e8r4, 0.25f)
            if(h8 & (1u<<E8_RATCH8)) RAT_N(e8r8, 0.125f)
            if(h8 & (1u<<E8_STRUM)){             // rising ramp across each 1/8
                float ph=gpos/(sl*2.f); ph-=floorf(ph); oL*=ph; oR*=ph;
            }
            if(h8 & (1u<<E8_TRANCE2)){           // alt 16-step pattern, locked to gstep
                static const uint16_t P=0xC5B2; int st=gstep&15;
                float gt=((P>>st)&1)?1.f:0.f; static float e=1.f; e+=(gt-e)*0.03f; oL*=e; oR*=e;
            }
            if(h8 & (1u<<E8_STAB)){              // pluck decay each 1/16
                float ph=gpos/sl; ph-=floorf(ph); float a=1.f-ph; a*=a; oL*=a; oR*=a;
            }
            if(h8 & (1u<<E8_RINGLO)){            // ring mod ~90Hz
                static float ph=0.f; ph+=90.f/48000.f; if(ph>=1.f)ph-=1.f; float c=psin(ph);
                float m=0.4f+0.55f*s; oL=oL*(1.f-m)+oL*c*m; oR=oR*(1.f-m)+oR*c*m;
            }
            if(h8 & (1u<<E8_RINGHI)){            // ring mod ~360Hz
                static float ph=0.f; ph+=360.f/48000.f; if(ph>=1.f)ph-=1.f; float c=psin(ph);
                float m=0.4f+0.55f*s; oL=oL*(1.f-m)+oL*c*m; oR=oR*(1.f-m)+oR*c*m;
            }
            if(h8 & (1u<<E8_VIBR)){              // fast amp vibrato
                static float ph=0.f; ph+=7.f/48000.f*(sl>1.f?1.f:1.f); ph+=0.00010f; if(ph>=1.f)ph-=1.f;
                float m=1.f-(0.2f+0.3f*s)*0.5f*(1.f+psin(ph)); oL*=m; oR*=m;
            }
            if(h8 & (1u<<E8_CHOP)){              // random S&H gate each 1/16, on grid
                static int st=-1; static float lv=1.f; int cs=gstep;
                if(cs!=st){ st=cs; lv=((lcg()&3u)!=0u)?1.f:0.f; } oL*=lv; oR*=lv;
            }
            if(h8 & (1u<<E8_HALFG)){             // half-note gate
                float ph=gpos/(sl*8.f); ph-=floorf(ph); float gt=(ph<0.5f)?1.f:0.f;
                static float e=1.f; e+=(gt-e)*0.02f; oL*=e; oR*=e;
            }
            if(h8 & (1u<<E8_DBLG)){              // two pulses per 1/16
                float ph=gpos/(sl*0.5f); ph-=floorf(ph); float gt=(ph<0.5f)?1.f:0.f;
                static float e=1.f; e+=(gt-e)*0.06f; oL*=e; oR*=e;
            }
            if(h8 & (1u<<E8_GLCHG)){             // glitch gate: random short bursts, on grid
                static int st=-1; static float lv=1.f; int cs=(int)(gpos/(sl*0.5f));
                if(cs!=st){ st=cs; lv=((lcg()%5u)<2u)?0.f:1.f; } oL*=lv; oR*=lv;
            }
            if(h8 & (1u<<E8_REVG)){              // reverse env swell each 1/16
                float ph=gpos/sl; ph-=floorf(ph); oL*=ph*ph; oR*=ph*ph;
            }
            if(h8 & (1u<<E8_SILENCE)){ oL=0.f; oR=0.f; }
            #undef RAT_N
            { const float mx_ = b.mix[7]; if(mx_ < 1.f){ oL = mdL_ + (oL - mdL_) * mx_; oR = mdR_ + (oR - mdR_) * mx_; } }
        }

        // ══ ORDERED MASTER CHAIN: RING → DIST → FILTER → CRUSH ══
        // RING (FX2 F2_RING bit) — must precede distortion + filter
        {
            static float rphase=0.f;
            if((fx_eff2 >> F2_RING) & 1u){
                float fc = 40.f*powf(37.5f,fs[1])*(1.f + 0.3f*fl[1]*sinf(rphase*0.07f));
                rphase += fc/48000.f; if(rphase>=1.f) rphase-=1.f;
                float c=psin(rphase);
                oL = oL*(1.f-fs[1]*0.9f) + (oL*c)*fs[1]*0.9f;
                oR = oR*(1.f-fs[1]*0.9f) + (oR*c)*fs[1]*0.9f;
            }
        }
        // DIST
        if((fh >> FX_DIST) & 1u){
            float g=4.f+fs[0]*10.f; float dc=tanhf(0.08f*g)*0.62f;
            oL=tanhf((oL+0.08f)*g)*0.62f-dc; oR=tanhf((oR+0.08f)*g)*0.62f-dc;
        }
        // FILTER (FX_FLT) — post-mix resonant bandpass DJ sweep, FSR = LFO speed
        {
            static float mlp=0,mbp=0,mlpR=0,mbpR=0,mlfo=0.f;
            if((fh >> FX_FLT) & 1u){
                mlfo += 0.025f + fs[0]*0.30f; if(mlfo>=1.f) mlfo-=1.f;
                float tri = 1.f - fabsf(2.f*mlfo-1.f);
                float fc = 200.f * powf(17.5f, tri);
                float f = 2.f*sinf(3.14159265f*fc/48000.f), q=0.3f;
                mlp += f*mbp;  float hp = oL - mlp - q*mbp;  mbp += f*hp;
                mlpR+= f*mbpR; float hpR= oR - mlpR- q*mbpR; mbpR+= f*hpR;
                oL = mbp; oR = mbpR;
                if(mlp> 4.f)mlp= 4.f; else if(mlp<-4.f)mlp=-4.f;
                if(mbp> 4.f)mbp= 4.f; else if(mbp<-4.f)mbp=-4.f;
                if(mlpR> 4.f)mlpR= 4.f; else if(mlpR<-4.f)mlpR=-4.f;
                if(mbpR> 4.f)mbpR= 4.f; else if(mbpR<-4.f)mbpR=-4.f;
            } else { mlp=mbp=mlpR=mbpR=0.f; }
        }
        // CRUSH (after the filter)
        if(crush){
            if(crn==0){ float qs=24.f-fs[0]*18.f; crL=(float)(int)(oL*qs)/qs; crR=(float)(int)(oR*qs)/qs; }
            crn=(crn+1u)%cint; oL=crL; oR=crR;
        } else crn=0;

        L[i] = oL;
        R[i] = oR;
        gstep_samp++;   // advance the grid-locked sub-step position
    }
    rb_w = rw;
}
