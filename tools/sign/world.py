import sys, glob, re; sys.path.insert(0,'.')
import rw, numpy as np
MAPS='orig/LA/'  # the game's data/maps/LA (text IDE and IPL files)
def ide_names():
    names={}
    for f in glob.glob(MAPS+'*.ide'):
        sec=None
        for line in open(f,errors='replace'):
            line=line.strip()
            if not line or line.startswith('#'): continue
            if line in ('objs','tobj','anim','end','path','2dfx','txdp','peds','cars','hier','weap'): sec=line; continue
            if sec in ('objs','tobj','anim'):
                p=[x.strip() for x in line.split(',')]
                names[int(p[0])]=(p[1].lower(), p[2].lower())
    return names
def instances():
    out=[]
    for f in glob.glob(MAPS+'*.ipl'):
        sec=None
        for line in open(f,errors='replace'):
            line=line.strip()
            if line in ('inst','end','cull','pick','path','enex','grge','zone','jump','tcyc','auzo','mult','cars','occl'): sec=line; continue
            if sec=='inst' and line and not line.startswith('#'):
                p=[x.strip() for x in line.split(',')]
                out.append({'id':int(p[0]),'name':p[1].lower(),'pos':tuple(map(float,p[3:6])),'rot':tuple(map(float,p[6:10])),'lod':int(p[10]),'src':f.split('/')[-1]})
    for f in sorted(glob.glob('orig/lahills_stream*.ipl')):
        for e in rw.read_bnry_ipl(open(f,'rb').read()):
            e['src']=f; out.append(e)
    return out
def quat_matrix(q):
    x,y,z,w=q
    # GTA IPL quaternions are conjugated (inverse rotation)
    x,y,z=-x,-y,-z
    return np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])
def cols():
    out={}
    for f in sorted(glob.glob('orig/lahills_*.col')):
        for m in rw.read_col(open(f,'rb').read()):
            m.file=f; out[m.name.lower()]=m
    return out
