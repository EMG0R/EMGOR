#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <functional>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "async_compiler.h"
#include "chain/effects.h"
#include "chain/ops.h"
#include "control/mapping.h"
#include "control/receiver.h"
#include "core/kernels.h"
#include "home/home.h"
#include "home/launcher.h"
#include "home/nav.h"
#include "io/profile.h"
#include "mod.h"
#include "palette/palette.h"
#include "patch.h"
#include "platform/platform.h"
#include "preset.h"
#include "renderer.h"
#include "state.h"
#include "ui/panel.h"

using namespace nv;

namespace {

ChainSlot makeSlot(const char* id) {
    ChainSlot s;
    s.def = findEffect(id);
    return s;
}

// The starting chain shows the generator, not a stylised version of it.
// KALEIDO is present but BYPASSED: it is a strong look you reach for, and
// leaving it on by default made every fractal read as the same six-petal
// rosette. Press 2 to bring it in.
//
// It also still demonstrates the fusion rule: FEEDBACK opens a pass and
// SATURATE + LEVELS + GRAIN all fuse onto a second one.
std::vector<ChainSlot> defaultChain() {
    std::vector<ChainSlot> c = {
        makeSlot("feedback"),
        makeSlot("kaleido"),
        makeSlot("saturate"),
        makeSlot("levels"),
        makeSlot("grain"),
    };
    c[1].enabled = false;
    return c;
}

// Which view this process is showing. HOME is a VIEW, not a program: only one
// process can be DRM master, and the engine already owns the display, a GLES
// context, a font and a touch-ready panel renderer. A separate menu binary
// would duplicate all of that to draw four words, and would have to hand the
// screen back and forth every time you glanced at it.
enum class View {
    Home,
    Synth,
};

struct App {
    Renderer renderer;
    Panel panel;
    AsyncCompiler compiler;
    Receiver receiver;
    MappingTable mappings;
    ParamBank params;
    LfoBank lfos;
    MacroBank macros;
    // Which LFO the next parameter you touch gets bound to, if any.
    ModLearn learn;
    // The mapping row whose amount a finger is currently dragging, or -1.
    // Held here rather than in the Panel because the Panel does not own the
    // table and must never be able to change it behind the engine's back.
    int mapDrag = -1;
    // The frame loop's clock, so the input callbacks -- which are handed no
    // time of their own -- can arm and expire a learn against the same one.
    double clock = 0.0;
    Selection sel;
    std::vector<ChainSlot> chain;
    View view = View::Home;
    HomeMenu home;
    NavInput nav;
    HomeCommands cmds;
    // Pending out-of-process launch. Handled at the top of the next frame
    // rather than inside the input callback: dropping DRM master from inside
    // an event handler would tear the display out from under the frame that
    // is still being drawn.
    std::string pendingLaunch;
    std::string pendingLabel;
    bool running = true;
    bool showPanel = true;
    std::string presetDir = "presets";
    std::string status;      // last preset action, shown in the panel header
    double statusTime = 0.0;

    void savePreset(int slot) {
        PresetData p = capturePreset(params, sel, chain, &mappings);
        std::string path = presetPath(presetDir, slot);
        char msg[160];
        if (savePresetFile(path, p)) {
            std::snprintf(msg, sizeof(msg), "SAVED %d", slot);
        } else {
            std::snprintf(msg, sizeof(msg), "SAVE %d FAILED", slot);
        }
        status = msg;
        std::printf("[nv] %s -> %s\n", msg, path.c_str());
        std::fflush(stdout);
    }

    void loadPreset(int slot) {
        PresetData p;
        std::string err;
        char msg[160];
        if (!loadPresetFile(presetPath(presetDir, slot), p, &err)) {
            std::snprintf(msg, sizeof(msg), "NO PRESET %d", slot);
            status = msg;
            return;
        }
        // Order is load-bearing: the selection and chain decide WHICH knobs
        // exist, so the bank has to be rebuilt before any value can be
        // applied by id.
        applyPresetSelection(p, sel, chain);
        applyKernel();
        rebuildPatchParams(params, kernel(), morphTarget(), chain,
                           channelBKernel());
        renderer.requestChain(chain);
        renderer.setPalettes(sel.paletteA, sel.paletteB);
        // Replaces the PATCH's modulation rows and leaves the rig's mapping
        // rows exactly as they are. A recall changes the sound, not the wiring
        // between this instrument and the hardware in front of it.
        applyPresetLfos(p, mappings);
        refreshModInfo();
        int unknown = 0;
        int applied = 0;
        {
            // A recall is a MOVE, not a cut. Same call as everywhere else,
            // with the rate set around it -- which is also why loading at
            // startup and loading on stage can differ without two code paths:
            // GLIDE is 0 until the frame loop starts reading the knob, so
            // NV_LOAD lands instantly and SHIFT+key flies.
            GlideScope g(params, params.presetGlideSeconds());
            applied = applyPresetParams(p, params, &unknown);
        }
        renderer.governor().reset();

        std::snprintf(msg, sizeof(msg), "LOADED %d (%d)", slot, applied);
        status = msg;
        std::printf("[nv] loaded preset %d: %d params", slot, applied);
        if (unknown > 0) {
            // Not an error: a preset from a newer build carries knobs this
            // one has never heard of.
            std::printf(", %d unknown skipped", unknown);
        }
        std::printf("\n");
        std::fflush(stdout);
    }

    // Pushes the two facts about modulation that are not floats -- what each
    // LFO points at, and how many parameters each macro drives -- into the
    // panel. Not per frame: neither can change without one of the few events
    // that call this, and rebuilding two vectors every frame to say the same
    // thing would be work the scene is paying for.
    void refreshModInfo() {
        std::vector<LfoInfo> targets;
        std::vector<int> fanout(kMacroCount, 0);
        for (int n = 0; n < kLfoCount; ++n) {
            LfoInfo info;
            for (const Mapping& m : lfoTargets(mappings, n)) {
                ++info.count;
                if (info.count == 1) info.first = m.param;
                if (params.find(m.param) < 0) info.dangling = true;
            }
            targets.push_back(info);
        }
        for (const auto& m : mappings.all()) {
            for (int n = 0; n < kMacroCount; ++n) {
                if (m.source == macroSource(n)) ++fanout[n];
            }
        }
        panel.setModInfo(targets, fanout);

        // ...and the whole table, for the MAP page. Same call, because these
        // are the same fact asked at two levels of detail, and two refresh
        // points would be two chances to show a page that disagrees with the
        // engine.
        std::vector<PanelMapRow> rows;
        rows.reserve(mappings.size());
        for (const auto& m : mappings.all()) {
            PanelMapRow r;
            r.source = m.source;
            r.param = m.param;
            r.lo = m.lo;
            r.hi = m.hi;
            r.mod = m.mod;
            r.relative = (m.mode == MapMode::Relative);
            r.dangling = (params.find(m.param) < 0);
            rows.push_back(r);
        }
        panel.setMappings(rows);
    }

    // ---- the MAP page's two edits ---------------------------------------
    //
    // Both go through the same MappingTable the learn gesture, the wire and
    // the file all write, so the page is a view of the routing rather than a
    // second copy of it.

    // Only PATCH rows are editable on the device. A rig row (sensor:, cc:,
    // macro:) came out of the performer's mapping file, nothing here writes
    // that file back, and an edit you cannot save is an edit you lose on the
    // next start -- which is worse than not offering it.
    bool mapRowIsPatch(int row) const {
        return row >= 0 && row < static_cast<int>(mappings.size()) &&
               mappings.all()[static_cast<size_t>(row)].mod;
    }

    void removeMapRow(int row) {
        if (!mapRowIsPatch(row)) return;
        const Mapping m = mappings.all()[static_cast<size_t>(row)];
        mappings.removeRow(row);
        // Indices shift under a removal, so a drag holding one is holding a
        // stale one.
        mapDrag = -1;
        char msg[160];
        std::snprintf(msg, sizeof(msg), "%s - %s", m.source.c_str(),
                      m.param.c_str());
        status = msg;
        std::printf("[mod] removed %s -> %s\n", m.source.c_str(),
                    m.param.c_str());
        std::fflush(stdout);
        refreshModInfo();
    }

    // `t` is 0..1 across the bar; the middle is zero and either side of it is
    // the same modulation in opposite directions. Stored as a span within
    // 0..1 so the number in a preset stays readable either way round.
    void dragMapAmount(int row, float t) {
        if (!mapRowIsPatch(row)) return;
        float span = (t - 0.5f) * 2.0f;
        // A detent at the centre, for the same reason DEPTH has one: "off" has
        // to be a place a finger can find on a five-pixel bar.
        if (std::fabs(span) < 0.03f) span = 0.0f;
        if (span >= 0.0f) mappings.setRange(row, 0.0f, span);
        else mappings.setRange(row, -span, 0.0f);
        refreshModInfo();
    }

    // Point an LFO at a parameter. THE one place: the finger on the panel, the
    // L key, `/nv/lfo` off the wire and NV_LFO at startup all arrive here, so
    // there is one answer to what an assignment does, one place that reports
    // it, and one place that refreshes what the panel draws.
    //
    // `viaLearn` is the only difference between them, and it is a property of
    // the GESTURE rather than of the assignment: re-pointing an LFO at what it
    // already drives means "stop" when a hand did it (the bind gesture is its
    // own undo) and means what it says when a message did it (a datagram can
    // arrive twice).
    void setLfoTarget(int slot, const std::string& id, bool viaLearn) {
        char msg[160];
        if (slot < 0 || slot >= kLfoCount) {
            std::snprintf(msg, sizeof(msg), "NO LFO %d", slot + 1);
            status = msg;
            return;
        }
        BindResult r = viaLearn ? learnLfoTarget(mappings, slot, id)
                                : assignLfoTarget(mappings, slot, id);
        int fanout = mappings.countFor(lfoSource(slot));
        switch (r) {
            case BindResult::Assigned:
                // How many it drives NOW, because a learn adds rather than
                // replaces and "L1 > g.zoom" alone would read as though the
                // other two destinations had just been dropped.
                if (fanout > 1) {
                    std::snprintf(msg, sizeof(msg), "L%d + %s (%d)", slot + 1,
                                  id.c_str(), fanout);
                } else {
                    std::snprintf(msg, sizeof(msg), "L%d > %s", slot + 1,
                                  id.c_str());
                }
                break;
            case BindResult::Cleared:
                // Same reason in reverse: removing one destination of three is
                // not the same event as switching the modulator off.
                if (fanout > 0) {
                    std::snprintf(msg, sizeof(msg), "L%d - %s (%d LEFT)",
                                  slot + 1, id.c_str(), fanout);
                } else {
                    std::snprintf(msg, sizeof(msg), "L%d OFF", slot + 1);
                }
                break;
            case BindResult::Rejected:
                // The only rejection is an LFO pointed at its own knobs, and
                // saying so beats a gesture that silently does nothing.
                std::snprintf(msg, sizeof(msg), "L%d CANNOT MODULATE ITSELF",
                              slot + 1);
                break;
        }
        status = msg;
        std::printf("[mod] %s\n", msg);
        std::fflush(stdout);
        // Assigning something not in the bank right now is normal, not an
        // error -- the tail changes with the kernel and the chain. The panel
        // draws it in ALERT so it is visible rather than mysterious.
        // ...but only once there IS a dynamic tail. At startup NV_LFO is read
        // before the first chain is built, so every effect knob would be
        // reported missing by an engine that is simply not finished starting.
        if (r == BindResult::Assigned && params.find(id) < 0 &&
            params.count() > params.globalEnd()) {
            std::printf("[mod] %s is not in the bank right now (inert until it "
                        "is)\n", id.c_str());
        }
        refreshModInfo();
    }

    const KernelDef& kernel() const {
        return kernelBank()[sel.kernelIndex];
    }

    // The morph target, or null when morphing is off or the pair cannot be
    // interpolated. Null means the shader compiles without the B side at all.
    const KernelDef* morphTarget() const {
        if (sel.kernelB == sel.kernelIndex) return nullptr;
        const KernelDef& b = kernelBank()[sel.kernelB];
        if (!canMorph(kernel(), b)) return nullptr;
        return &b;
    }

    const KernelDef* channelBKernel() const {
        if (sel.sourceB != SourceKind::Fractal) return nullptr;
        const auto& bank = kernelBank();
        if (bank.empty()) return nullptr;
        int i = std::clamp(sel.channelBKernel, 0,
                           static_cast<int>(bank.size()) - 1);
        return &bank[i];
    }

    void applyKernel() {
        renderer.setKernel(kernel(), static_cast<ColorMode>(sel.colorMode),
                           morphTarget());
        if (sel.sourceB == SourceKind::Fractal) {
            renderer.setChannelBKernel(
                kernelBank()[std::clamp(sel.channelBKernel, 0,
                                        static_cast<int>(kernelBank().size()) - 1)],
                static_cast<ColorMode>(sel.colorMode));
        }
        renderer.governor().setDetailEffective(kernel().kind == KernelKind::Escape);
    }

    void setSource(int channel, int kind) {
        if (channel < 0 || channel > 1) return;
        SourceKind requested = SourceKind::None;
        if (kind >= static_cast<int>(SourceKind::Fractal) &&
            kind <= static_cast<int>(SourceKind::Stream)) {
            requested = static_cast<SourceKind>(kind);
        }
        if (channel == 0) {
            // Only FRACTAL is live in this slice. Keep the selection truthful
            // until video/draw/stream adapters exist; pretending a stream is
            // rendering while silently drawing a fractal is worse than the
            // documented fallback.
            sel.sourceA = SourceKind::Fractal;
        } else {
            sel.sourceB = requested == SourceKind::Fractal
                              ? SourceKind::Fractal : SourceKind::None;
            if (sel.sourceB == SourceKind::Fractal) applyKernel();
        }
        rebuild();
        renderer.governor().reset();
    }

    void cycleMorphTarget(int delta) {
        const auto& bank = kernelBank();
        int n = static_cast<int>(bank.size());
        // Step to the next kernel that can actually be morphed with the
        // current one, so the control never lands somewhere inert.
        for (int i = 0; i < n; ++i) {
            sel.kernelB = ((sel.kernelB + delta) % n + n) % n;
            if (canMorph(kernel(), bank[sel.kernelB])) break;
        }
        applyKernel();
        rebuild();
    }

    // Any structural change goes through here: the parameter bank and the
    // compiled chain are rebuilt together, so they can never disagree about
    // which knob belongs to which effect.
    void rebuild() {
        rebuildPatchParams(params, kernel(), morphTarget(), chain,
                           channelBKernel());
        renderer.requestChain(chain);
        // Which rows DANGLE is a fact about the bank, and the bank just
        // changed. Without this the MAP page and the TARGET buttons would keep
        // reporting the previous patch's missing knobs.
        refreshModInfo();
    }

    void cycleKernel(int delta) {
        const auto& bank = kernelBank();
        int n = static_cast<int>(bank.size());
        sel.kernelIndex = ((sel.kernelIndex + delta) % n + n) % n;
        // Moving A may invalidate the morph pair (escape vs field).
        applyKernel();
        // Global params keep their values AND their indices across the
        // change; only the dynamic tail is rebuilt, preserving by id. That is
        // what makes moving between fractals feel continuous.
        rebuild();
        renderer.governor().reset();
    }

    // Absolute versions of the cycles above, for a control surface that knows
    // what it wants rather than a key that steps. Wrapped rather than clamped
    // so an out-of-range index from the wire cannot land the instrument on a
    // silent edge case.
    void setKernel(int index) {
        int n = static_cast<int>(kernelBank().size());
        int next = ((index % n) + n) % n;
        if (next == sel.kernelIndex) return;
        sel.kernelIndex = next;
        applyKernel();
        rebuild();
        renderer.governor().reset();
    }

    // Every discrete selection reachable ABSOLUTELY, by index, and wrapped
    // rather than clamped so a stray index off the wire cannot park the
    // instrument on a silent edge case. These exist because a remote surface
    // with only PREV and NEXT has to step through thirty entries to reach one,
    // which is not an instrument, it is a menu.
    void setKernelB(int index) {
        int n = static_cast<int>(kernelBank().size());
        int next = ((index % n) + n) % n;
        if (next == sel.kernelB) return;
        sel.kernelB = next;
        applyKernel();
        rebuild();
    }

    void setChannelBKernel(int index) {
        int n = static_cast<int>(kernelBank().size());
        int next = ((index % n) + n) % n;
        if (next == sel.channelBKernel) return;
        sel.channelBKernel = next;
        applyKernel();
        rebuild();
        renderer.governor().reset();
    }

    void setColorMode(int index) {
        int n = static_cast<int>(ColorMode::kCount);
        int next = ((index % n) + n) % n;
        if (next == sel.colorMode) return;
        sel.colorMode = next;
        applyKernel();
    }

    void setTrapShape(int index) {
        int n = static_cast<int>(TrapShape::kCount);
        sel.trapShape = ((index % n) + n) % n;
    }

    void setPalette(int which, int index) {
        int n = static_cast<int>(paletteBank().size());
        int next = ((index % n) + n) % n;
        if (which == 0) sel.paletteA = next;
        else            sel.paletteB = next;
        renderer.setPalettes(sel.paletteA, sel.paletteB);
    }

    // What is IN each bank, on request. The names have to travel: a browser
    // that carried its own copy of the generator list would go stale the day
    // an entry is added here, and there would then be two lists to keep in
    // step -- which is the failure this whole engine is arranged to avoid.
    void sendLists(const std::string& what) {
        auto want = [&](const char* w) {
            return what.empty() || what == w;
        };
        // The PARAMETER bank, described rather than guessed.
        //
        // `/nv/state` carries an id and a number, which is enough to light a
        // fader and nothing else -- not what to call it, not which section it
        // belongs in, not whether it is bipolar. So every remote surface had
        // to re-derive all of that from the id, and web/static/video.js grew a
        // NICE table of hand-written labels, a GROUPS table, and three regexes
        // guessing bipolarity from suffixes like `.cre` and `.drift`. Every
        // one of those is a copy of something the engine already knows, kept
        // in step by hand -- which is exactly how JRNY came to exist on the
        // panel and not on the web page.
        //
        // Sent as ONE list so a surface learns the shape in a single ask. The
        // name field packs the rest with '|' because listDatagrams carries two
        // strings and the append-only rule says add a bank, not a wire type.
        // '|' appears in no label in the bank; group and label are the
        // engine's own strings, so a new knob is drawn correctly by a page
        // that has never heard of it.
        if (want("param")) {
            std::vector<ListEntry> e;
            for (int i = 0; i < params.count(); ++i) {
                const ParamDef& d = params.def(i);
                // Hidden entries are the padding that keeps uKernelP
                // four-wide -- the same ones stateDatagrams skips.
                if (d.hidden) continue;
                char meta[192];
                std::snprintf(meta, sizeof(meta), "%s|%s|%g|%g|%g",
                              d.group.c_str(), d.label.c_str(),
                              d.min, d.max, d.def);
                e.push_back({d.id, meta});
            }
            receiver.sendToPeer(Receiver::listDatagrams("param", e));
        }
        if (want("kernel")) {
            std::vector<ListEntry> e;
            for (const auto& k : kernelBank()) e.push_back({k.id, k.name});
            receiver.sendToPeer(Receiver::listDatagrams("kernel", e));
        }
        if (want("palette")) {
            std::vector<ListEntry> e;
            for (const auto& p : paletteBank()) e.push_back({p.name, p.name});
            receiver.sendToPeer(Receiver::listDatagrams("palette", e));
        }
        if (want("color")) {
            std::vector<ListEntry> e;
            for (int i = 0; i < static_cast<int>(ColorMode::kCount); ++i) {
                const char* n = colorModeName(static_cast<ColorMode>(i));
                e.push_back({n, n});
            }
            receiver.sendToPeer(Receiver::listDatagrams("color", e));
        }
        if (want("trap")) {
            std::vector<ListEntry> e;
            for (int i = 0; i < static_cast<int>(TrapShape::kCount); ++i) {
                const char* n = trapShapeName(static_cast<TrapShape>(i));
                e.push_back({n, n});
            }
            receiver.sendToPeer(Receiver::listDatagrams("trap", e));
        }
        if (want("mode")) {
            std::vector<ListEntry> e;
            for (int i = 0; i < static_cast<int>(Mode::kCount); ++i) {
                const char* n = modeName(static_cast<Mode>(i));
                e.push_back({n, n});
            }
            receiver.sendToPeer(Receiver::listDatagrams("mode", e));
        }
        if (want("op")) {
            std::vector<ListEntry> e;
            for (const auto& op : jitOpBank()) e.push_back({op.id, op.name});
            receiver.sendToPeer(Receiver::listDatagrams("op", e));
        }
    }

    void setMode(int index) {
        int n = static_cast<int>(Mode::kCount);
        Mode next = static_cast<Mode>(((index % n) + n) % n);
        if (next == sel.mode) return;
        sel.mode = next;
        renderer.governor().reset();
        std::printf("[nv] mode %s\n", modeName(sel.mode));
        std::fflush(stdout);
    }

    // Every remote command lands here, so the one place that knows what a
    // preset load does to the bank is still the only place that does it.
    void applyCommand(const Command& c) {
        switch (c.kind) {
            case CommandKind::Preset: loadPreset(c.index); break;
            case CommandKind::Mode:   setMode(c.index); break;
            case CommandKind::Kernel: setKernel(c.index); break;
            case CommandKind::KernelB: setKernelB(c.index); break;
            case CommandKind::ChannelBKernel: setChannelBKernel(c.index); break;
            case CommandKind::Color:   setColorMode(c.index); break;
            case CommandKind::Trap:    setTrapShape(c.index); break;
            case CommandKind::Palette: setPalette(c.index, c.index2); break;
            case CommandKind::Op: {
                int i = params.find(gid::kOpMode);
                if (i >= 0) {
                    int n = static_cast<int>(jitOpBank().size());
                    params.setValueNow(i, static_cast<float>(((c.index % n) + n) % n));
                }
                break;
            }
            case CommandKind::Source: setSource(c.index, c.index2); break;
            // Answered where the banks are, which is here. The receiver owns
            // the wire and deliberately knows about none of them.
            case CommandKind::ListRequest: sendLists(c.id); break;
            case CommandKind::LfoTarget:
                setLfoTarget(c.index, c.id, /*viaLearn=*/false);
                break;
            // Parameters were applied inside poll(), against the bank
            // directly -- they need nothing this struct owns.
            case CommandKind::Param:  break;
        }
    }

    void cyclePalette(int which, int delta) {
        int n = static_cast<int>(paletteBank().size());
        if (which == 0) sel.paletteA = ((sel.paletteA + delta) % n + n) % n;
        else            sel.paletteB = ((sel.paletteB + delta) % n + n) % n;
        renderer.setPalettes(sel.paletteA, sel.paletteB);
    }

    void nudge(const char* id, float delta) {
        int i = params.find(id);
        if (i >= 0) params.nudgeNorm(i, delta);
    }

    // One place decides what a navigation step means, whichever transport it
    // arrived on -- joystick CC, a finger, or a development keyboard.
    void applyNav(Nav n) {
        if (view == View::Home) {
            switch (n) {
                case Nav::Prev: home.move(-1); break;
                case Nav::Next: home.move(+1); break;
                case Nav::Select: activateHome(); break;
                // Already home. Saying so beats silence: the user pressed
                // something and needs to know it was heard.
                case Nav::Home: home.setMessage("ALREADY AT HOME"); break;
                default: break;
            }
            return;
        }
        // In a mode: the only navigation that means anything is "get me out".
        if (n == Nav::Home || n == Nav::Back) {
            view = View::Home;
            home.clearMessage();
            std::printf("[home] returned to HOME\n");
            std::fflush(stdout);
        }
    }

    void activateHome() {
        switch (home.activate()) {
            case HomeAction::EnterVideo:
                // In-process, so there is no handoff and no wait. This is the
                // reason HOME lives inside the engine at all.
                view = View::Synth;
                home.clearMessage();
                std::printf("[home] -> VIDEO (in process)\n");
                std::fflush(stdout);
                break;
            case HomeAction::LaunchAudio:
                if (cmds.haveAudio()) {
                    pendingLaunch = cmds.audio;
                    pendingLabel = "AUDIO";
                } else {
                    home.setMessage("NO AUDIO COMMAND - SEE deploy/README");
                }
                break;
            case HomeAction::LaunchGames:
                if (cmds.haveGames()) {
                    pendingLaunch = cmds.games;
                    pendingLabel = "GAMES";
                } else {
                    home.setMessage("NO GAMES COMMAND - SEE deploy/README");
                }
                break;
            case HomeAction::LaunchNorns:
                if (cmds.haveNorns()) {
                    pendingLaunch = cmds.norns;
                    pendingLabel = "NORNS";
                } else {
                    home.setMessage("NO NORNS COMMAND - SEE deploy/README");
                }
                break;
            case HomeAction::Unavailable:
                home.setMessage("NOT BUILT YET");
                break;
            default:
                break;
        }
    }

    void onKey(const KeyEvent& e) {
        if (!e.pressed) return;

        // HOME owns the keyboard while it is up: the synth's forty bindings
        // would otherwise edit a patch nobody can see.
        if (view == View::Home) {
            switch (e.key) {
                case KeyUp: case '[': applyNav(Nav::Prev); return;
                case KeyDown: case ']': applyNav(Nav::Next); return;
                case '\r': case '\n': case ' ': applyNav(Nav::Select); return;
                case KeyEscape: running = false; return;
                default: return;
            }
        }

        switch (e.key) {
            // From a mode, ESC goes HOME rather than quitting: on the device
            // quitting drops the display to a bare console, which is not
            // something any control should do by accident.
            //
            // An armed LEARN takes it first. ESC is the universal "get me out
            // of this", and the modal state is the innermost thing to be out
            // of -- leaving the page you were on as well would be two things
            // happening for one press.
            case KeyEscape:
                if (learn.armed()) {
                    learn.cancel();
                    status = "LEARN CANCELLED";
                } else {
                    applyNav(Nav::Home);
                }
                break;
            // Arm LEARN, stepping through the LFOs and then off. The keyboard
            // equivalent of tapping a TARGET button -- same state, same
            // escape, so a development machine with no touchscreen can drive
            // and verify the identical path the device uses.
            case 'L': {
                int next = learn.armed() ? learn.slot() + 1 : 0;
                if (next >= kLfoCount) {
                    learn.cancel();
                    status = "LEARN CANCELLED";
                } else {
                    learn.arm(next, clock);
                    char m[64];
                    std::snprintf(m, sizeof(m), "L%d LEARN", next + 1);
                    status = m;
                    std::printf("[mod] %s armed -- touch a parameter\n", m);
                    std::fflush(stdout);
                }
                break;
            }
            // TAB walks the pages. With a dedicated control surface the panel
            // is always up, so a show/hide toggle had nothing to do.
            case KeyTab: panel.cyclePage(e.shift ? -1 : +1); break;
            case 'M':
                panel.setPage(panel.page() == Page::Input ? Page::Main : Page::Input);
                break;
            case 'V':
                sel.mode = static_cast<Mode>(
                    (static_cast<int>(sel.mode) + 1) % static_cast<int>(Mode::kCount));
                std::printf("[nv] mode %s\n", modeName(sel.mode));
                std::fflush(stdout);
                renderer.governor().reset();
                break;
            case 'X':
                // Scatter the swarm / wipe the canvas -- whichever mode owns
                // the surface right now.
                if (sel.mode == Mode::Particles) renderer.particles().seed();
                else if (sel.mode == Mode::Draw) renderer.ink().clear();
                break;

            case '[': cycleKernel(-1); break;
            case ']': cycleKernel(+1); break;
            case '9': cycleMorphTarget(-1); break;
            case '\\': cycleMorphTarget(+1); break;
            case 'G': nudge(gid::kMorph, -0.05f); break;
            case 'H': nudge(gid::kMorph, +0.05f); break;

            case ';': cyclePalette(0, -1); break;
            case '\'': cyclePalette(0, +1); break;
            case ',': cyclePalette(1, -1); break;
            case '.': cyclePalette(1, +1); break;

            case 'C':
                sel.colorMode = (sel.colorMode + 1) % static_cast<int>(ColorMode::kCount);
                // The mode decides which intrinsic features the inner loop
                // computes, so it selects a different compiled program.
                applyKernel();
                renderer.governor().reset();
                break;
            case 'T':
                sel.trapShape = (sel.trapShape + 1) % static_cast<int>(TrapShape::kCount);
                break;

            case KeyLeft:  nudge(gid::kCenterX, -0.005f); break;
            case KeyRight: nudge(gid::kCenterX, +0.005f); break;
            case KeyUp:    nudge(gid::kCenterY, +0.005f); break;
            case KeyDown:  nudge(gid::kCenterY, -0.005f); break;

            case '=': nudge(gid::kZoom, +0.01f); break;
            case '-': nudge(gid::kZoom, -0.01f); break;
            case 'Q': nudge(gid::kDetail, -0.02f); break;
            case 'W': nudge(gid::kDetail, +0.02f); break;
            case 'A': nudge(gid::kRenderScale, -0.02f); break;
            case 'S': nudge(gid::kRenderScale, +0.02f); break;
            case 'E': nudge(gid::kRotation, -0.005f); break;
            case 'R': nudge(gid::kRotation, +0.005f); break;
            case 'P': nudge(gid::kPaletteMix, +0.05f); break;
            case 'O': nudge(gid::kPaletteMix, -0.05f); break;
            case 'K': nudge(gid::kPaletteShift, +0.02f); break;

            case '0': params.resetToDefaults(); break;

            default:
                // 1..9 bypass the corresponding chain slot. Bypass is free: a
                // disabled slot is not compiled into any pass at all.
                if (e.key >= '1' && e.key <= '8') {
                    int slot = e.key - '0';
                    // Digit alone bypasses an effect; with a modifier the same
                    // key is a patch button, which is how an instrument works.
                    if (e.ctrl) {
                        savePreset(slot);
                    } else if (e.shift) {
                        loadPreset(slot);
                    } else {
                        size_t i = static_cast<size_t>(slot - 1);
                        if (i < chain.size()) {
                            chain[i].enabled = !chain[i].enabled;
                            rebuild();
                        }
                    }
                }
                break;
        }
    }
};

// Reads the scene back and writes a binary PPM.
//
// Exists because verifying "does it look right" through the window server is
// unreliable -- an occluded window may not even be composited. This captures
// what the ENGINE produced, not what the desktop happened to be showing, and
// it doubles as frame export.
bool writePPM(const char* path, int w, int h) {
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());

    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    // glReadPixels origin is bottom-left; PPM is top-down.
    for (int y = h - 1; y >= 0; --y) {
        for (int x = 0; x < w; ++x) {
            const unsigned char* p = &px[(static_cast<size_t>(y) * w + x) * 4];
            std::fputc(p[0], f);
            std::fputc(p[1], f);
            std::fputc(p[2], f);
        }
    }
    std::fclose(f);
    return true;
}

// ---- scripted input -------------------------------------------------------
//
// NV_SCRIPT="10:tap:0.12,0.31;40:key:3+ctrl" fires taps and key presses at
// given frame numbers, through the same two callbacks a finger and a keyboard
// go through.
//
// It exists because the touch paths are the one part of this engine that
// cannot be checked from a test binary (they need a laid-out panel, which
// needs GL) and cannot be checked by hand in a scripted run either. Paired
// with NV_SHOT it turns "I touched it and it worked" into a frame on disk. It
// is not a second input mechanism: it builds a PointerEvent/KeyEvent and hands
// it to the handler the platform hands its own to.
struct ScriptStep {
    int frame = 0;
    bool isKey = false;
    bool isSet = false;         // set a parameter in REAL units at this frame
    float x = 0.0f, y = 0.0f;   // tap, normalized
    int key = 0;
    bool ctrl = false, shift = false;
    std::string id;             // isSet
    float value = 0.0f;         // isSet
};

int scriptKeyCode(const std::string& s) {
    if (s == "TAB") return KeyTab;
    if (s == "ESC") return KeyEscape;
    if (s == "UP") return KeyUp;
    if (s == "DOWN") return KeyDown;
    if (s == "LEFT") return KeyLeft;
    if (s == "RIGHT") return KeyRight;
    if (s.size() == 1) return std::toupper(static_cast<unsigned char>(s[0]));
    return 0;
}

std::vector<std::string> splitOn(const std::string& s, char c) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i <= s.size()) {
        size_t j = s.find(c, i);
        if (j == std::string::npos) j = s.size();
        out.push_back(s.substr(i, j - i));
        i = j + 1;
    }
    return out;
}

std::vector<ScriptStep> parseScript(const std::string& spec) {
    std::vector<ScriptStep> out;
    for (const auto& item : splitOn(spec, ';')) {
        if (item.empty()) continue;
        std::vector<std::string> f = splitOn(item, ':');
        if (f.size() < 3) continue;
        ScriptStep st;
        st.frame = std::atoi(f[0].c_str());
        if (f[1] == "tap") {
            std::vector<std::string> xy = splitOn(f[2], ',');
            if (xy.size() < 2) continue;
            st.x = static_cast<float>(std::atof(xy[0].c_str()));
            st.y = static_cast<float>(std::atof(xy[1].c_str()));
        } else if (f[1] == "set") {
            // `<frame>:set:<id>=<real value>`. NV_SET does the same thing at
            // startup; this does it AT A FRAME, which is the only way to prove
            // that a knob move does not make the picture jump -- the claim is
            // about the boundary between two frames, and a value that was
            // already there before frame 0 has no boundary to inspect.
            std::vector<std::string> kv = splitOn(f[2], '=');
            if (kv.size() < 2) continue;
            st.isSet = true;
            st.id = kv[0];
            st.value = static_cast<float>(std::atof(kv[1].c_str()));
        } else if (f[1] == "key") {
            st.isKey = true;
            std::vector<std::string> parts = splitOn(f[2], '+');
            st.key = scriptKeyCode(parts[0]);
            for (size_t i = 1; i < parts.size(); ++i) {
                if (parts[i] == "ctrl") st.ctrl = true;
                if (parts[i] == "shift") st.shift = true;
            }
            if (st.key == 0) continue;
        } else {
            continue;
        }
        out.push_back(st);
    }
    return out;
}

void printBanner() {
    // One machine-readable line, before the human-readable list. A test
    // harness scraping the two-column banner had already miscounted the bank
    // twice by the time this was added.
    std::printf("[nv] kernels %zu colors %d traps %d palettes %zu modes %d\n",
                kernelBank().size(), static_cast<int>(ColorMode::kCount),
                static_cast<int>(TrapShape::kCount), paletteBank().size(),
                static_cast<int>(Mode::kCount));
    std::printf(
        "NEPTR VIDEO\n"
        "  [ ]      generator      9 \\   morph target   G H   morph amount\n"
        "  C        colour mode    T     trap shape     ; '   palette A\n"
        "  , .      palette B      O P   palette mix    K     palette rotate\n"
        "  arrows   pan            - =   zoom           E R   rotate\n"
        "  Q W      detail         A S   resolution     0     reset params\n"
        "  1-8      bypass effect  M     input monitor\n"
        "  V        mode           X     reset layer    TAB   panel page\n"
        "  L        arm LFO learn, then touch a parameter (ESC cancels)\n"
        "  SHIFT+1-8  load preset   CTRL+1-8  save preset\n"
        "  ESC      quit\n"
        "\n  MODES: SYNTH (generator only) / PARTICLES (swarm steered by the\n"
        "  generator field) / DRAW (paint; the brush takes the generator's\n"
        "  colour). Both layers composite BEFORE the effect chain, so feedback\n"
        "  and warp process them too. Drag on the SCENE surface to draw.\n\n");

    // The bank is long enough that cycling blind is no way to find anything.
    std::printf("GENERATORS (NV_KERNEL=n)\n");
    const auto& bank = kernelBank();
    std::string fam;
    for (size_t i = 0; i < bank.size(); ++i) {
        if (bank[i].family != fam) {
            fam = bank[i].family;
            std::printf("\n  %s\n", fam.c_str());
        }
        std::printf("   %2zu %-22s%s", i, bank[i].name.c_str(),
                    (i % 2) ? "\n" : "");
    }
    std::printf("\n\nCOLOUR MODES (NV_COLOR=n)\n  ");
    for (int i = 0; i < static_cast<int>(ColorMode::kCount); ++i) {
        std::printf("%d %-8s", i, colorModeName(static_cast<ColorMode>(i)));
    }
    std::printf("\n\n");
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    // Dump the bank and exit, BEFORE any platform is created. Tooling that
    // needs to iterate the generators (tests/depth_sweep.py) must be able to
    // ask what they are without taking a display to find out.
    if (std::getenv("NV_LIST_KERNELS")) {
        for (const KernelDef& k : kernelBank()) {
            std::printf("%s\t%s\t%s\t%.1f\n", k.id.c_str(), k.name.c_str(),
                        k.kind == KernelKind::Field ? "field" : "escape",
                        k.journeyDepth);
        }
        return 0;
    }

    App app;
    std::unique_ptr<Platform> plat(createPlatform());

    if (!plat->init()) {
        std::fprintf(stderr, "platform init failed\n");
        return 1;
    }

    const IoProfile ioProfile = ioProfileFromEnv();
    std::printf("[io] boundary %s %dx%d @ %dfps %dkbps %dms auto=%d\n",
                ioTransportName(ioProfile.transport), ioProfile.width,
                ioProfile.height, ioProfile.fps, ioProfile.bitrateKbps,
                ioProfile.latencyMs, ioProfile.autoAdapt ? 1 : 0);

    int maxW = 0, maxH = 0;
    plat->maxSurface(maxW, maxH);
    std::printf("[nv] buffers allocated once at %dx%d\n", maxW, maxH);

    const int ctxCount = plat->contextCount();
    // Pool size is a launch-time choice: it decides how finely a "tight"
    // swarm can reconstruct an image, and it is the number whose ceiling the
    // Pi actually has to be measured against.
    int poolDim = 256;
    if (const char* v = std::getenv("NV_POOL")) {
        poolDim = std::max(16, std::min(2048, std::atoi(v)));
    }
    app.renderer.setPoolDim(poolDim);
    if (!app.renderer.init(maxW, maxH, ctxCount)) {
        std::fprintf(stderr, "renderer init failed\n");
        return 1;
    }
    // The panel is bolted down at some angle; everything above the platform
    // seam already lays out in the rotated logical size, so only the two
    // passes that touch the real framebuffer need to know.
    app.renderer.setRotation(plat->rotation());
    app.panel.setRotation(plat->rotation());
    if (!app.panel.init(ctxCount)) {
        std::fprintf(stderr, "panel init failed\n");
        return 1;
    }

    // If the platform can hand the worker a shared context, chain edits stop
    // touching the render thread's critical path entirely. NV_INLINE_COMPILE
    // forces the fallback path, so the two can be compared directly.
    if (std::getenv("NV_INLINE_COMPILE")) {
        std::printf("[nv] shader compilation forced inline\n");
    } else if (app.compiler.start(plat.get())) {
        app.renderer.attachCompiler(&app.compiler);
        std::printf("[nv] shader compilation is off-thread\n");
    }

    registerGlobalParams(app.params);

    // Control ingress. NV_MAPPINGS points at a mapping file; without one the
    // built-in defaults follow the CC block the audio engine already uses, so
    // an attached controller works with no configuration.
    if (const char* mapPath = std::getenv("NV_MAPPINGS")) {
        std::string err;
        if (!app.mappings.loadFile(mapPath, &err)) {
            std::fprintf(stderr, "[ctrl] %s\n", err.c_str());
            return 1;  // an explicitly requested mapping file failing is fatal
        }
        std::printf("[ctrl] %zu mappings from %s\n", app.mappings.size(), mapPath);
    } else {
        app.mappings.loadDefaults();
    }

    // Start on the monitor page. Useful when the first thing you want to know
    // is whether a controller is reaching the engine at all.
    if (std::getenv("NV_MONITOR")) app.panel.setPage(Page::Input);
    // ...and the same hatch for any page. NV_MONITOR is this for exactly one
    // of them; a panel page is otherwise only reachable by pressing TAB, which
    // means a scripted run -- a screenshot, a check that a new page lays out at
    // all -- cannot see it.
    if (const char* v = std::getenv("NV_PAGE")) {
        int n = static_cast<int>(Page::kCount);
        app.panel.setPage(static_cast<Page>(((std::atoi(v) % n) + n) % n));
    }

    // Open on a specific fractal / colouring rather than always cycling to it
    // from the keyboard.
    if (const char* v = std::getenv("NV_KERNEL")) {
        int n = static_cast<int>(kernelBank().size());
        app.sel.kernelIndex = ((std::atoi(v) % n) + n) % n;
    }
    if (const char* v = std::getenv("NV_MORPH")) {
        int i = app.params.find(gid::kMorph);
        if (i >= 0) app.params.setNorm(i, static_cast<float>(std::atof(v)));
    }
    if (const char* v = std::getenv("NV_KERNEL_B")) {
        int n = static_cast<int>(kernelBank().size());
        app.sel.kernelB = ((std::atoi(v) % n) + n) % n;
    }
    if (const char* v = std::getenv("NV_KERNEL_B2")) {
        int n = static_cast<int>(kernelBank().size());
        app.sel.channelBKernel = ((std::atoi(v) % n) + n) % n;
    }
    if (const char* v = std::getenv("NV_SOURCE_B")) {
        app.setSource(1, std::atoi(v));
    }
    if (const char* v = std::getenv("NV_OP")) {
        int i = app.params.find(gid::kOpMode);
        int n = static_cast<int>(jitOpBank().size());
        if (i >= 0) app.params.setValueNow(i, static_cast<float>((std::atoi(v) % n + n) % n));
    }
    // NV_SET="id=value,id=value" sets any parameter by its id, in REAL units.
    // One escape hatch instead of an env var per knob.
    auto applyEnvSet = [&app](const char* text, bool reportMissing) {
        if (!text) return;
        std::string spec(text);
        size_t i = 0;
        while (i < spec.size()) {
            size_t comma = spec.find(',', i);
            if (comma == std::string::npos) comma = spec.size();
            std::string item = spec.substr(i, comma - i);
            size_t eq = item.find('=');
            if (eq != std::string::npos) {
                std::string id = item.substr(0, eq);
                float val = static_cast<float>(std::atof(item.c_str() + eq + 1));
                int pi = app.params.find(id);
                if (pi >= 0) app.params.setValue(pi, val);
                else if (reportMissing)
                    std::fprintf(stderr, "[nv] no such parameter: %s\n", id.c_str());
            }
            i = comma + 1;
        }
    };
    if (const char* v = std::getenv("NV_SET")) {
        // Globals and operator controls already exist. Dynamic A/B/morph and
        // effect ids are installed after the selection and chain are rebuilt;
        // the same id-addressed grammar is applied again at that seam.
        applyEnvSet(v, true);
    }
    if (const char* v = std::getenv("NV_PRESETS")) app.presetDir = v;
    if (const char* v = std::getenv("NV_LOAD")) app.loadPreset(std::atoi(v));

    // NV_LFO="0=g.palette_shift,2=fx0.amount" assigns modulation targets. The
    // rest of an LFO -- rate, depth, phase, shape -- is in the parameter bank
    // and therefore already reachable through NV_SET, so this covers the one
    // part of a modulator that is a NAME rather than a number. Same shape as
    // NV_SET on purpose: one escape hatch grammar, not two.
    if (const char* v = std::getenv("NV_LFO")) {
        std::string spec(v);
        size_t i = 0;
        while (i < spec.size()) {
            size_t comma = spec.find(',', i);
            if (comma == std::string::npos) comma = spec.size();
            std::string item = spec.substr(i, comma - i);
            size_t eq = item.find('=');
            if (eq != std::string::npos) {
                int slot = std::atoi(item.substr(0, eq).c_str());
                std::string id = item.substr(eq + 1);
                // Through the same door as the panel and the wire, so the env
                // hatch cannot drift from what the instrument itself does --
                // it reports the same way and obeys the same rules.
                app.setLfoTarget(slot, id, /*viaLearn=*/false);
            }
            i = comma + 1;
        }
    }
    // Deliberately AFTER NV_LOAD: a preset carries its own LFO targets, and
    // the env var is the escape hatch you reach for to override what a preset
    // says, not something a preset should silently undo.
    const char* saveSlot = std::getenv("NV_SAVE");
    if (const char* v = std::getenv("NV_MODE")) {
        int n = static_cast<int>(Mode::kCount);
        app.sel.mode = static_cast<Mode>(((std::atoi(v) % n) + n) % n);
    }
    if (const char* v = std::getenv("NV_COLOR")) {
        int n = static_cast<int>(ColorMode::kCount);
        app.sel.colorMode = ((std::atoi(v) % n) + n) % n;
    }

    const char* portEnv = std::getenv("NV_OSC_PORT");
    int oscPort = portEnv ? std::atoi(portEnv) : kDefaultSensorPort;
    if (app.receiver.open(oscPort)) {
        std::printf("[ctrl] listening for OSC on %d\n", oscPort);
    }

    // ---- HOME ------------------------------------------------------------
    // The engine boots into the mode selector. The synth is one entry in it,
    // reached in-process with no handoff at all.
    app.cmds = loadHomeCommands();
    app.nav.setInvertY(std::getenv("NV_HOME_INVERT_Y") != nullptr);
    std::printf("[home] AUDIO: %s\n",
                app.cmds.haveAudio() ? app.cmds.audio.c_str()
                                     : "(none -- deploy/mode-audio.sh missing)");
    std::printf("[home] GAMES: %s\n",
                app.cmds.haveGames() ? app.cmds.games.c_str()
                                     : "(none -- deploy/mode-games.sh missing)");
    std::printf("[home] NORNS: %s\n",
                app.cmds.haveNorns() ? app.cmds.norns.c_str()
                                     : "(none -- deploy/mode-norns.sh missing)");
    // The scripted runs are non-interactive by definition: a selftest or a
    // preset write that stopped on a menu would hang a deploy check forever.
    if (std::getenv("NV_SELFTEST") || saveSlot) app.view = View::Synth;
    if (const char* v = std::getenv("NV_HOME")) {
        app.view = (v[0] == '0') ? View::Synth : View::Home;
    }

    app.chain = defaultChain();
    // NV_CHAIN=none bypasses every slot at startup. The generator on its own
    // is the only way to ask "did the FRACTAL move" and get an answer: with
    // FEEDBACK and GRAIN in the default chain, consecutive frames differ
    // whatever the kernel is doing, so every kernel would pass a motion test
    // and the test would be worth nothing.
    if (const char* v = std::getenv("NV_CHAIN")) {
        if (std::string(v) == "none") {
            for (auto& s : app.chain) s.enabled = false;
        }
    }
    app.applyKernel();
    rebuildPatchParams(app.params, app.kernel(), app.morphTarget(), app.chain,
                       app.channelBKernel());
    applyEnvSet(std::getenv("NV_SET"), false);
    app.renderer.setPalettes(app.sel.paletteA, app.sel.paletteB);

    // The first chain is built synchronously: there is nothing to keep
    // drawing while we wait, so async would just show a blank first frame.
    if (!app.renderer.setChainBlocking(app.chain)) {
        std::fprintf(stderr, "chain compile failed\n");
        return 1;
    }

    app.refreshModInfo();
    // Modulation is invisible until it moves something, so say what is wired
    // at startup. A silent LFO and an unassigned one look identical on screen.
    for (int n = 0; n < kLfoCount; ++n) {
        for (const Mapping& m : lfoTargets(app.mappings, n)) {
            std::printf("[mod] LFO %d -> %-16s %.3f Hz  depth %.2f  span "
                        "%.2f..%.2f\n", n, m.param.c_str(),
                        lfoRateHz(app.params, n),
                        app.params.valueOf(lfoParamId(n, "depth"), 0.0f),
                        m.lo, m.hi);
        }
    }
    for (const auto& m : app.mappings.all()) {
        for (int n = 0; n < kMacroCount; ++n) {
            if (m.source != macroSource(n)) continue;
            std::printf("[mod] MACRO %d -> %-16s %.2f..%.2f\n", n,
                        m.param.c_str(), m.lo, m.hi);
        }
    }

    auto keyFn = [&app](const KeyEvent& e) { app.onKey(e); };
    plat->onKey(keyFn);
    // Touch on HOME: down highlights the entry, release inside the SAME entry
    // activates it. That is the gesture model the rest of the instrument
    // already uses (engine-ui.md: touch is note-on/note-off, and a drag off the
    // element cancels), so a finger behaves the same way on every screen.
    int homeTouched = -1;

    // Held rather than passed straight in, so a scripted run (NV_SCRIPT) can
    // feed the IDENTICAL handler real input goes through. A verification path
    // that entered somewhere else would be verifying itself.
    std::function<void(const PointerEvent&)> pointerFn =
        [&app, &plat, &homeTouched](const PointerEvent& e) {
        if (app.view == View::Home) {
            int hit = app.panel.homeEntryAt(e.x, e.y);
            if (!e.moved && e.down) {
                homeTouched = hit;
                if (hit >= 0) app.home.setSelected(hit);
            } else if (!e.moved && !e.down) {
                if (hit >= 0 && hit == homeTouched) app.activateHome();
                homeTouched = -1;
            } else if (e.down && hit >= 0 && hit == homeTouched) {
                app.home.setSelected(hit);
            }
            return;
        }

        const auto& o = plat->outputs();
        bool onScene = (e.outputIndex >= 0 && e.outputIndex < static_cast<int>(o.size()) &&
                        o[e.outputIndex].role == OutputRole::Scene);
        // Draw mode owns the SCENE surface; the panel keeps the control one,
        // so you can adjust the brush without painting with the same finger.
        if (app.sel.mode == Mode::Draw && onScene) {
            if (e.down) app.renderer.ink().addDab(e.x, e.y);
            else app.renderer.ink().endStroke();
            return;
        }
        // Only the surface actually showing the panel accepts drags. Without
        // this, dragging in the scene window would move whatever slider sits
        // at the same relative position on the other surface.
        const auto& outs = plat->outputs();
        bool onPanelSurface =
            (e.outputIndex >= 0 && e.outputIndex < static_cast<int>(outs.size()) &&
             outs[e.outputIndex].role == OutputRole::Panel);
        bool panelIsOverlay = (outs.size() == 1 && app.showPanel);
        if (!onPanelSurface && !panelIsOverlay) return;
        if (app.panel.page() == Page::Input) return;  // nothing to drag there

        // ---- LEARN ------------------------------------------------------
        // A tap on an LFO's TARGET button arms it; the next parameter you
        // touch, on any page, becomes that LFO's target. The bind is on the
        // DOWN edge and consumes the event, so the slider you named is never
        // also dragged by the finger that named it.
        if (!e.moved && e.down) {
            int b = app.panel.modTargetAt(e.x, e.y);
            if (b >= 0) {
                app.learn.toggle(b, plat->now());
                if (app.learn.armed()) {
                    std::printf("[mod] L%d LEARN armed -- touch a parameter\n",
                                b + 1);
                } else {
                    std::printf("[mod] LEARN cancelled\n");
                }
                std::fflush(stdout);
                return;
            }
            if (app.learn.armed()) {
                int i = app.panel.paramAt(e.x, e.y);
                if (i >= 0) {
                    app.setLfoTarget(app.learn.slot(), app.params.def(i).id,
                                     /*viaLearn=*/true);
                    app.learn.cancel();
                    return;
                }
                // Missed everything. NOT a cancel: a fat-fingered tap between
                // two rows should leave you still armed rather than silently
                // dropping the mode you are half-way through.
                return;
            }
        }
        // ---- GEN --------------------------------------------------------
        // A tap is a selection. Nothing here drags, so it is handled on the
        // DOWN edge and consumed: sliding a finger across the grid must not
        // sweep the instrument through six generators, each of which is a
        // shader compile.
        if (app.panel.page() == Page::Gen) {
            if (!e.moved && e.down) {
                int g = app.panel.genCellAt(e.x, e.y);
                if (g >= 0) app.setKernel(g);
            }
            return;
        }

        // While armed, a drag moves nothing. The one gesture the surface has
        // is spoken for, and a slider that moved as you named it would be a
        // parameter edit nobody asked for.
        if (app.learn.armed()) return;

        // ---- MAP --------------------------------------------------------
        // The routing zone. Two gestures, split by which half of the row the
        // finger is in: the NAME half removes a destination, the AMOUNT half
        // drags how much of the parameter's travel it gets.
        //
        // Both are refused on rows this instrument does not own. A sensor or
        // encoder mapping came from the performer's file, and a page that let
        // a stray tap delete it would make the file untrustworthy -- you would
        // never know whether what you edited last night is still there.
        if (app.panel.page() == Page::Map) {
            if (!e.moved && e.down) {
                bool onAmount = false;
                int row = app.panel.mapRowAt(e.x, e.y, &onAmount);
                if (row >= 0 && app.mapRowIsPatch(row)) {
                    if (onAmount) {
                        app.mapDrag = row;
                        app.dragMapAmount(row, app.panel.mapAmountAt(row, e.x));
                    } else {
                        app.removeMapRow(row);
                    }
                }
            } else if (e.moved && e.down && app.mapDrag >= 0) {
                app.dragMapAmount(app.mapDrag,
                                  app.panel.mapAmountAt(app.mapDrag, e.x));
            } else if (!e.down) {
                app.mapDrag = -1;
            }
            return;
        }

        if (!e.moved) {
            if (e.down) app.panel.onPointerDown(e.x, e.y);
            else app.panel.onPointerUp();
        } else if (e.down) {
            app.panel.onPointerMove(e.x, e.y, app.params);
        }
    };
    plat->onPointer(pointerFn);

    // The generator list goes to the panel once. It is constant for the life
    // of the process -- the bank is built at static-init and nothing adds to
    // it -- so pushing it per frame would be work the scene pays for.
    {
        std::vector<std::string> names;
        for (const auto& k : kernelBank()) names.push_back(k.name);
        app.panel.setGenerators(names, app.sel.kernelIndex, app.sel.kernelB);
    }

    if (const char* v = std::getenv("NV_GOVERNOR")) {
        if (std::atoi(v) == 0) {
            app.renderer.governor().setEnabled(false);
            std::printf("[nv] governor DISABLED (NV_GOVERNOR=0)\n");
        }
    }

    printBanner();

    // Which output shows what. On the Pi this comes from the connector roles;
    // on a single-output machine the panel becomes a toggleable overlay.
    int sceneIdx = 0, panelIdx = -1;
    {
        const auto& outs = plat->outputs();
        for (size_t i = 0; i < outs.size(); ++i) {
            if (outs[i].role == OutputRole::Panel) panelIdx = static_cast<int>(i);
            else sceneIdx = static_cast<int>(i);
        }
        // With ONE display there is nowhere to put the scene except where the
        // panel would go, so the panel starts hidden and the visuals show.
        // Defaulting it visible made a panel-only rig unable to display the
        // instrument at all -- and under DRM there is no keyboard yet to
        // press anything to fix it. NV_PANEL=1 opens on the controls instead.
        if (panelIdx < 0) app.showPanel = (std::getenv("NV_PANEL") != nullptr);
    }

    // NV_SELFTEST=1 drives chain edits on a timer and reports the worst frame
    // time. It exists because off-thread compilation is the one thing here
    // whose whole purpose is "you cannot see it happen" -- which makes it
    // exactly the thing that needs measuring rather than eyeballing.
    const bool selftest = std::getenv("NV_SELFTEST") != nullptr;
    int selftestFrame = 0;
    int selftestEdits = 0;
    std::vector<double> frameMs;
    std::vector<MonitorRow> monitor;

    // The governor's threshold is a multiple of the REAL refresh interval, so
    // it has to come from the display rather than an assumed 60.
    {
        const auto& outs = plat->outputs();
        if (!outs.empty() && outs[sceneIdx].refreshHz > 1.0) {
            app.renderer.governor().configure(outs[sceneIdx].refreshHz);
            std::printf("[nv] scene %s %dx%d @ %.0f Hz\n", outs[sceneIdx].name.c_str(),
                        outs[sceneIdx].width, outs[sceneIdx].height,
                        outs[sceneIdx].refreshHz);
        }
    }

    // Frame export / verification.
    const bool quarterTurn =
        (plat->rotation() == 90 || plat->rotation() == 270);
    std::vector<ScriptStep> script;
    if (const char* v = std::getenv("NV_SCRIPT")) {
        script = parseScript(v);
        std::printf("[script] %zu step%s\n", script.size(),
                    script.size() == 1 ? "" : "s");
    }

    const char* shotPath = std::getenv("NV_SHOT");
    const char* shotFrameEnv = std::getenv("NV_SHOT_FRAME");
    // A LIST, not a number. One frame per process was enough to check a
    // layout; it is not enough to check that time is reversible, because that
    // claim is about two frames of the SAME run. A single entry still writes
    // exactly the path given, so every existing invocation is unchanged.
    std::vector<int> shotFrames;
    if (shotFrameEnv) {
        for (const auto& f : splitOn(shotFrameEnv, ',')) {
            if (!f.empty()) shotFrames.push_back(std::atoi(f.c_str()));
        }
    }
    if (shotFrames.empty()) shotFrames.push_back(120);
    const int lastShotFrame = *std::max_element(shotFrames.begin(), shotFrames.end());
    auto shotIndexFor = [&](int frame) {
        for (size_t i = 0; i < shotFrames.size(); ++i) {
            if (shotFrames[i] == frame) return static_cast<int>(i);
        }
        return -1;
    };
    auto shotPathFor = [&](int idx) {
        if (shotFrames.size() == 1) return std::string(shotPath ? shotPath : "");
        std::string base = shotPath ? shotPath : "";
        size_t dot = base.rfind('.');
        std::string stem = (dot == std::string::npos) ? base : base.substr(0, dot);
        std::string ext = (dot == std::string::npos) ? "" : base.substr(dot);
        return stem + "." + std::to_string(idx) + ext;
    };

    // A FIXED frame interval, for the tests that have to be reproducible.
    // Everything about motion here is integrated from dt, which is exactly
    // right on stage and exactly wrong under a test: two runs of the same
    // script would reach different scene times and no frame could be compared
    // with any other. Unset -- which is every real run -- changes nothing.
    double fixedDt = 0.0;
    if (const char* v = std::getenv("NV_FIXED_DT")) fixedDt = std::atof(v);

    // Frame-time instrumentation, for comparing two builds of the inner loop.
    // The status line prints an averaged fps once a second, which cannot
    // resolve a few percent; this prints the real per-frame interval.
    const bool printFrameTime = std::getenv("NV_FRAMETIME") != nullptr;
    int maxFrames = 0;
    if (const char* v = std::getenv("NV_MAX_FRAMES")) maxFrames = std::atoi(v);

    // NV_MAX_SECONDS: a WALL-CLOCK lifetime cap, which is the right unit
    // for "do not hang forever". NV_MAX_FRAMES is a render-count cap, and
    // a render count stopped meaning a duration the moment headless runs
    // lost vsync -- 600 frames was ~10s behind a swap interval and is
    // ~0.2s without one, so a harness that starts the engine and then
    // sleeps was talking to a process that had already exited. Frames are
    // the unit you want for determinism; seconds for a safety net.
    double maxSeconds = 0.0;
    if (const char* v = std::getenv("NV_MAX_SECONDS")) maxSeconds = std::atof(v);
    double startClock = -1.0;

    int frameNo = 0;

    // Save once the state is fully built, then exit: lets a preset be written
    // from a scripted run rather than only by hand.
    if (saveSlot) {
        app.savePreset(std::atoi(saveSlot));
        app.running = false;
    }

    double last = plat->now();
    startClock = plat->now();
    double statTimer = 0.0;
    double sceneTime = 0.0;
    // The generator's own motion phase. Separate from sceneTime and wrapped
    // into its period, so it stays exact forever -- see MotionClock. It lives
    // out here with sceneTime for the same reason sceneTime does: a kernel
    // change, a chain recompile and a preset load must not touch it, or the
    // picture jumps at exactly the moment the performer changes something.
    MotionClock motionClock;
    // The zoom journey. Like motionClock it lives out here rather than in the
    // renderer, because a kernel change, a chain recompile and a preset load
    // must not touch where the dive has got to -- landing back at the surface
    // because the performer changed a colour mode is exactly the jump this
    // whole design is trying to make impossible.
    Journey journey;
    // Runtime boundary steering: opt-in, see the note at its use site.
    const bool steerEnabled = std::getenv("NV_STEER") != nullptr;
    // Forces every generator to the full five-decade dive regardless of its
    // own journeyDepth. This exists for the depth sweep that MEASURES those
    // per-kernel numbers in the first place -- without it the sweep could only
    // ever re-measure the values it was trying to establish.
    double journeyDepthOverride = -1.0;
    if (const char* v = std::getenv("NV_JOURNEY_DEPTH")) {
        journeyDepthOverride = std::atof(v);
    }
    // Pins the dive to one entry in the target table, for the sweep that
    // decides which entries are worth keeping.
    // Aim anywhere, and pin the travel. Together with NV_JOURNEY_DEPTH set to
    // the full span these make logZoom exactly the travel, which is what lets
    // a search harness render a named coordinate at a named depth instead of
    // flying there and hoping.
    if (const char* v = std::getenv("NV_JOURNEY_AIM")) {
        double ax = 0.0, ay = 0.0;
        if (std::sscanf(v, "%lf,%lf", &ax, &ay) == 2) journey.forceAim(ax, ay);
    }
    if (const char* v = std::getenv("NV_JOURNEY_TRAVEL")) {
        journey.setTravel(std::atof(v));
    }
    if (const char* v = std::getenv("NV_JOURNEY_TARGET")) {
        journey.forceTarget(std::atoi(v));
    }
    // Where the motion path STARTS is random per run. Two boots of the same
    // preset should not be the same performance: the phase decides which part
    // of the seed's circle, which exponent, which relaxation you come up on,
    // and starting every session at the same point made the instrument feel
    // like a recording. NV_MOTION_PHASE pins it, because a reversibility test
    // needs the same run twice and randomness is exactly what it cannot have.
    if (const char* v = std::getenv("NV_MOTION_PHASE")) {
        motionClock.setPhase(std::atof(v));
    } else {
        motionClock.setPhase(
            MotionClock::kPeriod *
            static_cast<double>(std::chrono::steady_clock::now()
                                    .time_since_epoch()
                                    .count() % 1000000) / 1000000.0);
    }
    int frames = 0;
    int fps = 0;
    FrameStats fs;

    while (app.running) {
        bool hotplugged = false;
        if (!plat->pollEvents(hotplugged)) break;

        const auto& outs = plat->outputs();
        if (outs.empty()) continue;
        if (sceneIdx >= static_cast<int>(outs.size())) sceneIdx = 0;
        const Output& scene = outs[sceneIdx];
        if (scene.width <= 0 || scene.height <= 0) continue;

        double t = plat->now();
        double dt = t - last;
        last = t;
        // The GPU runs ASYNCHRONOUSLY, so the loop's own dt measures how long
        // it took to SUBMIT a frame, not to draw one -- which is why the first
        // cut of this reported 0.4 ms for a 256-iteration Mandelbrot at 720p
        // and "measured" the motion as making it faster. glFinish at the end
        // of the previous frame is what makes dt the real cost.
        if (printFrameTime) std::printf("[ft] %.4f\n", dt * 1000.0);
        if (fixedDt > 0.0) dt = fixedDt;
        app.clock = t;

        // Scripted input, replayed by frame number into the real handlers.
        for (const auto& s : script) {
            if (s.frame != frameNo) continue;
            if (s.isSet) {
                int pi = app.params.find(s.id);
                if (pi >= 0) {
                    // NOW, not glided: this is standing in for a finger on a
                    // fader, and a glide would smear the very frame boundary
                    // the test is looking at.
                    app.params.setValueNow(pi, s.value);
                    std::printf("[script] frame %d set %s = %g\n", frameNo,
                                s.id.c_str(), static_cast<double>(s.value));
                } else {
                    std::printf("[script] frame %d set %s -- NO SUCH PARAM\n",
                                frameNo, s.id.c_str());
                }
            } else if (s.isKey) {
                KeyEvent k;
                k.key = s.key;
                k.pressed = true;
                k.ctrl = s.ctrl;
                k.shift = s.shift;
                std::printf("[script] frame %d key %d%s%s\n", frameNo, s.key,
                            s.ctrl ? " +ctrl" : "", s.shift ? " +shift" : "");
                keyFn(k);
            } else {
                PointerEvent p;
                p.outputIndex = (panelIdx >= 0) ? panelIdx : sceneIdx;
                p.x = s.x;
                p.y = s.y;
                // Down then up, in one frame: a tap. Nothing here needs to
                // simulate a drag, and a synthetic drag that skipped the
                // intermediate positions would not be one anyway.
                p.down = true;
                std::printf("[script] frame %d tap %.3f,%.3f\n", frameNo, s.x, s.y);
                pointerFn(p);
                p.down = false;
                pointerFn(p);
            }
            std::fflush(stdout);
        }
        // An armed LEARN expires on its own. It has to: while it is armed the
        // next parameter you touch is consumed by it, so one left running
        // behind your back would eat a slider you meant to drag.
        app.learn.tick(t);
        app.panel.setLearn(app.learn.armed() ? app.learn.slot() : -1,
                           app.learn.secondsLeft(t));

        // The glide rate is read from the knob BEFORE anything can set a
        // target this frame: a parameter's glide time is frozen at the moment
        // its target is set, so reading the knob afterwards would apply this
        // frame's rate to next frame's moves.
        app.params.setGlideSeconds(app.params.valueOf(gid::kGlide, 0.0f));

        if (selftest) {
            // Every frame past warmup is recorded, so the summary is about
            // the whole run and cannot be skewed by how frames are labelled.
            if (selftestFrame > 60) frameMs.push_back(dt * 1000.0);
            if (selftestFrame > 60 && selftestFrame % 20 == 0) {
                size_t slot = static_cast<size_t>(selftestEdits % 5);
                app.chain[slot].enabled = !app.chain[slot].enabled;
                app.rebuild();
                ++selftestEdits;
            }
            if (selftestEdits >= 40 && !app.renderer.chainPending()) {
                std::vector<double> s = frameMs;
                std::sort(s.begin(), s.end());
                auto pct = [&s](double p) {
                    return s.empty() ? 0.0
                                     : s[static_cast<size_t>(p * (s.size() - 1))];
                };
                int over20 = 0, over100 = 0;
                for (double v : frameMs) {
                    if (v > 20.0) ++over20;
                    if (v > 100.0) ++over100;
                }
                size_t worstAt = 0;
                for (size_t i = 0; i < frameMs.size(); ++i) {
                    if (frameMs[i] > frameMs[worstAt]) worstAt = i;
                }
                std::printf(
                    "[selftest] %d edits over %zu frames | median %.2f  p99 %.2f  "
                    "max %.2f ms (frame %zu) | %d >20ms  %d >100ms\n",
                    selftestEdits, frameMs.size(), pct(0.5), pct(0.99),
                    s.empty() ? 0.0 : s.back(), worstAt, over20, over100);
                app.running = false;
            }
            ++selftestFrame;
        }

        // External control, drained once per frame. Non-blocking, so an idle
        // socket costs one syscall.
        app.receiver.poll(app.mappings, app.params, t);
        for (const auto& c : app.receiver.takeCommands()) app.applyCommand(c);

        // ...and the state goes back the other way, to whoever is driving.
        // Here rather than beside the render, because a surface has to keep
        // seeing the truth even while HOME is up or a mode owns the screen.
        // Every discrete selection, in the order the wire format documents.
        // The grid on the phone lights the button that is actually current --
        // which is the other half of direct selection, and the half that gets
        // forgotten: a surface that can jump anywhere but cannot say where it
        // is will disagree with the instrument the moment anything else moves.
        app.receiver.broadcastState(
            app.params,
            {static_cast<int>(app.sel.mode), app.sel.kernelIndex,
             app.sel.kernelB, app.sel.colorMode, app.sel.trapShape,
             app.sel.paletteA, app.sel.paletteB,
             static_cast<int>(std::lround(app.params.valueOf(gid::kOpMode, 0.0f))),
             static_cast<int>(app.sel.sourceA), static_cast<int>(app.sel.sourceB),
             app.sel.channelBKernel},
            t);

        // Navigation is a SECOND CONSUMER of the same decoded stream, not a
        // second input path: one socket, one wire format, one decoder. The
        // mapping table is not involved because it maps sources to
        // parameters, and "move the cursor" is not a parameter.
        for (const auto& ev : app.receiver.lastEvents()) {
            app.nav.onControl(ev.source, ev.value, t);
        }
        app.nav.tick(t);
        Nav n;
        while (app.nav.poll(n)) app.applyNav(n);

        // Accumulate every source ever seen, so the monitor shows a control
        // that has gone quiet rather than only what arrived this frame.
        for (const auto& ev : app.receiver.lastEvents()) {
            auto it = std::find_if(monitor.begin(), monitor.end(),
                                   [&ev](const MonitorRow& r) {
                                       return r.source == ev.source;
                                   });
            if (it == monitor.end()) {
                MonitorRow r;
                r.source = ev.source;
                for (const auto& m : app.mappings.all()) {
                    if (m.source == ev.source) {
                        r.target = m.param;
                        break;
                    }
                }
                monitor.push_back(r);
                it = monitor.end() - 1;
            }
            it->value = ev.value;
            it->lastSeen = t;
        }

        // A newly finished chain goes live here, between frames, never
        // half-way through one.
        app.renderer.pumpCompiler();

        // SPEED scales the clock the whole engine runs on, so every
        // time-dependent effect and modulation slows down together.
        // SPEED is SIGNED and time is ACCUMULATED. Both halves matter:
        // accumulating means changing the rate changes only the derivative, so
        // a speed move (including straight through zero into reverse) can never
        // make the picture jump; signed means the accumulator can run
        // backwards, and every kernel's motion is a function of that one
        // number, so the whole instrument rewinds together. At exactly 0 the
        // accumulator does not move and the frame is frozen.
        float speed = app.params.valueOf(gid::kSpeed, 1.0f);
        sceneTime += dt * speed;
        motionClock.advance(dt * speed);
        // The journey advances on the SAME signed, accumulated clock, so
        // reverse rewinds the dive and speed 0 freezes it -- and, because the
        // frame is a pure function of the accumulated travel, reversing back
        // through a recycle returns to the dive it came from exactly rather
        // than to a newly chosen one.
        // Per-generator travel lives in ONE place: KernelDef::journeyDepth,
        // read where the frame is built below. Rate is deliberately NOT a
        // second per-kernel knob -- depth already produces different speeds,
        // because a shallower dive covers fewer decades in the same time.
        journey.advance(dt * speed, app.params.valueOf(gid::kJourney, 0.25f));

        // ---- modulation and slew, in this order ------------------------
        // The LFOs write this frame's offsets; the macros read them (so an
        // LFO can drive a macro and fan out through it) and push whatever
        // moved into the mapping table, which sets TARGETS; the bank then
        // advances every current towards its target once, with the real dt.
        //
        // Once per frame, here, and not inside the renderer: value() is read
        // dozens of times while a frame is built and every one of them has to
        // see the same number, or a parameter shared by two passes would
        // arrive at each of them slightly further along its glide.
        // The LFO clock is SCENE time: a modulator that ignored SPEED would be
        // the one thing on the instrument that did not slow down with it.
        updateLfos(app.lfos, app.mappings, app.params,
                   static_cast<float>(dt) * speed);
        applyMacros(app.macros, app.mappings, app.params);
        app.params.update(static_cast<float>(dt));

        // ---- HOME ------------------------------------------------------
        // Drawn on EVERY surface. There is nothing else to show while the
        // menu is up, and leaving the projector on the last synth frame would
        // make the instrument look like it was still running a mode.
        if (app.view == View::Home) {
            float hold = app.nav.homeHoldProgress(t);
            for (int i = 0; i < static_cast<int>(outs.size()); ++i) {
                plat->beginFrame(i);
                app.panel.drawHome(homeItems(), app.home.selected(),
                                   app.home.message(), hold, outs[i].width,
                                   outs[i].height, plat->currentContext());
                if (shotPath && shotIndexFor(frameNo) >= 0 && i == sceneIdx) {
                    // PHYSICAL dimensions: glReadPixels sees the real
                    // framebuffer, which under a quarter turn is the transpose
                    // of the logical size everything else works in.
                    int sw = outs[i].width, sh = outs[i].height;
                    if (quarterTurn) std::swap(sw, sh);
                    std::string path = shotPathFor(shotIndexFor(frameNo));
                    if (writePPM(path.c_str(), sw, sh)) {
                        std::printf("[nv] wrote %s (%dx%d) frame %d t=%.9f "
                                    "phase=%.9f\n",
                                    path.c_str(), sw, sh, frameNo, sceneTime,
                                    motionClock.phase());
                    }
                    if (frameNo >= lastShotFrame) app.running = false;
                }
                plat->endFrame(i);
            }
            ++frameNo;
            if (maxFrames > 0 && frameNo >= maxFrames) app.running = false;
            if (maxSeconds > 0.0 && startClock >= 0.0 &&
                plat->now() - startClock >= maxSeconds) app.running = false;

            // The handoff happens HERE, between frames, with no framebuffer
            // bound and nothing half-drawn -- never from inside an input
            // callback, which would pull the display out from under a frame
            // that is still being rendered.
            if (!app.pendingLaunch.empty()) {
                std::string cmd = app.pendingLaunch;
                std::string label = app.pendingLabel;
                app.pendingLaunch.clear();
                std::printf("[home] -> %s (handing over the display)\n",
                            label.c_str());
                std::fflush(stdout);

                // While the child owns the screen we keep draining the control
                // bus, so holding the joystick click still brings you back --
                // the same gesture, from a mode that is not even our process.
                struct Pump {
                    App* app;
                    Platform* plat;
                } pump{&app, plat.get()};
                int code = runModeWithDisplay(
                    *plat, cmd,
                    [](void* u) {
                        Pump* p = static_cast<Pump*>(u);
                        double now = p->plat->now();
                        p->app->receiver.poll(p->app->mappings, p->app->params,
                                              now);
                        // Not applied here: a preset load rebuilds the chain
                        // and asks the renderer to compile, and we do not own
                        // the display right now. They wait in the queue and
                        // land on the first frame after the child exits.
                        for (const auto& ev : p->app->receiver.lastEvents()) {
                            p->app->nav.onControl(ev.source, ev.value, now);
                        }
                        p->app->nav.tick(now);
                        Nav nv;
                        while (p->app->nav.poll(nv)) {
                            if (nv == Nav::Home || nv == Nav::Back) return false;
                        }
                        return true;
                    },
                    &pump);

                char msg[96];
                std::snprintf(msg, sizeof(msg), "%s EXITED (%d)", label.c_str(),
                              code);
                app.home.setMessage(msg);
                // The clock jumped by however long the child ran; feeding that
                // to the governor would read as one catastrophic frame.
                last = plat->now();
                app.renderer.governor().reset();
            }

            app.renderer.governor().observe(dt);
            continue;
        }

        plat->beginFrame(sceneIdx);
        {
            float mv[4];
            motionClock.vector(app.params.valueOf(gid::kEvolve, 1.0f), mv);
            app.renderer.setMotion(mv);
            // How far to dive is the GENERATOR's property, not the knob's.
            // JOURNEY sets the rate for the whole instrument; the kernel says
            // whether there is anything down there to descend into. A field
            // kernel answers 0 and simply holds normal position.
            const double depth = journeyDepthOverride >= 0.0
                                     ? journeyDepthOverride
                                     : app.kernel().journeyDepth;
            // Hand the journey this generator's own measured aims. Cheap
            // and idempotent, but only meaningful when the kernel changes --
            // which is also the only moment it is safe for them to change.
            {
                const auto& aims = app.kernel().journeyAims;
                journey.setAims(aims.empty() ? nullptr : aims.data(),
                                static_cast<int>(aims.size() / 2));
            }
            app.renderer.setJourney(journey.frame(depth));

            // Steer toward the boundary the renderer measured in the last
            // frame it looked at.
            //
            // Only while this generator is actually diving: a parked kernel
            // (journeyDepth 0) has no dive to aim, and recording steer
            // samples for one would leave a path behind that the next diving
            // kernel would inherit. And only ever forward -- steerTo ignores
            // a sample at or behind the newest, which is what lets reversing
            // REPLAY the recorded path instead of overwriting it with a fresh
            // measurement, and so keeps a rewound frame byte-identical.
            //
            // OFF BY DEFAULT, pending per-kernel seed targets. Runtime
            // steering was built to fix the fact that all 16 seed targets are
            // MANDELBROT coordinates, which is why 16 of 22 escape kernels
            // have no dive. It does not fix it, and it is not free: measured
            // by tests/depth_sweep.py it left BURNING SHIP exactly as dead
            // (edge 3.3 -> 0.3 -> 0.0 within two decades) and cost MANDELBROT
            // its deepest decade (0.65/14 -> 0.14/0.7 at five).
            //
            // The reason is that steering can only refine an aim that is
            // already in the right region. A kernel whose seed coordinate
            // lands nowhere near its own structure cannot be rescued by
            // nudging the centre a third of the way toward the nearest
            // boundary. That needs a per-kernel target, found by searching
            // the plane for each map -- which is the actual fix and is what
            // NV_STEER exists to keep out of the way of until then.
            if (steerEnabled && depth > 0.0) {
                const auto& fp = app.renderer.focus();
                if (fp.valid) journey.steerTo(journey.travel(), fp.x, fp.y);
            }
        }
        fs = app.renderer.renderScene(app.params, app.sel, app.chain, sceneTime,
                                      dt, scene.width, scene.height,
                                      scene.aspect(), plat->currentContext());

        PanelStatus st;
        st.status = app.status;
        st.mode = modeName(app.sel.mode);
        st.kernelName = app.kernel().name;
        // Which cell is lit follows the SELECTION, wherever it was changed --
        // a key, a tap on the grid, or a datagram. Two copies of "which
        // generator is current" is exactly the drift this engine avoids
        // everywhere else.
        app.panel.setGeneratorSelection(app.sel.kernelIndex, app.sel.kernelB);
        st.colorMode = colorModeName(static_cast<ColorMode>(app.sel.colorMode));
        st.trapShape = trapShapeName(static_cast<TrapShape>(app.sel.trapShape));
        st.paletteA = paletteBank()[app.sel.paletteA].name;
        st.paletteB = paletteBank()[app.sel.paletteB].name;
        st.fps = fps;
        st.renderWidth = fs.renderWidth;
        st.renderHeight = fs.renderHeight;
        st.passes = fs.passes;
        st.iterations = fs.iterations;
        st.governed = app.renderer.governor().throttling();
        st.oscPort = app.receiver.port();
        st.oscListening = app.receiver.isOpen();
        st.mappingCount = static_cast<int>(app.mappings.size());

        auto drawSurface = [&](int sw, int sh) {
            app.panel.draw(app.params, st, monitor, t, sw, sh,
                           plat->currentContext());
        };

        // The scene surface. With a dedicated control surface present it stays
        // PURE VISUALS -- nothing is ever drawn over it.
        if (panelIdx < 0 && app.showPanel) {
            drawSurface(scene.width, scene.height);
        } else {
            app.renderer.present(scene.width, scene.height, plat->currentContext());
        }
        // Grab before the flip: the back buffer still holds this frame.
        if (maxFrames > 0 && frameNo >= maxFrames) app.running = false;
        if (maxSeconds > 0.0 && startClock >= 0.0 &&
            plat->now() - startClock >= maxSeconds) app.running = false;

        if (shotPath && shotIndexFor(frameNo) >= 0) {
            int sw = scene.width, sh = scene.height;
            if (quarterTurn) std::swap(sw, sh);
            std::string path = shotPathFor(shotIndexFor(frameNo));
            if (writePPM(path.c_str(), sw, sh)) {
                // Scene time and motion phase go in the line, not just the
                // path: every claim about reversibility and freezing is a
                // claim about two frames being at the SAME point on the
                // clock, and a test that could not read the clock would be
                // comparing pictures and hoping.
                std::printf("[nv] wrote %s (%dx%d) frame %d t=%.9f "
                            "phase=%.9f\n",
                            path.c_str(), sw, sh, frameNo, sceneTime,
                            motionClock.phase());
            }
            if (frameNo >= lastShotFrame) app.running = false;
        }
        ++frameNo;

        plat->endFrame(sceneIdx);
        if (printFrameTime) glFinish();

        if (panelIdx >= 0 && panelIdx < static_cast<int>(outs.size())) {
            const Output& p = outs[panelIdx];
            plat->beginFrame(panelIdx);
            drawSurface(p.width, p.height);
            plat->endFrame(panelIdx);
        }

        app.renderer.governor().observe(dt);

        ++frames;
        statTimer += dt;
        if (statTimer >= 1.0) {
            fps = frames;
            std::printf("[nv] %3d fps | %-13s %-6s %-6s | %dx%d iter %d | %d pass%s%s\n",
                        fps, app.kernel().name.c_str(), st.colorMode.c_str(),
                        st.trapShape.c_str(), fs.renderWidth, fs.renderHeight,
                        fs.iterations, fs.passes, fs.passes == 1 ? "" : "es",
                        st.governed ? "  [governed]" : "");
            std::fflush(stdout);
            frames = 0;
            statTimer = 0.0;
        }
    }

    app.compiler.stop();
    app.panel.shutdown();
    app.renderer.shutdown();
    plat->shutdown();
    return 0;
}
