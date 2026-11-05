"""Execute the production savestate load on a Zelda and Sheik pair."""
from pathlib import Path
import subprocess
import unittest
import test_settings as fixtures
from test_lowlaser import load_probe

ROOT = Path(__file__).resolve().parents[1]
FIRST, SECOND = 0, 1


def build():
    subprocess.run([
        'C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe', '-O2', '-w',
        '-ffreestanding', '-fno-builtin', '-msdata=none', '-mcpu=750', '-nostdlib',
        '-Wl,-Ttext=0x101000,-e,TestStateLoaded,--unresolved-symbols=ignore-all',
        'tests/savestate_probe.c', '-o', 'build/savestate-probe.elf'
    ], cwd=ROOT, check=True)


class ZeldaAndSheikTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def setUp(self):
        self.m = fixtures.Machine()
        self.m.cpu.reg_write(fixtures.gpr(13), 0x804DB6A0)
        load_probe(self.m, 'savestate-probe.elf', 'TestStateLoaded')
        self.m.call('TestStateInit')
        self.wait, self.sleep = self.info(9), self.info(10)
        self.assertLess(self.info(11), 0x300000)  # Fits where the probe keeps it.

    def info(self, key): return self.m.call('TestStateInfo', key)
    def active(self): return self.info(0)
    def positions(self): return self.info(1), self.info(2)
    def states(self): return self.info(3), self.info(4)
    def ms(self): return self.info(5), self.info(6)
    def asleep(self): return self.info(7), self.info(8)

    def assert_restored(self):
        self.assertEqual(self.active(), FIRST)
        self.assertEqual(self.positions(), (10, 20))
        self.assertEqual(self.states(), (self.wait, self.sleep))
        self.assertEqual(self.ms(), (0, 1))
        self.assertEqual(self.asleep(), (0, 1))

    def test_a_load_with_no_transform_puts_the_pair_back(self):
        self.m.call('TestStateSave')
        self.m.call('TestStateMove', 77)
        self.m.call('TestStateLoad')
        self.assert_restored()

    def test_the_lab_gets_back_the_form_that_was_saved(self):
        """Zelda's data used to land on Sheik, who stayed awake at Zelda's saved
        spot, while the sleeping Zelda came back as the active fighter."""
        self.m.call('TestStateSave')
        self.m.call('TestStateTransform')
        self.assertEqual(self.active(), SECOND)
        self.m.call('TestStateLoadExact')
        self.assert_restored()
        self.assertEqual(self.info(12), 0)

    def test_an_event_reset_keeps_the_form_you_changed_to(self):
        self.m.call('TestStateSave')
        self.m.call('TestStateTransform')
        self.m.call('TestStateLoad')
        self.assertEqual(self.active(), SECOND)
        self.assertEqual(self.info(12), 1)
        self.assertEqual(self.states(), (self.sleep, self.wait))
        self.assertEqual(self.positions()[1], 10)  # Where the saved form was.
        self.assertEqual(self.ms(), (1, 0))
        self.assertEqual(self.asleep(), (1, 0))

    def test_a_reset_in_the_form_you_started_as_transforms_nothing(self):
        self.m.call('TestStateSave')
        self.m.call('TestStateLoad')
        self.assertEqual(self.info(12), 0)
        self.assert_restored()

    def test_transforming_twice_loads_the_same_way(self):
        self.m.call('TestStateSave')
        self.m.call('TestStateTransform')
        self.m.call('TestStateTransform')
        self.m.call('TestStateLoad')
        self.assert_restored()

    def test_resets_can_follow_each_other_through_both_forms(self):
        self.m.call('TestStateSave')
        self.m.call('TestStateTransform')
        self.m.call('TestStateLoad')
        self.m.call('TestStateTransform')
        self.m.call('TestStateLoad')
        self.assert_restored()

    def test_a_state_saved_transformed_comes_back_exact_or_keeps_your_form(self):
        self.m.call('TestStateTransform')
        self.m.call('TestStateSave')
        self.m.call('TestStateTransform')
        self.m.call('TestStateLoadExact')
        self.assertEqual(self.active(), SECOND)
        self.assertEqual(self.states(), (self.sleep, self.wait))
        self.m.call('TestStateTransform')
        self.m.call('TestStateLoad')
        self.assertEqual(self.active(), FIRST)
        self.assertEqual(self.states(), (self.wait, self.sleep))

    def test_the_other_form_takes_over_a_common_state_and_its_variables(self):
        self.m.call('TestStateSet', self.info(16), 7)  # On the ledge: which ledge is a variable.
        self.m.call('TestStateSave')
        self.m.call('TestStateTransform')
        self.m.call('TestStateLoad')
        self.assertEqual(self.states()[1], self.info(16))
        self.assertEqual(self.info(13), 7)

    def test_a_special_state_means_nothing_to_the_other_form(self):
        self.m.call('TestStateSet', 350, 0)
        self.m.call('TestStateSave')
        self.m.call('TestStateTransform')
        self.m.call('TestStateLoad')
        self.assertEqual(self.states()[1], self.wait)

    def test_the_form_left_asleep_wont_wake_up_with_its_inputs_off(self):
        """Under Fire saves before Go, with both forms locked, and only unlocks
        the awake one afterward."""
        self.m.call('TestStateLock', FIRST, 1)
        self.m.call('TestStateLock', SECOND, 1)
        self.m.call('TestStateSave')
        self.m.call('TestStateLock', FIRST, 0)
        self.m.call('TestStateLock', SECOND, 0)
        self.m.call('TestStateLoad')
        self.assertEqual((self.info(14), self.info(15)), (1, 0))

    def test_the_old_events_keep_the_form_the_same_way(self):
        self.m.call('TestStateSave')
        self.m.call('TestStateTransform')
        self.m.call('TestStateNoteForms')
        self.m.call('TestStateLoadExact')  # Stands in for the ASM load.
        self.assertEqual(self.active(), FIRST)
        self.m.call('TestStateKeepForms')
        self.assertEqual(self.active(), SECOND)


if __name__ == '__main__':
    unittest.main(verbosity=2)
