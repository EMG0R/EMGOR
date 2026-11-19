import sys,time,statistics; sys.path.insert(0,'bringup/tools')
from dev import Dev
d=Dev(); time.sleep(3)
def stt(): return [m['st'] for m in d.fr if isinstance(m.get('st'),dict)][-1]
def last(k): return [m[k] for m in d.fr if k in m][-1]
def w(t): time.sleep(t)
h=[m for m in d.fr if m.get('t')=='hello'][-1]; print('fw',h['fw'],'rst',h.get('rst'),'bc',h.get('bc')); print('boot st',stt())
l=[m for m in d.fr if m.get('t')=='s'][-1]; print('raw faders',l['v'][:3],'-> ffault',stt()['ffault'],'(real faders at the top, no sim)')
for c in ['nosave on','sim off','ogain 0','sim fader 2 0','sim noise -40']: d.send(c); w(0.2)
w(2.0)
# CHECK 1
F=[m['mo'][1]/10 for m in d.fr[-300:] if 'mo' in m]; dry=statistics.mean(F); dr=last('dr'); print('CHECK 1 fader 0: out rms %.1f dB (noise -40 dBFS peak-ish; unchanged dry), drone on=%d, dropouts %d -> %s'%(dry,dr['on'],last('ol')[2],'PASS' if dr['on']==0 else 'FAIL'))
# CHECK 2 sweep
dp0=last('ol')[2]; i0=d.mark()
for v in list(range(0,1001,100))+list(range(1000,-1,-100)): d.send('sim fader 2 %d'%v); w(0.7)
w(1.5); F2=[m['mo'][1]/10 for m in d.fr[i0:] if 'mo' in m]; fs=[m['fs'][1] for m in d.fr[i0:] if 'fs' in m]
print('CHECK 2 sweep 0->1000->0: out rms min %.1f max %.1f (dry %.1f); dropouts +%d; xrun %d gap %d -> %s'%(min(F2),max(F2),dry,last('ol')[2]-dp0,stt()['xrun'],stt()['gap'],'PASS' if (min(F2)>dry-1.0 and last('ol')[2]==dp0 and stt()['xrun']==0) else 'CHECK'))
d.send('sim off'); d.send('nosave off'); w(0.3); print('end st',stt()); d.close()
