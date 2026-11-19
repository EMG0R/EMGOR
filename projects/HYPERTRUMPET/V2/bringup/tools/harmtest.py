import sys,time,statistics; sys.path.insert(0,'bringup/tools')
from dev import Dev
d=Dev(); time.sleep(1.2)
def st(): return [m['pads'] for m in d.fr if 'pads' in m][-1]
def w(t): time.sleep(t)
def last(k): return [m[k] for m in d.fr if k in m][-1]
for c in ['nosave on','sim off','ogain 0','sim fader 1 0','sim tone 440 -45']: d.send(c); w(0.2)
w(1.0)
if not st().get('hm'): d.send('sim encpress'); w(0.6)
print('fw',[m['fw'] for m in d.fr if m.get('t')=='hello'][-1:],'hm',st().get('hm'))
semi=[-10,-9,-8,-7,-6,-5,-4,-3,-2,-1,1,2,5,6,8,9]
res=[]
for k in range(1,17):
    i=d.mark(); d.send('sim padhold %d'%k); w(1.2); hw=[m['hw'] for m in d.fr[i:] if 'hw' in m]
    rms=max(x[0] for x in hw[-5:])/10 if hw else None; pk=max(x[1] for x in hw[-5:])/10 if hw else None
    res.append((k,semi[k-1],rms,pk,st()['hv'])); d.send('sim padrel %d'%k); w(0.5)
for r in res: print('pad %2d  %+3d st  voice wet rms %s dBFS  pk %s  held mask %d'%r)
# 4th press steals oldest
for k in (1,2,3,4): d.send('sim padhold %d'%k); w(0.5)
print('4 held: mask',st()['hv'],'hw',last('hw'))
for k in (1,2,3,4): d.send('sim padrel %d'%k)
w(1.0); print('after release hw',last('hw'),'harmony on?',[x for m in d.fr[-400:] if 'xf' in m for x in m['xf'] if x[0]==15][-1:] ,'refusals',st().get('rf'))
d.send('sim encpress'); w(0.5)
# mod block CPU per mode
d.send('sim tone 440 -30'); w(0.4)
if not (st()['on']>>3)&1: d.send('sim pad 4'); w(0.6)
for mode in (0,1,2):
    d.send('p mod MODE %d'%mode); w(2.0); xs=[x for m in d.fr[-900:] if 'xf' in m for x in m['xf'] if x[0]==6]
    print('mod mode',mode,'cpu avg %.1f%% worst %.1f%%'%(statistics.mean(x[2] for x in xs[-20:])/10,max(x[3] for x in xs[-20:])/10))
d.send('p mod MODE 0'); d.send('sim pad 4'); d.send('sim off'); d.send('nosave off'); w(0.5); d.close()
