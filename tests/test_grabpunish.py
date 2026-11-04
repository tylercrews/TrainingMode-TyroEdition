"""Execute the production 2v1 Grab Punish event, frame by frame."""
from pathlib import Path
import re
import subprocess
import unittest
import test_settings as fixtures
from test_lowlaser import load_probe

ROOT = Path(__file__).resolve().parents[1]
HMN, MATE, ENEMY = 0, 1, 2
WAIT, CATCH = 14, 0xD4
DPAD_LEFT = 1
PERCENT_ROW, MASH_ROW, RELEASE_ROW = 2, 3, 5
NO_MASH = 0
RED, WHITE, YELLOW = 0xFF2828, 0xFFFFFF, 0xFFF000
RED_PULSE = (180, 140, 100, 60, 20, 60, 100, 140)
SETUP, GRAB, RELEASE, RESOLVE, FOLLOWUP = range(5)
GRACE, FOLLOWUP_MIN = 12, 60
SUCCESS_SFX, PARTIAL_SFX, FAILURE_SFX = 303, 0xAD, 0xAF


def build():
    subprocess.run([
        'C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe', '-O2', '-w',
        '-ffreestanding', '-fno-builtin', '-msdata=none', '-mcpu=750', '-nostdlib',
        '-Wl,-Ttext=0x101000,-e,TestGPInit,--unresolved-symbols=ignore-all',
        'tests/grabpunish_probe.c', '-o', 'build/grabpunish-probe.elf'
    ], cwd=ROOT, check=True)


class GrabPunishTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def setUp(self):
        self.m = fixtures.Machine()
        load_probe(self.m, 'grabpunish-probe.elf', 'TestGPInit')
        self.m.call('TestGPChoose', MASH_ROW, NO_MASH)  # Frames left then equal the timer.

    def info(self, key): return self.m.call('TestGPInfo', key)
    def step(self, frames=1):
        # In chunks: one emulator call has an instruction budget.
        for done in range(0, frames, 50):
            self.m.call('TestGPThink', min(frames - done, 50))
    def text(self, key):
        return bytes(self.m.cpu.mem_read(self.m.call('TestGPText', key), 24)).split(b'\0', 1)[0].decode()
    def labels(self): return [self.text(i) for i in range(3)]
    def shown(self): return [self.text(i) for i in (3, 4, 5)]
    def time(self): return self.text(3)
    def big(self): return self.text(6)
    def red(self): return bool(self.info(10))
    def phase(self): return self.info(18)
    def grab(self, timer=20):
        """Frame 1 sets up, frame 2 presses Z, then the grab connects."""
        self.step(2)
        self.step()
        self.m.call('TestGPGrabbed', timer)
        self.step()
        self.assertEqual(self.phase(), GRAB)
    def let_go(self, length=30):
        self.m.call('TestGPLetGo', length)
        self.step()
    def hit(self, who, damage=10):
        self.m.call('TestGPHit', who, damage)
        self.step()
    def finish(self):
        """Through the same-attack window, so the attempt is counted."""
        self.step(GRACE)
        self.assertEqual(self.phase(), FOLLOWUP)

    # Regressions from the first Dolphin test.
    def test_enemy_percent_is_not_queued_as_damage_too(self):
        self.step(30)
        self.assertEqual((self.info(3), self.info(4), self.info(5)), (60, 0, 60))
        self.assertEqual((self.info(0), self.info(1)), (1, 1))  # One save, one load, no loop.

    def test_a_missed_grab_moves_only_the_cpus(self):
        self.m.call('TestGPMissGrabs', 1)
        self.step(2)
        loads, mate, enemy = self.info(0), self.info(7), self.info(8)
        self.m.call('TestGPMove', HMN, 40)
        self.step(30)
        self.assertEqual(self.info(0), loads)     # No savestate, so game_timer
        self.assertGreater(self.info(2), 3)       # passes the pause menu's 3.
        self.assertEqual(self.info(6), 40)        # You stay where you went.
        self.assertNotEqual((self.info(7), self.info(8)), (mate, enemy))  # Next gap.

    def test_the_teammate_presses_z_on_the_second_frame(self):
        self.step(1)
        self.assertEqual(self.info(19), 0)
        self.step(1)
        self.assertNotEqual(self.info(19), 0)

    # The panel.
    def test_panel_is_three_rows_with_no_attempts_yet(self):
        self.step(3)
        self.assertEqual(self.info(9), 3)
        self.assertEqual(self.labels(), ['Grab Time Left', 'Enemy Hits', 'Ally Hits'])
        self.assertEqual(self.shown(), ['-', '-', '-'])
        self.assertEqual(self.big(), '')

    def attempt(self, enemy, ally):
        self.grab()
        if ally:
            self.m.call('TestGPHit', MATE, 10)
        if enemy:
            self.m.call('TestGPHit', ENEMY, 10)
        self.step()
        self.let_go()
        self.step(GRACE)
        if not enemy:
            self.step(30)
        self.assertEqual(self.phase(), FOLLOWUP)
        self.m.call('TestGPDown', DPAD_LEFT)
        self.step()
        self.m.call('TestGPDown', 0)

    def test_an_attack_on_both_counts_for_both(self):
        self.attempt(True, False)
        self.attempt(True, True)
        self.attempt(False, False)
        self.attempt(False, True)
        self.assertEqual(self.shown()[1:], ['2/4 (50.0%)', '2/4 (50.0%)'])

    def test_a_third_shows_one_decimal(self):
        self.attempt(True, False)
        self.attempt(False, False)
        self.attempt(False, False)
        self.assertEqual(self.shown()[1:], ['1/3 (33.3%)', '0/3 (0.0%)'])

    # The countdown.
    def test_grab_time_left_counts_down_then_on_through_the_release(self):
        self.grab(20)
        self.assertEqual((self.time(), self.big()), ('20f', '20'))
        self.assertEqual(self.info(14), WHITE)
        self.m.call('TestGPTimer', 7)
        self.step()
        self.assertEqual((self.time(), self.big()), ('7f', '7'))
        self.let_go(30)
        self.assertEqual((self.phase(), self.time(), self.big()), (RELEASE, 'Release 30f', '30'))
        self.assertEqual(self.info(14), YELLOW)
        self.step(1)
        self.assertEqual((self.time(), self.big()), ('Release 29f', '29'))
        self.step(28)
        self.assertEqual((self.time(), self.big()), ('Release 1f', '1'))
        self.assertFalse(self.red())
        self.step(1)
        self.assertEqual((self.phase(), self.time(), self.big()), (FOLLOWUP, 'Escaped', ''))
        self.assertGreater(self.info(15), 50)  # Bigger than the panel's 0.5.

    def test_donkey_kongs_shorter_release_comes_from_the_animation(self):
        self.grab()
        self.let_go(20)
        self.assertEqual(self.time(), 'Release 20f')
        self.step(19)
        self.assertEqual(self.time(), 'Release 1f')
        self.step()
        self.assertEqual(self.time(), 'Escaped')

    def test_an_escape_is_counted_when_they_can_act(self):
        self.grab()
        self.let_go()
        self.step(29)
        self.assertEqual(self.shown()[1:], ['-', '-'])
        self.step()
        self.assertEqual(self.shown()[1:], ['0/1 (0.0%)', '0/1 (0.0%)'])
        self.assertEqual(self.info(16), FAILURE_SFX)

    # The red overlay.
    def test_enemy_pulses_red_from_the_frame_they_can_act(self):
        self.grab()
        self.let_go()
        self.step(29)
        self.assertFalse(self.red())
        self.assertEqual(self.info(13), 0)  # Never earlier, even for a frame.
        seen = []
        for _ in range(9):
            self.step()
            self.assertTrue(self.red())
            seen.append(self.info(12))
        self.assertEqual(self.info(11), RED)
        self.assertEqual(seen, list(RED_PULSE) + [RED_PULSE[0]])

    def test_red_ends_with_the_attempt(self):
        self.grab()
        self.let_go()
        self.step(30)
        self.assertTrue(self.red())
        self.step(FOLLOWUP_MIN)
        self.assertEqual(self.phase(), SETUP)
        self.assertFalse(self.red())

    def test_a_hit_enemy_never_turns_red(self):
        self.grab()
        self.m.call('TestGPTimer', 12)
        self.step()
        self.hit(ENEMY)
        self.m.call('TestGPState', ENEMY, WAIT)
        self.step(FOLLOWUP_MIN)
        self.assertEqual(self.info(13), 0)

    # Hits.
    def test_a_hit_in_the_grab_shows_the_frames_that_were_left(self):
        self.grab()
        self.m.call('TestGPTimer', 12)
        self.step()
        self.m.call('TestGPTimer', 11)
        self.m.call('TestGPHit', ENEMY, 10)
        self.m.call('TestGPState', ENEMY, WAIT)
        self.step()
        self.assertEqual((self.phase(), self.time(), self.big()), (RESOLVE, 'Grab Hit, 12f', '12'))
        self.assertEqual(self.info(14), WHITE)
        self.finish()
        self.assertEqual(self.shown()[1:], ['1/1 (100.0%)', '0/1 (0.0%)'])
        self.assertEqual(self.info(16), SUCCESS_SFX)

    def test_a_release_hit_shows_as_one_but_only_counts_if_the_setting_is_on(self):
        for setting, shown, sound in ((0, '0/1 (0.0%)', FAILURE_SFX), (1, '1/1 (100.0%)', SUCCESS_SFX)):
            self.setUp()
            self.m.call('TestGPChoose', RELEASE_ROW, setting)
            self.grab()
            self.let_go()
            self.step(9)
            self.hit(ENEMY)
            self.assertEqual((self.phase(), self.time(), self.big()), (RESOLVE, 'Release Hit, 21f', '21'))
            self.assertEqual(self.info(14), YELLOW)
            self.finish()
            self.assertEqual(self.time(), 'Release Hit, 21f')
            self.assertEqual(self.shown()[1], shown)
            self.assertEqual(self.info(16), sound)
            self.assertFalse(self.red())

    def test_a_hit_after_they_can_act_is_too_late(self):
        self.m.call('TestGPChoose', RELEASE_ROW, 1)
        self.grab()
        self.let_go()
        self.step(30)
        self.hit(ENEMY)
        self.assertEqual((self.time(), self.shown()[1]), ('Escaped', '0/1 (0.0%)'))

    def test_hitting_only_your_teammate_lets_go(self):
        self.grab()
        self.hit(MATE)
        self.let_go()
        self.assertEqual(self.time(), 'Release 30f')
        self.step(30)
        self.assertEqual((self.time(), self.shown()[1:]), ('Let Go', ['0/1 (0.0%)', '1/1 (100.0%)']))
        self.assertTrue(self.red())

    def test_the_attack_that_hit_your_teammate_can_still_land_as_a_grab_hit(self):
        self.grab()
        self.m.call('TestGPTimer', 15)
        self.step()
        self.hit(MATE)
        self.let_go()
        self.step(GRACE - 2)
        self.hit(ENEMY)
        self.assertEqual(self.time(), 'Grab Hit, 15f')
        self.finish()
        self.assertEqual(self.shown()[1:], ['1/1 (100.0%)', '1/1 (100.0%)'])
        self.assertEqual(self.info(16), PARTIAL_SFX)

    def test_a_later_hit_after_your_teammate_is_a_release_hit(self):
        self.grab()
        self.hit(MATE)
        self.let_go()
        self.step(GRACE + 2)
        self.hit(ENEMY)
        self.assertTrue(self.time().startswith('Release Hit'))
        self.finish()
        self.assertEqual(self.shown()[1:], ['0/1 (0.0%)', '1/1 (100.0%)'])

    def test_hitting_your_teammate_after_they_mash_out_isnt_a_let_go(self):
        self.grab()
        self.let_go()
        self.step(5)
        self.hit(MATE)
        self.step(30)
        self.assertEqual((self.time(), self.shown()[2]), ('Escaped', '1/1 (100.0%)'))

    def test_a_weak_hit_keeps_the_attempt_until_they_are_let_go(self):
        self.grab()
        self.hit(ENEMY, 1)  # A weak hit that doesn't break the grab.
        self.assertEqual(self.phase(), GRAB)
        self.m.call('TestGPState', ENEMY, WAIT)
        self.step()
        self.assertEqual(self.phase(), RESOLVE)

    # Calling a callback EventVars never binds jumps to null on the console.
    def test_every_event_var_callback_called_is_bound(self):
        table = (ROOT / 'src/events.c').read_text()
        for source in (ROOT / 'src').glob('*.c'):
            for name in set(re.findall(r'event_vars->(\w+)\(', source.read_text(errors='ignore'))):
                self.assertRegex(table, rf'\.{name} = ', f'{source.name} calls an unbound {name}')

    # The setting.
    def test_changing_count_release_hits_saves_and_restarts_the_totals(self):
        self.attempt(True, False)
        self.assertEqual(self.shown()[1], '1/1 (100.0%)')
        self.m.call('TestGPChoose', RELEASE_ROW, 1)
        self.step()
        self.assertEqual(self.info(17), 1)
        self.assertEqual(self.shown()[1:], ['-', '-'])


if __name__ == '__main__':
    unittest.main(verbosity=2)
