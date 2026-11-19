import sys,time; sys.path.insert(0,'bringup/tools')
from dev import Dev
d=Dev(); time.sleep(1.2)
def st(): return [m['pads'] for m in d.fr if 'pads' in m][-1]
def w(t): time.sleep(t)
for c in ['nosave on','sim off','ogain 0','sim fader 1 0','sim tone 440 -30']: d.send(c); w(0.2)
w(1.0)
if not (st()['on']>>3)&1: d.send('sim pad 4'); w(0.8)
print('hw joy j[1] at rest:',[m['j'] for m in d.fr if 'j' in m][-1])
for mode in (0,1,2):
    d.send('p mod MODE %d'%mode); w(0.4); out=[]
    for y in (0,50,200,500,800,1000,-800,0):
        d.send('sim joy %d'%y); w(0.5); i=d.mark(); w(0.4)
        L=[m['mo'][0]/10 for m in d.fr[i:] if 'mo' in m]; out.append((y,st()['jf'],round(max(L),1)))
    print('mode',mode,'(y, [push x1000, fb x1000], out pk dBFS):',out)
print('cpu',[m['cpu'] for m in d.fr if 'cpu' in m][-1],'st',[m['st'] for m in d.fr if 'st' in m][-1])
d.send('sim pad 4'); d.send('sim off'); d.send('nosave off'); w(0.5); d.close()
