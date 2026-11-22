# Read the 11_bench JSON from the spare Seed and derive the host-to-Seed factor.  venv python only:
#   ../../.venv/bin/python tests/bench_read.py /dev/cu.usbmodem18063371376 [passes]
# Prints each state as it arrives, then per effect: Seed ns/sample (cycles / 48 / clk), host ns/sample (tests/fx_budget.cpp), factor F = seed / host, and the Seed load % of one block.
import serial, sys, json, time, statistics
HOST = {  # ns/sample on the dev Mac, tests/fx_budget.cpp (reverb LITE), rerun it after any DSP change
 'reverb': 17.7, 'drone': 23.0, 'harm': 5.8, 'looper': 3.3, 'freeze': 24.2, 'blur': 19.9, 'flange': 10.7, 'chorus': 9.6, 'mood': 10.1, 'grain': 15.4, 'delay': 11.3, 'roar': 10.8, 'ring': 5.7, 'wah': 5.9, 'autopan': 4.4, 'harmony': 27.7}
port = sys.argv[1]; want = int(sys.argv[2]) if len(sys.argv) > 2 else 1
s = serial.Serial(port, 115200, timeout=1); s.dtr = True; s.rts = True; time.sleep(0.3); s.write(b'hello\n')
tab = None; got = 0; t0 = time.time()
while got < want and time.time() - t0 < 300 * want:
    l = s.readline().decode(errors='ignore').strip()
    if not l.startswith('{'): continue
    try: m = json.loads(l)
    except Exception: continue
    if m.get('t') == 'bench': print('%-7s n=%d avg=%d worst=%d cycles  (%.1f%% / %.1f%% of a block)' % (m['fx'], m['n'], m['avg'], m['worst'], m['avgpct'] / 10, m['worstpct'] / 10))
    if m.get('t') == 'bench_table': tab = m; got += 1
if not tab: sys.exit('no table received')
clk = tab['clk']; idle = [r for r in tab['rows'] if r[0] == 'idle'][0][1]
print('\nclk %d Hz, budget %d cycles per block, idle chain overhead %d cycles' % (clk, tab['budget'], idle))
print('%-7s %10s %10s %9s %8s %8s' % ('effect', 'seed ns/s', 'host ns/s', 'F', 'avg %', 'worst %'))
fs = []
for name, avg, worst in tab['rows']:
    if name in ('idle', 'all'): continue
    ns = (avg - idle) / 48.0 / clk * 1e9; f = ns / HOST[name] if name in HOST and ns > 0 else 0
    if f: fs.append(f)
    print('%-7s %10.1f %10.1f %9.1f %7.1f%% %7.1f%%' % (name, ns, HOST.get(name, 0), f, 100.0 * (avg - idle) / tab['budget'], 100.0 * (worst - idle) / tab['budget']))
for r in tab['rows']:
    if r[0] == 'combo': print('\nharmony (3 voices) + chorus + reverb: avg %.1f%%, worst %.1f%% of a block (target < 60%% peak): %s' % (100.0 * (r[1] - idle) / tab['budget'], 100.0 * (r[2] - idle) / tab['budget'], 'PASS' if (r[2] - idle) / tab['budget'] < 0.60 else 'FAIL'))
if fs: print('\nF per effect: median %.1f, min %.1f, max %.1f   (all-on: %s)' % (statistics.median(fs), min(fs), max(fs), [r for r in tab['rows'] if r[0] == 'all'][0][1:]))
