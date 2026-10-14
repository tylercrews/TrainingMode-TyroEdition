"""Exercise the independently versioned score-region preference extension."""
from pathlib import Path
import struct
import unittest
import test_settings as fixtures
from test_settings import Machine, RECORD, ROOT

EXT = 0x8045A6C0 + 0x1AE0
COUNTS = (9, 2, 2, 3, 3, 3, 4, 2)
DEFAULTS = (0, 0, 1, 0, 0, 0, 0, 0)

class ExtensionTests(unittest.TestCase):
    def setUp(self):
        self.m = Machine()
        self.m.init()
        self.m.cpu.mem_write(EXT - 4, b'\xA5' * 100)
        self.m.call('TestClearDirty')

    def test_absent_defaults_are_read_only_and_first_edit_only_owns_twelve_bytes(self):
        m = self.m
        before = bytes(m.cpu.mem_read(EXT - 4, 100))
        core = m.record()
        self.assertEqual(tuple(m.call('Settings_Get', 27, i) for i in range(8)), DEFAULTS)
        self.assertEqual(m.call('Settings_Get', 14, 30), 0)
        self.assertEqual(bytes(m.cpu.mem_read(EXT - 4, 100)), before)
        m.call('Settings_Set', 27, 0, 8)
        self.assertEqual(m.call('TestDirty'), 0)  # Defer while native card resources are absent.
        m.cpu.mem_write(0x80433330, struct.pack('>I', 1))
        m.cpu.mem_write(0x80433374, struct.pack('>I', 0x80407000))
        m.cpu.mem_write(0x80432A68, struct.pack('>2I', 0x80409000, 0x8040A000))
        m.call('Settings_CommitPending')
        self.assertEqual(m.call('TestDirty'), 1)
        self.assertEqual(m.record(), core)
        self.assertEqual(bytes(m.cpu.mem_read(EXT - 4, 4)), b'\xA5' * 4)
        self.assertEqual(bytes(m.cpu.mem_read(EXT + 12, 84)), b'\xA5' * 84)
        self.assertEqual(bytes(m.cpu.mem_read(EXT, 8)), b'TYEX\x01\x04\xA5\x5A')

    def test_every_value_and_invalid_write_preserve_neighbors_and_reloaded_preferences(self):
        m = self.m
        for index, count in enumerate(COUNTS):
            for value in range(count):
                before = tuple(m.call('Settings_Get', 27, i) for i in range(8))
                m.call('Settings_Set', 27, index, value)
                self.assertEqual(m.call('Settings_Get', 27, index), value)
                for other in range(8):
                    if other != index: self.assertEqual(m.call('Settings_Get', 27, other), before[other])
            snapshot = bytes(m.cpu.mem_read(EXT, 92))
            for value in (count, 0xFFFFFFFF):
                m.call('Settings_Set', 27, index, value)
                self.assertEqual(bytes(m.cpu.mem_read(EXT, 92)), snapshot)
        before = bytes(m.cpu.mem_read(EXT, 92))
        fresh = Machine(); fresh.put(m.record()); fresh.cpu.mem_write(EXT, before)
        self.assertEqual(tuple(fresh.call('Settings_Get', 27, i) for i in range(8)),
                         tuple(count - 1 for count in COUNTS))

    def test_new_osd_palette_and_editor_leave_core_palette_bytes_unchanged(self):
        m = self.m
        core = m.record()
        for color in range(8):
            m.call('Settings_Set', 14, 30, color)
            self.assertEqual(m.call('Settings_Get', 14, 30), color)
            self.assertEqual(bool(m.call('Settings_Get', 0, 0) & (1 << 30)), bool(color))
            self.assertEqual(m.record(), core)
        m.call('Settings_Set', 14, 30, 7)
        m.call('Settings_Set', 17, 18, 1)  # Native Boolean exit pass retains chosen palette.
        self.assertEqual(m.call('Settings_Get', 14, 30), 7)
        m.call('Settings_Set', 17, 18, 0)
        self.assertEqual(m.call('Settings_Get', 14, 30), 0)
        m.call('TestEditorInit'); m.call('TestEditorInput', 0x200, 21)
        self.assertEqual(m.call('Settings_Get', 14, 30), 1)
        self.assertEqual(m.record(), core)
        m.call('Settings_Set', 0, 0, m.call('Settings_Get', 0, 0) & ~(1 << 30))
        self.assertEqual(m.call('Settings_Get', 14, 30), 0)

    def test_foreign_future_core_and_future_extension_are_private_and_byte_preserving(self):
        for mode in ('foreign', 'core', 'extension'):
            with self.subTest(mode=mode):
                m = Machine(); m.init()
                m.cpu.mem_write(EXT, b'TYEX\x02\x04\xA5\x5A' + bytes(84))
                if mode == 'foreign': m.cpu.mem_write(0x80000000, b'GTME01')
                elif mode == 'core':
                    core = bytearray(m.record()); core[10] &= 0x3F; m.put(core)
                before = bytes(m.cpu.mem_read(EXT, 92)); core = m.record()
                m.call('Settings_Set', 27, 0, 8); m.call('Settings_Set', 14, 30, 6)
                self.assertEqual(m.call('Settings_Get', 27, 0), 8)
                self.assertEqual(m.call('Settings_Get', 14, 30), 6)
                self.assertEqual(bytes(m.cpu.mem_read(EXT, 92)), before)
                self.assertEqual(m.record(), core)

    def test_extension_repair_preserves_padding_and_other_preferences(self):
        m = self.m
        m.call('Settings_Set', 27, 0, 8)
        m.cpu.mem_write(EXT + 8, b'\xFD\xFF\xFF\xA5')
        before = bytes(m.cpu.mem_read(EXT, 12))
        m.call('Settings_Get', 27, 0)
        after = bytes(m.cpu.mem_read(EXT, 12))
        self.assertEqual(after[:9], before[:9])
        self.assertEqual(after[10] & 0x80, 0x80)
        self.assertEqual(after[11], 0xA5)
        self.assertEqual(m.call('Settings_Get', 27, 0), 0)
        self.assertEqual(m.call('Settings_Get', 14, 30), 5)

class RelocatedExtensionTests(unittest.TestCase):
    dat_path = fixtures.RelocatedDATSettingsTests.dat_path
    def setUp(self): fixtures.RelocatedDATSettingsTests.setUp(self)
    def test_extension_roundtrip_in_optimized_production_dat(self):
        m = self.m
        m.init()
        core = m.record()
        for index, count in enumerate(COUNTS): m.call('DAT_Set', 27, index, count - 1)
        for color in range(8):
            m.call('DAT_Set', 14, 30, color)
            self.assertEqual(m.call('DAT_Get', 14, 30), color)
        m.call('DAT_Set', 17, 18, 1)
        self.assertEqual(m.call('DAT_Get', 14, 30), 7)
        self.assertEqual(m.record(), core)
        saved = bytes(m.cpu.mem_read(EXT, 92))
        fixture = fixtures.RelocatedDATSettingsTests(); fixture.setUp()
        fixture.m.put(core); fixture.m.cpu.mem_write(EXT, saved)
        self.assertEqual(tuple(fixture.m.call('DAT_Get', 27, i) for i in range(8)),
                         tuple(count - 1 for count in COUNTS))
        self.assertEqual(fixture.m.call('DAT_Get', 14, 30), 7)

if __name__ == '__main__': unittest.main()
