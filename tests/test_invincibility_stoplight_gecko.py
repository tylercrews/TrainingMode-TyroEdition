"""Execute the distributed Invincibility Stoplight Gecko payload in a PowerPC emulator.

python scripts/build_invincibility_stoplight_gecko.py
python tests/test_invincibility_stoplight_gecko.py -v
Requires Unicorn (the existing build/test-deps install is supported). The
checks alongside the other codes use their committed .gct files.
"""
from pathlib import Path
import random
import struct
import unittest

from test_invincibility_gecko import (FT, GX_REST, HOOK, HURT_STATUS, LAVENDER, OBJ, BLUE, ROOT,
                                      Machine as OverlayMachine, Uc, UC_ARCH_PPC, UC_MODE_32,
                                      UC_MODE_BIG_ENDIAN, UC_HOOK_CODE, gpr, install, reg)

CODES = ROOT / "slippi-gecko-codes"
STOPLIGHT_HOOK = 0x80080E1C
RED, YELLOW = 0xFF282896, 0xFFF00096  # TM_CUE_ENDING_RED_COLOR / TM_CUE_ENDING_YELLOW_COLOR.
ACTIONABLE_YELLOW = 0xFFF000DC
SCRIPT = 0x80403000
# The roll's last protected frames: 0x68 state 2 on frame 4, 0x68 state 0 on frame 20.
ROLL = [(2 << 26) | 4, (26 << 26) | 2, (2 << 26) | 20, 26 << 26, 0]


class Machine(OverlayMachine):
    """The Overlay tests' fighter and GX callback, with any set of the codes installed."""

    def __init__(self, allocation=0x81700000, overlay=False, actionable=False):
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
        self.w(FT + 0x89C, 1, "f")  # Animation rate.
        self.w(FT + 0x3E4, 100, "f")  # No script command due.
        self.w(FT + 0x10, 14)  # Wait
        self.colors = []
        self.actionable = actionable
        self.cpu.hook_add(UC_HOOK_CODE, self.native)
        self.allocations = {}
        allocation = install(self.cpu, CODES / "invincibility-stoplight/invincibility-stoplight.gct",
                             allocation, self.allocations)
        if overlay:
            allocation = install(self.cpu, CODES / "invincibility-overlay/invincibility-overlay.gct",
                                 allocation, self.allocations)
        if actionable:
            install(self.cpu, CODES / "actionable/actionable.gct", allocation, self.allocations)
        self.player(0, accent=1)
        self.fighter(0)

    def ledge(self, frames, respawn=0, held=0, star=-1, kind=None):
        """The engine's timers as they stand after this frame's update."""
        if kind is None:
            kind = 2 if frames else 1 if respawn else 0
        self.protect(game=kind)
        self.w(FT + 0x1990, frames)
        self.w(FT + 0x1994, respawn)
        self.b(FT + 0x2221, 0x80 if held else 0)
        self.b(FT + 0x221D, 0x02 if star >= 0 else 0)
        self.w(FT + 0x2004, max(star, 0))

    def script(self, words, timer, frame=10, offset=0):
        """Point the script at words[offset], timer frames before its next command."""
        self.cpu.mem_write(SCRIPT, struct.pack(">" + "I" * len(words), *words))
        self.w(FT + 0x3E4, timer, "f")
        self.w(FT + 0x3E8, frame, "f")
        self.w(FT + 0x3EC, SCRIPT + offset * 4)

    def capsules(self, count, state):
        self.b(FT + 0x119E, count)
        for i in range(15):
            self.w(FT + 0x11A0 + i * 0x4C, state)
            self.w(FT + 0x11A0 + i * 0x4C + 0x40, i)  # Bone index.


class InvincibilityStoplightGeckoTests(unittest.TestCase):
    def countdown(self, m, top=10):
        colors = []
        for frames in range(top, -1, -1):
            m.ledge(frames)
            colors.append(m.draw())
        return colors

    def test_ledge_countdown_is_four_red_then_four_yellow_at_two_allocations(self):
        for base in [0x80010000, 0x81700000]:
            with self.subTest(base=hex(base)):
                m = Machine(base)
                self.assertEqual(self.countdown(m, 12), [0] * 4 + [RED] * 4 + [YELLOW] * 4 + [0])

    def test_over_the_overlay_code_the_same_for_everyone(self):
        m = Machine(overlay=True)
        self.assertEqual(sorted(m.allocations), [HOOK, STOPLIGHT_HOOK])
        self.assertEqual(self.countdown(m), [BLUE] * 2 + [RED] * 4 + [YELLOW] * 4 + [0])
        m.player(0, accent=0, cpu=1)
        self.assertEqual(self.countdown(m), [LAVENDER] * 2 + [RED] * 4 + [YELLOW] * 4 + [0])

    def test_timers_combine_as_the_native_countdown_does(self):
        m = Machine()
        for args, color in [
            ((6, 1), RED), ((1, 6), RED), ((2, 6), RED), ((2, 12), 0), ((0, 2), YELLOW),
            ((2, 0, 1), 0),  # The 0x2221 hold: no timer ends it.
            ((0, 0, 0, -1, 1), 0),  # Set with no timer.
            ((0, 0, 0, 6), RED), ((0, 0, 0, 1), YELLOW), ((0, 0, 0, 9), 0),
            ((0, 0, 0, 0), 0),  # Starman with no timer.
        ]:
            with self.subTest(args=args):
                m.ledge(*args)
                self.assertEqual(m.draw(), color)

    def test_script_body_state_commands(self):
        m = Machine()
        colors = []
        for frame in range(10, 21):
            m.protect(script=0 if frame == 20 else 2)
            m.script(ROLL, 20 - frame, frame, offset=3)
            colors.append(m.draw())
        self.assertEqual(colors, [0] * 2 + [RED] * 4 + [YELLOW] * 4 + [0])
        m.protect(script=2)
        m.script([(2 << 26) | 16, 26 << 26, 0], 0)  # A synchronous timer still to run.
        self.assertEqual(m.draw(), RED)
        m.protect()
        m.capsules(15, 2)
        for words, timer, color in [
            ([27 << 26, 0], 2, YELLOW),  # 0x6C: every hurtbox normal.
            ([(28 << 26) | (4 << 18), 0], 6, RED),  # 0x70: bone 4 normal, so not the whole body.
            ([(28 << 26) | (99 << 18), 0], 1, 0),  # No hurtbox on bone 99.
        ]:
            with self.subTest(words=words):
                m.script(words, timer)
                self.assertEqual(m.draw(), color)

    def test_hitlag_holds_the_script_but_not_the_timers(self):
        m = Machine()
        m.b(FT + 0x2219, 0x04)
        m.protect(script=2)
        m.script([26 << 26, 0], 2)
        self.assertEqual(m.draw(), 0)
        m.protect()
        m.ledge(2)
        self.assertEqual(m.draw(), YELLOW)

    def test_unknown_ends_and_dead_or_sleeping_fighters_show_nothing(self):
        m = Machine(overlay=True)
        m.protect(script=2)
        m.w(FT + 0x3E4, 0, "f")
        m.w(FT + 0x3EC, 0x1000)  # An unreadable script.
        self.assertEqual(m.draw(), BLUE)
        m.w(FT + 0x3EC, 0)
        m.ledge(2)
        m.w(FT + 4, 18)  # Marth's counter, over a timer.
        m.w(FT + 0x10, 369)
        m.b(FT + 0x221B, 0xC0)
        self.assertEqual(m.draw(), BLUE)
        m.b(FT + 0x221B, 0)
        m.w(FT + 4, 14)  # Yoshi's double-jump armor.
        m.w(FT + 0x10, 27)
        m.w(FT + 0x18B4, 1, "f")
        self.assertEqual(m.draw(), BLUE)
        m.w(FT + 0x18B4, 0, "f")
        self.assertEqual(m.draw(), YELLOW)
        for flag in [0x40, 0x10]:  # dead, sleep
            m.b(FT + 0x221F, flag)
            self.assertEqual(m.draw(), 0)

    def test_render_restores_all_native_color_bytes_in_both_selected_slots(self):
        for overlay in [False, True]:
            for colanim in [0, 10]:
                with self.subTest(overlay=overlay, colanim=colanim):
                    m = Machine(overlay=overlay)
                    m.cpu.mem_write(FT + 0x408, random.Random(9).randbytes(0x180))
                    m.w(FT + 0x430, colanim)
                    m.ledge(6)
                    before = bytes(m.cpu.mem_read(FT, 0x2400))
                    selected = 0 if colanim else 1
                    for _ in range(3):
                        self.assertEqual(m.draw(), RED)
                        self.assertEqual(m.colors[-1][1], [0x80 if i == selected else 0 for i in range(3)])
                        self.assertEqual(bytes(m.cpu.mem_read(FT, 0x2400)), before)

    def test_draw_preserves_nonvolatile_registers(self):
        for overlay in [False, True]:
            with self.subTest(overlay=overlay):
                m = Machine(overlay=overlay)
                m.protect(script=2)
                m.script(ROLL, 2, 18, offset=3)
                rng = random.Random(72)
                for i in range(14, 32):
                    m.cpu.reg_write(gpr(i), rng.getrandbits(32))
                    m.cpu.reg_write(getattr(reg, f"UC_PPC_REG_FPR{i}"),
                                    struct.unpack(">Q", struct.pack(">d", i + 0.5))[0])
                m.cpu.reg_write(reg.UC_PPC_REG_CR, 0x12345678)
                expected = [m.cpu.reg_read(gpr(i)) for i in [1, 2, 13] + list(range(14, 32))]
                expected_fpr = [m.cpu.reg_read(getattr(reg, f"UC_PPC_REG_FPR{i}")) for i in range(14, 32)]
                self.assertEqual(m.draw(), YELLOW)
                self.assertEqual([m.cpu.reg_read(gpr(i)) for i in [1, 2, 13] + list(range(14, 32))], expected)
                self.assertEqual([m.cpu.reg_read(getattr(reg, f"UC_PPC_REG_FPR{i}")) for i in range(14, 32)],
                                 expected_fpr)
                self.assertEqual(m.cpu.reg_read(reg.UC_PPC_REG_CR) & 0x00FFF000, 0x00345000)  # cr2-cr4

    def test_with_the_overlay_and_actionable_codes_actionable_still_wins(self):
        m = Machine(overlay=True, actionable=True)
        self.assertEqual(sorted(m.allocations), [0x8006D9EC, 0x800805C8, HOOK, STOPLIGHT_HOOK])
        m.ledge(6)
        before = bytes(m.cpu.mem_read(FT, 0x2400))
        m.tick(1, 42, 1)  # Landing lag, still three frames out.
        self.assertEqual(m.draw(), RED)
        m.tick(2, 42, 3)
        self.assertEqual(m.draw(), ACTIONABLE_YELLOW)  # Training Mode's order: timing wins.
        self.assertEqual(bytes(m.cpu.mem_read(FT, 0x2400))[0x408:0x588], before[0x408:0x588])
        m.tick(3, 14, 0)
        m.tick(4, 14, 1)
        m.tick(5, 14, 2)
        m.ledge(10)
        self.assertEqual(m.draw(), BLUE)
        m.ledge(1)
        self.assertEqual(m.draw(), YELLOW)

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
            # The hook's displaced stw r0,4(r1), after the callback's mflr r0.
            self.assertEqual(read(iso, HOOK, 8), bytes.fromhex("7C0802A6 90010004"))
            # PlayerThink_Animation's timers, ahead of its freeze check: ledge,
            # respawn (both clear kind_game at 0 unless 0x2221 0x80 holds it), starman.
            self.assertEqual(read(iso, 0x8006A414, 48), bytes.fromhex(
                "807F1990 2C030000 4182005C 3803FFFF 901F1990 801F1990"
                "2C000000 40820048 881F2221 5400CFFF 4082003C 801F1994"))
            self.assertEqual(read(iso, 0x8006A478, 20), bytes.fromhex(
                "807F1994 2C030000 4182006C 3803FFFF 901F1994"))
            self.assertEqual(read(iso, 0x8006A4EC, 32), bytes.fromhex(
                "881F221D 5400FFFF 4182006C 807F2004 2C030000 41820060 3803FFFF 901F2004"))
            self.assertEqual(read(iso, 0x8006AA64, 12), bytes.fromhex("881F2219 5400F7FF 4082010C"))
            # Commands 0x68, 0x6C and 0x70 (table index = command - 10).
            self.assertEqual(read(iso, 0x803C06E8 + 16 * 4, 12),
                             bytes.fromhex("80071A14 80071A58 80071A9C"))
            self.assertEqual(read(iso, 0x80071A28, 16), bytes.fromhex(
                "80840008 80840000 548401BE 48009BF9"))  # kind_script = low 26 bits (0x8007B62C).
            self.assertEqual(read(iso, 0x80071A6C, 16), bytes.fromhex(
                "80840008 80840000 548401BE 48009649"))  # Every hurtbox (0x8007B0C0).
            self.assertEqual(read(iso, 0x80071AB0, 24), bytes.fromhex(
                "80A40008 A0850000 80A50000 5484F63E 54A503BE 48009665"))  # A bone's hurtbox (0x8007B128).
            self.assertEqual(read(iso, 0x8007B63C, 8), bytes.fromhex("8063002C 90831988"))
            self.assertEqual(read(iso, 0x8007B0D4, 4), bytes.fromhex("908511A0"))
            self.assertEqual(read(iso, 0x8007B14C, 12), bytes.fromhex("800311E0 38E311A0 7C002000"))


if __name__ == "__main__":
    unittest.main()
