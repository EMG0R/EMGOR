// Control ingress.
//
// Listens on a UDP port for OSC and turns each message into a source id + a
// normalized value, which the mapping table then applies. Non-blocking and
// drained once per frame -- control must never make the render loop wait on
// a socket.
//
// This is a CONSUMER of the existing DEMIURGE bus, not a new one. The address
// conventions it understands are configured, not invented, so the same
// sensor/controller traffic that drives the audio engine drives this.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "control/mapping.h"
#include "control/osc.h"
#include "state.h"

namespace nv {

// The engine's own control port.
//
// NOT 8002. That is demiurge-io's telemetry destination and it has exactly ONE
// owner -- demiurge_web's SensorIn binds it whenever it runs, there is no
// SO_REUSEPORT, and whoever binds second silently receives nothing. Listening
// there would have looked correct and delivered an empty stream.
//
// 8004 is this engine's. Nothing sends to it yet: demiurge-io's telemetry has a
// single destination today, so `/sensor` goes to 8002 and `/cc` goes only to
// csound on 7770. The decoding below is complete and tested and will work
// unchanged the day that fan-out lands -- but until it does, OSC is a path that
// WILL exist, not one that carries traffic. NV_OSC_PORT moves it.
constexpr int kDefaultSensorPort = 8004;

struct ControlEvent {
    std::string source;
    float value = 0.0f;  // already normalized to 0..1
};

// A control surface addresses a knob BY ITS ID. That is the whole difference
// between this and everything above: `/sensor`, `/cc` and `/p` are *sources*
// that a mapping table points at something, and the mapping is the point --
// you do not want a LiDAR hard-wired to zoom. A fader on a phone is the
// opposite. It already knows which parameter it is; routing it through a
// mapping table would mean inventing a source id for it, writing a mapping
// line for every one of forty knobs, and giving the instrument two ways to
// answer "what does this control do".
//
// So the `/nv/` namespace is direct, and it is a namespace rather than four
// unrelated addresses so the receiver can tell "a command I do not recognise"
// from "a mapped source", and drop the first instead of feeding it to the
// mapping table as `osc:/nv/whatever`.
enum class CommandKind {
    Param,      // set a parameter's normalized position by id
    Preset,     // load a preset slot
    Mode,       // SYNTH / PARTICLES / DRAW
    Kernel,     // generator index
    KernelB,    // morph target index
    ChannelBKernel, // Channel B generator index
    Color,      // colour mode index
    Trap,       // trap shape index
    Palette,    // index / index2 = which slot (0 = A, 1 = B) and which palette
    Op,         // jit.op catalogue index
    Source,     // index = channel, index2 = SourceKind wire value
    LfoTarget,  // point an LFO at a parameter id (empty = unassign)
    // "Tell me what is in that bank." A surface with no way to ask this can
    // only offer PREV and NEXT through thirty generators, which is what the
    // browser had -- so the list has to be on the wire, not compiled into
    // whoever is drawing the buttons.
    ListRequest,
};

struct Command {
    CommandKind kind = CommandKind::Param;
    std::string id;      // Param, LfoTarget; ListRequest: which bank ("" = all)
    float value = 0.0f;  // Param only, normalized 0..1
    int index = 0;       // Preset / Mode / Kernel / Color / Trap / LFO slot
    int index2 = 0;      // Palette: which palette
};

// One row of an enumerable bank, on its way out to a surface. The engine owns
// the banks; the receiver owns the wire -- so the entries are handed in rather
// than reached for, and receiver.cpp stays free of every bank in the program.
struct ListEntry {
    std::string id;
    std::string name;
};

class Receiver {
public:
    ~Receiver();

    // Binds UDP `port` on loopback. Returns false (with a message on stderr)
    // if the port is taken -- the engine then runs without external control
    // rather than refusing to start.
    bool open(int port);
    void close();
    bool isOpen() const { return fd_ >= 0; }
    int port() const { return port_; }

    // Drains every pending datagram and applies each event through `table`.
    // `/nv/param` is applied straight to `bank` here; the commands that need
    // more than the bank (a preset, a mode, a generator) queue up in
    // takeCommands() for the caller, which is the only place that owns them.
    //
    // `now` is the caller's clock, used only to decide whether the sender is
    // still around for the state broadcast. Defaulted so the tests and the
    // pump loop can ignore it.
    int poll(const MappingTable& table, ParamBank& bank, double now = 0.0);

    // The non-parameter commands from the most recent poll(). Draining is the
    // read: they are one-shot events, not state.
    std::vector<Command> takeCommands();

    // The events from the most recent poll(), for the panel's monitor page.
    // Kept so raw incoming data can be watched even when nothing is mapped
    // to it -- which is what makes mapping a new sensor possible at all.
    const std::vector<ControlEvent>& lastEvents() const { return recent_; }

    // How a DEMIURGE bus message becomes a source id and a 0..1 value.
    // Separated out because it is pure logic and gets tested without a socket.
    //
    //   /sensor  <id:s> <norm:f>          -> "sensor:<id>"     (demiurge-io
    //                                        telemetry; already 0..1)
    //   /cc      <ch:i> <num:i> <val:i>   -> "cc:<ch>:<num>"   (val 0..127,
    //                                        ch is 1-indexed Csound convention)
    //   /p       <id:i> <val:f>           -> "p:<id>"          (already 0..1)
    //   anything else, 1+ numeric arg     -> "osc:<address>"
    //
    // The scaling of each is KNOWN from the address, not guessed. /cc divides
    // by 127 always -- inferring it from the magnitude would read a CC value
    // of 1 as full scale instead of near zero.
    static bool eventFromOsc(const OscMessage& msg, ControlEvent& out);

    // True for anything in the `/nv/` namespace, recognised or not. Checked
    // BEFORE eventFromOsc so a malformed `/nv/param` is dropped rather than
    // falling through to the "unknown address, guess the scale" branch and
    // arriving as a source called `osc:/nv/param`.
    static bool isCommand(const std::string& address);

    //   /nv/param  <id:s> <norm:f>   set that parameter's 0..1 position
    //   /nv/preset <slot:i>          load a preset slot
    //   /nv/mode   <n:i>             0 synth, 1 particles, 2 draw
    //   /nv/kernel <n:i>             generator index
    //   /nv/lfo    <slot:i> <id:s>   point that LFO at a parameter; "" clears
    //
    // `/nv/lfo` is the wire form of the same assignment the device's own LEARN
    // gesture makes, and it lands in the same assignLfoTarget() -- one
    // mechanism with two front doors, not two mechanisms. It is ABSOLUTE
    // (never a toggle) because a datagram can arrive twice and a repeat must
    // not mean the opposite of the first one.
    //
    // An id the bank does not have is not an error and never becomes one: the
    // bank's dynamic tail changes with the kernel and the chain, so a surface
    // built against one patch will inevitably name knobs that are not here
    // right now. Same rule as a preset's unknown keys.
    static bool commandFromOsc(const OscMessage& msg, Command& out);

    // ---- state broadcast --------------------------------------------------
    //
    // A control surface that only ever WRITES drifts: load a preset, let the
    // governor thin the swarm, touch the panel on the device, and every
    // remote slider is now describing a rig that has moved on. So the last
    // endpoint we heard from gets the whole bank back, ~10 times a second.
    //
    // Deliberately NOT a subscription. There is no register/renew/unregister,
    // nothing to leak and nothing to get out of step, because the fact we
    // want -- "somebody over there is driving this" -- is already carried by
    // their traffic. Sending stops on its own `kPeerTimeout` seconds after
    // they go quiet.
    static constexpr double kBroadcastInterval = 0.1;
    static constexpr double kPeerTimeout = 5.0;

    // The datagrams one broadcast consists of: `/nv/sel <mode:i> <kernel:i>`
    // followed by one `/nv/state <id:s> <norm:f>` per visible parameter,
    // bundled and split to stay inside a sane datagram. Pure, so the whole
    // shape is testable with no socket and no engine.
    // `sel` is the discrete selection, in a fixed order that a reader may
    // stop short of: mode, kernel, kernelB, colorMode, trapShape, paletteA,
    // paletteB, op, sourceA, sourceB, channelBKernel. It grew from two, and
    // appending is the only way it can grow --
    // a surface written against the old pair still reads the old pair.
    static std::vector<std::vector<uint8_t>> stateDatagrams(
        const ParamBank& bank, const std::vector<int>& sel);

    // Sends that, if a peer is current and the interval has elapsed. Returns
    // true if it sent. Cheap enough to call every frame -- that is the point.
    bool broadcastState(const ParamBank& bank, const std::vector<int>& sel,
                        double now);

    // ---- enumeration ------------------------------------------------------
    //
    // The answer to `/nv/list <what>`:
    //
    //   /nv/list_item <what:s> <index:i> <id:s> <name:s>   ... one per entry
    //   /nv/list_end  <what:s> <count:i>
    //
    // bundled and split like the state broadcast. Sent ONCE, on request,
    // rather than folded into the 10 Hz broadcast: these strings never change
    // while the process runs, and re-sending thirty of them ten times a second
    // to say the same thing would be most of the bandwidth on the link.
    //
    // list_end is not decoration. UDP loses datagrams, and a grid that has
    // silently received 27 of 30 buttons looks exactly like a bank with 27
    // entries; the count is how a surface knows to ask again.
    static std::vector<std::vector<uint8_t>> listDatagrams(
        const std::string& what, const std::vector<ListEntry>& entries);

    // Sends a reply to the peer we last heard from, now, ignoring the
    // broadcast interval -- this is an answer to a question, not a tick.
    bool sendToPeer(const std::vector<std::vector<uint8_t>>& datagrams);

private:
    int fd_ = -1;
    int port_ = 0;
    std::vector<ControlEvent> recent_;
    std::vector<Command> commands_;

    // Whoever last spoke to us. One peer, not a list: this is a performer's
    // phone, and a fan-out to every address that ever sent a packet would be
    // a subscription list wearing a disguise.
    bool havePeer_ = false;
    double peerSeen_ = 0.0;
    double lastSend_ = 0.0;
    uint8_t peer_[128] = {0};  // an opaque sockaddr_storage; see the .cpp
    unsigned peerLen_ = 0;
};

}  // namespace nv
