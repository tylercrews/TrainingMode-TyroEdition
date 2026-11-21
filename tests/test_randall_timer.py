"""The Randall Timer: a map of Randall's loop through Yoshi's Story's match timer.

python tests/test_randall_timer.py -v
Builds tests/randall_timer_probe.c, which includes the production
src/stage_overlay_game.c, and runs it as PowerPC code. The stage, Randall's
cloud joint, the settings and the timer's joints are fixtures, and the GX FIFO
writes are recorded, so this checks what the overlay asks the GPU to draw, not
the picture. With melee.iso at the repo root, it also checks the research the
overlay rests on: Randall's path and the stage walls in GrSt.dat, the timer's
joints and the HUD camera in IfAll.usd, and where the game keeps the timer.
It drives the map through a whole lap of Randall's real path, frame by frame.
"""
from pathlib import Path
import math
import struct
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "build/test-deps"))
from unicorn import UC_HOOK_MEM_WRITE
from test_score_storage import NativeMachine

ELF = ROOT / "build/randall-timer-test.elf"
FIFO = 0xCC008000
GX_TRIANGLES, GX_POINTS = 0x90, 0xB8
FLAGS, EXTERNAL, INTERNAL, MAP, TIMER, MAP_INDEX, CLOUD, TIMER_FLAGS = range(8)
LEFT, RIGHT = -1, 1
# IfAll's ScInfTim: the root at ScInfDmg's joint 13, and its ten joints in
# ifTime's order (hour and minute separators, hours, minutes, centiseconds,
# seconds). The hours and their separator are hidden until an hour.
VANILLA_TIMER = (0.0, 17.65)
VANILLA_DIGITS = [(-7.7, 0.0), (-1.3, 0.0), (-12.2, 0.0), (-9.6, 0.0), (-5.8, 0.0),
                  (-3.2, 0.0), (6.1, -0.3), (8.05, -0.3), (0.6, 0.0), (3.2, 0.0)]
HOURS = (0, 2, 3)


def build():
    compiler = Path("C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe")
    subprocess.run([
        str(compiler) if compiler.exists() else "powerpc-eabi-gcc", "-O2", "-w",
        "-ffreestanding", "-fno-builtin", "-msdata=none", "-mcpu=750", "-nostdlib",
        "-Wl,-Ttext=0x80600000,-e,TestInit,--unresolved-symbols=ignore-all",
        "tests/randall_timer_probe.c", "-o", str(ELF)
    ], cwd=ROOT, check=True)


def bits(value):
    return struct.unpack(">I", struct.pack(">f", value))[0]


def f32(word):
    return struct.unpack(">f", struct.pack(">I", word))[0]


def premultiply(rgba, alpha):
    r, g, b = rgba >> 24, rgba >> 16 & 255, rgba >> 8 & 255
    return (r * alpha // 255, g * alpha // 255, b * alpha // 255, alpha)


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
        self.cpu.mem_map(0xCC000000, 0x10000)
        self.fifo = []
        self.cpu.hook_add(UC_HOOK_MEM_WRITE, self.write, begin=FIFO, end=FIFO + 3)
        self.fn("TestInit")
        self.timer(*VANILLA_TIMER)
        self.out, self.top, self.bottom, self.line, self.spacing, self.length, self.edge = (
            f32(self.fn("TestConstant", i)) for i in range(7))
        self.halves = [f32(self.fn("TestHalfWidth", i)) for i in range(10)]

    def write(self, uc, access, address, size, value, data):
        self.fifo.append((size, value & 0xFFFFFFFF))

    def fn(self, name, *args):
        return self.call(self.symbols[name], *args)

    def info(self, what):
        return self.fn("TestInfo", what)

    def timer(self, x, y, digits=VANILLA_DIGITS, hours=False):
        self.fn("TestTimer", bits(x), bits(y))
        for i, (dx, dy) in enumerate(digits):
            self.fn("TestDigit", i, bits(dx), bits(dy), int(i in HOURS and not hours))

    def cloud(self, x, y):
        self.fn("TestCloud", bits(x), bits(y))

    def draws(self, entry="TestDraw", *args):
        """Each GXBegin's (shape, point size, depth compare, depth update, vertices)."""
        self.fifo = []
        self.fn(entry, *args)
        stream, out = list(self.fifo), []
        for i in range(self.fn("TestBegins")):
            shape, count, size, compare, update = (self.fn("TestBegin", i, w) for w in range(5))
            vertices = []
            for _ in range(count):
                (s0, x), (s1, y), (s2, z) = stream[:3]
                rgba = tuple(value for _, value in stream[3:7])
                assert (s0, s1, s2) == (4, 4, 4) and all(s == 1 for s, _ in stream[3:7])
                vertices.append((f32(x), f32(y), f32(z), rgba))
                stream = stream[7:]
            out.append((shape, size, compare, update, vertices))
        assert not stream, "every vertex belongs to a GXBegin"
        return out

    def draw(self, x, y, render_pass=0):
        self.cloud(x, y)
        return self.draws("TestDraw", render_pass)

    def arrow(self, x, y):
        """The fill's center, direction and color, and the outline's color."""
        self.cloud(x, y)
        draws = self.draws("TestArrow")
        return arrow_of(draws, self.length)


def arrow_of(draws, length):
    (_, _, _, _, edge), (_, _, _, _, fill) = draws[-2:]
    tip = fill[0][:2]
    tail = ((fill[3][0] + fill[5][0]) / 2, (fill[3][1] + fill[5][1]) / 2)
    center = ((tip[0] + tail[0]) / 2, (tip[1] + tail[1]) / 2)
    direction = ((tip[0] - tail[0]) / length, (tip[1] - tail[1]) / length)
    return center, direction, fill[0][3], edge[0][3]


def timer_edges(m, timer=VANILLA_TIMER, digits=VANILLA_DIGITS, hours=False):
    shown = [i for i in range(10) if hours or i not in HOURS]
    left = min(timer[0] + digits[i][0] - m.halves[i] for i in shown)
    right = max(timer[0] + digits[i][0] + m.halves[i] for i in shown)
    return left, right


def mapping(m, timer=VANILLA_TIMER, **kw):
    """Stage units to the HUD camera's, as the overlay should map them."""
    left, right = timer_edges(m, timer, **kw)
    cx, sx = (left + right) / 2, (right - left) / 2 / m.out
    sy, mid = m.line / ((m.top - m.bottom) / 2), (m.top + m.bottom) / 2
    return lambda x, y: (cx + x * sx, timer[1] + (y - mid) * sy)


def path(m):
    return [(f32(m.fn("TestPath", i, 0)), f32(m.fn("TestPath", i, 1))) for i in range(m.info(5))]


def off_track(point, polyline):
    """How far a point is from a polyline."""
    best = math.inf
    for a, b in zip(polyline, polyline[1:]):
        dx, dy = b[0] - a[0], b[1] - a[1]
        t = max(0, min(1, ((point[0] - a[0]) * dx + (point[1] - a[1]) * dy) / (dx * dx + dy * dy)))
        best = min(best, math.dist(point, (a[0] + dx * t, a[1] + dy * t)))
    return best


class RandallTimerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build()

    def setUp(self):
        self.m = Machine()
        self.colors = [self.m.fn("TestColor", i) for i in range(8)]

    def assertNear(self, a, b, places=4):
        for x, y in zip(a, b):
            self.assertAlmostEqual(x, y, places=places)

    def test_it_draws_dots_under_an_outlined_arrow_with_depth_off(self):
        m = self.m
        draws = m.draw(0, m.bottom)
        self.assertEqual([d[0] for d in draws], [GX_POINTS, GX_POINTS, GX_TRIANGLES, GX_TRIANGLES])
        self.assertEqual([d[1] for d in draws[:2]], [m.info(9), m.info(8)])  # A dark dot under each light one.
        self.assertEqual((m.info(9), m.info(8)), (24, 12))  # 4 and 2 pixels.
        self.assertEqual(len(draws[0][4]), len(draws[1][4]))
        self.assertEqual([len(d[4]) for d in draws[2:]], [9, 9])  # Head and shaft, outline then fill.
        self.assertTrue(all(d[2] == 0 and d[3] == 0 for d in draws))  # No depth test or write.
        self.assertTrue(all(v[2] == 0 for d in draws for v in d[4]))  # The timer's plane.

    def test_the_timer_is_the_stage_between_its_walls(self):
        m = self.m
        left, right = timer_edges(m)
        self.assertAlmostEqual(left, -5.8 - 1.30078125, places=5)  # Minutes' tens...
        self.assertAlmostEqual(right, 8.05 + 0.98828125, places=5)  # ...to the small centiseconds.
        to_hud = mapping(m)
        for x, y in ((-m.out, m.bottom), (m.out, m.bottom), (m.out, m.top), (-m.out, m.top), (0, m.bottom),
                     (97.475, -28.7), (-97.475, -28.7), (-95.725, m.bottom), (30, m.top)):
            with self.subTest(x=x, y=y):
                center, *_ = m.arrow(x, y)
                self.assertNear(center, to_hud(x, y))
        self.assertAlmostEqual(m.arrow(-m.out, m.bottom)[0][0], left, places=4)
        self.assertAlmostEqual(m.arrow(m.out, m.top)[0][0], right, places=4)

    def test_both_lines_run_through_the_numbers(self):
        m = self.m
        bottom = m.arrow(0, m.bottom)[0][1]
        top = m.arrow(0, m.top)[0][1]
        self.assertAlmostEqual(bottom, VANILLA_TIMER[1] - m.line, places=4)
        self.assertAlmostEqual(top, VANILLA_TIMER[1] + m.line, places=4)
        self.assertLess(m.line, 1.30078125)  # Inside the big digits' height.

    def test_the_arrow_points_the_way_randall_moves(self):
        m = self.m
        for (x, y), want in (((0, m.bottom), (1, 0)), ((-90, m.bottom), (1, 0)), ((0, m.top), (-1, 0)),
                             ((90, m.top), (-1, 0)), ((97.475, -28.7), (0, 1)), ((-97.475, -28.7), (0, -1))):
            with self.subTest(x=x, y=y):
                _, direction, *_ = m.arrow(x, y)
                self.assertNear(direction, want, places=4)
        # Rounding the bottom-right corner: right and up at once.
        _, (dx, dy), *_ = m.arrow(97.2, -37.4)
        self.assertGreater(dx, 0.05)
        self.assertGreater(dy, 0.05)
        self.assertAlmostEqual(math.hypot(dx, dy), 1, places=4)

    def test_the_arrow_is_faint_behind_the_timer_and_solid_past_it(self):
        m, c = self.m, self.colors
        solid = (premultiply(c[0], c[0] & 255), premultiply(c[1], c[1] & 255))
        faint = (premultiply(c[0], c[4]), premultiply(c[1], c[5]))
        self.assertEqual(solid[0], (235, 40, 40, 255))  # Red.
        self.assertLess(faint[0][3], solid[0][3] // 2)  # Much more see-through, but there.
        self.assertGreater(faint[0][3], 0)
        for x, want in ((-m.out - 0.01, solid), (-m.out + 0.01, faint), (0, faint), (m.out - 0.01, faint),
                        (m.out + 0.01, solid), (-97.475, solid), (97.475, solid)):
            with self.subTest(x=x):
                _, _, fill, edge = m.arrow(x, m.bottom if abs(x) < 97 else -28.7)
                self.assertEqual((fill, edge), want)

    def test_the_dots_run_evenly_around_the_whole_loop(self):
        m, c = self.m, self.colors
        to_hud = mapping(m)
        loop = [to_hud(*p) for p in path(m)]
        length = sum(math.dist(a, b) for a, b in zip(loop, loop[1:]))
        edges, dots = (d[4] for d in m.draw(0, m.bottom)[:2])
        self.assertEqual(len(dots), int(length / m.spacing))
        self.assertEqual([v[:2] for v in edges], [v[:2] for v in dots])
        self.assertNear(dots[0][:2], loop[0])  # From where Randall starts.
        for dot in dots:
            self.assertLess(off_track(dot[:2], loop), 1e-3)
        gaps = [math.dist(a[:2], b[:2]) for a, b in zip(dots, dots[1:] + dots[:1])]
        # Evenly spaced along the loop; straight-line gaps only shrink across a corner.
        self.assertAlmostEqual(max(gaps), length / len(dots), places=3)
        self.assertGreater(min(gaps), 0.6 * length / len(dots))
        # Both lines, and the joins past the timer.
        ys = sorted({round(v[1], 3) for v in dots})
        self.assertAlmostEqual(ys[0], VANILLA_TIMER[1] - m.line, places=3)
        self.assertAlmostEqual(ys[-1], VANILLA_TIMER[1] + m.line, places=3)
        left, right = timer_edges(m)
        xs = [v[0] for v in dots]
        self.assertLess(min(xs), left - 0.8 * (left - to_hud(-97.475, 0)[0]))
        self.assertGreater(max(xs), right + 0.8 * (to_hud(97.475, 0)[0] - right))
        # Dimmer behind the numbers.
        for edge, dot in zip(edges, dots):
            behind = left <= dot[0] <= right
            self.assertEqual(dot[3], premultiply(c[2], c[6] if behind else c[2] & 255))
            self.assertEqual(edge[3], premultiply(c[3], c[7] if behind else c[3] & 255))
        self.assertTrue(any(left <= v[0] <= right for v in dots) and any(v[0] < left for v in dots))

    def test_an_hour_widens_the_timer_and_the_map(self):
        m = self.m
        m.timer(*VANILLA_TIMER, hours=True)
        left, right = timer_edges(m, hours=True)
        self.assertAlmostEqual(left, -12.2 - 1.30078125, places=5)  # The hours' tens.
        self.assertAlmostEqual(m.arrow(-m.out, m.bottom)[0][0], left, places=4)
        self.assertAlmostEqual(m.arrow(m.out, m.bottom)[0][0], right, places=4)

    def test_the_map_follows_the_timer(self):
        m = self.m
        m.timer(5.0, 10.0)
        to_hud = mapping(m, timer=(5.0, 10.0))
        for x, y in ((-m.out, m.bottom), (60, m.top), (97.475, -28.7)):
            self.assertNear(m.arrow(x, y)[0], to_hud(x, y))

    def test_a_huge_timer_keeps_the_dots_in_their_buffer(self):
        m = self.m
        m.timer(0.0, 0.0, digits=[(-200.0 + 40 * i, 0.0) for i in range(10)], hours=True)
        edges, dots = (d[4] for d in m.draw(0, m.bottom)[:2])
        self.assertEqual(len(dots), m.info(7))
        self.assertEqual(len(edges), m.info(7))

    def test_nothing_draws_without_everything_it_needs(self):
        cases = (
            ("off", FLAGS, 0), ("Battlefield", EXTERNAL, None), ("no map GObj (hazards off)", MAP, 0),
            ("not Randall's GObj", MAP_INDEX, 3), ("no cloud joint", CLOUD, 0), ("no timer", TIMER, 0),
            ("hidden timer", TIMER_FLAGS, None),
        )
        for name, what, value in cases:
            with self.subTest(name):
                m = Machine()
                if what == EXTERNAL:
                    m.fn("TestSet", EXTERNAL, m.info(3))
                    m.fn("TestSet", INTERNAL, 0)
                else:
                    m.fn("TestSet", what, m.info(10) if what == TIMER_FLAGS else value)
                self.assertEqual(m.draw(80, m.bottom), [])
                self.assertEqual(m.fifo, [])
        with self.subTest("no digit showing"):
            m = Machine()
            for i in range(10):
                m.fn("TestDigit", i, 0, 0, 1)
            self.assertEqual(m.draw(80, m.bottom), [])
        for render_pass in (1, 2):
            with self.subTest(render_pass=render_pass):
                m = Machine()
                self.assertEqual(m.draw(80, m.bottom, render_pass), [])  # Pass 0 only, before the numbers.
        with self.subTest("after a scene change"):
            m = Machine()
            m.fn("TestSceneChange")
            self.assertEqual(m.draw(80, m.bottom), [])
        with self.subTest("no real position"):
            m = Machine()
            m.fn("TestCloud", 0x7FC00000, bits(m.bottom))  # NaN.
            self.assertEqual(m.draws("TestDraw", 0), [])

    def test_match_start_adds_a_stage_callback_and_a_hud_one(self):
        m = self.m
        m.fn("TestSceneChange")
        m.fn("TestMatchStart")
        self.assertEqual(m.fn("TestGObjs"), 2)
        links = [tuple(m.fn("TestLink", i, w) for w in range(4)) for i in range(m.fn("TestLinks"))]
        self.assertEqual(links, [(0, 1, 5, 0), (1, 2, m.info(6), 0)])  # StageOverlay_GX, RandallTimer_GX.
        self.assertEqual(m.info(6), 8)
        self.assertEqual(m.info(11), 1)
        m.fn("TestMatchStart")  # Once per match.
        self.assertEqual(m.fn("TestGObjs"), 2)
        m.draws("TestStageGX", 2)  # The stage callback has nothing of the timer's.
        self.assertEqual(m.fn("TestBegins"), 0)

    def test_the_flag_is_the_one_the_settings_tests_save(self):
        self.assertEqual(self.m.info(0), 14)  # TM_FLAG_RANDALL_TIMER


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

    def half(self, at):
        return struct.unpack_from(">H", self.data, 32 + at)[0]

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

    def turn(x, y):
        return (tx + x * math.cos(rz) - y * math.sin(rz), ty + x * math.sin(rz) + y * math.cos(rz))

    def place(u):
        for i in range(cv_count - 1):
            if lengths[i] <= u <= lengths[i + 1]:
                t = (u - lengths[i]) / (lengths[i + 1] - lengths[i])
                return turn(cvs[i][0] + (cvs[i + 1][0] - cvs[i][0]) * t, cvs[i][1] + (cvs[i + 1][1] - cvs[i][1]) * t)
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
        "points": [tuple(scale * c for c in turn(*cv)) for cv in cvs],
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


def quad_half_width(dat, joint):
    """Half the X span of a joint's first PObj, read through its display list."""
    pobj = dat.word(dat.word(joint + 0x10) + 0xC)
    attrs, at = [], dat.word(pobj + 8)
    while dat.word(at) != 0xFF:  # GX_VA_NULL ends the list.
        attr, kind, count, comp = struct.unpack_from(">4I", dat.data, 32 + at)
        attrs.append((attr, kind, count, comp, dat.data[32 + at + 0x10], dat.half(at + 0x12), dat.word(at + 0x14)))
        at += 0x18
    raw = dat.bytes(dat.word(pobj + 0x10), dat.half(pobj + 0xE) * 32)
    count, at, xs = struct.unpack_from(">H", raw, 1)[0], 3, []
    for _ in range(count):
        for attr, kind, comps, comp, frac, stride, data in attrs:
            assert kind == 2, "GX_INDEX8"
            if attr == 9:  # GX_VA_POS, s16 fixed point
                assert comp == 3
                x = struct.unpack_from(">h", dat.data, 32 + data + raw[at] * stride)[0] / (1 << frac)
                xs.append(x)
            at += 1
    return (max(xs) - min(xs)) / 2


@unittest.skipUnless((ROOT / "melee.iso").exists(), "needs a vanilla melee.iso at the repo root")
class IsoTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.path = randall_path()
        build()
        m = Machine()
        cls.out = m.out

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

    def test_the_overlays_path_is_randalls_spline(self):
        m = Machine()
        table = path(m)
        self.assertEqual(len(table), len(self.path["points"]))
        for mine, real in zip(table, self.path["points"]):
            for a, b in zip(mine, real):
                self.assertAlmostEqual(a, b, places=4)
        self.assertEqual(table[0], table[-1])  # A closed loop...
        self.assertAlmostEqual(table[0][0], self.path["frames"][0][0], places=3)  # ...from where frame 0 is.
        ys = {round(y, 3) for _, y in table}
        self.assertEqual((min(ys), max(ys)), (round(m.bottom, 3), round(m.top, 3)))

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

    def test_a_lap_on_the_map_takes_as_long_behind_the_timer_as_behind_the_stage(self):
        m, frames = Machine(), self.path["frames"]
        to_hud = mapping(m)
        loop = [to_hud(*p) for p in path(m)]
        left, right = timer_edges(m)
        faint = premultiply(m.fn("TestColor", 0), m.fn("TestColor", 4))
        arrows = [m.arrow(x, y) for x, y in frames]
        where = []
        for frame, ((cx, cy), direction, fill, _) in enumerate(arrows):
            with self.subTest(frame=frame):
                self.assertLess(off_track((cx, cy), loop), 1e-3)  # On the track the dots draw.
                nxt = to_hud(*frames[(frame + 1) % len(frames)])
                step = (nxt[0] - cx, nxt[1] - cy)
                # It points the way Randall goes next, within 60 degrees: on a
                # corner it turns one short segment at a time.
                self.assertGreater(direction[0] * step[0] + direction[1] * step[1], 0.5 * math.hypot(*step))
                inside = left <= cx <= right
                self.assertEqual(fill == faint, inside)
                where.append(0 if inside else LEFT if cx < left else RIGHT)
        stage = [0 if abs(x) <= self.out else LEFT if x < 0 else RIGHT for x, _ in frames]
        self.assertEqual(where, stage)  # Frame for frame.
        self.assertEqual(stage.count(0), 2 * 297)
        self.assertEqual((stage.count(LEFT), stage.count(RIGHT)), (303, 303))
        # Frame 0: Randall's start, the bottom-left corner, heading right toward the stage.
        (cx, cy), direction, *_ = arrows[0]
        for a, b in zip((cx, cy), to_hud(*frames[0])):
            self.assertAlmostEqual(a, b, places=4)
        self.assertAlmostEqual(direction[0], 1, places=4)

    def test_the_timer_joints_are_where_the_overlay_measures_them(self):
        m = Machine()
        dat = Dat("IfAll.usd")
        root = dat.word(dat.word(dat.roots["ScInfTim_scene_models"]))
        self.assertEqual(dat.floats(root + 0x2C, 3), (0.0, 0.0, 0.0))
        joint, joints = dat.word(root + 8), []
        while joint:
            joints.append(joint)
            joint = dat.word(joint + 0xC)
        self.assertEqual(len(joints), m.info(4))
        for i, joint in enumerate(joints):
            with self.subTest(joint=i):
                x, y, _ = dat.floats(joint + 0x2C, 3)
                self.assertAlmostEqual(x, VANILLA_DIGITS[i][0], places=4)
                self.assertAlmostEqual(y, VANILLA_DIGITS[i][1], places=4)
                self.assertEqual(dat.floats(joint + 0x20, 3), (1.0, 1.0, 1.0))
                self.assertEqual(quad_half_width(dat, joint), m.halves[i])
        # The root goes where ScInfDmg's joint 13 is (ifAll_802F370C, ifTime_CreateTimers).
        scene = dat.word(dat.word(dat.roots["ScInfDmg_scene_data"]))
        order = []

        def walk(j):
            while j:
                order.append(j)
                walk(dat.word(j + 8))
                j = dat.word(j + 0xC)

        walk(dat.word(scene))
        self.assertAlmostEqual(dat.floats(order[13] + 0x2C, 2)[1], VANILLA_TIMER[1], places=4)
        self.assertEqual(dat.floats(order[13] + 0x2C, 1)[0], VANILLA_TIMER[0])

    def test_the_hud_camera_shows_the_whole_map(self):
        m = Machine()
        dat = Dat("IfAll.usd")
        cam = dat.word(dat.word(dat.roots["ScInfDmg_scene_data"] + 4))
        self.assertEqual(dat.half(cam + 6), 1)  # Perspective.
        eye, interest = dat.word(cam + 0x18), dat.word(cam + 0x1C)
        self.assertEqual((dat.floats(eye + 4, 3), dat.floats(interest + 4, 3)), ((0, 0, 64.0), (0, 0, 0)))
        fov, aspect = dat.floats(cam + 0x30, 2)
        high = 64 * math.tan(math.radians(fov) / 2)
        wide = high * aspect
        self.assertAlmostEqual(high, 24.27, places=2)
        for hours in (False, True):
            with self.subTest(hours=hours):
                m.timer(*VANILLA_TIMER, hours=hours)
                points = [v[:2] for d in m.draw(97.475, -28.7) for v in d[4]]
                points += [v[:2] for d in m.draw(-97.475, -28.7) for v in d[4]]
                self.assertLess(max(abs(x) for x, _ in points), wide - 2)
                self.assertLess(max(y for _, y in points), high - 2)

    def test_the_game_keeps_the_timers_gobj_where_the_overlay_reads_it(self):
        m = NativeMachine()
        word = lambda at: struct.unpack(">I", m.cpu.mem_read(at, 4))[0]
        # HUD_Timer_Initialize: r31 = 0x804A1078, stc_matchhud (0x804A0FD8) + 0xA0,
        # and it stores the timer GObj (r29) there, or 0 with no timer.
        self.assertEqual((word(0x802F4484), word(0x802F4488)), (0x3C60804A, 0x3BE31078))
        self.assertEqual(word(0x802F47F4), 0x93BF0000)  # stw r29, 0(r31)
        self.assertEqual(word(0x802F44AC), 0x901F0000)  # stw r0, 0(r31): no timer this match.
        self.assertEqual(0x804A1078 - 0x804A0FD8, 0xA0)

    def test_the_hud_camera_draws_link_8_and_the_timer_is_in_link_11(self):
        m = NativeMachine()
        word = lambda at: struct.unpack(">I", m.cpu.mem_read(at, 4))[0]
        # InitHUD's camera: gxlink_prios = 0xD00, links 8, 10 (its light) and 11.
        self.assertEqual((word(0x802F39AC), word(0x802F39B0)), (0x38000D00, 0x901D0024))
        # HUD_Timer_Initialize: GObj_AddGXLink(timer, HSD_GObj_JObjCallback, 11, 0).
        self.assertEqual((word(0x802F45F8), word(0x802F45FC)), (0x38A0000B, 0x38C00000))
        self.assertEqual(word(0x802F4600) >> 26, 18)  # bl
        self.assertEqual((0x802F4600 + (word(0x802F4600) & 0x03FFFFFC)) & 0xFFFFFFFF, 0x8039069C)


if __name__ == "__main__":
    unittest.main()
