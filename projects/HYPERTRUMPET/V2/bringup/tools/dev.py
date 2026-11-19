# dev.py: tiny serial harness for the 12_drone tests. usage: dev.py <script.py-like steps>; imported by the test scripts.
import serial, time, json, threading
P = '/dev/cu.usbmodem327F347733331'
class Dev:
    def __init__(s):
        s.s = serial.Serial(P, 115200, timeout=0.05); s.fr = []; s.run = True; s.buf = b''
        s.t = threading.Thread(target=s._rd, daemon=True); s.t.start()
    def _rd(s):
        while s.run:
            try: d = s.s.read(4096)
            except Exception: break
            if not d: continue
            s.buf += d
            while b'\n' in s.buf:
                l, s.buf = s.buf.split(b'\n', 1)
                try: m = json.loads(l)
                except Exception: continue
                m['_t'] = time.time(); s.fr.append(m)
    def send(s, c): s.s.write((c + '\n').encode()); s.s.flush()
    def mark(s): return len(s.fr)
    def since(s, i, key): return [m[key] for m in s.fr[i:] if key in m]
    def close(s): s.run = False; time.sleep(0.1); s.s.close()
