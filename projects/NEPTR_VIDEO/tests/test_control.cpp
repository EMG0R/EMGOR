// OSC parsing runs on bytes off a socket, and the mapping table is what
// every external control passes through. Both are pure logic, and both are
// places where being wrong is silent -- so both are tested directly.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "check.h"
#include "control/mapping.h"
#include "control/osc.h"
#include "control/receiver.h"

using namespace nv;

namespace {

// --- a minimal OSC writer, so the tests build their own input -------------
void putU32(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back(static_cast<uint8_t>(v >> 24));
    b.push_back(static_cast<uint8_t>(v >> 16));
    b.push_back(static_cast<uint8_t>(v >> 8));
    b.push_back(static_cast<uint8_t>(v));
}

void putStr(std::vector<uint8_t>& b, const std::string& s) {
    for (char c : s) b.push_back(static_cast<uint8_t>(c));
    b.push_back(0);
    while (b.size() % 4 != 0) b.push_back(0);
}

void putF32(std::vector<uint8_t>& b, float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    putU32(b, u);
}

std::vector<uint8_t> msgFloat(const std::string& addr, float v) {
    std::vector<uint8_t> b;
    putStr(b, addr);
    putStr(b, ",f");
    putF32(b, v);
    return b;
}

std::vector<uint8_t> msgIntInt(const std::string& addr, int32_t a, int32_t c) {
    std::vector<uint8_t> b;
    putStr(b, addr);
    putStr(b, ",ii");
    putU32(b, static_cast<uint32_t>(a));
    putU32(b, static_cast<uint32_t>(c));
    return b;
}

// --- OSC ------------------------------------------------------------------
void testParseFloat() {
    auto b = msgFloat("/p/tilt", 0.25f);
    std::vector<OscMessage> out;
    CHECK(oscParse(b.data(), b.size(), out));
    CHECK_EQ(static_cast<int>(out.size()), 1);
    CHECK(out[0].address == "/p/tilt");
    CHECK_EQ(static_cast<int>(out[0].args.size()), 1);
    CHECK_NEAR(out[0].args[0].number(), 0.25f, 1e-6f);
}

void testParseInts() {
    auto b = msgIntInt("/cc", 74, 100);
    std::vector<OscMessage> out;
    CHECK(oscParse(b.data(), b.size(), out));
    CHECK_EQ(static_cast<int>(out[0].args.size()), 2);
    CHECK_EQ(out[0].args[0].i, 74);
    // number() must not care whether the sender chose int or float.
    CHECK_NEAR(out[0].args[1].number(), 100.0f, 1e-6f);
}

void testAddressPaddingIsExact() {
    // A 3-char address is 4 bytes with its null -- the pad must add nothing.
    // Off-by-one here shifts every argument and is invisible until values
    // come out as garbage.
    auto b = msgFloat("/ab", 1.0f);
    CHECK_EQ(static_cast<int>(b.size()), 4 + 4 + 4);
    std::vector<OscMessage> out;
    CHECK(oscParse(b.data(), b.size(), out));
    CHECK_NEAR(out[0].args[0].number(), 1.0f, 1e-6f);
}

void testStringArg() {
    std::vector<uint8_t> b;
    putStr(b, "/name");
    putStr(b, ",s");
    putStr(b, "julia");
    std::vector<OscMessage> out;
    CHECK(oscParse(b.data(), b.size(), out));
    CHECK(out[0].args[0].s == "julia");
}

void testBundleFlattens() {
    auto m1 = msgFloat("/a", 0.5f);
    auto m2 = msgFloat("/b", 0.75f);
    std::vector<uint8_t> b;
    putStr(b, "#bundle");
    putU32(b, 0);
    putU32(b, 1);  // immediate time tag
    putU32(b, static_cast<uint32_t>(m1.size()));
    b.insert(b.end(), m1.begin(), m1.end());
    putU32(b, static_cast<uint32_t>(m2.size()));
    b.insert(b.end(), m2.begin(), m2.end());

    std::vector<OscMessage> out;
    CHECK(oscParse(b.data(), b.size(), out));
    CHECK_EQ(static_cast<int>(out.size()), 2);
    CHECK(out[0].address == "/a");
    CHECK(out[1].address == "/b");
}

void testTruncatedIsRejectedCleanly() {
    auto b = msgFloat("/p/tilt", 0.25f);
    for (size_t cut = 1; cut < b.size(); ++cut) {
        std::vector<OscMessage> out;
        // A short read must never yield a half-parsed message, or a truncated
        // packet becomes a random parameter jump.
        oscParse(b.data(), cut, out);
        if (!out.empty()) {
            CHECK_EQ(static_cast<int>(out[0].args.size()), 1);
        }
    }
}

void testGarbageIsRejected() {
    const uint8_t junk[16] = {0xff, 0x00, 0x12, 0x9a, 0x01, 0x02, 0x03, 0x04,
                              0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c};
    std::vector<OscMessage> out;
    CHECK(!oscParse(junk, sizeof(junk), out));
    CHECK(out.empty());
}

void testUnknownTypeTagRejected() {
    std::vector<uint8_t> b;
    putStr(b, "/x");
    putStr(b, ",z");  // unknown width: everything after is unreadable
    putU32(b, 0);
    std::vector<OscMessage> out;
    CHECK(!oscParse(b.data(), b.size(), out));
}

// --- DEMIURGE bus decoding -------------------------------------------------
// These assert the ACTUAL wire protocol of demiurge-io / the NEPTR engine, not
// a convention invented here. Getting any of them wrong is silent: a control
// simply drives the wrong thing, or nothing.

OscMessage parseOne(const std::vector<uint8_t>& b) {
    std::vector<OscMessage> out;
    oscParse(b.data(), b.size(), out);
    return out.empty() ? OscMessage{} : out[0];
}

void testSensorTelemetry() {
    // demiurge-io telemetry: /sensor <id:s> <norm:f>, tags ",sf".
    std::vector<uint8_t> b;
    putStr(b, "/sensor");
    putStr(b, ",sf");
    putStr(b, "tof1");
    putF32(b, 0.42f);

    ControlEvent ev;
    CHECK(Receiver::eventFromOsc(parseOne(b), ev));
    // The id is a STRING ARGUMENT, not part of the address. If it does not
    // reach the source id, every sensor collapses onto one control.
    CHECK(ev.source == "sensor:tof1");
    CHECK_NEAR(ev.value, 0.42f, 1e-6f);
}

void testSensorIdsStayDistinct() {
    std::vector<uint8_t> a, b;
    putStr(a, "/sensor"); putStr(a, ",sf"); putStr(a, "jx"); putF32(a, 0.1f);
    putStr(b, "/sensor"); putStr(b, ",sf"); putStr(b, "jy"); putF32(b, 0.9f);
    ControlEvent ea, eb;
    CHECK(Receiver::eventFromOsc(parseOne(a), ea));
    CHECK(Receiver::eventFromOsc(parseOne(b), eb));
    CHECK(ea.source != eb.source);
}

void testCcIsAlwaysDividedBy127() {
    // /cc <ch:i> <num:i> <val:i>, val 0..127. A CC value of 1 is NEAR ZERO.
    // Guessing the scale from magnitude would make it full scale -- the exact
    // bug this test exists to prevent.
    auto m = parseOne([] {
        std::vector<uint8_t> b;
        putStr(b, "/cc");
        putStr(b, ",iii");
        putU32(b, 1);
        putU32(b, 20);
        putU32(b, 1);
        return b;
    }());
    ControlEvent ev;
    CHECK(Receiver::eventFromOsc(m, ev));
    CHECK(ev.source == "cc:1:20");
    CHECK_NEAR(ev.value, 1.0f / 127.0f, 1e-5f);
}

void testCcFullScale() {
    auto m = parseOne([] {
        std::vector<uint8_t> b;
        putStr(b, "/cc");
        putStr(b, ",iii");
        putU32(b, 16);
        putU32(b, 119);
        putU32(b, 127);
        return b;
    }());
    ControlEvent ev;
    CHECK(Receiver::eventFromOsc(m, ev));
    // Channel is the 1-indexed Csound convention, not the wire nibble.
    CHECK(ev.source == "cc:16:119");
    CHECK_NEAR(ev.value, 1.0f, 1e-6f);
}

void testExpressionIdReachesSource() {
    // /p <id:i> <val:f>. The id is sparse and lives in an ARGUMENT, so all 35
    // expression targets share the address "/p".
    auto mk = [](int id, float v) {
        std::vector<uint8_t> b;
        putStr(b, "/p");
        putStr(b, ",if");
        putU32(b, static_cast<uint32_t>(id));
        putF32(b, v);
        return b;
    };
    ControlEvent a, c;
    CHECK(Receiver::eventFromOsc(parseOne(mk(30, 0.5f)), a));
    CHECK(Receiver::eventFromOsc(parseOne(mk(72, 0.5f)), c));
    CHECK(a.source == "p:30");
    CHECK(c.source == "p:72");
    CHECK(a.source != c.source);
    // Already normalized: it must not be divided again.
    CHECK_NEAR(a.value, 0.5f, 1e-6f);
}

void testMalformedBusMessagesRejected() {
    ControlEvent ev;
    // /cc with too few args must not be half-read into a wrong source.
    std::vector<uint8_t> b;
    putStr(b, "/cc");
    putStr(b, ",ii");
    putU32(b, 1);
    putU32(b, 20);
    CHECK(!Receiver::eventFromOsc(parseOne(b), ev));

    // /sensor whose first arg is not a string is not the message we think.
    std::vector<uint8_t> c;
    putStr(c, "/sensor");
    putStr(c, ",ff");
    putF32(c, 1.0f);
    putF32(c, 0.5f);
    CHECK(!Receiver::eventFromOsc(parseOne(c), ev));
}

void testValuesAreClamped() {
    std::vector<uint8_t> b;
    putStr(b, "/sensor");
    putStr(b, ",sf");
    putStr(b, "ax");
    putF32(b, -3.0f);
    ControlEvent ev;
    CHECK(Receiver::eventFromOsc(parseOne(b), ev));
    CHECK_NEAR(ev.value, 0.0f, 1e-6f);
}

ParamBank freshBank() {
    ParamBank b;
    registerGlobalParams(b);
    return b;
}

// --- /nv: direct control, the way a control surface addresses a knob -------
// Everything above is a SOURCE that a mapping points somewhere. These are
// direct sets by parameter id, and the failure modes are different: an id
// that no longer exists, a value that is not normalized, a verb from a newer
// surface than this build.

std::vector<uint8_t> msgStrFloat(const std::string& addr, const std::string& s,
                                 float v) {
    std::vector<uint8_t> b;
    putStr(b, addr);
    putStr(b, ",sf");
    putStr(b, s);
    putF32(b, v);
    return b;
}

std::vector<uint8_t> msgInt(const std::string& addr, int32_t v) {
    std::vector<uint8_t> b;
    putStr(b, addr);
    putStr(b, ",i");
    putU32(b, static_cast<uint32_t>(v));
    return b;
}

void testParamCommandDecodes() {
    Command c;
    CHECK(Receiver::isCommand("/nv/param"));
    CHECK(Receiver::commandFromOsc(parseOne(msgStrFloat("/nv/param", "g.zoom",
                                                        0.75f)), c));
    CHECK(c.kind == CommandKind::Param);
    CHECK(c.id == "g.zoom");
    CHECK_NEAR(c.value, 0.75f, 1e-6f);
}

void testParamValueIsClampedNotScaled() {
    // The value is a NORMALIZED position and nothing else. A surface that
    // sends 1.0 means the top of the travel, not "divide me by 127".
    Command c;
    CHECK(Receiver::commandFromOsc(parseOne(msgStrFloat("/nv/param", "g.detail",
                                                        1.0f)), c));
    CHECK_NEAR(c.value, 1.0f, 1e-6f);
    CHECK(Receiver::commandFromOsc(parseOne(msgStrFloat("/nv/param", "g.detail",
                                                        -2.0f)), c));
    CHECK_NEAR(c.value, 0.0f, 1e-6f);
    CHECK(Receiver::commandFromOsc(parseOne(msgStrFloat("/nv/param", "g.detail",
                                                        4.0f)), c));
    CHECK_NEAR(c.value, 1.0f, 1e-6f);
}

void testParamSetsNormalizedPositionByCurve() {
    // The bank owns the curve; the wire never sees it. ZOOM is exponential
    // over four decades, so norm 0.5 is the geometric middle -- if this ever
    // came out as the arithmetic middle, the wire would be carrying real
    // units and every stored surface position would be wrong.
    ParamBank b = freshBank();
    int z = b.find(gid::kZoom);
    b.setNorm(z, 0.5f);
    CHECK_NEAR(b.norm(z), 0.5f, 1e-6f);
    CHECK_NEAR(b.value(z), 0.1f, 1e-3f);   // sqrt(1e-4 * 1e2)
}

void testUnknownParamIdIsIgnoredNotFatal() {
    // The bank's dynamic tail changes with the kernel and the chain, so a
    // surface WILL name knobs that are not in this patch. Same rule as a
    // preset's unknown keys: skip it, keep going.
    Command c;
    CHECK(Receiver::commandFromOsc(
        parseOne(msgStrFloat("/nv/param", "fx9.nonexistent", 0.5f)), c));
    ParamBank b = freshBank();
    CHECK_EQ(b.find(c.id), -1);
}

void testParamWithNoIdIsRejected() {
    Command c;
    // Empty id: would find nothing anyway, but accepting it means a bug
    // upstream reads as "the engine ignored me" instead of a rejection.
    CHECK(!Receiver::commandFromOsc(parseOne(msgStrFloat("/nv/param", "", 0.5f)),
                                    c));
    // Id sent as a number rather than a string is not this message.
    std::vector<uint8_t> b;
    putStr(b, "/nv/param");
    putStr(b, ",ff");
    putF32(b, 3.0f);
    putF32(b, 0.5f);
    CHECK(!Receiver::commandFromOsc(parseOne(b), c));
}

void testPresetModeKernelDecode() {
    Command c;
    CHECK(Receiver::commandFromOsc(parseOne(msgInt("/nv/preset", 3)), c));
    CHECK(c.kind == CommandKind::Preset);
    CHECK_EQ(c.index, 3);

    CHECK(Receiver::commandFromOsc(parseOne(msgInt("/nv/mode", 1)), c));
    CHECK(c.kind == CommandKind::Mode);
    CHECK_EQ(c.index, 1);

    CHECK(Receiver::commandFromOsc(parseOne(msgInt("/nv/kernel", 12)), c));
    CHECK(c.kind == CommandKind::Kernel);
    CHECK_EQ(c.index, 12);
}

std::vector<uint8_t> msgIntStr(const std::string& addr, int32_t n,
                               const std::string& s) {
    std::vector<uint8_t> b;
    putStr(b, addr);
    putStr(b, ",is");
    putU32(b, static_cast<uint32_t>(n));
    putStr(b, s);
    return b;
}

void testLfoTargetDecodes() {
    // The wire form of the assignment the device's LEARN gesture makes. The
    // target is a STRING for the same reason a preset stores one: an index
    // would drift onto a different knob the moment the kernel or the chain
    // changed the bank's tail.
    Command c;
    CHECK(Receiver::isCommand("/nv/lfo"));
    CHECK(Receiver::commandFromOsc(parseOne(msgIntStr("/nv/lfo", 2, "g.detail")),
                                   c));
    CHECK(c.kind == CommandKind::LfoTarget);
    CHECK_EQ(c.index, 2);
    CHECK(c.id == "g.detail");

    // An empty target is LEGAL and means unassign -- the only way a surface
    // can turn one off, and distinct from never having sent the message.
    CHECK(Receiver::commandFromOsc(parseOne(msgIntStr("/nv/lfo", 0, "")), c));
    CHECK(c.kind == CommandKind::LfoTarget);
    CHECK(c.id.empty());

    // Malformed: one argument, or the two in the wrong order. Dropped rather
    // than half-applied to slot 0.
    CHECK(!Receiver::commandFromOsc(parseOne(msgInt("/nv/lfo", 1)), c));
    CHECK(!Receiver::commandFromOsc(
        parseOne(msgStrFloat("/nv/lfo", "g.detail", 1.0f)), c));
}

void testIntArgAcceptedAsFloat() {
    // A browser encodes 2 and 2.0 differently and means the same thing.
    std::vector<uint8_t> b;
    putStr(b, "/nv/mode");
    putStr(b, ",f");
    putF32(b, 2.0f);
    Command c;
    CHECK(Receiver::commandFromOsc(parseOne(b), c));
    CHECK(c.kind == CommandKind::Mode);
    CHECK_EQ(c.index, 2);
}

void testUnknownVerbInNamespaceIsClaimedButRefused() {
    // THE point of a namespace. `/nv/wobble` must be DROPPED -- if it fell
    // through to the mapping path it would arrive as source `osc:/nv/wobble`
    // and a mapping line could pick it up, so a surface from a newer build
    // would quietly drive an unrelated knob on an older engine.
    CHECK(Receiver::isCommand("/nv/wobble"));
    Command c;
    CHECK(!Receiver::commandFromOsc(parseOne(msgInt("/nv/wobble", 1)), c));
    // And the mapping path is never even consulted, because isCommand() is
    // what poll() branches on -- but prove the two namespaces stay apart.
    CHECK(!Receiver::isCommand("/sensor"));
    CHECK(!Receiver::isCommand("/cc"));
    CHECK(!Receiver::isCommand("/p"));
    CHECK(!Receiver::isCommand("/nv"));      // no trailing slash: not ours
    CHECK(!Receiver::isCommand("/"));        // must not read off the end
}

void testMissingArgsRejected() {
    Command c;
    std::vector<uint8_t> b;
    putStr(b, "/nv/preset");
    putStr(b, ",");
    CHECK(!Receiver::commandFromOsc(parseOne(b), c));
}

// --- direct selection ------------------------------------------------------
//
// A surface with only PREV and NEXT has to step through thirty generators to
// reach one, which is a menu, not an instrument. These are the verbs that make
// a grid of buttons possible, and the enumeration that tells a surface what to
// draw on them.

void testDirectSelectionVerbsDecode() {
    struct { const char* addr; CommandKind kind; } cases[] = {
        {"/nv/kernel_b", CommandKind::KernelB},
        {"/nv/kernel_b2", CommandKind::ChannelBKernel},
        {"/nv/color",    CommandKind::Color},
        {"/nv/trap",     CommandKind::Trap},
    };
    for (const auto& c : cases) {
        OscMessage m;
        m.address = c.addr;
        OscArg a0; a0.type = OscArg::Type::Int; a0.i = 17;
        m.args.push_back(a0);
        Command out;
        CHECK(Receiver::isCommand(m.address));
        CHECK(Receiver::commandFromOsc(m, out));
        CHECK(out.kind == c.kind);
        CHECK_EQ(out.index, 17);
    }
}

void testOperatorAndSourceVerbsDecode() {
    OscMessage op;
    op.address = "/nv/op";
    OscArg oi; oi.type = OscArg::Type::Int; oi.i = 51;
    op.args.push_back(oi);
    Command out;
    CHECK(Receiver::commandFromOsc(op, out));
    CHECK(out.kind == CommandKind::Op);
    CHECK_EQ(out.index, 51);

    OscMessage source;
    source.address = "/nv/source";
    OscArg channel; channel.type = OscArg::Type::Int; channel.i = 1;
    OscArg kind; kind.type = OscArg::Type::Int; kind.i = 0; // FRACTAL
    source.args = {channel, kind};
    CHECK(Receiver::commandFromOsc(source, out));
    CHECK(out.kind == CommandKind::Source);
    CHECK_EQ(out.index, 1);
    CHECK_EQ(out.index2, 0);

    source.args.pop_back();
    CHECK(!Receiver::commandFromOsc(source, out));
}

void testPaletteVerbCarriesSlotAndIndex() {
    OscMessage m;
    m.address = "/nv/palette";
    OscArg a0; a0.type = OscArg::Type::Int; a0.i = 1;   // slot B
    OscArg a1; a1.type = OscArg::Type::Int; a1.i = 12;
    m.args.push_back(a0);
    m.args.push_back(a1);
    Command out;
    CHECK(Receiver::commandFromOsc(m, out));
    CHECK(out.kind == CommandKind::Palette);
    CHECK_EQ(out.index, 1);
    CHECK_EQ(out.index2, 12);

    // One argument is not a palette command: which slot is not optional, and
    // guessing would silently drive A when the surface meant B.
    m.args.pop_back();
    CHECK(!Receiver::commandFromOsc(m, out));
}

void testListRequestDecodesWithAndWithoutArgument() {
    OscMessage m;
    m.address = "/nv/list";
    Command out;
    // Bare: "everything you have". A surface coming up cold knows one address
    // and no bank names at all.
    CHECK(Receiver::commandFromOsc(m, out));
    CHECK(out.kind == CommandKind::ListRequest);
    CHECK(out.id.empty());

    OscArg a0; a0.type = OscArg::Type::String; a0.s = "kernel";
    m.args.push_back(a0);
    CHECK(Receiver::commandFromOsc(m, out));
    CHECK(out.kind == CommandKind::ListRequest);
    CHECK(out.id == "kernel");
}

void testListDatagramsCarryNamesAndTerminate() {
    std::vector<ListEntry> e;
    for (int i = 0; i < 30; ++i) {
        char id[32], nm[32];
        std::snprintf(id, sizeof(id), "gen%d", i);
        std::snprintf(nm, sizeof(nm), "GENERATOR %d", i);
        e.push_back({id, nm});
    }
    auto dgs = Receiver::listDatagrams("kernel", e);
    CHECK(!dgs.empty());

    std::vector<OscMessage> msgs;
    for (const auto& d : dgs) {
        CHECK(d.size() <= 1200);
        CHECK(oscParse(d.data(), d.size(), msgs));
    }

    int items = 0, ends = 0, count = -1;
    for (const auto& m : msgs) {
        if (m.address == "/nv/list_item") {
            CHECK(m.args.size() == 4);
            CHECK(m.args[0].s == "kernel");
            int idx = m.args[1].i;
            CHECK(idx == items);
            CHECK(m.args[2].s == e[idx].id);
            CHECK(m.args[3].s == e[idx].name);
            ++items;
        } else if (m.address == "/nv/list_end") {
            CHECK(m.args.size() == 2);
            CHECK(m.args[0].s == "kernel");
            count = m.args[1].i;
            ++ends;
        }
    }
    CHECK_EQ(items, 30);
    // Exactly one terminator, and it agrees. UDP drops datagrams, and a grid
    // that quietly received 27 of 30 buttons looks identical to a bank with
    // 27 entries -- the count is the only way a surface can know to ask again.
    CHECK_EQ(ends, 1);
    CHECK_EQ(count, 30);
}

void testListSplitsAndLosesNothing() {
    std::vector<ListEntry> e;
    for (int i = 0; i < 300; ++i) {
        char id[64];
        std::snprintf(id, sizeof(id), "a_rather_long_generator_id_%d", i);
        e.push_back({id, id});
    }
    auto dgs = Receiver::listDatagrams("kernel", e);
    CHECK(dgs.size() > 1);
    int items = 0;
    for (const auto& d : dgs) {
        CHECK(d.size() <= 1200);
        std::vector<OscMessage> msgs;
        CHECK(oscParse(d.data(), d.size(), msgs));
        for (const auto& m : msgs) if (m.address == "/nv/list_item") ++items;
    }
    CHECK_EQ(items, 300);
}

// --- state broadcast -------------------------------------------------------

void testStateDatagramsRoundTrip() {
    ParamBank bank = freshBank();
    int z = bank.find(gid::kZoom);
    bank.setNorm(z, 0.25f);

    auto dgs = Receiver::stateDatagrams(bank, {1, 7});
    CHECK(!dgs.empty());

    std::vector<OscMessage> msgs;
    for (const auto& d : dgs) {
        // Every datagram must parse on its own: they are separate packets and
        // may arrive out of order or not at all.
        CHECK(oscParse(d.data(), d.size(), msgs));
        CHECK(d.size() <= 1200);
    }

    CHECK(msgs[0].address == "/nv/sel");
    CHECK_EQ(msgs[0].args[0].i, 1);
    CHECK_EQ(msgs[0].args[1].i, 7);

    int found = 0, states = 0;
    for (size_t i = 1; i < msgs.size(); ++i) {
        CHECK(msgs[i].address == "/nv/state");
        CHECK(msgs[i].args[0].type == OscArg::Type::String);
        ++states;
        if (msgs[i].args[0].s == gid::kZoom) {
            CHECK_NEAR(msgs[i].args[1].number(), 0.25f, 1e-6f);
            ++found;
        }
    }
    CHECK_EQ(found, 1);
    // The whole bank, not a sample of it: a surface that receives a partial
    // list has no way to tell a missing knob from an unchanged one.
    CHECK_EQ(states, bank.count());
}

void testSelCarriesEverySelectionAndStaysBackwardCompatible() {
    // The line grew from two ints to seven so a remote grid can light the
    // button that is actually current. Appending is the only way it could
    // grow: a surface written against the old pair still reads the old pair,
    // in the same two places, and simply ignores the rest.
    ParamBank bank = freshBank();
    const std::vector<int> sel = {1, 7, 12, 3, 2, 5, 9};
    std::vector<OscMessage> msgs;
    for (const auto& d : Receiver::stateDatagrams(bank, sel)) {
        CHECK(oscParse(d.data(), d.size(), msgs));
    }
    CHECK(msgs[0].address == "/nv/sel");
    CHECK(msgs[0].args.size() == sel.size());
    for (size_t i = 0; i < sel.size(); ++i) {
        CHECK_EQ(msgs[0].args[i].i, sel[i]);
    }
}

void testStateSkipsHiddenPadding() {
    ParamBank bank = freshBank();
    ParamDef pad{"pad0", "-", "patch", 0.0f, 1.0f, 0.0f, Curve::Linear};
    pad.hidden = true;
    bank.add(pad);
    bank.add({"visible0", "V", "patch", 0.0f, 1.0f, 0.0f, Curve::Linear});

    std::vector<OscMessage> msgs;
    for (const auto& d : Receiver::stateDatagrams(bank, {0, 0})) {
        oscParse(d.data(), d.size(), msgs);
    }
    bool sawPad = false, sawVisible = false;
    for (const auto& m : msgs) {
        if (m.address != "/nv/state") continue;
        if (m.args[0].s == "pad0") sawPad = true;
        if (m.args[0].s == "visible0") sawVisible = true;
    }
    CHECK(!sawPad);
    CHECK(sawVisible);
}

void testStateSplitsIntoSaneDatagrams() {
    // A bundle bigger than the path MTU is dropped SILENTLY -- the sliders
    // would just stop updating with no error anywhere. So the split is not an
    // optimisation, it is the difference between working and failing quietly.
    ParamBank bank;
    for (int i = 0; i < 400; ++i) {
        char id[32];
        std::snprintf(id, sizeof(id), "fx%d.a_long_parameter_name", i);
        bank.add({id, "X", "g", 0.0f, 1.0f, 0.5f, Curve::Linear});
    }
    auto dgs = Receiver::stateDatagrams(bank, {0, 0});
    CHECK(dgs.size() > 1);
    int total = 0;
    for (const auto& d : dgs) {
        CHECK(d.size() <= 1200);
        std::vector<OscMessage> msgs;
        CHECK(oscParse(d.data(), d.size(), msgs));
        for (const auto& m : msgs) if (m.address == "/nv/state") ++total;
    }
    // Nothing is lost at a split boundary.
    CHECK_EQ(total, 400);
}

void testEncoderPadsLikeTheParser() {
    // An address or id whose length is already a multiple of 4 still needs
    // its null AND a full pad word. Off by one here shifts every argument
    // after it, which shows up as garbage values rather than a parse error.
    auto m = oscMessage("/abc", "wxyz", 0.5f);   // both exactly 4 chars
    CHECK_EQ(static_cast<int>(m.size()), 8 + 4 + 8 + 4);
    std::vector<OscMessage> out;
    CHECK(oscParse(m.data(), m.size(), out));
    CHECK(out[0].address == "/abc");
    CHECK(out[0].args[0].s == "wxyz");
    CHECK_NEAR(out[0].args[1].number(), 0.5f, 1e-6f);
}

// --- mapping ---------------------------------------------------------------

void testAbsoluteMapping() {
    MappingTable t;
    CHECK(t.loadFromString("cc:1:74 g.detail abs\n"));
    ParamBank b = freshBank();
    CHECK(t.apply(ccSource(1, 74), 1.0f, b));
    CHECK_NEAR(b.norm(b.find(gid::kDetail)), 1.0f, 1e-6f);
    CHECK(t.apply(ccSource(1, 74), 0.0f, b));
    CHECK_NEAR(b.norm(b.find(gid::kDetail)), 0.0f, 1e-6f);
}

void testUnmappedSourceIsNotAnError() {
    MappingTable t;
    t.loadFromString("cc:1:74 g.detail abs\n");
    ParamBank b = freshBank();
    CHECK(!t.apply(ccSource(1, 9), 1.0f, b));
}

void testRelativeMapping() {
    MappingTable t;
    CHECK(t.loadFromString("cc:1:70 g.zoom rel 0 1 0.1\n"));
    ParamBank b = freshBank();
    int z = b.find(gid::kZoom);
    b.setNorm(z, 0.5f);
    t.apply(ccSource(1, 70), 1.0f, b);   // one detent CW
    CHECK_NEAR(b.norm(z), 0.6f, 1e-5f);
    t.apply(ccSource(1, 70), 0.0f, b);   // one detent CCW
    CHECK_NEAR(b.norm(z), 0.5f, 1e-5f);
}

void testRangeLimits() {
    MappingTable t;
    CHECK(t.loadFromString("cc:1:74 g.detail abs 0.25 0.75\n"));
    ParamBank b = freshBank();
    int d = b.find(gid::kDetail);
    t.apply(ccSource(1, 74), 0.0f, b);
    CHECK_NEAR(b.norm(d), 0.25f, 1e-6f);
    t.apply(ccSource(1, 74), 1.0f, b);
    CHECK_NEAR(b.norm(d), 0.75f, 1e-6f);
}

void testInvertedRange() {
    // lo > hi is not a mistake to reject -- it is how a control is inverted.
    MappingTable t;
    CHECK(t.loadFromString("cc:1:74 g.detail abs 1 0\n"));
    ParamBank b = freshBank();
    int d = b.find(gid::kDetail);
    t.apply(ccSource(1, 74), 0.0f, b);
    CHECK_NEAR(b.norm(d), 1.0f, 1e-6f);
    t.apply(ccSource(1, 74), 1.0f, b);
    CHECK_NEAR(b.norm(d), 0.0f, 1e-6f);
}

void testOneSourceDrivesManyParams() {
    // A macro control: one knob, several destinations.
    MappingTable t;
    CHECK(t.loadFromString("cc:1:20 g.zoom abs\ncc:1:20 g.rotation abs\n"));
    ParamBank b = freshBank();
    CHECK(t.apply(ccSource(1, 20), 1.0f, b));
    CHECK_NEAR(b.norm(b.find(gid::kZoom)), 1.0f, 1e-6f);
    CHECK_NEAR(b.norm(b.find(gid::kRotation)), 1.0f, 1e-6f);
}

void testCommentsAndBlankLines() {
    MappingTable t;
    CHECK(t.loadFromString("# a comment\n\n  \ncc:1:74 g.detail abs  # trailing\n"));
    CHECK_EQ(static_cast<int>(t.size()), 1);
}

void testMissingParamIsAnError() {
    MappingTable t;
    std::string err;
    CHECK(!t.loadFromString("cc:1:74\n", &err));
    CHECK(!err.empty());
}

void testBadModeIsAnError() {
    MappingTable t;
    std::string err;
    CHECK(!t.loadFromString("cc:1:74 g.detail sideways\n", &err));
    CHECK(!err.empty());
}

void testDanglingParamIsKeptAndReported() {
    // A mapping may name an effect knob that is not in the chain right now.
    // It must survive and be reported, not be silently dropped.
    MappingTable t;
    CHECK(t.loadFromString("cc:1:74 fx3.radius abs\n"));
    ParamBank b = freshBank();
    CHECK_EQ(static_cast<int>(t.size()), 1);
    CHECK(!t.apply(ccSource(1, 74), 1.0f, b));
    auto dangling = t.danglingParams(b);
    CHECK_EQ(static_cast<int>(dangling.size()), 1);
    CHECK(dangling[0] == "fx3.radius");
}

// ---- one table, two characters ------------------------------------------

void testTheSourceDecidesWhetherARowAddsOrSets() {
    // The rule the whole unification rests on, checked at the level of the
    // TABLE rather than of the LFO: nothing about a row's character is
    // written on the line, so a file cannot get it wrong and a later reader
    // cannot mistake it.
    MappingTable t;
    std::string err;
    CHECK(t.loadFromString(
        "sensor:jx g.center_x abs\n"
        "cc:1:20   g.speed    rel 0 1 0.02\n"
        "macro:0   g.zoom     abs 0.2 0.8\n"
        "lfo:2     g.zoom     0.0 0.4\n",
        &err));
    CHECK_EQ(static_cast<int>(t.size()), 4);
    CHECK(!t.all()[0].mod);
    CHECK(!t.all()[1].mod);
    // A macro is a KNOB. It has a position, so it sets -- being on the
    // instrument rather than on the wire changes nothing about that.
    CHECK(!t.all()[2].mod);
    CHECK(t.all()[3].mod);
    CHECK(isModSource("lfo:0"));
    CHECK(!isModSource("macro:0"));
    CHECK(!isModSource("sensor:lfo"));
}

void testAModulatorRowIsRefusedAsRelative() {
    MappingTable t;
    std::string err;
    CHECK(!t.loadFromString("lfo:0 g.zoom rel 0 1 0.02\n", &err));
    CHECK(!err.empty());
}

void testARangeMayStartWithoutAModeWord() {
    // `lfo:0 g.zoom 0 0.4` -- a modulator is neither absolute nor relative, so
    // requiring it to write one of those words would be asking for a fact that
    // does not exist. A numeric third token is the range starting early.
    MappingTable t;
    std::string err;
    CHECK(t.loadFromString("lfo:0 g.zoom 0.1 0.4\n", &err));
    CHECK_NEAR(t.all()[0].lo, 0.1f, 1e-6f);
    CHECK_NEAR(t.all()[0].hi, 0.4f, 1e-6f);
    // ...and a third token that is neither a keyword nor a number is still an
    // error rather than a silently ignored typo.
    MappingTable bad;
    CHECK(!bad.loadFromString("lfo:0 g.zoom sideways\n", &err));
    CHECK(!err.empty());
}

void testRuntimeEditsKeepTheIndexHonest() {
    // The table stopped being read-once the moment a learn gesture and a MAP
    // page could edit it, so the index it looks rows up by has to be rebuilt
    // by every one of those edits. A stale index is a mapping firing at the
    // wrong parameter, which is invisible until it is on stage.
    MappingTable t;
    CHECK(t.loadFromString(
        "lfo:0 g.zoom 0 1\n"
        "sensor:jx g.center_x abs\n"
        "lfo:0 g.detail 0 1\n"));
    CHECK_EQ(t.countFor("lfo:0"), 2);
    CHECK_EQ(t.countFor("sensor:jx"), 1);

    // Remove the row in the MIDDLE: every index after it shifts, and a table
    // that did not reindex would now drive g.detail from the joystick.
    t.removeRow(t.findRow("sensor:jx", "g.center_x"));
    CHECK_EQ(t.countFor("sensor:jx"), 0);
    CHECK_EQ(t.countFor("lfo:0"), 2);

    ParamBank b = freshBank();
    t.applyMod("lfo:0", 0.5f, b);
    CHECK_NEAR(b.mod(b.find("g.zoom")), 0.5f, 1e-5f);
    CHECK_NEAR(b.mod(b.find("g.detail")), 0.5f, 1e-5f);
    CHECK_EQ(b.mod(b.find("g.center_x")), 0.0f);

    // Out-of-range edits do nothing rather than corrupting anything.
    t.removeRow(-1);
    t.removeRow(99);
    CHECK(!t.setRange(99, 0.0f, 1.0f));
    CHECK_EQ(static_cast<int>(t.size()), 2);

    // A row added at runtime gets its character from its source, exactly like
    // a parsed one -- there is no way to add a row that lies about itself.
    Mapping m;
    m.source = "lfo:1";
    m.param = "g.speed";
    t.addRow(m);
    CHECK(t.all()[t.findRow("lfo:1", "g.speed")].mod);
}

void testClearSourceTouchesOnlyThatSource() {
    // What a preset recall does to the table, at the table's own level.
    MappingTable t;
    CHECK(t.loadFromString(
        "lfo:0 g.zoom 0 1\n"
        "lfo:1 g.detail 0 1\n"
        "sensor:jx g.center_x abs\n"
        "macro:0 g.speed abs\n"));
    t.clearSource("lfo:0");
    CHECK_EQ(t.countFor("lfo:0"), 0);
    CHECK_EQ(t.countFor("lfo:1"), 1);
    CHECK_EQ(t.countFor("sensor:jx"), 1);
    CHECK_EQ(t.countFor("macro:0"), 1);
    // Clearing something that was never there is not an error.
    t.clearSource("lfo:3");
    CHECK_EQ(static_cast<int>(t.size()), 3);
}

void testAModulatorRowIsInertUntilItsParameterExists() {
    // Same rule the whole table has always had, now reaching modulation: a
    // row naming a knob that is not in the bank right now is kept and quiet.
    MappingTable t;
    CHECK(t.loadFromString("lfo:0 fx9.radius 0 1\n"));
    ParamBank b = freshBank();
    int fired = t.applyMod("lfo:0", 1.0f, b);
    CHECK_EQ(fired, 0);
    CHECK_EQ(static_cast<int>(t.size()), 1);
    auto dangling = t.danglingParams(b);
    CHECK_EQ(static_cast<int>(dangling.size()), 1);
}

void testDefaultsAreValid() {
    MappingTable t;
    t.loadDefaults();
    CHECK(t.size() >= 8);
    ParamBank b = freshBank();
    // Every default must name a parameter that actually exists.
    CHECK(t.danglingParams(b).empty());
}

}  // namespace

int main() {
    testParseFloat();
    testParseInts();
    testAddressPaddingIsExact();
    testStringArg();
    testBundleFlattens();
    testTruncatedIsRejectedCleanly();
    testGarbageIsRejected();
    testUnknownTypeTagRejected();

    testSensorTelemetry();
    testSensorIdsStayDistinct();
    testCcIsAlwaysDividedBy127();
    testCcFullScale();
    testExpressionIdReachesSource();
    testMalformedBusMessagesRejected();
    testValuesAreClamped();

    testParamCommandDecodes();
    testParamValueIsClampedNotScaled();
    testParamSetsNormalizedPositionByCurve();
    testUnknownParamIdIsIgnoredNotFatal();
    testParamWithNoIdIsRejected();
    testPresetModeKernelDecode();
    testLfoTargetDecodes();
    testIntArgAcceptedAsFloat();
    testUnknownVerbInNamespaceIsClaimedButRefused();
    testMissingArgsRejected();

    testDirectSelectionVerbsDecode();
    testOperatorAndSourceVerbsDecode();
    testPaletteVerbCarriesSlotAndIndex();
    testListRequestDecodesWithAndWithoutArgument();
    testListDatagramsCarryNamesAndTerminate();
    testListSplitsAndLosesNothing();

    testStateDatagramsRoundTrip();
    testSelCarriesEverySelectionAndStaysBackwardCompatible();
    testStateSkipsHiddenPadding();
    testStateSplitsIntoSaneDatagrams();
    testEncoderPadsLikeTheParser();

    testAbsoluteMapping();
    testUnmappedSourceIsNotAnError();
    testRelativeMapping();
    testRangeLimits();
    testInvertedRange();
    testOneSourceDrivesManyParams();
    testCommentsAndBlankLines();
    testMissingParamIsAnError();
    testBadModeIsAnError();
    testDanglingParamIsKeptAndReported();
    testTheSourceDecidesWhetherARowAddsOrSets();
    testAModulatorRowIsRefusedAsRelative();
    testARangeMayStartWithoutAModeWord();
    testRuntimeEditsKeepTheIndexHonest();
    testClearSourceTouchesOnlyThatSource();
    testAModulatorRowIsInertUntilItsParameterExists();
    testDefaultsAreValid();
    return check::finish("control");
}
