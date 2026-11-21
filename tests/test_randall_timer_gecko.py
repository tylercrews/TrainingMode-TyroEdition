"""Execute the Randall Timer Gecko code in a PowerPC emulator.

python tests/test_randall_timer_gecko.py -v
Requires Unicorn (the existing build/test-deps install is supported). The
native functions the code calls are stubbed, and the timer, Randall's map GObj
and their joints are fixtures in memory. This is the code's original timer tint;
Tyro's own Randall Timer now draws a map of the loop instead (see
tests/test_randall_timer.py). With build/Start.dol and melee.iso, it
also runs the game's own stage-ID lookup and checks the hook's instruction.
"""
from pathlib import Path
import math
import re
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "build/test-deps"))
from unicorn import Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE
from unicorn import ppc_const as reg

TEXT = ROOT / "slippi-gecko-codes/randall-timer/randall-timer.txt"
HOOK, ALLOCATION, GOBJ = 0x8016E750, 0x81700000, 0x80E00000
STOP, R13 = 0x80004000, 0x804DB6A0
TIMER_HUD = 0x804A1078  # stc_matchhud->timer_gobj
STORY_EXTERNAL, STORY_INTERNAL, BATTLEFIELD_EXTERNAL = 8, 10, 31
WHITE, GRAY, PART0 = 0xFFFFFFFF, 0xB3B3B3FF, 0x8040207F
PURPLE, TEAL, OUT_X = 0xA064FFFF, 0x00B4A0FF, 52.675
LEFT, RIGHT = -1, 1
NATIVES = {
    0x803901F0: 'GObj_Create', 0x8039069C: 'GObj_AddGXLink',
    0x80225194: 'Stage_GetExternalID', 0x8022519C: 'Stage_ExternalToInternal',
    0x801C2BA4: 'Stage_GetMapGObj', 0x8000B1CC: 'JOBJ_GetWorldPosition',
}
# Fixture addresses: two timers, Randall's map GObj and its joints.
TIMERS = (0x80E10000, 0x80E80000)
MAP, MAP_DATA, MAP_ROOT, CLOUD = 0x80F00000, 0x80F01000, 0x80F02000, 0x80F03000
PARTS = 13


def gpr(n):
    return getattr(reg, f"UC_PPC_REG_{n}")


def code():
    """The C2 code's address and instructions, from the distributed text."""
    words = []
    for line in TEXT.read_text().splitlines():
        if line and line[0] not in '$*':
            words += [int(word, 16) for word in line.split()]
    assert words[0] >> 24 == 0xC2 and words[1] * 2 == len(words) - 2, 'one C2 code'
    return 0x80000000 | (words[0] & 0x1FFFFFF), words[2:]


def times(color, tint):
    channels = [(color >> s & 255) * (tint >> s & 255) // 255 for s in (24, 16, 8)]
    return channels[0] << 24 | channels[1] << 16 | channels[2] << 8 | (color & 255)


def untinted():
    return [PART0] + [GRAY] * 2 + [WHITE] * 10


def tinted(tint):
    colors = [times(c, tint) for c in untinted()]
    colors[12] = WHITE  # The spline joint's "DObj" is never touched.
    return colors


class Machine:
    def __init__(self):
        self.cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
        self.cpu.mem_map(0x80000000, 0x1800000)
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
        self.external, self.internal, self.map_present = STORY_EXTERNAL, STORY_INTERNAL, True
        self.position = (0.0, -18.9, 0.0)
        self.calls = []
        self.build_map()
        for timer in (0, 1):
            self.build_timer(timer)
        self.use_timer(0)
        self.cpu.hook_add(UC_HOOK_CODE, self.native)

    def word(self, at, value=None):
        if value is None:
            return struct.unpack(">I", self.cpu.mem_read(at, 4))[0]
        self.cpu.mem_write(at, struct.pack(">I", value & 0xFFFFFFFF))

    def build_map(self):
        self.cpu.mem_write(MAP, bytes(0x100))
        self.cpu.mem_write(MAP_DATA, bytes(0x100))
        self.cpu.mem_write(MAP_ROOT, bytes(0x100))
        self.cpu.mem_write(CLOUD, bytes(0x100))
        self.word(MAP + 0x28, MAP_ROOT)
        self.word(MAP + 0x2C, MAP_DATA)
        self.word(MAP_DATA + 0x14, 2)
        self.word(MAP_ROOT + 0x10, CLOUD)

    # A timer like IfAll's ScInfTim: ten joints of one DObj each under the
    # root (parts 1-10, the first two gray). Joint 11 has no DObj but a child
    # with two (parts 11 and 0); joint 12 is a spline pointing at part 12.
    def joint(self, timer, i):
        return TIMERS[timer] + 0x1000 + i * 0x100

    def dobj(self, timer, part):
        return TIMERS[timer] + 0x3000 + part * 0x40

    def material(self, timer, part):
        return self.dobj(timer, part) + 0x20

    def build_timer(self, timer, parts=PARTS, extra=0):
        base = TIMERS[timer]
        self.cpu.mem_write(base, bytes(0x8000))
        root, nested = self.joint(timer, 0), self.joint(timer, 13)
        self.word(base + 0x28, root)
        for part in range(parts + extra):
            dobj = self.dobj(timer, part)
            mobj = dobj + 0x10
            self.word(dobj + 0x8, mobj)
            self.word(mobj + 0xC, self.material(timer, part))
            self.word(self.material(timer, part) + 4,
                      PART0 if part == 0 else GRAY if part <= 2 else WHITE)
        self.word(root + 0x10, self.joint(timer, 1))
        for i in range(1, 13):
            j = self.joint(timer, i)
            self.word(j + 0x0C, root)
            self.word(j + 0x08, self.joint(timer, i + 1) if i < 12 else 0)
            self.word(j + 0x18, self.dobj(timer, i) if i <= 10 else 0)
        self.word(self.joint(timer, 11) + 0x10, nested)
        self.word(nested + 0x18, self.dobj(timer, 11))
        self.word(self.dobj(timer, 11) + 0x4, self.dobj(timer, 0))
        self.word(self.joint(timer, 12) + 0x14, 0x4000)  # JOBJ_SPLINE
        self.word(self.joint(timer, 12) + 0x18, self.dobj(timer, 12))
        if extra:  # More DObjs chained after part 0 than the code has room for.
            last = self.dobj(timer, 0)
            for part in range(PARTS, PARTS + extra):
                self.word(last + 0x4, self.dobj(timer, part))
                last = self.dobj(timer, part)

    def use_timer(self, timer):
        self.word(TIMER_HUD, 0 if timer is None else TIMERS[timer])

    def colors(self, timer=0, count=PARTS):
        return [self.word(self.material(timer, part) + 4) for part in range(count)]

    def native(self, cpu, address, size, _):
        name = NATIVES.get(address)
        if not name:
            return
        args = tuple(cpu.reg_read(gpr(n)) for n in range(3, 8))
        self.calls.append((name, args))
        result = None
        if name == 'GObj_Create':
            result = self.created
        elif name == 'Stage_GetExternalID':
            result = self.external
        elif name == 'Stage_ExternalToInternal':
            result = self.internal if args[0] == self.external else 0xFFFFFFFF
        elif name == 'Stage_GetMapGObj':
            result = MAP if args[0] == 2 and self.map_present else 0
        elif name == 'JOBJ_GetWorldPosition':
            assert args[0] == CLOUD and args[1] == 0, 'the cloud joint, no offset'
            cpu.mem_write(args[2], struct.pack(">3f", *self.position))
        if result is not None:
            cpu.reg_write(gpr(3), result)
        # Clobber the volatile registers, cr0 and the float scratch registers
        # the way a real call may.
        for n in (0, 4, 5, 6, 7, 8, 9, 10, 11, 12) + (() if result is not None else (3,)):
            cpu.reg_write(gpr(n), 0xDEAD0000 | n)
        for n in range(14):
            cpu.reg_write(getattr(reg, f"UC_PPC_REG_FPR{n}"), 0x7FF8DEAD0000DEAD)
        cpu.reg_write(reg.UC_PPC_REG_CR0, 0)
        cpu.reg_write(reg.UC_PPC_REG_PC, cpu.reg_read(reg.UC_PPC_REG_LR))

    def named(self, name):
        return [args for called, args in self.calls if called == name]

    def start_match(self):
        """Run StartMelee's hooked instruction. Returns the expected registers."""
        registers = {n: 0x80100000 + n * 0x100 for n in range(0, 32) if n not in (1, 2, 13)}
        for n, value in registers.items():
            self.cpu.reg_write(gpr(n), value)
        self.cpu.reg_write(reg.UC_PPC_REG_CR, 0x24680000)
        self.cpu.reg_write(reg.UC_PPC_REG_LR, 0x8016E74C)
        sp = self.cpu.reg_read(gpr(1))
        self.calls = []
        self.cpu.emu_start(HOOK, HOOK + 4)
        links = self.named('GObj_AddGXLink')
        if links:
            self.callback = links[0][1]
        registers[4] = 0x80170000  # Its replaced instruction, lis r4,0x8017.
        assert self.cpu.reg_read(gpr(1)) == sp, 'stack restored'
        return registers

    def draw(self, x=None, pass_number=2):
        if x is not None:
            self.position = (x, -18.9, 0.0)
        saved = {n: 0x81000000 + n for n in range(14, 32)}
        for n, value in saved.items():
            self.cpu.reg_write(gpr(n), value)
        sp = self.cpu.reg_read(gpr(1))
        self.calls = []
        self.cpu.reg_write(gpr(3), GOBJ)
        self.cpu.reg_write(gpr(4), pass_number)
        self.cpu.reg_write(reg.UC_PPC_REG_LR, STOP)
        self.cpu.emu_start(self.callback, STOP, count=100000)
        assert self.cpu.reg_read(reg.UC_PPC_REG_PC) == STOP, 'returned'
        assert self.cpu.reg_read(gpr(1)) == sp, 'stack restored'
        for n, value in saved.items():
            assert self.cpu.reg_read(gpr(n)) == value, f'r{n} preserved'


def source_wall():
    """RANDALL_TIMER_OUT_X, as src/stage_overlay_game.c defines it. Tyro's Randall
    Timer now draws a map of Randall's loop instead of tinting, but its stage walls
    are the same number."""
    source = (ROOT / "src/stage_overlay_game.c").read_text()
    return float(re.search(r"#define RANDALL_TIMER_OUT_X ([0-9.]+)f", source).group(1))


class HookTests(unittest.TestCase):
    def test_hook_creates_the_overlay_and_restores_every_register(self):
        m = Machine()
        registers = m.start_match()
        self.assertEqual([args[:3] for args in m.named('GObj_Create')], [(0, 7, 0)])
        (gobj, callback, link, priority, _), = m.named('GObj_AddGXLink')
        self.assertEqual((gobj, link, priority), (GOBJ, 5, 0))
        self.assertTrue(ALLOCATION <= callback < ALLOCATION + 0x400)
        for n, value in registers.items():
            self.assertEqual(m.cpu.reg_read(gpr(n)), value, f'r{n}')
        self.assertEqual(m.cpu.reg_read(reg.UC_PPC_REG_LR), 0x8016E74C)
        self.assertEqual(m.cpu.reg_read(reg.UC_PPC_REG_CR), 0x24680000)

    def test_failed_gobj_creation_skips_the_link(self):
        m = Machine()
        m.created = 0
        registers = m.start_match()
        self.assertEqual(m.named('GObj_AddGXLink'), [])
        for n, value in registers.items():
            self.assertEqual(m.cpu.reg_read(gpr(n)), value, f'r{n}')

    def test_its_numbers_are_training_modes(self):
        _, words = code()
        out = source_wall()
        self.assertEqual(out, OUT_X)
        data = struct.pack(f">{len(words)}I", *words)
        self.assertIn(struct.pack(">f", out) + struct.pack(">2I", PURPLE, TEAL), data)


class UpdateTests(unittest.TestCase):
    def setUp(self):
        self.m = Machine()
        self.m.start_match()

    def test_left_is_purple_right_is_teal_and_behind_the_stage_is_untinted(self):
        m = self.m
        just = 0.001
        for x, side in ((-97.5, LEFT), (-OUT_X - just, LEFT), (-OUT_X + just, 0), (0.0, 0), (OUT_X - just, 0),
                        (OUT_X + just, RIGHT), (97.5, RIGHT), (-OUT_X - just, LEFT), (0.0, 0)):
            with self.subTest(x=x):
                m.draw(x)
                self.assertEqual(m.colors(), untinted() if not side else tinted(PURPLE if side == LEFT else TEAL))
        self.assertEqual(m.colors(1), untinted())  # The other timer isn't the match's.

    def test_white_digits_become_the_tint_and_gray_separators_keep_their_shading(self):
        m = self.m
        m.draw(-80.0)
        colors = m.colors()
        self.assertEqual(colors[3:11], [PURPLE] * 8)
        self.assertEqual(colors[1], 0x7046B3FF)
        self.assertEqual(colors[0] & 255, 0x7F)  # The material's own alpha.

    def test_it_writes_only_when_the_side_changes(self):
        m = self.m
        m.draw(80.0)
        m.word(m.material(0, 5) + 4, 0x12345678)
        m.draw(81.0)
        self.assertEqual(m.word(m.material(0, 5) + 4), 0x12345678)
        m.draw(-81.0)
        self.assertEqual(m.colors(), tinted(PURPLE))
        m.draw(0.0)
        self.assertEqual(m.colors(), untinted())

    def test_other_stages_do_nothing(self):
        m = self.m
        m.external, m.internal = BATTLEFIELD_EXTERNAL, 24
        for x in (-80.0, 80.0):
            m.draw(x)
            self.assertEqual([name for name, _ in m.calls], ['Stage_GetExternalID', 'Stage_ExternalToInternal'])
        self.assertEqual(m.colors(), untinted())

    def test_no_randall_reads_as_behind_the_stage(self):
        for case in ('hazards', 'index', 'cloud', 'root'):
            with self.subTest(case=case):
                m = Machine(); m.start_match()
                m.draw(80.0)
                self.assertEqual(m.colors(), tinted(TEAL))
                if case == 'hazards':
                    m.map_present = False
                elif case == 'index':
                    m.word(MAP_DATA + 0x14, 3)
                elif case == 'cloud':
                    m.word(MAP_ROOT + 0x10, 0)
                else:
                    m.word(MAP + 0x28, 0)
                m.draw(80.0)
                self.assertEqual(m.colors(), untinted())
                self.assertEqual(m.named('JOBJ_GetWorldPosition'), [])

    def test_not_a_number_reads_as_behind_the_stage(self):
        m = self.m
        m.draw(80.0)
        m.draw(math.nan)
        self.assertEqual(m.colors(), untinted())

    def test_no_timer_does_nothing(self):
        m = self.m
        m.use_timer(None)
        m.draw(80.0)
        self.assertEqual(m.calls, [])
        self.assertEqual(m.colors(), untinted())

    def test_a_new_timer_saves_its_own_colors_and_forgets_the_old_one(self):
        m = self.m
        m.draw(80.0)
        m.use_timer(1)
        m.word(m.material(1, 3) + 4, 0x204060FF)
        m.draw(80.0)
        self.assertEqual(m.colors(0), tinted(TEAL))  # The old one is left alone.
        self.assertEqual(m.word(m.material(1, 3) + 4), times(0x204060FF, TEAL))
        m.draw(0.0)
        self.assertEqual(m.word(m.material(1, 3) + 4), 0x204060FF)

    def test_a_new_match_forgets_the_timer_even_at_the_same_address(self):
        m = self.m
        m.draw(-80.0)
        m.start_match()
        m.build_timer(0)  # A fresh timer where the old one was.
        m.draw(-80.0)  # Still out on the left: it has to tint again.
        self.assertEqual(m.colors(), tinted(PURPLE))
        m.draw(0.0)
        self.assertEqual(m.colors(), untinted())

    def test_it_saves_at_most_sixteen_materials(self):
        m = self.m
        m.build_timer(0, extra=6)  # 12 materials, then 6 more after part 0.
        m.draw(80.0)
        colors = m.colors(count=PARTS + 6)
        self.assertEqual(colors[:12], tinted(TEAL)[:12])
        self.assertEqual(colors[12], WHITE)  # The spline.
        # Tree order: parts 1-11, part 0, then the chain after it, until 16 are saved.
        self.assertEqual(colors[PARTS:PARTS + 4], [times(WHITE, TEAL)] * 4)
        self.assertEqual(colors[PARTS + 4:], [WHITE] * 2)

    def test_only_the_last_render_pass_runs_it(self):
        m = self.m
        for pass_number in (0, 1, 3):
            m.draw(80.0, pass_number)
            self.assertEqual(m.calls, [])
        self.assertEqual(m.colors(), untinted())


@unittest.skipUnless((ROOT / "build/Start.dol").exists(), "needs build/Start.dol")
class NativeTests(unittest.TestCase):
    def test_hook_replaces_lis_r4_and_yoshis_story_is_internal_stage_10(self):
        sys.path.insert(0, str(ROOT / "tests"))
        from test_score_storage import NativeMachine
        m = NativeMachine()
        self.assertEqual(struct.unpack(">I", m.cpu.mem_read(HOOK, 4))[0], 0x3C808017)  # lis r4,0x8017
        self.assertEqual(m.call(0x8022519C, STORY_EXTERNAL), STORY_INTERNAL)  # Stage_ExternalToInternal


if __name__ == "__main__":
    unittest.main()
