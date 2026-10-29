"""Build the Blast Zone Overlay Gecko code.

Assembles slippi-gecko-codes/blast-zone-overlay with bin/hgecko (which needs
devkitPPC and DEVKITPPC set), then writes its paste-able text file.

Usage: python -I scripts/build_blast_zone_gecko.py
"""
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT / 'slippi-gecko-codes/blast-zone-overlay'
GCT = ROOT / 'build/blast-zone-overlay.gct'
HOOK = 0x8016E8C0
TEXT = 'blast-zone-overlay.txt'
LINES = ['$Blast Zone Overlay (Slippi NTSC 1.02)',
         '*Every stage: a red line on the blast zones, with a red glow past them.',
         '*Visual only; reads the stage and never changes game state.']


def main():
    tool = {'win32': 'hgecko.exe', 'darwin': 'hgecko_macos_arm64'}.get(sys.platform, 'hgecko')
    GCT.parent.mkdir(exist_ok=True)
    subprocess.run([str(ROOT / 'bin' / tool), '-q', str(FOLDER), str(GCT)], check=True)
    data = GCT.read_bytes()
    assert data[:8] == bytes.fromhex('00D0C0DE00D0C0DE') and data[-8:] == bytes.fromhex('F000000000000000')
    first, count = struct.unpack_from('>II', data, 8)
    assert first >> 24 == 0xC2 and 0x80000000 | (first & 0x1FFFFFF) == HOOK, f'unexpected code {first:08X}'
    end = 16 + count * 8
    assert end == len(data) - 8, 'expected exactly one code'
    lines = LINES + [f'{data[i:i + 4].hex().upper()} {data[i + 4:i + 8].hex().upper()}' for i in range(8, end, 8)]
    (FOLDER / TEXT).write_text('\n'.join(lines) + '\n', newline='\n')
    print(f'wrote {TEXT} ({count} lines at {HOOK:08X})')


if __name__ == '__main__':
    main()
