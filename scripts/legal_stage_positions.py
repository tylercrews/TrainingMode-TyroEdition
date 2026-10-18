"""Print legal-stage platform reference positions from a vanilla Melee ISO.

Uses each stage DAT's static collision lines (scaled by grGroundParam) and the
same rules as Low Laser Puff Practice's live placement in src/lowlaser.c:
platform = floor line with native flag 0x0100, side platforms are the ones
nearest center beyond X = +/-10, top is the highest platform spanning X = 0,
"on" is the platform's center and "under" is the highest solid floor there.

Fountain of Dreams is skipped: its platform lines are stored in joint-local
coordinates and move at runtime, so only live geometry gives real positions.

Usage: python scripts/legal_stage_positions.py [path/to/melee.iso]
"""
import struct
import sys

# (DAT, name, external stage ID, enabled floor line indices or None for all).
# Stadium's base layout is its normal group; Yoshi's excludes Randall's line.
STAGES = [
    ('GrNLa.dat', 'Final Destination', 32, None),
    ('GrNBa.dat', 'Battlefield', 31, None),
    ('GrOp.dat', 'Dream Land', 28, None),
    ('GrSt.dat', "Yoshi's Story", 8, {1, 2, 3, 4, 5, 6}),
    ('GrPs.dat', 'Pokemon Stadium', 3, {34, 35, 36, 51, 52, 53, 54}),
]


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


def floors(dat):
    """(index, ax, ay, bx, by, low_flags) for every floor line, in stage units."""
    _, size, relocs, publics, externs = struct.unpack_from('>5I', dat)
    roots_at = 32 + size + 4 * relocs
    strings = roots_at + 8 * (publics + externs)
    roots = {}
    for i in range(publics):
        data, name = struct.unpack_from('>II', dat, roots_at + i * 8)
        roots[dat[strings + name:].split(b'\0', 1)[0].decode()] = 32 + data
    verts, vert_count, lines, line_count = struct.unpack_from('>4I', dat, roots['coll_data'])
    scale = struct.unpack_from('>f', dat, roots['grGroundParam'])[0]
    result = []
    for i in range(line_count):
        at = 32 + lines + i * 16
        start, end = struct.unpack_from('>hh', dat, at)
        high, low = struct.unpack_from('>HH', dat, at + 12)
        if not high & 1 or not (0 <= start < vert_count and 0 <= end < vert_count):
            continue
        a = struct.unpack_from('>2f', dat, 32 + verts + start * 8)
        b = struct.unpack_from('>2f', dat, 32 + verts + end * 8)
        (ax, ay), (bx, by) = sorted(((a[0] * scale, a[1] * scale), (b[0] * scale, b[1] * scale)))
        result.append((i, ax, ay, bx, by, low))
    return result


def ground_y(solid, x):
    ys = [ay + (by - ay) * (x - ax) / (bx - ax) for _, ax, ay, bx, by, _ in solid if ax <= x <= bx]
    return max(ys) if ys else None


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else 'melee.iso'
    with open(path, 'rb') as iso:
        print('| Stage | Main floor X | Platform | Line (left -> right) | On plat (Puff) | Under plat (Puff) |')
        print('| --- | --- | --- | --- | --- | --- |')
        for filename, name, stage_id, normal in STAGES:
            lines = [l for l in floors(read_file(iso, filename)) if normal is None or l[0] in normal]
            solid = [l for l in lines if not l[5] & 0x100]
            platforms = [l for l in lines if l[5] & 0x100 and l[3] - l[1] >= 10]
            edges = '%.4f .. %.4f' % (min(l[1] for l in solid), max(l[3] for l in solid))
            picks = {
                'Left': min((l for l in platforms if (l[1] + l[3]) / 2 < -10), key=lambda l: abs(l[1] + l[3]), default=None),
                'Top': max((l for l in platforms if l[1] <= 0 <= l[3]), key=lambda l: l[2], default=None),
                'Right': min((l for l in platforms if (l[1] + l[3]) / 2 > 10), key=lambda l: abs(l[1] + l[3]), default=None),
            }
            if not platforms:
                print('| %s (%d) | %s | none | - | - | - |' % (name, stage_id, edges))
                continue
            for side, line in picks.items():
                if line is None:
                    print('| %s (%d) | %s | %s | none | N/A | N/A |' % (name, stage_id, edges, side))
                    continue
                _, ax, ay, bx, by, _ = line
                x = (ax + bx) / 2
                y = ay + (by - ay) * (x - ax) / (bx - ax)
                under = '-' if side == 'Top' else '(%.4f, %.4f)' % (x, ground_y(solid, x))
                print('| %s (%d) | %s | %s | (%.4f, %.4f) -> (%.4f, %.4f) | (%.4f, %.4f) | %s |' %
                      (name, stage_id, edges, side, ax, ay, bx, by, x, y, under))


if __name__ == '__main__':
    main()
