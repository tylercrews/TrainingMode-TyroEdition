"""Execute the production Edgeguard knockback, recovery and Up-B Overlay logic."""
import importlib.util
import math
from pathlib import Path
import re
import struct
import unittest
import recovery_sim as sim
import test_settings as fixtures
from test_lowlaser import load_probe
from unicorn import UC_HOOK_MEM_WRITE

ROOT = Path(__file__).resolve().parents[1]
# The Gecko codes' generator, whose shapes the Up-B Overlays must match.
_spec = importlib.util.spec_from_file_location('upb_overlay_numbers', ROOT / 'scripts/upb_overlay_numbers.py')
numbers = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(numbers)
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


build = sim.build


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


SIDE = 22  # TM_EG_SIDE
SAVED_RANDOM, SAVED_LEFT, SAVED_RIGHT = range(3)  # TM_EG_SIDE_*, the save's order
ROW_LEFT, ROW_RIGHT, ROW_RANDOM = range(3)  # the menu's order
DPAD_LEFT, DPAD_RIGHT, DPAD_DOWN, DPAD_UP = 1, 2, 4, 8


class SideTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def setUp(self):
        self.m = fixtures.Machine()
        load_probe(self.m, 'edgeguard-probe.elf', 'TestEdgeguardInit')

    def test_the_menu_lists_left_right_random_and_defaults_to_random(self):
        header = (ROOT / 'src' / 'edgeguard.h').read_text()
        table = re.search(r'Values_Side\[\] = \{(.*?)\};', header).group(1)
        self.assertEqual(re.findall(r'"([^"]+)"', table), ['Left', 'Right', 'Random'])
        self.assertIn('.val = EG_SIDE_RANDOM,', header)

    def test_saved_sides_keep_their_meaning(self):
        # 0 is a save that never set Side, and still reads as Random.
        for saved, row in ((SAVED_RANDOM, ROW_RANDOM), (SAVED_LEFT, ROW_LEFT), (SAVED_RIGHT, ROW_RIGHT)):
            with self.subTest(saved=saved):
                self.m.call('TestPreference', SIDE, saved)
                self.m.call('TestLoadSettings')
                self.assertEqual(self.m.call('TestSideRow'), row)
                self.m.call('TestPreference', SIDE, 3)
                self.m.call('TestChangeSide', row)
                self.assertEqual(self.m.call('TestReadPreference', SIDE), saved)

    def test_left_and_right_always_use_their_ledge(self):
        for rand in (0, 1):
            self.m.call('TestRand', 0, rand)
            for row, ledge in ((ROW_LEFT, 0), (ROW_RIGHT, 1), (ROW_RANDOM, rand)):
                self.m.call('TestChangeSide', row)
                self.assertEqual(self.m.call('TestChooseSide'), ledge, (row, rand))

    def reset_timer(self):
        value = self.m.call('TestResetTimer')
        return value if value < 0x80000000 else value - 0x100000000

    def test_the_dpad_picks_a_side_saves_it_and_restarts(self):
        self.m.call('TestChangeSide', ROW_RANDOM)
        # The last press picks the side already chosen, which only restarts.
        for button, row, saved in ((DPAD_RIGHT, ROW_RIGHT, SAVED_RIGHT), (DPAD_LEFT, ROW_LEFT, SAVED_LEFT),
                                   (DPAD_LEFT, ROW_LEFT, SAVED_LEFT)):
            self.m.call('TestClearReset')
            self.m.call('TestDPad', button)
            self.assertEqual(self.m.call('TestSideRow'), row)
            self.assertEqual(self.m.call('TestReadPreference', SIDE), saved)
            self.assertEqual(self.reset_timer(), 0)

    def test_up_and_down_leave_the_side_alone(self):
        self.m.call('TestChangeSide', ROW_RANDOM)
        for button in (0, DPAD_DOWN, DPAD_UP):
            self.m.call('TestDPad', button)
            self.assertEqual(self.m.call('TestSideRow'), ROW_RANDOM)
            self.assertEqual(self.reset_timer(), -1)


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


# Row bits, as in recovery.c's enums.
FF_LOW, FF_LEDGE, SIDEB_SWEETSPOT, SIDEB_TO_LEDGE = 1, 1 << 3, 1 << 7, 1 << 8
LG_JUMP, LG_CYCLONE, LG_MISSILE, LG_SWEETSPOT, LG_BACKFLIP, LG_FAST = 1, 2, 4, 1 << 4, 1 << 5, 1 << 6
LG_DEFAULTS = LG_JUMP | LG_CYCLONE | LG_MISSILE
MISS, GRAB_STARTUP, GRAB_DASH, GRAB_ENDLAG = 0, 2, 3, 4
SPOTS = [(height, distance) for height in sim.HEIGHTS for distance in sim.DISTANCES]


class SideBRulesTests(unittest.TestCase):
    """The side-B simulation against the rules in mpcoll.c, on real stage lines."""
    @classmethod
    def setUpClass(cls): build()

    def setUp(self):
        self.m = fixtures.Machine()
        load_probe(self.m, 'edgeguard-probe.elf', 'TestEdgeguardInit')

    def stage(self, name):
        sim.Sim(self.m, 'Fox', name)

    def side_b(self, out, h):
        value = self.m.call('TestSideBResult', milli(out), milli(h), 0)
        value = value if value < 0x80000000 else value - 0x100000000
        return (value // 100, value % 100)

    def inset(self, depth):
        value = self.m.call('TestWallInset', milli(depth))
        return (value if value < 0x80000000 else value - 0x100000000) / 1000

    def test_the_stages_side_is_read_from_its_lines(self):
        self.stage('Battlefield')
        self.assertAlmostEqual(self.inset(-3), 1.71, delta=.01)
        self.assertGreater(self.inset(-10), 13)  # Under the lip: only the underside, far inland.
        self.stage('Final Destination')
        self.assertAlmostEqual(self.inset(-8), 0, delta=.01)

    def test_a_level_dash_never_grabs(self):
        # Before: a dash reaching the box counted as a grab on that step, but a
        # frame only looks for a ledge if it moved down.
        self.stage('Final Destination')
        for out in (20, 30, 45, 60, 75):
            for h in (-8, -9, -10, -12):
                self.assertNotEqual(self.side_b(out, h)[0], GRAB_DASH, (out, h))

    def test_the_first_grab_out_of_a_dash_is_the_endlags_first_fall(self):
        self.stage('Final Destination')
        self.assertEqual(self.side_b(45, -9), (GRAB_ENDLAG, 6))

    def test_a_dash_into_a_tall_wall_stops_and_grabs_from_low(self):
        self.stage('Final Destination')
        self.assertEqual(self.side_b(45, -15)[0], GRAB_ENDLAG)

    def test_the_same_dash_goes_under_battlefields_lip(self):
        # Two steps reach the ledge, and the third goes on with no wall to stop it.
        # This is the suicide: the plan used to call step two a grab.
        self.stage('Battlefield')
        self.assertEqual(self.side_b(45, -15)[0], MISS)

    def test_a_dash_at_the_top_of_battlefields_wall_stops_there(self):
        self.stage('Battlefield')
        self.assertEqual(self.side_b(45, -9)[0], GRAB_ENDLAG)

    def test_low_on_battlefield_only_an_exact_dash_works(self):
        self.stage('Battlefield')
        self.assertEqual(self.side_b(sim.FIGHTERS['Fox']['step'] * 3 + 18, -13)[0], GRAB_ENDLAG)

    def test_firefox_travel_is_the_moves_real_distance(self):
        sim.Sim(self.m, 'Fox', 'Battlefield')
        self.assertAlmostEqual(self.m.call('TestFirefoxTravel') / 1000, 81.5, delta=.01)
        sim.Sim(self.m, 'Falco', 'Battlefield')
        self.assertAlmostEqual(self.m.call('TestFirefoxTravel') / 1000, 60.1, delta=.2)


class ClosedLoopTests(unittest.TestCase):
    """Production Recovery_Think, frame by frame, against tests/recovery_sim.py.

    Fewer attempts than the tables in the notes: `python tests/recovery_sim.py`."""
    @classmethod
    def setUpClass(cls):
        build()
        cls.m = sim.machine()

    def attempts(self, name, stage, rows, samples=2, spots=SPOTS, side=1):
        recovery = sim.Sim(self.m, name, stage, side)
        for height, distance in spots:
            for i in range(samples):
                recovery.launch(height, distance, (i + .5) / samples)
                self.m.call('TestSeed', 1 + i * 7919)
                recovery.grab_state = None
                result = recovery.run(rows)
                yield (height, distance, i), result, recovery

    def test_ledge_sweetspot_never_goes_under_the_stage(self):
        for name in ('Fox', 'Falco'):
            for stage in ('Battlefield', 'Pokemon Stadium', "Yoshi's Story"):
                for spot, result, cpu in self.attempts(name, stage, SIDEB_SWEETSPOT):
                    self.assertIn(result, ('ledge', 'stage'), (name, stage, spot))

    def test_ledge_sweetspot_is_high_enough_for_the_wall_or_an_exact_dash(self):
        dashes = 0
        for spot, result, cpu in self.attempts('Fox', 'Battlefield', SIDEB_SWEETSPOT):
            if cpu.dash_h is not None:
                dashes += 1
                self.assertGreaterEqual(cpu.dash_h, -13.6, spot)
                self.assertEqual(result, 'ledge', spot)
        self.assertGreater(dashes, 20)

    def test_it_works_from_the_left_ledge_too(self):
        for spot, result, cpu in self.attempts('Fox', 'Battlefield', SIDEB_SWEETSPOT, side=-1):
            self.assertIn(result, ('ledge', 'stage'), spot)

    def test_up_b_to_ledge_grabs_the_ledge(self):
        for name in ('Fox', 'Falco'):
            for stage in ('Battlefield', 'Final Destination', "Yoshi's Story"):
                for spot, result, cpu in self.attempts(name, stage, FF_LEDGE):
                    self.assertEqual(result, 'ledge', (name, stage, spot))

    def launch_spots(self, rows):
        return [(cpu.log[0][2], cpu.log[0][3]) for _, _, cpu in self.attempts('Fox', 'Battlefield', rows) if cpu.log]

    def test_up_b_to_ledge_starts_as_soon_as_it_can(self):
        # Before: it fell to the one distance where the travel ends at the ledge.
        cpu = sim.Sim(self.m, 'Fox', 'Final Destination')
        cpu.launch('Low', 'Near')
        self.m.call('TestSeed', 1)
        hitstun = cpu.hitstun
        for _ in range(hitstun + 9):  # Hitlag, hitstun, and a frame or two.
            cpu.step(FF_LEDGE)
        self.assertTrue(cpu.log, 'no up-B yet')
        self.assertGreater(cpu.log[0][3], -40)

    def test_up_b_to_ledge_comes_from_all_over(self):
        spots = self.launch_spots(FF_LEDGE)
        self.assertGreater(max(x for x, _ in spots) - min(x for x, _ in spots), 50)
        self.assertGreater(max(y for _, y in spots) - min(y for _, y in spots), 70)

    def test_up_b_to_ledge_from_over_the_ledge_grabs_in_flight(self):
        grabs = sum(cpu.grab_state == sim.FOX_FF_TRAVEL for _, _, cpu in
                    self.attempts('Fox', 'Final Destination', FF_LEDGE, spots=[('High', 'Near'), ('High', 'Medium'), ('Very High', 'Medium')]))
        self.assertGreater(grabs, 3)

    def test_up_b_to_ledge_double_jumps_on_some_attempts_with_the_row_on(self):
        cpu = sim.Sim(self.m, 'Fox', 'Battlefield')
        jumped = 0
        for seed in range(1, 25):
            cpu.launch('Low', 'Near', seed / 25)
            self.m.call('TestSeed', seed)
            self.assertEqual(cpu.run(FF_LEDGE | 1 << 4), 'ledge', seed)
            jumped += cpu.jump_h is not None
        self.assertTrue(4 < jumped < 20, jumped)

    def test_a_side_b_that_cant_make_it_goes_for_the_ledge_with_the_up_b(self):
        # Before: a double jump and a side-B from under the ledge, which only SDs.
        for spot, result, cpu in self.attempts('Fox', 'Battlefield', SIDEB_SWEETSPOT, spots=[('Very Low', 'Near')]):
            self.assertEqual(result, 'ledge', spot)

    def test_luigi_mashes_a_cyclone_before_the_up_b(self):
        cyclones = 0
        for spot, result, cpu in self.attempts('Luigi', 'Battlefield', LG_DEFAULTS, samples=3):
            if cpu.cyclone_start is not None:
                cyclones += 1
                self.assertGreater(cpu.cyclone_top - cpu.cyclone_start, 20, spot)
        self.assertGreater(cyclones, 30)  # Of 60. Before: about 1 in 10, and it gained about 1.

    def test_fast_mashing_gains_more(self):
        def mean_gain(rows):
            gains = [cpu.cyclone_top - cpu.cyclone_start for _, _, cpu in self.attempts('Luigi', 'Battlefield', rows)
                     if cpu.cyclone_start is not None]
            return sum(gains) / len(gains)
        self.assertGreater(mean_gain(LG_DEFAULTS | LG_FAST), mean_gain(LG_DEFAULTS) + 5)

    def test_luigi_recovers_from_nearly_everywhere(self):
        attempts = list(self.attempts('Luigi', 'Battlefield', LG_DEFAULTS, samples=3))
        died = [spot for spot, result, _ in attempts if result not in ('ledge', 'stage')]
        self.assertLessEqual(len(died), 3, died)  # Before: half of them.

    def test_up_b_sweetspot_grabs_coming_down_from_its_top(self):
        grabs = 0
        for spot, result, cpu in self.attempts('Luigi', 'Battlefield', LG_DEFAULTS | LG_SWEETSPOT):
            if cpu.grab_state == sim.LG_UPB:
                grabs += 1
                self.assertLessEqual(cpu.upb_h, -50, spot)  # So low only the top reaches.
                self.assertLessEqual(cpu.grab_frame, 36, spot)
        self.assertGreater(grabs, 20)

    def test_the_plain_up_b_ends_around_the_ledge(self):
        for spot, result, cpu in self.attempts('Luigi', 'Battlefield', LG_JUMP, spots=[('Low', 'Close'), ('Very Low', 'Close')]):
            self.assertEqual(result, 'ledge', spot)
            if cpu.upb_h is not None:  # A fall past the ledge grabs it too.
                self.assertLessEqual(cpu.upb_h + 41.8, 8.1, spot)

    def test_backflip_jump_sweetspot_is_a_backflip_with_no_up_b(self):
        flips = 0
        for spot, result, cpu in self.attempts('Luigi', 'Battlefield', LG_BACKFLIP | LG_DEFAULTS):
            if cpu.backflipped and result == 'ledge' and cpu.upb_h is None:
                flips += 1
                self.assertIn(cpu.grab_state, (cpu.states[sim.JUMP_B], cpu.states[sim.FALL]), spot)
        self.assertGreater(flips, 10)


class LuigiCycloneFlagTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def setUp(self):
        self.m = fixtures.Machine()
        load_probe(self.m, 'edgeguard-probe.elf', 'TestEdgeguardInit')

    def test_any_nonzero_flag_is_cleared_for_luigi(self):
        self.m.call('TestSimConfigure', self.m.call('TestState', sim.KIND_LUIGI), 0)
        self.m.call('TestRefreshCyclone')
        self.assertEqual(self.m.call('TestCycloneFlag'), 0)

    def test_every_attempt_starts_with_the_lift(self):
        # The flag's only cleared by a Cyclone touching down. A CPU never does
        # that, and a savestate brings back whatever the match started with.
        self.m.call('TestSimConfigure', self.m.call('TestState', sim.KIND_LUIGI), 0)
        self.assertEqual(self.m.call('TestResetCpuClears', 1), 0)

    def test_another_fighters_var_is_left_alone(self):
        self.m.call('TestSimConfigure', self.m.call('TestState', sim.KIND_FOX), 0)
        self.assertEqual(self.m.call('TestResetCpuClears', 1), 1)


class RecoveryRowSaveTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def setUp(self):
        self.m = fixtures.Machine()
        load_probe(self.m, 'edgeguard-probe.elf', 'TestEdgeguardInit')

    def test_every_row_saves_and_loads(self):
        for menu, count in ((0, 12), (1, 12), (2, 7)):
            for row in range(count):
                self.m.call('TestSaveRows', menu, 1 << row)
                self.assertEqual(self.m.call('TestLoadRows', menu), 1 << row, (menu, row))
            self.m.call('TestSaveRows', menu, (1 << count) - 1)
            self.assertEqual(self.m.call('TestLoadRows', menu), (1 << count) - 1, menu)

    def test_old_saves_keep_their_rows_where_they_were(self):
        LUIGI, FOX = 15, 8  # TM_EG_RECOVERY_LUIGI, TM_EG_RECOVERY_FOX
        self.m.call('TestPreference', LUIGI, 0b10111)  # Double Jump to Green Missile, and Fast Mashing.
        self.assertEqual(self.m.call('TestLoadRows', 2), 0b1000111)
        self.m.call('TestPreference', FOX, 1 | 1 << 3 | 1 << 4 | 1 << 5)  # Low, Jump, Instant, Fast Fall.
        self.assertEqual(self.m.call('TestLoadRows', 0), 1 | 1 << 4 | 1 << 5 | 1 << 6)


class UpBOverlayTests(unittest.TestCase):
    """The Player and CPU Up-B Overlays: Up-B Sweetspot's guides, as the Slippi
    Up-B Overlay codes draw them, at both ledges."""
    GX_PIPE = 0xCC008000
    PLAYER, CPU = 0, 1
    MARIO, FOX, FALCON, SHEIK, POPO, YOSHI, GAW = 0, 1, 2, 7, 10, 14, 24
    # Ledge-grab boxes in tenths (snap x, y, height), from the ISO.
    BOXES = {FOX: (110, 130, 90), SHEIK: (110, 150, 110), FALCON: (90, 170, 110), MARIO: (120, 150, 100)}
    SLUGS = {FOX: 'fox', SHEIK: 'sheik', FALCON: 'falcon', MARIO: 'mario'}
    YOURS = {numbers.BLUE: (0x30, 0xc8, 0xff, 0xff), numbers.DIM_BLUE: (0x1c, 0x6c, 0x94, 0xff),
             numbers.WHITE: (0xff, 0xff, 0xff, 0xff)}
    CPUS = {numbers.BLUE: (0xff, 0x96, 0x32, 0xff), numbers.DIM_BLUE: (0x94, 0x57, 0x1d, 0xff),
            numbers.WHITE: (0xff, 0xd8, 0xb0, 0xff)}
    # The bar and the X are symmetric, so they're drawn the same way round off
    # either ledge, not mirrored point by point.
    SYMMETRIC = ('the snap: coming down, the up-B grabs here', 'the target X')

    @classmethod
    def setUpClass(cls): build()

    def setUp(self):
        self.m = fixtures.Machine()
        load_probe(self.m, 'edgeguard-probe.elf', 'TestEdgeguardInit')
        self.writes = []
        self.m.cpu.mem_map(self.GX_PIPE & ~0xFFF, 0x1000)
        self.m.cpu.hook_add(UC_HOOK_MEM_WRITE, self.pipe_write, begin=self.GX_PIPE, end=self.GX_PIPE + 3)
        self.m.call('TestOverlayInit')

    def pipe_write(self, cpu, access, address, size, value, data):
        self.writes.append((size, value & ((1 << size * 8) - 1)))

    def fighter(self, ply, kind):
        self.m.call('TestOverlayFighter', ply, kind, *self.BOXES.get(kind, (110, 130, 90)))

    def drawn(self, pass_=2):
        """Every vertex of one drawn frame, in order, as (x, y, color)."""
        self.writes.clear()
        self.m.call('TestOverlayDraw', pass_)
        vertices = []
        for i in range(0, len(self.writes), 7):
            x, y, z, *color = self.writes[i:i + 7]
            x, y = (struct.unpack('>f', struct.pack('>I', v[1]))[0] for v in (x, y))
            vertices.append((x, y, tuple(part[1] for part in color)))
        return vertices

    def expected(self, kind, colors):
        """What the Gecko generator draws for the probe's fighter, off the left
        ledge (mirrored) and then the right, as [(points, color, symmetric)]."""
        slug, (width, y_offset, height) = self.SLUGS[kind], (v / 10 for v in self.BOXES[kind])
        ecb = next(c[6] for c in numbers.CHARACTERS if c[0] == slug)
        target = ((ecb + width) / 2, -y_offset)
        n = {'target': target, 'snap': (target[0], target[1] + height / 2)}
        if kind in (self.FOX, self.SHEIK):
            distance = (lambda tilt: 81.6) if kind == self.FOX else (lambda tilt: 20 * (tilt + 1))
            n.update(distance=distance, threshold=.5, stick=numbers.stick(.28, .28), rings=[distance(1)])
        else:
            rise, forward, end_rise, end_forward, tilt = numbers.PATHS[slug]
            n.update(path=(rise, forward, end_rise, end_forward), tilt=18.0 if tilt else 0.0)
        shapes = []
        for ledge, out in ((-85, -1), (85, 1)):
            for _, _, color, points, comment in numbers.shapes(slug, n):
                shapes.append(([(ledge + out * x, y) for x, y in points], colors[color], comment in self.SYMMETRIC))
        return shapes

    def assert_draws(self, drawn, *fighters):
        shapes = [shape for kind, colors in fighters for shape in self.expected(kind, colors)]
        self.assertEqual(len(drawn), sum(len(points) for points, _, _ in shapes))
        at = 0
        for points, color, symmetric in shapes:
            got = drawn[at:at + len(points)]
            at += len(points)
            self.assertEqual({c for _, _, c in got}, {color})
            got = [(round(x, 2), round(y, 2)) for x, y, _ in got]
            if symmetric:
                got, points = sorted(got), sorted((round(x, 2), round(y, 2)) for x, y in points)
            for (x, y), point in zip(got, points):
                self.assertAlmostEqual(x, point[0], 1)
                self.assertAlmostEqual(y, point[1], 1)

    def test_both_rows_are_off_until_saved_on(self):
        self.m.call('TestLoadSettings')
        self.assertEqual([self.m.call('TestOverlayRow', row, key) for row in (0, 1) for key in (0, 1)], [0] * 4)
        for row in (0, 1):
            self.m.call('TestPreference', self.m.call('TestOverlayPreference', row), 1)
        self.m.call('TestLoadSettings')
        self.assertEqual([self.m.call('TestOverlayRow', row, key) for row in (0, 1) for key in (0, 1)], [1] * 4)

    def test_each_row_saves_its_own_setting(self):
        player, cpu = (self.m.call('TestOverlayPreference', row) for row in (0, 1))
        self.m.call('TestChangeOverlay', 1, 1)
        self.assertEqual((self.m.call('TestReadPreference', player), self.m.call('TestReadPreference', cpu)), (0, 1))
        self.m.call('TestChangeOverlay', 0, 1)
        self.m.call('TestChangeOverlay', 1, 0)
        self.assertEqual((self.m.call('TestReadPreference', player), self.m.call('TestReadPreference', cpu)), (1, 0))

    def test_nothing_draws_with_both_rows_off_or_on_other_passes(self):
        self.fighter(self.PLAYER, self.FOX)
        self.fighter(self.CPU, self.FOX)
        self.assertEqual(self.drawn(), [])
        self.m.call('TestOverlayRows', 1, 1)
        self.assertEqual(self.drawn(0) + self.drawn(1), [])
        self.assertTrue(self.drawn(2))

    def test_your_fox_gets_the_gecko_codes_ring_at_both_ledges(self):
        self.fighter(self.PLAYER, self.FOX)
        self.fighter(self.CPU, self.FALCON)
        self.m.call('TestOverlayRows', 1, 0)
        drawn = self.drawn()
        self.assert_draws(drawn, (self.FOX, self.YOURS))
        # The 2/3 ring and its ticks draw over the stage, off both ledges.
        self.assertEqual(self.m.call('TestOverlayOnTop'), 2 * (53 + 6))
        # The X is half of the standing ECB (2) plus the box's reach (11) out, 13 down.
        crosses = [v[:2] for v in drawn if v[2] == self.YOURS[numbers.WHITE]][-4:]
        self.assertAlmostEqual(sum(x for x, _ in crosses) / 4, 85 + 6.5, 3)
        self.assertAlmostEqual(sum(y for _, y in crosses) / 4, -13, 3)

    def test_the_cpus_is_orange_and_draws_before_yours(self):
        self.fighter(self.PLAYER, self.FOX)
        self.fighter(self.CPU, self.SHEIK)
        self.m.call('TestOverlayRows', 1, 1)
        self.assert_draws(self.drawn(), (self.SHEIK, self.CPUS), (self.FOX, self.YOURS))
        self.m.call('TestOverlayRows', 0, 1)
        self.assert_draws(self.drawn(), (self.SHEIK, self.CPUS))
        self.assertEqual(self.m.call('TestOverlayOnTop'), 0)  # Sheik's half ring stays behind the stage.

    def test_paths_start_from_the_standing_ecbs_x(self):
        self.fighter(self.CPU, self.FALCON)  # Falcon's ECB is 4.07 out, no stick angle.
        self.m.call('TestOverlayRows', 0, 1)
        self.assert_draws(self.drawn(), (self.FALCON, self.CPUS))
        self.fighter(self.PLAYER, self.MARIO)  # Mario's also has the dim path for holding toward the stage.
        self.m.call('TestOverlayRows', 1, 0)
        self.assert_draws(self.drawn(), (self.MARIO, self.YOURS))
        self.assertEqual(self.m.call('TestOverlayOnTop'), 0)

    def test_fighters_with_no_guide_draw_nothing_and_grey_out_their_row(self):
        for player, cpu in ((self.YOSHI, self.POPO), (self.FOX, self.YOSHI), (self.POPO, self.FALCON)):
            with self.subTest(player=player, cpu=cpu):
                self.fighter(self.PLAYER, player)
                self.fighter(self.CPU, cpu)
                self.m.call('TestOverlayDisable')
                self.assertEqual((self.m.call('TestOverlayRow', 0, 2), self.m.call('TestOverlayRow', 1, 2)),
                                 (player in (self.YOSHI, self.POPO), cpu in (self.YOSHI, self.POPO)))
        self.fighter(self.PLAYER, self.YOSHI)
        self.fighter(self.CPU, self.POPO)
        self.m.call('TestOverlayRows', 1, 1)
        self.assertEqual(self.drawn(), [])

    def test_the_standing_ecbs_are_the_gecko_codes(self):
        for slug, _, kind, *_, ecb in numbers.CHARACTERS:
            self.assertEqual(self.m.call('TestStandingFront', kind), round(ecb * 100), slug)
        self.assertEqual(self.m.call('TestStandingFront', self.GAW), 218)  # Posed the same way, Wait1.
        self.assertEqual(self.m.call('TestStandingFront', self.POPO), 200)  # The game's minimum.


if __name__ == '__main__':
    unittest.main()
