#!/usr/bin/env python3
"""make_set.py -- rebuilds dryrun.als (gzip'd Live 12 set XML) = template + the dryrun device path.
   python3 make_set.py [--amxd PATH] [--out PATH] [--userlib] [--check FILE.als]
Template (template/one_midi_track_instrument.xml): Emory's 'marimb comp.als' (Live 12.3b16, real file saved by Live) cut down to ONE MIDI track
holding one MxDeviceInstrument: the return tracks, their send holders, the arrangement clip were removed (original untouched; derivation: delete
the arrangement MidiClip + <Sends> children of the track, both <ReturnTrack> blocks and the <SendsPre> entries). Everything else is Live's own output.
Substituted: the MxPatchRef FileRef (relative path from the set's folder, absolute Path, size, CRC-16/BUYPASS of the .amxd, mtime) and the track name.
--userlib points the device at the User Library copy (~/Music/Ableton/User Library/Presets/Instruments/Max Instrument/PMOR/PMOR dryrun.amxd) instead.
Nothing here touches Live."""
import argparse, gzip, os, re, sys
from xml.dom import minidom

HERE = os.path.dirname(os.path.abspath(__file__))
TEMPLATE = os.path.join(HERE, "template", "one_midi_track_instrument.xml")
AMXD = os.path.join(HERE, "PMOR dryrun.amxd")
USERLIB = os.path.expanduser("~/Music/Ableton/User Library/Presets/Instruments/Max Instrument/PMOR/PMOR dryrun.amxd")
TRACK_NAME = "PMOR dryrun"


def crc16(data):
    """Live's OriginalCrc: CRC-16/BUYPASS (poly 0x8005, init 0, no reflection); verified against marimba.amxd (6104 B -> 8425) from a Live-saved set."""
    c = 0
    for b in data:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x8005) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c


def esc(s):
    return s.replace("&", "&amp;").replace('"', "&quot;").replace("<", "&lt;")


def build(amxd, out):
    xml = open(TEMPLATE, encoding="utf-8").read()
    data = open(amxd, "rb").read()
    rel = os.path.relpath(amxd, os.path.dirname(os.path.abspath(out)))
    fileref = ('<RelativePathType Value="1" />\n\t\t\t\t\t\t\t\t\t\t\t\t<RelativePath Value="%s" />\n\t\t\t\t\t\t\t\t\t\t\t\t<Path Value="%s" />'
               % (esc(rel), esc(os.path.abspath(amxd))))
    xml, n = re.subn(r'<RelativePathType Value="1" />\s*<RelativePath Value="[^"]*marimba\.amxd" />\s*<Path Value="[^"]*marimba\.amxd" />', fileref, xml)
    assert n == 1, "device FileRef not found in template"
    for tag, val in (("OriginalFileSize", len(data)), ("OriginalCrc", crc16(data)), ("LastModDate", int(os.path.getmtime(amxd)))):
        pat = r'(<MxPatchRef Id="\d+">.*?<%s Value=")\d+(")' % tag
        xml, n = re.subn(pat, r"\g<1>%d\g<2>" % val, xml, count=1, flags=re.S)
        assert n == 1, tag
    xml = xml.replace('<EffectiveName Value="1-marimba" />', '<EffectiveName Value="%s" />' % TRACK_NAME, 1)
    xml.encode("utf-8")
    return xml


def check(als):
    raw = gzip.open(als, "rb").read()
    dom = minidom.parseString(raw)
    tracks = dom.getElementsByTagName("MidiTrack")
    devs = dom.getElementsByTagName("MxDeviceInstrument")
    refs = dom.getElementsByTagName("MxPatchRef")
    path = refs[0].getElementsByTagName("Path")[0].getAttribute("Value") if refs else ""
    return {"midi_tracks": len(tracks), "mx_instruments": len(devs), "audio_tracks": len(dom.getElementsByTagName("AudioTrack")),
            "return_tracks": len(dom.getElementsByTagName("ReturnTrack")), "device_path": path, "device_exists": os.path.exists(path)}


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--amxd", default=None)
    ap.add_argument("--out", default=os.path.join(HERE, "dryrun.als"))
    ap.add_argument("--userlib", action="store_true", help="reference the User Library copy of the device")
    ap.add_argument("--check", metavar="ALS", help="parse an existing .als and print its summary (no write)")
    a = ap.parse_args()
    if a.check:
        r = check(a.check)
        print(r)
        sys.exit(0 if r["midi_tracks"] == 1 and r["mx_instruments"] == 1 else 1)
    amxd = a.amxd or (USERLIB if a.userlib else AMXD)
    if not os.path.exists(amxd):
        sys.exit("device not found: " + amxd)
    xml_text = build(amxd, a.out)
    with open(a.out, "wb") as raw:     # mtime=0 -> reproducible bytes
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0) as gz:
            gz.write(xml_text.encode("utf-8"))
    print("wrote", a.out, check(a.out))
