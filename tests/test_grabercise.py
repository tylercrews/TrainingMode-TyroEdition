"""Run the production Grabs-ercise event against tests/grabercise_probe.c.

The probe's engine follows the decomp's Wait, Walk, Turn, Dash, Run, TurnRun,
Guard and roll rules (see src/grabercise.c), so these check that the CPU's
inputs walk, run, dash and roll at full speed the way the game would take
them, and turn back before the end of their floor. None of this is Dolphin.
"""
from pathlib import Path
import struct
import subprocess
import unittest
import test_settings as fixtures
from test_lowlaser import load_probe
from test_techchase import plco_common
from unicorn import ppc_const as reg

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
GRAB_WINDOW = 30  # Frames a hit CPU stays, after the hitlag, for a follow-up grab, and a juggled one once it can act.
LATE_SHOW_FRAMES = 60  # A late count a hit or grab stopped stays up this long.
LATE_RED = (255, 40, 40)  # TM_CUE_RED_COLOR, the Missed L Cancel overlay's red,
LATE_PULSE = [180, 140, 100, 60, 20, 60, 100, 140]  # and its pulse (TM_CUE_RED_PULSE).
LATE_Y, LATE_UNDER_COUNTER = 12.9, 6.65  # Late Frames OSD: under the match timer, or a grid row lower.
COUNTER_UNDER_TIMER = 4  # Counter Position's Under Timer.
# Menu rows and saved preferences.
(OPT_RETRY, OPT_STAGE, OPT_CHARACTERS, OPT_FREEPRACTICE, OPT_SUCCESS, OPT_PERCENT, OPT_DI, OPT_LATE_OSD,
 OPT_MOVEMENT, OPT_STANDING, OPT_WALK, OPT_RUN, OPT_DASH, OPT_DASH_FULL, OPT_DASH_HALF, OPT_ROLL, OPT_COLLISION,
 OPT_COUNTER, OPT_DEFAULTS, OPT_EXIT) = range(20)
(BEST, FREE_PRACTICE, SUCCESS, PERCENT, STANDING, PREF_WALK, PREF_RUN, PREF_DASH, PREF_DASH_FULL, PREF_DASH_HALF,
 PREF_ROLL, COLLISION, MOVEMENT, DI, LATE_OSD) = range(15)  # CPU Movement came last, then CPU DI and Late Frames OSD.
STAND_STILL, STAND_SHIELD, STAND_RANDOM = range(3)  # Standing CPU's choices.
MOVEMENT_RANDOM, MOVEMENT_STILL, MOVEMENT_MOVING = range(3)  # CPU Movement's.
THROW, HIT = range(2)  # Success Criteria's.
RANDOM_PERCENT = 0  # CPU Percent's first choice; choice n > 0 is (n - 1) * 10%.
DI_NONE, DI_RANDOM = range(2)  # CPU DI's.
# CPU DI's Random: the stick all the way one of its eight ways.
DI_WAYS = {(127, 0), (89, 89), (0, 127), (-89, 89), (-127, 0), (-89, -89), (0, -127), (89, -89)}
MOVE_ROWS = (OPT_WALK, OPT_RUN, OPT_DASH, OPT_DASH_FULL, OPT_DASH_HALF, OPT_ROLL)
MOVE_PREFS = (PREF_WALK, PREF_RUN, PREF_DASH, PREF_DASH_FULL, PREF_DASH_HALF, PREF_ROLL)
# States, from the MexTK enum.
ASID_WAIT, ASID_FALL, ASID_LANDING = 14, 29, 42
ASID_CAPTUREPULLEDHI, ASID_CAPTUREWAITHI, ASID_THROWNF, ASID_THROWNHI = 223, 224, 239, 241
ASID_SHOULDEREDWAIT, ASID_THROWNFF = 266, 271
ASID_DAMAGEN1, ASID_DAMAGEN2, ASID_DAMAGEFLYN, ASID_DOWNBOUNDU = 78, 79, 88, 183
ASID_CAPTURECAPTAIN, ASID_SLEEP = 275, 11
ASID_GUARDON, ASID_GUARD, ASID_GUARDSETOFF = 178, 179, 181
ASID_DEADDOWN, ASID_DEADUP, ASID_DEADUPSTAR, ASID_DEADUPFALL, ASID_DEADUPFALLHITCAMERAICE = 0, 3, 4, 6, 10
(ASID_DOWNWAITU, ASID_DOWNDAMAGEU, ASID_DOWNSTANDU, ASID_DOWNATTACKU, ASID_DOWNFOWARDU,
 ASID_DOWNBACKU) = range(184, 190)
ASID_DOWNBOUNDD = 191
ASID_DOWNSTANDD, ASID_DOWNATTACKD, ASID_DOWNFOWARDD, ASID_DOWNBACKD = range(194, 198)
ASID_PASSIVE, ASID_PASSIVESTANDF, ASID_PASSIVESTANDB = 199, 200, 201
PAD_TRIGGER_R = 0x20
# Hit mode's tech chase: the landings and getups, as techchase_logic.h numbers them.
IN_PLACE, AWAY, TOWARD, MISS = range(4)
STAND_UP, ROLL_AWAY, ROLL_TOWARD, GETUP_ATTACK = range(4)
TECH_STICK = 40  # The stick a tech or getup roll holds, and how long ago its L/R press reads.
TECH_PRESS_AGO = 10
LAND_FRAMES = 4  # A tech roll's stick goes out this many frames of falling before the floor.


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
        defaults = {BEST: 0, FREE_PRACTICE: 0, SUCCESS: THROW, PERCENT: RANDOM_PERCENT, STANDING: STAND_RANDOM,
                    COLLISION: 0, MOVEMENT: MOVEMENT_RANDOM, DI: DI_NONE, LATE_OSD: 1,
                    **{pref: 1 for pref in MOVE_PREFS}}
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
        names = [p.string(p.m.call('TestGrabsRowName', row)) for row in range(20)]
        self.assertEqual(names, ['Retry', 'Change Stage', 'Change Characters', 'Free Practice',
                                 'Success Criteria', 'CPU Percent', 'CPU DI', 'Late Frames OSD', 'CPU Movement',
                                 'Standing CPU',
                                 'Full Speed Walk', 'Full Speed Run', 'Traveling Dash Dance',
                                 'Full Dash Dance', 'Half Dash Dance', 'Rolling',
                                 'Fighter Collision', 'Counter Position', 'Reset Event Settings', 'Exit'])

    def test_timed_runs_grey_out_the_movement_rows_and_free_practice_opens_them(self):
        # The Free Practice row itself is never greyed out, and shows the saved mode.
        p = Probe()
        rows = (OPT_SUCCESS, OPT_MOVEMENT, OPT_STANDING) + MOVE_ROWS + (OPT_COLLISION,)
        self.assertEqual([p.row(row, 1) for row in rows], [1] * 10)
        self.assertEqual((p.row(OPT_FREEPRACTICE), p.row(OPT_FREEPRACTICE, 1), p.info(12)), (0, 0, 0))
        p = Probe(prefs={FREE_PRACTICE: 1})
        self.assertEqual([p.row(row, 1) for row in rows], [0] * 10)
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

    def test_cpu_movement_is_random_still_only_or_moving_only_and_saved(self):
        p = Probe(prefs={FREE_PRACTICE: 1})
        self.assertEqual([p.string(p.m.call('TestGrabsRowChoice', OPT_MOVEMENT, i)) for i in range(3)],
                         ['Random', 'Still Only', 'Moving Only'])
        self.assertEqual((p.row(OPT_MOVEMENT), p.row(OPT_MOVEMENT, 2)), (MOVEMENT_RANDOM, 1))  # OPTKIND_STRING
        for choice in (MOVEMENT_STILL, MOVEMENT_MOVING, MOVEMENT_RANDOM):
            p.menu(OPT_MOVEMENT, choice)
            self.assertEqual(p.pref(MOVEMENT), choice)
        self.assertEqual(Probe(prefs={FREE_PRACTICE: 1, MOVEMENT: MOVEMENT_MOVING}).row(OPT_MOVEMENT), MOVEMENT_MOVING)

    def test_cpu_movement_picks_standing_moving_or_both_and_timed_runs_mix(self):
        def moves(count, **prefs):
            p = Probe(prefs={globals()[name]: value for name, value in prefs.items()})
            return [p.m.call('TestGrabsChooseMove') for _ in range(count)]
        self.assertEqual(set(moves(200, FREE_PRACTICE=1, MOVEMENT=MOVEMENT_STILL)), {STAND})
        moving = moves(400, FREE_PRACTICE=1, MOVEMENT=MOVEMENT_MOVING)
        self.assertEqual(set(moving), {WALK, RUN, DASH, DASH_FULL, DASH_HALF, ROLL})
        only = moves(200, FREE_PRACTICE=1, MOVEMENT=MOVEMENT_MOVING, PREF_WALK=0, PREF_RUN=0, PREF_DASH=0, PREF_ROLL=0)
        self.assertEqual(set(only), {DASH_FULL, DASH_HALF})  # Only the movements that are on.
        # Random, and timed runs whatever's saved, stand half the time.
        for prefs in ({'FREE_PRACTICE': 1}, {'MOVEMENT': MOVEMENT_STILL}, {'MOVEMENT': MOVEMENT_MOVING}):
            picks = moves(600, **prefs)
            self.assertEqual(set(picks), {STAND, WALK, RUN, DASH, DASH_FULL, DASH_HALF, ROLL})
            self.assertAlmostEqual(picks.count(STAND) / len(picks), .5, delta=.08)

    def test_a_cpu_spawned_after_a_cpu_movement_change_follows_it(self):
        p = Probe(prefs={FREE_PRACTICE: 1})
        def spawned_moves():
            moves = set()
            for _ in range(30):
                p.respawn()
                moves.add(p.spot()[3])
            return moves
        p.menu(OPT_MOVEMENT, MOVEMENT_STILL)
        self.assertEqual(spawned_moves(), {STAND})
        p.menu(OPT_MOVEMENT, MOVEMENT_MOVING)
        self.assertNotIn(STAND, spawned_moves())

    def test_cpu_movement_greys_out_the_rows_it_makes_do_nothing(self):
        p = Probe(prefs={FREE_PRACTICE: 1})
        greyed = lambda: (p.row(OPT_STANDING, 1), [p.row(row, 1) for row in MOVE_ROWS])
        self.assertEqual(greyed(), (0, [0] * 6))
        p.menu(OPT_MOVEMENT, MOVEMENT_STILL)  # Nothing moves: the movements do nothing.
        self.assertEqual(greyed(), (0, [1] * 6))
        p.menu(OPT_MOVEMENT, MOVEMENT_MOVING)  # Everything moves: Standing CPU does nothing.
        self.assertEqual(greyed(), (1, [0] * 6))
        p.menu(OPT_MOVEMENT, MOVEMENT_RANDOM)
        self.assertEqual(greyed(), (0, [0] * 6))
        p = Probe(prefs={FREE_PRACTICE: 1, MOVEMENT: MOVEMENT_STILL})  # From the save too.
        self.assertEqual(greyed(), (0, [1] * 6))
        p = Probe(prefs={MOVEMENT: MOVEMENT_RANDOM})  # Timed runs grey out every one.
        self.assertEqual(greyed(), (1, [1] * 6))

    def test_timed_runs_mix_every_movement_and_free_practice_only_the_ones_on(self):
        p = Probe(prefs={**{pref: 0 for pref in MOVE_PREFS}, PREF_WALK: 1})
        moves = [p.m.call('TestGrabsChooseMove') for _ in range(600)]
        self.assertEqual(set(moves), {STAND, WALK, RUN, DASH, DASH_FULL, DASH_HALF, ROLL})
        self.assertAlmostEqual(moves.count(STAND) / len(moves), .5, delta=.08)
        p = Probe(prefs={FREE_PRACTICE: 1, **{pref: 0 for pref in MOVE_PREFS}, PREF_RUN: 1, PREF_DASH_HALF: 1})
        moves = [p.m.call('TestGrabsChooseMove') for _ in range(400)]
        self.assertEqual(set(moves), {STAND, RUN, DASH_HALF})
        self.assertAlmostEqual(moves.count(STAND) / len(moves), .5, delta=.08)

    def test_success_criteria_is_throw_or_hit_and_saved(self):
        p = Probe(prefs={FREE_PRACTICE: 1})
        self.assertEqual([p.string(p.m.call('TestGrabsRowChoice', OPT_SUCCESS, i)) for i in range(2)],
                         ['Throw', 'Hit'])
        self.assertEqual(p.row(OPT_SUCCESS), THROW)  # The default.
        p.m.call('TestGrabsClock', 1, 7)
        p.menu(OPT_SUCCESS, HIT)
        self.assertEqual((p.pref(SUCCESS), p.info(0)), (HIT, 0))  # Saved, and the count starts over.
        self.assertEqual(Probe(prefs={FREE_PRACTICE: 1, SUCCESS: HIT}).row(OPT_SUCCESS), HIT)

    def test_cpu_percent_is_random_or_0_to_120_and_saved(self):
        p = Probe(prefs={FREE_PRACTICE: 1, SUCCESS: HIT})
        self.assertEqual([p.string(p.m.call('TestGrabsRowChoice', OPT_PERCENT, i)) for i in range(14)],
                         ['Random'] + ['%d%%' % (10 * i) for i in range(13)])
        self.assertEqual(p.row(OPT_PERCENT), RANDOM_PERCENT)  # The default.
        for choice in (13, 1, RANDOM_PERCENT):
            p.menu(OPT_PERCENT, choice)
            self.assertEqual(p.pref(PERCENT), choice)
        self.assertEqual(Probe(prefs={FREE_PRACTICE: 1, SUCCESS: HIT, PERCENT: 7}).row(OPT_PERCENT), 7)

    def test_cpu_percent_is_greyed_out_but_in_hit_mode(self):
        self.assertEqual(Probe(prefs={SUCCESS: HIT}).row(OPT_PERCENT, 1), 1)  # Timed runs.
        p = Probe(prefs={FREE_PRACTICE: 1})
        self.assertEqual(p.row(OPT_PERCENT, 1), 1)  # Throw.
        p.menu(OPT_SUCCESS, HIT)
        self.assertEqual(p.row(OPT_PERCENT, 1), 0)
        p.menu(OPT_SUCCESS, THROW)
        self.assertEqual(p.row(OPT_PERCENT, 1), 1)
        self.assertEqual(Probe(prefs={FREE_PRACTICE: 1, SUCCESS: HIT}).row(OPT_PERCENT, 1), 0)

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
        p = Probe(prefs={BEST: 17, FREE_PRACTICE: 1, SUCCESS: HIT, PERCENT: 9, PREF_WALK: 0, PREF_DASH: 0,
                         COLLISION: 1, MOVEMENT: MOVEMENT_STILL, DI: DI_RANDOM, LATE_OSD: 0})
        p.menu(OPT_COUNTER, 3)
        p.select(OPT_DEFAULTS)
        self.assertEqual([p.pref(i) for i in range(15)],
                         [17, 0, THROW, RANDOM_PERCENT, STAND_RANDOM, 1, 1, 1, 1, 1, 1, 0, MOVEMENT_RANDOM, DI_NONE, 1])
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

    def percents(self, p, count=1):
        spawns = []
        for _ in range(count):
            p.respawn()
            spawns.append(p.info(3))
        return spawns

    def test_hit_mode_cpus_appear_at_cpu_percent(self):
        for choice, percent in ((1, 0), (9, 80), (13, 120)):
            p = Probe(prefs={FREE_PRACTICE: 1, SUCCESS: HIT, PERCENT: choice})
            self.assertEqual(set(self.percents(p, 5)), {percent})

    def test_a_random_percent_is_a_new_pick_from_0_to_120_in_tens_every_time(self):
        p = Probe(prefs={FREE_PRACTICE: 1, SUCCESS: HIT})
        spawns = self.percents(p, 300)
        self.assertEqual(set(spawns), set(range(0, 130, 10)))
        self.assertGreater(sum(a != b for a, b in zip(spawns, spawns[1:])), 200)

    def test_throw_mode_and_timed_runs_appear_at_0(self):
        for prefs in ({FREE_PRACTICE: 1, PERCENT: 9}, {FREE_PRACTICE: 1, PERCENT: RANDOM_PERCENT},
                      {SUCCESS: HIT, PERCENT: 9}):
            p = Probe(prefs=prefs)
            p.m.call('TestGrabsPercent', 45)
            self.assertEqual(set(self.percents(p, 20)), {0}, prefs)

    def test_a_change_reaches_a_cpu_waiting_to_be_grabbed_at_once(self):
        p = Probe(prefs={FREE_PRACTICE: 1, SUCCESS: HIT, PERCENT: 2})
        p.run(1)  # The first spawn.
        self.assertEqual(p.info(3), 10)
        p.menu(OPT_PERCENT, 7)
        self.assertEqual(p.info(3), 60)
        p.menu(OPT_SUCCESS, THROW)  # Throw is 0%,
        self.assertEqual(p.info(3), 0)
        p.menu(OPT_SUCCESS, HIT)  # and Hit CPU Percent's again.
        self.assertEqual(p.info(3), 60)

    def test_a_change_leaves_a_hit_grabbed_or_juggled_cpu_its_damage(self):
        p = Probe(prefs={FREE_PRACTICE: 1, SUCCESS: HIT, PERCENT: 2})
        p.place(-20, MAIN_LEFT, STAND, 1)
        p.m.call('TestGrabsPercent', 33)
        p.set_state(ASID_DAMAGEN1, hitlag=4)  # Hit, before any grab.
        p.run(1)
        p.menu(OPT_PERCENT, 7)
        self.assertEqual(p.info(3), 33)
        p.set_state(ASID_CAPTUREPULLEDHI)  # Grabbed.
        p.run(1)
        p.menu(OPT_PERCENT, 8)
        self.assertEqual(p.info(3), 33)
        p.set_state(ASID_THROWNF)
        p.run(10)
        p.set_state(ASID_DAMAGEFLYN, air=1)  # Thrown, and juggled.
        p.m.call('TestGrabsMoveCPU', -20000, 200000)
        p.run(1)
        p.menu(OPT_SUCCESS, THROW)
        p.menu(OPT_SUCCESS, HIT)
        p.menu(OPT_PERCENT, 9)
        self.assertEqual((p.info(3), p.info(23)), (33, 1))
        p.respawn()  # Its next appearance is at the new percent.
        self.assertEqual(p.info(3), 80)

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

    def test_a_best_past_six_bits_saves_as_the_most_it_holds(self):
        p = self.start()
        p.m.call('TestGrabsClock', 3600, 300)
        p.run(1)
        self.assertEqual(p.pref(BEST), 63)

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


class GrabHitModeTests(unittest.TestCase):
    """Success Criteria Hit: a grab counts once a hit follows its throw."""
    @classmethod
    def setUpClass(cls): build()

    def thrown(self, timed=False, high=False):
        p = Probe(prefs={SUCCESS: HIT} if timed else {FREE_PRACTICE: 1, SUCCESS: HIT})
        p.place(-20, MAIN_LEFT, STAND, 1)
        p.set_state(ASID_CAPTUREPULLEDHI)
        p.run(1)
        p.set_state(ASID_THROWNF)
        p.run(10)
        p.set_state(ASID_DAMAGEFLYN, air=1)  # The throw lets go.
        if high:
            p.m.call('TestGrabsMoveCPU', -20000, 200000)
        p.run(1)
        return p

    def hit(self, p, hitlag=4):
        p.set_state(ASID_DAMAGEFLYN, hitlag=hitlag, air=1)
        p.run(1)

    def test_the_grab_and_throw_alone_dont_count_and_the_cpu_stays_up(self):
        p = self.thrown(high=True)
        self.assertEqual((p.info(0), p.info(22), p.state(), p.info(23)), (0, 0, ASID_DAMAGEFLYN, 1))

    def test_the_first_hit_counts_once_and_every_hit_adds_to_the_hits(self):
        p = self.thrown(high=True)
        self.hit(p)
        self.assertEqual((p.info(0), p.info(22)), (1, 1))
        p.run(20)
        for hits in (2, 3):
            self.hit(p)
            p.run(20)
            self.assertEqual((p.info(0), p.info(22), p.state()), (1, hits, ASID_DAMAGEFLYN))
        # A hit landing during another's hitlag is a hit too.
        self.hit(p, hitlag=8)
        p.run(3)
        self.hit(p, hitlag=8)
        self.assertEqual((p.info(0), p.info(22)), (1, 5))

    def test_hitlag_running_down_isnt_a_new_hit(self):
        p = self.thrown(high=True)
        self.hit(p, hitlag=10)
        p.run(15)
        self.assertEqual(p.info(22), 1)

    def test_the_juggle_lasts_half_a_second_once_the_cpu_could_act_landed_or_not(self):
        # Out of hitstun without tumbling, high up: it would fall for over a second.
        p = self.thrown(high=True)
        p.set_state(ASID_FALL, air=1)
        p.run(GRAB_WINDOW - 1)  # (The thrown CPU could act from the frame after the throw.)
        self.assertEqual((p.state(), p.info(23), p.info(33)), (ASID_FALL, 1, GRAB_WINDOW))
        p.run(1)
        self.assertEqual((p.state(), p.info(21), p.info(23), p.info(0)), (ASID_WAIT, 0, 0, 0))
        # A CPU that lands first gets no more for it: the count runs through its landing.
        p = self.thrown()
        p.set_state(ASID_FALL, air=1)
        p.run(5)
        self.assertEqual(p.state(), ASID_WAIT)  # Landed, on its feet.
        p.run(GRAB_WINDOW - 6)
        self.assertEqual((p.info(23), p.info(33)), (1, GRAB_WINDOW))
        p.run(1)
        self.assertEqual((p.info(23), p.info(0)), (0, 0))

    def test_a_hit_on_the_ground_starts_its_window_over(self):
        p = self.thrown()
        p.run(5)  # Landed.
        p.set_state(ASID_DAMAGEN1, hitlag=4)
        p.run(3 + GRAB_WINDOW)
        self.assertEqual((p.state(), p.info(0), p.info(22)), (ASID_DAMAGEN1, 1, 1))
        p.run(1)
        self.assertEqual((p.state(), p.info(23), p.info(0)), (ASID_WAIT, 0, 1))

    def test_a_knockdown_doesnt_end_the_juggle(self):
        p = self.thrown()
        self.hit(p)
        p.run(4)
        p.m.call('TestGrabsDownBound')
        p.run(1)
        # A tech chase: it stays down, for its getup (GrabTechChaseTests).
        self.assertEqual((p.state(), p.info(23), p.info(24), p.info(0), p.info(22)), (ASID_DOWNBOUNDU, 1, 1, 1, 1))

    def test_a_cpu_knocked_off_the_top_comes_back_at_once(self):
        for state in range(ASID_DEADUP, ASID_DEADUPFALLHITCAMERAICE + 1):  # Star and screen KOs.
            with self.subTest(state=state):
                p = self.thrown(high=True)
                self.hit(p)  # Up air, say: it counts,
                p.run(5)
                self.assertEqual((p.state(), p.info(23)), (ASID_DAMAGEFLYN, 1))
                p.set_state(state, air=1)
                p.run(1)  # and the next CPU is out.
                self.assertEqual((p.state(), p.info(23), p.info(0), p.info(22)), (ASID_WAIT, 0, 1, 1))
                x, y, line, *_ = p.spot()
                self.assertIn(line, FLOORS)

    def test_a_throw_off_the_top_comes_back_at_once_too(self):
        p = self.thrown(high=True)
        p.set_state(ASID_DEADUPSTAR, air=1)
        p.run(1)
        self.assertEqual((p.state(), p.info(23), p.info(0), p.info(22)), (ASID_WAIT, 0, 0, 0))

    def test_a_hit_off_the_top_comes_back_at_once_in_any_mode(self):
        for prefs in ({}, {FREE_PRACTICE: 1}, {FREE_PRACTICE: 1, SUCCESS: HIT}):
            with self.subTest(prefs=prefs):
                p = Probe(prefs=prefs)
                p.place(-20, MAIN_LEFT, STAND, 1)
                p.set_state(ASID_DAMAGEFLYN, hitlag=4, air=1)  # A hit before any grab,
                p.m.call('TestGrabsMoveCPU', -20000, 100000)
                p.run(10)
                self.assertEqual((p.state(), p.info(21)), (ASID_DAMAGEFLYN, 1))
                p.set_state(ASID_DEADUPFALL, air=1)  # off the top.
                p.run(1)
                self.assertEqual((p.state(), p.info(21), p.info(0)), (ASID_WAIT, 0, 0))

    def test_a_cpu_thrown_off_the_stage_comes_back(self):
        p = self.thrown(high=True)
        p.m.call('TestGrabsMoveCPU', -90000, 20000)
        p.run(1)
        self.assertEqual((p.state(), p.info(23)), (ASID_WAIT, 0))

    def test_the_cpu_presses_nothing_while_its_juggled(self):
        p = self.thrown(high=True)
        for _ in range(20):
            p.run(1)
            self.assertEqual((p.info(10), p.info(11)), (0, 0))

    def test_hits_before_the_grab_dont_count(self):
        p = Probe(prefs={FREE_PRACTICE: 1, SUCCESS: HIT})
        p.place(-20, MAIN_LEFT, STAND, 1)
        p.set_state(ASID_DAMAGEN1, hitlag=4)
        p.run(10)
        p.set_state(ASID_CAPTUREPULLEDHI)
        p.run(1)
        self.assertEqual((p.info(0), p.info(22), p.info(21)), (0, 0, 0))

    def test_timed_runs_count_throws(self):
        p = self.thrown(timed=True)
        self.assertEqual((p.info(0), p.state()), (1, ASID_WAIT))

    def test_a_command_grab_still_sends_the_cpu_away(self):
        p = Probe(prefs={FREE_PRACTICE: 1, SUCCESS: HIT})
        p.place(-20, MAIN_LEFT, STAND, 1)
        p.set_state(ASID_CAPTURECAPTAIN)
        p.run(5)
        p.set_state(ASID_DAMAGEFLYN, air=1)
        p.run(1)
        self.assertEqual((p.state(), p.info(0), p.info(23)), (ASID_WAIT, 0, 0))


def juggled(landing=None, getup=-1, x=10, height=30, you=-40, facing=1, di=DI_NONE):
    """Hit mode: grabbed, thrown, and falling in knockback onto open ground
    (under the top platform's right half), with its landing and a missed
    tech's getup picked, or (None, -1) left to the event, and CPU DI."""
    p = Probe(prefs={FREE_PRACTICE: 1, SUCCESS: HIT, DI: di})
    p.place(-20, MAIN_LEFT, STAND, 1)
    p.set_state(ASID_CAPTUREPULLEDHI)
    p.run(1)
    p.set_state(ASID_THROWNF)
    p.run(10)
    p.set_state(ASID_DAMAGEFLYN, air=1)
    p.m.call('TestGrabsMoveCPU', int(x * 1000), int(height * 1000))
    p.m.call('TestGrabsHuman', int(you * 1000))
    p.m.call('TestGrabsFace', facing)
    p.run(1)
    if landing is not None:
        p.m.call('TestGrabsPlan', landing, getup)
    p.m.call('TestGrabsResetStats')
    return p


class GrabTechChaseTests(unittest.TestCase):
    """Hit mode's juggle: a knockback landing techs or misses with Techchase's
    Tyro setting's chances, and the CPU stays for its tech or getup."""
    @classmethod
    def setUpClass(cls): build()

    def land(self, p):
        frames = 0
        while p.state() == ASID_DAMAGEFLYN:
            p.run(1)
            frames += 1
            self.assertLess(frames, 200)
        return frames

    def chase(self, p):
        """Lands, then runs out the tech or the getup: the frames it stayed."""
        self.land(p)
        p.run(1)  # The event's think sees the landing.
        frames = 1
        while p.info(24):
            self.assertEqual(p.info(23), 1)
            p.run(1)
            frames += 1
            self.assertLess(frames, 400)
        return frames

    def run_out_late(self, p):
        """Up again and unhit, the CPU could act: half a second of it, pulsing
        red, then it's back somewhere else, uncounted."""
        self.assertEqual((p.state(), p.info(23), p.info(33), p.info(34)), (ASID_WAIT, 1, 1, 1))
        p.run(GRAB_WINDOW - 1)
        self.assertEqual((p.state(), p.info(23), p.info(33), p.info(34)), (ASID_WAIT, 1, GRAB_WINDOW, 1))
        p.run(1)
        self.assertEqual((p.state(), p.info(23), p.info(33), p.info(34)), (ASID_WAIT, 0, 0, 0))
        self.assertEqual((p.info(0), p.info(22)), (0, 0))

    def test_the_landings_and_getups_follow_techchases_tyro_setting(self):
        p = Probe()
        landings = [p.m.call('TestGrabsPick', 0, roll) for roll in range(100)]
        self.assertEqual([landings.count(i) for i in range(4)], [60, 15, 15, 10])
        getups = [p.m.call('TestGrabsPick', 1, roll) for roll in range(100)]
        self.assertEqual([getups.count(i) for i in range(4)], [25, 25, 25, 25])

    def test_each_flight_picks_its_landing_once_and_each_hit_picks_again(self):
        p = juggled()
        picks = set()
        while p.state() == ASID_DAMAGEFLYN:
            picks.add(p.info(25))
            p.run(1)
        self.assertEqual(len(picks), 1)
        self.assertIn(picks.pop(), range(4))
        # Hit after hit, high up: each picks anew, with the Tyro setting's chances.
        p = juggled(height=3000)
        picks = []
        for _ in range(400):
            p.set_state(ASID_DAMAGEFLYN, hitlag=2, air=1)
            p.run(1)
            picks.append(p.info(25))
            p.run(2)  # Its hitlag runs out.
        self.assertEqual(p.info(22), 400)
        for landing, chance in zip(range(4), (.6, .15, .15, .1)):
            self.assertAlmostEqual(picks.count(landing) / len(picks), chance, delta=.06)

    def test_each_landing_techs_as_picked_and_the_cpu_stays_for_it(self):
        for you in (-40, 60):
            for facing in (1, -1):
                away = 1 if 10 > you else -1
                for landing in (IN_PLACE, AWAY, TOWARD, MISS):
                    with self.subTest(you=you, facing=facing, landing=landing):
                        p = juggled(landing, you=you, facing=facing)
                        frames = self.chase(p)
                        first, moved = p.stat(32), p.stat(33) / 1000
                        if landing == IN_PLACE:
                            self.assertEqual((first, moved), (ASID_PASSIVE, 0))
                        elif landing == MISS:
                            self.assertEqual(first, ASID_DOWNBOUNDU)
                        else:
                            self.assertIn(first, (ASID_PASSIVESTANDF, ASID_PASSIVESTANDB))
                            self.assertGreater(moved * (away if landing == AWAY else -away), 10)
                        self.assertGreaterEqual(frames, 26)  # The whole tech or getup.
                        self.run_out_late(p)

    def test_a_missed_tech_gets_up_as_picked_right_away(self):
        for facing in (1, -1):
            for getup in (STAND_UP, ROLL_AWAY, ROLL_TOWARD, GETUP_ATTACK):
                with self.subTest(facing=facing, getup=getup):
                    p = juggled(MISS, getup, facing=facing)
                    frames = self.chase(p)
                    state, moved = p.stat(34), p.stat(33) / 1000
                    if getup == STAND_UP:
                        self.assertEqual(state, ASID_DOWNSTANDU)
                    elif getup == GETUP_ATTACK:
                        self.assertEqual(state, ASID_DOWNATTACKU)
                    else:
                        self.assertIn(state, (ASID_DOWNFOWARDU, ASID_DOWNBACKU))
                        self.assertGreater(moved * (1 if getup == ROLL_AWAY else -1), 10)  # You're on the left.
                    self.assertLessEqual(frames, 26 + 30 + 3)  # DownBound, then the getup.
                    self.run_out_late(p)

    def test_a_missed_tech_picks_its_getup_as_it_lands(self):
        p = juggled(MISS)
        self.land(p)
        p.run(1)
        self.assertIn(p.info(26), range(4))

    def test_the_tech_reads_l_r_as_pressed_and_a_missed_tech_as_never(self):
        for landing in (IN_PLACE, AWAY, TOWARD, MISS):
            with self.subTest(landing=landing):
                p = juggled(landing)
                p.run(1)
                # The input proc counts the frame since the press.
                expect = (255,) if landing == MISS else (TECH_PRESS_AGO + 1, 255)
                self.assertEqual((p.info(27), p.info(28))[:len(expect)], expect)

    def test_a_tech_rolls_stick_goes_out_only_as_it_lands(self):
        for landing, stick in ((AWAY, TECH_STICK), (TOWARD, -TECH_STICK), (IN_PLACE, 0), (MISS, 0)):
            with self.subTest(landing=landing):
                p = juggled(landing, height=40)
                sticks = []
                while p.state() == ASID_DAMAGEFLYN:
                    p.run(1)
                    sticks.append(p.info(11))
                held = [i for i, s in enumerate(sticks) if s]
                if not stick:
                    self.assertEqual(held, [])
                    continue
                # Nothing on the way down, then away from you (on the left) or toward
                # you for its last frames, right through the landing.
                self.assertEqual({sticks[i] for i in held}, {stick})
                self.assertEqual(held, list(range(len(sticks) - len(held), len(sticks))))
                self.assertLessEqual(len(held), LAND_FRAMES + 1)

    def test_a_tech_rolls_stick_stays_out_in_hitstun_but_never_in_hitlag(self):
        p = juggled(AWAY, height=3000)
        p.m.call('TestGrabsFall', 0, 1)  # In hitstun, which doesn't drift.
        p.run(1)
        self.assertEqual(p.info(11), TECH_STICK)
        p.set_state(ASID_DAMAGEFLYN, hitlag=5, air=1)  # Another hit,
        p.m.call('TestGrabsFall', 0, 1)
        p.run(1)
        p.m.call('TestGrabsPlan', AWAY, -1)  # with the same landing picked.
        sticks = [p.info(11)]
        for _ in range(4):
            p.run(1)
            sticks.append(p.info(11))
        self.assertEqual(sticks, [0, 0, 0, 0, TECH_STICK])  # Its hitlag, then out again.

    def test_a_hit_on_the_tech_or_the_knockdown_counts_and_the_juggle_goes_on(self):
        for landing in (IN_PLACE, MISS):
            with self.subTest(landing=landing):
                p = juggled(landing)
                self.land(p)
                p.run(10)  # Partway into the tech, or still down.
                self.assertEqual(p.info(24), 1)
                p.set_state(ASID_DAMAGEN1, hitlag=4)
                p.run(1)
                self.assertEqual((p.info(0), p.info(22), p.info(24), p.info(23), p.info(25)), (1, 1, 0, 1, -1))
                p.run(2 + GRAB_WINDOW)  # Then the window, as after any hit on the ground.
                self.assertEqual((p.state(), p.info(23)), (ASID_DAMAGEN1, 1))
                p.run(1)
                self.assertEqual((p.state(), p.info(23), p.info(0)), (ASID_WAIT, 0, 1))

    def test_a_hit_on_a_cpu_lying_down_counts_and_keeps_the_chase(self):
        p = juggled(MISS)
        self.land(p)
        p.set_state(ASID_DOWNDAMAGEU, hitlag=4)  # A jab that keeps it down.
        p.run(1)
        self.assertEqual((p.info(0), p.info(22), p.info(24), p.info(23)), (1, 1, 1, 1))

    def test_a_tech_regrabbed_starts_the_juggle_over_at_its_throw(self):
        p = juggled(IN_PLACE)
        self.land(p)
        p.run(30)  # The tech's end, which a grab can catch.
        p.set_state(ASID_CAPTUREPULLEDHI)
        p.run(1)
        self.assertEqual((p.info(23), p.info(24), p.info(0)), (0, 0, 0))
        p.set_state(ASID_THROWNF)
        p.run(10)
        p.set_state(ASID_DAMAGEFLYN, hitlag=3, air=1)  # Its throw lets go, with hitlag:
        p.m.call('TestGrabsMoveCPU', 10000, 30000)
        p.run(1)
        self.assertEqual((p.info(23), p.info(22), p.info(0)), (1, 0, 0))  # not a hit.
        self.assertIn(p.info(25), range(4))  # A new landing.

    def test_throw_mode_and_hits_before_a_grab_never_tech(self):
        for prefs in ({}, {FREE_PRACTICE: 1}, {FREE_PRACTICE: 1, SUCCESS: HIT}):
            with self.subTest(prefs=prefs):
                p = Probe(prefs=prefs)
                p.place(10, MAIN_RIGHT, STAND, 1)
                p.set_state(ASID_DAMAGEFLYN, hitlag=4, air=1)  # Hit, not grabbed.
                p.m.call('TestGrabsMoveCPU', 10000, 20000)
                p.m.call('TestGrabsResetStats')
                while p.state() == ASID_DAMAGEFLYN:
                    p.run(1)
                    self.assertEqual((p.info(11), p.info(27)), (0, 255))
                self.assertEqual(p.stat(32), ASID_DOWNBOUNDU)  # Knocked down,
                p.run(1)
                self.assertEqual((p.state(), p.info(21)), (ASID_WAIT, 0))  # and sent away at once.


class GrabDITests(unittest.TestCase):
    """Hit mode's CPU DI: None, or Random, a full stick one of its eight ways,
    picked for each throw and each hit and held through it. The probe reads it
    where the game does: as the throw lets go (TestGrabsRelease) and as the
    hitlag ends, from the stick of the last input proc."""
    @classmethod
    def setUpClass(cls): build()

    def stick(self, p): return p.info(11), p.info(29)  # cpu.lstickX and Y.
    def processed(self, p): return p.info(30), p.info(31)  # input.lstick, in the same units.
    def read(self, p): return p.stat(37), p.stat(38)  # The last stick the game DI'd with.

    def hit_mode(self, di=DI_RANDOM):
        p = Probe(prefs={FREE_PRACTICE: 1, SUCCESS: HIT, DI: di})
        p.place(-20, MAIN_LEFT, STAND, 1)
        return p

    def grab(self, p):
        """A standard grab: the stick stays out of the hold, where it would mash."""
        p.set_state(ASID_CAPTUREPULLEDHI)
        p.run(1)
        p.set_state(ASID_CAPTUREWAITHI)
        for _ in range(5):
            p.run(1)
            self.assertEqual((self.stick(p), p.info(10)), ((0, 0), 0))

    def throw(self, p, state=ASID_THROWNF, frames=8):
        """Thrown for `frames`, then let go: the sticks held, and what the release read."""
        p.set_state(state)
        held = []
        for _ in range(frames):
            p.run(1)
            held.append(self.stick(p))
            self.assertEqual(self.processed(p), held[-1])  # What the input proc decodes.
        p.m.call('TestGrabsRelease')
        return held, self.read(p)

    def hits(self, p, count, hitlag=4):
        """`count` hits high in the air, each run out: the way each held."""
        ways = []
        for _ in range(count):
            p.set_state(ASID_DAMAGEFLYN, hitlag=hitlag, air=1)
            held = []
            for _ in range(hitlag - 1):  # The think sees hitlag - 1 frames of it.
                p.run(1)
                held.append(self.stick(p))
                self.assertEqual(self.processed(p), held[-1])
            self.assertEqual(len(set(held)), 1)  # One way, held through the hitlag.
            reads = p.stat(36)
            p.run(1)  # The hitlag ends: the game DIs with what was held,
            self.assertEqual((p.stat(36), self.read(p)), (reads + 1, held[0]))
            self.assertNotIn(self.stick(p), DI_WAYS)  # and the stick is let go.
            ways.append(held[0])
        return ways

    def assertEvenly(self, ways):
        self.assertEqual(set(ways), DI_WAYS)
        for way in DI_WAYS:
            self.assertAlmostEqual(ways.count(way) / len(ways), 1 / 8, delta=.07)

    def test_cpu_di_is_none_or_random_and_saved(self):
        p = self.hit_mode(DI_NONE)
        self.assertEqual([p.string(p.m.call('TestGrabsRowChoice', OPT_DI, i)) for i in range(2)], ['None', 'Random'])
        self.assertEqual((p.row(OPT_DI), p.row(OPT_DI, 2)), (DI_NONE, 1))  # The default; OPTKIND_STRING.
        for choice in (DI_RANDOM, DI_NONE, DI_RANDOM):
            p.menu(OPT_DI, choice)
            self.assertEqual(p.pref(DI), choice)
        self.assertEqual(self.hit_mode(DI_RANDOM).row(OPT_DI), DI_RANDOM)

    def test_cpu_di_is_greyed_out_but_in_hit_mode(self):
        self.assertEqual(Probe(prefs={SUCCESS: HIT, DI: DI_RANDOM}).row(OPT_DI, 1), 1)  # Timed runs.
        p = Probe(prefs={FREE_PRACTICE: 1})
        self.assertEqual(p.row(OPT_DI, 1), 1)  # Throw.
        p.menu(OPT_SUCCESS, HIT)
        self.assertEqual(p.row(OPT_DI, 1), 0)
        p.menu(OPT_SUCCESS, THROW)
        self.assertEqual(p.row(OPT_DI, 1), 1)
        self.assertEqual(self.hit_mode().row(OPT_DI, 1), 0)

    def test_random_di_holds_one_way_through_each_throw_and_the_release_takes_it(self):
        p = self.hit_mode()
        ways = []
        for _ in range(240):
            self.grab(p)  # Each a regrab in the juggle, as a tech regrabbed is.
            held, read = self.throw(p)
            self.assertEqual(len(set(held)), 1)
            self.assertIn(held[0], DI_WAYS)
            self.assertEqual(read, held[0])
            ways.append(read)
            p.m.call('TestGrabsMoveCPU', -20000, 200000)
            p.run(1)
            self.assertEqual(self.stick(p), (0, 0))  # Let go, it holds nothing.
        self.assertEvenly(ways)

    def test_dks_cargo_throws_take_di_but_carrying_doesnt(self):
        p = self.hit_mode()
        p.set_state(ASID_CAPTUREPULLEDHI)
        p.run(1)
        p.set_state(ASID_SHOULDEREDWAIT)  # Carried, where the stick would mash.
        for _ in range(5):
            p.run(1)
            self.assertEqual(self.stick(p), (0, 0))
        held, read = self.throw(p, ASID_THROWNFF)
        self.assertEqual((len(set(held)), read), (1, held[0]))
        self.assertIn(read, DI_WAYS)

    def test_random_di_holds_one_way_through_each_hit_and_the_hitlags_end_takes_it(self):
        p = juggled(height=3000, di=DI_RANDOM)
        self.assertEvenly(self.hits(p, 240))
        self.assertEqual(p.stat(35), 0)  # Held all along, never a flick: never a Smash DI.

    def test_a_hit_during_anothers_hitlag_picks_again_without_a_smash_di(self):
        p = juggled(height=3000, di=DI_RANDOM)
        changed = 0
        for _ in range(60):
            p.set_state(ASID_DAMAGEFLYN, hitlag=8, air=1)
            p.run(2)
            first = self.stick(p)
            p.set_state(ASID_DAMAGEFLYN, hitlag=8, air=1)  # More hitlag than was left: a new hit.
            p.run(1)
            second = self.stick(p)
            self.assertIn(second, DI_WAYS)
            changed += first != second
            p.run(8)
        self.assertGreater(changed, 40)  # About 7 in 8 pick another way.
        self.assertEqual(p.stat(35), 0)

    def test_random_di_takes_hits_before_the_grab_but_not_shield_hits(self):
        p = self.hit_mode()
        p.set_state(ASID_DAMAGEN1, hitlag=4)
        p.run(1)
        way = self.stick(p)
        self.assertIn(way, DI_WAYS)
        p.run(3)
        self.assertEqual((self.read(p), p.stat(35)), (way, 0))
        p = self.hit_mode()
        p.m.call('TestGrabsShield', 1)
        p.run(20)
        p.set_state(ASID_GUARDSETOFF, hitlag=6)
        for _ in range(6):
            p.run(1)
            self.assertEqual(self.stick(p), (0, 0))  # In shield, holding R and nothing else.
            self.assertTrue(p.info(10) & PAD_TRIGGER_R)

    def test_none_throw_mode_and_timed_runs_never_di(self):
        for prefs in ({FREE_PRACTICE: 1, SUCCESS: HIT, DI: DI_NONE},  # None.
                      {FREE_PRACTICE: 1, SUCCESS: THROW, DI: DI_RANDOM},  # Throw mode,
                      {SUCCESS: HIT, DI: DI_RANDOM}):  # and timed runs, whatever's saved.
            with self.subTest(prefs=prefs):
                p = Probe(prefs=prefs)
                p.place(-20, MAIN_LEFT, STAND, 1)
                p.set_state(ASID_DAMAGEN1, hitlag=4)  # A hit before the grab,
                for _ in range(4):
                    p.run(1)
                    self.assertEqual(self.stick(p), (0, 0))
                self.assertEqual((p.stat(36), self.read(p)), (1, (0, 0)))
                self.grab(p)  # and the throw.
                held, read = self.throw(p, ASID_THROWNHI)
                self.assertEqual((set(held), read), ({(0, 0)}, (0, 0)))

    def test_a_juggle_with_random_di_still_techs_as_picked(self):
        for landing, stick in ((AWAY, TECH_STICK), (TOWARD, -TECH_STICK)):
            with self.subTest(landing=landing):
                p = juggled(landing, height=3000, di=DI_RANDOM)
                p.set_state(ASID_DAMAGEFLYN, hitlag=5, air=1)
                p.m.call('TestGrabsFall', 0, 1)  # In hitstun.
                p.run(1)
                p.m.call('TestGrabsPlan', landing, -1)
                sticks = [self.stick(p)]
                for _ in range(4):
                    p.run(1)
                    sticks.append(self.stick(p))
                self.assertEqual(len(set(sticks[:4])), 1)  # Its DI through the hitlag,
                self.assertIn(sticks[0], DI_WAYS)
                self.assertEqual(sticks[4], (stick, 0))  # then the tech roll's stick.


class GrabLateTests(unittest.TestCase):
    """Hit mode: once the juggled CPU could act again, it pulses red, Late
    Frames OSD counts the frames under the match timer, and half a second of it
    sends the CPU away. A hit or grab stops the count and shows it a second."""
    @classmethod
    def setUpClass(cls): build()

    def late_juggle(self, prefs=None):
        """Grabbed, thrown, and in the throw's hitstun high up, where it can't act."""
        p = Probe(prefs={FREE_PRACTICE: 1, SUCCESS: HIT, **(prefs or {})})
        p.place(-20, MAIN_LEFT, STAND, 1)
        p.set_state(ASID_CAPTUREPULLEDHI)
        p.run(1)
        p.set_state(ASID_THROWNF)
        p.run(10)
        p.set_state(ASID_DAMAGEFLYN, air=1)
        p.m.call('TestGrabsMoveCPU', 10000, 3000000)
        p.m.call('TestGrabsFall', 0, 1)
        p.run(1)
        return p

    def overlay(self, p):
        """The CPU's timing overlay: None, or its color and alpha."""
        if not p.info(34): return None
        color = p.info(35) & 0xFFFFFFFF
        return (color >> 24, color >> 16 & 255, color >> 8 & 255), color & 255

    def drawn(self, p):
        """What Late Frames OSD draws: None, or its text, its rect's y and its color."""
        if not p.m.call('TestGrabsDraw'): return None
        color = p.m.call('TestGrabsDrawn', 2)
        return (p.string(p.m.call('TestGrabsDrawn', 0)), signed(p.m.call('TestGrabsDrawn', 1)) / 1000,
                (color >> 24, color >> 16 & 255, color >> 8 & 255, color & 255))

    def test_late_frames_osd_is_on_saved_and_greyed_out_but_in_hit_mode(self):
        p = Probe(prefs={FREE_PRACTICE: 1})
        self.assertEqual((p.row(OPT_LATE_OSD), p.row(OPT_LATE_OSD, 1), p.row(OPT_LATE_OSD, 2)), (1, 1, 5))  # Toggle.
        p.menu(OPT_SUCCESS, HIT)
        self.assertEqual(p.row(OPT_LATE_OSD, 1), 0)
        for value in (0, 1, 0):
            p.menu(OPT_LATE_OSD, value)
            self.assertEqual(p.pref(LATE_OSD), value)
        self.assertEqual(Probe(prefs={FREE_PRACTICE: 1, SUCCESS: HIT, LATE_OSD: 0}).row(OPT_LATE_OSD), 0)
        self.assertEqual(Probe(prefs={SUCCESS: HIT}).row(OPT_LATE_OSD, 1), 1)  # Timed runs.

    def test_it_isnt_late_in_hitstun_and_pulses_red_from_the_first_frame_it_could_act(self):
        p = self.late_juggle()
        for _ in range(20):
            p.run(1)
            self.assertEqual((p.info(33), self.overlay(p), self.drawn(p)), (0, None, None))
        p.m.call('TestGrabsFall', 0, 0)  # Its hitstun ends.
        alphas = []
        for frame in range(1, 17):
            p.run(1)
            color, alpha = self.overlay(p)
            self.assertEqual((p.info(33), color), (frame, LATE_RED))
            self.assertEqual(self.drawn(p), ('Late %df' % frame, LATE_Y, LATE_RED + (255,)))
            alphas.append(alpha)
        self.assertEqual(alphas, LATE_PULSE * 2)

    def test_half_a_second_of_it_sends_the_cpu_away_even_high_in_the_air(self):
        p = self.late_juggle()
        p.m.call('TestGrabsFall', 0, 0)
        p.run(GRAB_WINDOW)
        self.assertEqual((p.state(), p.info(23), p.info(33)), (ASID_DAMAGEFLYN, 1, GRAB_WINDOW))
        self.assertGreater(p.spot()[1], 2000)  # Still far up.
        p.run(1)
        self.assertEqual((p.state(), p.info(23), p.info(33), self.overlay(p), p.info(0)), (ASID_WAIT, 0, 0, None, 0))
        self.assertEqual(self.drawn(p)[0], 'Late %df' % GRAB_WINDOW)  # The count that ran out stays up.

    def test_a_hit_stops_the_count_and_shows_how_late_it_was_for_a_second(self):
        p = self.late_juggle()
        p.m.call('TestGrabsFall', 0, 0)
        p.run(7)
        p.set_state(ASID_DAMAGEFLYN, hitlag=4, air=1)
        p.m.call('TestGrabsFall', 0, 1)
        p.run(1)
        self.assertEqual((p.info(33), self.overlay(p), p.info(36), p.info(22), p.info(0)), (0, None, 7, 1, 1))
        self.assertEqual(self.drawn(p), ('Late 7f', LATE_Y, LATE_RED + (255,)))
        p.run(LATE_SHOW_FRAMES - 1)
        self.assertEqual(self.drawn(p)[0], 'Late 7f')
        p.run(1)
        self.assertIsNone(self.drawn(p))
        self.assertEqual(p.info(23), 1)  # The juggle goes on.

    def test_a_hit_before_it_could_act_isnt_late(self):
        p = self.late_juggle()
        p.set_state(ASID_DAMAGEFLYN, hitlag=4, air=1)
        p.m.call('TestGrabsFall', 0, 1)
        p.run(1)
        self.assertEqual((p.info(22), p.info(36), self.overlay(p), self.drawn(p)), (1, 0, None, None))

    def test_a_late_regrab_stops_the_count_too(self):
        p = self.late_juggle()
        p.m.call('TestGrabsFall', 0, 0)
        p.run(5)
        p.set_state(ASID_CAPTUREPULLEDHI)
        p.run(1)
        self.assertEqual((p.info(33), self.overlay(p), p.info(36)), (0, None, 5))
        self.assertEqual(self.drawn(p)[0], 'Late 5f')

    def test_a_tech_drops_the_count_until_the_cpu_could_act_again(self):
        p = juggled(IN_PLACE)  # Thrown with no hitstun: it could act on the way down.
        self.assertGreater(p.info(33), 0)
        while p.state() == ASID_DAMAGEFLYN:
            self.assertEqual(self.overlay(p)[0], LATE_RED)
            p.run(1)
        p.run(1)  # The think sees the tech.
        while p.info(24):
            self.assertEqual((p.info(33), self.overlay(p), self.drawn(p)), (0, None, None))
            p.run(1)
        self.assertEqual((p.state(), p.info(33), self.drawn(p)[0]), (ASID_WAIT, 1, 'Late 1f'))

    def test_with_the_osd_off_it_still_pulses_and_goes(self):
        p = self.late_juggle({LATE_OSD: 0})
        p.m.call('TestGrabsFall', 0, 0)
        p.run(5)
        self.assertEqual((p.info(33), self.overlay(p)[0], self.drawn(p)), (5, LATE_RED, None))
        p.run(GRAB_WINDOW - 4)
        self.assertEqual((p.info(23), self.drawn(p)), (0, None))

    def test_the_osd_goes_a_row_lower_with_the_grab_count_under_the_timer_and_hides_while_paused(self):
        p = self.late_juggle()
        p.m.call('TestGrabsFall', 0, 0)
        p.run(3)
        self.assertAlmostEqual(self.drawn(p)[1], LATE_Y, delta=.002)
        p.menu(OPT_COUNTER, COUNTER_UNDER_TIMER)
        self.assertAlmostEqual(self.drawn(p)[1], LATE_Y - LATE_UNDER_COUNTER, delta=.002)
        p.m.call('TestGrabsPause', 1)
        self.assertIsNone(self.drawn(p))
        p.run(5)  # Paused, nothing counts.
        p.m.call('TestGrabsPause', 0)
        self.assertEqual(self.drawn(p)[0], 'Late 3f')

    def test_throw_mode_and_hits_before_the_grab_are_never_late(self):
        for prefs in ({FREE_PRACTICE: 1}, {FREE_PRACTICE: 1, SUCCESS: HIT}):
            with self.subTest(prefs=prefs):
                p = Probe(prefs=prefs)
                p.place(-20, MAIN_LEFT, STAND, 1)
                p.set_state(ASID_DAMAGEN1, hitlag=4)  # A hit before any grab: its own window.
                for _ in range(GRAB_WINDOW):
                    p.run(1)
                    self.assertEqual((p.info(33), self.overlay(p), self.drawn(p)), (0, None, None))


# The native tech and getup checks the event's inputs go through, run from the
# local DOL (sizes from GTME01.map) with the game's r2 constants, r13 and
# PlCo.dat's common data. Their state entries and the stick angle go to the
# probe; every other call out of them is stubbed to return 0 (no hammer, no
# C-stick, no animation frames left).
R2, R13 = 0x804DF9E0, 0x804DB6A0
NATIVE = {
    0x800986B0: 0x7C,   # Tech_L/RCheck&FrameWindowCheck
    0x8009872C: 0xA4,   # Interrupt_NeutralTech
    0x80098928: 0xAC,   # Interrupt_TechRollF/B
    0x80097DC0: 0x5C,   # AnimationInterrupt_DownBoundU/D
    0x8009802C: 0x50,   # Interrupt_AS_DownWaitU/D
    0x800980BC: 0xA4,   # Interrupt_DownStandU/D
    0x80098214: 0x110,  # Interrupt_DownBack/ForwardU/D
    0x80098400: 0xD4,   # Interrupt_DownAttackU/D_FromDownBound
    0x800984D4: 0x98,   # Interrupt_DownAttackU/D_FromDownBoundWait
}
ENTRIES = {
    0x800987D0: 'TestGrabsNativePassive',   # AS_199_Passive_NeutralTech
    0x800989D4: 'TestGrabsNativeEnter',     # AS_200/201_PassiveStandF/B (r4: the state)
    0x80098160: 'TestGrabsNativeEnter',     # DownStand's entry
    0x80098324: 'TestGrabsNativeEnter',     # a getup roll's
    0x8009856C: 'TestGrabsNativeEnter',     # DownAttack's
    0x80097E8C: 'TestGrabsNativeDownWait',  # AS_184/192_DownWaitU/D
    0x8007D964: 'TestGrabsStickAngle',      # Joystick_Angle_Retrieve_AbsX
}


class NativeChecks:
    def __init__(self, p):
        self.m = m = p.m
        dol = (ROOT / 'build/Start.dol').read_bytes()
        def native(address, size):
            for i in range(18):
                offset, base, length = [struct.unpack_from('>I', dol, at + i * 4)[0] for at in (0, 0x48, 0x90)]
                if base <= address and address + size <= base + length:
                    return dol[offset + address - base:offset + address - base + size]
            raise AssertionError('Native routine missing from DOL')
        def jump(address, target):
            m.cpu.mem_write(address, struct.pack('>4I', 0x3D800000 | target >> 16,
                0x618C0000 | target & 0xFFFF, 0x7D8903A6, 0x4E800420))
        for address, size in NATIVE.items():
            code = native(address, size)
            for i in range(0, size, 4):
                word = struct.unpack_from('>I', code, i)[0]
                op, base, displacement = word >> 26, word >> 16 & 31, word & 0xFFFF
                if op == 18 and word & 1:  # bl
                    target = address + i + (word & 0x03FFFFFC) - (0x04000000 if word & 0x02000000 else 0)
                    if target in ENTRIES:
                        jump(target, m.symbols[ENTRIES[target]])
                    elif target not in NATIVE:
                        m.cpu.mem_write(target, bytes.fromhex('386000004e800020'))
                elif op in (32, 48, 50) and base == 2:  # lwz/lfs/lfd from r2's constants
                    constant = R2 + displacement - (0x10000 if displacement & 0x8000 else 0)
                    m.cpu.mem_write(constant, native(constant, 8 if op == 50 else 4))
        for address, size in NATIVE.items():
            m.cpu.mem_write(address, native(address, size))
        common = struct.unpack('>I', m.cpu.mem_read(R13 - 0x514C, 4))[0]  # stc_ftcommon
        m.cpu.mem_write(common, plco_common())

    def run(self, address):
        """Runs a native check on the CPU: the state it entered, or None."""
        m = self.m
        m.call('TestGrabsNativeEntered')
        cpu = m.call('TestGrabsCPU')
        m.symbols['native'] = address
        saved = m.cpu.reg_read(reg.UC_PPC_REG_2)
        m.cpu.reg_write(reg.UC_PPC_REG_2, R2)
        m.cpu.reg_write(reg.UC_PPC_REG_13, R13)
        try:
            m.call('native', cpu)
        finally:
            m.cpu.reg_write(reg.UC_PPC_REG_2, saved)
        state = signed(m.call('TestGrabsNativeEntered'))
        return None if state < 0 else state


FORWARD, BACK = (ASID_DOWNFOWARDU, ASID_DOWNFOWARDD), (ASID_DOWNBACKU, ASID_DOWNBACKD)


class GrabNativeChaseTests(unittest.TestCase):
    """The event's tech and getup inputs, through the game's own checks."""
    @classmethod
    def setUpClass(cls): build()

    def landing(self, landing, you, facing):
        p = juggled(landing, you=you, facing=facing)
        checks = NativeChecks(p)
        p.m.call('TestGrabsMoveCPU', 10000, 2000)  # About to land.
        p.m.call('TestGrabsFall', -2000, 0)
        p.m.call('TestGrabsThinkInput')
        # ftCo_80090184's order: a tech roll, a tech in place, or else a knockdown.
        return checks.run(0x80098928) or checks.run(0x8009872C) or ASID_DOWNBOUNDU

    def test_the_games_tech_check_takes_each_landing(self):
        for you in (-40, 60):
            for facing in (1, -1):
                away = 1 if 10 > you else -1
                for landing in (IN_PLACE, AWAY, TOWARD, MISS):
                    with self.subTest(you=you, facing=facing, landing=landing):
                        if landing in (AWAY, TOWARD):
                            way = away if landing == AWAY else -away
                            expect = ASID_PASSIVESTANDF if way == facing else ASID_PASSIVESTANDB
                        else:
                            expect = ASID_PASSIVE if landing == IN_PLACE else ASID_DOWNBOUNDU
                        self.assertEqual(self.landing(landing, you, facing), expect)

    def knockdown(self, getup, facing):
        p = juggled(MISS, getup, facing=facing)
        checks = NativeChecks(p)
        while p.state() == ASID_DAMAGEFLYN:
            p.run(1)
        self.assertEqual(p.state(), ASID_DOWNBOUNDU)
        return p, checks

    def getup_ways(self, getup, facing):
        """You're on the left: away is right."""
        way = 1 if getup == ROLL_AWAY else -1
        return FORWARD if way == facing else BACK

    def test_the_games_getup_checks_take_each_getup_from_a_knockdown(self):
        for facing in (1, -1):
            for getup in (STAND_UP, ROLL_AWAY, ROLL_TOWARD, GETUP_ATTACK):
                with self.subTest(facing=facing, getup=getup):
                    p, checks = self.knockdown(getup, facing)
                    for _ in range(3):
                        p.m.call('TestGrabsThinkInput')
                    entered = checks.run(0x80097DC0)  # DownBound's last frame.
                    if getup == GETUP_ATTACK:
                        self.assertEqual(entered, ASID_DOWNATTACKU)
                    elif getup == STAND_UP:
                        # Standing waits for DownWait, which takes it at once.
                        self.assertEqual(entered, ASID_DOWNWAITU)
                        p.m.call('TestGrabsDownWait')
                        p.m.call('TestGrabsThinkInput')
                        self.assertEqual(checks.run(0x8009802C), ASID_DOWNSTANDU)
                    else:
                        self.assertIn(entered, self.getup_ways(getup, facing))

    def test_the_games_getup_checks_take_each_getup_from_down_wait(self):
        # As after a jab that keeps it down: the getup comes from DownWait.
        for facing in (1, -1):
            for getup in (STAND_UP, ROLL_AWAY, ROLL_TOWARD, GETUP_ATTACK):
                with self.subTest(facing=facing, getup=getup):
                    p, checks = self.knockdown(getup, facing)
                    p.m.call('TestGrabsDownWait')
                    entered = None
                    for _ in range(2):  # A comes afresh every other frame.
                        p.m.call('TestGrabsThinkInput')
                        entered = entered or checks.run(0x8009802C)
                    if getup == STAND_UP:
                        self.assertEqual(entered, ASID_DOWNSTANDU)
                    elif getup == GETUP_ATTACK:
                        self.assertEqual(entered, ASID_DOWNATTACKU)
                    else:
                        self.assertIn(entered, self.getup_ways(getup, facing))


if __name__ == '__main__':
    unittest.main()
