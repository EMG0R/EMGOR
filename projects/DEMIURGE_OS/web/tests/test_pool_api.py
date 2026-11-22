"""/api/pool/* tests: temp DEMIURGE root, fake pool UDP listener on 17xxx.
No live ports (never 8080/9102), no audio."""
import sys, json, pathlib, socket, threading, time, http.client, struct
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
FX = pathlib.Path(__file__).parent / "fixtures"
import pytest

demiurge_web = pytest.importorskip("demiurge_web")
if demiurge_web._dio_pool is None:
    pytest.skip("demiurge_io not importable", allow_module_level=True)


def _decode(d):
    def s(i):
        j = d.index(b"\0", i)
        return d[i:j].decode(), (j + 4) & ~3
    a, i = s(0); t, i = s(i)
    args = []
    for c in t[1:]:
        if c == "s":
            v, i = s(i); args.append(v)
        else:
            args.append(struct.unpack(">f", d[i:i + 4])[0]); i += 4
    return a, args


class FakePool:
    def __init__(self, port):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", port)); self.sock.settimeout(2)
        self.port = port

    def recv(self):
        return _decode(self.sock.recvfrom(2048)[0])


def _manifest(root, hard=False):
    (root / "params").mkdir(parents=True)
    (root / "params" / "nld-test.json").write_text(json.dumps({
        "stage": "nld-test", "port": 17900, "version": 1,
        "params": [
            {"path": "/nld-test/density", "label": "Density", "default": 0.35,
             "proto": "p", "map": "hard"},
            {"path": "/nld-test/tone", "label": "Tone", "default": 0.5,
             "proto": "p"}]}))


@pytest.fixture
def rig(tmp_path):
    root = tmp_path / "demiurge"
    _manifest(root)
    (root / "live.conf").write_text("# set: testpatch\nchain =\n")
    (root / "patches").mkdir()
    (root / "sensors.json").write_text(json.dumps(
        {"sources": [{"id": "tof1", "label": "ToF 1"}], "maps": []}))
    fp = FakePool(17101)
    sensors = demiurge_web.SensorLink(config_path=root / "sensors.json",
                                      expr_targets_path=FX / "expr_targets.json",
                                      port=17102)
    pool = demiurge_web.PoolLink(root=root, expr_targets_path=FX / "expr_targets.json",
                                 port=17101, sensors=sensors)
    sensors.inp.on_param = pool.note
    sensors.start()
    app = demiurge_web.App()
    srv = demiurge_web.make_server(app, 0, pathlib.Path(__file__).resolve().parents[1] / "static",
                                   sensors=sensors, pool=pool)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    yield root, fp, pool, srv.server_address[1]
    srv.shutdown(); fp.sock.close()


def call(port, method, path, body=None):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=3)
    c.request(method, path, json.dumps(body) if body is not None else None,
              {"Content-Type": "application/json"})
    r = c.getresponse()
    return r.status, json.loads(r.read() or b"{}")


def test_params_merged_with_hard_soft_and_csound(rig):
    root, fp, pool, port = rig
    st, d = call(port, "GET", "/api/pool/params")
    assert st == 200
    by = {p["path"]: p for p in d["params"]}
    assert by["/nld-test/density"]["map"] == "hard"
    assert by["/nld-test/tone"]["map"] == "soft"
    assert by["/nld-test/tone"]["default"] == 0.5 and by["/nld-test/tone"]["value"] == 0.5
    assert any(p.startswith("/csound/") for p in by)       # expr targets merged


def test_sources_config_plus_seen(rig):
    root, fp, pool, port = rig
    pool.sensors.state.set("newone", 0.4)
    st, d = call(port, "GET", "/api/pool/sources")
    ids = {s["id"]: s for s in d["sources"]}
    assert ids["tof1"]["configured"] and not ids["newone"]["configured"]
    assert ids["newone"]["live"] == 0.4


def test_set_sends_p_to_pool_and_updates_value(rig):
    root, fp, pool, port = rig
    st, d = call(port, "POST", "/api/pool/set", {"path": "/nld-test/tone", "value": 1.7})
    assert st == 200 and d["value"] == 1.0
    addr, args = fp.recv()
    assert addr == "/p" and args[0] == "/nld-test/tone" and args[1] == pytest.approx(1.0)
    by = {x["path"]: x for x in call(port, "GET", "/api/pool/params")[1]["params"]}
    assert by["/nld-test/tone"]["value"] == 1.0
    assert call(port, "POST", "/api/pool/set", {"path": "x"})[0] == 400


def test_events_stream_relays_param_echo(rig):
    root, fp, pool, port = rig
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=4)
    c.request("GET", "/api/pool/events")
    r = c.getresponse()
    assert r.status == 200 and r.getheader("Content-Type") == "text/event-stream"
    assert r.fp.readline().startswith(b": open")
    r.fp.readline()
    # the pool's telemetry: OSC /param <path> <val> to the sensor/telemetry port
    time.sleep(0.2)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(demiurge_web.osc_encode("/param", "/nld-test/tone", 0.25),
                ("127.0.0.1", 17102))
    line = r.fp.readline()
    assert line.startswith(b"data: ")
    ev = json.loads(line[6:])
    assert ev["path"] == "/nld-test/tone" and ev["value"] == pytest.approx(0.25)
    c.close()


def _map(**kw):
    m = {"source": "tof1", "param": "/nld-test/tone", "zone_lo": 0.1,
         "zone_hi": 0.9, "invert": True, "curve": "lin", "mode": "continuous"}
    m.update(kw)
    return m


def test_map_write_soft_atomic_and_upsert(rig):
    root, fp, pool, port = rig
    st, d = call(port, "POST", "/api/pool/map", _map())
    assert st == 200 and d["patch"] == "testpatch"
    f = root / "patches" / "testpatch.map.json"
    doc = json.loads(f.read_text())
    assert doc["maps"][0]["target"] == {"param": "/nld-test/tone"}
    assert doc["maps"][0]["invert"] is True and doc["maps"][0]["zone_lo"] == 0.1
    assert not list((root / "patches").glob("*.tmp"))
    call(port, "POST", "/api/pool/map", _map(zone_hi=0.8))      # upsert, not append
    doc = json.loads(f.read_text())
    assert len(doc["maps"]) == 1 and doc["maps"][0]["zone_hi"] == 0.8
    st, got = call(port, "GET", "/api/pool/map")
    assert st == 200 and got["maps"][0]["zone_hi"] == 0.8


def test_map_hard_refused_409_and_nothing_written(rig):
    root, fp, pool, port = rig
    st, d = call(port, "POST", "/api/pool/map", _map(param="/nld-test/density"))
    assert st == 409 and "hard" in d["error"]
    assert not (root / "patches" / "testpatch.map.json").exists()
    st, d = call(port, "POST", "/api/pool/map",
                 {"maps": [_map(), _map(param="/nld-test/density")]})   # whole-doc form
    assert st == 409 and not (root / "patches" / "testpatch.map.json").exists()


def test_map_validation(rig):
    root, fp, pool, port = rig
    assert call(port, "POST", "/api/pool/map", _map(param="/nope/x"))[0] == 400
    assert call(port, "POST", "/api/pool/map", _map(zone_lo=0.9, zone_hi=0.1))[0] == 400
    assert call(port, "POST", "/api/pool/map", _map(curve="zig"))[0] == 400
    assert call(port, "POST", "/api/pool/map", {"param": "/nld-test/tone"})[0] == 400


def test_map_no_active_patch(rig):
    root, fp, pool, port = rig
    (root / "live.conf").write_text("chain =\n")
    assert call(port, "POST", "/api/pool/map", _map())[0] == 400
    assert call(port, "GET", "/api/pool/map")[1]["maps"] == []


def test_examples_route_contained(rig):
    root, fp, pool, port = rig
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=3)
    c.request("GET", "/examples/nonlinear_daylight.html")
    r = c.getresponse(); body = r.read()
    assert r.status == 200 and b"nonlinear" in body.lower()
    c.request("GET", "/examples/../demiurge_web.py")
    r = c.getresponse(); r.read()
    assert r.status == 404
