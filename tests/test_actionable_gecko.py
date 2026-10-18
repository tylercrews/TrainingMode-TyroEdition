"""Execute the distributed Gecko payload in a big-endian PowerPC emulator.

python scripts/build_actionable_gecko.py
python tests/test_actionable_gecko.py -v
Requires Unicorn (the existing build/test-deps install is supported).
"""
from pathlib import Path
import random
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "build/test-deps"))
sys.path.insert(0, str(ROOT / "build/actionable-gecko/deps"))
from unicorn import Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE
from unicorn import ppc_const as reg

STOP = 0x80004000
OBJ, FT = 0x80BF0000, 0x80C00000
YELLOW, GREEN = 0xFFF000DC, 0x50FF5ADC


def gpr(n):
    return getattr(reg, f"UC_PPC_REG_{n}")


class Machine:
    def __init__(self, allocation=0x81700000):
        self.cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
        self.cpu.mem_map(0x80000000, 0x1800000)
        self.cpu.reg_write(reg.UC_PPC_REG_MSR, 1 << 13)
        self.cpu.reg_write(gpr(1), 0x817F0000)
        self.cpu.reg_write(gpr(2), 0x804DF9E0)
        self.cpu.reg_write(gpr(13), 0x804DB6A0)
        self.cpu.mem_write(OBJ + 0x2C, struct.pack(">I", FT))
        self.w(FT + 8, 1)
        self.w(FT + 0x89C, 1, "f")
        self.w(FT + 0x3E4, 100, "f")
        self.colors = []
        self.boundary = 5
        self.native_length = False
        self.cpu.hook_add(UC_HOOK_CODE, self.native)
        code = (ROOT / "slippi-gecko-codes/actionable/actionable.gct").read_bytes()
        at = 8
        self.allocations = {}
        while at < len(code):
            first, second = struct.unpack_from(">II", code, at)
            at += 8
            if first >> 24 != 0xC2:
                continue
            address = 0x80000000 | (first & 0x1FFFFFF)
            body = bytearray(code[at:at + second * 8])
            at += second * 8
            end = allocation + len(body) - 4
            struct.pack_into(">I", body, len(body) - 4,
                             0x48000000 | ((address + 4 - end) & 0x3FFFFFC))
            self.cpu.mem_write(allocation, bytes(body))
            self.w(address, 0x48000000 | ((allocation - address) & 0x3FFFFFC))
            self.allocations[address] = allocation
            allocation += (len(body) + 31) & ~31

    def w(self, address, value, kind="I"):
        self.cpu.mem_write(address, struct.pack(">" + kind, value))

    def native(self, cpu, address, size, _):
        if address == 0x8006F484 and not self.native_length:
            cpu.reg_write(reg.UC_PPC_REG_FPR1,
                          struct.unpack(">Q", struct.pack(">d", self.boundary))[0])
            cpu.reg_write(reg.UC_PPC_REG_PC, cpu.reg_read(reg.UC_PPC_REG_LR))
        elif address == 0x800805CC:
            # The native renderer's displaced instruction must see the C caller's LR.
            assert cpu.reg_read(gpr(0)) == cpu.reg_read(reg.UC_PPC_REG_LR)
            data = struct.unpack(">I", cpu.mem_read(cpu.reg_read(gpr(3)) + 0x2C, 4))[0]
            selected = 0 if struct.unpack(">I", cpu.mem_read(data + 0x430, 4))[0] else 1
            col = data + 0x408 + selected * 0x80
            color = struct.unpack(">I", cpu.mem_read(col + 0x2C, 4))[0]
            self.colors.append(color if cpu.mem_read(col + 0x7C, 1)[0] & 0x80 else 0)
            cpu.reg_write(reg.UC_PPC_REG_PC, cpu.reg_read(reg.UC_PPC_REG_LR))

    def run(self, address, end=STOP):
        self.cpu.reg_write(reg.UC_PPC_REG_LR, STOP)
        self.cpu.emu_start(address, end, count=100000)
        assert self.cpu.reg_read(reg.UC_PPC_REG_PC) == end

    def xer(self, value=None):
        # Unicorn's direct XER API does not synchronize its lazy SO/OV/CA flags.
        saved = self.cpu.reg_read(gpr(0))
        if value is not None:
            self.cpu.reg_write(gpr(0), value)
        self.cpu.mem_write(0x80005000, bytes.fromhex("7C0103A6" if value is not None else "7C0102A6"))
        self.cpu.emu_start(0x80005000, 0x80005004, count=1)
        result = self.cpu.reg_read(gpr(0))
        self.cpu.reg_write(gpr(0), saved)
        return result

    def tick(self, number, state, frame, instance=0):
        self.w(0x804DB6A0 - 0x49AC, number & 0xFFFFFFFF)
        self.w(FT + 0x10, state)
        self.w(FT + 0x894, frame, "f")
        self.w(FT + 0x2088, instance, "H")
        self.cpu.reg_write(gpr(3), OBJ)
        self.run(0x8006D9EC, 0x8006D9F0)

    def draw(self):
        self.cpu.reg_write(gpr(3), OBJ)
        self.cpu.reg_write(gpr(4), 2)
        self.cpu.reg_write(gpr(5), 1)
        self.run(0x800805C8)
        return self.colors[-1]


class GeckoTests(unittest.TestCase):
    def test_relocation_at_two_unrelated_handler_allocations(self):
        for base in [0x80010000, 0x81700000]:
            with self.subTest(base=hex(base)):
                m = Machine(base)
                m.tick(1, 42, 3)
                self.assertEqual(m.draw(), YELLOW)
                m.tick(2, 42, 4)
                self.assertEqual(m.draw(), YELLOW)
                m.tick(3, 14, 0)
                self.assertEqual(m.draw(), GREEN)
                m.tick(4, 14, 1)
                self.assertEqual(m.draw(), GREEN)
                m.tick(5, 14, 2)
                self.assertEqual(m.draw(), 0)

    def test_actual_injection_preserves_register_context(self):
        m = Machine()
        m.w(FT + 0x10, 42)
        m.w(FT + 0x894, 3, "f")
        rng = random.Random(71)
        for i in range(32):
            if i not in [1, 2, 13]:
                m.cpu.reg_write(gpr(i), rng.getrandbits(32))
        m.cpu.reg_write(gpr(3), OBJ)
        for i in range(32):
            m.cpu.reg_write(getattr(reg, f"UC_PPC_REG_FPR{i}"),
                            struct.unpack(">Q", struct.pack(">d", i + 0.25))[0])
        controls = [reg.UC_PPC_REG_CR, reg.UC_PPC_REG_CTR, reg.UC_PPC_REG_FPSCR]
        for r, value in zip(controls, [0x12345678, 0x12345678, 0]):
            m.cpu.reg_write(r, value)
        m.xer(0xA0000028)
        expected_xer = m.xer()
        expected_gpr = [m.cpu.reg_read(gpr(i)) for i in range(32)]
        expected_fpr = [m.cpu.reg_read(getattr(reg, f"UC_PPC_REG_FPR{i}")) for i in range(32)]
        expected_control = [m.cpu.reg_read(r) for r in controls]
        before = bytes(m.cpu.mem_read(FT, 0x2400))
        m.run(0x8006D9EC, 0x8006D9F0)
        expected_gpr[0] = STOP  # displaced native mflr r0
        self.assertEqual([m.cpu.reg_read(gpr(i)) for i in range(32)], expected_gpr)
        self.assertEqual([m.cpu.reg_read(getattr(reg, f"UC_PPC_REG_FPR{i}")) for i in range(32)], expected_fpr)
        self.assertEqual([m.cpu.reg_read(r) for r in controls], expected_control)
        self.assertEqual(m.xer(), expected_xer)
        self.assertEqual(bytes(m.cpu.mem_read(FT, 0x2400)), before)

    def test_render_restores_all_native_color_bytes_in_both_selected_slots(self):
        for colanim in [0, 10]:
            m = Machine()
            m.cpu.mem_write(FT + 0x408, random.Random(8).randbytes(0x180))
            m.w(FT + 0x430, colanim)
            m.tick(1, 42, 3)
            before = bytes(m.cpu.mem_read(FT, 0x2400))
            for _ in range(3):
                self.assertEqual(m.draw(), YELLOW)
                self.assertEqual(bytes(m.cpu.mem_read(FT, 0x2400)), before)

    def test_rollback_replaces_discarded_future_and_repeated_ticks_are_idempotent(self):
        m = Machine()
        m.tick(1, 42, 3)
        m.tick(2, 42, 4)
        m.tick(3, 14, 0)
        self.assertEqual(m.draw(), GREEN)
        # Rewind before recovery: corrected input enters unsupported hitstun.
        m.tick(2, 87, 0)
        self.assertEqual(m.draw(), 0)
        m.tick(3, 87, 1)
        self.assertEqual(m.draw(), 0)
        # Rewind again and recover directly into a different ordinary action.
        m.tick(2, 42, 4)
        m.tick(3, 20, 0)
        for _ in range(3):
            m.tick(3, 20, 0)
            self.assertEqual(m.draw(), GREEN)
        m.tick(4, 44, 0, 1)
        self.assertEqual(m.draw(), GREEN)

    def test_hitlag_respawn_unknown_and_ring_wrap(self):
        m = Machine()
        m.tick(20, 42, 3)
        self.assertEqual(m.draw(), YELLOW)
        m.cpu.mem_write(FT + 0x221A, b"\x20")
        m.tick(21, 42, 3)
        self.assertEqual(m.draw(), 0)
        m.cpu.mem_write(FT + 0x221A, b"\0")
        m.tick(22, 42, 4)
        self.assertEqual(m.draw(), YELLOW)
        m.w(FT + 8, 2)
        m.tick(23, 14, 0)
        self.assertEqual(m.draw(), 0)
        for frame in range(24, 90):
            m.tick(frame, 14, frame - 23)
        m.tick(90, 42, 3)
        m.tick(91, 42, 4)
        m.tick(92, 14, 0)
        self.assertEqual(m.draw(), GREEN)
        m.tick(93, 347, 0)
        self.assertEqual(m.draw(), 0)

    def test_native_iasa_lookahead_and_lcancel_rate(self):
        m = Machine()
        # Native script wait one frame, then enable IASA, then end.
        script = 0x80C04000
        m.cpu.mem_write(script, bytes.fromhex("04000001 58000000 00000000"))
        m.w(FT + 0x3E4, 0, "f")
        m.w(FT + 0x3E8, 10, "f")
        m.w(FT + 0x3EC, script)
        m.boundary = 40
        m.tick(1, 65, 10)
        self.assertEqual(m.draw(), YELLOW)
        # Scaled aerial landing reaches a ten-frame boundary in two ticks.
        m.boundary = 10
        m.w(FT + 0x89C, 2, "f")
        m.tick(2, 70, 6)
        self.assertEqual(m.draw(), YELLOW)
        m.tick(3, 70, 8)
        self.assertEqual(m.draw(), YELLOW)
        m.tick(4, 14, 0)
        self.assertEqual(m.draw(), GREEN)

    @unittest.skipUnless((ROOT / "melee.iso").is_file(), "Native Melee ISO not available")
    def test_actual_native_animation_length_query(self):
        m = Machine()
        # Execute these functions from the user's vanilla disc, not a fixture
        # reimplementation, including the native main/blend-skeleton selection.
        with (ROOT / "melee.iso").open("rb") as iso:
            self.assertEqual(iso.read(8), b"GALE01\x00\x02")
            iso.seek(0x420)
            dol = struct.unpack(">I", iso.read(4))[0]
            iso.seek(dol)
            header = iso.read(0x100)
            for address, length in [(0x8006F484, 0x44), (0x8000BE40, 0x8C)]:
                for index in range(18):
                    offset, base, size = [struct.unpack_from(">I", header, field + index * 4)[0]
                                          for field in (0, 0x48, 0x90)]
                    if base <= address and address + length <= base + size:
                        iso.seek(dol + offset + address - base)
                        m.cpu.mem_write(address, iso.read(length))
                        break
                else:
                    self.fail(f"Native DOL region missing: {address:x}")
        m.native_length = True
        for blend in [0, 1]:
            jobj, aobj = 0x80C08000 + blend * 0x100, 0x80C09000 + blend * 0x100
            m.w(OBJ + 0x28 if blend == 0 else FT + 0x8AC, jobj)
            m.w(jobj + 0x7C, aobj)
            m.w(aobj + 0xC, 5 + blend * 10, "f")
            m.w(FT + 0x8A4, blend, "f")
            m.tick(1 + blend * 10, 42, 3 + blend * 10)
            self.assertEqual(m.draw(), YELLOW)
            m.tick(2 + blend * 10, 42, 4 + blend * 10)
            self.assertEqual(m.draw(), YELLOW)
            m.tick(3 + blend * 10, 14, 0)
            self.assertEqual(m.draw(), GREEN)


if __name__ == "__main__":
    unittest.main()
