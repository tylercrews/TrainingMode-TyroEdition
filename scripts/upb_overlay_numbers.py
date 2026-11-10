"""Print, and with --write regenerate, the Up-B Overlay Gecko codes' shapes
from a vanilla Melee ISO.

Each code draws the targets the Up-B Sweetspot event's line guides draw
(src/upbsweetspot.c), fixed for its character instead of read live. The aimed
up-Bs also get a B0XX code, which draws what the event's B0XX Angles setting
does:

- Target: where the fighter is when the ledge sits in the middle of its
  ledge-grab box. Out from the ledge by half of (ECB front + ledge snap x),
  down by ledge snap y (FtCollDesc 0x10 and 0x14). The event reads the ECB
  live on the angel platform; here it is ECB_FRONT. A white X.
- Aimed up-Bs (Fox, Falco, Sheik, Zelda, Mewtwo, Pikachu, Pichu): a ring around
  the target at the travel distance, from the special attributes the event's
  AimedDistance reads. It's the offstage half, except for the UNDER_STAGE
  characters, who can up-B from under the stage: theirs carries on under it
  to 2/3 of a circle. The ring leaves out the angles the stick can't aim, a
  gap either side of straight across, up and down (see STICK_UNITS), with a
  white tick at each straight one and a bit of ring through it. The teleports
  and Quick Attack go farther the more you tilt the stick, so they also get a
  dim edge at the least tilt that aims at each angle, and their white marks
  run between the two, with a bit of each through their ends.
- B0XX codes, for the same seven: the white X, and a dot where each of the
  B0XX's up-B angles (B0XX_PLAIN, B0XX_MOD_B, B0XX_MOD) sweetspots.
- Path up-Bs (Mario, Dr. Mario, Luigi, Marth, Roy, Falcon, Ganondorf, Peach,
  Kirby, Samus): a blue dot where to up-B for the fastest grab, and a line to
  where the slower grab, falling back at the end, starts. Mario, Dr. Mario,
  Marth and Roy also get a dim one for holding the stick toward the stage,
  turned by their tilt attribute. Root motion is UpB_Info in the event.

Points are stored for the right ledge as (out from the ledge, up from it); the
code mirrors them for the left. They don't depend on the stage, so one table
covers all six legal stages.

Usage: python -I scripts/upb_overlay_numbers.py [path/to/melee.iso] [--write]
       --write rewrites slippi-gecko-codes/up-b-overlay/*.asm.
"""
from collections import namedtuple
import importlib.util
import math
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT / 'slippi-gecko-codes/up-b-overlay'
_spec = importlib.util.spec_from_file_location('dj_platforms', ROOT / 'scripts/sweetspot_dj_platforms.py')
_dj = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_dj)
f32, read_file, roots = _dj.f32, _dj.read_file, _dj.roots

BLUE, DIM_BLUE, WHITE = 0x30C8FFFF, 0x1C6C94FF, 0xFFFFFFFF
# Line widths and point size in sixths of a pixel, as the event draws them.
ARC_WIDTH, LINE_WIDTH, DOT_SIZE = 10, 12, 42
ARC_STEP = 4        # ARC_STEP in the event: degrees of ring per line
B0XX_DOT_SIZE = 30  # B0XX_DOT_SIZE in the event
TARGET_SIZE = 2.5   # half the X's width

# Slug, name, internal kind (FTKIND), DAT, hook, the instruction it replaces,
# ECB front. Every hook is an unconditional instruction in StartMelee that
# isn't a branch, so all 17 codes can run together, alongside Slippi's own
# StartMelee hooks (8016E748, 8016E74C, 8016E904) and the other codes in
# slippi-gecko-codes (8016E8C0, 8016E8C8, 8016E8E0, 8016E8E8).
#
# ECB front: how far the ECB reaches toward the ledge standing on the
# platform. RebirthWait plays Wait1; mpColl_LoadECB_JObj spans six joints,
# centers the ECB when it's narrower than 10, and keeps each side at least 2
# out. Averaged over Wait1's loop, with the skeleton posed from the ISO's
# animation (see notes/UPB-SWEETSPOT-RESEARCH.md).
CHARACTERS = [
    ('fox', 'Fox', 1, 'PlFx.dat', 0x8016E898, 'lis r3, -32697', 2.00),
    ('falco', 'Falco', 22, 'PlFc.dat', 0x8016E89C, 'addi r3, r3, -18784', 2.30),
    ('marth', 'Marth', 18, 'PlMs.dat', 0x8016E8A0, 'lwz r12, 9472(r3)', 2.00),
    ('sheik', 'Sheik', 7, 'PlSk.dat', 0x8016E8A4, 'cmplwi r12, 0', 3.07),
    ('falcon', 'Captain Falcon', 2, 'PlCa.dat', 0x8016E8CC, 'cmplwi r12, 0', 4.07),
    ('peach', 'Peach', 9, 'PlPe.dat', 0x8016E908, 'rlwinm. r0, r0, 29, 31, 31', 2.00),
    ('pikachu', 'Pikachu', 12, 'PlPk.dat', 0x8016E91C, 'lwz r0, 36(r1)', 2.00),
    ('samus', 'Samus', 13, 'PlSs.dat', 0x8016E920, 'lwz r31, 28(r1)', 2.03),
    ('luigi', 'Luigi', 17, 'PlLg.dat', 0x8016E924, 'lwz r30, 24(r1)', 2.36),
    ('mario', 'Mario', 0, 'PlMr.dat', 0x8016E928, 'addi r1, r1, 32', 2.15),
    ('drmario', 'Dr. Mario', 21, 'PlDr.dat', 0x8016E92C, 'mtlr r0', 2.14),
    ('ganondorf', 'Ganondorf', 25, 'PlGn.dat', 0x8016E80C, 'cmpwi r3, 0', 5.67),
    ('mewtwo', 'Mewtwo', 16, 'PlMt.dat', 0x8016E804, 'addi r30, r3, -18784', 3.14),
    ('zelda', 'Zelda', 19, 'PlZd.dat', 0x8016E800, 'lis r3, -32697', 2.00),
    ('roy', 'Roy', 26, 'PlFe.dat', 0x8016E7F8, 'li r4, 0', 2.21),
    ('pichu', 'Pichu', 23, 'PlPc.dat', 0x8016E7F4, 'lhz r3, 14(r31)', 2.00),
    ('kirby', 'Kirby', 4, 'PlKb.dat', 0x8016E7EC, 'li r3, 64', 2.00),
]
# The B0XX codes' hooks: more of StartMelee's unconditional instructions that
# aren't branches, with no float register live across them. They run before
# it loads the stage, like the six earliest above.
B0XX_HOOKS = {
    'fox': (0x8016E7D8, 'li r3, 31'),
    'falco': (0x8016E7D0, 'li r3, 0'),
    'sheik': (0x8016E7BC, 'mr r3, r31'),
    'zelda': (0x8016E7B0, 'li r3, 70'),
    'mewtwo': (0x8016E76C, 'li r3, 1'),
    'pikachu': (0x8016E764, 'addi r3, r3, 22356'),
    'pichu': (0x8016E760, 'lis r3, -32734'),
}
# Every code: key names its text file, and b0xx says which kind it is.
Code = namedtuple('Code', 'key slug name kind dat hook original ecb b0xx')
CODES = [Code(c[0], *c, False) for c in CHARACTERS] + [
    Code(f'{slug}-b0xx', slug, name, kind, dat, *B0XX_HOOKS[slug], ecb, True)
    for slug, name, kind, dat, _, _, ecb in CHARACTERS if slug in B0XX_HOOKS]

# Aimed up-Bs. Fox and Falco: travel speed 0x74 for 0x68 frames, slowing by
# 0x78 a frame from frame 0x70. The rest: (frames, speed per tilt, base
# speed, least tilt), from TravelFrames, TravelSpeed and AimedThreshold.
SPACIES = ('fox', 'falco')
# 2/3 of a ring, on under the stage, stopping 30 degrees short of level so it
# doesn't stick up into it, as the event gives Fox and Falco.
UNDER_STAGE = ('fox', 'falco')
UNDER_STAGE_SWEEP = 240   # as in the event
LEVEL_TICK = 6      # LEVEL_TICK in the event: half a tick's length
STRAIGHT_ARC = 2    # STRAIGHT_ARC in the event: degrees of ring either side of a straight mark
# The stick the aimed up-Bs read (input.lstick): 80ths of full tilt inside a
# circle of radius 80 (gmmain.c's pad clamp), with an axis at or under its
# deadzone (PlCo.dat ftLoadCommonData 0x0 across, 0x4 up, both 0.28) read as
# 0 by Fighter_procInput. A tilted axis reads at least 23/80, so the shallowest
# angle off straight is atan(23 / 76), 16.84 degrees: 76 is the most the other
# axis can read with 23 inside the circle.
STICK_UNITS = 80
# The B0XX's up-B sticks for one quadrant, in 80ths (toward the stage, up), as
# HayBox's Melee mode gives them, "up to date with B0XX V3 specifications"
# (src/modes/Melee20Button.cpp). Mod X and Mod Y on a diagonal, alone and with
# C-Down, C-Left, C-Up or C-Right, step from 23 degrees to 67, 4.4 apart, and
# holding B too gives the same angles longer. Mod Y with B across is full.
B0XX_PLAIN = ((80, 0), (0, 80), (56, 56))
B0XX_MOD_B = ((53, 0), (0, 43), (73, 31), (70, 36), (68, 42), (59, 43), (51, 43),    # Mod X + B
              (80, 0), (0, 59), (31, 73), (36, 70), (42, 68), (47, 64), (47, 57))    # Mod Y + B
B0XX_MOD = ((53, 0), (0, 43), (59, 25), (56, 29), (63, 39), (56, 41), (49, 42),      # Mod X
            (27, 0), (0, 59), (25, 59), (29, 56), (39, 63), (41, 56), (51, 61))      # Mod Y
# Spots closer than this are the same dot: holding B moves Fox's and Falco's
# under half a degree, since their travel doesn't depend on tilt.
B0XX_SAME_SPOT = 1.0
TRAVEL = {
    'sheik': (0x38, 0x44, 0x48, 0x40),
    'zelda': (0x48, 0x54, 0x58, 0x50),
    'mewtwo': (0x50, 0x5C, 0x60, 0x58),
    'pikachu': (0x60, 0x90, 0x94, 0x8C),
    'pichu': (0x60, 0x90, 0x94, 0x8C),
}
# Path up-Bs: rise and forward at the peak, then at the end of the animation,
# and the special attribute holding the stick tilt in degrees (0 for none).
# Must match UpB_Info in src/upbsweetspot.c.
PATHS = {
    'mario': (40.4, 17.6, 33.3, 20.9, 0x28),
    'drmario': (40.4, 17.6, 33.3, 20.9, 0x28),
    'luigi': (45.9, 0.0, 37.9, 0.0, 0),
    'marth': (47.3, 10.2, 34.5, 11.5, 0x38),
    'roy': (44.5, 12.3, 32.4, 13.0, 0x38),
    'falcon': (37.6, 11.9, 13.1, 20.2, 0),
    'ganondorf': (41.9, 13.2, 14.6, 22.5, 0),
    'peach': (36.8, 3.4, 34.4, 4.1, 0),
    'kirby': (52.2, 0.0, 0.3, 0.0, 0),
    'samus': (47.3, 0.0, 44.0, 0.0, 0),
}


def fighter(dat):
    """Special attribute readers and the ledge snap x and y."""
    body, found = roots(dat)
    ft = next(at for name, at in found.items() if name.startswith('ftData'))
    special, coll = (struct.unpack_from('>I', body, ft + at)[0] for at in (0x4, 0x44))
    attr_f = lambda at: struct.unpack_from('>f', body, special + at)[0]
    attr_i = lambda at: struct.unpack_from('>i', body, special + at)[0]
    snap_x, snap_y = struct.unpack_from('>2f', body, coll + 0x10)
    return attr_f, attr_i, snap_x, snap_y


def spacie_distance(attr_f):
    speed, slow, distance = f32(attr_f(0x74)), f32(attr_f(0x78)), 0.0
    frames, slow_from = int(attr_f(0x68)), int(attr_f(0x70))
    for frame in range(1, frames + 1):
        if frame >= slow_from:
            speed = f32(speed - slow)
        distance = f32(distance + speed)
    return distance


def deadzones(iso):
    """The stick deadzones across and up, from PlCo.dat."""
    body, found = roots(read_file(iso, 'PlCo.dat'))
    data = struct.unpack_from('>I', body, found['ftLoadCommonData'])[0]
    return struct.unpack_from('>2f', body, data)


def least_axis(deadzone):
    """The least an axis reads once it isn't 0, in 80ths: the step past the deadzone."""
    return int(f32(f32(deadzone) * STICK_UNITS + 1))


def gap(least):
    """Degrees from straight to the shallowest angle the stick aims, when the
    axis across straight reads `least` and the other as much as the circle allows."""
    return math.degrees(math.atan2(least, int(math.sqrt(STICK_UNITS ** 2 - least ** 2))))


def stick(across, up):
    """The stick from its deadzones: the least each axis reads (across, up),
    and the gaps beside straight across and straight up, in degrees."""
    least = (least_axis(across), least_axis(up))
    return {'least': least, 'gaps': (gap(least[1]), gap(least[0]))}


def ring_arcs(sweep, gaps):
    """Each stretch of a ring the stick can aim at, as (from, to) ring angles:
    90 is straight above the target, 0 straight out, -90 below. Each quarter
    between straight directions loses a gap at both ends."""
    level, upright = gaps
    arcs = []
    for top in (90, 0, -90):
        if top <= 90 - sweep:
            break
        start = top - (upright if top else level)
        end = max(top - 90 + (level if top else upright), 90 - sweep)
        if start > end:
            arcs.append((start, end))
    return arcs


def straights(sweep):
    """The straight directions on a ring, as ring angles."""
    return [angle for angle in (90, 0, -90, -180) if angle >= 90 - sweep]


def least_tilt(angle, least, threshold):
    """The least stick that aims at ring angle `angle`: past the up-B's threshold,
    with both axes reading at least their least."""
    a = math.radians(angle)
    tilt = max(threshold, least[1] / STICK_UNITS / abs(math.sin(a)), least[0] / STICK_UNITS / abs(math.cos(a)))
    return min(tilt, 1.0)


def arc_points(target, radius, start, end, steps):
    """Points along a ring stretch, where radius(angle) is the distance out."""
    points = []
    for step in range(steps + 1):
        angle = start - (start - end) * step / steps
        a, r = math.radians(angle), radius(angle)
        points.append((target[0] + math.cos(a) * r, target[1] + math.sin(a) * r))
    return points


def b0xx_spots(distance, threshold, spacie, least, sweep):
    """[(out, up, color)] from the target: where up-B with each B0XX stick ends
    at the target, by the move's own rule, within the ring's sweep. Plain sticks
    are white, Mod X and Mod Y with B blue, and without B dim, each left out
    where an earlier one already is."""
    spots = []
    for sticks, color in ((B0XX_PLAIN, WHITE), (B0XX_MOD_B, BLUE), (B0XX_MOD, DIM_BLUE)):
        for bx, by in sticks:
            for sx in (1, -1):
                for sy in (1, -1):
                    x, y = sx * bx, sy * by
                    x, y = (0 if abs(x) < least[0] else x), (0 if abs(y) < least[1] else y)
                    length = math.hypot(x, y)
                    tilt = min(length / STICK_UNITS, 1.0)
                    aimed = (abs(x) + abs(y)) / STICK_UNITS >= threshold if spacie else tilt > threshold
                    if not aimed:
                        x, y, length, tilt = 0, 1, 1, 1.0
                    out, up = x / length, -y / length
                    angle = math.degrees(math.atan2(up, out))
                    if angle > 90.5 or angle < 89.5 - sweep:
                        continue
                    d = distance(tilt)
                    spot = (out * d, up * d)
                    if any(math.hypot(spot[0] - s[0], spot[1] - s[1]) < B0XX_SAME_SPOT for s in spots):
                        continue
                    spots.append((*spot, color))
    return spots


def numbers(slug, dat, ecb, stick_numbers):
    """The character's target and guide sizes."""
    attr_f, attr_i, snap_x, snap_y = fighter(dat)
    result = {'target': ((ecb + snap_x) / 2, -snap_y), 'snap': (snap_x, snap_y)}
    if slug in SPACIES:
        full = spacie_distance(attr_f)
        result['rings'] = [full]
        result['distance'] = lambda tilt: full
        result['threshold'] = attr_f(0x64)
        result['stick'] = stick_numbers
    elif slug in TRAVEL:
        frames, per_tilt, base, least = TRAVEL[slug]
        speed = lambda tilt: f32(attr_f(per_tilt) * tilt + attr_f(base))
        result['distance'] = lambda tilt: f32(attr_i(frames) * speed(tilt))
        result['threshold'] = attr_f(least)
        result['rings'] = [result['distance'](1), result['distance'](result['threshold'])]
        result['stick'] = stick_numbers
    else:
        rise, forward, end_rise, end_forward, tilt = PATHS[slug]
        result['path'] = (rise, forward, end_rise, end_forward)
        result['tilt'] = attr_f(tilt) if tilt else 0.0
    return result


def aimed_shapes(slug, n, b0xx):
    """The ring stretches, their least-tilt edges and the straight marks, or
    the B0XX dots."""
    target, distance, threshold = n['target'], n['distance'], n['threshold']
    spacie, least = slug in SPACIES, n['stick']['least']
    sweep = UNDER_STAGE_SWEEP if slug in UNDER_STAGE else 180
    drawn = []
    if b0xx:
        spots = b0xx_spots(distance, threshold, spacie, least, sweep)
        for color, what in ((DIM_BLUE, 'Mod X or Mod Y without B'),
                            (BLUE, 'Mod X or Mod Y, B held or not' if spacie else 'Mod X or Mod Y, with B'),
                            (WHITE, 'no modifier')):
            points = [(target[0] + x, target[1] + y) for x, y, c in spots if c == color]
            if points:
                drawn.append(('GX_POINTS', B0XX_DOT_SIZE, color, points, f'B0XX: {what}'))
        return drawn
    full = distance(1)
    arcs = ring_arcs(sweep, n['stick']['gaps'])
    bit_steps = int(2 * STRAIGHT_ARC / ARC_STEP) + 1   # as the event steps any stretch of ring
    if not spacie:
        for start, end in arcs:
            steps = 2 * (int((start - end) / ARC_STEP) + 1)
            points = arc_points(target, lambda angle: distance(least_tilt(angle, least, threshold)), start, end, steps)
            drawn.append(('GX_LINESTRIP', ARC_WIDTH, DIM_BLUE, points,
                          f'least tilt that aims, {start:.2f} to {end:.2f} degrees'))
        for angle in straights(sweep):
            points = arc_points(target, lambda _: distance(threshold), angle + STRAIGHT_ARC, angle - STRAIGHT_ARC,
                                bit_steps)
            drawn.append(('GX_LINESTRIP', ARC_WIDTH, DIM_BLUE, points, f'least tilt through the {angle} degree mark'))
    for start, end in arcs:
        points = arc_points(target, lambda angle: full, start, end, int((start - end) / ARC_STEP) + 1)
        drawn.append(('GX_LINESTRIP', ARC_WIDTH, BLUE, points,
                      f'ring at the full travel {full:.2f}, {start:.2f} to {end:.2f} degrees'))
    for angle in straights(sweep):
        points = arc_points(target, lambda _: full, angle + STRAIGHT_ARC, angle - STRAIGHT_ARC, bit_steps)
        drawn.append(('GX_LINESTRIP', ARC_WIDTH, BLUE, points, f'ring through the {angle} degree mark'))
    inner, outer = (full - LEVEL_TICK, full + LEVEL_TICK) if spacie else (distance(threshold), full)
    points = []
    for angle in straights(sweep):
        c, s = math.cos(math.radians(angle)), math.sin(math.radians(angle))
        points += [(target[0] + c * inner, target[1] + s * inner), (target[0] + c * outer, target[1] + s * outer)]
    drawn.append(('GX_LINES', LINE_WIDTH, WHITE, points, 'straight across, up and down: hold straight here'))
    return drawn


def shapes(slug, n, b0xx=False):
    """(GX shape, width, color, points, comment), drawn in order: dim first,
    then blue, then white, then the X."""
    tx, ty = n['target']
    drawn = aimed_shapes(slug, n, b0xx) if 'rings' in n else []
    if 'path' in n:
        rise, forward, end_rise, end_forward = n['path']
        tilts = ([(n['tilt'], DIM_BLUE)] if n['tilt'] else []) + [(0.0, BLUE)]
        for tilt, color in tilts:
            c, s = math.cos(math.radians(tilt)), math.sin(math.radians(tilt))
            fast = (tx + forward * c + rise * s, ty - (rise * c - forward * s))
            slow = (tx + end_forward * c + end_rise * s, ty - (end_rise * c - end_forward * s))
            what = f'stick toward the stage ({tilt:g} degrees)' if tilt else 'stick held up'
            drawn.append(('GX_LINES', LINE_WIDTH, color, [fast, slow], f'{what}: fastest grab to slowest'))
            drawn.append(('GX_POINTS', DOT_SIZE, color, [fast], f'{what}: up-B here'))
    s = TARGET_SIZE
    drawn.append(('GX_LINES', LINE_WIDTH, WHITE,
                  [(tx - s, ty - s), (tx + s, ty + s), (tx - s, ty + s), (tx + s, ty - s)], 'the target X'))
    return drawn


def asm(code, n):
    tx, ty = n['target']
    title = f'{code.name} Up-B Overlay' + (' B0XX' if code.b0xx else '')
    what = ("Sweetspot event's B0XX Angles at both ledges. Written by" if code.b0xx else
            "Sweetspot event's targets at both ledges. Written by")
    lines = [
        f'    # To be inserted at {code.hook:08x}',
        f'    # {title} for vanilla NTSC 1.02 Melee / Slippi.',
        f'    # While {code.name} is in the match on a legal stage, draws the Up-B',
        f'    # {what}',
        '    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared',
        '    # in up-b-overlay.inc.',
        '',
        f'    .set FIGHTER_KIND, {code.kind}',
        '',
        '    # Points are (out from the right ledge, up from it); the left ledge',
        '    # mirrors them.',
        '    .macro character_data',
        f'    # Target: the ledge mid-grab-box, {tx:.2f} out (ECB front {code.ecb:.2f} + snap',
        f'    # {n["snap"][0]:g}, halved) and {-ty:g} down.',
    ]
    for shape, width, color, points, comment in shapes(code.slug, n, code.b0xx):
        lines.append(f'    shape {shape}, {width}, {len(points)}, 0x{color:08X}    # {comment}')
        lines += [f'    point {x:.4f}, {y:.4f}' for x, y in points]
    lines += [
        '    .long 0                          # end',
        '    .endm',
        '',
        '    .macro original_instruction',
        f'    {code.original}',
        '    .endm',
        '',
        '    .include "up-b-overlay.inc"',
    ]
    return '\n'.join(lines) + '\n'


def asm_path(code):
    return FOLDER / (f'{code.name} Up-B Overlay' + (' B0XX' if code.b0xx else '') + '.asm')


def code_numbers(iso, code, stick_numbers):
    return numbers(code.slug, read_file(iso, code.dat), code.ecb, stick_numbers)


def main():
    args = [arg for arg in sys.argv[1:] if arg != '--write']
    path = args[0] if args else ROOT / 'melee.iso'
    with open(path, 'rb') as iso:
        stick_numbers = stick(*deadzones(iso))
        print('stick: least {} and {} 80ths across and up; gaps {:.2f} beside level, {:.2f} beside upright'.format(
            *stick_numbers['least'], *stick_numbers['gaps']))
        for code in CODES:
            n = code_numbers(iso, code, stick_numbers)
            tx, ty = n['target']
            if code.b0xx:
                guide = f"{len(b0xx_spots(n['distance'], n['threshold'], code.slug in SPACIES, n['stick']['least'], UNDER_STAGE_SWEEP if code.slug in UNDER_STAGE else 180))} B0XX dots"
            elif 'rings' in n:
                guide = ', '.join(f'{r:.2f}' for r in n['rings']) + ' rings'
            else:
                guide = f"path {n['path']}" + (f", tilt {n['tilt']:g}" if n['tilt'] else '')
            print(f'{code.key}: hook {code.hook:08X}, target {tx:.2f} out, {-ty:g} down; {guide}')
            if '--write' in sys.argv:
                with open(asm_path(code), 'wb') as f:
                    f.write(asm(code, n).encode('utf-8'))
    if '--write' in sys.argv:
        print(f'wrote {len(CODES)} files in {FOLDER.relative_to(ROOT)}')


if __name__ == '__main__':
    main()
