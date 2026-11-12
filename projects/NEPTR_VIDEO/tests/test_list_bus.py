#!/usr/bin/env python3
"""The enumeration and direct-selection verbs, over a real socket.

The wire FORMAT is pinned in test_control.cpp; this is the other half -- that a
RUNNING engine answers /nv/list with its actual bank, and that the direct
selection verbs move the actual selection and come back in /nv/sel. A surface
that draws a grid of thirty buttons depends on both halves, and neither can be
checked by reading the encoder.

  ./tests/test_list_bus.py [path/to/neptr-video]
"""
import socket, struct, subprocess, sys, time, os
def pad(b):
    b += b'\0'; return b + b'\0' * ((4 - len(b) % 4) % 4)
def msg(addr, *args):
    tags = ','; body = b''
    for a in args:
        if isinstance(a, str): tags += 's'; body += pad(a.encode())
        elif isinstance(a, int): tags += 'i'; body += struct.pack('>i', a)
        else: tags += 'f'; body += struct.pack('>f', a)
    return pad(addr.encode()) + pad(tags.encode()) + body
def parse(data):
    out = []
    def one(d):
        i = d.index(b'\0'); addr = d[:i].decode()
        p = (i + 4) & ~3
        j = d.index(b'\0', p); tags = d[p:j].decode(); p = ((j + 4) & ~3)
        args = []
        for t in tags[1:]:
            if t == 'i': args.append(struct.unpack('>i', d[p:p+4])[0]); p += 4
            elif t == 'f': args.append(struct.unpack('>f', d[p:p+4])[0]); p += 4
            elif t == 's':
                k = d.index(b'\0', p); args.append(d[p:k].decode()); p = ((k + 4) & ~3)
        out.append((addr, args))
    if data[:8] == b'#bundle\0':
        p = 16
        while p < len(data):
            n = struct.unpack('>i', data[p:p+4])[0]; one(data[p+4:p+4+n]); p += 4 + n
    else:
        one(data)
    return out

PORT = 8104
env = dict(os.environ, NV_HEADLESS='1', NV_HOME='0', NV_SINGLE_WINDOW='1', NV_GEOM='320x240',
           NV_OSC_PORT=str(PORT), NV_MAX_SECONDS='30', NV_FIXED_DT='0.0166')
BINARY = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'build',
    'neptr-video')
proc = subprocess.Popen([BINARY], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(2.5)
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.settimeout(3.0)
s.sendto(msg('/nv/list', 'kernel'), ('127.0.0.1', PORT))
items, end = [], None
t0 = time.time()
while time.time() - t0 < 3.0:
    try: d, _ = s.recvfrom(65535)
    except socket.timeout: break
    for addr, a in parse(d):
        if addr == '/nv/list_item': items.append(a)
        elif addr == '/nv/list_end': end = a
    if end: break
print('items', len(items), 'end', end)
for a in items[:3] + items[-2:]: print(' ', a)
# direct select + read back /nv/sel
s.sendto(msg('/nv/kernel', 12), ('127.0.0.1', PORT))
s.sendto(msg('/nv/color', 4), ('127.0.0.1', PORT))
s.sendto(msg('/nv/palette', 1, 9), ('127.0.0.1', PORT))
# Collect for a while and keep the LAST one: the first tick out is whatever
# the selection was when the datagram was queued, which may predate the
# commands we just sent.
sel = None; t0 = time.time()
while time.time() - t0 < 2.0:
    try: d, _ = s.recvfrom(65535)
    except socket.timeout: break
    for addr, a in parse(d):
        if addr == '/nv/sel': sel = a
print('sel', sel)
proc.terminate()
ok = (end == ['kernel', 30] and len(items) == 30 and sel and sel[1] == 12
      and sel[3] == 4 and sel[6] == 9)
print('PASS' if ok else 'FAIL')
sys.exit(0 if ok else 1)
