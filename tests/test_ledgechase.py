"""PowerPC regressions for production Ledgechase, native IASA, and relocated DAT."""
from pathlib import Path
import struct
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "build/test-deps"))
from unicorn import Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN
from unicorn import ppc_const as reg


def build():
    compiler = Path("C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe")
    subprocess.run([
        str(compiler) if compiler.exists() else "powerpc-eabi-gcc", "-O2", "-w",
        "-ffreestanding", "-fno-builtin", "-msdata=none", "-mcpu=750", "-nostdlib",
        "-Wl,-Ttext=0x101000,-e,TestLCInit,--unresolved-symbols=ignore-all",
        str(ROOT / "tests/ledgechase_probe.c"), "-o", str(ROOT / "build/ledgechase-test.elf")
    ], cwd=ROOT, check=True)


class Machine:
    def __init__(self):
        self.cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
        self.cpu.mem_map(0x100000, 0x100000)
        self.cpu.mem_map(0x80000000, 0x1800000)
        self.cpu.reg_write(reg.UC_PPC_REG_MSR, 1 << 13)
        self.cpu.reg_write(reg.UC_PPC_REG_13, 0x804DB6A0)
        self.cpu.reg_write(reg.UC_PPC_REG_2, 0x804DF9E0)
        self.symbols = {}
        elf = (ROOT / "build/ledgechase-test.elf").read_bytes()
        shoff = struct.unpack_from(">I", elf, 0x20)[0]
        stride, count = struct.unpack_from(">HH", elf, 0x2E)
        sections = [struct.unpack_from(">10I", elf, shoff + i * stride) for i in range(count)]
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
        self.call("TestLCInit")
        self.think = "TestLCStart"

    def call(self, name, *args):
        self.cpu.reg_write(reg.UC_PPC_REG_1, 0x80400000)
        self.cpu.reg_write(reg.UC_PPC_REG_LR, 0x100000)
        for i, value in enumerate(args, 3):
            self.cpu.reg_write(getattr(reg, "UC_PPC_REG_" + str(i)), value & 0xFFFFFFFF)
        self.cpu.emu_start(self.symbols[name], 0x100000, count=1000000)
        if self.cpu.reg_read(reg.UC_PPC_REG_PC) != 0x100000:
            raise AssertionError("PowerPC call did not return: " + name)
        value = self.cpu.reg_read(reg.UC_PPC_REG_3)
        return value if value < 0x80000000 else value - 0x100000000

    def step(self):
        if self.think == "DAT_Think":
            self.call("TestLCEngineTick")
            self.call(self.think, self.call("TestLCObject"))
        else:
            self.call(self.think)

    def info(self, index):
        return self.call("TestLCInfo", index)

    def text(self, address):
        return bytes(self.cpu.mem_read(address, 96)).split(b"\0", 1)[0].decode()

    def jump(self, address, target):
        self.cpu.mem_write(address, struct.pack(">4I", 0x3D800000 | target >> 16,
            0x618C0000 | target & 0xFFFF, 0x7D8903A6, 0x4E800420))

    def native_dol(self):
        dol = (ROOT / "build/Start.dol").read_bytes()
        for i in range(18):
            offset, base, size = [struct.unpack_from(">I", dol, at + i * 4)[0] for at in (0, 0x48, 0x90)]
            if offset and size:
                self.cpu.mem_write(base, dol[offset:offset + size])
        # Native data initialization overwrites fixed-address pointers. Restore
        # only the fixture pointers, retaining the actual native constants.
        self.call("TestLCInit")

    def native_ledge_iasa(self):
        self.native_dol()
        self.jump(0x800693AC, self.symbols["TestLCNativeEntry"])
        self.jump(0x80081544, self.symbols["Fighter_MoveToCliff"])
        for address in (0x8006EBA4, 0x8007E2F4, 0x800DF6F8, 0x800DF72C, 0x800DF79C):
            self.cpu.mem_write(address, bytes.fromhex("386000004e800020"))
        # Pure inward horizontal inputs have angle zero on both ledges. The
        # native option/percent checks run; only the trigonometry is replaced.
        self.cpu.mem_write(0x80403FF0, struct.pack(">f", 0))
        for address in (0x8007D964, 0x8007D99C):
            self.cpu.mem_write(address, bytes.fromhex("3d808040c02c3ff04e800020"))
        self.call("TestLCUseNativeIASA", 0x8009A8FC)

    def native_fastfall(self):
        self.native_dol()
        self.jump(self.symbols["Fighter_PhysAir_CheckFastfall"], 0x8007D528)
        # The only call in the native fastfall check is its sound effect.
        for pc in range(0x8007D528, 0x8007D5BC, 4):
            word = struct.unpack(">I", self.cpu.mem_read(pc, 4))[0]
            if word >> 26 == 18 and word & 1:
                delta = word & 0x03FFFFFC
                if delta & 0x02000000:
                    delta -= 0x04000000
                self.cpu.mem_write(pc + delta, bytes.fromhex("386000004e800020"))

    def stub_unrelated_calls(self, start, end, keep):
        for pc in range(start, end, 4):
            word = struct.unpack(">I", self.cpu.mem_read(pc, 4))[0]
            if word >> 26 == 18 and word & 1:
                delta = word & 0x03FFFFFC
                if delta & 0x02000000:
                    delta -= 0x04000000
                target = pc + delta
                if target not in keep:
                    self.cpu.mem_write(target, bytes.fromhex("386000004e800020"))

    def native_ground_iasa(self):
        self.native_dol()
        self.call("TestLCNativeGroundParameters")
        self.stub_unrelated_calls(0x8008A4D4, 0x8008A644,
            {0x8008BFC4, 0x80099794, 0x80091A4C, 0x800CA094})
        for begin, end in ((0x80092758, 0x80092870), (0x80092A78, 0x80092B70),
                           (0x80093EC0, 0x8009403C)):
            self.stub_unrelated_calls(begin, end, {0x8009980C, 0x8009917C})
        for address, symbol in ((0x80099894, "TestLCNativeSpotEnter"),
                                (0x800992A8, "TestLCNativeEntry"),
                                (0x800CA120, "TestLCNativeDashEnter"),
                                (0x8008C3E0, "TestLCNativeSmashEnter"),
                                (0x800923B4, "TestLCNativeGuardEnter"),
                                (0x800939B4, "TestLCNativeReflectEnter")):
            self.jump(address, self.symbols[symbol])
        # Unused C-stick alternatives are inactive; the production event uses
        # digital shoulder/A and a fresh left-stick tilt for these actions.
        for address in (0x800DF1C8, 0x800DF8E8, 0x800DF8B0):
            self.cpu.mem_write(address, bytes.fromhex("386000004e800020"))
        self.cpu.mem_write(0x80403FF0, struct.pack(">f", 0))
        self.cpu.mem_write(0x8007D964, bytes.fromhex("3d808040c02c3ff04e800020"))

    def relocated_dat(self):
        path = ROOT / "build/ledgechase-test.dat"
        if not path.exists():
            raise AssertionError("Run tests/run_settings_tests.sh to compile build/ledgechase-test.dat")
        dat = path.read_bytes()
        _, data_size, reloc_count, root_count, ref_count = struct.unpack_from(">5I", dat)
        nodes = 32 + data_size + reloc_count * 4
        strings = nodes + (root_count + ref_count) * 8
        header = None
        for i in range(root_count):
            offset, name = struct.unpack_from(">II", dat, nodes + i * 8)
            if dat[strings + name:].split(b"\0", 1)[0] == b"evFunction":
                header = struct.unpack_from(">6I", dat, 32 + offset)
        if header is None:
            raise AssertionError("evFunction root missing")
        code, table, count, exports, export_count, size = header
        base = 0x80700000
        payload = bytearray(dat[32 + code:32 + code + size])
        for i in range(count):
            word, target = struct.unpack_from(">II", dat, 32 + table + i * 8)
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
                struct.pack_into(">I", payload, offset, old | (target - base - offset) & 0x03FFFFFC)
            elif kind == 26:
                struct.pack_into(">I", payload, offset, (target - base - offset) & 0xFFFFFFFF)
            else:
                raise AssertionError("Unsupported native MEX relocation " + str(kind))
        self.cpu.mem_write(base, bytes(payload))
        for i in range(export_count):
            index, offset = struct.unpack_from(">II", dat, 32 + exports + i * 8)
            if index in (0, 2):
                self.symbols["DAT_Init" if index == 0 else "DAT_Think"] = base + offset
            elif index == 3:
                self.dat_menu = struct.unpack(">I", self.cpu.mem_read(base + offset, 4))[0]
        for line in (ROOT / "MexTK/melee.link").read_text().splitlines():
            if ":" not in line:
                continue
            address, symbol = line.split(":", 1)
            if symbol in self.symbols:
                self.jump(int(address, 16), self.symbols[symbol])
        self.call("DAT_Init", self.call("TestLCObject"))
        self.think = "DAT_Think"

    def dat_option(self, row):
        return self.call("TestLCAnyOption", self.dat_menu, row)


class LedgechaseTests(unittest.TestCase):
    def setUp(self):
        self.m = Machine()

    def test_menu_order_and_none_default_start_normal_punish_window(self):
        self.assertEqual(self.m.text(self.m.call('TestLCMenuName', 0)), 'Ledge Option Chances')
        self.assertEqual(self.m.call('TestLCMenuValue', 6), 0)
        self.m.step()
        self.m.call('TestLCPrepared', 0, 1)
        self.m.step()
        self.m.call('TestLCState', 2, 14, 0, 1)
        calls, loads = self.m.info(15), self.m.info(2)
        self.m.step()
        self.assertEqual(self.m.info(12), 1)
        self.assertEqual(self.m.info(13), 29)
        self.assertEqual(self.m.info(15), calls)
        for _ in range(29): self.m.step()
        self.assertEqual(self.m.info(2), loads + 1)

    def test_roll_consumes_input_once_and_never_repeats_when_it_finishes(self):
        self.m.call('TestLCPreference', 6, 2)
        self.m.step()
        self.m.call('TestLCPrepared', 0, 1)
        self.m.step()
        self.m.call('TestLCState', 2, 14, 0, 1)
        self.m.step()  # Shield transition.
        self.m.step()  # One roll.
        self.assertEqual(self.m.call('TestLCStateValue', 2), 233)
        ft = self.m.call('TestLCFighterData', 2)
        self.assertEqual(bytes(self.m.cpu.mem_read(ft + 0x620, 8)), bytes(8))
        # Raw CPU held/stick fields also must be released immediately.
        self.assertEqual(self.m.call('TestLCInputNeutral'), 1)
        calls, loads = self.m.info(15), self.m.info(2)
        for _ in range(20): self.m.step()
        self.assertEqual(self.m.info(15), calls)
        self.m.call('TestLCState', 2, 14, 0, 1)
        for _ in range(29): self.m.step()
        self.assertEqual((self.m.info(15), self.m.info(2)), (calls, loads))
        self.m.step()
        self.assertEqual(self.m.info(2), loads + 1)

    def test_presets_manual_chances_and_disabled_placeholders(self):
        expected = [(25, 25, 25, 25), (100, 0, 0, 0), (0, 100, 0, 0),
                    (0, 0, 100, 0), (0, 0, 0, 100), (60, 10, 15, 15)]
        for preset, chances in enumerate(expected):
            self.assertEqual(self.m.call("TestLCChangePreset", preset), preset)
            self.assertEqual(tuple(self.m.call("TestLCChance", i) for i in range(4)), chances)
        writes = self.m.info(0)
        for i, value in enumerate((95, 0, 100, 17)):
            self.m.call("TestLCChangeChance", i, value)
            chances = tuple(self.m.call("TestLCChance", j) for j in range(4))
            self.assertEqual(sum(chances), 100)
            self.assertTrue(all(0 <= chance <= 100 for chance in chances))
        self.assertEqual(self.m.info(0), writes)
        for row in (5, 6):
            self.assertEqual(self.m.call("TestLCChanceDisabled", row), 1)
            self.assertEqual(self.m.text(self.m.call("TestLCChanceValueString", row)), "N/A")
        self.m.call("TestLCInit")
        self.assertEqual(tuple(self.m.call("TestLCChance", i) for i in range(4)), expected[5])
        self.assertEqual(self.m.info(0), writes)  # Read/load never writes settings.

    def test_reset_restores_userdata_without_losing_initialization_or_side(self):
        self.m.call("TestLCPreference", 0, 1)
        self.m.step()
        self.assertEqual((self.m.info(1), self.m.info(2), self.m.info(3), self.m.info(4), self.m.info(19)), (1, 1, 1, 1, 2))
        for _ in range(4):
            self.m.step()
        self.assertEqual((self.m.info(1), self.m.info(2), self.m.info(3)), (1, 1, 1))
        self.assertEqual(self.m.call("TestLCProtection", 0), 0)
        self.assertEqual(self.m.call("TestLCProtection", 2), 1)
        self.assertEqual(self.m.info(0), 0)
        self.m.call("TestLCMenu", 1, 2)  # Swap mode stays on current side.
        self.assertEqual(self.m.info(4), 1)
        for success, expected in [(0, 1), (1, 0), (0, 0), (1, 1)]:
            self.m.call("TestLCReset", success)
            self.assertEqual(self.m.info(4), expected)
            self.assertEqual(self.m.info(3), 1)
        self.assertEqual(self.m.info(1), 1)
        self.assertEqual(self.m.info(0), 1)  # Attempts never save the temporary side.

    def test_percent_hud_all_five_distances_and_ice_climbers(self):
        self.m.call("TestLCFollowers", 1)
        self.m.call("TestLCPreference", 1, 1)
        self.m.call("TestLCPreference", 2, 6)
        self.m.step()
        self.assertEqual((self.m.info(17), self.m.info(18)), (150, 100))
        for index in (0, 1):
            self.assertEqual(self.m.call("TestLCProtection", index), 0)
        for index in (2, 3):
            self.assertEqual(self.m.call("TestLCProtection", index), 1)
        self.assertEqual(self.m.call("TestLCFollowerLog", 1), 1)
        self.assertEqual(self.m.call("TestLCFollowerLog", 3), 1)
        for side in (0, 1):
            self.m.call("TestLCMenu", 1, side)
            for option, distance in enumerate((4, 12, 24, 40, 80)):
                self.m.call("TestLCMenu", 4, option)
                x = self.m.call("TestLCPosition", 0, 0) / 1000
                self.assertAlmostEqual(x, (85.5657 - distance) * (1 if side else -1), places=2)
                self.assertEqual(self.m.call("TestLCPosition", 0, 1), 0)

    def test_sleeping_zelda_sheik_alternate_is_not_awakened_or_controlled(self):
        for main, alternate in ((19, 7), (7, 19)):
            self.m = Machine()
            self.m.call("TestLCAlternate", main, alternate)
            self.m.step()
            self.assertEqual(self.m.call("TestLCStateValue", 3), 11)  # Sleep.
            self.m.call("TestLCPrepared", 1, 1)
            # Hidden alternate hit/freeze flags must not affect the event.
            self.m.call("TestLCHit", 3, 1)
            self.m.call("TestLCNativeFreeze", 3, 1)
            self.m.step()
            self.assertEqual(self.m.info(7), 1)
            self.assertEqual(self.m.info(10), -1)

    def test_first_legal_preference_and_five_vulnerable_frame_cap(self):
        frames = [self.m.call("TestLCChooseFrame", roll, 123, 61) for roll in range(100)]
        self.assertEqual(frames.count(1), 70)
        other = [self.m.call("TestLCChooseFrame", 99, delay, 61) for delay in range(65)]
        self.assertEqual((min(other), max(other)), (2, 66))
        self.m.step()
        self.m.call("TestLCPrepared", 1, 1)
        self.m.step()  # Usually act immediately while fresh protection remains.
        self.assertEqual((self.m.info(7), self.m.info(8), self.m.info(9)), (1, 1, 0))
        self.m.call("TestLCReset", 0)
        self.m.call("TestLCPrepared", 1, 10000)
        self.m.call("TestLCSetProtected", 0)
        for frame in range(1, 6):
            self.m.step()
            self.assertEqual(self.m.info(7), int(frame == 5))
        self.assertEqual((self.m.info(8), self.m.info(9)), (5, 5))

    def test_native_four_ledge_options_and_exact_100_percent_threshold(self):
        self.m.native_ledge_iasa()
        # Actual native CliffWait IASA, option eligibility, and percentage
        # branches execute; animation/physics entry endpoints are fixtures.
        for percent, expected in [(0, (255, 262, 259, 257)), (99, (255, 262, 259, 257)),
                                  (100, (254, 260, 258, 256)), (150, (254, 260, 258, 256))]:
            self.m.call("TestLCSetPercent", percent)
            for option, state in enumerate(expected):
                self.m.call("TestLCPrepared", option, 1)
                self.assertEqual(self.m.call("TestLCLedgeInput", option), state)

    def test_followups_wait_for_native_landing_and_finish_before_reset_window(self):
        self.m.call("TestLCPreference", 6, 1)
        self.m.step()
        self.m.call("TestLCPrepared", 1, 1)
        self.m.step()
        self.m.call("TestLCState", 2, 42, 0, 3)  # Native normal landing lag=4.
        self.m.step()
        self.assertEqual(self.m.info(12), 0)
        self.m.call("TestLCState", 2, 42, 0, 4)
        self.m.step()
        self.assertEqual(self.m.info(12), 1)
        self.assertEqual(self.m.info(13), -1)
        loads = self.m.info(2)
        for _ in range(20):
            self.m.step()  # Spotdodge is still active.
        self.assertEqual(self.m.info(2), loads)
        self.m.call("TestLCState", 2, 14, 0, 1)  # Spotdodge completion.
        for _ in range(29):
            self.m.step()
        self.assertEqual(self.m.info(2), loads)
        self.m.step()
        self.assertEqual(self.m.info(2), loads + 1)

    def test_all_followups_and_character_specific_smashes(self):
        for action in range(1,5):
            self.m.call("TestLCState", 2, 14, 0, 1)
            self.assertEqual(self.m.call("TestLCFollowup", action), 0 if action == 2 else 1)
            if action == 2:
                self.assertEqual(self.m.call("TestLCFollowup", action), 1)  # Native shield transition precedes roll.
        for kind in (8, 9, 24):
            self.m.call("TestLCSetKind", kind)
            self.m.call("TestLCState", 2, 14, 0, 1)
            self.assertEqual(self.m.call("TestLCFollowup", 4), 1)

    def test_actual_native_wait_and_guard_eligibility_for_all_four_followups(self):
        self.m.native_ground_iasa()
        self.m.step()
        for action, expected in ((1, 235), (2, 233), (3, 20), (4, 60)):
            self.m.call("TestLCState", 2, 14, 0, 1)
            self.m.call("TestLCUseNativeIASA", 0x8008A4D4)
            result = self.m.call("TestLCFollowup", action)
            if action == 2:
                self.assertEqual(result, 0)  # Native shield entry occurs first.
                self.assertEqual(self.m.call("TestLCFollowup", action), 1)
            else:
                self.assertEqual(result, 1)
            self.assertEqual(self.m.call("TestLCStateValue", 2), expected)
        # Roll eligibility is also preserved in all three ordinary native
        # shield IASA paths, including a fresh powershield transition.
        for state, callback in ((178, 0x80092758), (179, 0x80092A78), (182, 0x80093EC0)):
            self.m.call("TestLCState", 2, state, 0, 1)
            self.m.call("TestLCUseNativeIASA", callback)
            self.assertEqual(self.m.call("TestLCFollowup", 2), 1)

    def test_actual_native_fastfall_only_after_first_descending_frame(self):
        self.m.native_fastfall()
        self.assertEqual(self.m.call("TestLCFastfall", 1000), 0)
        self.assertEqual(self.m.call("TestLCFastfall", 0), 0)
        self.assertEqual(self.m.call("TestLCFastfall", -1), 1)
        self.m.call("TestLCNativeFreeze", 2, 1)
        self.assertEqual(self.m.call("TestLCFastfall", -1000), 0)

    def test_pause_native_freeze_and_follower_hit_feedback_do_not_age_attempt(self):
        self.m.call("TestLCFollowers", 1)
        self.m.step()
        self.m.call("TestLCPrepared", 1, 1)
        self.m.call("TestLCPause", 2)
        for _ in range(3): self.m.step()
        self.assertEqual(self.m.info(8), 0)
        self.m.call("TestLCPause", 0)
        self.m.call("TestLCNativeFreeze", 3, 1)
        for _ in range(3): self.m.step()
        self.assertEqual(self.m.info(8), 0)
        self.m.call("TestLCHit", 3, 1)
        self.m.step()
        self.assertEqual((self.m.info(10), self.m.info(11)), (15, 1))
        for _ in range(3): self.m.step()
        self.assertEqual(self.m.info(10), 15)
        self.m.call("TestLCNativeFreeze", 3, 0)
        loads = self.m.info(2)
        for _ in range(15): self.m.step()
        self.assertEqual(self.m.info(2), loads + 1)

    def test_active_nana_death_resets_without_swapping_on_failure(self):
        self.m.call("TestLCFollowers", 1)
        self.m.call("TestLCPreference", 0, 2)
        self.m.step()
        loads, side = self.m.info(2), self.m.info(4)
        self.m.call("TestLCDead", 3, 1)
        self.m.step()
        self.assertEqual(self.m.info(2), loads + 1)
        self.assertEqual(self.m.info(4), side)

    def test_optimized_dat_initialization_reset_and_menu_with_real_relocations(self):
        self.m.relocated_dat()
        self.assertEqual(self.m.info(0), 0)
        self.assertEqual(self.m.call("TestLCAnyOptionValue", self.m.dat_option(4)), 1)
        self.assertEqual(self.m.call("TestLCAnyOptionValue", self.m.dat_option(5)), 7)
        self.m.step()
        self.assertEqual((self.m.info(1), self.m.info(2), self.m.info(3)), (1, 1, 1))
        self.m.call("TestLCAnyOptionChange", self.m.dat_option(1), 1)
        self.assertEqual((self.m.info(4), self.m.info(19)), (1, 2))
        self.m.call("TestLCAnyOptionChange", self.m.dat_option(2), 1)
        self.m.call("TestLCAnyOptionChange", self.m.dat_option(3), 6)
        self.assertEqual((self.m.info(17), self.m.info(18)), (150, 100))
        self.assertEqual(self.m.call("TestLCProtection", 0), 0)
        submenu = self.m.call("TestLCAnySubmenu", self.m.dat_option(0))
        preset = self.m.call("TestLCAnyOption", submenu, 0)
        self.m.call("TestLCAnyOptionChange", preset, 5)
        expected = (60, 10, 15, 15)
        for row, chance in enumerate(expected, 1):
            option = self.m.call("TestLCAnyOption", submenu, row)
            self.assertEqual(self.m.call("TestLCAnyOptionValue", option), chance)
        saves = self.m.info(1)
        for _ in range(4): self.m.step()
        self.assertEqual(self.m.info(1), saves)  # Full userdata restores cannot trigger a fresh save loop.


if __name__ == "__main__":
    build()
    unittest.main(verbosity=2)
