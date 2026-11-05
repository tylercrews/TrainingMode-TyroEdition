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

    def test_a_load_after_transforming_gives_each_form_its_own_data(self):
        """Zelda's data used to land on Sheik, who stayed awake at Zelda's saved
        spot, while the sleeping Zelda came back as the active fighter."""
        self.m.call('TestStateSave')
        self.m.call('TestStateTransform')
        self.assertEqual(self.active(), SECOND)
        self.m.call('TestStateLoad')
        self.assert_restored()

    def test_transforming_twice_loads_the_same_way(self):
        self.m.call('TestStateSave')
        self.m.call('TestStateTransform')
        self.m.call('TestStateTransform')
        self.m.call('TestStateLoad')
        self.assert_restored()

    def test_a_load_after_transforming_can_be_repeated(self):
        self.m.call('TestStateSave')
        self.m.call('TestStateTransform')
        self.m.call('TestStateLoad')
        self.m.call('TestStateTransform')
        self.m.call('TestStateLoad')
        self.assert_restored()

    def test_a_state_saved_transformed_loads_transformed(self):
        self.m.call('TestStateTransform')
        self.m.call('TestStateSave')
        self.m.call('TestStateTransform')
        self.m.call('TestStateLoad')
        self.assertEqual(self.active(), SECOND)
        self.assertEqual(self.states(), (self.sleep, self.wait))


if __name__ == '__main__':
    unittest.main(verbosity=2)
