"""Execute the Double-Jump Height Indicator Gecko code in a PowerPC emulator.

python tests/test_dj_height_gecko.py -v
Requires Unicorn (the existing build/test-deps install is supported). The
native functions it calls are stubbed and the GX FIFO writes are recorded, so
this checks what the code asks the GPU to draw, not the picture itself.
"""
import importlib.util
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "build/test-deps"))
from unicorn import (Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE,
                     UC_HOOK_MEM_WRITE)
from unicorn import ppc_const as reg

CODE = ROOT / "slippi-gecko-codes/dj-height-indicator/dj-height-indicator.txt"
HOOK, ALLOCATION, STOP = 0x8016E8E0, 0x81700000, 0x80004000
GOBJ, LISTS, OBJECTS, FIGHTERS = 0x80E00000, 0x80BE0000, 0x80BF0000, 0x80C00000
R13, FIFO = 0x804DB6A0, 0xCC008000
FOX, FALCO, MARTH, POPO = 1, 22, 18, 10
GX_LINES = 0xA8
# Full side-B, drift either way, depth under the ledge, color: the numbers
# scripts/sweetspot_dj_platforms.py prints for the vanilla ISO.
CHARACTERS = {FOX: (83.740, 15.860, 53.037, 0xFF8C00FF), FALCO: (77.080, 18.510, 54.778, 0x30C8FFFF)}
# External stage ID: (left line, right line), live ledges (x, y) from the script.
STAGES = {
    2: ((0x03, 0x07), (63.35, 0.62)),
    3: ((0x33, 0x36), (87.75, 0.0)),
    8: ((0x02, 0x06), (56.0, -3.5)),
    28: ((0x03, 0x05), (77.27, 0.01)),
    31: ((0x00, 0x05), (68.4, 0.0)),
    32: ((0x00, 0x02), (85.57, 0.0)),
}
NATIVES = {
    0x803901F0: 'GObj_Create', 0x8039069C: 'GObj_AddGXLink', 0x80225194: 'Stage_GetExternalID',
    0x80053ECC: 'Stage_GetLeftOfLineCoordinates', 0x80053DA4: 'Stage_GetRightOfLineCoordinates',
    0x8036C244: 'HSD_ClearVtxDesc', 0x80341510: 'GXSetCurrentMtx', 0x8036A288: 'COBJ_GetCurrent',
    0x803695F0: 'COBJ_GetViewingMtx', 0x80341494: 'GXLoadPosMtxImm',
    0x80361A74: 'HSD_StateSetLineWidth', 0x8036199C: 'HSD_SetupRenderMode',
    0x80361BB8: 'HSD_StateSetZMode', 0x8033C414: 'GXSetVtxAttrFmt', 0x8033BF00: 'GXSetVtxDesc',
    0x80361AD8: 'HSD_StateSetCullMode', 0x8033D0DC: 'GXBegin',
}


def gpr(n):
    return getattr(reg, f"UC_PPC_REG_{n}")


def f32(value):
    return struct.unpack(">f", struct.pack(">f", value))[0]


def body():
    """The C2 code's address and instructions, from the distributed text."""
    words = []
    for line in CODE.read_text().splitlines():
        if line and line[0] not in '$*':
            words += [int(word, 16) for word in line.split()]
    assert words[0] >> 24 == 0xC2 and words[1] * 2 == len(words) - 2, 'one C2 code'
    return 0x80000000 | (words[0] & 0x1FFFFFF), words[2:]


class Machine:
    def __init__(self):
        self.cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
        self.cpu.mem_map(0x80000000, 0x1800000)
        self.cpu.mem_map(0xCC000000, 0x10000)
        self.cpu.reg_write(reg.UC_PPC_REG_MSR, 1 << 13)  # Floating point on.
        self.cpu.reg_write(gpr(1), 0x817F0000)
        self.cpu.reg_write(gpr(13), R13)
        address, words = body()
        assert address == HOOK
        # Install like the code handler: branch in, and the final word back out.
        end = ALLOCATION + 4 * (len(words) - 1)
        words[-1] = 0x48000000 | ((HOOK + 4 - end) & 0x3FFFFFC)
        self.cpu.mem_write(ALLOCATION, struct.pack(f">{len(words)}I", *words))
        self.w(HOOK, 0x48000000 | ((ALLOCATION - HOOK) & 0x3FFFFFC))
        self.w(R13 - 0x3E74, LISTS)
        self.stage, self.ledges, self.created = 32, ((-85.57, 0.0), (85.57, 0.0)), GOBJ
        self.calls, self.fifo = [], []
        self.cpu.hook_add(UC_HOOK_CODE, self.native)
        self.cpu.hook_add(UC_HOOK_MEM_WRITE, self.write, begin=FIFO, end=FIFO + 3)
        self.fighters([])

    def w(self, address, value, kind="I"):
        self.cpu.mem_write(address, struct.pack(">" + kind, value))

    def fighters(self, kinds):
        """Link one fighter GObj per kind into the fighter list (p_link 8)."""
        self.w(LISTS + 0x20, OBJECTS if kinds else 0)
        for i, kind in enumerate(kinds):
            obj, data = OBJECTS + i * 0x40, FIGHTERS + i * 0x1000
            self.w(obj + 0x08, obj + 0x40 if i + 1 < len(kinds) else 0)
            self.w(obj + 0x2C, data)
            self.w(data + 0x04, kind)

    def native(self, cpu, address, size, _):
        name = NATIVES.get(address)
        if not name:
            return
        args = tuple(cpu.reg_read(gpr(n)) for n in range(3, 8))
        self.calls.append((name, args))
        if name == 'GObj_Create':
            cpu.reg_write(gpr(3), self.created)
        elif name == 'Stage_GetExternalID':
            cpu.reg_write(gpr(3), self.stage)
        elif name.startswith('Stage_Get'):
            x, y = self.ledges[name == 'Stage_GetRightOfLineCoordinates']
            cpu.mem_write(args[1], struct.pack(">3f", x, y, 0))
        elif name == 'COBJ_GetCurrent':
            cpu.reg_write(gpr(3), 0x80D00000)
        # Clobber the volatile registers the way a real call may.
        for n in (0, 5, 6, 7, 8, 9, 10, 11, 12):
            cpu.reg_write(gpr(n), 0xDEAD0000 | n)
        cpu.reg_write(reg.UC_PPC_REG_PC, cpu.reg_read(reg.UC_PPC_REG_LR))

    def write(self, cpu, access, address, size, value, _):
        self.fifo.append((size, value & 0xFFFFFFFF))

    def named(self, name):
        return [args for called, args in self.calls if called == name]

    def start_match(self, r3=0x80470000):
        registers = {n: 0x80100000 + n * 0x100 for n in range(3, 32) if n != 13}  # r13 is the SDA base.
        registers[3] = r3
        for n, value in registers.items():
            self.cpu.reg_write(gpr(n), value)
        self.cpu.reg_write(reg.UC_PPC_REG_LR, 0x8016E8E0 - 0x24)
        sp = self.cpu.reg_read(gpr(1))
        self.cpu.emu_start(HOOK, HOOK + 4)
        links = self.named('GObj_AddGXLink')
        self.callback = links[0][1] if links else None
        registers[30] = r3  # The original instruction, mr r30,r3.
        return registers, sp

    def draw(self, pass_number=2):
        assert self.callback, 'start_match first'
        saved = {n: 0x81000000 + n for n in range(14, 32)}
        for n, value in saved.items():
            self.cpu.reg_write(gpr(n), value)
        sp = self.cpu.reg_read(gpr(1))
        self.calls, self.fifo = [], []
        self.cpu.reg_write(gpr(3), self.created)
        self.cpu.reg_write(gpr(4), pass_number)
        self.cpu.reg_write(reg.UC_PPC_REG_LR, STOP)
        self.cpu.emu_start(self.callback, STOP)
        assert self.cpu.reg_read(gpr(1)) == sp, 'stack restored'
        for n, value in saved.items():
            assert self.cpu.reg_read(gpr(n)) == value, f'r{n} preserved'

    def lines(self):
        """FIFO writes as (x, y, z, color) vertices."""
        assert all(size == 4 for size, _ in self.fifo)
        words = [value for _, value in self.fifo]
        assert len(words) % 4 == 0
        return [(*struct.unpack(">3f", struct.pack(">3I", *words[i:i + 3])), words[i + 3])
                for i in range(0, len(words), 4)]


class HookTests(unittest.TestCase):
    def test_match_start_creates_one_world_overlay_gobj_and_restores_registers(self):
        m = Machine()
        expected, sp = m.start_match()
        self.assertEqual([args[:3] for args in m.named('GObj_Create')], [(0, 7, 0)])
        (gobj, callback, link, priority, _), = m.named('GObj_AddGXLink')
        self.assertEqual((gobj, link, priority), (GOBJ, 5, 0))
        self.assertTrue(ALLOCATION <= callback < ALLOCATION + 0x400)
        for n, value in expected.items():
            self.assertEqual(m.cpu.reg_read(gpr(n)), value, f'r{n}')
        self.assertEqual(m.cpu.reg_read(gpr(1)), sp)
        self.assertEqual(m.cpu.reg_read(reg.UC_PPC_REG_LR), 0x8016E8E0 - 0x24)

    def test_failed_gobj_creation_skips_the_draw_link(self):
        m = Machine()
        m.created = 0
        expected, _ = m.start_match()
        self.assertEqual(m.named('GObj_AddGXLink'), [])
        self.assertEqual(m.cpu.reg_read(gpr(30)), expected[30])


class DrawTests(unittest.TestCase):
    def setUp(self):
        self.m = Machine()
        self.m.start_match()

    def expected(self, kinds, left, right):
        lines = []
        for kind in kinds:
            full, drift, depth, color = CHARACTERS[kind]
            for (x, y), offstage in ((left, -1), (right, 1)):
                center, height = f32(f32(x) + offstage * f32(full)), f32(f32(y) - f32(depth))
                lines.append((f32(center - f32(drift)), height, 0.0, color))
                lines.append((f32(center + f32(drift)), height, 0.0, color))
        return lines

    def assertLines(self, actual, expected):
        self.assertEqual(len(actual), len(expected))
        for got, want in zip(actual, expected):
            self.assertAlmostEqual(got[0], want[0], places=3)
            self.assertAlmostEqual(got[1], want[1], places=3)
            self.assertEqual(got[2:], want[2:])

    def test_every_legal_stage_draws_both_characters_under_both_ledges(self):
        m = self.m
        m.fighters([MARTH, FALCO, POPO, FOX])  # Order in the list doesn't matter: Fox draws first.
        for stage, ((left_line, right_line), (x, y)) in STAGES.items():
            with self.subTest(stage=stage):
                m.stage, m.ledges = stage, ((-x, y), (x, y))
                m.draw()
                self.assertEqual([args[0] for args in m.named('Stage_GetLeftOfLineCoordinates')], [left_line])
                self.assertEqual([args[0] for args in m.named('Stage_GetRightOfLineCoordinates')], [right_line])
                self.assertEqual([args[:3] for args in m.named('GXBegin')], [(GX_LINES, 0, 4)] * 2)
                self.assertEqual([args[0] for args in m.named('HSD_StateSetLineWidth')], [16, 16])
                self.assertEqual([args[:3] for args in m.named('HSD_StateSetZMode')], [(0, 7, 0)] * 2)
                self.assertLines(m.lines(), self.expected([FOX, FALCO], m.ledges[0], m.ledges[1]))

    def test_platforms_follow_each_ledge_separately(self):
        m = self.m
        m.fighters([FOX])
        m.stage, m.ledges = 8, ((-70.5, 2.25), (81.25, -3.5))
        m.draw()
        self.assertLines(m.lines(), self.expected([FOX], *m.ledges))
        # Fox's line on Final Destination, worked by hand from the script's numbers.
        m.stage, m.ledges = 32, ((-85.5657, 0.0), (85.5657, 0.0))
        m.draw()
        left0, left1, right0, right1 = m.lines()
        self.assertAlmostEqual(left0[0], -185.1657, places=3)
        self.assertAlmostEqual(left1[0], -153.4457, places=3)
        self.assertAlmostEqual(right0[0], 153.4457, places=3)
        self.assertAlmostEqual(right1[0], 185.1657, places=3)
        self.assertAlmostEqual(left0[1], -53.037, places=3)

    def test_only_the_characters_in_the_match_draw(self):
        m = self.m
        for kinds, drawn in (([FALCO], [FALCO]), ([FOX, FOX], [FOX]), ([FALCO, FALCO, FOX], [FOX, FALCO])):
            with self.subTest(kinds=kinds):
                m.fighters(kinds)
                m.draw()
                self.assertLines(m.lines(), self.expected(drawn, *m.ledges))
        for kinds in ([], [MARTH], [POPO, 11]):
            with self.subTest(kinds=kinds):
                m.fighters(kinds)
                m.draw()
                self.assertEqual(m.fifo, [])
                self.assertEqual(m.named('GXBegin'), [])
                self.assertEqual(m.named('Stage_GetLeftOfLineCoordinates'), [])

    def test_a_fighter_gobj_without_data_is_skipped(self):
        m = self.m
        m.fighters([FALCO, FOX])
        m.w(OBJECTS + 0x2C, 0)
        m.draw()
        self.assertLines(m.lines(), self.expected([FOX], *m.ledges))

    def test_other_stages_and_render_passes_draw_nothing(self):
        m = self.m
        m.fighters([FOX, FALCO])
        for stage in (0, 1, 4, 22, 24, 33, 255):
            with self.subTest(stage=stage):
                m.stage = stage
                m.draw()
                self.assertEqual(m.fifo, [])
                self.assertEqual(m.named('Stage_GetLeftOfLineCoordinates'), [])
        m.stage = 32
        for pass_number in (0, 1, 3):
            with self.subTest(pass_number=pass_number):
                m.draw(pass_number)
                self.assertEqual(m.calls, [])


@unittest.skipUnless((ROOT / "melee.iso").exists(), "needs a vanilla melee.iso at the repo root")
class IsoTests(unittest.TestCase):
    def test_hardcoded_numbers_match_the_iso(self):
        spec = importlib.util.spec_from_file_location('platforms', ROOT / 'scripts/sweetspot_dj_platforms.py')
        platforms = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(platforms)
        with open(ROOT / "melee.iso", "rb") as iso:
            for (dat, _, dash, drop), kind in zip(platforms.CHARACTERS, (FOX, FALCO)):
                numbers = platforms.character(platforms.read_file(iso, dat), dash, drop)
                full, drift, depth, _ = CHARACTERS[kind]
                self.assertAlmostEqual(numbers['full'], full, places=3)
                self.assertAlmostEqual(numbers['drift'], drift, places=3)
                self.assertAlmostEqual(numbers['depth'], depth, places=3)
            for dat, _, stage, left_line, right_line in platforms.STAGES:
                (lx, ly), (rx, ry) = platforms.ledges(platforms.read_file(iso, dat), left_line, right_line)
                lines, (x, y) = STAGES[stage]
                self.assertEqual((left_line, right_line), lines)
                self.assertAlmostEqual(rx, x, places=2)
                self.assertAlmostEqual(-lx, x, places=2)
                self.assertAlmostEqual(ry, y, places=2)


if __name__ == "__main__":
    unittest.main()
