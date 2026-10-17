"""Exercise the independently versioned score-region preference extension."""
from pathlib import Path
import struct
import unittest
import test_settings as fixtures
from test_settings import Machine, RECORD, ROOT

EXT = 0x8045A6C0 + 0x1AE0
COUNTS = (9, 2, 2, 3, 3, 3, 4, 2, 8, 4, 4)
DEFAULTS = (0, 0, 1, 0, 0, 0, 0, 0, 7, 0, 0)
LEDGE_COUNTS = (4, 2, 7, 15, 5, 8, 33, 12, 3, 3)
LEDGE_DEFAULTS = (0, 0, 0, 0, 1, 7, 0, 0, 2, 0)
PS_COUNTS = (4,61,61,5,2,4,2,2,2,2)
PS_DEFAULTS = (0,0,20,0,0,0,1,1,0,1)
SHIELD_DEFAULT_BYTES = bytes.fromhex("00000100")
SHIELD_FIELDS = ((33,(17,4,2,2,2),(0,0,0,0,0)), (34,(15,5,2,4,2),(0,0,1,0,0)))
PS_DEFAULT_BYTES = bytes.fromhex("0014B000")

def upgraded(raw):
    value = bytearray(raw)
    value[4:6] = bytes((12,16))
    value[16:20] = PS_DEFAULT_BYTES
    value[20:24] = SHIELD_DEFAULT_BYTES
    preset = (int.from_bytes(value[12:16], 'little') >> 6) & 7
    value[19] = preset if preset < 7 else 0
    value[14] = (value[14] & 0x3F) | 0x80
    value[15] &= 0x3F
    return bytes(value)

FIELDS = ((27, COUNTS, DEFAULTS), (28, LEDGE_COUNTS, LEDGE_DEFAULTS), (29, (2,2), (0,0)), (30, (2,2), (1,0)), (31, PS_COUNTS, PS_DEFAULTS))
REGION_SIZE, OWNED_SIZE = 88, 24

class ExtensionTests(unittest.TestCase):
    def setUp(self):
        self.m = Machine()
        self.m.init()
        self.m.cpu.mem_write(EXT - 4, b'\xA5' * 100)
        self.m.call('TestClearDirty')

    def test_absent_defaults_are_read_only_and_first_edit_only_owns_twenty_bytes(self):
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
        self.assertEqual(bytes(m.cpu.mem_read(EXT + OWNED_SIZE, 72)), b'\xA5' * 72)
        self.assertEqual(bytes(m.cpu.mem_read(EXT, 8)), b'TYEX\x0C\x10\xA5\x5A')

    def test_every_value_and_invalid_write_preserve_neighbors_and_reloaded_preferences(self):
        m = self.m
        for field, counts, defaults in FIELDS + SHIELD_FIELDS:
            for index, count in enumerate(counts):
                for value in range(count):
                    before = {(f, i): m.call('Settings_Get', f, i)
                              for f, options, _ in FIELDS + SHIELD_FIELDS for i in range(len(options))}
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
        for field, counts, _ in FIELDS + SHIELD_FIELDS:
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
        m.call('Settings_Set', 17, 6, 1)  # Native Boolean exit pass (A-Smash physical row 6) retains chosen palette.
        self.assertEqual(m.call('Settings_Get', 14, 30), 7)
        m.call('Settings_Set', 17, 6, 0)
        self.assertEqual(m.call('Settings_Get', 14, 30), 0)
        m.call('TestEditorInit'); m.call('TestEditorInput', 0x200, 6)  # A-Smash Release row
        self.assertEqual(m.call('Settings_Get', 14, 30), 1)
        self.assertEqual(m.record(), core)
        m.call('Settings_Set', 0, 0, m.call('Settings_Get', 0, 0) & ~(1 << 30))
        self.assertEqual(m.call('Settings_Get', 14, 30), 0)

    def test_foreign_future_core_and_future_extension_are_private_and_byte_preserving(self):
        for mode in ('foreign', 'core', 'extension'):
            with self.subTest(mode=mode):
                m = Machine(); m.init()
                m.cpu.mem_write(EXT, b'TYEX\x08\x08\xA5\x5A' + bytes(REGION_SIZE))
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
        self.assertEqual(after[14]&0xF8, 0xB8)
        self.assertEqual(after[15], 0x80)
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
                         b'TYEX\x0C\x10\xA5\x5A\xFD\x98\xFA\x87\x00\x72\x90\x00' + PS_DEFAULT_BYTES + SHIELD_DEFAULT_BYTES)
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

    def test_v2_migration_preserves_old_actions_and_initializes_appended_bytes(self):
        for action in range(4):
            m = Machine(); m.init()
            # Valid previous preferences, with canaries in every unclaimed bit.
            legacy = bytearray(b'TYEX\x02\x08\xA5\x5A\xFD\x98\xFA\xA7\x00\x72\xFC\xA5')
            bits = int.from_bytes(legacy[12:15], 'little')
            legacy[12:15] = ((bits & ~(3 << 15)) | (action << 15)).to_bytes(3, 'little')
            m.cpu.mem_write(EXT, bytes(legacy) + b'\xE5' * (REGION_SIZE - len(legacy)))
            core = m.record()
            self.assertEqual(m.call('Settings_Get', 28, 6), (1,13,31,11)[action])
            after = bytes(m.cpu.mem_read(EXT, REGION_SIZE))
            self.assertEqual(after[:4], legacy[:4])
            self.assertEqual(after[4:6], bytes((12,16)))
            self.assertEqual(after[6:11], legacy[6:11])
            self.assertEqual(after[11], legacy[11] & 0x87)
            mask = ~((15 << 18) | (3 << 22)) & 0xFFFFFF
            self.assertEqual(int.from_bytes(after[12:15], 'little') & mask, int.from_bytes(legacy[12:15], 'little') & mask)
            self.assertEqual(after[15], (1,13,31,11)[action])
            self.assertEqual(after[16:20], PS_DEFAULT_BYTES)
            self.assertEqual(after[OWNED_SIZE:], b'\xE5' * (REGION_SIZE - OWNED_SIZE))
            self.assertEqual(m.record(), core)
            m.call('Settings_Set', 28, 6, 0)
            self.assertEqual(m.call('Settings_Get', 28, 6), 0)

    def test_unsupported_version_size_and_marker_remain_byte_preserving(self):
        for version, size, marker in ((0,8,0xA5), (8,8,0xA5), (2,4,0xA5), (1,8,0xA5), (2,8,0)):
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
            m.cpu.mem_write(EXT,bytes(legacy)+b'\xE5'*(REGION_SIZE-len(legacy)))
            self.assertEqual(m.call('Settings_Get',28,6),(0,1,13,31,11)[action])
            self.assertEqual(tuple(m.call('Settings_Get',27,i) for i in (9,10)),(0,0))
            self.assertEqual(m.call('Settings_Get',29,0),0)
            after=bytes(m.cpu.mem_read(EXT,REGION_SIZE))
            expected=bytearray(legacy);expected[4]=7;expected[11]&=0x87;expected[14]=(expected[14]&0xC3)|0x10;expected[15]=(expected[15]&0xC0)|(0,1,13,31,11)[action]
            self.assertEqual(after,upgraded(expected)+b'\xE5'*(REGION_SIZE-OWNED_SIZE))

    def test_v4_migration_preserves_particle_height_distance_and_clears_score_marker(self):
        m=self.m
        m.call('Settings_Set',29,0,1)
        m.call('Settings_Set',27,9,3);m.call('Settings_Set',27,10,2)
        legacy=bytearray(m.cpu.mem_read(EXT,REGION_SIZE))
        legacy[4]=4;legacy[5]=8;legacy[14]|=8  # Former padding must not identify a count record.
        m.cpu.mem_write(EXT,bytes(legacy))
        self.assertEqual(m.call('Settings_Get',29,1),0)
        self.assertEqual(m.call('Settings_Get',29,0),1)
        self.assertEqual((m.call('Settings_Get',27,9),m.call('Settings_Get',27,10)),(3,2))
        expected=bytearray(legacy);expected[4]=7;expected[14]=(expected[14]&0xC7)|0x10
        self.assertEqual(bytes(m.cpu.mem_read(EXT,REGION_SIZE)),upgraded(expected))

    def test_v5_migration_preserves_all_existing_preferences_and_initializes_laser_toggles(self):
        m = self.m
        for field, counts, _ in FIELDS:
            if field >= 30: continue
            for index, count in enumerate(counts): m.call('Settings_Set', field, index, count - 1)
        legacy = bytearray(m.cpu.mem_read(EXT, REGION_SIZE))
        # Build a valid v5 action and preset in their original locations.
        legacy[4] = 5; legacy[5] = 8
        bits = int.from_bytes(legacy[12:15], "little")
        bits = (bits & ~((7 << 15) | (7 << 6))) | (4 << 15) | (5 << 6)
        legacy[12:15] = bits.to_bytes(3, "little")
        legacy[14] |= 0x30  # Former padding does not own the new toggles.
        m.cpu.mem_write(EXT, bytes(legacy))
        self.assertEqual((m.call('Settings_Get',30,0), m.call('Settings_Get',30,1)), (1,0))
        expected = bytearray(legacy); expected[4] = 7; expected[14] = (expected[14] & ~0x30) | 0x10; expected[15] = (expected[15] & 0xC0) | 11
        self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE)), upgraded(expected))
        self.assertEqual(m.call('Settings_Get',29,1), 1)  # Under Fire count marker survives.
        for field, counts, _ in FIELDS:
            if field >= 30: continue
            self.assertEqual(tuple(m.call('Settings_Get',field,i) for i in range(len(counts))),
                             tuple(5 if field == 28 and i == 3 else 11 if field == 28 and i == 6 else 0 if field == 28 and i in (7,9) else 2 if field == 28 and i == 8 else count - 1
                                   for i, count in enumerate(counts)))

    def test_v6_migration_preserves_all_other_preferences_and_spare_bits(self):
        for old_action, new_action in enumerate((0,1,13,31,11)):
            m = Machine(); m.init()
            for field, counts, _ in FIELDS:
                for index, count in enumerate(counts):
                    m.call("Settings_Set", field, index, min(count - 1, 5) if field == 28 and index == 3 else count - 1)
            legacy = bytearray(m.cpu.mem_read(EXT, REGION_SIZE))
            legacy[4] = 6; legacy[5] = 8
            bits = int.from_bytes(legacy[12:15], "little")
            legacy[12:15] = ((bits & ~((7 << 15) | (7 << 6))) | old_action << 15 | 5 << 6).to_bytes(3, "little")
            legacy[15] = 0xFF
            m.cpu.mem_write(EXT, bytes(legacy))
            self.assertEqual(m.call("Settings_Get", 28, 6), new_action)
            expected = bytearray(legacy); expected[4] = 7; expected[15] = 0xC0 | new_action
            self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE)), upgraded(expected))

    def test_v7_migration_preserves_every_existing_setting_and_claims_only_four_bytes(self):
        m = self.m
        for field, counts, _ in FIELDS:
            if field == 31: continue
            for index, count in enumerate(counts): m.call('Settings_Set',field,index,count-1)
        legacy = bytearray(m.cpu.mem_read(EXT,REGION_SIZE))
        legacy[4:6] = bytes((7,8))
        bits = int.from_bytes(legacy[12:15], 'little')
        legacy[12:15] = ((bits & ~(7 << 6)) | 6 << 6).to_bytes(3, 'little')
        legacy[16:] = b'\xE5' * (REGION_SIZE-16)
        m.cpu.mem_write(EXT,bytes(legacy))
        core = m.record()
        self.assertEqual(tuple(m.call('Settings_Get',31,i) for i in range(10)),PS_DEFAULTS)
        self.assertEqual(bytes(m.cpu.mem_read(EXT,REGION_SIZE)),upgraded(legacy))
        for field, counts, _ in FIELDS:
            if field == 31: continue
            self.assertEqual(tuple(m.call('Settings_Get',field,i) for i in range(len(counts))),
                tuple(6 if field == 28 and i == 3 else 0 if field == 28 and i in (7,9) else 2 if field == 28 and i == 8 else n-1 for i,n in enumerate(counts)))
        self.assertEqual(m.record(),core)

    def test_opaque_future_extension_cannot_be_claimed_by_powershield(self):
        m = self.m
        future = b'TYEX\x0B\x0C\xA5\x5A' + bytes(range(80))
        m.cpu.mem_write(EXT,future)
        for index, count in enumerate(PS_COUNTS): m.call('Settings_Set',31,index,count-1)
        self.assertEqual(bytes(m.cpu.mem_read(EXT,88)),future)

    def test_v8_migration_preserves_powershield_and_all_old_presets_and_defaults_aerial(self):
        for preset in range(7):
            m = Machine(); m.init()
            for field, counts, _ in FIELDS:
                for index, count in enumerate(counts): m.call('Settings_Set', field, index, count-1)
            old = bytearray(m.cpu.mem_read(EXT, REGION_SIZE))
            old[4:6] = bytes((8,12))
            bits = int.from_bytes(old[12:16], 'little')
            old[12:16] = ((bits & ~(7 << 6)) | preset << 6).to_bytes(4, 'little')
            old[19] = 0xFF  # Unowned v8 byte cannot turn on aerial attacks.
            m.cpu.mem_write(EXT, bytes(old))
            self.assertEqual(m.call('TMExtension_Read', EXT, 28, 3), preset)
            self.assertEqual(m.call('TMExtension_Read', EXT, 28, 7), 0)
            self.assertEqual(tuple(m.call('TMExtension_Read', EXT, 31, i) for i in range(len(PS_COUNTS))),
                             tuple(n-1 for n in PS_COUNTS))
            self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE)), bytes(old))
            self.assertEqual(m.call('Settings_Get', 28, 3), preset)
            expected = bytearray(old); expected[4:6] = bytes((12,16)); expected[20:24] = SHIELD_DEFAULT_BYTES; expected[19] = preset
            expected[14] = (expected[14] & 0x3F) | 0x80; expected[15] &= 0x3F
            self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE)), bytes(expected))
            self.assertEqual(m.call('Settings_Get', 28, 7), 0)

    def test_new_ledge_fields_repair_without_touching_powershield_or_old_storage(self):
        m = self.m
        m.call('Settings_Set', 31, 0, 3)
        m.cpu.mem_write(EXT+19, b'\xFF')
        before = bytes(m.cpu.mem_read(EXT, OWNED_SIZE))
        self.assertEqual(m.call('Settings_Get', 28, 3), 0)
        self.assertEqual(m.call('Settings_Get', 28, 7), 0)
        after = bytes(m.cpu.mem_read(EXT, OWNED_SIZE))
        self.assertEqual(after[:19], before[:19])
        self.assertEqual(after[19], 0)

    def test_v9_migration_keeps_all_fourteen_presets_aerials_and_powershield(self):
        for preset in range(14):
            m = Machine(); m.init()
            for field, counts, _ in FIELDS:
                for index, count in enumerate(counts): m.call('Settings_Set', field, index, count-1)
            old = bytearray(m.cpu.mem_read(EXT, REGION_SIZE))
            old[4:6] = bytes((9,12))
            old[14] |= 0xC0; old[15] |= 0xC0  # Former padding must not select entry/height.
            old[19] = 0xB0 | preset
            m.cpu.mem_write(EXT, bytes(old))
            self.assertEqual(m.call('TMExtension_Read', EXT, 28, 3), preset)
            self.assertEqual(m.call('TMExtension_Read', EXT, 28, 7), 11)
            self.assertEqual((m.call('TMExtension_Read', EXT, 28, 8),m.call('TMExtension_Read', EXT, 28, 9)), (2,0))
            self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE)), bytes(old))
            self.assertEqual(m.call('Settings_Get', 28, 3), preset)
            expected = bytearray(old); expected[4:6] = bytes((12,16)); expected[20:24] = SHIELD_DEFAULT_BYTES
            expected[14] = (expected[14] & 0x3F) | 0x80; expected[15] &= 0x3F
            self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE)), bytes(expected))
            self.assertEqual(m.call('Settings_Get', 28, 7), 11)
            self.assertEqual(tuple(m.call('Settings_Get',31,i) for i in range(len(PS_COUNTS))),tuple(n-1 for n in PS_COUNTS))

    def test_invalid_v9_preset_does_not_turn_into_new_preset_during_migration(self):
        m = self.m
        m.call('Settings_Set', 28, 3, 14)
        old = bytearray(m.cpu.mem_read(EXT, REGION_SIZE)); old[4:6] = bytes((9,12))
        m.cpu.mem_write(EXT, bytes(old))
        self.assertEqual(m.call('TMExtension_Read', EXT, 28, 3), 0)
        self.assertEqual(m.call('Settings_Get', 28, 3), 0)

    def test_v10_migration_preserves_all_owned_bytes_and_initializes_only_new_word(self):
        m=self.m
        for field, counts, _ in FIELDS:
            for index, count in enumerate(counts): m.call('Settings_Set',field,index,count-1)
        old=bytearray(m.cpu.mem_read(EXT,REGION_SIZE))
        old[4:6]=bytes((10,12)); old[20:24]=b'\xFF'*4
        m.cpu.mem_write(EXT,bytes(old))
        for field, counts, defaults in SHIELD_FIELDS:
            self.assertEqual(tuple(m.call('TMExtension_Read',EXT,field,i) for i in range(len(counts))),defaults)
        self.assertEqual(bytes(m.cpu.mem_read(EXT,REGION_SIZE)),bytes(old))
        self.assertEqual(m.call('Settings_Get',28,8),2)
        expected=bytearray(old);expected[4:6]=bytes((12,16));expected[20:24]=SHIELD_DEFAULT_BYTES
        self.assertEqual(bytes(m.cpu.mem_read(EXT,REGION_SIZE)),bytes(expected))
        for field, counts, _ in FIELDS:
            self.assertEqual(tuple(m.call('Settings_Get',field,i) for i in range(len(counts))),tuple(n-1 for n in counts))
        for field, counts, defaults in SHIELD_FIELDS:
            self.assertEqual(tuple(m.call('Settings_Get',field,i) for i in range(len(counts))),defaults)

    def test_v11_migration_keeps_actions_and_other_preferences_and_claims_only_two_padding_bits(self):
        for action in range(16):
            m=Machine();m.init()
            for field,counts,_ in FIELDS+SHIELD_FIELDS:
                for index,count in enumerate(counts):m.call('Settings_Set',field,index,count-1)
            old=bytearray(m.cpu.mem_read(EXT,REGION_SIZE))
            old[4:6]=bytes((11,16))
            word=int.from_bytes(old[20:24],'little')
            old[20:24]=((word&~(15<<9))|(action<<9)|0xFFF80000).to_bytes(4,'little')
            m.cpu.mem_write(EXT,bytes(old))
            self.assertEqual(m.call('TMExtension_Read',EXT,34,0),action if action<10 else 0)
            self.assertEqual(m.call('TMExtension_Read',EXT,34,4),0)
            self.assertEqual(bytes(m.cpu.mem_read(EXT,REGION_SIZE)),bytes(old))
            self.assertEqual(m.call('Settings_Get',34,0),action if action<10 else 0)
            expected=bytearray(old);expected[4]=12
            expected[20]=(expected[20]&0xF0) # Version-11 invalid attack padding repairs to choice zero.
            word=int.from_bytes(expected[20:24],'little')&~((1<<19)|(1<<20))
            if action>=10:word&=~(15<<9)
            expected[20:24]=word.to_bytes(4,'little')
            self.assertEqual(bytes(m.cpu.mem_read(EXT,REGION_SIZE)),bytes(expected))
            self.assertEqual(tuple(m.call('Settings_Get',33,i) for i in range(5)),(0,3,1,1,1))
            self.assertEqual(tuple(m.call('Settings_Get',34,i) for i in range(1,5)),(4,1,3,0))

    def test_v11_reversal_migration_keeps_fourteen_choices_and_rejects_old_invalid_values(self):
        for attack in range(16):
            m=Machine();m.init();m.call('Settings_Set',33,1,3)
            old=bytearray(m.cpu.mem_read(EXT,REGION_SIZE));old[4]=11
            old[20]=(old[20]&0xF0)|attack
            old[22]|=0xF8;old[23]=0xA5
            m.cpu.mem_write(EXT,bytes(old))
            expected=attack if attack<14 else 0
            self.assertEqual(m.call('TMExtension_Read',EXT,33,0),expected)
            self.assertEqual(bytes(m.cpu.mem_read(EXT,REGION_SIZE)),bytes(old))
            self.assertEqual(m.call('Settings_Get',33,0),expected)
            after=bytes(m.cpu.mem_read(EXT,REGION_SIZE))
            self.assertEqual(after[22],old[22]&~0x18)
            self.assertEqual(after[23:],bytes(old[23:]))
            self.assertEqual(m.call('Settings_Get',33,1),3)


class RelocatedExtensionTests(unittest.TestCase):
    dat_path = fixtures.RelocatedDATSettingsTests.dat_path
    def setUp(self): fixtures.RelocatedDATSettingsTests.setUp(self)
    def test_extension_roundtrip_in_optimized_production_dat(self):
        m = self.m
        m.init()
        core = m.record()
        for field, counts, _ in FIELDS + SHIELD_FIELDS:
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
        for field, counts, _ in FIELDS + SHIELD_FIELDS:
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
        self.assertEqual(bytes(m.cpu.mem_read(EXT,8)), b'TYEX\x0C\x10\xA5\x5A')
        self.assertEqual(bytes(m.cpu.mem_read(EXT+OWNED_SIZE,REGION_SIZE-OWNED_SIZE)),
                         b'\xE5'*(REGION_SIZE-OWNED_SIZE))
        self.assertEqual(m.record(),core)

if __name__ == '__main__': unittest.main()
