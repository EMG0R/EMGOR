// input.cpp — CONTROL plane input: 32-entry event ring + inputPoll().
// Turns raw hw levels (encoder/joystick/pads) into the InputEvent stream nav.cpp consumes.
#include "input.h"
#include "nav.h"          // nav.focus_pad gates the joystick gesture detector
#include "../config.h"
#include <math.h>

// ==================================================================== ring
#define RING_CAP 32
static InputEvent s_ring[RING_CAP];
static uint8_t s_head = 0, s_tail = 0, s_count = 0;

bool inputPush(const InputEvent& e) {
    if(s_count >= RING_CAP) return false;   // full: drop (never blocks)
    s_ring[s_head] = e;
    s_head = (uint8_t)((s_head + 1) % RING_CAP);
    s_count++;
    return true;
}

bool inputPop(InputEvent& e) {
    if(s_count == 0) return false;
    e = s_ring[s_tail];
    s_tail = (uint8_t)((s_tail + 1) % RING_CAP);
    s_count--;
    return true;
}

static inline void push(InputType t, int16_t a, int16_t b, uint32_t now) {
    InputEvent e; e.type = t; e.a = a; e.b = b; e.t_ms = now; e.rpt = false;
    inputPush(e);
}

// ==================================================================== buttons: debounce + both-press
#define BTN_DEBOUNCE_MS 8

struct Btn { bool raw = false; uint32_t last_change_ms = 0; };
static Btn s_enc_btn, s_joy_btn;

// -1 = no pending single press waiting for a possible chord; 0 = encoder, 1 = joystick
static int8_t  s_last_press_btn = -1;
static uint32_t s_last_press_ms = 0;

// returns true exactly on a debounced press edge (low->high)
static bool debouncedPress(bool raw, Btn& b, uint32_t now) {
    if(raw != b.raw) {
        if(now - b.last_change_ms >= BTN_DEBOUNCE_MS) {
            b.raw = raw;
            b.last_change_ms = now;
            return raw;   // only a rising edge counts as a press
        }
        // bounce inside the debounce window: ignore, keep old accepted state
    }
    return false;
}

// Cross-module fix (integration review): the previous version always pushed
// the solo press event (IN_ENC_PRESS / IN_JOY_PRESS) THEN separately detected
// the chord and pushed IN_BOTH_PRESS after it. nav.cpp processes events in
// ring order, so by the time it saw IN_BOTH_PRESS, nav.last_single had
// already been overwritten by the SECOND button's own solo event — nav could
// no longer tell which button was truly first, and (worse) the joystick's
// solo navigate action fired even when it was really the second half of a
// chord. Fix: decide the chord BEFORE emitting a solo press. The encoder
// still ALWAYS pushes IN_ENC_PRESS (per spec, tap tempo fires unconditionally
// on every encoder press, chord or not); the joystick's solo IN_JOY_PRESS is
// suppressed when it is completing a chord, so it never performs a stray
// navigation. IN_BOTH_PRESS.a carries which button was pressed FIRST
// (0 = encoder, 1 = joystick) so nav.cpp can undo the correct action without
// relying on nav.last_single.
static void handleButton(bool is_encoder, uint32_t now) {
    int8_t me    = is_encoder ? 0 : 1;
    int8_t other = is_encoder ? 1 : 0;
    bool chord = (s_last_press_btn == other) && ((now - s_last_press_ms) <= NAV_DOUBLE_WINDOW_MS);
    if(chord) {
        if(is_encoder) push(IN_ENC_PRESS, 0, 0, now);   // nothing else is ever on this button
        push(IN_BOTH_PRESS, other, 0, now);             // a = which button pressed first
        s_last_press_btn = -1;
    } else {
        push(is_encoder ? IN_ENC_PRESS : IN_JOY_PRESS, 0, 0, now);
        s_last_press_btn = me;
        s_last_press_ms  = now;
    }
}

// ==================================================================== joystick nav (flick + repeat)
struct AxisNav {
    bool    armed = false;
    int8_t  dir   = 0;
    uint32_t next_repeat_ms = 0;
};
static AxisNav s_navx, s_navy;
#define NAV_FLICK_THRESH 0.55f
#define NAV_FLICK_HYST   0.35f

// updates one axis's armed/repeat state; sets fire=true + dir when it should emit this poll
static void axisNavStep(AxisNav& s, float v, uint32_t now, bool& fire, int8_t& dir, bool& repeat) {
    float av = v < 0.f ? -v : v;
    fire = false; repeat = false;
    if(!s.armed) {
        if(av >= NAV_FLICK_THRESH) {
            s.armed = true;
            s.dir = v > 0.f ? 1 : -1;
            s.next_repeat_ms = now + NAV_REPEAT_DELAY_MS;
            fire = true; dir = s.dir;
        }
    } else {
        if(av < NAV_FLICK_HYST) {
            s.armed = false;
        } else if(av >= NAV_FLICK_THRESH && now >= s.next_repeat_ms) {
            s.next_repeat_ms = now + (1000u / NAV_REPEAT_HZ);
            fire = true; dir = s.dir; repeat = true;
        }
    }
}

// ==================================================================== joystick gesture (2 full circles)
static bool     s_gest_active = false;
static float    s_gest_angle_accum = 0.f;   // radians, signed
static float    s_gest_last_angle  = 0.f;
static uint32_t s_gest_below_since = 0;
#define GEST_R_ON      0.60f
#define GEST_R_OFF     0.45f
#define GEST_RESET_MS 300
#define GEST_TURNS     (4.f * 3.14159265f)   // TWO full rotations
#define GEST_REVERSAL (0.5f * 3.14159265f)    // > pi/2 reversal resets

static void gestureStep(float x, float y, uint32_t now) {
    if(nav.focus_pad < 0) { s_gest_active = false; s_gest_angle_accum = 0.f; s_gest_below_since = 0; return; }
    float r = sqrtf(x * x + y * y);
    if(r > GEST_R_ON) {
        float ang = atan2f(y, x);
        if(!s_gest_active) {
            s_gest_active = true;
            s_gest_angle_accum = 0.f;
            s_gest_last_angle = ang;
        } else {
            float d = ang - s_gest_last_angle;
            while(d >  3.14159265f) d -= 2.f * 3.14159265f;
            while(d < -3.14159265f) d += 2.f * 3.14159265f;
            // A flick through the centre shows up here as a ~pi jump (the stick was
            // briefly inside the dead ring). That is NOT rotation: any step larger than
            // a quarter turn between two polls restarts the count.
            if((d < 0.f ? -d : d) > 1.5707963f) {
                s_gest_angle_accum = 0.f; s_gest_last_angle = ang; s_gest_below_since = 0; return;
            }
            if(s_gest_angle_accum != 0.f &&
               ((d > 0.f) != (s_gest_angle_accum > 0.f)) &&
               (d < 0.f ? -d : d) > GEST_REVERSAL) {
                s_gest_angle_accum = 0.f;      // sharp reversal: start over
            } else {
                s_gest_angle_accum += d;
            }
            s_gest_last_angle = ang;
            if((s_gest_angle_accum < 0.f ? -s_gest_angle_accum : s_gest_angle_accum) >= GEST_TURNS) {
                // atan2 angle increases counter-clockwise (math convention) -> accum>0 == CCW
                int8_t a = s_gest_angle_accum > 0.f ? -1 : +1;   // +1 CW, -1 CCW
                push(IN_JOY_GESTURE, a, 0, now);
                s_gest_angle_accum = 0.f;
            }
        }
        s_gest_below_since = 0;
    } else if(r < GEST_R_OFF) {
        // back inside the ring: a real circle never comes here — restart immediately
        s_gest_active = false; s_gest_angle_accum = 0.f; s_gest_below_since = 0;
    } else {
        s_gest_below_since = 0;   // between OFF and ON: hold, neither accumulate nor reset
    }
}

// ==================================================================== pads
static uint16_t s_pads_prev = 0;
static uint32_t s_pad_press_ms[N_PADS] = {0};
static uint32_t s_pad_next_hold_ms[N_PADS] = {0};   // threshold (relative, ms since press) for next IN_PAD_HOLD

static void padsStep(uint16_t pads_now, uint32_t now) {
    uint16_t pressed  = (uint16_t)(pads_now & (uint16_t)~s_pads_prev);
    uint16_t released = (uint16_t)(s_pads_prev & (uint16_t)~pads_now);
    for(uint8_t i = 0; i < N_PADS; i++) {
        uint16_t bit = (uint16_t)(1u << i);
        if(pressed & bit) {
            push(IN_PAD_PRESS, (int16_t)i, 0, now);
            s_pad_press_ms[i] = now;
            s_pad_next_hold_ms[i] = 500;
        } else if(released & bit) {
            push(IN_PAD_RELEASE, (int16_t)i, 0, now);
            s_pad_next_hold_ms[i] = 0;
        } else if(pads_now & bit) {
            uint32_t held_ms = now - s_pad_press_ms[i];
            if(s_pad_next_hold_ms[i] != 0 && held_ms >= s_pad_next_hold_ms[i]) {
                push(IN_PAD_HOLD, (int16_t)i, (int16_t)(held_ms > 32767u ? 32767 : held_ms), now);
                s_pad_next_hold_ms[i] += 100;
            }
        }
    }
    s_pads_prev = pads_now;
}

// ==================================================================== joystick-button hold
static bool     s_joy_down = false;
static uint32_t s_joy_down_ms = 0, s_joy_next_hold = 0;
static void joyHoldStep(bool down, uint32_t now) {
    if(!down) { s_joy_down = false; return; }
    if(!s_joy_down) { s_joy_down = true; s_joy_down_ms = now; s_joy_next_hold = 500; return; }
    uint32_t held = now - s_joy_down_ms;
    if(held >= s_joy_next_hold) {
        push(IN_JOY_HOLD, (int16_t)(held > 32767u ? 32767 : held), 0, now);
        s_joy_next_hold += 100;
    }
}

// ==================================================================== encoder
static void encoderStep(int enc_delta, uint32_t now) {
    int n = enc_delta < 0 ? -enc_delta : enc_delta;
    int8_t dir = enc_delta > 0 ? 1 : -1;
    for(int i = 0; i < n; i++) push(IN_ENC_TURN, dir, 0, now);
}

// ==================================================================== inputPoll
void inputPoll(uint32_t now_ms, int enc_delta, bool enc_sw, bool joy_sw, float joy_x, float joy_y, uint16_t pads_now) {
    encoderStep(enc_delta, now_ms);

    if(debouncedPress(enc_sw, s_enc_btn, now_ms)) handleButton(true, now_ms);
    if(debouncedPress(joy_sw, s_joy_btn, now_ms)) handleButton(false, now_ms);
    joyHoldStep(s_joy_btn.raw, now_ms);

    bool fire_x = false, fire_y = false, rpt_x = false, rpt_y = false; int8_t dx = 0, dy = 0;
    axisNavStep(s_navx, joy_x, now_ms, fire_x, dx, rpt_x);
    axisNavStep(s_navy, joy_y, now_ms, fire_y, dy, rpt_y);
    if(fire_x && fire_y) {
        // dominant axis wins on a diagonal flick
        float ax = joy_x < 0.f ? -joy_x : joy_x, ay = joy_y < 0.f ? -joy_y : joy_y;
        if(ax >= ay) fire_y = false; else fire_x = false;
    }
    if(fire_x) { InputEvent e; e.type = IN_JOY_NAV; e.a = dx; e.b = 0;  e.t_ms = now_ms; e.rpt = rpt_x; inputPush(e); }
    if(fire_y) { InputEvent e; e.type = IN_JOY_NAV; e.a = 0;  e.b = dy; e.t_ms = now_ms; e.rpt = rpt_y; inputPush(e); }

    gestureStep(joy_x, joy_y, now_ms);

    padsStep(pads_now, now_ms);
}
