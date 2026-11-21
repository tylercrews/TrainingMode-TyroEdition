"""Execute the production Lab settings carry across a stage change and Reset to Default Settings."""
import re
import struct
import subprocess
import unittest
from pathlib import Path
from test_menu import Machine, ROOT, reg

# Probe tables (Option() in lab_session_probe.c) and lab.h row indices.
GENERAL, CPU, TECH, INFO, TRAILS, RNG, RECOVERY, ADV, LOG, STADIUM, CONTROLS, RECORD, STORY = range(13)
GEN_PCNT, GEN_LOCK, GEN_MODEL, GEN_HIT, GEN_CAM, GEN_HUD, GEN_INPUT, GEN_SPEED = 1, 2, 3, 4, 7, 10, 12, 13
GEN_TIMER_LOOP = 19
STORY_RANDALL_ON = 0
MATCH_TIMER_COUNTDOWN, MATCH_TIMER_COUNTUP = 2, 3
RANDALL_GROUP = 2  # Its index in the game's collision groups, in the probe.
# TestLabRandall keys.
HIDDEN, PUFF_TIMER, COLL_CALLS, LAST_COLL, REQUESTS, REQUESTED, DIGIT_SETS, SECONDS, CENTIS, ANIMS, RANDALL_ON = range(11)
LOOP = 8 * 60 * 60
CPU_PCNT, CPU_TDI, CPU_BEHAVE, CPU_INTANG = 0, 3, 8, 17
TECH_TECH, TECH_AWAY_CHANCE, TECH_CHANCES = 0, 8, (7, 8, 9, 10)
INFO_PRESET, INFO_ROW2 = 0, 3
ADV_COUNT, ADV_HITNUM, ADV_LOGIC, ADV_GROUND = 8, 0, 1, 2
LOG_COUNT, LOG_ACTION = 8, 1
REC_LOOP = 7
INTANG_COLANIM = 10
BATTLEFIELD = 31
NAMES = 0x80480000
# External CKIND_* IDs, and the game's costume counts by external ID (0x803d51a0).
FOX, MARTH, PUFF, FALCO = 2, 9, 15, 20
COSTUMES = [6, 5, 4, 4, 6, 4, 5, 4, 5, 5, 4, 4, 5, 4, 4, 5, 5, 6, 5, 5, 4, 5, 5, 5, 4, 5]
BUTTON_A, BUTTON_X, BUTTON_Y = 0x100, 0x400, 0x800


def build():
    compiler = Path('C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe')
    exports = re.findall(r'\b(TestLab\w+)\(', (ROOT / 'tests/lab_session_probe.c').read_text())
    subprocess.run([str(compiler) if compiler.exists() else 'powerpc-eabi-gcc',
        '-O2', '-w', '-ffreestanding', '-fno-builtin', '-msdata=none', '-mcpu=750', '-nostdlib',
        '-ffunction-sections', '-fdata-sections',
        '-Wl,-Ttext=0x101000,-e,TestLabBoot,--gc-sections,--unresolved-symbols=ignore-all',
        *['-Wl,--undefined=' + name for name in exports],
        'tests/lab_session_probe.c', '-o', 'build/lab-session-test.elf'], cwd=ROOT, check=True)


class Lab:
    def __init__(self):
        self.m = Machine('lab-session-test.elf', 'TestLabBoot')
        self.m.cpu.reg_write(reg.UC_PPC_REG_MSR, 1 << 13)  # percents and TDI are floats
        self.next_name = NAMES

    def call(self, name, *args): return self.m.call(name, *args)
    def init(self): return self.call('TestLabInit')
    def choose(self, table, index, value): self.call('TestLabChoose', table, index, value)
    def value(self, table, index): return self.call('TestLabValue', table, index)
    def disabled(self, table, index): return self.call('TestLabDisabled', table, index)
    def info(self, key): return self.call('TestLabInfo', key)
    def text(self, address): return bytes(self.m.cpu.mem_read(address & 0xFFFFFFFF, 64)).split(b'\0', 1)[0]
    def name(self, text):
        address = self.next_name
        self.m.cpu.mem_write(address, text + b'\0')
        self.next_name += 64
        return address

    def fighter(self, slot):
        base = 33 + slot * 2
        return (self.info(base), self.info(base + 1))

    def carry_into(self, new):
        for symbol in ('carry_tag', 'carry_size', 'carry_data'):
            address, size = self.m.symbols[symbol], (2048 if symbol == 'carry_data' else 4)
            new.m.cpu.mem_write(address, bytes(self.m.cpu.mem_read(address, size)))

    def customize(self):
        """Change one or more rows in every carried table, through each row's menu callback."""
        self.choose(GENERAL, GEN_PCNT, 40); self.choose(GENERAL, GEN_LOCK, 1)
        self.choose(GENERAL, GEN_MODEL, 1); self.choose(GENERAL, GEN_HIT, 1)
        self.choose(GENERAL, GEN_CAM, 2); self.choose(GENERAL, GEN_HUD, 0)
        self.choose(GENERAL, GEN_SPEED, 2); self.choose(GENERAL, GEN_INPUT, 2)
        self.choose(CPU, CPU_PCNT, 75); self.choose(CPU, CPU_INTANG, 1)
        self.choose(CPU, CPU_BEHAVE, 1); self.choose(CPU, CPU_TDI, 4)
        self.choose(TECH, TECH_AWAY_CHANCE, 55)  # rebalances the other three chances
        self.choose(TECH, TECH_TECH, 2)
        self.choose(INFO, INFO_PRESET, 3); self.choose(INFO, INFO_ROW2, 1)
        self.choose(TRAILS, 0, 1); self.choose(TRAILS, 1, 2)
        self.choose(RNG, 0, 6)
        self.choose(RECOVERY, 0, 0)
        self.choose(ADV, ADV_HITNUM, 4)  # copies to every page and shows page 4
        self.choose(ADV, 3 * ADV_COUNT + ADV_LOGIC, 2); self.choose(ADV, 3 * ADV_COUNT + ADV_GROUND, 5)
        self.choose(LOG, 2 * LOG_COUNT + LOG_ACTION, 7)
        self.call('TestLabLogState', 2, 44, self.name(b'Attack11'))
        self.choose(STADIUM, 0, 1)
        self.choose(CONTROLS, 0, 2); self.choose(RECORD, REC_LOOP, 1)
        self.call('TestLabCustomOSD', 14, self.name(b'Wait'))
        self.call('TestLabCustomOSD', 42, self.name(b'Landing'))
        self.call('TestLabTDI', 3)
        self.chances = [self.value(TECH, i) for i in TECH_CHANCES]

    def assert_carried(self, test, peach_item=6):
        test.assertEqual([self.value(GENERAL, i) for i in (GEN_PCNT, GEN_LOCK, GEN_MODEL, GEN_HIT, GEN_CAM, GEN_HUD, GEN_SPEED)],
                         [40, 1, 1, 1, 2, 0, 2])
        test.assertEqual([self.value(CPU, i) for i in (CPU_PCNT, CPU_INTANG, CPU_BEHAVE, CPU_TDI)], [75, 1, 1, 4])
        # Effects follow the values: camera, HUD, model/collision displays, percents, intangibility, RNG, trails.
        # Stage Only hides both fighters but not the stage.
        test.assertEqual([self.info(k) for k in (0, 1, 2, 25, 26, 27)], [3, 1, 1, 3, 0, 0])
        test.assertEqual([self.info(k) for k in (3, 4, 5, 6, 7, 8, 10, 11)], [40, 75, 40, 75, INTANG_COLANIM, peach_item, 1, 2])
        # Lock Player Percent captured the carried percent; the CPU lock stayed off.
        test.assertEqual((self.info(28), self.info(29)), (40, 0))


class LabSessionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def test_session_lists_only_unsaved_lab_rows(self):
        lab = Lab(); lab.init()
        # General 13 (no Input Display; Timer 8m Inf Loop is one), CPU 18, Tech 16, Info 2x10,
        # Trails 2, Peach RNG 2, Fox recovery 12, Advanced Counter 10x8, Action Log 10x7.
        # Reset adds Stadium's row.
        self.assertEqual((lab.info(20), lab.info(21)), (233, 234))
        # On Yoshi's Story, Reset adds only the Lab's own Randall row: Randall
        # Overlay and Randall Timer are saved Global settings.
        lab.call('TestLabRandallInit', 1)
        self.assertEqual((lab.info(20), lab.info(21)), (233, 234))
        self.assertEqual(lab.call('TestLabRandall', 11), GEN_TIMER_LOOP)

    def test_timer_loop_is_an_unsaved_session_row_that_starts_off(self):
        old = Lab(); old.init()
        self.assertEqual(old.value(GENERAL, GEN_TIMER_LOOP), 0)
        old.choose(GENERAL, GEN_TIMER_LOOP, 1)
        old.call('TestLabStageSwap', 1)
        new = Lab(); old.carry_into(new)
        self.assertEqual(new.init(), 1)
        new.call('TestLabFirstFrame')
        self.assertEqual(new.value(GENERAL, GEN_TIMER_LOOP), 1)  # Kept across a stage change...
        new.call('TestLabReset')
        self.assertEqual(new.value(GENERAL, GEN_TIMER_LOOP), 0)  # ...and back Off on Reset.

    def test_reset_restores_every_default_and_its_effect_in_place(self):
        lab = Lab(); lab.init()
        lab.customize()
        lab.call('TestLabHit', 0, 80)
        lab.call('TestLabReset')
        self.assertEqual([lab.value(GENERAL, i) for i in (GEN_PCNT, GEN_LOCK, GEN_MODEL, GEN_HIT, GEN_CAM, GEN_HUD, GEN_SPEED)],
                         [0, 0, 0, 0, 0, 1, 0])
        self.assertEqual([lab.value(CPU, i) for i in (CPU_PCNT, CPU_INTANG, CPU_BEHAVE, CPU_TDI)], [0, 0, 0, 0])
        self.assertEqual([lab.info(k) for k in (0, 1, 2, 25, 26, 27)], [1, 0, 0, 0, 3, 0])
        self.assertEqual([lab.info(k) for k in (3, 4, 5, 6, 7, 8, 9, 10, 11)], [0, 0, 0, 0, -INTANG_COLANIM, 0, 0, 0, 0])
        # Rebalanced chances return exactly; Tech Option re-enables them.
        self.assertEqual([lab.value(TECH, i) for i in TECH_CHANCES], [25, 25, 25, 25])
        self.assertEqual([lab.disabled(TECH, i) for i in TECH_CHANCES], [0, 0, 0, 0])
        self.assertEqual((lab.value(INFO, INFO_PRESET), lab.value(INFO, INFO_ROW2)), (0, 0))
        self.assertEqual((lab.value(RECOVERY, 0), lab.value(STADIUM, 0)), (1, 0))
        self.assertEqual([lab.value(ADV, page * ADV_COUNT + ADV_HITNUM) for page in range(10)], [1] * 10)
        self.assertEqual((lab.value(ADV, 3 * ADV_COUNT + ADV_LOGIC), lab.disabled(ADV, 3 * ADV_COUNT + ADV_GROUND)), (0, 1))
        self.assertEqual((lab.value(LOG, 2 * LOG_COUNT + LOG_ACTION), lab.info(17), lab.info(31)), (2, 0, 0))
        self.assertEqual([lab.info(k) for k in (12, 15, 18, 19, 24)], [0, 0, 0, 0, 2])
        # Saved preferences and the recording setup are not Lab session settings.
        self.assertEqual((lab.value(GENERAL, GEN_INPUT), lab.value(CONTROLS, 0), lab.value(RECORD, REC_LOOP)), (2, 2, 1))

    def test_stage_change_carries_settings_into_the_reloaded_lab(self):
        old = Lab(); self.assertEqual(old.init(), 0)
        old.customize()
        old.call('TestLabOnload', 3)
        # Stadium is current: its row does nothing.
        self.assertEqual(old.info(32), 1)
        old.call('TestLabStageSwap', 5)
        self.assertEqual((old.info(22), old.info(30), old.info(23)), (-1, 0, 3))
        old.call('TestLabStageSwap', 1)
        self.assertEqual((old.info(22), old.info(30)), (BATTLEFIELD, 1))
        # The imported recording is not reloaded, and Character RNG is cleared as on Exit.
        self.assertEqual((old.info(23), old.info(8)), (-1, 0))

        # A fresh Lab .dat; only the boot-loaded carry buffer survives.
        new = Lab()
        for symbol in ('carry_tag', 'carry_size', 'carry_data'):
            address, size = old.m.symbols[symbol], (2048 if symbol == 'carry_data' else 4)
            new.m.cpu.mem_write(address, bytes(old.m.cpu.mem_read(address, size)))
        self.assertEqual(new.init(), 1)
        self.assertEqual(new.value(GENERAL, GEN_CAM), 0)  # applied on the first frame
        new.call('TestLabFirstFrame')
        new.assert_carried(self)
        self.assertEqual([new.value(TECH, i) for i in TECH_CHANCES], old.chances)
        self.assertEqual([new.disabled(TECH, i) for i in TECH_CHANCES], [1, 1, 1, 1])
        # Rows are kept even though the preset's callback would rewrite them.
        self.assertEqual((new.value(INFO, INFO_PRESET), new.value(INFO, INFO_ROW2)), (3, 1))
        self.assertEqual(new.value(RECOVERY, 0), 0)
        self.assertEqual((new.info(18), new.value(ADV, 3 * ADV_COUNT + ADV_LOGIC),
                          new.value(ADV, 3 * ADV_COUNT + ADV_GROUND), new.disabled(ADV, 3 * ADV_COUNT + ADV_GROUND)),
                         (3, 2, 5, 0))
        self.assertEqual((new.value(LOG, 2 * LOG_COUNT + LOG_ACTION), new.info(17), new.info(31)), (7, 44, 1))
        self.assertEqual(new.text(new.call('TestLabLogName', 2)), b'Attack11')
        self.assertEqual([new.info(k) for k in (12, 13, 14)], [2, 14, 42])
        self.assertEqual([new.text(new.call('TestLabCustomOSDName', i)) for i in range(2)],
                         [b'Remove OSD: Wait', b'Remove OSD: Landing'])
        self.assertEqual((new.info(15), new.info(16)), (3, 25))
        # Stage-specific, saved and recording rows are not carried.
        self.assertEqual((new.value(STADIUM, 0), new.info(9)), (0, -1))
        self.assertEqual((new.value(GENERAL, GEN_INPUT), new.value(RECORD, REC_LOOP)), (0, 0))
        # Reset afterwards returns to the reloaded Lab's own defaults.
        new.call('TestLabReset')
        self.assertEqual([new.value(GENERAL, i) for i in (GEN_PCNT, GEN_CAM, GEN_HUD)], [0, 0, 1])

    def test_carry_is_taken_once(self):
        old = Lab(); old.init()
        old.call('TestLabStageSwap', 0)
        new = Lab()
        for symbol in ('carry_tag', 'carry_size', 'carry_data'):
            address, size = old.m.symbols[symbol], (2048 if symbol == 'carry_data' else 4)
            new.m.cpu.mem_write(address, bytes(old.m.cpu.mem_read(address, size)))
        self.assertEqual((new.init(), new.init()), (1, 0))


def game_countdown(frames, start=8 * 60):
    """A VS match's timer after this many frames: time_ms counts up from 59, and
    each time it wraps, seconds go down; it shows 99 - ms * 99 / 59 (8016CD98, 8016AF0C)."""
    seconds, ms = start, 59
    for _ in range(frames):
        ms += 1
        if ms == 60:
            ms = 0
            if seconds:
                seconds -= 1
    return seconds, 99 - int(ms * 99.0 / 59.0)


class TimerLoopTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def setUp(self):
        self.lab = Lab(); self.lab.init()

    def digits(self, frames):
        value = self.lab.call('TestLabTimerDigits', frames)
        return value // 100, value % 100

    def test_it_counts_down_from_8_minutes_as_a_vs_match_timer_shows_it(self):
        self.assertEqual(self.digits(0), (480, 0))  # 8:00.00
        self.assertEqual(self.digits(1), (479, 99))
        self.assertEqual(self.digits(60), (479, 0))
        self.assertEqual(self.digits(582), (470, 31))  # 7:50.31, as a VS match shows it 582 frames in.
        self.assertEqual(self.digits(LOOP - 1), (0, 2))
        self.assertEqual(game_countdown(582), (470, 31))
        # Every frame of the 8 minutes, as the game counts them down.
        seconds, ms = 480, 59
        for frames in range(LOOP + 1):
            with self.subTest(frames=frames):
                self.assertEqual(self.digits(frames), (seconds, 99 - int(ms * 99.0 / 59.0)))
            ms += 1
            if ms == 60:
                ms = 0
                seconds = max(0, seconds - 1)

    def test_it_reaches_zero_then_starts_again_from_8_minutes(self):
        self.assertEqual(self.digits(LOOP), (0, 0))  # 0:00.00...
        self.assertEqual(self.digits(LOOP + 1), (479, 99))  # ...then the loop starts again.
        self.assertEqual(self.digits(2 * LOOP), (0, 0))
        for frames in (1, 582, 12345, LOOP - 1):
            self.assertEqual(self.digits(frames + 3 * LOOP), self.digits(frames))
        self.assertEqual(LOOP % 1200, 0)  # 24 of Randall's laps.

    def test_it_redraws_the_timer_only_while_on_and_counting_up(self):
        lab = self.lab
        lab.call('TestLabTimer', MATCH_TIMER_COUNTUP, 9, 41)  # 0:09.69 counting up, 581 frames.
        lab.call('TestLabTimerLoop', 1)
        self.assertEqual(lab.call('TestLabRandall', DIGIT_SETS), 0)  # Off: the game's own timer.
        lab.choose(GENERAL, GEN_TIMER_LOOP, 1)
        lab.call('TestLabTimerLoop', 1)
        self.assertEqual([lab.call('TestLabRandall', k) for k in (DIGIT_SETS, SECONDS, CENTIS, ANIMS)],
                         [1, 470, 32, 1])  # The same moment: 7:50.32, applied to the digits.
        lab.call('TestLabTimer', MATCH_TIMER_COUNTUP, 10, 0)
        lab.call('TestLabTimerLoop', 1)
        self.assertEqual((lab.call('TestLabRandall', SECONDS), lab.call('TestLabRandall', CENTIS)), (470, 0))
        lab.call('TestLabTimerLoop', 0)  # No timer this match.
        self.assertEqual(lab.call('TestLabRandall', DIGIT_SETS), 0)
        lab.call('TestLabTimer', MATCH_TIMER_COUNTDOWN, 10, 0)  # Not the Lab's own count-up timer.
        lab.call('TestLabTimerLoop', 1)
        self.assertEqual(lab.call('TestLabRandall', DIGIT_SETS), 0)

    def test_timer_frames_read_what_the_timer_shows(self):
        lab = self.lab
        lab.call('TestLabTimer', MATCH_TIMER_COUNTUP, 9, 41)
        self.assertEqual(lab.call('TestLabTimerFrames'), 581)
        # Counting down from 8:00: a VS match 582 frames in shows 7:50.31.
        lab.call('TestLabTimerStart', 480, 0)
        seconds, ms = 480, 59
        for frames in range(0, 2000):
            if frames % 97 == 0 or frames in (1, 59, 60, 61, 582):
                with self.subTest(frames=frames):
                    lab.call('TestLabTimer', MATCH_TIMER_COUNTDOWN, seconds, ms)
                    self.assertEqual(lab.call('TestLabTimerFrames'), frames)
            ms += 1
            if ms == 60:
                ms, seconds = 0, seconds - 1


class RandallTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def setUp(self):
        self.lab = Lab(); self.lab.init()
        self.assertEqual(self.lab.call('TestLabRandallInit', 1), RANDALL_GROUP)

    def r(self, key): return self.lab.call('TestLabRandall', key)

    def test_randall_starts_on(self):
        self.assertEqual(self.lab.value(STORY, STORY_RANDALL_ON), 1)
        self.lab.call('TestLabRandallThink')
        self.assertEqual([self.r(k) for k in (HIDDEN, COLL_CALLS)], [0, 0])

    def test_off_hides_him_and_switches_off_his_platform_and_puffs(self):
        lab = self.lab
        lab.choose(STORY, STORY_RANDALL_ON, 0)  # Right away, for a change while paused.
        self.assertEqual([self.r(k) for k in (HIDDEN, PUFF_TIMER, COLL_CALLS, LAST_COLL)], [1, 0x7FFF, 1, -(RANDALL_GROUP + 1)])
        self.assertEqual(self.r(13), 0)  # Only the cloud: his other joints keep their flags.
        for _ in range(3):
            lab.call('TestLabRandallThink')
        self.assertEqual([self.r(k) for k in (HIDDEN, PUFF_TIMER, COLL_CALLS)], [1, 0x7FFF, 1])  # Once.
        lab.choose(STORY, STORY_RANDALL_ON, 1)
        self.assertEqual([self.r(k) for k in (HIDDEN, PUFF_TIMER, COLL_CALLS, LAST_COLL)], [0, 0, 2, RANDALL_GROUP + 1])

    def test_reset_turns_him_back_on(self):
        lab = self.lab
        lab.choose(STORY, STORY_RANDALL_ON, 0)
        lab.call('TestLabReset')
        self.assertEqual([self.r(k) for k in (RANDALL_ON, HIDDEN, LAST_COLL)], [1, 0, RANDALL_GROUP + 1])

    def test_he_keeps_to_the_timer_as_a_match_keeps_him(self):
        lab, offset = self.lab, self.r(12)
        self.assertEqual(offset, 125)
        lab.call('TestLabTimer', MATCH_TIMER_COUNTUP, 9, 41)  # 581 frames.
        now = (581 + offset) % 1200
        for frame in (now, now - 1, now + 1):  # In step, or a frame off: left alone.
            with self.subTest(frame=frame):
                lab.call('TestLabCloudFrame', frame)
                lab.call('TestLabRandallThink')
                self.assertEqual(self.r(REQUESTS), 0)
        for frame in (now - 2, now + 2, 0, 600):  # Off the timer: on it from the next frame.
            with self.subTest(frame=frame):
                lab.call('TestLabCloudFrame', frame)
                lab.call('TestLabRandallThink')
                self.assertEqual((self.r(REQUESTS), self.r(REQUESTED)), (1, now + 1))
        # Across the end of his loop: 1199 and 0 are a frame apart.
        lab.call('TestLabTimer', MATCH_TIMER_COUNTUP, 0, 1199 - offset)
        for frame in (0, 1198):
            lab.call('TestLabCloudFrame', frame)
            lab.call('TestLabRandallThink')
            self.assertEqual(self.r(REQUESTS), 0)
        lab.call('TestLabCloudFrame', 1)
        lab.call('TestLabRandallThink')
        self.assertEqual(self.r(REQUESTED), 0)  # 1199's next frame.

    def test_switched_off_he_still_keeps_to_the_timer(self):
        lab = self.lab
        lab.choose(STORY, STORY_RANDALL_ON, 0)
        lab.call('TestLabTimer', MATCH_TIMER_COUNTUP, 100, 0)
        lab.call('TestLabCloudFrame', 3)
        lab.call('TestLabRandallThink')
        self.assertEqual(self.r(REQUESTED), (6000 + self.r(12) + 1) % 1200)

    def test_nothing_happens_without_him(self):
        lab = Lab(); lab.init()
        self.assertEqual(lab.call('TestLabRandallInit', 0), -1)
        lab.choose(STORY, STORY_RANDALL_ON, 0)
        lab.call('TestLabCloudFrame', 500)
        lab.call('TestLabRandallThink')
        self.assertEqual([lab.call('TestLabRandall', k) for k in (HIDDEN, COLL_CALLS, REQUESTS)], [0, 0, 0])


class FighterSwapTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def lab(self, cpu=MARTH, cpu_costume=1):
        lab = Lab()
        lab.call('TestLabLive', 0, FOX, 0)
        lab.call('TestLabLive', 1, cpu, cpu_costume)
        lab.init()
        return lab

    def test_selectors_are_the_top_session_rows_in_tier_order(self):
        lab = self.lab()
        self.assertEqual([lab.call('TestLabSessionRow', row) for row in (0, 1)],
                         [lab.call('TestLabFighterOption', slot) for slot in (0, 1)])
        self.assertEqual([lab.call('TestLabFighterIndex', kind) for kind in (FOX, MARTH, PUFF, FALCO)], [0, 1, 2, 3])
        self.assertEqual(sorted(lab.call('TestLabFighterIndex', kind) for kind in range(26)), list(range(26)))
        self.assertEqual([lab.call('TestLabCostumeNum', kind) for kind in range(26)], COSTUMES)

    def test_selectors_start_on_the_fighters_in_play(self):
        lab = self.lab()
        self.assertEqual((lab.info(37), lab.info(38)), (0, 1))
        self.assertEqual((lab.fighter(0), lab.fighter(1)), ((FOX, 0), (MARTH, 1)))
        self.assertEqual(lab.text(lab.call('TestLabFighterStatus', 1, 1)), b'Playing now: Marth, Red.')
        self.assertEqual(lab.text(lab.call('TestLabFighterStatus', 1, 0)), b'Selected: Marth, Red.')

    def test_left_right_picks_the_default_costume_unless_the_other_fighter_has_it(self):
        lab = self.lab()
        lab.call('TestLabPickFighter', 0, 3)
        self.assertEqual(lab.fighter(0), (FALCO, 0))
        lab.call('TestLabPickFighter', 1, 3)  # Falco vs Falco: the CPU moves to the next costume
        self.assertEqual(lab.fighter(1), (FALCO, 1))
        self.assertEqual(lab.text(lab.call('TestLabFighterStatus', 1, 0)), b'Selected: Falco, Red.')
        self.assertEqual(lab.text(lab.call('TestLabFighterStatus', 1, 1)), b'Playing now: Marth, Red.')

    def test_x_and_y_cycle_costumes_and_skip_the_other_fighters(self):
        lab = self.lab(cpu=FOX, cpu_costume=1)
        self.assertEqual(lab.call('TestLabFighterButton', 1, BUTTON_X), 1)
        self.assertEqual(lab.fighter(1), (FOX, 2))
        lab.call('TestLabFighterButton', 1, BUTTON_X)
        self.assertEqual(lab.fighter(1), (FOX, 3))
        lab.call('TestLabFighterButton', 1, BUTTON_X)  # wraps past the player's costume 0
        self.assertEqual(lab.fighter(1), (FOX, 1))
        lab.call('TestLabFighterButton', 1, BUTTON_Y)
        self.assertEqual(lab.fighter(1), (FOX, 3))
        self.assertEqual(lab.text(lab.call('TestLabCostumeName', FOX, 3)), b'Green')
        self.assertEqual(lab.text(lab.call('TestLabCostumeName', FOX, 0)), b'Default')
        # Other buttons are left to the menu.
        self.assertEqual(lab.call('TestLabFighterButton', 1, 0x10), 0)

    def test_style_shows_the_costume_color_and_stock_icon(self):
        lab = self.lab()
        self.assertEqual(lab.call('TestLabFighterStyle', 1), 1)
        self.assertEqual((lab.info(44), lab.info(45), lab.info(46)), (MARTH, 1, 0xFF5A5A))
        unchanged = lab.text(lab.info(47))
        lab.call('TestLabFighterButton', 1, BUTTON_X)  # Marth Green
        lab.call('TestLabFighterStyle', 1)
        self.assertEqual((lab.info(45), lab.info(46)), (2, 0x5ADC5A))
        self.assertNotIn(b'A: Restart', unchanged)
        self.assertIn(b'A: Restart', lab.text(lab.info(47)))

    def test_a_restarts_only_when_a_pick_changed_keeping_the_unchanged_fighter(self):
        lab = self.lab()
        lab.call('TestLabFighterButton', 0, BUTTON_A)
        self.assertEqual((lab.info(39), lab.info(30), lab.info(43)), (-9, 0, 4))
        lab.call('TestLabOnload', 3)
        lab.call('TestLabPickFighter', 1, 2)
        lab.call('TestLabFighterButton', 0, BUTTON_A)
        self.assertEqual([lab.info(k) for k in (39, 41, 42)], [-1, PUFF, 0])
        # Settings are carried and the CSS recording import is dropped, as for a stage change.
        self.assertEqual((lab.info(30), lab.info(23), lab.info(43)), (1, -1, 41))

    def test_character_change_carries_only_rows_that_belong_to_no_fighter(self):
        fresh = Lab(); fresh.init()
        defaults = (fresh.value(RECOVERY, 0), fresh.value(RNG, 0))
        old = self.lab()
        old.call('TestLabKinds', 1, 18)
        old.customize()
        old.call('TestLabPickFighter', 1, 2)
        old.call('TestLabFighterButton', 1, BUTTON_A)
        self.assertEqual(old.info(30), 1)
        # A new CPU: its recovery rows and the Character RNG rows start fresh.
        new = Lab(); old.carry_into(new)
        new.call('TestLabKinds', 1, 15)
        self.assertEqual(new.init(), 1)
        new.call('TestLabFirstFrame')
        new.assert_carried(self, peach_item=0)
        self.assertEqual((new.value(RECOVERY, 0), new.value(RNG, 0)), defaults)
        # Same CPU, new player: the CPU's recovery rows carry, Character RNG doesn't.
        same_cpu = Lab(); old.carry_into(same_cpu)
        same_cpu.call('TestLabKinds', 22, 18)
        same_cpu.init(); same_cpu.call('TestLabFirstFrame')
        self.assertEqual((same_cpu.value(RECOVERY, 0), same_cpu.value(RNG, 0)), (0, defaults[1]))
        # Same fighters (a stage change): everything carries.
        same = Lab(); old.carry_into(same)
        same.call('TestLabKinds', 1, 18)
        same.init(); same.call('TestLabFirstFrame')
        self.assertEqual((same.value(RECOVERY, 0), same.value(RNG, 0)), (0, 6))


if __name__ == '__main__':
    unittest.main(verbosity=2)
