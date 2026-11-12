#!/usr/bin/env python3
"""
GOR Lab Control Panel  (PORTABLE / remote edition)
==================================================
Runs on YOUR Mac (or any machine) and drives the machine-lab robotic-instrument
server on the remote Mac mini ("the mini") entirely over SSH.

This is the portable sibling of ~/Desktop/GOR_STYLE/control_panel.py which only
runs locally ON the mini.  Same UI, same detection logic, same server jobs --
the difference is that every probe and every server command is executed THROUGH
SSH to the mini instead of via local subprocess.

Three stacked sections:
  (A) Server control  -- big Start/Stop toggle (mirrors ~/Desktop/"RUN SERVER 2")
  (B) Device status   -- green/red grid for the six instruments
  (C) Live OSC monitor-- scrolling decoded OSC feed (remote UDP listener, streamed)

Plus a top bar to set / auto-resolve the mini's host or IP.

Stdlib only: tkinter, socket-free (remote does OSC), struct, subprocess, threading,
queue, time, shutil.  All SSH work happens off the Tk main thread; a queue is
drained by root.after().  Every SSH call is wrapped in try/except and uses a
short ConnectTimeout so a flaky link never crashes the UI.
"""

import os
import time
import queue
import shutil
import threading
import subprocess
import tkinter as tk
from tkinter import font as tkfont

# --------------------------------------------------------------------------
# SSH / remote configuration
# --------------------------------------------------------------------------
SSH_USER = "mtiid"
SSH_PASS = "blackberry"
DEFAULT_HOST = "192.168.0.103"       # Ethernet; Wi-Fi fallback 192.168.0.124
BONJOUR_HOST = "MTIIDs-Mac-mini.local"
CANDIDATE_IPS = ["192.168.0.103", "192.168.0.124"]
CONNECT_TIMEOUT = 8                  # ssh -o ConnectTimeout

REMOTE_CHUCK = "/opt/homebrew/bin/chuck"
REMOTE_PY = "/usr/bin/python3"       # python on the mini (3.9.x) -- for OSC listener
MLC = "~/git/machineLabCode"         # remote machineLabCode root

# The three server components launched by "RUN SERVER 2" (backgrounded via nohup)
SERVER_JOBS = [
    {"name": "serial-robot-server",
     "cwd": MLC + "/serial-robot-server",
     "cmd": "master.ck --port:8888",
     "log": "/tmp/chuck1.log",
     "match": "serial-robot-server/master.ck"},
    {"name": "hiduino-robot-server",
     "cwd": MLC + "/hiduino-robot-server",
     "cmd": "master.ck --port:8889",
     "log": "/tmp/chuck2.log",
     "match": "hiduino-robot-server/master.ck"},
    {"name": "OSC-send-receive",
     "cwd": MLC + "/signalSendClasses/OSC",
     "cmd": "serverOpenOSCSendNRecieve.ck",
     "log": "/tmp/chuck3.log",
     "match": "serverOpenOSCSendNRecieve.ck"},
]

# Instrument detection signatures (identical logic to the local app).
MODULETTES_SERIAL = ""               # pin once known; empty = "any Teensy that isn't Tammy"
TAMMY_SERIAL = "15109820"

INSTRUMENTS = [
    {"label": "BreakBot",          "kind": "name",  "sig": "KarmetiK_BreakBot"},
    {"label": "GanaPati",          "kind": "name",  "sig": "KarmetiK_GanaPati"},
    {"label": "Tammy",             "kind": "teensy_serial", "sig": TAMMY_SERIAL},
    {"label": "Modulettes",        "kind": "teensy_serial", "sig": MODULETTES_SERIAL},
    {"label": "Marimbot",          "kind": "board", "sig": "Arduino Mega 2560"},
    {"label": "Lydia (Trimpbeat)", "kind": "board", "sig": "Generic CDC"},
]

# OSC ports (on the mini)
PORT_MONITOR_TAP = 7001   # server re-broadcasts every received msg here (server RUNNING)
PORT_FRONT_DOOR = 8001    # true front door (bind directly when server STOPPED)

POLL_INTERVAL = 2.0
MAX_LOG_LINES = 500

# --------------------------------------------------------------------------
# Colors / style  (same palette as the local app)
# --------------------------------------------------------------------------
BG = "#0e0f13"
PANEL = "#171a21"
FG = "#e6e6e6"
FG_DIM = "#8a8f99"
GREEN = "#22c55e"
GREEN_DK = "#0f3d24"
RED = "#ef4444"
RED_DK = "#3d1414"
ACCENT = "#3b82f6"
WARN = "#f59e0b"


# --------------------------------------------------------------------------
# Shared host config (thread-safe getter/setter)
# --------------------------------------------------------------------------
_cfg_lock = threading.Lock()
_host = DEFAULT_HOST


def get_host():
    with _cfg_lock:
        return _host


def set_host(h):
    global _host
    with _cfg_lock:
        _host = h.strip()


# --------------------------------------------------------------------------
# SSH plumbing
# --------------------------------------------------------------------------
def _sshpass_bin():
    return shutil.which("sshpass") or "/opt/homebrew/bin/sshpass"


def _ssh_available():
    """Return (ok, message).  False if sshpass or ssh is missing."""
    if not shutil.which("ssh"):
        return False, "ssh not found on this Mac"
    if not (shutil.which("sshpass") or os.path.exists("/opt/homebrew/bin/sshpass")):
        return False, "sshpass not found (brew install sshpass)"
    return True, ""


def _ssh_argv(host, remote_cmd):
    return [
        _sshpass_bin(), "-p", SSH_PASS,
        "ssh",
        "-o", "StrictHostKeyChecking=no",
        "-o", "UserKnownHostsFile=/dev/null",
        "-o", "ConnectTimeout=%d" % CONNECT_TIMEOUT,
        "-o", "ServerAliveInterval=5",
        "-o", "ServerAliveCountMax=2",
        "-o", "LogLevel=ERROR",
        "%s@%s" % (SSH_USER, host),
        remote_cmd,
    ]


def ssh_script(host, script, timeout=CONNECT_TIMEOUT + 8):
    """Run a bash script on the mini via `bash -s` (script fed on stdin).

    Returns (rc, stdout_text).  Never raises -- on any failure returns
    (-1, "").  Feeding the script on stdin avoids all shell-quoting hell.
    """
    try:
        argv = _ssh_argv(host, "bash -s")
        p = subprocess.run(
            argv, input=script.encode("utf-8"),
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            timeout=timeout)
        return p.returncode, p.stdout.decode("utf-8", "replace")
    except subprocess.TimeoutExpired:
        return -1, ""
    except Exception:
        return -1, ""


# --------------------------------------------------------------------------
# Combined remote probe -- one SSH round-trip returns everything we need
# --------------------------------------------------------------------------
PROBE_SCRIPT = r"""
set +e
echo '<<<PGREP>>>'
pgrep -fl chuck 2>/dev/null
echo '<<<IOREG>>>'
ioreg -p IOUSB -l -w 0 2>/dev/null
echo '<<<SERIAL>>>'
ls /dev/cu.usbmodem* 2>/dev/null
echo '<<<MIDI>>>'
# bound --probe so a stalled probe can never accumulate/orphan (macOS has no `timeout`)
/opt/homebrew/bin/chuck --probe 2>&1 & __pp=$!
( sleep 6; kill $__pp 2>/dev/null ) & __kk=$!
wait $__pp 2>/dev/null
kill $__kk 2>/dev/null
echo '<<<IFCONFIG>>>'
ifconfig 2>/dev/null
echo '<<<END>>>'
"""


def _split_sections(text):
    sections = {}
    cur = None
    buf = []
    for line in text.splitlines():
        if line.startswith("<<<") and line.endswith(">>>"):
            if cur is not None:
                sections[cur] = buf
            cur = line[3:-3]
            buf = []
        else:
            buf.append(line)
    if cur is not None:
        sections[cur] = buf
    return sections


def _parse_usb_names(ioreg_lines):
    names = []
    for line in ioreg_lines:
        if '"USB Product Name"' in line:
            parts = line.split("=", 1)
            if len(parts) == 2:
                names.append(parts[1].strip().strip('"'))
    return names


def _parse_teensy_serials(ioreg_lines):
    """USB serials for devices whose product name is 'Teensy MIDI'."""
    serials = set()
    last_product = None
    for line in ioreg_lines:
        if '"USB Product Name"' in line:
            parts = line.split("=", 1)
            if len(parts) == 2:
                last_product = parts[1].strip().strip('"').strip()
        elif '"USB Serial Number"' in line:
            parts = line.split("=", 1)
            if len(parts) == 2 and last_product == "Teensy MIDI":
                serials.add(parts[1].strip().strip('"').strip())
    return serials


def _parse_midi_outputs(midi_lines):
    names = []
    grab = False
    for line in midi_lines:
        if "MIDI outputs" in line:
            grab = True
            continue
        if grab:
            if "MIDI inputs" in line or line.strip() == "":
                if "MIDI inputs" in line:
                    break
                continue
            names.append(line.strip())
    return names


def _parse_ips(ifc_lines):
    ips = set()
    for line in ifc_lines:
        line = line.strip()
        if line.startswith("inet ") and "127.0.0.1" not in line:
            parts = line.split()
            if len(parts) >= 2:
                ips.add(parts[1])
    return sorted(ips)


def probe_all(host):
    """One SSH round-trip.  Returns (ok, devices, details, server_running, ips)."""
    rc, text = ssh_script(host, PROBE_SCRIPT)
    if rc != 0 or "<<<END>>>" not in text:
        return False, {}, {}, False, []
    sec = _split_sections(text)

    pgrep = "\n".join(sec.get("PGREP", []))
    ioreg = sec.get("IOREG", [])
    serial_ports = [l.strip() for l in sec.get("SERIAL", []) if l.strip()]
    midi_lines = sec.get("MIDI", [])
    ifc = sec.get("IFCONFIG", [])

    usb_names = _parse_usb_names(ioreg)
    teensy_serials = _parse_teensy_serials(ioreg)
    midi_names = _parse_midi_outputs(midi_lines)
    ips = _parse_ips(ifc)

    name_blob = "\n".join(usb_names + midi_names)
    has_serial = len(serial_ports) > 0

    devices = {}
    for inst in INSTRUMENTS:
        sig = inst["sig"]
        if inst["kind"] == "name":
            connected = sig.lower() in name_blob.lower()
        elif inst["kind"] == "teensy_serial":
            if inst["label"] == "Tammy":
                connected = TAMMY_SERIAL in teensy_serials
            elif inst["label"] == "Modulettes":
                if MODULETTES_SERIAL:
                    connected = MODULETTES_SERIAL in teensy_serials
                else:
                    # any Teensy that is NOT Tammy -> guarantees mutual exclusivity
                    connected = any(sn != TAMMY_SERIAL for sn in teensy_serials)
            else:
                connected = sig != "" and sig in teensy_serials
        else:  # board -- match USB product name and require a serial port
            connected = (sig.lower() in name_blob.lower()) and has_serial
        devices[inst["label"]] = connected

    server_running = any(job["match"] in pgrep for job in SERVER_JOBS)
    details = {
        "usb": usb_names,
        "serial": serial_ports,
        "midi": midi_names,
        "teensy_serials": sorted(teensy_serials),
    }
    return True, devices, details, server_running, ips


# --------------------------------------------------------------------------
# Server start/stop over SSH  (nohup-backgrounded so they survive the session)
# --------------------------------------------------------------------------
def _detach_cmd(cwd, argv, log):
    """A shell fragment that launches `argv` FULLY detached from the SSH session.

    macOS has no `setsid` binary, and plain `nohup ... &` (even with stdio
    redirected) still (a) gets SIGTERM when the ssh channel closes and (b) makes
    ChucK hold the ssh stdout pipe open, so the ssh command never returns.
    The fix is a proper double-fork daemonize inside python: the direct child of
    the shell exits immediately (ssh returns), the grandchild re-parents to init
    in a new session with ALL inherited fds closed and stdio pointed at /dev/null
    and the log, so nothing ChucK does can hold the ssh channel or take a SIGTERM.
    """
    pyargv = ", ".join("'%s'" % a for a in argv)
    py = "\n".join([
        "import os",
        "if os.fork() > 0: os._exit(0)",          # parent exits -> ssh returns
        "os.setsid()",                             # new session, no ctty
        "if os.fork() > 0: os._exit(0)",          # not a session leader
        "d = os.open('/dev/null', os.O_RDONLY)",
        "l = os.open('%s', os.O_WRONLY | os.O_CREAT | os.O_APPEND, 420)" % log,
        "os.dup2(d, 0); os.dup2(l, 1); os.dup2(l, 2)",
        "os.closerange(3, 256)",                   # drop any inherited ssh fds
        "os.chdir(os.path.expanduser('%s'))" % cwd,
        "os.execv('%s', [%s])" % (argv[0], pyargv),
    ])
    return "%s -c \"%s\"" % (REMOTE_PY, py)


def build_start_script():
    lines = ["set +e", "killall chuck 2>/dev/null", "sleep 0.4"]
    for job in SERVER_JOBS:
        argv = [REMOTE_CHUCK] + job["cmd"].split()
        lines.append(_detach_cmd(job["cwd"], argv, job["log"]))
    lines.append("sleep 0.6")
    lines.append("echo STARTED")
    return "\n".join(lines) + "\n"


def build_stop_script():
    return "killall chuck 2>/dev/null; echo STOPPED\n"


def server_start(host):
    return ssh_script(host, build_start_script())


def server_stop(host):
    return ssh_script(host, build_stop_script())


# --------------------------------------------------------------------------
# Host auto-resolve
# --------------------------------------------------------------------------
def resolve_host():
    """Try Bonjour (dns-sd), then ping-test .103/.124.  Returns ip or None."""
    # 1) dns-sd Bonjour lookup
    try:
        p = subprocess.Popen(
            ["dns-sd", "-G", "v4", BONJOUR_HOST],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        deadline = time.time() + 4
        while time.time() < deadline:
            line = p.stdout.readline()
            if not line:
                break
            line = line.decode("utf-8", "replace")
            if BONJOUR_HOST in line and "Add" in line:
                for tok in line.split():
                    if tok.count(".") == 3 and tok.replace(".", "").isdigit():
                        try:
                            p.terminate()
                        except Exception:
                            pass
                        return tok
        try:
            p.terminate()
        except Exception:
            pass
    except Exception:
        pass
    # 2) fall back to candidate IPs -- quick reachability probe
    for ip in CANDIDATE_IPS:
        try:
            r = subprocess.run(
                ["ping", "-c", "1", "-t", "2", ip],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                timeout=4)
            if r.returncode == 0:
                return ip
        except Exception:
            continue
    return None


# --------------------------------------------------------------------------
# Remote OSC listener source -- streamed over SSH; one decoded line per msg
# --------------------------------------------------------------------------
REMOTE_OSC_LISTENER = r'''
import sys, socket, struct
port = int(sys.argv[1]) if len(sys.argv) > 1 else 8001

def osc_string(data, i):
    end = data.find(b"\x00", i)
    if end == -1:
        raise ValueError("unterminated")
    s = data[i:end].decode("utf-8", "replace")
    nxt = end + 1
    pad = (4 - (nxt % 4)) % 4
    return s, nxt + pad

def decode(data):
    out = []
    if data[:8] == b"#bundle\x00":
        i = 16
        while i + 4 <= len(data):
            (size,) = struct.unpack_from(">i", data, i); i += 4
            if size <= 0 or i + size > len(data): break
            out.extend(decode(data[i:i+size])); i += size
        return out
    try:
        addr, i = osc_string(data, 0)
        if i >= len(data) or data[i:i+1] != b",":
            return [(addr, [])]
        tt, i = osc_string(data, i)
        args = []
        for t in tt[1:]:
            if t == "i":
                (v,) = struct.unpack_from(">i", data, i); i += 4; args.append(v)
            elif t == "f":
                (v,) = struct.unpack_from(">f", data, i); i += 4; args.append(round(v, 4))
            elif t == "s":
                v, i = osc_string(data, i); args.append(v)
            elif t in ("T", "F", "N", "I"):
                args.append({"T": True, "F": False, "N": None, "I": "inf"}[t])
            else:
                args.append("<%s?>" % t); break
        out.append((addr, args))
    except Exception as e:
        out.append(("<decode-error>", [str(e)]))
    return out

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
try:
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
except Exception:
    pass
try:
    s.bind(("0.0.0.0", port))
except Exception as e:
    sys.stderr.write("BINDFAIL %d %s\n" % (port, e)); sys.stderr.flush()
    sys.exit(1)
sys.stderr.write("BOUND %d\n" % port); sys.stderr.flush()
while True:
    try:
        data, addr = s.recvfrom(65535)
    except Exception:
        break
    src = "%s:%d" % (addr[0], addr[1])
    for a, args in decode(data):
        try:
            print(src + "\t" + a + "\t" + ("[" + ", ".join(str(x) for x in args) + "]"), flush=True)
        except Exception:
            pass
'''


# --------------------------------------------------------------------------
# OSC monitor thread -- streams remote listener stdout; rebinds on state change
# --------------------------------------------------------------------------
class OSCMonitor(threading.Thread):
    def __init__(self, out_queue):
        super().__init__(daemon=True)
        self.q = out_queue
        self._lock = threading.Lock()
        self._desired_port = None
        self._stop = threading.Event()
        self.proc = None
        self.active_port = None

    def set_port(self, port):
        with self._lock:
            self._desired_port = port

    def stop(self):
        self._stop.set()
        self._kill_proc()

    def _kill_proc(self):
        p = self.proc
        self.proc = None
        if p is not None:
            try:
                p.terminate()
            except Exception:
                pass
            try:
                p.kill()
            except Exception:
                pass

    def run(self):
        bound_port = None
        while not self._stop.is_set():
            with self._lock:
                want = self._desired_port
            if want is None:
                time.sleep(0.2)
                continue
            if want != bound_port or self.proc is None or self.proc.poll() is not None:
                self._kill_proc()
                bound_port = want
                try:
                    host = get_host()
                    argv = _ssh_argv(host, "%s -u - %d" % (REMOTE_PY, want))
                    self.proc = subprocess.Popen(
                        argv, stdin=subprocess.PIPE,
                        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
                    self.proc.stdin.write(REMOTE_OSC_LISTENER.encode("utf-8"))
                    self.proc.stdin.close()
                    self.active_port = want
                    self.q.put(("_osc_bind", want, None, None))
                except Exception as e:
                    self.active_port = None
                    self.q.put(("_osc_error", want, str(e), None))
                    self._kill_proc()
                    time.sleep(1.5)
                    continue
            # read one line (blocking, but proc dies on stop -> readline returns "")
            try:
                line = self.proc.stdout.readline()
            except Exception:
                line = b""
            if not line:
                # stream ended -> disconnected; retry
                if not self._stop.is_set():
                    self.q.put(("_osc_dropped", bound_port, None, None))
                self._kill_proc()
                self.active_port = None
                time.sleep(1.5)
                continue
            try:
                s = line.decode("utf-8", "replace").rstrip("\n")
                parts = s.split("\t", 2)
                if len(parts) == 3:
                    self.q.put(("osc", parts[0], parts[1], parts[2]))
            except Exception:
                pass
        self._kill_proc()


# --------------------------------------------------------------------------
# Poll thread -- one combined SSH probe per interval
# --------------------------------------------------------------------------
class PollThread(threading.Thread):
    def __init__(self, out_queue):
        super().__init__(daemon=True)
        self.q = out_queue
        self._stop = threading.Event()

    def stop(self):
        self._stop.set()

    def run(self):
        while not self._stop.is_set():
            try:
                host = get_host()
                ok, devices, details, running, ips = probe_all(host)
                if ok:
                    self.q.put(("link", True, None, None))
                    self.q.put(("server", running, None, None))
                    self.q.put(("devices", devices, details, None))
                    self.q.put(("ips", ips, None, None))
                else:
                    self.q.put(("link", False, None, None))
            except Exception as e:
                self.q.put(("_poll_error", str(e), None, None))
            for _ in range(int(POLL_INTERVAL * 5)):
                if self._stop.is_set():
                    break
                time.sleep(0.2)


# --------------------------------------------------------------------------
# Main application
# --------------------------------------------------------------------------
class ControlPanel:
    def __init__(self, root):
        self.root = root
        self.q = queue.Queue()
        self.server_running = False
        self.osc_mode_port = None
        self.link_ok = None
        self.busy = False               # a start/stop SSH op is in flight

        root.title("GOR  —  Lab Control Panel (remote)")
        root.configure(bg=BG)
        root.geometry("860x940")
        root.minsize(760, 720)

        self.mono = tkfont.Font(family="Menlo", size=12)
        self.mono_sm = tkfont.Font(family="Menlo", size=11)
        self.big = tkfont.Font(family="Helvetica Neue", size=20, weight="bold")
        self.hdr = tkfont.Font(family="Helvetica Neue", size=13, weight="bold")

        self._build_host_bar()
        self._build_server_section()
        self._build_device_section()
        self._build_osc_section()

        # SSH availability gate
        ok, msg = _ssh_available()
        if not ok:
            self._log("[FATAL] %s -- app will not be able to reach the mini." % msg)
            self.server_sub.config(text="SSH unavailable: %s" % msg, fg=RED)

        self.poll = PollThread(self.q)
        self.poll.start()
        self.monitor = OSCMonitor(self.q)
        self.monitor.start()

        root.protocol("WM_DELETE_WINDOW", self.on_close)
        self.root.after(120, self._drain_queue)

    # ---- top bar: host / resolve ------------------------------------------
    def _build_host_bar(self):
        bar = tk.Frame(self.root, bg=PANEL, padx=16, pady=10)
        bar.pack(fill="x", padx=12, pady=(12, 0))

        tk.Label(bar, text="MINI HOST", bg=PANEL, fg=FG_DIM,
                 font=self.mono_sm).pack(side="left")
        self.host_var = tk.StringVar(value=get_host())
        self.host_entry = tk.Entry(bar, textvariable=self.host_var, width=20,
                                   font=self.mono, bg="#0a0b0e", fg=FG,
                                   insertbackground=FG, relief="flat")
        self.host_entry.pack(side="left", padx=(8, 8))
        self.host_entry.bind("<Return>", lambda e: self._apply_host())

        tk.Button(bar, text="Set", command=self._apply_host,
                  font=self.mono_sm, bg="#22303f", fg=FG, relief="flat",
                  activebackground=ACCENT, cursor="hand2").pack(side="left")
        tk.Button(bar, text="Auto-resolve", command=self._auto_resolve,
                  font=self.mono_sm, bg="#22303f", fg=FG, relief="flat",
                  activebackground=ACCENT, cursor="hand2").pack(side="left",
                                                                padx=(8, 0))
        self.link_lbl = tk.Label(bar, text="link: …", bg=PANEL, fg=FG_DIM,
                                 font=self.mono_sm, anchor="e")
        self.link_lbl.pack(side="right")

    def _apply_host(self):
        h = self.host_var.get().strip()
        if h:
            set_host(h)
            self._log("[host] now targeting %s" % h)
            self.link_lbl.config(text="link: probing…", fg=WARN)

    def _auto_resolve(self):
        self._log("[resolve] looking for the mini…")
        self.link_lbl.config(text="link: resolving…", fg=WARN)

        def work():
            ip = resolve_host()
            self.q.put(("_resolved", ip, None, None))
        threading.Thread(target=work, daemon=True).start()

    # ---- section A: server -------------------------------------------------
    def _build_server_section(self):
        frame = tk.Frame(self.root, bg=PANEL, padx=16, pady=14)
        frame.pack(fill="x", padx=12, pady=(8, 6))

        self.server_lbl = tk.Label(frame, text="Lab Server: —",
                                   bg=PANEL, fg=FG, font=self.big, anchor="w")
        self.server_lbl.pack(fill="x")

        self.server_btn = tk.Button(
            frame, text="START", command=self.toggle_server,
            font=self.big, bg=GREEN_DK, fg=FG, activebackground=GREEN,
            activeforeground="#000000", relief="flat", height=2, bd=0,
            highlightthickness=0, cursor="hand2")
        self.server_btn.pack(fill="x", pady=(10, 4))

        self.server_sub = tk.Label(
            frame, text="Detecting server state…",
            bg=PANEL, fg=FG_DIM, font=self.mono_sm, anchor="w", justify="left")
        self.server_sub.pack(fill="x")

    # ---- section B: devices ------------------------------------------------
    def _build_device_section(self):
        frame = tk.Frame(self.root, bg=PANEL, padx=16, pady=12)
        frame.pack(fill="x", padx=12, pady=6)

        tk.Label(frame, text="INSTRUMENTS", bg=PANEL, fg=FG,
                 font=self.hdr, anchor="w").pack(fill="x")

        grid = tk.Frame(frame, bg=PANEL)
        grid.pack(fill="x", pady=(8, 4))
        self.dev_cells = {}
        cols = 3
        for idx, inst in enumerate(INSTRUMENTS):
            r, c = divmod(idx, cols)
            cell = tk.Label(grid, text=inst["label"], bg=RED_DK, fg=FG,
                            font=self.mono, width=16, height=3,
                            relief="flat", bd=0, highlightthickness=2,
                            highlightbackground=RED)
            cell.grid(row=r, column=c, padx=5, pady=5, sticky="nsew")
            self.dev_cells[inst["label"]] = cell
        for c in range(cols):
            grid.columnconfigure(c, weight=1)

        tk.Label(frame,
                 text="Marimbot / Lydia are matched by board type (Arduino "
                      "Mega 2560 / Generic CDC) since both are serial Arduinos;\n"
                      "exact identity needs the server handshake. Tammy/Modulettes "
                      "matched by Teensy serial so one Teensy never lights both.",
                 bg=PANEL, fg=FG_DIM, font=self.mono_sm,
                 anchor="w", justify="left").pack(fill="x", pady=(4, 0))

    # ---- section C: OSC ----------------------------------------------------
    def _build_osc_section(self):
        frame = tk.Frame(self.root, bg=PANEL, padx=16, pady=12)
        frame.pack(fill="both", expand=True, padx=12, pady=(6, 12))

        top = tk.Frame(frame, bg=PANEL)
        top.pack(fill="x")
        tk.Label(top, text="LIVE OSC MONITOR", bg=PANEL, fg=FG,
                 font=self.hdr, anchor="w").pack(side="left")
        self.osc_mode_lbl = tk.Label(top, text="listener: —",
                                     bg=PANEL, fg=ACCENT, font=self.mono_sm,
                                     anchor="e")
        self.osc_mode_lbl.pack(side="right")

        self.ip_lbl = tk.Label(frame, text="mini IP: …",
                               bg=PANEL, fg=FG, font=self.mono_sm,
                               anchor="w", justify="left")
        self.ip_lbl.pack(fill="x", pady=(4, 0))

        logwrap = tk.Frame(frame, bg=BG)
        logwrap.pack(fill="both", expand=True, pady=(8, 0))
        self.log = tk.Text(logwrap, bg="#0a0b0e", fg="#c8f7c8",
                           font=self.mono_sm, wrap="none", relief="flat",
                           bd=0, highlightthickness=0, state="disabled",
                           insertbackground=FG)
        yscroll = tk.Scrollbar(logwrap, command=self.log.yview)
        self.log.configure(yscrollcommand=yscroll.set)
        yscroll.pack(side="right", fill="y")
        self.log.pack(side="left", fill="both", expand=True)

        self.log_lines = 0
        self._log("[app started] portable/remote control panel — target %s"
                  % get_host())

    # ---- server control ----------------------------------------------------
    def toggle_server(self):
        if self.busy:
            return
        if self.server_running:
            self._do_server_op(server_stop, "STOP")
        else:
            self._do_server_op(server_start, "START")

    def _do_server_op(self, fn, label):
        self.busy = True
        self.server_btn.config(state="disabled")
        self._log("[server] %s requested over SSH…" % label)
        host = get_host()

        def work():
            rc, out = fn(host)
            self.q.put(("_server_op", label, rc, out))
        threading.Thread(target=work, daemon=True).start()

    def _set_server_state(self, running):
        self.server_running = running
        if running:
            self.server_lbl.config(text="Lab Server: RUNNING", fg=GREEN)
            self.server_btn.config(text="STOP", bg=RED_DK, activebackground=RED)
            want = PORT_MONITOR_TAP
        else:
            self.server_lbl.config(text="Lab Server: STOPPED", fg=RED)
            self.server_btn.config(text="START", bg=GREEN_DK, activebackground=GREEN)
            want = PORT_FRONT_DOOR
        if want != self.osc_mode_port:
            self.osc_mode_port = want
            self.monitor.set_port(want)
            self._update_osc_mode_label(want, pending=True)

    def _update_osc_mode_label(self, port, pending=False, error=None, dropped=False):
        if error:
            self.osc_mode_lbl.config(
                text="listener: bind %s FAILED (%s)" % (port, error), fg=RED)
            return
        if dropped:
            self.osc_mode_lbl.config(
                text="listener: monitor disconnected — retrying…", fg=WARN)
            return
        mode = "server monitor tap" if port == PORT_MONITOR_TAP else "front door (true src)"
        prefix = "connecting" if pending else "listening"
        self.osc_mode_lbl.config(
            text="listener: %s UDP %s  (%s)" % (prefix, port, mode), fg=ACCENT)

    # ---- device UI ---------------------------------------------------------
    def _update_devices(self, devices):
        for label, connected in devices.items():
            cell = self.dev_cells.get(label)
            if cell is None:
                continue
            if connected:
                cell.config(bg=GREEN_DK, highlightbackground=GREEN, fg="#d6ffd6")
            else:
                cell.config(bg=RED_DK, highlightbackground=RED, fg="#ffd6d6")

    def _dim_devices(self):
        for cell in self.dev_cells.values():
            cell.config(bg="#1a1a1a", highlightbackground=FG_DIM, fg=FG_DIM)

    def _update_ips(self, ips):
        if ips:
            self.ip_lbl.config(text="mini IP: %s" % "   ".join(ips))
        else:
            self.ip_lbl.config(text="mini IP: (none detected)")

    def _update_link(self, ok):
        if ok == self.link_ok:
            return
        self.link_ok = ok
        if ok:
            self.link_lbl.config(text="link: connected (%s)" % get_host(), fg=GREEN)
            self.server_sub.config(text="Connected to the mini over SSH.", fg=FG_DIM)
        else:
            self.link_lbl.config(text="link: UNREACHABLE", fg=RED)
            self.server_sub.config(
                text="Cannot reach %s over SSH. Check the IP / Auto-resolve, "
                     "and that the mini is on." % get_host(), fg=RED)
            self._dim_devices()

    # ---- logging -----------------------------------------------------------
    def _log(self, text):
        self.log.config(state="normal")
        self.log.insert("end", text + "\n")
        self.log_lines += 1
        if self.log_lines > MAX_LOG_LINES:
            self.log.delete("1.0", "100.0")
            self.log_lines -= 100
        self.log.see("end")
        self.log.config(state="disabled")

    def _log_osc(self, src, addr, args):
        t = time.strftime("%H:%M:%S")
        self._log("%s  %-21s  %s %s" % (t, src, addr, args))

    # ---- queue drain (main thread) ----------------------------------------
    def _drain_queue(self):
        try:
            while True:
                kind, a, b, c = self.q.get_nowait()
                if kind == "osc":
                    self._log_osc(a, b, c)
                elif kind == "link":
                    self._update_link(bool(a))
                elif kind == "server":
                    running = bool(a)
                    if running != self.server_running and not self.busy:
                        self._set_server_state(running)
                elif kind == "devices":
                    self._update_devices(a)
                elif kind == "ips":
                    self._update_ips(a)
                elif kind == "_resolved":
                    if a:
                        self.host_var.set(a)
                        set_host(a)
                        self._log("[resolve] found mini at %s" % a)
                        self.link_lbl.config(text="link: probing %s…" % a, fg=WARN)
                    else:
                        self._log("[resolve] could not find the mini "
                                  "(tried Bonjour + %s)" % ", ".join(CANDIDATE_IPS))
                        self.link_lbl.config(text="link: not found", fg=RED)
                elif kind == "_server_op":
                    label, rc, out = a, b, c
                    self.busy = False
                    self.server_btn.config(state="normal")
                    if rc == 0:
                        self._log("[server] %s ok" % label)
                        self._set_server_state(label == "START")
                    else:
                        self._log("[server] %s FAILED (rc=%s) — check link/host"
                                  % (label, rc))
                elif kind == "_osc_bind":
                    self._update_osc_mode_label(a, pending=False)
                    self._log("[osc] remote listener bound UDP %d" % a)
                elif kind == "_osc_error":
                    self._update_osc_mode_label(a, error=b)
                    self._log("[osc error] UDP %s: %s" % (a, b))
                elif kind == "_osc_dropped":
                    self._update_osc_mode_label(a, dropped=True)
                    self._log("[osc] monitor disconnected (UDP %s) — retrying" % a)
                elif kind == "_poll_error":
                    self._log("[poll error] %s" % a)
        except queue.Empty:
            pass
        self.root.after(120, self._drain_queue)

    # ---- shutdown ----------------------------------------------------------
    def on_close(self):
        # Closing the panel must NOT take down a running lab server.
        try:
            self.poll.stop()
            self.monitor.stop()
        except Exception:
            pass
        self.root.destroy()


def _raise_to_front(root):
    try:
        root.lift()
        root.attributes("-topmost", True)
        root.after(700, lambda: root.attributes("-topmost", False))
        root.focus_force()
    except Exception:
        pass
    try:
        subprocess.Popen(
            ["osascript", "-e",
             'tell application "System Events" to set frontmost of '
             '(first process whose unix id is %d) to true' % os.getpid()],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except Exception:
        pass


def main():
    root = tk.Tk()
    ControlPanel(root)
    root.after(200, lambda: _raise_to_front(root))
    root.mainloop()


if __name__ == "__main__":
    main()
