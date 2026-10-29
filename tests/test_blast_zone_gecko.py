"""Execute the Blast Zone Overlay Gecko code in a PowerPC emulator.

python tests/test_blast_zone_gecko.py -v
Requires Unicorn (the existing build/test-deps install is supported). The
native functions the code calls are stubbed and the GX FIFO writes are
recorded, so this checks what the code asks the GPU to draw, not the picture.
"""
from pathlib import Path
import math
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "build/test-deps"))
from unicorn import (Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE,
                     UC_HOOK_MEM_WRITE)
from unicorn import ppc_const as reg

TEXT = ROOT / "slippi-gecko-codes/blast-zone-overlay/blast-zone-overlay.txt"
HOOK, ALLOCATION, GOBJ = 0x8016E8C0, 0x81700000, 0x80E00000
STOP, R13, FIFO, STAGE = 0x80004000, 0x804DB6A0, 0xCC008000, 0x8049E6C8
GX_QUADS, GX_LINESTRIP, GX_POINTS = 0x80, 0xB0, 0xB8
LINE_WIDTH, GLOW = 18, 24.0
LINE, GLOW_COLOR, CLEAR = 0xFF0000FF, 0x78000078, 0
# Blast zones (left, right, top, bottom) and camera offset (x, y), as stored at
# stc_stage 0x74..0x80 and 0x10..0x14. The first is Final Destination's.
ZONES = [
    ((-246.0, 246.0, 188.0, -140.0), (0.0, 0.0)),
    ((-175.7, 173.6, 168.0, -91.0), (0.0, 0.0)),
    ((-230.0, 230.0, 180.0, -111.0), (12.5, -7.25)),  # A scrolled camera offset moves them.
]
NATIVES = {
    0x803901F0: 'GObj_Create', 0x8039069C: 'GObj_AddGXLink',
    0x8036C244: 'HSD_ClearVtxDesc', 0x80341510: 'GXSetCurrentMtx', 0x8036A288: 'COBJ_GetCurrent',
    0x803695F0: 'COBJ_GetViewingMtx', 0x80341494: 'GXLoadPosMtxImm',
    0x80361A74: 'HSD_StateSetLineWidth', 0x80361C60: 'HSD_StateSetPointSize',
    0x8036199C: 'HSD_SetupRenderMode', 0x80361BB8: 'HSD_StateSetZMode',
    0x8033C414: 'GXSetVtxAttrFmt', 0x8033BF00: 'GXSetVtxDesc',
    0x80361AD8: 'HSD_StateSetCullMode', 0x8033D0DC: 'GXBegin',
}


def gpr(n):
    return getattr(reg, f"UC_PPC_REG_{n}")


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
        self.calls, self.fifo = [], []
        self.set_zones(*ZONES[0])
        self.cpu.hook_add(UC_HOOK_CODE, self.native)
        self.cpu.hook_add(UC_HOOK_MEM_WRITE, self.write, begin=FIFO, end=FIFO + 3)

    def set_zones(self, zones, offset):
        self.cpu.mem_write(STAGE + 0x10, struct.pack(">2f", *offset))
        self.cpu.mem_write(STAGE + 0x74, struct.pack(">4f", *zones))

    def native(self, cpu, address, size, _):
        name = NATIVES.get(address)
        if not name:
            return
        args = tuple(cpu.reg_read(gpr(n)) for n in range(3, 8))
        self.calls.append((name, args))
        if name == 'GObj_Create':
            cpu.reg_write(gpr(3), self.created)
        elif name == 'COBJ_GetCurrent':
            cpu.reg_write(gpr(3), 0x80D00000)
        # Clobber the volatile registers, cr0 and the float scratch registers
        # the way a real call may.
        for n in (0, 5, 6, 7, 8, 9, 10, 11, 12):
            cpu.reg_write(gpr(n), 0xDEAD0000 | n)
        for n in range(14):
            cpu.reg_write(getattr(reg, f"UC_PPC_REG_FPR{n}"), 0x7FF8DEAD0000DEAD)
        cpu.reg_write(reg.UC_PPC_REG_CR0, 0)
        cpu.reg_write(reg.UC_PPC_REG_PC, cpu.reg_read(reg.UC_PPC_REG_LR))

    def write(self, cpu, access, address, size, value, _):
        self.fifo.append((size, value & 0xFFFFFFFF))

    def named(self, name):
        return [args for called, args in self.calls if called == name]

    def start_match(self):
        """Run StartMelee's hooked instruction. Returns the expected registers."""
        registers = {n: 0x80100000 + n * 0x100 for n in range(3, 32) if n != 13}  # r13 is the SDA base.
        for n, value in registers.items():
            self.cpu.reg_write(gpr(n), value)
        self.cpu.reg_write(reg.UC_PPC_REG_LR, 0x8016E8B0)
        sp = self.cpu.reg_read(gpr(1))
        self.calls = []
        self.cpu.emu_start(HOOK, HOOK + 4)
        links = self.named('GObj_AddGXLink')
        if links:
            self.callback = links[0][1]
        registers[3] = 255  # Its replaced instruction, li r3,255.
        assert self.cpu.reg_read(gpr(1)) == sp, 'stack restored'
        return registers

    def draw(self, pass_number=2):
        saved = {n: 0x81000000 + n for n in range(14, 32)}
        for n, value in saved.items():
            self.cpu.reg_write(gpr(n), value)
        sp = self.cpu.reg_read(gpr(1))
        self.calls, self.fifo = [], []
        self.cpu.reg_write(gpr(3), GOBJ)
        self.cpu.reg_write(gpr(4), pass_number)
        self.cpu.reg_write(reg.UC_PPC_REG_LR, STOP)
        self.cpu.emu_start(self.callback, STOP)
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


def expected(zones, offset):
    """The glow bands, the closed line, then a point on each corner."""
    left, right, top, bottom = (f32(f32(edge) + f32(offset[i > 1])) for i, edge in enumerate(zones))
    g = f32(GLOW)
    inner = [(left, bottom), (right, bottom), (right, top), (left, top)]
    outer = [(f32(left - g), f32(bottom - g)), (f32(right + g), f32(bottom - g)),
             (f32(right + g), f32(top + g)), (f32(left - g), f32(top + g))]
    vertices = []
    for i in range(4):
        j = (i + 1) % 4
        vertices += [(*inner[i], 0.0, GLOW_COLOR), (*inner[j], 0.0, GLOW_COLOR),
                     (*outer[j], 0.0, CLEAR), (*outer[i], 0.0, CLEAR)]
    vertices += [(*inner[i % 4], 0.0, LINE) for i in range(5)]
    vertices += [(*corner, 0.0, LINE) for corner in inner]
    return vertices


class HookTests(unittest.TestCase):
    def test_hook_creates_the_overlay_and_restores_registers(self):
        m = Machine()
        registers = m.start_match()
        self.assertEqual([args[:3] for args in m.named('GObj_Create')], [(0, 7, 0)])
        (gobj, callback, link, priority, _), = m.named('GObj_AddGXLink')
        self.assertEqual((gobj, link, priority), (GOBJ, 5, 0))
        self.assertTrue(ALLOCATION <= callback < ALLOCATION + 0x400)
        for n, value in registers.items():
            self.assertEqual(m.cpu.reg_read(gpr(n)), value, f'r{n}')
        self.assertEqual(m.cpu.reg_read(reg.UC_PPC_REG_LR), 0x8016E8B0)

    def test_failed_gobj_creation_skips_the_draw_link(self):
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

    def assertVertices(self, actual, wanted):
        self.assertEqual(len(actual), len(wanted))
        for got, want in zip(actual, wanted):
            self.assertEqual(got, want)

    def test_draws_the_glow_line_and_corners_on_the_live_blast_zones(self):
        m = self.m
        for zones, offset in ZONES:
            with self.subTest(zones=zones, offset=offset):
                m.set_zones(zones, offset)
                m.draw()
                self.assertEqual([args[:3] for args in m.named('GXBegin')],
                                 [(GX_QUADS, 0, 16), (GX_LINESTRIP, 0, 5), (GX_POINTS, 0, 4)])
                self.assertEqual([args[:2] for args in m.named('HSD_StateSetLineWidth')],
                                 [(0, 5), (LINE_WIDTH, 5), (LINE_WIDTH, 5)])
                self.assertEqual([args[:2] for args in m.named('HSD_StateSetPointSize')],
                                 [(0, 5), (LINE_WIDTH, 5), (LINE_WIDTH, 5)])
                self.assertEqual([args[:3] for args in m.named('HSD_StateSetZMode')], [(0, 7, 0)] * 3)
                self.assertVertices(m.vertices(), expected(zones, offset))

    def test_final_destination_worked_by_hand(self):
        m = self.m
        m.draw()
        vertices = m.vertices()
        self.assertEqual(vertices[:4], [(-246.0, -140.0, 0.0, GLOW_COLOR), (246.0, -140.0, 0.0, GLOW_COLOR),
                                        (270.0, -164.0, 0.0, CLEAR), (-270.0, -164.0, 0.0, CLEAR)])
        self.assertEqual([v[:2] for v in vertices[16:21]],
                         [(-246.0, -140.0), (246.0, -140.0), (246.0, 188.0), (-246.0, 188.0), (-246.0, -140.0)])

    def test_empty_or_inverted_blast_zones_and_other_passes_draw_nothing(self):
        m = self.m
        for zones in ((0.0, 0.0, 0.0, 0.0), (100.0, -100.0, 50.0, -50.0), (-100.0, 100.0, -50.0, 50.0),
                      (math.nan, 100.0, 50.0, -50.0)):
            with self.subTest(zones=zones):
                m.set_zones(zones, (0.0, 0.0))
                m.draw()
                self.assertEqual(m.fifo, [])
                self.assertEqual(m.calls, [])
        m.set_zones(*ZONES[0])
        for pass_number in (0, 1, 3):
            with self.subTest(pass_number=pass_number):
                m.draw(pass_number)
                self.assertEqual(m.calls, [])
                self.assertEqual(m.fifo, [])


@unittest.skipUnless((ROOT / "melee.iso").exists(), "needs a vanilla melee.iso at the repo root")
class IsoTests(unittest.TestCase):
    def test_code_reads_the_blast_zones_exactly_like_the_game_getters(self):
        """Each Stage_GetBlastzone* returns stc_stage's edge plus its camera
        offset, the sum the code computes; Player_CheckForDeath calls them."""
        with open(ROOT / "melee.iso", "rb") as iso:
            iso.seek(0x420)
            dol = struct.unpack(">I", iso.read(4))[0]
            iso.seek(dol)
            header = iso.read(0x100)

            def read(address, count):
                for i in range(18):
                    offset, start, size = (struct.unpack_from(">I", header, at + 4 * i)[0] for at in (0, 0x48, 0x90))
                    if start <= address < start + size:
                        iso.seek(dol + offset + address - start)
                        return struct.unpack(f">{count}I", iso.read(4 * count))
                raise AssertionError(f"{address:08X} not in the DOL")

            for getter, edge, offset in ((0x80224B38, 0x78, 0x10), (0x80224B50, 0x74, 0x10),
                                         (0x80224B68, 0x7C, 0x14), (0x80224B80, 0x80, 0x14)):
                with self.subTest(getter=hex(getter)):
                    self.assertEqual(read(getter, 6), (
                        0x3C60804A, 0x3863E6C8,       # r3 = 0x8049E6C8 (stc_stage)
                        0xC0230000 | edge,            # lfs f1, edge(r3)
                        0xC0030000 | offset,          # lfs f0, offset(r3)
                        0xEC21002A, 0x4E800020))      # fadds f1, f1, f0; blr
            # Player_CheckForDeath: right, left, top, then bottom.
            calls = []
            for at, word in enumerate(read(0x800D3158, 113)):
                if word >> 26 == 18 and word & 3 == 1:
                    target = (0x800D3158 + 4 * at + ((word & 0x3FFFFFC) ^ 0x2000000) - 0x2000000) & 0xFFFFFFFF
                    if 0x80224B38 <= target <= 0x80224B80:
                        calls.append(target)
            self.assertEqual(calls, [0x80224B38, 0x80224B50, 0x80224B68, 0x80224B80])


if __name__ == "__main__":
    unittest.main()
