"""Execute the production Target Practice event against the ISO's fighters and stages."""
from collections import Counter
from pathlib import Path
import re
import struct
import subprocess
import sys
import unittest
import test_settings as fixtures
from test_lowlaser import LEGAL, load_probe
from unicorn import UC_HOOK_MEM_WRITE

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import laser_heights  # noqa: E402

FOX, FALCO = 1, 22  # FTKIND_FOX, FTKIND_FALCO
NAMES = {FOX: 'Fox', FALCO: 'Falco'}
ITEM_FOXLASER, ITEM_FALCOLASER = 54, 55
OPT_FREEPRACTICE, OPT_SHDL, OPT_FULL_HOP, OPT_LINES, OPT_SH_COLOR, OPT_FH_COLOR = 3, 4, 5, 6, 7, 8
OPT_LOW_LINE, OPT_DOUBLE_JUMP, OPT_POINTERS, OPT_COUNTER, OPT_DEFAULTS = 9, 10, 11, 12, 13
FIFO, GX_LINES, GX_TRIANGLEFAN, GX_LINESTRIP = 0xCC008000, 0xA8, 0xA0, 0xB0
# A target: its four ticks (Target Pointers) in one batch, then the bullseye's four discs.
TARGET_BATCHES = [(GX_LINES, 16, 18, 1)] + [(GX_TRIANGLEFAN, 22, 0, 1)] * 4
# Hop Peak Lines' colors: the OSD palette's White to Lavender (osd_style.c and .h), each line
# its row's color, Orange and Teal by default, darker over the main floor, as picked over the
# side platforms and lighter over the top platform, at alpha 230 premultiplied.
OSD_STYLE = (ROOT / 'src/osd_style.c').read_text().split('OSD_PaletteColor', 1)[1].split('};', 1)[0]
LINE_RGB = [int(h, 16) >> 8 for h in re.findall(r'0x([0-9A-Fa-f]{8})', OSD_STYLE)][1:]
LINE_NAMES = re.findall(r'"([^"]+)"', (ROOT / 'src/osd_style.h').read_text().split('TM_OSD_COLOR_NAMES', 1)[1]
                        .split('#define', 1)[0])[1:]
ORANGE, TEAL = LINE_NAMES.index('Orange'), LINE_NAMES.index('Teal')
DARK, BASE, LIGHT = range(3)


def line_color(value, shade):
    """A Hop Peak Line's premultiplied color: 3/5 of each channel dark, halfway to white light."""
    rgb = [LINE_RGB[value] >> shift & 0xFF for shift in (16, 8, 0)]
    rgb = [v * 3 // 5 if shade == DARK else v + (255 - v) // 2 if shade == LIGHT else v for v in rgb]
    return tuple(v * 230 // 255 for v in rgb) + (230,)


# Double Jump Height: the red line, premultiplied, from blast zone to blast zone.
DJ_RED = (230, 30, 30, 230)
# Action states, as MexTK numbers them (Melee's own), and Fox's and Falco's
# specials after the common 341 (the decomp's ftFox_MotionState).
ASID_DEADDOWN, ASID_REBIRTHWAIT, ASID_JUMPF, ASID_JUMPB, ASID_FALL = 0, 13, 25, 26, 29
ASID_JUMPAERIALF, ASID_JUMPAERIALB = 27, 28
ASID_FALLSPECIAL, ASID_FALLSPECIALB, ASID_ATTACKAIRN, ASID_ESCAPEAIR = 35, 37, 65, 236
ASID_CLIFFCATCH, ASID_CLIFFWAIT, ASID_CLIFFESCAPEQUICK, ASID_CLIFFJUMPSLOW1 = 252, 253, 259, 260
FX_SPECIALAIRNLOOP, FX_SPECIALAIRNEND, FX_SPECIALSSTART, FX_SPECIALAIRS = 345, 346, 347, 351
FX_SPECIALHIHOLDAIR, FX_SPECIALAIRHI, FX_SPECIALHIFALL, FX_SPECIALHIBOUND, FX_SPECIALLWSTART = 354, 356, 358, 359, 360
FX_SPECIALAIRLWLOOP = 366  # The shine, which jumps out.
HEADN = 35  # MexTK's FtCommonBoneNames; RThumbNb, laser_heights.RTHUMBNB, is 51.
INSET, FROM_YOU, FROM_LAST = 8, 30, 25
MATCHGX_EFFECT = 7
SIZE_SCALE_AT = 0x804DC790  # The item hitbox command's size scale, about 1/256.
CKIND_FOX, CKIND_FALCO = 2, 20
GRKINDEXT_IZUMI, GRKINDEXT_CASTLE, GRKINDEXT_KONGO, GRKINDEXT_BATTLE = 2, 4, 5, 31
LEGAL_IDS = (32, 31, 28, 8, GRKINDEXT_IZUMI, 3)

# Fountain's platform lines are joint-local in its DAT, so they go where the game
# starts them (notes/LEGAL-STAGE-PLATFORMS.md): the fixed top at 42.75, the left
# side at 16.125 and the right at 22.125. Its main floor is the DAT's own.
FOUNTAIN = ('GrIz.dat', GRKINDEXT_IZUMI, None)
FOUNTAIN_PLATFORMS = [(0, -49.5, 16.125, -21, 16.125, 0x100), (1, 21, 22.125, 49.5, 22.125, 0x100),
                      (2, -14.25, 42.75, 14.25, 42.75, 0x100)]
STAGES = LEGAL + [FOUNTAIN]


def build():
    subprocess.run([
        'C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe', '-O2', '-w',
        '-ffreestanding', '-fno-builtin', '-msdata=none', '-mcpu=750', '-nostdlib',
        '-Wl,-Ttext=0x101000,-e,TestTargetsLoaded,--unresolved-symbols=ignore-all',
        'tests/targets_probe.c', '-o', 'build/targets-probe.elf'
    ], cwd=ROOT, check=True)


def dol_float(address):
    dol = (ROOT / 'build/Start.dol').read_bytes()
    for section in range(18):
        offset, base, size = [struct.unpack_from('>I', dol, at + section * 4)[0] for at in (0, 0x48, 0x90)]
        if base <= address < base + size:
            return struct.unpack_from('>f', dol, offset + address - base)[0]
    raise AssertionError('address not in the DOL')


def fighters():
    with open(ROOT / 'melee.iso', 'rb') as iso:
        return {kind: laser_heights.Fighter(iso, NAMES[kind]) for kind in NAMES}


def short_hop(attr, press=0, frames=64):
    """Height after each airborne frame of a short hop with B on airborne frame `press` (0 for
    none): the first frame skips gravity unless B comes on it, then gravity, then the move."""
    return laser_heights.hop(attr['hop_v'], attr['gravity'], attr['terminal'], press, frames)


# FH Multi Lasers as the menu lists it: Off, All Combos, then the combos, highest second
# laser first. FH_SAVED is each one's saved value (TMTargetsFullHop): High-Low took Peak-Low's
# 4, and High-Lowest's old 6 reads as High-Low.
FH_OFF, FH_ALL, FH_HI_HI_LOW, FH_HIGH_MEDHIGH, FH_HIGH_MEDIUM, FH_HIGH_LOW = range(6)
FH_COMBOS = range(FH_HI_HI_LOW, FH_HIGH_LOW + 1)
FH_TEXT = ('Off', 'All Combos', 'Hi-Hi-Low', 'High-MedHigh', 'High-Medium', 'High-Low')
FH_SAVED = {FH_OFF: 0, FH_ALL: 1, FH_HI_HI_LOW: 2, FH_HIGH_MEDHIGH: 3, FH_HIGH_MEDIUM: 5, FH_HIGH_LOW: 4}
FH_OLD_HIGH_LOWEST = 6
REACH = 1.5 + 300 / 256  # A target's radius plus a laser's hitbox.
HHL_SPREAD = 6  # Hi-Hi-Low's high targets, left and right of the low one.


def f32(value): return struct.unpack('>f', struct.pack('>f', value))[0]


def full_hop(attr, press=0, frames=96, fastfall=0):
    """Height after each airborne frame of a full hop with B on airborne frame `press` (0 for
    none), in the probe's float32 arithmetic on its attributes (each in ten-thousandths), with
    a fastfall on airborne frame `fastfall` (0 for none): it takes only if the fighter was
    already falling, and from then on the fall is the fastfall speed. None if it doesn't take.
    The first frame skips gravity (and the fastfall check) unless B comes on it."""
    jump, gravity, terminal, fast_v = (f32(round(attr[k] * 10000) / 10000)
                                       for k in ('jump_v', 'gravity', 'terminal', 'fastfall'))
    y, v, fast = [0.0], jump, False
    for frame in range(1, frames):
        if frame > 1 or press == 1:
            fast = fast or (frame == fastfall and v < 0)
            v = -fast_v if fast else max(f32(v - gravity), -terminal)
        y.append(f32(y[-1] + v))
    return y if fast or not fastfall else None


def jump_shots(y, c, press, count):
    """The heights of `count` looped shots from B on airborne frame `press`, or None if
    the fighter lands first or a shot would start under the floor."""
    heights = []
    for i in range(count):
        shot = press + c['delay'] + i * c['loop']
        if shot >= len(y) or y[shot - 1] + c['ecb_bottom'] <= 0 or y[shot - 1] + c['air'] <= 0:
            return None
        heights.append(y[shot - 1] + c['air'])
    return heights


def hop_shots(attr, c, press, count, fastfall=0):
    """jump_shots on the full hop that B on airborne frame `press` makes, or None."""
    y = full_hop(attr, press, fastfall=fastfall)
    return y and jump_shots(y, c, press, count)


def peak_press(attr, c):
    """The press whose first shot is a full hop's highest, the earliest of any that tie."""
    firsts = {p: hop_shots(attr, c, p, 1)[0] for p in range(1, 80) if hop_shots(attr, c, p, 1)}
    return max(firsts, key=lambda p: (firsts[p], -p))


def full_hop_combos(fighter):
    """Each combo's press frame and heights, by the doc's rules."""
    c, a = fighter.constants(), fighter.attr
    doubles = [p for p in range(1, 90) if hop_shots(a, c, p, 2)]
    assert doubles == list(range(1, len(doubles) + 1))  # A later press only lands sooner.
    second = lambda p: hop_shots(a, c, p, 2)[1]
    last, highest = doubles[-1], max(doubles, key=lambda p: (second(p), -p))
    peak = peak_press(a, c)  # The first shot comes from the frame before: the peak.
    near = lambda lo, hi, h: min(range(lo, hi + 1), key=lambda p: (abs(second(p) - h), p))
    # Hi-Hi-Low: of the presses that fit three shots (from the first), the one whose first two
    # come nearest the same height, the earliest of any within .01 of it.
    triples = [p for p in range(1, 90) if hop_shots(a, c, p, 3)]
    assert triples == list(range(1, len(triples) + 1))
    hi_hi_low, gap = None, None
    for p in triples:
        shots = hop_shots(a, c, p, 3)
        if hi_hi_low is None or abs(shots[0] - shots[1]) < gap - .01:
            hi_hi_low, gap = p, abs(shots[0] - shots[1])
    presses = {FH_HI_HI_LOW: (hi_hi_low, 3),
               FH_HIGH_MEDHIGH: (near(min(highest, peak), peak, (second(peak) + second(highest)) / 2), 2),
               FH_HIGH_MEDIUM: (near(peak, last, (second(peak) + second(last)) / 2), 2)}
    combos = {combo: (press, hop_shots(a, c, press, count), 0) for combo, (press, count) in presses.items()}
    # High-Low: of every press and fastfall whose first shot is from the peak, the lowest
    # second shot (the earliest fastfall, then the earliest press, of any that tie). B on the
    # first frame walks its own hop, any later B the other.
    top, high_low = hop_shots(a, c, peak, 1)[0], None
    for fastfall in range(1, 96):
        walks = {hop: full_hop(a, hop, fastfall=fastfall) for hop in (1, 2)}
        for press in range(1, 96 - c['delay'] - c['loop']):
            walk = walks[min(press, 2)]
            shots = walk and jump_shots(walk, c, press, 2)
            if shots and shots[0] >= top - .001 and (high_low is None or shots[1] < high_low[1][1]):
                high_low = (press, shots, fastfall)
    combos[FH_HIGH_LOW] = high_low
    return combos


def signed(value): return value - (1 << 32) if value >= 1 << 31 else value


class TargetsMachine:
    def __init__(self, kind, attr, free_practice=0, shdl=0, best=0, counter=0, seed=12345):
        self.m = fixtures.Machine()
        load_probe(self.m, 'targets-probe.elf', 'TestTargetsLoaded')
        self.m.call('TestTargetsFighter', kind, *[round(attr[k] * 10000) for k in
                                                  ('jump_v', 'hop_v', 'air_jump_mult', 'gravity', 'terminal',
                                                   'fastfall')])
        self.m.call('TestTargetsSeed', seed)
        self.lines = []

    def init(self, free_practice=0, shdl=0, best=0, counter=0, lines=0, full_hop=0, double_jump=0, colors=None,
             pointers=1, full_hop_saved=None, low_line=0):
        """`full_hop`: the FH Multi Lasers row saved, as the menu lists it, or `full_hop_saved`
        the value stored. `colors`: the saved (short hop, full hop) line colors, or None for the
        defaults. `pointers` (the saved Target Pointers) and `low_line` (Lowest Laser Lines: 0
        Off, else a line color plus one) are packed into the same argument."""
        packed = 0 if colors is None else (colors[0] + 1) | (colors[1] + 1) << 8
        packed |= (not pointers) << 16 | low_line << 17
        saved = FH_SAVED[full_hop] if full_hop_saved is None else full_hop_saved
        self.m.call('TestTargetsInit', free_practice, shdl, best, counter, lines, saved, double_jump, packed)

    def air(self, air=1, used=1, max_jumps=2, state=ASID_FALL, facing=1):
        self.m.call('TestTargetsAir', air, used, max_jumps, state, facing)

    def capture(self):
        """Record the GX FIFO: GFX_AddVtx writes x, y, z, then the color's bytes."""
        self.m.cpu.mem_map(FIFO, 0x1000)
        self.fifo = []
        self.m.cpu.hook_add(UC_HOOK_MEM_WRITE, lambda cpu, access, address, size, value, user:
                            self.fifo.append((size, value & 0xFFFFFFFF)), begin=FIFO, end=FIFO + 3)

    def draw(self, pass_=2):
        """The batches one GX pass starts: (shape, count, width, no depth), and the vertices,
        each (x, y, z, color)."""
        self.fifo.clear()
        count = self.m.call('TestTargetsDraw', pass_)
        batches = [tuple(self.m.call('TestTargetsDrawInfo', i, k) for k in range(4)) for i in range(count)]
        vertices = []
        for at in range(0, len(self.fifo), 7):
            sizes, values = zip(*self.fifo[at:at + 7])
            assert sizes == (4, 4, 4, 1, 1, 1, 1), sizes
            vertices.append((*struct.unpack('>3f', struct.pack('>3I', *values[:3])), tuple(values[3:])))
        return batches, vertices

    def stage(self, stage_id, floors, normal=None):
        self.m.call('TestTargetsStage', stage_id)
        self.lines = []
        for index, ax, ay, bx, by, flags in floors:
            on = normal is None or index in normal
            self.m.call('TestTargetsAddLine', *[round(v * 1000) for v in (ax, ay, bx, by)], flags, on)
            if on:
                a, b = sorted(((ax, ay), (bx, by)))
                self.lines.append((a[0], a[1], b[0], b[1], bool(flags & 0x100)))

    def flat(self, platforms=()):
        """A flat floor from -80 to 80, and one-way platforms (left, right, y)."""
        floors = [(0, -80, 0, 80, 0, 0)] + [(i + 1, l, y, r, y, 0x100) for i, (l, r, y) in enumerate(platforms)]
        self.stage(31, floors)

    def info(self, key): return signed(self.m.call('TestTargetsInfo', key))
    def you(self, x=0, y=0, control=1): self.m.call('TestTargetsYou', round(x * 1000), round(y * 1000), control)
    def think(self): self.m.call('TestTargetsThink')
    def targets(self):
        return [(signed(self.m.call('TestTargetsTarget', i, 1)) / 1000, signed(self.m.call('TestTargetsTarget', i, 2)) / 1000)
                for i in range(3) if self.m.call('TestTargetsTarget', i, 0)]
    def laser(self, slot, a, b, mine=1, kind=ITEM_FOXLASER, state=3, size=300 / 256):
        self.m.call('TestTargetsLaser', slot, mine | state << 1 | kind << 8,
                    *[round(v * 1000) for v in (*a, *b)], round(size * 1000))
    def shoot_all(self): self.m.call('TestTargetsShootAll')
    def place(self, x, y): self.m.call('TestTargetsPlace', round(x * 1000), round(y * 1000))
    def choose(self, row, value): self.m.call('TestTargetsChoose', row, value)
    def label(self):
        return bytes(self.m.cpu.mem_read(self.m.call('TestTargetsLabel'), 32)).split(b'\0', 1)[0].decode()

    def ground_y(self, x):
        return max(ay + (by - ay) * (x - ax) / (bx - ax) for ax, ay, bx, by, platform in self.lines
                   if not platform and ax <= x <= bx)


class TargetConstantTests(unittest.TestCase):
    """The heights come from Fox's and Falco's own skeletons and scripts."""
    @classmethod
    def setUpClass(cls):
        build()
        cls.fighters = fighters()

    def machine(self, kind, **kw):
        machine = TargetsMachine(kind, self.fighters[kind].attr)
        machine.flat()
        machine.init(**kw)
        return machine

    def test_blaster_table_matches_the_skeletons_and_scripts(self):
        m = self.machine(FOX)
        for kind, fighter in self.fighters.items():
            with self.subTest(fighter=NAMES[kind]):
                expected = fighter.constants()
                falco = kind == FALCO
                blaster = lambda key: m.m.call('TestTargetsBlaster', falco, key)
                for key, name in enumerate(('air', 'ground', 'ecb_bottom')):
                    self.assertAlmostEqual(blaster(key) / 10000, expected[name], delta=.0002)
                self.assertEqual((blaster(3), blaster(4)), (expected['delay'], expected['loop']))
        # SmashWiki's Fox: an aerial laser startup of 10 (the press is frame 1), a shot every 10.
        self.assertEqual((m.m.call('TestTargetsBlaster', 0, 3) + 1, m.m.call('TestTargetsBlaster', 0, 4)), (10, 10))

    def test_the_lowest_target_needs_a_low_laser(self):
        m = self.machine(FOX)
        scale = dol_float(SIZE_SCALE_AT)
        self.assertAlmostEqual(scale, 1 / 256, delta=1e-6)
        radius = m.m.call('TestTargetsBlaster', 0, 6) / 10000
        for kind, fighter in self.fighters.items():
            with self.subTest(fighter=NAMES[kind]):
                c = fighter.constants()
                lowest = m.m.call('TestTargetsBlaster', kind == FALCO, 5) / 10000
                self.assertAlmostEqual(lowest, max(c['air'] - c['ecb_bottom'], 0), delta=.0002)
                reach = radius + fighter.laser_hitbox() * scale
                # A standing laser passes over the lowest target; the lowest laser hits its center.
                self.assertGreater(c['ground'] - lowest, reach)
                self.assertGreater(c['ground'], lowest)
        self.assertGreater(self.fighters[FOX].constants()['air'], self.fighters[FOX].constants()['ecb_bottom'])
        self.assertLess(self.fighters[FALCO].constants()['air'], self.fighters[FALCO].constants()['ecb_bottom'])

    def test_only_fox_fits_two_lasers_in_a_short_hop(self):
        for kind, fighter in self.fighters.items():
            with self.subTest(fighter=NAMES[kind]):
                m = self.machine(kind, free_practice=1, shdl=1)
                m.you(); m.think()
                c = fighter.constants()
                hop = lambda press: short_hop(fighter.attr, press)
                fits = [press for press in range(1, 30) if press + c['delay'] + c['loop'] < 64 and
                        hop(press)[press + c['delay'] + c['loop'] - 1] + c['ecb_bottom'] > 0]
                self.assertEqual(m.info(12), len(fits))
                self.assertEqual(fits, [1, 2, 3] if kind == FOX else [])
                for press in range(1, 6):
                    first, y = press + c['delay'], hop(press)
                    self.assertEqual(m.m.call('TestTargetsShots', press, 0), int(press in fits))
                    if press in fits:
                        self.assertAlmostEqual(m.m.call('TestTargetsShots', press, 1) / 10000, y[first - 1] + c['air'], delta=.001)
                        self.assertAlmostEqual(m.m.call('TestTargetsShots', press, 2) / 10000,
                                               y[first + c['loop'] - 1] + c['air'], delta=.001)
        # A short hop's first frame skips gravity, so it rises 2.1 and then 8.55, unless B comes on
        # that frame. So B on frame 1 shoots from a lower hop's peak (17.83), B on frame 2 from the
        # higher one's (19.93), and B on frame 3 a frame past it, with the lowest second shot (5.08).
        c, attr = self.fighters[FOX].constants(), self.fighters[FOX].attr
        rise = laser_heights.rise(attr['hop_v'], attr['gravity'])
        self.assertAlmostEqual(short_hop(attr, 1)[9], rise, delta=.001)
        self.assertAlmostEqual(short_hop(attr, 2)[10], attr['hop_v'] + rise, delta=.001)
        m = self.machine(FOX)
        for press, by_hand in ((1, (17.83, 5.48)), (2, (19.93, 7.58)), (3, (19.73, 5.08))):
            with self.subTest(press=press):
                for key, height in zip((1, 2), by_hand):
                    self.assertAlmostEqual(m.m.call('TestTargetsShots', press, key) / 10000, height, delta=.01)
                self.assertGreaterEqual(m.m.call('TestTargetsShots', press, 2), m.m.call('TestTargetsBlaster', 0, 5))

    def test_full_hop_and_double_jump_from_live_attributes(self):
        for kind, fighter in self.fighters.items():
            with self.subTest(fighter=NAMES[kind]):
                a = fighter.attr
                expected = (a['jump_v'] + laser_heights.rise(a['jump_v'], a['gravity']) +
                            laser_heights.rise(a['jump_v'] * a['air_jump_mult'], a['gravity']))
                self.assertAlmostEqual(self.machine(kind).info(32) / 10000, expected, delta=.002)
                self.assertAlmostEqual(expected - laser_heights.rise(a['jump_v'] * a['air_jump_mult'], a['gravity']),
                                       max(full_hop(a)), delta=.001)
        # By hand: 3.68 + 27.6 + 40.204 for Fox, 4.1 + 47.4 + 41.778 for Falco.
        self.assertAlmostEqual(self.machine(FOX).info(32) / 10000, 71.484, delta=.002)
        self.assertAlmostEqual(self.machine(FALCO).info(32) / 10000, 93.278, delta=.002)


    def test_full_hop_combos_are_timed_from_live_attributes(self):
        # By hand: (press frame, heights above the floor) for each combo.
        # By hand: (press frame, heights above the floor, fastfall frame or 0) for each combo.
        by_hand = {
            FOX: {FH_HI_HI_LOW: (3, (37.11, 38.26, 17.02), 0), FH_HIGH_MEDHIGH: (6, (39.87, 34.12), 0),
                  FH_HIGH_MEDIUM: (13, (38.26, 17.02), 0), FH_HIGH_LOW: (9, (40.56, 6.56), 18)},
            FALCO: {FH_HI_HI_LOW: (6, (50.38, 49.34, 7.99), 0), FH_HIGH_MEDHIGH: (10, (54.20, 42.28), 0),
                    FH_HIGH_MEDIUM: (19, (52.85, 17.29), 0), FH_HIGH_LOW: (14, (55.30, 2.65), 27)},
        }
        for kind, fighter in self.fighters.items():
            expected = full_hop_combos(fighter)
            m = self.machine(kind, free_practice=1)
            m.you(); m.think()
            combo = lambda c, key: signed(m.m.call('TestTargetsFullHop', c, key))
            for c in FH_COMBOS:
                with self.subTest(fighter=NAMES[kind], combo=FH_TEXT[c]):
                    press, heights, fastfall = expected[c]
                    self.assertEqual((combo(c, 0), combo(c, 1), combo(c, 8)), (len(heights), press, fastfall))
                    hand = by_hand[kind][c]
                    self.assertEqual((press, len(heights), fastfall), (hand[0], len(hand[1]), hand[2]))
                    for i, (h, by) in enumerate(zip(heights, hand[1])):
                        self.assertAlmostEqual(combo(c, 2 + i) / 10000, h, delta=.001)
                        self.assertAlmostEqual(combo(c, 2 + i) / 10000, by, delta=.01)
            with self.subTest(fighter=NAMES[kind], combo='High-Low'):
                # Its first laser flies along the full hop's Hop Peak Line, and its second is no
                # lower than the lowest laser there is.
                self.assertAlmostEqual(combo(FH_HIGH_LOW, 2), m.m.call('TestTargetsPeak', 1), delta=20)
                self.assertGreaterEqual(combo(FH_HIGH_LOW, 3), m.m.call('TestTargetsBlaster', kind == FALCO, 5))
                # The fastfall is the first frame it can take: a frame sooner the fighter isn't
                # falling yet. A frame later, the second laser passes over the low target.
                c, (press, (high, low), fastfall) = fighter.constants(), expected[FH_HIGH_LOW]
                self.assertIsNone(full_hop(fighter.attr, press, fastfall=fastfall - 1))
                late = hop_shots(fighter.attr, c, press, 2, fastfall + 1)
                self.assertEqual(late[0], high)
                self.assertGreater(late[1] - low, REACH)
                self.assertAlmostEqual(late[1], {FOX: 9.73, FALCO: 5.83}[kind], delta=.01)
                # The doc's other timings that break both targets: (B, fastfall, heights).
                for p, f, hand in {FOX: ((10, 19, (40.33, 6.33)), (11, 20, (39.87, 5.87))),
                                   FALCO: ((15, 28, (55.15, 2.33)), (16, 29, (54.83, 1.84)))}[kind]:
                    shots = hop_shots(fighter.attr, c, p, 2, f)
                    for shot, by, target in zip(shots, hand, (high, low)):
                        self.assertAlmostEqual(shot, by, delta=.01)
                        self.assertLess(abs(shot - target), REACH)
            with self.subTest(fighter=NAMES[kind]):
                # Each double's second laser is out of reach of the next one's, so each is its own drill.
                seconds = [combo(c, 3) / 10000 for c in (FH_HIGH_MEDHIGH, FH_HIGH_MEDIUM, FH_HIGH_LOW)]
                for higher, lower in zip(seconds, seconds[1:]):
                    self.assertGreater(higher - lower, REACH)
            with self.subTest(fighter=NAMES[kind], combo='Hi-Hi-Low'):
                # Its two high lasers straddle the apex, each in reach of the other's target, and
                # those targets sit side by side, HHL_SPREAD either side of the low one.
                first, second = combo(FH_HI_HI_LOW, 2) / 10000, combo(FH_HI_HI_LOW, 3) / 10000
                self.assertLess(abs(first - second), REACH)
                self.assertEqual([combo(FH_HI_HI_LOW, 5 + i) / 10000 for i in range(3)], [-HHL_SPREAD, HHL_SPREAD, 0])
                for other in (c for c in FH_COMBOS if c != FH_HI_HI_LOW):
                    self.assertEqual([combo(other, 5 + i) for i in range(3)], [0, 0, 0])
        # Three shots fit from B on airborne frames 1 to 7 (Fox) and 1 to 8 (Falco). Fox's frames 3
        # and 4 tie, their first two shots 1.15 apart either way round: Hi-Hi-Low takes 3.
        for kind, presses in ((FOX, 7), (FALCO, 8)):
            c, a = self.fighters[kind].constants(), self.fighters[kind].attr
            self.assertEqual([p for p in range(1, 12) if hop_shots(a, c, p, 3)], list(range(1, presses + 1)))
        # B on the first frame keeps the hop's first frame's gravity, so its hop peaks a frame
        # sooner and its full speed lower: 27.6 against 31.28 for Fox, 47.4 against 51.5 for Falco.
        for kind, (first, later) in ((FOX, (27.6, 31.28)), (FALCO, (47.4, 51.5))):
            a = self.fighters[kind].attr
            self.assertAlmostEqual(max(full_hop(a, 1)), first, delta=.001)
            self.assertAlmostEqual(max(full_hop(a, 2)), later, delta=.001)


class TargetPlacementTests(unittest.TestCase):
    """Spawns on each legal stage's native collision lines."""
    @classmethod
    def setUpClass(cls):
        build()
        import test_techchase
        cls.fighters = fighters()
        cls.floors = {name: test_techchase.native_floors(name) for name, _, _ in LEGAL}
        cls.floors[FOUNTAIN[0]] = FOUNTAIN_PLATFORMS + [
            line for line in test_techchase.native_floors(FOUNTAIN[0]) if not line[-1] & 0x100]

    def run_targets(self, kind, filename, stage_id, normal, count, free_practice=0, shdl=0, you=(0, 0), full_hop=0):
        m = TargetsMachine(kind, self.fighters[kind].attr)
        m.stage(stage_id, self.floors[filename], normal)
        m.init(free_practice, shdl, full_hop=full_hop)
        m.you(*you)
        m.think()
        spawns = []
        for _ in range(count):
            spawns.append(m.targets())
            m.shoot_all()
        return m, spawns

    def test_singles_lie_between_a_low_laser_and_a_full_hop_double_jump_laser(self):
        for kind, fighter in self.fighters.items():
            c = fighter.constants()
            lowest = max(c['air'] - c['ecb_bottom'], 0)
            for filename, stage_id, normal in STAGES:
                with self.subTest(fighter=NAMES[kind], stage=filename):
                    m, spawns = self.run_targets(kind, filename, stage_id, normal, 80)
                    ground = [l for l in m.lines if not l[4]]
                    left, right = min(l[0] for l in ground), max(l[2] for l in ground)
                    top = max(max(l[1], l[3]) for l in m.lines) + m.info(32) / 10000 + c['air']
                    self.assertAlmostEqual(m.info(33) / 1000, max(max(l[1], l[3]) for l in m.lines), delta=.002)
                    fractions = []
                    for spawn in spawns:
                        self.assertEqual(len(spawn), 1)  # Timed runs: singles.
                        x, y = spawn[0]
                        self.assertGreaterEqual(x, left + INSET - .002)
                        self.assertLessEqual(x, right - INSET + .002)
                        floor = m.ground_y(x) + lowest
                        self.assertGreaterEqual(y, floor - .003)
                        self.assertLessEqual(y, top + .003)
                        fractions.append((y - floor) / (top - floor))
                    # Spread over the whole range, not bunched at one end.
                    self.assertLess(min(fractions), .2)
                    self.assertGreater(max(fractions), .8)

    def test_spawns_keep_away_from_you_and_from_the_last_target(self):
        m, spawns = self.run_targets(FOX, 'GrNBa.dat', 31, None, 80, you=(10, 0))
        last = None
        for spawn in spawns:
            x, y = spawn[0]
            self.assertGreaterEqual(((x - 10) ** 2 + y ** 2) ** .5, FROM_YOU - .01)
            if last:
                self.assertGreaterEqual(((x - last[0]) ** 2 + (y - last[1]) ** 2) ** .5, FROM_LAST - .01)
            last = (x, y)

    def pair_heights(self):
        """Fox's SH double laser pairs, (high, low) above the floor, for B on airborne frames 1
        to 3. B on frame 1 keeps the hop's first frame's gravity, so its hop is lower."""
        c, attr, pairs = self.fighters[FOX].constants(), self.fighters[FOX].attr, []
        for press in (1, 2, 3):
            y = short_hop(attr, press)
            pairs.append(tuple(y[press + c['delay'] + i * c['loop'] - 1] + c['air'] for i in (0, 1)))
        return pairs

    def pair_floor(self, m, spawn):
        """(press, floor) for a pair: B on frames 1 and 2 make the same shape 2.1 apart, so the
        floor under it tells them apart."""
        (ux, uy), (lx, ly) = spawn
        self.assertEqual(ux, lx)
        for press, (high, low) in enumerate(self.pair_heights(), 1):
            surface = ly - low
            floor = abs(uy - surface - high) < .003 and self.floor_kind(m, ux, surface, fail=False)
            if floor:
                return press, floor
        self.fail(f'no pair fits {spawn}')

    def floor_kind(self, m, x, surface, fail=True):
        """The floor a pair is over, at least INSET from its ends: main, left, right or top."""
        ground = [l for l in m.lines if not l[4]]
        left, right = min(l[0] for l in ground), max(l[2] for l in ground)
        if abs(surface - m.ground_y(x)) < .003 and left + INSET - .002 <= x <= right - INSET + .002:
            return 'main'
        for ax, ay, bx, by, platform in m.lines:
            if platform and ax + INSET - .002 <= x <= bx - INSET + .002 and \
                    abs(ay + (by - ay) * (x - ax) / (bx - ax) - surface) < .003:
                return 'top' if ax < 0 < bx else 'left' if bx < 0 else 'right'
        if fail:
            self.fail(f'no floor under the pair at ({x}, {surface})')

    def test_fox_pairs_come_off_each_kind_of_floor_the_stage_has_evenly(self):
        # Fountain's side platforms move and Stadium has no top platform; FD has none at all.
        offered = {'GrNLa.dat': {'main'}, 'GrPs.dat': {'main', 'left', 'right'}, 'GrIz.dat': {'main', 'top'}}
        # Far below every floor, where keeping away from you never turns a spot down, and at
        # stage center, where it turns down much of a narrow main floor (Yoshi's).
        for (filename, stage_id, normal), you in [(s, y) for s in STAGES for y in ((0, -300), (0, 0))]:
            with self.subTest(stage=filename, you=you):
                m, spawns = self.run_targets(FOX, filename, stage_id, normal, 240, free_practice=1, shdl=1, you=you)
                pairs = [s for s in spawns if len(s) == 2]
                # About half: a mix, not every spawn.
                self.assertGreater(len(pairs), 80)
                self.assertLess(len(pairs), 160)
                kinds, presses = Counter(), Counter()
                for pair in pairs:
                    press, floor = self.pair_floor(m, pair)
                    kinds[floor] += 1
                    presses[press] += 1
                self.assertEqual(set(presses), {1, 2, 3}, presses)  # Every press that fits.
                expected = offered.get(filename, {'main', 'left', 'right', 'top'})
                self.assertEqual(set(kinds), expected, kinds)
                # Main floor, side platforms and top platform each picked as often as the others.
                # A small platform can't take two pairs in a row (every spot on it is within
                # FROM_LAST of the last), so Fountain's top gets about a third, not half.
                groups = [{'main'}, {'left', 'right'}, {'top'}]
                groups = [g for g in groups if g & expected]
                for group in groups:
                    share = sum(kinds[k] for k in group) / len(pairs)
                    self.assertGreater(share, .5 / len(groups), kinds)
                    self.assertLess(share, 1.5 / len(groups), kinds)
                if 'left' in expected:
                    self.assertGreater(min(kinds['left'], kinds['right']), len(pairs) / len(groups) / 4, kinds)

    def test_pairs_keep_away_from_you_too(self):
        # On Battlefield's top platform, every spot over it is within FROM_YOU of you.
        m, spawns = self.run_targets(FOX, 'GrNBa.dat', GRKINDEXT_BATTLE, None, 120, free_practice=1, shdl=1,
                                     you=(0, 54.4))
        kinds = Counter()
        for spawn in spawns:
            for x, y in spawn:
                self.assertGreaterEqual((x ** 2 + (y - 54.4) ** 2) ** .5, FROM_YOU - .01)
            if len(spawn) == 2:
                kinds[self.pair_floor(m, spawn)[1]] += 1
        self.assertEqual(set(kinds), {'main', 'left', 'right'}, kinds)

    def group_kind(self, m, kind, spawn):
        """What a spawn is: 'single', 'sh' (a short hop pair) or a full hop combo, with the
        floor it's over. Every target of a group is at its heights over the floor, at one x
        but for Hi-Hi-Low's high two, side by side."""
        if len(spawn) == 1:
            return 'single', None
        shapes = [('sh', heights, (0, 0)) for heights in self.pair_heights()] if kind == FOX else []
        for c in FH_COMBOS:
            shots = m.m.call('TestTargetsFullHop', c, 0)
            shapes.append((c, [m.m.call('TestTargetsFullHop', c, 2 + i) / 10000 for i in range(shots)],
                           [signed(m.m.call('TestTargetsFullHop', c, 5 + i)) / 10000 for i in range(shots)]))
        for name, heights, offsets in shapes:
            surface, x = spawn[0][1] - heights[0], spawn[0][0] - offsets[0]
            floor = len(heights) == len(spawn) and all(abs(t[1] - surface - h) < .003 and abs(t[0] - x - o) < .003
                                                       for t, h, o in zip(spawn, heights, offsets)) and \
                self.floor_kind(m, x, surface, fail=False)
            if floor:
                return name, floor
        self.fail(f'no multi laser fits {spawn}')

    def test_all_combos_makes_about_half_the_spawns_a_combo_over_each_kind_of_floor(self):
        offered = {'GrNLa.dat': {'main'}, 'GrPs.dat': {'main', 'left', 'right'}, 'GrIz.dat': {'main', 'top'}}
        for kind in (FOX, FALCO):
            for filename, stage_id, normal in STAGES:
                with self.subTest(fighter=NAMES[kind], stage=filename):
                    m, spawns = self.run_targets(kind, filename, stage_id, normal, 200, free_practice=1,
                                                 full_hop=FH_ALL, you=(0, -300))
                    found = [self.group_kind(m, kind, s) for s in spawns]
                    combos = Counter(name for name, _ in found if name != 'single')
                    floors = Counter(floor for name, floor in found if name != 'single')
                    self.assertNotIn('sh', combos)
                    self.assertGreater(sum(combos.values()), 65)
                    self.assertLess(sum(combos.values()), 135)
                    self.assertEqual(set(combos), set(FH_COMBOS), combos)
                    self.assertEqual(set(floors), offered.get(filename, {'main', 'left', 'right', 'top'}), floors)

    def test_one_combo_comes_every_spawn(self):
        for kind in (FOX, FALCO):
            for combo in FH_COMBOS:
                with self.subTest(fighter=NAMES[kind], combo=FH_TEXT[combo]):
                    m, spawns = self.run_targets(kind, 'GrNBa.dat', GRKINDEXT_BATTLE, None, 40, free_practice=1,
                                                 full_hop=combo, you=(0, -300))
                    found = [self.group_kind(m, kind, s) for s in spawns]
                    self.assertEqual({name for name, _ in found}, {combo})
                    self.assertEqual({floor for _, floor in found}, {'main', 'left', 'right', 'top'})

    def test_sh_pairs_share_the_spawns_with_full_hop_combos(self):
        # One combo: half SH pairs, half the combo, no singles. All Combos: a third each.
        for full_hop, wanted in ((FH_HIGH_LOW, {'sh': .5, FH_HIGH_LOW: .5}),
                                 (FH_ALL, {'single': 1 / 3, 'sh': 1 / 3, 'fh': 1 / 3})):
            with self.subTest(full_hop=FH_TEXT[full_hop]):
                m, spawns = self.run_targets(FOX, 'GrNBa.dat', GRKINDEXT_BATTLE, None, 240, free_practice=1, shdl=1,
                                             full_hop=full_hop, you=(0, -300))
                names = Counter()
                for spawn in spawns:
                    name = self.group_kind(m, FOX, spawn)[0]
                    names['fh' if full_hop == FH_ALL and name in FH_COMBOS else name] += 1
                self.assertEqual(set(names), set(wanted), names)
                for name, share in wanted.items():
                    self.assertGreater(names[name], 240 * share * .7, names)
                    self.assertLess(names[name], 240 * share * 1.3, names)

    def test_full_hop_combos_keep_away_from_you(self):
        # On Battlefield's top platform, every spot over it is within FROM_YOU of you.
        m, spawns = self.run_targets(FALCO, 'GrNBa.dat', GRKINDEXT_BATTLE, None, 60, free_practice=1,
                                     full_hop=FH_HI_HI_LOW, you=(0, 54.4))
        floors = Counter()
        for spawn in spawns:
            for x, y in spawn:
                self.assertGreaterEqual((x ** 2 + (y - 54.4) ** 2) ** .5, FROM_YOU - .01)
            floors[self.group_kind(m, FALCO, spawn)[1]] += 1
        self.assertEqual(set(floors), {'main', 'left', 'right'}, floors)

    def test_no_full_hop_combos_in_timed_runs_or_with_the_setting_off(self):
        for kind, free_practice, full_hop in ((FOX, 0, FH_ALL), (FALCO, 0, FH_HIGH_LOW), (FOX, 1, FH_OFF)):
            with self.subTest(fighter=NAMES[kind], free_practice=free_practice, full_hop=FH_TEXT[full_hop]):
                _, spawns = self.run_targets(kind, 'GrNBa.dat', 31, None, 40, free_practice, full_hop=full_hop)
                self.assertTrue(all(len(s) == 1 for s in spawns))

    def test_no_pairs_for_falco_timed_runs_or_with_the_setting_off(self):
        for kind, free_practice, shdl in ((FALCO, 1, 1), (FOX, 0, 1), (FOX, 1, 0)):
            with self.subTest(fighter=NAMES[kind], free_practice=free_practice, shdl=shdl):
                _, spawns = self.run_targets(kind, 'GrNBa.dat', 31, None, 40, free_practice, shdl)
                self.assertTrue(all(len(s) == 1 for s in spawns))


BLAST = (-224, 224)  # The probe's blast zones.


def peak_floors(lines):
    """(left x, right x, y, shade) of each floor's lines, in drawing order: the main floor
    at the middle (dark), then FindPlatform's left, top (light) and right platforms, a side
    one only on its side of x = 0, and none closer than 1 to the main floor."""
    main = max(ay + (by - ay) * -ax / (bx - ax) for ax, ay, bx, by, platform in lines
               if not platform and ax <= 0 <= bx)
    platforms = [l for l in lines if l[4] and l[2] - l[0] >= 10]
    center = lambda l: (l[0] + l[2]) / 2
    height = lambda l: l[1] + (l[3] - l[1]) * (center(l) - l[0]) / (l[2] - l[0])
    floors = [(BLAST[0], BLAST[1], main, DARK)]
    for side in (-1, 0, 1):
        if side:
            found = [l for l in platforms if center(l) * side > 10]
            pick = min(found, key=lambda l: abs(center(l)), default=None)
        else:
            pick = max((l for l in platforms if l[0] <= 0 <= l[2]), key=height, default=None)
        if pick and height(pick) >= main + 1:
            floors.append((0 if side > 0 else BLAST[0], 0 if side < 0 else BLAST[1], height(pick),
                           BASE if side else LIGHT))
    return floors


class TargetPeakLineTests(unittest.TestCase):
    """Hop Peak Lines: a short hop's and a full hop's peak laser over each floor. Lowest Laser
    Lines: the lowest laser over each floor, the same way."""
    @classmethod
    def setUpClass(cls):
        build()
        import test_techchase
        cls.fighters = fighters()
        cls.floors = {name: test_techchase.native_floors(name) for name, _, _ in LEGAL}
        cls.floors[FOUNTAIN[0]] = FOUNTAIN_PLATFORMS + [
            line for line in test_techchase.native_floors(FOUNTAIN[0]) if not line[-1] & 0x100]

    def start(self, kind=FOX, filename='GrNBa.dat', stage_id=GRKINDEXT_BATTLE, normal=None, lines=1, **kw):
        m = TargetsMachine(kind, self.fighters[kind].attr)
        m.stage(stage_id, self.floors[filename], normal)
        m.init(lines=lines, **kw)
        m.capture()
        m.you(control=0)  # The fighter's first frame; targets wait for control, so only lines draw.
        m.think()
        return m

    def peaks(self, kind):
        """A short hop's and a full hop's highest laser above the floor, from the ISO."""
        a, c = self.fighters[kind].attr, self.fighters[kind].constants()
        return [laser_heights.peak_laser(a, c, a[v])[0] for v in ('hop_v', 'jump_v')]

    def lines(self, vertices):
        """Each GX_LINES pair as (left x, right x, y, color)."""
        for a, b in zip(vertices[::2], vertices[1::2]):
            self.assertEqual((a[1], a[2], a[3]), (b[1], b[2], b[3]))
        return [(a[0], b[0], a[1], a[3]) for a, b in zip(vertices[::2], vertices[1::2])]

    def assertLines(self, got, floors, kind, colors=(ORANGE, TEAL)):
        sh, fh = self.peaks(kind)
        wanted = [(left, right, y + h, line_color(color, shade))
                  for left, right, y, shade in floors for h, color in zip((sh, fh), colors)]
        self.assertEqual(len(got), len(wanted))
        for g, w in zip(got, wanted):
            self.assertAlmostEqual(g[0], w[0], delta=.001)
            self.assertAlmostEqual(g[1], w[1], delta=.001)
            self.assertAlmostEqual(g[2], w[2], delta=.002)
            self.assertEqual(g[3], w[3])

    def test_heights_are_a_short_hops_and_a_full_hops_peak_laser(self):
        # By hand: a hop's first frame skips gravity, so Fox's short hop peaks at 2.1 + 8.55 and
        # his full hop at 3.68 + 27.6, which B on frames 2 and 8 shoot from, with his aerial laser
        # 9.28 over TopN. Falco's full hop peaks at 4.1 + 47.4 (B on 14); his short hop peaks at
        # 1.9 + 9.68 on airborne frame 12, too soon for any B but the first, which takes the
        # skip away, so B on 2 shoots from 11.44. His laser is 3.80 up.
        for kind, by_hand in ((FOX, (19.933, 40.563)), (FALCO, (15.238, 55.298))):
            with self.subTest(fighter=NAMES[kind]):
                m = self.start(kind)
                got = [m.m.call('TestTargetsPeak', hop) / 10000 for hop in (0, 1)]
                for g, wanted, hand in zip(got, self.peaks(kind), by_hand):
                    self.assertAlmostEqual(g, wanted, delta=.0005)
                    self.assertAlmostEqual(g, hand, delta=.001)
        # A short hop's peak laser is the first shot of SH Double Lasers' B on frame 2, and a
        # full hop's is High-Low's first.
        m = self.start(FOX)
        self.assertAlmostEqual(m.m.call('TestTargetsPeak', 0), m.m.call('TestTargetsShots', 2, 1), delta=2)
        self.assertAlmostEqual(m.m.call('TestTargetsPeak', 1), m.m.call('TestTargetsFullHop', FH_HIGH_LOW, 2), delta=2)

    def test_every_legal_stage_gets_lines_over_its_floor_and_platforms(self):
        counts = {'GrNLa.dat': 1, 'GrPs.dat': 3}  # FD has no platforms; Stadium no top one.
        for kind in (FOX, FALCO):
            for filename, stage_id, normal in STAGES:
                with self.subTest(fighter=NAMES[kind], stage=filename):
                    m = self.start(kind, filename, stage_id, normal)
                    batches, vertices = m.draw()
                    floors = peak_floors(m.lines)
                    self.assertEqual(len(floors), counts.get(filename, 4))
                    self.assertEqual(batches, [(GX_LINES, 4 * len(floors), 12, 1)])  # Over everything.
                    self.assertTrue(all(v[2] == 0 for v in vertices))
                    self.assertLines(self.lines(vertices), floors, kind)

    def test_side_platforms_stop_at_the_middle_so_dream_lands_read_apart(self):
        m = self.start(FOX, 'GrOp.dat', 28)
        got = self.lines(m.draw()[1])
        sh, fh = self.peaks(FOX)
        floors = [(-224, 224, 0.0088, DARK), (-224, 0, 30.1421, BASE), (-224, 224, 51.4253, LIGHT),
                  (0, 224, 30.2425, BASE)]
        self.assertLines(got, floors, FOX)
        # The right platform's lines sit 0.1 over the left's, on their own half.
        self.assertAlmostEqual(got[6][2] - got[2][2], 30.2425 - 30.1421, delta=.001)

    def test_fountains_lines_follow_its_side_platforms_and_go_while_one_is_sunk(self):
        m = self.start(FOX, 'GrIz.dat', GRKINDEXT_IZUMI)
        main = 0.0028  # Its floor at the middle.
        for left, right in ((12.375, 27.375), (27.375, 12.375), (19.875, 16.125), (1.5, 22.125)):
            with self.subTest(left=left, right=right):
                m.m.call('TestTargetsMoveLine', 0, round(left * 1000), round(left * 1000))
                m.m.call('TestTargetsMoveLine', 1, round(right * 1000), round(right * 1000))
                self.assertLines(self.lines(m.draw()[1]),
                                 [(-224, 224, main, DARK), (-224, 0, left, BASE), (-224, 224, 42.75, LIGHT),
                                  (0, 224, right, BASE)], FOX)
        # Hidden, the game leaves the left one 0.375 up, inside the stage: its lines go.
        m.m.call('TestTargetsMoveLine', 0, 375, 375)
        batches, vertices = m.draw()
        self.assertEqual(batches, [(GX_LINES, 12, 12, 1)])
        self.assertLines(self.lines(vertices), [(-224, 224, main, DARK), (-224, 224, 42.75, LIGHT),
                                                (0, 224, 22.125, BASE)], FOX)

    def test_each_hop_has_a_color_darker_over_the_floor_and_lighter_over_the_top(self):
        # By hand: Orange FF9628 and Teal 00B4A0 at 230/255 over the side platforms, 3/5 of each
        # channel over the main floor and halfway to white over the top platform.
        self.assertEqual([line_color(ORANGE, s) for s in (DARK, BASE, LIGHT)],
                         [(138, 81, 21, 230), (230, 135, 36, 230), (230, 182, 132, 230)])
        self.assertEqual([line_color(TEAL, s) for s in (DARK, BASE, LIGHT)],
                         [(0, 97, 86, 230), (0, 162, 144, 230), (114, 195, 186, 230)])
        m = self.start()
        _, vertices = m.draw()
        floors = peak_floors(m.lines)
        self.assertEqual([f[3] for f in floors], [DARK, BASE, LIGHT, BASE])
        self.assertLines(self.lines(vertices), floors, FOX)
        self.assertEqual([v[3] for v in vertices[::2]],
                         [line_color(c, s) for s in (DARK, BASE, LIGHT, BASE) for c in (ORANGE, TEAL)])

    def test_the_color_rows_offer_the_osd_palette_save_and_recolor_at_once(self):
        self.assertEqual(len(LINE_NAMES), 15)
        self.assertEqual(len(LINE_RGB), 15)
        m = self.start()
        self.assertEqual((m.info(50), m.info(51), m.info(54), m.info(55), m.info(56)), (ORANGE, TEAL, ORANGE, TEAL, 1))
        for row in (OPT_SH_COLOR, OPT_FH_COLOR):
            texts = [bytes(m.m.cpu.mem_read(m.m.call('TestTargetsColorText', row, v), 16)).split(b'\0', 1)[0].decode()
                     for v in range(15)]
            self.assertEqual(texts, LINE_NAMES)
        floors = peak_floors(m.lines)
        for sh, fh in ((1, 12), (0, 14), (TEAL, 3)):  # Red and Gold, White and Lavender, Teal and Blue.
            with self.subTest(sh=LINE_NAMES[sh], fh=LINE_NAMES[fh]):
                m.choose(OPT_SH_COLOR, sh); m.choose(OPT_FH_COLOR, fh)
                self.assertEqual((m.info(52), m.info(53), m.info(4), m.info(29)), (sh, fh, 0, 0))  # Saved, no restart.
                self.assertLines(self.lines(m.draw()[1]), floors, FOX, (sh, fh))
        # Saved colors load into the rows, their values and the menu's copies alike, and draw.
        m = self.start(colors=(2, 6))
        self.assertEqual((m.info(50), m.info(51), m.info(54), m.info(55)), (2, 6, 2, 6))
        self.assertLines(self.lines(m.draw()[1]), floors, FOX, (2, 6))

    def test_the_lines_reach_the_blast_zones(self):
        m = self.start(FOX)
        m.m.call('TestTargetsBlast', -230500, 198250)
        got = self.lines(m.draw()[1])
        self.assertEqual([(round(l, 3), round(r, 3)) for l, r, _, _ in got],
                         [(-230.5, 198.25)] * 2 + [(-230.5, 0)] * 2 + [(-230.5, 198.25)] * 2 + [(0, 198.25)] * 2)

    def test_the_row_saves_and_the_lines_show_at_once(self):
        m = self.start(lines=0)
        self.assertEqual((m.info(36), m.info(38)), (0, 0))
        self.assertEqual(m.draw(), ([], []))
        m.choose(OPT_LINES, 1)
        self.assertEqual((m.info(37), m.info(4), m.info(29)), (1, 0, 0))  # Saved; no retry or restart.
        self.assertEqual(m.draw()[0], [(GX_LINES, 16, 12, 1)])
        m.choose(OPT_LINES, 0)
        self.assertEqual((m.info(37), m.draw()), (0, ([], [])))
        # A saved On loads into the row, its value and the menu's copy alike.
        m = self.start(lines=1)
        self.assertEqual((m.info(36), m.info(38)), (1, 1))
        # Timed runs and Free Practice both draw them.
        m = self.start(lines=1, free_practice=1)
        self.assertEqual(len(m.draw()[0]), 1)

    def test_nothing_before_the_fighters_first_frame_or_on_other_passes(self):
        m = TargetsMachine(FOX, self.fighters[FOX].attr)
        m.flat(((-50, -20, 27), (20, 50, 27)))
        m.init(lines=1)
        m.capture()
        self.assertEqual(m.draw(), ([], []))  # Its heights aren't known yet.
        m.you(control=0); m.think()
        for pass_ in (0, 1, 3):
            self.assertEqual(m.draw(pass_), ([], []))
        self.assertEqual(m.draw()[0], [(GX_LINES, 12, 12, 1)])

    def test_the_lines_go_under_the_targets(self):
        m = self.start()
        m.you(); m.think()
        self.assertEqual(len(m.targets()), 1)
        batches, _ = m.draw()
        self.assertEqual(batches[0], (GX_LINES, 16, 12, 1))
        self.assertGreater(len(batches), 1)  # The target, drawn after.

    def low(self, kind):
        """The lowest laser above the floor, from the ISO: TopN as far under the floor as the
        laser loop's ECB bottom allows, never under the floor itself."""
        c = self.fighters[kind].constants()
        return max(c['air'] - c['ecb_bottom'], 0)

    def assertLowLines(self, got, floors, kind, color):
        wanted = [(left, right, y + self.low(kind), line_color(color, shade)) for left, right, y, shade in floors]
        self.assertEqual(len(got), len(wanted))
        for g, w in zip(got, wanted):
            self.assertAlmostEqual(g[0], w[0], delta=.001)
            self.assertAlmostEqual(g[1], w[1], delta=.001)
            self.assertAlmostEqual(g[2], w[2], delta=.002)
            self.assertEqual(g[3], w[3])

    def test_lowest_laser_lines_sit_at_the_lowest_laser_over_every_floor(self):
        # By hand: Fox's aerial laser is 9.2829 over TopN, which can sit 4.4322 under the floor
        # with his legs tucked, so 4.8507 up. Falco's would start under the floor, so his hugs it.
        for kind, by_hand in ((FOX, 4.8507), (FALCO, 0)):
            with self.subTest(fighter=NAMES[kind]):
                m = self.start(kind, lines=0, low_line=ORANGE + 1)
                self.assertAlmostEqual(m.info(67) / 10000, self.low(kind), delta=.0003)
                self.assertAlmostEqual(m.info(67) / 10000, by_hand, delta=.0002)
                # The bottom of a single target's range.
                self.assertEqual(m.info(67), m.m.call('TestTargetsBlaster', kind == FALCO, 5))
        counts = {'GrNLa.dat': 1, 'GrPs.dat': 3}  # FD has no platforms; Stadium no top one.
        for kind in (FOX, FALCO):
            for filename, stage_id, normal in STAGES:
                with self.subTest(fighter=NAMES[kind], stage=filename):
                    m = self.start(kind, filename, stage_id, normal, lines=0, low_line=TEAL + 1)
                    batches, vertices = m.draw()
                    floors = peak_floors(m.lines)
                    self.assertEqual(len(floors), counts.get(filename, 4))
                    self.assertEqual(batches, [(GX_LINES, 2 * len(floors), 12, 1)])  # Over everything.
                    self.assertTrue(all(v[2] == 0 for v in vertices))
                    self.assertLowLines(self.lines(vertices), floors, kind, TEAL)

    def test_lowest_laser_lines_are_off_by_default_and_a_color_saves_and_shows_at_once(self):
        m = self.start(lines=0)
        self.assertEqual((m.info(63), m.info(65), m.info(66)), (0, 0, 16))  # Off, then the 15 colors.
        texts = [bytes(m.m.cpu.mem_read(m.m.call('TestTargetsColorText', OPT_LOW_LINE, v), 16)).split(b'\0', 1)[0]
                 .decode() for v in range(16)]
        self.assertEqual(texts, ['Off'] + LINE_NAMES)
        self.assertEqual(m.draw(), ([], []))
        floors = peak_floors(m.lines)
        for value in (1, 15, ORANGE + 1, 3):  # White, Lavender, Orange, Green.
            with self.subTest(color=LINE_NAMES[value - 1]):
                m.choose(OPT_LOW_LINE, value)
                self.assertEqual((m.info(64), m.info(4), m.info(29)), (value, 0, 0))  # Saved; no retry or restart.
                self.assertLowLines(self.lines(m.draw()[1]), floors, FOX, value - 1)
        m.choose(OPT_LOW_LINE, 0)
        self.assertEqual((m.info(64), m.draw()), (0, ([], [])))
        # A saved color loads into the row, its value and the menu's copy alike, and draws in
        # timed runs, Free Practice and paused.
        for free_practice in (0, 1):
            with self.subTest(free_practice=free_practice):
                m = self.start(lines=0, low_line=12, free_practice=free_practice)
                self.assertEqual((m.info(63), m.info(65)), (12, 12))
                self.assertLowLines(self.lines(m.draw()[1]), floors, FOX, 11)
                m.m.call('TestTargetsPause', 1)
                self.assertLowLines(self.lines(m.draw()[1]), floors, FOX, 11)
                m.m.call('TestTargetsPause', 0)

    def test_lowest_laser_lines_and_hop_peak_lines_are_their_own_rows(self):
        m = self.start(lines=1, low_line=0)
        m.choose(OPT_LOW_LINE, 5)  # Yellow.
        self.assertEqual(m.draw()[0], [(GX_LINES, 16, 12, 1), (GX_LINES, 8, 12, 1)])  # Peak lines, then these.
        m.choose(OPT_LINES, 0)
        floors = peak_floors(m.lines)
        batches, vertices = m.draw()
        self.assertEqual(batches, [(GX_LINES, 8, 12, 1)])
        self.assertLowLines(self.lines(vertices), floors, FOX, 4)
        # The hop colors never color them, nor they the hop lines.
        m.choose(OPT_SH_COLOR, 0); m.choose(OPT_FH_COLOR, 1)
        self.assertLowLines(self.lines(m.draw()[1]), floors, FOX, 4)
        m.choose(OPT_LINES, 1)
        self.assertLines(self.lines(m.draw()[1][:16]), floors, FOX, (0, 1))

    def test_lowest_laser_lines_wait_for_the_fighters_first_frame_and_draw_on_one_pass(self):
        m = TargetsMachine(FOX, self.fighters[FOX].attr)
        m.flat(((-50, -20, 27), (20, 50, 27)))
        m.init(low_line=1)
        m.capture()
        self.assertEqual(m.draw(), ([], []))  # The fighter's height isn't known yet.
        m.you(control=0); m.think()
        for pass_ in (0, 1, 3):
            self.assertEqual(m.draw(pass_), ([], []))
        self.assertEqual(m.draw()[0], [(GX_LINES, 6, 12, 1)])

    def test_lowest_laser_lines_go_under_the_double_jump_line_and_the_targets(self):
        m = self.start(lines=1, low_line=1, double_jump=1)
        m.air()  # In the air with the double jump left.
        m.place(30, 40)
        batches, _ = m.draw()
        self.assertEqual(batches[:3], [(GX_LINES, 16, 12, 1), (GX_LINES, 8, 12, 1), (GX_LINES, 2, 12, 1)])
        self.assertGreater(len(batches), 3)  # Then the target.


class TargetDoubleJumpTests(unittest.TestCase):
    """Double Jump Height: a red line where a laser shot at a double jump's peak flies, which
    stays at that height once the double jump comes."""
    @classmethod
    def setUpClass(cls):
        build()
        cls.fighters = fighters()

    def start(self, kind=FOX, double_jump=1, you=(4, 20), **kw):
        m = TargetsMachine(kind, self.fighters[kind].attr)
        m.flat(((-50, -20, 27), (20, 50, 27)))
        m.init(double_jump=double_jump, **kw)
        m.capture()
        m.air()
        m.you(*you, control=0)  # The fighter's first frame; targets wait for control.
        m.think()
        return m

    def rise(self, kind):
        a = self.fighters[kind].attr
        return laser_heights.rise(a['jump_v'] * a['air_jump_mult'], a['gravity'])

    def laser(self, kind, feet):
        """Where a laser shot at the peak of a double jump from `feet` flies."""
        return feet + self.rise(kind) + self.fighters[kind].constants()['air']

    def assertLine(self, m, y, blast=BLAST):
        """One red line at y, from blast zone to blast zone."""
        batches, vertices = m.draw()
        self.assertEqual(batches, [(GX_LINES, 2, 12, 1)])  # Over everything.
        self.assertEqual([v[3] for v in vertices], [DJ_RED, DJ_RED])
        for v, end in zip(vertices, blast):
            self.assertAlmostEqual(v[0], end, delta=.002)
            self.assertAlmostEqual(v[1], y, delta=.002)
            self.assertEqual(v[2], 0)

    def assertNoLine(self, m):
        self.assertEqual((m.info(57), m.draw()), (0, ([], [])))

    def test_the_rise_is_a_double_jumps_from_the_iso(self):
        # By hand: Fox's double jump starts at 3.68 x 1.2 = 4.416 and Falco's at 4.1 x 0.94,
        # less 0.23 and 0.17 gravity a frame: 40.204 and 41.778, the doc's full hop + double jump.
        for kind, by_hand in ((FOX, 40.204), (FALCO, 41.778)):
            with self.subTest(fighter=NAMES[kind]):
                m = self.start(kind)
                self.assertAlmostEqual(m.info(48) / 10000, self.rise(kind), delta=.0005)
                self.assertAlmostEqual(m.info(48) / 10000, by_hand, delta=.001)
                a = self.fighters[kind].attr
                self.assertAlmostEqual((m.info(32) - m.info(48)) / 10000, max(full_hop(a)), delta=.001)

    def test_a_line_across_the_stage_where_a_laser_at_the_peak_flies(self):
        # By hand: the rise and the aerial laser over TopN, 40.204 + 9.2829 for Fox and
        # 41.778 + 3.7980 for Falco, whichever way the fighter faces.
        for kind, by_hand in ((FOX, 49.487), (FALCO, 45.576)):
            for facing in (1, -1):
                with self.subTest(fighter=NAMES[kind], facing=facing):
                    m = self.start(kind)
                    m.air(facing=facing)
                    self.assertLine(m, self.laser(kind, 20))
                    self.assertLine(m, 20 + by_hand)

    def test_it_follows_the_fighter_every_frame_and_reaches_the_blast_zones(self):
        m = self.start()
        m.you(-30, 61.5, control=0)
        self.assertLine(m, self.laser(FOX, 61.5))
        # Wherever the fighter is across, the line spans the live blast zones.
        m.m.call('TestTargetsBlast', -230500, 198250)
        m.you(150, 40, control=0)
        self.assertLine(m, self.laser(FOX, 40), (-230.5, 198.25))

    def test_only_while_the_double_jump_can_still_come(self):
        m = self.start()
        shown = [(1, 1, 2, s) for s in (ASID_JUMPF, ASID_JUMPB, ASID_FALL, ASID_ATTACKAIRN, FX_SPECIALAIRNLOOP,
                                         FX_SPECIALAIRNEND, FX_SPECIALLWSTART, FX_SPECIALAIRLWLOOP, ASID_CLIFFJUMPSLOW1,
                                         ASID_REBIRTHWAIT + 1, ASID_FALLSPECIAL - 1, ASID_FALLSPECIALB + 1)]
        shown += [(1, 0, 2, ASID_FALL)]  # Off the edge with both jumps.
        hidden = [(0, 1, 2, ASID_FALL), (1, 2, 2, ASID_FALL), (1, 3, 2, ASID_FALL)]  # Grounded, or the jump spent.
        hidden += [(1, 1, 2, s) for s in (ASID_DEADDOWN, ASID_REBIRTHWAIT, ASID_FALLSPECIAL, ASID_FALLSPECIALB,
                                          ASID_ESCAPEAIR, ASID_CLIFFCATCH, ASID_CLIFFWAIT, ASID_CLIFFESCAPEQUICK,
                                          FX_SPECIALSSTART, FX_SPECIALAIRS, FX_SPECIALHIHOLDAIR, FX_SPECIALAIRHI,
                                          FX_SPECIALHIFALL, FX_SPECIALHIBOUND)]
        for air, used, max_jumps, state in shown + hidden:
            with self.subTest(air=air, used=used, state=state):
                m.air(air, used, max_jumps, state)  # No think: nothing's held.
                self.assertEqual(len(m.draw()[0]), (air, used, max_jumps, state) in shown)

    def test_the_line_stays_at_the_double_jumps_peak_laser_until_landing(self):
        for kind in (FOX, FALCO):
            for jump in (ASID_JUMPAERIALF, ASID_JUMPAERIALB):
                with self.subTest(fighter=NAMES[kind], jump=jump):
                    m = self.start(kind)
                    m.you(4, 23.5, control=0); m.think()  # Falling with the jump: live.
                    held = self.laser(kind, 23.5)
                    self.assertEqual(m.info(57), 0)
                    self.assertLine(m, held)
                    # The jump's first frame: it has already risen a frame by the think.
                    m.air(used=2, state=jump); m.you(4.5, 27.9, control=0); m.think()
                    self.assertEqual(m.info(57), 1)
                    self.assertLine(m, held)
                    # It holds through the rise, the fall, an aerial, the laser and even helpless,
                    # wherever the fighter drifts.
                    for feet, x, state in ((50, 11, jump), (60, 13, ASID_FALL), (40, 8, ASID_ATTACKAIRN),
                                           (30, 7, FX_SPECIALAIRNLOOP), (5, 6, ASID_FALLSPECIAL)):
                        m.air(used=2, state=state); m.you(x, feet, control=0); m.think()
                        self.assertLine(m, held)
                    # Paused, and with the row turned off and on.
                    m.m.call('TestTargetsPause', 1)
                    self.assertLine(m, held)
                    m.m.call('TestTargetsPause', 0)
                    m.choose(OPT_DOUBLE_JUMP, 0)
                    self.assertEqual(m.draw(), ([], []))
                    m.choose(OPT_DOUBLE_JUMP, 1)
                    self.assertLine(m, held)
                    # Landing ends it.
                    m.air(air=0, used=0, state=ASID_FALL); m.think()
                    self.assertNoLine(m)

    def test_the_ledge_or_a_ko_ends_it_and_the_jump_coming_back_shows_it_live(self):
        for end in (ASID_CLIFFCATCH, ASID_CLIFFWAIT, ASID_DEADDOWN, ASID_REBIRTHWAIT):
            with self.subTest(end=end):
                m = self.start()
                m.air(used=2, state=ASID_JUMPAERIALF); m.think()
                self.assertEqual(m.info(57), 1)
                m.air(used=2, state=end); m.think()
                self.assertNoLine(m)
                m.air(used=2, state=ASID_FALL); m.think()  # Off again with no jump: still nothing.
                self.assertNoLine(m)
        # The jump given back while held: the live line from where the fighter is.
        m = self.start()
        m.air(used=2, state=ASID_JUMPAERIALB); m.think()
        m.air(used=1, state=ASID_FALL); m.you(-10, 35, control=0); m.think()
        self.assertEqual(m.info(57), 0)
        self.assertLine(m, self.laser(FOX, 35))

    def test_up_b_spends_the_jump_and_the_line_just_goes(self):
        m = self.start()
        for state in (FX_SPECIALHIHOLDAIR, FX_SPECIALAIRHI, ASID_FALLSPECIAL):
            m.air(used=2, state=state); m.think()
            self.assertNoLine(m)
        # A JumpAerial state holds the line only the frame after the jump could come.
        m.air(used=2, state=ASID_JUMPAERIALF); m.think()
        self.assertNoLine(m)

    def test_a_retry_holds_nothing(self):
        m = self.start()
        m.air(used=2, state=ASID_JUMPAERIALF); m.think()
        self.assertEqual(m.info(57), 1)
        m.init(double_jump=1)
        self.assertEqual(m.draw(), ([], []))
        m.think()
        self.assertNoLine(m)

    def test_the_row_saves_and_the_line_shows_at_once(self):
        m = self.start(double_jump=0)
        self.assertEqual((m.info(45), m.info(47)), (0, 0))
        self.assertEqual(m.draw(), ([], []))
        m.choose(OPT_DOUBLE_JUMP, 1)
        self.assertEqual((m.info(46), m.info(4), m.info(29)), (1, 0, 0))  # Saved; no retry or restart.
        self.assertEqual(len(m.draw()[0]), 1)
        m.choose(OPT_DOUBLE_JUMP, 0)
        self.assertEqual((m.info(46), m.draw()), (0, ([], [])))
        # A saved On loads into the row, its value and the menu's copy alike.
        m = self.start(double_jump=1)
        self.assertEqual((m.info(45), m.info(47)), (1, 1))
        # Timed runs and Free Practice both draw it, and paused too.
        m = self.start(free_practice=1)
        self.assertEqual(len(m.draw()[0]), 1)
        m.m.call('TestTargetsPause', 1)
        self.assertEqual(len(m.draw()[0]), 1)
        m.m.call('TestTargetsPause', 0)

    def test_nothing_before_the_fighters_first_frame_or_on_other_passes(self):
        m = TargetsMachine(FOX, self.fighters[FOX].attr)
        m.flat()
        m.init(double_jump=1)
        m.capture()
        m.air()
        self.assertEqual(m.draw(), ([], []))  # Its rise isn't known yet.
        m.you(control=0); m.think()
        for pass_ in (0, 1, 3):
            self.assertEqual(m.draw(pass_), ([], []))
        self.assertEqual(len(m.draw()[0]), 1)

    def test_it_goes_over_the_peak_lines_and_under_the_targets(self):
        m = self.start(lines=1)
        m.place(30, 40)
        batches, _ = m.draw()
        self.assertEqual(batches[:2], [(GX_LINES, 12, 12, 1), (GX_LINES, 2, 12, 1)])  # Lines, then this line,
        self.assertGreater(len(batches), 2)  # then the target.


class TargetEventTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build()
        cls.fighters = fighters()

    def start(self, kind=FOX, platforms=((-50, -20, 27), (20, 50, 27), (-15, 15, 54)), **kw):
        m = TargetsMachine(kind, self.fighters[kind].attr)
        m.flat(platforms)
        m.init(**kw)
        return m

    def test_the_first_target_waits_for_control(self):
        m = self.start()
        m.you(control=0)
        for _ in range(3): m.think()
        self.assertEqual(m.targets(), [])
        m.you(control=1); m.think()
        self.assertEqual(len(m.targets()), 1)

    def test_a_laser_through_a_target_breaks_it_scores_and_stops_there(self):
        m = self.start()
        m.you(); m.think()
        (x, y), = m.targets()
        m.laser(0, (x - 7, y), (x - .5, y))
        m.think()
        self.assertEqual((m.info(0), m.info(8), m.info(18)), (1, 1, 1))
        self.assertEqual((m.m.call('TestTargetsLaserInfo', 0, 0), m.m.call('TestTargetsLaserInfo', 0, 1)), (1, 1))
        self.assertEqual(m.info(34), 1)  # The ring starts.
        (nx, ny), = m.targets()
        self.assertNotEqual((nx, ny), (x, y))  # The next one is out.
        m.m.call('TestTargetsClearLasers')
        for age in range(2, 13):
            m.think()
            self.assertEqual(m.info(34), age)
        m.think()
        self.assertEqual(m.info(34), 0)

    def test_a_laser_sweeping_past_the_target_in_one_frame_still_hits(self):
        m = self.start()
        m.you(); m.think()
        (x, y), = m.targets()
        m.laser(0, (x - 4, y + 1), (x + 4, y + 1))  # Both ends out of reach, the middle through it.
        m.think()
        self.assertEqual(m.info(0), 1)

    def test_reach_is_the_target_plus_the_lasers_hitbox(self):
        m = self.start()
        m.you(); m.think()
        reach = 1.5 + 300 / 256
        for dy, hit in ((reach + .01, 0), (reach - .01, 1)):
            m.place(0, 30)
            m.laser(0, (-10, 30 + dy), (10, 30 + dy))
            m.think()
            m.m.call('TestTargetsClearLasers')
            self.assertEqual(m.info(0), hit)

    def test_other_lasers_new_hitboxes_and_other_items_never_hit(self):
        m = self.start()
        m.you(); m.think()
        m.place(0, 30)
        m.laser(0, (-5, 30), (5, 30), mine=0)
        m.laser(1, (-5, 30), (5, 30), state=1)  # Enabled, but not placed yet.
        m.laser(2, (-5, 30), (5, 30), kind=0x2B)
        m.laser(3, (-5, 30), (5, 30), state=0)
        m.think()
        self.assertEqual(m.info(0), 0)
        self.assertEqual([m.m.call('TestTargetsLaserInfo', i, 0) for i in range(4)], [0, 0, 0, 0])
        m.laser(1, (-5, 30), (5, 30), kind=ITEM_FALCOLASER, state=2)
        m.think()
        self.assertEqual(m.info(0), 1)

    def test_a_standing_laser_misses_the_lowest_target_and_a_low_laser_hits_it(self):
        for kind, fighter in self.fighters.items():
            with self.subTest(fighter=NAMES[kind]):
                c = fighter.constants()
                lowest = max(c['air'] - c['ecb_bottom'], 0)
                m = self.start(kind, platforms=())
                m.you(); m.think()
                for height, hit in ((c['ground'], 0), (lowest + .01, 1)):
                    m.place(40, lowest)
                    m.laser(0, (10, height), (17, height), kind=ITEM_FALCOLASER if kind == FALCO else ITEM_FOXLASER)
                    m.laser(0, (17, height), (45, height), kind=ITEM_FALCOLASER if kind == FALCO else ITEM_FOXLASER)
                    m.think()
                    m.m.call('TestTargetsClearLasers')
                    self.assertEqual(m.info(0), hit)

    def test_a_laser_breaks_one_target_and_a_pair_waits_for_both(self):
        m = self.start(free_practice=1, shdl=1)
        m.you(); m.think()
        for _ in range(60):
            if len(m.targets()) == 2: break
            m.shoot_all()
        (ux, uy), (lx, ly) = m.targets()
        m.laser(0, (lx - 3, ly), (lx + 3, ly))
        m.think()
        m.m.call('TestTargetsClearLasers')
        self.assertEqual(m.targets(), [(ux, uy)])
        score = m.info(0)
        m.think()
        self.assertEqual(m.targets(), [(ux, uy)])  # Still waiting on the other.
        m.laser(0, (ux - 3, uy), (ux + 3, uy))
        m.think()
        self.assertEqual(m.info(0), score + 1)
        self.assertNotIn((ux, uy), m.targets())
        self.assertGreater(len(m.targets()), 0)

    def test_the_camera_frames_the_targets(self):
        m = self.start(free_practice=1, shdl=1)
        m.you(); m.think()
        for _ in range(60):
            targets = m.targets()
            xs, ys = [t[0] for t in targets], [t[1] for t in targets]
            self.assertAlmostEqual(m.info(14) / 1000, xs[0], delta=.002)
            self.assertEqual((m.info(58), m.info(59)), (-10000, 10000))  # Pairs and singles: one x.
            self.assertAlmostEqual(m.info(15) / 1000, (min(ys) + max(ys)) / 2, delta=.002)
            self.assertAlmostEqual(m.info(16) / 1000, (max(ys) - min(ys)) / 2 + 10, delta=.002)
            self.assertAlmostEqual(m.info(17) / 1000, -(max(ys) - min(ys)) / 2 - 10, delta=.002)
            m.shoot_all()

    def test_pause_holds_everything(self):
        m = self.start()
        m.you(); m.think()
        (x, y), = m.targets()
        m.m.call('TestTargetsPause', 1)
        m.laser(0, (x - 3, y), (x + 3, y))
        m.think()
        self.assertEqual((m.info(0), m.targets()), (0, [(x, y)]))
        m.m.call('TestTargetsPause', 0)
        m.think()
        self.assertEqual(m.info(0), 1)

    def test_falco_reads_not_available_and_fox_unlocks_it_only_in_free_practice(self):
        for kind, free_practice, disabled, unavailable in ((FOX, 1, 0, 0), (FOX, 0, 1, 0), (FALCO, 1, 1, 1), (FALCO, 0, 1, 1)):
            with self.subTest(fighter=NAMES[kind], free_practice=free_practice):
                m = self.start(kind, free_practice=free_practice, shdl=1)
                self.assertEqual(m.info(9), 1)  # Greyed out until the fighter's known.
                m.you(); m.think()
                self.assertEqual((m.info(9), m.info(10), m.info(11)), (disabled, unavailable, 1))
                self.assertEqual(m.info(13), free_practice)

    def test_full_hop_row_unlocks_in_free_practice_for_fox_and_falco(self):
        for kind in (FOX, FALCO):
            for free_practice in (1, 0):
                with self.subTest(fighter=NAMES[kind], free_practice=free_practice):
                    m = self.start(kind, free_practice=free_practice, full_hop=FH_HIGH_MEDIUM)
                    self.assertEqual(m.info(39), 1)  # Greyed out until the fighter's known.
                    self.assertEqual((m.info(41), m.info(43)), (FH_HIGH_MEDIUM, FH_HIGH_MEDIUM))  # Loaded.
                    m.you(); m.think()
                    self.assertEqual((m.info(39), m.info(40), m.info(41)), (1 - free_practice, 0, FH_HIGH_MEDIUM))
        self.assertEqual(m.info(44), 6)
        texts = [bytes(m.m.cpu.mem_read(m.m.call('TestTargetsFullHopText', v), 16)).split(b'\0', 1)[0].decode()
                 for v in range(6)]
        self.assertEqual(tuple(texts), FH_TEXT)

    def test_full_hop_rows_save_in_their_old_order(self):
        # Each saved value loads into its row: High-Low took Peak-Low's 4, High-Lowest's old 6
        # reads as High-Low, and 7, no row, as Off.
        rows = {0: FH_OFF, 1: FH_ALL, 2: FH_HI_HI_LOW, 3: FH_HIGH_MEDHIGH, 4: FH_HIGH_LOW, 5: FH_HIGH_MEDIUM,
                FH_OLD_HIGH_LOWEST: FH_HIGH_LOW, 7: FH_OFF}
        for saved, row in rows.items():
            with self.subTest(saved=saved):
                m = self.start(free_practice=1, full_hop_saved=saved)
                self.assertEqual((m.info(41), m.info(43), m.info(42)), (row, row, saved))  # Read, not written.
        # Each row picked saves its own value.
        m = self.start(free_practice=1)
        m.you(); m.think()
        for row in (FH_HIGH_MEDIUM, FH_HIGH_LOW, FH_ALL, FH_HI_HI_LOW, FH_HIGH_MEDHIGH, FH_OFF):
            m.choose(OPT_FULL_HOP, row)
            self.assertEqual((m.info(41), m.info(42)), (row, FH_SAVED[row]))

    def test_full_hop_saves_and_reaches_the_next_spawn(self):
        m = self.start(free_practice=1)
        m.you(); m.think()
        m.choose(OPT_FULL_HOP, FH_HIGH_LOW)
        self.assertEqual((m.info(42), m.info(4)), (FH_SAVED[FH_HIGH_LOW], 0))  # Saved, no restart.
        m.shoot_all()  # The single that was out.
        high, low = (m.m.call('TestTargetsFullHop', FH_HIGH_LOW, k) / 10000 for k in (2, 3))
        for _ in range(10):
            (ux, uy), (lx, ly) = m.targets()
            self.assertEqual(ux, lx)
            self.assertAlmostEqual(uy - ly, high - low, delta=.003)
            m.shoot_all()
        m.choose(OPT_FULL_HOP, FH_OFF)
        self.assertEqual(m.info(42), FH_OFF)
        m.shoot_all()  # The pair that was out.
        sizes = []
        for _ in range(10):
            sizes.append(len(m.targets()))
            m.shoot_all()
        self.assertEqual(set(sizes), {1})

    def test_hi_hi_low_is_three_targets_and_waits_for_all_three(self):
        m = self.start(free_practice=1, full_hop=FH_HI_HI_LOW)
        m.you(); m.think()
        group = m.targets()
        self.assertEqual(len(group), 3)
        # The high two side by side, about level, either side of the low one.
        (lx, ly), (rx, ry), (x, y) = group
        self.assertEqual((round(x - lx, 3), round(rx - x, 3)), (HHL_SPREAD, HHL_SPREAD))
        self.assertLess(abs(ly - ry), REACH)
        self.assertGreater(min(ly, ry) - y, 20)
        # The camera takes them all in.
        self.assertAlmostEqual(m.info(14) / 1000, x, delta=.002)
        self.assertAlmostEqual(m.info(58) / 1000, -HHL_SPREAD - 10, delta=.002)
        self.assertAlmostEqual(m.info(59) / 1000, HHL_SPREAD + 10, delta=.002)
        for i, (x, y) in enumerate(group):
            m.laser(0, (x - 3, y), (x + 3, y))
            m.think()
            m.m.call('TestTargetsClearLasers')
            self.assertEqual(m.info(0), i + 1)
            if i < 2:
                self.assertEqual(m.targets(), group[i + 1:])  # The rest wait.
        self.assertEqual(len(m.targets()), 3)
        self.assertNotEqual(m.targets(), group)

    def test_double_lasers_saves_and_reaches_the_next_spawn(self):
        m = self.start(free_practice=1)
        m.you(); m.think()
        m.choose(OPT_SHDL, 1)
        self.assertEqual((m.info(3), m.info(4)), (1, 0))  # Saved, no restart.
        sizes = []
        for _ in range(40):
            m.shoot_all()
            sizes.append(len(m.targets()))
        self.assertIn(2, sizes)
        m.choose(OPT_SHDL, 0)
        self.assertEqual(m.info(3), 0)
        for _ in range(3): m.shoot_all()  # Any pair left out finishes first.
        sizes = []
        for _ in range(40):
            m.shoot_all()
            sizes.append(len(m.targets()))
        self.assertEqual(set(sizes), {1})

    def test_timed_run_ends_at_the_minute_and_saves_a_better_best(self):
        for best, score, saved, sfx in ((2, 3, 3, 325), (5, 3, 5, 324), (254, 300, 255, 325)):
            with self.subTest(best=best, score=score):
                m = self.start(best=best)
                m.you(); m.think()
                self.assertEqual(m.info(31), best)
                for _ in range(score): m.shoot_all()
                m.m.call('TestTargetsClock', 3599); m.think()
                self.assertEqual(m.info(5), 0)
                m.m.call('TestTargetsClock', 3600); m.think()
                self.assertEqual((m.info(5), m.info(1), m.info(6), m.info(30)), (1, saved, sfx, 2))  # MATCHENDKIND_VSGAME
                self.assertEqual(m.info(7), 0x9C40 if saved != best else 0)
                self.assertEqual(m.label(), 'Best: %d')

    def test_free_practice_counts_up_without_an_end_and_saves_its_toggle(self):
        m = self.start(free_practice=1)
        m.you(); m.think()
        self.assertEqual((m.info(13), m.info(19)), (1, 1))
        self.assertEqual(m.label(), 'Free Practice')
        m.m.call('TestTargetsClock', 3600); m.think()
        self.assertEqual(m.info(5), 0)
        m.choose(OPT_FREEPRACTICE, 0)
        self.assertEqual((m.info(2), m.info(4)), (0, 1))  # Saved, then a restart.
        m = self.start()
        m.choose(OPT_FREEPRACTICE, 1)
        self.assertEqual((m.info(2), m.info(4)), (1, 1))

    def test_reset_keeps_the_best_and_the_counter_position(self):
        m = self.start(free_practice=1, shdl=1, best=17, counter=3, lines=1, full_hop=FH_ALL, double_jump=1,
                       colors=(1, 12), pointers=0, low_line=6)
        self.assertEqual((m.info(36), m.info(41), m.info(45), m.info(50), m.info(51), m.info(60), m.info(63)),
                         (1, FH_ALL, 1, 1, 12, 0, 6))
        m.m.call('TestTargetsSelect', OPT_DEFAULTS)
        self.assertEqual((m.info(2), m.info(3), m.info(1), m.info(20), m.info(4)), (0, 0, 17, 3, 1))
        self.assertEqual((m.info(37), m.info(36)), (0, 0))  # Hop Peak Lines goes Off too,
        self.assertEqual((m.info(42), m.info(41)), (FH_OFF, FH_OFF))  # FH Multi Lasers,
        self.assertEqual((m.info(46), m.info(45)), (0, 0))  # Double Jump Height,
        self.assertEqual((m.info(52), m.info(53), m.info(50), m.info(51)), (ORANGE, TEAL, ORANGE, TEAL))  # the colors,
        self.assertEqual((m.info(64), m.info(63)), (0, 0))  # Lowest Laser Lines,
        self.assertEqual((m.info(61), m.info(60)), (1, 1))  # and Target Pointers comes back On.

    def test_target_pointers_are_on_by_default_save_and_show_or_go_at_once(self):
        m = self.start()
        m.capture()
        self.assertEqual((m.info(60), m.info(62)), (1, 1))  # On by default.
        m.place(30, 40)
        batches, vertices = m.draw()
        self.assertEqual(batches, TARGET_BATCHES)
        # Four ticks on the axes through the target's center, outside the bullseye, black under white.
        ticks = vertices[:16]
        self.assertEqual([v[3] for v in ticks], [(0, 0, 0, 255)] * 8 + [(255, 255, 255, 255)] * 8)
        for x, y, z, _ in ticks:
            self.assertTrue(abs(x - 30) < .001 or abs(y - 40) < .001)
            self.assertGreater(max(abs(x - 30), abs(y - 40)), 1.5)
        self.assertEqual({(round(x - 30, 3), round(y - 40, 3)) for x, y, _, _ in ticks[:8]},
                         {(2.5, 0), (5, 0), (-2.5, 0), (-5, 0), (0, 2.5), (0, 5), (0, -2.5), (0, -5)})
        m.choose(OPT_POINTERS, 0)
        self.assertEqual((m.info(61), m.info(4), m.info(29)), (0, 0, 0))  # Saved; no retry or restart.
        batches, vertices = m.draw()
        self.assertEqual(batches, TARGET_BATCHES[1:])  # Just the bullseye.
        self.assertEqual(len(vertices), 4 * 22)
        m.choose(OPT_POINTERS, 1)
        self.assertEqual((m.info(61), m.draw()[0]), (1, TARGET_BATCHES))
        # A saved Off loads into the row, its value and the menu's copy alike, in either mode.
        for free_practice in (0, 1):
            m = self.start(pointers=0, free_practice=free_practice)
            m.capture()
            self.assertEqual((m.info(60), m.info(62)), (0, 0))
            m.place(-20, 30)
            self.assertEqual(m.draw()[0], TARGET_BATCHES[1:])

    def test_target_pointers_leave_the_break_ring_and_every_target_of_a_group(self):
        m = self.start(pointers=0)
        m.capture()
        m.you(control=1); m.think()
        m.shoot_all()
        self.assertEqual(m.info(34), 1)  # The ring is out,
        batches, _ = m.draw()
        self.assertEqual(batches[0], (GX_LINESTRIP, 21, 18, 1))  # and drawn as it was,
        self.assertNotIn(TARGET_BATCHES[0], batches)  # with no ticks on the next target.

    def test_counter_position_is_eggs_ercises_and_moves_the_count_at_once(self):
        m = self.start(counter=4)
        self.assertEqual((m.info(21), m.info(22), m.info(23), m.info(24)), (4, 320000, 75000, 90000))
        m.choose(OPT_COUNTER, 3)
        self.assertEqual((m.info(20), m.info(22), m.info(23), m.info(24), m.info(4)), (3, 590000, 408000, 423000, 0))

    def test_targets_draw_on_the_effect_link_over_everything(self):
        m = self.start()
        self.assertEqual((m.info(25), m.info(26), m.info(27)), (1, MATCHGX_EFFECT, 255))

    def test_only_the_legal_stages_play_and_any_other_restarts_on_battlefield(self):
        for stage_id in LEGAL_IDS + (GRKINDEXT_CASTLE, GRKINDEXT_KONGO):
            legal = stage_id in LEGAL_IDS
            with self.subTest(stage=stage_id):
                m = TargetsMachine(FOX, self.fighters[FOX].attr)
                m.stage(stage_id, [(0, -80, 0, 80, 0, 0)])
                m.init()
                m.you()
                for _ in range(3): m.think()
                restarts, stage = m.m.call('TestTargetsRestart', 0), signed(m.m.call('TestTargetsRestart', 1))
                if legal:
                    self.assertEqual((restarts, len(m.targets())), (0, 1))
                else:
                    # Once, before any target, and nothing else happens while it goes.
                    self.assertEqual((restarts, stage, m.targets(), m.info(9)), (1, GRKINDEXT_BATTLE, [], 1))

    def test_change_character_offers_fox_and_falco_and_restarts_with_the_pick(self):
        for kind in (FOX, FALCO):
            with self.subTest(fighter=NAMES[kind]):
                m = self.start(kind)
                swap = lambda key: signed(m.m.call('TestTargetsSwap', key))
                # Its own submenu with the player's row only: there's no CPU to change.
                self.assertEqual((swap(0), swap(1), swap(2)), (1, 1, 1))
                self.assertEqual((swap(3), swap(4), swap(5)), (2, CKIND_FOX, CKIND_FALCO))
                self.assertEqual((swap(6), swap(7), swap(8)), (int(kind == FALCO), 0, 1))
                text = bytes(m.m.cpu.mem_read(m.m.call('TestTargetsSwapText', 2), 80)).split(b'\0', 1)[0].decode()
                self.assertIn('Fox or Falco', text)
                self.assertNotIn('both', text)
                m.m.call('TestTargetsSwapPress', 0)  # A with nothing picked: no restart.
                self.assertEqual((swap(9), swap(13)), (0, 4))
                other = FALCO if kind == FOX else FOX
                m.m.call('TestTargetsSwapPick', int(other == FALCO))
                m.m.call('TestTargetsSwapPress', 1)  # X: the next costume.
                self.assertEqual(swap(14), 1)
                m.m.call('TestTargetsSwapPress', 0)
                self.assertEqual((swap(9), swap(10), swap(11), swap(12), swap(13)),
                                 (1, CKIND_FALCO if other == FALCO else CKIND_FOX, 1, -1, 1))


if __name__ == '__main__':
    unittest.main()
