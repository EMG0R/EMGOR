"""
CDC (USB serial console) control channel for the Pocket OpGorator / PMOR.

The device exposes a USB CDC-ACM serial console (shows up as
/dev/cu.usbmodemNNNNNN1 on macOS) supporting these commands (per firmware
spec): HELP, STATUS, RATE, DIAG, BEAT ON, BEAT OFF, BEATLVL 0-127,
SY0..SY5, and `dfu` (reboots into the USB bootloader -- see main.py / ui.py
for the two-step confirmation required before this ever gets sent).

Three problems this module solves:

1. **Finding the right port.** This Mac has other USB serial devices
   attached. Blindly grabbing the first /dev/cu.usbmodem* glob match would
   occasionally grab someone else's device. Instead we walk the macOS IOKit
   registry (`ioreg -p IOUSB -l -w0`), find the USB device node whose
   product string matches this device (via `device_match.matches`, so
   "Pocket OpGorator" and "PMOR" both work), and read the specific
   IOCalloutDevice path hanging off *that* node's CDC-ACM child. See
   `parse_ioreg_tree()` / `resolve_port_path()` -- both are pure functions
   over text, unit-testable without ioreg or hardware.

2. **Parsing STATUS/DIAG without a byte-exact spec for their output
   format.** The wire format of STATUS/DIAG's response text isn't part of
   the documented command list, so parsing is deliberately tolerant: every
   line matching a loose `key: value` / `key = value` shape gets folded
   into `state.cdc.fields` (always available for display, even for fields
   we don't specifically recognize), AND a best-effort set of named fields
   (sample_rate, role, bpm, tick_count, tx_count, rx_count) is extracted by
   matching common key aliases. The full raw response text is always kept
   too (`last_status_raw` / `last_diag_raw`), so nothing the device sends is
   ever lost even if a field's key doesn't match any alias.

3. **Never blocking or interfering with audio/MIDI.** This runs entirely on
   its own threads (connect/health-check, line reader, slow DIAG-poll
   timer) touching only the serial fd -- there is no shared state with the
   audio callback or MIDI thread other than the same lock-free
   single-writer pattern used throughout this app (see state.py). DIAG is
   polled on its own slow timer (default 1.5s), never faster, per spec.
"""
import os
import re
import subprocess
import threading
import time

try:
    import serial
except ImportError:  # pragma: no cover - exercised only if pyserial missing
    serial = None

from device_match import matches as _device_name_matches

CONNECT_POLL_INTERVAL_S = 1.0
DIAG_POLL_INTERVAL_S = 1.5
RESPONSE_TIMEOUT_S = 1.0     # give up waiting for a STATUS/DIAG block after this
IDLE_FLUSH_S = 0.15          # flush a response block after this much silence
READ_TIMEOUT_S = 0.2         # pyserial read() timeout, drives reader loop cadence
BAUDRATE = 115200            # CDC-ACM ignores this in practice, set anyway

_KV_RE = re.compile(r"^\s*([A-Za-z0-9_ /.\-]{1,40}?)\s*[:=]\s*(.+?)\s*$")

# key aliases -> canonical field name, checked as "does this alias appear in
# the (lowercased) key". Order matters only in that first match wins within
# a category, which doesn't matter here since categories are disjoint.
_FIELD_ALIASES = {
    "sample_rate": ("samplerate", "sample rate", "sample_rate", " rate", "fs"),
    "role": ("role",),
    "bpm": ("bpm",),
    "tick_count": ("tick",),
    "tx_count": ("tx",),
    "rx_count": ("rx",),
}


# ---------------------------------------------------------------------
# Port resolution -- pure functions, unit-testable without ioreg/hardware
# ---------------------------------------------------------------------

def parse_ioreg_tree(text):
    """Parse `ioreg -p IOUSB -l -w0` output into a list of
    (usb_product_name_or_None, callout_device_path) pairs, one per
    IOCalloutDevice found anywhere in the tree, paired with the nearest
    ancestor node's USB Product Name (a CDC-ACM serial client sits several
    levels below the actual USB device node, so this has to walk up).

    Pure text parsing, no subprocess calls -- fully unit-testable with a
    fabricated ioreg dump.
    """
    node_re = re.compile(r"^(\s*)\+-o\s+\S.*?<class ")
    product_re = re.compile(r'"USB Product Name"\s*=\s*"(.*)"')
    callout_re = re.compile(r'"IOCalloutDevice"\s*=\s*"(.*)"')

    stack = []  # list of dicts: {"indent": int, "product": str|None}
    results = []

    for line in text.splitlines():
        m = node_re.match(line)
        if m:
            indent = len(m.group(1))
            while stack and stack[-1]["indent"] >= indent:
                stack.pop()
            inherited = stack[-1]["product"] if stack else None
            stack.append({"indent": indent, "product": inherited})
            continue

        m = product_re.search(line)
        if m and stack:
            stack[-1]["product"] = m.group(1)
            continue

        m = callout_re.search(line)
        if m:
            product = stack[-1]["product"] if stack else None
            results.append((product, m.group(1)))

    return results


def _run_ioreg():
    try:
        proc = subprocess.run(
            ["ioreg", "-p", "IOUSB", "-l", "-w0"],
            capture_output=True, text=True, timeout=5.0,
        )
        return proc.stdout
    except Exception:
        return None


def resolve_port_path(port_override=None):
    """Find the /dev/cu.* path belonging to the Pocket OpGorator / PMOR's
    CDC-ACM console, by product-name match in the IOKit USB tree.

    Deliberately does NOT fall back to "just glob /dev/cu.usbmodem*" --
    this Mac has other USB serial devices attached, and grabbing the wrong
    one would mean sending firmware commands (including `dfu`!) to the
    wrong hardware. If ioreg can't be run or nothing matches, returns None
    and the caller shows "waiting" like every other panel.
    """
    if port_override:
        return port_override
    text = _run_ioreg()
    if text is None:
        return None
    for product, path in parse_ioreg_tree(text):
        if _device_name_matches(product):
            return path
    return None


# ---------------------------------------------------------------------
# Response parsing -- pure function, unit-testable
# ---------------------------------------------------------------------

def parse_kv_lines(lines):
    """Fold a list of response lines into a lowercased key->value dict.
    Lines that don't look like `key: value` or `key = value` are ignored
    for this dict (but the raw text is preserved separately by the
    caller)."""
    fields = {}
    for line in lines:
        m = _KV_RE.match(line)
        if m:
            key = m.group(1).strip().lower()
            val = m.group(2).strip()
            fields[key] = val
    return fields


def extract_named_fields(fields):
    """Best-effort extraction of the handful of fields the UI wants to
    show prominently, from the generic fields dict. Returns a dict with
    keys from _FIELD_ALIASES, values as-parsed strings/numbers, or None
    where nothing matched. Never raises on malformed values."""
    out = {name: None for name in _FIELD_ALIASES}
    for key, val in fields.items():
        for name, aliases in _FIELD_ALIASES.items():
            if out[name] is not None:
                continue
            if any(alias in key for alias in aliases):
                out[name] = _coerce_number(val)
    return out


def _coerce_number(val):
    """Try int, then float, then just return the trimmed string."""
    s = val.strip()
    m = re.match(r"^-?\d+$", s)
    if m:
        try:
            return int(s)
        except ValueError:
            pass
    m = re.match(r"^-?\d+\.\d+$", s)
    if m:
        try:
            return float(s)
        except ValueError:
            pass
    # e.g. "48000hz", "role 1 (master)" -- pull the first number out
    m = re.search(r"-?\d+(\.\d+)?", s)
    if m:
        try:
            return float(m.group(0)) if "." in m.group(0) else int(m.group(0))
        except ValueError:
            return s
    return s


# ---------------------------------------------------------------------
# Engine
# ---------------------------------------------------------------------

class CdcEngine:
    def __init__(self, state, port_override=None,
                 diag_interval=DIAG_POLL_INTERVAL_S):
        self.state = state
        self.port_override = port_override
        self.diag_interval = diag_interval

        self._ser = None
        self._write_lock = threading.Lock()
        self._stop = threading.Event()
        self._connect_thread = None
        self._reader_thread = None
        self._diag_thread = None
        self._connected_flag = threading.Event()  # set while a port is open

        self._pending_command = None   # "STATUS" / "DIAG" / None
        self._pending_buffer = []
        self._pending_deadline = 0.0
        self._last_line_at = 0.0
        self._pending_lock = threading.Lock()

    # ---------- lifecycle ----------

    def start(self):
        if serial is None:
            self.state.cdc.last_error = "pyserial not installed"
            return
        self._connect_thread = threading.Thread(target=self._connect_loop, daemon=True)
        self._connect_thread.start()

    def stop(self):
        self._stop.set()
        self._connected_flag.clear()
        for t in (self._connect_thread, self._reader_thread, self._diag_thread):
            if t:
                t.join(timeout=2.0)
        self._close()

    # ---------- connect / health-check loop ----------

    def _connect_loop(self):
        while not self._stop.is_set():
            try:
                if self._ser is None:
                    self._try_connect()
                else:
                    self._check_still_present()
            except Exception as e:
                self.state.cdc.last_error = f"poll error: {e}"
                self._close()
            self._stop.wait(CONNECT_POLL_INTERVAL_S)

    def _try_connect(self):
        path = resolve_port_path(self.port_override)
        if not path:
            self.state.cdc.connected = False
            return
        try:
            ser = serial.Serial(path, baudrate=BAUDRATE, timeout=READ_TIMEOUT_S)
        except Exception as e:
            self.state.cdc.last_error = f"open {path} failed: {e}"
            return
        self._ser = ser
        self.state.cdc.connected = True
        self.state.cdc.port_path = path
        self.state.cdc.last_error = ""

        self._connected_flag.set()
        self._reader_thread = threading.Thread(target=self._reader_loop, daemon=True)
        self._reader_thread.start()
        self._diag_thread = threading.Thread(target=self._diag_loop, daemon=True)
        self._diag_thread.start()

    def _check_still_present(self):
        path = self.state.cdc.port_path
        if path and not os.path.exists(path):
            self.state.cdc.last_error = "CDC port disappeared"
            self._close()
            self.state.cdc.connected = False

    def _close(self):
        self._connected_flag.clear()
        ser = self._ser
        self._ser = None
        if ser is not None:
            try:
                ser.close()
            except Exception:
                pass

    # ---------- reader thread ----------

    def _reader_loop(self):
        ser = self._ser
        while not self._stop.is_set() and self._connected_flag.is_set() and ser is not None:
            try:
                raw = ser.readline()
            except Exception as e:
                self.state.cdc.last_error = f"read failed: {e}"
                self._close()
                self.state.cdc.connected = False
                return
            now = time.time()
            if raw:
                try:
                    line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                except Exception:
                    line = ""
                if line:
                    self._handle_line(line, now)
                    self.state.cdc.last_rx_at = now
                    self._last_line_at = now
            self._maybe_flush_pending(now)

    def _handle_line(self, line, now):
        # Always fold into the running generic fields dict, whether or not
        # a STATUS/DIAG request is currently pending -- the device may emit
        # unsolicited lines (e.g. echoing BEAT/SY confirmations).
        kv = parse_kv_lines([line])
        if kv:
            self.state.cdc.fields.update(kv)
            self._apply_named_fields()

        with self._pending_lock:
            if self._pending_command is not None:
                self._pending_buffer.append(line)

    def _maybe_flush_pending(self, now):
        with self._pending_lock:
            if self._pending_command is None:
                return
            idle = (now - self._last_line_at) > IDLE_FLUSH_S if self._last_line_at else False
            expired = now > self._pending_deadline
            if not (idle or expired):
                return
            cmd = self._pending_command
            text = "\n".join(self._pending_buffer)
            self._pending_command = None
            self._pending_buffer = []
        if cmd == "STATUS":
            self.state.cdc.last_status_raw = text
        elif cmd == "DIAG":
            self.state.cdc.last_diag_raw = text
            self.state.cdc.last_diag_at = now

    def _apply_named_fields(self):
        named = extract_named_fields(self.state.cdc.fields)
        st = self.state.cdc
        if named["sample_rate"] is not None:
            st.sample_rate = named["sample_rate"]
        if named["role"] is not None:
            st.role = named["role"]
        if named["bpm"] is not None:
            st.bpm = named["bpm"]
        if named["tick_count"] is not None:
            st.tick_count = named["tick_count"]
        if named["tx_count"] is not None:
            st.tx_count = named["tx_count"]
        if named["rx_count"] is not None:
            st.rx_count = named["rx_count"]

    # ---------- DIAG slow-poll thread ----------

    def _diag_loop(self):
        while not self._stop.is_set() and self._connected_flag.is_set():
            self.send_command("DIAG")
            self._stop.wait(self.diag_interval)

    # ---------- commands (callable from any thread, e.g. the UI thread) ----------

    def send_command(self, cmd):
        """Send a raw command line to the device. Returns True if it was
        actually written. Never raises. Marks STATUS/DIAG as "pending" so
        the next burst of response lines gets captured as a block; other
        commands are fire-and-forget (the device's own next STATUS/DIAG
        will confirm state changes, and we also optimistically update local
        cdc state below for immediate UI feedback)."""
        ser = self._ser
        if ser is None:
            return False
        cmd = cmd.strip()
        upper = cmd.upper()
        if upper in ("STATUS", "DIAG"):
            with self._pending_lock:
                self._pending_command = upper
                self._pending_buffer = []
                self._pending_deadline = time.time() + RESPONSE_TIMEOUT_S
        try:
            with self._write_lock:
                ser.write((cmd + "\r\n").encode("ascii", errors="replace"))
                ser.flush()
            self.state.cdc.last_tx_at = time.time()
        except Exception as e:
            self.state.cdc.last_error = f"write failed: {e}"
            self._close()
            self.state.cdc.connected = False
            return False

        self._optimistic_update(upper, cmd)
        return True

    def _optimistic_update(self, upper, original_cmd):
        st = self.state.cdc
        if upper.startswith("SY") and len(upper) == 3 and upper[2].isdigit():
            st.sy_mode = int(upper[2])
        elif upper == "BEAT ON":
            st.beat_on = True
        elif upper == "BEAT OFF":
            st.beat_on = False
        elif upper.startswith("BEATLVL"):
            parts = upper.split()
            if len(parts) == 2 and parts[1].isdigit():
                st.beat_level = max(0, min(127, int(parts[1])))
        elif upper == "DFU":
            st.dfu_sent = True
            st.dfu_sent_at = time.time()

    # ---------- convenience wrappers ----------

    def set_sy_mode(self, n):
        if 0 <= n <= 5:
            return self.send_command(f"SY{n}")
        return False

    def set_beat(self, on):
        return self.send_command("BEAT ON" if on else "BEAT OFF")

    def set_beat_level(self, level):
        level = max(0, min(127, int(level)))
        return self.send_command(f"BEATLVL {level}")

    def send_dfu(self):
        """Reboots the device into the USB bootloader. Callers (main.py)
        MUST gate this behind an explicit two-step confirmation -- this
        method itself sends immediately, no confirmation here."""
        return self.send_command("dfu")
