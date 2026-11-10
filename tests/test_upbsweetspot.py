"""Execute the production Up-B Sweetspot event's Marth and up-B line, frame by frame."""
import importlib.util
import math
from pathlib import Path
import struct
import subprocess
import unittest
from unicorn import UC_HOOK_MEM_WRITE
import test_settings as fixtures
from test_lowlaser import load_probe

ROOT = Path(__file__).resolve().parents[1]
# The Gecko codes' generator, whose shapes the event's guides must match.
_spec = importlib.util.spec_from_file_location('upb_overlay_numbers', ROOT / 'scripts/upb_overlay_numbers.py')
numbers = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(numbers)
DPAD_LEFT = 1
FSMASH_FRAME, DTILT_FRAME = 13, 7  # MARTH_FSMASH_FRAME, MARTH_DTILT_FRAME
FSMASH, DTILT, OFF = 0, 1, 2  # TMUpBMarth
FOX_UP_B = 354
FTKIND_FOX, FTKIND_FALCON, FTKIND_SHEIK, FTKIND_FALCO = 1, 2, 7, 22
FALCON_UP_B = 354
FALCON_PATH = 37.6, 11.9  # UpB_Info's rise and forward.
FALL_SPECIAL = 35
GX_PIPE = 0xCC008000
YELLOW = (0xff, 0xe0, 0x00, 0xff)
BLUE = (0x30, 0xc8, 0xff, 0xff)
DIM_BLUE = (0x1c, 0x6c, 0x94, 0xff)
WHITE = (0xff, 0xff, 0xff, 0xff)


def build():
    subprocess.run([
        'C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe', '-O2', '-w',
        '-ffreestanding', '-fno-builtin', '-msdata=none', '-mcpu=750', '-nostdlib',
        '-Wl,-Ttext=0x101000,-e,TestUpBLoaded,--unresolved-symbols=ignore-all',
        'tests/upbsweetspot_probe.c', '-o', 'build/upbsweetspot-probe.elf'
    ], cwd=ROOT, check=True)


class MarthTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def start(self, marth=FSMASH):
        self.m = fixtures.Machine()
        load_probe(self.m, 'upbsweetspot-probe.elf', 'TestUpBLoaded')
        self.m.call('TestUpBInit', marth)

    def info(self, key):
        value = self.m.call('TestUpBInfo', key)
        return value - (1 << 32) if value >> 31 else value
    def step(self, frames=1): self.m.call('TestUpBThink', frames)
    def swings(self): return self.info(0)
    def freezes(self): return self.info(1)
    def swinging(self): return bool(self.info(2))
    def on_platform(self): return bool(self.info(4))
    def dtilts(self): return self.info(8)

    def test_marth_swings_as_the_attempt_starts_with_you_still_on_the_platform(self):
        self.start()
        self.step()
        self.assertTrue(self.on_platform())
        self.assertEqual(self.swings(), 1)
        self.assertTrue(self.swinging())

    def test_he_freezes_with_the_hitboxes_out_before_you_leave_the_platform(self):
        self.start()
        self.step(FSMASH_FRAME + 2)
        self.assertTrue(self.on_platform())
        self.assertEqual((self.freezes(), self.info(3)), (1, 0))
        self.assertEqual(self.info(5), 1)

    def test_he_freezes_on_the_last_frame_before_the_hitboxes_clear(self):
        self.start()
        self.step(FSMASH_FRAME + 2)
        self.assertEqual((self.info(9), self.info(5)), (FSMASH_FRAME, 1))

    def test_the_tipper_is_moved_over_the_ledge_on_that_freeze(self):
        self.start()
        self.step(FSMASH_FRAME + 2)
        self.assertEqual(self.info(6), -850)  # Over the left ledge, at -85.

    def test_the_hitboxes_just_linger_while_you_wait(self):
        self.start()
        self.step(FSMASH_FRAME + 2)
        self.step(200)
        self.assertEqual((self.swings(), self.freezes(), self.info(3), self.info(5)), (1, 1, 0, 1))

    def test_your_up_b_doesnt_start_anything(self):
        self.start()
        self.step(FSMASH_FRAME + 2)
        self.m.call('TestUpBStart', FOX_UP_B)
        self.step(5)
        self.assertEqual((self.swings(), self.freezes()), (1, 1))

    def test_a_reset_swings_again(self):
        self.start()
        self.step(FSMASH_FRAME + 2)
        self.m.call('TestUpBDown', DPAD_LEFT)
        self.step()
        self.m.call('TestUpBDown', 0)
        self.assertEqual(self.swings(), 2)
        self.assertEqual(self.info(3), 100)  # The savestate let go of the freeze.
        self.step(FSMASH_FRAME + 2)
        self.assertEqual(self.freezes(), 2)

    def test_a_finished_attempt_lets_the_swing_end(self):
        self.start()
        self.step(FSMASH_FRAME + 2)
        self.m.call('TestUpBFakeGrab')
        self.step()
        self.assertEqual(self.info(3), 100)

    def test_marth_off_leaves_him_standing(self):
        self.start(marth=OFF)
        self.step(FSMASH_FRAME + 2)
        self.assertEqual((self.swings(), self.dtilts(), self.freezes()), (0, 0, 0))
        self.assertFalse(self.swinging())

    def test_the_freeze_holds_if_anything_starts_him_moving_again(self):
        self.start()
        self.step(FSMASH_FRAME + 2)
        self.m.call('TestUpBRate', 100)
        self.step()
        self.assertEqual(self.info(3), 0)

    def test_down_tilt_swings_as_the_attempt_starts_instead_of_the_forward_smash(self):
        self.start(marth=DTILT)
        self.step()
        self.assertEqual((self.dtilts(), self.swings()), (1, 0))

    def test_down_tilt_freezes_on_its_first_active_frame(self):
        self.start(marth=DTILT)
        self.step(DTILT_FRAME - 1)
        self.assertEqual(self.freezes(), 0)
        self.step(3)
        self.assertEqual((self.freezes(), self.info(3), self.info(5)), (1, 0, 1))

    def test_it_stands_where_side_b_has_it_so_the_tip_reaches_past_the_ledge(self):
        self.start(marth=DTILT)
        self.step(DTILT_FRAME + 2)
        self.assertEqual(self.info(6), -880)  # 15 in from the ledge at -85, and 18 of reach.

    def test_down_tilt_stays_frozen_like_the_forward_smash(self):
        self.start(marth=DTILT)
        self.step(DTILT_FRAME + 2)
        self.m.call('TestUpBRate', 100)
        self.step(200)
        self.assertEqual((self.freezes(), self.info(3), self.info(5)), (2, 0, 1))

    def test_a_finished_attempt_lets_the_down_tilt_end(self):
        self.start(marth=DTILT)
        self.step(DTILT_FRAME + 2)
        self.m.call('TestUpBFakeGrab')
        self.step(3)
        self.assertEqual(self.info(3), 100)

    def test_changing_the_attack_waits_for_the_next_reset(self):
        self.start()
        self.step(2)
        self.m.call('TestUpBAttack', DTILT)
        self.step(FSMASH_FRAME)
        self.assertEqual((self.dtilts(), self.freezes(), self.info(3)), (0, 1, 0))  # Still the forward smash.
        self.m.call('TestUpBDown', DPAD_LEFT)
        self.step()
        self.m.call('TestUpBDown', 0)
        self.assertEqual((self.swings(), self.dtilts()), (1, 1))


class UpBLineTests(unittest.TestCase):
    """The yellow line, read from the vertices the event sends to the GX pipe."""
    START = (-120, -40)

    @classmethod
    def setUpClass(cls): build()

    def setUp(self):
        self.m = fixtures.Machine()
        load_probe(self.m, 'upbsweetspot-probe.elf', 'TestUpBLoaded')
        self.m.cpu.mem_map(GX_PIPE & ~0xFFF, 0x1000)
        self.writes = []
        self.m.cpu.hook_add(UC_HOOK_MEM_WRITE, self.pipe_write, begin=GX_PIPE, end=GX_PIPE + 3)
        self.m.call('TestUpBInit', OFF)
        self.step()
        self.m.call('TestUpBStart', FALCON_UP_B)
        self.m.call('TestUpBKind', FTKIND_FALCON)
        self.move(*self.START)

    def pipe_write(self, cpu, access, address, size, value, data):
        self.writes.append((size, value & ((1 << size * 8) - 1)))

    def step(self, frames=1): self.m.call('TestUpBThink', frames)

    def move(self, x, y):
        self.m.call('TestUpBMove', x, y)
        self.step()

    def drawn(self, wanted):
        """Every vertex of one drawn frame in this color, in order, as (x, y)."""
        self.writes.clear()
        self.m.call('TestUpBDraw')
        vertices = []
        for i in range(0, len(self.writes), 7):
            x, y, z, *color = self.writes[i:i + 7]
            if tuple(part[1] for part in color) == wanted:
                vertices.append(tuple(round(struct.unpack('>f', struct.pack('>I', v[1]))[0], 3)
                                      for v in (x, y)))
        return vertices

    def yellow(self): return self.drawn(YELLOW)

    def cross(self, at):
        x, y, size = at[0], at[1], 2.5
        return [(x - size, y - size), (x + size, y + size), (x - size, y + size), (x + size, y - size)]

    def assert_line(self, x, y):
        """The line from the start through (x, y), as long as the whole path."""
        vertices = self.yellow()
        self.assertEqual(len(vertices), 7)
        start, end, dot = vertices[:3]
        self.assertEqual((start, dot), (self.START, end))
        out_x, out_y = x - self.START[0], y - self.START[1]
        scale = math.hypot(*FALCON_PATH) / math.hypot(out_x, out_y)
        self.assertAlmostEqual(end[0], self.START[0] + out_x * scale, places=2)
        self.assertAlmostEqual(end[1], self.START[1] + out_y * scale, places=2)
        self.assertEqual(vertices[3:], self.cross(self.START))

    def test_only_the_start_shows_until_the_up_b_has_risen(self):
        self.assertEqual(self.yellow(), self.cross(self.START))
        self.move(-121, -48)  # Mario and Peach drop before they rise.
        self.assertEqual(self.yellow(), self.cross(self.START))
        self.move(-110, -35)  # Not far enough yet for a frame of drift not to matter.
        self.assertEqual(self.yellow(), self.cross(self.START))

    def test_the_line_shows_the_angle_it_set_out_at(self):
        self.move(-112, -20)
        self.assert_line(-112, -20)

    def test_the_angle_doesnt_move_for_the_rest_of_the_up_b(self):
        self.move(-112, -20)
        for x, y in ((-100, -5), (-130, 0), (-90, 10), (-125, -30)):
            self.move(x, y)
            self.assert_line(-112, -20)

    def test_a_later_up_b_in_the_same_attempt_ignored_after_the_ledge_grab(self):
        self.move(-112, -20)
        self.m.call('TestUpBFakeGrab')
        self.step()
        self.m.call('TestUpBState', FALCON_UP_B)
        self.move(-100, 30)
        self.assert_line(-112, -20)

    def test_a_reset_clears_it(self):
        self.move(-100, -5)
        self.m.call('TestUpBDown', DPAD_LEFT)
        self.step()
        self.m.call('TestUpBDown', 0)
        self.assertEqual(self.yellow(), [])

    def test_game_and_watch_gets_a_place_to_up_b_from(self):
        self.m.call('TestUpBGameAndWatch')
        # The X sits at (-85, 0) in this fixture: no ECB or ledge box.
        fast, slow, dot = self.drawn(BLUE)[:3]
        self.assertEqual(fast, dot)
        self.assertEqual(fast, (-94.1, -75.6))
        self.assertEqual(slow, (-96.9, -65.8))

    def test_game_and_watch_shows_the_full_stick_angle_too(self):
        self.m.call('TestUpBGameAndWatch')
        fast, slow, dot = self.drawn(DIM_BLUE)[:3]
        self.assertEqual(fast, dot)
        # 22.5 degrees past the path: 0.75 of the 30 per unit of stick past 0.25.
        self.assertAlmostEqual(fast[0], -85 - (9.1 * 0.92388 + 75.6 * 0.38268), 1)
        self.assertAlmostEqual(fast[1], -(75.6 * 0.92388 - 9.1 * 0.38268), 1)

    def assert_at(self, vertices, points):
        self.assertEqual(len(vertices), len(points))
        for vertex, point in zip(vertices, points):
            self.assertAlmostEqual(vertex[0], point[0], 1)
            self.assertAlmostEqual(vertex[1], point[1], 1)

    def on_top(self): return self.m.call('TestUpBOnTopVertices')

    # The probe's aimed fixtures (TestUpBAimed): the X at (-85, 0) on the left
    # ledge, Firefox's 81.6 for any stick past 0.5 of |x| + |y|, and Sheik's
    # 20 * (tilt + 1) past a 0.5 tilt, with PlCo.dat's 0.28 deadzones.
    FIXTURES = {FTKIND_FOX: ('fox', lambda tilt: 81.6), FTKIND_FALCO: ('falco', lambda tilt: 81.6),
                FTKIND_SHEIK: ('sheik', lambda tilt: 20 * (tilt + 1))}

    def generated(self, kind, b0xx):
        """What the Gecko generator draws for the fixture: points by color, in order."""
        slug, distance = self.FIXTURES[kind]
        n = {'target': (0, 0), 'distance': distance, 'threshold': .5, 'stick': numbers.stick(.28, .28)}
        drawn = {}
        for _, _, color, points, _ in numbers.aimed_shapes(slug, n, b0xx):
            drawn.setdefault(tuple(color.to_bytes(4, 'big')), []).extend((-85 - x, y) for x, y in points)
        return drawn

    def assert_dot(self, vertices, point):
        self.assertLess(min(math.hypot(x - point[0], y - point[1]) for x, y in vertices), .05)

    def at_angle(self, degrees, radius):
        """A point on a ring around the X, by ring angle: 90 above it, 0 out past the ledge."""
        a = math.radians(degrees)
        return (-85 - math.cos(a) * radius, math.sin(a) * radius)

    def test_fox_and_falcos_ring_leaves_out_what_the_stick_cant_aim(self):
        gap = math.degrees(math.atan2(23, 76))  # The least tilted axis, with the most the circle allows.
        for kind in FTKIND_FOX, FTKIND_FALCO:
            with self.subTest(kind=kind):
                self.m.call('TestUpBAimed', kind)
                ring = self.drawn(BLUE)
                # Three stretches of 81.6 around the X: out and up, out and down, and in under
                # the stage to 30 degrees short of level, each ending 16.84 short of straight.
                self.assertEqual(len(ring), 16 + 16 + 12 + 3 * 3)
                self.assert_at([ring[0], ring[15], ring[16], ring[31], ring[32], ring[43]],
                               [self.at_angle(a, 81.6) for a in (90 - gap, gap, -gap, gap - 90, -90 - gap, -150)])
                # Then a bit of ring through each straight mark: 2 degrees either side.
                self.assert_at(ring[44:], [self.at_angle(straight + 2 - 2 * i, 81.6)
                                           for straight in (90, 0, -90) for i in range(3)])
                self.assert_at(ring, self.generated(kind, False)[BLUE])
                # After the ledge lines: a tick across the ring above, level with and below the X.
                ticks = self.drawn(WHITE)[4:10]
                self.assert_at(ticks, [(-85, 75.6), (-85, 87.6), (-160.6, 0), (-172.6, 0), (-85, -75.6), (-85, -87.6)])
                self.assert_at(ticks, self.generated(kind, False)[WHITE])
                self.assertEqual(self.drawn(DIM_BLUE), [])
                self.assertEqual(self.on_top(), 53 + 6)

    def test_fox_aim_lines_also_draw_over_the_stage(self):
        self.m.call('TestUpBAimed', FTKIND_FOX)
        self.step()  # Up-B state 354 is his charge.
        self.drawn(BLUE)
        self.assertEqual(self.on_top(), 53 + 6 + 2 + 2 + 1)  # The ring, ticks, both lines and the dot.

    def test_sheiks_band_follows_the_least_tilt_that_aims_and_stays_behind_the_stage(self):
        self.m.call('TestUpBAimed', FTKIND_SHEIK)
        full, least, generated = self.drawn(BLUE), self.drawn(DIM_BLUE), self.generated(FTKIND_SHEIK, False)
        self.assertEqual((len(full), len(least)), (16 + 16 + 3 * 3, 31 + 31 + 3 * 3))
        # A bit of each through the ends of the straight marks: the blue at 40, the dim at 30.
        self.assert_at(full[32:35], [self.at_angle(92 - 2 * i, 40) for i in range(3)])
        self.assert_at(least[62:65], [self.at_angle(92 - 2 * i, 30) for i in range(3)])
        self.assert_at(full, generated[BLUE])
        self.assert_at(least, generated[DIM_BLUE])
        # The least-tilt edge starts at the ring, 23/80 across needing nearly a full
        # tilt so close to straight, and is the 0.5 threshold's 30 halfway round.
        self.assert_at([least[0], least[15], least[30]],
                       [self.at_angle(73.16, 20 * (1 + .2875 / math.cos(math.radians(73.16)))),
                        self.at_angle(45, 30), self.at_angle(16.84, 20 * (1 + .2875 / math.sin(math.radians(16.84))))])
        # Straight up, down and across, any tilt past the threshold works: from 30 out to 40.
        marks = self.drawn(WHITE)[4:10]
        self.assert_at(marks, [(-85, 30), (-85, 40), (-115, 0), (-125, 0), (-85, -30), (-85, -40)])
        self.assert_at(marks, generated[WHITE])
        self.assertEqual(self.on_top(), 0)

    def test_b0xx_angles_draw_a_dot_for_each_of_foxs_instead_of_the_ring(self):
        self.m.call('TestUpBAimed', FTKIND_FOX)
        self.m.call('TestUpBB0XX', 1)
        dots, generated = self.drawn(BLUE), self.generated(FTKIND_FOX, True)
        # Mod X and Mod Y, with C-buttons or not, up and down toward the stage and up
        # away from it under the stage, as far as its 30 degrees short of level.
        self.assertEqual(len(dots), 10 + 10 + 8)
        self.assert_at(dots, generated[BLUE])
        # Mod X up and toward the stage, with B: (73, 31), 23 degrees, from below and out.
        self.assert_dot(dots, (-85 - 81.6 * 73 / math.hypot(73, 31), -81.6 * 31 / math.hypot(73, 31)))
        # No modifier: straight up, down and across, and the diagonals.
        self.assert_at(self.drawn(WHITE)[4:10], generated[WHITE])
        self.assertEqual(len(generated[WHITE]), 6)
        self.assertEqual(self.drawn(DIM_BLUE), [])  # B moves Fox's under half a degree.
        self.assertEqual(self.on_top(), 28 + 6)

    def test_b0xx_angles_for_sheik_also_show_the_shorter_spots_without_b(self):
        self.m.call('TestUpBAimed', FTKIND_SHEIK)
        self.m.call('TestUpBB0XX', 1)
        generated = self.generated(FTKIND_SHEIK, True)
        blue, dim, white = self.drawn(BLUE), self.drawn(DIM_BLUE), self.drawn(WHITE)[4:-4]
        self.assertEqual((len(blue), len(dim), len(white)), (25, 16, 5))
        self.assert_at(blue, generated[BLUE])
        self.assert_at(dim, generated[DIM_BLUE])
        self.assert_at(white, generated[WHITE])
        # Mod X up toward the stage: 59, 25 without B is 0.80 of a tilt, so 36 away.
        tilt = math.hypot(59, 25) / 80
        self.assert_dot(dim, (-85 - 20 * (tilt + 1) * 59 / math.hypot(59, 25), -20 * (tilt + 1) * 25 / math.hypot(59, 25)))
        self.assertEqual(self.on_top(), 0)

    def test_b0xx_angles_keep_the_live_lines(self):
        self.m.call('TestUpBAimed', FTKIND_FOX)
        self.m.call('TestUpBB0XX', 1)
        self.step()
        self.drawn(BLUE)
        self.assertEqual(self.on_top(), 28 + 6 + 2 + 2 + 1)

    def test_b0xx_angles_leave_the_paths_alone(self):
        self.m.call('TestUpBGameAndWatch')
        before = self.drawn(BLUE)
        self.m.call('TestUpBB0XX', 1)
        self.assertEqual(self.drawn(BLUE), before)

    def test_paths_still_draw_behind_the_stage(self):
        self.move(-100, -5)
        self.drawn(YELLOW)
        self.assertEqual(self.on_top(), 0)


if __name__ == '__main__':
    unittest.main(verbosity=2)
