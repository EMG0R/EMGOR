#!/home/gorpi7v5/vcv/venv/bin/python
"""vcv-check <patch.vcv|dir> ... : which modules each Rack patch needs vs what Cardinal ships.
Reads patch.json from the .vcv (zstd/gzip tar, or plain json) and the plugin manifests
bundled with the installed Cardinal. Exit 1 if any patch has a missing module.
Cardinal also drops/renames some modules; 'plugin ok, model missing' means a version gap."""
import sys, os, json, glob, subprocess, tarfile, io, gzip, collections
CARD = os.environ.get("CARDINAL_RES", "/opt/demiurge/cardinal/bin/resources/PluginManifests")
# Core is built into Rack, so it has no manifest. Cardinal remaps Core/AudioInterface2 -> HostAudio2
# (string table in binary). Core MIDI_CV/MIDICC_CV/MIDIMap/Notes: remap UNVERIFIED -> reported separately.
CORE_REMAP = {"AudioInterface2": "Cardinal/HostAudio2"}
def shipped():
    d = {}
    for f in glob.glob(CARD + "/*.json"):
        j = json.load(open(f)); d[j["slug"]] = {m["slug"] for m in j.get("modules", [])}
    return d
def load(path):
    raw = open(path, "rb").read()
    for dec in (lambda r: subprocess.run(["zstd", "-dc"], input=r, capture_output=True, check=True).stdout,
                gzip.decompress, lambda r: r):
        try: data = dec(raw)
        except Exception: continue
        try: return json.loads(data)
        except Exception: pass
        try:
            with tarfile.open(fileobj=io.BytesIO(data)) as t:
                name = next(n for n in t.getnames() if n.lstrip("./") == "patch.json")
                return json.load(t.extractfile(name))
        except Exception: pass
    raise ValueError("unreadable patch")
def mods(pj):
    out = []
    def walk(ms):
        for m in ms:
            out.append((m.get("plugin"), m.get("model")))
            if "modules" in m: walk(m["modules"])
    walk(pj.get("modules", [])); return out
print_core = set()
def main(args):
    S = shipped(); bad = 0
    files = []
    for a in args or ["."]:
        files += sorted(glob.glob(a + "/**/*.vcv", recursive=True)) if os.path.isdir(a) else [a]
    for f in files:
        try: pj = load(f)
        except Exception as e: print(f"{f}: ERROR {e}"); bad += 1; continue
        ms = mods(pj); miss = collections.Counter(); wrongver = collections.Counter()
        for p, m in ms:
            if p == "Core": (print_core.add((m, CORE_REMAP.get(m, "UNVERIFIED")))) ; continue
            if p not in S: miss[(p, m)] += 1
            elif m not in S[p]: wrongver[(p, m)] += 1
        ok = not miss and not wrongver
        print("   Core modules:", sorted(print_core)); print_core.clear()
        print(f"{f}: {len(ms)} modules, rack {pj.get('version','?')}, {'OK' if ok else 'MISSING'}")
        for (p, m), n in sorted(miss.items(), key=str): print(f"   missing plugin: {p} / {m} x{n}")
        for (p, m), n in sorted(wrongver.items(), key=str): print(f"   plugin shipped, model absent: {p} / {m} x{n}")
        bad += 0 if ok else 1
    return 1 if bad else 0
sys.exit(main(sys.argv[1:]))
