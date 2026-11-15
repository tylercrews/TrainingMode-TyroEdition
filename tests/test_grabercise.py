"""Run the production Grabs-ercise event against tests/grabercise_probe.c.

The probe's engine follows the decomp's Wait, Walk, Turn, Dash, Run, TurnRun,
Guard and roll rules (see src/grabercise.c), so these check that the CPU's
inputs walk, run, dash and roll at full speed the way the game would take
them, and turn back before the end of their floor. None of this is Dolphin.
"""
from pathlib import Path
import subprocess
import unittest
import test_settings as fixtures
from test_lowlaser import load_probe

ROOT = Path(__file__).resolve().parents[1]
BATTLEFIELD, FINAL_DESTINATION = 0, 1
FOX, MARTH, GAME_AND_WATCH, LUIGI = range(4)
STAND, WALK, RUN, DASH, DASH_FULL, DASH_HALF, ROLL = range(7)
# Battlefield's lines in the probe: the ground's halves, a wall, then the
# left, right and top platforms.
MAIN_LEFT, MAIN_RIGHT, WALL, LEFT_PLATFORM, RIGHT_PLATFORM, TOP_PLATFORM = range(6)
GROUND = (MAIN_LEFT, MAIN_RIGHT)
FLOORS = {
    MAIN_LEFT: (-68.4, 68.4, 0), MAIN_RIGHT: (-68.4, 68.4, 0),
    LEFT_PLATFORM: (-57.6, -20, 27.2), RIGHT_PLATFORM: (20, 57.6, 27.2), TOP_PLATFORM: (-18.8, 18.8, 54.4),
}
FD_EDGE = 85.6
INSET, FROM_YOU, FROM_LAST = 8, 30, 25
NEAR_END = 4  # A moving CPU's feet get this close to each end of its floor before it turns.
GRAB_WINDOW = 30  # Frames a hit CPU stays, after the hitlag, for a follow-up grab.
# Menu rows and saved preferences.
(OPT_RETRY, OPT_STAGE, OPT_CHARACTERS, OPT_FREEPRACTICE, OPT_STANDING, OPT_WALK, OPT_RUN, OPT_DASH,
 OPT_DASH_FULL, OPT_DASH_HALF, OPT_ROLL, OPT_COLLISION, OPT_COUNTER, OPT_DEFAULTS, OPT_EXIT) = range(15)
(BEST, FREE_PRACTICE, STANDING, PREF_WALK, PREF_RUN, PREF_DASH, PREF_DASH_FULL, PREF_DASH_HALF, PREF_ROLL,
 COLLISION) = range(10)
STAND_STILL, STAND_SHIELD, STAND_RANDOM = range(3)  # Standing CPU's choices.
MOVE_ROWS = (OPT_WALK, OPT_RUN, OPT_DASH, OPT_DASH_FULL, OPT_DASH_HALF, OPT_ROLL)
MOVE_PREFS = (PREF_WALK, PREF_RUN, PREF_DASH, PREF_DASH_FULL, PREF_DASH_HALF, PREF_ROLL)
# States, from the MexTK enum.
ASID_WAIT, ASID_FALL = 14, 29
ASID_CAPTUREPULLEDHI, ASID_CAPTUREWAITHI, ASID_THROWNF = 223, 224, 239
ASID_DAMAGEN1, ASID_DAMAGEN2, ASID_DAMAGEFLYN, ASID_DOWNBOUNDU = 78, 79, 88, 183
ASID_CAPTURECAPTAIN, ASID_SLEEP = 275, 11
ASID_GUARDON, ASID_GUARD, ASID_GUARDSETOFF = 178, 179, 181
PAD_TRIGGER_R = 0x20


def build():
    subprocess.run([
        'C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe', '-O2', '-w',
        '-ffreestanding', '-fno-builtin', '-msdata=none', '-mcpu=750', '-nostdlib',
        '-Wl,-Ttext=0x101000,-e,TestGrabsLoaded,--unresolved-symbols=ignore-all',
        'tests/grabercise_probe.c', '-o', 'build/grabercise-probe.elf'
    ], cwd=ROOT, check=True)


def signed(value): return value - (1 << 32) if value >> 31 else value


class Probe:
    def __init__(self, stage=BATTLEFIELD, character=FOX, signed_timer=0, prefs=None, follower=False):
        self.m = fixtures.Machine()
        load_probe(self.m, 'grabercise-probe.elf', 'TestGrabsLoaded')
        defaults = {BEST: 0, FREE_PRACTICE: 0, STANDING: STAND_RANDOM, COLLISION: 0, **{pref: 1 for pref in MOVE_PREFS}}
        defaults.update(prefs or {})
        for index, value in defaults.items():
            self.m.call('TestGrabsPref', index, value)
        self.m.call('TestGrabsInit', stage, character, signed_timer)
        if follower:
            self.m.call('TestGrabsFollower')

    def run(self, frames):
        while frames > 0:
            self.m.call('TestGrabsRun', min(frames, 10))
            frames -= 10

    def place(self, x, line, move, direction):
        self.m.call('TestGrabsPlace', int(x * 1000), line, move, direction)

    def spot(self):
        x, y, line, move, direction = (signed(self.m.call('TestGrabsSpot', key)) for key in range(5))
        return x / 1000, y / 1000, line, move, direction

    def state(self): return self.m.call('TestGrabsSpot', 6)
    def info(self, key): return signed(self.m.call('TestGrabsInfo', key))
    def stat(self, key): return signed(self.m.call('TestGrabsStat', key))
    def speed(self, key): return self.stat(key) / 1000
    def row(self, row, key=0): return self.m.call('TestGrabsRow', row, key)
    def pref(self, index): return self.m.call('TestGrabsPrefValue', index)
    def menu(self, row, value): self.m.call('TestGrabsMenu', row, value)
    def select(self, row): self.m.call('TestGrabsSelect', row)
    def respawn(self, you=-40): self.m.call('TestGrabsRespawn', int(you * 1000))
    def set_state(self, state, hitlag=0, air=0): self.m.call('TestGrabsState', state, hitlag, air)
    def char(self, key): return self.m.call('TestGrabsChar', key) / 1000
    def string(self, address):
        return bytes(self.m.cpu.mem_read(address, 40)).split(b'\0', 1)[0].decode()


class GrabSettingsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def test_the_menu_has_the_stage_and_character_selects(self):
        p = Probe()
        names = [p.string(p.m.call('TestGrabsRowName', row)) for row in range(15)]
        self.assertEqual(names, ['Retry', 'Change Stage', 'Change Characters', 'Free Practice', 'Standing CPU',
                                 'Full Speed Walk', 'Full Speed Run', 'Traveling Dash Dance',
                                 'Full Dash Dance', 'Half Dash Dance', 'Rolling',
                                 'Fighter Collision', 'Counter Position', 'Reset Event Settings', 'Exit'])

    def test_timed_runs_grey_out_the_movement_rows_and_free_practice_opens_them(self):
        # The Free Practice row itself is never greyed out, and shows the saved mode.
        p = Probe()
        self.assertEqual([p.row(row, 1) for row in (OPT_STANDING,) + MOVE_ROWS + (OPT_COLLISION,)], [1] * 8)
        self.assertEqual((p.row(OPT_FREEPRACTICE), p.row(OPT_FREEPRACTICE, 1), p.info(12)), (0, 0, 0))
        p = Probe(prefs={FREE_PRACTICE: 1})
        self.assertEqual([p.row(row, 1) for row in (OPT_STANDING,) + MOVE_ROWS + (OPT_COLLISION,)], [0] * 8)
        self.assertEqual((p.row(OPT_FREEPRACTICE), p.row(OPT_FREEPRACTICE, 1), p.info(12)), (1, 0, 1))

    def test_the_free_practice_toggle_is_saved_and_restarts_either_way(self):
        p = Probe()
        self.assertEqual(p.row(OPT_FREEPRACTICE, 2), 5)  # OPTKIND_TOGGLE
        p.menu(OPT_FREEPRACTICE, 1)
        self.assertEqual((p.pref(FREE_PRACTICE), p.info(4)), (1, 1))
        # The next match starts from the saved choice: Free Practice.
        p = Probe(prefs={FREE_PRACTICE: p.pref(FREE_PRACTICE)})
        self.assertEqual((p.row(OPT_FREEPRACTICE), p.info(12)), (1, 1))
        p.menu(OPT_FREEPRACTICE, 0)
        self.assertEqual((p.pref(FREE_PRACTICE), p.info(4)), (0, 1))
        p = Probe(prefs={FREE_PRACTICE: p.pref(FREE_PRACTICE)})
        self.assertEqual((p.row(OPT_FREEPRACTICE), p.info(12)), (0, 0))

    def test_the_movement_rows_are_saved_and_the_last_one_stays_on(self):
        p = Probe(prefs={FREE_PRACTICE: 1})
        for row, pref in zip(MOVE_ROWS[:-1], MOVE_PREFS[:-1]):
            p.menu(row, 0)
            self.assertEqual((p.row(row), p.pref(pref)), (0, 0))
        p.menu(OPT_ROLL, 0)
        self.assertEqual((p.row(OPT_ROLL), p.pref(PREF_ROLL), p.info(8)), (1, 1, 1))
        p.menu(OPT_DASH, 1)
        p.menu(OPT_ROLL, 0)
        self.assertEqual((p.row(OPT_ROLL), p.pref(PREF_ROLL), p.info(8)), (0, 0, 1))

    def test_a_save_with_every_movement_off_reads_as_all_on(self):
        p = Probe(prefs={FREE_PRACTICE: 1, **{pref: 0 for pref in MOVE_PREFS}})
        self.assertEqual([p.row(row) for row in MOVE_ROWS], [1] * 6)

    def test_timed_runs_mix_every_movement_and_free_practice_only_the_ones_on(self):
        p = Probe(prefs={**{pref: 0 for pref in MOVE_PREFS}, PREF_WALK: 1})
        moves = [p.m.call('TestGrabsChooseMove') for _ in range(600)]
        self.assertEqual(set(moves), {STAND, WALK, RUN, DASH, DASH_FULL, DASH_HALF, ROLL})
        self.assertAlmostEqual(moves.count(STAND) / len(moves), .5, delta=.08)
        p = Probe(prefs={FREE_PRACTICE: 1, **{pref: 0 for pref in MOVE_PREFS}, PREF_RUN: 1, PREF_DASH_HALF: 1})
        moves = [p.m.call('TestGrabsChooseMove') for _ in range(400)]
        self.assertEqual(set(moves), {STAND, RUN, DASH_HALF})
        self.assertAlmostEqual(moves.count(STAND) / len(moves), .5, delta=.08)

    def test_standing_cpu_is_stand_shield_or_random_and_saved(self):
        p = Probe(prefs={FREE_PRACTICE: 1})
        self.assertEqual([p.string(p.m.call('TestGrabsRowChoice', OPT_STANDING, i)) for i in range(3)],
                         ['Stand', 'Shield', 'Random'])
        self.assertEqual(p.row(OPT_STANDING), STAND_RANDOM)  # The default.
        for choice in (STAND_STILL, STAND_SHIELD, STAND_RANDOM):
            p.menu(OPT_STANDING, choice)
            self.assertEqual(p.pref(STANDING), choice)
        self.assertEqual(Probe(prefs={FREE_PRACTICE: 1, STANDING: STAND_SHIELD}).row(OPT_STANDING), STAND_SHIELD)

    def test_standing_cpus_stand_shield_or_mix_and_timed_runs_mix(self):
        def shields(**prefs):
            p = Probe(prefs={globals()[name]: value for name, value in prefs.items()})
            return [p.m.call('TestGrabsChooseShield') for _ in range(200)]
        self.assertEqual(set(shields(FREE_PRACTICE=1, STANDING=STAND_STILL)), {0})
        self.assertEqual(set(shields(FREE_PRACTICE=1, STANDING=STAND_SHIELD)), {1})
        for prefs in ({'FREE_PRACTICE': 1, 'STANDING': STAND_RANDOM}, {'STANDING': STAND_STILL}):
            picks = shields(**prefs)
            self.assertAlmostEqual(sum(picks) / len(picks), .5, delta=.12)

    def test_fighter_collision_is_saved_and_shown_in_free_practice(self):
        p = Probe(prefs={FREE_PRACTICE: 1, COLLISION: 1})
        self.assertEqual(p.info(16), 1)
        p.menu(OPT_COLLISION, 0)
        self.assertEqual((p.pref(COLLISION), p.info(16)), (0, 0))
        self.assertEqual(Probe(prefs={COLLISION: 1}).info(16), 0)  # Timed runs never show it.

    def test_reset_event_settings_restores_the_defaults_but_keeps_your_best(self):
        p = Probe(prefs={BEST: 17, FREE_PRACTICE: 1, PREF_WALK: 0, PREF_DASH: 0, COLLISION: 1})
        p.menu(OPT_COUNTER, 3)
        p.select(OPT_DEFAULTS)
        self.assertEqual([p.pref(i) for i in range(10)], [17, 0, STAND_RANDOM, 1, 1, 1, 1, 1, 1, 0])
        self.assertEqual((p.info(17), p.info(4)), (3, 1))
        self.assertEqual(p.row(OPT_FREEPRACTICE), 0)


class GrabSpawnTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def spawns(self, p, count, you=(-40,)):
        spots = []
        for i in range(count):
            p.respawn(you[i % len(you)])
            spots.append(p.spot())
        return spots

    def test_spawns_cover_the_platforms_the_ground_under_them_and_open_ground(self):
        p = Probe()
        # You under the left platform keep the CPU off it, so you move around.
        spots = self.spawns(p, 300, you=(-40, 0, 40))
        on = {line: [s for s in spots if s[2] == line] for line in FLOORS}
        for line in (LEFT_PLATFORM, RIGHT_PLATFORM, TOP_PLATFORM):
            self.assertGreater(len(on[line]), 15, line)
        ground = on[MAIN_LEFT] + on[MAIN_RIGHT]
        under = [s for s in ground if any(FLOORS[l][0] <= s[0] <= FLOORS[l][1] for l in (LEFT_PLATFORM, RIGHT_PLATFORM, TOP_PLATFORM))]
        open_ground = [s for s in ground if s not in under]
        self.assertGreater(len(under), 30)
        self.assertGreater(len(open_ground), 10)
        for x, y, line, move, direction in spots:
            left, right, height = FLOORS[line]
            self.assertAlmostEqual(y, height, places=2)
            self.assertGreaterEqual(x, left + INSET - .01)
            self.assertLessEqual(x, right - INSET + .01)

    def test_spawns_keep_away_from_you_and_the_last_spot(self):
        p = Probe()
        last = None
        for you in (-40, 0, 40, -10, 25) * 20:
            p.respawn(you)
            x, y, *_ = p.spot()
            self.assertGreaterEqual(((x - you) ** 2 + y ** 2) ** .5, FROM_YOU - .01)
            if last:
                self.assertGreaterEqual(((x - last[0]) ** 2 + (y - last[1]) ** 2) ** .5, FROM_LAST - .01)
            last = (x, y)

    def test_final_destination_spawns_anywhere_along_the_ground(self):
        p = Probe(stage=FINAL_DESTINATION)
        xs = [s[0] for s in self.spawns(p, 200, you=(0,))]
        self.assertLess(min(xs), -FD_EDGE + INSET + 15)
        self.assertGreater(max(xs), FD_EDGE - INSET - 15)
        self.assertTrue(all(abs(x) <= FD_EDGE - INSET + .01 for x in xs))

    def test_a_spawn_resets_the_cpu_and_picks_its_movement(self):
        p = Probe()
        p.m.call('TestGrabsPercent', 45)
        p.respawn()
        self.assertEqual((p.state(), p.info(3), p.info(9), p.info(2)), (ASID_WAIT, 0, 15, 0))
        x, y, line, move, direction = p.spot()
        self.assertIn(move, (STAND, WALK, RUN, DASH, ROLL))
        self.assertEqual(signed(p.m.call('TestGrabsSpot', 5)), direction)

    def test_the_first_spawn_is_on_the_first_frame(self):
        p = Probe()
        p.run(1)
        x, y, line, *_ = p.spot()
        self.assertIn(line, FLOORS)
        self.assertEqual(p.state() in (ASID_WAIT,) or p.stat(0) == 1, True)


class GrabMovementTests(unittest.TestCase):
    """Each movement, for four characters and both ways the tilt timer might
    treat a stick flipped across the middle."""
    @classmethod
    def setUpClass(cls): build()

    def moving(self, move, character, signed_timer, line=MAIN_LEFT, x=-20, direction=1, frames=600, stage=BATTLEFIELD):
        p = Probe(stage=stage, character=character, signed_timer=signed_timer)
        p.place(x, line, move, direction)
        p.run(frames)
        return p

    def cases(self):
        for character in (FOX, MARTH, GAME_AND_WATCH, LUIGI):
            for signed_timer in (0, 1):
                yield character, signed_timer

    def assert_crosses(self, p, reach=NEAR_END, line=MAIN_LEFT):
        """Back and forth across a floor, right up to each end: within reach."""
        left, right, _ = FLOORS[line]
        self.assertLessEqual(p.stat(13) / 1000, left + reach)
        self.assertGreaterEqual(p.stat(14) / 1000, right - reach)

    def assert_stays_on(self, p, line):
        left, right, height = FLOORS[line]
        self.assertEqual(p.stat(1), 0, 'fell off')
        self.assertGreaterEqual(p.stat(13) / 1000, left - .01)
        self.assertLessEqual(p.stat(14) / 1000, right + .01)
        self.assertAlmostEqual(p.stat(15) / 1000, height, places=2)
        self.assertAlmostEqual(p.stat(16) / 1000, height, places=2)

    def test_walking_reaches_full_speed_without_dashing_and_turns_at_each_end(self):
        for character, signed_timer in self.cases():
            with self.subTest(character=character, signed_timer=signed_timer):
                p = self.moving(WALK, character, signed_timer)
                self.assertGreaterEqual(p.speed(10), p.char(0) * .98)
                self.assertEqual((p.stat(7), p.stat(2), p.stat(3)), (0, 0, 0))
                self.assertGreaterEqual(p.stat(8), 3)
                self.assert_stays_on(p, MAIN_LEFT)
                self.assert_crosses(p)

    def test_running_turns_around_by_crouching_and_dashing_out(self):
        for character, signed_timer in self.cases():
            with self.subTest(character=character, signed_timer=signed_timer):
                p = self.moving(RUN, character, signed_timer)
                self.assertGreaterEqual(p.stat(2), 3)
                self.assertGreaterEqual(p.speed(11), p.char(1) * .98)
                # Every end: a crouch, then a dash out of it, and never a run
                # turnaround or a drop through a floor.
                self.assertGreaterEqual(p.stat(29), 3)
                self.assertGreaterEqual(p.stat(30), p.stat(29) - 1)
                self.assertEqual((p.stat(5), p.stat(31)), (0, 0))
                self.assert_stays_on(p, MAIN_LEFT)
                self.assert_crosses(p)

    def test_dash_dancing_cuts_each_dash_off_with_a_quick_dash_back(self):
        for character, signed_timer in self.cases():
            with self.subTest(character=character, signed_timer=signed_timer):
                p = self.moving(DASH, character, signed_timer)
                self.assertEqual(p.stat(2), 0, 'ran')
                # Each dash the way it's heading lasts until the frame its
                # script would make it a run, and no longer.
                self.assertLessEqual(p.stat(22), p.m.call('TestGrabsChar', 3) + 1)
                self.assertGreaterEqual(p.stat(18), 20)
                # Then a dash back of a frame or two, against that dash, that
                # really moves the CPU back, before the next dash forward.
                self.assertGreaterEqual(p.stat(19), 15)
                self.assertEqual(p.stat(20), p.stat(19))
                self.assertEqual(p.stat(21), p.stat(20))
                self.assertGreaterEqual(p.speed(12), p.char(2) * .99)
                self.assert_stays_on(p, MAIN_LEFT)
                self.assert_crosses(p)

    def test_full_dash_dance_dashes_full_dashes_back_and_forth_in_place(self):
        for character, signed_timer in self.cases():
            with self.subTest(character=character, signed_timer=signed_timer):
                p = self.moving(DASH_FULL, character, signed_timer)
                run_frame = p.m.call('TestGrabsChar', 3)
                self.assertEqual(p.stat(2), 0, 'ran')
                self.assertGreaterEqual(p.stat(23), 25)
                self.assertEqual(p.stat(24), p.stat(23) - 1)  # Each dash goes the other way.
                # Every dash after the first (from standing) goes until it would
                # become a run, and as far one way as the other, so it stays put.
                self.assertEqual((p.stat(25), p.stat(26)), (run_frame + 1, run_frame + 1))
                self.assertLess((p.stat(27) - p.stat(28)) / 1000, 1)
                self.assertLess(p.stat(14) - p.stat(13), p.stat(27) * 1.5)
                self.assert_stays_on(p, MAIN_LEFT)

    def test_half_dash_dance_dashes_half_as_far_back_and_forth(self):
        for character, signed_timer in self.cases():
            with self.subTest(character=character, signed_timer=signed_timer):
                full = self.moving(DASH_FULL, character, signed_timer)
                p = self.moving(DASH_HALF, character, signed_timer)
                run_frame = p.m.call('TestGrabsChar', 3)
                self.assertEqual(p.info(18), run_frame)  # Learned from its first dash, a full one.
                self.assertEqual(p.stat(2), 0, 'ran')
                self.assertGreaterEqual(p.stat(23), 40)
                self.assertEqual(p.stat(24), p.stat(23) - 1)
                self.assertEqual((p.stat(25), p.stat(26)), (run_frame // 2 + 1, run_frame // 2 + 1))
                self.assertLess((p.stat(27) - p.stat(28)) / 1000, 1)
                self.assertAlmostEqual(p.stat(27) / full.stat(27), .5, delta=.1)
                self.assert_stays_on(p, MAIN_LEFT)

    def test_a_dance_in_place_near_an_end_or_on_a_platform_stays_on(self):
        for move in (DASH_FULL, DASH_HALF):
            for character in (FOX, LUIGI):
                for line, x, direction in ((MAIN_RIGHT, 60, 1), (MAIN_LEFT, -60, -1), (LEFT_PLATFORM, -38.8, 1),
                                           (LEFT_PLATFORM, -52, -1), (TOP_PLATFORM, 12, 1)):
                    with self.subTest(move=move, character=character, line=line, x=x):
                        p = self.moving(move, character, 0, line=line, x=x, direction=direction, frames=400)
                        self.assert_stays_on(p, line if line != MAIN_RIGHT else MAIN_LEFT)
                        self.assertGreaterEqual(p.stat(23), 10)
                        self.assertEqual(p.stat(2), 0)

    def test_rolling_rolls_again_and_again_and_rolls_back_at_each_end(self):
        for character, signed_timer in self.cases():
            with self.subTest(character=character, signed_timer=signed_timer):
                p = self.moving(ROLL, character, signed_timer)
                self.assertGreaterEqual(p.stat(6), 12)
                self.assertGreaterEqual(p.stat(8), 2)
                self.assertEqual(p.stat(2), 0)
                self.assert_stays_on(p, MAIN_LEFT)
                self.assert_crosses(p)
                self.assertTrue(p.info(10) & PAD_TRIGGER_R)
                self.assertEqual(p.info(15), 60)  # The shield stays full.

    def test_every_movement_goes_right_up_to_each_end_of_a_platform(self):
        for move in (WALK, RUN, DASH, ROLL):
            for line in (LEFT_PLATFORM, TOP_PLATFORM):
                for character in (FOX, MARTH, GAME_AND_WATCH, LUIGI):
                    for direction in (1, -1):
                        with self.subTest(move=move, line=line, character=character, direction=direction):
                            left, right, _ = FLOORS[line]
                            p = self.moving(move, character, 0, line=line, x=(left + right) / 2, direction=direction, frames=600)
                            self.assert_stays_on(p, line)
                            self.assertGreaterEqual(p.stat(8), 2)
                            self.assert_crosses(p, line=line)
                            self.assertEqual(p.stat(31), 0)  # A crouch never drops through.

    def test_a_crouch_on_a_platform_waits_for_an_old_down_tilt(self):
        # Game & Watch and Luigi get to run on Battlefield's platforms, so they
        # crouch there: never dropping through, and still dashing out.
        for character in (GAME_AND_WATCH, LUIGI):
            with self.subTest(character=character):
                p = self.moving(RUN, character, 0, line=LEFT_PLATFORM, x=-38.8, frames=600)
                self.assertGreaterEqual(p.stat(29), 10)
                self.assertGreaterEqual(p.stat(30), p.stat(29) - 1)
                self.assertEqual((p.stat(31), p.stat(1), p.stat(5)), (0, 0, 0))

    def test_a_cpu_spawned_facing_the_edge_heads_back_first(self):
        for move in (WALK, RUN, DASH, ROLL):
            with self.subTest(move=move):
                p = self.moving(move, FOX, 0, line=MAIN_RIGHT, x=60, direction=1, frames=200)
                self.assert_stays_on(p, MAIN_LEFT)

    def test_final_destination_runs_end_to_end(self):
        for move in (WALK, RUN, DASH, ROLL):
            with self.subTest(move=move):
                p = self.moving(move, FOX, 0, x=0, frames=900, stage=FINAL_DESTINATION)
                self.assertEqual(p.stat(1), 0)
                self.assertLess(p.stat(13) / 1000, -FD_EDGE + 45)
                self.assertGreater(p.stat(14) / 1000, FD_EDGE - 45)

    def test_a_standing_cpu_presses_nothing(self):
        p = self.moving(STAND, FOX, 0, frames=120)
        self.assertEqual((p.stat(9), p.stat(13), p.stat(14)), (0, -20000, -20000))

    def test_a_shielding_cpu_holds_shield_that_never_runs_out(self):
        p = Probe()
        p.place(-20, MAIN_LEFT, STAND, 1)
        p.m.call('TestGrabsShield', 1)
        p.run(20)
        self.assertEqual(p.info(20), ASID_GUARD)
        self.assertTrue(p.info(10) & PAD_TRIGGER_R)
        p.m.call('TestGrabsShieldHealth', 5)
        p.run(1)
        self.assertEqual(p.info(15), 60)
        self.assertEqual((p.stat(13), p.stat(14)), (-20000, -20000))  # It doesn't go anywhere.

    def test_a_hit_on_the_shield_keeps_the_cpu_where_it_is(self):
        p = Probe()
        p.place(-20, MAIN_LEFT, STAND, 1)
        p.m.call('TestGrabsShield', 1)
        p.run(20)
        p.set_state(ASID_GUARDSETOFF, hitlag=6)
        p.run(30)
        # Shield stun and its hitlag are the CPU's own: no respawn, no count,
        # and it's back in shield.
        self.assertEqual((p.info(20), p.info(0)), (ASID_GUARD, 0))
        self.assertEqual(p.spot()[:3], (-20.0, 0.0, MAIN_LEFT))


class GrabCountTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def start(self, **kwargs):
        p = Probe(**kwargs)
        p.place(-20, MAIN_LEFT, STAND, 1)
        return p

    def test_a_grab_counts_once_and_the_throw_sends_the_cpu_somewhere_else(self):
        p = self.start()
        p.set_state(ASID_CAPTUREPULLEDHI)
        p.run(1)
        self.assertEqual((p.info(0), p.info(2)), (1, 1))
        for state, frames in ((ASID_CAPTUREWAITHI, 30), (ASID_THROWNF, 10)):
            p.set_state(state)
            p.run(frames)
            self.assertEqual((p.info(0), p.state()), (1, state))
        self.assertEqual(p.info(11), 0)  # No mashing out.
        p.m.call('TestGrabsPercent', 9)
        p.set_state(ASID_DAMAGEFLYN, hitlag=3, air=1)
        p.run(2)
        self.assertEqual(p.state(), ASID_DAMAGEFLYN)  # Still in the throw's hitlag.
        p.run(1)
        self.assertEqual((p.state(), p.info(2), p.info(3), p.info(0)), (ASID_WAIT, 0, 0, 1))
        p.set_state(ASID_CAPTUREPULLEDHI)
        p.run(1)
        self.assertEqual(p.info(0), 2)

    def test_a_hit_leaves_the_cpu_for_a_grab_then_sends_it_away_without_counting(self):
        p = Probe()
        p.place(-20, MAIN_LEFT, WALK, 1)
        p.run(30)
        p.set_state(ASID_DAMAGEN1, hitlag=4)
        for frame in range(1, 4 + GRAB_WINDOW):  # The hitlag, then the window.
            if frame == 15:
                p.set_state(ASID_WAIT)  # Its hitstun is over: it stays, and doesn't walk on.
            p.run(1)
            self.assertEqual((p.info(21), p.info(10), p.info(11)), (1, 0, 0), frame)
        self.assertEqual(p.state(), ASID_WAIT)
        p.run(1)
        self.assertEqual((p.info(21), p.info(0)), (0, 0))  # Back somewhere else, uncounted.

    def test_a_grab_after_a_hit_counts_and_the_throw_sends_the_cpu_away(self):
        p = self.start()
        p.set_state(ASID_DAMAGEN1, hitlag=4)
        p.run(4 + GRAB_WINDOW - 1)  # The window's last frame.
        self.assertEqual(p.state(), ASID_DAMAGEN1)
        p.set_state(ASID_CAPTUREPULLEDHI)
        p.run(1)
        self.assertEqual((p.info(0), p.info(2)), (1, 1))
        p.set_state(ASID_DAMAGEFLYN, hitlag=3, air=1)  # The throw.
        p.run(2)
        self.assertEqual(p.state(), ASID_DAMAGEFLYN)
        p.run(1)
        self.assertEqual((p.state(), p.info(21), p.info(0)), (ASID_WAIT, 0, 1))

    def test_each_new_hit_starts_the_window_over(self):
        p = self.start()
        p.set_state(ASID_DAMAGEN1, hitlag=4)
        p.run(4 + GRAB_WINDOW - 5)
        p.set_state(ASID_DAMAGEN2, hitlag=4)  # Five frames before the first window would end.
        p.run(4 + GRAB_WINDOW - 1)
        self.assertEqual((p.state(), p.info(21)), (ASID_DAMAGEN2, 1))
        p.run(1)
        self.assertEqual((p.state(), p.info(21), p.info(0)), (ASID_WAIT, 0, 0))

    def test_a_knockdown_sends_the_cpu_away_at_once(self):
        p = self.start()
        p.set_state(ASID_DAMAGEN1, hitlag=4)
        p.run(10)
        p.set_state(ASID_DOWNBOUNDU)
        p.run(1)
        self.assertEqual((p.state(), p.info(21), p.info(0)), (ASID_WAIT, 0, 0))

    def test_a_hit_off_the_stage_still_comes_back_at_once(self):
        p = self.start()
        p.set_state(ASID_DAMAGEFLYN, hitlag=4, air=1)
        p.m.call('TestGrabsMoveCPU', -90000, 20000)
        p.run(3)
        self.assertEqual(p.state(), ASID_DAMAGEFLYN)  # Hitlag.
        p.run(1)
        self.assertEqual((p.state(), p.info(21)), (ASID_WAIT, 0))

    def test_a_command_grab_plays_out_without_counting(self):
        p = self.start()
        p.set_state(ASID_CAPTURECAPTAIN)
        p.run(20)
        self.assertEqual((p.state(), p.info(0)), (ASID_CAPTURECAPTAIN, 0))
        p.set_state(ASID_DAMAGEFLYN, air=1)
        p.run(1)
        self.assertEqual((p.state(), p.info(0)), (ASID_WAIT, 0))

    def test_a_cpu_off_the_stage_comes_back(self):
        p = self.start()
        p.set_state(ASID_FALL, air=1)
        p.m.call('TestGrabsMoveCPU', -90000, -20000)
        p.run(1)
        self.assertEqual(p.state(), ASID_WAIT)
        x, y, line, *_ = p.spot()
        self.assertIn(line, FLOORS)

    def test_a_fall_onto_a_lower_floor_keeps_moving(self):
        p = self.start()
        p.set_state(ASID_FALL, air=1)
        p.m.call('TestGrabsMoveCPU', -40000, 10000)
        p.run(20)
        self.assertEqual((p.info(0), p.spot()[0]), (0, -40))

    def test_the_timed_minute_saves_a_new_best(self):
        p = self.start(prefs={BEST: 4})
        p.m.call('TestGrabsClock', 3600, 9)
        p.run(1)
        self.assertEqual((p.info(5), p.pref(BEST), p.info(6), p.info(7)), (1, 9, 325, 1))

    def test_the_timed_minute_keeps_a_higher_best(self):
        p = self.start(prefs={BEST: 12})
        p.m.call('TestGrabsClock', 3600, 9)
        p.run(1)
        self.assertEqual((p.info(5), p.pref(BEST), p.info(6), p.info(7)), (1, 12, 324, 0))

    def test_a_best_past_seven_bits_saves_as_the_most_it_holds(self):
        p = self.start()
        p.m.call('TestGrabsClock', 3600, 300)
        p.run(1)
        self.assertEqual(p.pref(BEST), 127)

    def test_free_practice_never_ends(self):
        p = self.start(prefs={FREE_PRACTICE: 1})
        p.m.call('TestGrabsClock', 3600, 9)
        p.run(1)
        self.assertEqual((p.info(5), p.pref(BEST)), (0, 0))

    def test_a_cpu_nana_sits_out(self):
        p = Probe(follower=True)
        p.run(2)
        self.assertEqual(p.info(13), ASID_SLEEP)
        self.assertEqual(p.info(14), -999)


if __name__ == '__main__':
    unittest.main()
