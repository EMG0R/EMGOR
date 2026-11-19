# rd.py <port> <out.txt>: open the Seed's USB serial, send hello, save 3 s of output, print key telemetry fields
import serial,time,re,sys
p=sys.argv[1]
for _ in range(80):
  try: s=serial.Serial(p,115200,timeout=0.3); break
  except Exception: time.sleep(0.25)
else: print('PORT BUSY'); sys.exit()
time.sleep(1.5);s.reset_input_buffer();s.write(b'hello\n');t=time.time();b=b''
while time.time()-t<3:b+=s.read(16384)
s.close();d=b.decode(errors='replace')
open(sys.argv[2],'w').write(d)
for k in [r'"fw":"[^"]+"',r'"fs":\[[^\]]*\]',r'"dr":\{[^}]*\}',r'"cpu":\{[^}]*\}',r'"st":\{[^}]*\}',r'"tr":\{[^}]*\}',r'"strip":\{[^}]*\}']:
  m=re.findall(k,d);print(m[-1][:300] if m else 'none '+k)
