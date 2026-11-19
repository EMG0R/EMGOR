import sys,time,statistics; sys.path.insert(0,'bringup/tools')
from dev import Dev
d=Dev(); time.sleep(1.5)
def st(): return [m['pads'] for m in d.fr if 'pads' in m][-1]
def stt(): return [m['st'] for m in d.fr if 'st' in m][-1]
def bl(): return [m['bl'] for m in d.fr if 'bl' in m][-1]
def w(t): time.sleep(t)
for c in ['nosave on','sim off','ogain 0','sim fader 1 0','sim noise -40']: d.send(c); w(0.2)
w(2.5)
rows=[m['bl'] for m in d.fr[-800:] if 'bl' in m]; print('OFF (capture only): avg %.2f%% worst %.2f%%  on=%d'%(statistics.mean(r[1] for r in rows[-40:])/10,max(r[2] for r in rows)/10,rows[-1][0]))
s0=stt(); i0=d.mark(); b=statistics.mean([m['mo'][1]/10 for m in d.fr[-300:] if 'mo' in m])
d.send('sim pad 7'); w(1.0)   # tap -> toggle after the 350 ms double-tap window
print('after tap: bl',bl(),'pads on bit',(st()['on']>>6)&1)
w(4.0); rows=[m['bl'] for m in d.fr[-900:] if 'bl' in m]; print('ON: avg %.1f%% worst %.1f%%'%(statistics.mean(r[1] for r in rows[-40:])/10,max(r[2] for r in rows)/10))
F=[m['mo'][1]/10 for m in d.fr[i0:] if 'mo' in m]; print('level steady %.1f dB; during on: min %.1f max %.1f; xrun %d->%d gap %d->%d; dropouts %s'%(b,min(F),max(F),s0['xrun'],stt()['xrun'],s0['gap'],stt()['gap'],[m['ol'][2] for m in d.fr if 'ol' in m][-1]))
d.send('sim joy 1000'); w(2.0); print('joy up: speedmul live; cpu bl',bl()); d.send('sim joy -2000'); 
d.send('sim pad 7'); w(0.1); d.send('sim pad 7'); w(0.8); print('double tap -> cue, still on?',(st()['on']>>6)&1,bl())
d.send('sim padhold 7'); w(0.5); print('hold -> clear ok; on?',(st()['on']>>6)&1)
d.send('sim pad 7'); w(1.0); print('tap -> toggle off: on?',(st()['on']>>6)&1)
print('fader map pickup: sel',st()['sel'],'fl',st()['fl'])
d.send('sim off'); d.send('nosave off'); w(0.5); d.close()
