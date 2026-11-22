import sys,time,statistics; sys.path.insert(0,'bringup/tools')
from dev import Dev
d=Dev(); time.sleep(1.2)
def st(): return [m['pads'] for m in d.fr if 'pads' in m][-1]
def w(t): time.sleep(t)
def xf(gi):
    for m in reversed(d.fr):
        if 'xf' in m:
            for x in m['xf']:
                if x[0]==gi: return x
for c in ['nosave on','sim off','ogain 0','sim fader 1 0','sim noise -40']: d.send(c); w(0.2)
w(1.0)
if (st()['on']>>3)&1: d.send('sim pad 4'); w(0.8)
print('fw',[m['fw'] for m in d.fr if m.get('t')=='hello'][-1:],'ax',st()['ax'],'md',st().get('md'))
for mode in (0,1,2):
    d.send('p mod MODE %d'%mode); w(0.5)
    for rep in range(3):
        w(1.2); i0=d.mark(); base=[m['mo'][1]/10 for m in d.fr[i0-300:i0] if 'mo' in m]; b=statistics.mean(base)
        drops0=[m['ol'][2] for m in d.fr if 'ol' in m][-1]
        d.send('sim pad 4'); w(1.5)
        F=[(m['ms'],m['mo'][1]/10) for m in d.fr[i0:] if m.get('t')=='s' and 'mo' in m]
        early=[v for ms,v in F[:60]]; late=[v for ms,v in F[-100:]]
        gap=[v for v in early if v<b-15]
        drops1=[m['ol'][2] for m in d.fr if 'ol' in m][-1]
        print('mode',mode,'rep',rep,'steady %.1f dB; first 300 ms: min %.1f; frames >15 dB under: %d; late mean %.1f; dropout counter +%d; xf %s'%(b,min(early),len(gap),statistics.mean(late),drops1-drops0,xf(1)))
        d.send('sim pad 4'); w(0.8)
d.send('sim pad 4'); w(0.8); md0=st().get('md')
d.send('sim enc 1'); w(0.4); a=st().get('md'); d.send('sim enc 1'); w(0.4); b2=st().get('md'); d.send('sim enc 1'); w(0.4); c3=st().get('md'); d.send('sim enc -5'); w(0.4); e=st().get('md')
print('encoder modes: start',md0,'+1',a,'+1',b2,'+1 (wall)',c3,'-5 (wall)',e,'sel',st()['sel'])
print('cpu',[m['cpu'] for m in d.fr if 'cpu' in m][-1],'st',[m['st'] for m in d.fr if 'st' in m][-1])
d.send('sim pad 4'); d.send('sim off'); d.send('nosave off'); w(0.5); d.close()
