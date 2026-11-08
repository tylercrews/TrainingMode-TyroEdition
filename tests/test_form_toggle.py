"""Execute the production Zelda and Sheik toggle on the character select."""
import re
import subprocess
import unittest
from pathlib import Path
from test_menu import Machine, ROOT, reg

ZELDA, SHEIK = 18, 19  # CKIND_* (external)
ZELDA_ICON, OTHER_ICON, NO_ICON = 7, 3, 25
HUMAN, CPU = 0, 1  # The solo CSS's two doors.
SOLO, VS = 1, 4  # Its maximum players.
NO_HINT, OFFERS_ZELDA, OFFERS_SHEIK = 0, 1, 2


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
        self.m.cpu.reg_write(reg.UC_PPC_REG_MSR, 1 << 13)  # The hints are placed in floats.

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

    def hint(self, door=0): return self.info(7, door)

    def test_every_door_z_can_reach_gets_a_hint(self):
        for event, players, made in (('Wavedash', SOLO, 1), ('Lab', SOLO, 2), ('GrabPunish', VS, 4)):
            self.enter(event, players)
            self.assertEqual(self.info(6), made, event)

    def test_the_hint_names_the_form_z_changes_to(self):
        self.enter('Wavedash')
        self.frame()
        self.assertEqual(self.hint(), NO_HINT)
        self.door(HUMAN)
        self.frame()
        self.assertEqual((self.hint(), self.info(8)), (OFFERS_ZELDA, 1))  # In gold.
        self.press()
        self.assertEqual(self.hint(), OFFERS_SHEIK)
        self.press()
        self.assertEqual(self.hint(), OFFERS_ZELDA)

    def test_the_hint_sits_centered_under_the_name(self):
        self.enter('Wavedash')
        self.door(HUMAN)
        self.frame()
        # The name's 200 wide box at 0.058 starts at -27.2, so it centers on
        # -21.4; its 32 tall line at 0.055 runs from 20.2 to 21.96.
        self.assertAlmostEqual(self.info(9) / 1000, -21.4, places=2)
        self.assertAlmostEqual(self.info(10) / 1000, 21.96 + 0.4, places=2)
        self.assertEqual(self.info(11), 45)
        self.assertAlmostEqual(self.info(12) / 1000, 1.4 * 200 * 0.058 / 0.045, places=1)
        self.assertEqual(self.info(13), 111)  # Centered, squeezed to fit, tight.

    def test_the_team_select_hint_goes_under_that_doors_name(self):
        self.enter('GrabPunish', VS)
        self.door(2)
        self.frame()
        self.assertEqual([self.hint(i) for i in range(4)], [NO_HINT, NO_HINT, OFFERS_ZELDA, NO_HINT])
        self.assertAlmostEqual(self.info(9, 2) / 1000, 3.1 + 80 * 0.058, places=2)
        self.assertAlmostEqual(self.info(10, 2) / 1000, 18.6 + 32 * 0.055 + 0.4, places=2)

    def test_the_hint_shows_only_where_z_lands(self):
        self.enter('Lab')
        self.door(HUMAN)
        self.door(CPU)
        self.frame()
        self.assertEqual((self.hint(HUMAN), self.hint(CPU)), (OFFERS_ZELDA, NO_HINT))
        self.m.call('TestFormHold', 0, CPU)
        self.frame()
        self.assertEqual((self.hint(HUMAN), self.hint(CPU)), (NO_HINT, OFFERS_ZELDA))

    def test_no_hint_where_z_does_nothing(self):
        self.enter('Wavedash')
        self.door(HUMAN, OTHER_ICON)
        self.frame()
        self.assertEqual(self.hint(), NO_HINT)
        self.enter('Edgeguard')  # Its CPU is always Sheik.
        self.door(HUMAN, OTHER_ICON)
        self.door(CPU)
        self.frame()
        self.assertEqual(self.hint(CPU), NO_HINT)

    def test_an_open_nametag_list_hides_the_hint(self):
        self.enter('Wavedash')
        self.door(HUMAN)
        self.m.call('TestFormTagList', HUMAN)
        self.frame()
        self.assertEqual(self.hint(), NO_HINT)

    def test_an_unplugged_team_select_port_shows_no_hint(self):
        self.enter('GrabPunish', VS)
        self.door(0)
        self.door(1)
        self.frame()
        self.assertEqual([self.hint(i) for i in range(2)], [OFFERS_ZELDA, OFFERS_ZELDA])
        for port in (1, 2, 3):
            self.m.call('TestFormUnplug', port)
        self.frame()
        self.assertEqual([self.hint(i) for i in range(2)], [OFFERS_ZELDA, NO_HINT])

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
