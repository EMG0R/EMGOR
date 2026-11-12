#include "control/receiver.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

#include "control/osc.h"

namespace nv {
namespace {

// One broadcast is split into datagrams no bigger than this. The whole bank is
// ~45 parameters and comfortably under 3 KB, which loopback would carry in one
// piece -- but the browser's server need not be on this machine forever, and a
// datagram that fragments is a datagram that an intervening MTU can drop
// whole. Splitting costs one extra sendto and removes the question.
constexpr size_t kMaxDatagram = 1200;

}  // namespace

Receiver::~Receiver() { close(); }

bool Receiver::open(int port) {
    close();
    fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) {
        std::fprintf(stderr, "[ctrl] socket: %s\n", std::strerror(errno));
        return false;
    }

    int one = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        // Not fatal: the engine runs fine with no external control, and
        // refusing to start over a busy port would be worse than the loss.
        // Deliberately no SO_REUSEPORT -- sharing the port would split the
        // datagrams between us and the other listener at random.
        std::fprintf(stderr, "[ctrl] cannot bind UDP %d: %s\n", port,
                     std::strerror(errno));
        if (port == kDefaultSensorPort) {
            std::fprintf(stderr,
                         "[ctrl] %d is demiurge-io's telemetry port and has one "
                         "owner (demiurge_web binds it when running).\n"
                         "[ctrl] running without external control. Set "
                         "NV_OSC_PORT to listen elsewhere.\n",
                         kDefaultSensorPort);
        }
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    // Non-blocking: poll() is called from the render loop and must return
    // immediately when there is nothing waiting.
    int flags = fcntl(fd_, F_GETFL, 0);
    fcntl(fd_, F_SETFL, flags | O_NONBLOCK);

    port_ = port;
    return true;
}

void Receiver::close() {
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
    port_ = 0;
}

bool Receiver::eventFromOsc(const OscMessage& msg, ControlEvent& out) {
    const auto& a = msg.args;
    char buf[96];

    if (msg.address == "/sensor") {
        // demiurge-io telemetry: /sensor <id:string> <norm:float>, already
        // conditioned to 0..1 and decimated to 20 Hz.
        if (a.size() < 2 || a[0].type != OscArg::Type::String) return false;
        out.source = "sensor:" + a[0].s;
        out.value = a[1].number();

    } else if (msg.address == "/cc") {
        // /cc <ch:i> <num:i> <val:i>. The channel is 1-indexed Csound
        // convention, not the 0-indexed wire nibble.
        if (a.size() < 3) return false;
        out.source = ccSource(a[0].i, a[1].i);
        // Always /127: a CC value of 1 is near zero, and inferring the scale
        // from the magnitude would turn it into full scale.
        out.value = a[2].number() / 127.0f;

    } else if (msg.address == "/p") {
        // /p <id:i> <val:f>, already normalized. The id is an argument, not
        // part of the address, so it has to go into the source id here --
        // otherwise every expression target collapses onto one source.
        if (a.size() < 2) return false;
        std::snprintf(buf, sizeof(buf), "p:%d", a[0].i);
        out.source = buf;
        out.value = a[1].number();

    } else {
        // Unknown address: the last numeric argument, with the scale guessed
        // from its magnitude. Only here, where nothing better is known.
        float v = 0.0f;
        bool found = false;
        for (const auto& arg : a) {
            if (arg.type != OscArg::Type::String) {
                v = arg.number();
                found = true;
            }
        }
        if (!found) return false;
        out.source = "osc:" + msg.address;
        out.value = (v > 1.0f) ? v / 127.0f : v;
    }

    if (out.value < 0.0f) out.value = 0.0f;
    if (out.value > 1.0f) out.value = 1.0f;
    return true;
}

bool Receiver::isCommand(const std::string& address) {
    return address.compare(0, 4, "/nv/") == 0;
}

bool Receiver::commandFromOsc(const OscMessage& msg, Command& out) {
    const auto& a = msg.args;

    if (msg.address == "/nv/param") {
        // The id is a STRING ARGUMENT, exactly as it is for /sensor: it is
        // the parameter's own stable id, so a surface built today still
        // addresses the same knob after the bank is reordered.
        if (a.size() < 2 || a[0].type != OscArg::Type::String) return false;
        if (a[0].s.empty()) return false;
        out.kind = CommandKind::Param;
        out.id = a[0].s;
        // NORMALIZED, never real units. A control surface has no business
        // knowing that zoom is exponential over four decades, and a value in
        // real units would silently change meaning the day a range is widened
        // -- which is the same reason presets store the opposite way round
        // (they persist for a year; this arrives 20 times a second and only
        // has to mean the same thing as the fader that sent it).
        out.value = a[1].number();
        if (out.value < 0.0f) out.value = 0.0f;
        if (out.value > 1.0f) out.value = 1.0f;
        return true;
    }

    if (msg.address == "/nv/source") {
        if (a.size() < 2 || a[0].type == OscArg::Type::String ||
            a[1].type == OscArg::Type::String) return false;
        out.kind = CommandKind::Source;
        out.index = static_cast<int>(a[0].number());
        out.index2 = static_cast<int>(a[1].number());
        return true;
    }

    if (msg.address == "/nv/lfo") {
        // <slot:i> <id:s>. The slot is a number because there are four of
        // them and they have no names; the target is a string for exactly the
        // reason presets store one -- an index would drift onto a different
        // knob the moment the kernel or the chain changed.
        if (a.size() < 2) return false;
        if (a[0].type == OscArg::Type::String) return false;
        if (a[1].type != OscArg::Type::String) return false;
        out.kind = CommandKind::LfoTarget;
        out.index = static_cast<int>(a[0].number());
        out.id = a[1].s;   // empty is legal: it means unassign
        return true;
    }

    if (msg.address == "/nv/list") {
        // The one verb that asks rather than tells. A missing or empty
        // argument means "everything you have", so a surface coming up cold
        // needs to know one address and no bank names at all.
        out.kind = CommandKind::ListRequest;
        out.id = (!a.empty() && a[0].type == OscArg::Type::String) ? a[0].s : "";
        return true;
    }

    if (msg.address == "/nv/palette") {
        // <slot:i> <index:i>. Two numbers because there are two palette slots
        // and the crossfade between them is a control of its own; a single
        // "set the palette" would have to pick one and the other would need a
        // second address saying the same thing.
        if (a.size() < 2) return false;
        if (a[0].type == OscArg::Type::String) return false;
        if (a[1].type == OscArg::Type::String) return false;
        out.kind = CommandKind::Palette;
        out.index = static_cast<int>(a[0].number());
        out.index2 = static_cast<int>(a[1].number());
        return true;
    }

    // The rest are one integer. number() rather than .i, because a browser
    // that JSON-encodes 2 and a browser that encodes 2.0 must not mean
    // different things.
    if (a.empty() || a[0].type == OscArg::Type::String) return false;
    int n = static_cast<int>(a[0].number());

    if (msg.address == "/nv/preset") {
        out.kind = CommandKind::Preset;
    } else if (msg.address == "/nv/mode") {
        out.kind = CommandKind::Mode;
    } else if (msg.address == "/nv/kernel") {
        out.kind = CommandKind::Kernel;
    } else if (msg.address == "/nv/kernel_b") {
        out.kind = CommandKind::KernelB;
    } else if (msg.address == "/nv/kernel_b2") {
        out.kind = CommandKind::ChannelBKernel;
    } else if (msg.address == "/nv/op") {
        out.kind = CommandKind::Op;
    } else if (msg.address == "/nv/color") {
        out.kind = CommandKind::Color;
    } else if (msg.address == "/nv/trap") {
        out.kind = CommandKind::Trap;
    } else {
        return false;  // in the namespace, but not a verb we know
    }
    out.index = n;
    return true;
}

std::vector<Command> Receiver::takeCommands() {
    std::vector<Command> out;
    out.swap(commands_);
    return out;
}

int Receiver::poll(const MappingTable& table, ParamBank& bank, double now) {
    recent_.clear();
    if (fd_ < 0) return 0;

    uint8_t buf[2048];
    int applied = 0;
    // Bounded so a flood cannot hold the render loop; anything left over is
    // read next frame, and control is 30-60Hz data at worst.
    for (int i = 0; i < 64; ++i) {
        // recvfrom, not recv: the reply address for the state broadcast is
        // carried by the surface's own traffic and is not configured
        // anywhere. That is the entire subscription mechanism.
        uint8_t from[128];
        socklen_t fromLen = sizeof(from);
        ssize_t n = ::recvfrom(fd_, buf, sizeof(buf), 0,
                               reinterpret_cast<sockaddr*>(from), &fromLen);
        if (n <= 0) break;

        std::vector<OscMessage> msgs;
        if (!oscParse(buf, static_cast<size_t>(n), msgs)) continue;

        // Only a packet we could actually read makes someone a peer. Noise on
        // the port should not start a 10 Hz stream at it.
        if (fromLen > 0 && fromLen <= sizeof(peer_)) {
            std::memcpy(peer_, from, fromLen);
            peerLen_ = static_cast<unsigned>(fromLen);
            havePeer_ = true;
            peerSeen_ = now;
        }

        for (const auto& m : msgs) {
            // Checked first, and `continue` either way: an unrecognised /nv/
            // address must be dropped, not handed to the mapping path as
            // `osc:/nv/...` where a stale mapping line could pick it up.
            if (isCommand(m.address)) {
                Command c;
                if (!commandFromOsc(m, c)) continue;
                if (c.kind == CommandKind::Param) {
                    int idx = bank.find(c.id);
                    // Unknown id: ignored, never fatal. The dynamic tail of
                    // the bank changes with the kernel and the chain, so a
                    // surface will legitimately name knobs that do not exist
                    // in this patch.
                    if (idx >= 0) {
                        bank.setNorm(idx, c.value);
                        ++applied;
                    }
                } else {
                    commands_.push_back(c);
                }
                continue;
            }

            ControlEvent ev;
            if (!eventFromOsc(m, ev)) continue;
            recent_.push_back(ev);
            if (table.apply(ev.source, ev.value, bank)) ++applied;
        }
    }
    return applied;
}

std::vector<std::vector<uint8_t>> Receiver::listDatagrams(
    const std::string& what, const std::vector<ListEntry>& entries) {
    std::vector<std::vector<uint8_t>> out;
    std::vector<std::vector<uint8_t>> pending;
    size_t bytes = 16;
    auto flush = [&]() {
        if (pending.empty()) return;
        out.push_back(oscBundle(pending));
        pending.clear();
        bytes = 16;
    };
    auto add = [&](std::vector<uint8_t> m) {
        size_t cost = m.size() + 4;
        if (bytes + cost > kMaxDatagram) flush();
        bytes += cost;
        pending.push_back(std::move(m));
    };
    for (size_t i = 0; i < entries.size(); ++i) {
        add(oscListItem("/nv/list_item", what, static_cast<int32_t>(i),
                        entries[i].id, entries[i].name));
    }
    // Always last, and always in its own place in the sequence: a surface
    // counts items until it sees this and asks again if the two disagree.
    add(oscMessage("/nv/list_end", what,
                   static_cast<int32_t>(entries.size())));
    flush();
    return out;
}

bool Receiver::sendToPeer(const std::vector<std::vector<uint8_t>>& datagrams) {
    if (fd_ < 0 || !havePeer_) return false;
    for (const auto& dg : datagrams) {
        ::sendto(fd_, dg.data(), dg.size(), 0,
                 reinterpret_cast<const sockaddr*>(peer_),
                 static_cast<socklen_t>(peerLen_));
    }
    return true;
}

std::vector<std::vector<uint8_t>> Receiver::stateDatagrams(
    const ParamBank& bank, const std::vector<int>& sel) {
    std::vector<std::vector<uint8_t>> out;
    std::vector<std::vector<uint8_t>> pending;
    size_t bytes = 16;  // "#bundle" + time tag

    auto flush = [&]() {
        if (pending.empty()) return;
        out.push_back(oscBundle(pending));
        pending.clear();
        bytes = 16;
    };
    auto add = [&](std::vector<uint8_t> m) {
        size_t cost = m.size() + 4;  // each element carries its own size
        if (bytes + cost > kMaxDatagram) flush();
        bytes += cost;
        pending.push_back(std::move(m));
    };

    // What is not a knob, first: the mode and the generator decide which
    // knobs even exist, so a surface reading this list learns the shape
    // before the values.
    add(oscMessageInts("/nv/sel", std::vector<int32_t>(sel.begin(), sel.end())));

    for (int i = 0; i < bank.count(); ++i) {
        const ParamDef& d = bank.def(i);
        // Hidden entries are the padding that keeps uKernelP four-wide. They
        // are not drawn on the device's own panel and there is nothing for a
        // remote surface to do with them either.
        if (d.hidden) continue;
        // The TARGET, not the live position. A remote fader is a control, and
        // a control has to sit where it was put -- streaming the modulated
        // value back at 10 Hz would make every fader on the phone twitch in
        // time with the LFOs and jump around during a preset glide.
        add(oscMessage("/nv/state", d.id, bank.target(i)));
    }
    flush();
    return out;
}

bool Receiver::broadcastState(const ParamBank& bank, const std::vector<int>& sel,
                              double now) {
    if (fd_ < 0 || !havePeer_) return false;
    // They have gone quiet: stop, without either side having to say so.
    if (now - peerSeen_ > kPeerTimeout) return false;
    if (now - lastSend_ < kBroadcastInterval) return false;
    lastSend_ = now;

    for (const auto& dg : stateDatagrams(bank, sel)) {
        // Fire and forget, and deliberately unchecked beyond the send: a
        // surface that has walked out of wifi range must not become an error
        // path in the render loop.
        ::sendto(fd_, dg.data(), dg.size(), 0,
                 reinterpret_cast<const sockaddr*>(peer_),
                 static_cast<socklen_t>(peerLen_));
    }
    return true;
}

}  // namespace nv
