#!/usr/bin/env python3
"""effect_ctl.py — deterministic effect on/off control for NEPTR Phase 4,
driven entirely over MIDI (claude_midi.sh) + a brief OSC state snapshot.
Runs ON THE PI (uses live paths under $HOME, whoever that is).

Why this exists: dsp_check.py needs to turn a SPECIFIC effect fully ON
(not just "toggle", which could turn it OFF if it was already on from a
previous session/manual use) without guessing at gkCurrentMenu, and
without permanently binding NEPTR's OSC feedback port (8000) — that port
already has exactly one listener, neptr-ui.service, and stealing packets
from it with a second bound socket would silently drop UI updates on the
touchscreen. So: briefly stop neptr-ui, bind 8000 ourselves, ask the
engine for a full state re-broadcast (ch16 CC102, the same "sync request"
neptr-ui itself sends at startup), read menu + every relevant bypass/row
flag from that snapshot, then immediately restart neptr-ui. The window is
under ~1.5s and is *always* closed via try/finally, mirroring the
"rewires temporarily, always restores" pattern in testing/latency/.

Usage:
    effect_ctl.py snapshot                 # print full OSC state dump (JSON)
    effect_ctl.py ir-on <row 0-4>           # force IR VERB row N ON (menu 18)
    effect_ctl.py ir-off                    # force IR VERB fully bypassed
    effect_ctl.py gvb-on                    # force GRAINVERB (menu 9, slot1) ON
    effect_ctl.py gvb-off                   # force GRAINVERB OFF
    effect_ctl.py goto-menu <n>             # navigate to menu n (0-18), no toggle
"""
import json
import os
import socket
import struct
import subprocess
import sys
import time

CLAUDE_MIDI = os.path.expanduser("~/NEPTR_phase4/scripts/claude_midi.sh")
UI_SERVICE = "neptr-ui.service"
OSC_PORT = 8000
MENU_COUNT = 19

# --------------------------------------------------------------------
# CRITICAL: csound/handler/handler_remap.orc applies a PHYSICAL->LOGICAL
# CC remap to every incoming CC (from the Teensy AND from anything else
# on the same ALSA seq bus, including claude_midi.sh injections) before
# handler_menus.orc's dispatch chain ever sees it. The dispatch chain
# (nav.orc, toggle_slotN.orc) checks LOGICAL CC numbers. To make a
# logical event happen, you must inject the RAW/PHYSICAL CC that the
# remap turns INTO that logical number — sending the logical number
# directly gets remapped AGAIN into something else entirely (e.g.
# sending logical CC73 "menu next" directly is treated as raw CC73,
# which the remap turns into logical CC75 = slot1 toggle — so it
# silently toggles an unrelated effect instead of changing menus).
#
# Inverse table, read directly off handler/handler_remap.orc:
#   want logical CC70 (menu PREV)  -> send raw CC76
#   want logical CC73 (menu NEXT)  -> send raw CC74
#   want logical CC74 (slot0 tgl)  -> send raw CC70
#   want logical CC75 (slot1 tgl)  -> send raw CC73
#   want logical CC76 (slot2 tgl)  -> send raw CC72
#   want logical CC71 (slot3 tgl)  -> send raw CC71 (unchanged)
#   want logical CC72 (slot4 tgl)  -> send raw CC75
# --------------------------------------------------------------------
SLOT_CC = {0: 70, 1: 73, 2: 72, 3: 71, 4: 75}   # RAW CCs to send
MENU_NEXT_CC = 74   # RAW CC to send for logical "menu next" (73)
MENU_PREV_CC = 76   # RAW CC to send for logical "menu prev" (70)

IR_MENU = 18
GVB_MENU = 9
GVB_SLOT = 1


def midi(*args):
    subprocess.run([CLAUDE_MIDI, *[str(a) for a in args]],
                    check=True, capture_output=True, text=True)


def tap(cc, ch=1):
    midi("tap", cc, ch)
    time.sleep(0.06)


# ---------------------------------------------------------------- OSC ----
def _decode_osc(data):
    """Minimal OSC 1.0 message decoder: address + ,typetag + args (i/f only)."""
    def cstr(buf, off):
        end = buf.index(b"\x00", off)
        s = buf[off:end].decode("utf-8", "replace")
        pad = (end + 4) & ~3  # next 4-byte boundary after the NUL
        return s, pad

    addr, off = cstr(data, 0)
    if not addr.startswith("/"):
        return None
    tags, off = cstr(data, off)
    if not tags.startswith(","):
        return addr, []
    vals = []
    for t in tags[1:]:
        if t == "f":
            vals.append(struct.unpack(">f", data[off:off + 4])[0]); off += 4
        elif t == "i":
            vals.append(struct.unpack(">i", data[off:off + 4])[0]); off += 4
        else:
            break
    return addr, vals


def _snapshot_once(window=1.2):
    """Stop neptr-ui, bind :8000, pulse CC102 sync request (several times,
    in case one pulse's UDP packet or the engine's reply is dropped),
    collect every OSC message for `window` seconds, restart neptr-ui,
    return dict."""
    subprocess.run(["sudo", "systemctl", "stop", UI_SERVICE], check=True)
    state = {}
    try:
        time.sleep(0.15)  # let the old UI's socket actually close
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(("127.0.0.1", OSC_PORT))
        sock.settimeout(0.05)
        deadline = time.time() + window
        next_pulse = 0.0
        while time.time() < deadline:
            if time.time() >= next_pulse:
                midi("cc", 102, 127, 16)   # sync request pulse
                next_pulse = time.time() + 0.3   # repeat every 300ms
            try:
                data, _ = sock.recvfrom(4096)
            except socket.timeout:
                continue
            r = _decode_osc(data)
            if r and r[1]:
                state[r[0]] = r[1][0]
        sock.close()
    finally:
        subprocess.run(["sudo", "systemctl", "start", UI_SERVICE], check=True)
    return state


def snapshot(window=1.2, min_keys=40, retries=3):
    """Robust wrapper around _snapshot_once: the engine's CC102 full-state
    rebroadcast is observed to sometimes only partially arrive (UDP loss /
    neptr-ui-restart race) — a snapshot with too few keys silently makes
    every ensure_on/ensure_off/already_on decision downstream unreliable
    (this was root-caused 2026-08-23: dsp_check reported IR reverb SILENT
    live even after the tablecopy DSP fix landed and an isolated offline
    render at ksmps=64 proved the DSP itself works — the live "SILENT"
    was actually effect_ctl never successfully engaging the effect because
    ir_on()'s already_on check was reading a stale/incomplete state dict).
    Retries the whole stop/bind/pulse/restart cycle up to `retries` times
    until at least `min_keys` OSC addresses are captured."""
    best = {}
    for attempt in range(retries):
        st = _snapshot_once(window)
        if len(st) > len(best):
            best = st
        if len(st) >= min_keys:
            return st
        time.sleep(0.2)
    return best


def current_menu():
    st = snapshot()
    return int(st.get("/menu", 0)), st


def goto_menu(target):
    cur, _ = current_menu()
    steps = (target - cur) % MENU_COUNT
    for _ in range(steps):
        tap(MENU_NEXT_CC)
    return cur, steps


# ---------------------------------------------------------- IR / GVB -----
def ir_on(row):
    assert 0 <= row <= 4
    cur, st = current_menu()
    steps = (IR_MENU - cur) % MENU_COUNT
    for _ in range(steps):
        tap(MENU_NEXT_CC)
    already_on = st.get(f"/ir{row}_state", 1) == 0  # 0=on, 1=off (see handler_osc.orc)
    if already_on:
        return  # already the active row and unbypassed — nothing to do
    tap(SLOT_CC[row])
    return


def ir_off():
    cur, st = current_menu()
    steps = (IR_MENU - cur) % MENU_COUNT
    for _ in range(steps):
        tap(MENU_NEXT_CC)
    # find whichever row (if any) is currently ON and tap its slot again
    # (same-row tap = OFF per toggle_slotN.orc)
    for row in range(5):
        if st.get(f"/ir{row}_state", 1) == 0:
            tap(SLOT_CC[row])
            return
    # nothing was on — nothing to do


def gvb_on():
    cur, st = current_menu()
    steps = (GVB_MENU - cur) % MENU_COUNT
    for _ in range(steps):
        tap(MENU_NEXT_CC)
    already_on = st.get("/gvb_state", 1) == 0
    if already_on:
        return
    tap(SLOT_CC[GVB_SLOT])


def gvb_off():
    cur, st = current_menu()
    steps = (GVB_MENU - cur) % MENU_COUNT
    for _ in range(steps):
        tap(MENU_NEXT_CC)
    if st.get("/gvb_state", 1) == 0:
        tap(SLOT_CC[GVB_SLOT])


def ensure_on(menu, slot, state_addr):
    """Navigate to `menu`, and tap `slot`'s toggle ONLY if `state_addr`
    (an OSC state address, e.g. '/od_state') does not already read 0 (on).
    Blind toggling is unsafe — if the effect was already on from a
    previous test, a blind tap turns it OFF instead of leaving it on."""
    cur, st = current_menu()
    steps = (menu - cur) % MENU_COUNT
    for _ in range(steps):
        tap(MENU_NEXT_CC)
    already_on = st.get(state_addr, 1) == 0
    if not already_on:
        tap(SLOT_CC[slot])


def ensure_off(menu, slot, state_addr):
    cur, st = current_menu()
    steps = (menu - cur) % MENU_COUNT
    for _ in range(steps):
        tap(MENU_NEXT_CC)
    is_on = st.get(state_addr, 1) == 0
    if is_on:
        tap(SLOT_CC[slot])


def main():
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(1)
    cmd = sys.argv[1]
    if cmd == "snapshot":
        print(json.dumps(snapshot(), indent=2, sort_keys=True))
    elif cmd == "ir-on":
        ir_on(int(sys.argv[2])); print(f"ir row {sys.argv[2]} -> ON")
    elif cmd == "ir-off":
        ir_off(); print("ir -> OFF")
    elif cmd == "gvb-on":
        gvb_on(); print("grainverb -> ON")
    elif cmd == "gvb-off":
        gvb_off(); print("grainverb -> OFF")
    elif cmd == "goto-menu":
        c, s = goto_menu(int(sys.argv[2])); print(f"menu {c} -> {sys.argv[2]} ({s} taps)")
    else:
        print(f"unknown command: {cmd}"); sys.exit(1)


if __name__ == "__main__":
    main()
