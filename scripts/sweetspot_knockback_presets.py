"""Print where each Side-B Sweetspot Knockback preset ends hitstun.

Reads the presets from the Knockbacks table in src/sweetspot.c, and Fox's and
Falco's gravity, falling speed and air drift and the common knockback rules
(PlCo.dat ftLoadCommonData: speed 0.03 per unit of knockback at 0x100,
hitstun 0.4 frames per unit at 0x154, decay 0.051 a frame at 0x204) from a
vanilla ISO. Like the game (Fighter_procUpdate), each frame after hitlag:
gravity pulls your own speed down to terminal velocity, the knockback speed
shrinks by the decay along its own angle, and both move you. There is no air
drift in hitstun, which lasts floor(0.4 * knockback) frames.

Every attempt starts 12 out from the ledge and 6 above it. "out" is how far
from the ledge you are when hitstun ends, and "height" how far above (+) or
below (-) it, at the corners of each preset's knockback and angle ranges.

"Fall in" says whether, holding toward the stage with no double jump, your
feet pass 8.5 to 17.5 under the ledge (the ledge-grab box reaching the ledge)
while you're inside the side-B's reach: past its shortest (27.58) and inside a
full one (83.74 for Fox, 77.08 for Falco). Knockback left over after hitstun
keeps pushing you, so this is a guide, not a guarantee.

Usage: python -I scripts/sweetspot_knockback_presets.py [path/to/melee.iso]
"""
import importlib.util
import math
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('platforms', ROOT / 'scripts/sweetspot_dj_platforms.py')
platforms = importlib.util.module_from_spec(spec)
spec.loader.exec_module(platforms)

CHARACTERS = [('PlFx.dat', 'Fox', 83.74), ('PlFc.dat', 'Falco', 77.08)]
SHORTEST_SIDE_B = 27.58
START_OUT, START_HEIGHT = 12.0, 6.0


def presets():
    """(name, knockback min, max, angle min, max) rows of the C table."""
    source = (ROOT / 'src/sweetspot.c').read_text()
    table = source.split('Knockbacks[KB_RANDOM_PRESET] = {', 1)[1].split('};', 1)[0]
    number = r'\s*([\d.]+)\s*'
    rows = re.findall(r'\{' + ','.join([number] * 4) + r'\},\s*//\s*([^:\n]+)', table)
    return [(name.strip(), *map(float, values)) for *values, name in rows]


def common(iso):
    body, found = platforms.roots(platforms.read_file(iso, 'PlCo.dat'))
    data = struct.unpack_from('>I', body, found['ftLoadCommonData'])[0]
    return [struct.unpack_from('>f', body, data + at)[0] for at in (0x100, 0x154, 0x204)]


def fighter(iso, dat):
    body, found = platforms.roots(platforms.read_file(iso, dat))
    ft = next(at for name, at in found.items() if name.startswith('ftData'))
    attributes = struct.unpack_from('>I', body, ft)[0]
    a = lambda at: struct.unpack_from('>f', body, attributes + at)[0]
    return {'gravity': a(0x5C), 'terminal': a(0x60), 'accel': a(0x64) + a(0x68),
            'drift_max': a(0x6C), 'friction': a(0x70)}


def launch(ft, rules, knockback, angle):
    """Hitstun's end (out, height), then whether drifting in finds the window."""
    speed, hitstun_rate, decay = rules
    hitstun = int(hitstun_rate * knockback)
    v = knockback * speed
    kb_x, kb_y = v * math.cos(math.radians(angle)), v * math.sin(math.radians(angle))
    self_x = self_y = 0.0
    out, height = START_OUT, START_HEIGHT
    end, window = None, False
    for frame in range(hitstun + 120):
        if frame >= hitstun:  # Free to act: hold toward the stage.
            self_x = max(self_x - ft['accel'], -ft['drift_max'])
        self_y = max(self_y - ft['gravity'], -ft['terminal'])
        size = math.hypot(kb_x, kb_y)
        if size < decay:
            kb_x = kb_y = 0.0
        else:
            kb_x -= decay * kb_x / size
            kb_y -= decay * kb_y / size
        out += self_x + kb_x
        height += self_y + kb_y
        if frame == hitstun - 1:
            end = (out, height)
        if frame >= hitstun and -17.5 <= height <= -8.5 and SHORTEST_SIDE_B <= out <= ft['full']:
            window = True
    return end, window


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else 'melee.iso'
    with open(path, 'rb') as iso:
        rules = common(iso)
        chars = []
        for dat, name, full in CHARACTERS:
            ft = fighter(iso, dat)
            ft['full'] = full
            chars.append((name, ft))
    print(f'Knockback speed {rules[0]:.3f}/unit, hitstun {rules[1]:.2f} frames/unit, decay {rules[2]:.3f}/frame')
    for name, ft in chars:
        print(f"{name}: gravity {ft['gravity']:.3f}, falls up to {ft['terminal']:.2f}, "
              f"drifts up to {ft['drift_max']:.2f} ({ft['accel']:.2f} a frame)")
    print()
    print('| Preset | Knockback | Angle | ' + ' | '.join(f'{n} out | {n} height | {n} fall in' for n, _ in chars) + ' |')
    print('| --- | --- | --- | ' + ' | '.join('--- | --- | ---' for _ in chars) + ' |')
    for preset, kb_min, kb_max, angle_min, angle_max in presets():
        cells = []
        for _, ft in chars:
            runs = [launch(ft, rules, kb, angle) for kb in (kb_min, kb_max) for angle in (angle_min, angle_max)]
            outs = [end[0] for end, _ in runs]
            heights = [end[1] for end, _ in runs]
            fall_in = sum(window for _, window in runs)
            cells += [f'{min(outs):.0f} to {max(outs):.0f}', f'{min(heights):+.0f} to {max(heights):+.0f}',
                      'yes' if fall_in == len(runs) else 'no' if not fall_in else 'some']
        print(f'| {preset} | {kb_min:g}-{kb_max:g} | {angle_min:g}-{angle_max:g} | ' + ' | '.join(cells) + ' |')


if __name__ == '__main__':
    main()
