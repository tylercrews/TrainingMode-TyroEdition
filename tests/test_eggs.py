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
    UNDER_TIMER: (320, 55, CENTER),
}


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


if __name__ == '__main__':
    unittest.main()
