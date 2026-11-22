import sys,time,statistics; sys.path.insert(0,'bringup/tools')
from dev import Dev
d=Dev(); time.sleep(1.2)
def st(): return [m['pads'] for m in d.fr if 'pads' in m][-1]
def stt(): return [m['st'] for m in d.fr if 'st' in m][-1]
def w(t): time.sleep(t)
d.send('nosave off'); d.send('sim off'); w(0.3); d.send('preset reset'); w(3.5); d.send('nosave on')
for c in ['ogain 0','sim fader 1 0','sim tone 440 -45']: d.send(c); w(0.2)
w(1.0)
# harmony level
if not st()['hm']: d.send('sim encpress'); w(0.6)
i=d.mark(); d.send('sim padhold 5'); w(1.5); hw=[m['hw'] for m in d.fr[i:] if 'hw' in m]; print('harmony 1 voice: wet rms %.1f dBFS (dry tone -48.0 rms) -> %.1f dB re dry'%(max(x[0] for x in hw[-5:])/10, max(x[0] for x in hw[-5:])/10+48.0)); d.send('sim padrel 5'); w(0.5); d.send('sim encpress'); w(0.5)
# glitch toggle xruns
s0=stt(); d.send('sim tone 440 -30')
for r in range(8):
    d.send('sim pad 5'); w(1.2); d.send('sim pad 5'); w(1.5)
s1=stt(); print('glitch 8 toggles: xrun %d->%d gap %d->%d cb max %d cyc (%.2f ms)'%(s0['xrun'],s1['xrun'],s0['gap'],s1['gap'],s1['cb'],s1['cb']/480000.0))
# admission: stack effects
order=[(13,'drone'),(2,'trace'),(4,'mod'),(5,'glitch'),(16,'tape'),(3,'wah'),(1,'ring')]
d.send('sim fader 1 0'); w(0.3)
for pad,name in order:
    rf0=st()['rf']; d.send('sim pad %d'%pad); w(1.2); p=st()
    print('on %-7s -> on=%s rf %d->%d why=%s cpu=%s shed=%s'%(name,(p['on']>>(pad-1))&1,rf0,p['rf'],p.get('why'),[m['cpu'] for m in d.fr if 'cpu' in m][-1],p.get('shed')))
for pad,name in order: 
    if (st()['on']>>(pad-1))&1: d.send('sim pad %d'%pad); w(0.4)
print('final st',stt(),'pads',{k:st()[k] for k in ('rf','shed','on')})
d.send('sim off'); d.send('nosave off'); w(0.5); d.close()
