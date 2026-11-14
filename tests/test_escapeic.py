"""Execute the production Escape IC Grab event, frame by frame."""
from pathlib import Path
import subprocess
import unittest
import test_settings as fixtures
from test_lowlaser import load_probe

ROOT = Path(__file__).resolve().parents[1]
WAIT, CATCH, CAPTURE_PULLED, CAPTURE_CUT = 14, 0xD4, 0xDF, 0xE5
Z = 0x10
SETUP, GRAB, RESULT = range(3)
PERCENT_ROW = 0
SETUP_FRAMES, RESULT_FRAMES = 40, 60
GRAB_FRAME = 7  # The probe's grab connects on Popo's seventh frame.
DPAD_LEFT, DPAD_RIGHT, DPAD_DOWN, DPAD_UP = 1, 2, 4, 8
NEW_BEST_SFX, ESCAPE_SFX = 303, 0xAD
CYAN, WHITE = 0x00FFFF, 0xFFFFFF


def build():
    subprocess.run([
        'C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe', '-O2', '-w',
        '-ffreestanding', '-fno-builtin', '-msdata=none', '-mcpu=750', '-nostdlib',
        '-Wl,-Ttext=0x101000,-e,TestICInit,--unresolved-symbols=ignore-all',
        'tests/escapeic_probe.c', '-o', 'build/escapeic-probe.elf'
    ], cwd=ROOT, check=True)


class EscapeICTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def setUp(self):
        self.m = fixtures.Machine()
        load_probe(self.m, 'escapeic-probe.elf', 'TestICInit')
        self.step()  # The first frame saves the state and sets up the first grab.

    def info(self, key):
        value = self.m.call('TestICInfo', key)
        return value if value < 0x80000000 else value - 0x100000000
    def step(self, frames=1):
        for done in range(0, frames, 40):  # In chunks: one emulator call has an instruction budget.
            self.m.call('TestICThink', min(frames - done, 40))
    def string(self, address):
        return bytes(self.m.cpu.mem_read(address, 32)).split(b'\0', 1)[0].decode()
    def text(self, key): return self.string(self.m.call('TestICText', key))
    def labels(self): return [self.text(i) for i in range(4)]
    def shown(self): return [self.text(i) for i in range(4, 8)]
    def big(self): return self.text(8)
    def phase(self): return self.info(1)
    def choose(self, row, value): self.m.call('TestICChoose', row, value)
    def dpad(self, buttons):
        self.m.call('TestICDPad', buttons)
        self.step()
        self.m.call('TestICDPad', 0)
    def grab(self):
        """Popo presses Z on the second frame; his grab connects on its seventh."""
        self.step(1 + GRAB_FRAME)
        self.assertEqual((self.phase(), self.info(8)), (GRAB, CAPTURE_PULLED))
    def escape(self, inputs=0):
        """Mash `inputs` a frame until you're out."""
        self.m.call('TestICMash', inputs, 10000)
        for _ in range(400):
            if self.phase() == RESULT: return
            self.step()
        self.fail('never got out')

    # The layout.
    def test_you_in_the_middle_with_popo_facing_you_and_nana_behind_him(self):
        self.assertEqual((self.info(2), self.info(3), self.info(4)), (0, -80, -140))  # HSD_Randi(2) gave 0: the left.
        self.assertEqual((self.info(5), self.info(6), self.info(7)), (-1, 1, 1))
        self.assertEqual(self.info(8), WAIT)

    def test_you_cant_act_until_the_grab_connects(self):
        self.assertEqual(self.info(10), 0)
        self.m.call('TestICReWait')
        self.step()
        self.assertEqual(self.info(10), 0)

    def test_popo_presses_z_on_the_second_frame_and_both_climbers_have_no_ai(self):
        self.step()
        self.assertEqual(self.info(11), 0)
        self.step()
        self.assertEqual((self.info(11) & Z, self.info(12)), (Z, 1515))
        self.step()
        self.assertEqual((self.info(11), self.info(9)), (0, CATCH))

    # The grab.
    def test_the_grab_holds_as_if_tied_with_handicap_off(self):
        self.m.call('TestICPlacing', 1, 7)  # Second place, handicap 7: the game holds 30 + 16 + 30.
        self.dpad(DPAD_LEFT)
        self.grab()
        self.assertEqual(self.info(16), 750)  # 75, as when tied with handicap off.

    def test_without_mashing_you_get_out_when_the_hold_runs_out(self):
        for percent, hold in ((0, 75), (5, 155), (20, 395)):
            with self.subTest(percent=percent):
                self.choose(PERCENT_ROW, percent)
                self.dpad(DPAD_LEFT)
                self.assertEqual((self.info(22), self.info(23)), (percent * 10, percent * 10))
                self.grab()
                self.escape(0)
                self.assertEqual((self.info(13), self.info(14)), (hold, 0))
                self.assertEqual(self.shown()[:2], [f'{hold}f', f'{hold}f'])

    def test_the_no_mash_hold_is_75_and_16_a_step(self):
        self.assertEqual([self.m.call('TestICHold', p) for p in range(21)], [75 + 16 * p for p in range(21)])

    def test_each_frame_s_mash_inputs_count_from_the_timer_s_drop(self):
        self.grab()
        self.m.call('TestICMash', 1, 3)  # 7 a frame for three frames: 21.
        self.step(3)
        self.assertEqual((self.info(13), self.info(14), self.big()), (3, 3, 'Mash 3'))
        self.m.call('TestICMash', 2, 2)  # 13 a frame for two: 26.
        self.step(2)
        self.assertEqual((self.info(13), self.info(14)), (5, 7))
        self.m.call('TestICMash', 0, 1)
        self.step()
        self.assertEqual((self.info(13), self.info(14)), (6, 7))
        self.assertEqual(self.phase(), GRAB)

    def test_one_input_a_frame_gets_out_of_75_in_11(self):
        self.grab()
        self.escape(1)  # 75 / 7: the eleventh frame's input counts too.
        self.assertEqual((self.info(13), self.info(14), self.info(15)), (11, 11, 11))
        self.assertEqual(self.labels(), ['Held For', 'No-Mash Hold', 'Best at 0%', 'Mash Speed'])
        self.assertEqual(self.shown(), ['11f', '75f', '11f', '60.0/s'])
        self.assertEqual((self.big(), self.info(24), self.info(19)), ('Mash 11', CYAN, NEW_BEST_SFX))

    def test_the_fastest_mash_gets_out_of_75_in_6(self):
        self.grab()
        self.escape(2)
        self.assertEqual((self.info(13), self.info(14)), (6, 12))

    # Session bests.
    def test_each_percent_keeps_its_own_fastest_escape(self):
        self.grab()
        self.escape(1)  # 11 frames: the first is a best.
        self.step(RESULT_FRAMES)
        self.grab()
        self.escape(0)  # 75: slower, not a best.
        self.assertEqual((self.info(15), self.info(17), self.info(18), self.info(19)), (10, 11, 11, ESCAPE_SFX))
        self.assertEqual((self.big(), self.info(24)), ('Mash 0', WHITE))
        self.assertEqual(self.text(11), '11f (11)')
        self.step(RESULT_FRAMES)
        self.grab()
        self.escape(2)  # 6: faster, a new best.
        self.assertEqual((self.info(15), self.info(17), self.info(18)), (11, 6, 12))
        self.assertEqual(self.text(11), '6f (12)')
        self.dpad(DPAD_UP)  # 10% has none yet.
        self.assertEqual((self.info(17), self.shown()[2], self.text(12)), (0, '-', '-'))
        self.assertEqual(self.labels()[2], 'Best at 10%')

    def test_session_bests_has_a_row_for_every_percent(self):
        self.assertEqual(self.info(30), 21)
        names = [self.string(self.m.call('TestICBestName', i)) for i in range(21)]
        self.assertEqual(names, [f'{p * 10}%' for p in range(21)])
        self.assertEqual({self.text(11 + i) for i in range(21)}, {'-'})

    def test_the_result_stays_up_then_the_next_grab_starts(self):
        self.grab()
        self.escape(2)
        loads = self.info(0)
        self.step(RESULT_FRAMES - 1)
        self.assertEqual((self.phase(), self.info(0)), (RESULT, loads))
        self.step()
        self.assertEqual((self.phase(), self.info(0)), (SETUP, loads + 1))
        self.assertEqual(self.big(), 'Mash 12')  # Up until the next grab,
        self.grab()
        self.assertEqual(self.big(), 'Mash 0')  # which starts over.

    def test_a_missed_grab_is_tried_again_from_another_gap(self):
        self.m.call('TestICMissGrabs', 1)
        for frames in range(1, SETUP_FRAMES + 2):  # Once his whiffed grab ends.
            self.step()
            if self.info(3) != -80: break
        self.assertEqual((self.info(3), self.phase(), self.info(9)), (-50, SETUP, WAIT))  # The next gap, 5.
        self.assertLess(frames, SETUP_FRAMES)
        self.m.call('TestICMissGrabs', 0)
        self.step(2 + GRAB_FRAME)
        self.assertEqual(self.phase(), GRAB)

    def test_a_ko_restarts(self):
        loads = self.info(0)
        self.m.call('TestICKill', 0)
        self.step()
        self.assertEqual((self.info(0), self.phase()), (loads + 1, SETUP))

    # The menu and D-pad.
    def test_the_handoff_row_is_there_but_disabled(self):
        self.assertEqual((self.info(26), self.info(27), self.text(9), self.text(10)), (1, 1, 'None', 'Handoff (Coming Soon)'))

    def test_dpad_up_and_down_change_your_percent_save_and_restart(self):
        loads = self.info(0)
        self.dpad(DPAD_UP)
        self.assertEqual((self.info(28), self.info(20), self.info(0), self.info(23)), (1, 1, loads + 1, 10))
        self.assertEqual(self.info(29) & DPAD_UP, DPAD_UP)  # Held through the rewind.
        self.dpad(DPAD_DOWN)
        self.dpad(DPAD_DOWN)  # Not under 0%, but it still restarts.
        self.assertEqual((self.info(28), self.info(20), self.info(0)), (0, 0, loads + 3))
        self.choose(PERCENT_ROW, 20)
        self.dpad(DPAD_UP)
        self.assertEqual(self.info(28), 20)

    def test_dpad_left_and_right_restart_a_grab_uncounted(self):
        self.grab()
        self.m.call('TestICMash', 1, 3)
        self.step(3)
        loads = self.info(0)
        self.dpad(DPAD_RIGHT)
        self.assertEqual((self.info(0), self.phase(), self.info(17)), (loads + 1, SETUP, 0))

    def test_your_percent_row_saves(self):
        self.choose(PERCENT_ROW, 13)
        self.assertEqual((self.info(20), self.info(21)), (13, 1))


if __name__ == '__main__':
    unittest.main(verbosity=2)
