"""Run Techchase's production C menu/positioning/punish logic as PowerPC code."""
from pathlib import Path
import re
import struct
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "build/test-deps"))
LEDGE_MARGIN = 5  # TC_LEDGE_MARGIN
SIDES = ((0, -1), (1, 1))  # Player Side Left / Right, and the direction from the CPU to HMN.
DISTANCES = (10, 20, 30, 45)  # Right Next To, Close, Medium, Past Tech Roll; Random is 4.
DPAD_LEFT, DPAD_RIGHT, DPAD_DOWN, DPAD_UP, BUTTON_A = 1, 2, 4, 8, 0x100
SETTING_TECHCHASE, DISTANCE_INDEX, SIDE_INDEX = 27, 10, 11  # TM_SETTING_TECHCHASE, TM_TECHCHASE_DISTANCE/_SIDE
FOLLOWUP_INDEX = 7  # TM_TECHCHASE_FOLLOWUP, Spotdodge Punish's old index
# CPU Followup Action, in menu and saved order.
NONE, SPOTDODGE, ROLL_AWAY, ROLL_IN, SHINE, RANDOM = range(6)
FOX, MARTH, FALCO = 1, 18, 22  # internal fighter kinds
BUTTON_B, BUTTON_X, TRIGGER_R, PAD_LR = 0x200, 0x400, 0x20, 0x80000000
SHINE_START, SHINE_LOOP = 360, 361  # Fox's and Falco's grounded reflector
# TestTCASID's order.
(WAIT, TECH, GUARDON, GUARDREFLECT, ESCAPEF, ESCAPEB, KNEEBEND, JUMPF, ESCAPEAIR,
 LANDINGFALLSPECIAL, ESCAPE, DAMAGEFALL, GUARD, JUMPB, DAMAGEHI1) = range(15)
# The game's small-data bases, and where the tests keep PlCo.dat's common data.
R2, R13, COMMON = 0x804DF9E0, 0x804DB6A0, 0x80403000
# The native interrupt checks a followup's inputs go through, run from the
# local DOL (sizes from GTME01.map). Every other call out of them is stubbed
# inactive, except the state entries in ENTRIES, which go to the probe.
NATIVE = {
    0x8008A4D4: 0x170,  # Interrupt_AS_Wait
    0x80096540: 0x9C,   # Interrupt_Side-B_Grounded
    0x800D6824: 0x68,   # Interrupt_Neutral-B_Grounded
    0x800D68C0: 0x68,   # Interrupt_Down-B_Grounded
    0x800D695C: 0x68,   # Interrupt_Up-B_Grounded
    0x80099794: 0x78,   # Interrupt_SpotDodgeWithShieldHeldCheck
    0x80091A4C: 0x8C,   # Interrupt_Shield
    0x80092758: 0x118,  # Interrupt_AS_GuardOn
    0x80092BCC: 0x1C,   # DataOffset_PressingL/R_ThisFrame_SetFlag
    0x80093694: 0x78,   # Interrupt_GuardReflect
    0x8009980C: 0x88,   # Interrupt_SpotDodge (out of shield)
    0x8009917C: 0xE8,   # Interrupt_Roll
    0x800E8B14: 0x50,   # InterruptAS_FoxShineLoop
    0x800CAED0: 0xA8,   # Interrupt_Jump_Grounded
    0x80099A58: 0x44,   # Interrupt_EscapeAir_Airdodge
}
GUARDON_IASA, SHINE_LOOP_IASA, AIRDODGE_CHECK = 0x80092758, 0x800E8B14, 0x80099A58
ENTRIES = {
    0x80099894: "TestTCEnterSpotdodge", 0x800923B4: "TestTCEnterGuardOn",
    0x800939B4: "TestTCEnterGuardReflect", 0x80093850: "TestTCEnterGuardReflect",
    0x800992A8: "TestTCEnterRoll", 0x800CB4E0: "TestTCEnterKneebend",
    0x80099A9C: "TestTCEnterAirdodge", 0x800965DC: "TestTCEnterWrongSpecial",
}
# The grounded specials' per-character entries, by kind: Side-B, Neutral-B,
# Down-B and Up-B, from the lis/addi in each check.
SPECIAL_TABLES = {0x803C13E8: "TestTCEnterWrongSpecial", 0x803C167C: "TestTCEnterWrongSpecial",
                  0x803C1700: "TestTCEnterShine", 0x803C1784: "TestTCEnterWrongSpecial"}
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
        self.lines = []
        self.call("TestTCInit")

    def call(self, name, *args):
        self.cpu.reg_write(reg.UC_PPC_REG_1, 0x80400000)
        self.cpu.reg_write(reg.UC_PPC_REG_LR, 0x100000)
        for i, value in enumerate(args, 3):
            self.cpu.reg_write(getattr(reg, "UC_PPC_REG_" + str(i)), value & 0xFFFFFFFF)
        self.cpu.emu_start(self.symbols[name], 0x100000, count=100000)
        value = self.cpu.reg_read(reg.UC_PPC_REG_3)
        return value if value < 0x80000000 else value - 0x100000000

    def stage(self, stage_id):
        self.lines = []
        return self.call("TestTCStage", stage_id)

    def line(self, ax, ay, bx, by, flags=0, enabled=1):
        self.lines.append((ax, ay, bx, by, flags, enabled))
        return self.call("TestTCAddLine", *[round(v * 1000) for v in (ax, ay, bx, by)], flags, enabled)

    def player_x(self, cpu_x, direction, spacing):
        """Where HMN should stand: spacing to its side of the CPU, but at least
        LEDGE_MARGIN in from the ends of the enabled solid floors."""
        solid = [(min(ax, bx), max(ax, bx)) for ax, _, bx, _, flags, enabled in self.lines
                 if enabled and not flags & 0x100]
        left, right = min(l for l, _ in solid), max(r for _, r in solid)
        return min(max(cpu_x + direction * spacing, left + LEDGE_MARGIN), right - LEDGE_MARGIN)

    def hook_reset(self, test):
        """Return at Reset's entry: the savestate/engine work needs a real match."""
        reset = self.call("TestTCResetAddress")
        calls = []
        def body(cpu, address, size, user):
            calls.append(True)
            cpu.reg_write(reg.UC_PPC_REG_PC, cpu.reg_read(reg.UC_PPC_REG_LR))
        hook = self.cpu.hook_add(UC_HOOK_CODE, body, begin=reset, end=reset)
        test.addCleanup(self.cpu.hook_del, hook)
        return calls

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

    def native_followups(self, kind):
        """Run NATIVE's interrupt checks from the local DOL for a CPU of this
        kind, with the game's r2 constants, r13 and PlCo.dat's common data.
        Wait's IASA is the one Techchase calls; a test sets each other state's
        IASA itself, as the state's entry would."""
        dol = (ROOT / "build/Start.dol").read_bytes()
        def native(address, size):
            for i in range(18):
                offset, base, length = [struct.unpack_from(">I", dol, at + i * 4)[0] for at in (0, 0x48, 0x90)]
                if base <= address and address + size <= base + length:
                    return dol[offset + address - base:offset + address - base + size]
            raise AssertionError("Native routine missing from DOL")
        def jump(address, target):
            self.cpu.mem_write(address, struct.pack(">4I", 0x3D800000 | target >> 16,
                0x618C0000 | target & 0xFFFF, 0x7D8903A6, 0x4E800420))
        for address, size in NATIVE.items():
            code = native(address, size)
            for i in range(0, size, 4):
                word = struct.unpack_from(">I", code, i)[0]
                op, base, displacement = word >> 26, word >> 16 & 31, word & 0xFFFF
                if op == 18 and word & 1:
                    displacement = word & 0x03FFFFFC
                    if displacement & 0x02000000:
                        displacement -= 0x04000000
                    target = address + i + displacement
                    if target in ENTRIES:
                        jump(target, self.symbols[ENTRIES[target]])
                    elif target not in NATIVE:
                        self.cpu.mem_write(target, bytes.fromhex("386000004e800020"))
                elif op in (32, 48, 50) and base == 2:  # lwz/lfs/lfd from r2's constants
                    constant = R2 + displacement - (0x10000 if displacement & 0x8000 else 0)
                    self.cpu.mem_write(constant, native(constant, 8 if op == 50 else 4))
        for address, size in NATIVE.items():
            self.cpu.mem_write(address, native(address, size))
        for table, entry in SPECIAL_TABLES.items():
            self.cpu.mem_write(table + 4 * kind, struct.pack(">I", self.symbols[entry]))
        jump(self.symbols["TestTCNativeWaitIASA"], 0x8008A4D4)
        self.cpu.reg_write(reg.UC_PPC_REG_2, R2)
        self.cpu.reg_write(reg.UC_PPC_REG_13, R13)
        self.cpu.mem_write(COMMON, plco_common())
        self.cpu.mem_write(R13 - 0x514C, struct.pack(">I", COMMON))  # stc_ftcommon
        self.call("TestTCSetCPUKind", kind)

    def tick(self, state, iasa=0, airborne=0):
        """A frame: the input proc, then the event's followup step."""
        return self.call("TestTCFollowupTick", state, iasa, airborne)

    def state(self):
        return self.call("TestTCFollowupState")

    def held(self, down):
        """This frame's processed buttons, held or newly pressed (down)."""
        return self.call("TestTCHeld", down) & 0xFFFFFFFF

    def text(self, address):
        return bytes(self.cpu.mem_read(address, 24)).split(b"\0", 1)[0].decode()


def iso_file(filename):
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
                return iso.read(file_size)
    raise AssertionError(filename + " missing from base ISO")


def dat_roots(dat):
    _, size, reloc_count, public_count, external_count = struct.unpack_from(">5I", dat)
    root_offset = 32 + size + 4 * reloc_count
    string_offset = root_offset + 8 * (public_count + external_count)
    roots = {}
    for i in range(public_count):
        data, name = struct.unpack_from(">II", dat, root_offset + i * 8)
        roots[dat[string_offset + name:].split(b"\0", 1)[0].decode()] = 32 + data
    return roots


def plco_common():
    """The base ISO's PlCo.dat ftCommonData: the thresholds the native checks read."""
    dat = iso_file("PlCo.dat")
    common = 32 + struct.unpack_from(">I", dat, dat_roots(dat)["ftLoadCommonData"])[0]
    return dat[common:common + 0x400]


def native_floors(filename):
    """Read the main ISO's actual DAT collider flags/vertices and stage scale."""
    dat = iso_file(filename)
    roots = dat_roots(dat)
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

    def getups(self):
        return tuple(self.m.call("TestTCChance", j) for j in range(5, 9))

    def test_presets_and_only_preset_is_persisted(self):
        # Menu order: tech chances, then the saved number. Getup Attack Only
        # (6) is saved as 9 so older saves keep In Place / Toward through Tyro.
        expected = [((40, 30, 30, 0), 0), ((25, 25, 25, 25), 1), ((100, 0, 0, 0), 2),
                    ((0, 100, 0, 0), 3), ((0, 0, 100, 0), 4), ((0, 0, 0, 100), 5),
                    ((0, 0, 0, 100), 9), ((50, 0, 50, 0), 6), ((0, 50, 50, 0), 7),
                    ((60, 15, 15, 10), 8)]
        self.assertEqual(self.m.call("TestTCPresetCount"), len(expected))
        self.m.call("TestTCSetChance", 4, 81)  # Miss-tech wait stays session-local.
        for i, (percentages, saved) in enumerate(expected):
            self.assertEqual(self.m.call("TestTCSetPreset", i), saved)
            self.assertEqual(tuple(self.m.call("TestTCChance", j) for j in range(4)), percentages)
            self.assertEqual(self.m.call("TestTCChance", 4), 81)
            self.assertEqual(self.getups(), (0, 0, 0, 100) if i == 6 else (25, 25, 25, 25))
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
        self.m.call("TestTCSetPreset", 9)
        self.assertEqual(tuple(self.m.call("TestTCChance", j) for j in range(4)), expected[9][0])
        count = self.m.call("TestTCWriteCount")
        self.assertEqual(self.m.call("TestTCReloadChances"), count)
        self.assertEqual(self.m.call("TestTCPreset"), 9)
        self.assertEqual(tuple(self.m.call("TestTCChance", j) for j in range(4)), expected[9][0])
        self.assertEqual(tuple(self.m.call("TestTCChance", j) for j in range(4, 9)), (0, 25, 25, 25, 25))

    def test_getup_attack_only_sets_the_getups_and_leaving_it_restores_them(self):
        self.m.call("TestTCSetChance", 5, 70)  # A session edit: Stand 70%.
        edited = self.getups()
        self.m.call("TestTCSetPreset", 0)  # Other presets keep it.
        self.assertEqual(self.getups(), edited)
        self.m.call("TestTCSetPreset", 6)
        self.assertEqual(tuple(self.m.call("TestTCChance", j) for j in range(4)), (0, 0, 0, 100))
        self.assertEqual(self.getups(), (0, 0, 0, 100))
        self.m.call("TestTCSetPreset", 6)  # Reselecting it changes nothing.
        self.assertEqual(self.getups(), (0, 0, 0, 100))
        self.m.call("TestTCSetPreset", 5)  # Miss Tech Only: back to the even getups.
        self.assertEqual(tuple(self.m.call("TestTCChance", j) for j in range(4)), (0, 0, 0, 100))
        self.assertEqual(self.getups(), (25, 25, 25, 25))
        self.m.call("TestTCSetChance", 6, 70)  # Then later presets keep edits again.
        edited = self.getups()
        self.m.call("TestTCSetPreset", 1)
        self.assertEqual(self.getups(), edited)
        # Saved Getup Attack Only reloads as itself, with its getups.
        self.m.call("TestTCSetPreset", 6)
        self.m.call("TestTCReloadChances")
        self.assertEqual(self.m.call("TestTCPreset"), 6)
        self.assertEqual(self.getups(), (0, 0, 0, 100))
        # Every older saved number still reloads its old preset.
        for saved, preset in enumerate((0, 1, 2, 3, 4, 5, 7, 8, 9, 6)):
            with self.subTest(saved=saved):
                self.assertEqual(self.m.call("TestTCLoadSaved", saved), preset)
        self.assertEqual(self.m.call("TestTCLoadSaved", 10), 0)  # Out of range reads Original.

    def test_native_static_legal_stage_platforms_and_grounded_human(self):
        # Stadium hazards are disabled by the event, leaving its normal group
        # (floor lines 34-36, 51-54) active. Randall is removed on Yoshi's.
        for filename, stage_id, normal_lines in [
            ("GrSt.dat", 8, {2, 3, 4, 5, 6, 1}),
            ("GrNBa.dat", 31, None), ("GrOp.dat", 28, None),
            ("GrPs.dat", 3, {34, 35, 36, 51, 52, 53, 54}),
            ("GrNLa.dat", 32, None)
        ]:
            for side, direction in SIDES:
                with self.subTest(stage=filename, side=side):
                    self.m.stage(stage_id)
                    for index, ax, ay, bx, by, flags in native_floors(filename):
                        self.m.line(ax, ay, bx, by, flags, normal_lines is None or index in normal_lines)
                    self.m.call("TestTCSetSide", side)
                    self.m.call("TestTCChooseLocation", 0)
                    # Left keeps the original x=-5/x=5 spots; Right mirrors them.
                    self.assertEqual(self.m.position(0)[0], 5 * direction)
                    self.assertEqual(self.m.position(1)[0], -5 * direction)
                    for location in (1, 2, 3):
                        actual = self.m.call("TestTCChooseLocation", location)
                        expected = 0 if filename == "GrNLa.dat" or filename == "GrPs.dat" and location == 3 else location
                        self.assertEqual(actual, expected)
                        hmn, cpu = self.m.position(0), self.m.position(1)
                        self.assertAlmostEqual(hmn[0], self.m.player_x(cpu[0], direction, 10), places=3)
                        self.assertGreater((hmn[0] - cpu[0]) * direction, 0)  # HMN is on its side.
                        self.assertLess(hmn[1], 1)
                        if actual:
                            self.assertGreater(cpu[1], 40)
                        if actual in (0, 3):
                            self.assertAlmostEqual(cpu[0], -5 * direction, places=3)
                        self.assertEqual(self.m.call("TestTCFacing", 0), -direction)
                        self.assertEqual(self.m.call("TestTCFacing", 1), direction)
                    self.assertEqual(self.m.call("TestTCLocationDisabled"), filename == "GrNLa.dat")
                    self.assertEqual(self.m.call("TestTCWriteCount"), 0)  # No save writes for per-stage fallback.
        # Yoshi's side platforms overhang its ledges (x = +-56), so left of the
        # left platform's middle (-43.75) HMN stops 5 in from the ledge.
        self.m.stage(8)
        for index, ax, ay, bx, by, flags in native_floors("GrSt.dat"):
            self.m.line(ax, ay, bx, by, flags, index in {2, 3, 4, 5, 6, 1})
        self.m.call("TestTCSetSide", 0)
        self.assertEqual(self.m.call("TestTCChooseLocation", 1), 1)
        self.assertAlmostEqual(self.m.position(1)[0], -43.75, places=3)
        self.assertAlmostEqual(self.m.position(0)[0], -51, places=3)
        self.m.call("TestTCSetSide", 1)  # The old layout: toward stage center.
        self.assertEqual(self.m.call("TestTCChooseLocation", 1), 1)
        self.assertAlmostEqual(self.m.position(0)[0], -33.75, places=3)

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

    def test_saved_height_distance_and_side_on_every_available_legal_stage_target(self):
        m=self.m
        # Bound savestate/engine reset work; execute the real menu callbacks,
        # settings API writes and production live-geometry placement.
        reset_calls=m.hook_reset(self)
        m.call('TestTCRandom',250000,0)  # Random spacing is 15 here.
        for filename,stage,normal in [('GrSt.dat',8,{1,2,3,4,5,6}),('GrNBa.dat',31,None),
                                    ('GrOp.dat',28,None),('GrPs.dat',3,{34,35,36,51,52,53,54}),
                                    ('GrNLa.dat',32,None),('GrIz.dat',2,None)]:
            m.stage(stage)
            if filename=='GrIz.dat':
                m.line(-65,0,65,0)
                for ax,ay,bx,by in [(-49,11,-21,11),(21,35,49,35),(-14,43,14,43)]:m.line(ax,ay,bx,by,0x100)
            else:
                for index,ax,ay,bx,by,flags in native_floors(filename):
                    m.line(ax,ay,bx,by,flags,normal is None or index in normal)
            for side,direction in SIDES:
                m.call('TestTCChangeSide',side)
                self.assertEqual(m.call('TestTCPlacementSaved',SIDE_INDEX),side)
                self.assertEqual(m.call('TestTCLastWriteIndex'),SIDE_INDEX)
                for location in range(4):
                    m.call('TestTCChangePlacement',0,0)
                    selected=m.call('TestTCChooseLocation',location)
                    base=m.position(1)
                    for height,units in enumerate((27,15,40,54)):
                        for distance,spacing in enumerate(DISTANCES+(15,)):
                            with self.subTest(stage=filename,side=side,location=location,height=height,distance=distance):
                                m.call('TestTCChangePlacement',height,distance)
                                cpu,hmn=m.position(1),m.position(0)
                                self.assertEqual(m.call('TestTCPlacementSaved',9),height)
                                self.assertEqual(m.call('TestTCPlacementSaved',DISTANCE_INDEX),distance)
                                self.assertEqual(m.call('TestTCPlacementSaved',SIDE_INDEX),side)
                                self.assertEqual(m.call('TestTCLastWriteField'),SETTING_TECHCHASE)
                                self.assertEqual(m.call('TestTCLastWriteIndex'),DISTANCE_INDEX)
                                self.assertEqual(m.call('TestTCLocationAvailable',selected),1)
                                self.assertAlmostEqual(cpu[1]-base[1],units-27,delta=.002)
                                self.assertAlmostEqual(hmn[0],m.player_x(cpu[0],direction,spacing),delta=.002)
                                self.assertGreater((hmn[0]-cpu[0])*direction,0)
                                self.assertLess(hmn[1],1)
                                before=(cpu,hmn)
                                self.assertEqual(m.call('TestTCReloadPlacement'),1)
                                self.assertEqual((m.position(1),m.position(0)),before)
        self.assertGreater(len(reset_calls),0)

    def test_random_distance_rolls_each_reset_from_right_next_to_through_medium(self):
        m = self.m
        m.hook_reset(self)
        m.stage(32)
        m.line(-85.5657, 0, 85.5657, 0)
        self.assertEqual(m.call('TestTCRowCount', 1), 5)
        names = [bytes(m.cpu.mem_read(m.call('TestTCRowValue', 1, i), 16)).split(b'\0', 1)[0] for i in range(5)]
        self.assertEqual(names, [b'Right Next To', b'Close', b'Medium', b'Past Tech Roll', b'Random'])
        for distance, spacing in enumerate(DISTANCES):  # The fixed spacings never roll.
            m.call('TestTCRandom', 500000, 0)
            m.call('TestTCChangePlacement', 0, distance)
            self.assertEqual(m.call('TestTCRandomCalls', 1), 0)
            self.assertAlmostEqual(m.position(1)[0] - m.position(0)[0], spacing, delta=.002)
        # Each attempt rolls once, anywhere from Right Next To (10) to Medium (30).
        m.call('TestTCChangePlacement', 0, 4)
        for roll in (0, 250000, 500000, 731000, 999999):
            m.call('TestTCRandom', roll, 0)
            m.call('TestTCApply')
            self.assertEqual(m.call('TestTCRandomCalls', 1), 1)
            self.assertEqual(m.position(1)[0], 5)
            self.assertAlmostEqual(m.position(1)[0] - m.position(0)[0], 10 + 20 * roll / 1e6, delta=.002)
        self.assertEqual(m.call('TestTCPlacementSaved', DISTANCE_INDEX), 4)
        m.call('TestTCRandom', 500000, 0)
        self.assertEqual(m.call('TestTCReloadPlacement'), 1)  # The saved Random still rolls.
        self.assertAlmostEqual(m.position(1)[0] - m.position(0)[0], 20, delta=.002)
        self.assertEqual(m.call('TestTCRandomCalls', 1), 1)

    def test_player_side_menu_random_side_and_saving(self):
        m = self.m
        m.hook_reset(self)
        m.stage(32)
        m.line(-85.5657, 0, 85.5657, 0)
        name = lambda address: bytes(m.cpu.mem_read(address, 16)).split(b'\0', 1)[0]
        self.assertEqual(name(m.call('TestTCRowName', 0)), b'Player Side')
        self.assertEqual(m.call('TestTCRowIndex', 0), m.call('TestTCRowIndex', 1) + 1)  # Just after Player Distance.
        self.assertEqual(m.call('TestTCRowCount', 0), 3)
        self.assertEqual([name(m.call('TestTCRowValue', 0, i)) for i in range(3)], [b'Left', b'Right', b'Random'])
        for side, direction in SIDES:  # A fixed side never rolls.
            m.call('TestTCRandom', 0, 1 - side)
            m.call('TestTCChangeSide', side)
            self.assertEqual(m.call('TestTCRandomCalls', 0), 0)
            self.assertEqual(m.position(0)[0], 5 * direction)
        for coin, direction in ((0, -1), (1, 1), (0, -1)):  # Random flips again each reset.
            m.call('TestTCRandom', 0, coin)
            m.call('TestTCChangeSide', 2)
            self.assertEqual(m.call('TestTCRandomCalls', 0), 1)
            self.assertEqual((m.position(0)[0], m.position(1)[0]), (5 * direction, -5 * direction))
            self.assertEqual(m.call('TestTCFacing', 0), -direction)
            self.assertEqual(m.call('TestTCPlacementSaved', SIDE_INDEX), 2)
            self.assertEqual(m.call('TestTCLastWriteField'), SETTING_TECHCHASE)
            self.assertEqual(m.call('TestTCLastWriteIndex'), SIDE_INDEX)
        m.call('TestTCAutomatic', 0)  # Choosing a side restores automatic placement.
        m.call('TestTCChangeSide', 0)
        self.assertEqual(m.call('TestTCAutomatic', -1), 1)

    def test_dpad_left_and_right_pick_the_side_and_restart(self):
        m = self.m
        resets = m.hook_reset(self)
        m.call('TestTCSetSide', 0)
        m.call('TestTCAutomatic', 0)  # Manually moved fighters.
        writes = m.call('TestTCWriteCount')
        for buttons in (0, DPAD_UP, DPAD_DOWN, BUTTON_A, DPAD_UP | DPAD_DOWN):  # Other buttons do nothing.
            self.assertEqual(m.call('TestTCDPad', buttons), 0)
        self.assertEqual(len(resets), 0)
        # D-Pad left on Left only restarts, as it always has, keeping the fighters.
        self.assertEqual(m.call('TestTCDPad', DPAD_LEFT), 0)
        self.assertEqual((len(resets), m.call('TestTCWriteCount'), m.call('TestTCAutomatic', -1)), (1, writes, 0))
        # A new side is chosen, saved and laid out automatically, like the menu row.
        self.assertEqual(m.call('TestTCDPad', DPAD_RIGHT), 1)
        self.assertEqual(m.call('TestTCSideRow'), 1 | 1 << 8)
        self.assertEqual((len(resets), m.call('TestTCAutomatic', -1)), (2, 1))
        self.assertEqual((m.call('TestTCPlacementSaved', SIDE_INDEX), m.call('TestTCLastWriteIndex')), (1, SIDE_INDEX))
        self.assertEqual(m.call('TestTCDPad', DPAD_RIGHT), 1)
        self.assertEqual((len(resets), m.call('TestTCWriteCount')), (3, writes + 1))
        self.assertEqual(m.call('TestTCDPad', DPAD_LEFT), 0)
        self.assertEqual((len(resets), m.call('TestTCPlacementSaved', SIDE_INDEX)), (4, 0))
        m.call('TestTCSetSide', 2)  # From Random, the D-Pad picks a fixed side.
        self.assertEqual(m.call('TestTCDPad', DPAD_LEFT), 0)
        self.assertEqual(m.call('TestTCSideRow'), 0)
        self.assertEqual(m.call('TestTCDPad', DPAD_LEFT | DPAD_RIGHT), 0)  # Both at once: left wins.

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

    def asid(self, which):
        return self.m.call("TestTCASID", which)

    def test_followup_row_names_its_choices_and_shine_is_fox_and_falcos(self):
        m = self.m
        self.assertEqual(m.text(m.call("TestTCFollowupRowName")), "CPU Followup Action")
        self.assertEqual(m.call("TestTCFollowupRowCount"), 6)
        for kind, shine in ((FOX, "Shine"), (FALCO, "Shine"), (MARTH, "Shine (N/A)"), (0, "Shine (N/A)")):
            with self.subTest(kind=kind):
                self.assertEqual(m.call("TestTCSetCPUKind", kind), shine == "Shine")
                self.assertEqual([m.text(m.call("TestTCFollowupLabel", i)) for i in range(6)],
                                 ["None", "Spotdodge", "Roll Away", "Roll In", shine, "Random"])

    def test_followup_menu_skips_an_unavailable_shine_and_saves_each_choice(self):
        m = self.m
        m.hook_reset(self)
        m.call("TestTCSetCPUKind", MARTH)
        m.call("TestTCSetFollowupRow", ROLL_IN)
        self.assertEqual(m.call("TestTCChangeFollowup", SHINE), RANDOM)  # Right, past Shine.
        self.assertEqual((m.call("TestTCFollowupSaved"), m.call("TestTCLastWriteField"),
                          m.call("TestTCLastWriteIndex")), (RANDOM, SETTING_TECHCHASE, FOLLOWUP_INDEX))
        self.assertEqual(m.call("TestTCChangeFollowup", SHINE), ROLL_IN)  # Left, past it again.
        self.assertEqual(m.call("TestTCChangeFollowup", RANDOM), RANDOM)  # From None, left wraps.
        m.call("TestTCSetFollowupRow", NONE)
        self.assertEqual(m.call("TestTCChangeFollowup", RANDOM), RANDOM)
        self.assertEqual(m.call("TestTCChangeFollowup", NONE), NONE)  # Right wraps.
        m.call("TestTCSetFollowupRow", SPOTDODGE)
        self.assertEqual(m.call("TestTCChangeFollowup", SHINE), ROLL_IN)  # A picked N/A goes back.
        m.call("TestTCSetCPUKind", FOX)
        m.call("TestTCSetFollowupRow", ROLL_IN)
        self.assertEqual(m.call("TestTCChangeFollowup", SHINE), SHINE)
        self.assertEqual(m.call("TestTCFollowupSaved"), SHINE)

    def test_saved_followup_loads_and_a_saved_shine_is_none_for_other_cpus(self):
        m = self.m
        for kind in (FOX, FALCO, MARTH):
            m.call("TestTCSetCPUKind", kind)
            for saved in range(6):
                with self.subTest(kind=kind, saved=saved):
                    writes = m.call("TestTCWriteCount")
                    expected = NONE if saved == SHINE and kind == MARTH else saved
                    self.assertEqual(m.call("TestTCLoadFollowup", saved), expected)
                    self.assertEqual((m.call("TestTCWriteCount"), m.call("TestTCFollowupSaved")), (writes, saved))
        # Spotdodge Punish saved Off as 0 and On as 1: None and Spotdodge.
        self.assertEqual((m.call("TestTCLoadFollowup", 0), m.call("TestTCLoadFollowup", 1)), (NONE, SPOTDODGE))

    def test_random_followup_picks_again_each_attempt_and_includes_shine_for_fox_and_falco(self):
        m = self.m
        m.call("TestTCSetFollowupRow", RANDOM)
        for kind, choices in ((MARTH, (SPOTDODGE, ROLL_AWAY, ROLL_IN)), (FOX, (SPOTDODGE, ROLL_AWAY, ROLL_IN, SHINE)),
                              (FALCO, (SPOTDODGE, ROLL_AWAY, ROLL_IN, SHINE))):
            m.call("TestTCSetCPUKind", kind)
            for coin in range(8):
                with self.subTest(kind=kind, coin=coin):
                    self.assertEqual(m.call("TestTCRollFollowup", coin), choices[coin % len(choices)])
                    self.assertEqual(m.call("TestTCRandiMax"), len(choices))
        for kind in (MARTH, FOX):  # A fixed choice never rolls.
            m.call("TestTCSetCPUKind", kind)
            for choice in (NONE, SPOTDODGE, ROLL_AWAY, ROLL_IN, SHINE):
                m.call("TestTCSetFollowupRow", choice)
                expected = NONE if choice == SHINE and kind == MARTH else choice
                self.assertEqual((m.call("TestTCRollFollowup", 3), m.call("TestTCRandiMax")), (expected, 0))

    def test_rolls_shield_for_one_frame_then_roll_relative_to_stage_center(self):
        m = self.m
        m.stage(32)
        m.line(-85.5657, 0, 85.5657, 0)
        m.native_followups(MARTH)
        wait, tech, guardon = self.asid(WAIT), self.asid(TECH), self.asid(GUARDON)
        roll_f, roll_b = self.asid(ESCAPEF), self.asid(ESCAPEB)
        # (followup, CPU x, human x, roll direction): away from or toward x = 0,
        # whichever side the human is on. The CPU faces right, so left is EscapeB.
        for followup, cpu_x, hmn_x, direction in ((ROLL_AWAY, 20, 30, 1), (ROLL_AWAY, 20, 10, 1),
                                                  (ROLL_AWAY, -20, -30, -1), (ROLL_IN, 20, 10, -1),
                                                  (ROLL_IN, 20, 30, -1), (ROLL_IN, -20, -10, 1)):
            with self.subTest(followup=followup, cpu_x=cpu_x, hmn_x=hmn_x):
                m.call("TestTCFollowupBegin", followup, cpu_x * 1000, hmn_x * 1000, 0)
                self.assertEqual(m.tick(tech), 0)
                self.assertEqual(m.call("TestTCProtected"), 0)  # Not its last frame yet.
                # First Wait frame: shield only, held, never a fresh digital
                # press, so it's GuardOn, not GuardReflect's powershield.
                self.assertEqual(m.tick(wait), 1)
                self.assertEqual(m.state(), guardon)
                self.assertEqual((m.held(0), m.held(1)), (TRIGGER_R | PAD_LR, 0))
                self.assertEqual(m.call("TestTCRaw"), TRIGGER_R << 16)
                self.assertEqual(m.call("TestTCFollowupPhase"), 2)
                self.assertEqual(m.call("TestTCProtected"), 0)
                # Next frame, still holding: a fresh full smash to the side rolls.
                self.assertEqual(m.tick(-1, GUARDON_IASA), 0)
                self.assertEqual(m.state(), roll_f if direction > 0 else roll_b)
                self.assertEqual((m.call("TestTCStick", 0), m.call("TestTCTimer", 0)), (direction * 1000, 0))
                self.assertEqual(m.held(1), 0)  # Still no powershield press.
                self.assertEqual(m.call("TestTCEnterCalls"), 2)
                m.call("TestTCAnimation", 35, 1000, 0)
                for _ in range(34):
                    self.assertEqual(m.tick(-1), 0)
                    self.assertEqual((m.call("TestTCRaw"), m.call("TestTCProtected")), (0, 0))
                self.assertEqual(m.call("TestTCFollowupLength"), 35)
                self.assertEqual(m.tick(wait), 2)  # The reset timer starts now,
                self.assertEqual((m.call("TestTCFollowupPhase"), m.call("TestTCProtected")), (3, 1))  # intangible.
                self.assertEqual(m.call("TestTCEnterCalls"), 2)

    def test_shine_jumps_out_and_wavelands_away_then_the_cpu_is_intangible(self):
        for kind in (FOX, FALCO):
            with self.subTest(kind=kind):
                m = Machine()
                m.stage(32)
                m.line(-85.5657, 0, 85.5657, 0)
                m.native_followups(kind)
                asid = lambda which: m.call("TestTCASID", which)
                m.call("TestTCFollowupBegin", SHINE, 5000, -5000, 0)
                self.assertEqual(m.tick(asid(TECH)), 0)
                # First Wait frame: Down B, read through the cached special
                # classification, and nothing else (Side, Neutral or Up B).
                self.assertEqual(m.tick(asid(WAIT)), 1)
                self.assertEqual(m.state(), SHINE_START)
                self.assertEqual(m.held(1), BUTTON_B)
                self.assertEqual([m.call("TestTCTimer", i) for i in (2, 3, 4, 5)], [0, 255, 255, 255])
                self.assertEqual(m.call("TestTCRaw"), BUTTON_B << 16 | 0x81)  # B and down for the next frame.
                self.assertEqual(m.call("TestTCFollowupPhase"), 2)
                for _ in range(3):  # B stays held through the start.
                    self.assertEqual(m.tick(SHINE_START), 0)
                    self.assertEqual(m.call("TestTCRaw"), BUTTON_B << 16)
                    self.assertEqual(m.call("TestTCProtected"), 0)
                # Its loop: a fresh X, with B still held, jumps out.
                self.assertEqual(m.tick(SHINE_LOOP, SHINE_LOOP_IASA), 0)
                self.assertEqual(m.state(), asid(KNEEBEND))
                self.assertEqual((m.held(0), m.held(1)), (BUTTON_B | BUTTON_X, BUTTON_X))
                self.assertEqual(m.call("TestTCWaveland"), 1)  # Away from the human, with room.
                for _ in range(3):  # X lets go in jumpsquat: a short hop.
                    self.assertEqual(m.tick(asid(KNEEBEND)), 0)
                    self.assertEqual(m.call("TestTCRaw"), 0)
                # Its first airborne frame: an air dodge down and away, a waveland.
                self.assertEqual(m.tick(asid(JUMPF), AIRDODGE_CHECK, 1), 0)
                self.assertEqual(m.state(), asid(ESCAPEAIR))
                self.assertEqual((m.call("TestTCAirdodgeStick", 0), m.call("TestTCAirdodgeStick", 1)), (90, -90))
                self.assertEqual(m.held(1), TRIGGER_R | PAD_LR)
                self.assertEqual(m.tick(asid(ESCAPEAIR), 0, 1), 0)
                m.call("TestTCAnimation", 30, 3000, 0)  # The landing's slowed to its 10 frames.
                for _ in range(10):
                    self.assertEqual(m.tick(asid(LANDINGFALLSPECIAL)), 0)
                    self.assertEqual(m.call("TestTCProtected"), 0)
                self.assertEqual(m.call("TestTCFollowupLength"), 10)
                self.assertEqual(m.call("TestTCEnterCalls"), 3)  # Shine, jumpsquat, air dodge: nothing else.
                self.assertEqual(m.tick(asid(WAIT)), 2)
                self.assertEqual((m.call("TestTCFollowupPhase"), m.call("TestTCProtected")), (3, 1))

    def test_waveland_turns_back_or_lands_in_place_where_its_floor_ends(self):
        m = self.m
        m.stage(32)
        m.line(-60, 0, 60, 0)  # 0: the stage
        m.line(-15, 30, 15, 30, 0x100)  # 1: a small platform
        m.line(20, 30, 60, 30, 0x100)  # 2: a wide one
        m.native_followups(FOX)
        # (CPU x, human x, floor line, waveland direction and stick)
        for cpu_x, hmn_x, ground, direction in ((10, 0, 0, 1), (10, 20, 0, -1), (40, 30, 0, -1),
                                                (-40, -30, 0, 1), (0, -5, 1, 0), (25, 20, 2, 1),
                                                (55, 50, 2, -1), (-1, 1, -1, -1)):
            with self.subTest(cpu_x=cpu_x, hmn_x=hmn_x, ground=ground):
                m.call("TestTCFollowupBegin", SHINE, cpu_x * 1000, hmn_x * 1000, ground)
                m.tick(self.asid(TECH))
                m.tick(self.asid(WAIT))
                m.tick(SHINE_LOOP, SHINE_LOOP_IASA)
                self.assertEqual(m.call("TestTCWaveland"), direction)
                m.tick(self.asid(KNEEBEND))
                m.tick(self.asid(JUMPF), AIRDODGE_CHECK, 1)
                self.assertEqual(m.state(), self.asid(ESCAPEAIR))
                stick = (direction * 90, -90) if direction else (0, -127)
                self.assertEqual((m.call("TestTCAirdodgeStick", 0), m.call("TestTCAirdodgeStick", 1)), stick)

    def test_a_followup_hit_or_none_never_inputs_and_none_keeps_the_old_reset(self):
        m = self.m
        m.native_followups(FOX)
        wait = self.asid(WAIT)
        m.call("TestTCFollowupBegin", NONE, 0, 1000, -1)
        self.assertEqual(m.tick(self.asid(TECH)), 0)
        self.assertEqual(m.tick(wait), 2)  # None: the reset timer starts on the first Wait frame.
        self.assertEqual((m.call("TestTCEnterCalls"), m.call("TestTCRaw"), m.call("TestTCProtected")), (0, 0, 1))
        for followup in (ROLL_AWAY, SHINE):  # Hit out of its first state, it stops.
            m.call("TestTCFollowupBegin", followup, 0, 1000, -1)
            m.tick(self.asid(TECH))
            m.tick(wait)
            self.assertEqual(m.tick(self.asid(DAMAGEHI1), GUARDON_IASA if followup == ROLL_AWAY else SHINE_LOOP_IASA), 0)
            self.assertEqual((m.call("TestTCEnterCalls"), m.call("TestTCRaw")), (1, 0))

    def test_practice_clock_counts_real_unpaused_time_at_any_game_speed(self):
        m = self.m
        bus = 162000000  # The GameCube's bus clock; the time base runs at a quarter of it.
        second = bus // 4
        tick = 0xFFF00000  # Near the top, so the clock wraps on the way.
        m.call("TestTCClockStart", tick, bus)
        def frames(count, length, paused=0):
            nonlocal tick
            for _ in range(count):
                tick = (tick + length) & 0xFFFFFFFF
                seconds = m.call("TestTCClockFrame", tick, paused)
            return seconds
        self.assertEqual(frames(59, second // 60), 0)
        self.assertEqual(frames(1, second // 60), 1)
        self.assertEqual(frames(600, second // 60, paused=1), 1)  # The pause menu doesn't count.
        self.assertEqual(frames(1, second // 60), 1)  # The next frame adds only its own sixtieth.
        self.assertEqual(frames(59, second // 60), 2)
        # Slow motion: half as many frames a second, each twice as long.
        self.assertEqual(frames(30, second // 30), 3)
        self.assertEqual(frames(1, 3 * second + second // 2), 6)  # A long frame counts in full,
        self.assertEqual(frames(30, second // 60), 7)  # and its leftover half second carries.

    def test_cpu_palette_and_off_preserve_native_colors(self):
        self.assertEqual(self.m.call("TestTCMenuCPUColorCount"), 8)
        for value in range(8):
            self.assertEqual(self.m.call("TestTCSetCPUColor", value), value)
            self.assertEqual(self.m.call("TestTCLastWriteIndex"), 8)
        title = self.m.call("TestTCChancesTitle")
        self.assertEqual(bytes(self.m.cpu.mem_read(title, 32)).split(b"\0", 1)[0], b"Tech Chances")
        self.assertEqual(self.m.call("TestTCNativeColorsUnchanged"), 1)


if __name__ == "__main__":
    build()
    unittest.main(verbosity=2)
