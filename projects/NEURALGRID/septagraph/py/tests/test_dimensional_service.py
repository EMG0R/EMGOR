"""HTTP surface of the dimensional service, driven against a real server."""
import json
import threading
import urllib.request
from http.server import ThreadingHTTPServer

import pytest

from septagraph.dimensional.app import MODE_CALIBRATE, MODE_RUN, DimensionalApp
from septagraph.dimensional.service import make_handler


@pytest.fixture
def server(tmp_path):
    app = DimensionalApp(calibration_path=tmp_path / "calibration.json")
    httpd = ThreadingHTTPServer(("127.0.0.1", 0), make_handler(app, None))
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    base = f"http://127.0.0.1:{httpd.server_address[1]}"
    yield app, base
    httpd.shutdown()
    httpd.server_close()


def get(base, path):
    with urllib.request.urlopen(base + path, timeout=5) as r:
        return r.status, r.read()


def post(base, path, body=None):
    req = urllib.request.Request(
        base + path,
        data=json.dumps(body or {}).encode(),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=5) as r:
        return json.loads(r.read())


class TestState:
    def test_serves_the_app_state_as_json(self, server):
        _, base = server
        status, body = get(base, "/api/state")
        assert status == 200
        assert json.loads(body)["mode"] == MODE_CALIBRATE

    def test_reports_the_link_even_with_no_hardware(self, server):
        _, base = server
        assert json.loads(get(base, "/api/state")[1])["link"]["connected"] is False

    def test_serves_the_visualization_at_the_root(self, server):
        _, base = server
        status, body = get(base, "/")
        assert status == 200 and b"viz.js" in body

    def test_serves_the_calibration_ui_at_setup(self, server):
        _, base = server
        status, body = get(base, "/setup")
        assert status == 200 and b"dimensional" in body

    def test_serves_the_visualization_assets(self, server):
        _, base = server
        assert get(base, "/static/viz.js")[0] == 200
        assert get(base, "/static/viz.css")[0] == 200

    def test_serves_its_static_assets(self, server):
        _, base = server
        assert get(base, "/static/app.js")[0] == 200
        assert get(base, "/static/app.css")[0] == 200

    def test_unknown_paths_are_404_not_a_traceback(self, server):
        _, base = server
        with pytest.raises(urllib.error.HTTPError) as e:
            get(base, "/nope")
        assert e.value.code == 404


class TestCalibrationRoutes:
    def test_next_advances_a_step(self, server):
        app, base = server
        post(base, "/api/calibrate/next")
        assert app.calibration.current.key == "softpot_top"

    def test_next_past_the_last_step_hands_off_to_the_encoder(self, server):
        app, base = server
        for _ in range(len(app.calibration.steps)):
            post(base, "/api/calibrate/next")
        assert app.mode == MODE_RUN

    def test_finish_jumps_straight_to_the_encoder(self, server):
        app, base = server
        assert post(base, "/api/calibrate/finish")["mode"] == MODE_RUN

    def test_redo_returns_to_a_named_step(self, server):
        app, base = server
        post(base, "/api/calibrate/next")
        post(base, "/api/calibrate/next")
        post(base, "/api/calibrate/redo", {"channel": "softpot_top"})
        assert app.calibration.current.key == "softpot_top"

    def test_start_relaunches_calibration_from_run(self, server):
        app, base = server
        post(base, "/api/calibrate/finish")
        assert post(base, "/api/calibrate/start")["mode"] == MODE_CALIBRATE

    def test_a_malformed_body_does_not_crash_the_server(self, server):
        app, base = server
        req = urllib.request.Request(
            base + "/api/calibrate/redo", data=b"not json",
            headers={"Content-Type": "application/json"}, method="POST",
        )
        with urllib.request.urlopen(req, timeout=5) as r:
            assert r.status == 200
        assert get(base, "/api/state")[0] == 200


def _settle_rest(app):
    """The pad program starts by learning the housing's resting load."""
    for _ in range(10):
        app.on_analog((0, 0, 0, 0, 3300, 1400, 0, 0))


class TestFsrRoutes:
    def test_start_enters_the_pad_program(self, server):
        app, base = server
        assert post(base, "/api/fsr/start")["mode"] == "fsr"

    def test_skip_moves_to_the_next_pad(self, server):
        app, base = server
        post(base, "/api/fsr/start")
        _settle_rest(app)
        post(base, "/api/fsr/skip")
        assert app.fsr.current_pad == 1

    def test_goto_returns_to_a_pad(self, server):
        app, base = server
        post(base, "/api/fsr/start")
        _settle_rest(app)
        post(base, "/api/fsr/skip")
        post(base, "/api/fsr/goto", {"pad": 0})
        assert app.fsr.current_pad == 0

    def test_finish_hands_off_to_the_encoder(self, server):
        app, base = server
        post(base, "/api/fsr/start")
        assert post(base, "/api/fsr/finish")["mode"] == "run"

    def test_state_carries_the_pad_to_hit(self, server):
        app, base = server
        post(base, "/api/fsr/start")
        _settle_rest(app)
        import json as _j, urllib.request
        st = _j.loads(get(base, "/api/state")[1])
        assert st["fsr"]["pad"] == 0
