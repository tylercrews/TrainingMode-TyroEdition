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
        # Keep the native C-stick smash check; unused aerial C-stick paths
        # are inactive in this grounded IASA fixture.
        for address in (0x800DF8E8, 0x800DF8B0):
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
        self.m.call('TestLCPreference', 6, 13)
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

    def test_presets_and_all_six_session_chances(self):
        old = [(25, 25, 25, 25), (100, 0, 0, 0), (0, 100, 0, 0),
                    (0, 0, 100, 0), (0, 0, 0, 100), (60, 10, 15, 15), (50, 0, 50, 0)]
        expected = [row + (0, 0) for row in old] + [(0,0,0,0,100,0), (0,0,0,0,0,100),
            (20,20,20,20,20,0), (20,20,20,20,0,20), (17,17,17,17,16,16), (0,25,0,0,0,75), (0,0,0,0,50,50),
            (10,10,10,10,10,50)]
        self.assertEqual(self.m.call('TestLCPresetCount'), len(expected))
        for preset, chances in enumerate(expected):
            self.assertEqual(self.m.call("TestLCChangePreset", preset), preset)
            self.assertEqual(tuple(self.m.call("TestLCChance", i) for i in range(6)), chances)
        writes = self.m.info(0)
        for i, value in enumerate((95, 0, 100, 17, 100, 100)):
            self.m.call("TestLCChangeChance", i, value)
            chances = tuple(self.m.call("TestLCChance", j) for j in range(6))
            self.assertEqual(sum(chances), 100)
            self.assertTrue(all(0 <= chance <= 100 for chance in chances))
        self.assertEqual(self.m.info(0), writes)
        for row in (5, 6):
            self.assertEqual(self.m.call("TestLCChanceDisabled", row), 0)
        for option in range(6):
            chances = [0] * 6
            chances[option] = 100
            for roll in range(100):
                self.assertEqual(self.m.call('TestLCSelect', *chances, roll), option)
        self.m.call("TestLCInit")
        self.assertEqual(tuple(self.m.call("TestLCChance", i) for i in range(6)), expected[-1])
        self.assertEqual(self.m.info(0), writes)  # Read/load never writes settings.

    def prepare_return(self, option=5, side=0, followers=0):
        self.m.call('TestLCFollowers', followers)
        self.m.call('TestLCPreference', 0, side)
        self.m.step()
        self.m.call('TestLCPrepared', option, 1)

    def prepare_aerial(self, choice, option=4, side=0, kind=22):
        self.m.call('TestLCPreference', 7, choice)
        self.prepare_return(option=option, side=side)
        self.m.call('TestLCJumpKind', 2, kind)
        self.m.step(); self.m.step()

    def test_all_six_preset_is_exactly_equal_until_a_manual_edit(self):
        self.m.step()
        self.m.call('TestLCChangePreset', 11)
        for roll in range(6):
            self.m.call('TestLCForceRoll', roll)
            self.m.call('TestLCReset', 0)
            self.assertEqual(self.m.info(5), roll)
        self.m.call('TestLCChangeChance', 5, 100)
        self.assertEqual(self.m.call('TestLCCustomChances'), 1)
        for roll in range(100):
            self.m.call('TestLCForceRoll', roll)
            self.m.call('TestLCReset', 0)
            self.assertEqual(self.m.info(5), 5)
        self.m.call('TestLCChangePreset', 11)
        self.assertEqual(self.m.call('TestLCCustomChances'), 0)

    def prepare_low_hop(self, choice=6, kind=22, mode=0, velocity=3854, gravity=170):
        self.m.call('TestLCPreference', 7, choice)
        self.m.call('TestLCPreference', 9, mode)
        self.prepare_return(option=4)
        self.m.call('TestLCJumpKind', 2, kind)
        self.m.call('TestLCJumpAttributes', velocity, gravity, 3100)
        self.m.call('TestLCReturnPosition', 0, -3, -7, 0, 0)
        self.m.step()

    def test_low_hop_fastfalls_to_target_then_native_jump_clears_fastfall(self):
        self.m.native_fastfall()
        self.prepare_low_hop()
        target = self.m.call('TestLCDropInfo', 0, 0) / 1000
        self.assertTrue(-35 < target < -30)  # Falco's real v=3.854, g=.17.
        self.m.call('TestLCReturnPosition', 0, -3, -10, 0, -1)
        self.m.step()
        self.assertEqual((self.return_info(), self.return_info(1), self.return_info(4)), (1,1,1))
        self.assertEqual(self.m.call('TestLCStick', 1), -127)
        self.assertEqual(self.m.call('TestLCAirInfo', 0, 1), 0)
        self.m.call('TestLCReturnPosition', 0, -3, -35, 0, -3)
        before = self.m.call('TestLCPosition', 2, 1)
        self.m.step()
        self.assertEqual((self.return_info(), self.return_info(1), self.return_info(4)), (2,2,0))
        self.assertEqual(self.m.call('TestLCPosition', 2, 1), before)  # No position shortcut.

    def test_drop_targets_vary_by_character_move_height_mode_and_manual_adjustment(self):
        targets = {}
        for name, args in [('falco_laser', (6,22,0,3854,170)), ('marth_nair', (1,18,0,2112,85)),
                           ('falco_dair', (3,22,0,3854,170)), ('medium', (6,22,1,3854,170)),
                           ('original', (6,22,2,3854,170))]:
            self.m = Machine()
            self.prepare_low_hop(*args)
            targets[name] = self.m.call('TestLCDropInfo', 0, 0) / 1000
        self.assertLess(targets['falco_laser'], targets['marth_nair'] - 10)
        self.assertAlmostEqual(targets['falco_dair'] - targets['falco_laser'], 12, places=2)
        self.assertAlmostEqual(targets['medium'] - targets['falco_laser'], 12, places=2)
        self.assertAlmostEqual(targets['original'], -7, places=2)
        self.m = Machine(); self.prepare_low_hop()
        writes = self.m.info(0)
        self.m.call('TestLCSetup', 2, 2)
        self.assertEqual(self.m.info(0), writes)  # Session-only tuning.
        self.m.call('TestLCPrepared', 4, 1)
        self.m.call('TestLCJumpKind', 2, 22)
        self.m.call('TestLCJumpAttributes', 3854, 170, 3100)
        self.m.call('TestLCReturnPosition', 0, -3, -7, 0, 0)
        self.m.step()
        self.assertAlmostEqual(self.m.call('TestLCDropInfo', 0, 0)/1000, targets['falco_laser']-2, places=2)

    def test_low_laser_starts_below_lip_only_when_rise_can_clear_it(self):
        self.prepare_low_hop()
        self.m.call('TestLCReturnPosition', 0, -3, -35, 0, -3)
        self.m.step()
        for inside, height, velocity in ((-10,-8,3), (-3,-16,3), (-3,-12,1)):
            self.m.call('TestLCReturnPosition', 0, inside, height, 0, velocity)
            self.m.step()
            self.assertEqual(self.m.call('TestLCAirInfo', 0, 1), 0)
        self.m.call('TestLCReturnPosition', 0, -3, -12, 0, 3)
        self.m.step()
        self.assertEqual(self.m.call('TestLCStateValue', 2), 344)
        self.assertEqual(self.m.call('TestLCAirInfo', 0, 1), 1)

    def test_animation_driven_hops_have_bounded_drop_profiles(self):
        for kind, extra in ((8,4), (14,6), (9,2), (16,4)):
            self.m = Machine()
            self.prepare_low_hop(choice=1, kind=kind)
            self.assertEqual(self.m.call('TestLCDropInfo', 0, 0), (-7-extra)*1000)

    def test_fall_entry_uses_real_catch_and_wait_before_attempt_clocks(self):
        self.m.call('TestLCPreference', 8, 1)
        self.m.step()
        self.assertEqual(self.m.call('TestLCStateValue', 2), 29)
        self.assertEqual(self.m.call('TestLCPosition', 2, 1), 70000)
        self.assertEqual((self.m.call('TestLCEntryInfo', 0), self.m.info(8), self.m.call('TestLCEntryInfo', 4)), (0,0,0))
        self.m.call('TestLCState', 2, 252, 1, 1)
        self.m.step()
        self.assertEqual(self.m.call('TestLCStateValue', 2), 252)  # Never force CliffWait.
        self.m.call('TestLCState', 2, 253, 1, 0)
        self.m.step()
        self.assertEqual((self.m.call('TestLCEntryInfo', 0), self.m.info(8)), (1,0))
        self.m.step()
        self.assertEqual(self.m.info(8), 1)

    def test_jump_entry_requests_one_native_jump_and_mirrors_sides(self):
        for side in (0,1):
            self.m = Machine()
            self.m.call('TestLCPreference', 0, side)
            self.m.call('TestLCPreference', 8, 2)
            self.m.step()
            self.assertEqual(self.m.call('TestLCEntryInfo', 2), 1)
            self.assertEqual(self.return_info(1), 2)
            self.assertLess(self.m.call('TestLCPosition', 2, 1), 0)
            x = self.m.call('TestLCPosition', 2, 0)
            self.assertEqual(x > 0, bool(side))
            for _ in range(5): self.m.step()
            self.assertEqual(self.return_info(1), 2)
            self.assertEqual(self.m.call('TestLCStick', 4), 0)
            self.assertEqual(self.m.info(7), 0)

    def test_approach_waits_for_both_climbers_and_freeze_does_not_age_it(self):
        self.m.call('TestLCFollowers', 1)
        self.m.call('TestLCPreference', 8, 1)
        self.m.step()
        self.m.call('TestLCState', 2, 253, 1, 0)
        self.m.call('TestLCState', 3, 252, 1, 1)
        elapsed = self.m.call('TestLCEntryInfo', 1)
        self.m.call('TestLCNativeFreeze', 3, 1)
        self.m.step()
        self.assertEqual(self.m.call('TestLCEntryInfo', 1), elapsed)
        self.m.call('TestLCNativeFreeze', 3, 0)
        self.m.step()
        self.assertEqual(self.m.call('TestLCEntryInfo', 0), 0)
        self.m.call('TestLCState', 3, 253, 1, 0)
        self.m.step()
        self.assertEqual(self.m.call('TestLCEntryInfo', 0), 1)

    def test_entry_failure_timeout_retries_without_success_or_clock_leak(self):
        self.m.call('TestLCPreference', 8, 1)
        self.m.step()
        loads = self.m.info(2)
        for _ in range(239): self.m.step()
        self.assertEqual(self.m.info(2), loads+1)
        self.assertEqual(self.m.info(11), 0)
        self.assertEqual(self.m.call('TestLCEntryInfo', 4), 0)

    def test_aerial_catalogue_and_shared_inputs(self):
        expected = ['None', 'Neutral Air', 'Forward Air', 'Down Air', 'Back Air', 'Up Air',
                    'Neutral B (Repeat)', 'Up B', 'Side B Toward', 'Side B Away', 'Down B', 'Random']
        self.assertEqual(self.m.call('TestLCAirCount'), len(expected))
        self.assertEqual([self.m.text(self.m.call('TestLCAirName', i)) for i in range(len(expected))], expected)
        for choice, inputs in ((1,(0,0,0,0,0x100)), (2,(0,0,127,0,0)), (3,(0,0,0,-127,0)),
                               (4,(0,0,-127,0,0)), (5,(0,0,0,127,0)), (6,(0,0,0,0,0x200)),
                               (7,(0,127,0,0,0x200)), (8,(-127,0,0,0,0x200)),
                               (9,(127,0,0,0,0x200)), (10,(0,-127,0,0,0x200))):
            self.m = Machine()
            self.prepare_aerial(choice)
            self.m.call('TestLCUseNativeIASA', self.m.symbols['TestLCNoIASA'])
            self.m.call('TestLCHumanSide', -1)
            self.m.call('TestLCReturnPosition', 0, 3, 6, 0, 1)
            self.m.step()
            self.assertEqual(tuple(self.m.call('TestLCStick', i) for i in range(5)), inputs, choice)

    def test_hop_attack_waits_for_rising_stage_clearance_and_runs_once(self):
        self.prepare_aerial(1)
        for inside, height, bottom, vertical in ((-1,10,0,1), (2,3,0,1), (2,8,-10,1), (2,8,0,-1)):
            self.m.call('TestLCReturnPosition', 0, inside, height, bottom, vertical)
            self.m.step()
            self.assertEqual(self.m.call('TestLCAirInfo', 0, 1), 0)
        self.m.call('TestLCReturnPosition', 0, 3, 6, 0, 1)
        self.m.step()
        self.assertEqual(self.m.call('TestLCStateValue', 2), 65)
        self.assertEqual(self.m.call('TestLCAirInfo', 0, 1), 1)
        for _ in range(5): self.m.step()
        self.m.call('TestLCState', 2, 29, 1, 0)
        self.m.step()
        self.assertEqual(self.m.call('TestLCStateValue', 2), 29)
        self.m.call('TestLCState', 2, 14, 0, 0)
        self.m.step()
        self.assertEqual(self.m.info(12), 1)  # Grounded followup still completes.

    def test_native_ledge_jump_attacks_only_after_recovery_and_dash_ignores_aerial(self):
        self.prepare_aerial(1, option=1)
        self.m.call('TestLCReturnPosition', 0, 6, 12, 0, 1)
        for state in (260,261,262,263):
            self.m.call('TestLCState', 2, state, 1, 30)
            self.m.step()
            self.assertEqual(self.m.call('TestLCAirInfo', 0, 1), 0)
        self.m.call('TestLCState', 2, 29, 1, 0)
        self.m.step()
        self.assertEqual(self.m.call('TestLCStateValue', 2), 65)
        self.m = Machine()
        self.prepare_aerial(1, option=5)
        self.m.call('TestLCReturnPosition', 0, 2, 6, 0, 1)
        self.m.step()
        self.assertEqual(self.m.call('TestLCStateValue', 2), 236)
        self.assertEqual(self.m.call('TestLCAirInfo', 0, 1), 0)

    def test_pending_air_attack_consumed_between_callbacks_is_not_repeated(self):
        self.prepare_aerial(1)
        self.m.call('TestLCUseNativeIASA', self.m.symbols['TestLCNoIASA'])
        self.m.call('TestLCReturnPosition', 0, 2, 6, 0, 1)
        self.m.step()
        self.assertEqual(self.m.call('TestLCAirInfo', 0, 3), 1)
        self.m.call('TestLCState', 2, 65, 1, 0)
        self.m.step()
        self.assertEqual(self.m.call('TestLCAirInfo', 0, 1), 1)
        self.assertEqual(self.m.call('TestLCStick', 4), 0)

    def test_falco_repeats_native_blaster_window_then_releases_after_second_loop(self):
        self.m.native_dol()
        self.prepare_aerial(6)
        self.m.call('TestLCReturnPosition', 0, 3, 8, 0, 1)
        self.m.step()  # Enter native-special startup through the event's input.
        self.assertEqual(self.m.call('TestLCStateValue', 2), 344)
        self.m.call('TestLCUseNativeIASA', 0x800E6B08)  # Actual shared Fox/Falco blaster IASA.
        self.m.call('TestLCBlasterWindow', 1)
        self.m.step()
        self.assertEqual(self.m.call('TestLCBlasterRepeat'), 1)
        self.m.step()
        self.assertEqual(self.m.call('TestLCStick', 4), 0)  # Release between presses.
        self.m.call('TestLCUseNativeIASA', 0x800E6B30)
        self.m.call('TestLCState', 2, 345, 1, 2)
        self.m.call('TestLCReturnPosition', 0, 4, 12, 0, -1)
        self.m.step()
        self.assertEqual(self.m.call('TestLCAirInfo', 0, 2), 1)
        self.assertEqual(self.return_info(4), 0)  # Do not cut the repeat short with fastfall.
        self.m.call('TestLCState', 2, 345, 1, 0)  # Native animation restarts for shot two.
        self.m.call('TestLCBlasterWindow', 1)
        self.m.step()
        self.assertEqual(self.m.call('TestLCAirInfo', 0, 2), 2)
        self.assertEqual(self.m.call('TestLCStick', 4), 0)
        self.assertEqual(self.m.call('TestLCBlasterRepeat'), 0)  # No third-loop request.
        self.assertEqual(self.return_info(4), 0)
        self.m.call('TestLCState', 2, 346, 1, 0)
        self.m.step()
        self.assertEqual(self.return_info(4), 1)

    def test_random_aerial_is_chosen_once_per_body_and_reset_clears_progress(self):
        self.m.call('TestLCPreference', 7, 11)
        self.prepare_return(option=4, followers=1)
        chosen = [self.m.call('TestLCAirInfo', ms, 0) for ms in (0,1)]
        valid = {self.m.call('TestLCAirActionID', i) for i in range(1,11)}
        self.assertTrue(all(action in valid for action in chosen))
        for _ in range(4): self.m.step()
        self.assertEqual([self.m.call('TestLCAirInfo', ms, 0) for ms in (0,1)], chosen)
        self.m.call('TestLCReset', 0)
        self.assertEqual([self.m.call('TestLCAirInfo', ms, 1) for ms in (0,1)], [0,0])

    def return_info(self, field=0, ms=0):
        return self.m.call('TestLCReturnInfo', ms, field)

    def test_hop_and_dash_drop_then_jump_on_separate_frames_on_both_ledges(self):
        for option in (4, 5):
            for side in (0, 1):
                self.m = Machine()
                self.prepare_return(option, side)
                self.m.step()
                self.assertEqual(self.m.call('TestLCStateValue', 2), 29)  # Fall, not native ledge jump.
                self.assertEqual((self.return_info(), self.return_info(1)), (1, 1))
                self.m.step()
                self.assertEqual((self.return_info(), self.return_info(1)), (2, 2))
                self.assertEqual(self.m.call('TestLCStick', 0), 127 if side == 0 else -127)

    def test_return_observes_jump_consumed_between_callbacks(self):
        self.prepare_return()
        self.m.step()
        # Native IASA can consume the queued jump before the next event tick.
        self.m.call('TestLCNativeJumpEnter', self.m.call('TestLCFighter', 2))
        self.m.step()
        self.assertEqual((self.return_info(), self.return_info(1)), (2, 2))

    def test_native_release_and_aerial_jump_dispatch_for_entire_selectable_roster(self):
        # Native release eligibility, jump-count check, per-character jump
        # dispatch, and R-button air-dodge check; entries/physics are fixtures.
        for kind in range(27):
            self.m = Machine()
            self.m.native_ledge_iasa()
            self.m.jump(0x800CC730, self.m.symbols['TestLCNativeFallEnter'])
            for address in (0x800C5240, 0x800D2D0C):
                self.m.cpu.mem_write(address, bytes.fromhex('386000004e800020'))
            for address in (0x800CBBC0, 0x800CBD18, 0x800CBE98, 0x800CC0E8, 0x800CC238, 0x800D74A4):
                self.m.jump(address, self.m.symbols['TestLCNativeJumpEnter'])
            self.m.jump(0x80099A9C, self.m.symbols['TestLCNativeAirDodgeEnter'])
            self.prepare_return()
            self.m.call('TestLCJumpKind', 2, kind)
            self.m.call('TestLCUseNativeIASA', 0x8009A8FC)
            self.m.step()
            self.assertEqual(self.return_info(), 1, kind)
            self.m.call('TestLCUseNativeIASA', self.m.symbols['TestLCNativeReturnIASA'])
            self.m.step()
            self.assertEqual((self.return_info(), self.return_info(1)), (2, 2), kind)
            self.m.call('TestLCReturnPosition', 0, 1, 4, 0, 1)
            self.m.step()
            self.assertEqual((self.return_info(), self.m.call('TestLCStateValue', 2)), (3, 236), kind)

    def test_clearance_uses_both_origin_and_collision_bottom_and_mirrors_dodge(self):
        for side in (0, 1):
            self.m = Machine()
            self.prepare_return(side=side)
            self.m.step(); self.m.step()
            for inside, height, bottom in ((-11, 20, 0), (-4, 5, 0), (1, -2, 20), (1, 4, -4)):
                self.m.call('TestLCReturnPosition', 0, inside, height, bottom, 1)
                self.m.step()
                self.assertEqual(self.return_info(), 2)
            self.m.call('TestLCReturnPosition', 0, -4, 6, 0, 1)
            self.m.call('TestLCUseNativeIASA', self.m.symbols['TestLCNoIASA'])
            self.m.step()
            self.assertEqual([self.m.call('TestLCStick', c) for c in (0, 1)],
                             [90 if side == 0 else -90, -90])
            # Observe an air dodge started by native IASA between ticks.
            self.m.call('TestLCState', 2, 236, 1, 0)
            self.m.step()
            self.assertEqual(self.return_info(), 3)
            self.assertEqual(self.m.call('TestLCInputNeutral'), 1)
            self.m.call('TestLCState', 2, 43, 0, 9)  # Special landing lag, not actionable.
            self.m.step()
            self.assertEqual(self.m.info(12), 0)
            self.m.call('TestLCState', 2, 14, 0, 0)
            self.m.step()
            self.assertEqual(self.m.info(12), 1)

    def test_jump_is_released_for_float_and_multijumps_and_hop_fastfall_waits_for_stage(self):
        for kind in (4, 15, 8, 14, 16, 9):  # Kirby/Puff; Ness/Yoshi/Mewtwo/Peach.
            self.m = Machine()
            self.prepare_return(option=4)
            self.m.call('TestLCJumpKind', 2, kind)
            self.m.step(); self.m.step()
            self.assertEqual(self.return_info(3), 0)  # Release even before the next native tick.
            self.m.step()
            self.assertEqual(self.return_info(3), 0)
            self.assertEqual(self.return_info(2), 0)
            self.m.call('TestLCReturnPosition', 0, -1, 10, 0, -1)
            self.m.step()
            self.assertEqual(self.return_info(4), 0)
            self.m.call('TestLCReturnPosition', 0, 4, 10, 0, -1)
            self.m.step()
            self.assertEqual(self.return_info(4), 1)
            self.assertEqual(self.return_info(1), 2)

    def test_ice_climbers_keep_independent_returns_and_freeze_together(self):
        self.prepare_return(followers=1)
        self.m.step(); self.m.step()
        self.m.call('TestLCReturnPosition', 0, 1, 4, 0, 1)
        self.m.call('TestLCReturnPosition', 1, -5, -4, 0, 1)
        self.m.call('TestLCNativeFreeze', 3, 1)
        self.m.step()
        self.assertEqual((self.return_info(ms=0), self.return_info(ms=1)), (2, 2))
        self.m.call('TestLCNativeFreeze', 3, 0)
        self.m.step()
        self.assertEqual((self.return_info(ms=0), self.return_info(ms=1)), (3, 2))
        self.m.call('TestLCReset', 0)
        self.assertEqual((self.return_info(ms=0), self.return_info(ms=1)), (0, 0))

    def test_cpu_action_catalogue_matches_ground_choices_without_slots_or_advanced(self):
        expected = ["None", "Spotdodge", "Shield", "Grab", "Up B", "Side B Toward",
                    "Side B Away", "Down B", "Neutral B", "Up Smash", "Down Smash",
                    "Forward Smash", "Roll Away", "Roll Towards", "Roll Random",
                    "Neutral Air", "Forward Air", "Down Air", "Back Air", "Up Air",
                    "Jab", "Forward Tilt", "Up Tilt", "Down Tilt", "Dash Attack",
                    "Short Hop", "Full Hop", "Wavedash Away", "Wavedash Towards",
                    "Wavedash Down", "Dash Back", "Dash Through", "Random"]
        self.assertEqual(self.m.call("TestLCActionCount"), len(expected))
        self.assertEqual([self.m.text(self.m.call("TestLCActionName", i)) for i in range(len(expected))], expected)
        self.assertEqual(self.m.text(self.m.call("TestLCMenuName", 2)), "CPU 100%")

    def test_shared_ground_actions_supply_buttons_sticks_and_multiframe_sequences(self):
        self.m.step()
        self.m.call("TestLCHumanSide", -1)
        # Leave native IASA deferred so the actual production table inputs can
        # be inspected before Melee consumes them on its next callback.
        self.m.call("TestLCUseNativeIASA", self.m.symbols["TestLCNoIASA"])
        def command(action, state=14, air=0, frame=1):
            self.m.call("TestLCState", 2, state, air, frame)
            result = self.m.call("TestLCFollowup", action)
            return result, tuple(self.m.call("TestLCStick", i) for i in range(5))
        for action, expected in ((2,(0,0,0,0,0x20)), (4,(0,127,0,0,0x200)),
                (5,(-127,0,0,0,0x200)), (6,(127,0,0,0,0x200)),
                (7,(0,-127,0,0,0x200)), (8,(0,0,0,0,0x200)),
                (9,(0,0,0,127,0)), (10,(0,0,0,-127,0)), (11,(0,0,-127,0,0)),
                (20,(0,0,0,0,0x100)), (21,(-80,0,0,0,0x100)),
                (22,(0,80,0,0,0x100)), (23,(0,-80,0,0,0x100))):
            with self.subTest(action=action):
                self.m.call("TestLCFollowupReset")
                self.assertEqual(command(action), (0,expected))
                self.assertEqual(command(action, 60)[0], 1)  # Native entry observed once.
                self.assertEqual(self.m.call("TestLCInputNeutral"), 1)
        for action, final in ((15,(0,0,0,0,0x100)), (16,(0,0,127,0,0)),
                             (17,(0,0,0,-127,0)), (18,(0,0,-127,0,0)),
                             (19,(0,0,0,127,0)), (27,(65,-45,0,0,0x40)),
                             (28,(-64,-45,0,0,0x40)), (29,(0,-80,0,0,0x40))):
            with self.subTest(action=action):
                self.m.call("TestLCFollowupReset")
                self.assertEqual(command(action), (0,(0,0,0,0,0x400)))
                self.assertEqual(command(action, 24)[0], 0)  # Jumpsquat waits for takeoff.
                self.assertEqual(command(action, 25, 1), (0,final))
                self.assertEqual(command(action, 65, 1)[0], 1)
        self.m.call("TestLCFollowupReset")
        self.assertEqual(command(3)[1], (-127,0,0,0,0))
        self.assertEqual(command(3,20)[1], (-127,0,0,0,0x800))
        self.assertEqual(command(3,24)[1], (0,0,0,0,0x10))
        self.m.call("TestLCFollowupReset")
        command(24)
        self.assertEqual(command(24,20,0,4)[1][-1], 0)
        self.assertEqual(command(24,20,0,5)[1][-1], 0x100)
        self.m.call("TestLCFollowupReset")
        self.assertEqual(command(26)[1][-1], 0x400)
        self.assertEqual(command(26,24)[1][-1], 0x400)  # Full hop keeps jump held.
        self.assertEqual(command(26,25,1)[0], 1)

    def test_random_resolves_to_one_supported_action_for_each_body_per_attempt(self):
        self.m.call("TestLCFollowers", 1)
        self.m.call("TestLCPreference", 6, 32)
        self.m.step()
        for _ in range(30):
            self.m.call("TestLCReset", 0)
            for body in (0,1):
                self.assertIn(self.m.call("TestLCChosenAction", body),
                              [9,3,33,12,13,14,26,27,29,30,38,39])

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
        for index, percent in ((0,150),(1,150),(2,100),(3,100)):
            self.assertEqual(self.m.call("TestLCPendingDamage", index), 0)
            self.assertEqual(self.m.call("TestLCApplyPendingDamage", index), percent)
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
        for action in (1,13,31,11):
            self.m.call("TestLCFollowupReset")
            self.m.call("TestLCState", 2, 14, 0, 1)
            self.assertEqual(self.m.call("TestLCFollowup", action), 0 if action in (13,31) else 1)
            if action in (13,31):
                self.assertEqual(self.m.call("TestLCFollowup", action), 1)  # Observe the roll/dash sequence finishing.
        for kind in (8, 9, 24):
            self.m.call("TestLCFollowupReset")
            self.m.call("TestLCSetKind", kind)
            self.m.call("TestLCState", 2, 14, 0, 1)
            self.assertEqual(self.m.call("TestLCFollowup", 11), 1)

    def test_actual_native_wait_and_guard_eligibility_for_all_four_followups(self):
        self.m.native_ground_iasa()
        self.m.step()
        for action, expected in ((1, 235), (13, 233), (31, 20), (11, 60)):
            self.m.call("TestLCFollowupReset")
            self.m.call("TestLCState", 2, 14, 0, 1)
            self.m.call("TestLCUseNativeIASA", 0x8008A4D4)
            result = self.m.call("TestLCFollowup", action)
            if action in (13,31):
                self.assertEqual(result, 0)  # Shared roll/dash sequences use a second command.
                self.assertEqual(self.m.call("TestLCFollowup", action), 1)
            else:
                self.assertEqual(result, 1)
            self.assertEqual(self.m.call("TestLCStateValue", 2), expected)
        # Roll eligibility is also preserved in all three ordinary native
        # shield IASA paths, including a fresh powershield transition.
        for state, callback in ((178, 0x80092758), (179, 0x80092A78), (182, 0x80093EC0)):
            self.m.call("TestLCState", 2, state, 0, 1)
            self.m.call("TestLCUseNativeIASA", callback)
            self.assertEqual(self.m.call("TestLCFollowup", 13), 1)

    def test_actual_native_fastfall_only_after_first_descending_frame(self):
        self.m.native_fastfall()
        self.assertEqual(self.m.call("TestLCFastfall", 1000), 0)
        self.assertEqual(self.m.call("TestLCFastfall", 0), 0)
        self.assertEqual(self.m.call("TestLCFastfall", -1), 1)
        self.m.call("TestLCNativeFreeze", 2, 1)
        self.assertEqual(self.m.call("TestLCFastfall", -1000), 0)

    def test_roll_started_between_callbacks_is_latched_and_never_requested_again(self):
        self.m.native_ground_iasa()
        self.m.call("TestLCPreference", 6, 13)
        self.m.step()
        self.m.call("TestLCPrepared", 0, 1)
        self.m.step()
        self.m.call("TestLCState", 2, 14, 0, 1)
        self.m.call("TestLCUseNativeIASA", 0x8008A4D4)
        self.m.step()  # Wait -> shield.
        self.m.call("TestLCUseNativeIASA", self.m.symbols["TestLCNoIASA"])
        self.m.step()  # Queue the shared table's terminal roll command.
        self.m.call("TestLCUseNativeIASA", 0x80092758)
        self.assertEqual(self.m.info(12),0)
        self.assertEqual(self.m.call("TestLCEngineIASA"),233)
        self.m.step()  # Observe native roll before eligibility filtering.
        self.assertEqual(self.m.info(12),1)
        self.assertEqual(self.m.call("TestLCInputNeutral"),1)
        for _ in range(20):self.m.step()
        self.m.call("TestLCState",2,14,0,1)
        loads=self.m.info(2)
        for _ in range(29):
            self.m.step()
            self.assertEqual(self.m.call("TestLCStateValue",2),14)
            self.assertEqual(self.m.call("TestLCInputNeutral"),1)
        self.assertEqual(self.m.info(2),loads)
        self.m.step()
        self.assertEqual(self.m.info(2),loads+1)

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
        self.m.call("TestLCAnyOptionChange", preset, 6)
        for row, chance in enumerate((50,0,50,0), 1):
            option = self.m.call("TestLCAnyOption", submenu, row)
            self.assertEqual(self.m.call("TestLCAnyOptionValue", option), chance)
        self.m.call("TestLCAnyOptionChange", self.m.dat_option(6), 11)
        self.m.call("TestLCPrepared", 0, 1)
        self.m.step()
        self.m.call("TestLCState", 2, 14, 0, 1)
        self.m.step()
        self.assertEqual(self.m.call("TestLCStateValue", 2), 60)
        self.assertEqual(self.m.info(12), 1)
        saves = self.m.info(1)
        for _ in range(4): self.m.step()
        self.assertEqual(self.m.info(1), saves)  # Full userdata restores cannot trigger a fresh save loop.

    def test_optimized_dat_six_chances_and_new_return_sequences(self):
        self.m.relocated_dat()
        self.m.step()
        submenu = self.m.call('TestLCAnySubmenu', self.m.dat_option(0))
        for option in (4, 5):
            row = self.m.call('TestLCAnyOption', submenu, option + 1)
            self.m.call('TestLCAnyOptionChange', row, 100)
            for i in range(6):
                row = self.m.call('TestLCAnyOption', submenu, i + 1)
                self.assertEqual(self.m.call('TestLCAnyOptionValue', row), 100 if i == option else 0)
            for side in (0, 1):
                self.m.call('TestLCAnyOptionChange', self.m.dat_option(1), side)
                self.assertEqual(self.m.info(5), option)
                self.m.call('TestLCPrepared', option, 1)
                self.m.step(); self.m.step()
                self.assertEqual((self.return_info(), self.return_info(1)), (2, 2))
                if option == 5:
                    self.m.call('TestLCReturnPosition', 0, 1, 4, 0, 1)
                    self.m.step()
                    self.assertEqual(self.return_info(), 3)

    def test_optimized_dat_saved_aerial_setting_executes_shared_air_command(self):
        self.m.relocated_dat()
        self.m.step()
        self.m.call('TestLCAnyOptionChange', self.m.dat_option(7), 1)
        self.assertEqual(self.m.call('TestLCAirInfo', 0, 0), self.m.call('TestLCAirActionID', 1))
        self.m.call('TestLCPrepared', 4, 1)
        self.m.step(); self.m.step()
        self.m.call('TestLCReturnPosition', 0, 3, 6, 0, 1)
        self.m.step()
        self.assertEqual(self.m.call('TestLCStateValue', 2), 65)
        self.assertEqual(self.m.call('TestLCAirInfo', 0, 1), 1)

    def test_optimized_dat_setup_menu_drives_approach_and_saved_low_height(self):
        self.m.relocated_dat()
        self.m.step()
        submenu = self.m.call('TestLCAnySubmenu', self.m.dat_option(8))
        entry = self.m.call('TestLCAnyOption', submenu, 0)
        height = self.m.call('TestLCAnyOption', submenu, 1)
        self.m.call('TestLCAnyOptionChange', entry, 1)
        self.assertEqual((self.m.call('TestLCStateValue', 2),self.m.call('TestLCEntryInfo',0)), (29,0))
        self.assertEqual(self.m.call('TestLCPosition', 2, 1), 70000)
        self.m.call('TestLCAnyOptionChange', entry, 2)
        self.m.step()
        self.assertEqual(self.m.call('TestLCEntryInfo', 2), 1)
        self.m.call('TestLCAnyOptionChange', height, 0)
        self.assertEqual(self.m.call('TestLCAnyOptionValue', height), 0)
        self.m.call('TestLCAnyOptionChange', entry, 0)
        self.assertEqual((self.m.call('TestLCStateValue', 2),self.m.call('TestLCEntryInfo',0)), (253,1))


if __name__ == "__main__":
    build()
    unittest.main(verbosity=2)
