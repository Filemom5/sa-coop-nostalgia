import sys; sys.path.insert(0,'.')
import rw, render, numpy as np
txd=rw.read_txd(open('orig/mulhouslahills.txd','rb').read())
def mesh_of(path, night=False, bright=1.6):
    g=rw.read_dff(open(path,'rb').read()).geoms[0]
    texs=[txd[m.texture.lower()].rgba if m.texture and m.texture.lower() in txd else None for m in g.materials]
    c=(g.night if night else g.prelit)/255.0
    c=c.copy(); c[:,3]=1; c[:,:3]=np.clip(c[:,:3]*bright,0,1)
    return render.Mesh(g.verts, g.tris[:,:3], uvs=g.uvs[0], colors=c, textures=texs, tex_of_tri=g.tris[:,3])
def ground_mesh():
    T=np.load('orig/ground.npy')-np.array([1413.4140625,-804.7421875,83.4375])
    keep=(np.abs(T[:,:,0]).max(1)<90)&(np.abs(T[:,:,1]).max(1)<60)
    T=T[keep]
    V=T.reshape(-1,3); F=np.arange(len(V)).reshape(-1,3)
    return render.Mesh(V,F,colors=np.tile((0.55,0.5,0.38,1),(len(V),1)))
def views(prefix, d, night=False, ground=True):
    ms=[mesh_of(d+'/vinesign1_cunte.dff',night), mesh_of(d+'/vinesign1_cunte01.dff',night)]
    if ground: ms.append(ground_mesh())
    amb=0.55
    render.render(ms, eye=(0,-110,6), target=(0,0,0), w=1400,h=480,fov=26,ambient=amb).save(prefix+'_front.png')
    render.render(ms, eye=(-35,-48,6), target=(-20,0,0), w=1100,h=600,fov=45,ambient=amb).save(prefix+'_left.png')
    render.render(ms, eye=(30,-45,4), target=(18,0,0), w=1100,h=600,fov=45,ambient=amb).save(prefix+'_right.png')
    render.render(ms, eye=(10,70,20), target=(0,0,-2), w=1400,h=600,fov=45,ambient=amb).save(prefix+'_back.png')
