#include "home/nav.h"

#include <algorithm>
#include <cstdio>

namespace nv {

bool HoldCombo::update(bool down, double now) {
    if (!down) {
        down_ = false;
        fired_ = false;   // release re-arms; one hold is one action
        return false;
    }
    if (!down_) {
        down_ = true;
        fired_ = false;
        since_ = now;
        return false;
    }
    if (!fired_ && now - since_ >= hold_) {
        fired_ = true;
        return true;
    }
    return false;
}

void HoldCombo::reset() {
    down_ = false;
    // Deliberately NOT clearing fired_: after an action fires, the finger is
    // usually still down. Leaving it latched means the next fire needs a real
    // release, which is what stops one long press from firing twice.
    fired_ = true;
}

float HoldCombo::progress(double now) const {
    if (!down_ || fired_ || hold_ <= 0.0) return 0.0f;
    return static_cast<float>(std::min(1.0, (now - since_) / hold_));
}

void NavInput::push(Nav n) {
    // A menu that queues fifty pending steps because a stick was waggled is a
    // menu that keeps moving after you stop. Bound it.
    if (queue_.size() < 8) queue_.push_back(n);
}

// True on the rising edge only: a momentary button answers once when it goes
// down, the way a physical arcade button does, and a held one does not repeat.
static bool pressed(bool& state, float value) {
    bool down = value >= 0.5f;
    bool edge = down && !state;
    state = down;
    return edge;
}

void NavInput::onControl(const std::string& source, float value, double now) {
    char buf[24];
    auto is = [&](int cc) {
        std::snprintf(buf, sizeof(buf), "cc:%d:%d", navcc::kChannel, cc);
        return source == buf;
    };

    if (is(navcc::kUp)) {
        if (pressed(upDown_, value)) push(Nav::Prev);
        return;
    }
    if (is(navcc::kDown)) {
        if (pressed(downDown_, value)) push(Nav::Next);
        return;
    }
    if (is(navcc::kSelect)) {
        if (pressed(selectDown_, value)) push(Nav::Select);
        return;
    }
    if (is(navcc::kBack)) {
        if (pressed(backDown_, value)) push(Nav::Back);
        return;
    }
    if (is(navcc::kJoyBtn)) {
        bool down = value >= 0.5f;
        // Read BEFORE update(): a release re-arms the combo, which clears the
        // very flag that says whether this press already did its job.
        bool alreadyFired = homeHold_.fired();
        if (homeHold_.update(down, now)) push(Nav::Home);
        // A release that never matured into a hold was a tap, and a tap
        // selects. Deciding on the RELEASE is what lets one button carry both
        // meanings without the tap firing on the way to the hold.
        if (!down && joyBtnDown_ && !alreadyFired) push(Nav::Select);
        joyBtnDown_ = down;
        return;
    }
    if (is(navcc::kJoyY)) {
        // Absolute axis: fire on the crossing, re-arm only at centre.
        if (axisState_ == 0) {
            if (value >= kFar) {
                axisState_ = +1;
                push(invertY_ ? Nav::Prev : Nav::Next);
            } else if (value <= kNear) {
                axisState_ = -1;
                push(invertY_ ? Nav::Next : Nav::Prev);
            }
        } else if (value > kRearmLo && value < kRearmHi) {
            axisState_ = 0;
        }
        return;
    }
}

void NavInput::tick(double now) {
    // The joystick click sends one CC on press and one on release, so nothing
    // arrives while it is HELD -- the hold has to mature on the clock, not on
    // the wire. Re-asserting the current state each frame is what does that.
    if (homeHold_.armed() && homeHold_.update(true, now)) push(Nav::Home);
}

bool NavInput::poll(Nav& out) {
    if (queue_.empty()) return false;
    out = queue_.front();
    queue_.pop_front();
    return true;
}

}  // namespace nv
