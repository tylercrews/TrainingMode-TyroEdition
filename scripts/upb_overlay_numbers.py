"""Print, and with --write regenerate, the Up-B Overlay Gecko codes' shapes
from a vanilla Melee ISO.

Each code draws the targets the Up-B Sweetspot event's line guides draw
(src/upbsweetspot.c), fixed for its character instead of read live:

- Target: where the fighter is when the ledge sits in the middle of its
  ledge-grab box. Out from the ledge by half of (ECB front + ledge snap x),
  down by ledge snap y (FtCollDesc 0x10 and 0x14). The event reads the ECB
  live on the angel platform; here it is ECB_FRONT. A white X.
- Aimed up-Bs (Fox, Falco, Sheik, Zelda, Mewtwo, Pikachu, Pichu): a ring around
  the target at the travel distance, from the special attributes the event's
  AimedDistance reads. It's the offstage half, except for the UNDER_STAGE
  characters, who can up-B from under the stage: theirs carries on under it
  to 2/3 of a circle, and gets a white tick on the ring level with the target,
  where holding straight toward the stage sweetspots. The teleports and Quick
  Attack go farther the more you tilt the stick, so they also get a dim ring
  at the least tilt that still aims.
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
ARC_STEPS = 40      # ARC_STEPS in the event
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

# Aimed up-Bs. Fox and Falco: travel speed 0x74 for 0x68 frames, slowing by
# 0x78 a frame from frame 0x70. The rest: (frames, speed per tilt, base
# speed, least tilt), from TravelFrames, TravelSpeed and AimedThreshold.
SPACIES = ('fox', 'falco')
# 2/3 of a ring (on under the stage, stopping 30 degrees short of level so it
# doesn't stick up into it) and the level tick, as the event gives Fox and
# Falco; the README's to-do list tracks the other codes.
UNDER_STAGE = ('fox',)
UNDER_STAGE_SWEEP, UNDER_STAGE_STEPS = 240, 60   # as in the event
LEVEL_TICK = 6      # LEVEL_TICK in the event: half the tick's length
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


def numbers(slug, dat, ecb):
    """The character's target and guide sizes."""
    attr_f, attr_i, snap_x, snap_y = fighter(dat)
    result = {'target': ((ecb + snap_x) / 2, -snap_y), 'snap': (snap_x, snap_y)}
    if slug in SPACIES:
        result['rings'] = [spacie_distance(attr_f)]
    elif slug in TRAVEL:
        frames, per_tilt, base, least = TRAVEL[slug]
        speed = lambda tilt: f32(attr_f(per_tilt) * tilt + attr_f(base))
        result['rings'] = [f32(attr_i(frames) * speed(1)), f32(attr_i(frames) * speed(attr_f(least)))]
    else:
        rise, forward, end_rise, end_forward, tilt = PATHS[slug]
        result['path'] = (rise, forward, end_rise, end_forward)
        result['tilt'] = attr_f(tilt) if tilt else 0.0
    return result


def shapes(slug, n):
    """(GX shape, width, color, points, comment), drawn in order: dim first,
    then blue, then the X."""
    tx, ty = n['target']
    drawn = []
    for i, radius in enumerate(n.get('rings', ())):
        points = []
        sweep, steps = (UNDER_STAGE_SWEEP, UNDER_STAGE_STEPS) if slug in UNDER_STAGE else (180, ARC_STEPS)
        for step in range(steps + 1):
            angle = math.radians(90 - sweep * step / steps)
            points.append((tx + math.cos(angle) * radius, ty + math.sin(angle) * radius))
        what = 'ring at the full travel' if i == 0 else 'dim ring at the least tilt'
        drawn.append(('GX_LINESTRIP', ARC_WIDTH, BLUE if i == 0 else DIM_BLUE, points, f'{what}, {radius:.2f}'))
        if i == 0 and slug in UNDER_STAGE:
            drawn.append(('GX_LINES', LINE_WIDTH, WHITE,
                          [(tx + radius - LEVEL_TICK, ty), (tx + radius + LEVEL_TICK, ty)],
                          'tick: hold straight toward the stage from here'))
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


def asm(character, n):
    slug, name, kind, _, hook, original, ecb = character
    tx, ty = n['target']
    lines = [
        f'    # To be inserted at {hook:08x}',
        f'    # {name} Up-B Overlay for vanilla NTSC 1.02 Melee / Slippi.',
        f'    # While {name} is in the match on a legal stage, draws the Up-B',
        '    # Sweetspot event\'s targets at both ledges. Written by',
        '    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared',
        '    # in up-b-overlay.inc.',
        '',
        f'    .set FIGHTER_KIND, {kind}',
        '',
        '    # Points are (out from the right ledge, up from it); the left ledge',
        '    # mirrors them.',
        '    .macro character_data',
        f'    # Target: the ledge mid-grab-box, {tx:.2f} out (ECB front {ecb:.2f} + snap',
        f'    # {n["snap"][0]:g}, halved) and {-ty:g} down.',
    ]
    for shape, width, color, points, comment in shapes(slug, n):
        lines.append(f'    shape {shape}, {width}, {len(points)}, 0x{color:08X}    # {comment}')
        lines += [f'    point {x:.4f}, {y:.4f}' for x, y in points]
    lines += [
        '    .long 0                          # end',
        '    .endm',
        '',
        '    .macro original_instruction',
        f'    {original}',
        '    .endm',
        '',
        '    .include "up-b-overlay.inc"',
    ]
    return '\n'.join(lines) + '\n'


def asm_path(character):
    return FOLDER / f'{character[1]} Up-B Overlay.asm'


def main():
    args = [arg for arg in sys.argv[1:] if arg != '--write']
    path = args[0] if args else ROOT / 'melee.iso'
    with open(path, 'rb') as iso:
        for character in CHARACTERS:
            slug, name, _, dat, hook, _, ecb = character
            n = numbers(slug, read_file(iso, dat), ecb)
            tx, ty = n['target']
            guide = (', '.join(f'{r:.2f}' for r in n['rings']) + ' rings') if 'rings' in n else (
                f"path {n['path']}" + (f", tilt {n['tilt']:g}" if n['tilt'] else ''))
            print(f'{name}: hook {hook:08X}, target {tx:.2f} out, {-ty:g} down; {guide}')
            if '--write' in sys.argv:
                asm_path(character).write_text(asm(character, n), newline='\n')
    if '--write' in sys.argv:
        print(f'wrote {len(CHARACTERS)} files in {FOLDER.relative_to(ROOT)}')


if __name__ == '__main__':
    main()
