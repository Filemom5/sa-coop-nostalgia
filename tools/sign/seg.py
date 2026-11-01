import sys; sys.path.insert(0,'.')
import rw, numpy as np
def load(name):
    return rw.read_dff(open('orig/%s.dff'%name,'rb').read()).geoms[0]
def components(g, eps=1e-3):
    # union-find over triangles sharing a vertex position
    key=np.round(g.verts/eps).astype(np.int64)
    _,canon=np.unique(key,axis=0,return_inverse=True)
    canon=canon.ravel()
    parent=list(range(len(g.verts)))
    def f(x):
        while parent[x]!=x:
            parent[x]=parent[parent[x]]; x=parent[x]
        return x
    first={}
    for i,c in enumerate(canon):
        if c in first: parent[f(i)]=f(first[c])
        else: first[c]=i
    for a,b,c,_ in g.tris:
        ra,rb,rc=f(a),f(b),f(c)
        parent[rb]=ra; parent[f(rc)]=f(ra)
    roots=np.array([f(t[0]) for t in g.tris])
    labels={r:i for i,r in enumerate(dict.fromkeys(roots))}
    return np.array([labels[r] for r in roots])
