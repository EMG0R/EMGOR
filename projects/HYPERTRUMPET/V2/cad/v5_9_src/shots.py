import json,sys
S=sys.argv[1]; out=sys.argv[2]; name=sys.argv[3]; ids=sys.argv[4].split(',')
shell=dict(stl=S,color=[0.36,0.20,0.62],name='shell',rough=0.15)
sh=lambda n,l,t,lens=45,res=(1400,1000): dict(name=name+'_'+n,loc=l,target=t,lens=lens,res=list(res))
def rk(d,rot,e=4.0):
    d['keyrot']=rot; d['keyE']=e; return d
ALL=dict(
 seamface=sh('seamface',[700,640,30],[745,568,30],45),
 seamface2=sh('seamface2',[720,620,90],[742,568,85],40),
 lobeedge=sh('lobeedge',[600,560,5],[735,548,5],35),
 minusx=sh('minusx',[560,640,110],[750,550,20],50),
 below=sh('below',[640,470,-120],[755,550,-10],45),
 back=sh('back',[760,300,40],[755,545,30],60),
 front34=sh('front34',[640,700,60],[750,555,10],50),
 sweep=sh('sweep',[650,480,-60],[755,552,-20],40),
 sweep2=sh('sweep2',[690,420,-40],[760,552,-18],45),
 sweepfront=sh('sweepfront',[660,640,-50],[760,556,-18],45),
 backfull=sh('backfull',[835,230,50],[835,545,40],55,(1500,1000)),
 backlobe=sh('backlobe',[800,300,55],[750,545,50],50),
 frontfull=sh('frontfull',[835,900,50],[835,560,40],55,(1500,1000)),
 rimtop=sh('rimtop',[800,330,150],[800,545,118],45),
 backbot=sh('backbot',[740,380,-40],[760,541,-18],40),
 backbot2=sh('backbot2',[700,420,-70],[758,545,-20],40),
 seam_corner=sh('seam_corner',[700,640,-45],[771,576,-36],25),
 seam_corner2=sh('seam_corner2',[740,700,-30],[771,578,-36],25),
 seam_z100=sh('seam_z100',[640,520,100],[729,550,100],25),
 seam_z100b=sh('seam_z100b',[690,640,100],[738,568,100],25),
 seam_z2=sh('seam_z2',[720,650,2],[765,568,2],25),
 seam_x782=sh('seam_x782',[760,420,-10],[782,552,-12],25),
 rake_minusx=rk(sh('rake_minusx',[560,640,110],[750,550,20],50),(80,0,-70)),
 rake_edge=rk(sh('rake_edge',[620,570,15],[735,548,15],30),(85,0,-60)),
 rake_sweep=rk(sh('rake_sweep',[650,480,-60],[755,552,-20],40),(75,0,120)),
 rake_top=rk(sh('rake_top',[800,600,200],[800,555,60],45),(80,0,20)),
 rake_bottom=rk(sh('rake_bottom',[800,520,-250],[800,545,10],45),(80,0,160)),
 rake_plusx=rk(sh('rake_plusx',[1100,550,60],[890,552,60],45),(80,0,-120)),
 rake_front=rk(sh('rake_front',[835,900,50],[835,560,40],55,(1500,1000)),(80,0,60)),
 rake_back=rk(sh('rake_back',[835,230,50],[835,545,40],55,(1500,1000)),(78,0,-30)),
 rimleft=sh('rimleft',[700,380,60],[735,545,60],40),
)
json.dump(dict(objects=[shell],out=out,shots=[ALL[i] for i in ids]),open('spec_'+name+'.json','w'))
