// input.h — CONTROL plane input events. hw modules produce them; ui/nav consumes them.
// All buttons act on PRESS (impact), never release.
#pragma once
#include <stdint.h>

enum InputType : uint8_t {
    IN_NONE = 0,
    IN_ENC_TURN,        // a = ±1 per detent
    IN_ENC_PRESS,       // encoder click (tap tempo)
    IN_JOY_PRESS,       // joystick click
    IN_BOTH_PRESS,      // both within NAV_DOUBLE_WINDOW_MS → volume; the first single press is UNDONE by nav
                        // (a = which button was pressed FIRST: 0 = encoder, 1 = joystick)
    IN_JOY_NAV,         // a = dx (-1/0/+1), b = dy ; game-console style: one per flick, repeats while held
    IN_JOY_GESTURE,     // a = +1 two clockwise circles, -1 two counter-clockwise (while a pad is focused)
    IN_PAD_PRESS,       // a = pad 0..15
    IN_PAD_RELEASE,     // a = pad
    IN_PAD_HOLD,        // a = pad, b = held ms (fires at 500, then every 100 ms while held)
    IN_JOY_HOLD,        // a = held ms of the joystick button (fires at 500, then every 100 ms while held)
};

struct InputEvent {
    InputType type;
    int16_t   a, b;
    uint32_t  t_ms;
    bool      rpt;      // IN_JOY_NAV only: true for the auto-repeats of a held stick, false for the initial flick.
                        // Toggles (step-edit enter/exit) must act on the flick alone -- a held stick used to
                        // re-fire them at 8 Hz and flap the mode.
};

// ring buffer, CONTROL plane only
bool inputPush(const InputEvent& e);
bool inputPop(InputEvent& e);
// called every main-loop pass with the current raw states; generates the events above
void inputPoll(uint32_t now_ms, int enc_delta, bool enc_sw, bool joy_sw, float joy_x, float joy_y, uint16_t pads_now);
