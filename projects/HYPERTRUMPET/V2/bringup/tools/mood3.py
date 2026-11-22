import sys,time,statistics; sys.path.insert(0,'bringup/tools')
from dev import Dev
d=Dev(); time.sleep(2)
def st(): return [m['pads'] for m in d.fr if 'pads' in m][-1]
def stt(): return [m['st'] for m in d.fr if isinstance(m.get('st'),dict)][-1]
def w(t): time.sleep(t)
h=[m for m in d.fr if m.get('t')=='hello'][-1]; print('fw',h['fw'],'rst',h['rst'],'bc',h.get('bc'))
d.send('nosave on'); w(0.2)
# reset moodii params to defaults (names changed: TIME -> LENGTH)
d.send('pd moodii'); w(2.0); P={}
for m in d.fr[-2500:]:
    if m.get('t')=='pd' and m.get('fx')=='moodii':
        for p in m['p']: P[p['n']]=p
print('moodii params:',{n:(round(p['v'],3),round(p['def'],3)) for n,p in P.items()})
for n,p in P.items():
    if abs(p['v']-p['def'])>1e-3: d.send('p moodii %s %.4f'%(n,p['def'])); w(0.15)
for c in ['sim off','ogain 0','sim fader 1 0','sim tone 440 -30']: d.send(c); w(0.2)
w(1.0)
if (st()['on']>>5)&1: d.send('sim pad 6'); w(0.6)
d.send('sim pad 6'); w(1.0); d.send('p moodii MIX 1.0'); w(0.5)
def mz(): return [m['mz'] for m in d.fr if 'mz' in m][-1]/10.0
for y in (0,1000,-1000,0):
    d.send('sim joy %d'%y); w(2.0); print('MOOD joystick %+5d: wet pitch %.1f Hz (input 440)'%(y,mz()))
# routing: select MOOD (pad 6 selected) -> mod block / blooper must not see the stick
d.send('sim joy 1000'); w(1.0); p=st(); print('selected MOOD: mod joy push (jf) =',p['jf'],'-> expected 0; blooper rate',[m['bl'] for m in d.fr if 'bl' in m][-1][3]/1000.0)
d.send('sim joy -2000'); d.send('sim pad 6'); w(0.5)
# blooper with no loop
b=[m['bl'] for m in d.fr if 'bl' in m][-1]; print('blooper, no loop: bl',b,'(has source = last field)')
# loop record then blooper
d.send('sim tone 440 -30'); d.send('sim pad 12'); w(1.5); d.send('sim pad 12'); w(1.0)
b=[m['bl'] for m in d.fr if 'bl' in m][-1]; print('after recording a loop: bl',b)
d.send('sim pad 7'); w(1.0); d.send('p blooper PROB 1.0'); w(3.0); b=[m['bl'] for m in d.fr if 'bl' in m][-1]; print('blooper ON over the loop: bl',b,'cpu',[m['cpu'] for m in d.fr if 'cpu' in m][-1],'st',stt())
d.send('sim pad 7'); w(0.5); d.send('sim padhold 12'); w(0.5); d.send('p blooper PROB 0.45'); d.send('p moodii MIX 0.5'); d.send('sim off'); d.send('nosave off'); w(0.5); d.close()
