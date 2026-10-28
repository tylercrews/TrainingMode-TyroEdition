"""Build the Fox and Falco Double-Jump Height Indicator Gecko codes.

Assembles slippi-gecko-codes/dj-height-indicator with bin/hgecko (which needs
devkitPPC and DEVKITPPC set), then writes one paste-able text file per code.

Usage: python -I scripts/build_dj_height_gecko.py
"""
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT / 'slippi-gecko-codes/dj-height-indicator'
GCT = ROOT / 'build/dj-height-indicator.gct'
# Hook address: text file, Slippi code name, description lines.
CODES = {
    0x8016E8E0: ('fox-dj-height-indicator.txt', 'Fox Double-Jump Height Indicator (Slippi NTSC 1.02)',
                 'Legal stages: orange line under each ledge and an X at ledge height.'),
    0x8016E8E8: ('falco-dj-height-indicator.txt', 'Falco Double-Jump Height Indicator (Slippi NTSC 1.02)',
                 'Legal stages: sky blue line under each ledge and an X at ledge height.'),
}
NOTE = 'Double jump from the line, drift to the X, side-B at the peak. Visual only.'


def main():
    tool = {'win32': 'hgecko.exe', 'darwin': 'hgecko_macos_arm64'}.get(sys.platform, 'hgecko')
    GCT.parent.mkdir(exist_ok=True)
    subprocess.run([str(ROOT / 'bin' / tool), '-q', str(FOLDER), str(GCT)], check=True)
    data = GCT.read_bytes()
    assert data[:8] == bytes.fromhex('00D0C0DE00D0C0DE') and data[-8:] == bytes.fromhex('F000000000000000')
    at, built = 8, set()
    while at < len(data) - 8:
        first, count = struct.unpack_from('>II', data, at)
        assert first >> 24 == 0xC2, f'unexpected code type {first:08X}'
        address, end = 0x80000000 | (first & 0x1FFFFFF), at + 8 + count * 8
        filename, title, description = CODES[address]
        lines = ['$' + title, '*' + description, '*' + NOTE]
        lines += [f'{data[i:i + 4].hex().upper()} {data[i + 4:i + 8].hex().upper()}' for i in range(at, end, 8)]
        (FOLDER / filename).write_text('\n'.join(lines) + '\n', newline='\n')
        print(f'wrote {filename} ({count} lines at {address:08X})')
        built.add(address)
        at = end
    assert built == set(CODES), 'expected one code per character'


if __name__ == '__main__':
    main()
