"""Check the new event's actual relocated shared-DAT catalogue and CSS contract."""
import struct
import unittest
from test_score_storage import NativeMachine, gpr


class LedgechaseCatalogueTests(unittest.TestCase):
    def setUp(self):
        self.m = NativeMachine()
        self.functions = self.m.load_services()

    def word(self, pointer):
        return struct.unpack('>I', self.m.cpu.mem_read(pointer, 4))[0]

    def text(self, pointer):
        return bytes(self.m.cpu.mem_read(pointer, 160)).split(b'\0', 1)[0].decode()

    def test_general_seventeenth_entry_preserves_existing_order(self):
        pages = self.functions[0]
        self.assertEqual([self.word(self.word(pages + page * 4) + 4) for page in range(4)], [6, 17, 11, 2])
        general = self.word(pages + 4)
        events = self.word(general + 8)
        expected = ['Training Lab', 'L-Cancel Training', 'Ledgedash Training',
                    'Wavedash Training', 'Techchase Training']
        for index, name in enumerate(expected):
            descriptor = self.word(events + index * 4)
            self.assertEqual(self.text(self.word(descriptor)).strip(), name)
        descriptor = self.word(events + 16 * 4)
        self.assertEqual(self.text(self.word(descriptor)).strip(), 'Ledgechase Training')
        self.assertEqual(self.text(self.word(descriptor + 8)), 'ledgechase')

    def test_the_doubles_page_has_both_2v1_events_on_the_team_select(self):
        pages = self.functions[0]
        doubles = self.word(pages + 3 * 4)
        self.assertEqual(self.text(self.word(doubles)), 'Doubles')
        events = self.word(doubles + 8)
        for index, (name, file) in enumerate((('Grab Punish Practice', 'grabpunish'), ('Passing Practice', 'passing'))):
            descriptor = self.word(events + index * 4)
            self.assertEqual(self.text(self.word(descriptor)).strip(), name)
            self.assertEqual(self.text(self.word(descriptor + 8)), file)
            self.assertEqual(self.m.call(self.functions[10], 3, index), 0)  # SLCHRKIND_VS
            self.assertEqual(self.m.call(self.functions[11], 3, index), 1)  # stage select
            self.assertEqual(self.text(self.m.call(self.functions[15], 3, index)), 'TM/grabpunishCSS.dat')

    def test_only_the_team_select_opens_its_doors(self):
        machine, functions = self.m, self.functions
        css, memcard = 0x80497758, self.word(machine.cpu.reg_read(gpr(13)) - 30656)
        # Grab Punish and Passing Practice (Doubles), then Ledgechase (General Tech).
        for page, event, kinds in ((3, 0, (0, 1, 1, 3)), (3, 1, (0, 1, 1, 3)), (1, 16, (3, 3, 3, 3))):
            with self.subTest(page=page, event=event):
                machine.call(functions[28], 2, 0, page)  # TM_SETTING_EVENT_PAGE
                machine.cpu.mem_write(memcard + 0x535, bytes((event,)))
                machine.cpu.mem_write(css, struct.pack('>H', 1))
                for door in range(4):
                    machine.cpu.mem_write(css + 0x70 + door * 36, bytes((2, 3)))
                machine.call(functions[39], 2)  # TM_CHARACTERS_OPEN
                self.assertEqual(tuple(machine.cpu.mem_read(css + 0x71 + door * 36, 1)[0] for door in range(4)), kinds)

    def test_upb_sweetspot_sits_under_side_b_sweetspot_with_a_stage_select(self):
        character = self.word(self.word(self.functions[0] + 8) + 8)
        names = [self.text(self.word(self.word(character + index * 4))).strip() for index in (2, 3)]
        self.assertEqual(names, ['Side-B Sweetspot', 'Up-B Sweetspot'])
        self.assertEqual(self.text(self.word(self.word(character + 3 * 4) + 8)), 'upbsweetspot')
        self.assertEqual(self.m.call(self.functions[11], 2, 3), 1)  # stage select

    def test_the_escape_events_end_the_character_page_with_their_cpu_and_stage(self):
        character = self.word(self.functions[0] + 2 * 4)
        self.assertEqual(self.word(character + 4), 11)
        events = self.word(character + 8)
        for index, name, file, cpu, stage in ((9, 'Escape Samus Up-B', 'escapesamus', 16, 31),  # CKIND_SAMUS, Battlefield
                                              (10, 'Escape IC Grab (Beta)', 'escapeic', 14, 32)):  # CKIND_ICECLIMBERS, FD
            with self.subTest(event=name):
                descriptor = self.word(events + index * 4)
                self.assertEqual(self.text(self.word(descriptor)).strip(), name)
                self.assertEqual(self.text(self.word(descriptor + 8)), file)
                self.assertEqual(self.m.call(self.functions[10], 2, index), 14)  # SLCHRKIND_EVENT
                self.assertEqual(self.m.call(self.functions[11], 2, index), 0)  # no stage select
                self.assertEqual(self.m.call(self.functions[12], 2, index), cpu)
                self.assertEqual(self.m.call(self.functions[13], 2, index), stage)

    def test_ledgegrab_speedrun_ends_the_minigames_page_with_no_cpu_or_score_slot(self):
        functions, machine = self.functions, self.m
        minigames = self.word(self.word(functions[0]) + 8)
        descriptor = self.word(minigames + 5 * 4)
        self.assertEqual(self.text(self.word(descriptor)).strip(), 'Ledgegrab Speedrun')
        self.assertEqual(self.text(self.word(descriptor + 8)), 'ledgegrab')
        self.assertEqual(machine.call(functions[11], 0, 5), 1)  # stage select
        self.assertEqual(machine.call(functions[12], 0, 5), 0xFFFFFFFF)  # no CPU
        machine.call(functions[28], 2, 0, 0)  # TM_SETTING_EVENT_PAGE
        self.assertEqual(machine.call(functions[43], 5), 0)
        reaction = machine.call(functions[43], 3)
        self.assertNotEqual(reaction, 0)
        machine.call(functions[28], 2, 0, 1)
        self.assertEqual(machine.call(functions[43], 0), reaction + 2 * 4)  # Slot 5 stays Training Lab's.

    def test_both_fighters_selectable_fixed_fd_and_percent_hud_enabled(self):
        functions = self.functions
        self.assertEqual(self.m.call(functions[10], 1, 16), self.m.call(functions[10], 1, 4))
        self.assertEqual(self.m.call(functions[11], 1, 16), 0)
        self.assertEqual(self.m.call(functions[12], 1, 16), 0xFFFFFFFF)
        self.assertEqual(self.m.call(functions[13], 1, 16), 32)
        whitelist = self.m.call(functions[25], 16, 1)
        self.assertEqual(bytes(self.m.cpu.mem_read(whitelist, 8)), b'\xFF' * 8)
        descriptor = self.m.call(functions[1], 1, 16)
        match = self.word(descriptor + 40)
        self.assertTrue(self.word(match) & 0x01000000)

    def test_saved_seventeenth_event_restores_native_cursor_and_scroll_window(self):
        machine, functions = self.m, self.functions
        machine.call(functions[28],2,0,1)
        machine.call(functions[28],26,0,16)
        machine.install_hook(0x8024E858,0x80610000)
        machine.call(0x8024E858,0,0,stop=0x8024E85C)
        self.assertEqual(machine.cpu.reg_read(gpr(30)),16)
        # Execute the actual native scroll initializer and its page-size hooks.
        # Nine visible rows over 17 events scroll down to event 9 at most, so
        # event 16 (index 15) is row 7 and Ledgechase (16) the last row. Grab
        # Punish was an 18th until it moved to the Doubles page in T10.
        machine.install_hook(0x8024E438,0x80612000)
        machine.install_hook(0x8024CFFC,0x80614000)
        scroll=0x8050A000
        for event, row in ((15, 7), (16, 8)):
            machine.cpu.mem_write(scroll-4,b'\xA5'*0x84)
            machine.call(0x8024E420,scroll,event)
            self.assertEqual(bytes(machine.cpu.mem_read(scroll,1)),bytes((row,)))
            self.assertEqual(self.word(scroll+4),8)
            self.assertEqual(bytes(machine.cpu.mem_read(scroll-4,4)),b'\xA5'*4)
            self.assertEqual(bytes(machine.cpu.mem_read(scroll+0x7C,4)),b'\xA5'*4)
        self.assertEqual(machine.call(functions[27],26,0),16)
        # Character-specific Tech's 11 events (T10) scroll two at most: the
        # two escape events are its last two rows.
        machine.call(functions[28],2,0,2)
        for event, row in ((9, 7), (10, 8)):
            machine.cpu.mem_write(scroll-4,b'\xA5'*0x84)
            machine.call(0x8024E420,scroll,event)
            self.assertEqual((bytes(machine.cpu.mem_read(scroll,1)), self.word(scroll+4)), (bytes((row,)), 2))
        # The Doubles page's two events never scroll.
        machine.call(functions[28],2,0,3)
        for event in (0, 1):
            machine.cpu.mem_write(scroll-4,b'\xA5'*0x84)
            machine.call(0x8024E420,scroll,event)
            self.assertEqual((bytes(machine.cpu.mem_read(scroll,1)), self.word(scroll+4)), (bytes((event,)), 0))


if __name__ == '__main__':
    unittest.main(verbosity=2)
