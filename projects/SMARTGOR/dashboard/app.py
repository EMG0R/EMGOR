"""smartGOR dashboard — FastAPI + MQTT bridge for the room installation.

Runs against a real Mosquitto broker (Pi) or, when no broker is reachable,
falls back to demo mode with simulated device state so the UI works anywhere.
"""

import asyncio
import json
import os
import random
import threading
import time
from contextlib import asynccontextmanager
from pathlib import Path

import httpx
import yaml
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse, JSONResponse

import paho.mqtt.client as mqtt

BASE = Path(__file__).parent
CONFIG = yaml.safe_load((BASE / "config.yaml").read_text())
SCENES_PATH = BASE / "scenes.json"

ACTIVE_ZONES = CONFIG.get("active_plant_zones", [1, 2, 3])
SWITCHES = CONFIG.get("switches", [])

STRIP_CFG = CONFIG.get("strip", {})
STRIP_HTTP_URL = STRIP_CFG.get("http_url", "http://fcob-strip.local").rstrip("/")
STRIP_SET_TOPIC = STRIP_CFG.get("set_topic", "lights/strip/set")
STRIP_STATE_TOPIC = STRIP_CFG.get("state_topic", "lights/strip/state")
STRIP_POLL_INTERVAL = float(STRIP_CFG.get("poll_interval", 3))
# Keys the ESP32 /api/set contract understands.
STRIP_KEYS = ("mode", "color", "brightness", "style")


def initial_state():
    state = {
        "strip": {"name": "FCOB Strip", "hostname": "fcob-strip", "mode": "generative", "style": "nebula",
                  "brightness": 128, "color": "#8b2fc9", "wifi": False, "ip": None, "online": False},
        "lamp1": {"name": "Corner Lamp 1", "on": False, "color": "#8b2fc9", "brightness": 70, "effect": "static"},
        "lamp2": {"name": "Corner Lamp 2", "on": False, "color": "#5d2fc9", "brightness": 70, "effect": "static"},
        "jars": {"name": "NeoPixel Jars", "scene": "off", "palette": "purple"},
        "tank": {"level_pct": None, "low": False},
        "plants": {str(z): {"moisture": None, "last_watered": None} for z in ACTIVE_ZONES},
        "switches": {s["id"]: {"name": s["name"], "on": False} for s in SWITCHES},
    }
    return state


class Hub:
    """Holds device state, MQTT connection (or demo simulator), and WS fanout."""

    def __init__(self):
        self.state = initial_state()
        self.lock = threading.Lock()
        self.clients: set[WebSocket] = set()
        self.loop: asyncio.AbstractEventLoop | None = None
        self.mqtt_client: mqtt.Client | None = None
        self.mqtt_connected = False
        self.demo = False
        # HTTP transport for the FCOB strip ESP32.
        self.http = httpx.Client(timeout=2.0)
        self.strip_online = False
        self._stop = False

    # ---------- state + fanout ----------

    def snapshot(self):
        with self.lock:
            return json.loads(json.dumps(self.state))

    def broadcast(self):
        if self.loop is None:
            return
        payload = {"type": "state", "demo": self.demo, "mqtt": self.mqtt_connected,
                   "state": self.snapshot()}
        asyncio.run_coroutine_threadsafe(self._send_all(payload), self.loop)

    async def _send_all(self, payload):
        dead = []
        for ws in list(self.clients):
            try:
                await ws.send_json(payload)
            except Exception:
                dead.append(ws)
        for ws in dead:
            self.clients.discard(ws)

    # ---------- command handling ----------

    def command(self, topic: str, payload: dict | str):
        """Publish a command; in demo mode (or optimistically) mirror it into state."""
        # The FCOB strip has a real HTTP API — route it there (dual-path with MQTT).
        if topic == STRIP_SET_TOPIC and isinstance(payload, dict):
            self.strip_set(payload)
            return
        if self.mqtt_client and self.mqtt_connected:
            body = payload if isinstance(payload, str) else json.dumps(payload)
            self.mqtt_client.publish(topic, body)
        self._apply_command_locally(topic, payload)
        self.broadcast()

    # ---------- FCOB strip HTTP transport ----------

    @staticmethod
    def _normalize_strip_payload(payload: dict) -> dict:
        """Map UI/scene payloads onto the ESP32 {mode,color,brightness,style} contract."""
        out = {k: payload[k] for k in STRIP_KEYS if k in payload}
        # Legacy on/off -> brightness (the device has no on flag, just brightness/mode).
        if "on" in payload and "brightness" not in out:
            out["brightness"] = payload.get("brightness", 128) if payload["on"] else 0
        # Sending a color implies solid mode (matches the device's own behaviour).
        if "color" in out:
            out.setdefault("mode", "solid")
        return out

    def strip_set(self, payload: dict):
        """Send a strip command over HTTP to the ESP32 AND to MQTT if a broker is up."""
        body = self._normalize_strip_payload(payload)
        if not body:
            return
        # Optimistic local update so the UI feels instant even before the poll.
        with self.lock:
            self.state["strip"].update(body)
        # HTTP path (works now).
        try:
            resp = self.http.post(f"{STRIP_HTTP_URL}/api/set", json=body)
            resp.raise_for_status()
            self._ingest_strip_state(resp.json())
        except Exception:
            # Device unreachable — don't crash; keep optimistic state, mark offline.
            self.strip_online = False
        # MQTT path (works later, once the broker is online).
        if self.mqtt_client and self.mqtt_connected:
            try:
                self.mqtt_client.publish(STRIP_SET_TOPIC, json.dumps(body))
            except Exception:
                pass
        self.broadcast()

    def _ingest_strip_state(self, data: dict):
        """Merge a device /api/state (or /api/set) response into local strip state."""
        if not isinstance(data, dict):
            return
        with self.lock:
            for k in ("name", "hostname", "mode", "style", "brightness", "color", "wifi", "ip"):
                if k in data:
                    self.state["strip"][k] = data[k]
            self.strip_online = True
            self.state["strip"]["online"] = True

    def _poll_strip(self):
        """Background poller: keep live strip state; mark offline when unreachable."""
        while not self._stop:
            try:
                resp = self.http.get(f"{STRIP_HTTP_URL}/api/state")
                resp.raise_for_status()
                self._ingest_strip_state(resp.json())
            except Exception:
                if self.strip_online:
                    self.broadcast()  # push the offline transition once
                self.strip_online = False
                with self.lock:
                    self.state["strip"]["online"] = False
            else:
                self.broadcast()
            time.sleep(STRIP_POLL_INTERVAL)

    def _apply_command_locally(self, topic: str, payload):
        with self.lock:
            parts = topic.split("/")
            if topic == "lights/strip/set" and isinstance(payload, dict):
                self.state["strip"].update(payload)
            elif topic in ("lights/lamp1/set", "lights/lamp2/set") and isinstance(payload, dict):
                self.state[parts[1]].update(payload)
            elif topic == "lights/jars/scene" and isinstance(payload, dict):
                self.state["jars"]["scene"] = payload.get("scene", self.state["jars"]["scene"])
            elif topic == "lights/jars/palette" and isinstance(payload, dict):
                self.state["jars"]["palette"] = payload.get("palette", self.state["jars"]["palette"])
            elif len(parts) == 3 and parts[0] == "plants" and parts[2] == "water":
                zone = parts[1].removeprefix("zone")
                if zone in self.state["plants"]:
                    self.state["plants"][zone]["last_watered"] = time.strftime("%H:%M:%S")
                    if self.demo and self.state["plants"][zone]["moisture"] is not None:
                        self.state["plants"][zone]["moisture"] = min(
                            95, self.state["plants"][zone]["moisture"] + random.randint(15, 25))
            elif len(parts) == 4 and parts[0] == "room" and parts[1] == "switch" and parts[3] == "set":
                sid = parts[2]
                if sid in self.state["switches"]:
                    on = payload == "on" or (isinstance(payload, dict) and payload.get("on"))
                    self.state["switches"][sid]["on"] = bool(on)

    # ---------- MQTT ingest ----------

    def on_mqtt_message(self, _client, _userdata, msg):
        try:
            text = msg.payload.decode()
        except UnicodeDecodeError:
            return
        with self.lock:
            parts = msg.topic.split("/")
            if parts[:2] == ["plants", "tank"]:
                try:
                    self.state["tank"]["level_pct"] = float(text)
                except ValueError:
                    self.state["tank"]["low"] = text.lower() in ("low", "1", "true")
            elif len(parts) == 3 and parts[0] == "plants" and parts[2] == "moisture":
                zone = parts[1].removeprefix("zone")
                if zone in self.state["plants"]:
                    try:
                        self.state["plants"][zone]["moisture"] = float(text)
                    except ValueError:
                        pass
        self.broadcast()

    def start_mqtt(self):
        cfg = CONFIG["mqtt"]
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                             client_id=cfg.get("client_id", "smartgor-dashboard"))
        client.on_message = self.on_mqtt_message

        def on_connect(c, _u, _f, rc, _p=None):
            self.mqtt_connected = rc == 0
            if rc == 0:
                c.subscribe([("plants/#", 0), ("lights/+/status", 0), ("room/#", 0)])
            self.broadcast()

        def on_disconnect(*_a):
            self.mqtt_connected = False
            self.broadcast()

        client.on_connect = on_connect
        client.on_disconnect = on_disconnect
        try:
            client.connect(cfg.get("host", "localhost"), cfg.get("port", 1883),
                           cfg.get("keepalive", 30))
            client.loop_start()
            self.mqtt_client = client
            # give the broker a moment; if it never connects we run demo sim anyway
            threading.Timer(3.0, self._check_demo_fallback).start()
        except OSError:
            self._enter_demo()

    def _check_demo_fallback(self):
        if not self.mqtt_connected:
            self._enter_demo()

    def _enter_demo(self):
        self.demo = True
        threading.Thread(target=self._demo_sim, daemon=True).start()
        self.broadcast()

    def _demo_sim(self):
        with self.lock:
            for z in self.state["plants"].values():
                z["moisture"] = random.randint(35, 80)
            self.state["tank"]["level_pct"] = 72.0
        self.broadcast()
        while True:
            time.sleep(5)
            with self.lock:
                for z in self.state["plants"].values():
                    if z["moisture"] is not None:
                        z["moisture"] = max(5, round(z["moisture"] - random.uniform(0, 0.8), 1))
                if self.state["tank"]["level_pct"] is not None:
                    self.state["tank"]["level_pct"] = max(
                        0, round(self.state["tank"]["level_pct"] - random.uniform(0, 0.2), 1))
                    self.state["tank"]["low"] = self.state["tank"]["level_pct"] < 15
            self.broadcast()


hub = Hub()


@asynccontextmanager
async def lifespan(app: FastAPI):
    hub.loop = asyncio.get_running_loop()
    hub.start_mqtt()
    threading.Thread(target=hub._poll_strip, daemon=True).start()
    yield
    hub._stop = True
    hub.http.close()
    if hub.mqtt_client:
        hub.mqtt_client.loop_stop()


app = FastAPI(title="smartGOR dashboard", lifespan=lifespan)


@app.get("/")
def index():
    return FileResponse(BASE / "static" / "index.html")


@app.get("/api/state")
def get_state():
    return {"demo": hub.demo, "mqtt": hub.mqtt_connected, "state": hub.snapshot()}


@app.get("/api/scenes")
def get_scenes():
    return json.loads(SCENES_PATH.read_text())


@app.post("/api/scene/{name}")
def run_scene(name: str):
    scenes = json.loads(SCENES_PATH.read_text())
    if name not in scenes:
        return JSONResponse({"error": "unknown scene"}, status_code=404)
    for cmd in scenes[name]["commands"]:
        hub.command(cmd["topic"], cmd["payload"])
    return {"ok": True, "ran": name}


@app.post("/api/command")
async def post_command(body: dict):
    topic, payload = body.get("topic"), body.get("payload")
    if not topic:
        return JSONResponse({"error": "topic required"}, status_code=400)
    hub.command(topic, payload)
    return {"ok": True}


@app.post("/api/strip")
async def post_strip(body: dict):
    """Direct strip control — {mode?, color?, brightness?, style?}. Routes to HTTP + MQTT."""
    hub.strip_set(body)
    return {"ok": True, "online": hub.strip_online, "strip": hub.snapshot()["strip"]}


@app.post("/api/plants/water/{zone}")
def water(zone: int):
    hub.command(f"plants/zone{zone}/water", {"duration_s": 3})
    return {"ok": True, "zone": zone}


@app.websocket("/ws")
async def ws_endpoint(ws: WebSocket):
    await ws.accept()
    hub.clients.add(ws)
    await ws.send_json({"type": "state", "demo": hub.demo, "mqtt": hub.mqtt_connected,
                        "state": hub.snapshot()})
    try:
        while True:
            await ws.receive_text()  # client pings; commands go over REST
    except WebSocketDisconnect:
        hub.clients.discard(ws)


if __name__ == "__main__":
    import uvicorn
    srv = CONFIG.get("server", {})
    uvicorn.run(app, host=srv.get("host", "0.0.0.0"), port=srv.get("port", 8420))
