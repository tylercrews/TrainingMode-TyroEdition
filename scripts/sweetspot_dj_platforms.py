"""Print the Side-B Sweetspot double-jump platforms from a vanilla Melee ISO.

Uses the same rules as the event's Double-Jump Height Indicator in
src/sweetspot.c, in single-precision floats like the game:

- Double jump: Y speed = jump V * air jump V multiplier, X speed = full stick *
  air jump H multiplier. Gravity applies on the first physics frame. With the
  stick held, X speed above the drift max slows by air friction each frame.
  "rise" is the height at the peak and "drift" how far the jump moves sideways
  by then.
- Full side-B: three dash steps (78/4 for Fox, 60/4 for Falco, times the model
  scale) plus the aerial endlag drift (special attributes 0x3C speed, 0x40
  decay). This is the white 0 dot of the event's range guide.
- Height: the platform is where your feet double jump so the peak puts the
  middle of the ledge-grab box (FtCollDesc 0x14, unscaled) on the ledge. Fox
  drops 1/6 during side-B startup, so his platform is 1/6 higher.
- Ledges: the event's line IDs, walked to the end of the floor like the game's
  Stage_GetLeft/RightOfLineCoordinates, scaled by grGroundParam.

The Slippi Gecko code (slippi-gecko-codes/dj-height-indicator) hardcodes the
three per-character numbers this prints.

Usage: python -I scripts/sweetspot_dj_platforms.py [path/to/melee.iso]
"""
import struct
import sys

# (DAT, name, external stage ID, left ledge line, right ledge line), as in
# GetLedgePositions in src/sweetspot.c.
STAGES = [
    ('GrIz.dat', 'Fountain of Dreams', 2, 0x03, 0x07),
    ('GrPs.dat', 'Pokemon Stadium', 3, 0x33, 0x36),
    ('GrSt.dat', "Yoshi's Story", 8, 0x02, 0x06),
    ('GrOp.dat', 'Dream Land', 28, 0x03, 0x05),
    ('GrNBa.dat', 'Battlefield', 31, 0x00, 0x05),
    ('GrNLa.dat', 'Final Destination', 32, 0x00, 0x02),
]
CHARACTERS = [('PlFx.dat', 'Fox', 78.0, 1 / 6), ('PlFc.dat', 'Falco', 60.0, 0.0)]


def f32(value):
    return struct.unpack('>f', struct.pack('>f', value))[0]


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


def roots(dat):
    """Body (DAT data section) and its public roots by name."""
    _, size, relocs, publics, externs = struct.unpack_from('>5I', dat)
    roots_at = 32 + size + 4 * relocs
    strings = roots_at + 8 * (publics + externs)
    found = {}
    for i in range(publics):
        data, name = struct.unpack_from('>II', dat, roots_at + i * 8)
        found[dat[strings + name:].split(b'\0', 1)[0].decode()] = data
    return dat[32:32 + size], found


def character(dat, dash, drop):
    body, found = roots(dat)
    ft = next(at for name, at in found.items() if name.startswith('ftData'))
    common, special, coll = (struct.unpack_from('>I', body, ft + at)[0] for at in (0x0, 0x4, 0x44))
    a = lambda at: struct.unpack_from('>f', body, common + at)[0]
    jump_v, air_v, air_h = a(0x40), a(0x50), a(0x54)
    gravity, stick_mul, base, drift_max, friction, air_max, scale = (
        a(0x5C), a(0x64), a(0x68), a(0x6C), a(0x70), a(0x78), a(0x8C))
    speed_y, speed_x = f32(jump_v * air_v), f32(air_h)
    accel = f32(stick_mul + base)
    rise = drift = 0.0
    frames = 0
    while True:
        speed_y = f32(speed_y - gravity)
        if speed_y <= 0:
            break
        step = accel
        if speed_x + step > drift_max:
            step = -friction
            if speed_x + step < drift_max:
                step = f32(drift_max - speed_x)
            if speed_x + step > air_max:
                step = f32(air_max - speed_x)
        speed_x = f32(speed_x + step)
        rise, drift, frames = f32(rise + speed_y), f32(drift + speed_x), frames + 1

    end_speed, end_decay = struct.unpack_from('>2f', body, special + 0x3C)
    endlag = 0.0
    for _ in range(40):
        end_speed = f32(end_speed - end_decay)
        if end_speed <= 0:
            break
        endlag = f32(endlag + end_speed)
    full = f32(endlag + 3 * f32(f32(dash / 4) * scale))
    grab_y, grab_height = struct.unpack_from('>2f', body, coll + 0x14)
    depth = f32(grab_y + rise - drop)
    return {'rise': rise, 'frames': frames, 'drift': drift, 'full': full, 'depth': depth,
            'grab_y': grab_y, 'grab_height': grab_height}


def ledges(dat, left_line, right_line):
    body, found = roots(dat)
    verts, _, lines, _ = struct.unpack_from('>4I', body, found['coll_data'])
    scale = struct.unpack_from('>f', body, found['grGroundParam'])[0]
    line = lambda i: struct.unpack_from('>4h', body, lines + i * 16)
    floor = lambda i: struct.unpack_from('>H', body, lines + i * 16 + 12)[0] & 1
    vert = lambda i: (f32(struct.unpack_from('>f', body, verts + i * 8)[0] * scale),
                      f32(struct.unpack_from('>f', body, verts + i * 8 + 4)[0] * scale))
    # Walk to the end of the connected floor, like mpLib_80053ECC/80053DA4.
    while line(left_line)[2] != -1 and floor(line(left_line)[2]):
        left_line = line(left_line)[2]
    while line(right_line)[3] != -1 and floor(line(right_line)[3]):
        right_line = line(right_line)[3]
    return vert(line(left_line)[0]), vert(line(right_line)[1])


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else 'melee.iso'
    with open(path, 'rb') as iso:
        chars = [(name, character(read_file(iso, dat), dash, drop)) for dat, name, dash, drop in CHARACTERS]
        for name, c in chars:
            print(f"{name}: double jump rises {c['rise']:.3f} in {c['frames']} frames, drifts {c['drift']:.3f}; "
                  f"full side-B {c['full']:.3f}; ledge-grab box {c['grab_y']:.1f} +/- {c['grab_height'] / 2:.1f} "
                  f"above the feet; platform {c['depth']:.3f} under the ledge")
        print()
        print('| Stage | Ledges (x, y) | ' + ' | '.join(f'{name} platforms (x from..to, y)' for name, _ in chars) + ' |')
        print('| --- | --- | ' + ' | '.join('---' for _ in chars) + ' |')
        for dat, stage, _, left_line, right_line in STAGES:
            (lx, ly), (rx, ry) = ledges(read_file(iso, dat), left_line, right_line)
            cells = []
            for _, c in chars:
                center = rx + c['full']
                cells.append(f"+/-{center - c['drift']:.2f}..{center + c['drift']:.2f}, {ry - c['depth']:.2f}")
            print(f'| {stage} | +/-{rx:.2f}, {ry:.2f} | ' + ' | '.join(cells) + ' |')
            if abs(lx + rx) > 1e-3 or abs(ly - ry) > 1e-3:
                print(f'  ({stage} is not symmetric: left ledge {lx:.4f}, {ly:.4f})')


if __name__ == '__main__':
    main()
