# Demiurge Web UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** A Pi-hosted daemon (`demiurge-web.service`) that publishes `~/.demiurge/status` and serves a browser mirror of the demiurge companion TUI at `http://demiurge.local:8080`, with patch/power/audio controls.

**Architecture:** One stdlib-only Python file with three layers: pure parser functions (unit-testable, no I/O), a 2s collector thread that probes locally and writes the status file atomically, and a `ThreadingHTTPServer` serving static files, `GET /api/status`, an SSE stream, and three POST action endpoints that shell out to existing commands. Frontend is vanilla HTML/JS/CSS, no build step.

**Tech Stack:** Python 3.11 stdlib only (Pi Bookworm); pytest (Mac-side dev venv only, never on the Pi); vanilla JS + canvas.

## Global Constraints (from spec — verbatim requirements)

- Scope: DEMIURGE system layer ONLY. No NEPTR effect state, ever.
- Mirror `demiurge/docs/companion.md`: same fields, names, bar semantics, thresholds (temp warn 70 / crit 80 / max 85; PWR: ≤1.6 GHz LOW, ≤2.4 GHz MED, else HIGH), patch list 1–9 with `▸` active + power tags, `nam` hidden.
- Stdlib only on the Pi. No Flask, no pip deps, no build step, no CDN assets.
- Service: `User=gorpi7`, `CPUAffinity=0-2`, `Nice=10`, `WantedBy=multi-user.target`, NOT `PartOf=demiurge.service`.
- Controls shell out to `demiurge-set <name>`, the Pi-side power helper (user-owned — invoke, never reimplement; confirm exact name on the Pi before Task 6 deploy), `sudo systemctl start|stop|restart demiurge`. Stop/restart = two-step arm in the UI.
- Status file: `~/.demiurge/status`, JSON, atomic write (tmp + rename), every 2 s.
- LAN-open, port 8080, bind all interfaces.
- Actions serialized, logged to `~/.demiurge/logs/web-actions.log`, each triggers an immediate frame.
- Probe failure → field `null`, frame still ships.
- Dev on Mac uses `--mock` mode with fixture probe outputs. Never pip install outside a venv (user rule).

## File Structure

- `web/demiurge_web.py` — the daemon (parsers at top, collector, HTTP server, `main()` with `--port`, `--mock`, `--once` flags).
- `web/static/index.html`, `web/static/app.js`, `web/static/style.css` — frontend.
- `web/tests/test_parsers.py`, `web/tests/test_server.py` — pytest.
- `web/tests/fixtures/` — canned probe outputs (`vcgencmd_*.txt`, `live.conf`, `sets/*.conf`, `meminfo.txt`, `loadavg.txt`, `stat.txt`).
- `config/demiurge-web.service` — systemd unit.
- `setup-script/setup-demiurge.sh` — Modify: append install step.

---

### Task 0: Version control for DEMIURGE_OS

DEMIURGE_OS is not a git repo; the plan's commit discipline needs one. **Ask the user first** (they were offered `git init` and haven't confirmed). If declined, skip all commit steps in later tasks.

- [x] **Step 1: Confirm with user, then init**

```bash
cd "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/DEMIURGE_OS"
git init
printf '%s\n' '.DS_Store' '__pycache__/' '*.pyc' '.venv/' > .gitignore
git add -A && git commit -m "chore: baseline snapshot of DEMIURGE_OS before web UI"
```

- [x] **Step 2: Dev venv for tests (Mac only)**

```bash
cd web 2>/dev/null || mkdir -p web && cd web
python3 -m venv .venv && .venv/bin/pip install -q pytest
```

### Task 1: Pure parsers + fixtures

**Files:**
- Create: `web/demiurge_web.py` (parsers section), `web/tests/test_parsers.py`, `web/tests/fixtures/*`

**Interfaces (Produces):**
- `parse_throttled(s) -> {"hex": str, "now": [str], "past": [str]} | None`
- `derive_power(max_khz: int) -> "LOW"|"MED"|"HIGH"`
- `parse_temp(s) -> float|None` (from `temp=57.4'C`)
- `parse_meminfo(text) -> {"total_kb": int, "avail_kb": int}`
- `parse_loadavg(text, ncores) -> {"load1": float, "pct": float}`
- `parse_liveconf(text) -> {"set": str|None, "power": str|None, "chains": [[str,...]], "has_input": bool}`
- `parse_sets(files: dict[str,str]) -> [{"file": str, "set": str, "power": str|None}]` (sorted, `nam` excluded)
- `percore_cpu(prev_stat, cur_stat) -> [float,...]` (from two `/proc/stat` snapshots)

- [x] **Step 1: Create fixtures.** Copy real inputs where available: `cp demiurge/live.conf web/tests/fixtures/live.conf`; create `web/tests/fixtures/throttled_0x50005.txt` containing `throttled=0x50005`, `temp.txt` containing `temp=57.4'C`, `meminfo.txt`/`loadavg.txt`/`stat.txt` captured from any Linux box or hand-written in procfs format, and two set files `sets/neptr.conf` (`# set: neptr` + `# power: high` headers) and `sets/nam.conf`.

- [x] **Step 2: Write failing tests** (`web/tests/test_parsers.py`):

```python
import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from demiurge_web import (parse_throttled, derive_power, parse_temp,
                          parse_meminfo, parse_loadavg, parse_liveconf, parse_sets)
FX = pathlib.Path(__file__).parent / "fixtures"

def test_throttled_decode_now_and_past():
    t = parse_throttled((FX / "throttled_0x50005.txt").read_text())
    assert t["hex"] == "0x50005"
    assert t["now"] == ["UNDERVOLT", "THROTTLE"]          # bits 0,2
    assert set(t["past"]) == {"UNDERVOLT", "THROTTLE"}    # bits 16,18

def test_throttled_clean():
    assert parse_throttled("throttled=0x0") == {"hex": "0x0", "now": [], "past": []}

def test_power_thresholds():
    assert derive_power(1_500_000) == "LOW"
    assert derive_power(2_400_000) == "MED"
    assert derive_power(2_800_000) == "HIGH"

def test_temp():
    assert parse_temp("temp=57.4'C") == 57.4

def test_liveconf_real_file():
    lc = parse_liveconf((FX / "live.conf").read_text())
    assert lc["set"] is not None
    assert lc["chains"], "chain block must parse to at least one row"

def test_sets_sorted_and_nam_hidden():
    files = {"neptr.conf": (FX / "sets/neptr.conf").read_text(),
             "nam.conf":   (FX / "sets/nam.conf").read_text()}
    out = parse_sets(files)
    assert [s["set"] for s in out] == ["neptr"] and out[0]["power"] == "high"
```

- [x] **Step 3: Run to verify fail:** `web/.venv/bin/pytest web/tests/test_parsers.py -q` → import error (module empty/missing).

- [x] **Step 4: Implement the parsers section of `web/demiurge_web.py`:**

```python
#!/usr/bin/env python3
"""demiurge-web: status daemon + browser mirror of the demiurge companion TUI.
Stdlib only. Layers: parsers (pure) / collector (2s thread) / HTTP server."""
import json, os, re, glob, time, queue, argparse, threading, subprocess
from collections import deque
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from pathlib import Path

THROTTLE_BITS = [(0, "UNDERVOLT"), (1, "ARM-CAP"), (2, "THROTTLE"), (3, "TEMP-LIM")]

def parse_throttled(s):
    m = re.search(r"0x([0-9a-fA-F]+)", s or "")
    if not m:
        return None
    v = int(m.group(1), 16)
    return {"hex": "0x%x" % v,
            "now":  [n for b, n in THROTTLE_BITS if v >> b & 1],
            "past": [n for b, n in THROTTLE_BITS if v >> (b + 16) & 1]}

def derive_power(max_khz):
    mhz = max_khz / 1000.0
    return "LOW" if mhz <= 1600 else ("MED" if mhz <= 2400 else "HIGH")

def parse_temp(s):
    m = re.search(r"temp=([\d.]+)", s or "")
    return float(m.group(1)) if m else None

def parse_meminfo(text):
    d = dict(re.findall(r"^(\w+):\s+(\d+)", text, re.M))
    return {"total_kb": int(d["MemTotal"]), "avail_kb": int(d["MemAvailable"])}

def parse_loadavg(text, ncores):
    load1 = float(text.split()[0])
    return {"load1": load1, "pct": min(100.0, 100.0 * load1 / max(1, ncores))}

def parse_liveconf(text):
    out = {"set": None, "power": None, "chains": [], "has_input": False}
    cur = None
    for line in text.splitlines():
        s = line.strip()
        if s.startswith("# set:"):
            out["set"] = s.split(":", 1)[1].strip()
        elif s.startswith("# power:"):
            out["power"] = s.split(":", 1)[1].strip()
        elif re.match(r"^chain\s*=", line):
            cur = []
            out["chains"].append(cur)
            rest = line.split("=", 1)[1].strip()
            if rest:
                cur.append(rest)
        elif cur is not None and line[:1] in (" ", "\t") and s and not s.startswith("#"):
            if re.match(r"^in\d+\s*->", s):
                out["has_input"] = True
                s = s.split("->", 1)[1].strip()
            if s and s != "out":
                base = os.path.basename(s)
                cur.append(re.sub(r"\.(csd|ck|pd|scd|dsp|cpp|strudel|py|rnbo)$",
                                  "", base))
        elif cur is not None and line[:1] not in (" ", "\t"):
            cur = None
    out["chains"] = [c for c in out["chains"] if c]
    return out

def parse_sets(files):
    out = []
    for fname in sorted(files):
        stem = fname.rsplit(".", 1)[0]
        if stem == "nam":
            continue
        text = files[fname]
        mset = re.search(r"^#\s*set:\s*(.+)$", text, re.M)
        mpow = re.search(r"^#\s*power:\s*(.+)$", text, re.M)
        out.append({"file": fname,
                    "set": (mset.group(1).strip() if mset else stem),
                    "power": (mpow.group(1).strip() if mpow else None)})
    return out

def percore_cpu(prev_stat, cur_stat):
    def rows(t):
        return {l.split()[0]: [int(x) for x in l.split()[1:]]
                for l in t.splitlines() if re.match(r"^cpu\d+ ", l)}
    p, c = rows(prev_stat), rows(cur_stat)
    pcts = []
    for k in sorted(c, key=lambda s: int(s[3:])):
        if k not in p:
            continue
        dt = sum(c[k]) - sum(p[k])
        didle = (c[k][3] + c[k][4]) - (p[k][3] + p[k][4])
        pcts.append(0.0 if dt <= 0 else round(100.0 * (dt - didle) / dt, 1))
    return pcts
```

- [x] **Step 5: Run to verify pass:** `web/.venv/bin/pytest web/tests/test_parsers.py -q` → all pass. If `test_liveconf_real_file` fails, fix the parser against the real fixture (the grammar reference is `demiurge/docs/config-reference.md`) — do not weaken the test.

- [x] **Step 6: Commit** — `git add web && git commit -m "feat(web): status parsers with fixture tests"`

### Task 2: Probe collector, mock mode, status file

**Files:** Modify `web/demiurge_web.py`; Create `web/tests/test_server.py` (collector tests)

**Interfaces (Produces):**
- `class Probes` with `.run(cmd_list) -> str|None` and `.read(path) -> str|None`; `MockProbes(fixture_dir)` subclass mapping known cmds/paths to fixture files.
- `collect(probes, prev) -> dict` — one status frame. Keys: `ts, host, uptime_s, temp_c, pmic_temp_c, freq{cur,min,max}, load{load1,pct}, mem{total_kb,avail_kb}, throttled{...}, volt_core, ext5v, cores_online, ncores, gov, audio, ui, pwr, patch, power_tag, chains, has_input, patches[], percore[], xrun_60s, throttle_60s`
- `write_status(frame, path)` — atomic tmp+rename.

- [x] **Step 1: Write failing tests** (collector via mocks):

```python
def test_collect_mock_frame(tmp_path):
    from demiurge_web import MockProbes, collect, write_status
    frame = collect(MockProbes(FX), prev=None)
    assert frame["pwr"] in ("LOW", "MED", "HIGH")
    assert frame["throttled"]["hex"].startswith("0x")
    assert isinstance(frame["patches"], list)
    write_status(frame, tmp_path / "status")
    import json
    assert json.loads((tmp_path / "status").read_text())["pwr"] == frame["pwr"]

def test_probe_failure_yields_null():
    from demiurge_web import MockProbes, collect
    p = MockProbes(FX); p.broken = {"vcgencmd measure_temp"}
    assert collect(p, prev=None)["temp_c"] is None
```

- [x] **Step 2: Run → FAIL.**

- [x] **Step 3: Implement.** `Probes.run` = `subprocess.run(cmd, capture_output=True, text=True, timeout=3)` returning stdout or `None` on any exception/nonzero. `Probes.read` = `Path.read_text()` or `None`. Real probe map (each wrapped so failure → `None` field):
  - temp `vcgencmd measure_temp`; pmic `vcgencmd measure_temp pmic`; throttled `vcgencmd get_throttled`; volt `vcgencmd measure_volts core`; ext5v `vcgencmd pmic_read_adc` (regex `EXT5V_V.*?([\d.]+)`)
  - freq/gov from sysfs `scaling_cur_freq`, `cpuinfo_min_freq`, `scaling_max_freq`, `scaling_governor`; cores from `/sys/devices/system/cpu/online` + `os.cpu_count()`
  - load `/proc/loadavg`, mem `/proc/meminfo`, uptime `/proc/uptime`, percore from consecutive `/proc/stat` (needs `prev`)
  - audio/ui: `systemctl is-active demiurge` / `neptr-ui` (stdout strip == "active")
  - patch/chains: `~/demiurge/live.conf` via `parse_liveconf`; patches: glob `~/demiurge/sets/*.conf` → `parse_sets`
  - xrun_60s: `journalctl -u demiurge --since -60s -o cat` lines matching `re.compile("xrun", re.I)` count (`None` on failure); throttle_60s: in-process deque of (ts, now-bits-nonempty) over 60 s — no `/dev/shm` files needed since the daemon is persistent.
  `MockProbes` maps each command string / path to a file in `fixtures/`; unknown or `broken` → `None`. `write_status` writes `path.tmp` then `os.replace`.

- [x] **Step 4: Run → PASS.** `web/.venv/bin/pytest web/tests -q`

- [x] **Step 5: Commit** — `git commit -am "feat(web): probe collector, mock mode, atomic status file"`

### Task 3: HTTP server — static, /api/status, SSE, history

**Files:** Modify `web/demiurge_web.py`; Modify `web/tests/test_server.py`

**Interfaces (Produces):**
- `App` class: `.history` (deque maxlen 300), `.subscribe() -> queue.Queue`, `.publish(frame)`; `make_server(app, port, static_dir) -> ThreadingHTTPServer`
- Routes: `GET /` + static files; `GET /api/status` (latest frame JSON); `GET /api/history` (JSON array, served once at page load); `GET /api/events` (SSE: `data: <frame-json>\n\n` per publish, heartbeat comment every 15 s); 404 otherwise.
- `main()`: `--port 8080 --mock <dir> --once --interval 2.0`. `--once` prints one frame JSON and exits (the `demiurge --once` twin). Collector loop thread: collect → write_status → publish.

- [x] **Step 1: Write failing tests** — start server on port 0 with `MockProbes`, then: `GET /api/status` returns JSON with `pwr` key; `GET /api/events` yields a `data:` line within 3 s after a `publish()`; `GET /` returns 200 text/html; dead SSE clients are pruned (subscribe, close socket, publish twice, assert subscriber count drops).

- [x] **Step 2: Run → FAIL.**

- [x] **Step 3: Implement.** `BaseHTTPRequestHandler` subclass with `do_GET`; SSE handler loops `q.get(timeout=15)` writing frames, sends `: ping\n\n` on timeout, removes queue from `app.subs` on `BrokenPipeError/ConnectionResetError`. Static serving restricted to `static_dir` (resolve + prefix check, no path traversal). JSON responses `Content-Type: application/json`.

- [x] **Step 4: Run → PASS**, then a manual smoke: `web/.venv/bin/python web/demiurge_web.py --mock web/tests/fixtures --once | python3 -m json.tool`.

- [x] **Step 5: Commit** — `git commit -am "feat(web): http server with SSE push and history"`

### Task 4: Control actions

**Files:** Modify `web/demiurge_web.py`; Modify `web/tests/test_server.py`

**Interfaces (Produces):**
- `Actions(runner, sets_provider, log_path)` with `.patch(name)`, `.power(level)`, `.audio(verb)` → `{"ok": bool, "cmd": str, "output": str}`; a single `threading.Lock` serializes all three.
- Routes: `POST /api/patch {"name": ...}` (400 unless name in current sets list), `POST /api/power {"level": "low|med|high"}`, `POST /api/audio {"verb": "start|stop|restart"}`. Each success triggers an immediate collect+publish.
- Commands: patch → `["demiurge-set", name]`; power → `[POWER_HELPER, level]` where `POWER_HELPER = os.environ.get("DEMIURGE_POWER_CMD", "demiurge-power")` (env override so the exact user-owned helper name is a config concern, not code); audio → `["sudo", "systemctl", verb, "demiurge"]`.
- Every call appends a timestamped line to `~/.demiurge/logs/web-actions.log` (mkdir parents).

- [x] **Step 1: Write failing tests** with a fake runner recording argv: patch validates against sets and runs `demiurge-set`; unknown patch → 400 and runner NOT called; audio stop runs sudo systemctl; action log file gains a line; concurrent calls serialize (fake runner sleeps, assert no overlap flag).

- [x] **Step 2: Run → FAIL.**  

- [x] **Step 3: Implement** (`do_POST` reads Content-Length, parses JSON, 400 on garbage; 500 with stderr text in body on nonzero exit).

- [x] **Step 4: Run full suite → PASS.**

- [x] **Step 5: Commit** — `git commit -am "feat(web): patch/power/audio actions, serialized and logged"`

### Task 5: Frontend — companion mirror

**Files:** Create `web/static/index.html`, `web/static/style.css`, `web/static/app.js`

Layout mirrors the companion top-to-bottom: header (host + uptime) → bars TEMP/FREQ/LOAD/MEM → THROTTLE hex + four decode chips (red=now, yellow=past) → VOLT · CORES · GOV row → status row `AUDIO ●` / `UI ✓` / `PWR` → `PATCH <name>` + signal row `IN ─▶ neptrPhase4 ─▶ OUT` → PATCHES grid (tap tiles, `▸` on active, power tag) → power segmented LOW/MED/HIGH → START / STOP / RESTART. Below: four canvas sparklines (temp, load, xrun/min, EXT5V) fed from `/api/history` + SSE. Disconnect banner on SSE `onerror` with auto-retry.

- [x] **Step 1: Write the three files.** Key contracts (keep exactly — tests and manual QA rely on them):
  - `app.js`: `const es = new EventSource("/api/events"); es.onmessage = e => render(JSON.parse(e.data));` initial paint from `fetch("/api/status")` + `fetch("/api/history")`.
  - `render(frame)` maps 1:1 from the Task 2 frame keys; bar fill % and color classes: temp `warn>=70 crit>=80` scale 0–85; freq scaled min→max; load/mem 0–100.
  - Actions: `post(path, body)` → on non-ok, toast with response text. STOP/RESTART use `armButton(btn)`: first tap adds `.armed` (red, label `CONFIRM STOP`), 3 s timeout disarms; second tap fires. Patch tiles use a one-tap confirm sheet ("Switch to <name> — restarts audio").
  - `style.css`: near-black `#0a0a0c` background, mono/system font stack, `--ok:#38d16a; --warn:#e8c341; --crit:#e0443e`, touch targets ≥48px, `@media (max-width:480px)` single column. No external assets.

- [x] **Step 2: Manual verify on the Mac:** `web/.venv/bin/python web/demiurge_web.py --mock web/tests/fixtures --port 8080` → open `http://localhost:8080`, check: all fields render, throttle chips show the 0x50005 fixture as UNDERVOLT+THROTTLE now and past, patch tile tap → confirm sheet → POST fires (fake runner logs in mock mode), STOP arms red then times out, SSE reconnect banner appears when the server is Ctrl-C'd.

- [x] **Step 3: Commit** — `git add web/static && git commit -m "feat(web): companion-mirror frontend"`

### Task 6: Service unit + installer + deploy

**Files:** Create `config/demiurge-web.service`; Modify `setup-script/setup-demiurge.sh` (append phase)

- [x] **Step 1: Unit file:**

```ini
[Unit]
Description=Demiurge web UI (status daemon + browser mirror)
After=network.target

[Service]
Type=simple
User=gorpi7
ExecStart=/usr/bin/python3 /opt/demiurge/web/demiurge_web.py --port 8080
Environment=DEMIURGE_POWER_CMD=demiurge-power
CPUAffinity=0-2
Nice=10
Restart=on-failure
RestartSec=3

[Install]
WantedBy=multi-user.target
```

- [x] **Step 2: Installer step** in `setup-demiurge.sh` (follow its existing phase pattern): copy `web/` → `/opt/demiurge/web/`, unit → `/etc/systemd/system/`, `daemon-reload`, `enable --now demiurge-web`.

- [ ] **Step 3: BLOCKED-ON-PI checklist (do not skip; do when Pi is on):** _(Pi was off on 2026-07-13 — still pending)_
  1. Confirm the Pi-side power helper's real name/path (`which demiurge-power` as gorpi7); set `DEMIURGE_POWER_CMD` in the unit accordingly. **Never modify the helper itself.**
  2. Deploy via the staging pattern (rsync `web/` + unit to `/tmp` on the Pi, then `mv`), enable service.
  3. Verify: `curl -s localhost:8080/api/status | python3 -m json.tool`; open from a phone; one patch-switch round-trip; `taskset -pc $(pgrep -f demiurge_web)` shows 0–2; `top` shows ~0% CPU; confirm `~/.demiurge/status` updates every 2 s; xrun/journalctl probe returns data (needs gorpi7 in `systemd-journal` group — add if not).

- [x] **Step 4: Commit** — `git add config setup-script && git commit -m "feat(web): systemd unit + installer step"`

---

## Self-Review Notes

- Spec coverage: all frame fields (Task 2 key list) match the spec's two tables; SSE/history/sparklines Task 3+5; controls+confirm Task 4+5; error handling (null fields, action stderr, SSE prune, parse-failure banner is the empty-patches case rendering a warning) Tasks 2–5; testing strategy = spec's unit/mock/on-Pi triple.
- Deviation from spec (deliberate): throttle/xrun 60 s windows use in-process deques instead of pi-monitor's `/dev/shm` files — the daemon is persistent so external history files are unnecessary; noted here per "no silent changes".
- Open item from spec carried into Task 6 Step 3: power-helper name confirmation. On-Pi verification is the only Pi-dependent step; Tasks 0–5 run entirely on the Mac.
