import json,sys
S=sys.argv[1]; out=sys.argv[2]; name=sys.argv[3]
shell=dict(stl=S,color=[0.36,0.20,0.62],name='shell',rough=0.15)
def sh(n,l,t,rot,lens=40,res=(1500,1000),e=4.0): return dict(name=name+'_'+n,loc=l,target=t,lens=lens,res=list(res),keyrot=rot,keyE=e)
C=[835,545,55]
def so(n,l,t,rot,sc,res=(1500,1000)): d=sh(n,l,t,rot,res=res); d['ortho']=sc; return d
shots=[so('front',[835,1250,50],C,(80,0,60),230),so('back',[835,-200,50],C,(80,0,-30),230),so('top',[835,545,900],[835,545,60],(80,0,20),215),so('bottom',[835,545,-800],[835,545,-10],(80,0,160),215),
 so('end_minusx',[100,550,40],[760,550,40],(80,0,-70),170,(1200,1200)),so('end_plusx',[1500,550,60],[900,550,60],(80,0,-120),170,(1200,1200)),
 sh('q34_a',[1080,860,330],C,(75,0,40),lens=50),sh('q34_b',[560,170,-190],C,(75,0,150),lens=50)]
json.dump(dict(objects=[shell],out=out,shots=shots),open('spec_'+name+'.json','w'))
