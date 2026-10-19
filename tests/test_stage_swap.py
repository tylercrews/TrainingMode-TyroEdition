"""Execute the production in-event stage change service and its EventInit handoff."""
import re
import subprocess
import unittest
from pathlib import Path
from test_menu import Machine, ROOT

IZUMI, PSTAD, BATTLE, FD = 2, 3, 31, 32  # GRKINDEXT_*
FOX, MARTH, PUFF, ZELDA, SHEIK, FALCO = 2, 9, 15, 18, 19, 20  # CKIND_* (external)
RETRY = 8                                  # MATCHENDKIND_RETRY
CARRY_TAG, CARRY_SIZE = 0x4C616231, 2048


def build():
    compiler = Path('C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe')
    exports = re.findall(r'\b(TestStageSwap\w+)\(', (ROOT / 'tests/stage_swap_probe.c').read_text())
    subprocess.run([str(compiler) if compiler.exists() else 'powerpc-eabi-gcc',
        '-O2', '-fno-inline', '-w', '-ffreestanding', '-fno-builtin',
        '-msdata=none', '-mcpu=750', '-nostdlib', '-ffunction-sections', '-fdata-sections',
        '-Wl,-Ttext=0x101000,-e,TestStageSwapInit,--gc-sections,--unresolved-symbols=ignore-all',
        *['-Wl,--undefined=' + name for name in exports],
        'tests/stage_swap_probe.c', 'src/events.c',
        '-o', 'build/stage-swap-test.elf'], cwd=ROOT, check=True)


class StageSwapTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def setUp(self):
        self.m = Machine('stage-swap-test.elf', 'TestStageSwapInit')
        self.low_laser = self.m.symbols['LowLaser']
        self.slalom = self.m.symbols['Slalom']
        self.lab = self.m.symbols['Lab']

    def info(self, *keys): return [self.m.call('TestStageSwapInfo', key) for key in keys]
    def enter(self, event): self.m.call('TestStageSwapEnter', event)
    def prep(self, event):
        located = self.m.call('TestStageSwapLocate', event)
        self.assertGreaterEqual(located, 0)
        self.m.call('TestStageSwapPrep', located)
    def store(self, tag=CARRY_TAG, size=64): return self.m.call('TestStageSwapStore', tag, size)
    def take(self, tag=CARRY_TAG, size=64): return self.m.call('TestStageSwapTake', tag, size)

    def test_restart_ends_as_retry_and_match_prep_loads_the_new_stage_once(self):
        self.m.call('TestStageSwapQueue', FD)  # chosen on the SSS
        self.enter(self.low_laser)
        self.prep(self.low_laser)
        self.assertEqual(self.info(0, 1, 2), [FD, FD, 0])
        self.m.call('TestStageSwapRestart', IZUMI)
        self.assertEqual(self.info(3, 4, 5, 6), [1, 1, RETRY, 3])
        self.assertEqual(self.info(0, 2, 15), [FD, 0, 0])  # nothing is loaded mid-match
        self.prep(self.low_laser)
        # The whole cache is dropped first, so no initialized file can move.
        self.assertEqual(self.info(0, 1, 2, 15), [IZUMI, IZUMI, 1, 1])
        # A later plain retry keeps the new stage without reloading the table.
        self.prep(self.low_laser)
        self.assertEqual(self.info(0, 1, 2), [IZUMI, IZUMI, 1])

    def test_fixed_stage_events_cannot_change_stage(self):
        self.m.call('TestStageSwapQueue', BATTLE)
        self.enter(self.slalom)
        self.m.call('TestStageSwapRestart', IZUMI)
        self.assertEqual(self.info(3, 4), [0, 0])
        self.prep(self.slalom)
        self.assertEqual(self.info(0, 1, 2), [BATTLE, FD, 0])

    def test_carry_survives_only_into_the_restarted_event(self):
        buffer = self.store()
        self.assertNotEqual(buffer, 0)
        self.enter(self.low_laser)
        self.m.call('TestStageSwapRestart', PSTAD)
        self.prep(self.low_laser)
        self.assertEqual(self.take(size=68), 0)
        self.assertEqual(self.take(tag=CARRY_TAG + 1), 0)
        self.assertEqual(self.take(), buffer)
        self.assertEqual(self.take(), 0)  # taken once
        # Without a stage change, the next event start discards it.
        self.store()
        self.prep(self.low_laser)
        self.assertEqual(self.take(), 0)

    def fighters(self, *args): return self.m.call('TestStageSwapFighters', *args)

    def test_fighter_restart_queues_both_fighters_at_match_prep(self):
        self.m.call('TestStageSwapQueue', FD)
        self.m.call('TestStageSwapQueueFighter', 0, FOX, 0)
        self.m.call('TestStageSwapQueueFighter', 1, MARTH, 0)  # chosen on the CSS
        self.m.call('TestStageSwapQueueFighter', 2, PUFF, 0)
        self.m.call('TestStageSwapSlotByte', 1, 1)
        self.enter(self.lab)
        self.prep(self.lab)
        self.assertEqual(self.info(11, 13, 2), [FOX, MARTH, 0])
        self.assertEqual(self.fighters(FALCO, 1, ZELDA, 2), 1)
        self.assertEqual(self.info(3, 4, 5, 6), [1, 1, RETRY, 3])
        self.assertEqual(self.info(7, 9, 2), [FOX, MARTH, 0])  # nothing is loaded mid-match
        self.prep(self.lab)
        self.assertEqual(self.info(7, 8, 9, 10, 2, 15), [FALCO, 1, ZELDA, 2, 1, 1])
        # The rest of the table survives the cache reset, including each slot's other bytes.
        self.assertEqual(self.info(0, 16, 17), [FD, PUFF, 1])
        # A CPU Zelda picked in-event stays Zelda, also after a later stage change.
        self.assertEqual(self.info(11, 12, 13, 14), [FALCO, 1, ZELDA, 2])
        self.m.call('TestStageSwapRestart', BATTLE)
        self.prep(self.lab)
        self.assertEqual(self.info(0, 1, 11, 13, 2), [BATTLE, BATTLE, FALCO, ZELDA, 2])
        # A new event start keeps the convention of turning a CPU Zelda into Sheik.
        self.prep(self.lab)
        self.assertEqual(self.info(13, 2), [SHEIK, 2])

    def test_fighter_restart_keeps_a_fighter_given_kind_minus_one(self):
        self.m.call('TestStageSwapQueueFighter', 0, FOX, 0)
        self.m.call('TestStageSwapQueueFighter', 1, ZELDA, 1)
        self.enter(self.lab)
        self.prep(self.lab)
        self.assertEqual(self.info(13), [SHEIK])
        self.assertEqual(self.fighters(PUFF, 3, -1, 0), 1)
        self.prep(self.lab)
        self.assertEqual(self.info(7, 8, 9, 10, 11, 13), [PUFF, 3, ZELDA, 1, PUFF, SHEIK])

    def test_fighter_restart_refuses_fixed_fighters(self):
        self.m.call('TestStageSwapQueueFighter', 0, FOX, 0)
        self.enter(self.low_laser)  # the target is always Jigglypuff
        self.assertEqual(self.fighters(-1, 0, MARTH, 0), 0)
        self.assertEqual(self.fighters(-1, 0, -1, 0), 0)
        self.assertEqual(self.info(3, 4), [0, 0])
        self.assertEqual(self.fighters(FALCO, 0, -1, 0), 1)
        self.prep(self.low_laser)
        self.assertEqual(self.info(11, 13), [FALCO, PUFF])

    def test_carry_rejects_untagged_and_oversize_requests(self):
        self.assertEqual(self.store(tag=0), 0)
        self.assertEqual(self.store(size=CARRY_SIZE + 1), 0)
        self.assertNotEqual(self.store(size=CARRY_SIZE), 0)


if __name__ == '__main__':
    unittest.main(verbosity=2)
