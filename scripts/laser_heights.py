"""Print where Fox's and Falco's lasers spawn, from the ISO's skeletons and animations.

ftFx_SpecialN_FtGetHoldJoint (decomp ftfoxspecialn.c) puts a shot at the
RThumbNb joint's world matrix times (0, 1.2325, 4.2636), with z zeroed, on the
frame the Loop script sets cmd_vars[2]. The shot flies flat (blaster angle 0),
so this point's height is the laser's height the whole way. RThumbNb is common
part 51; PlCo.dat's parts table (ftLoadCommonData + 0x10) maps it to each
model's joint. The skeleton is posed like scripts/fighter_ecb_pose.py: TransN's
X and Z are root motion and zeroed, no blending, the model's scale.

The ECB bottom is mpColl_LoadECB_JObj's (the same six joints and minimums as
fighter_ecb_pose.py), so an airborne fighter lands once TopN + bottom reaches
the floor. src/targets.c's TP_* constants come from this, and
tests/test_targets.py checks them against it.

Usage: python -I scripts/laser_heights.py   (from the repository root, melee.iso there)
"""
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fighter_move_data as f
import fighter_ecb_pose as pose

LASER_OFFSET = (0.0, 1.2325000762939453, 4.263599872589111)
RTHUMBNB = 51
FIGHTERS = {'Fox': ('PlFx', 1), 'Falco': ('PlFc', 22)}


def actions(body, table):
    out = {}
    for i in range(700):
        at = table + i * 0x18
        if at + 0x18 > len(body):
            break
        name_ptr = f.u32(body, at)
        if name_ptr and name_ptr < len(body):
            name = body[name_ptr:].split(b'\0', 1)[0].decode('ascii', 'replace')
            out.setdefault(name.split('ACTION_')[-1].replace('_figatree', ''),
                           (f.u32(body, at + 4), f.u32(body, at + 8), f.u32(body, at + 0xC)))
    return out


def joint_flags(body, at, out):
    """HSD_Joint flags (offset 4), in the same depth-first order as pose.joints."""
    while at:
        out.append(f.u32(body, at + 4))
        if f.u32(body, at + 8):
            joint_flags(body, f.u32(body, at + 8), out)
        at = f.u32(body, at + 0xC)


def fire_frame(body, script):
    """The Loop frame whose script sets cmd_vars[2] (the shot)."""
    for entry in f.script_vars(body, script):
        frame, var = entry.split(' ')
        if var.startswith('var2='):
            return int(frame[1:])
    raise SystemExit('no shot in script')


class Fighter:
    def __init__(self, iso, name):
        prefix, kind = FIGHTERS[name]
        self.pl, self.aj = f.read_file(iso, prefix + '.dat'), f.read_file(iso, prefix + 'AJ.dat')
        pl = self.pl
        model = f.read_file(iso, prefix + 'Nr.dat')
        co_body, co_roots = f.archive(f.read_file(iso, 'PlCo.dat'))
        parts = f.u32(co_body, f.u32(co_body, co_roots['ftLoadCommonData'] + 0x10) + kind * 4)
        self.thumb = co_body[f.u32(co_body, parts + 4) + RTHUMBNB]
        self.body, roots = f.archive(pl)
        ft = next(off for n, off in roots.items() if n.startswith('ftData'))
        common = f.u32(self.body, ft)
        self.scale = f.f32(self.body, common + 0x8C)
        self.attr = {name: f.f32(self.body, common + off) for off, name in (
            (0x40, 'jump_v'), (0x4C, 'hop_v'), (0x50, 'air_jump_mult'), (0x5C, 'gravity'),
            (0x60, 'terminal'), (0x74, 'fastfall'))}
        self.ecb_ids = struct.unpack_from('>6h', self.body, f.u32(self.body, ft + 0x44))
        self.actions = actions(self.body, f.u32(self.body, ft + 0x0C))
        mb, mr = f.archive(model)
        root = next(off for n, off in mr.items()
                    if n.endswith('_joint') and 'matanim' not in n and 'shapeanim' not in n)
        self.joints, self.flags = [], []
        pose.joints(mb, root, -1, self.joints)
        joint_flags(mb, root, self.flags)

    def world(self, move, frame):
        off, size, _ = self.actions[move]
        ab, ar = f.archive(self.aj[off:off + size])
        tree = next(o for n, o in ar.items() if n.endswith('figatree'))
        frames = int(f.f32(ab, tree + 8))
        nodes, tracks = f.u32(ab, tree + 0xC), f.u32(ab, tree + 0x10)
        counts, i = [], 0
        while ab[nodes + i] != 0xFF and i < len(self.joints) + 2:
            counts.append(ab[nodes + i])
            i += 1
        curves, t = {}, 0
        for j, c in enumerate(counts):
            for _ in range(c):
                at = tracks + t * 0xC
                length, = struct.unpack_from('>H', ab, at)
                kind, vfmt, tfmt, data = ab[at + 4], ab[at + 5], ab[at + 6], f.u32(ab, at + 8)
                curves[(j, kind)] = f.curve(ab[data:data + length], vfmt, tfmt, frames)
                t += 1
        frame = min(frame, frames)
        world, scl = [None] * len(self.joints), [None] * len(self.joints)
        for j, (parent, r, s, tv) in enumerate(self.joints):
            def g(kind, default):
                c = curves.get((j, kind))
                return c[min(frame, len(c) - 1)] if c else default
            rr = (g(1, r[0]), g(2, r[1]), g(3, r[2]))
            tt = [g(5, tv[0]), g(6, tv[1]), g(7, tv[2])]
            ss = (g(8, s[0]), g(9, s[1]), g(10, s[2]))
            if j == 1:
                tt[0], tt[2] = tv[0], tv[2]  # TransN: root motion
            if j == 0:
                ss, rr, tt = (self.scale,) * 3, (0, 0, 0), [0, 0, 0]
            # HSD_JObjMakeMatrix: scl accumulates the scales down the tree and
            # HSD_MtxSRT cancels the parent's (local[i][k] *= ps[k] / ps[i]),
            # but a JOBJ_CLASSICAL_SCALE joint copies its parent's scl, and the
            # root has none. Every Fox and Falco joint is classical, so their
            # stretched arm bones (scale X up to 1.2) shear their children.
            ps = scl[parent] if parent >= 0 else None
            if self.flags[j] & 8:
                scl[j] = ps
            else:
                scl[j] = ss if ps is None else tuple(a * b for a, b in zip(ss, ps))
            m = pose.local(tt, rr, ss)
            if ps is not None:
                for i in range(3):
                    for k in range(3):
                        m[i][k] *= ps[k] / ps[i]
            world[j] = m if parent < 0 else pose.mul(world[parent], m)
        return world, frames

    def laser(self, move, frame):
        """(height, forward) of the shot above and ahead of TopN."""
        m, _ = self.world(move, frame)
        m = m[self.thumb]
        x, y, z = LASER_OFFSET
        point = [m[i][0] * x + m[i][1] * y + m[i][2] * z + m[i][3] for i in range(3)]
        return point[1], point[2]

    def ecb_bottom(self, move, frame):
        m, _ = self.world(move, frame)
        ys = [m[i][1][3] for i in self.ecb_ids]
        bottom, top = min(ys), max(ys)
        if top - bottom < 10:
            mid = .5 * (top + bottom)
            bottom = mid - .5 * (top - bottom)
        return max(bottom, 0.)

    def fire(self, loop):
        return fire_frame(self.body, self.actions[loop][2])

    def frames(self, move):
        return self.world(move, 0)[1]

    def constants(self):
        """src/targets.c's Blaster entry. The shot leaves after Start's frames
        and the Loop's fire frame, counting the B press as the first (SmashWiki
        gives Fox's aerial laser a startup of 10), so it's `delay` frames after
        the press; looped shots are a Loop's length apart. The ECB bottom is the
        frame before the shot's, which decides whether the fighter is still in
        the air to shoot."""
        fire = self.fire('SpecialAirNLoop')
        return {
            'air': self.laser('SpecialAirNLoop', fire)[0],
            'ground': self.laser('SpecialNLoop', self.fire('SpecialNLoop'))[0],
            'ecb_bottom': self.ecb_bottom('SpecialAirNLoop', fire - 1),
            'delay': self.frames('SpecialAirNStart') + fire - 1,
            'loop': self.frames('SpecialAirNLoop'),
        }

    def laser_hitbox(self):
        """The laser article's first hitbox command (state 0): its raw size,
        which the game scales by about 1/256 (0x804DC790)."""
        ft = next(off for n, off in f.archive(self.pl)[1].items() if n.startswith('ftData'))
        desc = f.u32(self.body, f.u32(self.body, ft + 0x48))
        script = f.u32(self.body, f.u32(self.body, desc + 0xC) + 0xC)
        assert self.body[script] >> 2 == 0x2C >> 2, 'not a hitbox command'
        return struct.unpack_from('>H', self.body, script + 4)[0]


def rise(v, gravity):
    """Apex rise of a jump that applies gravity before moving from its first frame: a
    double jump (ftCo_JumpAerial_Phys)."""
    height = 0.
    for _ in range(256):
        v -= gravity
        if v <= 0:
            break
        height += v
    return height


def hop(v, gravity, terminal, press=0, frames=96):
    """Height after each airborne frame of a jump off the ground at speed v, with B on
    airborne frame `press` (0 for none). The jump's physics skips gravity on its first
    frame (ftCo_Jump_Phys: mv.co.jump.x4 starts false), so it rises its whole speed. B on
    that frame takes the skip away: the input proc runs between the anim proc that starts
    the jump and the physics, so the aerial laser's physics (ft_80084DB0, gravity first)
    runs instead."""
    y = [0.]
    for frame in range(1, frames):
        if frame > 1 or press == 1:
            v = max(v - gravity, -terminal)
        y.append(y[-1] + v)
    return y


def peak_laser(attr, constants, v):
    """(height, press, from): the highest laser a hop at speed v can shoot above the floor
    it's from, the B press that shoots it (the earliest of any that tie), and TopN's height
    it comes from. B on airborne frame p shoots `delay` frames later, from where the
    fighter was the frame before."""
    best = None
    for press in range(1, 80):
        from_y = hop(v, attr['gravity'], attr['terminal'], press)[press + constants['delay'] - 1]
        if best is None or from_y + constants['air'] > best[0]:
            best = (from_y + constants['air'], press, from_y)
    return best


def main():
    with open('melee.iso', 'rb') as iso:
        for name in FIGHTERS:
            ft = Fighter(iso, name)
            a = ft.attr
            print(f'{name}: thumb joint {ft.thumb}, scale {ft.scale}, ' +
                  ', '.join(f'{k} {v:g}' for k, v in a.items()))
            for loop in ('SpecialNLoop', 'SpecialAirNLoop'):
                fire = ft.fire(loop)
                for frame in range(max(fire - 1, 0), fire + 2):
                    y, z = ft.laser(loop, frame)
                    mark = ' <- shot' if frame == fire else ''
                    print(f'  {loop} f{frame}: laser {y:.4f} up, {z:.4f} ahead, '
                          f'ECB bottom {ft.ecb_bottom(loop, frame):.4f}{mark}')
            c = ft.constants()
            short, full = (max(hop(a[v], a['gravity'], a['terminal'])) for v in ('hop_v', 'jump_v'))
            double = rise(a['jump_v'] * a['air_jump_mult'], a['gravity'])
            print(f'  short hop {short:.4f}, full hop {full:.4f}, double jump {double:.4f}, '
                  f'full hop + double jump {full + double:.4f}')
            for v, name in (('hop_v', 'short hop'), ('jump_v', 'full hop')):
                laser, press, from_y = peak_laser(a, c, a[v])
                print(f'  {name} peak laser {laser:.4f} up: B on airborne frame {press}, from {from_y:.4f}')
            print(f'  targets.c: {{{c["air"]:.4f}f, {c["ground"]:.4f}f, {c["ecb_bottom"]:.4f}f, '
                  f'{c["delay"]}, {c["loop"]}}}; lowest laser {max(c["air"] - c["ecb_bottom"], 0):.4f}, '
                  f'laser hitbox {ft.laser_hitbox()}/256')


if __name__ == '__main__':
    main()
