#!/usr/bin/env python3
"""Overnight soak runner for the HYPERTRUMPET V2 instrument Seed (test 10, bringup-1.8+).  venv python only:
    ../../.venv/bin/python tools/soak.py [--port /dev/cu.usbmodem327F347733331] [--stress-min 40] [--seed 12345] [--quick] [--mock]
Phases: 1 MAP (every row of docs/V1_MAP_AUDIT.md through the sim inputs, asserted via `state`), 2 PER-FX (each effect alone, mix 1.0, 60 s, mute on, param sweep),
3 STRESS (everything, seeded random actions). Each phase is one invocation of at most about 5 minutes: --phase map | fx1 | fx2 | fx3 | fx4 | stress (default stress 4 min, fx 6 s windows). Writes logs/soak_<ts>.jsonl and logs/soak_<ts>_summary.md.
The web page owns the serial port: POST `__release 3600` first (re-sent every 50 min) and `__take` at exit. Never sends dfu, never flashes.
"""
import sys, os, time, json, threading, queue, random, argparse, glob, math, datetime, urllib.request, traceback

LOGDIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'logs')
CMD_URL = 'http://127.0.0.1:8731/cmd'
IDS = ['reverb', 'drone', 'harm', 'looper', 'freeze', 'blur', 'flange', 'chorus', 'mood', 'grain', 'delay', 'roar', 'ring', 'wah', 'autopan', 'harmony']   # registry order = telemetry order
GI = {n: i for i, n in enumerate(IDS)}
CHAIN_V1 = [2, 15, 3, 4, 1, 5, 6, 14, 7, 8, 0, 9, 10, 11, 12, 13]     # v1 order (harmonizer, looper, freeze, drone, blur, flanger, chorus, MOOD, reverb, grain, delay, roar, ringmod) + autopan after the flanger (v1 had it off the audio path) + wah last

def post(cmd):
    try:
        req = urllib.request.Request(CMD_URL, data=cmd.encode(), headers={'x-ht': '1'}, method='POST'); return urllib.request.urlopen(req, timeout=3).read().decode()
    except Exception as e:
        return 'ERR %s' % e

class Result:
    def __init__(self): self.rows = []; self.events = []; self.scen = []
    def add(self, phase, name, ok, measured='', expect='', sev='MED'):
        self.rows.append(dict(phase=phase, name=name, ok=bool(ok), measured=measured, expect=expect, sev=sev))
        print(('PASS ' if ok else 'FAIL ') + '[%s] %s   measured=%s expect=%s' % (phase, name, measured, expect), flush=True)
        return ok

class Dev:
    def __init__(self, port, logf):
        import serial
        self.ser = serial.Serial(port, 115200, timeout=0.2); self.ser.dtr = True; self.ser.rts = True
        self.q_state = queue.Queue(); self.q_pd = queue.Queue(); self.frame = {}; self.hello = {}; self.hello_ts = 0.0; self.tr = {}; self.last_frame_ts = 0.0
        self.olh = []; self.logf = logf; self.lock = threading.Lock(); self.stop = False; self.lastlog = 0.0; self.err = None; self.frames = 0; self.t0 = time.time()
        self.th = threading.Thread(target=self._rd, daemon=True); self.th.start()
    def log(self, kind, **kw):
        with self.lock: self.logf.write(json.dumps(dict(ts=round(time.time() - self.t0, 3), k=kind, **kw)) + '\n'); self.logf.flush()
    def _rd(self):
        buf = b''
        while not self.stop:
            try: d = self.ser.read(4096)
            except Exception as e: self.err = str(e); self.log('serial_error', err=str(e)); return
            if not d: continue
            buf += d
            while b'\n' in buf:
                l, buf = buf.split(b'\n', 1)
                try: m = json.loads(l.decode(errors='ignore'))
                except Exception: continue
                t = m.get('t')
                if t == 's':
                    self.frame.update(m); self.last_frame_ts = time.time(); self.frames += 1
                    if 'tr' in m: self.tr = m['tr']
                    if 'ol' in m: self.olh.append((time.time(), m['ol'])); self.olh = self.olh[-2000:]
                    if time.time() - self.lastlog >= 1.0:
                        self.lastlog = time.time(); self.log('s', **{k: m[k] for k in ('fs', 'fx', 'cpu', 'st', 'tr', 'in', 'ov', 'lp') if k in m})
                elif t in ('hello', 'hello2'):
                    self.hello.update({k: v for k, v in m.items() if k != 't'}); self.hello_ts = time.time(); self.log(t, **{k: v for k, v in m.items() if k != 't'})
                elif t == 'state': self.q_state.put(m); self.log('state', **{k: v for k, v in m.items() if k != 't'})
                elif t == 'pd': self.q_pd.put(m)
                elif t in ('perr',): self.log('perr', **m)
    def send(self, line): self.ser.write((line + '\n').encode()); self.ser.flush()
    def state(self, timeout=2.0):
        while not self.q_state.empty(): self.q_state.get_nowait()
        self.send('state')
        try: return self.q_state.get(timeout=timeout)
        except queue.Empty: return None
    def hello_ok(self, timeout=1.0):
        t0 = time.time(); self.send('hello')
        while time.time() - t0 < timeout:
            if self.hello_ts >= t0: return True
            time.sleep(0.02)
        return False
    def close(self):
        self.stop = True; time.sleep(0.3)
        try: self.ser.close()
        except Exception: pass

class Mock:                                    # --mock: exercises the runner without hardware (all MAP rows will FAIL, nothing may raise)
    def __init__(self): self.frame = {'fx': [[0, 0, 0, 0, 0]] * 16, 'cpu': {'a': 1, 'p': 2, 's': 3}, 'st': {'xrun': 0, 'nan': [0] * 16, 'maxcyc': [0] * 16, 'cb': 0, 'rec': 0, 'gap': 0}, 'tr': {'err': [0] * 5, 're': 0}, 'op': [0, 0]}; self.hello = {'ord': CHAIN_V1, 'lights': {'strip': {'done': 1}}}; self.frames = 1; self.err = None; self.sent = []; self.tr = {'err': [0] * 5}; self.olh = []
    def send(self, l): self.sent.append(l)
    def log(self, kind, **kw): pass
    def state(self, timeout=2.0): return {'ms': int(time.time() * 1000) % 100000000, 'fx': [dict(id=n, ok=1, st=0, lat=0, hold=0, tgt=0, mix=0, ref=0, nan=0, p={}) for n in IDS], 'fs': [0, 0, 0], 'warp': 0, 'dmode': 0, 'smooth': 0, 'joy': 0, 'frz': 0, 'frzclr': 0, 'foc': -1, 'intvl': 5, 'loop': [0, 0, 0], 'mute': 0, 'in': 0, 'hmode': 0, 'hmix': 0.7, 'hv': [0, 0, 0], 'ex': [-1, 0]}
    def hello_ok(self, timeout=1.0): return True
    def close(self): pass
    q_pd = queue.Queue()

def fxs(s): return {f['id']: f for f in s['fx']} if s else {}
def sleep(t): time.sleep(t)

def snap(d):
    f = d.frame; st = f.get('st', {}); sg = f.get('sgf', [0, 0, 0]); tr = f.get('tr', {}); cpu = f.get('cpu', {})
    return dict(t=time.time(), xrun=st.get('xrun', 0), gap=st.get('gap', 0), nan=sum(st.get('nan', [0])), sgn=sg[0], sg=sg[1], sgmax=sg[2], lp33=f.get('lp33', 0), late=tr.get('late', 0), cpu_a=cpu.get('a', 0), cpu_p=cpu.get('p', 0), cpu_s=cpu.get('s', 0), on=sum(1 for x in f.get('fx', []) if x and x[0] > 0))

def delta(a, b):
    dt = max(1e-6, b['t'] - a['t']); return dict(dt=round(dt, 1), xrun=b['xrun'] - a['xrun'], gap=b['gap'] - a['gap'], sg=b['sg'] - a['sg'], sgn=b['sgn'] - a['sgn'], sgmax=b['sgmax'], lp33=b['lp33'] - a['lp33'], late=b['late'] - a['late'], cpu_a=b['cpu_a'] / 10, cpu_s=b['cpu_s'] / 10, on=b['on'], sg_per_min=round((b['sg'] - a['sg']) / dt * 60, 2))

def pearson(x, y):
    n = len(x)
    if n < 3: return None
    mx, my = sum(x) / n, sum(y) / n; sxx = sum((a - mx) ** 2 for a in x); syy = sum((b - my) ** 2 for b in y)
    return None if sxx == 0 or syy == 0 else sum((a - mx) * (b - my) for a, b in zip(x, y)) / math.sqrt(sxx * syy)

class Soak:
    def __init__(self, dev, res, args): self.d = dev; self.R = res; self.a = args; self.sc = 1.0 if not args.quick else 0.25; self.fxsecs = 0.3 if args.quick else args.fx_secs
    def cmd(self, l): self.d.send(l)
    def st(self): return self.d.state()
    def rst(self): self.cmd('sim reset'); sleep(0.9 * self.sc + 0.4)
    def tap(self, n, hold=0.12): self.cmd('sim pad %d down' % n); sleep(hold); self.cmd('sim pad %d up' % n); sleep(0.22)
    def need(self, ph, name, s):
        if s is None: self.R.add(ph, name, False, 'no state reply', 'state', 'CRIT'); return False
        return True
    # ------------------------------------------------------------------ phase 1
    def phase1(self):
        P = 'MAP'; R = self.R; d = self.d
        s = self.st(); 
        if not self.need(P, 'state command answers', s): return
        R.add(P, 'state command answers', True, 'ok', 'json')
        hl = d.hello_ok(1.5); order = d.hello.get('ord')
        R.add(P, 'chain order = v1 (harm, looper, freeze, drone, blur, flange, [autopan], chorus, MOOD, reverb, grain, delay, roar, ringmod, wah)', order == CHAIN_V1, order, CHAIN_V1)
        # latch pads for the sensor-only effects: sensors never switch an effect on
        for n, name in ((1, 'flange'), (2, 'roar'), (6, 'mood'), (15, 'autopan'), (16, 'wah')):
            self.rst(); self.tap(n); sleep(0.2); a = fxs(self.st())[name]['lat']; self.tap(n); sleep(0.2); b = fxs(self.st())[name]['lat']
            R.add(P, 'pad %d = %s on/off latch' % (n, name), (a, b) == (1, 0), (a, b), '(1,0)')
        self.rst(); self.cmd('sim pot 1 800'); self.cmd('sim lidar 333'); self.cmd('sim joy 800 800 0'); sleep(0.6); f = fxs(self.st())
        R.add(P, 'sensors alone never switch an effect on (pot 1, lidar, joystick moved, pads untouched)', all(f[n]['lat'] == 0 and f[n]['st'] == 0 for n in ('roar', 'flange', 'mood', 'autopan', 'wah')), {n: (f[n]['lat'], f[n]['st']) for n in ('roar', 'flange', 'mood')}, 'all off')
        # pad 4 hold + pad 3 clear
        self.rst(); self.cmd('sim pad 4 down'); sleep(0.3); s = self.st(); fz = fxs(s)['freeze']
        R.add(P, 'pad 4 press: freeze switches on (let-in held)', fz['lat'] == 1 and fz['st'] >= 1, (fz['lat'], fz['st']), 'lat 1, running')
        self.cmd('sim pad 4 up'); sleep(0.3); fz = fxs(self.st())['freeze']
        R.add(P, 'pad 4 release: freeze stays on (frozen sound holds)', fz['lat'] == 1, fz['lat'], 1)
        self.tap(3); t0 = time.time(); vals = []
        for tt in (0.4, 1.2, 2.0):
            sleep(max(0, t0 + tt - time.time())); fzz = fxs(self.st())['freeze']; vals.append((round(time.time() - t0, 2), fzz['p'].get('FREEZE')))
        ok = all(v is not None for _, v in vals) and vals[0][1] > vals[1][1] > vals[2][1] and all(abs(v - 100 * (1 - t / 2.3)) < 18 for t, v in vals)
        R.add(P, 'pad 3: freeze factor ramps 100 -> 0 over 2.3 s', ok, vals, 'about 100*(1-t/2.3)')
        sleep(max(0, t0 + 3.2 - time.time())); fz = fxs(self.st())['freeze']
        R.add(P, 'pad 3: after the ramp the factor is back at 100 and the freeze is off', fz['p'].get('FREEZE') == 100 and fz['lat'] == 0, (fz['p'].get('FREEZE'), fz['lat']), '100, lat 0')
        # latch pads: act on release
        for n, name in ((5, 'ring'), (7, 'blur'), (8, 'chorus')):
            self.rst(); self.cmd('sim pad %d down' % n); sleep(0.25); a = fxs(self.st())[name]['lat']; self.cmd('sim pad %d up' % n); sleep(0.3); b = fxs(self.st())[name]['lat']
            self.tap(n); sleep(0.2); c = fxs(self.st())[name]['lat']
            R.add(P, 'pad %d %s latch: off while pressed, on after release, off after the next tap' % (n, name), (a, b, c) == (0, 1, 0), (a, b, c), '(0,1,0)')
        # delay pads
        self.rst(); self.cmd('sim fsr 500'); self.tap(9); s = self.st(); dl = fxs(s)['delay']
        R.add(P, 'pad 9: delay on', s['dmode'] == 1 and dl['lat'] == 1, (s['dmode'], dl['lat']), '(1,1)')
        self.tap(11); sleep(0.3); s = self.st(); dl = fxs(s)['delay']
        R.add(P, 'pad 11: preset A (mix 1.0, feedback 0.787), pad 9 off (exclusive)', s['dmode'] == 2 and abs(dl['tgt'] - 1.0) < 0.01 and abs(dl['p'].get('FB', 0) - 0.787) < 0.01, (s['dmode'], dl['tgt'], dl['p'].get('FB')), '(2, 1.0, 0.787)')
        self.tap(12); sleep(0.4); s = self.st(); dl = fxs(s)['delay']; tm = dl['p'].get('TIME', 0)
        R.add(P, 'pad 12: preset B (mix 0.31, feedback 0.24), pad 11 off', s['dmode'] == 3 and abs(dl['tgt'] - 0.31) < 0.01 and abs(dl['p'].get('FB', 0) - 0.24) < 0.01, (s['dmode'], dl['tgt'], dl['p'].get('FB')), '(3, 0.31, 0.24)')
        R.add(P, 'pad 12 on: FSR drives delay time = 600 ms x sqrt(FSR) (FSR 0.5 -> 424 ms)', abs(tm - 424.3) < 8, tm, 424.3)
        self.cmd('sim fsr 1000'); sleep(0.3); tm = fxs(self.st())['delay']['p'].get('TIME', 0); R.add(P, 'pad 12: FSR 1.0 -> 600 ms', abs(tm - 600) < 8, tm, 600)
        self.tap(12); sleep(0.3); s = self.st(); R.add(P, 'pad 12 again: delay off', s['dmode'] == 0 and fxs(s)['delay']['lat'] == 0, (s['dmode'], fxs(s)['delay']['lat']), '(0,0)')
        self.tap(10); s = self.st(); a = s['smooth']; self.tap(10); s = self.st(); R.add(P, 'pad 10: smooth-mode latch toggles', (a, s['smooth']) == (1, 0), (a, s['smooth']), '(1,0)')
        # looper
        self.rst(); self.tap(13); sleep(0.4); s = self.st(); lp = s['loop']
        R.add(P, 'pad 13 tap: looper on, recording', fxs(s)['looper']['lat'] == 1 and lp[0] == 1, lp, 'state 1')
        sleep(1.5); self.tap(13); sleep(0.8); s = self.st(); lp = s['loop']
        R.add(P, 'pad 13 tap: record closes, loop plays (length > 0)', lp[0] == 2 and lp[2] > 0, lp, 'state 2, len > 0')
        self.tap(14); sleep(0.4); s = self.st(); R.add(P, 'pad 14 tap while playing: stop', s['loop'][0] == 4, s['loop'], 'state 4')
        self.tap(14); sleep(0.4); s = self.st(); R.add(P, 'pad 14 tap while stopped: play', s['loop'][0] == 2, s['loop'], 'state 2')
        self.cmd('sim pad 13 down'); sleep(0.8); self.cmd('sim pad 13 up'); sleep(0.9); s = self.st(); R.add(P, 'pad 13 hold: delete all', s['loop'][1] == 0 and s['loop'][0] == 0, s['loop'], 'layers 0, state 0')
        # focus
        self.rst(); self.cmd('sim pad 8 down'); sleep(0.9); self.cmd('sim pad 8 up'); sleep(0.3); s = self.st()
        R.add(P, 'pad 8 long press focuses chorus without toggling it', s['foc'] == GI['chorus'] and fxs(s)['chorus']['lat'] == 0, (s['foc'], fxs(s)['chorus']['lat']), (GI['chorus'], 0))
        # faders
        for k, (tgt, label) in enumerate((('reverb', 'reverb'), ('drone', 'drone'), ('grain', 'grain'))):
            self.rst(); self.cmd('sim fader %d 0' % (k + 1)); sleep(0.5); a = self.st()['fs'][k]
            self.cmd('sim fader %d 1000' % (k + 1)); sleep(0.7); s = self.st(); b = s['fs'][k]; tg = fxs(s)[tgt]['tgt'] if tgt else None
            self.cmd('sim fader %d 500' % (k + 1)); sleep(0.5); m = self.st()['fs'][k]
            self.cmd('sim fader %d 0' % (k + 1)); sleep(0.5); s2 = self.st(); z = s2['fs'][k]
            R.add(P, 'fader %d -> %s: 0 reads exactly 0, 1000 exactly 1.0, 500 about 0.5, back to exactly 0' % (k + 1, label), a == 0 and b == 1.0 and 0.4 < m < 0.6 and z == 0, (a, b, m, z), '(0, 1.0, ~0.5, 0)')
            if tgt: R.add(P, 'fader %d drives the %s mix target (1.0 at the top)' % (k + 1, tgt), tg is not None and tg > 0.99, tg, 1.0)
        # accel
        self.rst(); bad = []
        for y in (-1.0, -0.5, 0.0, 0.5, 1.0):
            self.cmd('sim accel 0 %g 0' % y); sleep(0.35); w = self.st()['warp']; t = (y + 1) / 2; e = min(1, max(0, 1.236 * t - 0.118))
            if abs(w - e) > 0.02: bad.append((y, w, round(e, 3)))
        R.add(P, 'accel Y -> reverb warp (v1 curve 1.236 t - 0.118, clipped)', not bad, bad or 'all within 0.02', 'curve')
        # pot / fsr / lidar
        self.rst(); self.cmd('sim pot 1 0'); sleep(0.3); r0 = fxs(self.st())['roar']
        self.cmd('sim pot 1 500'); sleep(0.4); r1 = fxs(self.st())['roar']; self.cmd('sim pot 1 1000'); sleep(0.3); r2 = fxs(self.st())['roar']; self.cmd('sim pot 1 10'); sleep(0.4); r3 = fxs(self.st())['roar']
        R.add(P, 'pot 1 -> roar DRIVE (the pot never switches the roar: pad 2 does)', r1['lat'] == 0 and abs(r1['p'].get('DRIVE', -1) - 0.5) < 0.02 and abs(r2['p'].get('DRIVE', -1) - 1.0) < 0.02 and r3['lat'] == 0, (r1['lat'], r1['p'].get('DRIVE'), r2['p'].get('DRIVE'), r3['lat']), '(0, 0.5, 1.0, 0)')
        self.rst(); out = []
        for f_ in (0, 250, 500, 1000):
            self.cmd('sim fsr %d' % f_); sleep(0.35); out.append((f_, fxs(self.st())['delay']['p'].get('TIME'), round(max(48, 600 * (f_ / 1000) ** 0.5), 1)))
        R.add(P, 'FSR is ALWAYS delay time: 600 ms x sqrt(FSR), min 48 ms (delay not even switched on)', all(t is not None and abs(t - e) < 8 for _, t, e in out), out, 'law')
        self.rst(); self.tap(1); out = []
        for mm, e in ((56, 0.0), (194.5, 0.5), (333, 1.0), (400, 1.0)):
            self.cmd('sim lidar %g' % mm); sleep(0.35); tg = fxs(self.st())['flange']['tgt']; out.append((mm, tg, e))
        R.add(P, 'lidar (inverted, far = more) -> flanger mix = (mm-56)/(333-56) while pad 1 has it on', all(abs(t - e) < 0.03 for _, t, e in out), out, 'curve')
        self.rst(); self.cmd('sim lidar 333'); self.tap(15); sleep(0.5); ap = fxs(self.st())['autopan']; a1 = (ap['p'].get('DEPTH'), ap['p'].get('PERIOD'))
        self.cmd('sim lidar 194.5'); sleep(0.4); ap = fxs(self.st())['autopan']; a2 = (ap['p'].get('DEPTH'), ap['p'].get('PERIOD'))
        R.add(P, 'autopan (v1 p autopan law): lidar x sets depth x and period 200 + 400 (1 - x) ms; runs when pad 15 toggles it', ap['lat'] == 1 and a1[0] is not None and abs(a1[0] - 1.0) < 0.03 and abs(a1[1] - 200) < 10 and abs(a2[0] - 0.5) < 0.03 and abs(a2[1] - 400) < 12, (a1, a2), '((1.0, 200), (0.5, 400))')
        # joystick drives MOOD (pad 6 switches it)
        self.rst(); self.tap(6); sleep(0.3)
        self.cmd('sim joy 1000 0 0'); sleep(0.3); x1 = fxs(self.st())['mood']['p'].get('PITCH'); self.cmd('sim joy -1000 0 0'); sleep(0.3); x2 = fxs(self.st())['mood']['p'].get('PITCH')
        self.cmd('sim joy 0 1000 0'); sleep(0.3); y1 = fxs(self.st())['mood']['p'].get('MOOD'); self.cmd('sim joy 0 -1000 0'); sleep(0.3); y2 = fxs(self.st())['mood']['p'].get('MOOD')
        R.add(P, 'joystick X -> MOOD pitch index 0..8, Y -> mood probability 0..1 (pad 6 on)', x1 == 8 and x2 == 0 and abs((y1 or -1) - 1.0) < 0.02 and abs(y2 if y2 is not None else 9) < 0.02, (x1, x2, y1, y2), '(8, 0, 1.0, 0)')
        # harmony page: either encoder button toggles it
        self.rst(); s = self.st(); a = s['hmode']; self.cmd('sim enc 1 press'); sleep(0.3); b = self.st()['hmode']; self.cmd('sim enc 2 press'); sleep(0.3); c = self.st()['hmode']; self.cmd('sim enc 2 press'); sleep(0.3); d_ = self.st()['hmode']
        R.add(P, 'either encoder button toggles the HARMONY page (0 -> 1 -> 0 -> 1)', (a, b, c, d_) == (0, 1, 0, 1), (a, b, c, d_), '(0,1,0,1)')
        self.rst(); self.cmd('sim enc 1 press'); sleep(0.3); m0 = self.st()['hmix']; self.cmd('sim enc 1 +4'); sleep(0.3); m1 = self.st()['hmix']; self.cmd('sim enc 1 -2'); sleep(0.3); m2 = self.st()['hmix']
        R.add(P, 'harmony page: encoder 1 rotation sets the harmony mix (+0.05 per detent)', abs(m1 - m0 - 0.2) < 0.02 and abs(m2 - m1 + 0.1) < 0.02, (m0, m1, m2), '(0.7, 0.9, 0.8)')
        self.cmd('sim enc 1 press'); sleep(0.3); self.cmd('sim enc 1 +3'); sleep(0.3); m3 = self.st()['hmix']
        R.add(P, 'FX page: encoder rotation does nothing (mix unchanged)', abs(m3 - m2) < 0.001, (m2, m3), 'unchanged')
        # harmony pads: 16 intervals, latch, max 3 voices, the 4th press steals the oldest
        self.rst(); self.cmd('sim enc 1 press'); sleep(0.3)
        self.tap(6); s = self.st(); hv1 = s['hv']; self.tap(1); self.tap(5); s = self.st(); hv3 = s['hv']; hl = fxs(s)['harmony']['lat']
        R.add(P, 'harmony pads: pad 6 = +7, pad 1 = +2, pad 5 = +6 (three voices, harmony switches on)', hv1[0] == 7 and sorted(hv3) == [2, 6, 7] and hl == 1, (hv1, hv3, hl), '[7,0,0] then {2,6,7}')
        self.tap(10); s = self.st(); hv4 = s['hv']
        R.add(P, 'harmony pads: a 4th press (pad 10 = +11) steals the oldest voice (+7)', sorted(hv4) == [2, 6, 11], hv4, '{2,6,11}')
        self.tap(1); self.tap(5); self.tap(10); s = self.st()
        R.add(P, 'harmony pads latch: pressing the selected pads again clears every voice, harmony off', s['hv'] == [0, 0, 0] and fxs(s)['harmony']['lat'] == 0, s['hv'], '[0,0,0]')
        self.tap(16); self.tap(11); self.tap(12); s = self.st(); R.add(P, 'harmony pad table: pad 16 = +21, 11 = +13, 12 = +14', sorted(s['hv']) == [13, 14, 21], s['hv'], '{13,14,21}')
        self.cmd('sim enc 1 press'); sleep(0.3); s = self.st(); R.add(P, 'back on the FX page the harmony voices keep running (only the pad meaning swaps)', s['hmode'] == 0 and fxs(s)['harmony']['lat'] == 1, (s['hmode'], fxs(s)['harmony']['lat']), '(0,1)')
        # exclusivity: reverb / grain / drone, last fader moved wins, re-arm at 0
        self.rst(); self.cmd('sim fader 1 600'); sleep(0.6); s = self.st(); o1 = (s['ex'][0], fxs(s)['reverb']['tgt'])
        self.cmd('sim fader 2 700'); sleep(0.6); s = self.st(); f = fxs(s); o2 = (s['ex'][0], round(f['reverb']['tgt'], 2), round(f['drone']['tgt'], 2), s['ex'][1])
        self.cmd('sim fader 1 900'); sleep(0.5); s = self.st(); f = fxs(s); o3 = (s['ex'][0], round(f['reverb']['tgt'], 2), round(f['drone']['tgt'], 2))
        self.cmd('sim fader 1 0'); sleep(0.5); s = self.st(); o4 = s['ex']; self.cmd('sim fader 1 800'); sleep(0.6); s = self.st(); f = fxs(s); o5 = (s['ex'][0], round(f['reverb']['tgt'], 2), round(f['drone']['tgt'], 2))
        self.cmd('sim fader 3 500'); sleep(0.6); s = self.st(); f = fxs(s); o6 = (s['ex'][0], round(f['grain']['tgt'], 2), round(f['reverb']['tgt'], 2))
        R.add(P, 'exclusivity (reverb / grain / drone): last fader moved wins, the displaced one is silenced and blocked until its fader has been at 0, then re-armed',
              o1 == (0, 0.6) and o2[0] == 1 and o2[1] == 0 and o2[2] > 0.6 and (o2[3] & 1) and o3[0] == 1 and o3[1] == 0 and (o4[1] & 1) == 0 and o5[0] == 0 and o5[2] == 0 and o5[1] > 0.7 and o6[0] == 9 and o6[1] > 0.45 and o6[2] == 0, dict(o1=o1, o2=o2, o3=o3, o4=o4, o5=o5, o6=o6), 'see row')
        # no control = off
        self.rst(); s = self.st(); f = fxs(s)
        R.add(P, 'effects with no v1 control (grain, wah) stay off at rest', f['grain']['st'] == 0 and f['wah']['st'] == 0 and f['grain']['lat'] == 0 and f['wah']['lat'] == 0, (f['grain']['st'], f['wah']['st']), '(0,0)')
        self.cmd('mute on'); sleep(0.3); m1 = self.st()['mute']; self.cmd('mute off'); sleep(0.3); m0 = self.st()['mute']
        R.add(P, 'mute on / off', (m1, m0) == (1, 0), (m1, m0), '(1,0)')

    # ------------------------------------------------------------------ phase 2
    def setup(self, name, reset=True):
        if reset: self.rst()
        if name == 'reverb': self.cmd('sim fader 1 1000')
        elif name == 'drone': self.cmd('sim fader 2 1000')
        elif name == 'harm': self.cmd('fx harm on'); self.cmd('p harm MIX 1')
        elif name == 'harmony': self.cmd('sim enc 1 press'); sleep(0.3); self.tap(6); self.tap(1); self.tap(5); self.cmd('sim enc 1 press')
        elif name == 'looper': self.tap(13); sleep(2.0); self.tap(13); sleep(0.5); self.tap(13); sleep(2.2)
        elif name == 'freeze': self.cmd('sim pad 4 down')
        elif name == 'blur': self.tap(7)
        elif name == 'flange': self.cmd('sim lidar 333'); self.tap(1)
        elif name == 'chorus': self.tap(8)
        elif name == 'mood': self.tap(6)
        elif name == 'grain': self.cmd('sim fader 3 1000')
        elif name == 'delay': self.tap(11)
        elif name == 'roar': self.cmd('sim pot 1 800'); self.tap(2)
        elif name == 'ring': self.tap(5)
        elif name == 'wah': self.tap(16)
        elif name == 'autopan': self.cmd('sim lidar 333'); self.tap(15)
    def params_of(self, name):
        while not self.d.q_pd.empty(): self.d.q_pd.get_nowait()
        self.cmd('pd %s' % name); out = []; t0 = time.time(); n = None
        while time.time() - t0 < 2.0:
            try: m = self.d.q_pd.get(timeout=0.3)
            except queue.Empty: continue
            if m.get('fx') != name: continue
            out += m['p']; n = m['n']
            if len(out) >= n: break
        return [p for p in out if p['i'] > 0 and 'lock' not in p]
    def window(self, secs):
        a0 = self.d.frame.get('st', {}); pk = 0; avg = []; op = [0, 0]; t0 = time.time(); cp = []
        while time.time() - t0 < secs:
            f = self.d.frame; c = f.get('cpu')
            if c: avg.append(c['a']); pk = max(pk, c['p'])
            o = f.get('op')
            if o: op = [max(op[0], o[0]), max(op[1], o[1])]
            sleep(0.25)
        return dict(cpu_avg=round(sum(avg) / len(avg) / 10, 2) if avg else None, cpu_peak=pk / 10, op=op)
    def phase2(self):
        P = 'FX'; R = self.R; d = self.d; self.cmd('mute on'); sleep(0.3); hold = self.fxsecs; sweep = self.fxsecs
        self.rst(); sleep(2); s0 = snap(d); sleep(hold); dl = delta(s0, snap(d)); R.scen.append(dict(name='idle (nothing on)', **dl)); d.log('scenario', name='idle', **dl)
        for name in (self.a.fx.split(',') if self.a.fx else IDS):
            if d.err: break
            self.setup(name); sleep(2.0 * self.sc); gi = GI[name]; pre = dict(d.frame.get('st', {})); s0 = snap(d)
            w = self.window(hold); dl = delta(s0, snap(d)); self.R.scen.append(dict(name=name, **dl)); d.log('scenario', name=name, **dl); f = d.frame.get('fx', [[0] * 5] * 16)[gi]; on = f[0]
            R.add(P, '%s on at mix 1.0 for %.0f s (mute on)' % (name, hold), on >= 1, 'state %s' % on, 'running', 'HIGH')
            ps = self.params_of(name); t0 = time.time(); k = 0
            for up in (True, False):
                t1 = time.time()
                while time.time() - t1 < sweep:
                    fr = (time.time() - t1) / sweep; fr = fr if up else 1 - fr
                    for p in ps:
                        v = p['min'] + (p['max'] - p['min']) * fr
                        if p.get('fl', 0) & 128 and p['min'] > 0: v = p['min'] * (p['max'] / p['min']) ** fr
                        self.cmd('p %s %s %.4f' % (name, p['n'], v)); k += 1
                    sleep(0.25)
            w2 = self.window(2.0); post = d.frame.get('st', {})
            dx = lambda key: (post.get(key, 0) or 0) - (pre.get(key, 0) or 0)
            nan = (post.get('nan', [0] * 16)[gi] - pre.get('nan', [0] * 16)[gi]) if post and pre else 0
            R.add(P, '%s: no xrun, no non-finite block during steady state + %d-param sweep' % (name, len(ps)), dx('xrun') == 0 and dx('gap') == 0 and nan == 0, dict(xrun=dx('xrun'), gap=dx('gap'), nan=nan), '0', 'HIGH')
            R.add(P, '%s: cpu avg %.1f%% peak %.1f%% (callback total, effect itself avg %.1f%% peak %.1f%%)' % (name, w['cpu_avg'] or -1, w['cpu_peak'], f[1] / 10, f[2] / 10), (w['cpu_peak'] < 100), dict(w, fx_cyc=f[1] / 10, fx_pk=f[2] / 10, maxcyc=post.get('maxcyc', [0] * 16)[gi] if post else None), 'peak < 100 %', 'MED')
            R.events.append(dict(phase=P, fx=name, window=w, post=w2, sweep_cmds=k))
            d.log('fx_result', fx=name, steady=w, after_sweep=w2)
        self.cmd('sim reset'); self.cmd('mute off'); sleep(1)
    def phase_fx4(self):
        P = 'FX4'; R = self.R; d = self.d; self.cmd('mute on'); sleep(0.3); secs = self.fxsecs
        def sweep(names):
            ps = [(n, p) for n in names for p in self.params_of(n)]
            for up in (True, False):
                t1 = time.time()
                while time.time() - t1 < secs:
                    fr = (time.time() - t1) / secs; fr = fr if up else 1 - fr
                    for n, p in ps:
                        v = p['min'] + (p['max'] - p['min']) * fr
                        if p.get('fl', 0) & 128 and p['min'] > 0: v = p['min'] * (p['max'] / p['min']) ** fr
                        self.cmd('p %s %s %.4f' % (n, p['n'], v))
                    sleep(0.25)
            return len(ps)
        def harmony3():                                          # +4 (pad 3), +7 (pad 6), +11 (pad 10) on the harmony page, mix 1.0 with encoder 1, back to the FX page
            self.rst(); self.cmd('sim enc 1 press'); sleep(0.3); self.tap(3); self.tap(6); self.tap(10); self.cmd('sim enc 1 +8'); sleep(0.3); s = self.st(); self.cmd('sim enc 1 press'); sleep(0.3); return s
        for name, extra in (('harmony (3 voices +4 +7 +11, mix 1.0)', []), ('harmony + chorus + reverb', ['chorus', 'reverb'])):
            s = harmony3(); ok = s is not None and sorted(s['hv']) == [4, 7, 11] and s['hmix'] >= 0.99
            R.add(P, '%s: voices and mix set' % name, ok, (s['hv'], s['hmix']) if s else None, '[4,7,11], mix 1.0', 'HIGH')
            if 'chorus' in extra: self.tap(8)
            if 'reverb' in extra: self.cmd('sim fader 1 1000')
            sleep(3.0 * self.sc); pre = dict(d.frame.get('st', {})); s0 = snap(d); w = self.window(secs); dl = delta(s0, snap(d)); self.R.scen.append(dict(name=name, **dl))
            names = ['harmony'] + extra; n = sweep(names); w2 = self.window(2.0); post = d.frame.get('st', {})
            dx = lambda k: (post.get(k, 0) or 0) - (pre.get(k, 0) or 0); nan = (sum(post.get('nan', [0])) - sum(pre.get('nan', [0]))) if post and pre else 0
            R.add(P, '%s: no xrun, no gap, no non-finite block (steady + %d-param sweep)' % (name, n), dx('xrun') == 0 and dx('gap') == 0 and nan == 0, dict(xrun=dx('xrun'), gap=dx('gap'), nan=nan), '0', 'HIGH')
            fx = d.frame.get('fx', [[0] * 5] * 16); on = [IDS[i] for i, x in enumerate(fx) if x and x[0] > 0]
            R.add(P, '%s: callback peak %.1f%% (avg %.1f%%), effects running %s' % (name, w['cpu_peak'], w['cpu_avg'] or -1, on), w['cpu_peak'] < 60 if 'chorus' in extra else w['cpu_peak'] < 100, dict(w, harmony_cyc=fx[GI['harmony']][1] / 10, harmony_pk=fx[GI['harmony']][2] / 10), 'combo peak < 60 %', 'HIGH' if 'chorus' in extra else 'MED')
            d.log('fx4', name=name, steady=w, after=w2)
        self.cmd('sim reset'); self.cmd('mute off'); sleep(1)
    # ------------------------------------------------------------------ phase 3
    def phase_tog(self):
        P = 'TOG'; R = self.R; d = self.d; self.cmd('mute on'); self.cmd('sim noise -18' if not self.a.tone else 'sim tone 440 -18'); sleep(2.0); self.rst(); sleep(1.0)
        def olwin(t0, t1): return [o for (t, o) in d.olh if t0 <= t <= t1]
        for name in IDS:
            if d.err: break
            self.rst(); sleep(1.0); w = olwin(time.time() - 0.8, time.time()); base = (sum(o[1] for o in w) / len(w)) / 10.0 if w else -99   # average level (dB) before the toggle
            s0 = self.st(); ms0 = s0['ms'] if s0 else 0; dr0 = (d.frame.get('ol') or [0, 0, 0])[2]; t_on = time.time()
            self.setup(name, reset=False); sleep(2.0); t_off = time.time(); w = olwin(t_on - 0.05, t_off); mn = min((o[1] for o in w), default=-1200) / 10.0   # dip metric: the lowest per-frame AVERAGE level (a noise block-min is just chi-square scatter)
            self.cmd('sim reset'); sleep(1.0); w2 = olwin(t_off, time.time()); mn2 = min((o[1] for o in w2), default=-1200) / 10.0
            s1 = self.st(); ms1 = s1['ms'] if s1 else 0; dr1 = (d.frame.get('ol') or [0, 0, 0])[2]
            R.add(P, '%s: toggle on (2 s) and off (1 s) with a steady tone: no dropout, no reboot' % name, dr1 == dr0 and ms1 > ms0, dict(base_db=round(base, 1), min_on_db=mn, min_off_db=mn2, dip_on=round(base - mn, 1), dropouts=dr1 - dr0, ms=(ms0, ms1)), 'dropouts 0, ms increasing', 'CRIT')
            R.add(P, '%s: level dip at the toggle <= 7.5 dB (dry hold keeps it above -6 dB; additive effects only add)' % name, base - mn <= (9.5 if name == 'roar' else 7.5), dict(base_db=round(base, 1), min_on_db=mn, dip=round(base - mn, 1)), 'dip <= 7.5 dB (roar 9.5: its auto-gain law lowers the wet by design)', 'HIGH')
        self.cmd('sim tone off'); self.cmd('sim noise off'); self.cmd('sim reset'); self.cmd('mute off'); sleep(1)
    def phase3(self, minutes):
        P = 'STRESS'; R = self.R; d = self.d; rng = random.Random(self.a.seed); end = time.time() + minutes * 60; self.cmd('mute on'); self.cmd('sim reset')
        base = dict(d.frame.get('st', {})); trb = list((d.tr or {}).get('err', [0] * 5)); strip0 = (d.hello.get('lights') or {}).get('strip', {}).get('done'); nxt_chk = time.time() + 5; nxt_deep = time.time() + 60
        refused = accepted = 0; hello_fail = 0; ms_last = None; acts = 0; refuse_viol = 0
        def act():
            r = rng.random()
            if r < 0.30: self.tap(rng.randint(1, 16), hold=rng.choice([0.08, 0.15, 0.3]))
            elif r < 0.35: n = rng.randint(1, 16); self.cmd('sim pad %d down' % n); sleep(rng.choice([0.6, 0.9])); self.cmd('sim pad %d up' % n)
            elif r < 0.50:
                k = rng.randint(1, 3)
                for v in (rng.randint(0, 1000), rng.randint(0, 1000), 0 if rng.random() < 0.5 else 1000): self.cmd('sim fader %d %d' % (k, v)); sleep(0.25)
            elif r < 0.56: self.cmd('sim pot %d %d' % (rng.randint(1, 2), rng.randint(0, 1000)))
            elif r < 0.60: self.cmd('sim fsr %d' % rng.randint(0, 1000))
            elif r < 0.64: self.cmd('sim lidar %d' % rng.randint(30, 400))
            elif r < 0.68: self.cmd('sim accel 0 %.2f 0' % rng.uniform(-1.2, 1.2))
            elif r < 0.73: self.cmd('sim joy %d %d %d' % (rng.randint(-1000, 1000), rng.randint(-1000, 1000), rng.randint(0, 1)))
            elif r < 0.78: self.cmd('sim enc 1 %s' % rng.choice(['+1', '-1', '+3', '-3', 'press']))
            elif r < 0.88: self.cmd('fx %s toggle' % rng.choice(IDS[2:]))
            elif r < 0.97:
                nm = rng.choice(IDS); ps = self.params_of(nm)
                if ps: p = rng.choice(ps); self.cmd('p %s %s %.4f' % (nm, p['n'], rng.uniform(p['min'], p['max'])))
            else: self.cmd('sim reset')
        while time.time() < end and not d.err:
            act(); acts += 1; sleep(rng.uniform(0.05, 0.3))
            if time.time() >= nxt_chk:
                nxt_chk = time.time() + 5
                if not d.hello_ok(1.0): hello_fail += 1; R.add(P, 'hello answers within 1 s', False, 'timeout #%d' % hello_fail, '<1 s', 'CRIT')
            if time.time() >= nxt_deep:
                nxt_deep = time.time() + 60; s = self.st(); f = d.frame; stt = f.get('st', {}); trn = (d.tr or {}).get('err', [0] * 5)
                if s is None: R.add(P, 'state answers', False, 'timeout', 'json', 'CRIT'); continue
                if ms_last is not None and s['ms'] < ms_last: R.add(P, 'no reboot', False, 'uptime went back %d -> %d' % (ms_last, s['ms']), 'monotonic', 'CRIT')
                ms_last = s['ms']
                R.add(P, 'minute check: xrun/gap/nan/rec unchanged', (stt.get('xrun', 0), stt.get('gap', 0), sum(stt.get('nan', [0])), stt.get('rec', 0)) == (base.get('xrun', 0), base.get('gap', 0), sum(base.get('nan', [0])), base.get('rec', 0)), dict(xrun=stt.get('xrun'), gap=stt.get('gap'), nan=sum(stt.get('nan', [0])), rec=stt.get('rec')), 'unchanged', 'HIGH')
                if (d.tr or {}).get('ok', 1) == 0: R.add(P, '[SKIP] trellis not present: error counts not checked', True, 'trellis missing', 'skipped', 'LOW')
                else: R.add(P, 'trellis error counts not climbing', list(trn) == trb, dict(now=trn, start=trb), 'equal', 'HIGH')
                sd = (f.get('sgf') or [None])[0]; sleep(1.5); sd2 = (d.frame.get('sgf') or [None])[0]
                if sd is not None and sd2 is not None: R.add(P, 'strip frames sent keep climbing (strip.n; the DMA counter `done` stays 0 on the blocking path)', sd2 > sd, (sd, sd2), 'increasing', 'MED')
                cpu = f.get('cpu', {}); R.add(P, 'cpu slow peak within the refusal limit (<= 750 permille, runtime-guard sheds allowed)', cpu.get('s', 0) <= 750, cpu, '<= 750 (sheds allowed)', 'MED')
                for fx in f.get('fx', []):
                    if len(fx) > 4 and fx[4] & 2: refused += 1
                base = dict(stt); trb = list(trn); sn = snap(d)
                if getattr(self, '_lastsn', None): dl = delta(self._lastsn, sn); R.scen.append(dict(name='stress minute', **dl)); d.log('scenario', name='stress', **dl)
                self._lastsn = sn
        # forced overload: turn everything on, then ask for more; the rule must refuse before the total peak passes ~80 %
        self.cmd('sim reset'); sleep(1)
        for name in ('sim fader 1 1000', 'sim fader 2 1000', 'sim lidar 333', 'sim pot 1 800'): self.cmd(name)
        for n in IDS[2:]: self.cmd('fx %s on' % n); sleep(1.5)
        s = self.st(); sleep(3); cpu = d.frame.get('cpu', {}); fl = [fx[4] for fx in d.frame.get('fx', [])]
        R.add(P, 'CPU refusal rule: with everything requested the total slow peak stays <= 750 permille and some effect was refused or the sum fits', cpu.get('s', 9999) <= 750, dict(cpu=cpu, flags=fl), 'slow peak <= 750', 'MED')
        R.add(P, 'stress ran %d actions in %.0f min without a lockup' % (acts, minutes), hello_fail == 0 and not d.err, dict(acts=acts, hello_fail=hello_fail, err=d.err), 'no lockup', 'CRIT')
        stx = d.frame.get('st', {}); R.add(P, 'runtime guard: %d sheds during the run (allowed), and no xrun / gap ever (xrun %s, gap %s)' % (stx.get('shed', 0), stx.get('xrun'), stx.get('gap')), (stx.get('xrun', 0), stx.get('gap', 0)) == (0, 0), dict(shed=stx.get('shed'), shedlast=stx.get('shedlast'), xrun=stx.get('xrun'), gap=stx.get('gap')), 'xrun 0, gap 0', 'HIGH')
        self.cmd('sim reset'); self.cmd('mute off')

def summarize(res, args, path, t0):
    sev = {'CRIT': 0, 'HIGH': 1, 'MED': 2, 'LOW': 3}; fails = sorted([r for r in res.rows if not r['ok']], key=lambda r: sev.get(r['sev'], 9))
    L = ['# Soak summary %s' % datetime.datetime.now().strftime('%Y-%m-%d %H:%M'), '', 'Duration %.0f min, %d checks, **%d FAIL**, %d PASS. Log: `%s`' % ((time.time() - t0) / 60, len(res.rows), len(fails), len(res.rows) - len(fails), os.path.basename(path)), '']
    L += ['## Failures, most severe first', '']
    if not fails: L.append('None.')
    for r in fails: L.append('- **%s** [%s] %s: measured `%s`, expected `%s`' % (r['sev'], r['phase'], r['name'], json.dumps(r['measured'], default=str)[:300], r['expect']))
    L += ['', '## Per-effect CPU (phase 2, callback totals and the effect itself)', '', '| effect | cb avg % | cb peak % | effect avg % | effect peak % | op L/R max |', '|---|---|---|---|---|---|']
    for e in res.events:
        if e.get('phase') == 'FX': w = e['window']; L.append('| %s | %s | %s | %s | %s | %s |' % (e['fx'], w['cpu_avg'], w['cpu_peak'], '', '', w['op']))
    L += ['', '## Deadlines per scenario (does the strip glitch track missed deadlines or audio IRQ preemption?)', '', 'sg = strip frames stretched past nominal + 40 us (corrupt), sgmax = worst stretch us, xrun = callbacks longer than 1 ms, gap = late callbacks, lp33 = main-loop passes over 33 ms, late = Trellis frames late by > 2 periods', '', '| scenario | cb avg % | slow peak % | effects on | sg/min | sgmax us | xrun | gap | lp33 | late |', '|---|---|---|---|---|---|---|---|---|---|']
    for c in res.scen: L.append('| %s | %.1f | %.1f | %d | %s | %s | %d | %d | %d | %d |' % (c['name'], c['cpu_a'], c['cpu_s'], c['on'], c['sg_per_min'], c['sgmax'], c['xrun'], c['gap'], c['lp33'], c['late']))
    if len(res.scen) >= 4:
        cpu = [c['cpu_a'] for c in res.scen]; sg = [c['sg_per_min'] for c in res.scen]; xr = [c['xrun'] + c['gap'] for c in res.scen]; on = [c['on'] for c in res.scen]; idle = [c for c in res.scen if c['name'].startswith('idle')]
        r1, r2, r3 = pearson(cpu, sg), pearson(on, sg), pearson(xr, sg); f = lambda r: 'n/a' if r is None else '%.2f' % r
        L += ['', 'Correlation across scenarios: sg vs cpu avg = %s, sg vs effects on = %s, sg vs (xrun + gap) = %s.' % (f(r1), f(r2), f(r3))]
        if idle and idle[0]['sg_per_min'] > 0 and (r1 is None or r1 < 0.5): L.append('**With nothing running the strip still stretched %.1f frames/min and the stretch does not track CPU: it is audio IRQ preemption of the blocking send, not a missed deadline.**' % idle[0]['sg_per_min'])
        elif r3 is not None and r3 > 0.5: L.append('**Stretched frames track missed audio deadlines (xrun/gap).**')
    L += ['', '## All rows', '']
    for r in res.rows: L.append('- %s [%s] %s: %s' % ('PASS' if r['ok'] else 'FAIL', r['phase'], r['name'], json.dumps(r['measured'], default=str)[:200]))
    open(path.replace('.jsonl', '_summary.md'), 'w').write('\n'.join(L) + '\n')

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--port', default='/dev/cu.usbmodem327F347733331'); ap.add_argument('--stress-min', type=float, default=4); ap.add_argument('--fx', default='', help='comma list of effect ids for the fx phase (default all); --phase fx1/fx2/fx3 = groups of five'); ap.add_argument('--fx-secs', type=float, default=6, help='seconds for the steady window and for each sweep direction'); ap.add_argument('--seed', type=int, default=12345)
    ap.add_argument('--tone', action='store_true', help='tog phase: use the vibrato tone instead of noise'); ap.add_argument('--quick', action='store_true', help='short phases (smoke test)'); ap.add_argument('--mock', action='store_true'); ap.add_argument('--skip', default='', help='comma list of phases to skip: map,fx,stress'); ap.add_argument('--phase', default='', help='run ONE phase: map | fx | fx1 | fx2 | fx3 | fx4 (harmony + combo) | stress (each is at most about 5 minutes)')
    a = ap.parse_args(); os.makedirs(LOGDIR, exist_ok=True); ts = datetime.datetime.now().strftime('%Y%m%d_%H%M%S'); path = os.path.join(LOGDIR, 'soak_%s.jsonl' % ts); logf = open(path, 'w'); res = Result(); t0 = time.time()
    stop_rel = threading.Event()
    def releaser():
        while not stop_rel.wait(50 * 60): post('__release 3600')
    if not a.mock:
        print('release:', post('__release 3600')); time.sleep(2.5); threading.Thread(target=releaser, daemon=True).start()
    dev = None
    try:
        dev = Mock() if a.mock else Dev(a.port, logf)
        if not a.mock: dev.send('hello'); time.sleep(1.5); dev.send('nosave on')   # a run must never persist params: the Seed drops saves (and the web UI skips params.json pushes) while nosave is on
        groups = {'fx1': IDS[0:5], 'fx2': IDS[5:10], 'fx3': IDS[10:15]}
        if a.phase in groups: a.fx = ','.join(groups[a.phase]); a.phase = 'fx'
        sk = Soak(dev, res, a); skip = a.skip.split(',')
        if a.phase: skip = [n for n in ('map', 'fx', 'fx4', 'tog', 'stress') if n != a.phase]
        for nm, fn in (('map', sk.phase1), ('fx', sk.phase2), ('fx4', sk.phase_fx4), ('tog', sk.phase_tog), ('stress', lambda: sk.phase3(0.5 if a.quick or a.mock else a.stress_min))):
            if nm in skip: continue
            print('=== phase', nm, flush=True)
            try: fn()
            except Exception: res.add('RUNNER', 'phase %s raised' % nm, False, traceback.format_exc()[-600:], 'no exception', 'CRIT'); traceback.print_exc()
            if getattr(dev, 'err', None): res.add('RUNNER', 'serial link lost', False, dev.err, 'open', 'CRIT'); break
    finally:
        if dev: 
            try: dev.send('mute off'); dev.send('sim off'); dev.send('nosave off')
            except Exception: pass
            dev.close()
        summarize(res, a, path, t0); logf.close(); stop_rel.set()
        if not a.mock: print('take:', post('__take'))
        print('summary:', path.replace('.jsonl', '_summary.md'))
    return 0 if all(r['ok'] for r in res.rows) else 1

if __name__ == '__main__': sys.exit(main())
