"""Execute the production Edgeguard knockback and To Main Stage logic."""
import math
from pathlib import Path
import re
import subprocess
import unittest
import test_settings as fixtures
from test_lowlaser import load_probe

ROOT = Path(__file__).resolve().parents[1]
RANDOM, VERY_HIGH, HIGH, LEVEL, LOW, VERY_LOW, CUSTOM = range(7)
HEIGHTS = {VERY_HIGH: (51, 61), HIGH: (23, 33), LEVEL: (-5, 5), LOW: (-33, -23), VERY_LOW: (-61, -51)}
CLOSE, NEAR, MEDIUM, FAR = 1, 2, 3, 4
DISTANCES = {CLOSE: (20, 30), NEAR: (40, 50), MEDIUM: (60, 70), FAR: (80, 90)}
KB_HEIGHT, KB_DISTANCE = 7, 25  # TM_EG_KB_HEIGHT, TM_EG_KB_DISTANCE
MAG, ANGLE, DISTANCE, PERCENT = range(4)
LAUNCH_OFF_LEDGE = -2
# Gravity and terminal velocity of every Edgeguard CPU, from the ISO.
CPUS = {
    'Fox': (.23, 2.8), 'Falco': (.17, 3.1), 'Sheik': (.12, 2.13), 'Zelda': (.073, 1.4),
    'Falcon': (.13, 2.9), 'Marth': (.085, 2.2), 'Yoshi': (.093, 1.93), 'Peach': (.08, 1.5),
    'Luigi': (.069, 1.6), 'Dr. Mario': (.095, 1.7), 'Popo': (.1, 1.6), 'Puff': (.064, 1.3),
    'Samus': (.066, 1.4),
}
SPEED, RATE, DECAY = .03, .4, .051
DASH = 3 * 78 / 4 * .96  # Fox's full side-B dash


def build():
    subprocess.run([
        'C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe', '-O2', '-w',
        '-ffreestanding', '-fno-builtin', '-msdata=none', '-mcpu=750', '-nostdlib',
        '-Wl,-Ttext=0x101000,-e,TestEdgeguardInit,--unresolved-symbols=ignore-all',
        'tests/edgeguard_probe.c', '-o', 'build/edgeguard-probe.elf'
    ], cwd=ROOT, check=True)


def milli(value):
    return round(value * 1000)


def replay(gravity, terminal, inland, mag, angle):
    """Hitstun frame by frame, like Fighter_procUpdate. (out, height, lowest over the stage)."""
    speed = mag * SPEED
    kb_x, kb_y = speed * math.cos(math.radians(angle)), speed * math.sin(math.radians(angle))
    fall = 0.0
    out, height, lowest = -inland, 0.0, math.inf
    for _ in range(int(RATE * mag)):
        fall = min(fall + gravity, terminal)
        size = math.hypot(kb_x, kb_y)
        if size < DECAY:
            kb_x = kb_y = 0.0
        else:
            kb_x -= DECAY * kb_x / size
            kb_y -= DECAY * kb_y / size
        out += kb_x
        height += kb_y - fall
        if out <= 0:
            lowest = min(lowest, height)
    return out, height, lowest


class KnockbackTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def setUp(self):
        self.m = fixtures.Machine()
        load_probe(self.m, 'edgeguard-probe.elf', 'TestEdgeguardInit')

    def info(self, key):
        value = self.m.call('TestLaunchInfo', key)
        return (value if value < 0x80000000 else value - 0x100000000) / 1000

    def launch(self, name, height, distance, inland, rand, ledge_y=0.0, choice=0):
        gravity, terminal = CPUS[name]
        self.m.call('TestCPU', milli(gravity), milli(terminal))
        self.m.call('TestLedgeHeight', milli(ledge_y))
        self.m.call('TestRand', milli(rand), choice)
        self.m.call('TestRows', height, distance)
        self.m.call('TestLaunch', 1, inland)
        return self.info(MAG), self.info(ANGLE), self.info(DISTANCE), self.info(PERCENT)

    def test_every_cpu_ends_hitstun_on_the_chosen_spot_from_either_launch_spot(self):
        for name, (gravity, terminal) in CPUS.items():
            for height, ups in HEIGHTS.items():
                for distance, outs in DISTANCES.items():
                    for inland in (15, 5):
                        for rand in (0, .999):
                            mag, angle, start, percent = self.launch(name, height, distance, inland, rand)
                            spot = (name, height, distance, inland, rand)
                            self.assertIn(start, (inland, LAUNCH_OFF_LEDGE), spot)
                            out, up, lowest = replay(gravity, terminal, start, mag, angle)
                            self.assertAlmostEqual(out, outs[0] + (outs[1] - outs[0]) * rand, delta=2, msg=spot)
                            self.assertAlmostEqual(up, ups[0] + (ups[1] - ups[0]) * rand, delta=2, msg=spot)
                            if start > 0:
                                self.assertGreaterEqual(lowest, 1, spot)
                            self.assertTrue(40 < mag < 230, spot)
                            self.assertTrue(50 <= percent < 80, spot)

    def test_only_a_launch_that_would_touch_the_stage_moves_off_the_ledge(self):
        self.assertEqual(self.launch('Fox', LOW, NEAR, 15, .5)[2], 15)
        self.assertEqual(self.launch('Puff', HIGH, NEAR, 15, .5)[2], 15)
        mag, angle, start, _ = self.launch('Puff', VERY_LOW, NEAR, 15, .5)
        self.assertEqual(start, LAUNCH_OFF_LEDGE)
        self.assertLess(angle, 0)  # Puff falls too little in hitstun to end low any other way.
        self.assertEqual(self.launch('Fox', VERY_LOW, CLOSE, 15, .5)[2], LAUNCH_OFF_LEDGE)

    def test_the_spot_is_relative_to_the_ledge_on_every_stage(self):
        gravity, terminal = CPUS['Fox']
        for ledge_y in (-3.5, 0, .6):
            mag, angle, start, _ = self.launch('Fox', LEVEL, NEAR, 15, .5, ledge_y)
            self.assertAlmostEqual(replay(gravity, terminal, start, mag, angle)[1], ledge_y, delta=1.5)

    def test_random_rows_pick_a_row_each_reset_and_keep_the_other_axis(self):
        gravity, terminal = CPUS['Fox']
        for choice, height in enumerate(HEIGHTS):  # Random Height, fixed Distance.
            mag, angle, start, _ = self.launch('Fox', RANDOM, MEDIUM, 15, .5, choice=choice)
            out, up, _ = replay(gravity, terminal, start, mag, angle)
            self.assertAlmostEqual(out, 65, delta=1.5)
            self.assertAlmostEqual(up, sum(HEIGHTS[height]) / 2, delta=1.5)
        for choice, distance in enumerate(DISTANCES):  # Random Distance, fixed Height.
            mag, angle, start, _ = self.launch('Fox', HIGH, RANDOM, 15, .5, choice=choice)
            out, up, _ = replay(gravity, terminal, start, mag, angle)
            self.assertAlmostEqual(up, 28, delta=1.5)
            self.assertAlmostEqual(out, sum(DISTANCES[distance]) / 2, delta=1.5)

    def test_custom_uses_its_own_ranges_from_the_usual_spot(self):
        for row, value in enumerate((100, 120, 45, 60, 10, 30)):
            self.m.call('TestCustom', row, value)
        self.m.call('TestRand', 500, 7)
        self.m.call('TestRows', CUSTOM, NEAR)
        self.m.call('TestLaunch', 1, 15)
        self.assertEqual((self.info(MAG), self.info(ANGLE), self.info(DISTANCE), self.info(PERCENT)),
                         (110, 52.5, 15, 17))

    def test_saves_from_before_these_rows_keep_their_hit_strength(self):
        for hit_strength, expected in ((0, (LOW, CLOSE)), (1, (LOW, NEAR)), (2, (HIGH, MEDIUM)),
                                       (3, (VERY_LOW, FAR)), (4, (CUSTOM, NEAR))):
            self.m.call('TestPreference', KB_HEIGHT, hit_strength)
            self.m.call('TestPreference', KB_DISTANCE, 0)
            self.m.call('TestLoadKnockback')
            self.assertEqual((self.m.call('TestRow', 0), self.m.call('TestRow', 1)), expected)
            self.assertEqual(self.m.call('TestRow', 2), int(hit_strength == 4))

    def test_every_choice_saves_and_loads_including_random(self):
        for height in range(7):
            for distance in range(5):
                self.m.call('TestRows', height, distance)
                self.m.call('TestChangeHeight', height)
                self.assertEqual(self.m.call('TestReadPreference', KB_HEIGHT), height)
                self.assertEqual(self.m.call('TestReadPreference', KB_DISTANCE), distance + 1)
                self.m.call('TestRows', 0, 0)
                self.m.call('TestLoadKnockback')
                self.assertEqual((self.m.call('TestRow', 0), self.m.call('TestRow', 1)), (height, distance))

    def test_distance_is_greyed_out_only_while_custom(self):
        self.m.call('TestChangeHeight', CUSTOM)
        self.assertEqual(self.m.call('TestRow', 2), 1)
        self.m.call('TestChangeHeight', LEVEL)
        self.assertEqual(self.m.call('TestRow', 2), 0)
        self.m.call('TestChangeCustom')  # Editing a custom value selects Custom, as Hit Strength did.
        self.assertEqual((self.m.call('TestRow', 0), self.m.call('TestRow', 2)), (CUSTOM, 1))

    def test_menu_defaults_match_a_save_with_no_choice_yet(self):
        header = (ROOT / 'src' / 'edgeguard.h').read_text()
        self.assertIn('.val = KB_HEIGHT_LOW,', header)
        self.assertIn('.val = KB_DISTANCE_NEAR,', header)
        for name in ('Hit Strength', 'HitStrength'):
            self.assertNotIn(name, header)
        table = re.search(r'Values_KnockbackHeight\[\] = \{(.*?)\};', header).group(1)
        self.assertEqual(re.findall(r'"([^"]+)"', table),
                         ['Random', 'Very High', 'High', 'Level', 'Low', 'Very Low', 'Custom'])
        table = re.search(r'Values_KnockbackDistance\[\] = \{(.*?)\};', header).group(1)
        self.assertEqual(re.findall(r'"([^"]+)"', table), ['Random', 'Close', 'Near', 'Medium', 'Far'])


class ToMainStageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def setUp(self):
        self.m = fixtures.Machine()
        load_probe(self.m, 'edgeguard-probe.elf', 'TestEdgeguardInit')
        self.m.call('TestRand', 500, 0)

    def signed(self, value):
        return value if value < 0x80000000 else value - 0x100000000

    def think(self, out, h, vy=-2.0, kbx=0.0, kby=0.0):
        return self.m.call('TestStageThink', milli(out), milli(h), milli(vy), milli(kbx), milli(kby))

    def dash_height(self, out, h, kbx=0.0, kby=0.0):
        return self.signed(self.m.call('TestDashHeight', milli(out), milli(h), milli(kbx), milli(kby))) / 1000

    def plan(self, key, out=500, h=0, hmn_grounded=0):
        return self.m.call('TestStagePlan', key, milli(out), milli(h), hmn_grounded) / 1000

    def test_the_plan_targets_right_above_the_ledge(self):
        self.assertEqual((self.plan(0), self.plan(1)), (1, 5))

    def test_it_can_aim_past_a_full_dash_but_only_a_little(self):
        self.assertAlmostEqual(self.plan(2), DASH + 6, delta=.01)

    def test_a_player_at_the_ledge_gets_slashed_when_the_cpu_can_get_there(self):
        self.m.call('TestRand', 999, 0)  # The far end of "near where the CPU is".
        self.assertAlmostEqual(self.plan(2, out=52, h=-20), 60, delta=.05)
        self.assertAlmostEqual(self.plan(2, out=52, h=-20, hmn_grounded=1), DASH - 6, delta=.01)

    def test_a_player_at_the_ledge_never_costs_the_cpu_the_main_stage(self):
        self.assertAlmostEqual(self.plan(2, hmn_grounded=1), DASH + 6, delta=.01)

    def test_side_b_right_above_the_ledge_goes(self):
        self.assertEqual(self.think(40, 3), 1)

    def test_side_b_above_the_band_waits(self):
        self.assertEqual(self.think(40, 8), 0)

    def test_side_b_below_the_ledge_never_goes(self):
        self.assertEqual(self.think(40, -3), 0)

    def test_leftover_knockback_that_would_lift_the_dash_waits(self):
        # Before: this press height was in the band, and the dash ran near side platform height.
        self.assertGreater(self.dash_height(40, 3, kby=1.2), 15)
        self.assertEqual(self.think(40, 3, kby=1.2), 0)

    def test_leftover_knockback_that_lifts_a_low_press_into_the_band_goes(self):
        self.assertAlmostEqual(self.dash_height(40, -6, kby=1.0), 3.14, delta=.2)
        self.assertEqual(self.think(40, -6, kby=1.0), 1)

    def test_a_dash_just_short_of_the_ledge_still_lands_on_the_stage(self):
        self.assertEqual(self.think(61, 3), 1)

    def test_too_far_out_to_land_waits(self):
        self.assertEqual(self.think(90, 3), 0)

    def test_carry_counts_only_rising_knockback(self):
        self.assertEqual(self.m.call('TestCarryUp', 0, milli(-1)), 0)
        self.assertAlmostEqual(self.m.call('TestCarryUp', 0, milli(1)) / 1000, 1 / (2 * DECAY), delta=.05)


if __name__ == '__main__':
    unittest.main()
