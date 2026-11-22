"""Build the Randall Timer Minimap Gecko code.

Assembles slippi-gecko-codes/randall-timer-minimap with bin/hgecko (which needs
devkitPPC and DEVKITPPC set), then writes its paste-able text file.

Usage: python -I scripts/build_randall_timer_minimap_gecko.py
"""
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT / 'slippi-gecko-codes/randall-timer-minimap'
GCT = ROOT / 'build/randall-timer-minimap.gct'
HOOK = 0x8016E750
TEXT = 'randall-timer-minimap.txt'
LINES = ['$Randall Timer Minimap (Slippi NTSC 1.02) [Tyro]',
         "*Yoshi's Story: a map of Randall's loop through the match timer, with an arrow",
         '*where Randall is, behind the numbers while he is behind the stage. Visual only.']


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
