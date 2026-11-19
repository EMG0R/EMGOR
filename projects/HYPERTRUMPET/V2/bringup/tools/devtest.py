import sys,time,json; sys.path.insert(0,'bringup/tools')
from dev import Dev
def db(x): return x/10.0
def burst(d, pad, tone='440 -30', fader=None):
    d.send('nosave on'); d.send('sim off'); time.sleep(0.3)
    d.send('sim tone '+tone); time.sleep(1.0)
    if fader is not None: d.send('sim fader 1 %d'%fader)
    d.send('sim pad %d'%pad); time.sleep(3.0)
    i=d.mark(); d.send('sim tone off'); time.sleep(2.0)
    F=[m for m in d.fr[i:] if m.get('t')=='s' and 'mo' in m]
    return F
if __name__=='__main__':
    d=Dev(); time.sleep(1.5); d.send('hello'); time.sleep(1.5)
    h=[m for m in d.fr if m.get('t')=='hello'][-1]; print('fw',h.get('fw'),'rst',h.get('rst'))
    d.send('pd tape'); time.sleep(1); print([ (p['n'],p['v']) for m in d.fr if m.get('t')=='pd' and m.get('fx')=='tape' for p in m['p']])
    F=burst(d,16)
    for m in F[:1]: pass
    pk=[(m['ms'],db(m['mo'][0])) for m in F]
    t0=None
    for k in range(1,len(pk)):
        if pk[k-1][1]>-45 and pk[k][1]<pk[k-1][1]-6: t0=pk[k][0]; break
    print('drop at',t0, 'series', [(ms-t0,v) for ms,v in pk if t0 and -10<=ms-t0<=900][::6][:40])
    print('pads',[m['pads'] for m in d.fr if 'pads' in m][-1]); print('xf',[m['xf'] for m in d.fr if 'xf' in m][-1])
    d.send('sim off'); d.send('nosave off'); d.close()
