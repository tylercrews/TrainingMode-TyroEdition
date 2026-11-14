"""Execute the production Escape Samus Up-B event, frame by frame."""
from pathlib import Path
import subprocess
import unittest
import test_settings as fixtures
from test_lowlaser import load_probe

ROOT = Path(__file__).resolve().parents[1]
WAIT, FALL, UPB_GROUND = 14, 29, 353
POSITION_ROW, DISTANCE_ROW, SIDE_ROW, PERCENT_ROW = range(4)
GROUND, AIR, RANDOM_POSITION = range(3)
ON_TOP, SIDE, FAR, RANDOM_DISTANCE = range(4)
RANDOM_SIDE, LEFT, RIGHT = range(3)
SETUP, ATTACK, RESULT = range(3)
NONE, ESCAPED, LAUNCHED, MISSED = range(4)
SETUP_FRAMES, RESULT_FRAMES, UPB_DONE_FRAME, GIVE_UP_FRAMES = 20, 60, 33, 240
DPAD_LEFT, DPAD_RIGHT, DPAD_DOWN = 1, 2, 4
SUCCESS_SFX, FAILURE_SFX = 303, 0xAF
GREEN, RED, WHITE = 0x28FF3C, 0xFF2828, 0xFFFFFF


def build():
    subprocess.run([
        'C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe', '-O2', '-w',
        '-ffreestanding', '-fno-builtin', '-msdata=none', '-mcpu=750', '-nostdlib',
        '-Wl,-Ttext=0x101000,-e,TestEscInit,--unresolved-symbols=ignore-all',
        'tests/escapesamus_probe.c', '-o', 'build/escapesamus-probe.elf'
    ], cwd=ROOT, check=True)


class EscapeSamusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def setUp(self):
        self.m = fixtures.Machine()
        load_probe(self.m, 'escapesamus-probe.elf', 'TestEscInit')
        self.step()  # The first frame saves the state and sets up the first attempt.

    def info(self, key):
        value = self.m.call('TestEscInfo', key)
        return value if value < 0x80000000 else value - 0x100000000
    def step(self, frames=1):
        for done in range(0, frames, 40):  # In chunks: one emulator call has an instruction budget.
            self.m.call('TestEscThink', min(frames - done, 40))
    def text(self, key):
        return bytes(self.m.cpu.mem_read(self.m.call('TestEscText', key), 24)).split(b'\0', 1)[0].decode()
    def labels(self): return [self.text(i) for i in range(4)]
    def shown(self): return [self.text(i) for i in range(4, 8)]
    def big(self): return self.text(8)
    def you(self): return self.info(2) / 10, self.info(3) / 10
    def samus(self): return self.info(4) / 10, self.info(5) / 10
    def phase(self): return self.info(1)
    def choose(self, row, value): self.m.call('TestEscChoose', row, value)
    def restart(self):
        self.m.call('TestEscRestart')
        self.step()
    def attack(self):
        """Through the setup: Samus presses up-B on its last frame."""
        self.step(SETUP_FRAMES)
        self.assertEqual((self.phase(), self.info(9)), (ATTACK, UPB_GROUND))
        self.step()
    def hit(self, damage=1, launcher=False):
        self.m.call('TestEscHit', damage, int(launcher))
        self.step()
    def finish(self):
        """Run Samus's up-B out to its last hitbox, so the attempt resolves."""
        for _ in range(UPB_DONE_FRAME + 5):
            if self.phase() == RESULT: return
            self.step()
        self.assertEqual(self.phase(), RESULT)

    # The layout.
    def test_ground_puts_you_in_the_middle_and_samus_beside_you_facing_each_other(self):
        self.assertEqual(self.you(), (0, 0))
        self.assertEqual(self.info(8), WAIT)
        self.assertEqual(self.samus(), (-6, 0))  # Random side, and HSD_Randi(2) gave 0: left.
        self.assertEqual((self.info(6), self.info(7)), (-1, 1))
        self.assertEqual(self.info(12), 0)  # Grounded.

    def test_on_the_ground_you_cant_act_until_her_first_hit(self):
        self.assertEqual(self.info(10), 0)  # Wait's interrupt callback is cleared at once,
        self.m.call('TestEscReWait')
        self.step()
        self.assertEqual(self.info(10), 0)  # and every frame after,
        self.attack()
        self.m.call('TestEscReWait')
        self.step()
        self.assertEqual(self.info(10), 0)  # into her up-B,
        self.hit()
        self.assertEqual(self.info(10), 1)  # until the hit's own state takes over.

    def test_air_holds_you_six_above_the_stage_with_your_double_jump(self):
        self.choose(POSITION_ROW, AIR)
        self.restart()
        self.assertEqual(self.you(), (0, 6))
        self.assertEqual((self.info(11), self.info(12), self.info(10), self.info(31)), (1, 1, 0, 1))
        self.step(10)
        self.assertEqual(self.you(), (0, 6))
        self.assertEqual(self.samus(), (-6, 0))  # Samus stays on the ground below.

    def test_random_position_picks_each_attempt(self):
        self.choose(POSITION_ROW, RANDOM_POSITION)
        self.m.call('TestEscRandom', 1, 50)
        self.restart()
        self.assertEqual(self.you()[1], 6)
        self.m.call('TestEscRandom', 0, 50)
        self.restart()
        self.assertEqual(self.you()[1], 0)

    def test_distances_and_sides(self):
        for distance, side, random, x in ((ON_TOP, RIGHT, 0, 0), (SIDE, RIGHT, 0, 6), (FAR, LEFT, 0, -10),
                                          (FAR, RIGHT, 0, 10), (RANDOM_DISTANCE, RIGHT, 0, 5), (SIDE, RANDOM_SIDE, 1, 6)):
            with self.subTest(distance=distance, side=side):
                self.choose(DISTANCE_ROW, distance)
                self.choose(SIDE_ROW, side)
                self.m.call('TestEscRandom', random, 50)
                self.restart()
                self.assertEqual(self.samus(), (x, 0))
                if x:
                    self.assertEqual((self.info(6), self.info(7)), (1 if x > 0 else -1, -1 if x > 0 else 1))

    def test_your_percent_is_set_each_attempt_and_samus_stays_at_zero(self):
        self.choose(PERCENT_ROW, 7)
        self.assertEqual(self.info(14), 0)  # The next attempt.
        self.restart()
        self.assertEqual((self.info(13), self.info(14)), (70, 70))
        self.attack()
        self.hit(2)
        self.step()
        self.assertEqual(self.info(14), 72)

    # Samus.
    def test_samus_up_bs_on_the_setup_s_last_frame_with_no_ai(self):
        self.step(SETUP_FRAMES - 1)
        self.assertEqual((self.phase(), self.info(9)), (SETUP, WAIT))
        self.step()
        self.assertEqual((self.phase(), self.info(9), self.info(32)), (ATTACK, UPB_GROUND, 15))

    # Hits and SDI.
    def test_a_hit_counts_and_wraps_the_native_sdi_callback(self):
        self.attack()
        self.assertEqual(self.info(15), 0)
        self.hit(2)
        self.assertEqual((self.info(15), self.info(22)), (1, 1))
        self.step(3)
        self.hit()
        self.assertEqual((self.info(15), self.info(22)), (2, 1))

    def test_only_the_native_callbacks_that_move_you_are_sdi_inputs(self):
        self.attack()
        self.m.call('TestEscSDI', 2, 1)
        self.hit()
        calls = self.info(23)
        self.step(3)  # Three hitlag frames, two SDIs.
        self.assertEqual((self.info(16), self.info(23) - calls), (2, 3))
        self.assertEqual(self.you()[0], 12)
        self.m.call('TestEscSDI', 1, -1)
        self.hit()
        self.step(3)
        self.assertEqual((self.info(16), self.you()[0]), (3, 6))
        self.assertEqual(self.big(), 'SDI 3')

    def test_no_sdi_outside_hitlag(self):
        self.attack()
        self.m.call('TestEscSDI', 3, 1)
        self.step(5)
        self.assertEqual(self.info(16), 0)

    def test_falling_out_before_the_launcher_is_an_escape(self):
        self.attack()
        self.m.call('TestEscSDI', 2, 1)
        self.hit(2)
        self.step(2)
        self.hit()
        self.finish()
        self.assertEqual((self.info(20), self.info(18), self.info(19), self.info(21)), (ESCAPED, 1, 1, SUCCESS_SFX))
        self.assertEqual(self.labels(), ['Last Attempt', 'Escapes', 'Hits Taken', 'SDI Inputs'])
        self.assertEqual(self.shown(), ['Escaped', '1/1 (100.0%)', '2', '2 (avg 2.0)'])
        self.assertEqual((self.big(), self.info(25)), ('SDI 2', GREEN))

    def test_her_last_hit_is_a_launch(self):
        self.attack()
        self.hit()
        self.step(3)
        self.hit(1, launcher=True)
        self.assertEqual(self.info(17), 1)
        self.finish()
        self.assertEqual((self.info(20), self.info(18), self.info(19), self.info(21)), (LAUNCHED, 1, 0, FAILURE_SFX))
        self.assertEqual(self.shown()[:3], ['Launched', '0/1 (0.0%)', '2'])
        self.assertEqual(self.info(25), RED)

    def test_an_attempt_she_never_hits_is_a_miss_and_isnt_counted(self):
        self.attack()
        self.finish()
        self.assertEqual((self.info(20), self.info(18)), (MISSED, 0))
        self.assertEqual(self.shown(), ['Samus Missed', '-', '0', '0'])
        self.assertEqual(self.info(25), WHITE)

    def test_the_result_stays_up_then_the_next_attempt_starts_and_keeps_the_totals(self):
        self.attack()
        self.hit()
        self.finish()
        loads = self.info(0)
        self.step(RESULT_FRAMES - 1)
        self.assertEqual((self.phase(), self.info(0)), (RESULT, loads))
        self.step()
        self.assertEqual((self.phase(), self.info(0), self.info(15), self.info(16)), (SETUP, loads + 1, 0, 0))
        self.assertEqual(self.shown()[1], '1/1 (100.0%)')
        self.assertEqual(self.big(), 'SDI 0')

    def test_a_stuck_attempt_gives_up(self):
        self.step(SETUP_FRAMES)
        self.m.call('TestEscState', 1, FALL)  # She never gets her up-B out.
        self.step(GIVE_UP_FRAMES)
        self.assertEqual((self.phase(), self.info(20)), (RESULT, MISSED))

    def test_a_ko_ends_the_attempt(self):
        self.attack()
        self.hit(1, launcher=True)
        self.m.call('TestEscKill', 0)
        self.step()
        self.assertEqual((self.phase(), self.info(20)), (RESULT, LAUNCHED))

    # The D-pad.
    def test_dpad_left_and_right_pick_samus_s_side_save_and_restart_uncounted(self):
        self.attack()
        self.hit()
        loads = self.info(0)
        self.m.call('TestEscDPad', DPAD_RIGHT)
        self.step()
        self.m.call('TestEscDPad', 0)
        self.assertEqual((self.info(28), self.info(0), self.info(18), self.phase()), (RIGHT, loads + 1, 0, SETUP))
        self.assertEqual(self.samus(), (6, 0))
        self.assertEqual(self.info(33) & DPAD_RIGHT, DPAD_RIGHT)  # Held through the rewind.
        self.m.call('TestEscDPad', DPAD_LEFT)
        self.step()
        self.m.call('TestEscDPad', 0)
        self.assertEqual((self.info(28), self.samus()), (LEFT, (-6, 0)))

    def test_dpad_down_switches_ground_and_air(self):
        for expected, height in ((AIR, 6), (GROUND, 0), (AIR, 6)):
            self.m.call('TestEscDPad', DPAD_DOWN)
            self.step()
            self.m.call('TestEscDPad', 0)
            self.assertEqual((self.info(26), self.you()[1]), (expected, height))
        self.choose(POSITION_ROW, RANDOM_POSITION)
        self.m.call('TestEscDPad', DPAD_DOWN)
        self.step()
        self.assertEqual(self.info(26), GROUND)

    # The settings.
    def test_every_row_saves_as_it_changes(self):
        for row, value in ((POSITION_ROW, AIR), (DISTANCE_ROW, FAR), (SIDE_ROW, RIGHT), (PERCENT_ROW, 20)):
            self.choose(row, value)
            self.assertEqual(self.info(26 + row), value)
        self.assertEqual([self.info(26 + row) for row in range(4)], [AIR, FAR, RIGHT, 20])


if __name__ == '__main__':
    unittest.main(verbosity=2)
