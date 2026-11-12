#!/usr/bin/env bash
# Run ON gorcave (home LAN / tailnet). Finishes Part 2:
#  1) ensure the gate service is up
#  2) lock the light API behind the gate token (dashboard-api)
#  3) expose gate + terminal on the Tailscale funnel
#  4) verify
set -uo pipefail
echo "== 1. gate service =="
sudo systemctl enable --now gorcave-gate 2>/dev/null
systemctl is-active gorcave-gate

echo "== 2. lock lights proxy behind gate token =="
python3 - << 'PY'
p="/home/server/dashboard/app.py"; s=open(p).read()
if "_gate_ok(" not in s:
    helper='''
import base64, hmac, hashlib, time as _t
from pathlib import Path as _P
_GATE_CFG=_P("/home/server/gorcave-gate/config.json")
def _gate_secret():
    try: return __import__("json").loads(_GATE_CFG.read_text())["token_secret"]
    except Exception: return None
def _gate_ok(auth: str) -> bool:
    if not auth.lower().startswith("bearer "): return False
    tok=auth[7:]; sec=_gate_secret()
    if not sec: return False
    try:
        exp,sig=base64.urlsafe_b64decode(tok.encode()).decode().split(":",1)
        good=hmac.new(sec.encode(),exp.encode(),hashlib.sha256).hexdigest()
        return hmac.compare_digest(sig,good) and int(exp)>_t.time()
    except Exception: return False
'''
    s=s.replace("LIGHTS_BASE =", helper+"\nLIGHTS_BASE =",1)
    guard='''    if not _gate_ok(request.headers.get("authorization","")):
        return Response(content=__import__("json").dumps({"detail":"login required"}),
                        status_code=401, media_type="application/json")
    url = LIGHTS_BASE'''
    s=s.replace("    url = LIGHTS_BASE", guard, 1)
    open(p,"w").write(s); print("dashboard-api: lights proxy now requires a gate token")
else:
    print("dashboard-api: already locked")
PY
python3 -c 'import ast; ast.parse(open("/home/server/dashboard/app.py").read()); print("app.py parses clean")' || { echo "PARSE FAIL - not restarting"; exit 1; }
sudo systemctl restart dashboard-api; sleep 2; systemctl is-active dashboard-api

echo "== 3. funnel paths (gate + terminal) =="
sudo tailscale serve --bg --set-path=/api/gate  http://127.0.0.1:8795/api/gate  2>&1 | tail -1
sudo tailscale serve --bg --set-path=/terminal  http://127.0.0.1:8795/terminal  2>&1 | tail -1
sudo tailscale funnel --bg 443 2>&1 | tail -1   # re-assert funnel (serve --set-path can demote it)

echo "== 4. verify =="
echo -n "gate verify (no token -> 401): "; curl -s -o /dev/null -w "%{http_code}\n" http://127.0.0.1:8795/api/gate/verify
CODE=$(/home/server/gorcave-gate/.venv/bin/python -c "import json,pyotp;print(pyotp.TOTP(json.load(open('/home/server/gorcave-gate/config.json'))['totp_secret']).now())")
echo "live code now: $CODE  (use with your passphrase to log in)"
echo "funnel gate:  "; curl -s -o /dev/null -w "  https .../api/gate/verify -> %{http_code} (401 = up, gated)\n" https://gorcave.YOUR-TAILNET.ts.net/api/gate/verify
echo "funnel term:  "; curl -s -o /dev/null -w "  https .../terminal -> %{http_code} (200 = up)\n" https://gorcave.YOUR-TAILNET.ts.net/terminal
echo "lights now gated: "; curl -s -o /dev/null -w "  .../lights/state no-token -> %{http_code} (401 = locked)\n" https://gorcave.YOUR-TAILNET.ts.net/api/dash/lights/state
echo "== done =="
