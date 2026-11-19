import sys,time,statistics; sys.path.insert(0,'bringup/tools')
from dev import Dev
d=Dev(); time.sleep(1.2)
def lastp():
    for m in reversed(d.fr):
        if 'pads' in m: return m['pads']
def xf(gi):
    for m in reversed(d.fr):
        if 'xf' in m:
            for x in m['xf']:
                if x[0]==gi: return x
def w(t): time.sleep(t)
def st(): return [m['pads'] for m in d.fr if 'pads' in m][-1]
def lv(i0): return [(m['mo'][0]/10,m['mo'][1]/10) for m in d.fr[i0:] if m.get('t')=='s' and 'mo' in m]
def ensure(pad,want):
    w(0.5); on=(lastp()['on']>>(pad-1))&1
    if on!=want: d.send('sim pad %d'%pad); w(0.7)
for c in ['nosave on','sim off','ogain 0','sim fader 1 0','sim fader 2 0','sim fader 3 0']: d.send(c); w(0.2)
w(1.0); h=[m for m in d.fr if m.get('t')=='hello'][-1:]; print('fw',h[0].get('fw') if h else None,'ax',st().get('ax'),'rst',st().get('rst'),'txd',st().get('txd'))
for p in (1,2,3,9,12,13,14,15,16):
    ensure(p,0)
# trace (pad 2)
print('== TRACE (pad 2), noise -35'); d.send('sim noise -35'); w(1.0); ensure(2,1); w(2.0)
print(' trace xf',xf(5),' ax',st()['ax'],'why',st().get('why')); 
# measure worst: collect pk over 3 s
w(3.0); print(' trace xf after 3s',xf(5)); ensure(2,0)
print('== WAH (pad 3), tone -30'); d.send('sim noise off'); d.send('sim tone 440 -30'); w(0.8); ensure(3,1); w(3.0); print(' wah xf',xf(13)); ensure(3,0)
print('== RING (pad 1): base 0 -> off; enc +25 detents'); ensure(1,1); w(1.0); print(' ring on at base 0? xf',xf(12),'bl',st().get('bl')); d.send('sim enc 25'); w(1.5); print(' after enc: xf',xf(12),'bl',st().get('bl')); d.send('sim fsr 800'); w(1.0); print(' fsr 800: bl',st().get('bl'),' xf',xf(12)); d.send('sim fsr -1'); d.send('sim enc -25'); w(1.0); print(' back to 0: xf',xf(12)); ensure(1,0)
print('== DELAY enc/FSR (pad 16)'); ensure(16,1); w(0.8); print(' base',st().get('bl'),'xf',xf(10)); d.send('sim enc 10'); w(0.8); print(' enc +10: bl',st().get('bl')); d.send('sim fsr 700'); w(1.5); print(' fsr 700: bl',st().get('bl')); d.send('sim fsr 0'); w(1.5); print(' fsr 0: bl',st().get('bl')); d.send('sim fsr -1'); d.send('sim enc -10'); ensure(16,0)
print('== FREEZE rebuild + radio'); ensure(2,0); d.send('sim tone 440 -30'); w(0.5); ensure(13,1); w(0.5); print(' drone on:',(st()['on']>>12)&1)
d.send('sim pad 9'); w(1.5); p=st(); print(' freeze gate open: drone on?',(p['on']>>12)&1,'gate lit',(p['on']>>8)&1,'ax',p['ax'],'why',p.get('why')); fz=xf(4); print(' freeze xf',fz)
d.send('sim pad 9'); w(0.3); d.send('sim tone off'); w(1.0); L=lv(d.mark()-200); print(' frozen hold pk',max(x[0] for x in L))
d.send('sim tone 440 -30'); ensure(2,1); w(0.8); print(' trace while freeze on: on?',(st()['on']>>1)&1,'why',st().get('why'))
d.send('sim pad 13'); w(1.5); print(' drone on -> freeze reset:', 'freeze xf',xf(4),'drone on',(st()['on']>>12)&1); ensure(13,0)
ensure(2,1); w(1.5); print(' trace now (after freeze released): on',(st()['on']>>1)&1,'ax',st()['ax'],'xf',xf(5)); ensure(2,0)
print('cpu',[m['cpu'] for m in d.fr if 'cpu' in m][-1],'st',[m['st'] for m in d.fr if 'st' in m][-1],'txd',st().get('txd'))
d.send('sim off'); d.send('nosave off'); w(0.5); d.close()
