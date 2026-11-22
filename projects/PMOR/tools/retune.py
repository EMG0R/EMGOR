import re,subprocess,sys
SRC='/home/claude/pmor/DaisyExamples/seed/PMOR_V3/engine/drum_sounds.cpp'
TARGET={0:0.19,1:0.19,2:0.075,3:0.19}; CAP=2.2
def measure():
    subprocess.check_call("g++ -O2 -std=gnu++14 -I../pmor/DaisyExamples/seed/PMOR_V3 -I../pmor/DaisyExamples/seed/PMOR_V3/engine loud.cpp stub.cpp ../pmor/DaisyExamples/seed/PMOR_V3/engine/drums.cpp ../pmor/DaisyExamples/seed/PMOR_V3/engine/drum_sounds.cpp ../pmor/DaisyExamples/seed/PMOR_V3/engine/scale.cpp -o loud",shell=True)
    out=subprocess.check_output("./loud",shell=True).decode()
    m={}
    for ln in out.splitlines():
        r=re.match(r"(\d)/\s*(\d+)\s+(\S+)\s+pk=([\d.]+) loud=([\d.]+) rms=([\d.]+)",ln)
        m[(int(r[1]),int(r[2]))]=(float(r[4]),float(r[5]),float(r[6]),r[3])
    return m,out
src=open(SRC).read()
m,_=measure()
# rows: find each "/*N name*/ { ... .gain = X" in group order
lines=src.split('\n'); gi=-1; idx=0; changes=0
for i,ln in enumerate(lines):
    if re.search(r'^\s*static const DrumDef\b|DRUM_DEFS\[\d\]\[|^static const DrumDef', ln): pass
    r=re.match(r'\s*/\*\s*(\d+)\s+(\w+)\s*\*/', ln)
    if r:
        n=int(r[1])
        if n==0: gi+=1
        cur=(gi,n)
# simpler: process by scanning rows and .gain occurrences sequentially
rows=[]  # (line index of .gain, group, sound)
gi=-1
for i,ln in enumerate(lines):
    r=re.match(r'\s*/\*\s*(\d+)\s+(\w+)\s*\*/', ln)
    if r:
        n=int(r[1])
        if n==0: gi+=1
        cur=(gi,n)
    if '.gain =' in ln:
        rows.append((i,cur))
assert len(rows)==64, len(rows)
for i,(gr,sn) in rows:
    pk,loud,rms,name=m[(gr,sn)]
    g=float(re.search(r'\.gain = ([\d.]+)f',lines[i])[1])
    f=TARGET[gr]/loud
    if pk*f>CAP: f=CAP/pk
    ng=g*f
    lines[i]=re.sub(r'\.gain = [\d.]+f','.gain = %.3ff'%ng,lines[i])
open(SRC,'w').write('\n'.join(lines))
m2,out=measure(); print(out)
