import sys; sys.path.insert(0,'.')
from world import *
SIGN=np.array([1413.4140625,-804.7421875,83.4375])
def ground_tris(radius=220, skip=('vinesign1_cunte','vinesign1_cunte01')):
    names=ide_names(); inst=instances(); C=cols()
    tris=[]; owners=[]
    for e in inst:
        nm=names.get(e['id'],('?','?'))[0]
        if nm in skip or nm not in C: continue
        if np.linalg.norm(np.array(e['pos'][:2])-SIGN[:2])>radius: continue
        m=C[nm]
        if m.verts is None or not len(m.faces): continue
        R=quat_matrix(e['rot']); p=np.array(e['pos'])
        V=m.verts@R.T+p
        f=np.array([x[:3] for x in m.faces])
        tris.append(V[f]); owners+= [nm]*len(f)
    return np.concatenate(tris), owners
def height_at(T, x, y, below=None):
    # highest triangle under (x, y) [optionally below z]
    a,b,c=T[:,0],T[:,1],T[:,2]
    v0=b-a; v1=c-a
    d=v0[:,0]*v1[:,1]-v0[:,1]*v1[:,0]
    ok=np.abs(d)>1e-9
    px=x-a[:,0]; py=y-a[:,1]
    u=(px*v1[:,1]-py*v1[:,0])/np.where(ok,d,1)
    v=(v0[:,0]*py-v0[:,1]*px)/np.where(ok,d,1)
    inside=ok&(u>=-1e-6)&(v>=-1e-6)&(u+v<=1+1e-6)
    z=a[:,2]+u*v0[:,2]+v*v1[:,2]
    z=z[inside]
    if below is not None: z=z[z<below]
    return z.max() if len(z) else None
