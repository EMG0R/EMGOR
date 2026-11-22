import sys,time,statistics; sys.path.insert(0,'bringup/tools')
from dev import Dev
d=Dev(); time.sleep(1.2)
def st(): return [m['pads'] for m in d.fr if 'pads' in m][-1]
def w(t): time.sleep(t)
def last(k): return [m[k] for m in d.fr if k in m][-1]
d.send('nosave off'); d.send('sim off'); w(0.3)
if (st()['on']>>12)&1: d.send('sim pad 13'); w(0.8)
d.send('preset reset'); w(3.5)   # saved 2 s after: params restored to the new drone defaults
d.send('pd drone'); w(1.0); print('drone params:',[(p['n'],round(p['v'],3)) for m in d.fr if m.get('t')=='pd' and m.get('fx')=='drone' for p in m['p']][-14:])
d.send('nosave on'); d.send('ogain 0'); d.send('sim fader 1 0'); d.send('sim tone 440 -50'); w(1.2)
d.send('sim pad 13'); w(1.0); d.send('sim fader 1 1000'); w(1.0); d.send('sim fader 1 0'); w(0.5); d.send('sim fader 1 1000'); w(6.0)
i=d.mark()-1200; M=[m for m in d.fr[i:] if m.get('t')=='s']
dd=[m['dd'] for m in M if 'dd' in m][-1]; dr=[m['dr'] for m in M if 'dr' in m][-1]
print('wet rms %.1f dBFS pk %.1f dBFS (tone -50 in); dd %s; dr %s; mo %s'%(dd[4]/10,dd[5]/10,dd,dr,last('mo')))
pk=[m['dr']['pk'] for m in d.fr[-3000:] if 'dr' in m]; cy=[m['dr']['cyc'] for m in d.fr[-3000:] if 'dr' in m]
print('drone CPU avg %.1f%% (cyc) , worst block %.1f%%'%(statistics.mean(cy[-40:])/10, max(pk)/10), ' cpu',last('cpu'),'st',last('st'))
# toggle test on noise
d.send('sim tone off'); d.send('sim noise -40'); d.send('sim fader 1 300'); w(1.0); d.send('sim pad 13'); w(1.0); d.send('sim pad 13'); w(1.5)
for rep in range(3):
    w(1.0); i0=d.mark(); b=statistics.mean([m['mo'][1]/10 for m in d.fr[i0-300:i0] if 'mo' in m]); dp0=last('ol')[2]
    d.send('sim pad 13'); w(1.2); F=[m['mo'][1]/10 for m in d.fr[i0:] if m.get('t')=='s' and 'mo' in m]
    print(' toggle on: steady %.1f, first300ms min %.1f, frames>15dB under %d, dropouts +%d'%(b,min(F[:60]),len([v for v in F[:60] if v<b-15]),last('ol')[2]-dp0))
    d.send('sim pad 13'); w(1.0)
d.send('sim off'); d.send('nosave off'); w(0.5); d.close()
