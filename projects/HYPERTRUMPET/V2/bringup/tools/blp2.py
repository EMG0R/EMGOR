import sys,time,statistics; sys.path.insert(0,'bringup/tools')
from dev import Dev
d=Dev(); time.sleep(2)
def st(): return [m['pads'] for m in d.fr if 'pads' in m][-1]
def bl(): return [m['bl'] for m in d.fr if 'bl' in m][-1]
def w(t): time.sleep(t)
for c in ['nosave on','sim off','ogain 0','sim fader 1 0','sim noise -40']: d.send(c); w(0.2)
w(1.0)
if bl()[0]: d.send('sim pad 7'); w(1.0)
w(2.0); rows=[m['bl'] for m in d.fr[-800:] if 'bl' in m]; print('OFF capture-only: avg %.2f%% worst %.2f%% (on=%d)'%(statistics.mean(r[1] for r in rows[-40:])/10,max(r[2] for r in rows)/10,rows[-1][0]))
d.send('sim pad 7'); w(5.0); rows=[m['bl'] for m in d.fr[-900:] if 'bl' in m]; print('ON: avg %.1f%% worst %.1f%% (on=%d)'%(statistics.mean(r[1] for r in rows[-40:])/10,max(r[2] for r in rows)/10,rows[-1][0]))
i0=d.mark(); d.send('sim joy 1000'); w(2.0); r=[m['bl'] for m in d.fr[i0:] if 'bl' in m]; print('joystick up: bl avg %.1f%% worst %.1f%%'%(statistics.mean(x[1] for x in r[-30:])/10,max(x[2] for x in r)/10)); d.send('sim joy -2000'); w(0.5)
d.send('sim pad 7'); w(0.1); d.send('sim pad 7'); w(1.0); print('double-tap cue: still on =',bl()[0])
d.send('sim padhold 7'); w(0.6); print('hold clear: on =',bl()[0])
d.send('sim pad 7'); w(1.0); print('tap -> off: on =',bl()[0])
s=[m['st'] for m in d.fr if isinstance(m.get('st'),dict)][-1]; print('st',s)
d.send('sim off'); d.send('nosave off'); w(0.5); d.close()
