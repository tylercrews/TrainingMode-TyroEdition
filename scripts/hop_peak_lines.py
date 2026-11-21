"""Print, and with --write regenerate, the Hop Peak Lines Gecko codes' numbers.

From a vanilla ISO, for Fox and Falco: the laser shot at the peak of a short
hop and of a full hop, above the floor jumped from. That's the jump's rise
(the native jump applies gravity before moving, scripts/laser_heights.rise)
plus the aerial laser's height over TopN (laser_heights.Fighter.constants()).
A jump's peak is its highest point and a shot comes from where the fighter was
the frame before, so these are the highest lasers each jump can shoot.
src/targets.c works out the same from the live fighter.

For each legal stage: the main floor's height at the middle (x = 0), and its
platform lines: one-way floor lines at least 10 wide, from Stadium's base
layout and Yoshi's without Randall's line. The codes read those lines' live
vertices each frame, so their DAT coordinates (Fountain's are joint-local)
don't matter, only their IDs.

Usage: python -I scripts/hop_peak_lines.py [path/to/melee.iso] [--write]
       --write rewrites slippi-gecko-codes/hop-peak-lines/*.asm.
"""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import laser_heights  # noqa: E402
import legal_stage_positions as stages  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT / 'slippi-gecko-codes/hop-peak-lines'
NO_LINE = 0xFF
PLATFORMS = 3
# name, FIGHTER_KIND, hook, the instruction it replaces. StartMelee's last two
# unconditional, non-branch instructions no other code in this repository or
# Slippi's uses: mr r31,r3 after the prologue, and the final blr.
CODES = [
    ('Fox', 1, 0x8016E740, 'mr r31, r3'),
    ('Falco', 22, 0x8016E930, 'blr'),
]
# The legal stages by external ID, with the floor lines that count (None: all).
STAGES = sorted(stages.STAGES + [('GrIz.dat', 'Fountain of Dreams', 2, None)], key=lambda s: s[2])


def peaks(iso, name):
    """(short hop, full hop) peak laser above the floor, and the pieces."""
    fighter = laser_heights.Fighter(iso, name)
    a, air = fighter.attr, fighter.constants()['air']
    hop, full = laser_heights.rise(a['hop_v'], a['gravity']), laser_heights.rise(a['jump_v'], a['gravity'])
    return {'short': hop + air, 'full': full + air, 'hop': hop, 'jump': full, 'air': air}


def stage_numbers(iso):
    """[(external ID, name, [platform line IDs], main floor y at x = 0)]."""
    result = []
    for filename, name, stage_id, normal in STAGES:
        lines = [l for l in stages.floors(stages.read_file(iso, filename)) if normal is None or l[0] in normal]
        solid = [l for l in lines if not l[5] & 0x100]
        platforms = [l[0] for l in lines if l[5] & 0x100 and l[3] - l[1] >= 10]
        assert len(platforms) <= PLATFORMS, name
        result.append((stage_id, name, platforms, stages.ground_y(solid, 0)))
    return result


def asm_path(name):
    return FOLDER / f'{name} Hop Peak Lines.asm'


def asm(code, n, stage_table):
    name, kind, hook, original = code
    lines = [
        f'    # To be inserted at {hook:08x}',
        f'    # {name} Hop Peak Lines for vanilla NTSC 1.02 Melee / Slippi.',
        f'    # While {name} is in the match on a legal stage, draws a teal line where',
        f'    # a laser shot at the peak of {name}\'s short hop flies, and a purple one',
        '    # for a full hop, over the main floor and each platform. Written by',
        '    # scripts/hop_peak_lines.py from the ISO; the drawing is shared in',
        '    # hop-peak-lines.inc.',
        '',
        f'    .set FIGHTER_KIND, {kind}',
        '',
        '    .macro character_data',
        f'    .float {n["short"]:.6f}                # short hop: rises {n["hop"]:.4f}, laser {n["air"]:.4f} over TopN',
        f'    .float {n["full"]:.6f}                # full hop: rises {n["jump"]:.4f}',
        '    .endm',
        '',
        '    # External stage ID, platform lines (NO_LINE: none), main floor y at x = 0.',
        '    .macro stage_data',
    ]
    for stage_id, stage_name, platforms, main in stage_table:
        ids = ', '.join(f'0x{i:02X}' for i in platforms + [NO_LINE] * (PLATFORMS - len(platforms)))
        lines.append(f'    stage {stage_id}, {ids}, {main:.6f}'.ljust(52) + f'# {stage_name}')
    lines += [
        '    .endm',
        '',
        '    .macro original_instruction',
        f'    {original}',
        '    .endm',
        '',
        '    .include "hop-peak-lines.inc"',
    ]
    return '\n'.join(lines) + '\n'


def main():
    args = [arg for arg in sys.argv[1:] if arg != '--write']
    path = args[0] if args else str(ROOT / 'melee.iso')
    with open(path, 'rb') as iso:
        table = stage_numbers(iso)
        for code in CODES:
            n = peaks(iso, code[0])
            print(f'{code[0]}: short hop rises {n["hop"]:.4f}, full hop {n["jump"]:.4f}, aerial laser '
                  f'{n["air"]:.4f} over TopN: lines {n["short"]:.4f} (teal) and {n["full"]:.4f} (purple) up')
            if '--write' in sys.argv:
                asm_path(code[0]).write_text(asm(code, n, table), newline='\n')
                print(f'  wrote {asm_path(code[0]).name}')
        for stage_id, name, platforms, main_y in table:
            print(f'{name} ({stage_id}): main floor {main_y:.4f} at x = 0, platform lines {platforms}')


if __name__ == '__main__':
    main()
