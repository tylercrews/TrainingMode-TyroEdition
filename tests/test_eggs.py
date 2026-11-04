"""Execute the production Eggs-ercise event's Egg Counter Position."""
from pathlib import Path
import subprocess
import unittest
import test_settings as fixtures
from test_lowlaser import load_probe

ROOT = Path(__file__).resolve().parents[1]
TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT, UNDER_TIMER = range(5)
LEFT, CENTER, RIGHT = 0, 1, 2
SCORE, LABEL = 0, 1
LINE_HEIGHT = 32  # Text fills a line this tall down from trans.Y; see src/eggs.c.
SPOTS = {
    TOP_LEFT: (50, 25, LEFT),
    TOP_RIGHT: (590, 25, RIGHT),
    BOTTOM_LEFT: (50, 408, LEFT),
    BOTTOM_RIGHT: (590, 408, RIGHT),
    UNDER_TIMER: (320, 75, CENTER),
}
PLATFORM_LEFT, PLATFORM_RIGHT, PLATFORM_Y = 20, 58, 27
INSET, AIR_HEIGHT, CAMERA_TOP, AIR_CEILING = 5, 70, 150, 15
UNDER, IN_AIR = 0, 1
DIGIT_TOP = 4  # A full-size digit starts this far into its line.
TIMER_BOTTOM = 74  # The HUD timer's digits, measured from a Dolphin screenshot.


def build():
    subprocess.run([
        'C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe', '-O2', '-w',
        '-ffreestanding', '-fno-builtin', '-msdata=none', '-mcpu=750', '-nostdlib',
        '-Wl,-Ttext=0x101000,-e,TestEggsLoaded,--unresolved-symbols=ignore-all',
        'tests/eggs_probe.c', '-o', 'build/eggs-probe.elf'
    ], cwd=ROOT, check=True)


class EggCounterPositionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def start(self, position=TOP_LEFT, free_practice=0):
        self.m = fixtures.Machine()
        load_probe(self.m, 'eggs-probe.elf', 'TestEggsLoaded')
        self.m.call('TestEggsInit', position, free_practice)

    def info(self, key): return self.m.call('TestEggsInfo', key)
    def string(self, address):
        return bytes(self.m.cpu.mem_read(address, 32)).split(b'\0', 1)[0].decode()
    def spot(self, text):
        x, y, align = [self.m.call('TestEggsText', text, key) for key in range(3)]
        return x / 1000, y / 1000, align
    def assert_placed(self, position):
        x, y, align = SPOTS[position]
        self.assertEqual(self.spot(SCORE), (x, y, align))
        self.assertEqual(self.spot(LABEL), (x, y + 15, align))

    def test_top_left_is_the_original_spot_and_the_default_choice(self):
        self.start()
        self.assertEqual(self.spot(SCORE), (50, 25, LEFT))
        self.assertEqual(self.spot(LABEL), (50, 40, LEFT))
        self.assertEqual((self.info(2), self.info(3)), (TOP_LEFT, TOP_LEFT))

    def test_saved_position_places_the_score_and_label_when_the_event_starts(self):
        for position in SPOTS:
            with self.subTest(position=position):
                self.start(position)
                self.assert_placed(position)
                self.assertEqual((self.info(2), self.info(3)), (position, position))

    def test_every_position_keeps_the_pair_inside_the_screen_margin(self):
        margin = 25
        for position, (x, y, align) in SPOTS.items():
            with self.subTest(position=position):
                self.assertGreaterEqual(y, margin)
                self.assertLessEqual(y + 15 + LINE_HEIGHT, 480 - margin)
                self.assertGreaterEqual(min(x, 640 - x), margin)
        # The corners mirror each other, so moving the counter keeps its margin.
        self.assertEqual(SPOTS[BOTTOM_LEFT][1] + 15 + LINE_HEIGHT, 480 - SPOTS[TOP_LEFT][1])
        self.assertEqual(SPOTS[TOP_RIGHT][0], 640 - SPOTS[TOP_LEFT][0])

    def test_menu_row_names_each_choice_in_saved_order(self):
        self.start()
        self.assertEqual(self.string(self.m.call('TestEggsName')), 'Egg Counter Position')
        self.assertEqual((self.info(5), self.info(6)), (5, 1))
        self.assertEqual([self.string(self.m.call('TestEggsChoice', i)) for i in range(5)],
                         ['Top Left', 'Top Right', 'Bottom Left', 'Bottom Right', 'Under Timer'])

    def test_choosing_a_position_saves_it_and_moves_the_counter_at_once(self):
        self.start()
        for writes, position in enumerate((UNDER_TIMER, BOTTOM_RIGHT, TOP_RIGHT, BOTTOM_LEFT, TOP_LEFT), 1):
            self.m.call('TestEggsChoose', position)
            self.assert_placed(position)
            self.assertEqual((self.info(0), self.info(1)), (position, writes))
        self.assertEqual(self.info(7), 0)  # No restart: a timed run keeps going.

    def test_row_stays_usable_in_timed_mode_and_free_practice(self):
        for free_practice in (0, 1):
            with self.subTest(free_practice=free_practice):
                self.start(BOTTOM_RIGHT, free_practice)
                self.assertEqual(self.info(4), 0)
                self.m.call('TestEggsChoose', UNDER_TIMER)
                self.assert_placed(UNDER_TIMER)

    def test_free_practice_label_moves_with_the_score_and_stays_there(self):
        self.start(BOTTOM_RIGHT, 1)
        self.m.call('TestEggsThink')
        self.assertEqual(self.string(self.m.call('TestEggsLabel')), 'Free Practice')
        self.assertEqual(self.info(9), 1)
        self.assert_placed(BOTTOM_RIGHT)

    def test_reset_event_settings_returns_to_top_left_and_a_restart_shows_it(self):
        self.start(BOTTOM_LEFT, 1)
        self.m.call('TestEggsReset')
        self.assertEqual((self.info(0), self.info(2), self.info(3), self.info(7)), (TOP_LEFT, TOP_LEFT, TOP_LEFT, 1))
        self.assertEqual(self.info(8), 0)  # The rest of the reset still applies.
        self.m.call('TestEggsInit', self.info(0), 0)
        self.assert_placed(TOP_LEFT)


    def test_under_timer_clears_the_timer(self):
        self.start(UNDER_TIMER)
        self.assertGreater(self.spot(SCORE)[1] + DIGIT_TOP, TIMER_BOTTOM)


class EggSpawnTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def start(self, free_practice=1, platforms=1):
        self.m = fixtures.Machine()
        load_probe(self.m, 'eggs-probe.elf', 'TestEggsLoaded')
        self.m.call('TestEggsStage', self.stage_id(platforms), platforms)
        self.m.call('TestEggsInit', TOP_LEFT, free_practice)

    def stage_id(self, platforms):
        return self.m.call('TestEggsStageID', platforms)
    def signed(self, value): return value - (1 << 32) if value >> 31 else value
    def spawned(self, key): return self.signed(self.m.call('TestEggsSpawned', key)) / 1000
    def info(self, key): return self.m.call('TestEggsSpawnInfo', key)
    def spawn(self, pick, side=1):
        self.m.call('TestEggsSpawn', pick, side)
        return self.spawned(0), self.spawned(1), self.spawned(2)
    def set(self, option, value=1): self.m.call('TestEggsSpawnOption', option, value)
    def fall(self, velocity): return self.signed(self.m.call('TestEggsFall', velocity))

    def test_the_rows_are_free_practice_only_and_saved(self):
        self.start(free_practice=0)
        self.assertEqual((self.info(4), self.info(5)), (1, 1))
        self.start(free_practice=1)
        self.assertEqual((self.info(4), self.info(5)), (0, 0))
        self.set(IN_AIR)
        self.assertEqual((self.info(0), self.info(1)), (0, 1))
        self.set(UNDER)
        self.assertEqual((self.info(0), self.info(1)), (1, 1))

    def test_reset_event_settings_turns_them_off(self):
        self.start()
        self.set(UNDER); self.set(IN_AIR)
        self.m.call('TestEggsResetSpawn')
        self.assertEqual([self.info(key) for key in range(4)], [0, 0, 0, 0])

    def test_timed_runs_ignore_the_options(self):
        self.start(free_practice=0)
        self.set(UNDER); self.set(IN_AIR)
        for pick in (0, 1, 2):
            x, y, velocity = self.spawn(pick)
            self.assertGreaterEqual(velocity, 2)
            self.assertIn(y, (0, PLATFORM_Y))

    def test_with_both_off_eggs_spawn_as_before(self):
        self.start()
        for pick in (0, 1):
            x, y, velocity = self.spawn(pick)
            self.assertGreaterEqual(velocity, 2)
            self.assertIn(y, (0, PLATFORM_Y))

    def test_under_platform_eggs_are_on_the_ground_below_either_platform_with_no_hop(self):
        self.start()
        self.set(UNDER)
        for side in (1, 0, 1, 0, 1, 0):
            x, y, velocity = self.spawn(1, side)
            self.assertEqual((y, velocity), (0, 0))
            self.assertLessEqual(PLATFORM_LEFT + INSET, abs(x))
            self.assertLessEqual(abs(x), PLATFORM_RIGHT - INSET)
            self.assertEqual(x > 0, bool(side))

    def test_a_stage_without_side_platforms_spawns_a_normal_egg_instead(self):
        self.start(platforms=0)
        self.set(UNDER)
        x, y, velocity = self.spawn(1)
        self.assertEqual(y, 0)
        self.assertGreaterEqual(velocity, 2)

    def test_eggs_in_the_air_start_high_with_no_hop(self):
        self.start()
        self.set(IN_AIR)
        heights = []
        for i in range(8):
            x, y, velocity = self.spawn(1)
            ground = self.spawned(4)
            self.assertEqual(velocity, 0)
            self.assertEqual(self.spawned(3), y)
            self.assertGreaterEqual(y, ground + AIR_HEIGHT)
            self.assertLessEqual(y, CAMERA_TOP - AIR_CEILING)
            heights.append(y)
        self.assertGreater(len(set(heights)), 1)

    def test_only_eggs_in_the_air_fall_slowly(self):
        self.start()
        self.set(IN_AIR)
        self.spawn(1)
        self.assertEqual(self.fall(-3000), -600)
        self.assertEqual(self.fall(-300), -300)
        self.assertEqual(self.fall(2000), 2000)
        self.spawn(0)
        self.assertEqual(self.fall(-3000), -3000)

    def test_both_on_share_the_draw_with_normal_eggs(self):
        self.start()
        self.set(UNDER); self.set(IN_AIR)
        x, y, velocity = self.spawn(0)
        self.assertGreaterEqual(velocity, 2)
        # The far platform: eggs don't spawn right next to the last one.
        x, y, velocity = self.spawn(1, 0 if x > 0 else 1)
        self.assertEqual((y, velocity), (0, 0))
        x, y, velocity = self.spawn(2)
        self.assertEqual(velocity, 0)
        self.assertGreaterEqual(y, AIR_HEIGHT)


if __name__ == '__main__':
    unittest.main()
