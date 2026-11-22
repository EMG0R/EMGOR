// HOME: the menu state machine, the CC->navigation mapping, and the hold
// combo.
//
// All three fail SILENTLY when wrong -- a menu that moves two rows, a joystick
// that walks the cursor as it springs back, a hold that fires twice. None of
// that throws, and on the device you cannot step through it: the display is
// held by the process you are debugging. So it is pinned here.

#include "check.h"
#include "home/home.h"
#include "home/nav.h"

using namespace nv;

namespace {

// The five entries exist, in this order, and SEPTO RENDERER is present but
// marked unavailable rather than hidden.
void testItems() {
    const auto& items = homeItems();
    CHECK_EQ(static_cast<int>(items.size()), static_cast<int>(HomeEntry::kCount));
    CHECK(items[0].name == "AUDIO");
    CHECK(items[1].name == "VIDEO");
    CHECK(items[2].name == "GAMES");
    CHECK(items[3].name == "NORNS");
    CHECK(items[4].name == "SEPTO RENDERER");
    CHECK(items[0].available);
    CHECK(items[1].available);
    CHECK(items[2].available);
    CHECK(items[3].available);
    CHECK(!items[4].available);
}

void testMenuMove() {
    HomeMenu m;
    m.setSelected(0);
    CHECK_EQ(m.selected(), 0);

    m.move(+1);
    CHECK_EQ(m.selected(), 1);
    m.move(+1);
    m.move(+1);
    CHECK_EQ(m.selected(), 3);

    // Wraps in both directions -- on a five-item menu, walking off the end and
    // finding nothing is the difference between one press and four.
    m.move(+1);
    CHECK_EQ(m.selected(), 4);
    m.move(+1);
    CHECK_EQ(m.selected(), 0);
    m.move(-1);
    CHECK_EQ(m.selected(), 4);

    // The unavailable entry is NOT skipped: you must be able to land on it to
    // learn it exists.
    m.setSelected(3);
    m.move(+1);
    CHECK_EQ(m.selected(), 4);

    // Out-of-range setSelected is ignored, not clamped: it comes from a touch
    // hit test that returned -1 for "nothing there".
    m.setSelected(-1);
    CHECK_EQ(m.selected(), 4);
    m.setSelected(99);
    CHECK_EQ(m.selected(), 4);
}

void testMenuActivate() {
    HomeMenu m;
    m.setSelected(static_cast<int>(HomeEntry::Video));
    CHECK(m.activate() == HomeAction::EnterVideo);

    // NEPTR is the instrument, not a mode: entry 0 launches the AUDIO mode.
    m.setSelected(static_cast<int>(HomeEntry::Audio));
    CHECK(m.activate() == HomeAction::LaunchAudio);

    m.setSelected(static_cast<int>(HomeEntry::Games));
    CHECK(m.activate() == HomeAction::LaunchGames);

    m.setSelected(static_cast<int>(HomeEntry::Norns));
    CHECK(m.activate() == HomeAction::LaunchNorns);

    m.setSelected(static_cast<int>(HomeEntry::Septo));
    CHECK(m.activate() == HomeAction::Unavailable);
}

void testMessageClears() {
    HomeMenu m;
    m.setSelected(static_cast<int>(HomeEntry::Septo));
    m.setMessage("NOT BUILT YET");
    CHECK(m.message() == "NOT BUILT YET");
    // A complaint about a row you have left is noise.
    m.move(-1);
    CHECK(m.message().empty());
}

// ---- config resolution ----------------------------------------------------

void testConfValue() {
    const std::string text =
        "# a comment\n"
        "ui     = /home/x/venv/bin/python /home/x/UI.py\n"
        "audio = on   # trailing comment\n"
        "ui_cmd = /home/x/venv/bin/python /home/x/neptrPhase4_UI.py\n"
        "\n"
        "notakey\n";
    CHECK(confValue(text, "audio") == "on");
    CHECK(confValue(text, "ui_cmd") ==
          "/home/x/venv/bin/python /home/x/neptrPhase4_UI.py");
    // `ui` must not match `ui_cmd`: a prefix match in this parser would pick up
    // a neighbouring key with a similar name.
    CHECK(confValue(text, "ui") == "/home/x/venv/bin/python /home/x/UI.py");
    CHECK(confValue(text, "missing").empty());
}

void testCommandResolution() {
    const std::string ba = "/bin/sh deploy/mode-audio.sh";
    const std::string bg = "/bin/sh deploy/mode-games.sh";
    const std::string bn = "/bin/sh deploy/mode-norns.sh";
    auto sources = [&](const char* ea, const char* eg, const char* en) {
        HomeModeSources s;
        s.audio = {ba, ea};
        s.games = {bg, eg};
        s.norns = {bn, en};
        return s;
    };

    // With nothing configured, the shipped mode scripts are what runs.
    HomeCommands a = resolveHomeCommands("", sources(nullptr, nullptr, nullptr));
    CHECK(a.audio == ba);
    CHECK(a.games == bg);
    CHECK(a.norns == bn);
    CHECK(a.haveAudio());
    CHECK(a.haveGames());
    CHECK(a.haveNorns());

    // A missing script is not silently replaced by a guess: the entry simply
    // has no command and the menu refuses it.
    HomeCommands n = resolveHomeCommands("", HomeModeSources{});
    CHECK(!n.haveAudio());
    CHECK(!n.haveGames());
    CHECK(!n.haveNorns());

    // home.conf wins over the built-in script.
    HomeCommands b = resolveHomeCommands(
        "audio = /other\ngames = retroarch\nnorns = /n\n",
        sources(nullptr, nullptr, nullptr));
    CHECK(b.audio == "/other");
    CHECK(b.games == "retroarch");
    CHECK(b.norns == "/n");

    // Environment wins over both.
    HomeCommands c = resolveHomeCommands("audio = /other\n",
                                         sources("/env", "/g", "/envn"));
    CHECK(c.audio == "/env");
    CHECK(c.games == "/g");
    CHECK(c.norns == "/envn");

    // An empty env var must NOT blank a configured command -- unset and set-to-
    // empty are different things in a shell and only one of them is a choice.
    HomeCommands d = resolveHomeCommands("audio = /other\n", sources("", "", ""));
    CHECK(d.audio == "/other");
    CHECK(d.norns == bn);
}

// ---- navigation from the control bus --------------------------------------

// The joystick is an ABSOLUTE axis, so a step must fire on the crossing and
// re-arm only at centre. Without the re-arm the spring-back walks the cursor
// and the menu moves when you let go.
void testJoystickSteps() {
    NavInput in;
    Nav n;

    in.onControl("cc:1:28", 1.0f, 0.0);
    CHECK(in.poll(n));
    CHECK(n == Nav::Next);
    CHECK(!in.poll(n));

    // Held over: no further steps, however many packets arrive.
    in.onControl("cc:1:28", 1.0f, 0.1);
    in.onControl("cc:1:28", 0.95f, 0.2);
    in.onControl("cc:1:28", 0.80f, 0.3);
    CHECK(!in.poll(n));

    // Spring-back through the middle re-arms but must not itself step.
    in.onControl("cc:1:28", 0.5f, 0.4);
    CHECK(!in.poll(n));

    in.onControl("cc:1:28", 0.0f, 0.5);
    CHECK(in.poll(n));
    CHECK(n == Nav::Prev);
    CHECK(!in.poll(n));
}

// A stick resting just off centre must not stream steps.
void testJoystickDeadzone() {
    NavInput in;
    Nav n;
    for (int i = 0; i < 50; ++i) {
        in.onControl("cc:1:28", 0.55f, i * 0.01);
    }
    CHECK(!in.poll(n));
}

void testJoystickInvert() {
    NavInput in;
    in.setInvertY(true);
    Nav n;
    in.onControl("cc:1:28", 1.0f, 0.0);
    CHECK(in.poll(n));
    CHECK(n == Nav::Prev);
}

// Buttons answer on PRESS, once, and a release does not re-fire.
void testButtons() {
    NavInput in;
    Nav n;

    in.onControl("cc:1:68", 1.0f, 0.0);   // A down
    CHECK(in.poll(n));
    CHECK(n == Nav::Select);
    in.onControl("cc:1:68", 1.0f, 0.05);  // repeat while held
    CHECK(!in.poll(n));
    in.onControl("cc:1:68", 0.0f, 0.1);   // release
    CHECK(!in.poll(n));
    in.onControl("cc:1:68", 1.0f, 0.2);   // pressed again
    CHECK(in.poll(n));
    CHECK(n == Nav::Select);

    in.onControl("cc:1:67", 1.0f, 0.3);   // B
    CHECK(in.poll(n));
    CHECK(n == Nav::Back);

    // The d-pad, per BUTTONS.md: b4 UP = 63, b2 DOWN = 61.
    in.onControl("cc:1:63", 1.0f, 0.4);
    CHECK(in.poll(n));
    CHECK(n == Nav::Prev);
    in.onControl("cc:1:61", 1.0f, 0.5);
    CHECK(in.poll(n));
    CHECK(n == Nav::Next);
}

// One button, two meanings, decided on the release: tap selects, hold goes
// HOME. Getting this wrong means every attempt to leave a mode also activates
// whatever the cursor was sitting on.
void testJoyClickTapVsHold() {
    NavInput in;
    Nav n;

    in.onControl("cc:1:77", 1.0f, 0.0);
    CHECK(!in.poll(n));                    // nothing on the way down
    in.onControl("cc:1:77", 0.0f, 0.2);
    CHECK(in.poll(n));
    CHECK(n == Nav::Select);
    CHECK(!in.poll(n));

    // A hold fires HOME, and the release afterwards must NOT also select.
    in.onControl("cc:1:77", 1.0f, 1.0);
    in.tick(2.7);
    CHECK(in.poll(n));
    CHECK(n == Nav::Home);
    in.onControl("cc:1:77", 0.0f, 2.9);
    CHECK(!in.poll(n));
}

// Nothing outside the navigation block may move the menu: those sources are
// carrying sensor and parameter traffic on the same wire.
void testIgnoresOtherSources() {
    NavInput in;
    Nav n;
    in.onControl("cc:1:20", 1.0f, 0.0);   // an encoder
    in.onControl("cc:1:27", 1.0f, 0.0);   // joystick X: not a menu axis
    in.onControl("cc:1:60", 1.0f, 0.0);   // ARCADE: unbound in HOME on purpose
    in.onControl("cc:1:65", 1.0f, 0.0);   // X
    in.onControl("cc:1:66", 1.0f, 0.0);   // Y
    in.onControl("sensor:jy", 1.0f, 0.0);
    in.onControl("cc:2:63", 1.0f, 0.0);   // right CC, wrong channel
    in.onControl("p:3", 1.0f, 0.0);       // csound parameter ids are not ours
    CHECK(!in.poll(n));
}

// The joystick click sends ONE CC on press and one on release -- nothing
// arrives while it is held. So the hold has to mature on the clock, which is
// what tick() is for.
void testHoldToHome() {
    NavInput in;
    Nav n;

    in.onControl("cc:1:77", 1.0f, 0.0);
    CHECK(!in.poll(n));
    in.tick(0.5);
    CHECK(!in.poll(n));
    in.tick(1.4);
    CHECK(!in.poll(n));
    in.tick(1.6);
    CHECK(in.poll(n));
    CHECK(n == Nav::Home);

    // One hold is one action: keeping the finger down must not re-fire.
    in.tick(3.0);
    in.tick(5.0);
    CHECK(!in.poll(n));

    // Release re-arms.
    in.onControl("cc:1:77", 0.0f, 6.0);
    in.onControl("cc:1:77", 1.0f, 6.1);
    in.tick(7.8);
    CHECK(in.poll(n));
    CHECK(n == Nav::Home);
}

// A tap on the joystick click must not be read as a hold: an accidental knock
// must never drop you out of the mode you are playing.
void testTapIsNotHold() {
    NavInput in;
    Nav n;
    in.onControl("cc:1:77", 1.0f, 0.0);
    in.tick(0.2);
    in.onControl("cc:1:77", 0.0f, 0.3);
    in.tick(2.0);
    in.tick(4.0);
    // The tap is a Select, and crucially NOT a Home: a knock on the stick must
    // never drop you out of the mode you are playing.
    CHECK(in.poll(n));
    CHECK(n == Nav::Select);
    CHECK(!in.poll(n));
}

void testHoldComboProgress() {
    HoldCombo c(1.5);
    CHECK(!c.update(true, 0.0));
    CHECK_NEAR(c.progress(0.75), 0.5f, 0.01f);
    CHECK(c.update(true, 1.5));
    // Once fired the indicator must go quiet rather than sit pegged at full.
    CHECK_NEAR(c.progress(2.0), 0.0f, 0.001f);
    CHECK(!c.update(true, 3.0));
    CHECK(!c.update(false, 3.1));
    CHECK(!c.update(true, 3.2));
    CHECK(c.update(true, 4.8));
}

// The queue is bounded: a waggled stick must not leave the menu coasting
// through a dozen queued steps after the hand comes off.
void testQueueBounded() {
    NavInput in;
    for (int i = 0; i < 100; ++i) {
        in.onControl("cc:1:28", 1.0f, i * 0.01);
        in.onControl("cc:1:28", 0.5f, i * 0.01 + 0.005);
    }
    int drained = 0;
    Nav n;
    while (in.poll(n)) ++drained;
    CHECK(drained <= 8);
    CHECK(drained > 0);
}

// The button roles come from games/docs/BUTTONS.md, the system-wide authority.
// Pinning them here means a silent renumber shows up as a failing test rather
// than as a menu that has quietly stopped responding on the device.
void testCcNumbers() {
    CHECK_EQ(navcc::kChannel, 1);
    CHECK_EQ(navcc::kUp, 63);      // b4
    CHECK_EQ(navcc::kDown, 61);    // b2
    CHECK_EQ(navcc::kSelect, 68);  // b9 = A
    CHECK_EQ(navcc::kBack, 67);    // b8 = B
    CHECK_EQ(navcc::kJoyBtn, 77);
    CHECK_EQ(navcc::kJoyY, 28);
}

}  // namespace

int main() {
    testItems();
    testMenuMove();
    testMenuActivate();
    testMessageClears();
    testConfValue();
    testCommandResolution();
    testJoystickSteps();
    testJoystickDeadzone();
    testJoystickInvert();
    testButtons();
    testIgnoresOtherSources();
    testJoyClickTapVsHold();
    testHoldToHome();
    testTapIsNotHold();
    testHoldComboProgress();
    testQueueBounded();
    testCcNumbers();
    return check::finish("home");
}
