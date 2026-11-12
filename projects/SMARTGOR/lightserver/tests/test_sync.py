"""Output-delay AV sync: delay buffer math, live delay changes, 0 ==
passthrough, calibration-mode bypass, persistence round-trip."""
import json

import numpy as np
import pytest
from fastapi.testclient import TestClient

from lightserver.api import create_app
from lightserver.nodes import NodeConfig
from lightserver.persist import StateStore
from lightserver.ring import RingEngine, RingManager, RingSegment
from lightserver.sync import (MAX_DELAY_MS, SyncManager, frames_for_delay)

CONFIGS = [
    NodeConfig(name="fcob-strip", host="fcob-strip.local", pixel_count=800),
    NodeConfig(name="fcob-strip2", host="fcob-strip2.local", pixel_count=480),
    NodeConfig(name="fcob-strip3", host="fcob-strip3.local", pixel_count=640),
]


def make_segments():
    return [RingSegment("fcob-strip", 0, 800),
            RingSegment("fcob-strip2", 800, 480),
            RingSegment("fcob-strip3", 1280, 640)]


# -- frame count math -------------------------------------------------------

def test_frames_for_delay_at_max_30fps():
    # 500ms / 33.33ms = 15.0 -> ceil 15, +1 slack -> 16
    assert frames_for_delay(500, 30) == 16


def test_frames_for_delay_zero():
    assert frames_for_delay(0, 30) == 1


def test_frames_for_delay_rounds_up():
    # 100ms / 33.33ms = 3.0 -> ceil 3, +1 -> 4
    assert frames_for_delay(100, 30) == 4


# -- SyncManager buffer behavior ---------------------------------------------

def test_zero_delay_is_passthrough():
    sm = SyncManager(fps=30)
    f = np.full((10, 3), 5, dtype=np.uint8)
    sm.push(f)
    out = sm.delayed_frame(f)
    assert out is f


def test_delayed_frame_returns_frame_from_n_frames_ago():
    sm = SyncManager(fps=30)
    sm.set_delay_ms(100)  # ~3 frames ago at 30fps
    frames = [np.full((4, 3), i, dtype=np.uint8) for i in range(10)]
    for f in frames:
        sm.push(f)
    out = sm.delayed_frame(frames[-1])
    # 100ms / 33.33ms = 3.0 frames ago -> frames[-1 - 3] = frames[6]
    assert out[0, 0] == frames[6][0, 0]


def test_startup_ramp_up_clamps_to_oldest_available():
    sm = SyncManager(fps=30)
    sm.set_delay_ms(500)
    f0 = np.full((4, 3), 42, dtype=np.uint8)
    sm.push(f0)
    out = sm.delayed_frame(f0)
    # only one frame buffered -> clamp to it, never raise/underflow
    assert out[0, 0] == 42


def test_live_delay_change_takes_effect_next_frame():
    sm = SyncManager(fps=30)
    frames = [np.full((4, 3), i, dtype=np.uint8) for i in range(20)]
    for f in frames:
        sm.push(f)
    sm.set_delay_ms(0)
    assert sm.delayed_frame(frames[-1])[0, 0] == 19
    sm.set_delay_ms(200)  # 6 frames ago
    out = sm.delayed_frame(frames[-1])
    assert out[0, 0] == frames[13][0, 0]


def test_set_delay_ms_rejects_out_of_range():
    sm = SyncManager(fps=30)
    with pytest.raises(ValueError):
        sm.set_delay_ms(-1)
    with pytest.raises(ValueError):
        sm.set_delay_ms(MAX_DELAY_MS + 1)


def test_buffer_bounded_at_max_delay_frame_count():
    sm = SyncManager(fps=30)
    for i in range(1000):
        sm.push(np.full((4, 3), i % 256, dtype=np.uint8))
    assert len(sm._buffer) == frames_for_delay(MAX_DELAY_MS, 30)


# -- persistence --------------------------------------------------------

def test_sync_persist_round_trip():
    sm = SyncManager(fps=30)
    sm.set_delay_ms(150)
    saved = sm.for_persist()
    sm2 = SyncManager(fps=30)
    sm2.restore(saved)
    assert sm2.output_delay_ms == 150


def test_sync_restore_ignores_bad_values():
    sm = SyncManager(fps=30)
    sm.restore({"output_delay_ms": 99999})
    assert sm.output_delay_ms == 0
    sm.restore({"output_delay_ms": "nope"})
    assert sm.output_delay_ms == 0
    sm.restore(None)
    assert sm.output_delay_ms == 0


# -- RingEngine integration: delay applies to strips/satellites, not taps --

def ring_engine(mode="headless"):
    return RingEngine(make_segments(), CONFIGS, mode=mode, seed=1)


def test_strip_output_is_delayed_taps_get_current_frame():
    eng = ring_engine(mode="udp")
    fakes = {}
    for name in eng.senders:
        eng.senders[name].close()
        fakes[name] = eng.senders[name] = FakeSender()
    eng.streaming = True
    eng.sync.set_delay_ms(100)  # ~3 frames

    tap_frames = []
    orig_emit = eng.taps.emit
    def spy_emit(frame, scene=None, output_delay_ms=0):
        tap_frames.append(frame.copy())
        return orig_emit(frame, scene=scene, output_delay_ms=output_delay_ms)
    eng.taps.emit = spy_emit

    # paint a distinct color each frame at ring index 0, step 5 frames
    colors = []
    for i in range(6):
        c = (i * 10, 0, 0)
        eng.set_scene({"name": f"m{i}", "layers": [{"type": "paint",
                       "background": "#000000",
                       "pixels": {"0": "#%02x0000" % (i * 10)}}]}, crossfade=0)
        eng.step()
        colors.append(c)

    # taps always see the CURRENT frame (last step's paint)
    assert tap_frames[-1][0, 0] == colors[-1][0]
    # strip output at ring index 0 is delayed relative to current
    strip_frame_bytes = fakes["fcob-strip"].frames[-1]
    strip_r = strip_frame_bytes[0]
    assert strip_r != colors[-1][0]  # not the current frame's value
    eng.shutdown()


def test_zero_delay_strip_matches_current_frame():
    eng = ring_engine(mode="udp")
    fakes = {}
    for name in eng.senders:
        eng.senders[name].close()
        fakes[name] = eng.senders[name] = FakeSender()
    eng.streaming = True
    assert eng.sync.output_delay_ms == 0
    eng.set_scene({"name": "m", "layers": [{"type": "paint",
                   "background": "#000000", "pixels": {"0": "#0a0000"}}]},
                  crossfade=0)
    eng.step()
    assert fakes["fcob-strip"].frames[-1][0] == 0x0a
    eng.shutdown()


def test_calibration_active_bypasses_delay_for_everyone():
    eng = ring_engine(mode="udp")
    fakes = {}
    for name in eng.senders:
        eng.senders[name].close()
        fakes[name] = eng.senders[name] = FakeSender()
    eng.streaming = True
    eng.sync.set_delay_ms(300)
    eng.calibration.start()
    tap_frames = []
    orig_emit = eng.taps.emit
    def spy_emit(frame, scene=None, output_delay_ms=0):
        tap_frames.append(frame.copy())
        return orig_emit(frame, scene=scene, output_delay_ms=output_delay_ms)
    eng.taps.emit = spy_emit
    eng.step()
    eng.step()
    strip_slice = fakes["fcob-strip"].frames[-1]
    tap_last = tap_frames[-1]
    # overlay frame goes to strips AND taps identically (undelayed)
    assert bytes(strip_slice[:3]) == bytes(tap_last[0].tolist())
    eng.shutdown()


class FakeSender:
    def __init__(self):
        self.frames = []
        self.resolved_ip = "1.2.3.4"
        self.ip = "1.2.3.4"
        self.frames_sent = 0
        self.last_send_ts = None
        self.last_error = None

    def send_frame(self, b):
        self.frames.append(b)
        self.frames_sent += 1

    def close(self):
        pass


# -- API ----------------------------------------------------------------

@pytest.fixture()
def rmanager():
    m = RingManager.from_configs(CONFIGS, make_segments(), mode="headless")
    yield m
    m.shutdown()


@pytest.fixture()
def rclient(rmanager):
    return TestClient(create_app(rmanager.default, rmanager))


def test_get_sync_default(rclient):
    r = rclient.get("/sync")
    assert r.status_code == 200
    assert r.json() == {"output_delay_ms": 0}


def test_post_sync_sets_delay(rclient, rmanager):
    r = rclient.post("/sync", json={"output_delay_ms": 150})
    assert r.status_code == 200
    assert r.json() == {"output_delay_ms": 150}
    assert rmanager.ring.sync.output_delay_ms == 150


def test_post_sync_out_of_range_422(rclient):
    assert rclient.post("/sync", json={"output_delay_ms": -5}).status_code == 422
    assert rclient.post("/sync", json={"output_delay_ms": 501}).status_code == 422


def test_sync_persists_and_restores(tmp_path):
    store = StateStore(tmp_path / "state.json")
    m = RingManager.from_configs(CONFIGS, make_segments(), mode="headless")
    m.store = store
    m.ring.sync.set_delay_ms(220)
    m.persist()
    m.shutdown()
    saved = json.loads((tmp_path / "state.json").read_text())
    assert saved["sync"]["output_delay_ms"] == 220

    m2 = RingManager.from_configs(CONFIGS, make_segments(), mode="headless")
    m2.restore(store)
    assert m2.ring.sync.output_delay_ms == 220
    m2.shutdown()


def test_sync_persist_does_not_clobber_other_keys(tmp_path):
    store = StateStore(tmp_path / "state.json")
    m = RingManager.from_configs(CONFIGS, make_segments(), mode="headless")
    m.store = store
    m.ring.set_scene({"name": "keep", "layers": [{"type": "solid",
                      "color": "#112233"}]}, crossfade=0)
    m.ring.sync.set_delay_ms(75)
    m.persist()
    saved = json.loads((tmp_path / "state.json").read_text())
    assert saved["nodes"]["ring"]["scene"]["name"] == "keep"
    assert saved["sync"]["output_delay_ms"] == 75
    m.shutdown()


# -- semantic packet carries output_delay_ms ------------------------------

def test_semantic_packet_carries_output_delay_ms():
    eng = ring_engine(mode="udp")
    for name in eng.senders:
        eng.senders[name].close()
        eng.senders[name] = FakeSender()
    eng.streaming = True
    eng.sync.set_delay_ms(180)
    received = {}

    class FakeSock:
        def sendto(self, data, addr):
            if addr[1] == 9998:  # semantic_port only; ignore pixel chunks
                received["packet"] = json.loads(data.decode())

        def close(self):
            pass

    eng.taps._sock.close()
    eng.taps._sock = FakeSock()
    eng.taps.set_tap("t1", host="127.0.0.1", port=9999, semantic_port=9998)
    for _ in range(SEMANTIC_STEPS := 6):
        eng.step()
    assert received.get("packet", {}).get("output_delay_ms") == 180
    eng.shutdown()


# -- sync/pulse start/stop restores prior scene exactly ----------------------

def test_sync_pulse_start_stop_restores_prior_scene(rclient, rmanager):
    prior = {"name": "prior", "layers": [{"type": "solid", "color": "#445566"}]}
    rclient.post("/scene", json=prior)
    r = rclient.post("/sync/pulse/start")
    assert r.status_code == 200
    assert rmanager.ring.current_scene_name == "sync-pulse"
    r = rclient.post("/sync/pulse/stop")
    assert r.status_code == 200
    assert rmanager.ring.current_scene == prior


def test_sync_pulse_stop_without_start_is_a_noop(rclient, rmanager):
    prior = {"name": "prior2", "layers": [{"type": "solid", "color": "#010203"}]}
    rclient.post("/scene", json=prior)
    r = rclient.post("/sync/pulse/stop")
    assert r.status_code == 200
    assert rmanager.ring.current_scene == prior


def test_sync_ui_page_served(rclient):
    r = rclient.get("/sync/ui")
    assert r.status_code == 200
    assert "text/html" in r.headers["content-type"]
    assert "LED delay" in r.text
