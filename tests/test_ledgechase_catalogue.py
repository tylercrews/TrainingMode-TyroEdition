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
        self.assertEqual([self.word(self.word(pages + page * 4) + 4) for page in range(3)], [5, 17, 8])
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
        # Nine visible rows put event 17 at row 9, with eight preceding entries.
        machine.install_hook(0x8024E438,0x80612000)
        machine.install_hook(0x8024CFFC,0x80614000)
        scroll=0x8050A000
        machine.cpu.mem_write(scroll-4,b'\xA5'*0x84)
        machine.call(0x8024E420,scroll,16)
        self.assertEqual(bytes(machine.cpu.mem_read(scroll,1)),b'\x08')
        self.assertEqual(self.word(scroll+4),8)
        self.assertEqual(machine.call(functions[27],26,0),16)
        self.assertEqual(bytes(machine.cpu.mem_read(scroll-4,4)),b'\xA5'*4)
        self.assertEqual(bytes(machine.cpu.mem_read(scroll+0x7C,4)),b'\xA5'*4)


if __name__ == '__main__':
    unittest.main(verbosity=2)
