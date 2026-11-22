import sys,json,time,subprocess
# burst.py <pad> : tone on 3 s with the effect on, tone off, print mo/ol vs ms around the off edge
def q(*c): subprocess.run(['zsh','tools/cmdq.sh',*c])
def frames():
    out=[]
    for l in open('logs/tele.jsonl').read().splitlines()[-2000:]:
        try: m=json.loads(l.split(' ',1)[1])
        except: continue
        if m.get('t')=='s': out.append(m)
    return out
pad=sys.argv[1]
q('nosave on','sim off','sim fader 1 0','sim tone 440 -30'); time.sleep(2)
q('sim pad '+pad); time.sleep(3)
t_off=time.time(); q('sim tone off'); time.sleep(2.5)
F=frames()
# find the first frame where tone is gone: mo pk drops sharply
prev=None; rows=[]
for m in F:
    if 'mo' in m: rows.append((m['ms'],m['mo'][0],m['mo'][1],m.get('xf')))
# print compact: ms, pk dB
out=[]
base=None
for ms,pk,rms,xf in rows:
    out.append((ms,pk/10.0))
# locate drop
for i in range(1,len(out)):
    if out[i-1][1]>-45 and out[i][1]<out[i-1][1]-6:
        print('first drop at ms',out[i][0],'pk',out[i-1][1],'->',out[i][1]); start=out[i][0]; break
else: print('no drop found'); start=None
if start:
    print([ (ms-start,round(pk,1)) for ms,pk in out if start-20<=ms<=start+900][::8])
print('state', [ (m['pads'],m.get('xf')) for m in F if 'pads' in m][-1])
