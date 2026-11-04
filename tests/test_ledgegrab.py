"""Execute the production Ledgegrab Speedrun event and its relocated DAT."""
from pathlib import Path
import struct
import subprocess
import unittest
import test_settings as fixtures
from test_lowlaser import LEGAL, load_probe

ROOT = Path(__file__).resolve().parents[1]
WAIT, DASH, CLIFFCATCH, CLIFFWAIT = 14, 20, 252, 253
WHITE, CYAN, GREEN, YELLOW, RED = 0xFFFFFFFF, 0x00FFFFFF, 0x00FF00FF, 0xFFE000FF, 0xFF0000FF
DPAD_LEFT, DPAD_RIGHT = 1, 2
GOAL, START, FACING = 0, 1, 2
AT_LEDGE, AT_FAR, UNDER_CLOSE, UNDER_FAR, CENTER, TOP, CLOSE, FAR = range(8)
LEFT, RIGHT = 0, 1
TOWARD, AWAY = 0, 1
LINGER, DNF = 60, 1800
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
        self.step(30)
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

    def test_result_lingers_a_second_then_restarts_and_moves_previous_down(self):
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
        # 1/20 of 100 is 5 frames: green through 105, yellow through 115.
        for frames, color in ((105, GREEN), (106, YELLOW), (115, YELLOW), (116, RED), (100, CYAN), (99, CYAN)):
            with self.subTest(frames=frames):
                self.move(frames)
                self.grab()
                self.assertEqual((self.rows()[0], self.color()), (frames, color))
                self.step(LINGER)
        self.assertEqual(self.rows()[1], 99)

    def test_short_runs_keep_a_two_frame_green_window(self):
        self.finish(10)
        for frames, color in ((12, GREEN), (13, YELLOW), (16, YELLOW), (17, RED)):
            with self.subTest(frames=frames):
                self.move(frames)
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

    def test_left_and_right_goals_share_their_times(self):
        self.finish(50)
        self.choose(GOAL, RIGHT)
        self.assertEqual(self.rows(), (0, 50, 50))
        self.finish(40, x=68)
        self.assertEqual(self.rows(), (0, 40, 40))

    def test_all_records_fit_in_64_bytes(self):
        self.assertEqual(self.info(25), 64)

    def test_every_start_can_hold_a_best_at_once(self):
        for facing in (TOWARD, AWAY):
            for start in range(8):
                self.choose(START, start); self.choose(FACING, facing)
                self.finish(10 + start + 8 * facing)
        for facing in (TOWARD, AWAY):
            for start in range(8):
                self.choose(START, start); self.choose(FACING, facing)
                self.assertEqual(self.rows(), (0, 10 + start + 8 * facing, 10 + start + 8 * facing))

    def test_dpad_picks_that_goal_and_restarts_without_counting_the_run(self):
        self.move(20)
        self.m.call('TestLedgeDown', DPAD_RIGHT)
        self.step()
        self.m.call('TestLedgeDown', 0)
        self.assertEqual((self.info(13), self.rows(), self.info(30)), (RIGHT, (0, NONE, NONE), WAIT))
        self.assertAlmostEqual(self.info(5) / 1000, 63.4, 2)
        self.m.call('TestLedgeDown', DPAD_LEFT)
        self.step()
        self.m.call('TestLedgeDown', 0)
        self.assertEqual(self.info(13), LEFT)
        self.assertAlmostEqual(self.info(5) / 1000, -63.4, 2)

    def test_dpad_during_a_linger_keeps_the_finished_run_as_previous(self):
        self.move(50)
        self.grab()
        self.m.call('TestLedgeDown', DPAD_LEFT)
        self.step()
        self.m.call('TestLedgeDown', 0)
        self.assertEqual(self.rows(), (0, 50, 50))

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
        self.m.call('TestLedgeDown', DPAD_RIGHT)
        self.step(100)
        self.assertEqual((self.rows()[0], self.info(4), self.info(13)), (10, 1, LEFT))
        self.m.call('TestLedgeDown', 0)
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
        self.m.call('TestLedgeStageSwap', 4)
        self.assertEqual(self.info(11), FOUNTAIN)
        self.m.call('TestLedgeChoice', GOAL, LEFT)
        self.m.call('TestLedgeChoice', FACING, TOWARD)
        self.m.call('TestLedgeChoice', START, AT_LEDGE)
        self.m.call('TestLedgeInit')
        self.assertEqual((self.info(13), self.info(14), self.info(15)), (RIGHT, TOP, AWAY))

    def test_a_non_legal_stage_restarts_on_battlefield(self):
        self.start()
        self.m.call('TestLedgeStage', BIG_BLUE)
        self.line(-50, 0, 50, 0)
        self.step()
        self.assertEqual((self.info(11), self.info(9), self.info(10)), (BATTLEFIELD, 0, 0))


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


if __name__ == '__main__':
    unittest.main()
