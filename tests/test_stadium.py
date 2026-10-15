"""Run the Lab controller against the native Stadium transition state machine.

DVD completion, rendering, particles and terrain allocation are fixtures; the
actual DOL drives all six transition states, scaling, timing and terrain swaps.
"""
from pathlib import Path
import struct
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "build/test-deps"))
from test_score_storage import NativeMachine
from unicorn import ppc_const as reg


def build():
    subprocess.run([
        "C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe", "-O2", "-w",
        "-ffreestanding", "-fno-builtin", "-msdata=none", "-mcpu=750", "-nostdlib",
        "-Wl,-Ttext=0x80600000,-e,TestStadiumInit,--unresolved-symbols=ignore-all,"
        "--defsym,Stage_StadiumUpdate=0x801D4548",
        "tests/stadium_probe.c", "-o", "build/stadium-test.elf"
    ], cwd=ROOT, check=True)


class Machine(NativeMachine):
    def __init__(self, hazards_disabled=1, stage=3):
        super().__init__()
        elf = (ROOT / "build/stadium-test.elf").read_bytes()
        offset = struct.unpack_from(">I", elf, 0x20)[0]
        stride, count = struct.unpack_from(">HH", elf, 0x2E)
        sections = [struct.unpack_from(">10I", elf, offset + i * stride) for i in range(count)]
        self.symbols = {}
        for section in sections:
            _, kind, flags, address, at, size, link, _, _, entry = section
            if flags & 2 and size and kind != 8:
                self.cpu.mem_write(address, elf[at:at + size])
            if kind == 2:
                strings = sections[link]
                names = elf[strings[4]:strings[4] + strings[5]]
                for index in range(0, size, entry):
                    name, value, *_ = struct.unpack_from(">IIIBBH", elf, at + index)
                    self.symbols[names[name:].split(b"\0", 1)[0].decode()] = value
        # Native yakumono pointer: use short but nonzero transition durations.
        self.cpu.mem_write(0x804D6978, struct.pack(">I", 0x805D0000))
        self.cpu.mem_write(0x805D0000, struct.pack(">7I", 2, 2, 2, 2, 1, 2, 1))
        for address in (0x8018841C, 0x80225194):
            self.cpu.mem_write(address, bytes.fromhex("386000004e800020"))
        for address in (0x801C53EC, 0x801D435C, 0x801C7A04, 0x80057528,
                        0x80058560, 0x801C4A08, 0x801C65B0, 0x800575B0):
            self.cpu.mem_write(address, bytes.fromhex("4e800020"))
        # Camera_RequestQuake (resolve from this repository's native symbols).
        for line in (ROOT / "MexTK/melee.link").read_text().splitlines():
            if line.endswith(":Camera_Shake") or line.endswith(":Camera_RequestQuake"):
                self.cpu.mem_write(int(line.split(":")[0], 16), bytes.fromhex("4e800020"))
        self.jump(0x801D42B8, self.symbols["TestStadiumLoaded"])
        self.jump(0x801D10F8, self.symbols["TestStadiumCreate"])
        self.fn("TestStadiumInit", stage, hazards_disabled)

    def jump(self, at, target):
        self.cpu.mem_write(at, struct.pack(">4I", 0x3D800000 | target >> 16,
                            0x618C0000 | target & 0xFFFF, 0x7D8903A6, 0x4E800420))

    def fn(self, name, *args):
        try:
            return self.call(self.symbols[name], *args)
        except Exception as error:
            pc = self.cpu.reg_read(reg.UC_PPC_REG_PC)
            raise AssertionError(f"{name} failed at {pc:08X}, instruction "
                                 f"{bytes(self.cpu.mem_read(pc, 4)).hex()}") from error

    def info(self, index):
        return self.fn("TestStadiumInfo", index)

    def select(self, value):
        self.fn("TestStadiumSelect", value)
        expected = (5, 3, 4, 6, 9)[value]
        seen = []
        for _ in range(200):
            self.fn("TestStadiumTick")
            seen.append((self.info(0), self.info(6)))
            if seen[-1] == (0, expected):
                return seen
        raise AssertionError(f"Transformation did not settle: {seen[-10:]}")


class StadiumTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build()

    def test_every_form_and_return_to_normal_with_hazards_on_and_off(self):
        for hazards in (0, 1):
            with self.subTest(hazards_disabled=hazards):
                m = Machine(hazards)
                self.assertEqual((m.info(2), m.info(3)), (1, 1))
                for choice, file in ((1, "GrPs1.dat"), (2, "GrPs2.dat"),
                                     (3, "GrPs4.dat"), (4, "GrPs3.dat")):
                    m.select(choice)
                    pointer = m.fn("TestStadiumFile")
                    self.assertEqual(bytes(m.cpu.mem_read(pointer, 12)).split(b"\0", 1)[0].decode(), file)
                    loads = m.info(1)
                    for _ in range(100):
                        m.fn("TestStadiumTick")
                    self.assertEqual(m.info(0), 0)
                    self.assertEqual(m.info(6), (5, 3, 4, 6, 9)[choice])
                    self.assertEqual(m.info(1), loads)
                    m.select(0)
                    self.assertEqual(m.info(6), 5)
                self.assertEqual(m.info(4) > 0, not hazards)
                self.assertGreater(m.info(5), 0)

    def test_transform_to_transform_unloads_through_normal(self):
        m = Machine()
        m.select(1)
        states = m.select(4)
        self.assertIn((0, 5), states)
        self.assertEqual(states[-1], (0, 9))
        self.assertEqual(m.info(1), 2)

    def test_latest_selection_wins_during_load_without_restarting_it(self):
        m = Machine()
        m.fn("TestStadiumSelect", 1)
        m.fn("TestStadiumTick")
        self.assertNotEqual(m.info(0), 0)
        m.fn("TestStadiumSelect", 2)
        m.fn("TestStadiumSelect", 3)
        states = m.select(4)
        self.assertEqual(states[-1], (0, 9))
        self.assertEqual(m.info(1), 2)

    def test_other_stages_are_untouched_and_invalid_choices_ignored(self):
        m = Machine(stage=32)
        self.assertEqual((m.info(2), m.info(3)), (0, 0))
        self.assertEqual(m.fn("TestStadiumSelect", 99), 0)
        self.assertEqual(m.fn("TestStadiumSelect", 0xFFFFFFFF), 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
