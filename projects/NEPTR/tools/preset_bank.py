#!/usr/bin/env python3
"""preset_bank.py — read, edit and author NEPTR preset banks as TEXT.

WHY THIS EXISTS
---------------
The bank is a flat array of binary doubles with no names in it, so until now a
preset could only be made by physically setting the rig and capturing it. That
makes presets impossible to review, diff, back up meaningfully, or write away
from the instrument — and impossible to prepare for a show on a laptop.

This reads the SAME index->parameter mapping csound uses, straight out of
state_persist.orc (`gkATTK_mix = tab:k(3, giNeptrState)`), so the names here
can never drift from the engine: if a parameter moves index, this moves with
it. No second source of truth to maintain.

LAYOUT (measured, not guessed — verified against a live bank 2026-09-25)
    stamp 771217 sits at the LAST element     -> base = stamp_index - 8735
    slot N (1..17) values : base + (N-1)*512, 512 doubles
    occupancy flags       : base + 8704 + (N-1), 1.0 = occupied
    slots 1..12           : DUODECIMUS foot encoders
    slots 13..17          : onboard menu-0 rows

A/B GOTCHA: saves alternate between bank.ftab and bank.b.ftab, but the loader
always prefers bank.ftab and only falls back if A's stamp is invalid — and the
stamp is rewritten every save. So roughly half of all captures silently vanish
at boot. Every write here therefore writes BOTH files, which also makes the
result deterministic regardless of which one the engine reads next.

USAGE
    preset_bank.py dump [--slot N]           what is in the bank, by name
    preset_bank.py clear --slots 1-12        empty those slots
    preset_bank.py apply presets.json        write named values into slots
    preset_bank.py export out.json           dump every occupied slot as text
Add --bank DIR to point at a bank other than the default.
"""
import argparse, json, os, re, struct, shutil, sys, time

SLOT_SIZE   = 512
N_SLOTS     = 17
OCC_OFFSET  = 8704
GEN_OFFSET  = 8730          # monotonic save generation (state_persist.orc PB_GEN_I)
STAMP_OFFSET = 8735
STAMP_VALUE = 771217
DUODECIMUS  = range(1, 13)          # foot encoders
ONBOARD     = range(13, 18)         # menu-0 rows

HERE = os.path.dirname(os.path.abspath(__file__))
STATE_ORC = os.path.join(HERE, "..", "csound", "state_persist.orc")


def load_map(path=STATE_ORC):
    """{parameter name: index} straight from the engine's own restore code."""
    names = {}
    # Two forms exist in the wild and both are authoritative:
    #   gkATTK_mix = tab:k(3, giNeptrState)              (plain restore)
    #   if (gkATTK_mix != tab:k(3, giNeptrState)) then   (guarded restore)
    # Matching only the first silently produced an EMPTY map on the rig, which
    # would have been a very confusing way to fail, so accept either.
    pat = re.compile(r"(gk[A-Za-z0-9_]+)\s*!?=\s*tab:k\(\s*(\d+)\s*,\s*giNeptrState\s*\)")
    with open(path) as f:
        for line in f:
            m = pat.search(line)
            if m:
                names.setdefault(m.group(1), int(m.group(2)))
    if not names:
        sys.exit("could not parse any parameters from %s" % path)
    return names


class Bank:
    def __init__(self, path):
        self.path = path
        raw = open(path, "rb").read()
        self.n = len(raw) // 8
        self.v = list(struct.unpack("<%dd" % self.n, raw[: self.n * 8]))
        stamps = [i for i, x in enumerate(self.v) if abs(x - STAMP_VALUE) < 0.5]
        if not stamps:
            sys.exit("%s: no layout stamp %d — not a bank file?" % (path, STAMP_VALUE))
        self.base = stamps[-1] - STAMP_OFFSET
        if self.base < 0:
            sys.exit("%s: stamp at %d is too early for the layout" % (path, stamps[-1]))

    # ---- accessors -------------------------------------------------------
    def occupied(self, slot):
        return self.v[self.base + OCC_OFFSET + (slot - 1)] >= 0.5

    def set_occupied(self, slot, on):
        self.v[self.base + OCC_OFFSET + (slot - 1)] = 1.0 if on else 0.0

    def slot_region(self, slot):
        s = self.base + (slot - 1) * SLOT_SIZE
        return s, s + SLOT_SIZE

    def get(self, slot, index):
        s, _ = self.slot_region(slot)
        return self.v[s + index]

    def set(self, slot, index, value):
        s, _ = self.slot_region(slot)
        self.v[s + index] = float(value)

    def clear(self, slot):
        s, e = self.slot_region(slot)
        for i in range(s, e):
            self.v[i] = 0.0
        self.set_occupied(slot, False)

    def save(self, path=None):
        path = path or self.path
        with open(path, "wb") as f:
            f.write(struct.pack("<%dd" % self.n, *self.v))


def backup(path):
    ts = time.strftime("%Y%m%d-%H%M%S")
    dst = "%s.%s.bak" % (path, ts)
    shutil.copy2(path, dst)
    return dst


def bank_files(d):
    a, b = os.path.join(d, "bank.ftab"), os.path.join(d, "bank.b.ftab")
    have = [p for p in (a, b) if os.path.exists(p)]
    if not have:
        sys.exit("no bank.ftab / bank.b.ftab in %s" % d)
    return have


def parse_slots(spec):
    out = []
    for part in spec.split(","):
        part = part.strip()
        if "-" in part:
            lo, hi = part.split("-")
            out.extend(range(int(lo), int(hi) + 1))
        elif part:
            out.append(int(part))
    bad = [s for s in out if not 1 <= s <= N_SLOTS]
    if bad:
        sys.exit("slots out of range 1..%d: %s" % (N_SLOTS, bad))
    return sorted(set(out))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=["dump", "clear", "apply", "export"])
    ap.add_argument("file", nargs="?")
    ap.add_argument("--bank", default=os.path.expanduser("~/demiurge/presets/gorcore"))
    ap.add_argument("--slot", type=int)
    ap.add_argument("--slots")
    ap.add_argument("--all", action="store_true", help="dump every value, not just non-zero")
    a = ap.parse_args()

    names = load_map()
    by_index = {v: k for k, v in names.items()}
    files = bank_files(a.bank)

    if a.cmd in ("dump", "export"):
        # read the FRESHER file: saves alternate A/B and each dumps the whole
        # bank, so the higher generation is the complete current bank (this is
        # exactly the rule the engine's loader uses since 2026-09-25).
        cands = []
        for p in files:
            b = Bank(p)
            cands.append((b.v[b.base + GEN_OFFSET], os.path.getmtime(p), b))
        cands.sort(key=lambda c: (c[0], c[1]), reverse=True)
        bk = cands[0][2]
        gens = ", ".join("%s=gen %d" % (os.path.basename(c[2].path), int(c[0])) for c in cands)
        slots = [a.slot] if a.slot else [s for s in range(1, N_SLOTS + 1) if bk.occupied(s)]
        if a.cmd == "export":
            out = {}
            for s in slots:
                d = {}
                for idx, nm in sorted(by_index.items()):
                    val = bk.get(s, idx)
                    if a.all or val != 0.0:
                        d[nm] = round(val, 6)
                out[str(s)] = d
            dest = a.file or "presets_export.json"
            json.dump({"slots": out}, open(dest, "w"), indent=2, sort_keys=True)
            print("exported %d slot(s) -> %s" % (len(slots), dest))
            return
        print("bank: %s   base=%d   (%s)" % (bk.path, bk.base, gens))
        occ = [s for s in range(1, N_SLOTS + 1) if bk.occupied(s)]
        print("occupied: DUODECIMUS %s   onboard %s"
              % ([s for s in occ if s in DUODECIMUS] or "(none)",
                 [s for s in occ if s in ONBOARD] or "(none)"))
        for s in slots:
            vals = [(by_index[i], bk.get(s, i)) for i in sorted(by_index)
                    if a.all or bk.get(s, i) != 0.0]
            print("\n-- slot %d (%s) : %d non-zero --"
                  % (s, "DUODECIMUS" if s in DUODECIMUS else "onboard", len(vals)))
            for nm, val in vals[:400]:
                print("   %-28s %s" % (nm, round(val, 6)))
        return

    if a.cmd == "clear":
        if not a.slots:
            sys.exit("clear needs --slots, e.g. --slots 1-12")
        slots = parse_slots(a.slots)
        for p in files:
            print("backup:", backup(p))
            bk = Bank(p)
            for s in slots:
                bk.clear(s)
            bk.save()
            print("  cleared %s in %s" % (slots, os.path.basename(p)))
        return

    if a.cmd == "apply":
        if not a.file:
            sys.exit("apply needs a json file")
        spec = json.load(open(a.file))
        slots_spec = spec.get("slots", spec)
        unknown = sorted({k for d in slots_spec.values() for k in d} - set(names))
        if unknown:
            sys.exit("unknown parameter(s): %s" % unknown)
        for p in files:
            print("backup:", backup(p))
            bk = Bank(p)
            for slot_s, d in slots_spec.items():
                s = int(slot_s)
                bk.clear(s)                       # authored presets are absolute
                for nm, val in d.items():
                    bk.set(s, names[nm], val)
                bk.set_occupied(s, True)
            bk.save()
            print("  wrote slots %s in %s"
                  % (sorted(int(x) for x in slots_spec), os.path.basename(p)))
        return


if __name__ == "__main__":
    main()
