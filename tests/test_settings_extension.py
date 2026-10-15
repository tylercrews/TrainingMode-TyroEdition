"""Exercise the independently versioned score-region preference extension."""
from pathlib import Path
import struct
import unittest
import test_settings as fixtures
from test_settings import Machine, RECORD, ROOT

EXT = 0x8045A6C0 + 0x1AE0
COUNTS = (9, 2, 2, 3, 3, 3, 4, 2, 8, 4, 4)
DEFAULTS = (0, 0, 1, 0, 0, 0, 0, 0, 7, 0, 0)
LEDGE_COUNTS = (4, 2, 7, 6, 5, 8, 5)
LEDGE_DEFAULTS = (0, 0, 0, 0, 1, 7, 0)
FIELDS = ((27, COUNTS, DEFAULTS), (28, LEDGE_COUNTS, LEDGE_DEFAULTS), (29, (2,2), (0,0)))
REGION_SIZE, OWNED_SIZE = 88, 16

class ExtensionTests(unittest.TestCase):
    def setUp(self):
        self.m = Machine()
        self.m.init()
        self.m.cpu.mem_write(EXT - 4, b'\xA5' * 100)
        self.m.call('TestClearDirty')

    def test_absent_defaults_are_read_only_and_first_edit_only_owns_sixteen_bytes(self):
        m = self.m
        before = bytes(m.cpu.mem_read(EXT - 4, 100))
        core = m.record()
        for field, counts, defaults in FIELDS:
            self.assertEqual(tuple(m.call('Settings_Get', field, i) for i in range(len(counts))), defaults)
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
        self.assertEqual(bytes(m.cpu.mem_read(EXT + OWNED_SIZE, 80)), b'\xA5' * 80)
        self.assertEqual(bytes(m.cpu.mem_read(EXT, 8)), b'TYEX\x05\x08\xA5\x5A')

    def test_every_value_and_invalid_write_preserve_neighbors_and_reloaded_preferences(self):
        m = self.m
        for field, counts, defaults in FIELDS:
            for index, count in enumerate(counts):
                for value in range(count):
                    before = {(f, i): m.call('Settings_Get', f, i)
                              for f, options, _ in FIELDS for i in range(len(options))}
                    m.call('Settings_Set', field, index, value)
                    self.assertEqual(m.call('Settings_Get', field, index), value)
                    for (other_field, other_index), old in before.items():
                        if (other_field, other_index)!=(field,index):
                            self.assertEqual(m.call('Settings_Get', other_field, other_index), old)
                snapshot = bytes(m.cpu.mem_read(EXT, REGION_SIZE + 8))
                for value in (count, 0xFFFFFFFF):
                    m.call('Settings_Set', field, index, value)
                    self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE + 8)), snapshot)
            for index in (len(counts), 0xFFFFFFFF):
                m.call('Settings_Set', field, index, 1)
                self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE + 8)), snapshot)
        before = bytes(m.cpu.mem_read(EXT, REGION_SIZE + 8))
        fresh = Machine(); fresh.put(m.record()); fresh.cpu.mem_write(EXT, before)
        for field, counts, _ in FIELDS:
            self.assertEqual(tuple(fresh.call('Settings_Get', field, i) for i in range(len(counts))),
                             tuple(count - 1 for count in counts))

    def test_new_osd_palette_and_editor_leave_core_palette_bytes_unchanged(self):
        m = self.m
        core = m.record()
        for color in range(8):
            m.call('Settings_Set', 14, 30, color)
            self.assertEqual(m.call('Settings_Get', 14, 30), color)
            self.assertEqual(bool(m.call('Settings_Get', 0, 0) & (1 << 30)), bool(color))
            self.assertEqual(m.record(), core)
        m.call('Settings_Set', 14, 30, 7)
        m.call('Settings_Set', 17, 16, 1)  # Native Boolean exit pass retains chosen palette.
        self.assertEqual(m.call('Settings_Get', 14, 30), 7)
        m.call('Settings_Set', 17, 16, 0)
        self.assertEqual(m.call('Settings_Get', 14, 30), 0)
        m.call('TestEditorInit'); m.call('TestEditorInput', 0x200, 19)
        self.assertEqual(m.call('Settings_Get', 14, 30), 1)
        self.assertEqual(m.record(), core)
        m.call('Settings_Set', 0, 0, m.call('Settings_Get', 0, 0) & ~(1 << 30))
        self.assertEqual(m.call('Settings_Get', 14, 30), 0)

    def test_foreign_future_core_and_future_extension_are_private_and_byte_preserving(self):
        for mode in ('foreign', 'core', 'extension'):
            with self.subTest(mode=mode):
                m = Machine(); m.init()
                m.cpu.mem_write(EXT, b'TYEX\x06\x08\xA5\x5A' + bytes(REGION_SIZE))
                if mode == 'foreign': m.cpu.mem_write(0x80000000, b'GTME01')
                elif mode == 'core':
                    core = bytearray(m.record()); core[10] &= 0x3F; m.put(core)
                before = bytes(m.cpu.mem_read(EXT, 92)); core = m.record()
                m.call('Settings_Set', 27, 0, 8); m.call('Settings_Set', 14, 30, 6)
                m.call('Settings_Set', 28, 4, 4)
                self.assertEqual(m.call('Settings_Get', 27, 0), 8)
                self.assertEqual(m.call('Settings_Get', 14, 30), 6)
                self.assertEqual(m.call('Settings_Get', 28, 4), 4)
                self.assertEqual(bytes(m.cpu.mem_read(EXT, 92)), before)
                self.assertEqual(m.record(), core)

    def test_extension_repair_preserves_padding_and_other_preferences(self):
        m = self.m
        m.call('Settings_Set', 27, 0, 8)
        m.cpu.mem_write(EXT + 8, b'\xFD\xFF\xFF\xA5\xFF\xFF\xFF\xA5')
        before = bytes(m.cpu.mem_read(EXT, OWNED_SIZE))
        m.call('Settings_Get', 27, 0)
        after = bytes(m.cpu.mem_read(EXT, OWNED_SIZE))
        self.assertEqual(after[:9], before[:9])
        self.assertEqual(after[10] & 0x80, 0x80)
        self.assertEqual(after[11], 0xA5)
        self.assertEqual(after[14]&0xF8, 0xF8)
        self.assertEqual(after[15], 0xA5)
        self.assertEqual(m.call('Settings_Get', 27, 0), 0)
        self.assertEqual(m.call('Settings_Get', 14, 30), 5)
        self.assertEqual(m.call('Settings_Get', 28, 2), 0)
        self.assertEqual(m.call('Settings_Get', 28, 3), 0)
        self.assertEqual(m.call('Settings_Get', 28, 4), 1)

    def test_v1_migration_preserves_osd_tech_and_padding_and_initializes_new_preferences(self):
        m = self.m
        legacy = b'TYEX\x01\x04\xA5\x5A\xFD\x98\xFA\xA5'
        m.cpu.mem_write(EXT, legacy + b'\xE5' * (REGION_SIZE - len(legacy)))
        core = m.record()
        m.cpu.mem_write(0x80433330, struct.pack('>I', 1))
        m.cpu.mem_write(0x80433374, struct.pack('>I', 0x80407000))
        m.cpu.mem_write(0x80432A68, struct.pack('>2I', 0x80409000, 0x8040A000))
        self.assertEqual(m.call('Settings_Get', 14, 30), 5)
        self.assertEqual(tuple(m.call('Settings_Get', 27, i) for i in range(len(COUNTS))),
                         (8, 1, 0, 2, 2, 2, 3, 1, 7, 0, 0))
        self.assertEqual(tuple(m.call('Settings_Get', 28, i) for i in range(len(LEDGE_COUNTS))), LEDGE_DEFAULTS)
        self.assertEqual(bytes(m.cpu.mem_read(EXT, OWNED_SIZE)),
                         b'TYEX\x05\x08\xA5\x5A\xFD\x98\xFA\x87\x00\x72\x00\x00')
        self.assertEqual(bytes(m.cpu.mem_read(EXT + OWNED_SIZE, REGION_SIZE - OWNED_SIZE)),
                         b'\xE5' * (REGION_SIZE - OWNED_SIZE))
        self.assertEqual(m.record(), core)
        self.assertEqual(m.call('TestDirty'), 1)
        m.call('TestClearDirty')
        m.call('Settings_Get', 28, 0)
        self.assertEqual(m.call('TestDirty'), 0)  # A completed migration is not queued repeatedly.

    def test_v1_foreign_and_future_core_records_do_not_migrate(self):
        for foreign in (False, True):
            m = Machine(); m.init()
            legacy = b'TYEX\x01\x04\xA5\x5A\xFD\x98\xFA\xA5' + b'\xE5' * 80
            m.cpu.mem_write(EXT, legacy)
            if foreign: m.cpu.mem_write(0x80000000, b'GTME01')
            else:
                core = bytearray(m.record()); core[10]&=0x3F; m.put(core)
            m.call('Settings_Get', 28, 0)
            m.call('Settings_Set', 27, 8, 1)
            self.assertEqual(bytes(m.cpu.mem_read(EXT, len(legacy))), legacy)

    def test_v2_migration_preserves_actions_and_other_bits_without_expanding_record(self):
        for action in range(4):
            m = Machine(); m.init()
            # Valid previous preferences, with canaries in every unclaimed bit.
            legacy = bytearray(b'TYEX\x02\x08\xA5\x5A\xFD\x98\xFA\xA7\x00\x72\xFC\xA5')
            bits = int.from_bytes(legacy[12:15], 'little')
            legacy[12:15] = ((bits & ~(3 << 15)) | (action << 15)).to_bytes(3, 'little')
            m.cpu.mem_write(EXT, bytes(legacy) + b'\xE5' * (REGION_SIZE - OWNED_SIZE))
            core = m.record()
            self.assertEqual(m.call('Settings_Get', 28, 6), action + 1)
            after = bytes(m.cpu.mem_read(EXT, REGION_SIZE))
            self.assertEqual(after[:4], legacy[:4])
            self.assertEqual(after[4], 5)
            self.assertEqual(after[5:11], legacy[5:11])
            self.assertEqual(after[11], legacy[11] & 0x87)
            mask = ~((7 << 15) | (3 << 18)) & 0xFFFFFF
            self.assertEqual(int.from_bytes(after[12:15], 'little') & mask, bits & mask)
            self.assertEqual(after[15], legacy[15])
            self.assertEqual(after[16:], b'\xE5' * (REGION_SIZE - OWNED_SIZE))
            self.assertEqual(m.record(), core)
            m.call('Settings_Set', 28, 6, 0)
            self.assertEqual(m.call('Settings_Get', 28, 6), 0)

    def test_unsupported_version_size_and_marker_remain_byte_preserving(self):
        for version, size, marker in ((0,8,0xA5), (6,8,0xA5), (2,4,0xA5), (1,8,0xA5), (2,8,0)):
            m = Machine(); m.init()
            unknown = b'TYEX' + bytes((version,size,marker,0x5A)) + bytes(range(80))
            m.cpu.mem_write(EXT, unknown)
            for field, counts, defaults in FIELDS:
                self.assertEqual(tuple(m.call('Settings_Get',field,i) for i in range(len(counts))), defaults)
                m.call('Settings_Set',field,0,counts[0]-1)
            self.assertEqual(bytes(m.cpu.mem_read(EXT, len(unknown))), unknown)

    def test_v3_migration_keeps_current_actions_and_clears_only_newly_owned_padding(self):
        for action in range(5):
            m=Machine();m.init()
            legacy=bytearray(b'TYEX\x03\x08\xA5\x5A\xFD\x98\xFA\xFF\x00\x72\xFF\xA5')
            bits=int.from_bytes(legacy[12:15],'little')
            legacy[12:15]=((bits&~(7<<15))|(action<<15)).to_bytes(3,'little')
            m.cpu.mem_write(EXT,bytes(legacy)+b'\xE5'*(REGION_SIZE-OWNED_SIZE))
            self.assertEqual(m.call('Settings_Get',28,6),action)
            self.assertEqual(tuple(m.call('Settings_Get',27,i) for i in (9,10)),(0,0))
            self.assertEqual(m.call('Settings_Get',29,0),0)
            after=bytes(m.cpu.mem_read(EXT,REGION_SIZE))
            expected=bytearray(legacy);expected[4]=5;expected[11]&=0x87;expected[14]&=0xF3
            self.assertEqual(after,bytes(expected)+b'\xE5'*(REGION_SIZE-OWNED_SIZE))

    def test_v4_migration_preserves_particle_height_distance_and_clears_score_marker(self):
        m=self.m
        m.call('Settings_Set',29,0,1)
        m.call('Settings_Set',27,9,3);m.call('Settings_Set',27,10,2)
        legacy=bytearray(m.cpu.mem_read(EXT,REGION_SIZE))
        legacy[4]=4;legacy[14]|=8  # Former padding must not identify a count record.
        m.cpu.mem_write(EXT,bytes(legacy))
        self.assertEqual(m.call('Settings_Get',29,1),0)
        self.assertEqual(m.call('Settings_Get',29,0),1)
        self.assertEqual((m.call('Settings_Get',27,9),m.call('Settings_Get',27,10)),(3,2))
        expected=bytearray(legacy);expected[4]=5;expected[14]&=0xF7
        self.assertEqual(bytes(m.cpu.mem_read(EXT,REGION_SIZE)),bytes(expected))

class RelocatedExtensionTests(unittest.TestCase):
    dat_path = fixtures.RelocatedDATSettingsTests.dat_path
    def setUp(self): fixtures.RelocatedDATSettingsTests.setUp(self)
    def test_extension_roundtrip_in_optimized_production_dat(self):
        m = self.m
        m.init()
        core = m.record()
        for field, counts, _ in FIELDS:
            for index, count in enumerate(counts): m.call('DAT_Set', field, index, count - 1)
        for color in range(8):
            m.call('DAT_Set', 14, 30, color)
            self.assertEqual(m.call('DAT_Get', 14, 30), color)
        m.call('DAT_Set', 17, 16, 1)
        self.assertEqual(m.call('DAT_Get', 14, 30), 7)
        self.assertEqual(m.record(), core)
        saved = bytes(m.cpu.mem_read(EXT, REGION_SIZE))
        fixture = fixtures.RelocatedDATSettingsTests(); fixture.setUp()
        fixture.m.put(core); fixture.m.cpu.mem_write(EXT, saved)
        for field, counts, _ in FIELDS:
            self.assertEqual(tuple(fixture.m.call('DAT_Get', field, i) for i in range(len(counts))),
                             tuple(count - 1 for count in counts))
        self.assertEqual(fixture.m.call('DAT_Get', 14, 30), 7)

    def test_v1_migration_uses_real_production_dat_relocations(self):
        m = self.m; m.init()
        legacy = b'TYEX\x01\x04\xA5\x5A\x06\x98\x7A\xA0'
        m.cpu.mem_write(EXT, legacy + b'\xE5' * (REGION_SIZE - len(legacy)))
        core = m.record()
        self.assertEqual(m.call('DAT_Get',14,30), 6)
        self.assertEqual(tuple(m.call('DAT_Get',27,i) for i in range(len(COUNTS))),
                         (8,1,0,2,2,2,3,1,7,0,0))
        self.assertEqual(tuple(m.call('DAT_Get',28,i) for i in range(len(LEDGE_COUNTS))), LEDGE_DEFAULTS)
        self.assertEqual(bytes(m.cpu.mem_read(EXT,8)), b'TYEX\x05\x08\xA5\x5A')
        self.assertEqual(bytes(m.cpu.mem_read(EXT+OWNED_SIZE,REGION_SIZE-OWNED_SIZE)),
                         b'\xE5'*(REGION_SIZE-OWNED_SIZE))
        self.assertEqual(m.record(),core)

if __name__ == '__main__': unittest.main()
