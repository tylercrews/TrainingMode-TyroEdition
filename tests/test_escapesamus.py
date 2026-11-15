"""Execute the production Escape Samus Up-B event, frame by frame."""
from pathlib import Path
import subprocess
import unittest
import test_settings as fixtures
from test_lowlaser import load_probe

ROOT = Path(__file__).resolve().parents[1]
WAIT, FALL, UPB_GROUND = 14, 29, 353
FALL_SPECIAL, LANDING_FALL_SPECIAL, DAMAGE, CAPTURE_PULLED = 35, 43, 79, 223
POSITION_ROW, DISTANCE_ROW, SIDE_ROW, PERCENT_ROW, GOAL_ROW = range(5)
ESCAPE_GOAL, PUNISH_GOAL = range(2)
GROUND, AIR, RANDOM_POSITION = range(3)
ON_TOP, SIDE, FAR, RANDOM_DISTANCE = range(4)
RANDOM_SIDE, LEFT, RIGHT = range(3)
SETUP, ATTACK, PUNISH, RESULT = range(4)
NONE, ESCAPED, LAUNCHED, MISSED, PUNISHED, TOO_SLOW = range(6)
LANDING_LAG = 24  # PlSs special attribute 0x50, as the probe's Samus has it.
INVINCIBLE = 111  # Hurt kind 1, every hurtbox invincible, and the flash on.
SETUP_FRAMES, RESULT_FRAMES, UPB_DONE_FRAME, GIVE_UP_FRAMES = 20, 60, 33, 240
DPAD_LEFT, DPAD_RIGHT, DPAD_DOWN = 1, 2, 4
SUCCESS_SFX, FAILURE_SFX = 303, 0xAF
GREEN, RED, WHITE, YELLOW = 0x28FF3C, 0xFF2828, 0xFFFFFF, 0xFFDC28


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
    def finish(self, phase=RESULT):
        """Run Samus's up-B out to its last hitbox, so the attempt resolves."""
        for _ in range(UPB_DONE_FRAME + 5):
            if self.phase() == phase: return
            self.step()
        self.assertEqual(self.phase(), phase)
    def string(self, address):
        return bytes(self.m.cpu.mem_read(address, 48)).split(b'\0', 1)[0].decode()
    def punish_goal(self):
        self.choose(GOAL_ROW, PUNISH_GOAL)
        self.restart()
    def escape_into_the_window(self):
        """Punish: take a hit, fall out, and her up-B's hitboxes end."""
        self.attack()
        self.hit()
        self.finish(PUNISH)
    def until_samus(self, state, limit=200):
        for _ in range(limit):
            if self.info(9) == state: return
            self.step()
        self.fail(f'Samus never reached state {state}')

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

    # Goal: Punish.
    def test_punish_opens_a_window_once_you_re_out(self):
        self.punish_goal()
        self.escape_into_the_window()
        self.assertEqual((self.info(20), self.info(18), self.info(19), self.info(40)), (ESCAPED, 1, 1, 0))
        self.assertEqual(self.labels(), ['Last Attempt', 'Punishes', 'Escapes', 'SDI Inputs'])
        self.assertEqual(self.shown()[:3], ['Escaped', '-', '1/1 (100.0%)'])
        self.assertEqual(self.info(25), YELLOW)  # Still to punish.
        self.assertEqual(self.info(38), 0)  # Hittable.

    def test_samus_fast_falls_on_her_first_falling_frame(self):
        self.punish_goal()
        self.escape_into_the_window()
        self.until_samus(FALL_SPECIAL)
        entered = self.info(43)
        self.assertEqual(self.info(35), 0)  # Not yet falling: her up-B leaves her still.
        self.step()
        self.assertEqual((self.info(35), self.info(36)), (-127, entered + 1))
        self.step()
        self.assertEqual(self.info(35), 0)  # Already fast falling.
        self.until_samus(LANDING_FALL_SPECIAL, 30)
        self.assertEqual(self.info(37), 0)

    def test_too_slow_once_she_can_act_and_then_she_s_invincible(self):
        self.punish_goal()
        self.escape_into_the_window()
        self.until_samus(LANDING_FALL_SPECIAL)
        self.step(LANDING_LAG - 1)
        self.assertEqual((self.info(9), self.phase(), self.info(38)), (LANDING_FALL_SPECIAL, PUNISH, 0))
        self.step()  # Her first free frame.
        self.assertEqual((self.info(9), self.phase(), self.info(42), self.info(38)), (WAIT, RESULT, 1, INVINCIBLE))
        self.assertEqual((self.info(20), self.info(21), self.info(40), self.info(41)), (TOO_SLOW, FAILURE_SFX, 1, 0))
        self.assertEqual(self.shown()[:3], ['Too Slow', '0/1 (0.0%)', '1/1 (100.0%)'])
        self.assertEqual(self.info(25), RED)

    def test_a_hit_in_her_landing_lag_is_a_punish_and_the_scene_waits_for_her(self):
        self.punish_goal()
        self.escape_into_the_window()
        self.until_samus(LANDING_FALL_SPECIAL)
        self.step(LANDING_LAG - 1)
        self.m.call('TestEscHitSamus', 9, 20)
        self.step()
        self.assertEqual((self.info(39), self.info(20), self.info(21), self.info(40), self.info(41)),
                         (1, PUNISHED, SUCCESS_SFX, 1, 1))
        self.assertEqual((self.phase(), self.info(25)), (PUNISH, GREEN))  # Your combo goes on,
        self.assertEqual(self.shown()[:2], ['Punished', '1/1 (100.0%)'])
        self.step(18)
        self.assertEqual((self.info(9), self.phase(), self.info(38)), (DAMAGE, PUNISH, 0))  # through her hitstun,
        self.step(2)
        self.assertEqual((self.info(9), self.phase(), self.info(38)), (WAIT, RESULT, INVINCIBLE))  # until she can act.
        self.assertEqual((self.info(20), self.info(40), self.info(41)), (PUNISHED, 1, 1))

    def test_a_grab_is_a_punish(self):
        self.punish_goal()
        self.escape_into_the_window()
        self.until_samus(LANDING_FALL_SPECIAL)
        self.m.call('TestEscState', 1, CAPTURE_PULLED)
        self.step()
        self.assertEqual((self.info(20), self.info(41), self.phase()), (PUNISHED, 1, PUNISH))

    def test_hitting_her_out_of_her_up_b_is_a_punish_at_once(self):
        self.punish_goal()
        self.attack()
        self.hit()
        self.m.call('TestEscHitSamus', 3, 10)
        self.step()  # Out of her up-B: it resolves, and you weren't launched.
        self.assertEqual((self.phase(), self.info(20), self.info(40), self.info(41)), (PUNISH, PUNISHED, 1, 1))
        self.step(10)
        self.assertEqual((self.phase(), self.info(38)), (RESULT, INVINCIBLE))

    def test_hits_after_the_window_don_t_count(self):
        self.punish_goal()
        self.escape_into_the_window()
        self.until_samus(WAIT)
        self.m.call('TestEscHitSamus', 9, 5)
        self.step()
        self.assertEqual((self.info(20), self.info(41), self.info(40)), (TOO_SLOW, 0, 1))

    def test_launched_is_a_failed_punish_with_no_window(self):
        self.punish_goal()
        self.attack()
        self.hit()
        self.step(3)
        self.hit(1, launcher=True)
        self.finish()
        self.assertEqual((self.info(20), self.info(40), self.info(41), self.info(42)), (LAUNCHED, 1, 0, 0))
        self.assertEqual(self.info(25), RED)

    def test_a_miss_counts_no_punish_attempt(self):
        self.punish_goal()
        self.attack()
        self.finish()
        self.assertEqual((self.info(20), self.info(18), self.info(40)), (MISSED, 0, 0))

    def test_a_restart_in_the_window_leaves_the_punish_uncounted(self):
        self.punish_goal()
        self.escape_into_the_window()
        self.m.call('TestEscDPad', DPAD_LEFT)
        self.step()
        self.m.call('TestEscDPad', 0)
        self.assertEqual((self.phase(), self.info(18), self.info(19), self.info(40)), (SETUP, 1, 1, 0))
        self.assertEqual(self.shown()[1], '-')

    def test_she_stays_invincible_until_the_reset_clears_it(self):
        self.punish_goal()
        self.escape_into_the_window()
        self.until_samus(WAIT)
        loads = self.info(0)
        self.step(RESULT_FRAMES - 1)
        self.assertEqual((self.phase(), self.info(38), self.info(0)), (RESULT, INVINCIBLE, loads))
        self.step()
        self.assertEqual((self.phase(), self.info(38), self.info(0), self.info(42)), (SETUP, 0, loads + 1, 0))

    def test_the_escape_goal_never_fast_falls_or_goes_invincible(self):
        self.attack()
        self.hit()
        self.finish()
        for _ in range(RESULT_FRAMES - 1):
            self.step()
            self.assertNotEqual(self.info(35), -127)
            self.assertEqual(self.info(38), 0)

    def test_a_new_goal_takes_effect_on_the_next_attempt(self):
        self.attack()
        self.hit()
        self.choose(GOAL_ROW, PUNISH_GOAL)
        self.finish()  # This one's still Escape.
        self.assertEqual((self.info(20), self.labels()[1]), (ESCAPED, 'Escapes'))
        self.step(RESULT_FRAMES)
        self.assertEqual((self.phase(), self.labels()[1]), (SETUP, 'Punishes'))
        self.attack()
        self.hit()
        self.finish(PUNISH)

    # The menu.
    def test_help_is_its_own_menu(self):
        names = [self.string(self.m.call('TestEscMainName', i)) for i in range(self.info(46))]
        self.assertEqual(names, ['Your Position', 'Distance', 'Samus Side', 'Your Percent', 'Goal', 'Help', 'Exit'])
        self.assertEqual(self.info(45), 1)

    def test_help_covers_sdi_asdi_and_both_goals(self):
        rows = {}
        for row in range(self.info(44)):
            lines = [self.m.call('TestEscHelpLine', row, line) for line in range(4)]
            rows[self.string(self.m.call('TestEscHelpName', row))] = [self.string(at) for at in lines if at]
        self.assertEqual(list(rows), [
            'About This Event', 'Goal: Escape', 'Goal: Punish', 'What SDI Is', 'Fresh Flicks Only', 'Quarter Circles',
            "Don't Go Back and Forth", 'Which Way to SDI', 'ASDI', 'Hold the C-Stick', 'Her Last Hit',
            'Percent and Position', 'Reading the HUD'])
        for name, lines in rows.items():
            self.assertEqual(len(lines), 4, name)
            for line in lines:
                self.assertLessEqual(len(line), 47, (name, line))  # Within the widest event menu text.
        self.assertIn("can't SDI", ' '.join(rows['Hold the C-Stick']))
        self.assertIn('24 frames', ' '.join(rows['Goal: Punish']))

    # The settings.
    def test_every_row_saves_as_it_changes(self):
        for row, value in ((POSITION_ROW, AIR), (DISTANCE_ROW, FAR), (SIDE_ROW, RIGHT), (PERCENT_ROW, 20),
                           (GOAL_ROW, PUNISH_GOAL)):
            self.choose(row, value)
            self.assertEqual(self.info(34 if row == GOAL_ROW else 26 + row), value)
        self.assertEqual([self.info(26 + row) for row in range(4)] + [self.info(34)], [AIR, FAR, RIGHT, 20, PUNISH_GOAL])


if __name__ == '__main__':
    unittest.main(verbosity=2)
