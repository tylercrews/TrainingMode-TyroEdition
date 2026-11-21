"""Execute the production Target Practice event against the ISO's fighters and stages."""
from collections import Counter
from pathlib import Path
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
OPT_FREEPRACTICE, OPT_SHDL, OPT_LINES, OPT_COUNTER, OPT_DEFAULTS = 3, 4, 5, 6, 7
FIFO, GX_LINES = 0xCC008000, 0xA8
# Hop Peak Lines: teal for a short hop, purple for a full hop, premultiplied.
TEAL, PURPLE = (0, 162, 144, 230), (144, 90, 230, 230)
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


def short_hop(attr, frames=64):
    """Height after each airborne frame of a short hop: gravity, then the move."""
    y, v = [0.0], attr['hop_v']
    for _ in range(1, frames):
        v = max(v - attr['gravity'], -attr['terminal'])
        y.append(y[-1] + v)
    return y


def signed(value): return value - (1 << 32) if value >= 1 << 31 else value


class TargetsMachine:
    def __init__(self, kind, attr, free_practice=0, shdl=0, best=0, counter=0, seed=12345):
        self.m = fixtures.Machine()
        load_probe(self.m, 'targets-probe.elf', 'TestTargetsLoaded')
        self.m.call('TestTargetsFighter', kind, *[round(attr[k] * 10000) for k in
                                                  ('jump_v', 'hop_v', 'air_jump_mult', 'gravity', 'terminal')])
        self.m.call('TestTargetsSeed', seed)
        self.lines = []

    def init(self, free_practice=0, shdl=0, best=0, counter=0, lines=0):
        self.m.call('TestTargetsInit', free_practice, shdl, best, counter, lines)

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
                for i in range(2) if self.m.call('TestTargetsTarget', i, 0)]
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
                c, y = fighter.constants(), short_hop(fighter.attr)
                fits = [press for press in range(1, 30)
                        if press + c['delay'] + c['loop'] < 64 and y[press + c['delay'] + c['loop'] - 1] + c['ecb_bottom'] > 0]
                self.assertEqual(m.info(12), len(fits))
                self.assertEqual(fits, [1] if kind == FOX else [])
                for press in range(1, 4):
                    first = press + c['delay']
                    self.assertEqual(m.m.call('TestTargetsShots', press, 0), int(press in fits))
                    if press in fits:
                        self.assertAlmostEqual(m.m.call('TestTargetsShots', press, 1) / 10000, y[first - 1] + c['air'], delta=.001)
                        self.assertAlmostEqual(m.m.call('TestTargetsShots', press, 2) / 10000,
                                               y[first + c['loop'] - 1] + c['air'], delta=.001)
        # Fox's pair: the first shot at the short hop's peak, the second just before landing.
        c, attr = self.fighters[FOX].constants(), self.fighters[FOX].attr
        self.assertAlmostEqual(short_hop(attr)[9], laser_heights.rise(attr['hop_v'], attr['gravity']), delta=.001)
        m = self.machine(FOX)
        self.assertAlmostEqual(m.m.call('TestTargetsShots', 1, 1) / 10000, 17.83, delta=.01)
        self.assertAlmostEqual(m.m.call('TestTargetsShots', 1, 2) / 10000, 5.48, delta=.01)
        self.assertGreaterEqual(m.m.call('TestTargetsShots', 1, 2), m.m.call('TestTargetsBlaster', 0, 5))

    def test_full_hop_and_double_jump_from_live_attributes(self):
        for kind, fighter in self.fighters.items():
            with self.subTest(fighter=NAMES[kind]):
                a = fighter.attr
                expected = laser_heights.rise(a['jump_v'], a['gravity']) + laser_heights.rise(a['jump_v'] * a['air_jump_mult'], a['gravity'])
                self.assertAlmostEqual(self.machine(kind).info(32) / 10000, expected, delta=.002)
        self.assertAlmostEqual(self.machine(FOX).info(32) / 10000, 67.804, delta=.002)
        self.assertAlmostEqual(self.machine(FALCO).info(32) / 10000, 89.178, delta=.002)


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

    def run_targets(self, kind, filename, stage_id, normal, count, free_practice=0, shdl=0, you=(0, 0)):
        m = TargetsMachine(kind, self.fighters[kind].attr)
        m.stage(stage_id, self.floors[filename], normal)
        m.init(free_practice, shdl)
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
        c, y = self.fighters[FOX].constants(), short_hop(self.fighters[FOX].attr)
        return y[c['delay']] + c['air'], y[c['delay'] + c['loop']] + c['air']

    def floor_kind(self, m, x, surface):
        """The floor a pair is over, at least INSET from its ends: main, left, right or top."""
        ground = [l for l in m.lines if not l[4]]
        left, right = min(l[0] for l in ground), max(l[2] for l in ground)
        if abs(surface - m.ground_y(x)) < .003 and left + INSET - .002 <= x <= right - INSET + .002:
            return 'main'
        for ax, ay, bx, by, platform in m.lines:
            if platform and ax + INSET - .002 <= x <= bx - INSET + .002 and \
                    abs(ay + (by - ay) * (x - ax) / (bx - ax) - surface) < .003:
                return 'top' if ax < 0 < bx else 'left' if bx < 0 else 'right'
        self.fail(f'no floor under the pair at ({x}, {surface})')

    def test_fox_pairs_come_off_each_kind_of_floor_the_stage_has_evenly(self):
        high, low = self.pair_heights()
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
                kinds = Counter()
                for (ux, uy), (lx, ly) in pairs:
                    self.assertEqual(ux, lx)
                    surface = ly - low
                    self.assertAlmostEqual(uy - surface, high, delta=.003)
                    kinds[self.floor_kind(m, ux, surface)] += 1
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
        high, low = self.pair_heights()
        # On Battlefield's top platform, every spot over it is within FROM_YOU of you.
        m, spawns = self.run_targets(FOX, 'GrNBa.dat', GRKINDEXT_BATTLE, None, 120, free_practice=1, shdl=1,
                                     you=(0, 54.4))
        kinds = Counter()
        for spawn in spawns:
            for x, y in spawn:
                self.assertGreaterEqual((x ** 2 + (y - 54.4) ** 2) ** .5, FROM_YOU - .01)
            if len(spawn) == 2:
                kinds[self.floor_kind(m, spawn[1][0], spawn[1][1] - low)] += 1
        self.assertEqual(set(kinds), {'main', 'left', 'right'}, kinds)

    def test_no_pairs_for_falco_timed_runs_or_with_the_setting_off(self):
        for kind, free_practice, shdl in ((FALCO, 1, 1), (FOX, 0, 1), (FOX, 1, 0)):
            with self.subTest(fighter=NAMES[kind], free_practice=free_practice, shdl=shdl):
                _, spawns = self.run_targets(kind, 'GrNBa.dat', 31, None, 40, free_practice, shdl)
                self.assertTrue(all(len(s) == 1 for s in spawns))


BLAST = (-224, 224)  # The probe's blast zones.


def peak_floors(lines):
    """(left x, right x, y) of each floor's lines, in drawing order: the main floor at
    the middle, then FindPlatform's left, top and right platforms, a side one only on
    its side of x = 0, and none closer than 1 to the main floor."""
    main = max(ay + (by - ay) * -ax / (bx - ax) for ax, ay, bx, by, platform in lines
               if not platform and ax <= 0 <= bx)
    platforms = [l for l in lines if l[4] and l[2] - l[0] >= 10]
    center = lambda l: (l[0] + l[2]) / 2
    height = lambda l: l[1] + (l[3] - l[1]) * (center(l) - l[0]) / (l[2] - l[0])
    floors = [(BLAST[0], BLAST[1], main)]
    for side in (-1, 0, 1):
        if side:
            found = [l for l in platforms if center(l) * side > 10]
            pick = min(found, key=lambda l: abs(center(l)), default=None)
        else:
            pick = max((l for l in platforms if l[0] <= 0 <= l[2]), key=height, default=None)
        if pick and height(pick) >= main + 1:
            floors.append((0 if side > 0 else BLAST[0], 0 if side < 0 else BLAST[1], height(pick)))
    return floors


class TargetPeakLineTests(unittest.TestCase):
    """Hop Peak Lines: a short hop's and a full hop's peak laser over each floor."""
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
        """A short hop's and a full hop's peak laser above the floor, from the ISO."""
        a, c = self.fighters[kind].attr, self.fighters[kind].constants()
        return [laser_heights.rise(a[v], a['gravity']) + c['air'] for v in ('hop_v', 'jump_v')]

    def lines(self, vertices):
        """Each GX_LINES pair as (left x, right x, y, color)."""
        for a, b in zip(vertices[::2], vertices[1::2]):
            self.assertEqual((a[1], a[2], a[3]), (b[1], b[2], b[3]))
        return [(a[0], b[0], a[1], a[3]) for a, b in zip(vertices[::2], vertices[1::2])]

    def assertLines(self, got, floors, kind):
        sh, fh = self.peaks(kind)
        wanted = [(left, right, y + h, color) for left, right, y in floors for h, color in ((sh, TEAL), (fh, PURPLE))]
        self.assertEqual(len(got), len(wanted))
        for g, w in zip(got, wanted):
            self.assertAlmostEqual(g[0], w[0], delta=.001)
            self.assertAlmostEqual(g[1], w[1], delta=.001)
            self.assertAlmostEqual(g[2], w[2], delta=.002)
            self.assertEqual(g[3], w[3])

    def test_heights_are_a_short_hops_and_a_full_hops_peak_laser(self):
        # By hand: Fox's short hop rises 8.55 and his full hop 27.6, and his aerial laser
        # is 9.28 over TopN; Falco's rise 9.68 and 47.4, with his laser 3.80 up.
        for kind, by_hand in ((FOX, (17.833, 36.883)), (FALCO, (13.478, 51.198))):
            with self.subTest(fighter=NAMES[kind]):
                m = self.start(kind)
                got = [m.m.call('TestTargetsPeak', hop) / 10000 for hop in (0, 1)]
                for g, wanted, hand in zip(got, self.peaks(kind), by_hand):
                    self.assertAlmostEqual(g, wanted, delta=.0005)
                    self.assertAlmostEqual(g, hand, delta=.001)
        # A short hop's peak laser is SH Double Lasers' first shot.
        m = self.start(FOX)
        self.assertAlmostEqual(m.m.call('TestTargetsPeak', 0), m.m.call('TestTargetsShots', 1, 1), delta=2)

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
        floors = [(-224, 224, 0.0088), (-224, 0, 30.1421), (-224, 224, 51.4253), (0, 224, 30.2425)]
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
                                 [(-224, 224, main), (-224, 0, left), (-224, 224, 42.75), (0, 224, right)], FOX)
        # Hidden, the game leaves the left one 0.375 up, inside the stage: its lines go.
        m.m.call('TestTargetsMoveLine', 0, 375, 375)
        batches, vertices = m.draw()
        self.assertEqual(batches, [(GX_LINES, 12, 12, 1)])
        self.assertLines(self.lines(vertices), [(-224, 224, main), (-224, 224, 42.75), (0, 224, 22.125)], FOX)

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
        m = self.start(free_practice=1, shdl=1, best=17, counter=3, lines=1)
        self.assertEqual(m.info(36), 1)
        m.m.call('TestTargetsSelect', OPT_DEFAULTS)
        self.assertEqual((m.info(2), m.info(3), m.info(1), m.info(20), m.info(4)), (0, 0, 17, 3, 1))
        self.assertEqual((m.info(37), m.info(36)), (0, 0))  # Hop Peak Lines goes Off too.

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
