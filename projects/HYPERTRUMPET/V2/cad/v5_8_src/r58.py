import json, numpy as np
CAD='/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/HYPERTRUMPET_V2/cad'; S=CAD+'/stl_v5_8/'
shell=dict(stl=S+'shell_pebble_v5_8.stl',color=[0.36,0.20,0.62],name='shell',rough=0.18); door=dict(stl=S+'battery_door_v5_8.stl',color=[0.62,0.50,0.85],name='door',rough=0.18)
mount=dict(stl=S+'mount_original_arms_plate_v5_8.stl',color=[0.25,0.22,0.45],name='mount',rough=0.3); caps=dict(stl='out/caps_all.stl',color=[0.85,0.45,0.70],name='caps',rough=0.3)
sh=lambda n,l,t,lens=45,res=(1400,1000): dict(name=n,loc=l,target=t,lens=lens,res=list(res))
OUT=CAD+'/renders_v5_8'
json.dump(dict(objects=[shell,door,mount,caps],out=OUT,shots=[sh('v5_8_lobe_underside_user_angle',[640,380,-90],[752,545,-8],40),sh('v5_8_lobe_underside_b',[700,520,-110],[760,545,-10],40),sh('v5_8_fsr_3q',[1090,720,170],[905,568,112],48),sh('v5_8_three_quarter',[1150,900,330],[835,550,40],45),
 dict(name='v5_8_logo_straight',loc=[787,900,19.85],target=[787,568,19.85],ortho=96,res=[1700,800])]),open('r58a.json','w'))
json.dump(dict(objects=[shell,caps],out=OUT,shots=[sh('v5_8_back_open_pod_big_hole',[835,250,-60],[835,545,-8],55,(1500,1000)),sh('v5_8_back_open_pod_overview',[835,230,60],[835,545,40],55,(1500,1000)),sh('v5_8_back_big_hole_close',[820,360,-40],[818,538,-17],50)]),open('r58b.json','w'))
# exploded: mount moved -y 45, shell +y 0, side-by-side view from the back-side
import trimesh
M=trimesh.load(S+'mount_original_arms_plate_v5_8.stl',force='mesh'); M.apply_translation([0,-45,0]); M.export('out/mount_expl.stl')
mount2=dict(stl='out/mount_expl.stl',color=[0.25,0.22,0.45],name='mount',rough=0.3)
json.dump(dict(objects=[shell,mount2],out=OUT,shots=[sh('v5_8_rods_sockets_exploded',[1180,250,110],[835,520,50],40,(1600,1000)),sh('v5_8_rods_sockets_exploded_low',[1000,230,-30],[835,520,10],40,(1600,1000)),sh('v5_8_rods_sockets_exploded_top',[835,200,260],[835,520,50],40,(1600,1000))]),open('r58c.json','w'))
