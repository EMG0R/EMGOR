"""store.py - bank + sequence files and the OSC link for neptr_web (stdlib only).

Bank/seq files are Csound ftsave binaries: a ~2011-double FUNC header, then the
table (8736 doubles + 1 guard). We never parse the header: the layout stamp
sits at table index 8735, so base = (index of the last stamp) - 8735, exactly as
tools/preset_bank.py does. The sequence file reuses the bank's header (same
table size), see csound/preset_seq.orc for the layout.
"""
import json, os, re, shutil, socket, struct, time, threading

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
PRESET_DIR = os.environ.get("NEPTR_PRESET_DIR", os.path.expanduser("~/demiurge/presets/surf"))
STATE_DIR = os.environ.get("NEPTR_STATE_DIR", os.path.expanduser("~/demiurge/state"))
NAM_DIR = os.environ.get("NEPTR_NAM_DIR", os.path.expanduser("~/demiurge/profiles/nam"))
OSC_ADDR = ("127.0.0.1", int(os.environ.get("NEPTR_OSC_PORT", "7770")))

SLOT = 512
NSLOT = 17
OCC = 8704
GEN = 8730
STAMP_I = 8735
BANK_STAMP = 771217.0
SEQ_STAMP = 771218.0
SEQ_FLAG = 1100
SET_STAMP = 77000302.0
NSTEPS = 16
lock = threading.Lock()


# ---------------- OSC ----------------
def _pad(b):
    return b + b"\0" * (4 - len(b) % 4)

def osc_send(addr, *args):
    tags, body = ",", b""
    for a in args:
        if isinstance(a, float):
            tags += "f"; body += struct.pack(">f", a)
        else:
            tags += "i"; body += struct.pack(">i", int(a))
    pkt = _pad(addr.encode()) + _pad(tags.encode()) + body
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.sendto(pkt, OSC_ADDR)
    finally:
        s.close()

def osc_cc(ch, cc, val):
    osc_send("/cc", int(ch), int(cc), int(val))


# ---------------- table files ----------------
class Table:
    def __init__(self, raw, stamp, stamp_i):
        self.n = len(raw) // 8
        self.v = list(struct.unpack("<%dd" % self.n, raw[: self.n * 8]))
        idx = [i for i, x in enumerate(self.v) if abs(x - stamp) < 0.5]
        if not idx:
            raise ValueError("no layout stamp %d" % stamp)
        self.base = idx[-1] - stamp_i
        self.header_len = self.base

    def pack(self):
        return struct.pack("<%dd" % self.n, *self.v)

def _read(path, stamp, stamp_i):
    try:
        return Table(open(path, "rb").read(), stamp, stamp_i)
    except (OSError, ValueError):
        return None

def load_bank():
    """(bank Table, seq Table, gens) - freshest valid file of each pair."""
    best = {}
    for kind, names, stamp in (("bank", ("bank.ftab", "bank.b.ftab"), BANK_STAMP),
                               ("seq", ("seq.ftab", "seq.b.ftab"), SEQ_STAMP)):
        cands = []
        for nme in names:
            t = _read(os.path.join(PRESET_DIR, nme), stamp, STAMP_I)
            if t:
                cands.append((t.v[t.base + GEN], os.path.getmtime(os.path.join(PRESET_DIR, nme)), t))
        cands.sort(key=lambda c: (c[0], c[1]), reverse=True)
        best[kind] = cands[0][2] if cands else None
    if best["bank"] is None:
        raise RuntimeError("no valid bank in %s" % PRESET_DIR)
    if best["seq"] is None:
        best["seq"] = new_seq_from(best["bank"])
    return best["bank"], best["seq"]

def new_seq_from(bank):
    """Fresh all-legacy sequence table using the bank's header (same size)."""
    t = Table(bank.pack(), BANK_STAMP, STAMP_I)
    for i in range(t.base, t.base + 8736):
        t.v[i] = 0.0
    t.v[t.base + STAMP_I] = SEQ_STAMP
    return t

def _backup():
    d = os.path.join(PRESET_DIR, "web-backups")
    os.makedirs(d, exist_ok=True)
    ts = time.strftime("%Y%m%d-%H%M%S")
    for f in ("bank.ftab", "bank.b.ftab", "seq.ftab", "seq.b.ftab"):
        p = os.path.join(PRESET_DIR, f)
        if os.path.exists(p):
            shutil.copy2(p, os.path.join(d, "%s.%s" % (f, ts)))
    old = sorted(os.listdir(d))
    for f in old[:-80]:
        os.remove(os.path.join(d, f))

def _write_pair(t, name_a, name_b):
    raw = t.pack()
    for nme in (name_b, name_a):               # A last: the engine reads A
        p = os.path.join(PRESET_DIR, nme)
        tmp = p + ".tmp"
        with open(tmp, "wb") as f:
            f.write(raw)
            f.flush(); os.fsync(f.fileno())
        os.replace(tmp, p)

def save_bank(bank, seq, reload=True):
    """Write both tables to both A/B files with a generation above anything
    on disk, then tell the engine to re-read."""
    with lock:
        _backup()
        gens = [0.0]
        for nme in ("bank.ftab", "bank.b.ftab", "seq.ftab", "seq.b.ftab"):
            for stamp in (BANK_STAMP, SEQ_STAMP):
                t = _read(os.path.join(PRESET_DIR, nme), stamp, STAMP_I)
                if t:
                    gens.append(t.v[t.base + GEN])
        g = max(gens) + 100
        for t in (bank, seq):
            t.v[t.base + GEN] = g
        _write_pair(bank, "bank.ftab", "bank.b.ftab")
        _write_pair(seq, "seq.ftab", "seq.b.ftab")
    if reload:
        osc_send("/bank_reload", 1)
        threading.Timer(1.5, lambda: osc_send("/bank_reload", 1)).start()


# ---------------- slots ----------------
def slot_row(bank, s):
    a = bank.base + (s - 1) * SLOT
    return bank.v[a:a + SLOT]

def set_row(bank, s, row):
    a = bank.base + (s - 1) * SLOT
    bank.v[a:a + SLOT] = list(row)
    bank.v[bank.base + OCC + s - 1] = 1.0

def occupied(bank, s):
    return bank.v[bank.base + OCC + s - 1] >= 0.5

def get_seq(seq, s):
    if seq.v[seq.base + SEQ_FLAG + s - 1] < 0.5:
        return None
    out = []
    for j in range(NSTEPS):
        a = seq.base + ((s - 1) * NSTEPS + j) * 4
        t = int(seq.v[a])
        if t == 0:
            continue
        out.append(dict(type=t, a=seq.v[a + 1], b=seq.v[a + 2], c=seq.v[a + 3]))
    return out

def set_seq(seq, s, steps):
    """steps None = legacy (flag 0)."""
    for j in range(NSTEPS):
        a = seq.base + ((s - 1) * NSTEPS + j) * 4
        seq.v[a:a + 4] = [0.0, 0.0, 0.0, 0.0]
    if steps is None:
        seq.v[seq.base + SEQ_FLAG + s - 1] = 0.0
        return
    for j, st in enumerate(steps[:NSTEPS]):
        a = seq.base + ((s - 1) * NSTEPS + j) * 4
        seq.v[a:a + 4] = [float(st["type"]), float(st.get("a", 0)), float(st.get("b", 0)), float(st.get("c", 0))]
    seq.v[seq.base + SEQ_FLAG + s - 1] = 1.0

def clear_slot(bank, seq, s):
    a = bank.base + (s - 1) * SLOT
    bank.v[a:a + SLOT] = [0.0] * SLOT
    bank.v[bank.base + OCC + s - 1] = 0.0
    set_seq(seq, s, None)


# ---------------- live settings file ----------------
def read_live():
    """The engine's neptr_settings table (<= 2 s old). Returns list[512] or None."""
    best = None
    for nme in ("neptr_settings.ftab", "neptr_settings.b.ftab"):
        p = os.path.join(STATE_DIR, nme)
        try:
            raw = open(p, "rb").read()
        except OSError:
            continue
        n = len(raw) // 8
        if n < 600:
            continue
        v = struct.unpack("<%dd" % n, raw[: n * 8])
        base = n - 513                       # header, 512 table points, 1 guard point
        m = os.path.getmtime(p)
        if best is None or m > best[0]:
            best = (m, list(v[base:base + 512]))
    return best[1] if best else None


# ---------------- state map / catalogue / names ----------------
def state_map():
    pat = re.compile(r"(gk[A-Za-z0-9_]+)\s*!?=\s*tab:k\(\s*(\d+)\s*,\s*giNeptrState\s*\)")
    names = {}
    for line in open(os.path.join(ROOT, "csound", "state_persist.orc")):
        m = pat.search(line)
        if m:
            names.setdefault(m.group(1), int(m.group(2)))
    return names

def catalogue():
    return json.load(open(os.path.join(ROOT, "presets", "effects.json")))

def nam_profiles():
    try:
        return sorted(f[:-4] for f in os.listdir(NAM_DIR) if f.endswith(".nam"))
    except OSError:
        return []

NAMES_FILE = lambda: os.path.join(PRESET_DIR, "names.json")

def load_names():
    try:
        return json.load(open(NAMES_FILE()))
    except (OSError, ValueError):
        return {}

def save_names(d):
    tmp = NAMES_FILE() + ".tmp"
    json.dump(d, open(tmp, "w"), indent=1, sort_keys=True)
    os.replace(tmp, NAMES_FILE())
