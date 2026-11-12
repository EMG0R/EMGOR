// HOME: the boot-time mode selector.
//
// This lives INSIDE neptr-video rather than in a binary of its own, and that
// is not a convenience. KMS allows exactly one DRM master, so a separate menu
// process would have to hand the display to the engine and take it back for
// every glance at the menu -- and it would need its own GLES context, its own
// font and its own touch reader to draw four words. The engine already has all
// of that, so HOME is a view, not a program.
//
// Two of the four entries are OUT-OF-PROCESS (the audio UI, the arcade), and
// for those the display genuinely does change hands: see launcher.h.
//
// Everything here is pure logic -- no GL, no fork, no sockets -- because the
// failure mode of a menu is silent (it moves to the wrong row, or refuses to
// move at all) and that is exactly what a test can pin.
#pragma once

#include <string>
#include <vector>

namespace nv {

// NEPTR is the INSTRUMENT, not one of the choices -- it is the thing you are
// already inside. These are its modes, and the one that starts the audio
// engine is called AUDIO for exactly that reason. Listing "NEPTR" as a peer of
// VIDEO and GAMES would say the instrument is a sibling of its own modes.
enum class HomeEntry {
    Audio = 0,   // the audio engine + its UI: out of process
    Video,       // this engine's synth view: in process, instant
    Games,       // the arcade console: RetroArch, out of process
    Norns,       // monome norns scripts, as a demiurge patch: out of process
    Septo,       // SEPTO RENDERER: named, shown, not built
    kCount,
};

// What activating the current entry asks the caller to do. The menu decides
// WHAT should happen; main decides HOW, because only main can fork or touch
// the platform.
enum class HomeAction {
    None,          // nothing to do (already here, or nothing selected)
    EnterVideo,    // switch this process to the synth view
    LaunchAudio,   // drop DRM master, run the audio UI, take it back
    LaunchGames,   // drop DRM master, run the arcade, take it back
    LaunchNorns,   // drop DRM master, run the norns patch, take it back
    Unavailable,   // a real entry that is not built yet -- say so, do nothing
};

struct HomeItem {
    std::string name;
    std::string note;        // one line under the name
    bool available = true;
};

// The four entries, in display order. Free function rather than a member so
// the list is one fact in one place, readable without a menu instance.
const std::vector<HomeItem>& homeItems();

const char* homeEntryName(HomeEntry e);

class HomeMenu {
public:
    int selected() const { return selected_; }
    void setSelected(int i);

    // Moves by delta with wraparound. Unavailable entries are NOT skipped: a
    // placeholder you cannot land on is a placeholder nobody can find out
    // about, and "MULTI-DIM-DSP / NOT BUILT" is the whole point of showing it.
    void move(int delta);

    // What the current entry asks for. Also returns the entry, so a caller can
    // report which one was refused.
    HomeAction activate() const;

    // Set after an Unavailable activation so the view can say why. Cleared by
    // any movement -- a stale complaint about a row you have left is noise.
    const std::string& message() const { return message_; }
    void setMessage(const std::string& m) { message_ = m; }
    void clearMessage() { message_.clear(); }

private:
    int selected_ = static_cast<int>(HomeEntry::Video);
    std::string message_;
};

// The commands HOME runs for the out-of-process modes.
//
// Neither is a program this engine knows how to start. The audio UI is started
// by neptr-ui.service and the arcade by the games config, and BOTH carry things
// a command line does not -- environment, working directory, supplementary
// groups, aux processes. Re-implementing either from its command line produced
// a UI that came up rotated sideways on the device, which is what "two
// mechanisms for one job" looks like in practice.
//
// So HOME shells out to a small script per mode (deploy/mode-audio.sh,
// deploy/mode-games.sh, deploy/mode-norns.sh) whose entire job is to ask the
// existing owner and block until the mode is over.
struct HomeCommands {
    std::string audio;
    std::string games;
    std::string norns;

    bool haveAudio() const { return !audio.empty(); }
    bool haveGames() const { return !games.empty(); }
    bool haveNorns() const { return !norns.empty(); }
};

// The inputs resolveHomeCommands weighs, one group per out-of-process mode.
// A struct rather than a parameter per mode per source: NORNS was the third
// mode and would have been the seventh and eighth positional arguments, which
// is the point at which a call site stops being readable and a swapped pair
// stops being a compile error.
struct HomeModeSource {
    std::string builtin;      // the shipped mode script, if it exists on disk
    const char* env = nullptr;  // NV_HOME_* override; nullptr = unset
};

struct HomeModeSources {
    HomeModeSource audio;
    HomeModeSource games;
    HomeModeSource norns;
};

// Parses `key = value` lines, '#' comments, unknown keys ignored. Shared by
// live.conf and home.conf because they are the same trivial format, and a
// second parser for the same syntax is a second thing to get wrong.
std::string confValue(const std::string& text, const std::string& key);

// Resolution order, most specific first:
//   1. NV_HOME_AUDIO / NV_HOME_GAMES / NV_HOME_NORNS environment
//   2. audio = / games = / norns = in home.conf
//   3. the built-in mode script, when it exists on disk
// An entry with no command resolves to Unavailable rather than launching
// something wrong: a menu that runs the last thing it guessed is worse than
// one that admits it does not know. An empty environment variable is NOT a
// choice -- unset and set-to-empty are different things in a shell.
HomeCommands resolveHomeCommands(const std::string& homeConfText,
                                 const HomeModeSources& src);

// Reads home.conf, locates the mode scripts, and applies the same order.
HomeCommands loadHomeCommands();

}  // namespace nv
