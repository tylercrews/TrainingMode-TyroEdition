"""Run Techchase's production C menu/positioning/punish logic as PowerPC code."""
from pathlib import Path
import re
import struct
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "build/test-deps"))
from unicorn import Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE
from unicorn import ppc_const as reg


def build():
    compiler = Path("C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe")
    source = ROOT / "tests/techchase_probe.c"
    exports = re.findall(r"\b(TestTC\w+)\(", source.read_text())
    subprocess.run([
        str(compiler) if compiler.exists() else "powerpc-eabi-gcc", "-O2", "-w",
        "-ffreestanding", "-fno-builtin", "-msdata=none", "-mcpu=750", "-nostdlib",
        "-ffunction-sections", "-fdata-sections",
        "-Wl,-Ttext=0x101000,-e,TestTCInit,--gc-sections,--unresolved-symbols=ignore-all",
        *["-Wl,--undefined=" + name for name in set(exports)], str(source),
        "-o", str(ROOT / "build/techchase-test.elf")
    ], cwd=ROOT, check=True)


class Machine:
    def __init__(self):
        self.cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
        self.cpu.mem_map(0x100000, 0x100000)
        self.cpu.mem_map(0x80000000, 0x500000)
        self.cpu.reg_write(reg.UC_PPC_REG_MSR, 1 << 13)
        elf = (ROOT / "build/techchase-test.elf").read_bytes()
        shoff = struct.unpack_from(">I", elf, 0x20)[0]
        stride, count = struct.unpack_from(">HH", elf, 0x2E)
        sections = [struct.unpack_from(">10I", elf, shoff + i * stride) for i in range(count)]
        self.symbols = {}
        for section in sections:
            _, kind, flags, address, offset, size, link, _, _, entry = section
            if flags & 2 and size and kind != 8:
                self.cpu.mem_write(address, elf[offset:offset + size])
            if kind == 2:
                strings = sections[link]
                names = elf[strings[4]:strings[4] + strings[5]]
                for at in range(0, size, entry):
                    name, value, *_ = struct.unpack_from(">IIIBBH", elf, offset + at)
                    self.symbols[names[name:].split(b"\0", 1)[0].decode()] = value
        self.call("TestTCInit")

    def call(self, name, *args):
        self.cpu.reg_write(reg.UC_PPC_REG_1, 0x80400000)
        self.cpu.reg_write(reg.UC_PPC_REG_LR, 0x100000)
        for i, value in enumerate(args, 3):
            self.cpu.reg_write(getattr(reg, "UC_PPC_REG_" + str(i)), value & 0xFFFFFFFF)
        self.cpu.emu_start(self.symbols[name], 0x100000, count=100000)
        value = self.cpu.reg_read(reg.UC_PPC_REG_3)
        return value if value < 0x80000000 else value - 0x100000000

    def line(self, ax, ay, bx, by, flags=0, enabled=1):
        return self.call("TestTCAddLine", *[round(v * 1000) for v in (ax, ay, bx, by)], flags, enabled)

    def position(self, actor):
        return tuple(self.call("TestTCPosition", actor, i) / 1000 for i in (0, 1))

    def native_iasa(self):
        """Execute the real Wait IASA and spotdodge input eligibility routines.

        Other attacks are stubbed inactive; the animation-entry endpoint only
        records success, avoiding an unrelated native fighter-model fixture.
        """
        dol = (ROOT / "build/Start.dol").read_bytes()
        def native(address, size):
            for i in range(18):
                offset, base, length = [struct.unpack_from(">I", dol, at + i * 4)[0] for at in (0, 0x48, 0x90)]
                if base <= address and address + size <= base + length:
                    data = dol[offset + address - base:offset + address - base + size]
                    self.cpu.mem_write(address, data)
                    return data
            raise AssertionError("Native routine missing from DOL")
        def jump(address, target):
            self.cpu.mem_write(address, struct.pack(">4I", 0x3D800000 | target >> 16,
                0x618C0000 | target & 0xFFFF, 0x7D8903A6, 0x4E800420))
        wait = native(0x8008A4D4, 0x170)
        native(0x80099794, 0x78)
        for i in range(0, len(wait), 4):
            word = struct.unpack_from(">I", wait, i)[0]
            if word >> 26 == 18 and word & 1:
                displacement = word & 0x03FFFFFC
                if displacement & 0x02000000:
                    displacement -= 0x04000000
                target = 0x8008A4D4 + i + displacement
                if target != 0x80099794:
                    self.cpu.mem_write(target, bytes.fromhex("386000004e800020"))
        jump(self.symbols["TestTCNativeWaitIASA"], 0x8008A4D4)
        jump(0x80099894, self.symbols["TestTCNativeSpotdodgeEnter"])
        self.cpu.reg_write(reg.UC_PPC_REG_13, 0x804DFA00)
        self.cpu.mem_write(0x804DFA00 - 0x514C, struct.pack(">I", 0x80403000))
        self.cpu.mem_write(0x80403000 + 0x314, struct.pack(">fI", -0.7, 4))
        calls = []
        self.cpu.hook_add(UC_HOOK_CODE, lambda _, address, size, data: calls.append(address),
                         begin=0x80099794, end=0x80099794)
        return calls


def native_floors(filename):
    """Read the main ISO's actual DAT collider flags/vertices and stage scale."""
    with (ROOT / "melee.iso").open("rb") as iso:
        iso.seek(0x424)
        offset, size = struct.unpack(">II", iso.read(8))
        iso.seek(offset)
        fst = iso.read(size)
        count = struct.unpack_from(">I", fst, 8)[0]
        names = fst[12 * count:]
        for i in range(1, count):
            kind_name, file_offset, file_size = struct.unpack_from(">3I", fst, 12 * i)
            if names[kind_name & 0xFFFFFF:].split(b"\0", 1)[0].decode() == filename:
                iso.seek(file_offset)
                dat = iso.read(file_size)
                break
        else:
            raise AssertionError(filename + " missing from base ISO")
    _, size, reloc_count, public_count, external_count = struct.unpack_from(">5I", dat)
    root_offset = 32 + size + 4 * reloc_count
    string_offset = root_offset + 8 * (public_count + external_count)
    roots = {}
    for i in range(public_count):
        data, name = struct.unpack_from(">II", dat, root_offset + i * 8)
        roots[dat[string_offset + name:].split(b"\0", 1)[0].decode()] = 32 + data
    vertices, vertex_count, lines, line_count = struct.unpack_from(">4I", dat, roots["coll_data"])
    scale = struct.unpack_from(">f", dat, roots["grGroundParam"])[0]
    result = []
    for i in range(line_count):
        line = 32 + lines + i * 16
        start, end = struct.unpack_from(">hh", dat, line)
        high_flags, low_flags = struct.unpack_from(">HH", dat, line + 12)
        if not high_flags & 1 or not (0 <= start < vertex_count and 0 <= end < vertex_count):
            continue
        a = struct.unpack_from(">2f", dat, 32 + vertices + start * 8)
        b = struct.unpack_from(">2f", dat, 32 + vertices + end * 8)
        result.append((i, *[v * scale for v in (*a, *b)], low_flags))
    return result


class TechchaseTests(unittest.TestCase):
    def setUp(self):
        self.m = Machine()

    def test_presets_and_only_preset_is_persisted(self):
        expected = [(40, 30, 30, 0), (25, 25, 25, 25), (100, 0, 0, 0),
                    (0, 100, 0, 0), (0, 0, 100, 0), (0, 0, 0, 100),
                    (50, 0, 50, 0), (0, 50, 50, 0), (60, 15, 15, 10)]
        self.assertEqual(self.m.call("TestTCPresetCount"), len(expected))
        self.m.call("TestTCSetChance", 4, 81)  # Miss-tech wait stays session-local.
        for i, percentages in enumerate(expected):
            self.assertEqual(self.m.call("TestTCSetPreset", i), i)
            self.assertEqual(tuple(self.m.call("TestTCChance", j) for j in range(4)), percentages)
            self.assertEqual(self.m.call("TestTCChance", 4), 81)
            self.assertEqual(self.m.call("TestTCLastWriteField"), 27)
            self.assertEqual(self.m.call("TestTCLastWriteIndex"), 0)
        count = self.m.call("TestTCWriteCount")
        for i, percentage in enumerate((71, 0, 100, 37, 12, 83, 0, 100, 42)):
            self.m.call("TestTCSetChance", i, percentage)
            if i < 4:
                self.assertEqual(sum(self.m.call("TestTCChance", j) for j in range(4)), 100)
            if i >= 5:
                self.assertEqual(sum(self.m.call("TestTCChance", j) for j in range(5, 9)), 100)
        self.assertEqual(self.m.call("TestTCWriteCount"), count)
        self.m.call("TestTCSetPreset", 8)
        self.assertEqual(tuple(self.m.call("TestTCChance", j) for j in range(4)), expected[8])
        count = self.m.call("TestTCWriteCount")
        self.assertEqual(self.m.call("TestTCReloadChances"), count)
        self.assertEqual(tuple(self.m.call("TestTCChance", j) for j in range(4)), expected[8])
        self.assertEqual(tuple(self.m.call("TestTCChance", j) for j in range(4, 9)), (0, 25, 25, 25, 25))

    def test_native_static_legal_stage_platforms_and_grounded_human(self):
        # Stadium hazards are disabled by the event, leaving its normal group
        # (floor lines 34-36, 51-54) active. Randall is removed on Yoshi's.
        for filename, stage_id, normal_lines in [
            ("GrSt.dat", 8, {2, 3, 4, 5, 6, 1}),
            ("GrNBa.dat", 31, None), ("GrOp.dat", 28, None),
            ("GrPs.dat", 3, {34, 35, 36, 51, 52, 53, 54}),
            ("GrNLa.dat", 32, None)
        ]:
            with self.subTest(stage=filename):
                self.m.call("TestTCStage", stage_id)
                for index, ax, ay, bx, by, flags in native_floors(filename):
                    self.m.line(ax, ay, bx, by, flags, normal_lines is None or index in normal_lines)
                self.m.call("TestTCChooseLocation", 0)
                self.assertEqual(self.m.position(0)[0], -5)
                self.assertEqual(self.m.position(1)[0], 5)
                for location in (1, 2, 3):
                    actual = self.m.call("TestTCChooseLocation", location)
                    expected = 0 if filename == "GrNLa.dat" or filename == "GrPs.dat" and location == 3 else location
                    self.assertEqual(actual, expected)
                    hmn, cpu = self.m.position(0), self.m.position(1)
                    self.assertAlmostEqual(abs(cpu[0] - hmn[0]), 10, places=3)
                    self.assertLess(hmn[1], 1)
                    if actual:
                        self.assertGreater(cpu[1], 40)
                    self.assertEqual(self.m.call("TestTCFacing", 0), 1 if cpu[0] > hmn[0] else -1)
                self.assertEqual(self.m.call("TestTCLocationDisabled"), filename == "GrNLa.dat")
                self.assertEqual(self.m.call("TestTCWriteCount"), 0)  # No save writes for per-stage fallback.

    def test_dynamic_fountain_geometry_and_missing_platforms(self):
        # These are live transformed positions, including changing heights.
        # The native DAT's three local platform floors are all 0x0100.
        platforms = [line for line in native_floors("GrIz.dat") if line[-1] & 0x100]
        self.assertEqual(len(platforms), 3)
        self.m.call("TestTCStage", 2)
        self.m.line(-65, 0, 65, 0)
        self.m.line(-49, 11, -21, 11, 0x100)
        self.m.line(21, 35, 49, 35, 0x100)
        self.m.line(-14, 43, 14, 43, 0x100)
        for location, expected in [(1, (-35, 38)), (2, (35, 62)), (3, (5, 70))]:
            self.assertEqual(self.m.call("TestTCChooseLocation", location), location)
            self.assertEqual(self.m.position(1), expected)
        self.m.call("TestTCStage", 2)
        self.m.line(-65, 0, 65, 0)
        self.m.line(-49, 11, -21, 11, 0x100, 0)
        self.assertEqual(self.m.call("TestTCChooseLocation", 1), 0)
        self.assertEqual(self.m.call("TestTCLocationAvailable", 1), 0)
        self.assertEqual(self.m.position(0), (-5, 0))
        # A dynamic floor (0x0400) is not a one-way platform (0x0100).
        self.m.call("TestTCStage", 2)
        self.m.line(-65, 0, 65, 0)
        self.m.line(-49, 20, -21, 20, 0x400)
        self.assertEqual(self.m.call("TestTCChooseLocation", 1), 0)

    def test_spotdodge_first_wait_frame_and_reset_only_after_completion(self):
        self.m.call("TestTCDodgeReset")
        self.assertEqual(self.m.call("TestTCDodgeStep", 1, 0), 0)  # damage fall
        for _ in range(30):
            self.assertEqual(self.m.call("TestTCDodgeStep", 1, 1), 0)  # full tech
        self.assertEqual(self.m.call("TestTCDodgeCalls"), 0)
        self.assertEqual(self.m.call("TestTCDodgeStep", 1, 2), 1)  # first Wait
        self.assertEqual(self.m.call("TestTCDodgeCalls"), 1)
        self.assertEqual(self.m.call("TestTCDodgeState"), 1)
        self.assertEqual(self.m.call("TestTCDodgeInputs"), 1)
        for _ in range(24):
            self.assertEqual(self.m.call("TestTCDodgeStep", 1, 3), 0)
        self.assertEqual(self.m.call("TestTCDodgeStep", 1, 2), 2)  # timer starts now
        self.assertEqual(self.m.call("TestTCDodgePhase"), 3)
        self.assertEqual(self.m.call("TestTCDodgeCalls"), 1)  # never chain dodges

    def test_spotdodge_off_preserves_existing_wait_reset(self):
        self.m.call("TestTCDodgeReset")
        self.assertEqual(self.m.call("TestTCDodgeStep", 0, 1), 0)
        self.assertEqual(self.m.call("TestTCDodgeStep", 0, 2), 2)
        self.assertEqual(self.m.call("TestTCDodgeCalls"), 0)

    def test_actual_native_wait_iasa_accepts_first_frame_processed_input(self):
        calls = self.m.native_iasa()
        self.m.call("TestTCDodgeReset")
        self.assertEqual(self.m.call("TestTCDodgeStep", 1, 1), 0)
        self.assertEqual(calls, [])
        self.assertEqual(self.m.call("TestTCDodgeStep", 1, 2), 1)
        self.assertEqual(calls, [0x80099794])
        self.assertEqual(self.m.call("TestTCDodgeState"), 1)
        self.assertEqual(self.m.call("TestTCDodgeInputs"), 1)
        self.assertEqual(self.m.call("TestTCDodgeStep", 1, 3), 0)
        self.assertEqual(self.m.call("TestTCDodgeStep", 1, 2), 2)
        self.assertEqual(calls, [0x80099794])


if __name__ == "__main__":
    build()
    unittest.main(verbosity=2)
