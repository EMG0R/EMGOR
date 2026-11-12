// controls.cpp — encoder + joystick. VERBATIM from PMOR_V2.cpp:
//   Enc / ePoll / eInit                    L443–450, L551–566
//   joystick calibration + axes            L453–549 (normalisation changed: see joyAxis)
//   init (enc1, JOY_SW, ADC channels)      L4164–4180
//   detent accumulation ±4 → one detent    L4769–4781 → encoderDelta()
// No detent synthesis from the stick here (v2 L535–543) — nav does flick/repeat.
#include "hw.h"
#include "hw_priv.h"
#include "../config.h"
#include "../pmor_core/shared_state.h"
#include "daisy_seed.h"
using namespace daisy;

// ============================================================
// Encoders
// ============================================================
struct Enc {
    dsy_gpio a, b, sw;
    uint8_t  lab;
    volatile uint8_t psw;   // current switch level (0 = held) — read by main loop
    volatile int8_t delta;
    volatile bool   clicked;
};
static Enc  enc1;
static bool enc_ready = false;

// ---- press timestamps: written in the ISR (ePoll), consumed by the main loop. Single producer,
// single consumer, 8-bit indices => lock-free on Cortex-M7. ISR-side software debounce: a second
// falling edge inside ENC_PRESS_DEBOUNCE_SAMPLES of the last accepted one is contact bounce.
#define ENC_PRESS_RING 4
#define ENC_PRESS_DEBOUNCE_SAMPLES ((uint32_t)(0.015f * SR))   // 15 ms
static volatile uint32_t enc_press_ring[ENC_PRESS_RING];
static volatile uint8_t  enc_press_head = 0, enc_press_tail = 0;
static uint32_t          enc_press_last = 0;
static bool              enc_press_have = false;
static uint32_t          enc_isr_stamp = 0;     // sample instant of the poll in progress (set by joyPollFromIsr)

static void ePoll(Enc& e) {
    static const int8_t lut[4][4]={{0,-1,+1,0},{+1,0,0,-1},{-1,0,0,+1},{0,+1,-1,0}};
    uint8_t ab=(dsy_gpio_read(&e.a)<<1)|dsy_gpio_read(&e.b);
    if(ab!=e.lab){e.delta+=lut[e.lab][ab];e.lab=ab;}
    uint8_t sw=dsy_gpio_read(&e.sw);
    if(e.psw&&!sw) {
        e.clicked=true;
        // Press instant: it happened since the previous 750 Hz poll, so the middle of that
        // interval (half a block back) is the unbiased estimate -- worst-case error half a block (0.67 ms).
        uint32_t t = enc_isr_stamp - (uint32_t)(BLOCK / 2);
        if(!enc_press_have || (uint32_t)(t - enc_press_last) > ENC_PRESS_DEBOUNCE_SAMPLES) {
            uint8_t h = enc_press_head;
            uint8_t nh = (uint8_t)((h + 1) % ENC_PRESS_RING);
            if(nh != enc_press_tail) { enc_press_ring[h] = t; enc_press_head = nh; }   // full: drop newest
            enc_press_last = t; enc_press_have = true;
        }
    }
    e.psw=sw;
}
static void eInit(Enc& e, dsy_gpio_pin pa, dsy_gpio_pin pb, dsy_gpio_pin ps) {
    e.a ={pa,DSY_GPIO_MODE_INPUT,DSY_GPIO_PULLUP};
    e.b ={pb,DSY_GPIO_MODE_INPUT,DSY_GPIO_PULLUP};
    e.sw={ps,DSY_GPIO_MODE_INPUT,DSY_GPIO_PULLUP};
    dsy_gpio_init(&e.a); dsy_gpio_init(&e.b); dsy_gpio_init(&e.sw);
    e.lab=(dsy_gpio_read(&e.a)<<1)|dsy_gpio_read(&e.b);
    e.psw=1; e.delta=0; e.clicked=false;
}

// ── JOYSTICK (v2, Seed 3) ────────────────────────────────────────────────
//   X axis  -> D16 (A1), adc ch 1
//   Y axis  -> D17 (A2), adc ch 2
//   button  -> D24, active low
//
// joyPoll() is called from AudioCallback at 48000/64 = 750Hz.
static dsy_gpio joy_sw_g;

// Rest position, MEASURED. Calibration is deferred and averaged: joyPoll()
// runs from AudioCallback, which can start before the ADC has produced a real
// conversion. Sampling the very first call risks locking in garbage as
// "centre", after which an axis reads pinned or dead forever. So: skip the
// first 200 polls (~0.27s at 750Hz), then average 64 samples.
static uint16_t joy_cx = 32768, joy_cy = 32768;
static uint16_t joy_cal_n = 0;
static uint32_t joy_cal_ax = 0, joy_cal_ay = 0;
static bool     joy_cal = false;

// Centre, then normalise to ±1 against the MEASURED usable deflection and
// apply the normalised deadzone. Centre is MEASURED at first poll rather
// than assumed to be mid-scale: these sticks never rest exactly at 32768,
// and assuming they do gives a permanent slow drift in one direction.
//
// v3: JOY_MAX_RAW (950 on the 0..1023 scale) is the usable deflection from
// centre, i.e. 950/1023 of the 16-bit half-span (32768). v2 normalised
// against the full half-span so a real stick never reached ±1.
// v3 (EMGOR 2026-09-15, "the octave only uses 15 % of the stick"): the usable deflection is
// LEARNED per axis from the largest swing actually seen, so ±1.0 is always the real physical
// edge of THIS stick on THIS axis, whatever the ADC range turns out to be. Starts at a floor so
// the first nudge isn't full scale; only ever grows.
static float joy_maxdef[2] = { 12000.f, 12000.f };
static inline float joyAxis(uint16_t raw, uint16_t centre, int axis)
{
    float d = (float)raw - (float)centre;
    float a0 = d < 0.f ? -d : d;
    if (a0 > joy_maxdef[axis]) joy_maxdef[axis] = a0;
    float n = d / joy_maxdef[axis];
    if(n >  1.f) n =  1.f;
    if(n < -1.f) n = -1.f;
    // deadzone, rescaled so the edge still reaches exactly ±1
    float a = n < 0 ? -n : n;
    if(a < JOY_DEADZONE) return 0.f;
    a = (a - JOY_DEADZONE) / (1.f - JOY_DEADZONE);
    return n < 0 ? -a : a;
}

static void joyPoll()
{
    if(!joy_cal) {
        if(++joy_cal_n > 200) {                 // let the ADC settle first
            joy_cal_ax += hw.adc.Get(1);
            joy_cal_ay += hw.adc.Get(2);
            if(joy_cal_n >= 264) {              // 64 averaged samples
                joy_cx = (uint16_t)(joy_cal_ax >> 6);
                joy_cy = (uint16_t)(joy_cal_ay >> 6);
                joy_cal = true;
            }
        }
        g.joy_x = 0; g.joy_y = 0;
        g.joy_raw_x = hw.adc.Get(1); g.joy_raw_y = hw.adc.Get(2);
        g.joy_sw = (dsy_gpio_read(&joy_sw_g) == 0);
        g.joy_calibrated = false;
        return;                                  // no output until calibrated
    }

    uint16_t rx = hw.adc.Get(1), ry = hw.adc.Get(2);
    g.joy_raw_x = rx; g.joy_raw_y = ry;
    {
        float ax = joyAxis(rx, joy_cx, 0), ay = joyAxis(ry, joy_cy, 1);
#if JOY_SWAP_XY
        float lx = ay, ly = ax;
#else
        float lx = ax, ly = ay;
#endif
#if JOY_INVERT_X
        lx = -lx;
#endif
#if JOY_INVERT_Y
        ly = -ly;
#endif
        g.joy_x = lx;            // +X = RIGHT
        g.joy_y = ly;            // +Y = UP
    }
    g.joy_calibrated = true;

    uint8_t sw = dsy_gpio_read(&joy_sw_g);
    g.joy_sw = (sw == 0);
}

// ============================================================
// public
// ============================================================
void controlsInit() {
    // enc1 unchanged: A=D0 B=D1 SW=D2
    eInit(enc1,hw.GetPin(PIN_ENC_A), hw.GetPin(PIN_ENC_B), hw.GetPin(PIN_ENC_SW));

    // JOYSTICK:  X=D16(A1 adc1)  Y=D17(A2 adc2)  SW=D24
    joy_sw_g = {hw.GetPin(PIN_JOY_SW), DSY_GPIO_MODE_INPUT, DSY_GPIO_PULLUP};
    dsy_gpio_init(&joy_sw_g);
    enc_ready = true;

    // FSR analog (D15) stays channel 0 -- every hw.adc.GetFloat(0) untouched.
    // Joystick axes added as channels 1 and 2.
    AdcChannelConfig adc_cfg[3];
    adc_cfg[0].InitSingle(hw.GetPin(PIN_FSR));     // FSR   -> ch0
    adc_cfg[1].InitSingle(hw.GetPin(PIN_JOY_X));   // JOY X -> ch1
    adc_cfg[2].InitSingle(hw.GetPin(PIN_JOY_Y));   // JOY Y -> ch2
    hw.adc.Init(adc_cfg,3);
    hw.adc.Start();
}

// AUDIO plane, 750 Hz (v2 L2203: `if(enc_ready){ ePoll(enc1); joyPoll(); }`)
void joyPollFromIsr(uint32_t block_start_sample) {
    enc_isr_stamp = block_start_sample;
    if(enc_ready){ ePoll(enc1); joyPoll(); }
}

bool encoderTakePressSample(uint32_t now_sample, uint32_t max_age_samples, uint32_t* out_sample) {
    while(enc_press_tail != enc_press_head) {
        uint32_t t = enc_press_ring[enc_press_tail];
        enc_press_tail = (uint8_t)((enc_press_tail + 1) % ENC_PRESS_RING);
        if((uint32_t)(now_sample - t) <= max_age_samples) { *out_sample = t; return true; }
        // else stale: a press the main loop never turned into an event (missed edge) -- drop it
    }
    return false;
}

// Accumulate ±4 counts per step so one detent ≈ one action, not several.
int encoderDelta() {
    static int acc1 = 0;
    int d1 = enc1.delta; enc1.delta = 0;
    acc1 += d1;
    int detents = 0;
    while(acc1 >=  4) { detents++; acc1 -= 4; }
    while(acc1 <= -4) { detents--; acc1 += 4; }
    return detents;
}

bool encoderPressed() { return enc_ready && enc1.psw == 0; }
bool joyPressed()     { return g.joy_sw; }
