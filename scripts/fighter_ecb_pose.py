"""Print a move's ECB, frame by frame, by posing the fighter's skeleton from the ISO.

mpColl_LoadECB_JObj takes the box around six joints (ftData + 0x44), relative to
TopN, then applies its minimums. This does the same on the move's animation, for
an airborne fighter that can grab ledges (no +2 padding). TransN's X and Z are
root motion, which the game turns into velocity, so they're zeroed. No blending
between moves, and no scale but the model's. The side-B constants in
src/recovery.c (SP_*_DASH_ECB_*) come from this. See notes/EDGEGUARD-SPACIE-SIDE-B.md.

Usage: python -I scripts/fighter_ecb_pose.py PlFx PlFxNr.dat SpecialAirS [Move ...]
       Run from the repository root, with melee.iso there.
"""
import math
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fighter_move_data as f


def mat_identity():
    return [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]]


def mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def local(t,r,s):
    cx,sx=math.cos(r[0]),math.sin(r[0]); cy,sy=math.cos(r[1]),math.sin(r[1]); cz,sz=math.cos(r[2]),math.sin(r[2])
    # HSD_MtxSRT: T * Rz * Ry * Rx * S
    R=[[cy*cz, sx*sy*cz-cx*sz, cx*sy*cz+sx*sz],
       [cy*sz, sx*sy*sz+cx*cz, cx*sy*sz-sx*cz],
       [-sy,   sx*cy,          cx*cy]]
    m=mat_identity()
    for i in range(3):
        for j in range(3): m[i][j]=R[i][j]*s[j]
        m[i][3]=t[i]
    return m

def joints(body, at, parent, out):
    # HSD_Joint: name 0, flags 4, child 8, next C, dobj 10, rot 14, scale 20, trans 2C
    while at:
        child, nxt = f.u32(body, at+8), f.u32(body, at+0xC)
        r = struct.unpack_from('>3f', body, at+0x14); s = struct.unpack_from('>3f', body, at+0x20); t = struct.unpack_from('>3f', body, at+0x2C)
        idx = len(out); out.append((parent, r, s, t))
        if child: joints(body, child, idx, out)
        at = nxt

def run(char, nr, moves):
    with open('melee.iso','rb') as iso:
        pl, aj, model = f.read_file(iso, char+'.dat'), f.read_file(iso, char+'AJ.dat'), f.read_file(iso, nr)
    body, roots = f.archive(pl)
    ft = next(off for name, off in roots.items() if name.startswith('ftData'))
    common, table = f.u32(body, ft), f.u32(body, ft+0x0C)
    scale = f.f32(body, common+0x8C)
    x44 = f.u32(body, ft+0x44)
    ids = struct.unpack_from('>6h', body, x44); x124, sx, sy, sh = struct.unpack_from('>4f', body, x44+0xC)
    print(char, 'scale', scale, 'ecb joints', ids, 'x124', x124, 'snap', sx, sy, sh)
    mb, mr = f.archive(model)
    root = next(off for name, off in mr.items() if name.endswith('_joint') and 'matanim' not in name and 'shapeanim' not in name)
    js = []; joints(mb, root, -1, js)
    print(' joints', len(js), 'root', [n for n in mr if n.endswith('_joint')][:3])
    actions = {}
    for i in range(700):
        at = table+i*0x18
        if at+0x18 > len(body): break
        np = f.u32(body, at)
        if np and np < len(body):
            name = body[np:].split(b'\0',1)[0].decode('ascii','replace')
            actions.setdefault(name.split('ACTION_')[-1].replace('_figatree',''), (f.u32(body, at+4), f.u32(body, at+8)))
    for mv in moves:
        off,size = actions[mv]
        anim = aj[off:off+size]
        ab, ar = f.archive(anim)
        tree = next(o for n,o in ar.items() if n.endswith('figatree'))
        frames = int(f.f32(ab, tree+8)); nodes, tracks = f.u32(ab, tree+0xC), f.u32(ab, tree+0x10)
        # per joint track counts
        counts=[]; i=0
        while ab[nodes+i]!=0xFF and i<len(js)+2: counts.append(ab[nodes+i]); i+=1
        tr={}; t=0
        for j,c in enumerate(counts):
            for k in range(c):
                at=tracks+(t)*0xC; length,=struct.unpack_from('>H',ab,at)
                kind,vfmt,tfmt=ab[at+4],ab[at+5],ab[at+6]; data=f.u32(ab,at+8)
                tr[(j,kind)]=f.curve(ab[data:data+length],vfmt,tfmt,frames)
                t+=1
        print(mv,'frames',frames,'joints w/ tracks',len(counts))
        for fr in range(0, frames+1):
            world=[None]*len(js)
            for j,(p,r,s,tv) in enumerate(js):
                def g(kind,d):
                    c=tr.get((j,kind)); return c[min(fr,len(c)-1)] if c else d
                rr=(g(1,r[0]),g(2,r[1]),g(3,r[2])); tt=[g(5,tv[0]),g(6,tv[1]),g(7,tv[2])]; ss=(g(8,s[0]),g(9,s[1]),g(10,s[2]))
                if j==1: tt[0]=tv[0]; tt[2]=tv[2]   # TransN: root motion
                if j==0: ss=(scale,scale,scale); rr=(0,0,0); tt=[0,0,0]
                m=local(tt,rr,ss)
                world[j]= m if p<0 else mul(world[p],m)
            xs=[world[i][2][3] for i in ids]; ys=[world[i][1][3] for i in ids]   # forward is +Z, up is +Y
            l,r_,b,t2=min(xs),max(xs),min(ys),max(ys)
            size=abs(r_-l)
            if size<10: r_=.5*size; l=-r_
            size=abs(t2-b)
            if size<10: mid=.5*(t2+b); half=.5*size; t2=mid+half; b=mid-half
            r_=max(r_,2.); l=min(l,-2.); b=max(b,0.)
            print(f'  f{fr:2d} bottom {b:6.2f} top {t2:6.2f} left {l:6.2f} right {r_:6.2f} side_y {x124*1+.5*(b+t2):6.2f}')
if __name__=='__main__':
    run(sys.argv[1], sys.argv[2], sys.argv[3:])
