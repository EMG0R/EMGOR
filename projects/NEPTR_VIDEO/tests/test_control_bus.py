#!/usr/bin/env python3
"""
test_control_bus.py -- the pure half of deploy/control_bus.py.

WHAT THIS SUITE CAN AND CANNOT PROVE

It proves everything that happens between a UDP datagram and the frame line
handed to the arcade: OSC decoding, thresholding, latching, joystick
reconstruction, the tick. All of that is portable Python and runs on the Mac.

It proves NOTHING about the arcade itself. The virtual pad is a Linux uinput
device (`games/lib/gamepad.py` -> `evdev.UInput`) and the supervisor drives
RetroArch and SIGSTOPs the audio chain -- none of which exists on macOS. The
seam is deliberate and it is exactly the frame dict: this suite checks that the
dict is right, and `arcade_supervisor.py` is already the thing that turns a
correct frame into correct gameplay, unmodified and not retested here.

The `check.h` C++ suites next door use a bare assert-and-report style; this
does the same rather than importing unittest, so a failure prints the same
shape of line and the suite is runnable by hand with no runner.
"""

import os
import socket
import struct
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "deploy"))

import control_bus as cb   # noqa: E402

FAILED = []


def check(cond, what):
    if not cond:
        FAILED.append(what)
        print(f"  FAIL  {what}")
    else:
        print(f"  ok    {what}")


def osc(address, *args):
    """Encode an OSC message the way demiurge-io's osc.py does -- an
    independent implementation, so a shared bug cannot make the round trip
    look right. Any disagreement in padding shows up here as a decode
    failure, which is the point."""
    def s(x):
        b = x.encode("ascii") + b"\x00"
        return b + b"\x00" * (-len(b) % 4)
    tags, body = ",", b""
    for a in args:
        if isinstance(a, bool):
            raise TypeError("bool is not an OSC type")
        if isinstance(a, int):
            tags += "i"; body += struct.pack(">i", a)
        elif isinstance(a, float):
            tags += "f"; body += struct.pack(">f", a)
        elif isinstance(a, str):
            tags += "s"; body += s(a)
        else:
            raise TypeError(repr(a))
    return s(address) + s(tags) + body


def bundle(*msgs):
    out = b"#bundle\x00" + struct.pack(">Q", 1)
    for m in msgs:
        out += struct.pack(">i", len(m)) + m
    return out


# ── OSC decode ──────────────────────────────────────────────────────────────

def test_osc_decode():
    print("osc decode")
    got = cb.osc_parse(osc("/sensor", "b1", 1.0))
    check(got == [("/sensor", ["b1", 1.0])], "/sensor <s> <f> round-trips")

    got = cb.osc_parse(osc("/cc", 1, 60, 127))
    check(got == [("/cc", [1, 60, 127])], "/cc <i> <i> <i> round-trips")

    # String lengths either side of the padding boundary: the classic decode
    # bug is a string whose length is an exact multiple of 4 (its null
    # terminator forces a WHOLE extra pad word) or one short of it.
    for sid in ("b", "jx", "eb7", "tof1", "abcde"):
        got = cb.osc_parse(osc("/sensor", sid, 0.25))
        check(got == [("/sensor", [sid, 0.25])],
              f"/sensor id={sid!r} ({len(sid)} chars) decodes")

    got = cb.osc_parse(bundle(osc("/sensor", "b1", 1.0), osc("/cc", 1, 77, 127)))
    check(got == [("/sensor", ["b1", 1.0]), ("/cc", [1, 77, 127])],
          "a #bundle flattens in order")

    # Garbage must be dropped, never raise: this is fed straight off a socket
    # during a game, and an exception here is a frozen instrument.
    for bad in (b"", b"\x00\x00\x00\x00", b"not-osc", osc("/sensor", "b1")[:6],
                b"/sensor\x00,z\x00\x00\x00\x00\x00\x00"):
        check(cb.osc_parse(bad) == [], f"malformed packet {bad!r:.24} is dropped")


# ── thresholding ────────────────────────────────────────────────────────────

def test_threshold():
    print("threshold + hysteresis")
    # tick() between the levels, because that is what a frame IS. Reading two
    # levels into one frame is the fast-tap case and it has its own test
    # below -- conflating the two here is what made an early version of this
    # suite assert the bug (that a press cancelled by its release should
    # simply vanish) as if it were the requirement.
    p = cb.PadState()
    check(p.frame()["b1"] == 0, "buttons start released")

    check(p.apply_sensor("b1", 1.0) is True, "0->1 is an edge")
    check(p.frame()["b1"] == 1, "b1 is pressed at norm 1.0")
    check(p.apply_sensor("b1", 1.0) is False, "a repeat of the same level is no edge")
    p.tick()
    check(p.apply_sensor("b1", 0.0) is True, "1->0 is an edge")
    p.tick()
    check(p.frame()["b1"] == 0, "b1 is released at norm 0.0")

    # Hysteresis: inside the band nothing changes, in EITHER direction.
    p.apply_sensor("b2", 0.5); p.tick()
    check(p.frame()["b2"] == 0, "0.5 does not press from released")
    p.apply_sensor("b2", 0.61); p.tick()
    check(p.frame()["b2"] == 1, "0.61 presses")
    p.apply_sensor("b2", 0.5); p.tick()
    check(p.frame()["b2"] == 1, "0.5 does not release from pressed")
    p.apply_sensor("b2", 0.39); p.tick()
    check(p.frame()["b2"] == 0, "0.39 releases")

    # Every discrete control the arcade reads must be reachable, including the
    # joystick click the exit gesture is built on -- if jb never arrives there
    # is no way out of a game, which is the failure mode-games.sh exists for.
    for f in cb.DISCRETE_FIELDS:
        p2 = cb.PadState()
        p2.apply_sensor(f, 1.0)
        check(p2.frame()[f] == 1, f"{f} is reachable from /sensor")
    check("jb" in cb.DISCRETE_FIELDS, "jb (exit gesture) is a discrete field")
    check(len(cb.DISCRETE_FIELDS) == 17, "9 buttons + 7 encoder buttons + jb")

    # A sensor that is not a pad control must be IGNORED, not guessed at.
    p3 = cb.PadState()
    check(p3.apply_sensor("tof1", 1.0) is False, "a LiDAR reading is not a button")
    check(p3.seen_any is False, "an unrelated sensor does not count as pad input")


def test_edge_survives_to_a_frame():
    """A press and its release between two frames must not cancel out.

    REGRESSION. This was observed swallowing every button in the end-to-end
    run against the real demiurge-io: the bus is event-driven and this reader
    is not, so both datagrams of a fast tap land in one drain() and the naive
    'latch the latest level' collapses them to 'nothing happened'. The button
    is then never seen pressed AT ALL -- not late, not briefly: never.

    The Teensy does not dodge this by being better designed, it dodges it by
    re-sending its whole state 100 times a second. A bus consumer has to hold
    the unshown edge for a frame, and that is what is checked here.
    """
    print("an edge survives to a frame")
    p = cb.PadState()
    p.tick()
    check(p.frame()["b1"] == 0, "starts released")

    # Press and release with no tick in between -- one drain(), two datagrams.
    p.apply_sensor("b1", 1.0)
    p.apply_sensor("b1", 0.0)
    p.tick()
    check(p.frame()["b1"] == 1, "the press is still shown for one frame")
    p.tick()
    check(p.frame()["b1"] == 0, "and the release lands on the next frame")
    p.tick()
    check(p.frame()["b1"] == 0, "and it stays released (the hold is one-shot)")

    # The exit gesture is the one that must never stick: a jb that latches
    # down means the game quits the instant it opens.
    p.apply_sensor("jb", 1.0)
    p.apply_sensor("jb", 0.0)
    p.tick(); p.tick()
    check(p.frame()["jb"] == 0, "jb does not stick on after a fast tap")

    # The ordinary case must not gain latency: a press with a tick after it is
    # visible on that very frame, not the one after.
    p2 = cb.PadState()
    p2.apply_sensor("b9", 1.0)
    p2.tick()
    check(p2.frame()["b9"] == 1, "a normal press is shown on the next frame, not the one after")
    p2.tick()
    check(p2.frame()["b9"] == 1, "and stays down while held")
    p2.apply_sensor("b9", 0.0)
    p2.tick()
    check(p2.frame()["b9"] == 0, "and releases promptly")

    # A hold that ticks many times must not re-fire or drift.
    p3 = cb.PadState()
    p3.apply_sensor("b1", 1.0)
    seen = set()
    for _ in range(50):
        p3.tick()
        seen.add(p3.frame()["b1"])
    check(seen == {1}, "a held button reads 1 on every frame, with no chatter")

    # A bounce that ends where it started (press, release, press) is a HELD
    # button, not two taps -- the release had zero duration.
    p4 = cb.PadState()
    p4.apply_sensor("b2", 1.0)
    p4.apply_sensor("b2", 0.0)
    p4.apply_sensor("b2", 1.0)
    p4.tick()
    p4.tick()
    check(p4.frame()["b2"] == 1, "press-release-press in one gap reads as held")

    # Independence: a fast tap on one button must not delay another.
    p5 = cb.PadState()
    p5.apply_sensor("b1", 1.0)
    p5.apply_sensor("b1", 0.0)
    p5.apply_sensor("b9", 1.0)
    p5.tick()
    f = p5.frame()
    check(f["b1"] == 1 and f["b9"] == 1, "a held edge on b1 does not stall b9")


# ── joystick ────────────────────────────────────────────────────────────────

def test_axes():
    print("joystick reconstruction")
    p = cb.PadState()
    f = p.frame()
    check(f["jx"] == 2048 and f["jy"] == 2048,
          "the stick starts CENTRED (an uncentred start walks the menu by itself)")

    p.apply_sensor("jx", 0.0)
    check(p.frame()["jx"] == 0, "norm 0.0 -> raw 0")
    p.apply_sensor("jx", 1.0)
    check(p.frame()["jx"] == 4095, "norm 1.0 -> raw 4095")
    p.apply_sensor("jx", 0.5)
    check(p.frame()["jx"] == 2048, "norm 0.5 -> raw 2048 (centre)")

    # Out-of-range input must clamp rather than produce a raw the arcade's
    # 12-bit maths has never seen.
    p.apply_sensor("jy", 3.0)
    check(p.frame()["jy"] == 4095, "norm above 1 clamps")
    p.apply_sensor("jy", -1.0)
    check(p.frame()["jy"] == 0, "norm below 0 clamps")

    # The reconstruction has to survive the arcade's own deadzone, which is
    # what actually decides a direction. Reproduce lib/gamepad.py's _axis()
    # with games.conf's default deadzone and check the ends still read as
    # full deflection and the centre as neutral -- a rounding error here is a
    # stick that will not reach a direction and reads as broken hardware.
    def axis(raw, deadzone=900, center=2048):
        d = raw - center
        return -1 if d <= -deadzone else (1 if d >= deadzone else 0)

    for norm, want in ((0.0, -1), (0.1, -1), (0.5, 0), (0.55, 0), (0.9, 1), (1.0, 1)):
        p.apply_sensor("jx", norm)
        check(axis(p.frame()["jx"]) == want,
              f"norm {norm} reads as direction {want} through the arcade deadzone")


# ── the /cc fallback ────────────────────────────────────────────────────────

def test_cc_fallback():
    print("/cc fallback")
    # The CC table is BUTTONS.md's, and getting it wrong is the kind of bug
    # that makes a game playable but with the wrong buttons -- so the
    # canonical roles are pinned by number here, not by construction.
    roles = {60: "b1", 63: "b4", 61: "b2", 64: "b5", 62: "b3",
             68: "b9", 67: "b8", 65: "b6", 66: "b7", 77: "jb"}
    for cc, field in roles.items():
        check(cb.CC_TO_FIELD.get(cc) == field, f"CC{cc} is {field} (BUTTONS.md)")

    p = cb.PadState()
    p.apply_message("/cc", [1, 60, 127])
    p.tick()
    check(p.frame()["b1"] == 1, "/cc 60 127 presses ARCADE")
    p.apply_message("/cc", [1, 60, 0])
    p.tick()
    check(p.frame()["b1"] == 0, "/cc 60 0 releases ARCADE")

    # 7-bit joystick, the lossy path. It must still centre and still reach
    # both ends, or the fallback is worse than no fallback.
    p.apply_message("/cc", [1, 27, 64])
    check(abs(p.frame()["jx"] - 2048) < 40, "/cc jx 64 is about centre")
    p.apply_message("/cc", [1, 27, 127])
    check(p.frame()["jx"] == 4095, "/cc jx 127 is full right")
    p.apply_message("/cc", [1, 27, 0])
    check(p.frame()["jx"] == 0, "/cc jx 0 is full left")

    # An encoder is a RELATIVE delta on this bus, not a level. Feeding one to
    # a pad would latch a phantom button, so it must be ignored outright.
    p2 = cb.PadState()
    for cc in range(20, 27):
        check(p2.apply_message("/cc", [1, cc, 127]) is False,
              f"encoder CC{cc} is not a pad control")
    check(p2.seen_any is False, "encoders do not count as pad input")


# ── frames ──────────────────────────────────────────────────────────────────

def test_frame_line():
    print("frame line")
    p = cb.PadState()
    p.apply_sensor("b1", 1.0)
    p.apply_sensor("jx", 0.0)
    line = p.frame_line()
    check(line.startswith("NEPTR2 "), "the line has the arcade's frame prefix")

    # Parse it back with the ARCADE'S OWN parser if it is on this machine, so
    # the contract is checked against the real consumer and not a copy of it.
    # It is pure string handling with no imports, so it runs fine on the Mac.
    parsed = arcade_parse_frame(line)
    check(parsed is not None, "arcade_supervisor.parse_frame() accepts the line")
    if parsed:
        check(parsed["b1"] == 1, "b1 survives the round trip")
        check(parsed["jx"] == 0, "jx survives the round trip")
        # Every field, every frame: the supervisor reads frame.get(f, 0), so a
        # field that vanishes on release is indistinguishable from one that was
        # never sent -- and a stuck button in a game is unrecoverable.
        for f in cb.DISCRETE_FIELDS + cb.AXIS_FIELDS:
            check(f in parsed, f"{f} is present in every frame")
        check(all(isinstance(v, int) for v in parsed.values()),
              "every field parses as an int")


def arcade_parse_frame(line):
    """A local copy of arcade_supervisor's parse_frame, used only if the real
    one is not importable (it lives in DEMIURGE_OS, which is not deployed
    beside this tree on every machine). Preferring the real one keeps this
    honest; falling back keeps the suite runnable anywhere."""
    real = _real_parse_frame()
    if real is not None:
        return real(line)
    if not line.startswith("NEPTR2 "):
        return None
    out = {}
    for tok in line.split()[1:]:
        if "=" in tok:
            k, v = tok.split("=", 1)
            try:
                out[k] = int(v)
            except ValueError:
                pass
    return out


_REAL = "unset"


def _real_parse_frame():
    global _REAL
    if _REAL != "unset":
        return _REAL
    _REAL = None
    here = os.path.dirname(os.path.abspath(__file__))
    guess = os.path.abspath(os.path.join(
        here, "..", "..", "DEMIURGE_OS", "demiurge", "games", "lib", "teensy.py"))
    env = os.environ.get("NV_GAMES_DIR")
    cands = [os.path.join(env, "lib", "teensy.py")] if env else []
    cands.append(guess)
    for path in cands:
        if not os.path.isfile(path):
            continue
        # Read and exec only parse_frame's source: importing lib.teensy pulls
        # termios and would tie this suite to a platform for no reason.
        src = open(path).read()
        i = src.find("def parse_frame(")
        j = src.find("\nclass ", i)
        if i < 0:
            continue
        ns = {}
        exec(compile(src[i:j if j > 0 else len(src)], path, "exec"), ns)
        _REAL = ns.get("parse_frame")
        print(f"  ..    using the real parse_frame from {path}")
        break
    return _REAL


# ── the socket + tick half ──────────────────────────────────────────────────

def test_reader_over_udp():
    print("reader over a real socket")
    # A free port, taken by binding 0 and reading it back -- a hardcoded one
    # makes the suite fail for a reason that has nothing to do with the code.
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    probe.bind(("127.0.0.1", 0))
    port = probe.getsockname()[1]
    probe.close()

    r = cb.BusReader(port=port, tick_hz=200.0)
    tx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        gen = r.lines()
        first = next(gen)
        check(first.startswith("NEPTR2 "), "a frame ticks out with no input at all")
        check(arcade_parse_frame(first)["b1"] == 0, "and it reads as nothing pressed")

        tx.sendto(osc("/sensor", "b1", 1.0), ("127.0.0.1", port))
        got = None
        for _ in range(50):
            f = arcade_parse_frame(next(gen))
            if f["b1"] == 1:
                got = f
                break
        check(got is not None, "a /sensor press arrives over UDP and lands in a frame")

        tx.sendto(osc("/sensor", "b1", 0.0), ("127.0.0.1", port))
        rel = None
        for _ in range(50):
            f = arcade_parse_frame(next(gen))
            if f["b1"] == 0:
                rel = f
                break
        check(rel is not None, "and the release lands too")

        # The tick is the whole reason the jb-hold exit gesture can fire: the
        # supervisor evaluates the hold TIMER on frame arrival, so frames must
        # keep coming while a button is simply held down and nothing changes.
        tx.sendto(osc("/sensor", "jb", 1.0), ("127.0.0.1", port))
        t0 = time.monotonic()
        n = 0
        while time.monotonic() - t0 < 0.25:
            f = arcade_parse_frame(next(gen))
            n += 1
            if not f["jb"]:
                continue
        check(n > 5, f"frames keep ticking while jb is held (got {n} in 0.25 s)")
        check(arcade_parse_frame(next(gen))["jb"] == 1, "and jb stays latched down")
    finally:
        tx.close()
        r.close()


def main():
    for t in (test_osc_decode, test_threshold, test_edge_survives_to_a_frame,
              test_axes, test_cc_fallback,
              test_frame_line, test_reader_over_udp):
        t()
    print()
    if FAILED:
        print(f"FAILED {len(FAILED)}:")
        for f in FAILED:
            print(f"  - {f}")
        return 1
    print("control_bus: all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
