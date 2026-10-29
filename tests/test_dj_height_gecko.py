"""Execute the Fox and Falco Double-Jump Height Indicator Gecko codes in a
PowerPC emulator.

python tests/test_dj_height_gecko.py -v
Requires Unicorn (the existing build/test-deps install is supported). The
native functions the codes call are stubbed and the GX FIFO writes are
recorded, so this checks what the codes ask the GPU to draw, not the picture.
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

FOLDER = ROOT / "slippi-gecko-codes/dj-height-indicator"
STOP, R13, FIFO = 0x80004000, 0x804DB6A0, 0xCC008000
GX_LINES, TARGET = 0xA8, 2.5
# Text file, hook, then the numbers scripts/sweetspot_dj_platforms.py prints
# for the vanilla ISO: full side-B, platform half-width, depth, and color.
CODES = {
    'Fox': ("fox-dj-height-indicator.txt", 0x8016E8E0, (83.740, 10.573, 57.537, 0xFF8C00FF)),
    'Falco': ("falco-dj-height-indicator.txt", 0x8016E8E8, (77.080, 12.340, 59.278, 0x30C8FFFF)),
}
ALLOCATIONS = {'Fox': 0x81700000, 'Falco': 0x81710000}
GOBJS = {'Fox': 0x80E00000, 'Falco': 0x80E00100}
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


def code(character):
    """The C2 code's address and instructions, from the distributed text."""
    words = []
    for line in (FOLDER / CODES[character][0]).read_text().splitlines():
        if line and line[0] not in '$*':
            words += [int(word, 16) for word in line.split()]
    assert words[0] >> 24 == 0xC2 and words[1] * 2 == len(words) - 2, 'one C2 code'
    return 0x80000000 | (words[0] & 0x1FFFFFF), words[2:]


class Machine:
    def __init__(self, characters=('Fox', 'Falco')):
        self.cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
        self.cpu.mem_map(0x80000000, 0x1800000)
        self.cpu.mem_map(0xCC000000, 0x10000)
        self.cpu.reg_write(reg.UC_PPC_REG_MSR, 1 << 13)  # Floating point on.
        self.cpu.reg_write(gpr(1), 0x817F0000)
        self.cpu.reg_write(gpr(13), R13)
        for character in characters:
            hook, words = code(character)
            assert hook == CODES[character][1]
            # Install like the code handler: branch in, and the final word back out.
            allocation = ALLOCATIONS[character]
            end = allocation + 4 * (len(words) - 1)
            words[-1] = 0x48000000 | ((hook + 4 - end) & 0x3FFFFFC)
            self.cpu.mem_write(allocation, struct.pack(f">{len(words)}I", *words))
            self.w(hook, 0x48000000 | ((allocation - hook) & 0x3FFFFFC))
        self.stage, self.ledges, self.created = 32, ((-85.57, 0.0), (85.57, 0.0)), None
        self.calls, self.fifo, self.callbacks = [], [], {}
        self.cpu.hook_add(UC_HOOK_CODE, self.native)
        self.cpu.hook_add(UC_HOOK_MEM_WRITE, self.write, begin=FIFO, end=FIFO + 3)

    def w(self, address, value, kind="I"):
        self.cpu.mem_write(address, struct.pack(">" + kind, value))

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
        # Clobber the volatile registers and cr0 the way a real call may.
        for n in (0, 5, 6, 7, 8, 9, 10, 11, 12):
            cpu.reg_write(gpr(n), 0xDEAD0000 | n)
        cpu.reg_write(reg.UC_PPC_REG_CR0, 0)
        cpu.reg_write(reg.UC_PPC_REG_PC, cpu.reg_read(reg.UC_PPC_REG_LR))

    def write(self, cpu, access, address, size, value, _):
        self.fifo.append((size, value & 0xFFFFFFFF))

    def named(self, name):
        return [args for called, args in self.calls if called == name]

    def start_match(self, character, r3=0x80470000, r30=0x80103000):
        """Run StartMelee's hooked instruction. Returns the expected registers."""
        self.created = GOBJS[character] if self.created != 0 else 0
        registers = {n: 0x80100000 + n * 0x100 for n in range(3, 32) if n != 13}  # r13 is the SDA base.
        registers[3], registers[30] = r3, r30
        for n, value in registers.items():
            self.cpu.reg_write(gpr(n), value)
        self.cpu.reg_write(reg.UC_PPC_REG_LR, 0x8016E8B0)
        sp, hook = self.cpu.reg_read(gpr(1)), CODES[character][1]
        self.calls = []
        self.cpu.emu_start(hook, hook + 4)
        links = self.named('GObj_AddGXLink')
        if links:
            self.callbacks[character] = links[0][1]
        if character == 'Fox':
            registers[30] = r3  # Its replaced instruction, mr r30,r3.
        assert self.cpu.reg_read(gpr(1)) == sp, 'stack restored'
        return registers

    def draw(self, character, pass_number=2):
        saved = {n: 0x81000000 + n for n in range(14, 32)}
        for n, value in saved.items():
            self.cpu.reg_write(gpr(n), value)
        sp = self.cpu.reg_read(gpr(1))
        self.calls, self.fifo = [], []
        self.cpu.reg_write(gpr(3), GOBJS[character])
        self.cpu.reg_write(gpr(4), pass_number)
        self.cpu.reg_write(reg.UC_PPC_REG_LR, STOP)
        self.cpu.emu_start(self.callbacks[character], STOP)
        assert self.cpu.reg_read(gpr(1)) == sp, 'stack restored'
        for n, value in saved.items():
            assert self.cpu.reg_read(gpr(n)) == value, f'r{n} preserved'

    def vertices(self):
        """FIFO writes as (x, y, z, color) vertices."""
        assert all(size == 4 for size, _ in self.fifo)
        words = [value for _, value in self.fifo]
        assert len(words) % 4 == 0
        return [(*struct.unpack(">3f", struct.pack(">3I", *words[i:i + 3])), words[i + 3])
                for i in range(0, len(words), 4)]


def expected(character, left, right):
    """Under each ledge: the platform, then the X at ledge height."""
    full, width, depth, color = CODES[character][2]
    vertices = []
    for (x, y), offstage in ((left, -1), (right, 1)):
        x, y = f32(f32(x) + offstage * f32(full)), f32(y)
        for dx, dy in ((-width, -depth), (width, -depth),
                       (-TARGET, -TARGET), (TARGET, TARGET), (-TARGET, TARGET), (TARGET, -TARGET)):
            vertices.append((f32(x + f32(dx)), f32(y + f32(dy)), 0.0, color))
    return vertices


class HookTests(unittest.TestCase):
    def test_each_code_creates_its_own_overlay_and_restores_registers(self):
        m = Machine()
        for character in CODES:
            with self.subTest(character=character):
                registers = m.start_match(character)
                self.assertEqual([args[:3] for args in m.named('GObj_Create')], [(0, 7, 0)])
                (gobj, callback, link, priority, _), = m.named('GObj_AddGXLink')
                self.assertEqual((gobj, link, priority), (GOBJS[character], 5, 0))
                allocation = ALLOCATIONS[character]
                self.assertTrue(allocation <= callback < allocation + 0x400)
                for n, value in registers.items():
                    self.assertEqual(m.cpu.reg_read(gpr(n)), value, f'r{n}')
                self.assertEqual(m.cpu.reg_read(reg.UC_PPC_REG_LR), 0x8016E8B0)
        self.assertNotEqual(m.callbacks['Fox'], m.callbacks['Falco'])

    def test_falco_hook_still_sets_cr0_for_the_native_branch(self):
        m = Machine(('Falco',))
        for r30, flags in ((0, 0b0010), (0x80400000, 0b0100)):  # cmplwi: EQ, then GT.
            with self.subTest(r30=r30):
                m.start_match('Falco', r30=r30)
                self.assertEqual(m.cpu.reg_read(reg.UC_PPC_REG_CR0) & 0b1110, flags)

    def test_failed_gobj_creation_skips_the_draw_link(self):
        for character in CODES:
            with self.subTest(character=character):
                m = Machine((character,))
                m.created = 0
                registers = m.start_match(character)
                self.assertEqual(m.named('GObj_AddGXLink'), [])
                self.assertEqual(m.cpu.reg_read(gpr(30)), registers[30])


class DrawTests(unittest.TestCase):
    def setUp(self):
        self.m = Machine()
        for character in CODES:
            self.m.start_match(character)

    def assertVertices(self, actual, wanted):
        self.assertEqual(len(actual), len(wanted))
        for got, want in zip(actual, wanted):
            self.assertAlmostEqual(got[0], want[0], places=3)
            self.assertAlmostEqual(got[1], want[1], places=3)
            self.assertEqual(got[2:], want[2:])

    def test_every_legal_stage_draws_the_platform_and_x_under_both_ledges(self):
        m = self.m
        for character in CODES:
            for stage, ((left_line, right_line), (x, y)) in STAGES.items():
                with self.subTest(character=character, stage=stage):
                    m.stage, m.ledges = stage, ((-x, y), (x, y))
                    m.draw(character)
                    self.assertEqual([args[0] for args in m.named('Stage_GetLeftOfLineCoordinates')], [left_line])
                    self.assertEqual([args[0] for args in m.named('Stage_GetRightOfLineCoordinates')], [right_line])
                    self.assertEqual([args[:3] for args in m.named('GXBegin')], [(GX_LINES, 0, 12)])
                    self.assertEqual([args[0] for args in m.named('HSD_StateSetLineWidth')], [16])
                    self.assertEqual([args[:3] for args in m.named('HSD_StateSetZMode')], [(0, 7, 0)])
                    self.assertVertices(m.vertices(), expected(character, *m.ledges))

    def test_lines_follow_each_ledge_separately(self):
        m = self.m
        m.stage, m.ledges = 8, ((-70.5, 2.25), (81.25, -3.5))
        m.draw('Falco')
        self.assertVertices(m.vertices(), expected('Falco', *m.ledges))
        # Fox on Final Destination, worked by hand from the script's numbers.
        m.stage, m.ledges = 32, ((-85.5657, 0.0), (85.5657, 0.0))
        m.draw('Fox')
        vertices = m.vertices()
        self.assertEqual([round(v[0], 2) for v in vertices[:6]],
                         [-179.88, -158.73, -171.81, -166.81, -171.81, -166.81])
        self.assertEqual([round(v[1], 2) for v in vertices[:6]], [-57.54, -57.54, -2.5, 2.5, 2.5, -2.5])
        self.assertEqual([round(v[0], 2) for v in vertices[6:8]], [158.73, 179.88])

    def test_other_stages_and_render_passes_draw_nothing(self):
        m = self.m
        for character in CODES:
            for stage in (0, 1, 4, 22, 24, 33, 255):
                with self.subTest(character=character, stage=stage):
                    m.stage = stage
                    m.draw(character)
                    self.assertEqual(m.fifo, [])
                    self.assertEqual(m.named('Stage_GetLeftOfLineCoordinates'), [])
            m.stage = 32
            for pass_number in (0, 1, 3):
                with self.subTest(character=character, pass_number=pass_number):
                    m.draw(character, pass_number)
                    self.assertEqual(m.calls, [])


@unittest.skipUnless((ROOT / "melee.iso").exists(), "needs a vanilla melee.iso at the repo root")
class IsoTests(unittest.TestCase):
    def test_hardcoded_numbers_match_the_iso(self):
        spec = importlib.util.spec_from_file_location('platforms', ROOT / 'scripts/sweetspot_dj_platforms.py')
        platforms = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(platforms)
        with open(ROOT / "melee.iso", "rb") as iso:
            for dat, name, dash, drop in platforms.CHARACTERS:
                numbers = platforms.character(platforms.read_file(iso, dat), dash, drop)
                full, width, depth, _ = CODES[name][2]
                self.assertAlmostEqual(numbers['full'], full, places=3)
                self.assertAlmostEqual(numbers['width'], width, places=3)
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
