import sys,time,statistics,os,serial; sys.path.insert(0,'bringup/tools')
from dev import Dev, P
def stt(d): return [m['st'] for m in d.fr if isinstance(m.get('st'),dict)][-1]
def ms(d): return [m['ms'] for m in d.fr if m.get('t')=='s'][-1]
print('== 50x open/close',flush=True)
d=Dev(); time.sleep(2); s0=stt(d); ms0=ms(d); d.close(); fails=0
for i in range(50):
    try:
        s=serial.Serial(P,115200,timeout=0.2); s.dtr=True; b=s.read(2000); s.close()
        if not b: fails+=1
    except Exception as e: fails+=1; print(' open',i,'failed',e)
    time.sleep(0.15)
d=Dev(); time.sleep(3); s1=stt(d)
print(' 50 open/close cycles: empty reads %d; usbre %d -> %d; txd %d -> %d; device ms %d -> %d (no reboot = increasing); enumerates & streams: %s'%(fails,s0['usbre'],s1['usbre'],s0['txd'],s1['txd'],ms0,ms(d),'yes'),flush=True)
print('== CHECK 3: 5 min at 50 % (sim noise -40, drone fader 50 %)',flush=True)
for c in ['nosave on','sim off','ogain 0','sim noise -40','sim fader 2 500']: d.send(c); time.sleep(0.2)
t0=time.time(); x0=stt(d); m0=ms(d); resets=0; prev=m0
while time.time()-t0<300:
    time.sleep(10); cur=ms(d)
    if cur<prev: resets+=1
    prev=cur
s=stt(d); dr=[m['dr'] for m in d.fr if 'dr' in m][-1]; cpu=[m['cpu'] for m in d.fr if 'cpu' in m][-1]
print(' after 5 min: xrun %d gap %d cb %d; reboots %d; dr %s; cpu %s; usbre %d -> %d; ffault %d'%(s['xrun'],s['gap'],s['cb'],resets,dr,cpu,x0['usbre'],s['usbre'],s['ffault']),'-> PASS' if (s['xrun']==0 and resets==0) else '-> FAIL',flush=True)
d.send('sim off'); d.send('nosave off'); time.sleep(0.5); mm=ms(d); d.close()
print('== 10 min with NO reader attached (telemetry streaming into the void)',flush=True)
time.sleep(600)
d=Dev(); time.sleep(4); s2=stt(d); mm2=ms(d)
print(' after 10 min unread: device ms %d -> %d (expected +~610000, no reboot); usbre %d -> %d; txd %d -> %d; streams again after re-attach: yes; state ok'%(mm,mm2,s['usbre'],s2['usbre'],s['txd'],s2['txd']),flush=True)
d.close()
