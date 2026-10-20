"""Print the move data behind the Edgeguard recoveries from a vanilla Melee ISO.

For each move: its root motion (how far the TransN bone carries the fighter,
which the game turns into velocity every frame, scaled by the model scale)
and the frames its subaction script sets command variables (the windows the
move code checks, like a mash window or "may grab the ledge from here").
Moves driven by physics instead show no root motion.

The key decoder is a port of sysdolphin fobj.c (HSD_FObjInterpretAnim); note
the key data is little-endian. See notes/EDGEGUARD-NEW-CHARACTERS.md.

Usage: python -I scripts/fighter_move_data.py [path/to/melee.iso] [Pl?? Move ...]
       With no moves, prints the table the notes use.
"""
import struct
import sys

MOVES = [
    ('PlMs', ['SpecialAirHi']),  # reference: Marth's AI uses 60 to its grab point
    ('PlCa', ['SpecialAirHi']),  # reference: Falcon's AI uses 48
    ('PlYs', ['JumpAerialF', 'SpecialAirHi']),
    ('PlPe', ['JumpAerialF', 'SpecialAirHiStart']),
    ('PlLg', ['JumpAerialF', 'SpecialAirHi', 'SpecialAirLw']),
    ('PlDr', ['JumpAerialF', 'SpecialAirHi', 'SpecialAirLw']),
    ('PlPp', ['JumpAerialF', 'SpecialAirS1', 'SpecialAirS2', 'SpecialAirHiStart']),
    ('PlPr', ['JumpAerialF1', 'SpecialAirS']),
    ('PlSs', ['JumpAerialF', 'SpecialAirHi', 'AirCatch']),
    ('PlPk', ['SpecialAirHiStart', 'SpecialAirHiEnd']),
]
COMMON = [(0x40, 'jump_v'), (0x50, 'air_jump_v_mult'), (0x58, 'max_jumps'), (0x5C, 'gravity'),
          (0x60, 'terminal_vel'), (0x6C, 'air_drift_max'), (0x8C, 'model_scale')]
CON, LIN, SPL0, SPL, SLP, KEY = 1, 2, 3, 4, 5, 6
TRAY, TRAZ = 6, 7
CMD_LENGTHS = {0x14: 8, 0x1C: 8, 0x28: 20, 0x2C: 20, 0x44: 12, 0x7C: 8, 0x88: 12, 0x98: 28,
               0x9C: 16, 0xD8: 12, 0xDC: 12, 0xE0: 8, 0xE8: 16}


def read_file(iso, filename):
    iso.seek(0x424)
    offset, size = struct.unpack('>II', iso.read(8))
    iso.seek(offset)
    fst = iso.read(size)
    count = struct.unpack_from('>I', fst, 8)[0]
    names = fst[12 * count:]
    for i in range(1, count):
        name, file_offset, file_size = struct.unpack_from('>3I', fst, 12 * i)
        if names[name & 0xFFFFFF:].split(b'\0', 1)[0].decode() == filename:
            iso.seek(file_offset)
            return iso.read(file_size)
    raise SystemExit(filename + ' missing from ISO')


def archive(data):
    _, data_size, relocs, root_count, refs = struct.unpack('>5I', data[:20])
    body = data[0x20:0x20 + data_size]
    roots_at = 0x20 + data_size + relocs * 4
    strings_at = roots_at + (root_count + refs) * 8
    roots = {}
    for i in range(root_count):
        off, name = struct.unpack_from('>2I', data, roots_at + i * 8)
        roots[data[strings_at + name:].split(b'\0', 1)[0].decode()] = off
    return body, roots


def u32(b, o):
    return struct.unpack_from('>I', b, o)[0]


def f32(b, o):
    return struct.unpack_from('>f', b, o)[0]


class Keys:
    def __init__(self, data, vfmt, tfmt):
        self.d, self.p, self.vfmt, self.tfmt = data, 0, vfmt, tfmt

    def byte(self):
        self.p += 1
        return self.d[self.p - 1]

    def packed(self):
        value = shift = 0
        while True:
            b = self.byte()
            value |= (b & 0x7F) << shift
            shift += 7
            if not b & 0x80:
                return value

    def value(self, fmt):
        kind, frac = fmt & 0xE0, fmt & 0x1F
        if fmt == 0:
            self.p += 4
            return struct.unpack_from('<f', self.d, self.p - 4)[0]
        code, n = {0x20: ('<h', 2), 0x40: ('<H', 2), 0x60: ('<b', 1), 0x80: ('<B', 1)}[kind]
        self.p += n
        return struct.unpack_from(code, self.d, self.p - n)[0] / float(1 << frac)


def curve(data, vfmt, tfmt, frames):
    """Values at frames 0..frames, following sysdolphin fobj.c."""
    k = Keys(data, vfmt, tfmt)
    s = dict(p0=0.0, p1=0.0, d0=0.0, d1=0.0, fterm=0.0, time=0.0, op=0, op_intrp=0, pack=0, flags=0)

    def launch():
        if s['flags'] & 0x40:
            s['op_intrp'] = s['op']
            s['flags'] = (s['flags'] & ~0x40) | 0x80
            s['p0'] = s['p1']

    def load(state):
        if k.p >= len(data):
            return 6
        s['op_intrp'] = s['op']
        if not s['pack']:
            head = k.byte()
            s['op'], pack, shift, d = head & 0xF, ((head >> 4) & 7) + 1, 3, head
            while d & 0x80:
                d = k.byte()
                pack += (d & 0x7F) << shift
                shift += 7
            s['pack'] = pack
        s['pack'] -= 1
        op, nxt = s['op'], 3 if state == 1 else 4
        if op in (CON, LIN):
            s['p0'], s['p1'] = s['p1'], k.value(vfmt)
            if s['op_intrp'] != SLP:
                s['d0'], s['d1'] = s['d1'], 0.0
        elif op == SPL0:
            s['p0'], s['d0'], s['p1'], s['d1'] = s['p1'], s['d1'], k.value(vfmt), 0.0
        elif op == SPL:
            s['p0'], s['p1'] = s['p1'], k.value(vfmt)
            s['d0'], s['d1'] = s['d1'], k.value(tfmt)
        elif op == SLP:
            s['d0'], s['d1'] = s['d1'], k.value(tfmt)
            return state
        elif op == KEY:
            launch()
            s['p1'] = k.value(vfmt)
            s['flags'] |= 0x40
        else:
            return 0
        return nxt

    def output():
        op, t, f = s['op_intrp'], s['time'], s['fterm']
        if op == KEY:
            return s['p0']
        if op == CON:
            return s['p1'] if t >= f else s['p0']
        if op == LIN:
            if s['flags'] & 0x20:
                s['flags'] &= ~0x20
                s['d0'] = (s['p1'] - s['p0']) / f if f else 0.0
            return s['d0'] * t + s['p0']
        if not f:
            return s['p1']
        u = t / f
        return ((2 * u ** 3 - 3 * u ** 2 + 1) * s['p0'] + (-2 * u ** 3 + 3 * u ** 2) * s['p1']
                + ((u ** 3 - 2 * u ** 2 + u) * s['d0'] + (u ** 3 - u ** 2) * s['d1']) * f)

    state, values, last = 1, [], 0.0
    for frame in range(frames + 1):
        s['time'] += 1.0 if frame else 0.0
        spent = 0.0
        while True:
            if state == 6:
                s['time'] += spent
                launch()
                last = output()
                break
            if state in (1, 2):
                state = load(state)
            elif state == 3:
                if s['flags'] & 0x80:
                    last = output()
                if k.p >= len(data):
                    state = 6
                    continue
                s['fterm'] = k.packed()
                s['flags'] |= 0x20
                state = 2
            elif state == 4:
                if s['fterm'] <= s['time']:
                    spent = s['fterm']
                    s['time'] -= s['fterm']
                    state = 3
                    continue
                last = output()
                state = 5
                break
            elif state == 5:
                state = 4
            else:
                break
        values.append(last)
    return values


def root_motion(anim, scale):
    body, roots = archive(anim)
    tree = next(off for name, off in roots.items() if name.endswith('figatree'))
    frames = int(f32(body, tree + 8))
    nodes, tracks = u32(body, tree + 0x0C), u32(body, tree + 0x10)
    first = body[nodes]  # TransN is the second joint; skip TopN's tracks
    out = {}
    for t in range(first, first + body[nodes + 1]):
        at = tracks + t * 0xC
        length, = struct.unpack_from('>H', body, at)
        kind, vfmt, tfmt = body[at + 4], body[at + 5], body[at + 6]
        data_at = u32(body, at + 8)
        out[kind] = [v * scale for v in curve(body[data_at:data_at + length], vfmt, tfmt, frames)]
    return frames, out


def script_vars(body, script):
    """Frames at which the subaction script sets cmd_vars, following gotos."""
    frame, p, stack, sets = 0, script, [], []
    for _ in range(400):
        word = u32(body, p)
        op = (word >> 24) & 0xFC
        if op == 0x1C:
            p = u32(body, p + 4)
            continue
        if op == 0x14:
            stack.append(p + 8)
            p = u32(body, p + 4)
            continue
        if op in (0x00, 0x18):
            if not stack:
                break
            p = stack.pop()
            continue
        if op == 0x04:
            frame += word & 0x3FFFFFF
        elif op == 0x08:
            frame = word & 0x3FFFFFF
        elif op == 0x4C:
            sets.append(f'f{frame} var{(word >> 24) & 3}={word & 0xFFFFFF}')
        p += CMD_LENGTHS.get(op, 4)
    return sets


def report(iso, character, moves):
    pl, aj = read_file(iso, character + '.dat'), read_file(iso, character + 'AJ.dat')
    body, roots = archive(pl)
    ft = next(off for name, off in roots.items() if name.startswith('ftData'))
    common, table = u32(body, ft), u32(body, ft + 0x0C)
    scale = f32(body, common + 0x8C)
    attrs = ', '.join(f'{name} {u32(body, common + off) if name == "max_jumps" else round(f32(body, common + off), 3)}'
                      for off, name in COMMON)
    print(f'{character}: {attrs}')
    actions = {}
    for i in range(700):
        at = table + i * 0x18
        if at + 0x18 > len(body):
            break
        name_ptr = u32(body, at)
        if name_ptr and name_ptr < len(body):
            name = body[name_ptr:].split(b'\0', 1)[0].decode('ascii', 'replace')
            short = name.split('ACTION_')[-1].replace('_figatree', '')
            actions.setdefault(short, (u32(body, at + 4), u32(body, at + 8), u32(body, at + 0x0C)))
    for move in moves:
        if move not in actions:
            print(f'  {move}: not found')
            continue
        off, size, script = actions[move]
        frames, tracks = root_motion(aj[off:off + size], scale)
        ys, zs = tracks.get(TRAY, [0.0]), tracks.get(TRAZ, [0.0])
        peak = max(range(len(ys)), key=lambda f: ys[f])
        rise = ys[peak] - ys[0]
        motion = (f'rises {rise:.1f} by frame {peak}, {zs[min(peak, len(zs) - 1)] - zs[0]:.1f} forward'
                  if abs(rise) > 0.05 else 'no root motion (physics)')
        print(f'  {move} ({frames} frames): {motion}; cmd vars: {", ".join(script_vars(body, script)) or "none"}')


def main():
    args = sys.argv[1:]
    path = args.pop(0) if args and args[0].lower().endswith('.iso') else 'melee.iso'
    with open(path, 'rb') as iso:
        if args:
            report(iso, args[0], args[1:])
        else:
            for character, moves in MOVES:
                report(iso, character, moves)


if __name__ == '__main__':
    main()
