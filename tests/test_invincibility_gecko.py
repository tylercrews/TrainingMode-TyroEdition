"""Execute the distributed Invincibility Overlay Gecko payload in a PowerPC emulator.

python scripts/build_invincibility_gecko.py
python tests/test_invincibility_gecko.py -v
Requires Unicorn (the existing build/test-deps install is supported).
The Actionable Yellow-Green checks also need scripts/build_actionable_gecko.py's output.
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
PLAYERS, TEAMS_FLAG = 0x80453080, 0x8046DB70
HOOK = 0x80080E18
YELLOW = 0xFFF000DC
RED, BLUE, YELLOW_PORT, GREEN = 0xFF464670, 0x4691FF70, 0xFFE14170, 0x4BE16470
LAVENDER = 0xA06EFF70
SHADES = {0: [RED, 0xFF701E70, 0xFF82D270],         # Red, Orange, Pink
          1: [BLUE, LAVENDER, 0x00FFFF70],           # Blue, Lavender, Cyan
          2: [GREEN, 0x8CFF2870, 0x228B2270]}        # Green, Lime, Forest
# Native 0x8007B868 (vanilla NTSC 1.02): max(0x221D bit 0x02, script and game hurt status).
HURT_STATUS = bytes.fromhex(
    "8083002C 8804221D 5400FFFF 4182000C 38600001 48000008 38600000 80041988"
    "7C001800 40810008 7C030378 8004198C 7C001800 4C810020 7C030378 4E800020")
# The rest of the fighter GX callback, reduced to its 0x800805C8 renderer call.
GX_REST = bytes.fromhex("90010004 9421FFF0 38A00001 4BFFF7A1 80010014 38210010 7C0803A6 4E800020")


def gpr(n):
    return getattr(reg, f"UC_PPC_REG_{n}")


def install(cpu, path, allocation, allocations):
    code = path.read_bytes()
    at = 8
    while at < len(code):
        first, second = struct.unpack_from(">II", code, at)
        at += 8
        if first >> 24 != 0xC2:
            continue
        address = 0x80000000 | (first & 0x1FFFFFF)
        body = bytearray(code[at:at + second * 8])
        at += second * 8
        end = allocation + len(body) - 4
        struct.pack_into(">I", body, len(body) - 4, 0x48000000 | ((address + 4 - end) & 0x3FFFFFC))
        cpu.mem_write(allocation, bytes(body))
        cpu.mem_write(address, struct.pack(">I", 0x48000000 | ((allocation - address) & 0x3FFFFFC)))
        allocations[address] = allocation
        allocation += (len(body) + 31) & ~31
    return allocation


class Machine:
    def __init__(self, allocation=0x81700000, actionable=False):
        self.cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
        self.cpu.mem_map(0x80000000, 0x1800000)
        self.cpu.reg_write(reg.UC_PPC_REG_MSR, 1 << 13)
        self.cpu.reg_write(gpr(1), 0x817F0000)
        self.cpu.reg_write(gpr(2), 0x804DF9E0)
        self.cpu.reg_write(gpr(13), 0x804DB6A0)
        self.cpu.mem_write(OBJ + 0x2C, struct.pack(">I", FT))
        self.cpu.mem_write(0x8007B868, HURT_STATUS)
        self.cpu.mem_write(HOOK, bytes.fromhex("7C0802A6"))
        self.cpu.mem_write(HOOK + 4, GX_REST)
        self.w(FT + 8, 1)
        self.w(FT + 0x89C, 1, "f")
        self.w(FT + 0x3E4, 100, "f")
        self.w(FT + 0x10, 14)  # Wait
        self.colors = []
        self.actionable = actionable
        self.cpu.hook_add(UC_HOOK_CODE, self.native)
        self.allocations = {}
        allocation = install(self.cpu, ROOT / "slippi-gecko-codes/invincibility-overlay/invincibility-overlay.gct",
                             allocation, self.allocations)
        if actionable:
            install(self.cpu, ROOT / "slippi-gecko-codes/actionable/actionable.gct", allocation, self.allocations)

    def w(self, address, value, kind="I"):
        self.cpu.mem_write(address, struct.pack(">" + kind, value))

    def b(self, address, value):
        self.cpu.mem_write(address, bytes([value]))

    def native(self, cpu, address, size, _):
        if address == 0x8006F484:  # Animation length, for Actionable's timing.
            cpu.reg_write(reg.UC_PPC_REG_FPR1, struct.unpack(">Q", struct.pack(">d", 5))[0])
            cpu.reg_write(reg.UC_PPC_REG_PC, cpu.reg_read(reg.UC_PPC_REG_LR))
        elif address == 0x800805CC or (address == 0x800805C8 and not self.actionable):
            # The lower-level renderer reads the tint the native chooser selects.
            assert cpu.reg_read(gpr(3)) == OBJ and cpu.reg_read(gpr(4)) == 2
            data = struct.unpack(">I", cpu.mem_read(OBJ + 0x2C, 4))[0]
            selected = 0 if struct.unpack(">I", cpu.mem_read(data + 0x430, 4))[0] else 1
            col = data + 0x408 + selected * 0x80
            color = struct.unpack(">I", cpu.mem_read(col + 0x2C, 4))[0]
            flags = [cpu.mem_read(data + 0x408 + i * 0x80 + 0x7C, 1)[0] & 0x80 for i in range(3)]
            self.colors.append((color if flags[selected] else 0, flags))
            cpu.reg_write(reg.UC_PPC_REG_PC, cpu.reg_read(reg.UC_PPC_REG_LR))

    def run(self, address, end=STOP):
        self.cpu.reg_write(reg.UC_PPC_REG_LR, STOP)
        self.cpu.emu_start(address, end, count=100000)
        assert self.cpu.reg_read(reg.UC_PPC_REG_PC) == end

    def player(self, ply, accent=0, team=0, cpu=0, present=1):
        block = PLAYERS + ply * 0xE90
        self.w(block, 2 if present else 0)
        self.w(block + 8, cpu)
        self.b(block + 0x45, accent)
        self.b(block + 0x47, team)

    def fighter(self, ply):
        self.b(FT + 0xC, ply)

    def teams(self, on):
        self.b(TEAMS_FLAG, on)

    def protect(self, script=0, game=0):
        self.w(FT + 0x1988, script)
        self.w(FT + 0x198C, game)

    def draw(self):
        self.cpu.reg_write(gpr(3), OBJ)
        self.cpu.reg_write(gpr(4), 2)
        self.run(HOOK)
        return self.colors[-1][0]

    def tick(self, number, state, frame):
        self.w(0x804DB6A0 - 0x49AC, number & 0xFFFFFFFF)
        self.w(FT + 0x10, state)
        self.w(FT + 0x894, frame, "f")
        self.cpu.reg_write(gpr(3), OBJ)
        self.run(0x8006D9EC, 0x8006D9F0)


class InvincibilityGeckoTests(unittest.TestCase):
    def test_ffa_port_colors_and_lavender_cpus_at_two_allocations(self):
        for base in [0x80010000, 0x81700000]:
            with self.subTest(base=hex(base)):
                m = Machine(base)
                for ply, color in enumerate([RED, BLUE, YELLOW_PORT, GREEN]):
                    m.player(ply, accent=ply)
                    m.fighter(ply)
                    m.protect(game=2)
                    self.assertEqual(m.draw(), color)
                    m.protect()
                    self.assertEqual(m.draw(), 0)
                m.player(3, accent=4, cpu=1)
                m.protect(script=1)
                self.assertEqual(m.draw(), LAVENDER)
                m.player(3, accent=0, cpu=1)  # CPU is Lavender even with a Red accent.
                self.assertEqual(m.draw(), LAVENDER)

    def test_ffa_ignores_team_bytes(self):
        m = Machine()
        for ply in range(4):
            m.player(ply, accent=ply, team=1)
        m.fighter(2)
        m.protect(game=1)
        self.assertEqual(m.draw(), YELLOW_PORT)

    def test_teammates_get_their_own_shades_in_port_order(self):
        layouts = [
            [1, 2, 1, 1],  # Blue 3 v Green 1
            [0, 0, 1, 0],  # Red 3 v Blue 1
            [2, 0, 2, 2],  # Green 3 v Red 1
            [0, 1, 1, 0],  # Doubles
            [2, 2, 0, 0],
        ]
        for teams in layouts:
            with self.subTest(teams=teams):
                m = Machine()
                m.teams(1)
                for ply, team in enumerate(teams):
                    # Teams accents (Red 0, Blue 1, Green 3) are not what picks the shade.
                    m.player(ply, accent=[0, 1, 3][team], team=team)
                m.protect(game=2)
                for ply, team in enumerate(teams):
                    m.fighter(ply)
                    member = teams[:ply].count(team)
                    self.assertEqual(m.draw(), SHADES[team][member])

    def test_teams_shade_cpus_and_skip_empty_ports(self):
        m = Machine()
        m.teams(1)
        m.player(0, accent=1, team=1)
        m.player(1, accent=1, team=1, present=0)  # Empty port left over from CSS.
        m.player(2, accent=4, team=1, cpu=1)
        m.player(3, accent=3, team=2)
        m.protect(script=1)
        m.fighter(2)
        self.assertEqual(m.draw(), LAVENDER)  # Blue's second member, and a CPU.
        m.player(2, accent=4, team=0, cpu=1)
        self.assertEqual(m.draw(), RED)  # First on Red: CPUs take the team color.
        m.player(2, accent=4, team=3, cpu=1)  # No palette: fall back to FFA's CPU color.
        self.assertEqual(m.draw(), LAVENDER)
        m.player(2, accent=2, team=3, cpu=0)
        self.assertEqual(m.draw(), YELLOW_PORT)

    def test_every_protection_source(self):
        m = Machine()
        m.player(0, accent=1)
        m.fighter(0)
        self.assertEqual(m.draw(), 0)
        for script, game in [(1, 0), (2, 0), (0, 1), (0, 2)]:
            m.protect(script, game)
            self.assertEqual(m.draw(), BLUE)
        m.protect()
        m.b(FT + 0x221D, 0x02)  # Native invincibility bit read by 0x8007B868.
        self.assertEqual(m.draw(), BLUE)
        m.b(FT + 0x221D, 0)
        m.w(FT + 4, 14)  # Yoshi's double-jump armor.
        m.w(FT + 0x18B4, 1, "f")
        self.assertEqual(m.draw(), BLUE)
        m.w(FT + 0x18B4, 0, "f")
        self.assertEqual(m.draw(), 0)
        m.w(FT + 4, 1)  # Armor only counts for Yoshi.
        m.w(FT + 0x18B4, 1, "f")
        self.assertEqual(m.draw(), 0)
        for kind in [18, 26]:  # Marth and Roy counter stance, both states.
            for state in [369, 371]:
                m.w(FT + 4, kind)
                m.w(FT + 0x10, state)
                m.b(FT + 0x221B, 0xC0)
                self.assertEqual(m.draw(), BLUE)
                m.b(FT + 0x221B, 0x40)  # Latched bit 6 alone is recovery, not counter.
                self.assertEqual(m.draw(), 0)
        m.w(FT + 4, 1)
        m.w(FT + 0x10, 14)
        m.b(FT + 0x221B, 0)
        # Whole-body SetAllHurtCapsules, but not a lone protected limb.
        m.b(FT + 0x119E, 3)
        for i in range(3):
            m.w(FT + 0x11A0 + i * 0x4C, 2)
        self.assertEqual(m.draw(), BLUE)
        m.w(FT + 0x11A0 + 0x4C, 0)
        self.assertEqual(m.draw(), 0)

    def test_dead_sleeping_and_bad_ports_draw_natively(self):
        m = Machine()
        m.player(0, accent=1)
        m.fighter(0)
        m.protect(game=2)
        for flag in [0x40, 0x10]:  # dead, sleep
            m.b(FT + 0x221F, flag)
            self.assertEqual(m.draw(), 0)
        m.b(FT + 0x221F, 0)
        m.fighter(6)
        self.assertEqual(m.draw(), 0)
        m.fighter(0)
        self.assertEqual(m.draw(), BLUE)

    def test_render_restores_all_native_color_bytes_in_both_selected_slots(self):
        for colanim in [0, 10]:
            with self.subTest(colanim=colanim):
                m = Machine()
                m.cpu.mem_write(FT + 0x408, random.Random(8).randbytes(0x180))
                m.w(FT + 0x430, colanim)
                m.player(0, accent=3)
                m.fighter(0)
                m.protect(game=1)
                before = bytes(m.cpu.mem_read(FT, 0x2400))
                for _ in range(3):
                    self.assertEqual(m.draw(), GREEN)
                    selected = 0 if colanim else 1
                    # Only the selected tint is on for the draw itself.
                    self.assertEqual(m.colors[-1][1], [0x80 if i == selected else 0 for i in range(3)])
                    self.assertEqual(bytes(m.cpu.mem_read(FT, 0x2400)), before)

    def test_draw_preserves_nonvolatile_registers(self):
        m = Machine()
        m.player(0, accent=1)
        m.fighter(0)
        m.protect(game=2)
        rng = random.Random(71)
        for i in range(14, 32):
            m.cpu.reg_write(gpr(i), rng.getrandbits(32))
            m.cpu.reg_write(getattr(reg, f"UC_PPC_REG_FPR{i}"),
                            struct.unpack(">Q", struct.pack(">d", i + 0.25))[0])
        m.cpu.reg_write(reg.UC_PPC_REG_CR, 0x12345678)
        expected = [m.cpu.reg_read(gpr(i)) for i in [1, 2, 13] + list(range(14, 32))]
        expected_fpr = [m.cpu.reg_read(getattr(reg, f"UC_PPC_REG_FPR{i}")) for i in range(14, 32)]
        self.assertEqual(m.draw(), BLUE)
        self.assertEqual([m.cpu.reg_read(gpr(i)) for i in [1, 2, 13] + list(range(14, 32))], expected)
        self.assertEqual([m.cpu.reg_read(getattr(reg, f"UC_PPC_REG_FPR{i}")) for i in range(14, 32)], expected_fpr)
        self.assertEqual(m.cpu.reg_read(reg.UC_PPC_REG_CR) & 0x00FFF000, 0x00345000)  # cr2-cr4

    @unittest.skipUnless((ROOT / "slippi-gecko-codes/actionable/actionable.gct").is_file(),
                         "Actionable Yellow-Green code not built")
    def test_runs_alongside_actionable_and_its_yellow_green_win(self):
        m = Machine(actionable=True)
        self.assertEqual(sorted(m.allocations), [0x8006D9EC, 0x800805C8, HOOK])
        m.player(0, accent=1)
        m.fighter(0)
        m.protect(game=2)
        before = bytes(m.cpu.mem_read(FT, 0x2400))
        m.tick(1, 42, 1)  # Landing lag, still three frames out.
        self.assertEqual(m.draw(), BLUE)
        m.tick(2, 42, 3)
        self.assertEqual(m.draw(), YELLOW)  # Training Mode's order: timing wins.
        self.assertEqual(bytes(m.cpu.mem_read(FT, 0x2400))[0x408:0x588], before[0x408:0x588])
        m.protect()
        m.tick(3, 42, 4)
        self.assertEqual(m.draw(), YELLOW)
        m.tick(4, 14, 0)
        m.tick(5, 14, 1)
        m.tick(6, 14, 2)
        self.assertEqual(m.draw(), 0)
        m.protect(script=1)
        self.assertEqual(m.draw(), BLUE)

    @unittest.skipUnless((ROOT / "melee.iso").is_file(), "Native Melee ISO not available")
    def test_native_addresses_match_the_disc(self):
        def read(iso, address, length):
            iso.seek(0x420)
            dol = struct.unpack(">I", iso.read(4))[0]
            iso.seek(dol)
            header = iso.read(0x100)
            for index in range(18):
                offset, base, size = [struct.unpack_from(">I", header, field + index * 4)[0]
                                      for field in (0, 0x48, 0x90)]
                if base <= address and address + length <= base + size:
                    iso.seek(dol + offset + address - base)
                    return iso.read(length)
            self.fail(f"Native DOL region missing: {address:x}")

        with (ROOT / "melee.iso").open("rb") as iso:
            self.assertEqual(iso.read(8), b"GALE01\x00\x02")
            self.assertEqual(read(iso, 0x8007B868, len(HURT_STATUS)), HURT_STATUS)
            # The hook's displaced instruction, and the renderer call made inside.
            self.assertEqual(read(iso, HOOK, 8), bytes.fromhex("7C0802A6 90010004"))
            self.assertEqual(read(iso, 0x80081100, 4), bytes.fromhex("4BFFF4C9"))  # bl 0x800805C8
            # Fighters' GX callback is 0x80080E18 (lis r3,0x8008; addi r0,r3,0x0E18).
            self.assertEqual(read(iso, 0x8008111C, 8), bytes.fromhex("3C608008 38030E18"))
            # Fighter_GetTeam: player blocks at 0x80453080, 0xE90 apart, team at +0x47.
            self.assertEqual(read(iso, 0x800333B8, 20),
                             bytes.fromhex("1C9F0E90 3C608045 38033080 7C602214 88630047"))
            # Match setup reads MatchInfo (0x8046B6A0) + 0x24D0 as the teams flag.
            self.assertEqual(read(iso, 0x8016E2D4, 4), bytes.fromhex("3BE3B6A0"))
            self.assertEqual(read(iso, 0x8016E390, 4), bytes.fromhex("881F24D0"))


if __name__ == "__main__":
    unittest.main()
