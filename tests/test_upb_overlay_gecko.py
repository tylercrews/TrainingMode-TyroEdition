"""Execute the Up-B Overlay Gecko codes in a PowerPC emulator.

python tests/test_upb_overlay_gecko.py -v
Requires Unicorn (the existing build/test-deps install is supported). The
native functions the codes call are stubbed and the GX FIFO writes are
recorded, so this checks what the codes ask the GPU to draw, not the picture.
"""
import importlib.util
import math
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "build/test-deps"))
from unicorn import (Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE,
                     UC_HOOK_MEM_WRITE)
from unicorn import ppc_const as reg

_spec = importlib.util.spec_from_file_location('upb_overlay_numbers', ROOT / 'scripts/upb_overlay_numbers.py')
numbers = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(numbers)

FOLDER = numbers.FOLDER
CODES = {c.key: c for c in numbers.CODES}
STOP, R13, FIFO, SCRATCH = 0x80004000, 0x804DB6A0, 0xCC008000, 0x80F00000
GX = {'GX_LINES': 0xA8, 'GX_LINESTRIP': 0xB0, 'GX_POINTS': 0xB8}
# External stage ID: (left line, right line), live ledges (x, y) from
# scripts/sweetspot_dj_platforms.py.
STAGES = {
    2: ((0x03, 0x07), (63.35, 0.62)),
    3: ((0x33, 0x36), (87.75, 0.0)),
    8: ((0x02, 0x06), (56.0, -3.5)),
    28: ((0x03, 0x05), (77.27, 0.01)),
    31: ((0x00, 0x05), (68.4, 0.0)),
    32: ((0x00, 0x02), (85.57, 0.0)),
}
NATIVES = {
    0x803901F0: 'GObj_Create', 0x8039069C: 'GObj_AddGXLink', 0x80034110: 'Player_GetEntity',
    0x80225194: 'Stage_GetExternalID',
    0x80053ECC: 'Stage_GetLeftOfLineCoordinates', 0x80053DA4: 'Stage_GetRightOfLineCoordinates',
    0x8036C244: 'HSD_ClearVtxDesc', 0x80341510: 'GXSetCurrentMtx', 0x8036A288: 'COBJ_GetCurrent',
    0x803695F0: 'COBJ_GetViewingMtx', 0x80341494: 'GXLoadPosMtxImm',
    0x80361A74: 'HSD_StateSetLineWidth', 0x80361C60: 'HSD_StateSetPointSize',
    0x8036199C: 'HSD_SetupRenderMode', 0x80361BB8: 'HSD_StateSetZMode', 0x8033C414: 'GXSetVtxAttrFmt',
    0x8033BF00: 'GXSetVtxDesc', 0x80361AD8: 'HSD_StateSetCullMode', 0x8033D0DC: 'GXBegin',
}
SETS_CR0 = {'cmplwi', 'cmpwi', 'rlwinm.'}


def gpr(n):
    return getattr(reg, f"UC_PPC_REG_{n}")


def f32(value):
    return struct.unpack(">f", struct.pack(">f", value))[0]


def code(slug):
    """The C2 code's address and instructions, from the distributed text."""
    words = []
    for line in (FOLDER / f'{slug}-up-b-overlay.txt').read_text().splitlines():
        if line and line[0] not in '$*':
            words += [int(word, 16) for word in line.split()]
    assert words[0] >> 24 == 0xC2 and words[1] * 2 == len(words) - 2, 'one C2 code'
    return 0x80000000 | (words[0] & 0x1FFFFFF), words[2:]


def replaced(words):
    """The original instruction: the last one before the return slot and any nop padding."""
    at = len(words) - 2
    while words[at] == 0x60000000:
        at -= 1
    return words[at]


def shapes(slug):
    """(GX shape, width, count, color, points) from the code's .asm."""
    found = []
    for line in numbers.asm_path(CODES[slug]).read_text().splitlines():
        line = line.split('#')[0].strip()
        if line.startswith('shape '):
            kind, width, count, color = (part.strip() for part in line[6:].split(','))
            found.append((GX[kind], int(width), int(count), int(color, 16), []))
        elif line.startswith('point '):
            found[-1][4].append(tuple(f32(float(part)) for part in line[6:].split(',')))
    return found


def expected(slug, left, right):
    """Each ledge's shapes in order: left (points mirrored), then right."""
    drawn = []
    for (x, y), outward in ((left, -1.0), (right, 1.0)):
        for kind, width, count, color, points in shapes(slug):
            drawn.append((kind, width, count,
                          [(f32(f32(dx * outward) + f32(x)), f32(dy + f32(y)), 0.0, color) for dx, dy in points]))
    return drawn


class Machine:
    def __init__(self, slugs=tuple(CODES)):
        self.cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
        self.cpu.mem_map(0x80000000, 0x1800000)
        self.cpu.mem_map(0xCC000000, 0x10000)
        self.cpu.reg_write(reg.UC_PPC_REG_MSR, 1 << 13)  # Floating point on.
        self.cpu.reg_write(gpr(1), 0x817F0000)
        self.cpu.reg_write(gpr(13), R13)
        self.allocations, self.gobjs, self.originals = {}, {}, {}
        for i, slug in enumerate(slugs):
            hook, words = code(slug)
            assert hook == CODES[slug].hook
            self.originals[slug] = replaced(words)
            # Install like the code handler: branch in, and the final word back out.
            allocation = 0x81700000 + i * 0x1000
            end = allocation + 4 * (len(words) - 1)
            words[-1] = 0x48000000 | ((hook + 4 - end) & 0x3FFFFFC)
            self.cpu.mem_write(allocation, struct.pack(f">{len(words)}I", *words))
            self.w(hook, 0x48000000 | ((allocation - hook) & 0x3FFFFFC))
            self.allocations[slug], self.gobjs[slug] = allocation, 0x80E00000 + i * 0x100
        self.stage, self.ledges, self.created = 32, ((-85.57, 0.0), (85.57, 0.0)), None
        self.players = {}  # slot: (gobj, kind)
        self.calls, self.fifo, self.callbacks = [], [], {}
        self.cpu.hook_add(UC_HOOK_CODE, self.native)
        self.cpu.hook_add(UC_HOOK_MEM_WRITE, self.write, begin=FIFO, end=FIFO + 3)

    def w(self, address, value, kind="I"):
        self.cpu.mem_write(address, struct.pack(">" + kind, value))

    def native(self, cpu, address, size, _):
        name = NATIVES.get(address)
        if not name:
            return
        args = tuple(cpu.reg_read(gpr(n)) for n in range(3, 8))
        self.calls.append((name, args))
        if name == 'GObj_Create':
            cpu.reg_write(gpr(3), self.created)
        elif name == 'Player_GetEntity':
            assert args[0] < 6, 'slot in range'
            cpu.reg_write(gpr(3), self.players.get(args[0], (0, 0))[0])
        elif name == 'Stage_GetExternalID':
            cpu.reg_write(gpr(3), self.stage)
        elif name.startswith('Stage_Get'):
            x, y = self.ledges[name == 'Stage_GetRightOfLineCoordinates']
            cpu.mem_write(args[1], struct.pack(">3f", x, y, 0))
        elif name == 'COBJ_GetCurrent':
            cpu.reg_write(gpr(3), 0x80D00000)
        # Clobber the volatile registers and cr0 the way a real call may.
        for n in (0, 4, 5, 6, 7, 8, 9, 10, 11, 12):
            cpu.reg_write(gpr(n), 0xDEAD0000 | n)
        for n in range(14):
            cpu.reg_write(getattr(reg, f"UC_PPC_REG_FPR{n}"), 0x7FF8DEAD00000000 | n)
        cpu.reg_write(reg.UC_PPC_REG_CR0, 0)
        cpu.reg_write(reg.UC_PPC_REG_PC, cpu.reg_read(reg.UC_PPC_REG_LR))

    def write(self, cpu, access, address, size, value, _):
        self.fifo.append((size, value & 0xFFFFFFFF))

    def named(self, name):
        return [args for called, args in self.calls if called == name]

    def add_player(self, slot, kind, userdata=True):
        gobj = 0x80C00000 + slot * 0x100
        data = 0x80C80000 + slot * 0x1000
        self.w(gobj + 0x2C, data if userdata else 0)
        self.w(data + 0x4, kind)
        self.players[slot] = (gobj, kind)

    def registers(self):
        state = {n: self.cpu.reg_read(gpr(n)) for n in range(32) if n not in (2, 13)}
        state['lr'] = self.cpu.reg_read(reg.UC_PPC_REG_LR)
        state['cr'] = self.cpu.reg_read(reg.UC_PPC_REG_CR)
        return state

    def prime(self):
        """Distinct registers and the stack words the epilogue hooks load."""
        for n in range(32):
            if n not in (1, 2, 13):
                self.cpu.reg_write(gpr(n), 0x80100000 + n * 0x100)
        self.cpu.reg_write(gpr(1), 0x817F0000)
        self.cpu.reg_write(reg.UC_PPC_REG_LR, 0x8016E8B0)
        self.cpu.reg_write(reg.UC_PPC_REG_CR, 0x24680000)
        for offset in (24, 28, 36):
            self.w(0x817F0000 + offset, 0x80200000 + offset)

    def start_match(self, slug):
        """Run StartMelee's hooked instruction. Returns the registers its
        original instruction alone leaves."""
        self.prime()
        self.w(SCRATCH, self.originals[slug])
        self.cpu.emu_start(SCRATCH, SCRATCH + 4)
        wanted = self.registers()
        self.created = self.gobjs[slug] if self.created != 0 else 0
        self.prime()
        hook = CODES[slug].hook
        self.calls = []
        self.cpu.emu_start(hook, hook + 4)
        links = self.named('GObj_AddGXLink')
        if links:
            self.callbacks[slug] = links[0][1]
        return wanted

    def draw(self, slug, pass_number=2):
        saved = {n: 0x81000000 + n for n in range(14, 32)}
        for n, value in saved.items():
            self.cpu.reg_write(gpr(n), value)
        sp = self.cpu.reg_read(gpr(1))
        self.calls, self.fifo = [], []
        self.cpu.reg_write(gpr(3), self.gobjs[slug])
        self.cpu.reg_write(gpr(4), pass_number)
        self.cpu.reg_write(reg.UC_PPC_REG_LR, STOP)
        self.cpu.emu_start(self.callbacks[slug], STOP)
        assert self.cpu.reg_read(gpr(1)) == sp, 'stack restored'
        for n, value in saved.items():
            assert self.cpu.reg_read(gpr(n)) == value, f'r{n} preserved'

    def drawn(self):
        """GXBegin batches as (shape, width, count, [(x, y, z, color)])."""
        assert all(size == 4 for size, _ in self.fifo)
        words = [value for _, value in self.fifo]
        vertices = [(*struct.unpack(">3f", struct.pack(">3I", *words[i:i + 3])), words[i + 3])
                    for i in range(0, len(words), 4)]
        begins, widths = self.named('GXBegin'), self.named('HSD_StateSetLineWidth')
        batches = []
        for (shape, _, count, *_), (width, *_) in zip(begins, widths):
            batches.append((shape, width, count, vertices[:count]))
            vertices = vertices[count:]
        assert vertices == [], 'every vertex in a batch'
        return batches


class HookTests(unittest.TestCase):
    def test_hooks_are_distinct_and_outside_the_other_codes(self):
        hooks = [c.hook for c in numbers.CODES]
        self.assertEqual(len(hooks), len(set(hooks)))
        taken = {0x8016E748, 0x8016E74C, 0x8016E904, 0x8016E8C0, 0x8016E8C8, 0x8016E8E0, 0x8016E8E8}
        self.assertFalse(taken & set(hooks))
        self.assertTrue(all(0x8016E730 <= hook < 0x8016E934 for hook in hooks), 'inside StartMelee')

    def test_each_code_creates_its_overlay_then_acts_like_the_replaced_instruction(self):
        m = Machine()
        for slug, character in CODES.items():
            with self.subTest(slug=slug):
                wanted = m.start_match(slug)
                self.assertEqual([args[:3] for args in m.named('GObj_Create')], [(0, 7, 0)])
                (gobj, callback, link, priority, _), = m.named('GObj_AddGXLink')
                self.assertEqual((gobj, link, priority), (m.gobjs[slug], 5, 0))
                self.assertTrue(m.allocations[slug] <= callback < m.allocations[slug] + 0x1000)
                got = m.registers()
                if character.original.split()[0] not in SETS_CR0:
                    # Only cr0 may differ: nothing reads it across these hooks.
                    got['cr'] &= 0x0FFFFFFF
                    wanted['cr'] &= 0x0FFFFFFF
                self.assertEqual(got, wanted)
        self.assertEqual(len(set(m.callbacks.values())), len(CODES))

    def test_compares_still_set_cr0_for_the_native_branch(self):
        m = Machine(('sheik', 'ganondorf'))
        for slug, register, value, flags in (('sheik', 12, 0, 0b0010), ('sheik', 12, 0x80400000, 0b0100),
                                             ('ganondorf', 3, 0, 0b0010), ('ganondorf', 3, 5, 0b0100)):
            with self.subTest(slug=slug, value=value):
                m.prime()
                m.cpu.reg_write(gpr(register), value)
                m.created = m.gobjs[slug]
                hook = CODES[slug].hook
                m.cpu.emu_start(hook, hook + 4)
                self.assertEqual(m.cpu.reg_read(reg.UC_PPC_REG_CR) >> 28 & 0b1110, flags)

    def test_failed_gobj_creation_skips_the_draw_link(self):
        for slug in CODES:
            with self.subTest(slug=slug):
                m = Machine((slug,))
                m.created = 0
                wanted = m.start_match(slug)
                self.assertEqual(m.named('GObj_AddGXLink'), [])
                self.assertEqual(m.registers()[30], wanted[30])


class DrawTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.m = Machine()
        for slug in CODES:
            cls.m.start_match(slug)

    def setUp(self):
        self.m.players, self.m.stage = {}, 32

    def assertDrawn(self, actual, wanted):
        self.assertEqual([batch[:3] for batch in actual], [batch[:3] for batch in wanted])
        for got_batch, want_batch in zip(actual, wanted):
            for got, want in zip(got_batch[3], want_batch[3]):
                self.assertAlmostEqual(got[0], want[0], places=3)
                self.assertAlmostEqual(got[1], want[1], places=3)
                self.assertEqual(got[2:], want[2:])

    def test_every_legal_stage_draws_the_shapes_at_both_ledges(self):
        m = self.m
        for slug, character in CODES.items():
            m.players = {}
            m.add_player(1, character.kind)
            for stage, ((left_line, right_line), (x, y)) in STAGES.items():
                with self.subTest(slug=slug, stage=stage):
                    m.stage, m.ledges = stage, ((-x, y), (x, y))
                    m.draw(slug)
                    self.assertEqual([args[0] for args in m.named('Stage_GetLeftOfLineCoordinates')], [left_line])
                    self.assertEqual([args[0] for args in m.named('Stage_GetRightOfLineCoordinates')], [right_line])
                    self.assertEqual(m.named('HSD_StateSetLineWidth'), m.named('HSD_StateSetPointSize'))
                    self.assertTrue(all(args[:3] == (0, 7, 0) for args in m.named('HSD_StateSetZMode')))
                    self.assertDrawn(m.drawn(), expected(slug, *m.ledges))

    def test_shapes_follow_each_ledge_separately(self):
        m = self.m
        m.add_player(0, CODES['zelda'].kind)
        m.stage, m.ledges = 8, ((-70.5, 2.25), (81.25, -3.5))
        m.draw('zelda')
        self.assertDrawn(m.drawn(), expected('zelda', *m.ledges))

    def test_fox_on_final_destination_by_hand(self):
        # Target: (2 ECB + 11 snap) / 2 = 6.5 out, 13 down. Ring: 81.5, but for
        # the 16.84 degrees either side of straight the stick can't aim: the
        # least tilted axis, 23/80, with the most the circle allows, 76/80.
        m = self.m
        m.add_player(3, CODES['fox'].kind)
        m.stage, m.ledges = 32, ((-85.5657, 0.0), (85.5657, 0.0))
        m.draw('fox')
        batches = m.drawn()
        self.assertEqual([batch[:3] for batch in batches],
                         [(0xB0, 10, 16), (0xB0, 10, 16), (0xB0, 10, 12), (0xA8, 12, 6), (0xA8, 12, 4)] * 2)
        rounded = lambda v: (round(v[0], 2), round(v[1], 2), v[3])
        right, left = batches[5:], batches[:5]
        gap = math.degrees(math.atan2(23, 76))
        ring = lambda degrees: (round(92.0657 + 81.5 * math.cos(math.radians(degrees)), 2),
                                round(-13 + 81.5 * math.sin(math.radians(degrees)), 2), 0x30C8FFFF)
        # Out and up, out and down, and in under the stage to 30 degrees short of level.
        self.assertEqual([rounded(right[i][3][j]) for i in range(3) for j in (0, -1)],
                         [ring(a) for a in (90 - gap, gap, -gap, gap - 90, -90 - gap, -150)])
        self.assertEqual(rounded(right[0][3][0]), (115.67, 65.01, 0x30C8FFFF))
        self.assertEqual(rounded(right[2][3][-1]), (21.48, -53.75, 0x30C8FFFF))
        self.assertEqual(rounded(left[0][3][0]), (-115.67, 65.01, 0x30C8FFFF))
        # A tick across the ring above, level with and below the X: 6 each side of 81.5.
        self.assertEqual([rounded(v)[:2] for v in right[3][3]],
                         [(92.07, 62.5), (92.07, 74.5), (167.57, -13.0), (179.57, -13.0), (92.07, -88.5), (92.07, -100.5)])
        self.assertEqual([rounded(v)[:2] for v in left[3][3]][2:4], [(-167.57, -13.0), (-179.57, -13.0)])
        self.assertEqual([rounded(v) for v in right[4][3]],
                         [(89.57, -15.5, 0xFFFFFFFF), (94.57, -10.5, 0xFFFFFFFF),
                          (89.57, -10.5, 0xFFFFFFFF), (94.57, -15.5, 0xFFFFFFFF)])
        self.assertEqual(rounded(left[4][3][0]), (-89.57, -15.5, 0xFFFFFFFF))

    def test_fox_b0xx_on_final_destination_by_hand(self):
        # A dot 81.5 from the X (92.07, -13) for each of the B0XX's angles.
        m = self.m
        m.add_player(3, CODES['fox-b0xx'].kind)
        m.stage, m.ledges = 32, ((-85.5657, 0.0), (85.5657, 0.0))
        m.draw('fox-b0xx')
        batches = m.drawn()
        self.assertEqual([batch[:3] for batch in batches], [(0xB8, 30, 28), (0xB8, 30, 6), (0xA8, 12, 4)] * 2)
        rounded = lambda v: (round(v[0], 2), round(v[1], 2), v[3])
        blue, white = batches[3][3], batches[4][3]
        # Mod X with B up toward the stage, (73, 31): 23 degrees, so from below and out.
        self.assertEqual(rounded(blue[0]), (167.08, -44.86, 0x30C8FFFF))
        # No modifier: straight across, then up and down, then the diagonals, the
        # one up and away from the stage from in under it.
        self.assertEqual([rounded(v)[:2] for v in white],
                         [(173.57, -13.0), (92.07, -94.5), (92.07, 68.5), (149.69, -70.63), (149.69, 44.63),
                          (34.44, -70.63)])
        self.assertEqual(rounded(batches[1][3][0])[:2], (-173.57, -13.0))
        # Every dot is on the ring.
        for x, y, _, _ in blue + white:
            self.assertAlmostEqual(math.hypot(x - 92.0657, y + 13), 81.5, places=3)

    def test_mario_dots_by_hand(self):
        # Target 7.08 out, 15 down; Super Jump Punch peaks 40.4 up and 17.6 forward.
        m = self.m
        m.add_player(0, CODES['mario'].kind)
        m.stage, m.ledges = 31, ((-68.4, 0.0), (68.4, 0.0))
        m.draw('mario')
        batches = m.drawn()
        self.assertEqual([batch[:3] for batch in batches][:5],
                         [(0xA8, 12, 2), (0xB8, 42, 1), (0xA8, 12, 2), (0xB8, 42, 1), (0xA8, 12, 4)])
        dot = batches[8][3][0]  # the right ledge's blue dot: 68.4 + 7.075 + 17.6
        self.assertAlmostEqual(dot[0], 93.075, places=3)
        self.assertAlmostEqual(dot[1], -55.4, places=3)
        self.assertEqual(dot[3], 0x30C8FFFF)

    def test_only_while_the_character_is_in_the_match(self):
        m = self.m
        m.stage = 31
        m.draw('falco')
        self.assertEqual(m.fifo, [])
        self.assertEqual([args[0] for args in m.named('Player_GetEntity')], list(range(6)))
        self.assertEqual(m.named('Stage_GetExternalID'), [])
        m.add_player(0, CODES['fox'].kind)
        m.add_player(2, CODES['falco'].kind, userdata=False)  # still being set up
        m.draw('falco')
        self.assertEqual(m.fifo, [])
        m.add_player(5, CODES['falco'].kind)
        m.draw('falco')
        self.assertDrawn(m.drawn(), expected('falco', *m.ledges))

    def test_other_stages_and_render_passes_draw_nothing(self):
        m = self.m
        for slug, character in CODES.items():
            m.players = {}
            m.add_player(0, character.kind)
            for stage in (0, 1, 4, 22, 24, 33, 255):
                with self.subTest(slug=slug, stage=stage):
                    m.stage = stage
                    m.draw(slug)
                    self.assertEqual(m.fifo, [])
                    self.assertEqual(m.named('Stage_GetLeftOfLineCoordinates'), [])
            m.stage = 32
            for pass_number in (0, 1, 3):
                with self.subTest(slug=slug, pass_number=pass_number):
                    m.draw(slug, pass_number)
                    self.assertEqual(m.calls, [])


@unittest.skipUnless((ROOT / "melee.iso").exists(), "needs a vanilla melee.iso at the repo root")
class IsoTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.iso = open(ROOT / "melee.iso", "rb")

    @classmethod
    def tearDownClass(cls):
        cls.iso.close()

    def test_the_asm_files_match_the_iso(self):
        stick = numbers.stick(*numbers.deadzones(self.iso))
        for code in numbers.CODES:
            with self.subTest(code=code.key):
                n = numbers.code_numbers(self.iso, code, stick)
                self.assertEqual(numbers.asm_path(code).read_text(), numbers.asm(code, n))

    def test_the_stick_is_vanillas(self):
        # PlCo.dat's 0.28 deadzones: an axis reads at least 23/80 once it isn't 0.
        self.assertEqual(numbers.deadzones(self.iso), (f32(.28), f32(.28)))
        stick = numbers.stick(*numbers.deadzones(self.iso))
        self.assertEqual(stick['least'], (23, 23))
        self.assertAlmostEqual(stick['gaps'][0], 16.837, places=3)

    def test_each_hook_replaces_the_vanilla_instruction(self):
        self.iso.seek(0x420)
        dol_at = struct.unpack('>I', self.iso.read(4))[0]
        self.iso.seek(dol_at)
        header = self.iso.read(0x100)
        sections = list(zip(struct.unpack_from('>18I', header, 0), struct.unpack_from('>18I', header, 0x48),
                            struct.unpack_from('>18I', header, 0x90)))
        for slug in CODES:
            hook, words = code(slug)
            offset, address, _ = next(s for s in sections if s[1] <= hook < s[1] + s[2])
            self.iso.seek(dol_at + offset + hook - address)
            with self.subTest(slug=slug):
                self.assertEqual(struct.unpack('>I', self.iso.read(4))[0], replaced(words))

    def test_ledges_match_the_iso(self):
        spec = importlib.util.spec_from_file_location('platforms', ROOT / 'scripts/sweetspot_dj_platforms.py')
        platforms = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(platforms)
        for dat, _, stage, left_line, right_line in platforms.STAGES:
            (lx, ly), (rx, ry) = platforms.ledges(platforms.read_file(self.iso, dat), left_line, right_line)
            lines, (x, y) = STAGES[stage]
            self.assertEqual((left_line, right_line), lines)
            self.assertAlmostEqual(rx, x, places=2)
            self.assertAlmostEqual(-lx, x, places=2)
            self.assertAlmostEqual(ry, y, places=2)


if __name__ == "__main__":
    unittest.main()
