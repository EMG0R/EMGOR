import sys,time,statistics; sys.path.insert(0,'bringup/tools')
from dev import Dev
d=Dev(); time.sleep(2)
def st(): return [m['pads'] for m in d.fr if 'pads' in m][-1]
def w(t): time.sleep(t)
def pd(fx):
    d.send('pd '+fx); w(1.5); P={}
    for m in d.fr[-400:]:
        if m.get('t')=='pd' and m.get('fx')==fx:
            for p in m['p']: P[p['n']]=p
    return P
h=[m for m in d.fr if m.get('t')=='hello'][-1]; print('fw',h['fw'],'rst',h['rst'],'bc',h.get('bc'))
P=pd('moodii'); print('moodii params (v / def):',[(n,round(p['v'],3),round(p['def'],3)) for n,p in P.items()])
bad=[n for n,p in P.items() if abs(p['v']-p['def'])>1e-3]; print('non-default:',bad)
d.send('nosave on'); w(0.2)
for n in bad: d.send('p moodii %s %.4f'%(n,P[n]['def'])); w(0.15)   # reset moodii only
for c in ['sim off','ogain 0','sim fader 1 0','sim tone 440 -30']: d.send(c); w(0.2)
w(1.0)
def level(tag):
    d.send('sim pad 6'); w(2.5); i=d.mark(); w(2.0); F=[m['mo'][1]/10 for m in d.fr[i:] if 'mo' in m]; P2=[m['mo'][0]/10 for m in d.fr[i:] if 'mo' in m]
    hold=[m for m in d.fr if 'hw' in m]
    print(tag,'out rms mean %.1f (input tone rms -33.0), pk max %.1f; on=%d'%(statistics.mean(F),max(P2),(st()['on']>>5)&1)); d.send('sim pad 6'); w(1.0)
# dry only level
i=d.mark(); w(1.5); F=[m['mo'][1]/10 for m in d.fr[i:] if 'mo' in m]; print('dry only: rms %.1f'%statistics.mean(F))
for mix in (0.5,1.0):
    d.send('p moodii MIX %.2f'%mix); w(0.4); level('MIX %.1f'%mix)
for wm in (0,1,2):
    d.send('p moodii WMODE %d'%wm); d.send('p moodii MIX 1.0'); w(0.4); level('WMODE %d MIX 1.0'%wm)
d.send('p moodii WMODE %d'%int(P['WMODE']['def'])); d.send('p moodii MIX %.2f'%P['MIX']['def'])
d.send('sim off'); d.send('nosave off'); w(0.5); d.close()
