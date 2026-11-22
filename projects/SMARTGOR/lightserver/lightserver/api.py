"""FastAPI control surface for the engine(s). Binds 0.0.0.0:8800.

POST /prompt  {"text": "...", "node": "name"?}   NL -> scene -> engine(s)
              ("vibe" is accepted as an alias for "text")
POST /scene   {raw scene JSON}  (?node=name)     load a scene directly
POST /pixels  {"pixels": {"5":"#ff0000"}, "base": "black"|"current",
               "node": "name"?}                  per-pixel; DEFAULT NODE ONLY
                                                 unless node given (indices
                                                 are node-specific)
GET  /time                                        {"unix": time.time(), "monotonic":
                                                 time.monotonic()} -- no auth, trivially
                                                 cheap; poll repeatedly for min-RTT clock-
                                                 offset estimation (see docs/TAPS_DESIGN.md)
GET  /scene   (?node=name)                       currently-running scene JSON, verbatim
GET  /state   (?node=name)                       scene + fps + streaming stats
GET  /nodes                                      node registry + live status
POST /stop    (?node=name)                       stop streaming
POST /resume  (?node=name)                       resume streaming
GET  /scenes                                     list bundled example scenes
GET  /satellites                                 satellite "dot" nodes: anchor/mode/boost/
                                                 vivid_saturation/vivid_lift/effective_gain/online
POST /satellites/{name} {"anchor"?, "mode"?,
                        "color"?, "boost"?,
                        "vivid_saturation"?,
                        "vivid_lift"?}            reposition/switch mode/set independent color/
                                                 set brightness boost ("auto"|"vivid"|number|
                                                 "off")/set the "vivid" saturation floor
                                                 (0.0..1.0, independently settable)/set the
                                                 "vivid" lift exponent (0.3..1.5, default 0.7,
                                                 independently settable)
GET  /taps                                       list canvas taps (see taps.py, docs/TAPS_DESIGN.md)
POST /taps/{name}    {"port": int (required on   create/keepalive a self-expiring UDP subscription
                      first create), "fps_divisor"?,
                      "ttl"?, "offset"?, "count"?,
                      "semantic_port"?,
                      "enabled"?}                streaming ring[offset:offset+count] to the caller's
                                                 own source IP; {"enabled": false} disables.
                                                 `semantic_port` (optional, 1..65535) additionally
                                                 enables the semantic side-channel: one JSON UDP
                                                 packet at 10Hz to (same host, semantic_port) --
                                                 see docs/TAPS_DESIGN.md.
DELETE /taps/{name}                              immediate disable (same as {"enabled": false})
POST /calibration/start  {"points": {name:        enter calibration mode: dim ring + bright
                         {"index"?, "color"?}}}?  named markers, replacing normal output
POST /calibration/move   {"name": index} or       move one/many live markers (~10-20Hz safe)
                        {"points": {name: idx}}
POST /calibration/done                            persist live positions, exit calibration
POST /calibration/cancel                          exit calibration WITHOUT saving
GET  /calibration                                 SAVED named points (GORMAPPER contract)
GET  /calibration/status                          active? + live (possibly unsaved) points
GET  /calibrate                                   phone-friendly calibration web UI (HTML)
GET  /sync                                        {"output_delay_ms": int} -- current LED
                                                 output-delay AV sync setting
POST /sync    {"output_delay_ms": int}            set the LED output delay live (0..500,
                                                 422 out of range) -- see sync.py,
                                                 docs/SYNC_DESIGN.md
POST /sync/pulse/start                            activate the installed sync-pulse scene
                                                 (remembers the prior scene)
POST /sync/pulse/stop                             restore the scene running before pulse/start
                                                 (no-op if no pulse in progress)
GET  /sync/ui                                     phone-friendly sync-calibration web UI (HTML)

Node targeting (additive, backward compatible): endpoints keep their old
behavior when no node is given — /prompt, /scene, /stop, /resume apply to
ALL enabled nodes (single-node deployments are unaffected: "all" == node 1);
/pixels and /state target the default node (node 1) because pixel indices
and stats are per-node. Unknown/disabled node names -> 404.

RING MODE (RingManager): the ring is THE ONLY control abstraction. All
control endpoints act on the whole ring as one entity; any `node` param is
ACCEPTED AND IGNORED (never a 404) — the API physically cannot split the
ring. /pixels indices are RING indices (0..ring_len-1). /prompt generates
ring-space, wrap-aware scenes at the full ring resolution. GET /nodes still
reports per-strip transport status; GET /debug/node/{name} exposes one
strip's status for debugging (read-only).
"""
from __future__ import annotations

import json
import shutil
import subprocess
import time
from pathlib import Path

from fastapi import Body, FastAPI, HTTPException, Request
from fastapi.concurrency import run_in_threadpool
from fastapi.responses import HTMLResponse
from pydantic import BaseModel, Field, model_validator

from .engine import Engine
import re
from .nl import SCENES_DIR, TASTE_DIR, generate_scene
from .control_html import CONTROL_HTML
from .nodes import NodeManager
from .scene import SceneValidationError


class PromptBody(BaseModel):
    text: str | None = Field(default=None, min_length=1, max_length=2000)
    vibe: str | None = Field(default=None, min_length=1, max_length=2000)
    node: str | None = None

    @model_validator(mode="after")
    def _need_text(self):
        if not (self.text or self.vibe):
            raise ValueError("provide 'text' (or 'vibe')")
        return self

    @property
    def prompt_text(self) -> str:
        return self.text or self.vibe  # type: ignore[return-value]


class PixelsBody(BaseModel):
    pixels: dict[str, str]
    base: str = "black"  # or "current"
    node: str | None = None


class TapBody(BaseModel):
    port: int | None = None
    fps_divisor: int | None = None
    ttl: int | None = None
    offset: int | None = None
    count: int | None = None
    semantic_port: int | None = None  # optional: enables the semantic side-channel
    enabled: bool | None = None  # False -> disable (also DELETE does this)


class SyncBody(BaseModel):
    output_delay_ms: int = Field(ge=0, le=500)


class SatelliteBody(BaseModel):
    anchor: int | None = None
    mode: str | None = None
    color: str | None = None
    boost: str | float | None = None
    vivid_saturation: float | None = None
    vivid_lift: float | None = None


# Phone-friendly calibration UI (GET /calibrate): a single self-contained
# page, no external JS/CSS. Point names/colors/initial indices are NEVER
# hardcoded here -- the page builds its sliders from whatever
# POST /calibration/start returns, so a future change to the default
# point set on the server needs no client change (see
# docs/CALIBRATION_DESIGN.md).
CALIBRATE_HTML = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
<title>Room Calibration</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body {
    margin: 0; padding: 20px 16px 90px;
    background: #0b0b0f; color: #eee;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  }
  h1 { font-size: 1.1rem; font-weight: 600; margin: 0 0 4px; letter-spacing: .02em; }
  .sub { color: #888; font-size: .8rem; margin: 0 0 24px; }
  .point { margin-bottom: 22px; }
  .point-row { display: flex; justify-content: space-between; align-items: baseline;
              margin-bottom: 6px; }
  .point-name { font-weight: 600; font-size: .95rem; }
  .point-val { font-variant-numeric: tabular-nums; color: #aaa; font-size: .85rem; }
  input[type=range] {
    width: 100%; height: 34px; -webkit-appearance: none; appearance: none;
    background: transparent; touch-action: pan-y;
  }
  input[type=range]::-webkit-slider-runnable-track {
    height: 6px; border-radius: 3px; background: #26262e;
  }
  input[type=range]::-webkit-slider-thumb {
    -webkit-appearance: none; appearance: none; width: 26px; height: 26px;
    border-radius: 50%; margin-top: -10px; border: 3px solid #0b0b0f;
    background: var(--accent, #ffffff); box-shadow: 0 0 0 1px #444;
  }
  input[type=range]::-moz-range-track { height: 6px; border-radius: 3px; background: #26262e; }
  input[type=range]::-moz-range-thumb {
    width: 20px; height: 20px; border-radius: 50%; border: 3px solid #0b0b0f;
    background: var(--accent, #ffffff);
  }
  .actions {
    position: fixed; left: 0; right: 0; bottom: 0;
    display: flex; gap: 10px; padding: 12px 16px calc(12px + env(safe-area-inset-bottom));
    background: linear-gradient(to top, #0b0b0f 60%, transparent);
  }
  button {
    flex: 1; border: none; border-radius: 12px; padding: 16px 0;
    font-size: 1rem; font-weight: 700; letter-spacing: .04em;
    -webkit-tap-highlight-color: transparent;
  }
  #done { background: #2ecc71; color: #04210f; }
  #cancel { background: #2a2a33; color: #eee; }
  #msg {
    text-align: center; min-height: 1.2em; margin: -8px 0 16px;
    font-size: .85rem; color: #6fd38a;
  }
  footer { color: #555; font-size: .75rem; text-align: center; margin-top: 8px; }
</style>
</head>
<body>
<h1>Room Calibration</h1>
<p class="sub">Drag each slider to move that marker on the physical ring.</p>
<div id="msg"></div>
<div id="points"></div>
<div class="actions">
  <button id="cancel" type="button">CANCEL</button>
  <button id="done" type="button">DONE</button>
</div>
<footer>markers show live on the ring</footer>
<script>
(function () {
  "use strict";
  var pointsEl = document.getElementById("points");
  var msgEl = document.getElementById("msg");
  var maxIndex = 1919;
  var throttleMs = 100;  // ~10Hz
  var pending = {};      // name -> latest value queued for send
  var timers = {};       // name -> setTimeout handle

  function flash(text) {
    msgEl.textContent = text;
    setTimeout(function () { if (msgEl.textContent === text) msgEl.textContent = ""; }, 2000);
  }

  function sendMove(name, value) {
    pending[name] = value;
    if (timers[name]) return;
    timers[name] = setTimeout(function () {
      var body = {};
      body[name] = pending[name];
      timers[name] = null;
      fetch("/calibration/move", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(body),
      }).catch(function () {});
    }, throttleMs);
  }

  function buildSliders(points) {
    pointsEl.innerHTML = "";
    Object.keys(points).forEach(function (name) {
      var p = points[name];
      var wrap = document.createElement("div");
      wrap.className = "point";

      var row = document.createElement("div");
      row.className = "point-row";
      var label = document.createElement("span");
      label.className = "point-name";
      label.textContent = name;
      label.style.color = p.color;
      var val = document.createElement("span");
      val.className = "point-val";
      val.textContent = p.index;
      row.appendChild(label);
      row.appendChild(val);

      var input = document.createElement("input");
      input.type = "range";
      input.min = "0";
      input.max = String(maxIndex);
      input.value = String(p.index);
      input.style.setProperty("--accent", p.color);

      input.addEventListener("input", function () {
        val.textContent = input.value;
        sendMove(name, parseInt(input.value, 10));
      });

      wrap.appendChild(row);
      wrap.appendChild(input);
      pointsEl.appendChild(wrap);
    });
  }

  function start() {
    fetch("/calibration/start", { method: "POST" })
      .then(function (r) { return r.json(); })
      .then(function (data) { buildSliders(data.status.points); })
      .catch(function () { flash("Failed to start calibration"); });
  }

  document.getElementById("done").addEventListener("click", function () {
    fetch("/calibration/done", { method: "POST" })
      .then(function () { flash("Saved"); })
      .catch(function () { flash("Save failed"); });
  });

  document.getElementById("cancel").addEventListener("click", function () {
    fetch("/calibration/cancel", { method: "POST" })
      .then(function () { flash("Cancelled"); })
      .catch(function () { flash("Cancel failed"); });
  });

  start();
})();
</script>
</body>
</html>
"""


# Phone-friendly sync-calibration UI (GET /sync/ui): one big slider for
# `output_delay_ms`, throttled ~10Hz trailing-value POSTs to /sync (same
# pattern as the calibration page's ~10Hz move throttling), plus
# start/stop for the sync-pulse metronome scene. See docs/SYNC_DESIGN.md.
SYNC_UI_HTML = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
<title>Sync Calibration</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body {
    margin: 0; padding: 20px 16px 100px;
    background: #0b0b0f; color: #eee;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  }
  h1 { font-size: 1.1rem; font-weight: 600; margin: 0 0 4px; letter-spacing: .02em; }
  .sub { color: #888; font-size: .85rem; margin: 0 0 28px; line-height: 1.5; }
  .slider-block { margin-bottom: 8px; }
  .slider-row { display: flex; justify-content: space-between; align-items: baseline;
               margin-bottom: 10px; }
  .slider-label { font-weight: 600; font-size: 1rem; }
  .slider-val { font-variant-numeric: tabular-nums; color: #ffb454; font-size: 1.4rem;
               font-weight: 700; }
  input[type=range] {
    width: 100%; height: 40px; -webkit-appearance: none; appearance: none;
    background: transparent; touch-action: pan-y;
  }
  input[type=range]::-webkit-slider-runnable-track {
    height: 8px; border-radius: 4px; background: #26262e;
  }
  input[type=range]::-webkit-slider-thumb {
    -webkit-appearance: none; appearance: none; width: 30px; height: 30px;
    border-radius: 50%; margin-top: -11px; border: 3px solid #0b0b0f;
    background: #ffb454; box-shadow: 0 0 0 1px #444;
  }
  input[type=range]::-moz-range-track { height: 8px; border-radius: 4px; background: #26262e; }
  input[type=range]::-moz-range-thumb {
    width: 24px; height: 24px; border-radius: 50%; border: 3px solid #0b0b0f;
    background: #ffb454;
  }
  .pulse-actions { display: flex; gap: 10px; margin: 32px 0 8px; }
  .pulse-actions button {
    flex: 1; border: none; border-radius: 12px; padding: 16px 0;
    font-size: .95rem; font-weight: 700; letter-spacing: .03em;
    -webkit-tap-highlight-color: transparent;
  }
  #pulse-start { background: #ff5a5f; color: #2a0405; }
  #pulse-start.active { background: #444; color: #999; }
  .actions {
    position: fixed; left: 0; right: 0; bottom: 0;
    display: flex; gap: 10px; padding: 12px 16px calc(12px + env(safe-area-inset-bottom));
    background: linear-gradient(to top, #0b0b0f 60%, transparent);
  }
  #done { flex: 1; border: none; border-radius: 12px; padding: 16px 0;
         font-size: 1rem; font-weight: 700; letter-spacing: .04em;
         background: #2ecc71; color: #04210f; -webkit-tap-highlight-color: transparent; }
  #msg {
    text-align: center; min-height: 1.2em; margin: 16px 0 0;
    font-size: .85rem; color: #6fd38a;
  }
  footer { color: #555; font-size: .75rem; text-align: center; margin-top: 24px; }
</style>
</head>
<body>
<h1>Sync Calibration</h1>
<p class="sub">Start pulse. Watch wall + LEDs flash. Drag until they flash as ONE. Done.</p>
<div class="slider-block">
  <div class="slider-row">
    <span class="slider-label">LED delay</span>
    <span class="slider-val" id="val">0 ms</span>
  </div>
  <input type="range" id="delay" min="0" max="500" step="5" value="0">
</div>
<div class="pulse-actions">
  <button id="pulse-start" type="button">START PULSE</button>
</div>
<div id="msg"></div>
<div class="actions">
  <button id="done" type="button">DONE</button>
</div>
<footer>final slider value = end-to-end projector latency</footer>
<script>
(function () {
  "use strict";
  var slider = document.getElementById("delay");
  var val = document.getElementById("val");
  var pulseBtn = document.getElementById("pulse-start");
  var msgEl = document.getElementById("msg");
  var throttleMs = 100;  // ~10Hz, trailing value
  var pending = null;
  var timer = null;
  var pulsing = false;

  function flash(text) {
    msgEl.textContent = text;
    setTimeout(function () { if (msgEl.textContent === text) msgEl.textContent = ""; }, 2000);
  }

  function sendDelay(ms) {
    pending = ms;
    if (timer) return;
    timer = setTimeout(function () {
      var v = pending;
      timer = null;
      fetch("/sync", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ output_delay_ms: v }),
      }).catch(function () {});
    }, throttleMs);
  }

  slider.addEventListener("input", function () {
    var ms = parseInt(slider.value, 10);
    val.textContent = ms + " ms";
    sendDelay(ms);
  });

  pulseBtn.addEventListener("click", function () {
    if (!pulsing) {
      fetch("/sync/pulse/start", { method: "POST" })
        .then(function () {
          pulsing = true;
          pulseBtn.textContent = "STOP PULSE";
          pulseBtn.classList.add("active");
        })
        .catch(function () { flash("Failed to start pulse"); });
    } else {
      fetch("/sync/pulse/stop", { method: "POST" })
        .then(function () {
          pulsing = false;
          pulseBtn.textContent = "START PULSE";
          pulseBtn.classList.remove("active");
        })
        .catch(function () { flash("Failed to stop pulse"); });
    }
  });

  document.getElementById("done").addEventListener("click", function () {
    var stopPulse = pulsing ? fetch("/sync/pulse/stop", { method: "POST" }) : Promise.resolve();
    stopPulse.then(function () {
      pulsing = false;
      pulseBtn.textContent = "START PULSE";
      pulseBtn.classList.remove("active");
      flash("Done -- delay is already saved");
    }).catch(function () { flash("Stop failed"); });
  });

  fetch("/sync")
    .then(function (r) { return r.json(); })
    .then(function (data) {
      var ms = data.output_delay_ms || 0;
      slider.value = String(ms);
      val.textContent = ms + " ms";
    })
    .catch(function () {});
})();
</script>
</body>
</html>
"""


def create_app(engine: Engine, manager: NodeManager | None = None) -> FastAPI:
    if manager is None:
        manager = NodeManager.single(engine)
    app = FastAPI(title="lightserver", version="1.1.0")
    app.state.engine = engine
    app.state.manager = manager

    def _targets(node: str | None) -> list[tuple[str, Engine]]:
        try:
            return manager.targets(node)
        except KeyError:
            raise HTTPException(status_code=404,
                                detail=f"unknown or disabled node '{node}'")

    def _one(node: str | None) -> Engine:
        try:
            return manager.get(node)
        except KeyError:
            raise HTTPException(status_code=404,
                                detail=f"unknown or disabled node '{node}'")

    is_ring = bool(getattr(manager, "is_ring", False))

    @app.post("/prompt")
    async def prompt(body: PromptBody):
        # Ring mode: body.node is accepted and IGNORED (ring is atomic).
        engines = _targets(body.node)
        calibration_points = None
        saved = getattr(manager, "calibration_saved", None)
        if saved is not None:
            try:
                calibration_points = saved().get("points") or None
            except Exception:
                calibration_points = None
        scene, source = await run_in_threadpool(
            generate_scene, body.prompt_text, None,
            engines[0][1].n, is_ring, calibration_points)
        applied = []
        for name, eng in engines:
            try:
                eng.set_scene(scene)
                applied.append(name)
            except SceneValidationError as e:  # belt & suspenders; nl always validates
                raise HTTPException(status_code=502,
                                    detail=f"generated scene invalid: {e}")
        manager.persist()
        return {"ok": True, "source": source, "scene": scene, "nodes": applied}

    @app.post("/scene")
    async def scene(body: dict = Body(...), node: str | None = None):
        engines = _targets(node)
        applied = []
        try:
            for name, eng in engines:
                eng.set_scene(body)
                applied.append(name)
        except SceneValidationError as e:
            raise HTTPException(status_code=422, detail=str(e))
        manager.persist()
        return {"ok": True, "scene": engines[0][1].current_scene_name,
                "nodes": applied}

    @app.post("/scene/{name}")
    async def load_scene(name: str, node: str | None = None):
        """Instant-load a SAVED scene by name from scenes/ (or archive/ or
        taste/) straight onto the ring -- no Claude call, sub-second swap.
        This is the fastest way to switch the whole room to a known look."""
        safe = re.sub(r"[^a-zA-Z0-9_-]", "", name)
        for base in (SCENES_DIR, TASTE_DIR, SCENES_DIR / "archive"):
            fp = base / f"{safe}.json"
            if fp.is_file():
                try:
                    body = json.loads(fp.read_text())
                except (OSError, json.JSONDecodeError) as e:
                    raise HTTPException(status_code=422, detail=f"bad scene file: {e}")
                applied = []
                try:
                    for nm, eng in _targets(node):
                        eng.set_scene(body)
                        applied.append(nm)
                except SceneValidationError as e:
                    raise HTTPException(status_code=422, detail=str(e))
                manager.persist()
                return {"ok": True, "scene": _targets(node)[0][1].current_scene_name,
                        "loaded": safe, "nodes": applied}
        raise HTTPException(status_code=404, detail=f"no saved scene '{safe}'")

    @app.post("/pixels")
    async def pixels(body: PixelsBody):
        eng = _one(body.node)
        try:
            mapping = {}
            for k, v in body.pixels.items():
                i = int(k)
                if not 0 <= i < eng.n:
                    raise ValueError(f"pixel index {i} out of range 0..{eng.n - 1}")
                mapping[str(i)] = v
            applied = eng.set_pixels(mapping, base=body.base)
        except (ValueError, SceneValidationError) as e:
            raise HTTPException(status_code=422, detail=str(e))
        manager.persist()
        return {"ok": True, "count": len(applied["layers"][0]["pixels"]),
                "node": eng.name}

    @app.get("/time")
    async def get_time():
        """No-auth clock-sync probe: {"unix": time.time(), "monotonic":
        time.monotonic()}. Trivially cheap -- consumers poll it repeatedly
        and take the min-RTT sample to estimate this host's clock offset.
        See docs/TAPS_DESIGN.md."""
        return {"unix": time.time(), "monotonic": time.monotonic()}

    @app.get("/scene")
    async def get_scene(node: str | None = None):
        """Return the currently-running scene JSON verbatim (whatever was
        last accepted by POST /scene or /prompt), so a caller can read back
        what's live instead of tracking it out-of-band."""
        eng = _one(node)
        return {"scene": eng.current_scene, "node": eng.name}

    @app.get("/state")
    async def state(node: str | None = None):
        s = _one(node).stats()
        s["nodes"] = manager.status()
        return s

    @app.get("/nodes")
    async def nodes():
        out = {"default": manager.default_name, "nodes": manager.status()}
        if is_ring:
            out["ring"] = {"length": manager.default.n,
                           "segments": [s.as_dict()
                                        for s in manager.segments]}
        return out

    @app.get("/debug/node/{name}")
    async def debug_node(name: str):
        """Read-only per-strip status (ring mode keeps strips atomic for
        control, but transport debugging stays per-node)."""
        for d in manager.status():
            if d.get("name") == name:
                return d
        raise HTTPException(status_code=404, detail=f"unknown node '{name}'")

    @app.post("/stop")
    async def stop(node: str | None = None):
        stopped = []
        for name, eng in _targets(node):
            eng.stop_streaming()
            stopped.append(name)
        manager.persist()
        return {"ok": True, "streaming": False, "nodes": stopped,
                "note": "firmware falls back to onboard effect after 5s of silence"}

    @app.post("/resume")
    async def resume(node: str | None = None):
        resumed = []
        for name, eng in _targets(node):
            eng.resume_streaming()
            resumed.append(name)
        manager.persist()
        return {"ok": True, "streaming": engine.streaming, "nodes": resumed}

    @app.get("/satellites")
    async def satellites():
        """List satellite 'dot' nodes: anchor, mode, color, boost,
        vivid_saturation, vivid_lift, effective_gain, online. [] when the
        manager has none (non-ring deployments, or no satellites.json)."""
        getter = getattr(manager, "satellites_status", None)
        return getter() if getter else []

    @app.post("/satellites/{name}")
    async def set_satellite(name: str, body: SatelliteBody):
        """Reposition (anchor), switch mode (mirror/independent), set the
        independent-mode color, set the brightness boost
        ("off"|number|"auto"|"vivid"), or set the "vivid" saturation
        floor (vivid_saturation, 0.0..1.0 — independently settable, e.g.
        without resending boost), or set the "vivid" lift exponent
        (vivid_lift, 0.3..1.5, default 0.7 — also independently settable)
        of one satellite. Persists like a scene does."""
        setter = getattr(manager, "set_satellite", None)
        if setter is None:
            raise HTTPException(status_code=404, detail="satellites not available")
        try:
            result = setter(name, anchor=body.anchor, mode=body.mode,
                            color=body.color, boost=body.boost,
                            vivid_saturation=body.vivid_saturation,
                            vivid_lift=body.vivid_lift)
        except KeyError:
            raise HTTPException(status_code=404,
                                detail=f"unknown satellite '{name}'")
        except ValueError as e:
            raise HTTPException(status_code=422, detail=str(e))
        manager.persist()
        return {"ok": True, "satellite": result}

    @app.get("/taps")
    async def taps():
        """List every configured canvas tap (enabled or not): name, host,
        port, offset, count, fps_divisor, ttl, enabled,
        seconds_until_expiry, frames_sent. [] when taps aren't available
        (non-ring deployments)."""
        getter = getattr(manager, "taps_status", None)
        return getter() if getter else []

    @app.post("/taps/{name}")
    async def set_tap(name: str, body: TapBody, request: Request):
        """Create (port required) or keepalive (port optional once
        created) a canvas tap, refreshing its TTL. The destination host is
        ALWAYS derived from the request's source IP -- never from the
        body. {"enabled": false} disables immediately (same as DELETE)."""
        setter = getattr(manager, "set_tap", None)
        disabler = getattr(manager, "disable_tap", None)
        if setter is None or disabler is None:
            raise HTTPException(status_code=404, detail="taps not available")
        if body.enabled is False:
            try:
                disabler(name)
            except KeyError:
                raise HTTPException(status_code=404, detail=f"unknown tap '{name}'")
            manager.persist()
            return {"ok": True, "tap": manager.tap_status(name)}
        host = request.client.host if request.client else None
        if not host:
            raise HTTPException(status_code=400,
                                detail="cannot determine source host")
        try:
            result = setter(name, host=host, port=body.port,
                            fps_divisor=body.fps_divisor, ttl=body.ttl,
                            offset=body.offset, count=body.count,
                            semantic_port=body.semantic_port)
        except ValueError as e:
            raise HTTPException(status_code=422, detail=str(e))
        manager.persist()
        return {"ok": True, "tap": result}

    @app.delete("/taps/{name}")
    async def delete_tap(name: str):
        """Immediate disable (config is kept, so GET /taps still lists it;
        a later keepalive re-enables it)."""
        disabler = getattr(manager, "disable_tap", None)
        if disabler is None:
            raise HTTPException(status_code=404, detail="taps not available")
        try:
            disabler(name)
        except KeyError:
            raise HTTPException(status_code=404, detail=f"unknown tap '{name}'")
        manager.persist()
        return {"ok": True, "tap": manager.tap_status(name)}

    @app.get("/scenes")
    async def scenes():
        out = {}
        for p in sorted(Path(SCENES_DIR).glob("*.json")):
            try:
                out[p.stem] = json.loads(p.read_text()).get("name", p.stem)
            except (OSError, json.JSONDecodeError):
                continue
        return out


    def _slugify(name: str) -> str:
        slug = re.sub(r"[^a-z0-9]+", "-", (name or "").lower()).strip("-")
        return slug[:48] or "fav"

    @app.post("/like")
    async def like(body: dict | None = Body(default=None), node: str | None = None):
        """Thumbs-up the CURRENTLY-RUNNING scene: copy it into scenes/taste/
        so future /prompt generations bias toward this aesthetic. Optional
        body {"name": "..."} sets the saved filename; otherwise the scene's
        own name (or a timestamp) is used. This is how neptr learns Emory's
        taste over time -- one tap per look you love."""
        scene = _one(node).current_scene
        if not scene:
            raise HTTPException(status_code=409, detail="no scene is running")
        name = (body or {}).get("name") or scene.get("name") or f"fav-{int(time.time())}"
        TASTE_DIR.mkdir(parents=True, exist_ok=True)
        slug = _slugify(name)
        dest = TASTE_DIR / f"{slug}.json"
        i = 2
        while dest.exists():
            dest = TASTE_DIR / f"{slug}-{i}.json"
            i += 1
        dest.write_text(json.dumps(scene, indent=1))
        return {"ok": True, "saved": dest.stem,
                "taste_count": len(list(TASTE_DIR.glob("*.json")))}

    @app.get("/taste")
    async def taste():
        """List the favorites currently biasing generation (scenes/taste/)."""
        out = {}
        if TASTE_DIR.is_dir():
            for p in sorted(TASTE_DIR.glob("*.json")):
                try:
                    out[p.stem] = json.loads(p.read_text()).get("name", p.stem)
                except (OSError, json.JSONDecodeError):
                    continue
        return {"favorites": out, "count": len(out)}

    @app.delete("/taste/{name}")
    async def untaste(name: str):
        """Remove one favorite from the taste bias."""
        p = TASTE_DIR / f"{_slugify(name)}.json"
        if not p.is_file():
            raise HTTPException(status_code=404, detail=f"no favorite '{name}'")
        p.unlink()
        return {"ok": True, "removed": p.stem,
                "taste_count": len(list(TASTE_DIR.glob("*.json")))}

    # --- the 2 Miortior BLE poles (via mosquitto -> blebridge) -------------
    def _lamp_publish(lamp_id: str, payload: dict) -> None:
        pub = shutil.which("mosquitto_pub")
        if not pub:
            raise HTTPException(status_code=503, detail="mosquitto_pub not installed")
        topic = f"lights/lamp{lamp_id}/set"
        try:
            subprocess.run([pub, "-t", topic, "-m", json.dumps(payload)],
                           check=True, capture_output=True, timeout=4)
        except subprocess.CalledProcessError as e:
            raise HTTPException(status_code=502,
                                detail=f"publish failed: {e.stderr.decode()[:200]}")
        except subprocess.TimeoutExpired:
            raise HTTPException(status_code=504, detail="mqtt publish timed out")

    @app.post("/lamp/{lamp_id}")
    async def lamp(lamp_id: str, body: dict = Body(...)):
        """Control a Miortior pole (1 or 2): {on?,color?,brightness?,effect?}.
        Publishes to lights/lamp{id}/set on the local broker; the blebridge
        service translates it to BLE. Whole-lamp RGB (no per-pixel)."""
        if lamp_id not in ("1", "2"):
            raise HTTPException(status_code=404, detail="lamp must be 1 or 2")
        allowed = {k: body[k] for k in ("on", "color", "brightness", "effect")
                   if k in body}
        if not allowed:
            raise HTTPException(status_code=422, detail="nothing to set")
        _lamp_publish(lamp_id, allowed)
        return {"ok": True, "lamp": lamp_id, "sent": allowed}

    @app.get("/lamp/health")
    async def lamp_health():
        """Is the BLE pole bridge up? (systemctl is-active blebridge)"""
        try:
            r = subprocess.run(["systemctl", "is-active", "blebridge"],
                               capture_output=True, text=True, timeout=3)
            active = r.stdout.strip() == "active"
        except (subprocess.SubprocessError, OSError):
            active = False
        return {"ok": active, "bridge": "blebridge",
                "mqtt_pub": bool(shutil.which("mosquitto_pub"))}

    # -- room calibration --------------------------------------------------
    @app.post("/calibration/start")
    async def calibration_start(body: dict | None = Body(default=None)):
        """Enter calibration mode. Optional body: {"points": {name:
        {"index"?, "color"?}}}. Omitted -> default 6-point set, starting
        from any previously-saved indices for matching names."""
        starter = getattr(manager, "calibration_start", None)
        if starter is None:
            raise HTTPException(status_code=404, detail="calibration not available")
        points = (body or {}).get("points")
        try:
            result = starter(points)
        except (ValueError, KeyError) as e:
            raise HTTPException(status_code=422, detail=str(e))
        return {"ok": True, "status": result}

    @app.post("/calibration/move")
    async def calibration_move(body: dict = Body(...)):
        """Move one or more live markers. Body is either {"name": index}
        (a single point) or {"points": {"name": index, ...}}. Cheap --
        safe to call at ~10-20Hz while a UI slider drags."""
        mover = getattr(manager, "calibration_move", None)
        if mover is None:
            raise HTTPException(status_code=404, detail="calibration not available")
        updates = body.get("points") if isinstance(body.get("points"), dict) else body
        if not updates:
            raise HTTPException(status_code=422, detail="no points given")
        try:
            updates = {str(k): int(v) for k, v in updates.items()}
        except (TypeError, ValueError):
            raise HTTPException(status_code=422, detail="index values must be integers")
        try:
            result = mover(updates)
        except KeyError as e:
            raise HTTPException(status_code=404,
                                detail=f"unknown calibration point '{e.args[0]}'")
        except ValueError as e:
            raise HTTPException(status_code=422, detail=str(e))
        return {"ok": True, "status": result}

    @app.post("/calibration/done")
    async def calibration_done():
        """Persist all current live point positions, exit calibration mode.
        Normal scene rendering resumes on the very next frame."""
        doner = getattr(manager, "calibration_done", None)
        if doner is None:
            raise HTTPException(status_code=404, detail="calibration not available")
        result = doner()
        manager.persist()
        return {"ok": True, **result}

    @app.post("/calibration/cancel")
    async def calibration_cancel():
        """Exit calibration mode WITHOUT saving; normal scene resumes
        instantly."""
        canceler = getattr(manager, "calibration_cancel", None)
        if canceler is None:
            raise HTTPException(status_code=404, detail="calibration not available")
        canceler()
        return {"ok": True, "active": False}

    @app.get("/calibration")
    async def get_calibration():
        """The SAVED (persisted) named points -- the GORMAPPER consumer
        contract: {"points": {name: index, ...}, "updated": iso8601}.
        `points["projL"] > points["projR"]` means the projection window
        crosses the ring seam (wraps through index 0)."""
        getter = getattr(manager, "calibration_saved", None)
        if getter is None:
            raise HTTPException(status_code=404, detail="calibration not available")
        return getter()

    @app.get("/calibration/status")
    async def calibration_status():
        """Whether calibration mode is active, plus the live (possibly
        unsaved) point positions/colors while it is."""
        getter = getattr(manager, "calibration_status", None)
        if getter is None:
            raise HTTPException(status_code=404, detail="calibration not available")
        return getter()

    @app.get("/control", response_class=HTMLResponse)
    async def control_page():
        """The GORCAVE control panel: vibe box, quick color, saved looks,
        taste (like), power, and the 2 poles. Same-origin control surface."""
        return HTMLResponse(CONTROL_HTML)

    @app.get("/calibrate", response_class=HTMLResponse)
    async def calibrate_page():
        """Phone-friendly calibration UI: dynamically builds its sliders
        from POST /calibration/start's response, so it needs no changes if
        the server-side default point set changes."""
        return HTMLResponse(CALIBRATE_HTML)

    # -- output-delay AV sync -----------------------------------------------
    @app.get("/sync")
    async def get_sync():
        """Current LED output-delay AV sync setting: {"output_delay_ms": int}."""
        getter = getattr(manager, "sync_status", None)
        if getter is None:
            raise HTTPException(status_code=404, detail="sync not available")
        return getter()

    @app.post("/sync")
    async def set_sync(body: SyncBody):
        """Set the LED output delay live (applies within a frame or two,
        no restart). 0..500ms, 422 out of range."""
        setter = getattr(manager, "set_sync", None)
        if setter is None:
            raise HTTPException(status_code=404, detail="sync not available")
        try:
            result = setter(body.output_delay_ms)
        except ValueError as e:
            raise HTTPException(status_code=422, detail=str(e))
        manager.persist()
        return result

    @app.post("/sync/pulse/start")
    async def sync_pulse_start():
        """Activate the installed sync-pulse metronome scene, remembering
        the prior scene so /sync/pulse/stop can restore it exactly."""
        starter = getattr(manager, "sync_pulse_start", None)
        if starter is None:
            raise HTTPException(status_code=404, detail="sync not available")
        try:
            result = starter()
        except (OSError, json.JSONDecodeError) as e:
            raise HTTPException(status_code=500,
                                detail=f"cannot load sync-pulse scene: {e}")
        return {"ok": True, **result}

    @app.post("/sync/pulse/stop")
    async def sync_pulse_stop():
        """Restore whatever scene was running before /sync/pulse/start.
        No-op (keeps the current scene) if no pulse is in progress."""
        stopper = getattr(manager, "sync_pulse_stop", None)
        if stopper is None:
            raise HTTPException(status_code=404, detail="sync not available")
        result = stopper()
        return {"ok": True, **result}

    @app.get("/sync/ui", response_class=HTMLResponse)
    async def sync_ui_page():
        """Phone-friendly sync-calibration UI: one delay slider + pulse
        start/stop, per docs/SYNC_DESIGN.md."""
        return HTMLResponse(SYNC_UI_HTML)

    return app
