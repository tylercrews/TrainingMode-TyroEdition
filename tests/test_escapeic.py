"""Execute the production Escape IC Grab event, frame by frame."""
from pathlib import Path
import subprocess
import unittest
import test_settings as fixtures
from test_lowlaser import load_probe

ROOT = Path(__file__).resolve().parents[1]
WAIT, CATCH, CAPTURE_PULLED, CAPTURE_WAIT, CAPTURE_CUT, CAPTURE_JUMP = 14, 0xD4, 0xDF, 0xE0, 0xE5, 0xE6
CATCH_PULL, CATCH_WAIT, CATCH_ATTACK = 213, 216, 217
THROW_F, THROW_B, THROW_HI, THROW_LW = 219, 220, 221, 222
THROWN_F, THROWN_B, THROWN_HI, THROWN_LW = 239, 240, 241, 242
OUT_NONE, OUT_MASH, OUT_MISSED, OUT_THROWN = range(4)
GREEN_BIG = 0x50FF5A  # The big count after mashing out of a handoff.
JUMP_AERIAL_F, FALL, KNEE_BEND, ESCAPE_AIR = 27, 29, 24, 236
Z = 0x10
SETUP, GRAB, THROWN, RESULT = range(4)
PERCENT_ROW, HANDOFF_ROW, FLASH_ROW = 0, 1, 2
NONE, DOWN_THROW = 0, 1  # Handoff choices.
SETUP_FRAMES, RESULT_FRAMES = 40, 60
GRAB_FRAME = 7  # The probe's grab connects on Popo's seventh frame,
PULL_FRAMES = 2  # and his pull ends two frames later (his Catch script's 0x50 on frame 8).
RELEASE_FRAMES = 30  # CaptureCut's animation; CaptureJump acts from its 30th frame.
DPAD_LEFT, DPAD_RIGHT, DPAD_DOWN, DPAD_UP = 1, 2, 4, 8
NEW_BEST_SFX, ESCAPE_SFX = 303, 0xAD
CYAN, WHITE = 0x00FFFF, 0xFFFFFF
YELLOW, GREEN = 0xFFF000, 0x50FF5A  # The global cue's (TM_CUE_YELLOW_COLOR, TM_CUE_GREEN_COLOR).
# The flash after a release, frame by frame from the frame you get out: two
# yellow frames before you can act, then two green.
FLASH = [0] * (RELEASE_FRAMES - 2) + [YELLOW, YELLOW, GREEN, GREEN, 0]


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
    def string(self, address):  # Longer than any menu line, so the width checks see all of it.
        return bytes(self.m.cpu.mem_read(address, 64)).split(b'\0', 1)[0].decode()
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
    def pull(self):
        """Popo pulls you in: the timer runs from the next frame."""
        self.step(PULL_FRAMES)
        self.assertEqual(self.info(8), CAPTURE_WAIT)
    def escape(self, inputs=0):
        """Mash `inputs` a frame until you're out."""
        self.m.call('TestICMash', inputs, 10000)
        for _ in range(400):
            if self.phase() == RESULT: return
            self.step()
        self.fail('never got out')
    def overlays(self, frames):
        """Your timing overlay on the frame you got out and each after it."""
        shown = [self.info(34)]
        for _ in range(frames):
            self.step()
            shown.append(self.info(34))
        return shown

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

    def test_the_pull_isn_t_counted_and_the_timer_starts_after_it(self):
        self.grab()
        self.step()
        self.assertEqual((self.info(8), self.info(13), self.info(16)), (CAPTURE_PULLED, 0, 750))
        self.step()  # Popo's pull ends after your animation: no tick yet.
        self.assertEqual((self.info(8), self.info(13), self.info(16)), (CAPTURE_WAIT, 0, 750))
        self.step()
        self.assertEqual((self.info(13), self.info(14), self.info(16)), (1, 0, 740))
        self.assertEqual(self.shown()[0], '1f')

    def test_each_frame_s_mash_inputs_count_from_the_timer_s_drop(self):
        self.grab()
        self.pull()
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
    def help_text(self):
        rows = {}
        for row in range(self.info(31)):
            lines = [self.m.call('TestICHelpLine', row, line) for line in range(4)]
            rows[self.string(self.m.call('TestICHelpName', row))] = [self.string(at) for at in lines if at]
        return rows

    def test_help_is_its_own_menu(self):
        names = [self.string(self.m.call('TestICMainName', i)) for i in range(self.info(33))]
        self.assertEqual(names, ['Your Percent', 'Handoff', 'Actionable Flash', 'Session Bests', 'Help', 'Exit'])
        self.assertEqual(self.info(32), 1)

    def test_help_covers_what_counts_and_what_doesnt(self):
        rows = self.help_text()
        self.assertEqual(list(rows), [
            'About This Event', 'How Escaping Works', 'Buttons That Count', 'One Button a Frame',
            'L, R and Z', 'The Control Stick', "Neutral Doesn't Help", 'Stick Patterns', 'Use Both at Once',
            'Start Right Away', "What Doesn't Count", 'Ground or Air Release', 'What Pops You Up',
            'Choosing Your Release', 'Waggling Up and Down', 'Actionable Flash', 'Pummels', 'Reading the HUD',
            'Handoffs', 'DI on a Handoff', "Nana's Grab", "Mash in Nana's Grab", 'Back to Popo', 'At the Ledge',
            'Down Air Regrab', 'Forward Air Regrab', 'Back Throw Regrab', 'Popo Alone', 'Wobbling'])
        for name, lines in rows.items():
            self.assertEqual(len(lines), 4, name)
            for line in lines:
                self.assertLessEqual(len(line), 47, (name, line))  # Within the widest event menu text.
        counts, misses = ' '.join(rows['Buttons That Count']), ' '.join(rows["What Doesn't Count"])
        self.assertTrue(counts.startswith('A, B, X, Y, Z, L and R.'), counts)
        for missing in ('C-stick', 'D-pad', 'Start'):
            self.assertIn(missing, misses)
        # What decides the release, as CaptureWait does when it lets go (800DB974).
        air = ' '.join(rows['What Pops You Up'])
        for rule in ('X or Y in your first 16 frames', 'two-thirds up as you get out', 'ground release'):
            self.assertIn(rule, air)
        self.assertNotIn('pulled in counts', ' '.join(rows['Start Right Away']))

    def test_the_flash_row_is_a_toggle_described_in_four_lines(self):
        self.assertEqual(self.info(39), 1)
        lines = [self.m.call('TestICMainLine', FLASH_ROW, line) for line in range(4)]
        self.assertTrue(all(lines))
        self.assertIn('This setting is saved.', self.string(lines[3]))

    # The Actionable Flash.
    def test_the_flash_is_on_by_default_and_saves(self):
        self.assertEqual((self.info(35), self.info(36), self.info(37)), (1, 1, 0))
        self.choose(FLASH_ROW, 0)
        self.assertEqual((self.info(36), self.info(37)), (0, 1))
        self.m.call('TestICFlashPreference', 1)
        self.m.call('TestICLoadSettings')
        self.assertEqual(self.info(35), 1)

    def test_a_ground_release_flashes_yellow_then_green_as_you_can_act(self):
        self.grab()
        self.escape(2)
        self.assertEqual(self.info(8), CAPTURE_CUT)
        shown = self.overlays(len(FLASH) - 1)
        self.assertEqual(shown, FLASH)
        self.assertEqual((self.info(8), self.info(38)), (WAIT, 0))  # You can act; it's over.
        self.assertEqual(self.overlays(RESULT_FRAMES - len(FLASH) - 1), [0] * (RESULT_FRAMES - len(FLASH)))

    def test_an_air_release_flashes_green_from_its_30th_frame(self):
        self.m.call('TestICAirRelease', 1)
        self.grab()
        self.escape(2)
        self.assertEqual(self.info(8), CAPTURE_JUMP)
        self.assertEqual(self.overlays(len(FLASH) - 1), FLASH)
        self.assertEqual((self.info(8), self.info(40), self.info(38)), (CAPTURE_JUMP, (len(FLASH) - 1) * 10, 0))
        self.step(50 - (len(FLASH) - 1))  # Falling at its end, frame 50, doesn't flash again.
        self.assertEqual((self.info(8), self.info(34)), (FALL, 0))

    def test_jumping_on_your_first_frame_still_flashes_green(self):
        self.m.call('TestICAirRelease', 1)
        self.grab()
        self.escape(2)
        self.overlays(RELEASE_FRAMES - 1)
        self.assertEqual(self.info(34), YELLOW)
        self.m.call('TestICAct', JUMP_AERIAL_F)
        self.assertEqual(self.overlays(3)[1:], [GREEN, GREEN, 0])
        self.assertEqual(self.info(38), 0)

    def test_acting_into_an_untimed_state_ends_the_flash(self):
        for release, action in ((1, ESCAPE_AIR), (0, KNEE_BEND)):
            with self.subTest(release=release):
                self.m.call('TestICAirRelease', release)
                self.dpad(DPAD_LEFT)
                self.grab()
                self.escape(2)
                self.overlays(RELEASE_FRAMES - 1)
                self.m.call('TestICAct', action)
                self.step()
                self.assertEqual((self.info(34), self.info(38)), (0, 0))

    def test_off_the_flash_shows_nothing(self):
        self.choose(FLASH_ROW, 0)
        self.grab()
        self.escape(2)
        self.assertEqual(self.overlays(len(FLASH) - 1), [0] * len(FLASH))

    def test_only_a_mash_out_flashes(self):
        shown = []
        for _ in range(1 + GRAB_FRAME + PULL_FRAMES + 5):
            self.step()
            shown.append(self.info(34))  # The setup's Wait, the grab and the pull, then mashing.
        self.assertEqual((shown, self.info(38), self.phase()), ([0] * len(shown), 0, GRAB))

    def test_a_restart_clears_the_flash(self):
        self.grab()
        self.escape(2)
        self.overlays(RELEASE_FRAMES - 2)
        self.assertEqual(self.info(34), YELLOW)
        self.dpad(DPAD_LEFT)
        self.assertEqual((self.info(34), self.info(38), self.phase()), (0, 0, SETUP))


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

    # The handoffs.
    def handoff(self, choice=DOWN_THROW):
        """Pick a handoff and restart into it (HSD_Randi gives 0: Popo on your left)."""
        self.choose(HANDOFF_ROW, choice)
        self.dpad(DPAD_LEFT)

    def until(self, check, frames=200):
        for _ in range(frames):
            if check(): return
            self.step()
        self.fail('never happened')

    def down_throw(self):
        """Popo grabs you and down throws on his first CatchWait frame."""
        self.grab()
        self.step(PULL_FRAMES)
        self.assertEqual((self.info(9), self.info(8), self.phase()), (THROW_LW, THROWN_LW, THROWN))

    def regrabbed(self):
        """Run until Nana has you: her grab connects on your release frame."""
        self.until(lambda: self.info(49) >= 0 and self.phase() == GRAB)
        self.assertEqual((self.info(41), self.info(8)), (CATCH_PULL, CAPTURE_PULLED))

    def test_the_handoff_row_offers_none_and_down_throw_and_saves(self):
        values = [self.string(self.m.call('TestICHandoffValue', v)) for v in range(self.info(27))]
        self.assertEqual(values, ['None', 'Down Throw'])
        self.assertEqual((self.info(26), self.info(43), self.info(42)), (0, NONE, NONE))  # Enabled; None by default.
        self.choose(HANDOFF_ROW, DOWN_THROW)
        self.assertEqual((self.info(42), self.info(20)), (DOWN_THROW, 0))

    def hint(self, choice): return [self.string(self.m.call('TestICHintLine', choice, line)) for line in range(4)]
    def row_desc(self, row): return [self.string(self.m.call('TestICMainLine', row, line)) for line in range(4)]

    def test_the_handoff_row_shows_the_chosen_handoffs_hint(self):
        for choice in (DOWN_THROW, NONE):
            self.choose(HANDOFF_ROW, choice)
            self.assertEqual(self.row_desc(HANDOFF_ROW), self.hint(choice))
            for line in self.hint(choice):
                self.assertTrue(0 < len(line) <= 47, line)  # Within the widest event menu text.
        down = ' '.join(self.hint(DOWN_THROW))
        for advice in ("DI can't save you", "Mash in Nana's grab", 'pummels first half the time', 'down throw'):
            self.assertIn(advice, down)

    def test_a_saved_handoff_loads_with_its_hint(self):
        self.m.call('TestICHandoffPreference', DOWN_THROW)
        self.m.call('TestICLoadSettings')
        self.assertEqual((self.info(43), self.row_desc(HANDOFF_ROW)), (DOWN_THROW, self.hint(DOWN_THROW)))
        self.m.call('TestICHandoffPreference', NONE)
        self.m.call('TestICLoadSettings')
        self.assertEqual(self.row_desc(HANDOFF_ROW), self.hint(NONE))

    def test_on_a_handoff_nana_stands_on_top_of_popo(self):
        self.handoff()
        self.assertEqual((self.info(2), self.info(3), self.info(4)), (0, -80, -80))
        self.assertEqual((self.info(6), self.info(7)), (1, 1))  # Both face you.

    def test_with_no_handoff_popo_never_throws(self):
        self.grab()
        self.step(PULL_FRAMES + 30)
        self.assertEqual((self.info(9), self.info(8), self.phase()), (CATCH_WAIT, CAPTURE_WAIT, GRAB))

    def test_popo_down_throws_on_his_first_catchwait_frame(self):
        self.handoff()
        self.grab()
        self.step(PULL_FRAMES - 1)
        self.assertEqual(self.info(9), CATCH_PULL)
        self.step()
        self.assertEqual((self.info(9), self.info(8), self.phase(), self.info(53)), (THROW_LW, THROWN_LW, THROWN, -127))
        self.assertEqual((self.info(13), self.info(14)), (0, 0))  # No mash: he threw before your first tick.

    def test_a_throw_push_that_doesnt_take_is_pushed_again_from_neutral(self):
        self.handoff()
        self.m.call('TestICIgnoreThrows', 1)
        self.grab()
        self.step(PULL_FRAMES)  # His first push doesn't take,
        self.assertEqual((self.info(9), self.info(53)), (CATCH_WAIT, -127))
        for _ in range(2):  # and the stick has to read neutral before the next one.
            self.step()
            self.assertEqual((self.info(9), self.info(53)), (CATCH_WAIT, 0))
        self.step()
        self.assertEqual((self.info(9), self.info(8), self.phase()), (THROW_LW, THROWN_LW, THROWN))

    def test_nana_regrabs_you_on_your_release_frame_for_every_weight(self):
        # Released ceil(36 x weight / 100) frames into the throw, as in Nicki's data sheet.
        for weight, frames in ((55, 20), (75, 27), (100, 36), (117, 43)):  # Pichu, Fox, Mario, Bowser.
            with self.subTest(weight=weight):
                self.m.call('TestICWeight', weight)
                self.handoff()
                self.down_throw()
                started = self.info(50)
                self.regrabbed()
                released = self.info(49)
                self.assertEqual(released - started, frames)
                self.assertEqual(self.info(47), released - 6)  # Her Z, 6 frames before: her grab is out then.
                self.assertEqual((self.info(44), self.info(23)), (1, 6))  # One regrab; the throw's 6%.

    def test_a_regrab_that_cant_reach_you_misses(self):
        self.m.call('TestICRegrabWindow', 0)
        self.handoff()
        self.down_throw()
        self.until(lambda: self.info(49) >= 0)  # Released.
        released = self.info(50)
        self.until(lambda: self.phase() == RESULT)
        self.assertEqual(self.info(50) - released, 2)  # Her grab's last frame passed without you.
        self.assertEqual((self.info(45), self.info(44)), (OUT_MISSED, 0))
        self.assertEqual(self.labels(), ['Regrabs', 'Got Out', 'Held For', 'Mash Speed'])
        self.assertEqual(self.shown()[:2], ['0', 'Regrab Miss'])

    def test_nana_pummels_with_her_ai_s_chance_then_throws(self):
        self.handoff()
        # Her first roll is out of 2 (0 pummels), her second out of 4 (1 pummel so far); then the throw's.
        self.m.call('TestICRandoms', 0, 1, 2, 0)
        self.down_throw()
        self.regrabbed()
        self.step(PULL_FRAMES)
        self.assertEqual((self.info(41), self.info(51) & 0x100, self.info(54)), (CATCH_ATTACK, 0x100, 1))
        self.until(lambda: self.info(41) == CATCH_WAIT)  # She decides as she's back: 1 out of 4, no pummel.
        self.assertEqual((self.info(54), self.info(52)), (1, 0))  # The stick stays neutral this frame,
        self.step()
        self.assertEqual((self.info(41), self.info(8), self.phase()), (THROW_F, THROWN_F, THROWN))  # then throws.

    def test_nanas_throws_are_her_ai_s_one_in_four(self):
        for roll, throw, thrown, text in ((0, THROW_HI, THROWN_HI, 'Up Throw'), (2, THROW_F, THROWN_F, 'Fwd Throw'),
                                          (3, THROW_B, THROWN_B, 'Back Throw')):
            with self.subTest(text=text):
                self.handoff()
                self.m.call('TestICRandoms', 1, roll, 0, 0)  # No pummel, then the throw.
                self.down_throw()
                self.regrabbed()
                self.step(PULL_FRAMES + 1)
                self.assertEqual((self.info(41), self.info(8)), (throw, thrown))
                self.until(lambda: self.phase() == RESULT)
                self.assertEqual((self.info(45), self.info(46), self.info(44)), (OUT_THROWN, throw, 1))
                self.assertEqual(self.shown()[:2], ['1', text])

    def test_near_a_ledge_nana_throws_toward_it(self):
        # Popo and Nana are 8 left of you; Nana faces right. Within 30 of an edge
        # her AI throws toward it, rolls or not: forward to the right, back to the left.
        for shift, throw in ((700, THROW_F), (-700, THROW_B)):
            with self.subTest(shift=shift):
                self.handoff()
                self.m.call('TestICRandoms', 1, 0, 0, 0)  # No pummel; the roll would be up.
                self.down_throw()
                self.regrabbed()
                self.m.call('TestICShift', shift)
                self.step(PULL_FRAMES + 1)
                self.assertEqual(self.info(41), throw)
        self.handoff()
        self.m.call('TestICRandoms', 1, 0, 0, 0)
        self.down_throw()
        self.regrabbed()
        self.m.call('TestICShift', 600)  # Nana 52 right: 33.6 from the edge, too far.
        self.step(PULL_FRAMES + 1)
        self.assertEqual(self.info(41), THROW_HI)

    def test_nanas_down_throw_goes_back_to_popo_and_on(self):
        self.handoff()
        self.m.call('TestICRandoms', 1, 1, 1, 1)  # Nana down throws without a pummel, each time.
        self.down_throw()
        self.regrabbed()
        self.step(PULL_FRAMES + 1)
        self.assertEqual((self.info(41), self.info(8), self.phase()), (THROW_LW, THROWN_LW, THROWN))
        self.until(lambda: self.info(44) == 2)  # Popo, on top of her, regrabs it the same way.
        self.assertEqual((self.info(48), self.info(9), self.info(23)), (self.info(49) - 6, CATCH_PULL, 12))
        self.step(PULL_FRAMES)
        self.assertEqual((self.info(9), self.info(8)), (THROW_LW, THROWN_LW))  # And down throws again.
        self.until(lambda: self.info(44) == 3)
        self.assertEqual(self.info(41), CATCH_PULL)

    def test_you_can_mash_out_of_nanas_grab_during_a_pummel(self):
        self.handoff()
        self.m.call('TestICRandoms', 0, 0, 0, 0)  # She pummels.
        self.down_throw()
        self.regrabbed()
        self.escape(2)
        self.assertEqual((self.info(45), self.info(44), self.info(8)), (OUT_MASH, 1, CAPTURE_CUT))
        self.assertEqual(self.shown()[:2], ['1', 'Mashed Out'])
        self.assertEqual((self.big(), self.info(24)), ('Mash 14', GREEN_BIG))  # 84.6 at 6%: 7 frames of 13.
        self.assertEqual((self.info(17), self.info(38)), (0, 1))  # Not a session best; the flash is armed.

    def test_a_handoff_isnt_counted_in_session_bests(self):
        self.handoff()
        self.m.call('TestICRandoms', 0, 0, 0, 0)
        self.down_throw()
        self.regrabbed()
        self.escape(2)
        self.assertEqual(self.text(11), '-')
        self.assertNotEqual(self.info(19), NEW_BEST_SFX)

    def test_a_restart_mid_handoff_starts_over(self):
        self.handoff()
        self.down_throw()
        self.regrabbed()
        loads = self.info(0)
        self.dpad(DPAD_RIGHT)
        self.assertEqual((self.info(0), self.phase(), self.info(44), self.info(45)), (loads + 1, SETUP, 0, 0))
        self.assertEqual((self.info(3), self.info(4)), (-80, -80))

    def test_your_percent_row_saves(self):
        self.choose(PERCENT_ROW, 13)
        self.assertEqual((self.info(20), self.info(42), self.info(21)), (13, NONE, 2))  # Both saved rows.


if __name__ == '__main__':
    unittest.main(verbosity=2)
