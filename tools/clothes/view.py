import sys; sys.path.insert(0,'../sign')
import rw, render, numpy as np
from PIL import Image
g=rw.read_dff(open('orig/coach.dff','rb').read()).geoms[0]
ctr=(g.verts.min(0)+g.verts.max(0))/2
def views(texpath, out):
    tex=np.array(Image.open(texpath).convert('RGBA'))
    m=render.Mesh(g.verts,g.tris[:,:3],uvs=g.uvs[0],colors=np.ones((len(g.verts),4)),textures=[tex],tex_of_tri=np.zeros(len(g.tris),int))
    ims=[render.render([m],eye=ctr+np.array(e),target=ctr,w=500,h=620,fov=30,ambient=0.75,sun=(0.5,0.3,0.8)) for e in ((3,0.0,0.2),(-3,0,0.2),(2.2,2.2,0.3))]
    o=Image.new('RGB',(1500,620)); [o.paste(im,(i*500,0)) for i,im in enumerate(ims)]; o.save(out)
views('out/coach_final.png','/tmp/lilac_views.png'); views('out/coachsemi_final.png','/tmp/vapor_views.png')
