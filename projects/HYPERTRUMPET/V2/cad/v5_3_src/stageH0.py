import numpy as np, time
from scipy import ndimage as ndi
lo=np.array([758.,488.,-62.]); h=0.25
Sv=np.load('cache/Sv.npy'); Dv=np.load('cache/Dv.npy'); U0=Sv|Dv
def dil(X,r):  return ndi.distance_transform_edt(~X,sampling=h)<=r
def ero(X,r):  return ndi.distance_transform_edt(X,sampling=h)>r
U=ero(dil(U0,0.9),0.9); np.save('cache/U.npy',U); print(U0.sum(),U.sum())
