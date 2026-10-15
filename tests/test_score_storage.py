"""Execute native DOL save, checksum, score-access and reset boundaries.

Run with Unicorn available (the settings test dependencies also suffice):
    PYTHONPATH=build/tangibility-test-deps python tests/test_score_storage.py -v

No card file is written. CARDWrite is intercepted after the native code has
copied, checksummed and encrypted the payload, and the native decoder verifies
the captured sector. Real card/Dolphin error handling remains a separate check.
"""
from pathlib import Path
import re
import struct
import unittest

from unicorn import Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE
from unicorn import ppc_const as reg

ROOT = Path(__file__).resolve().parents[1]
BASE = 0x8045A6C0
SCORE_START, SPARE_START, SCORE_END = 0x1A70, 0x1AE0, 0x1B3C
SPARE_END, SPARE_SIZE = 0x1B38, 88
SAVE_START, SAVE_SIZE = 0x1868, 0x1790
STOP = 0x100000


def gpr(index):
    return getattr(reg, f"UC_PPC_REG_{index}")


class NativeMachine:
    def __init__(self):
        self.cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
        self.cpu.mem_map(STOP, 0x1000)
        self.cpu.mem_map(0x80000000, 0x1800000)
        self.dol = (ROOT / "build/Start.dol").read_bytes()
        self.sections = []
        for index in range(18):
            offset, base, size = [struct.unpack_from(">I", self.dol, at + index * 4)[0]
                                  for at in (0, 0x48, 0x90)]
            if size:
                self.sections.append((offset, base, size))
                self.cpu.mem_write(base, self.dol[offset:offset + size])
        self.cpu.reg_write(gpr(1), 0x80580000)
        self.cpu.reg_write(gpr(2), 0x804DF9E0)
        self.cpu.reg_write(gpr(13), 0x804DB6A0)
        self.cpu.reg_write(reg.UC_PPC_REG_MSR, 1 << 13)
        self.cpu.mem_write(0x804D3EE0, struct.pack(">I", BASE))

    def call(self, address, *arguments, stop=STOP, count=1000000):
        self.cpu.reg_write(reg.UC_PPC_REG_LR, stop)
        for index, value in enumerate(arguments, 3):
            self.cpu.reg_write(gpr(index), value & 0xFFFFFFFF)
        self.cpu.emu_start(address, stop, count=count)
        if self.cpu.reg_read(reg.UC_PPC_REG_PC) != stop:
            raise AssertionError(f"Native function did not finish: {address:08X}")
        return self.cpu.reg_read(gpr(3))

    def load_services(self):
        """Apply the native MEX relocation rules to the optimized shared DAT."""
        dat = (ROOT / "build/settings-relocated-test.dat").read_bytes()
        _, data_size, reloc_count, root_count, ref_count = struct.unpack_from(">5I", dat)
        nodes = 32 + data_size + reloc_count * 4
        strings = nodes + (root_count + ref_count) * 8
        for index in range(root_count):
            offset, name = struct.unpack_from(">II", dat, nodes + index * 8)
            if dat[strings + name:].split(b"\0", 1)[0] == b"tmFunction":
                code, table, count, exports, num_exports, size = struct.unpack_from(">6I", dat, 32 + offset)
                break
        else:
            raise AssertionError("Shared production DAT root missing")
        base = 0x80700000
        payload = bytearray(dat[32 + code:32 + code + size])
        for index in range(count):
            word, target = struct.unpack_from(">II", dat, 32 + table + index * 8)
            kind, offset = word >> 24, word & 0xFFFFFF
            if target & 0xF0000000 != 0x80000000:
                target = (target + base) & 0xFFFFFFFF
            if kind == 1:
                struct.pack_into(">I", payload, offset, target)
            elif kind == 4:
                struct.pack_into(">H", payload, offset, target & 0xFFFF)
            elif kind == 6:
                struct.pack_into(">H", payload, offset, ((target + 0x8000) >> 16) & 0xFFFF)
            elif kind == 10:
                old = struct.unpack_from(">I", payload, offset)[0]
                struct.pack_into(">I", payload, offset, old | ((target - base - offset) & 0x03FFFFFC))
            elif kind == 26:
                struct.pack_into(">I", payload, offset, (target - base - offset) & 0xFFFFFFFF)
            else:
                raise AssertionError(f"Unknown native MEX relocation {kind}")
        self.cpu.mem_write(base, bytes(payload))
        functions = {}
        for index in range(num_exports):
            slot, offset = struct.unpack_from(">II", dat, 32 + exports + index * 8)
            functions[slot] = base + offset
            self.cpu.mem_write(0x804DF9E0 - 200 + slot * 4, struct.pack(">I", base + offset))
        self.cpu.mem_write(0x80000000, b"TYRE01")
        self.cpu.mem_write(BASE + 0x1F24, bytes(44))
        return functions

    def install_hook(self, address, allocation):
        data = (ROOT / "build/settings-codes-test.gct").read_bytes()
        needle = struct.pack(">I", 0xC2000000 | (address & 0x01FFFFFF))
        offset = data.find(needle)
        if offset < 0:
            raise AssertionError(f"Native audit hook missing at {address:08X}")
        lines = struct.unpack_from(">I", data, offset + 4)[0]
        body = bytearray(data[offset + 8:offset + 8 + lines * 8])
        struct.pack_into(">I", body, len(body) - 4,
                         0x48000000 | ((address + 4 - allocation - len(body) + 4) & 0x03FFFFFC))
        self.cpu.mem_write(allocation, bytes(body))
        self.cpu.mem_write(address, struct.pack(">I", 0x48000000 | ((allocation - address) & 0x03FFFFFC)))


class ScoreStorageAudit(unittest.TestCase):
    def test_catalogue_retains_28_legacy_scores_and_reserves_slot_50_for_ledgechase(self):
        source = (ROOT / "src/events.c").read_text()
        counts = []
        for array in ("Minigames_Events", "General_Events", "CharacterSpecific_Events"):
            body = re.search(r"static EventDesc \*" + array + r"\[\] = \{(.*?)\};",
                             source, re.S).group(1)
            counts.append(len(re.findall(r"&\w+", body)))
        self.assertEqual(counts, [5, 17, 7])
        self.assertEqual(SCORE_START + 28 * 4, SPARE_START)
        self.assertEqual(SCORE_START + 50 * 4, SPARE_END)
        self.assertEqual((SCORE_END - SCORE_START) // 4, 51)
        self.assertEqual(SPARE_END - SPARE_START, SPARE_SIZE)

    def test_native_manifest_includes_all_88_spare_bytes_and_the_new_score(self):
        machine = NativeMachine()
        manifest = bytes(machine.cpu.mem_read(0x803BAB74, 10 * 12))
        self.assertEqual(struct.unpack_from(">III", manifest, 12), (SAVE_SIZE, 0, 0))
        self.assertEqual(machine.call(0x8015CC40), BASE + SAVE_START)
        self.assertGreaterEqual(SPARE_START, SAVE_START)
        self.assertLessEqual(SCORE_END, SAVE_START + SAVE_SIZE)
        self.assertEqual((SPARE_START - SAVE_START, SCORE_END - SAVE_START), (0x278, 0x2D4))
        self.assertEqual(SPARE_END - SAVE_START, 0x2D0)

    def test_unpatched_native_score_access_can_reach_spare_slots(self):
        machine = NativeMachine()
        machine.cpu.mem_write(BASE + SCORE_START - 4, b"\xA5" * 212)
        for slot in (0, 27, 28, 50):
            value = 0xABCD0000 | slot
            machine.call(0x8015CF70, slot, value)
            self.assertEqual(machine.call(0x8015CF5C, slot), value)
        self.assertEqual(bytes(machine.cpu.mem_read(BASE + SCORE_START - 4, 4)), b"\xA5" * 4)
        self.assertEqual(bytes(machine.cpu.mem_read(BASE + SCORE_END, 4)), b"\xA5" * 4)

    def test_native_records_clear_wipes_spare_but_stops_before_1b40(self):
        machine = NativeMachine()
        machine.cpu.mem_write(BASE + 0x1A64, b"\xA5" * 224)
        machine.call(0x8015EEC8)
        self.assertEqual(bytes(machine.cpu.mem_read(BASE + 0x1A64, 4)), b"\xA5" * 4)
        self.assertEqual(bytes(machine.cpu.mem_read(BASE + 0x1A68, 216)), bytes(216))
        self.assertEqual(bytes(machine.cpu.mem_read(BASE + 0x1B40, 4)), b"\xA5" * 4)

    def test_native_new_save_initialization_intentionally_clears_spare(self):
        machine = NativeMachine()
        machine.cpu.mem_write(BASE + SAVE_START - 4, b"\xA5" * (0x448 + 8))
        machine.call(0x8015F77C, stop=0x8015F78C)
        self.assertEqual(bytes(machine.cpu.mem_read(BASE + SAVE_START - 4, 4)), b"\xA5" * 4)
        self.assertEqual(bytes(machine.cpu.mem_read(BASE + SAVE_START, 0x448)), bytes(0x448))
        self.assertEqual(bytes(machine.cpu.mem_read(BASE + SAVE_START + 0x448, 4)), b"\xA5" * 4)

    def encoded_main_sector(self):
        machine = NativeMachine()
        state, sector, payload = 0x80504000, 0x80500000, 0x80508000
        data = bytes((index * 37 + 11) & 255 for index in range(SAVE_SIZE))
        machine.cpu.mem_write(payload, data)
        machine.cpu.mem_write(state, bytes(0x800))
        machine.cpu.mem_write(state, struct.pack(">I", sector))
        machine.cpu.mem_write(state + 8, struct.pack(">I", 0x2000))
        captured = []

        def card_write(cpu, address, size, context):
            if address == 0x80358204:
                buf, length = cpu.reg_read(gpr(4)), cpu.reg_read(gpr(5))
                captured.append(bytes(cpu.mem_read(buf, length)))
                cpu.reg_write(gpr(3), 0)
                cpu.reg_write(reg.UC_PPC_REG_PC, cpu.reg_read(reg.UC_PPC_REG_LR))

        machine.cpu.hook_add(UC_HOOK_CODE, card_write, begin=0x80358204, end=0x80358204)
        self.assertEqual(machine.call(0x803ACFC0, state, 1, 1, 7, payload, SAVE_SIZE, 1), 0)
        self.assertEqual(len(captured), 1)
        self.assertEqual(len(captured[0]), 0x2000)
        return machine, sector, data, captured[0]

    def test_native_serialization_crypto_round_trip_keeps_spare_and_neighbors(self):
        machine, sector, data, encoded = self.encoded_main_sector()
        machine.cpu.mem_write(sector, encoded)
        self.assertEqual(machine.call(0x803B31CC, sector, 0x2000), 0)
        decoded = bytes(machine.cpu.mem_read(sector + 0x20, SAVE_SIZE))
        self.assertEqual(decoded, data)
        self.assertEqual(decoded[0x278:0x2D0], data[0x278:0x2D0])
        self.assertEqual(decoded[0x274:0x278] + decoded[0x2D0:0x2D8],
                         data[0x274:0x278] + data[0x2D0:0x2D8])

    def test_native_checksum_covers_first_middle_and_last_spare_bytes(self):
        machine, sector, data, encoded = self.encoded_main_sector()
        digest = encoded[:16]
        # Change the plain serialized payload, recompute using the actual DOL
        # checksum routine, and prove each part of the spare range contributes.
        self.assertEqual(machine.call(0x803B31CC, sector, 0x2000), 0)
        plain = bytes(machine.cpu.mem_read(sector, 0x2000))
        output = 0x80503000
        for offset in (0x278, 0x2A5, 0x2CF, 0x2D3):
            changed = bytearray(plain)
            changed[0x20 + offset] ^= 1
            machine.cpu.mem_write(sector, bytes(changed))
            machine.call(0x803B2B20, sector + 16, 0x2000 - 16, output)
            self.assertNotEqual(bytes(machine.cpu.mem_read(output, 16)), digest)
        # Native load rejects a changed ciphertext byte in the spare range.
        changed = bytearray(encoded)
        changed[0x20 + 0x278] ^= 1
        machine.cpu.mem_write(sector, bytes(changed))
        self.assertEqual(machine.call(0x803B31CC, sector, 0x2000), 0xFFFFFFFF)

    def test_protected_score_hooks_keep_spare_bytes_and_all_29_stable_scores(self):
        machine = NativeMachine()
        functions = machine.load_services()
        self.assertIn(43, functions)
        machine.install_hook(0x8015CF60, 0x80600000)
        machine.install_hook(0x8015CF74, 0x80602000)
        canary = bytes((index * 13 + 3) & 255 for index in range(SPARE_SIZE))
        machine.cpu.mem_write(BASE + SPARE_START, canary)
        for page, count, first in ((0, 5, 0), (1, 17, 5), (2, 7, 21)):
            machine.call(functions[28], 2, 0, page)
            for event in range(count):
                slot = 50 if page==1 and event==16 else first+event
                value = 0xAABB0000 | slot
                machine.call(0x8015CF70, event, value)
                self.assertEqual(machine.call(0x8015CF5C, event), value)
                self.assertEqual(machine.call(functions[43], event), BASE + SCORE_START + slot * 4)
            for event in (count, count + 1, 28, 50, 51, 0xFFFFFFFF):
                before = bytes(machine.cpu.mem_read(BASE + SCORE_START, 204))
                self.assertEqual(machine.call(functions[43], event), 0)
                self.assertEqual(machine.call(0x8015CF5C, event), 0)
                machine.call(0x8015CF70, event, 0xFFFFFFFF)
                self.assertEqual(bytes(machine.cpu.mem_read(BASE + SCORE_START, 204)), before)
        self.assertEqual(bytes(machine.cpu.mem_read(BASE + SPARE_START, SPARE_SIZE)), canary)
        for slot in (*range(28),50):
            self.assertEqual(struct.unpack('>I',machine.cpu.mem_read(BASE+SCORE_START+slot*4,4))[0],
                             0xAABB0000|slot)

    def test_protected_native_records_reset_preserves_only_tyro_spare_bytes(self):
        machine = NativeMachine()
        functions = machine.load_services()
        self.assertIn(42, functions)
        machine.install_hook(0x8015EF14, 0x80604000)
        for identity, preserve in ((b"TYRE01", True), (b"GTME01", False),
                                   (b"TYRE02", False), (b"GALE01", False)):
            machine.cpu.mem_write(0x80000000, identity)
            machine.cpu.mem_write(BASE + 0x1A64, b"\xA5" * 224)
            machine.call(0x8015EEC8)
            self.assertEqual(bytes(machine.cpu.mem_read(BASE + 0x1A64, 4)), b"\xA5" * 4)
            self.assertEqual(bytes(machine.cpu.mem_read(BASE + 0x1A68, 120)), bytes(120))
            self.assertEqual(bytes(machine.cpu.mem_read(BASE + SPARE_START, SPARE_SIZE)),
                             b"\xA5" * SPARE_SIZE if preserve else bytes(SPARE_SIZE))
            self.assertEqual(bytes(machine.cpu.mem_read(BASE + SPARE_END, 8)), bytes(8))
            self.assertEqual(bytes(machine.cpu.mem_read(BASE + 0x1B40, 4)), b"\xA5" * 4)

    def test_future_core_and_extension_protect_opaque_old_slot_50_from_score_and_reset(self):
        for mode in ('extension','core'):
            machine=NativeMachine(); functions=machine.load_services()
            for address, allocation in ((0x8015CF60,0x80600000),(0x8015CF74,0x80602000),
                                        (0x8015EF14,0x80604000)):
                machine.install_hook(address,allocation)
            machine.call(functions[28],2,0,1)
            opaque=b'TYEX\x03\x54\xA5\x5A'+bytes(range(84))
            if mode=='core':
                opaque=b'TYEX\x02\x08\xA5\x5A'+bytes(range(84))
                core=bytearray(machine.cpu.mem_read(BASE+0x1F24,44))
                core[10]&=0x3F
                machine.cpu.mem_write(BASE+0x1F24,bytes(core))
            machine.cpu.mem_write(BASE+SPARE_START,opaque)
            core=bytes(machine.cpu.mem_read(BASE+0x1F24,44))
            self.assertEqual(machine.call(functions[43],16),0)
            self.assertEqual(machine.call(0x8015CF5C,16),0)
            machine.call(0x8015CF70,16,0xFFFFFFFF)
            self.assertEqual(bytes(machine.cpu.mem_read(BASE+SPARE_START,92)),opaque)
            # Existing scores still operate while the new score is quarantined.
            machine.call(0x8015CF70,0,123)
            self.assertEqual(machine.call(0x8015CF5C,0),123)
            machine.call(0x8015EEC8)
            self.assertEqual(bytes(machine.cpu.mem_read(BASE+SPARE_START,92)),opaque)
            self.assertEqual(bytes(machine.cpu.mem_read(BASE+0x1B3C,4)),bytes(4))
            self.assertEqual(bytes(machine.cpu.mem_read(BASE+0x1F24,44)),core)

    def test_recognized_v1_v2_and_v3_records_reset_score_50_without_migrating_saved_bytes(self):
        for header in (b'TYEX\x01\x04\xA5\x5A', b'TYEX\x02\x08\xA5\x5A', b'TYEX\x03\x08\xA5\x5A'):
            machine=NativeMachine(); functions=machine.load_services()
            machine.call(functions[28],2,0,1)
            machine.install_hook(0x8015EF14,0x80604000)
            saved=header+bytes(range(SPARE_SIZE-len(header)))
            machine.cpu.mem_write(BASE+SPARE_START,saved+b'\xFF'*8)
            core=bytes(machine.cpu.mem_read(BASE+0x1F24,44))
            machine.call(0x8015EEC8)
            self.assertEqual(bytes(machine.cpu.mem_read(BASE+SPARE_START,SPARE_SIZE)),saved)
            self.assertEqual(bytes(machine.cpu.mem_read(BASE+SPARE_END,8)),bytes(8))
            self.assertEqual(bytes(machine.cpu.mem_read(BASE+0x1F24,44)),core)

    def test_score_and_reset_hooks_preserve_native_register_context(self):
        machine = NativeMachine()
        functions = machine.load_services()
        machine.call(functions[28], 2, 0, 1)
        for address, allocation, arguments, stop, result in (
                (0x8015CF60, 0x80600000, (0,), STOP, 0x12345678),
                (0x8015CF74, 0x80602000, (0, 77), STOP, None),
                (0x8015EF14, 0x80604000, (BASE + 0x1A68, 216), 0x8015EF18, None)):
            machine.install_hook(address, allocation)
            machine.cpu.mem_write(BASE + SCORE_START + 5 * 4, struct.pack(">I", 0x12345678))
            for index in range(32):
                if index not in (1, 2, 13):
                    machine.cpu.reg_write(gpr(index), 0x34560000 + index)
            for index, argument in enumerate(arguments, 3):
                machine.cpu.reg_write(gpr(index), argument)
            before = [machine.cpu.reg_read(gpr(index)) for index in range(32)]
            machine.cpu.reg_write(reg.UC_PPC_REG_CR, 0xABCD1234)
            machine.cpu.reg_write(reg.UC_PPC_REG_CTR, 0x12345678)
            fp_regs = [getattr(reg, f"UC_PPC_REG_FPR{index}") for index in range(32)]
            for index, fp in enumerate(fp_regs):
                machine.cpu.reg_write(fp, 0x3FF0000000000000 + index)
            machine.call(address, *arguments, stop=stop)
            for index, value in enumerate(before):
                expected = result if index == 3 and result is not None else value
                self.assertEqual(machine.cpu.reg_read(gpr(index)), expected, f"{address:08X}: r{index}")
            self.assertEqual(machine.cpu.reg_read(reg.UC_PPC_REG_CR), 0xABCD1234)
            self.assertEqual(machine.cpu.reg_read(reg.UC_PPC_REG_CTR), 0x12345678)
            for index, fp in enumerate(fp_regs):
                self.assertEqual(machine.cpu.reg_read(fp), 0x3FF0000000000000 + index)


if __name__ == "__main__":
    unittest.main()
