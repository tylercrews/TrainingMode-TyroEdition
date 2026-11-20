"""The Randall Timer: Yoshi's Story's match timer shows which side Randall is out on.

python tests/test_randall_timer.py -v
Builds tests/randall_timer_probe.c, which includes the production
src/stage_overlay_game.c, and runs it as PowerPC code. The stage, Randall's
cloud joint, the settings and the timer's joints are fixtures. With melee.iso
at the repo root, it also checks the research the overlay rests on: Randall's
path and the stage walls in GrSt.dat, the timer's materials in IfAll.usd, and
where the game keeps the timer's GObj.
"""
from pathlib import Path
import math
import re
import struct
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "build/test-deps"))
from test_score_storage import NativeMachine

ELF = ROOT / "build/randall-timer-test.elf"
FLAGS, EXTERNAL, INTERNAL, MAP, TIMER, MAP_INDEX, CLOUD, REBUILD = range(8)
WHITE, GRAY, PART0 = 0xFFFFFFFF, 0xB3B3B3FF, 0x8040207F
LEFT, RIGHT = -1, 1


def build():
    compiler = Path("C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe")
    subprocess.run([
        str(compiler) if compiler.exists() else "powerpc-eabi-gcc", "-O2", "-w",
        "-ffreestanding", "-fno-builtin", "-msdata=none", "-mcpu=750", "-nostdlib",
        "-Wl,-Ttext=0x80600000,-e,TestInit,--unresolved-symbols=ignore-all",
        "tests/randall_timer_probe.c", "-o", str(ELF)
    ], cwd=ROOT, check=True)


class Machine(NativeMachine):
    def __init__(self):
        super().__init__()
        elf = ELF.read_bytes()
        offset = struct.unpack_from(">I", elf, 0x20)[0]
        stride, count = struct.unpack_from(">HH", elf, 0x2E)
        sections = [struct.unpack_from(">10I", elf, offset + i * stride) for i in range(count)]
        self.symbols = {}
        for _, kind, flags, address, at, size, link, _, _, entry in sections:
            if flags & 2 and size and kind != 8:
                self.cpu.mem_write(address, elf[at:at + size])
            if kind == 2:
                strings = sections[link]
                names = elf[strings[4]:strings[4] + strings[5]]
                for index in range(0, size, entry):
                    name, value, *_ = struct.unpack_from(">IIIBBH", elf, at + index)
                    self.symbols[names[name:].split(b"\0", 1)[0].decode()] = value
        self.fn("TestInit")
        self.parts = self.fn("TestInfo", 3)

    def fn(self, name, *args):
        return self.call(self.symbols[name], *args)

    def info(self, what):
        value = self.fn("TestInfo", what)
        return value - (1 << 32) if value & 0x80000000 else value

    def colors(self, timer=0):
        return [self.fn("TestColor", timer, part) for part in range(self.parts)]

    def frame(self, x=None):
        if x is not None:
            self.fn("TestCloud", round(x * 100) & 0xFFFFFFFF)
        self.fn("TestDraw", 2)


def untinted():
    return [PART0] + [GRAY] * 2 + [WHITE] * 10


def tinted(tint):
    def times(color):
        channels = [(color >> s & 255) * (tint >> s & 255) // 255 for s in (24, 16, 8)]
        return channels[0] << 24 | channels[1] << 16 | channels[2] << 8 | (color & 255)
    colors = [times(c) for c in untinted()]
    colors[12] = WHITE  # The spline joint's "DObj" is never touched.
    return colors


def f32(bits):
    return struct.unpack(">f", struct.pack(">I", bits))[0]


def palette():
    """OSD_PaletteColor's table, from src/osd_style.c."""
    source = (ROOT / "src/osd_style.c").read_text()
    table = source[source.index("palette[] = {"):source.index("};", source.index("palette[] = {"))]
    return [int(word, 16) for word in re.findall(r"0x([0-9A-Fa-f]{8})", table)]


class RandallTimerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build()

    def setUp(self):
        self.m = Machine()
        self.left, self.right, out = (self.m.fn("TestConstant", i) for i in range(3))
        self.out = f32(out)

    def test_colors_are_the_palettes_purple_and_teal(self):
        colors = palette()
        self.assertEqual((self.left, self.right), (colors[10], colors[11]))  # TM_COLOR_PURPLE, TM_COLOR_TEAL
        self.assertEqual((self.left, self.right), (0xA064FFFF, 0x00B4A0FF))
        self.assertEqual(self.m.info(5), 14)  # TM_FLAG_RANDALL_TIMER, the flag the settings tests save.

    def test_left_is_purple_right_is_teal_and_behind_the_stage_is_untinted(self):
        m, out = self.m, self.out
        for x, side in ((-97.5, LEFT), (-out - 0.01, LEFT), (-out + 0.01, 0), (0, 0), (out - 0.01, 0),
                        (out + 0.01, RIGHT), (97.5, RIGHT), (-out - 0.01, LEFT), (0, 0)):
            with self.subTest(x=x):
                m.frame(x)
                self.assertEqual(m.info(1), side)
                want = untinted() if not side else tinted(self.left if side == LEFT else self.right)
                self.assertEqual(m.colors(), want)
        self.assertEqual(m.colors(1), untinted())  # The other timer isn't the match's.

    def test_white_digits_become_the_tint_and_gray_separators_keep_their_shading(self):
        m = self.m
        m.frame(-80)
        colors = m.colors()
        self.assertEqual(colors[3:11], [self.left] * 8)
        self.assertEqual(colors[1], 0x70 << 24 | 0x46 << 16 | 0xB3 << 8 | 0xFF)  # 0xB3 times purple.
        self.assertEqual(colors[0] & 255, 0x7F)  # Alpha is the material's own.

    def test_off_never_touches_the_timer_or_looks_for_randall(self):
        m = self.m
        m.fn("TestSet", FLAGS, 0)
        for x in (-80, 0, 80):
            m.frame(x)
        self.assertEqual(m.colors(), untinted())
        self.assertEqual((m.info(0), m.info(2)), (0, 0))  # No position read, nothing saved.

    def test_turning_it_off_puts_the_timers_own_colors_back(self):
        m = self.m
        m.frame(80)
        self.assertEqual(m.colors(), tinted(self.right))
        m.fn("TestSet", FLAGS, 0)
        m.frame()
        self.assertEqual(m.colors(), untinted())
        self.assertEqual(m.info(1), 0)
        # And back on while Randall is still out.
        m.fn("TestSet", FLAGS, 1 << 14)
        m.frame()
        self.assertEqual(m.colors(), tinted(self.right))

    def test_it_writes_only_when_the_side_changes(self):
        m = self.m
        m.frame(80)
        m.fn("TestSetColor", 0, 5, 0x12345678)  # Something else colors a digit.
        m.frame(81)
        self.assertEqual(m.fn("TestColor", 0, 5), 0x12345678)
        m.frame(-81)
        self.assertEqual(m.colors(), tinted(self.left))

    def test_other_stages_do_nothing(self):
        m = self.m
        m.fn("TestSet", EXTERNAL, m.info(8))  # Battlefield.
        m.fn("TestSet", INTERNAL, 0)
        for x in (-80, 80):
            m.frame(x)
        self.assertEqual(m.colors(), untinted())
        self.assertEqual((m.info(0), m.info(1), m.info(2)), (0, 0, 0))

    def test_no_randall_reads_as_behind_the_stage(self):
        m = self.m
        for what, value in ((MAP, 0), (MAP_INDEX, 3), (CLOUD, 0)):
            with self.subTest(what=what):
                m.fn("TestInit")
                m.frame(80)
                self.assertEqual(m.colors(), tinted(self.right))
                m.fn("TestSet", what, value)  # Hazards removed it, or it isn't Randall.
                m.frame(80)
                self.assertEqual(m.colors(), untinted())
                self.assertEqual(m.info(1), 0)

    def test_no_timer_does_nothing(self):
        m = self.m
        m.fn("TestSet", TIMER, -1 & 0xFFFFFFFF)
        m.frame(80)
        self.assertEqual((m.info(0), m.info(2)), (0, 0))
        self.assertEqual(m.colors(), untinted())

    def test_a_new_timer_saves_its_own_colors_and_forgets_the_old_one(self):
        m = self.m
        m.frame(80)
        m.fn("TestSet", TIMER, 1)
        m.fn("TestSetColor", 1, 3, 0x204060FF)  # The new timer's own color.
        m.frame(80)
        self.assertEqual(m.colors(0), tinted(self.right))  # The old one is left alone.
        self.assertEqual(m.fn("TestColor", 1, 3), (0x20 * 0 // 255) << 24 | (0x40 * 0xB4 // 255) << 16 |
                         (0x60 * 0xA0 // 255) << 8 | 0xFF)
        m.frame(0)
        self.assertEqual(m.fn("TestColor", 1, 3), 0x204060FF)

    def test_a_scene_change_forgets_the_timer_even_at_the_same_address(self):
        m = self.m
        m.frame(-80)
        m.fn("TestSceneChange")
        m.fn("TestSet", REBUILD, 0)  # A fresh timer where the old one was.
        m.frame(-80)  # Still out on the left: it has to tint again.
        self.assertEqual(m.colors(), tinted(self.left))
        m.frame(0)
        self.assertEqual(m.colors(), untinted())

    def test_saves_every_material_in_the_tree_up_to_its_room(self):
        m = self.m
        m.frame(80)
        self.assertEqual(m.info(2), 12)  # 10 joints, the nested child's two, not the spline.
        self.assertLessEqual(12, m.info(4))

    def test_only_the_last_render_pass_runs_it(self):
        m = self.m
        m.fn("TestCloud", 8000)
        for render_pass in (0, 1, 3):
            m.fn("TestDraw", render_pass)
        self.assertEqual(m.colors(), untinted())
        self.assertEqual(m.info(0), 0)


# --- melee.iso research checks ---------------------------------------------

def iso_file(filename):
    with (ROOT / "melee.iso").open("rb") as iso:
        iso.seek(0x424)
        offset, size = struct.unpack(">II", iso.read(8))
        iso.seek(offset)
        fst = iso.read(size)
        count = struct.unpack_from(">I", fst, 8)[0]
        names = fst[12 * count:]
        for index in range(1, count):
            name, at, length = struct.unpack_from(">3I", fst, 12 * index)
            if names[name & 0xFFFFFF:].split(b"\0", 1)[0].decode() == filename:
                iso.seek(at)
                return iso.read(length)
    raise AssertionError(filename + " missing from melee.iso")


class Dat:
    def __init__(self, filename):
        self.data = iso_file(filename)
        _, size, relocs, publics, externs = struct.unpack_from(">5I", self.data)
        roots_at = 32 + size + 4 * relocs
        strings = roots_at + 8 * (publics + externs)
        self.roots = {}
        for index in range(publics):
            at, name = struct.unpack_from(">II", self.data, roots_at + index * 8)
            self.roots[self.data[strings + name:].split(b"\0", 1)[0].decode()] = at

    def word(self, at):
        return struct.unpack_from(">I", self.data, 32 + at)[0]

    def floats(self, at, count):
        return struct.unpack_from(f">{count}f", self.data, 32 + at)

    def bytes(self, at, count):
        return self.data[32 + at:32 + at + count]


def fobj_keys(dat, fobj):
    """(frame, value) keys of a linear FObj track with u8 values."""
    length, start, kind, frac, _ = struct.unpack_from(">IfBBB", dat.data, 32 + fobj + 4)
    data = dat.bytes(dat.word(fobj + 0x10), length)
    assert frac >> 5 == 4, "u8 values"
    scale, at, frame, keys = 1 << (frac & 31), 0, start, []
    while at < len(data) and data[at]:
        op = first = data[at]; at += 1
        count, shift = op >> 4 & 7, 3
        while op & 0x80:
            op = data[at]; at += 1
            count += (op & 0x7F) << shift; shift += 7
        assert first & 0xF == 2, "linear keys (HSD_A_OP_LIN)"
        for _ in range(count + 1):
            value = data[at] / scale; at += 1
            wait, shift = 0, 0
            while True:
                byte = data[at]; at += 1
                wait |= (byte & 0x7F) << shift; shift += 7
                if not byte & 0x80:
                    break
            keys.append((frame, value))
            frame += wait
    return kind, keys


def randall_path():
    """Randall's cloud joint, frame by frame over one loop, in stage units."""
    dat = Dat("GrSt.dat")
    head = dat.roots["map_head"]
    desc = dat.word(head + 8) + 2 * 0x34  # Map GObj 2's desc.
    root = dat.word(desc)
    cloud = dat.word(root + 8)
    spline_joint = dat.word(cloud + 0xC)
    assert dat.word(spline_joint + 4) & 0x4000, "JOBJ_SPLINE"
    cloud_anim = dat.word(dat.word(dat.word(desc + 4)))  # anim_joints[0]'s child, the cloud's.
    aobj = dat.word(cloud_anim + 8)
    flags, end = struct.unpack_from(">If", dat.data, 32 + aobj)
    kind, keys = fobj_keys(dat, dat.word(aobj + 8))
    spline = dat.word(spline_joint + 0x10)
    spline_kind, cv_count = struct.unpack_from(">HH", dat.data, 32 + spline)
    cvs = [dat.floats(dat.word(spline + 8) + 12 * i, 2) for i in range(cv_count)]
    lengths = dat.floats(dat.word(spline + 0x10), cv_count)
    rz = dat.floats(spline_joint + 0x14, 3)[2]
    tx, ty = dat.floats(spline_joint + 0x2C, 2)
    scale = dat.floats(dat.roots["grGroundParam"], 1)[0]

    def place(u):
        for i in range(cv_count - 1):
            if lengths[i] <= u <= lengths[i + 1]:
                t = (u - lengths[i]) / (lengths[i + 1] - lengths[i])
                x = cvs[i][0] + (cvs[i + 1][0] - cvs[i][0]) * t
                y = cvs[i][1] + (cvs[i + 1][1] - cvs[i][1]) * t
                return (tx + x * math.cos(rz) - y * math.sin(rz), ty + x * math.sin(rz) + y * math.cos(rz))
        raise AssertionError(u)

    def param(frame):
        for (f0, v0), (f1, v1) in zip(keys, keys[1:]):
            if f0 <= frame <= f1:
                return v0 + (v1 - v0) * (frame - f0) / (f1 - f0)
        raise AssertionError(frame)

    rest = dat.floats(cloud + 0x2C, 2)
    return {
        "dat": dat, "flags": flags, "end": end, "kind": kind, "keys": keys, "spline_kind": spline_kind,
        "cvs": cvs, "lengths": lengths, "scale": scale, "rest": rest, "start": place(0),
        "frames": [tuple(scale * c for c in place(param(f))) for f in range(int(end))],
    }


def windows(path, out):
    """Each run of frames the cloud spends past +/-out, as (side, first, last), wrapping the loop."""
    sides = [LEFT if x < -out else RIGHT if x > out else 0 for x, _ in path]
    start = sides.index(0)
    runs, current = [], None
    for step in range(len(sides)):
        frame = (start + step) % len(sides)
        if sides[frame] and (not current or current[0] != sides[frame]):
            current = [sides[frame], frame, frame]
            runs.append(current)
        elif sides[frame]:
            current[2] = frame
        else:
            current = None
    return [tuple(run) for run in runs]


@unittest.skipUnless((ROOT / "melee.iso").exists(), "needs a vanilla melee.iso at the repo root")
class IsoTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.path = randall_path()
        build()
        cls.out = f32(Machine().fn("TestConstant", 2))

    def test_randall_loops_a_linear_path_at_constant_speed_every_20_seconds(self):
        p = self.path
        self.assertEqual((p["end"], p["flags"] & 0x20000000), (1200.0, 0x20000000))  # AOBJ_ANIM_LOOP
        self.assertEqual(p["kind"], 4)  # HSD_A_J_PATH
        self.assertEqual(p["keys"], [(0.0, 0.0), (1200.0, 1.0)])
        self.assertEqual(p["spline_kind"], 0)  # Linear between control points.
        # The spline's lengths are its own cumulative arc length, normalized.
        steps = [math.dist(a, b) for a, b in zip(p["cvs"], p["cvs"][1:])]
        total = sum(steps)
        for i, length in enumerate(p["lengths"]):
            self.assertAlmostEqual(length, sum(steps[:i]) / total, places=3)
        for axis in (0, 1):
            self.assertAlmostEqual(p["start"][axis], p["rest"][axis], places=3)  # The cloud's rest position.

    def test_randall_is_out_about_5_seconds_on_each_side_once_a_loop(self):
        runs = windows(self.path["frames"], self.out)
        self.assertEqual(sorted(side for side, *_ in runs), [LEFT, RIGHT])
        for side, first, last in runs:
            length = (last - first) % 1200 + 1
            self.assertTrue(290 <= length <= 310, (side, first, last))
        (a, a_first, a_last), (b, b_first, b_last) = runs
        for gap in ((b_first - a_last) % 1200 - 1, (a_first - b_last) % 1200 - 1):
            self.assertTrue(290 <= gap <= 310, gap)  # Hidden as long between them.

    def test_randall_comes_out_low_on_the_right_and_high_on_the_left(self):
        frames = self.path["frames"]
        for side, first, last in windows(frames, self.out):
            (x0, y0), (x1, y1) = frames[first], frames[last]
            # Counter-clockwise: out low and back in high on the right, the reverse on the left.
            self.assertEqual(y0 < y1, side == RIGHT)
            self.assertGreater(max(abs(x) for x, _ in frames), 90)  # It swings well out.

    def test_the_threshold_is_the_stage_walls_at_randalls_heights(self):
        dat, scale = self.path["dat"], self.path["scale"]
        self.assertAlmostEqual(scale, 0.7, places=6)
        coll = dat.roots["coll_data"]
        verts_at, vert_count, lines_at, line_count = struct.unpack_from(">4I", dat.data, 32 + coll)
        verts = [dat.floats(verts_at + 8 * i, 2) for i in range(vert_count)]
        groups_at, group_count = struct.unpack_from(">II", dat.data, 32 + coll + 0x24)
        randall_verts = set()
        for group in range(group_count):
            start, count = struct.unpack_from(">HH", dat.data, 32 + groups_at + group * 0x28 + 0x24)
            if group == 0:
                randall_verts = set(range(start, start + count))  # Randall's own platform.
        stage = [(verts[a], verts[b]) for a, b in (struct.unpack_from(">hh", dat.data, 32 + lines_at + 16 * i)
                                                   for i in range(line_count))
                 if a not in randall_verts and b not in randall_verts]

        def wall(y, side):
            xs = [a[0] + (b[0] - a[0]) * (y - a[1]) / (b[1] - a[1]) for a, b in stage
                  if a[1] != b[1] and min(a[1], b[1]) <= y <= max(a[1], b[1]) and (a[0] > 0) == (side > 0)]
            return min(xs, key=abs)

        frames = self.path["frames"]
        for side, first, last in windows(frames, self.out):
            for frame in (first, last):  # Where the cloud crosses out and back in.
                y = frames[frame][1] / scale
                self.assertAlmostEqual(wall(y, side) * scale, side * self.out, places=4)

    def test_the_timer_digits_are_textures_times_their_materials_color(self):
        dat = Dat("IfAll.usd")
        models = dat.roots["ScInfTim_scene_models"]
        model = dat.word(models)
        materials = []

        def walk(joint):
            while joint:
                if not dat.word(joint + 4) & 0x4020:
                    dobj = dat.word(joint + 0x10)
                    while dobj:
                        mobj = dat.word(dobj + 8)
                        render, tobj, mat = dat.word(mobj + 4), dat.word(mobj + 8), dat.word(mobj + 0xC)
                        colormaps = []
                        while tobj:
                            colormaps.append(dat.word(tobj + 0x40) >> 16 & 0xF)
                            tev = dat.word(tobj + 0x58)
                            self.assertEqual(dat.word(tev + 0x1C) if tev else 0, 0)  # Custom TEV inactive.
                            tobj = dat.word(tobj + 4)
                        materials.append((render & 3, colormaps, dat.word(mat + 4)))
                        dobj = dat.word(dobj + 4)
                walk(dat.word(joint + 8))
                joint = dat.word(joint + 0xC)

        walk(dat.word(model))
        self.assertEqual(len(materials), 10)
        self.assertTrue(all(render == 1 for render, _, _ in materials))  # RENDER_CONSTANT: the material's color.
        digits = [m for m in materials if m[1] == [4]]  # TEX_COLORMAP_MODULATE
        self.assertEqual([m[2] for m in digits], [WHITE] * 8)
        self.assertEqual(sorted(m[2] for m in materials if m not in digits), [GRAY, GRAY])
        # Its material animations only change textures (the digits), never a color.
        matanims = dat.word(dat.word(model + 8))
        stack = [matanims]
        while stack:
            joint = stack.pop()
            if not joint:
                continue
            stack += [dat.word(joint), dat.word(joint + 4)]
            anim = dat.word(joint + 8)
            while anim:
                self.assertEqual(dat.word(anim + 4), 0)  # No material color AObj.
                anim = dat.word(anim)

    def test_the_game_keeps_the_timers_gobj_where_the_overlay_reads_it(self):
        m = NativeMachine()
        word = lambda at: struct.unpack(">I", m.cpu.mem_read(at, 4))[0]
        # HUD_Timer_Initialize: r31 = 0x804A1078, stc_matchhud (0x804A0FD8) + 0xA0,
        # and it stores the timer GObj (r29) there, or 0 with no timer.
        self.assertEqual((word(0x802F4484), word(0x802F4488)), (0x3C60804A, 0x3BE31078))
        self.assertEqual(word(0x802F47F4), 0x93BF0000)  # stw r29, 0(r31)
        self.assertEqual(word(0x802F44AC), 0x901F0000)  # stw r0, 0(r31): no timer this match.
        self.assertEqual(0x804A1078 - 0x804A0FD8, 0xA0)


if __name__ == "__main__":
    unittest.main()
