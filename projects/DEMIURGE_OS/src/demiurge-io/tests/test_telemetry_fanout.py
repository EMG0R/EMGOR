"""Fan-out: one conditioned bus, many consumers.

The single hardcoded destination made this bus single-consumer in practice —
whoever bound the port won and everyone else silently got nothing. These tests
pin the two properties that matter: every destination receives every packet,
and one dead consumer cannot starve the others.
"""
import socket
import time

from demiurge_io.telemetry import TelemetryBroadcaster, parse_destination


def _listener():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("127.0.0.1", 0))
    s.settimeout(1.0)
    return s, s.getsockname()


def test_parse_destination_forms():
    assert parse_destination("8004") == ("127.0.0.1", 8004)
    assert parse_destination("127.0.0.1:8004") == ("127.0.0.1", 8004)
    assert parse_destination(" 10.0.0.5:9001 ") == ("10.0.0.5", 9001)


def test_parse_destination_rejects_junk():
    for bad in ("nope", "host:", "127.0.0.1:70000", "127.0.0.1:0"):
        try:
            parse_destination(bad)
        except ValueError:
            continue
        raise AssertionError("accepted %r" % bad)


def test_sensor_reaches_every_destination():
    a, aa = _listener()
    b, ba = _listener()
    t = TelemetryBroadcaster([aa, ba])
    assert t.publish("tof1", 0.5, 0.0) is True
    for s in (a, b):
        data, _ = s.recvfrom(2048)
        assert data.startswith(b"/sensor")
        assert b"tof1" in data
    a.close(); b.close()


def test_cc_reaches_every_destination_and_is_not_decimated():
    a, aa = _listener()
    b, ba = _listener()
    t = TelemetryBroadcaster([aa, ba])
    # Three presses back to back: a decimator would swallow two of them and
    # lose real button presses.
    for _ in range(3):
        t.publish_cc(1, 60, 127)
    for s in (a, b):
        for _ in range(3):
            data, _ = s.recvfrom(2048)
            assert data.startswith(b"/cc")
    a.close(); b.close()


def test_one_dead_consumer_does_not_starve_the_others():
    a, aa = _listener()
    dead = ("127.0.0.1", 1)          # nothing is listening there
    t = TelemetryBroadcaster([dead, aa])
    t.publish_cc(1, 63, 127)
    data, _ = a.recvfrom(2048)
    assert data.startswith(b"/cc")
    a.close()


def test_sensor_still_decimated_per_source():
    a, aa = _listener()
    t = TelemetryBroadcaster([aa])
    now = time.monotonic()
    assert t.publish("tof1", 0.1, now) is True
    assert t.publish("tof1", 0.2, now + 0.001) is False   # inside 1/20 s
    assert t.publish("tof2", 0.3, now + 0.001) is True    # different source
    a.close()
