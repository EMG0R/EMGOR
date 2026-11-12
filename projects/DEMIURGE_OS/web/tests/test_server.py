import sys, json, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
FX = pathlib.Path(__file__).parent / "fixtures"


def test_collect_mock_frame(tmp_path):
    from demiurge_web import MockProbes, collect, write_status
    frame = collect(MockProbes(FX), prev=None)
    assert frame["pwr"] in ("LOW", "MED", "HIGH")
    assert frame["throttled"]["hex"].startswith("0x")
    assert isinstance(frame["patches"], list)
    write_status(frame, tmp_path / "status")
    assert json.loads((tmp_path / "status").read_text())["pwr"] == frame["pwr"]


def test_probe_failure_yields_null():
    from demiurge_web import MockProbes, collect
    p = MockProbes(FX); p.broken = {"vcgencmd measure_temp"}
    assert collect(p, prev=None)["temp_c"] is None


def test_percore_needs_prev():
    from demiurge_web import MockProbes, collect
    p = MockProbes(FX)
    f1 = collect(p, prev=None)
    assert f1["percore"] == []
    f2 = collect(p, prev=f1)
    assert isinstance(f2["percore"], list)


# ───────────────────────── http server ─────────────────────────

import http.client, socket, threading, time


def _start_server():
    from demiurge_web import App, MockProbes, collect, make_server
    app = App()
    frame = collect(MockProbes(FX), prev=None)
    app.publish(frame)
    static = pathlib.Path(__file__).resolve().parents[1] / "static"
    srv = make_server(app, 0, static)
    t = threading.Thread(target=srv.serve_forever, daemon=True)
    t.start()
    return app, srv, srv.server_address[1]


def test_api_status_and_history():
    app, srv, port = _start_server()
    try:
        c = http.client.HTTPConnection("127.0.0.1", port, timeout=3)
        c.request("GET", "/api/status")
        r = c.getresponse()
        assert r.status == 200
        assert r.getheader("Content-Type") == "application/json"
        assert json.loads(r.read())["pwr"] in ("LOW", "MED", "HIGH")
        c.request("GET", "/api/history")
        r = c.getresponse()
        hist = json.loads(r.read())
        assert isinstance(hist, list) and len(hist) == 1
    finally:
        srv.shutdown()


def test_index_served():
    app, srv, port = _start_server()
    try:
        c = http.client.HTTPConnection("127.0.0.1", port, timeout=3)
        c.request("GET", "/")
        r = c.getresponse()
        assert r.status == 200
        assert r.getheader("Content-Type").startswith("text/html")
        r.read()
        c.request("GET", "/../demiurge_web.py")
        assert c.getresponse().status in (400, 404)
    finally:
        srv.shutdown()


def test_sse_pushes_frame():
    from demiurge_web import MockProbes, collect
    app, srv, port = _start_server()
    try:
        s = socket.create_connection(("127.0.0.1", port), timeout=3)
        s.sendall(b"GET /api/events HTTP/1.1\r\nHost: x\r\n\r\n")
        time.sleep(0.3)
        app.publish(collect(MockProbes(FX), prev=None))
        s.settimeout(3)
        buf = b""
        while b"data: " not in buf:
            buf += s.recv(4096)
        assert b"data: " in buf and b'"pwr"' in buf
        s.close()
    finally:
        srv.shutdown()


def test_sse_dead_client_pruned():
    from demiurge_web import MockProbes, collect
    app, srv, port = _start_server()
    try:
        s = socket.create_connection(("127.0.0.1", port), timeout=3)
        s.sendall(b"GET /api/events HTTP/1.1\r\nHost: x\r\n\r\n")
        deadline = time.time() + 3
        while len(app.subs) < 1 and time.time() < deadline:
            time.sleep(0.05)
        assert len(app.subs) == 1
        s.close()
        frame = collect(MockProbes(FX), prev=None)
        app.publish(frame)
        time.sleep(0.2)
        app.publish(frame)
        deadline = time.time() + 3
        while len(app.subs) > 0 and time.time() < deadline:
            time.sleep(0.05)
        assert len(app.subs) == 0
    finally:
        srv.shutdown()


# ───────────────────────── actions ─────────────────────────

class FakeRunner:
    def __init__(self, rc=0, out="ok", delay=0.0):
        self.calls, self.rc, self.out, self.delay = [], rc, out, delay
        self.stdins = []          # what each call got on stdin (secrets path)
        self._busy, self.overlapped = False, False

    def __call__(self, cmd, stdin=None):
        if self._busy:
            self.overlapped = True
        self._busy = True
        self.calls.append(cmd)
        self.stdins.append(stdin)
        if self.delay:
            time.sleep(self.delay)
        self._busy = False
        return self.rc, self.out


def _actions(tmp_path, runner):
    from demiurge_web import Actions
    return Actions(runner, lambda: ["car_mode", "neptr"],
                   tmp_path / "logs" / "web-actions.log")


def test_patch_valid_runs_demiurge_set(tmp_path):
    r = FakeRunner()
    status, res = _actions(tmp_path, r).patch("neptr")
    assert status == 200 and res["ok"] is True
    assert r.calls == [["demiurge-set", "neptr"]]
    log = (tmp_path / "logs" / "web-actions.log").read_text()
    assert "demiurge-set neptr" in log


def test_patch_unknown_400_runner_not_called(tmp_path):
    r = FakeRunner()
    status, res = _actions(tmp_path, r).patch("evil; rm -rf /")
    assert status == 400 and res["ok"] is False
    assert r.calls == []


def test_audio_stop_runs_sudo_systemctl(tmp_path):
    r = FakeRunner()
    status, res = _actions(tmp_path, r).audio("stop")
    assert status == 200
    assert r.calls == [["sudo", "systemctl", "stop", "demiurge"]]


def test_audio_bad_verb_400(tmp_path):
    r = FakeRunner()
    status, _ = _actions(tmp_path, r).audio("reboot")
    assert status == 400 and r.calls == []


def test_power_level_validated(tmp_path):
    r = FakeRunner()
    a = _actions(tmp_path, r)
    status, _ = a.power("med")
    assert status == 200 and r.calls[0][-1] == "med"
    status, _ = a.power("MAX")
    assert status == 400 and len(r.calls) == 1


def test_action_failure_returns_500_with_output(tmp_path):
    r = FakeRunner(rc=1, out="boom")
    status, res = _actions(tmp_path, r).audio("start")
    assert status == 500 and res["ok"] is False and "boom" in res["output"]


def test_actions_serialized(tmp_path):
    import threading as th
    r = FakeRunner(delay=0.05)
    a = _actions(tmp_path, r)
    ts = [th.Thread(target=a.audio, args=("start",)) for _ in range(4)]
    [t.start() for t in ts]
    [t.join() for t in ts]
    assert len(r.calls) == 4 and r.overlapped is False


def test_post_patch_endpoint(tmp_path):
    import http.client
    from demiurge_web import App, MockProbes, collect, make_server, Actions
    app = App()
    app.publish(collect(MockProbes(FX), prev=None))
    r = FakeRunner()
    acts = Actions(r, lambda: ["neptr"], tmp_path / "web-actions.log")
    static = pathlib.Path(__file__).resolve().parents[1] / "static"
    srv = make_server(app, 0, static, actions=acts)
    t = threading.Thread(target=srv.serve_forever, daemon=True)
    t.start()
    try:
        c = http.client.HTTPConnection("127.0.0.1", srv.server_address[1], timeout=3)
        c.request("POST", "/api/patch", json.dumps({"name": "nope"}),
                  {"Content-Type": "application/json"})
        resp = c.getresponse()
        assert resp.status == 400
        resp.read()
        assert r.calls == []
        c.request("POST", "/api/patch", "not json{",
                  {"Content-Type": "application/json"})
        resp = c.getresponse()
        assert resp.status == 400
        resp.read()
        c.request("POST", "/api/patch", json.dumps({"name": "neptr"}),
                  {"Content-Type": "application/json"})
        resp = c.getresponse()
        assert resp.status == 200
        assert json.loads(resp.read())["ok"] is True
        assert r.calls == [["demiurge-set", "neptr"]]
    finally:
        srv.shutdown()


# ───────────── engine settings (demiurge-audio is the one write path) ─────

def test_audio_set_valid_runs_demiurge_audio(tmp_path):
    r = FakeRunner()
    status, res = _actions(tmp_path, r).audio_set("quantum", "256")
    assert status == 200 and res["ok"] is True
    assert r.calls == [["demiurge-audio", "quantum", "256"]]


def test_audio_set_accepts_float_bits(tmp_path):
    r = FakeRunner()
    status, _ = _actions(tmp_path, r).audio_set("bits", "float")
    assert status == 200
    assert r.calls == [["demiurge-audio", "bits", "float"]]


def test_audio_set_unknown_key_rejected(tmp_path):
    r = FakeRunner()
    status, res = _actions(tmp_path, r).audio_set("ksmps", "64")
    assert status == 400 and res["ok"] is False
    assert r.calls == []


def test_audio_set_shell_junk_rejected(tmp_path):
    r = FakeRunner()
    for bad in ("256; rm -rf /", "", "-3", None, "a" * 20):
        status, _ = _actions(tmp_path, r).audio_set("quantum", bad)
        assert status == 400
    assert r.calls == []


def test_audio_set_24_bits_reaches_cli_which_rejects_it(tmp_path):
    # "24" is argv-clean, so the server forwards it; refusing 24-bit is the
    # CLI's job (it owns the legal set) — exactly one place knows the rule.
    r = FakeRunner(rc=2, out="bits must be one of 16,32,float")
    status, res = _actions(tmp_path, r).audio_set("bits", "24")
    assert r.calls == [["demiurge-audio", "bits", "24"]]
    assert status == 500 and res["ok"] is False


# ───────────── wifi ─────────────

def test_wifi_join_saved_network_has_no_password(tmp_path):
    r = FakeRunner()
    status, _ = _actions(tmp_path, r).wifi_join("GOR_CAVE", None)
    assert status == 200
    assert r.calls == [["demiurge-wifi", "join", "GOR_CAVE"]]
    assert r.stdins == [None]


def test_wifi_join_password_goes_on_stdin_never_argv(tmp_path):
    r = FakeRunner()
    secret = "hunter2hunter2"
    status, _ = _actions(tmp_path, r).wifi_join("CoffeeShop", secret)
    assert status == 200
    assert r.calls == [["demiurge-wifi", "join", "CoffeeShop",
                        "--password-stdin"]]
    # the secret is on stdin ...
    assert r.stdins == [secret + "\n"]
    # ... and in no argument of the command line
    assert not any(secret in arg for arg in r.calls[0])


def test_wifi_join_password_never_reaches_the_action_log(tmp_path):
    r = FakeRunner()
    secret = "correct horse battery"
    _actions(tmp_path, r).wifi_join("CoffeeShop", secret)
    log = (tmp_path / "logs" / "web-actions.log").read_text()
    assert secret not in log
    assert "demiurge-wifi join CoffeeShop --password-stdin" in log


def test_wifi_join_short_password_rejected_without_echoing_it(tmp_path):
    r = FakeRunner()
    status, res = _actions(tmp_path, r).wifi_join("CoffeeShop", "short")
    assert status == 400 and r.calls == []
    assert "short" not in res["output"]
    log = (tmp_path / "logs" / "web-actions.log").read_text()
    assert "REJECTED" in log


def test_wifi_bad_ssid_rejected(tmp_path):
    r = FakeRunner()
    for bad in ("", "   ", None, 42, "x" * 33):
        status, _ = _actions(tmp_path, r).wifi_join(bad, None)
        assert status == 400
        status, _ = _actions(tmp_path, r).wifi_forget(bad)
        assert status == 400
    assert r.calls == []


def test_wifi_forget_and_scan(tmp_path):
    r = FakeRunner()
    a = _actions(tmp_path, r)
    a.wifi_forget("OldNet")
    a.wifi_scan()
    assert r.calls == [["demiurge-wifi", "forget", "OldNet"],
                       ["demiurge-wifi", "--scan"]]


# ───────────────────────── graph actions ─────────────────────────
#
# ONE write path: nothing here may ever compose a pw-link command. The CLI
# owns the graph exactly as demiurge-audio owns live.conf.

def test_graph_connect_and_disconnect_shell_out_to_the_cli(tmp_path):
    r = FakeRunner()
    a = _actions(tmp_path, r)
    assert a.graph_connect("alsa_input.plat:capture_FL", "csound6:input1")[0] == 200
    assert a.graph_disconnect("csound6:output1", "alsa_output.plat:playback_FL")[0] == 200
    assert r.calls == [
        ["demiurge-graph", "connect",
         "alsa_input.plat:capture_FL", "csound6:input1"],
        ["demiurge-graph", "disconnect",
         "csound6:output1", "alsa_output.plat:playback_FL"]]


def test_graph_reset_is_one_command_and_never_restarts_anything(tmp_path):
    r = FakeRunner()
    assert _actions(tmp_path, r).graph_reset()[0] == 200
    assert r.calls == [["demiurge-graph", "reset"]]
    assert not any("systemctl" in c for c in r.calls[0])


def test_graph_never_shells_out_to_pw_link(tmp_path):
    r = FakeRunner()
    a = _actions(tmp_path, r)
    a.graph_connect("a:out", "b:in")
    a.graph_disconnect("a:out", "b:in")
    a.graph_reset()
    assert all(c[0] == "demiurge-graph" for c in r.calls)


def test_graph_bad_port_names_rejected_runner_not_called(tmp_path):
    r = FakeRunner()
    a = _actions(tmp_path, r)
    for bad in ("", "nocolon", None, 42, "a:b; rm -rf /", "a:b`x`", "x" * 300):
        assert a.graph_connect(bad, "b:in")[0] == 400
        assert a.graph_disconnect("a:out", bad)[0] == 400
    assert r.calls == []


def test_collect_includes_the_graph(tmp_path):
    from demiurge_web import MockProbes, collect
    f = collect(MockProbes(FX), prev=None)
    assert f["graph"]["mode"] == "sync"
    assert f["graph"]["out_path"] == "direct"


def test_graph_probe_failure_yields_null_not_a_crash(tmp_path):
    from demiurge_web import MockProbes, collect
    p = MockProbes(FX); p.broken = {"demiurge-graph --list"}
    assert collect(p, prev=None)["graph"] is None


# ───────────────────────── NEPTR sensors (demiurge-io contract) ─────────────

def _sensor_source(**over):
    s = {"id": "tof1", "label": "LiDAR 1", "units": "mm", "in_lo": 20,
         "in_hi": 460, "dropout": "hold", "deadzone": 0.02, "smooth_ms": 40,
         "enabled": True}
    s.update(over)
    return s


def _sensor_map(**over):
    m = {"id": "m1", "source": "tof1", "enabled": True, "zone_lo": 0.0,
         "zone_hi": 0.5, "outside": "hold", "invert": False, "curve": "lin",
         "out_lo": 0.0, "out_hi": 1.0, "mode": "continuous",
         "combine": "replace", "slew_ms": 30,
         "target": {"sink": "csound", "kind": "expr", "id": 30}}
    m.update(over)
    return m


def _start_server_with_sensors(tmp_path):
    from demiurge_web import App, MockProbes, collect, make_server, SensorLink
    app = App()
    app.publish(collect(MockProbes(FX), prev=None))
    static = pathlib.Path(__file__).resolve().parents[1] / "static"
    sensors = SensorLink(config_path=tmp_path / "sensors.json",
                         expr_targets_path=FX / "expr_targets.json")
    srv = make_server(app, 0, static, sensors=sensors)
    srv.app = app             # exposed so tests can drive the SSE channels
                              # directly, same as _start_server()'s bare `app`
    t = threading.Thread(target=srv.serve_forever, daemon=True)
    t.start()
    return srv, srv.server_address[1], sensors


def test_get_sensors_empty_when_no_config(tmp_path):
    srv, port, sensors = _start_server_with_sensors(tmp_path)
    try:
        c = http.client.HTTPConnection("127.0.0.1", port, timeout=3)
        c.request("GET", "/api/neptr/sensors")
        r = c.getresponse()
        assert r.status == 200
        body = json.loads(r.read())
        assert body == {"sources": [], "maps": [], "live": {}}
    finally:
        srv.shutdown()


def test_get_expr_targets_from_fixture(tmp_path):
    srv, port, sensors = _start_server_with_sensors(tmp_path)
    try:
        c = http.client.HTTPConnection("127.0.0.1", port, timeout=3)
        c.request("GET", "/api/neptr/expr-targets")
        r = c.getresponse()
        assert r.status == 200
        targets = json.loads(r.read())
        assert len(targets) == 4
        assert {t["id"] for t in targets} == {30, 1, 21, 22}
    finally:
        srv.shutdown()


def test_put_sensors_valid_config_written_atomically(tmp_path):
    srv, port, sensors = _start_server_with_sensors(tmp_path)
    try:
        cfg = {"sources": [_sensor_source()], "maps": [_sensor_map()]}
        c = http.client.HTTPConnection("127.0.0.1", port, timeout=3)
        c.request("PUT", "/api/neptr/sensors", json.dumps(cfg),
                  {"Content-Type": "application/json"})
        r = c.getresponse()
        assert r.status == 200
        assert json.loads(r.read())["ok"] is True
        on_disk = json.loads((tmp_path / "sensors.json").read_text())
        assert on_disk["sources"][0]["id"] == "tof1"
        assert list(tmp_path.glob("*.tmp")) == []

        c.request("GET", "/api/neptr/sensors")
        r = c.getresponse()
        body = json.loads(r.read())
        assert body["sources"][0]["id"] == "tof1"
        assert body["maps"][0]["id"] == "m1"
    finally:
        srv.shutdown()


def test_put_sensors_rejects_malformed_config_does_not_write(tmp_path):
    srv, port, sensors = _start_server_with_sensors(tmp_path)
    try:
        c = http.client.HTTPConnection("127.0.0.1", port, timeout=3)
        c.request("PUT", "/api/neptr/sensors",
                  json.dumps({"sources": [_sensor_source(dropout="bogus")],
                             "maps": []}),
                  {"Content-Type": "application/json"})
        r = c.getresponse()
        assert r.status == 400
        body = json.loads(r.read())
        assert body["ok"] is False and "dropout" in body["error"]
        assert not (tmp_path / "sensors.json").exists()
    finally:
        srv.shutdown()


def test_put_sensors_rejects_unknown_expr_target_id(tmp_path):
    srv, port, sensors = _start_server_with_sensors(tmp_path)
    try:
        cfg = {"sources": [_sensor_source()],
              "maps": [_sensor_map(target={"sink": "csound", "kind": "expr",
                                           "id": 99999})]}
        c = http.client.HTTPConnection("127.0.0.1", port, timeout=3)
        c.request("PUT", "/api/neptr/sensors", json.dumps(cfg),
                  {"Content-Type": "application/json"})
        r = c.getresponse()
        assert r.status == 400
        assert not (tmp_path / "sensors.json").exists()
    finally:
        srv.shutdown()


def test_put_sensors_bad_json_400(tmp_path):
    srv, port, sensors = _start_server_with_sensors(tmp_path)
    try:
        c = http.client.HTTPConnection("127.0.0.1", port, timeout=3)
        c.request("PUT", "/api/neptr/sensors", "not json{",
                  {"Content-Type": "application/json"})
        r = c.getresponse()
        assert r.status == 400
    finally:
        srv.shutdown()


def test_put_sensors_without_sensors_link_503():
    from demiurge_web import App, MockProbes, collect, make_server
    app = App()
    app.publish(collect(MockProbes(FX), prev=None))
    static = pathlib.Path(__file__).resolve().parents[1] / "static"
    srv = make_server(app, 0, static)
    t = threading.Thread(target=srv.serve_forever, daemon=True)
    t.start()
    try:
        c = http.client.HTTPConnection("127.0.0.1", srv.server_address[1], timeout=3)
        c.request("PUT", "/api/neptr/sensors", json.dumps({"sources": [], "maps": []}),
                  {"Content-Type": "application/json"})
        r = c.getresponse()
        assert r.status == 503
    finally:
        srv.shutdown()


# ─────────────────── NEPTR sensors live push (replaces 5 Hz poll) ──────────

def test_sensor_events_stream_headers_and_initial_state(tmp_path):
    # A screen opened mid-sweep should see the CURRENT value on connect, not
    # a blank meter until the next OSC packet — that is the whole point of
    # sending a snapshot before the loop, mirroring /api/neptr/events.
    srv, port, sensors = _start_server_with_sensors(tmp_path)
    try:
        sensors.state.set("tof1", 0.77)
        s = socket.create_connection(("127.0.0.1", port), timeout=3)
        s.sendall(b"GET /api/neptr/sensors/events HTTP/1.1\r\nHost: x\r\n\r\n")
        s.settimeout(3)
        buf = b""
        while b"\r\n\r\n" not in buf:
            buf += s.recv(4096)
        head, _, rest = buf.partition(b"\r\n\r\n")
        assert b"200" in head.split(b"\r\n", 1)[0]
        assert b"text/event-stream" in head
        while b"data: " not in rest:
            rest += s.recv(4096)
        assert b'"tof1": 0.77' in rest or b'"tof1":0.77' in rest
        s.close()
    finally:
        srv.shutdown()


def test_sensor_events_stream_pushes_updates(tmp_path):
    # The HTTP handler only relays app.publish_sensors() onto the socket;
    # the OSC-arrival -> coalesce -> publish_sensors wiring lives in main()
    # (mirrors on_engine_change), so this drives the app channel directly,
    # same as test_sse_pushes_frame does for the system-frame channel.
    srv, port, sensors = _start_server_with_sensors(tmp_path)
    try:
        s = socket.create_connection(("127.0.0.1", port), timeout=3)
        s.sendall(b"GET /api/neptr/sensors/events HTTP/1.1\r\nHost: x\r\n\r\n")
        s.settimeout(3)
        buf = b""
        while b"data: " not in buf:      # consume the initial empty snapshot
            buf += s.recv(4096)
        time.sleep(0.2)
        srv.app.publish_sensors({"exp": 0.2})
        buf = b""
        while b'"exp"' not in buf:
            buf += s.recv(4096)
        assert b'"exp": 0.2' in buf or b'"exp":0.2' in buf
        s.close()
    finally:
        srv.shutdown()


def test_sensor_push_channel_coalesces_through_the_app():
    from demiurge_web import App
    app = App()
    q = app.subscribe_sensors()
    app.publish_sensors({"tof1": 0.5})
    assert q.get(timeout=1) == {"tof1": 0.5}
    app.unsubscribe_sensors(q)
    app.publish_sensors({"tof1": 0.9})
    assert q.empty()


# ───────────────────────── virtual hardware panel (NEPTR sim) ─────────────

def test_osc_encode_exact_bytes_for_a_known_cc_message():
    from demiurge_web import osc_encode
    # "/cc" ch=1 cc=20 val=127 — an encoder-1 clockwise click. Address "/cc\0"
    # is already 4 bytes (no extra padding); type-tag ",iii\0" is 5 bytes,
    # padded to 8; three big-endian int32 args follow.
    msg = osc_encode("/cc", 1, 20, 127)
    assert msg == (b"/cc\0" + b",iii\0\0\0\0"
                   + (1).to_bytes(4, "big") + (20).to_bytes(4, "big")
                   + (127).to_bytes(4, "big"))
    assert len(msg) == 24 and len(msg) % 4 == 0


def test_osc_encode_pads_a_longer_address():
    from demiurge_web import osc_encode
    # "/menu/x" -> 8 bytes with the null (still a 4-byte multiple by luck);
    # use an address that actually needs padding to prove the math.
    msg = osc_encode("/ab", 5)
    assert msg[:4] == b"/ab\0"
    assert msg[4:8] == b",i\0\0"
    assert msg[8:12] == (5).to_bytes(4, "big")
    assert len(msg) == 12


def test_sim_out_sends_the_exact_osc_cc_datagram():
    from demiurge_web import SimOut, osc_encode
    listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    listener.bind(("127.0.0.1", 0))
    listener.settimeout(3)
    port = listener.getsockname()[1]
    try:
        sim = SimOut(cc_host="127.0.0.1", cc_port=port)
        assert sim.cc(1, 20, 127) is True
        data, _ = listener.recvfrom(4096)
        assert data == osc_encode("/cc", 1, 20, 127)
    finally:
        listener.close()


def test_sim_out_sends_the_plain_text_sensor_line():
    from demiurge_web import SimOut
    listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    listener.bind(("127.0.0.1", 0))
    listener.settimeout(3)
    port = listener.getsockname()[1]
    try:
        sim = SimOut(sensor_host="127.0.0.1", sensor_port=port)
        assert sim.sensor("tof1", 137) is True
        data, _ = listener.recvfrom(4096)
        assert data == b"tof1 137\n"
    finally:
        listener.close()


class FakeSim:
    def __init__(self, ok=True):
        self.cc_calls = []
        self.sensor_calls = []
        self.ok = ok

    def cc(self, ch, cc, val):
        self.cc_calls.append((ch, cc, val))
        return self.ok

    def sensor(self, source_id, raw):
        self.sensor_calls.append((source_id, raw))
        return self.ok


def _actions_with_sim(tmp_path, sim, runner=None):
    from demiurge_web import Actions
    return Actions(runner or FakeRunner(), lambda: ["neptr"],
                   tmp_path / "logs" / "web-actions.log", sim=sim)


def test_neptr_sim_cc_sends_the_physical_encoder_cc(tmp_path):
    sim = FakeSim()
    status, res = _actions_with_sim(tmp_path, sim).neptr_sim_cc(1, 20, 127)
    assert status == 200 and res["ok"] is True
    assert sim.cc_calls == [(1, 20, 127)]


def test_neptr_sim_cc_rejects_out_of_range(tmp_path):
    sim = FakeSim()
    a = _actions_with_sim(tmp_path, sim)
    for ch, cc, val in [(0, 20, 127), (17, 20, 127), (1, 128, 0), (1, 20, 128),
                        (1, -1, 0), (1, 20, -1)]:
        status, res = a.neptr_sim_cc(ch, cc, val)
        assert status == 400 and res["ok"] is False
    assert sim.cc_calls == []


def test_neptr_sim_cc_rejects_non_int_and_bool(tmp_path):
    sim = FakeSim()
    a = _actions_with_sim(tmp_path, sim)
    for ch, cc, val in [(1.5, 20, 127), (1, "20", 127), (True, 20, 127)]:
        assert a.neptr_sim_cc(ch, cc, val)[0] == 400
    assert sim.cc_calls == []


def test_neptr_sim_cc_without_sim_unavailable(tmp_path):
    from demiurge_web import Actions
    a = Actions(FakeRunner(), lambda: ["neptr"],
               tmp_path / "logs" / "web-actions.log")
    status, res = a.neptr_sim_cc(1, 20, 127)
    assert status == 400 and "unavailable" in res["output"]


def test_neptr_sim_cc_send_failure_returns_500(tmp_path):
    sim = FakeSim(ok=False)
    status, res = _actions_with_sim(tmp_path, sim).neptr_sim_cc(1, 60, 127)
    assert status == 500 and res["ok"] is False


def test_neptr_sim_sensor_sends_the_raw_reading(tmp_path):
    sim = FakeSim()
    status, res = _actions_with_sim(tmp_path, sim).neptr_sim_sensor("tof1", 137)
    assert status == 200 and res["ok"] is True
    assert sim.sensor_calls == [("tof1", 137)]


def test_neptr_sim_sensor_dropout_is_minus_one_and_allowed(tmp_path):
    sim = FakeSim()
    status, res = _actions_with_sim(tmp_path, sim).neptr_sim_sensor("tof1", -1)
    assert status == 200 and res["ok"] is True
    assert sim.sensor_calls == [("tof1", -1)]


def test_neptr_sim_sensor_rejects_unknown_id(tmp_path):
    sim = FakeSim()
    status, res = _actions_with_sim(tmp_path, sim).neptr_sim_sensor("gyro_x", 0.4)
    assert status == 400 and res["ok"] is False
    assert sim.sensor_calls == []


def test_neptr_sim_sensor_rejects_bad_raw_value(tmp_path):
    sim = FakeSim()
    a = _actions_with_sim(tmp_path, sim)
    for bad in ("137", None, True, [1, 2]):
        status, res = a.neptr_sim_sensor("tof1", bad)
        assert status == 400 and res["ok"] is False
    assert sim.sensor_calls == []


def test_post_sim_cc_endpoint(tmp_path):
    from demiurge_web import App, MockProbes, collect, make_server, Actions
    app = App()
    app.publish(collect(MockProbes(FX), prev=None))
    sim = FakeSim()
    acts = Actions(FakeRunner(), lambda: ["neptr"],
                   tmp_path / "web-actions.log", sim=sim)
    static = pathlib.Path(__file__).resolve().parents[1] / "static"
    srv = make_server(app, 0, static, actions=acts)
    t = threading.Thread(target=srv.serve_forever, daemon=True)
    t.start()
    try:
        c = http.client.HTTPConnection("127.0.0.1", srv.server_address[1], timeout=3)
        c.request("POST", "/api/neptr/sim/cc",
                  json.dumps({"ch": 1, "cc": 26, "val": 0}),
                  {"Content-Type": "application/json"})
        r = c.getresponse()
        assert r.status == 200
        assert json.loads(r.read())["ok"] is True
        assert sim.cc_calls == [(1, 26, 0)]

        c.request("POST", "/api/neptr/sim/cc",
                  json.dumps({"ch": 1, "cc": 999, "val": 0}),
                  {"Content-Type": "application/json"})
        r = c.getresponse()
        assert r.status == 400
        r.read()
    finally:
        srv.shutdown()


def test_post_sim_sensor_endpoint(tmp_path):
    from demiurge_web import App, MockProbes, collect, make_server, Actions
    app = App()
    app.publish(collect(MockProbes(FX), prev=None))
    sim = FakeSim()
    acts = Actions(FakeRunner(), lambda: ["neptr"],
                   tmp_path / "web-actions.log", sim=sim)
    static = pathlib.Path(__file__).resolve().parents[1] / "static"
    srv = make_server(app, 0, static, actions=acts)
    t = threading.Thread(target=srv.serve_forever, daemon=True)
    t.start()
    try:
        c = http.client.HTTPConnection("127.0.0.1", srv.server_address[1], timeout=3)
        c.request("POST", "/api/neptr/sim/sensor",
                  json.dumps({"id": "exp", "raw": 2400}),
                  {"Content-Type": "application/json"})
        r = c.getresponse()
        assert r.status == 200
        assert json.loads(r.read())["ok"] is True
        assert sim.sensor_calls == [("exp", 2400)]

        c.request("POST", "/api/neptr/sim/sensor",
                  json.dumps({"id": "not_a_sensor", "raw": 1}),
                  {"Content-Type": "application/json"})
        r = c.getresponse()
        assert r.status == 400
        r.read()
    finally:
        srv.shutdown()


def test_sensor_in_updates_live_state_via_udp():
    import struct
    from demiurge_web import SensorState, SensorIn
    state = SensorState()
    listener = SensorIn(state, port=0)
    listener.sock = listener._bind()
    listener.status = "live"
    t = threading.Thread(target=listener._loop, daemon=True)
    t.start()
    port = listener.sock.getsockname()[1]
    try:
        def osc_str(s):
            b = s.encode("ascii") + b"\0"
            return b + b"\0" * (-len(b) % 4)
        msg = osc_str("/sensor") + osc_str(",sf") + osc_str("exp") \
            + struct.pack(">f", 0.66)
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.sendto(msg, ("127.0.0.1", port))
        deadline = time.time() + 3
        while "exp" not in state.snapshot() and time.time() < deadline:
            time.sleep(0.05)
        assert abs(state.snapshot()["exp"] - 0.66) < 1e-5
    finally:
        listener.stop()
