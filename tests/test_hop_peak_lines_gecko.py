"""Execute the Fox and Falco Hop Peak Lines Gecko codes in a PowerPC emulator.

python tests/test_hop_peak_lines_gecko.py -v
Requires Unicorn (the existing build/test-deps install is supported). The
native functions the codes call are stubbed, the stage's collision lines are
laid out in memory where r13 points the game's, and the GX FIFO writes are
recorded, so this checks what the codes ask the GPU to draw, not the picture.
"""
import importlib.util
from pathlib import Path
import re
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "build/test-deps"))
from unicorn import (Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE,
                     UC_HOOK_MEM_WRITE)
from unicorn import ppc_const as reg

_spec = importlib.util.spec_from_file_location('hop_peak_lines', ROOT / 'scripts/hop_peak_lines.py')
numbers = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(numbers)

FOLDER = numbers.FOLDER
CODES = {name: (kind, hook, original) for name, kind, hook, original in numbers.CODES}
TEXT = {'Fox': 'fox-hop-peak-lines.txt', 'Falco': 'falco-hop-peak-lines.txt'}
STOP, RETURN, R13, FIFO, SCRATCH = 0x80004000, 0x80004100, 0x804DB6A0, 0xCC008000, 0x80F00000
LINES_AT, DESCS_AT, VERTS_AT = 0x80B00000, 0x80B10000, 0x80B20000
GX_LINES, WIDTH = 0xA8, 12
ORANGE, TEAL = 0xE68724E6, 0x00A290E6  # Premultiplied RGBA: Target Practice's default Orange and Teal.
SUNK = 1.0
# External stage ID: line count, the platform lines' live ends (as the game
# starts them), and the main floor at x = 0. From notes/LEGAL-STAGE-PLATFORMS.md;
# Fountain's side platforms where they start (16.125 and 22.125) under its fixed
# top (42.75). Some lines run right to left, as a DAT may store them.
STAGES = {
    2: (8, {0: ((-49.5, 16.125), (-21.0, 16.125)), 1: ((49.5, 22.125), (21.0, 22.125)),
            2: ((-14.25, 42.75), (14.25, 42.75))}, 0.002775),
    3: (66, {35: ((-55.0, 25.0), (-25.0, 25.0)), 36: ((25.0, 25.0), (55.0, 25.0))}, 0.0),
    8: (7, {1: ((-59.5, 23.45), (-28.0, 23.45)), 4: ((15.75, 42.0), (-15.75, 42.0)),
            5: ((28.0, 23.45), (59.5, 23.45))}, 0.0),
    28: (6, {0: ((-61.3929, 30.1421), (-31.7254, 30.1421)), 1: ((31.7036, 30.2425), (63.0745, 30.2425)),
             2: ((-19.0181, 51.4253), (19.0171, 51.4253))}, 0.0088),
    31: (6, {2: ((-57.6, 27.2), (-20.0, 27.2)), 3: ((-18.8, 54.4), (18.8, 54.4)),
             4: ((20.0, 27.2), (57.6, 27.2))}, 0.0),
    32: (3, {}, 0.0),
}
BLAST = {2: (-198.75, 198.75), 3: (-230.0, 230.0), 8: (-175.7, 173.6), 28: (-255.0, 255.0),
         31: (-224.0, 224.0), 32: (-246.0, 246.0)}
NATIVES = {
    0x803901F0: 'GObj_Create', 0x8039069C: 'GObj_AddGXLink', 0x80034110: 'Player_GetEntity',
    0x80225194: 'Stage_GetExternalID', 0x80224B50: 'Stage_GetBlastzoneLeft',
    0x80224B38: 'Stage_GetBlastzoneRight', 0x80054ED8: 'GrColl_CheckIfLineEnabled',
    0x8036C244: 'HSD_ClearVtxDesc', 0x80341510: 'GXSetCurrentMtx', 0x8036A288: 'COBJ_GetCurrent',
    0x803695F0: 'COBJ_GetViewingMtx', 0x80341494: 'GXLoadPosMtxImm',
    0x80361A74: 'HSD_StateSetLineWidth', 0x8036199C: 'HSD_SetupRenderMode',
    0x80361BB8: 'HSD_StateSetZMode', 0x8033C414: 'GXSetVtxAttrFmt', 0x8033BF00: 'GXSetVtxDesc',
    0x80361AD8: 'HSD_StateSetCullMode', 0x8033D0DC: 'GXBegin',
}
# The map's names for the natives the codes call that no other code here does.
MAP_NAMES = {0x80224B50: 'StageInfo_BlastzoneLeftLoad_AddtoXAxisOffset',
             0x80224B38: 'StageInfo_BlastzoneRightLoad_AddtoXAxisOffset',
             0x80054ED8: 'Line_CheckIfEnabled(r3=LineID)'}


def gpr(n):
    return getattr(reg, f"UC_PPC_REG_{n}")


def f32(value):
    return struct.unpack(">f", struct.pack(">f", value))[0]


def double_bits(value):
    return struct.unpack(">Q", struct.pack(">d", value))[0]


def code(name):
    """The C2 code's address and instructions, from the distributed text."""
    words = []
    for line in (FOLDER / TEXT[name]).read_text().splitlines():
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


def heights(name):
    """character_data's (short hop, full hop) peak lasers, from the code's .asm."""
    text = numbers.asm_path(name).read_text()
    return tuple(f32(float(v)) for v in re.findall(r'^\s*\.float ([-\d.]+)', text, re.M)[:2])


def stage_table(name):
    """stage_data's {external ID: ([platform lines], main floor y)}, from the code's .asm."""
    table = {}
    for match in re.finditer(r'^\s*stage (\d+), (0x\w+), (0x\w+), (0x\w+), ([-\d.]+)', numbers.asm_path(name).read_text(), re.M):
        lines = [int(v, 16) for v in match.groups()[1:4] if int(v, 16) != numbers.NO_LINE]
        table[int(match[1])] = (lines, f32(float(match[5])))
    return table


class Machine:
    def __init__(self, names=tuple(CODES)):
        self.cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
        self.cpu.mem_map(0x80000000, 0x1800000)
        self.cpu.mem_map(0xCC000000, 0x10000)
        self.cpu.reg_write(reg.UC_PPC_REG_MSR, 1 << 13)  # Floating point on.
        self.cpu.reg_write(gpr(1), 0x817F0000)
        self.cpu.reg_write(gpr(13), R13)
        self.allocations, self.gobjs, self.originals = {}, {}, {}
        for i, name in enumerate(names):
            hook, words = code(name)
            assert hook == CODES[name][1]
            self.originals[name] = replaced(words)
            # Install like the code handler: branch in, and the final word back out.
            allocation = 0x81700000 + i * 0x1000
            end = allocation + 4 * (len(words) - 1)
            words[-1] = 0x48000000 | ((hook + 4 - end) & 0x3FFFFFC)
            self.cpu.mem_write(allocation, struct.pack(f">{len(words)}I", *words))
            self.w(hook, 0x48000000 | ((allocation - hook) & 0x3FFFFFC))
            self.allocations[name], self.gobjs[name] = allocation, 0x80E00000 + i * 0x100
        self.w(R13 - 0x51E4, LINES_AT)
        self.w(R13 - 0x51E8, VERTS_AT)
        self.created, self.players, self.enabled = None, {}, set()
        self.calls, self.fifo, self.callbacks = [], [], {}
        self.set_stage(32)
        self.cpu.hook_add(UC_HOOK_CODE, self.native)
        self.cpu.hook_add(UC_HOOK_MEM_WRITE, self.write, begin=FIFO, end=FIFO + 3)

    def w(self, address, value, kind="I"):
        self.cpu.mem_write(address, struct.pack(">" + kind, value))

    def set_stage(self, stage, platforms=None):
        """A stage's live lines: STAGES' platforms, or `platforms`, all enabled."""
        self.stage = stage
        count, lines, _ = STAGES.get(stage, (8, {}, 0))
        self.line_count, self.blast = count, BLAST.get(stage, (-200.0, 200.0))
        for line in range(count):
            self.w(LINES_AT + line * 8, 0)
        self.lines = {}
        for line, ends in (lines if platforms is None else platforms).items():
            self.move(line, *ends)
        self.enabled = set(self.lines)

    def move(self, line, a, b):
        """Put a line's ends where they are now (CollVert.pos_curr), as a moving platform does."""
        desc = DESCS_AT + line * 0x10
        self.w(LINES_AT + line * 8, desc)
        self.cpu.mem_write(desc, struct.pack(">hh", 2 * line, 2 * line + 1))
        for vertex, (x, y) in ((2 * line, a), (2 * line + 1, b)):
            self.cpu.mem_write(VERTS_AT + vertex * 0x18 + 8, struct.pack(">2f", x, y))
        self.lines[line] = (a, b)

    def native(self, cpu, address, size, _):
        name = NATIVES.get(address)
        if not name:
            return
        args = tuple(cpu.reg_read(gpr(n)) for n in range(3, 8))
        self.calls.append((name, args))
        # Clobber the volatile registers, cr0 and the volatile floats the way a real call may.
        for n in (0, 4, 5, 6, 7, 8, 9, 10, 11, 12):
            cpu.reg_write(gpr(n), 0xDEAD0000 | n)
        for n in range(14):
            cpu.reg_write(getattr(reg, f"UC_PPC_REG_FPR{n}"), 0x7FF8DEAD00000000 | n)
        cpu.reg_write(reg.UC_PPC_REG_CR0, 0)
        if name == 'GObj_Create':
            cpu.reg_write(gpr(3), self.created)
        elif name == 'Player_GetEntity':
            assert args[0] < 6, 'slot in range'
            cpu.reg_write(gpr(3), self.players.get(args[0], (0, 0))[0])
        elif name == 'Stage_GetExternalID':
            cpu.reg_write(gpr(3), self.stage)
        elif name.startswith('Stage_GetBlastzone'):
            # A single, as the game's getters compute it.
            cpu.reg_write(reg.UC_PPC_REG_FPR1, double_bits(f32(self.blast[name == 'Stage_GetBlastzoneRight'])))
        elif name == 'GrColl_CheckIfLineEnabled':
            # The game's own hangs on a line the stage doesn't have.
            assert 0 <= args[0] < self.line_count, f'line {args[0]} on stage {self.stage}'
            cpu.reg_write(gpr(3), int(args[0] in self.enabled))
        elif name == 'COBJ_GetCurrent':
            cpu.reg_write(gpr(3), 0x80D00000)
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
        """Distinct registers, and the link register a blr goes back to."""
        for n in range(32):
            if n not in (1, 2, 13):
                self.cpu.reg_write(gpr(n), 0x80100000 + n * 0x100)
        self.cpu.reg_write(gpr(1), 0x817F0000)
        self.cpu.reg_write(reg.UC_PPC_REG_LR, RETURN)
        self.cpu.reg_write(reg.UC_PPC_REG_CR, 0x24680000)

    def start_match(self, name):
        """Run StartMelee's hooked instruction. Returns the registers its
        original instruction alone leaves, and where it goes next."""
        self.prime()
        # Each code its own scratch, so no stale translation of another's runs.
        scratch = SCRATCH + 0x100 * list(CODES).index(name)
        self.w(scratch, self.originals[name])
        self.w(scratch + 4, 0x48000000 | ((RETURN - scratch - 4) & 0x3FFFFFC))  # b RETURN
        self.cpu.emu_start(scratch, RETURN)
        wanted = self.registers()
        self.created = self.gobjs[name] if self.created != 0 else 0
        self.prime()
        hook = CODES[name][1]
        self.calls = []
        # Fall-through hooks come back to the next instruction; a blr goes to LR.
        self.w(hook + 4, 0x48000000 | ((RETURN - hook - 4) & 0x3FFFFFC))
        self.cpu.emu_start(hook, RETURN)
        links = self.named('GObj_AddGXLink')
        if links:
            self.callbacks[name] = links[0][1]
        return wanted

    def draw(self, name, pass_number=2):
        saved = {n: 0x81000000 + n for n in range(14, 32)}
        for n, value in saved.items():
            self.cpu.reg_write(gpr(n), value)
        sp = self.cpu.reg_read(gpr(1))
        self.calls, self.fifo = [], []
        self.cpu.reg_write(gpr(3), self.gobjs[name])
        self.cpu.reg_write(gpr(4), pass_number)
        self.cpu.reg_write(reg.UC_PPC_REG_LR, STOP)
        self.cpu.emu_start(self.callbacks[name], STOP)
        assert self.cpu.reg_read(gpr(1)) == sp, 'stack restored'
        for n, value in saved.items():
            assert self.cpu.reg_read(gpr(n)) == value, f'r{n} preserved'

    def drawn(self):
        """The one GXBegin batch's (shape, width, count) and its lines, each
        (left x, right x, y, color), or None."""
        begins, widths = self.named('GXBegin'), self.named('HSD_StateSetLineWidth')
        if not begins:
            assert self.fifo == []
            return None
        assert len(begins) == len(widths) == 1
        assert all(size == 4 for size, _ in self.fifo)
        words = [value for _, value in self.fifo]
        vertices = [(*struct.unpack(">3f", struct.pack(">3I", *words[i:i + 3])), words[i + 3])
                    for i in range(0, len(words), 4)]
        assert len(vertices) == begins[0][2], 'every vertex in the batch'
        lines = []
        for a, b in zip(vertices[::2], vertices[1::2]):
            assert (a[1], a[3]) == (b[1], b[3]), (a, b)  # Level, one color.
            assert a[2] == b[2] == 0.0
            lines.append((a[0], b[0], a[1], a[3]))
        return (begins[0][0], widths[0][0], begins[0][2]), lines


def expected(name, stage, lines, enabled, blast):
    """The main floor's two lines across, then each enabled platform's, in the
    code's stage order, on its side of the middle, unless it's sunk."""
    short, full = heights(name)
    platforms, main = stage_table(name)[stage]
    floors = [(blast[0], blast[1], main)]
    for line in platforms:
        if line not in enabled or line not in lines:
            continue
        (ax, ay), (bx, by) = lines[line]
        y = f32(f32(f32(ay) + f32(by)) * 0.5)
        if y < f32(main + SUNK):
            continue
        left = blast[0] if not (ax > 0 and bx > 0) else 0.0
        right = blast[1] if not (ax < 0 and bx < 0) else 0.0
        floors.append((left, right, y))
    floors = [(f32(left), f32(right), y) for left, right, y in floors]
    return [(left, right, f32(f32(y) + height), color)
            for left, right, y in floors for height, color in ((short, ORANGE), (full, TEAL))]


class HookTests(unittest.TestCase):
    def test_hooks_are_free_and_inside_start_melee(self):
        hooks = {hook for _, hook, _ in CODES.values()}
        self.assertEqual(len(hooks), 2)
        # Every other code in this folder's parent, Slippi's own, and Training Mode's
        # Better VS Mode Frame Counter (8016E744) in ASM/.
        taken = {0x8016E748, 0x8016E74C, 0x8016E904, 0x8016E744}
        for asm in (ROOT / 'slippi-gecko-codes').rglob('*.asm'):
            if asm.parent != FOLDER:
                match = re.search(r'inserted at ([0-9a-fA-F]{8})', asm.read_text())
                taken.add(int(match[1], 16))
        self.assertGreater(len(taken), 30)
        self.assertFalse(taken & hooks)
        self.assertTrue(all(0x8016E730 <= hook < 0x8016E934 for hook in hooks), 'inside StartMelee')

    def test_natives_are_the_maps(self):
        names = {}
        for line in (ROOT / 'GTME01.map').read_text(errors='replace').splitlines():
            parts = line.split(maxsplit=4)
            if len(parts) == 5 and len(parts[0]) == 8:
                names[int(parts[0], 16)] = parts[4]
        for address, name in MAP_NAMES.items():
            self.assertEqual(names.get(address), name)

    def test_each_code_creates_its_overlay_then_acts_like_the_replaced_instruction(self):
        m = Machine()
        for name in CODES:
            with self.subTest(code=name):
                wanted = m.start_match(name)
                self.assertEqual([args[:3] for args in m.named('GObj_Create')], [(0, 7, 0)])
                (gobj, callback, link, priority, _), = m.named('GObj_AddGXLink')
                self.assertEqual((gobj, link, priority), (m.gobjs[name], 5, 0))
                self.assertTrue(m.allocations[name] <= callback < m.allocations[name] + 0x1000)
                self.assertEqual(m.registers(), wanted)  # r0, LR and CR included.
        self.assertEqual(wanted[31], 0x80100000 + 31 * 0x100)  # Falco's blr leaves r31.
        self.assertNotEqual(m.callbacks['Fox'], m.callbacks['Falco'])

    def test_fox_still_copies_r3_and_falco_still_returns(self):
        self.assertEqual(Machine().originals, {'Fox': 0x7C7F1B78, 'Falco': 0x4E800020})  # mr r31,r3 / blr
        m = Machine(('Fox',))
        wanted = m.start_match('Fox')
        self.assertEqual(wanted[31], 0x80100000 + 3 * 0x100)
        self.assertEqual(m.registers()[31], wanted[31])

    def test_failed_gobj_creation_skips_the_draw_link(self):
        for name in CODES:
            with self.subTest(code=name):
                m = Machine((name,))
                m.created = 0
                wanted = m.start_match(name)
                self.assertEqual(m.named('GObj_AddGXLink'), [])
                self.assertEqual(m.registers(), wanted)


class DrawTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.m = Machine()
        for name in CODES:
            cls.m.start_match(name)

    def setUp(self):
        self.m.players = {}
        self.m.set_stage(32)

    def assertLines(self, actual, wanted):
        self.assertIsNotNone(actual)
        (shape, width, count), lines = actual
        self.assertEqual((shape, width, count), (GX_LINES, WIDTH, 2 * len(wanted)))
        self.assertEqual(lines, wanted)

    def test_matches_target_practice(self):
        source = (ROOT / 'src/targets.c').read_text()
        self.assertIn('#define SUNK_PLATFORM 1.f', source)
        self.assertIn('#define PEAK_LINE_WIDTH 12', source)
        # The codes draw one color a hop: the event's defaults, Orange and Teal, as it draws them
        # over the side platforms (its base shade), at its alpha, premultiplied.
        self.assertIn('#define PEAK_ALPHA 230', source)
        # The names start with Lowest Laser Lines' Off, then the colors LineColors lists.
        names = re.findall(r'"([^"]+)"', re.search(r'LineColorText\[TM_TARGETS_LOW_LINE_COUNT\] = \{(.*?)\};', source, re.S)[1])
        self.assertEqual(names[0], 'Off')
        names = names[1:]
        rgbs = re.findall(r'\{(\d+), (\d+), (\d+)\}',
                          re.search(r'LineColors\[TM_TARGETS_COLOR_COUNT\]\[3\] = \{(.*?)\n\};', source, re.S)[1])
        premultiplied = lambda name: int.from_bytes(bytes([int(v) * 230 // 255 for v in rgbs[names.index(name)]] + [230]), 'big')
        self.assertEqual((ORANGE, TEAL), (premultiplied('Orange'), premultiplied('Teal')))
        settings = (ROOT / 'src/settings.h').read_text()
        self.assertIn('#define TM_TARGETS_SH_COLOR_DEFAULT (TM_COLOR_ORANGE - TM_COLOR_WHITE)', settings)
        self.assertIn('#define TM_TARGETS_FH_COLOR_DEFAULT (TM_COLOR_TEAL - TM_COLOR_WHITE)', settings)
        # Both codes list the same stages, the six legal ones.
        self.assertEqual(stage_table('Fox'), stage_table('Falco'))
        self.assertEqual(set(stage_table('Fox')), set(STAGES))
        for stage, (lines, main) in stage_table('Fox').items():
            self.assertEqual(set(lines), set(STAGES[stage][1]), stage)
            self.assertAlmostEqual(main, STAGES[stage][2], places=5)

    def test_every_legal_stage_draws_each_floors_two_lines(self):
        m = self.m
        counts = {2: 4, 3: 3, 8: 4, 28: 4, 31: 4, 32: 1}
        for name, (kind, _, _) in CODES.items():
            m.players = {}
            m.add_player(1, kind)
            for stage in STAGES:
                with self.subTest(code=name, stage=stage):
                    m.set_stage(stage)
                    m.draw(name)
                    self.assertEqual(sorted(args[0] for args in m.named('GrColl_CheckIfLineEnabled')),
                                     sorted(STAGES[stage][1]))
                    self.assertTrue(all(args[:3] == (0, 7, 0) for args in m.named('HSD_StateSetZMode')))
                    wanted = expected(name, stage, m.lines, m.enabled, m.blast)
                    self.assertEqual(len(wanted), 2 * counts[stage])
                    self.assertLines(m.drawn(), wanted)

    def test_fox_on_battlefield_by_hand(self):
        # Fox's short hop rises 2.1 on its first frame (no gravity) and then 8.55, and his
        # full hop 3.68 and then 27.6: B on airborne frames 2 and 8 shoot from those peaks,
        # 10.65 and 31.28, with his aerial laser 9.55 over TopN: 20.20 and 40.83 over each floor.
        m = self.m
        m.add_player(3, CODES['Fox'][0])
        m.set_stage(31)
        m.draw('Fox')
        (_, _, count), lines = m.drawn()
        self.assertEqual(count, 16)
        got = [(round(l, 3), round(r, 3), round(y, 3), color) for l, r, y, color in lines]
        self.assertEqual(got, [
            (-224, 224, 20.197, ORANGE), (-224, 224, 40.827, TEAL),        # the main floor
            (-224, 0, 47.397, ORANGE), (-224, 0, 68.027, TEAL),            # the left platform, to the middle
            (-224, 224, 74.597, ORANGE), (-224, 224, 95.227, TEAL),        # the top one, across
            (0, 224, 47.397, ORANGE), (0, 224, 68.027, TEAL),              # the right one, from the middle
        ])

    def test_falco_on_final_destination_by_hand(self):
        # Falco's short hop peaks at 11.58 on airborne frame 12, too soon for any B but the first, which
        # takes away the first frame's skipped gravity: B on frame 2 shoots from 11.44. His full hop's
        # B on frame 14 shoots from its 51.5 peak. His laser is 9.32 up: 20.76 and 60.82.
        m = self.m
        m.add_player(0, CODES['Falco'][0])
        m.set_stage(32)
        m.draw('Falco')
        (_, _, count), lines = m.drawn()
        self.assertEqual(count, 4)
        self.assertEqual([(l, r, round(y, 3), c) for l, r, y, c in lines],
                         [(-246, 246, 20.76, ORANGE), (-246, 246, 60.82, TEAL)])

    def test_dream_lands_side_lines_read_apart(self):
        m = self.m
        m.add_player(0, CODES['Fox'][0])
        m.set_stage(28)
        m.draw('Fox')
        lines = m.drawn()[1]
        left, right = lines[2], lines[4]  # Lines 0 (left) and 1 (right), in the code's order.
        self.assertEqual((left[:2], right[:2]), ((-255, 0), (0, 255)))
        self.assertAlmostEqual(right[2] - left[2], 30.2425 - 30.1421, places=4)

    def test_fountains_lines_follow_its_side_platforms_and_go_while_one_is_sunk(self):
        m = self.m
        m.add_player(0, CODES['Fox'][0])
        m.set_stage(2)
        for left, right in ((12.375, 27.375), (27.375, 12.375), (19.875, 16.125), (1.5, 22.125)):
            with self.subTest(left=left, right=right):
                m.move(0, (-49.5, left), (-21.0, left))
                m.move(1, (49.5, right), (21.0, right))
                m.draw('Fox')
                wanted = expected('Fox', 2, m.lines, m.enabled, m.blast)
                self.assertEqual([w[2] for w in wanted[2:4]], [f32(left + h) for h in heights('Fox')])
                self.assertLines(m.drawn(), wanted)
        # Hidden, the game leaves a side platform 0.375 up, inside the stage: no lines.
        m.move(1, (49.5, 0.375), (21.0, 0.375))
        m.draw('Fox')
        self.assertLines(m.drawn(), expected('Fox', 2, m.lines, m.enabled, m.blast))
        self.assertEqual(len(m.drawn()[1]), 6)

    def test_a_platform_the_game_turns_off_has_no_lines(self):
        m = self.m
        m.add_player(0, CODES['Falco'][0])
        # Stadium's transformations turn its base platforms off.
        m.set_stage(3)
        m.enabled = set()
        m.draw('Falco')
        self.assertLines(m.drawn(), expected('Falco', 3, m.lines, m.enabled, m.blast))
        self.assertEqual(len(m.drawn()[1]), 2)
        # One at a time, on Battlefield.
        m.set_stage(31)
        m.enabled = {2, 4}
        m.draw('Falco')
        self.assertLines(m.drawn(), expected('Falco', 31, m.lines, m.enabled, m.blast))
        self.assertEqual(len(m.drawn()[1]), 6)

    def test_the_lines_reach_the_live_blast_zones(self):
        m = self.m
        m.add_player(0, CODES['Fox'][0])
        m.set_stage(8)
        m.blast = (-160.25, 190.5)
        m.draw('Fox')
        lines = m.drawn()[1]
        self.assertEqual({(l, r) for l, r, _, _ in lines}, {(-160.25, 190.5), (-160.25, 0), (0, 190.5)})

    def test_both_codes_draw_their_own_heights(self):
        m = self.m
        m.add_player(0, CODES['Fox'][0])
        m.add_player(1, CODES['Falco'][0])
        m.set_stage(31)
        for name in CODES:
            m.draw(name)
            self.assertLines(m.drawn(), expected(name, 31, m.lines, m.enabled, m.blast))
        self.assertNotEqual(heights('Fox'), heights('Falco'))

    def test_only_while_the_character_is_in_the_match(self):
        m = self.m
        m.set_stage(31)
        m.draw('Falco')
        self.assertIsNone(m.drawn())
        self.assertEqual([args[0] for args in m.named('Player_GetEntity')], list(range(6)))
        self.assertEqual(m.named('Stage_GetExternalID'), [])
        m.add_player(0, CODES['Fox'][0])
        m.add_player(2, CODES['Falco'][0], userdata=False)  # still being set up
        m.draw('Falco')
        self.assertIsNone(m.drawn())
        m.add_player(5, CODES['Falco'][0])
        m.draw('Falco')
        self.assertLines(m.drawn(), expected('Falco', 31, m.lines, m.enabled, m.blast))

    def test_other_stages_and_render_passes_draw_nothing(self):
        m = self.m
        for name, (kind, _, _) in CODES.items():
            m.players = {}
            m.add_player(0, kind)
            for stage in (0, 1, 4, 22, 24, 33, 255):
                with self.subTest(code=name, stage=stage):
                    m.set_stage(stage)
                    m.draw(name)
                    self.assertIsNone(m.drawn())
                    self.assertEqual(m.named('GrColl_CheckIfLineEnabled'), [])  # It hangs on a bad line.
            m.set_stage(31)
            for pass_number in (0, 1, 3):
                with self.subTest(code=name, pass_number=pass_number):
                    m.draw(name, pass_number)
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
        table = numbers.stage_numbers(self.iso)
        for c in numbers.CODES:
            with self.subTest(code=c[0]):
                self.assertEqual(numbers.asm_path(c[0]).read_text(), numbers.asm(c, numbers.peaks(self.iso, c[0]), table))

    def test_the_heights_are_target_practices(self):
        # The event's Blasters table and the jumps' highest lasers, worked out the same way.
        sys.path.insert(0, str(ROOT / 'scripts'))
        import laser_heights
        for name, by_hand in (('Fox', (20.197, 40.827)), ('Falco', (20.76, 60.82))):
            fighter = laser_heights.Fighter(self.iso, name)
            a, c = fighter.attr, fighter.constants()
            short, full = heights(name)
            self.assertAlmostEqual(short, laser_heights.peak_laser(a, c, a['hop_v'])[0], places=5)
            self.assertAlmostEqual(full, laser_heights.peak_laser(a, c, a['jump_v'])[0], places=5)
            self.assertEqual((round(short, 3), round(full, 3)), by_hand)

    def test_the_platform_lines_are_the_legal_stages(self):
        # The same lines Target Practice's FindPlatform picks from, by DAT ID.
        for stage_id, _, platforms, main in numbers.stage_numbers(self.iso):
            with self.subTest(stage=stage_id):
                self.assertEqual(set(platforms), set(STAGES[stage_id][1]))
                self.assertAlmostEqual(main, STAGES[stage_id][2], places=5)

    def test_each_hook_replaces_the_vanilla_instruction(self):
        self.iso.seek(0x420)
        dol_at = struct.unpack('>I', self.iso.read(4))[0]
        self.iso.seek(dol_at)
        header = self.iso.read(0x100)
        sections = list(zip(struct.unpack_from('>18I', header, 0), struct.unpack_from('>18I', header, 0x48),
                            struct.unpack_from('>18I', header, 0x90)))
        for name in CODES:
            hook, words = code(name)
            offset, address, _ = next(s for s in sections if s[1] <= hook < s[1] + s[2])
            self.iso.seek(dol_at + offset + hook - address)
            with self.subTest(code=name):
                self.assertEqual(struct.unpack('>I', self.iso.read(4))[0], replaced(words))


if __name__ == "__main__":
    unittest.main()
