"""stage V: voxelise the v5.2 shell and door (exported from the blend) over the housing box at 0.25 mm."""
import numpy as np
from vox import voxelize
lo=np.array([758.,488.,-62.]); hi=np.array([915.,592.,-6.]); h=0.25
S0=np.load('cur/shell_pebble_v4.npz'); D0=np.load('cur/battery_door_v4.npz')
np.save('cache/Sv.npy',voxelize(S0['v'],S0['f'],lo,hi,h)); np.save('cache/Dv.npy',voxelize(D0['v'],D0['f'],lo,hi,h)); print('voxelised')
