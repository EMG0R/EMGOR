"""gorcave-gate — the 2FA front door to the Pi.

One small FastAPI service (localhost:8795, exposed on the Tailscale funnel at
/api/gate and /terminal). Login = passphrase + TOTP code -> a short-lived
HMAC bearer token. The token gates:
  * the web terminal (xterm.js <-> a PTY running Claude Code on the Pi)
  * the room-light API (dashboard-api verifies the same token)

Config: ~/gorcave-gate/config.json
  {"passphrase_hash","salt","totp_secret","token_secret"}
Nothing secret is ever served to the public; the TOTP secret is enrolled once
out-of-band (QR shown in a trusted session).
"""
from __future__ import annotations
import asyncio, base64, fcntl, hashlib, hmac, json, os, pty, signal, struct
import termios, time
from pathlib import Path

import pyotp
from fastapi import FastAPI, HTTPException, Request, WebSocket, WebSocketDisconnect
from fastapi.responses import HTMLResponse, JSONResponse

CONFIG = Path(__file__).resolve().parent / "config.json"
TOKEN_TTL = 12 * 3600           # 12h sessions
LOGIN_WINDOW = 300              # rate-limit window (s)
LOGIN_MAX = 8                   # max attempts per IP per window

cfg = json.loads(CONFIG.read_text())
app = FastAPI()
_attempts: dict[str, list[float]] = {}


# ---- auth core ----------------------------------------------------------
def _hash_pass(pw: str, salt: str) -> str:
    return hashlib.sha256((salt + pw).encode()).hexdigest()


def check_passphrase(pw: str) -> bool:
    return hmac.compare_digest(_hash_pass(pw, cfg["salt"]), cfg["passphrase_hash"])


def check_totp(code: str) -> bool:
    try:
        return pyotp.TOTP(cfg["totp_secret"]).verify(str(code).strip(), valid_window=1)
    except Exception:
        return False


def issue_token() -> str:
    exp = str(int(time.time()) + TOKEN_TTL)
    sig = hmac.new(cfg["token_secret"].encode(), exp.encode(), hashlib.sha256).hexdigest()
    return base64.urlsafe_b64encode(f"{exp}:{sig}".encode()).decode()


def valid_token(token: str | None) -> bool:
    if not token:
        return False
    try:
        exp, sig = base64.urlsafe_b64decode(token.encode()).decode().split(":", 1)
        good = hmac.new(cfg["token_secret"].encode(), exp.encode(), hashlib.sha256).hexdigest()
        return hmac.compare_digest(sig, good) and int(exp) > time.time()
    except Exception:
        return False


def bearer(request: Request) -> str | None:
    h = request.headers.get("authorization", "")
    return h[7:] if h.lower().startswith("bearer ") else None


def rate_ok(ip: str) -> bool:
    now = time.time()
    hits = [t for t in _attempts.get(ip, []) if now - t < LOGIN_WINDOW]
    _attempts[ip] = hits
    return len(hits) < LOGIN_MAX


# ---- endpoints ----------------------------------------------------------
@app.post("/api/gate/login")
async def login(request: Request):
    ip = request.client.host if request.client else "?"
    if not rate_ok(ip):
        raise HTTPException(status_code=429, detail="too many attempts, wait a few minutes")
    _attempts.setdefault(ip, []).append(time.time())
    body = await request.json()
    if check_passphrase(str(body.get("passphrase", ""))) and check_totp(body.get("code", "")):
        _attempts[ip] = []  # reset on success
        return {"ok": True, "token": issue_token(), "ttl": TOKEN_TTL}
    raise HTTPException(status_code=401, detail="wrong passphrase or code")


@app.get("/api/gate/verify")
async def verify(request: Request):
    if valid_token(bearer(request)):
        return {"ok": True}
    raise HTTPException(status_code=401, detail="invalid or expired token")


@app.get("/terminal", response_class=HTMLResponse)
async def terminal_page():
    return HTMLResponse(TERMINAL_HTML)


# ---- PTY web terminal ---------------------------------------------------
@app.websocket("/api/gate/pty")
async def pty_ws(ws: WebSocket):
    token = ws.query_params.get("token")
    if not valid_token(token):
        await ws.close(code=4401)
        return
    await ws.accept()
    pid, fd = pty.fork()
    if pid == 0:  # child -> Claude Code in the home dir, full-Pi context
        os.environ["TERM"] = "xterm-256color"
        os.execvp("bash", ["bash", "-lc", "cd ~ && claude"])
        os._exit(1)
    loop = asyncio.get_event_loop()

    def _read():
        try:
            data = os.read(fd, 65536)
        except OSError:
            data = b""
        if data:
            asyncio.run_coroutine_threadsafe(ws.send_bytes(data), loop)
        else:
            loop.remove_reader(fd)
    loop.add_reader(fd, _read)
    try:
        while True:
            msg = await ws.receive()
            if msg.get("type") == "websocket.disconnect":
                break
            if "bytes" in msg and msg["bytes"] is not None:
                os.write(fd, msg["bytes"])
            elif "text" in msg and msg["text"]:
                t = msg["text"]
                if t.startswith("\x00resize:"):  # "\0resize:cols,rows"
                    try:
                        cols, rows = (int(x) for x in t[8:].split(","))
                        fcntl.ioctl(fd, termios.TIOCSWINSZ,
                                    struct.pack("HHHH", rows, cols, 0, 0))
                    except Exception:
                        pass
                else:
                    os.write(fd, t.encode())
    except WebSocketDisconnect:
        pass
    finally:
        try:
            loop.remove_reader(fd)
        except Exception:
            pass
        try:
            os.kill(pid, signal.SIGKILL)
            os.close(fd)
        except Exception:
            pass


TERMINAL_HTML = r"""<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>gorcave · terminal</title>
<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/xterm/5.3.0/xterm.min.css">
<style>html,body{margin:0;height:100%;background:#0a0512}#t{height:100%;padding:6px}
#bar{position:fixed;top:0;right:0;font:11px monospace;color:rgba(178,163,214,.7);padding:6px 10px;z-index:2}</style>
</head><body>
<div id="bar">gorcave · neptr</div><div id="t"></div>
<script src="https://cdnjs.cloudflare.com/ajax/libs/xterm/5.3.0/xterm.min.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/xterm-addon-fit/0.8.0/xterm-addon-fit.min.js"></script>
<script>
const qs=new URLSearchParams(location.search);
const token=qs.get("token")||localStorage.getItem("gorcave_token")||"";
const term=new Terminal({fontFamily:"monospace",fontSize:13,cursorBlink:true,
  theme:{background:"#0a0512",foreground:"#ece7f7",cursor:"#b2a3d6"}});
const fit=new FitAddon.FitAddon();term.loadAddon(fit);term.open(document.getElementById("t"));fit.fit();
const proto=location.protocol==="https:"?"wss":"ws";
const ws=new WebSocket(`${proto}://${location.host}/api/gate/pty?token=${encodeURIComponent(token)}`);
ws.binaryType="arraybuffer";
ws.onopen=()=>{sendResize();term.focus();};
ws.onmessage=e=>term.write(typeof e.data==="string"?e.data:new Uint8Array(e.data));
ws.onclose=e=>term.write(`\r\n\x1b[31m[disconnected${e.code===4401?": bad/expired token — log in again":""}]\x1b[0m\r\n`);
term.onData(d=>ws.readyState===1&&ws.send(d));
function sendResize(){if(ws.readyState===1)ws.send(`\x00resize:${term.cols},${term.rows}`);}
addEventListener("resize",()=>{fit.fit();sendResize();});
</script></body></html>"""
