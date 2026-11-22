import sys,time,json,statistics; sys.path.insert(0,'bringup/tools')
from dev import Dev
d=Dev(); time.sleep(1.2)
def lastp(): 
    for m in reversed(d.fr):
        if 'pads' in m: return m['pads']
def lastxf(): 
    for m in reversed(d.fr):
        if 'xf' in m: return {x[0]:x for x in m['xf']}
def wait(t): time.sleep(t)
def ensure(pad, want):   # pad 1-based; bit = pad-1
    wait(0.5); on=(lastp()['on']>>(pad-1))&1
    if on!=want: d.send('sim pad %d'%pad); wait(0.6)
def levels(i0): 
    F=[m for m in d.fr[i0:] if m.get('t')=='s' and 'mo' in m]; return [(m['ms'],m['mo'][0]/10,m['mo'][1]/10,m['mo'][2]/10) for m in F]
for c in ['nosave on','sim off','ogain 0','sim fader 1 0','sim fader 2 0','sim fader 3 0']: d.send(c); wait(0.2)
wait(1.5)
print('fw',[m['fw'] for m in d.fr if m.get('t')=='hello'][-1:] , 'rst',lastp()['rst'])
# 0 all off
for p in (1,9,12,13,14,15,16):
    pass
# ---- 1 DELAY echo burst
print('== DELAY (pad 16)'); ensure(16,0); d.send('sim tone off'); wait(0.5)
ensure(16,1); wait(0.5); base=levels(d.mark()); 
i0=d.mark(); d.send('sim tone 440 -30'); wait(0.3); d.send('sim tone off'); wait(2.0)
L=levels(i0); loud=[x for x in L if x[1]>-45]
# edge: last frame of tone
t_off=max(x[0] for x in L if x[1]>-40 and x[0]<L[0][0]+700) if L else 0
def at(t,w=15): v=[x[1] for x in L if abs((x[0]-t_off)-t)<=w]; return max(v) if v else None
print('tone-off edge ms',t_off,'pk at +100:',at(100),' +375:',at(375,20),' +750:',at(750,20),' +1125:',at(1125,20),' floor(+2000):',at(1900,60))
xf=lastxf(); print('tape xf',xf.get(10))
ensure(16,0)
# ---- 2 FREEZE
print('== FREEZE (pad 9 gate, 10 reset)'); ensure(9,0); wait(0.3)
d.send('sim tone 440 -30'); wait(0.3); d.send('sim pad 9'); wait(1.2); d.send('sim pad 9'); wait(0.3); d.send('sim tone off'); wait(1.5)
L=levels(d.mark()-300); print('frozen hold level pk dB (tone off, gate closed):',max(x[1] for x in L[-100:]), 'freeze xf',lastxf().get(4))
i0=d.mark(); d.send('sim pad 10'); wait(1.2); L=levels(i0); 
pk=[x[1] for x in L]; print('after reset: pk first',max(pk[:20]) if pk else None,' last 200ms',max(pk[-40:]),' freeze xf',lastxf().get(4))
# ---- 3 LOOPER
print('== LOOPER (pad 12 start, 11 play/stop)'); d.send('sim tone 440 -30'); wait(0.3); d.send('sim pad 12'); wait(1.5); d.send('sim pad 12'); wait(0.3); d.send('sim tone off'); wait(1.0)
L=levels(d.mark()-200); print('loop playing, tone off: pk',max(x[1] for x in L), 'looper xf',lastxf().get(3))
d.send('sim pad 11'); wait(1.0); L=levels(d.mark()-200); print('after stop: pk',max(x[1] for x in L))
d.send('sim padhold 12'); wait(1.0); print('after clear: looper xf',lastxf().get(3), 'pads',lastp())
# ---- 4 CHORUS toggle, noise
print('== CHORUS (pad 1)'); ensure(1,0); d.send('sim noise -40'); wait(2.0); b=levels(d.mark()-400); 
rb=statistics.mean(x[2] for x in b); i0=d.mark(); d.send('sim pad 1'); wait(2.5); L=levels(i0)
ra=[x[2] for x in L]; print('rms before %.1f dB; after: min %.1f mean %.1f max %.1f ; dropouts(ol)'%(rb,min(ra[:100]),statistics.mean(ra[-200:]),max(ra)), [m['ol'][2] for m in d.fr if 'ol' in m][-1])
print('chorus xf',lastxf().get(7)); ensure(1,0)
print('cpu',[m['cpu'] for m in d.fr if 'cpu' in m][-1],'st',[m['st'] for m in d.fr if 'st' in m][-1])
d.send("sim off"); d.send("nosave off"); time.sleep(0.5); d.close()
