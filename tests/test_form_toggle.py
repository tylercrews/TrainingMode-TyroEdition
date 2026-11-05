"""Execute the production Zelda and Sheik toggle on the character select."""
import re
import subprocess
import unittest
from pathlib import Path
from test_menu import Machine, ROOT

ZELDA, SHEIK = 18, 19  # CKIND_* (external)
ZELDA_ICON, OTHER_ICON, NO_ICON = 7, 3, 25
HUMAN, CPU = 0, 1  # The solo CSS's two doors.
SOLO, VS = 1, 4  # Its maximum players.


def build():
    compiler = Path('C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe')
    exports = re.findall(r'\b(TestForm\w+)\(', (ROOT / 'tests/form_toggle_probe.c').read_text())
    subprocess.run([str(compiler) if compiler.exists() else 'powerpc-eabi-gcc',
        '-O2', '-fno-inline', '-w', '-ffreestanding', '-fno-builtin',
        '-msdata=none', '-mcpu=750', '-nostdlib', '-ffunction-sections', '-fdata-sections',
        '-Wl,-Ttext=0x101000,-e,TestFormInit,--gc-sections,--unresolved-symbols=ignore-all',
        *['-Wl,--undefined=' + name for name in exports],
        'tests/form_toggle_probe.c', 'src/events.c',
        '-o', 'build/form-toggle-test.elf'], cwd=ROOT, check=True)


class FormToggleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def setUp(self):
        self.m = Machine('form-toggle-test.elf', 'TestFormInit')

    def enter(self, event, players=SOLO):
        self.m.call('TestFormEnter', self.m.symbols[event], players)

    def door(self, door, icon=ZELDA_ICON): self.m.call('TestFormDoor', door, icon)
    def press(self, port=0): self.m.call('TestFormPress', port)
    def frame(self): self.m.call('TestFormFrame')
    def info(self, key, door=0): return self.m.call('TestFormInfo', key, door)
    def zelda(self, door=0): return self.info(0, door)
    def sheik_names(self, door=0): return self.info(1, door)
    def updates(self, door=0): return self.info(2, door)
    def voices(self): return self.info(3)
    def voice(self): return self.info(4)

    def test_it_runs_after_the_css_reads_its_input(self):
        self.enter('Wavedash')
        self.assertEqual(self.info(5), 2)

    def test_the_icon_says_sheik_until_z_chooses_zelda(self):
        self.enter('Wavedash')
        self.door(HUMAN)
        self.frame()
        self.assertEqual(self.sheik_names(), 1)
        self.press()
        self.assertEqual((self.zelda(), self.voices(), self.voice()), (1, 1, ZELDA))
        self.assertEqual(self.updates(), 1)  # The native name comes back.
        self.assertEqual(self.sheik_names(), 1)  # And nothing writes over it.

    def test_z_again_goes_back_to_sheik(self):
        self.enter('Wavedash')
        self.door(HUMAN)
        self.press()
        self.press()
        self.assertEqual((self.zelda(), self.voices(), self.voice()), (0, 2, SHEIK))
        self.assertEqual(self.sheik_names(), 1)

    def test_z_needs_the_icon_to_be_picked(self):
        self.enter('Wavedash')
        self.door(HUMAN, OTHER_ICON)
        self.press()
        self.assertEqual((self.zelda(), self.voices(), self.sheik_names()), (0, 0, 0))

    def test_a_chosen_nametag_keeps_its_place(self):
        self.enter('Wavedash')
        self.door(HUMAN)
        self.m.call('TestFormTag', HUMAN)
        self.frame()
        self.assertEqual(self.sheik_names(), 0)

    def test_only_your_controller_counts_on_the_solo_select(self):
        self.enter('Wavedash')
        self.door(HUMAN)
        self.press(1)
        self.assertEqual((self.zelda(), self.voices()), (0, 0))

    def test_z_reaches_the_cpu_when_youre_not_on_the_icon(self):
        self.enter('Lab')
        self.door(HUMAN, OTHER_ICON)
        self.door(CPU)
        self.press()
        self.assertEqual((self.zelda(HUMAN), self.zelda(CPU)), (0, 1))

    def test_a_held_puck_picks_its_door(self):
        self.enter('Lab')
        self.door(HUMAN)
        self.door(CPU)
        self.press()
        self.assertEqual((self.zelda(HUMAN), self.zelda(CPU)), (1, 0))
        self.m.call('TestFormHold', 0, CPU)
        self.press()
        self.assertEqual((self.zelda(HUMAN), self.zelda(CPU)), (1, 1))

    def test_a_held_puck_on_another_character_does_nothing(self):
        self.enter('Lab')
        self.door(HUMAN)
        self.door(CPU, OTHER_ICON)
        self.m.call('TestFormHold', 0, CPU)
        self.press()
        self.assertEqual((self.zelda(HUMAN), self.zelda(CPU), self.voices()), (0, 0, 0))

    def test_a_cpu_limited_to_sheik_cant_change_and_says_so(self):
        self.enter('Edgeguard')  # Only Sheik has a recovery there.
        self.door(HUMAN, OTHER_ICON)
        self.door(CPU)
        self.m.call('TestFormZelda', CPU, 1)  # Saved from an event that allows it.
        self.press()
        self.assertEqual((self.zelda(CPU), self.voices()), (1, 0))
        self.assertEqual(self.sheik_names(CPU), 1)

    def test_your_own_door_is_free_in_those_events(self):
        self.enter('Edgeguard')
        self.door(HUMAN)
        self.press()
        self.assertEqual(self.zelda(HUMAN), 1)

    def test_each_port_has_its_own_door_on_the_team_select(self):
        self.enter('GrabPunish', VS)
        for door in (0, 2, 3):
            self.door(door)
        self.press(2)
        self.assertEqual([self.zelda(i) for i in range(4)], [0, 0, 1, 0])

    def test_a_closed_door_on_the_team_select_isnt_a_target(self):
        self.enter('GrabPunish', VS)
        self.door(3, NO_ICON)
        self.press(3)
        self.assertEqual(self.voices(), 0)

    def test_a_team_select_port_with_no_zelda_of_its_own_reaches_a_cpu_that_has(self):
        self.enter('GrabPunish', VS)
        self.door(0, OTHER_ICON)
        self.door(1)
        self.press(0)
        self.assertEqual((self.zelda(0), self.zelda(1)), (0, 1))

    def test_legacy_events_ask_what_the_cpu_door_chose(self):
        self.enter('Combo')
        self.assertEqual(self.m.call('TestFormCpu'), SHEIK)
        self.m.call('TestFormZelda', CPU, 1)
        self.assertEqual(self.m.call('TestFormCpu'), ZELDA)
        self.enter('Edgeguard')
        self.m.call('TestFormZelda', CPU, 1)
        self.assertEqual(self.m.call('TestFormCpu'), SHEIK)


if __name__ == '__main__':
    unittest.main(verbosity=2)
