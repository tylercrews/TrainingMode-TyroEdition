"""Execute the production Up-B Sweetspot event's Marth, frame by frame."""
from pathlib import Path
import subprocess
import unittest
import test_settings as fixtures
from test_lowlaser import load_probe

ROOT = Path(__file__).resolve().parents[1]
DPAD_LEFT = 1
TIPPER_FRAME = 10  # MARTH_TIPPER_FRAME
FOX_UP_B = 354


def build():
    subprocess.run([
        'C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe', '-O2', '-w',
        '-ffreestanding', '-fno-builtin', '-msdata=none', '-mcpu=750', '-nostdlib',
        '-Wl,-Ttext=0x101000,-e,TestUpBLoaded,--unresolved-symbols=ignore-all',
        'tests/upbsweetspot_probe.c', '-o', 'build/upbsweetspot-probe.elf'
    ], cwd=ROOT, check=True)


class MarthTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def start(self, marth=1):
        self.m = fixtures.Machine()
        load_probe(self.m, 'upbsweetspot-probe.elf', 'TestUpBLoaded')
        self.m.call('TestUpBInit', marth)

    def info(self, key):
        value = self.m.call('TestUpBInfo', key)
        return value - (1 << 32) if value >> 31 else value
    def step(self, frames=1): self.m.call('TestUpBThink', frames)
    def swings(self): return self.info(0)
    def freezes(self): return self.info(1)
    def swinging(self): return bool(self.info(2))
    def on_platform(self): return bool(self.info(4))

    def test_marth_swings_as_the_attempt_starts_with_you_still_on_the_platform(self):
        self.start()
        self.step()
        self.assertTrue(self.on_platform())
        self.assertEqual(self.swings(), 1)
        self.assertTrue(self.swinging())

    def test_he_freezes_with_the_hitboxes_out_before_you_leave_the_platform(self):
        self.start()
        self.step(TIPPER_FRAME + 2)
        self.assertTrue(self.on_platform())
        self.assertEqual((self.freezes(), self.info(3)), (1, 0))
        self.assertEqual(self.info(5), 1)

    def test_the_tipper_is_moved_over_the_ledge_on_that_freeze(self):
        self.start()
        self.step(TIPPER_FRAME + 2)
        self.assertEqual(self.info(6), -850)  # Over the left ledge, at -85.

    def test_the_hitboxes_just_linger_while_you_wait(self):
        self.start()
        self.step(TIPPER_FRAME + 2)
        self.step(200)
        self.assertEqual((self.swings(), self.freezes(), self.info(3), self.info(5)), (1, 1, 0, 1))

    def test_your_up_b_doesnt_start_anything(self):
        self.start()
        self.step(TIPPER_FRAME + 2)
        self.m.call('TestUpBStart', FOX_UP_B)
        self.step(5)
        self.assertEqual((self.swings(), self.freezes()), (1, 1))

    def test_a_reset_swings_again(self):
        self.start()
        self.step(TIPPER_FRAME + 2)
        self.m.call('TestUpBDown', DPAD_LEFT)
        self.step()
        self.m.call('TestUpBDown', 0)
        self.assertEqual(self.swings(), 2)
        self.assertEqual(self.info(3), 100)  # The savestate let go of the freeze.
        self.step(TIPPER_FRAME + 2)
        self.assertEqual(self.freezes(), 2)

    def test_a_finished_attempt_lets_the_swing_end(self):
        self.start()
        self.step(TIPPER_FRAME + 2)
        self.m.call('TestUpBFakeGrab')
        self.step()
        self.assertEqual(self.info(3), 100)

    def test_marth_off_leaves_him_standing(self):
        self.start(marth=0)
        self.step(TIPPER_FRAME + 2)
        self.assertEqual((self.swings(), self.freezes()), (0, 0))
        self.assertFalse(self.swinging())


if __name__ == '__main__':
    unittest.main(verbosity=2)
