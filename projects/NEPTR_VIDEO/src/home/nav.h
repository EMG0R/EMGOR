// Menu navigation from the DEMIURGE control bus.
//
// Navigation is expressed as INTENTS -- prev / next / select / back / home --
// and every source resolves to one before anything downstream sees it. Touch
// produces them today; MIDI CCs off the DEMIURGE bus produce them the moment
// anything sends `/cc` to this engine's port. Adding a controller later is a
// table entry, not a redesign, and there is only ever one input system.
//
// The CC side is a second CONSUMER of the stream control/receiver.cpp already
// decodes: one socket, one wire format, one decoder. The MappingTable is
// deliberately not in the path -- it maps a source to a PARAMETER, and "move
// the cursor down one row" is not a parameter; routing it through there would
// mean inventing fake parameters for menu rows.
//
// NOTHING SENDS THESE YET. demiurge-io's telemetry has a single destination
// (`/sensor` to 8002) and `/cc` goes only to csound on 7770, so this path is
// decoded correctly and receives nothing until that fan-out is built. It has
// been exercised by sending real `/cc` datagrams at the engine; it has never
// been driven by a physical button.
//
// The button ROLES are not chosen here. They come from
// `DEMIURGE_OS/demiurge/games/docs/BUTTONS.md`, the system-wide authority on
// what each physical button IS -- so HOME's UP is the cabinet's UP, and A
// means A on every screen of the instrument.
//
//   b4 UP    = CC63     b2 DOWN  = CC61     (left d-pad)
//   b9 A     = CC68     b8 B     = CC67     (right face)
//   b1 ARCADE= CC60     b6/b7 X/Y = CC65/66 -- deliberately UNBOUND in HOME
//   joystick click (jb) = CC77
//   joystick Y (analog) = CC28  (SENSOR_MAP.md; 0..127, centre ~64)
//
// jb carries BOTH meanings, which is not a clash but the gesture the rest of
// the instrument already uses: a TAP selects, and a HOLD gets you back out.
// The arcade supervisor reserves jb from gameplay for exactly that hold, with
// the same 1.5 s, so the gesture means one thing everywhere.
//
// X, Y and ARCADE are left unbound on purpose. Giving them a meaning here that
// no other screen shares would be inventing a control.
#pragma once

#include <deque>
#include <string>

namespace nv {

enum class Nav {
    None = 0,
    Prev,     // up one entry
    Next,     // down one entry
    Select,   // activate the current entry
    Back,     // leave a submenu / cancel
    Home,     // long-press escape hatch: return to HOME from any mode
};

// The CC block HOME listens to. Named so a test and the code cannot drift.
namespace navcc {
constexpr int kChannel = 1;
constexpr int kUp = 63;        // b4 UP
constexpr int kDown = 61;      // b2 DOWN
constexpr int kSelect = 68;    // b9 A
constexpr int kBack = 67;      // b8 B
constexpr int kJoyBtn = 77;    // tap = select, hold = back to HOME
// The analog stick reaches the same intent as the d-pad. It is a different
// physical control, not a second mechanism: both resolve to Prev/Next before
// anything downstream can tell them apart.
constexpr int kJoyY = 28;
}  // namespace navcc

// Fires once when a signal has been held continuously for `hold` seconds, and
// will not fire again until it has been released. Same contract as the arcade
// supervisor's ComboDetector, because it is the same gesture on the same
// button and two different hold semantics on one instrument is a bug waiting.
class HoldCombo {
public:
    explicit HoldCombo(double hold = 1.5) : hold_(hold) {}

    // Returns true on the single tick the hold matures.
    bool update(bool down, double now);
    void reset();

    bool armed() const { return down_ && !fired_; }
    // True once this hold has already produced its action -- which is what
    // tells a release apart from a tap.
    bool fired() const { return fired_; }
    // 0..1 progress towards firing, for drawing a hold indicator.
    float progress(double now) const;

private:
    double hold_;
    bool down_ = false;
    bool fired_ = false;
    double since_ = 0.0;
};

// Turns decoded control events into discrete navigation steps.
//
// The joystick is an ABSOLUTE axis, not an encoder: holding it hard over sends
// one CC and then nothing, and letting go sends a stream of values as it
// springs back. So a step fires on CROSSING a threshold and cannot fire again
// until the stick has returned to centre. Without that re-arm the spring-back
// itself walks the cursor, and a menu that moves when you let go is unusable.
class NavInput {
public:
    // `source` and `value` are exactly what Receiver::eventFromOsc produced:
    // "cc:1:28" and 0..1. Anything else is ignored.
    void onControl(const std::string& source, float value, double now);

    // Lets held buttons mature even when no packet arrived this frame.
    void tick(double now);

    // Pops the oldest pending navigation event, or returns false.
    bool poll(Nav& out);

    // 0..1 progress of the return-to-HOME hold, for the view.
    float homeHoldProgress(double now) const { return homeHold_.progress(now); }

    // Joystick polarity is a property of how the stick is physically mounted,
    // which no amount of reading the wire can tell you. Defaults to
    // "high value = down the list"; NV_HOME_INVERT_Y flips it.
    void setInvertY(bool v) { invertY_ = v; }

    // Thresholds, exposed so the test states them rather than assuming.
    static constexpr float kFar = 0.72f;      // beyond this, a step fires
    static constexpr float kNear = 0.28f;
    static constexpr float kRearmLo = 0.38f;  // back inside this band = re-armed
    static constexpr float kRearmHi = 0.62f;

private:
    void push(Nav n);

    std::deque<Nav> queue_;
    int axisState_ = 0;      // -1 held low, 0 centred, +1 held high
    bool invertY_ = false;
    // One entry per momentary button, so a press fires once and a held button
    // does not repeat.
    bool upDown_ = false;
    bool downDown_ = false;
    bool selectDown_ = false;
    bool backDown_ = false;
    bool joyBtnDown_ = false;
    HoldCombo homeHold_{1.5};
};

}  // namespace nv
