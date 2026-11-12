"""A serialosc that lives on YOUR computer and points at the neuralGrid.

Why this file has to exist
--------------------------
Monome applications do not look for grids on the network. Every one of
them -- norns' matron, the Max objects, the SuperCollider library --
opens `127.0.0.1:12002` and asks the *local* serialosc daemon what is
plugged in. serialosc's whole job is enumerating *local USB*. Nothing in
that chain ever considers an IP address, so no amount of advertising
from the neuralGrid can make a stock client find it: the client is not
listening for it.

The honest fix would be to be a USB device, which is what a monome is.
This Pi cannot: it has no USB device controller at all (`/sys/class/udc`
is empty; every controller is host-only xhci), and even with one,
serialosc identifies grids as FTDI FT245R chips (VID 0x0403) with an
`m#######` serial speaking monome's serial protocol -- which would mean
writing a device-side FTDI emulation.

So: run this on the computer that wants a grid. It IS a serialosc as far
as your application is concerned, and it forwards to the neuralGrid. It
is served BY the grid with the grid's address already baked in, so there
is nothing to configure:

    curl -s http://neuralgrid.local:8767/monome-bridge | python3 -

Stdlib only, one file, no install. Ctrl-C to stop.
"""
from __future__ import annotations

import socket
import struct
import sys
import threading

# Filled in by the neuralGrid when it serves this file; the fallbacks are
# for running it straight out of the repo.
GRID_HOST = "neuralgrid.local"
GRID_PORT = 15000
DEVICE_ID = "neuralgrid"
DEVICE_TYPE = "monome 64"

SERIALOSC_PORT = 12002   # where every monome app looks. Not negotiable.
BRIDGE_DEVICE_PORT = 15009


# -- OSC, again, because this file cannot import anything ----------------

def _pad_str(s: str) -> bytes:
    b = s.encode("utf-8") + b"\x00"
    return b + b"\x00" * ((4 - len(b) % 4) % 4)


def osc_encode(address: str, *args) -> bytes:
    tags, body = ",", b""
    for a in args:
        if isinstance(a, bool):
            tags += "i"; body += struct.pack(">i", 1 if a else 0)
        elif isinstance(a, int):
            tags += "i"; body += struct.pack(">i", a)
        elif isinstance(a, float):
            tags += "f"; body += struct.pack(">f", a)
        else:
            tags += "s"; body += _pad_str(str(a))
    return _pad_str(address) + _pad_str(tags) + body


def osc_decode(data: bytes):
    try:
        if not data or data[0:1] != b"/":
            return None
        end = data.index(b"\x00")
        address = data[:end].decode("utf-8")
        i = (end + 4) & ~3
        if i >= len(data) or data[i:i + 1] != b",":
            return (address, [])
        tend = data.index(b"\x00", i)
        tags = data[i + 1:tend].decode("ascii")
        i = (tend + 4) & ~3
        args = []
        for t in tags:
            if t in "if":
                fmt = ">i" if t == "i" else ">f"
                args.append(struct.unpack(fmt, data[i:i + 4])[0]); i += 4
            elif t == "s":
                send = data.index(b"\x00", i)
                args.append(data[i:send].decode("utf-8", "replace"))
                i = (send + 4) & ~3
            else:
                return (address, args)
        return (address, args)
    except Exception:
        return None


def local_ip_towards(host: str) -> str:
    """Which of our addresses the grid will see us as."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect((host, 9))
        return s.getsockname()[0]
    except OSError:
        return "127.0.0.1"
    finally:
        s.close()


class Bridge:
    def __init__(self, grid_host=GRID_HOST, grid_port=GRID_PORT,
                 serialosc_port=SERIALOSC_PORT, device_port=BRIDGE_DEVICE_PORT):
        self.grid = (grid_host, grid_port)
        self.serialosc_port = serialosc_port
        self.device_port = device_port
        self.app_addr = None          # where the application wants its replies
        self.watchers = set()         # one-shot /serialosc/notify subscribers
        self.my_ip = local_ip_towards(grid_host)
        self._disc = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._dev = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._stop = threading.Event()

    # -- discovery half: pretend to be the local serialosc daemon -------
    def _serve_discovery(self):
        while not self._stop.is_set():
            try:
                data, addr = self._disc.recvfrom(4096)
            except OSError:
                return
            msg = osc_decode(data)
            if not msg:
                continue
            address, args = msg
            if address not in ("/serialosc/list", "/serialosc/notify"):
                continue
            try:
                to = (str(args[0]), int(args[1]))
            except Exception:
                continue
            if address == "/serialosc/list":
                self._disc.sendto(
                    osc_encode("/serialosc/device", DEVICE_ID, DEVICE_TYPE,
                               self.device_port), to)
                print(f"  app asked what exists -> told it {DEVICE_ID} "
                      f"on {self.device_port}")
            else:
                # notify means "tell me when this CHANGES". Answering it
                # straight away spins a client that re-lists on every add
                # (matron does) through list -> notify -> add -> list.
                self.watchers.add(to)

    # -- device half: relay between the app and the grid ----------------
    def _serve_device(self):
        while not self._stop.is_set():
            try:
                data, addr = self._dev.recvfrom(4096)
            except OSError:
                return
            if addr[0] == self.grid[0] and addr[1] == self.grid[1]:
                self._from_grid(data)
            else:
                self._from_app(data, addr)

    def _announce_route(self):
        """Tell the grid to send key presses HERE.

        A grid sends keys to whatever /sys/host + /sys/port it was told,
        and nowhere at all until it is told. Most apps send both, but an
        app that only drives lights would otherwise leave a grid that
        looks perfectly alive and whose presses vanish.
        """
        self._dev.sendto(osc_encode("/sys/host", self.my_ip), self.grid)
        self._dev.sendto(osc_encode("/sys/port", self.device_port), self.grid)

    def _from_app(self, data: bytes, addr):
        if addr != self.app_addr:
            self.app_addr = addr
            self._announce_route()
        self.app_addr = addr
        msg = osc_decode(data)
        if msg:
            address, args = msg
            # The app tells the device where to send key presses -- but
            # it names ITSELF, and the grid is on another machine whose
            # replies must come back through here. Point them at us.
            if address in ("/sys/info", "/sys/host", "/sys/port") and args:
                if address == "/sys/host":
                    args = [self.my_ip]
                elif address == "/sys/port":
                    args = [self.device_port]
                else:
                    args = [self.my_ip, self.device_port]
                data = osc_encode(address, *args)
        self._dev.sendto(data, self.grid)

    def _from_grid(self, data: bytes):
        if self.app_addr is None:
            return   # nothing has asked yet; a key with nowhere to go
        self._dev.sendto(data, self.app_addr)

    def start(self):
        try:
            self._disc.bind(("127.0.0.1", self.serialosc_port))
        except OSError as e:
            print(f"\ncannot take port {self.serialosc_port}: {e}")
            print("Something is already the serialosc on this machine --")
            print("on norns that is serialoscd itself. Stop it first:")
            print("    sudo systemctl stop serialosc")
            print("then run this again. (A real USB grid will not work")
            print("while this bridge is up, for the same reason.)")
            return False
        self._dev.bind(("0.0.0.0", self.device_port))
        for fn in (self._serve_discovery, self._serve_device):
            threading.Thread(target=fn, daemon=True).start()
        print(f"neuralGrid bridge up")
        print(f"  grid      : {self.grid[0]}:{self.grid[1]}")
        print(f"  serialosc : 127.0.0.1:{self.serialosc_port}  (what your app asks)")
        print(f"  device    : {self.my_ip}:{self.device_port}")
        print(f"\nStart your patch/script now -- it will find "
              f"'{DEVICE_ID}' as a {DEVICE_TYPE}.\n")
        return True

    def stop(self):
        self._stop.set()
        for s in (self._disc, self._dev):
            try:
                s.close()
            except OSError:
                pass


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    host = argv[0] if argv else GRID_HOST
    port = int(argv[1]) if len(argv) > 1 else GRID_PORT
    bridge = Bridge(grid_host=host, grid_port=port)
    if not bridge.start():
        return 1
    try:
        threading.Event().wait()
    except KeyboardInterrupt:
        print("\nbridge down")
        bridge.stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
