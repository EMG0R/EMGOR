import sys,json
# rdl.py N: last N telemetry lines -> compact summary of the s frames with heavy blocks, flags reboots
prev=None; rows=[]
L=open('logs/tele.jsonl').read().splitlines()[-int(sys.argv[1]):]
for l in L:
    try: m=json.loads(l.split(' ',1)[1])
    except: continue
    ms=m.get('ms')
    if m.get('t')=='s':
        if prev is not None and ms<prev: print('REBOOT',prev,'->',ms,l[:12])
        prev=ms
        if 'dd' in m: rows.append((l[:12],ms,m))
for t,ms,m in rows[-int(sys.argv[2] if len(sys.argv)>2 else 3):]:
    print(t,ms,{k:m.get(k) for k in ['fs','dd','mo','xf','pads','cpu','st']})
