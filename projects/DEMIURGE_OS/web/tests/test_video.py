"""The video engine link — the /nv/ namespace, both directions.

These run against a real UDP socket on loopback, standing in for the engine,
because every bug this file has caught was a WIRE bug: a bundle that was not
unbundled, and a state dict that was merged when it had to be replaced.
Testing the parsing in isolation would have caught neither.
"""
import socket
import struct
import sys
import time
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))


def _bundle(msgs):
    """The engine's own framing: "#bundle" + an 8-byte time tag, then each
    element prefixed by its int32 size (control/osc.cpp oscBundle)."""
    out = b"#bundle\0" + struct.pack(">II", 0, 1)
    for m in msgs:
        out += struct.pack(">I", len(m)) + m
    return out


def _fake_engine():
    """A socket standing in for neptr-video: binds a port, and replies to
    whoever last spoke to it — which is exactly the engine's own subscription
    rule (Receiver::poll uses recvfrom and remembers the peer)."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("127.0.0.1", 0))
    s.settimeout(2.0)
    return s, s.getsockname()[1]


def _wait(pred, secs=3.0):
    end = time.monotonic() + secs
    while time.monotonic() < end:
        if pred():
            return True
        time.sleep(0.02)
    return False


def test_osc_decode_any_unbundles():
    from demiurge_web import osc_decode_any, osc_encode
    data = _bundle([osc_encode("/nv/sel", 1, 7),
                    osc_encode("/nv/state", "g.zoom", 0.25),
                    osc_encode("/nv/state", "g.detail", 0.5)])
    got = osc_decode_any(data)
    assert [a for a, _ in got] == ["/nv/sel", "/nv/state", "/nv/state"]
    assert got[0][1] == [1, 7]
    assert got[1][1][0] == "g.zoom"
    assert abs(got[1][1][1] - 0.25) < 1e-6


def test_osc_decode_any_passes_a_bare_message_through():
    from demiurge_web import osc_decode_any, osc_encode
    assert osc_decode_any(osc_encode("/nv/sel", 0, 0)) == [("/nv/sel", [0, 0])]


def test_param_write_reaches_the_engine():
    from demiurge_web import VideoOut, osc_decode_any
    eng, port = _fake_engine()
    out = VideoOut(port=port)
    assert out.param("g.detail", 0.75)
    data, _ = eng.recvfrom(4096)
    addr, args = osc_decode_any(data)[0]
    assert addr == "/nv/param"
    assert args[0] == "g.detail"
    assert abs(args[1] - 0.75) < 1e-6


def test_select_verbs_are_single_ints():
    from demiurge_web import VideoOut, osc_decode_any
    eng, port = _fake_engine()
    out = VideoOut(port=port)
    for verb, n in (("mode", 2), ("kernel", 11), ("preset", 3)):
        out.select(verb, n)
        addr, args = osc_decode_any(eng.recvfrom(4096)[0])[0]
        assert addr == "/nv/" + verb
        assert args == [n]


def test_snapshot_reads_the_broadcast_and_pings_to_stay_subscribed():
    from demiurge_web import VideoOut, osc_encode
    eng, port = _fake_engine()
    out = VideoOut(port=port)
    out.want()                                  # somebody is looking
    # The keepalive is what makes us the peer; without a ping the engine has
    # no address to answer.
    _, peer = eng.recvfrom(4096)
    eng.sendto(_bundle([osc_encode("/nv/sel", 1, 4),
                        osc_encode("/nv/state", "g.zoom", 0.25)]), peer)
    # The first /nv/sel only OPENS a broadcast; it is the next one that
    # publishes it.
    eng.sendto(_bundle([osc_encode("/nv/sel", 1, 4)]), peer)
    assert _wait(lambda: out.snapshot()["params"])
    s = out.snapshot()
    assert s["live"] and s["mode"] == 1 and s["kernel"] == 4
    assert abs(s["params"]["g.zoom"] - 0.25) < 1e-6


def test_a_new_broadcast_replaces_the_bank_it_does_not_merge():
    """The bank's dynamic tail is rebuilt when the generator or the chain
    changes. A merged dict would keep serving the previous generator's knobs
    forever, and the browser would draw live-looking faders for parameters the
    engine ignores every write to."""
    from demiurge_web import VideoOut, osc_encode
    eng, port = _fake_engine()
    out = VideoOut(port=port)
    out.want()
    _, peer = eng.recvfrom(4096)

    eng.sendto(_bundle([osc_encode("/nv/sel", 0, 25),
                        osc_encode("/nv/state", "k.scale", 0.5),
                        osc_encode("/nv/state", "k.wobble", 0.2)]), peer)
    eng.sendto(_bundle([osc_encode("/nv/sel", 0, 3),
                        osc_encode("/nv/state", "k.cre", 0.9)]), peer)
    eng.sendto(_bundle([osc_encode("/nv/sel", 0, 3)]), peer)
    assert _wait(lambda: "k.cre" in out.snapshot()["params"])
    ids = set(out.snapshot()["params"])
    assert ids == {"k.cre"}, "stale ids survived a generator change: %r" % ids


def test_a_split_broadcast_is_assembled_before_it_is_published():
    """One broadcast is several datagrams (kMaxDatagram 1200) and only the
    first carries /nv/sel. Publishing per datagram would show the browser a
    third of the bank and mark the rest missing."""
    from demiurge_web import VideoOut, osc_encode
    eng, port = _fake_engine()
    out = VideoOut(port=port)
    out.want()
    _, peer = eng.recvfrom(4096)
    eng.sendto(_bundle([osc_encode("/nv/sel", 0, 0),
                        osc_encode("/nv/state", "a", 0.1)]), peer)
    eng.sendto(_bundle([osc_encode("/nv/state", "b", 0.2)]), peer)
    eng.sendto(_bundle([osc_encode("/nv/state", "c", 0.3)]), peer)
    eng.sendto(_bundle([osc_encode("/nv/sel", 0, 0)]), peer)
    assert _wait(lambda: len(out.snapshot()["params"]) == 3)
    assert set(out.snapshot()["params"]) == {"a", "b", "c"}


def test_video_param_validates_shape_only(tmp_path):
    from demiurge_web import Actions, VideoOut
    eng, port = _fake_engine()
    a = Actions(None, None, tmp_path / "log", video=VideoOut(port=port))
    assert a.video_param("g.detail", 0.5)[0] == 200
    eng.recvfrom(4096)
    # An id the current patch does not have is the ENGINE's business to
    # ignore: the bank's tail changes with the generator and the chain, so a
    # surface will legitimately name knobs that are not there.
    assert a.video_param("no.such.knob", 0.5)[0] == 200
    eng.recvfrom(4096)
    for bad in ("", "  ", 7, None, "x" * 65, "nøn-ascii"):
        assert a.video_param(bad, 0.5)[0] == 400
    for bad in (None, "0.5", True):
        assert a.video_param("g.detail", bad)[0] == 400


def test_video_select_whitelists_the_verb(tmp_path):
    from demiurge_web import Actions, VideoOut
    eng, port = _fake_engine()
    a = Actions(None, None, tmp_path / "log", video=VideoOut(port=port))
    assert a.video_select("kernel", 4)[0] == 200
    eng.recvfrom(4096)
    # `what` becomes an OSC address, so it is a whitelist and not a format
    # string. An unknown verb must never reach the wire.
    for bad in ("ping", "param", "", None, "../nv/param"):
        assert a.video_select(bad, 0)[0] == 400
    for bad in (None, 1.5, "2", True):
        assert a.video_select("kernel", bad)[0] == 400
