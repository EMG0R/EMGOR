import sys,time,statistics; sys.path.insert(0,'bringup/tools')
from dev import Dev
d=Dev(); time.sleep(1.2)
def st(): return [m['pads'] for m in d.fr if 'pads' in m][-1]
def w(t): time.sleep(t)
def last(k): return [m[k] for m in d.fr if k in m][-1]
h=[m for m in d.fr if m.get('t')=='hello']
print('fw',h[-1].get('fw') if h else None,'rst',h[-1].get('rst') if h else None,'bc',h[-1].get('bc') if h else None)
for c in ['nosave on','sim off','ogain 0','sim fader 1 0','sim fader 2 0','sim fader 3 0','sim tone 440 -30']: d.send(c); w(0.2)
w(1.0)
def xrows(gi):
    return [x for m in d.fr[-1500:] if 'xf' in m for x in m['xf'] if x[0]==gi]
def alloff():
    p=st()
    for pad in range(1,17):
        if (p['on']>>(pad-1))&1 and pad not in (9,10,11,12):
            d.send('sim pad %d'%pad); w(0.4)
    d.send('sim pad 10'); w(0.6)
def meas(name,gi,pad,pre=None,post=None):
    alloff(); w(0.5)
    if pre: pre()
    d.send('sim pad %d'%pad); w(3.0)
    if gi==1:
        rows=[m['dr'] for m in d.fr[-1500:] if 'dr' in m]; avg=statistics.mean(r['cyc'] for r in rows[-40:])/10; wk=max(r['pk'] for r in rows)/10
    else:
        rows=xrows(gi); avg=statistics.mean(r[2] for r in rows[-40:])/10 if rows else None; wk=max(r[3] for r in rows[-60:])/10 if rows else None
    print('%-10s avg %5.1f%%  worst %5.1f%%'%(name,avg if avg is not None else -1,wk if wk is not None else -1),flush=True)
    if post: post()
    alloff()
def up(): d.send('sim fader 1 1000'); w(1.0)
def dn(): d.send('sim fader 1 0'); w(0.5)
def enc_on(): d.send('sim enc 25'); w(0.8)
def enc_off(): d.send('sim enc -25'); w(0.3)
meas('drone',1,13,post=dn) if False else None
def meas2(name,gi,pad,after):
    alloff(); w(0.5); d.send('sim fader 1 0'); w(0.3); d.send('sim pad %d'%pad); w(0.5); after(); w(3.0)
    if gi==1:
        rows=[m['dr'] for m in d.fr[-1500:] if 'dr' in m]; avg=statistics.mean(r['cyc'] for r in rows[-40:])/10; wk=max(r['pk'] for r in rows)/10
    else:
        rows=xrows(gi); avg=statistics.mean(r[2] for r in rows[-40:])/10 if rows else -1; wk=max(r[3] for r in rows[-60:])/10 if rows else -1
    print('%-10s avg %5.1f%%  worst %5.1f%%'%(name,avg,wk),flush=True); alloff()
meas2('drone',1,13,up); meas2('ring',12,1,enc_on); enc_off(); meas2('glitch',11,5,lambda:None)
# freeze: gate open
alloff(); d.send('sim pad 9'); w(3.0); rows=xrows(4); print('%-10s avg %5.1f%%  worst %5.1f%%'%('freeze',statistics.mean(r[2] for r in rows[-40:])/10,max(r[3] for r in rows[-60:])/10)); d.send('sim pad 9'); d.send('sim pad 10'); w(1.0)
# looper: record 1 s then play
alloff(); d.send('sim pad 12'); w(1.5); d.send('sim pad 12'); w(2.5); rows=xrows(3); print('%-10s avg %5.1f%%  worst %5.1f%%'%('looper',statistics.mean(r[2] for r in rows[-40:])/10,max(r[3] for r in rows[-60:])/10)); d.send('sim padhold 12'); w(0.8)
print('idle cpu',last('cpu'),'ax',st().get('ax'),'st',last('st'))
d.send('sim off'); d.send('nosave off'); w(0.5); d.close()
