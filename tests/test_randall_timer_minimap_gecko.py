"""Execute the Randall Timer Minimap Gecko code in a PowerPC emulator.

python tests/test_randall_timer_minimap_gecko.py -v
Requires Unicorn (the existing build/test-deps install is supported). The
native functions the code calls are stubbed; the timer, Randall's map GObj and
joints and the match clock are fixtures in memory; and the GX FIFO writes are
recorded, so this checks what the code asks the GPU to draw, not the picture.
With devkitPPC and build/Start.dol, it also builds Tyro's own Randall Timer
Minimap (tests/randall_timer_probe.c, which includes src/stage_overlay_game.c)
and checks the code draws the same thing, and with melee.iso, frame for frame
through Randall's real lap.
"""
from pathlib import Path
from types import SimpleNamespace
import math
import re
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "build/test-deps"))
from unicorn import (Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE,
                     UC_HOOK_MEM_WRITE)
from unicorn import ppc_const as reg
import test_randall_timer as tyro
from test_randall_timer import VANILLA_TIMER, VANILLA_DIGITS, HOURS, along, off_track, arrow_of

TEXT = ROOT / "slippi-gecko-codes/randall-timer-minimap/randall-timer-minimap.txt"
SOURCE = ROOT / "src/stage_overlay_game.c"
HOOK, ALLOCATION, GOBJ = 0x8016E750, 0x81700000, 0x80E00000
STOP, R13, FIFO = 0x80004000, 0x804DB6A0, 0xCC008000
TIMER_HUD, MATCH = 0x804A1078, 0x8046B6A0  # stc_matchhud->timer_gobj, stc_match
STORY_EXTERNAL, STORY_INTERNAL, BATTLEFIELD_EXTERNAL, BATTLEFIELD_INTERNAL = 8, 10, 31, 24
GX_TRIANGLES, GX_POINTS = 0x90, 0xB8
HIDDEN = 0x10  # JOBJ_HIDDEN
NATIVES = {
    0x803901F0: 'GObj_Create', 0x8039069C: 'GObj_AddGXLink',
    0x80225194: 'Stage_GetExternalID', 0x8022519C: 'Stage_ExternalToInternal',
    0x801C2BA4: 'Stage_GetMapGObj', 0x8000B1CC: 'JOBJ_GetWorldPosition',
    0x8036C244: 'HSD_ClearVtxDesc', 0x80341510: 'GXSetCurrentMtx', 0x8036A288: 'COBJ_GetCurrent',
    0x803695F0: 'COBJ_GetViewingMtx', 0x80341494: 'GXLoadPosMtxImm',
    0x80361A74: 'HSD_StateSetLineWidth', 0x80361C60: 'HSD_StateSetPointSize',
    0x8036199C: 'HSD_SetupRenderMode', 0x80361BB8: 'HSD_StateSetZMode',
    0x8033C414: 'GXSetVtxAttrFmt', 0x8033BF00: 'GXSetVtxDesc',
    0x80361AD8: 'HSD_StateSetCullMode', 0x8033D0DC: 'GXBegin',
}
CAMERA = 0x80D00000
# Fixtures. As in the game, the map GObj's JObj is a scale joint over GrSt.dat's
# root, whose first child is the cloud and whose second is the spline.
MAP, MAP_DATA, SCALE, FILE_ROOT, CLOUD, SPLINE = (0x80F00000 + i * 0x1000 for i in range(6))
TIMER_GOBJ, TIMER_ROOT, DIGIT0 = 0x80E10000, 0x80E11000, 0x80E12000


def gpr(n):
    return getattr(reg, f"UC_PPC_REG_{n}")


def fpr(n):
    return getattr(reg, f"UC_PPC_REG_FPR{n}")


def f32(value):
    return struct.unpack(">f", struct.pack(">f", value))[0]


def code():
    """The C2 code's address and instructions, from the distributed text."""
    words = []
    for line in TEXT.read_text().splitlines():
        if line and line[0] not in '$*':
            words += [int(word, 16) for word in line.split()]
    assert words[0] >> 24 == 0xC2 and words[1] * 2 == len(words) - 2, 'one C2 code'
    return 0x80000000 | (words[0] & 0x1FFFFFF), words[2:]


def source_numbers():
    """The C version's numbers, from src/stage_overlay_game.c."""
    text = SOURCE.read_text()

    def define(name):
        return re.search(rf"#define RANDALL_TIMER_{name} \(?(-?[0-9.]+)f?\)?", text).group(1)

    def color(name):
        return tuple(int(c) for c in re.search(rf"#define RANDALL_TIMER_{name} \{{([0-9, ]+)\}}", text).group(1).split(","))

    path = re.search(r"randall_path\[RANDALL_PATH_POINTS\] = \{(.*?)\};", text, re.S).group(1)
    halves = {"SEPARATOR": None, "DIGIT": None, "SMALL": None}
    for name in halves:
        halves[name] = float(re.search(rf"#define TIMER_{name}_HALF ([0-9.]+)f", text).group(1))
    widths = re.search(r"timer_half_widths\[RANDALL_TIMER_DIGITS\] = \{(.*?)\};", text, re.S).group(1)
    return SimpleNamespace(
        out=f32(float(define("OUT_X"))), top=f32(float(define("TOP_Y"))), bottom=f32(float(define("BOTTOM_Y"))),
        line=f32(float(define("LINE_Y"))), spacing=f32(float(define("DOT_SPACING"))),
        length=f32(float(define("ARROW_LENGTH"))), edge=f32(float(define("ARROW_EDGE"))),
        gxlink=int(define("GXLINK")), dot_size=int(define("DOT_SIZE")), dot_edge=int(define("DOT_EDGE")),
        dots_max=int(define("DOTS_MAX")),
        colors=[color(name) for name in ("ARROW_COLOR", "ARROW_EDGE_COLOR", "DOT_COLOR", "DOT_EDGE_COLOR")],
        behind=[int(define(name)) for name in ("ARROW_BEHIND", "ARROW_EDGE_BEHIND", "DOT_BEHIND", "DOT_EDGE_BEHIND")],
        path=[tuple(f32(float(v)) for v in p) for p in re.findall(r"\{(-?[0-9.]+)f, (-?[0-9.]+)f\}", path)],
        halves=[f32(halves[w.split("_")[1]]) for w in re.findall(r"TIMER_\w+_HALF", widths)],
    )


N = source_numbers()


def shade(index, behind):
    """A color, premultiplied, at its alpha past the timer or behind it."""
    (r, g, b, a), alpha = N.colors[index], N.behind[index] if behind else N.colors[index][3]
    return (r * alpha // 255, g * alpha // 255, b * alpha // 255, alpha)


class Machine:
    def __init__(self):
        self.cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
        self.cpu.mem_map(0x80000000, 0x1800000)
        self.cpu.mem_map(0xCC000000, 0x10000)
        self.cpu.reg_write(reg.UC_PPC_REG_MSR, 1 << 13)  # Floating point on.
        self.cpu.reg_write(gpr(1), 0x817F0000)
        self.cpu.reg_write(gpr(13), R13)
        hook, words = code()
        assert hook == HOOK
        # Install like the code handler: branch in, and the final word back out.
        end = ALLOCATION + 4 * (len(words) - 1)
        words[-1] = 0x48000000 | ((hook + 4 - end) & 0x3FFFFFC)
        self.cpu.mem_write(ALLOCATION, struct.pack(f">{len(words)}I", *words))
        self.cpu.mem_write(hook, struct.pack(">I", 0x48000000 | ((ALLOCATION - hook) & 0x3FFFFFC)))
        self.created, self.callback = GOBJ, None
        self.external, self.internal, self.map_present = STORY_EXTERNAL, STORY_INTERNAL, True
        self.calls, self.fifo, self.begins = [], [], []
        self.state = {'size': None, 'width': None, 'z': None}
        self.build_map()
        self.timer(*VANILLA_TIMER)
        self.word(TIMER_HUD, TIMER_GOBJ)
        self.cloud(0.0, N.bottom)
        self.cpu.hook_add(UC_HOOK_CODE, self.native, begin=0x80000000, end=0x80400000)
        self.cpu.hook_add(UC_HOOK_MEM_WRITE, self.write, begin=FIFO, end=FIFO + 3)

    def word(self, at, value=None):
        if value is None:
            return struct.unpack(">I", self.cpu.mem_read(at, 4))[0]
        self.cpu.mem_write(at, struct.pack(">I", value & 0xFFFFFFFF))

    def joint(self, at, parent=0, child=0, sibling=0, flags=0, trans=(0.0, 0.0)):
        self.cpu.mem_write(at, bytes(0x80))
        self.word(at + 0x08, sibling)
        self.word(at + 0x0C, parent)
        self.word(at + 0x10, child)
        self.word(at + 0x14, flags)
        self.cpu.mem_write(at + 0x38, struct.pack(">3f", trans[0], trans[1], 0.0))

    def build_map(self):
        self.cpu.mem_write(MAP, bytes(0x100))
        self.cpu.mem_write(MAP_DATA, bytes(0x100))
        self.word(MAP + 0x28, SCALE)
        self.word(MAP + 0x2C, MAP_DATA)
        self.word(MAP_DATA + 0x14, 2)
        self.joint(SCALE, child=FILE_ROOT)
        self.joint(FILE_ROOT, parent=SCALE, child=CLOUD)
        self.joint(CLOUD, parent=FILE_ROOT, sibling=SPLINE)
        self.joint(SPLINE, parent=FILE_ROOT, flags=0x4000)  # JOBJ_SPLINE

    def digit(self, i):
        return DIGIT0 + i * 0x100

    def timer(self, x, y, digits=VANILLA_DIGITS, hours=False):
        """A timer like IfAll's ScInfTim: a root with ten joints, in ifTime's order."""
        self.cpu.mem_write(TIMER_GOBJ, bytes(0x40))
        self.word(TIMER_GOBJ + 0x28, TIMER_ROOT)
        self.joint(TIMER_ROOT, child=self.digit(0), trans=(x, y))
        for i, at in enumerate(digits):
            self.joint(self.digit(i), parent=TIMER_ROOT, sibling=self.digit(i + 1) if i < 9 else 0,
                       flags=HIDDEN if i in HOURS and not hours else 0, trans=at)

    def stage(self, external, internal):
        self.external, self.internal = external, internal

    def cloud(self, x, y):
        self.cpu.mem_write(CLOUD + 0x38, struct.pack(">2f", x, y))

    def time(self, mode, seconds, ms, start=(480, 0)):
        """The match timer's mode and clock, and the time a countdown starts from."""
        self.cpu.mem_write(MATCH + 0x24C8, bytes([mode]))
        self.word(MATCH + 0x28, seconds)
        self.cpu.mem_write(MATCH + 0x2C, struct.pack(">H", ms))
        self.word(MATCH + 0x24D8, start[0])
        self.cpu.mem_write(MATCH + 0x24DC, bytes([start[1]]))

    def world(self, joint):
        """Translation-only joints: the sum up their parents, as the C probe has them."""
        x = y = z = 0.0
        while joint:
            tx, ty, tz = struct.unpack(">3f", self.cpu.mem_read(joint + 0x38, 12))
            x, y, z = f32(x + tx), f32(y + ty), f32(z + tz)
            joint = self.word(joint + 0x0C)
        return x, y, z

    def native(self, cpu, address, size, _):
        name = NATIVES.get(address)
        if not name:
            return
        args = tuple(cpu.reg_read(gpr(n)) for n in range(3, 8))
        self.calls.append((name, args))
        result = None
        if name == 'GObj_Create':
            result = self.created
        elif name == 'Stage_GetExternalID':
            result = self.external
        elif name == 'Stage_ExternalToInternal':
            result = self.internal if args[0] == self.external else 0xFFFFFFFF
        elif name == 'Stage_GetMapGObj':
            result = MAP if args[0] == 2 and self.map_present else 0
        elif name == 'JOBJ_GetWorldPosition':
            assert args[1] == 0, 'no offset'
            cpu.mem_write(args[2], struct.pack(">3f", *self.world(args[0])))
        elif name == 'COBJ_GetCurrent':
            result = CAMERA
        elif name == 'HSD_StateSetPointSize':
            self.state['size'] = args[0] & 0xFF
        elif name == 'HSD_StateSetLineWidth':
            self.state['width'] = args[0] & 0xFF
        elif name == 'HSD_StateSetZMode':
            self.state['z'] = args[:3]
        elif name == 'GXBegin':
            assert self.state['size'] == self.state['width'], 'line width and point size together'
            self.begins.append((args[0], args[2] & 0xFFFF, self.state['size'], *self.state['z'][::2], args[1]))
        if result is not None:
            cpu.reg_write(gpr(3), result)
        # Clobber the volatile registers, cr0 and the float scratch registers
        # the way a real call may.
        for n in (0, 4, 5, 6, 7, 8, 9, 10, 11, 12) + (() if result is not None else (3,)):
            cpu.reg_write(gpr(n), 0xDEAD0000 | n)
        for n in range(14):
            cpu.reg_write(fpr(n), 0x7FF8DEAD0000DEAD)
        cpu.reg_write(reg.UC_PPC_REG_CR0, 0)
        cpu.reg_write(reg.UC_PPC_REG_PC, cpu.reg_read(reg.UC_PPC_REG_LR))

    def write(self, cpu, access, address, size, value, _):
        self.fifo.append((size, value & 0xFFFFFFFF))

    def named(self, name):
        return [args for called, args in self.calls if called == name]

    def start_match(self):
        """Run StartMelee's hooked instruction. Returns the expected registers."""
        registers = {n: 0x80100000 + n * 0x100 for n in range(0, 32) if n not in (1, 2, 13)}
        for n, value in registers.items():
            self.cpu.reg_write(gpr(n), value)
        self.cpu.reg_write(reg.UC_PPC_REG_CR, 0x24680000)
        self.cpu.reg_write(reg.UC_PPC_REG_LR, 0x8016E74C)
        sp = self.cpu.reg_read(gpr(1))
        self.calls = []
        self.cpu.emu_start(HOOK, HOOK + 4)
        links = self.named('GObj_AddGXLink')
        if links:
            self.callback = links[0][1]
        registers[4] = 0x80170000  # Its replaced instruction, lis r4,0x8017.
        assert self.cpu.reg_read(gpr(1)) == sp, 'stack restored'
        return registers

    def draws(self, render_pass=0):
        """Each GXBegin's (shape, point size, depth compare, depth update, vertices), as
        test_randall_timer's Machine.draws reads the C version's."""
        if self.callback is None:
            self.start_match()
        saved = {n: 0x81000000 + n for n in range(14, 32)}
        for n, value in saved.items():
            self.cpu.reg_write(gpr(n), value)
        saved_f = {n: 0x4000000000000000 + n for n in range(14, 32)}
        for n, value in saved_f.items():
            self.cpu.reg_write(fpr(n), value)
        sp = self.cpu.reg_read(gpr(1))
        self.calls, self.fifo, self.begins = [], [], []
        self.cpu.reg_write(gpr(3), GOBJ)
        self.cpu.reg_write(gpr(4), render_pass)
        self.cpu.reg_write(reg.UC_PPC_REG_LR, STOP)
        self.cpu.emu_start(self.callback, STOP, count=2000000)
        assert self.cpu.reg_read(reg.UC_PPC_REG_PC) == STOP, 'returned'
        assert self.cpu.reg_read(gpr(1)) == sp, 'stack restored'
        for n, value in saved.items():
            assert self.cpu.reg_read(gpr(n)) == value, f'r{n} preserved'
        for n, value in saved_f.items():
            assert self.cpu.reg_read(fpr(n)) == value, f'f{n} preserved'
        stream = b''.join(value.to_bytes(size, 'big') for size, value in self.fifo)
        out = []
        for shape, count, point, compare, update, fmt in self.begins:
            assert fmt == 0, 'GX_VTXFMT0'
            vertices = []
            for _ in range(count):
                x, y, z = struct.unpack_from(">3f", stream)
                vertices.append((x, y, z, tuple(stream[12:16])))
                stream = stream[16:]
            out.append((shape, point, compare, update, vertices))
        assert not stream, 'every vertex belongs to a GXBegin'
        return out

    def draw(self, x, y, render_pass=0):
        self.cloud(x, y)
        return self.draws(render_pass)

    def arrow(self, x, y):
        """The fill's center, direction and color, and the outline's color."""
        return arrow_of(self.draw(x, y), N.length)


def mapping(timer=VANILLA_TIMER, **kw):
    return tyro.mapping(N, timer, **kw)


def timer_edges(**kw):
    return tyro.timer_edges(N, **kw)


class HookTests(unittest.TestCase):
    def test_hook_adds_the_map_to_the_hud_camera_and_restores_every_register(self):
        m = Machine()
        registers = m.start_match()
        self.assertEqual([args[:3] for args in m.named('GObj_Create')], [(0, 7, 0)])
        (gobj, callback, link, priority, _), = m.named('GObj_AddGXLink')
        self.assertEqual((gobj, link, priority), (GOBJ, 8, 0))
        self.assertEqual(link, N.gxlink)  # The C version's link: the HUD camera's, under the timer.
        self.assertTrue(ALLOCATION <= callback < ALLOCATION + 0x1000)
        for n, value in registers.items():
            self.assertEqual(m.cpu.reg_read(gpr(n)), value, f'r{n}')
        self.assertEqual(m.cpu.reg_read(reg.UC_PPC_REG_LR), 0x8016E74C)
        self.assertEqual(m.cpu.reg_read(reg.UC_PPC_REG_CR), 0x24680000)

    def test_failed_gobj_creation_skips_the_link(self):
        m = Machine()
        m.created = 0
        registers = m.start_match()
        self.assertEqual(m.named('GObj_AddGXLink'), [])
        for n, value in registers.items():
            self.assertEqual(m.cpu.reg_read(gpr(n)), value, f'r{n}')


class DrawTests(unittest.TestCase):
    def setUp(self):
        self.m = Machine()
        self.m.start_match()

    def assertNear(self, a, b, places=4):
        for x, y in zip(a, b):
            self.assertAlmostEqual(x, y, places=places)

    def test_it_draws_dots_under_an_outlined_arrow_with_depth_off(self):
        m = self.m
        draws = m.draw(0, N.bottom)
        self.assertEqual([d[0] for d in draws], [GX_POINTS, GX_POINTS, GX_TRIANGLES, GX_TRIANGLES])
        self.assertEqual([d[1] for d in draws], [N.dot_edge, N.dot_size, 0, 0])
        self.assertEqual(len(draws[0][4]), len(draws[1][4]))
        self.assertEqual([len(d[4]) for d in draws[2:]], [9, 9])
        self.assertTrue(all(d[2] == 0 and d[3] == 0 for d in draws))  # No depth test or write.
        self.assertTrue(all(v[2] == 0 for d in draws for v in d[4]))
        # Each draw loads the current camera's viewing matrix.
        names = [name for name, _ in m.calls]
        self.assertEqual(names.count('GXBegin'), 4)
        self.assertEqual(names.count('COBJ_GetCurrent'), 4)
        self.assertTrue(all(args[0] == CAMERA for args in m.named('COBJ_GetViewingMtx')))
        self.assertEqual(m.named('HSD_SetupRenderMode')[0][0], 0x68000002)

    def test_the_timer_is_the_stage_between_its_walls(self):
        m = self.m
        left, right = timer_edges()
        to_hud = mapping()
        for x, y in ((-N.out, N.bottom), (N.out, N.bottom), (N.out, N.top), (0, N.bottom),
                     (97.475, -28.7), (-97.475, -28.7), (-95.725, N.bottom), (30, N.top)):
            with self.subTest(x=x, y=y):
                self.assertNear(m.arrow(x, y)[0], to_hud(x, y))
        self.assertAlmostEqual(m.arrow(-N.out, N.bottom)[0][0], left, places=4)
        self.assertAlmostEqual(m.arrow(N.out, N.top)[0][0], right, places=4)
        self.assertAlmostEqual(m.arrow(0, N.bottom)[0][1], VANILLA_TIMER[1] - N.line, places=4)
        self.assertAlmostEqual(m.arrow(0, N.top)[0][1], VANILLA_TIMER[1] + N.line, places=4)

    def test_the_arrow_points_the_way_randall_moves(self):
        m = self.m
        for (x, y), want in (((0, N.bottom), (1, 0)), ((-90, N.bottom), (1, 0)), ((0, N.top), (-1, 0)),
                             ((90, N.top), (-1, 0)), ((97.475, -28.7), (0, 1)), ((-97.475, -28.7), (0, -1)),
                             ((-95.725, N.bottom), (1, 0))):  # His start corner: the segment leaving it.
            with self.subTest(x=x, y=y):
                self.assertNear(m.arrow(x, y)[1], want, places=4)
        _, (dx, dy), *_ = m.arrow(97.2, -37.4)
        self.assertGreater(dx, 0.05)
        self.assertGreater(dy, 0.05)
        self.assertAlmostEqual(math.hypot(dx, dy), 1, places=4)

    def test_the_arrow_is_faint_behind_the_timer_and_solid_past_it(self):
        m = self.m
        solid, faint = (shade(0, False), shade(1, False)), (shade(0, True), shade(1, True))
        self.assertEqual(solid[0], (235, 40, 40, 255))
        for x, want in ((-N.out - 0.01, solid), (-N.out, faint), (0, faint), (N.out, faint),
                        (N.out + 0.01, solid), (-97.475, solid), (97.475, solid)):
            with self.subTest(x=x):
                _, _, fill, edge = m.arrow(x, N.bottom if abs(x) < 97 else -28.7)
                self.assertEqual((fill, edge), want)

    def test_the_dots_run_evenly_around_the_whole_loop(self):
        m = self.m
        to_hud = mapping()
        loop = [to_hud(*p) for p in N.path]
        length = sum(math.dist(a, b) for a, b in zip(loop, loop[1:]))
        edges, dots = (d[4] for d in m.draw(0, N.bottom)[:2])
        self.assertEqual(len(dots) % 4, 0)
        self.assertLessEqual(abs(len(dots) - length / N.spacing), 2)
        self.assertEqual([v[:2] for v in edges], [v[:2] for v in dots])
        step = length / len(dots)
        for i, dot in enumerate(dots):
            self.assertLess(off_track(dot[:2], loop), 1e-3)
            nearest = min(math.dist(dot[:2], other[:2]) for j, other in enumerate(dots) if j != i)
            self.assertLess(nearest, step * 1.01)
            self.assertGreater(nearest, step * 0.6)
        left, right = timer_edges()
        for edge, dot in zip(edges, dots):
            behind = left <= dot[0] <= right
            self.assertEqual(dot[3], shade(2, behind))
            self.assertEqual(edge[3], shade(3, behind))
        self.assertTrue(any(left <= v[0] <= right for v in dots) and any(v[0] < left for v in dots))

    def test_both_ends_of_the_track_look_the_same(self):
        m = self.m
        for hours in (False, True):
            with self.subTest(hours=hours):
                m.timer(*VANILLA_TIMER, hours=hours)
                to_hud = mapping(hours=hours)
                dots = [v[:2] for v in m.draw(0, N.bottom)[1][4]]
                middle = to_hud(0, 0)[0]
                for x, y in dots:
                    self.assertLess(min(math.dist((2 * middle - x, y), d) for d in dots), 0.02)
                for side in (-1, 1):
                    end = sorted((y for x, y in dots if abs(x - to_hud(side * 97.475, 0)[0]) < 1e-3))
                    self.assertEqual(len(end), 2, side)
                    self.assertAlmostEqual(sum(end), 2 * VANILLA_TIMER[1], delta=0.02)

    def test_an_hour_widens_the_timer_and_the_map(self):
        m = self.m
        m.timer(*VANILLA_TIMER, hours=True)
        left, right = timer_edges(hours=True)
        self.assertAlmostEqual(left, -12.2 - 1.30078125, places=5)
        self.assertAlmostEqual(m.arrow(-N.out, N.bottom)[0][0], left, places=4)
        self.assertAlmostEqual(m.arrow(N.out, N.bottom)[0][0], right, places=4)

    def test_the_map_follows_the_timer(self):
        m = self.m
        m.timer(5.0, 10.0)
        to_hud = mapping(timer=(5.0, 10.0))
        for x, y in ((-N.out, N.bottom), (60, N.top), (97.475, -28.7)):
            self.assertNear(m.arrow(x, y)[0], to_hud(x, y))

    def test_a_huge_timer_caps_the_dots(self):
        m = self.m
        m.timer(0.0, 0.0, digits=[(-200.0 + 40 * i, 0.0) for i in range(10)], hours=True)
        edges, dots = (d[4] for d in m.draw(0, N.bottom)[:2])
        self.assertEqual((len(edges), len(dots)), (N.dots_max, N.dots_max))

    def test_without_randall_the_arrow_is_where_the_timer_puts_him(self):
        m = self.m
        m.map_present = False
        to_hud = mapping()
        for seconds, ms in ((0, 0), (9, 41), (10, 0), (61, 30), (3599, 59)):
            with self.subTest(seconds=seconds, ms=ms):
                m.time(3, seconds, ms)  # Counting up.
                center, *_ = arrow_of(m.draws(), N.length)
                self.assertNear(center, to_hud(*along(N.path, ((seconds * 60 + ms + 125) % 1200) / 1200)), places=3)
        m.time(3, 0, 0)
        self.assertNear(arrow_of(m.draws(), N.length)[1], (1, 0))
        # Counting down from 8:00, as a VS match: 7:50.31 is 582 frames in, his frame 707.
        m.time(2, 470, 41, start=(480, 0))
        draws = m.draws()
        center, direction, *_ = arrow_of(draws, N.length)
        self.assertNear(center, to_hud(*along(N.path, 707 / 1200)), places=3)
        self.assertNear(direction, (-1, 0))
        self.assertEqual([d[0] for d in draws], [GX_POINTS, GX_POINTS, GX_TRIANGLES, GX_TRIANGLES])
        # Not Randall's GObj, or none under it: the same.
        for case in ('index', 'scale joint'):
            with self.subTest(case=case):
                m2 = Machine()
                m2.word(MAP_DATA + 0x14, 3) if case == 'index' else m2.word(MAP + 0x28, 0)
                m2.time(3, 0, 0)
                self.assertNear(arrow_of(m2.draws(), N.length)[0], to_hud(*along(N.path, 125 / 1200)), places=3)
                self.assertNotIn(CLOUD, [args[0] for args in m2.named('JOBJ_GetWorldPosition')])

    def test_nothing_draws_without_everything_it_needs(self):
        cases = {
            'Battlefield': lambda m: m.stage(BATTLEFIELD_EXTERNAL, BATTLEFIELD_INTERNAL),
            'no timer': lambda m: m.word(TIMER_HUD, 0),
            'no timer root': lambda m: m.word(TIMER_GOBJ + 0x28, 0),
            'hidden timer': lambda m: m.word(TIMER_ROOT + 0x14, HIDDEN),
            'no digit showing': lambda m: [m.word(m.digit(i) + 0x14, HIDDEN) for i in range(10)],
            'no digits': lambda m: m.word(TIMER_ROOT + 0x10, 0),
            'no stage root': lambda m: m.word(SCALE + 0x10, 0),
            'no cloud joint': lambda m: m.word(FILE_ROOT + 0x10, 0),
            'not a number': lambda m: m.cloud(math.nan, N.bottom),
            'far away': lambda m: m.cloud(0.0, 5000.0),
            'no Randall and no timer': lambda m: (setattr(m, 'map_present', False), m.word(TIMER_HUD, 0)),
            'no Randall on another stage': lambda m: (setattr(m, 'map_present', False),
                                                      m.stage(BATTLEFIELD_EXTERNAL, BATTLEFIELD_INTERNAL)),
        }
        for name, case in cases.items():
            with self.subTest(name):
                m = Machine()
                m.start_match()
                case(m)
                self.assertEqual(m.draws(), [])
                self.assertEqual(m.fifo, [])
        with self.subTest('Battlefield stops at the stage'):
            m = Machine()
            m.stage(BATTLEFIELD_EXTERNAL, BATTLEFIELD_INTERNAL)
            m.draws()
            self.assertEqual([name for name, _ in m.calls], ['Stage_GetExternalID', 'Stage_ExternalToInternal'])
        for render_pass in (1, 2, 3):
            with self.subTest(render_pass=render_pass):
                m = Machine()
                self.assertEqual(m.draws(render_pass), [])
                self.assertEqual(m.calls, [])

    def test_the_cloud_is_the_stage_roots_first_child(self):
        m = self.m
        m.draw(40, N.top)
        joints = [args[0] for args in m.named('JOBJ_GetWorldPosition')]
        self.assertEqual(joints[0], CLOUD)
        self.assertEqual(joints[1:], [m.digit(i) for i in range(10) if i not in HOURS] + [TIMER_ROOT])


# --- The same drawing as Tyro's own --------------------------------------------

def toolchain():
    compiler = Path("C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe")
    import shutil
    return (ROOT / "build/Start.dol").exists() and (compiler.exists() or shutil.which("powerpc-eabi-gcc"))


@unittest.skipUnless(toolchain(), "needs devkitPPC and build/Start.dol")
class SameAsTyroTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        tyro.build()

    def setUp(self):
        self.gecko, self.c = Machine(), tyro.Machine()

    def assertSame(self, gecko, c, label=''):
        self.assertEqual(len(gecko), 4, label)  # Something to compare: dots and the arrow.
        self.assertEqual([d[:4] for d in gecko], [d[:4] for d in c], label)
        for (*_, mine), (*_, theirs) in zip(gecko, c):
            self.assertEqual(len(mine), len(theirs), label)
            for a, b in zip(mine, theirs):
                self.assertEqual(a[3], b[3], label)  # Colors exactly.
                self.assertLess(max(abs(a[0] - b[0]), abs(a[1] - b[1]), abs(a[2] - b[2])), 1e-5, label)

    def timers(self):
        yield 'vanilla', VANILLA_TIMER, VANILLA_DIGITS, False
        yield 'an hour', VANILLA_TIMER, VANILLA_DIGITS, True
        yield 'moved', (5.0, 10.0), VANILLA_DIGITS, False
        yield 'huge', (0.0, 0.0), [(-200.0 + 40 * i, 0.0) for i in range(10)], True

    def test_the_same_drawing_all_around_his_path(self):
        points = [along(N.path, i / 240) for i in range(240)]
        points += [(-N.out, N.bottom), (N.out, N.top), (N.out + 0.001, -20.0), (0.0, 0.0), (150.0, -60.0)]
        for name, timer, digits, hours in self.timers():
            self.gecko.timer(*timer, digits=digits, hours=hours)
            self.c.timer(*timer, digits=digits, hours=hours)
            for x, y in points if name == 'vanilla' else points[::12]:
                with self.subTest(timer=name, x=x, y=y):
                    self.assertSame(self.gecko.draw(x, y), self.c.draw(x, y), (name, x, y))

    def test_the_same_drawing_by_the_timer_without_randall(self):
        self.gecko.map_present = False
        self.c.fn("TestSet", tyro.MAP, 0)
        for mode, seconds, ms, start in ((3, 0, 0, (0, 0)), (3, 61, 30, (0, 0)), (3, 3599, 59, (0, 0)),
                                         (2, 470, 41, (480, 0)), (2, 479, 59, (480, 0)), (2, 5, 0, (480, 0)),
                                         (2, 100, 12, (240, 30))):
            with self.subTest(mode=mode, seconds=seconds, ms=ms, start=start):
                self.gecko.time(mode, seconds, ms, start)
                self.c.fn("TestTimeStart", *start)
                self.c.fn("TestTime", mode, seconds, ms)
                self.assertSame(self.gecko.draws(), self.c.draws("TestDraw", 0))
        for frame in range(0, 1200, 7):  # Counting up, around his lap.
            self.gecko.time(3, frame // 60, frame % 60)
            self.c.fn("TestTime", 3, frame // 60, frame % 60)
            self.assertSame(self.gecko.draws(), self.c.draws("TestDraw", 0), frame)

    def test_nothing_where_tyro_draws_nothing(self):
        for name, gecko, c in (
                ('no timer', lambda m: m.word(TIMER_HUD, 0), lambda m: m.fn("TestSet", tyro.TIMER, 0)),
                ('hidden timer', lambda m: m.word(TIMER_ROOT + 0x14, HIDDEN),
                 lambda m: m.fn("TestSet", tyro.TIMER_FLAGS, m.info(10))),
                ('no cloud joint', lambda m: m.word(FILE_ROOT + 0x10, 0), lambda m: m.fn("TestSet", tyro.CLOUD, 0))):
            with self.subTest(name):
                g, t = Machine(), tyro.Machine()
                gecko(g)
                c(t)
                self.assertEqual(g.draw(80, N.bottom), [])
                self.assertEqual(t.draw(80, N.bottom), [])

    @unittest.skipUnless((ROOT / "melee.iso").exists(), "needs a vanilla melee.iso at the repo root")
    def test_the_same_drawing_through_randalls_real_lap(self):
        frames = tyro.randall_path()["frames"]
        for frame, (x, y) in enumerate(frames):
            self.assertSame(self.gecko.draw(x, y), self.c.draw(x, y), frame)


if __name__ == "__main__":
    unittest.main()
