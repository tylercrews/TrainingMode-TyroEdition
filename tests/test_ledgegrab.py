"""Execute the production Ledgegrab Speedrun event and its relocated DAT."""
from pathlib import Path
import struct
import subprocess
import unittest
import test_settings as fixtures
from test_lowlaser import LEGAL, load_probe
from unicorn import UC_HOOK_MEM_WRITE

ROOT = Path(__file__).resolve().parents[1]
WAIT, DASH, CLIFFCATCH, CLIFFWAIT, APPEAL = 14, 20, 252, 253, 264
WHITE, CYAN, GREEN, YELLOW, RED = 0xFFFFFFFF, 0x00FFFFFF, 0x00FF00FF, 0xFFE000FF, 0xFF0000FF
DPAD_LEFT, DPAD_RIGHT, DPAD_DOWN, DPAD_UP = 1, 2, 4, 8
BUTTON_A = 0x100
FIFO = 0xCC008000
GOAL, START, FACING, CAMERA, RESET = 0, 1, 2, 3, 4
AT_LEDGE, AT_FAR, UNDER_CLOSE, UNDER_FAR, CENTER, TOP, CLOSE, FAR = range(8)
LEFT, RIGHT = 0, 1
TOWARD, AWAY = 0, 1
CAMERA_GOAL, CAMERA_BOTH, CAMERA_FIGHTER = 0, 1, 2
FRAMED, NOT_FRAMED = 0, 1
RESET_INSTANT, RESET_SHORT, RESET_LINGER = 0, 1, 2
LINGER, DNF = 30, 1800  # The linger of Short, the default Reset Speed.
NONE, SHOWN_DNF = -1, -2
FD, BATTLEFIELD, FOUNTAIN, BIG_BLUE = 32, 31, 2, 4


def build():
    subprocess.run([
        'C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe', '-O2', '-w',
        '-ffreestanding', '-fno-builtin', '-msdata=none', '-mcpu=750', '-nostdlib',
        '-Wl,-Ttext=0x101000,-e,TestLedgeInit,--unresolved-symbols=ignore-all',
        'tests/ledgegrab_probe.c', '-o', 'build/ledgegrab-probe.elf'
    ], cwd=ROOT, check=True)


class Harness:
    def start(self):
        self.m = fixtures.Machine()
        load_probe(self.m, 'ledgegrab-probe.elf', 'TestLedgeInit')

    def info(self, key):
        value = self.m.call('TestLedgeInfo', key)
        return value if value < 0x80000000 else value - 0x100000000
    def color(self): return self.m.call('TestLedgeInfo', 3)
    def rows(self): return tuple(self.info(key) for key in (0, 1, 2))
    def step(self, frames=1):
        # In chunks: one emulator call has an instruction budget.
        for done in range(0, frames, 200):
            self.m.call('TestLedgeThink', min(frames - done, 200))
    def hold(self, buttons): self.m.call('TestLedgeHold', buttons)
    def press(self, button):
        self.hold(button)
        self.step()
        self.hold(0)
    def choose(self, row, value):
        self.m.call('TestLedgeChoice', row, value)
        self.step()
    def line(self, ax, ay, bx, by, flags=0, enabled=1):
        return self.m.call('TestLedgeAddLine', *[round(v * 1000) for v in (ax, ay, bx, by)], flags, enabled)
    def battlefield(self):
        self.m.call('TestLedgeStage', BATTLEFIELD)
        self.line(-68.4, 0, 68.4, 0)
        self.line(-57.6, 27.2, -20, 27.2, 0x100)
        self.line(20, 27.2, 57.6, 27.2, 0x100)
        self.line(-18.8, 54.4, 18.8, 54.4, 0x100)
    def move(self, frames):
        self.m.call('TestLedgeState', DASH, 0)
        self.step(frames)
    def grab(self, x=-68, state=CLIFFCATCH):
        self.m.call('TestLedgeState', state, x)
        self.step()
    def finish(self, frames, x=-68):
        """One full run, through the linger to the restart."""
        self.move(frames)
        self.grab(x)
        self.step(LINGER)
    def capture(self):
        self.m.cpu.mem_map(FIFO, 0x1000)
        self.fifo = []
        def write(cpu, access, address, size, value, user):
            self.fifo.append(value & 0xFFFFFFFF)
        self.m.cpu.hook_add(UC_HOOK_MEM_WRITE, write, begin=FIFO, end=FIFO + 3)
    def quads(self, pass_=2):
        """What one GX pass draws: (color, [(x, y) * 4]), in draw order."""
        self.fifo.clear()
        self.m.call('TestLedgeDraw', pass_)
        result = []
        for at in range(0, len(self.fifo), 7 * 4):
            corners = [struct.unpack('>2f', struct.pack('>2I', *self.fifo[at + i:at + i + 2]))
                       for i in range(0, 7 * 4, 7)]
            result.append((bytes(self.fifo[at + 3:at + 7]), corners))
        return result


class LedgegrabRunTests(Harness, unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def setUp(self):
        self.start()
        self.battlefield()
        self.step()

    def test_first_frame_saves_once_places_you_and_shows_no_times_yet(self):
        self.assertEqual((self.info(10), self.info(9), self.info(30)), (1, 1, WAIT))
        self.assertEqual(self.rows(), (0, NONE, NONE))
        self.assertEqual(tuple(chr(self.info(key)) for key in (19, 20, 21)), ('F', 'S', 'P'))
        self.assertEqual(self.color(), WHITE)

    def test_best_and_previous_sit_below_the_counter(self):
        self.assertEqual(self.info(18), 15)  # Outline and text for each of the 3 rows.
        self.assertLess(self.info(22), self.info(23))
        self.assertLess(self.info(23), self.info(24))

    def time(self): return tuple(self.info(key) for key in (43, 44, 45))

    def test_counter_reads_frames_then_the_time_in_minutes_seconds_and_hundredths(self):
        pointer = self.m.call('TestLedgeCounterFormat')
        text = bytes(self.m.cpu.mem_read(pointer, 40)).split(b'\0', 1)[0].decode()
        self.assertEqual(text, '%s: %d (%02d:%02d.%02d)')
        self.assertEqual(self.time(), (0, 0, 0))

    def test_counter_time_counts_up_with_the_frames(self):
        self.move(1)
        self.assertEqual(self.time(), (0, 0, 1))
        self.step(29)  # 30 frames.
        self.assertEqual(self.time(), (0, 0, 50))
        self.step(30)  # 60.
        self.assertEqual(self.time(), (0, 1, 0))
        self.step(1739)  # 1799, the last frame before a DNF.
        self.assertEqual((self.rows()[0], self.time()), (1799, (0, 29, 98)))

    def test_counter_time_clears_with_the_restart(self):
        self.finish(90)
        self.assertEqual(self.time(), (0, 0, 0))

    def test_count_waits_for_your_first_move_and_is_white_while_it_runs(self):
        self.step(500)
        self.assertEqual(self.rows()[0], 0)
        self.move(1)
        self.assertEqual((self.rows()[0], self.color()), (1, WHITE))
        self.step(41)
        self.assertEqual((self.rows()[0], self.color()), (42, WHITE))

    def test_run_stops_on_the_goal_ledge_and_a_first_result_is_a_cyan_best(self):
        self.move(42)
        self.grab()
        self.assertEqual(self.rows(), (42, 42, NONE))
        self.assertEqual(self.color(), CYAN)
        self.step(LINGER - 1)
        self.assertEqual(self.rows()[0], 42)

    def test_tether_style_arrival_directly_in_cliffwait_also_counts(self):
        self.move(42)
        self.grab(state=CLIFFWAIT)
        self.assertEqual(self.rows(), (42, 42, NONE))

    def test_the_other_ledge_does_not_stop_the_count(self):
        self.move(42)
        self.grab(x=68)
        self.assertEqual(self.rows(), (43, NONE, NONE))
        self.assertEqual(self.color(), WHITE)

    def test_result_lingers_then_restarts_and_moves_previous_down(self):
        self.move(42)
        self.grab()
        self.step(LINGER - 1)
        self.assertEqual((self.rows(), self.info(30)), ((42, 42, NONE), CLIFFCATCH))
        self.step()
        self.assertEqual((self.rows(), self.info(30)), ((0, 42, 42), WAIT))
        self.assertEqual(self.color(), WHITE)

    def test_previous_shows_the_run_before_during_a_linger(self):
        self.finish(50)
        self.move(60)
        self.grab()
        self.assertEqual(self.rows(), (60, 50, 50))
        self.step(LINGER)
        self.assertEqual(self.rows(), (0, 50, 60))

    def test_colors_grade_a_run_against_the_session_best(self):
        self.finish(100)
        # Green through 4 frames off, yellow through 10, red past that.
        for frames, color in ((101, GREEN), (104, GREEN), (105, YELLOW), (110, YELLOW), (111, RED),
                              (100, CYAN), (99, CYAN)):
            with self.subTest(frames=frames):
                self.move(frames)
                self.grab()
                self.assertEqual((self.rows()[0], self.color()), (frames, color))
                self.step(LINGER)
        self.assertEqual(self.rows()[1], 99)

    def test_color_ranges_are_the_same_for_short_and_long_runs(self):
        for best in (10, 1000):
            self.choose(START, CENTER if best == 10 else AT_FAR)
            self.finish(best)
            for off, color in ((4, GREEN), (10, YELLOW), (11, RED)):
                with self.subTest(best=best, off=off):
                    self.move(best + off)
                    self.grab()
                    self.assertEqual(self.color(), color)
                    self.step(LINGER)

    def test_thirty_seconds_is_a_red_dnf_that_lingers_and_restarts(self):
        self.move(DNF - 1)
        self.assertEqual((self.rows()[0], self.color()), (DNF - 1, WHITE))
        self.step()
        self.assertEqual((self.rows(), self.color()), ((SHOWN_DNF, NONE, NONE), RED))
        self.step(LINGER - 1)
        self.assertEqual(self.rows(), (SHOWN_DNF, NONE, NONE))
        self.step()
        self.assertEqual((self.rows(), self.color(), self.info(30)), ((0, NONE, SHOWN_DNF), WHITE, WAIT))

    def test_a_dnf_never_replaces_a_best_and_a_slower_catch_still_scores(self):
        self.finish(100)
        self.move(DNF)
        self.step(LINGER)
        self.assertEqual(self.rows(), (0, 100, SHOWN_DNF))
        self.finish(DNF - 1)
        self.assertEqual(self.rows(), (0, 100, DNF - 1))

    def test_dying_is_an_immediate_dnf_and_restart(self):
        self.finish(50)
        self.move(20)
        loads = self.info(9)
        self.m.call('TestLedgeDead')
        self.step()
        self.assertEqual((self.rows(), self.info(30), self.info(9)), ((0, 50, SHOWN_DNF), WAIT, loads + 1))

    def test_dying_during_a_linger_keeps_the_result(self):
        self.move(50)
        self.grab()
        self.m.call('TestLedgeDead')
        self.step()
        self.assertEqual(self.rows(), (0, 50, 50))

    def test_each_start_and_facing_keeps_its_own_times(self):
        self.finish(50)
        self.choose(START, CENTER)
        self.assertEqual(self.rows(), (0, NONE, NONE))
        self.finish(70)
        self.choose(FACING, AWAY)
        self.assertEqual(self.rows(), (0, NONE, NONE))
        self.finish(80)
        self.assertEqual(self.rows(), (0, 80, 80))
        self.choose(FACING, TOWARD)
        self.assertEqual(self.rows(), (0, 70, 70))
        self.choose(START, AT_LEDGE)
        self.assertEqual(self.rows(), (0, 50, 50))

    def best_row(self):
        return tuple(bytes(self.m.cpu.mem_read(self.m.call('TestLedgeBestText', which), 40)).split(b'\0', 1)[0].decode()
                     for which in (2, 0, 1))

    def test_session_best_reads_left_then_right(self):
        self.assertEqual(self.best_row(), ('Session Best L: %s  R: %s', '--', '--'))
        self.finish(50)
        self.assertEqual(self.best_row()[1:], ('50', '--'))
        self.choose(GOAL, RIGHT)
        self.finish(1234, x=68)
        self.assertEqual(self.best_row()[1:], ('50', '1234'))

    def test_a_goal_only_changes_its_own_session_best(self):
        self.finish(50)
        self.assertEqual((self.rows()[1], self.info(40)), (50, NONE))
        self.choose(GOAL, RIGHT)
        self.finish(40, x=68)
        self.assertEqual((self.rows()[1], self.info(40)), (50, 40))
        self.finish(45, x=68)  # Slower, so the right keeps its 40.
        self.assertEqual((self.rows()[1], self.info(40)), (50, 40))
        self.choose(GOAL, LEFT)
        self.finish(30)
        self.assertEqual((self.rows()[1], self.info(40)), (30, 40))

    def test_session_best_stays_up_for_both_sides_whichever_is_the_goal(self):
        self.finish(50)
        self.choose(GOAL, RIGHT)
        self.finish(60, x=68)
        self.choose(GOAL, LEFT)
        self.assertEqual((self.rows()[1], self.info(40)), (50, 60))

    def test_colors_compare_against_the_goal_in_play(self):
        self.finish(50)
        self.choose(GOAL, RIGHT)
        self.move(60)
        self.grab(68)
        self.assertEqual(self.color(), CYAN)  # A first time on this side, though the left is faster.
        self.step(LINGER)
        self.move(62)
        self.grab(68)
        self.assertEqual(self.color(), GREEN)  # Against 60, not the left's 50.

    def test_previous_is_shared_by_both_goals(self):
        self.finish(50)
        self.choose(GOAL, RIGHT)
        self.assertEqual(self.rows()[2], 50)
        self.finish(60, x=68)
        self.assertEqual(self.rows()[2], 60)
        self.choose(GOAL, LEFT)
        self.assertEqual(self.rows()[2], 60)

    def test_all_records_fit_in_96_bytes(self):
        self.assertEqual(self.info(25), 96)

    def test_every_start_can_hold_a_best_at_once(self):
        for facing in (TOWARD, AWAY):
            for start in range(8):
                self.choose(START, start); self.choose(FACING, facing)
                self.finish(10 + start + 8 * facing)
        for facing in (TOWARD, AWAY):
            for start in range(8):
                self.choose(START, start); self.choose(FACING, facing)
                self.assertEqual(self.rows(), (0, 10 + start + 8 * facing, 10 + start + 8 * facing))

    def test_dpad_left_and_right_cycle_the_start_and_wrap_around(self):
        for button, start in ((DPAD_RIGHT, AT_FAR), (DPAD_RIGHT, UNDER_CLOSE), (DPAD_LEFT, AT_FAR),
                              (DPAD_LEFT, AT_LEDGE), (DPAD_LEFT, FAR), (DPAD_RIGHT, AT_LEDGE)):
            loads = self.info(9)
            self.press(button)
            self.assertEqual((self.info(14), self.info(9)), (start, loads + 1))
        self.assertEqual((self.info(13), self.info(15)), (LEFT, TOWARD))  # Goal and facing stay.

    def test_dpad_left_and_right_move_you_to_the_start(self):
        self.assertAlmostEqual(self.info(5) / 1000, -63.4, 2)
        self.press(DPAD_RIGHT)
        self.assertAlmostEqual(self.info(5) / 1000, 63.4, 2)

    def test_dpad_skips_starts_this_stage_lacks_in_both_directions(self):
        self.m.call('TestLedgeStage', FD)
        self.line(-85, 0, 85, 0)
        self.choose(START, CENTER)
        self.press(DPAD_RIGHT)
        self.assertEqual(self.info(14), AT_LEDGE)
        self.press(DPAD_LEFT)
        self.assertEqual(self.info(14), CENTER)
        self.press(DPAD_LEFT)
        self.assertEqual(self.info(14), AT_FAR)

    def test_dpad_down_switches_the_goal_ledge_and_restarts(self):
        self.assertEqual(self.info(7), -1)
        self.press(DPAD_DOWN)
        self.assertEqual((self.info(13), self.info(14), self.info(15)), (RIGHT, AT_LEDGE, TOWARD))
        self.assertAlmostEqual(self.info(5) / 1000, 63.4, 2)
        self.assertEqual(self.info(7), 1)  # Still toward the goal, now the other way.
        self.press(DPAD_DOWN)
        self.assertEqual(self.info(13), LEFT)
        self.assertAlmostEqual(self.info(5) / 1000, -63.4, 2)

    def test_dpad_up_switches_facing_and_restarts_in_place(self):
        self.assertEqual(self.info(7), -1)
        self.press(DPAD_UP)
        self.assertEqual((self.info(15), self.info(7), self.info(13), self.info(14)), (AWAY, 1, LEFT, AT_LEDGE))
        self.assertAlmostEqual(self.info(5) / 1000, -63.4, 2)
        self.press(DPAD_UP)
        self.assertEqual((self.info(15), self.info(7)), (TOWARD, -1))

    def test_holding_the_dpad_acts_once_however_long_it_is_held(self):
        # Each restart rewinds the fighter's buttons, so only the pad can tell a
        # held button from a new press.
        for button, key, after in ((DPAD_RIGHT, 14, AT_FAR), (DPAD_LEFT, 14, AT_LEDGE),
                                   (DPAD_DOWN, 13, RIGHT), (DPAD_UP, 15, AWAY)):
            with self.subTest(button=button):
                loads = self.info(9)
                self.hold(button)
                self.step(12)
                self.assertEqual((self.info(key), self.info(9)), (after, loads + 1))
                self.hold(0)
                self.step(2)

    def test_pressing_again_after_letting_go_acts_again(self):
        self.hold(DPAD_RIGHT)
        self.step(6)
        self.hold(0)
        self.step(3)
        self.hold(DPAD_RIGHT)
        self.step(6)
        self.assertEqual(self.info(14), UNDER_CLOSE)

    def assert_no_taunt(self):
        self.hold(DPAD_UP)
        self.step(12)
        self.assertEqual((self.info(30), self.rows()[0]), (WAIT, 0))
        self.hold(0)
        self.step(12)
        self.assertEqual((self.info(30), self.rows()[0], self.info(15)), (WAIT, 0, AWAY))

    def test_dpad_up_never_starts_the_count_with_a_taunt(self):
        self.assert_no_taunt()

    def test_dpad_up_never_taunts_when_the_fighter_reads_input_first(self):
        # The savestate then holds a fighter already past its first input frame,
        # which takes the other path to its previous buttons.
        self.start()
        self.battlefield()
        self.m.call('TestLedgeFighterReadsFirst', 1)
        self.step()
        self.assert_no_taunt()

    def test_a_restart_only_hides_the_dpad_from_the_fighter(self):
        self.hold(DPAD_RIGHT | BUTTON_A)
        self.step()
        self.assertEqual(self.info(31), BUTTON_A)  # Holding A through a restart can still start a run.

    def test_any_dpad_press_drops_a_run_without_counting_it(self):
        for button in (DPAD_LEFT, DPAD_RIGHT, DPAD_DOWN, DPAD_UP):
            with self.subTest(button=button):
                self.move(20)
                self.press(button)
                self.assertEqual((self.rows(), self.info(30)), ((0, NONE, NONE), WAIT))

    def test_reset_speed_starts_on_short_and_comes_back_from_the_save(self):
        self.assertEqual(self.info(41), RESET_SHORT)
        self.m.call('TestLedgeSavedResetSpeed', RESET_LINGER)
        self.m.call('TestLedgeInit')
        self.assertEqual(self.info(41), RESET_LINGER)

    def test_reset_speed_saves_as_soon_as_it_changes_without_touching_the_run(self):
        self.move(10)
        loads = self.info(9)
        self.m.call('TestLedgeChoice', RESET, RESET_LINGER)
        self.assertEqual(self.info(42), RESET_LINGER)
        self.step()
        self.assertEqual((self.info(9), self.rows()[0]), (loads, 11))

    def test_linger_shows_a_result_for_a_second_and_short_for_half_of_one(self):
        for speed, frames in ((RESET_LINGER, 60), (RESET_SHORT, 30)):
            with self.subTest(speed=speed):
                self.choose(RESET, speed)
                self.move(10)
                self.grab()
                self.step(frames - 1)
                self.assertEqual(self.rows()[0], 10)
                self.step()
                self.assertEqual(self.rows()[0], 0)

    def test_instant_restarts_on_the_frame_you_grab_the_ledge(self):
        self.choose(RESET, RESET_INSTANT)
        self.move(42)
        loads = self.info(9)
        self.grab()
        self.assertEqual((self.rows(), self.info(30), self.info(9), self.color()),
                         ((0, 42, 42), WAIT, loads + 1, WHITE))

    def test_instant_still_times_the_next_run(self):
        self.choose(RESET, RESET_INSTANT)
        self.move(42)
        self.grab()
        self.step(20)
        self.assertEqual(self.rows()[0], 0)  # Waits for the next first move.
        self.move(40)
        self.grab()
        self.assertEqual(self.rows(), (0, 40, 40))

    def test_instant_restarts_a_dnf_at_once_too(self):
        self.choose(RESET, RESET_INSTANT)
        self.move(DNF)
        self.assertEqual((self.rows(), self.info(30)), ((0, NONE, SHOWN_DNF), WAIT))

    def test_instant_restarts_only_once_when_you_die(self):
        self.choose(RESET, RESET_INSTANT)
        self.move(50)
        loads = self.info(9)
        self.m.call('TestLedgeDead')
        self.step()
        self.assertEqual((self.rows(), self.info(9)), ((0, NONE, SHOWN_DNF), loads + 1))

    def test_dpad_during_a_linger_files_the_result_under_the_run_it_was(self):
        self.move(50)
        self.grab()
        self.press(DPAD_RIGHT)
        self.assertEqual((self.info(14), self.rows()), (AT_FAR, (0, NONE, NONE)))
        self.press(DPAD_LEFT)
        self.assertEqual((self.info(14), self.rows()), (AT_LEDGE, (0, 50, 50)))

    def test_help_explains_the_dpad_and_the_menu_text_fits(self):
        def desc(row, line):
            pointer = self.m.call('TestLedgeDesc', row, line)
            return bytes(self.m.cpu.mem_read(pointer, 80)).split(b'\0', 1)[0].decode() if pointer else ''
        help_text = ' '.join(desc(6, line) for line in range(4))
        for controls in ('D-Pad left/right: previous or next start position', 'D-Pad down: switch ledge',
                         'D-Pad up: switch facing'):
            self.assertIn(controls, help_text)
        for row in range(self.m.call('TestLedgeOptionCount')):
            for line in range(4):
                self.assertLessEqual(len(desc(row, line)), 53)  # Low Laser's longest line.

    def test_changing_a_start_during_a_linger_files_the_result_under_the_old_start(self):
        self.move(50)
        self.grab()
        self.choose(START, CENTER)
        self.assertEqual(self.rows(), (0, NONE, NONE))
        self.choose(START, AT_LEDGE)
        self.assertEqual(self.rows(), (0, 50, 50))

    def test_pausing_stops_the_count_hides_the_readout_and_ignores_the_dpad(self):
        self.move(10)
        self.m.call('TestLedgePause', 1)
        self.hold(DPAD_DOWN)
        self.step(100)
        self.assertEqual((self.rows()[0], self.info(4), self.info(13)), (10, 1, LEFT))
        self.hold(0)
        self.m.call('TestLedgePause', 0)
        self.step()
        self.assertEqual((self.rows()[0], self.info(4)), (11, 0))

    def test_menu_changes_wait_for_the_next_frame_to_restart(self):
        loads = self.info(9)
        self.m.call('TestLedgePause', 1)
        self.m.call('TestLedgeChoice', FACING, AWAY)
        self.step()
        self.assertEqual(self.info(9), loads)
        self.m.call('TestLedgePause', 0)
        self.step()
        self.assertEqual((self.info(9), self.info(15)), (loads + 1, AWAY))

    def test_rows_only_rewrite_when_they_change(self):
        writes = self.info(17)
        self.step(100)
        self.assertEqual(self.info(17), writes)
        self.move(1)
        self.assertEqual(self.info(17), writes + 5)

    def test_nana_starts_with_popo(self):
        self.m.call('TestLedgeNana', 1)
        self.choose(GOAL, RIGHT)
        self.assertEqual(self.info(27), self.info(5))
        self.assertEqual(self.info(28), self.info(5))

    def test_exit_ends_the_match(self):
        self.m.call('TestLedgeExit')
        self.assertEqual(self.info(12), 1)

    def test_change_stage_carries_the_menu_and_restarts(self):
        self.choose(GOAL, RIGHT); self.choose(START, TOP); self.choose(FACING, AWAY)
        self.choose(CAMERA, CAMERA_FIGHTER)
        self.m.call('TestLedgeStageSwap', 4)
        self.assertEqual(self.info(11), FOUNTAIN)
        self.m.call('TestLedgeChoice', GOAL, LEFT)
        self.m.call('TestLedgeChoice', FACING, TOWARD)
        self.m.call('TestLedgeChoice', START, AT_LEDGE)
        self.m.call('TestLedgeChoice', CAMERA, CAMERA_GOAL)
        self.m.call('TestLedgeInit')
        self.assertEqual((self.info(13), self.info(14), self.info(15), self.info(38)),
                         (RIGHT, TOP, AWAY, CAMERA_FIGHTER))

    def test_a_non_legal_stage_restarts_on_battlefield(self):
        self.start()
        self.m.call('TestLedgeStage', BIG_BLUE)
        self.line(-50, 0, 50, 0)
        self.step()
        self.assertEqual((self.info(11), self.info(9), self.info(10)), (BATTLEFIELD, 0, 0))


class LedgegrabMarkerTests(Harness, unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def setUp(self):
        self.start()
        self.battlefield()
        self.capture()

    def test_nothing_draws_until_the_ledges_are_found(self):
        self.assertEqual(self.quads(), [])
        self.step()
        self.assertEqual(len(self.quads()), 8)

    def test_markers_draw_once_a_frame_over_the_stage_and_fighters(self):
        self.step()
        self.assertEqual(self.quads(pass_=0), [])
        self.assertEqual(self.quads(pass_=1), [])
        self.assertEqual(len(self.quads()), 8)
        self.assertEqual((self.info(32), self.info(33), self.info(34)), (32, 128, 1))  # GX_QUADS, no depth.
        self.assertEqual((self.info(35), self.info(36)), (7, 255))  # Last in MATCHGX_EFFECT.

    def test_black_edges_go_under_a_red_x_and_a_green_exclamation_point(self):
        self.step()
        colors = [color for color, _ in self.quads()]
        black, red, green = bytes((0, 0, 0, 255)), bytes((255, 0, 0, 255)), bytes((0, 255, 0, 255))
        self.assertEqual(colors, [black] * 4 + [red, red, green, green])

    @staticmethod
    def bounds(quad):
        xs, ys = zip(*quad[1])
        return min(xs), max(xs), min(ys), max(ys)

    def assert_near(self, actual, expected):
        for a, e in zip(actual, expected):
            self.assertAlmostEqual(a, e, delta=.001)

    def test_goal_ledge_gets_the_exclamation_point_and_the_other_the_x(self):
        self.step()
        for wrong, target, press in ((68.4, -68.4, False), (-68.4, 68.4, True)):
            if press:
                self.press(DPAD_DOWN)
            quads = self.quads()
            for stroke in quads[4:6]:
                self.assert_near(self.bounds(stroke), (wrong - 9.5, wrong + 9.5, -9.5, 9.5))
            self.assert_near(self.bounds(quads[6]), (target - 2.25, target + 2.25, 20, 24.5))
            self.assert_near(self.bounds(quads[7]), (target - 2.75, target + 2.75, 26, 39))

    def test_x_strokes_cross_and_the_exclamation_bar_narrows_to_the_dot(self):
        self.step()
        quads = self.quads()
        rising, falling = quads[4][1], quads[5][1]
        self.assert_near(rising[0], (68.4 - 5.5, -9.5))
        self.assert_near(rising[2], (68.4 + 5.5, 9.5))
        self.assert_near(falling[0], (68.4 + 5.5, -9.5))
        self.assert_near(falling[2], (68.4 - 5.5, 9.5))
        bar = quads[7][1]
        self.assertAlmostEqual(bar[1][0] - bar[0][0], 2.5, delta=.001)
        self.assertAlmostEqual(bar[2][0] - bar[3][0], 5.5, delta=.001)

    def test_black_edge_surrounds_each_shape(self):
        self.step()
        quads = self.quads()
        # A stroke grows in length and thickness, so 1.5 along each axis.
        for edge, shape, grown in zip(quads[:4], quads[4:], (1.5, 1.5, .75, .75)):
            left, right, bottom, top = self.bounds(shape)
            self.assert_near(self.bounds(edge), (left - grown, right + grown, bottom - grown, top + grown))

    def test_marker_moves_with_the_goal_from_the_menu_too(self):
        self.step()
        self.choose(GOAL, RIGHT)
        self.assert_near(self.bounds(self.quads()[6])[:2], (66.15, 70.65))

    def framed(self):
        return tuple(self.m.call('TestLedgeSubject', ledge, 2) for ledge in (0, 1))

    def test_a_ledge_camera_subject_stands_at_the_ledge_and_keeps_the_marker_in_frame(self):
        self.step()
        self.assertEqual(self.info(37), 2)
        for ledge, x in ((0, -68.4), (1, 68.4)):
            subject = [self.m.call('TestLedgeSubject', ledge, key) for key in range(8)]
            signed = [v if v < 0x80000000 else v - 0x100000000 for v in subject]
            self.assertAlmostEqual(signed[0] / 1000, x, delta=.01)
            self.assertEqual(signed[1], 0)
            self.assertLess(signed[3], 0)
            self.assertGreater(signed[4], 0)
            self.assertGreaterEqual(signed[5] / 1000, 39.75)  # Past the top of the black edge.
            self.assertLess(signed[6], 0)
            self.assertEqual(signed[7], 1)

    def test_the_camera_starts_on_a_fake_player_at_the_goal_ledge_alone(self):
        self.step()
        self.assertEqual(self.info(38), CAMERA_GOAL)
        self.assertEqual(self.framed(), (FRAMED, NOT_FRAMED))

    def test_the_fake_player_moves_to_whichever_ledge_is_the_goal(self):
        self.step()
        self.press(DPAD_DOWN)
        self.assertEqual(self.framed(), (NOT_FRAMED, FRAMED))
        self.choose(GOAL, LEFT)
        self.assertEqual(self.framed(), (FRAMED, NOT_FRAMED))

    def test_camera_can_frame_both_ledges_or_only_the_fighter(self):
        self.step()
        self.choose(CAMERA, CAMERA_BOTH)
        self.assertEqual(self.framed(), (FRAMED, FRAMED))
        self.choose(CAMERA, CAMERA_FIGHTER)
        self.assertEqual(self.framed(), (NOT_FRAMED, NOT_FRAMED))
        self.press(DPAD_DOWN)  # The goal changing doesn't bring one back.
        self.assertEqual(self.framed(), (NOT_FRAMED, NOT_FRAMED))

    def test_changing_the_camera_applies_at_once_and_snaps_without_touching_the_run(self):
        self.step()
        self.move(10)
        loads, snaps = self.info(9), self.info(39)
        self.m.call('TestLedgePause', 1)
        self.m.call('TestLedgeChoice', CAMERA, CAMERA_BOTH)
        self.assertEqual(self.framed(), (FRAMED, FRAMED))
        self.assertEqual(self.info(39), snaps + 1)
        self.m.call('TestLedgePause', 0)
        self.step()
        self.assertEqual((self.info(9), self.rows()[0]), (loads, 11))

    def test_restarts_do_not_add_camera_subjects(self):
        self.step()
        self.press(DPAD_DOWN)
        self.finish(20, x=68)
        self.assertEqual(self.info(37), 2)


class LedgegrabPlatformTests(Harness, unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def setUp(self):
        self.start()
        self.battlefield()
        self.step()

    def test_missing_platform_is_skipped_in_the_selector_direction(self):
        self.m.call('TestLedgeStage', FD)
        self.line(-85, 0, 85, 0)
        self.choose(START, CENTER)
        self.assertEqual(self.info(16), 1 << AT_LEDGE | 1 << AT_FAR | 1 << CENTER)
        self.assertEqual(self.info(26), ord('T') | ord('N') << 8)  # "Top Plat (N/A)"
        self.m.call('TestLedgeChoice', START, TOP)  # Right from Center: past them all.
        self.assertEqual(self.info(14), AT_LEDGE)
        self.m.call('TestLedgeChoice', START, AT_FAR)
        self.m.call('TestLedgeChoice', START, UNDER_CLOSE)
        self.assertEqual(self.info(14), CENTER)
        self.m.call('TestLedgeChoice', START, UNDER_FAR)  # Left from Center.
        self.assertEqual(self.info(14), AT_FAR)

    def test_platform_that_vanishes_falls_back_to_center_with_its_own_times(self):
        # Fountain: a side platform's line can be disabled when a run restarts.
        self.m.call('TestLedgeStage', FOUNTAIN)
        self.line(-63, 0, 63, 0)
        platform = self.line(-44, 20, -16, 20, 0x100)
        self.line(16, 20, 44, 20, 0x100)
        self.choose(START, CLOSE)
        self.assertEqual(self.info(14), CLOSE)
        self.finish(50)
        self.assertEqual(self.rows(), (0, 50, 50))
        self.m.call('TestLedgeEnableLine', platform, 0)
        self.choose(GOAL, LEFT)
        self.assertEqual((self.info(14), self.info(5), self.rows()), (CENTER, 0, (0, NONE, NONE)))

    def test_a_carried_platform_falls_back_on_a_stage_without_it(self):
        self.choose(START, TOP)
        self.m.call('TestLedgeStage', FD)
        self.line(-85, 0, 85, 0)
        self.choose(GOAL, RIGHT)
        self.assertEqual(self.info(14), CENTER)


class LedgegrabStageTests(Harness, unittest.TestCase):
    """Production placement against each legal stage's native collision lines."""
    @classmethod
    def setUpClass(cls):
        build()
        import test_techchase
        cls.floors = {name: test_techchase.native_floors(name) for name, _, _ in LEGAL}

    def setUp(self):
        self.start()
        self.battlefield()
        self.step()

    def load(self, filename, stage, normal):
        self.m.call('TestLedgeStage', stage)
        lines = []
        for index, ax, ay, bx, by, flags in self.floors[filename]:
            enabled = normal is None or index in normal
            self.line(ax, ay, bx, by, flags, enabled)
            if enabled:
                a, b = sorted(((round(ax * 1000) / 1000, round(ay * 1000) / 1000), (round(bx * 1000) / 1000, round(by * 1000) / 1000)))
                lines.append((a[0], a[1], b[0], b[1], flags & 0x100))
        return lines

    @staticmethod
    def ground_y(lines, x):
        return max(ay + (by - ay) * (x - ax) / (bx - ax) for ax, ay, bx, by, platform in lines
                   if not platform and ax <= x <= bx and bx > ax)

    def test_markers_sit_on_each_stage_real_ledges(self):
        self.capture()
        for filename, stage, normal in LEGAL:
            with self.subTest(stage=filename):
                lines = self.load(filename, stage, normal)
                self.m.call('TestLedgeReset')
                self.step()
                left = min(l[0] for l in lines if not l[4]); right = max(l[2] for l in lines if not l[4])
                self.m.call('TestLedgeChoice', GOAL, LEFT)
                quads = self.quads()
                x_left, x_right, x_bottom, x_top = LedgegrabMarkerTests.bounds(quads[4])
                self.assertAlmostEqual((x_left + x_right) / 2, right, delta=.01)
                self.assertAlmostEqual((x_bottom + x_top) / 2, self.ground_y(lines, right), delta=.01)
                bang_left, bang_right, bang_bottom, _ = LedgegrabMarkerTests.bounds(quads[6])
                self.assertAlmostEqual((bang_left + bang_right) / 2, left, delta=.01)
                self.assertAlmostEqual(bang_bottom - 20, self.ground_y(lines, left), delta=.01)

    def test_every_start_is_mirrored_from_the_goal_ledge_on_every_legal_stage(self):
        for filename, stage, normal in LEGAL:
            lines = self.load(filename, stage, normal)
            left = min(l[0] for l in lines if not l[4]); right = max(l[2] for l in lines if not l[4])
            platforms = [((l[0] + l[2]) / 2, l[1]) for l in lines if l[4]]
            def side_platform(side):
                near = [p for p in platforms if p[0] * side > 10]
                return min(near, key=lambda p: abs(p[0])) if near else None
            top = max((p for p in platforms if abs(p[0]) < 10), key=lambda p: p[1], default=None)
            for goal, outward in ((LEFT, -1), (RIGHT, 1)):
                near_edge = right - 5 if outward > 0 else left + 5
                far_edge = left + 5 if outward > 0 else right - 5
                expected = {AT_LEDGE: (near_edge, None), AT_FAR: (far_edge, None), CENTER: (0, None)}
                close, far = side_platform(outward), side_platform(-outward)
                if close and far:
                    expected.update({CLOSE: close, FAR: far, UNDER_CLOSE: (close[0], None), UNDER_FAR: (far[0], None)})
                if top:
                    expected[TOP] = top
                for start in range(8):
                    for facing, sign in ((TOWARD, outward), (AWAY, -outward)):
                        with self.subTest(stage=filename, goal=goal, start=start, facing=facing):
                            self.m.call('TestLedgeChoice', GOAL, goal)
                            self.m.call('TestLedgeChoice', FACING, facing)
                            self.choose(START, start)
                            self.assertEqual(self.info(16), sum(1 << key for key in expected))
                            if start not in expected:
                                continue
                            self.assertEqual(self.info(14), start)
                            x, y = expected[start]
                            y = self.ground_y(lines, x) if y is None else y
                            self.assertAlmostEqual(self.info(5) / 1000, x, delta=.01)
                            self.assertAlmostEqual(self.info(6) / 1000, y, delta=.01)
                            self.assertEqual(self.info(7), sign)
                            self.assertGreaterEqual(self.info(8), 0)


class LedgegrabDATTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def test_real_relocated_dat_times_a_run_and_keeps_its_best(self):
        fixture = fixtures.RelocatedDATSettingsTests()
        fixture.dat_path = ROOT/'build/ledgegrab-test.dat'; fixture.dat_root = b'evFunction'
        fixture.setUp(); m = fixture.m
        load_probe(m, 'ledgegrab-probe.elf', 'TestLedgeInit')
        dat = fixture.dat_path.read_bytes()
        _, size, relocs, roots, refs = struct.unpack_from('>5I', dat)
        nodes = 32 + size + relocs * 4; strings = nodes + (roots + refs) * 8
        for i in range(roots):
            at, name = struct.unpack_from('>II', dat, nodes + i * 8)
            if dat[strings + name:].split(b'\0', 1)[0] == b'evFunction':
                _, _, _, exports, count, _ = struct.unpack_from('>6I', dat, 32 + at); break
        entries = dict(struct.unpack_from('>II', dat, 32 + exports + i * 8) for i in range(count))
        base = fixture.base
        for index, name in ((0, 'DAT_Init'), (1, 'DAT_Update'), (2, 'DAT_Think')): m.symbols[name] = base + entries[index]
        # Production DAT calls native addresses; substitute the same fixture endpoints.
        for line in (ROOT/'MexTK/melee.link').read_text().splitlines():
            if ':' not in line: continue
            address, name = line.split(':', 1)
            if name in m.symbols and name not in ('Event_Init', 'Event_Think', 'Event_Update'):
                target = m.symbols[name]
                m.cpu.mem_write(int(address, 16), struct.pack('>4I', 0x3D800000 | (target >> 16),
                    0x618C0000 | (target & 65535), 0x7D8903A6, 0x4E800420))
        for args in ((-68400, 0, 68400, 0, 0, 1), (-57600, 27200, -20000, 27200, 0x100, 1)):
            m.call('TestLedgeAddLine', *args)
        event = m.call('TestLedgeObject')
        def frame(count=1):
            for _ in range(count):
                m.call('TestLedgeTimer'); m.call('DAT_Think', event); m.call('DAT_Update')
        def rows(): return tuple(m.call('TestLedgeInfo', key) for key in (0, 1))
        m.call('TestLedgeReset')
        m.call('DAT_Init', event)
        frame()
        self.assertEqual(m.call('TestLedgeInfo', 30), WAIT)
        for frames, best, color in ((30, 30, CYAN), (31, 30, GREEN), (28, 28, CYAN)):
            m.call('TestLedgeState', DASH, 0)
            frame(frames)
            m.call('TestLedgeState', CLIFFCATCH, -68)
            frame()
            self.assertEqual(rows(), (frames, best))
            self.assertEqual(m.call('TestLedgeInfo', 3), color)
            frame(LINGER)
        self.assertEqual(m.call('TestLedgeInfo', 2), 28)
        # The DAT's own GX callback, through its relocated tables.
        writes = []
        m.cpu.mem_map(FIFO, 0x1000)
        m.cpu.hook_add(UC_HOOK_MEM_WRITE, lambda cpu, access, address, size, value, user: writes.append(size),
                       begin=FIFO, end=FIFO + 3)
        m.call('TestLedgeDraw', 2)
        self.assertEqual(writes, [4, 4, 4, 1, 1, 1, 1] * 32)
        self.assertEqual(m.call('TestLedgeInfo', 34), 1)


if __name__ == '__main__':
    unittest.main()
