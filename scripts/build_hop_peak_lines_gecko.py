"""Build the Fox and Falco Hop Peak Lines Gecko codes.

Assembles slippi-gecko-codes/hop-peak-lines with bin/hgecko (which needs
devkitPPC and DEVKITPPC set), then writes one paste-able text file per code.
scripts/hop_peak_lines.py --write regenerates the .asm files first, when the
numbers change.

Usage: python -I scripts/build_hop_peak_lines_gecko.py
"""
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT / 'slippi-gecko-codes/hop-peak-lines'
GCT = ROOT / 'build/hop-peak-lines.gct'
# Hook address: text file, Slippi code name, description line.
CODES = {
    0x8016E740: ('fox-hop-peak-lines.txt', 'Fox Hop Peak Lines (Slippi NTSC 1.02)',
                 "Teal: a laser at a short hop's peak. Purple: a full hop's. Over the floor and each platform."),
    0x8016E930: ('falco-hop-peak-lines.txt', 'Falco Hop Peak Lines (Slippi NTSC 1.02)',
                 "Teal: a laser at a short hop's peak. Purple: a full hop's. Over the floor and each platform."),
}
NOTE = 'Legal stages, while this character is in the match. Side platform lines stop at the middle. Visual only.'


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
