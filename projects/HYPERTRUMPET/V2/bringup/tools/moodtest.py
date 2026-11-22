import sys,time,statistics; sys.path.insert(0,'bringup/tools')
from dev import Dev
d=Dev(); time.sleep(2)
def st(): return [m['pads'] for m in d.fr if 'pads' in m][-1]
def stt(): return [m['st'] for m in d.fr if isinstance(m.get('st'),dict)][-1]
def w(t): time.sleep(t)
def xrow(gi): 
    r=[x for m in d.fr[-900:] if 'xf' in m for x in m['xf'] if x[0]==gi]; return r
h=[m for m in d.fr if m.get('t')=='hello']; print('fw',h[-1].get('fw'),'rst',h[-1].get('rst'),'bc',h[-1].get('bc'),'ax',st().get('ax'))
for c in ['nosave on','sim off','ogain 0','sim fader 1 0','sim fader 2 0','sim fader 3 0','sim noise -40']: d.send(c); w(0.2)
w(2.0)
# ---- BLOOPER
print('== BLOOPER (pad 7)')
rows=[m['bl'] for m in d.fr[-800:] if 'bl' in m]; print(' OFF (capture only): avg %.2f%% worst %.2f%% on=%d'%(statistics.mean(r[1] for r in rows[-40:])/10,max(r[2] for r in rows)/10,rows[-1][0]))
b=statistics.mean([m['mo'][1]/10 for m in d.fr[-300:] if 'mo' in m]); s0=stt(); i0=d.mark(); dp0=[m['ol'][2] for m in d.fr if 'ol' in m][-1]
d.send('sim pad 7'); w(1.0); print(' tap -> toggled on:',(st()['on']>>6)&1, 'bl',[m['bl'] for m in d.fr if 'bl' in m][-1])
w(4.0); rows=[m['bl'] for m in d.fr[-900:] if 'bl' in m]; F=[m['mo'][1]/10 for m in d.fr[i0:] if 'mo' in m]
print(' ON: avg %.1f%% worst %.1f%%; level steady %.1f dB, during min %.1f max %.1f; xrun %d->%d gap %d->%d; dropouts +%d'%(statistics.mean(r[1] for r in rows[-40:])/10,max(r[2] for r in rows)/10,b,min(F),max(F),s0['xrun'],stt()['xrun'],s0['gap'],stt()['gap'],[m['ol'][2] for m in d.fr if 'ol' in m][-1]-dp0))
d.send('sim fader 1 1000'); w(1.0); print(' fader 1 -> PROB pickup live:',st()['fl']); d.send('sim fader 1 0')
d.send('sim joy 1000'); w(1.5); rr=[m['bl'] for m in d.fr[-300:] if 'bl' in m]; print(' joystick up held: bl',rr[-1],'(SPEEDMUL param is driven while on)'); d.send('sim joy -2000'); w(0.5)
d.send('sim pad 7'); w(0.1); d.send('sim pad 7'); w(0.8); print(' double-tap -> cue (stays on):',(st()['on']>>6)&1)
d.send('sim padhold 7'); w(0.5); print(' hold -> clear: still on',(st()['on']>>6)&1)
d.send('sim pad 7'); w(1.0); print(' tap -> off:',(st()['on']>>6)&1)
# ---- MOOD
print('== MOOD (pad 6)')
d.send('sim fader 1 0'); w(0.3); d.send('sim tone 440 -30'); d.send('sim noise off'); w(0.5)
dp0=[m['ol'][2] for m in d.fr if 'ol' in m][-1]; i0=d.mark(); d.send('sim pad 6'); w(2.5)
r=xrow(8-6+6) if False else xrow(2); F=[m['mo'][1]/10 for m in d.fr[i0:] if 'mo' in m]
print(' toggle on: on=%d cpu avg %.1f%% worst %.1f%%; dropouts +%d; xrun %d gap %d'%((st()['on']>>5)&1,statistics.mean(x[2] for x in r[-40:])/10 if r else -1,max(x[3] for x in r[-60:])/10 if r else -1,[m['ol'][2] for m in d.fr if 'ol' in m][-1]-dp0,stt()['xrun'],stt()['gap']))
for wm in (0,1,2):
    d.send('p mood WMODE %d'%wm); w(2.0); r=xrow(2); print(' WMODE',wm,'cpu avg %.1f%% worst %.1f%%'%(statistics.mean(x[2] for x in r[-30:])/10,max(x[3] for x in r[-40:])/10),flush=True)
d.send('p mood WMODE 2'); w(0.5)
for clk in (0.0,0.25,0.5,0.75,1.0):
    d.send('p mood CLOCK %.2f'%clk); w(1.5); i=d.mark(); w(1.0); F=[m['mo'][1]/10 for m in d.fr[i:] if 'mo' in m]; r=xrow(2)
    print(' CLOCK %.2f: out rms mean %.1f min %.1f; cpu avg %.1f%% worst %.1f%%'%(clk,statistics.mean(F),min(F),statistics.mean(x[2] for x in r[-30:])/10,max(x[3] for x in r[-40:])/10))
d.send('p mood CLOCK 0.5'); d.send('sim pad 6'); w(0.5)
print('final st',stt(),'rf',st().get('rf'),'shed',st().get('shed'),'sdram n/a')
d.send('sim off'); d.send('nosave off'); w(0.5); d.close()
