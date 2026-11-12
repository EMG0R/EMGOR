#!/home/gorpi7v5/kivy-venv/bin/python
"""NEPTR body -> VCV Rack macros, on a private ALSA port "VCV Macros" (ch9).

Reads the NEPTR Serial Bridge (read-only, ch1) and sends ch9 CCs that only Rack
listens to, so nothing here reaches csound's MIDI bus (Midi Through):

  top-left encoder turn (ch1 CC26, CW=127 / CCW=0)  -> accumulated 0..127 -> CC30 probability
  arcade (ch1 CC60, 127 press / 0 release)          -> manual trigger gate, ALTERNATING
      voices: odd presses CC31 (L voice), even presses CC38 (R voice); the
      release closes the gate the press opened (wire_patch.py step 4a)
      (suppressed while the top-left encoder click, ch1 CC76, is held: that
       gesture is tap tempo, handled by the bridge)
  accelerometer X (ch1 CC31)                         -> CC32 tone
  accelerometer Y (ch1 CC32)                         -> CC33 filter (Ripples) / Plaits MORPH
  TOF 1 (ch1 CC30)                                   -> CC34
  TOF 2 (ch1 CC36)                                   -> CC35
  joystick X (ch1 CC27)                              -> CC36 envelope attack
  joystick Y (ch1 CC28)                              -> CC37 envelope decay

Probability persists in ~/vcv/state/macros.json. All current values are re-sent
every 3 s so a restarted Rack picks them up.
"""
import glob, json, os, subprocess, time
import mido

STATE = os.path.expanduser("~/vcv/state/macros.json")
CH = 8                    # MIDI ch9, 0-indexed
STEP = 4                  # probability change per encoder detent
SMOOTH = 0.35             # one-pole smoothing for accel / TOF (0 = frozen, 1 = raw)
RESEND_S = 3.0

ROUTE = {31: 32, 32: 33, 30: 34, 36: 35, 27: 36, 28: 37}   # ch1 sensor CC -> ch9 macro CC (27/28 joystick X/Y -> envelope)

# Radio patch buttons (ch1, body_control_map.md): A B X Y UP DOWN LEFT RIGHT -> slot 1..8.
# Slot N = the Nth patch (sorted by filename) in ~/demiurge_vcv/instruments; empty slot = no-op.
SLOT_CC = {65: 0, 66: 1, 67: 2, 68: 3, 61: 4, 62: 5, 63: 6, 64: 7}
PATCH_DIR = os.path.expanduser("~/demiurge_vcv/instruments")
RACK = os.path.expanduser("~/PRIME_RECOVERY/vcv/rack-try.sh")


def load():
    try:
        return json.load(open(STATE))
    except (OSError, ValueError):
        return {"prob": 64}


def save(st):
    os.makedirs(os.path.dirname(STATE), exist_ok=True)
    tmp = STATE + ".tmp"
    with open(tmp, "w") as f:
        json.dump(st, f)
    os.replace(tmp, STATE)


def main():
    st = load()
    out = mido.open_output("VCV Macros", virtual=True, client_name="VCV Macros")
    src = None
    while src is None:
        names = [n for n in mido.get_input_names() if n.startswith("NEPTR Serial Bridge")]
        if names:
            src = mido.open_input(names[0])
        else:
            time.sleep(1)
    vals = {30: st["prob"]}                  # macro CC -> last value sent
    smooth = {}
    held = False
    arc_cc = 38                              # last arcade side; first press -> 31 (L)
    arc_open = None                          # CC whose gate is open now
    switching = None                         # Popen of a running patch switch

    def send(cc, v):
        v = max(0, min(127, int(round(v))))
        if vals.get(cc) != v:
            vals[cc] = v
            out.send(mido.Message("control_change", channel=CH, control=cc, value=v))

    out.send(mido.Message("control_change", channel=CH, control=30, value=st["prob"]))
    last_resend = time.time()
    while True:
        for m in src.iter_pending():
            if m.type != "control_change" or m.channel != 0:
                continue
            cc, v = m.control, m.value
            if cc == 26:                                  # top-left encoder turn
                st["prob"] = max(0, min(127, st["prob"] + (STEP if v >= 64 else -STEP)))
                send(30, st["prob"])
                save(st)
            elif cc == 76:                                # top-left encoder click
                held = v >= 64
            elif cc == 60:                                # arcade
                if v >= 64 and held:
                    continue                              # tap-tempo gesture, not a note
                if v >= 64:
                    if arc_open is not None:              # double press: close first
                        vals.pop(arc_open, None); send(arc_open, 0)
                    arc_cc = 31 if arc_cc == 38 else 38
                    arc_open = arc_cc
                    vals.pop(arc_cc, None)                # always pass edges through
                    send(arc_cc, 127)
                elif arc_open is not None:
                    vals.pop(arc_open, None)
                    send(arc_open, 0)
                    arc_open = None
            elif cc in SLOT_CC:                           # radio patch select
                if v < 64 or (switching and switching.poll() is None):
                    continue
                patches = sorted(os.path.basename(p) for p in glob.glob(os.path.join(PATCH_DIR, "*.vcv")))
                i = SLOT_CC[cc]
                try:
                    cur = open(os.path.expanduser("~/vcv/state/current")).read().strip()
                except OSError:
                    cur = ""
                if i < len(patches) and patches[i] != cur:
                    print(f"slot {i + 1}: {patches[i]}", flush=True)
                    switching = subprocess.Popen(["bash", RACK, "start", patches[i]],
                                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                                 start_new_session=True)
            elif cc in ROUTE:
                tgt = ROUTE[cc]
                s = smooth.get(cc, v)
                s += SMOOTH * (v - s)
                smooth[cc] = s
                send(tgt, s)
        now = time.time()
        if now - last_resend >= RESEND_S:
            last_resend = now
            for cc, v in list(vals.items()):
                if cc not in (31, 38):
                    out.send(mido.Message("control_change", channel=CH, control=cc, value=v))
        time.sleep(0.002)


if __name__ == "__main__":
    main()
