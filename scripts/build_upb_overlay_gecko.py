"""Build the Up-B Overlay Gecko codes, one per character, plus a B0XX one for
each aimed up-B.

Assembles slippi-gecko-codes/up-b-overlay with bin/hgecko (which needs
devkitPPC and DEVKITPPC set), then writes one paste-able text file per code.
scripts/upb_overlay_numbers.py --write regenerates the .asm files first, when
the shapes change.

Usage: python -I scripts/build_upb_overlay_gecko.py
"""
import importlib.util
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT / 'slippi-gecko-codes/up-b-overlay'
GCT = ROOT / 'build/up-b-overlay.gct'
_spec = importlib.util.spec_from_file_location('upb_overlay_numbers', ROOT / 'scripts/upb_overlay_numbers.py')
numbers = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(numbers)
NOTE = 'Legal stages, while this character is in the match. Visual only.'


def description(code):
    if code.b0xx and code.slug in numbers.SPACIES:
        return 'Up-B from a dot with its B0XX angle (white: no modifier, blue: Mod X or Y).'
    if code.b0xx:
        return 'Up-B from a dot with its B0XX angle (white: no modifier, blue: Mod X or Y + B, dim: no B).'
    if code.slug in numbers.SPACIES:
        return 'Up-B from the blue ring and aim at the white X; in a gap, only straight (the tick) aims.'
    if code.slug in numbers.TRAVEL:
        return 'Up-B between the blue ring and the dim edge and aim at the white X; in a gap, only straight.'
    return 'Up-B at the blue dot for the fastest ledge grab; the white X is the grab spot.'


def main():
    tool = {'win32': 'hgecko.exe', 'darwin': 'hgecko_macos_arm64'}.get(sys.platform, 'hgecko')
    GCT.parent.mkdir(exist_ok=True)
    subprocess.run([str(ROOT / 'bin' / tool), '-q', str(FOLDER), str(GCT)], check=True)
    data = GCT.read_bytes()
    assert data[:8] == bytes.fromhex('00D0C0DE00D0C0DE') and data[-8:] == bytes.fromhex('F000000000000000')
    by_hook = {c.hook: c for c in numbers.CODES}
    at, built = 8, set()
    while at < len(data) - 8:
        first, count = struct.unpack_from('>II', data, at)
        assert first >> 24 == 0xC2, f'unexpected code type {first:08X}'
        address, end = 0x80000000 | (first & 0x1FFFFFF), at + 8 + count * 8
        code = by_hook[address]
        filename = f'{code.key}-up-b-overlay.txt'
        title = f'{code.name} Up-B Overlay' + (' B0XX' if code.b0xx else '')
        lines = [f'${title} (Slippi NTSC 1.02)', '*' + description(code), '*' + NOTE]
        lines += [f'{data[i:i + 4].hex().upper()} {data[i + 4:i + 8].hex().upper()}' for i in range(at, end, 8)]
        with open(FOLDER / filename, 'wb') as f:
            f.write(('\n'.join(lines) + '\n').encode('utf-8'))
        print(f'wrote {filename} ({count} lines at {address:08X})')
        built.add(address)
        at = end
    assert built == set(by_hook), 'expected one code per character and B0XX code'


if __name__ == '__main__':
    main()
