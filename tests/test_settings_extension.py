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
CURRENT, PAYLOAD = 19, 60  # TM_EXTENSION_VERSION, TM_EXTENSION_SIZE - 8
HEADER = b'TYEX\x13\x3C\xA5\x5A'
# Bytes 52..63 appended by version 18: Edgeguard's three words at their defaults
# (Custom Hit Strength 100-120 / 45-60 / 50-80%, player 0%, Normal, and each
# CPU's default recovery toggles).
EDGEGUARD = 39  # TM_SETTING_EDGEGUARD
# Recovery masks: Fox, Falco, Sheik, Falcon, Marth, then (word 64, version 19)
# Yoshi, Peach, Luigi, Dr. Mario, Ice Climbers, Jigglypuff, Samus. Then Player
# Position (At Ledge / Close / Halfway / Mid Stage) in word 60's spare bits 30-31.
EDGEGUARD_COUNTS = (301, 301, 91, 91, 999, 1000, 1000, 5, 64, 64, 64, 32, 16, 32, 32, 32, 32, 8, 16, 32, 4)
EDGEGUARD_DEFAULTS = (100, 120, 45, 60, 0, 50, 80, 1, 1, 1, 7, 3, 3, 7, 7, 7, 3, 3, 15, 15, 1)
EG_COUNT = len(EDGEGUARD_COUNTS)
EG_FIRST_NEW = 13  # TM_EG_RECOVERY_YOSHI: the first index added after version 18
EG_POSITION = 20  # TM_EG_PLAYER_POSITION
EDGEGUARD_BYTES = bytes.fromhex('64f0b47800c800050982630c')
EDGEGUARD2_BYTES = bytes.fromhex('e79cb17f')
# Bytes 24..67 appended by versions 15 to 19: one color nibble per OSD (all
# original colors), Low Laser's byte 35, the Fixed Grid cells (all Auto), the
# human/CPU costumes (Default) and the Edgeguard defaults.
COLOR_BYTES = bytes(28) + EDGEGUARD_BYTES + EDGEGUARD2_BYTES
RANDALL = 0x08  # Byte 8 bit 3, claimed by version 13 and cleared by older migrations.
ATTACK = 0x70  # Byte 8 bits 4-6: Act OoAttack palette, claimed by version 14.
NEW_BITS = RANDALL | ATTACK  # Padding that pre-13 records never owned.
TIMERS = 0x80  # Byte 8 bit 7: Don't Reset Timers, flag 10 (TM_FLAG_DONT_RESET_TIMERS), no version bump.

def upgraded(raw):
    value = bytearray(raw)
    value[4:6] = bytes((CURRENT,PAYLOAD))
    value[16:20] = PS_DEFAULT_BYTES
    value[20:24] = SHIELD_DEFAULT_BYTES
    value[24:68] = COLOR_BYTES
    preset = (int.from_bytes(value[12:16], 'little') >> 6) & 7
    value[19] = preset if preset < 7 else 0
    value[14] = (value[14] & 0x3F) | 0x80
    value[15] &= 0x3F
    value[8] &= ~NEW_BITS
    return bytes(value)

FIELDS = ((27, COUNTS, DEFAULTS), (28, LEDGE_COUNTS, LEDGE_DEFAULTS), (29, (2,2), (0,0)), (30, (2,2,10,5), (1,0,0,0)), (31, PS_COUNTS, PS_DEFAULTS))
REGION_SIZE, OWNED_SIZE = 88, 68
# Under Fire's Lava Overlay (index 2) lives in word 20, outside FIELDS' pre-v11 words.
PREF_COUNTS = {29: 3}
GRID = 37  # TM_SETTING_OSD_GRID_CELL: 22 five-bit cells in bytes 36..49.
COSTUME = 38  # TM_SETTING_COSTUME: human byte 50, CPU byte 51.
SWEETSPOT_DJ = 40  # TM_SETTING_SWEETSPOT_DJ: word 20 bit 22.
BLAST_ZONES = 11  # TM_FLAG_BLAST_ZONES, a flag (field 11): word 20 bit 23, no version bump.

class ExtensionTests(unittest.TestCase):
    def setUp(self):
        self.m = Machine()
        self.m.init()
        self.m.cpu.mem_write(EXT - 4, b'\xA5' * 100)
        self.m.call('TestClearDirty')

    def test_randall_overlay_flag_lives_in_extension_byte_eight_bit_three(self):
        m = self.m
        core = m.record()
        self.assertEqual(m.call('Settings_Get', 11, 9), 0)
        m.call('Settings_Set', 14, 30, 5)  # A-Smash palette shares byte 8.
        m.call('Settings_Set', 11, 9, 1)
        self.assertEqual(m.call('Settings_Get', 11, 9), 1)
        self.assertEqual(m.cpu.mem_read(EXT + 8, 1)[0] & 0x0F, RANDALL | 5)
        self.assertEqual(m.call('Settings_Get', 14, 30), 5)
        self.assertEqual(m.record(), core)  # The full core record has no room; it is untouched.
        m.call('Settings_Set', 11, 9, 2)  # Out of range is ignored.
        self.assertEqual(m.call('Settings_Get', 11, 9), 1)
        m.call('Settings_Set', 11, 9, 0)
        self.assertEqual(m.call('Settings_Get', 11, 9), 0)
        self.assertEqual(m.call('Settings_Get', 14, 30), 5)

    def test_dont_reset_timers_flag_lives_in_extension_byte_eight_bit_seven(self):
        m = self.m
        core = m.record()
        self.assertEqual(m.call('Settings_Get', 11, 10), 0)  # Default Off: savestates rewind the clock.
        m.call('Settings_Set', 14, 30, 5)  # Every other byte-8 field is set first.
        m.call('Settings_Set', 11, 9, 1)
        m.call('Settings_Set', 14, 27, 6)
        m.call('Settings_Set', 11, 10, 1)
        self.assertEqual(m.call('Settings_Get', 11, 10), 1)
        self.assertEqual(m.cpu.mem_read(EXT + 8, 1)[0], TIMERS | (6 << 4) | RANDALL | 5)
        self.assertEqual(bytes(m.cpu.mem_read(EXT, 8)), HEADER)  # Still version 19.
        self.assertEqual(m.record(), core)
        m.call('Settings_Set', 11, 10, 2)  # Out of range is ignored.
        self.assertEqual(m.call('Settings_Get', 11, 10), 1)
        fresh = Machine(); fresh.put(m.record()); fresh.cpu.mem_write(EXT, bytes(m.cpu.mem_read(EXT, OWNED_SIZE)))
        self.assertEqual(fresh.call('Settings_Get', 11, 10), 1)
        m.call('Settings_Set', 11, 10, 0)
        self.assertEqual(m.call('Settings_Get', 11, 10), 0)
        self.assertEqual(m.cpu.mem_read(EXT + 8, 1)[0], (6 << 4) | RANDALL | 5)
        self.assertEqual((m.call('Settings_Get', 14, 30), m.call('Settings_Get', 11, 9), m.call('Settings_Get', 14, 27)), (5, 1, 6))

    def test_version_12_padding_never_turns_randall_on(self):
        m = self.m
        m.call('Settings_Set', 31, 0, 1)  # Create a current extension.
        old = bytearray(m.cpu.mem_read(EXT, OWNED_SIZE))
        old[4:6] = bytes((12,16)); old[8] |= RANDALL
        m.cpu.mem_write(EXT, bytes(old))
        self.assertEqual(m.call('Settings_Get', 11, 9), 0)
        m.call('Settings_Set', 14, 30, 2)  # First edit migrates to version 13.
        after = bytes(m.cpu.mem_read(EXT, OWNED_SIZE))
        expected = bytearray(old); expected[4:6] = bytes((CURRENT,PAYLOAD)); expected[8] = (old[8] & ~(NEW_BITS | 7)) | 2
        self.assertEqual(after, bytes(expected))
        self.assertEqual(m.call('Settings_Get', 11, 9), 0)

    def test_act_oo_attack_palette_lives_in_extension_byte_eight_bits_four_to_six(self):
        m = self.m
        core = m.record()
        self.assertEqual(m.call('Settings_Get', 14, 27), 0)  # New OSDs default Off.
        self.assertEqual(m.call('Settings_Get', 0, 0) & (1 << 27), 0)
        m.call('Settings_Set', 14, 30, 5)
        m.call('Settings_Set', 11, 9, 1)
        m.call('Settings_Set', 14, 27, 6)
        self.assertEqual(m.call('Settings_Get', 14, 27), 6)
        self.assertEqual(m.cpu.mem_read(EXT + 8, 1)[0], (6 << 4) | RANDALL | 5)
        self.assertEqual((m.call('Settings_Get', 14, 30), m.call('Settings_Get', 11, 9)), (5, 1))
        self.assertEqual(m.call('Settings_Get', 0, 0) & ((1 << 27) | (1 << 30)), (1 << 27) | (1 << 30))
        self.assertEqual((m.call('Settings_Get', 15, 27), m.call('Settings_Get', 16, 27)), (1, 1))
        self.assertEqual(m.record(), core)  # Neither the palette nor the enable touch the core record.
        m.call('Settings_Set', 14, 27, 16)  # Out of range is ignored.
        self.assertEqual(m.call('Settings_Get', 14, 27), 6)
        m.call('Settings_Set', 16, 27, 0)  # Native row Off, then On restores White.
        self.assertEqual(m.call('Settings_Get', 14, 27), 0)
        m.call('Settings_Set', 16, 27, 1)
        self.assertEqual(m.call('Settings_Get', 14, 27), 1)
        m.call('Settings_Set', 0, 0, m.call('Settings_Get', 0, 0) & ~(1 << 27))  # Whole-mask writes too.
        self.assertEqual(m.call('Settings_Get', 14, 27), 0)
        self.assertEqual((m.call('Settings_Get', 14, 30), m.call('Settings_Get', 11, 9)), (5, 1))
        self.assertEqual(m.record()[:4], core[:4])

    def test_version_13_keeps_randall_and_never_turns_act_oo_attack_on(self):
        m = self.m
        m.call('Settings_Set', 31, 0, 1)  # Create a current extension.
        old = bytearray(m.cpu.mem_read(EXT, OWNED_SIZE))
        old[4:6] = bytes((13,16)); old[8] = (old[8] & ~0x7F) | ATTACK | RANDALL | 3
        m.cpu.mem_write(EXT, bytes(old))
        self.assertEqual(m.call('Settings_Get', 14, 27), 0)
        self.assertEqual(m.call('Settings_Get', 11, 9), 1)
        self.assertEqual(m.call('Settings_Get', 14, 30), 3)
        m.call('Settings_Set', 14, 27, 2)  # First edit migrates to version 14.
        expected = bytearray(old); expected[4:6] = bytes((CURRENT,PAYLOAD)); expected[8] = (old[8] & ~ATTACK) | (2 << 4)
        self.assertEqual(bytes(m.cpu.mem_read(EXT, OWNED_SIZE)), bytes(expected))
        self.assertEqual((m.call('Settings_Get', 14, 27), m.call('Settings_Get', 11, 9)), (2, 1))

    def test_act_oo_attack_editor_row_cycles_its_extension_palette(self):
        m = self.m
        fixtures.open_editor(m)
        row = fixtures.editor_row(27)[1]
        for expected in (1, 2):
            m.call('TestEditorInput', fixtures.MENU_A, row)
            self.assertEqual(m.call('Settings_Get', 14, 27), expected)
        m.call('TestEditorInput', fixtures.MENU_B, row)
        self.assertEqual(m.call('Settings_Get', 14, 27), 1)
        m.call('TestEditorAnimate', row)
        m.call('TestSettingsEditorWrite', fixtures.PHYSICAL[row], m.call('TestEditorCache', row))
        self.assertEqual(m.call('Settings_Get', 14, 27), 1)  # Native exit keeps the chosen palette.

    def test_new_colors_use_nibbles_by_category_and_keep_a_close_original_color(self):
        m = self.m
        core = bytearray(m.record())
        fallback = (5, 7, 7, 6, 3, 5, 4, 7)  # Orange..Lavender -> nearest original color.
        for slot, id in enumerate(fixtures.IDS + [30, 27]):
            color = 8 + slot % 8
            m.call('Settings_Set', 14, id, color)
            self.assertEqual(m.call('Settings_Get', 14, id), color)
            self.assertEqual(m.call('Settings_Get', 0, 0) >> id & 1, 1)
            nibble = m.cpu.mem_read(EXT + 24 + slot // 2, 1)[0] >> (slot & 1) * 4 & 15
            self.assertEqual(nibble, color - 7)
            if id < 30 and id != 27: self.assertEqual(m.read(14, id), fallback[color - 8])
        self.assertEqual(m.cpu.mem_read(EXT + 8, 1)[0] & 0x77, (fallback[4] << 4) | fallback[3])
        self.assertEqual(bytes(m.cpu.mem_read(EXT, 8)), HEADER)
        m.call('Settings_Set', 14, 20, 16)  # Out of range is ignored.
        self.assertEqual(m.call('Settings_Get', 14, 20), 8 + fixtures.IDS.index(20) % 8)
        m.call('Settings_Set', 14, 20, 3)  # An original color clears the nibble.
        self.assertEqual((m.call('Settings_Get', 14, 20), m.read(14, 20)), (3, 3))
        slot = fixtures.IDS.index(20)
        self.assertEqual(m.cpu.mem_read(EXT + 24 + slot // 2, 1)[0] >> (slot & 1) * 4 & 15, 0)
        # Off wins over a retained new color; turning the OSD back on restores it.
        m.call('Settings_Set', 14, 1, 14)
        m.call('Settings_Set', 15, 1, 0)
        self.assertEqual(m.call('Settings_Get', 14, 1), 0)
        m.call('Settings_Set', 15, 1, 1)
        self.assertEqual(m.call('Settings_Get', 14, 1), 14)
        fresh = Machine(); fresh.put(m.record()); fresh.cpu.mem_write(EXT, bytes(m.cpu.mem_read(EXT, REGION_SIZE)))
        self.assertEqual(fresh.call('Settings_Get', 14, 1), 14)
        self.assertEqual(fresh.call('Settings_Get', 14, 27), 8 + 20 % 8)

    def test_version_14_migration_keeps_every_byte_and_starts_all_original_colors(self):
        m = self.m
        m.call('Settings_Set', 14, 27, 4); m.call('Settings_Set', 11, 9, 1); m.call('Settings_Set', 14, 30, 6)
        old = bytearray(m.cpu.mem_read(EXT, REGION_SIZE))
        old[4:6] = bytes((14,16)); old[24:36] = b'\xFF' * 12  # Version 14 never owned these bytes.
        m.cpu.mem_write(EXT, bytes(old))
        self.assertEqual(tuple(m.call('Settings_Get', 14, id) for id in (27, 30)), (4, 6))
        self.assertEqual(m.call('Settings_Get', 14, 20), m.read(14, 20))  # Garbage is not a new color.
        self.assertEqual(m.call('Settings_Get', 11, 9), 1)
        expected = bytearray(old); expected[4:6] = bytes((CURRENT,PAYLOAD)); expected[24:OWNED_SIZE] = COLOR_BYTES
        self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE)), bytes(expected))
        m.cpu.mem_write(EXT + 24, b'\x9F')  # Invalid nibbles repair to the original color.
        for id in (0, 1): self.assertEqual(m.call('Settings_Get', 14, id), m.read(14, id))
        m.call('Settings_Get', 28, 0)  # Any extension access validates the record.
        self.assertEqual(m.cpu.mem_read(EXT + 24, 1)[0], 0)

    def test_version_15_migration_keeps_colors_and_low_laser_and_claims_only_grid_bytes(self):
        m = self.m
        m.call('Settings_Set', 14, 20, 12)  # Lime lives in a color nibble.
        m.call('Settings_Set', 30, 2, 9); m.call('Settings_Set', 30, 3, 4)  # Low Laser's byte 35.
        m.call('Settings_Set', 14, 27, 4); m.call('Settings_Set', 11, 9, 1)
        old = bytearray(m.cpu.mem_read(EXT, REGION_SIZE))
        old[4:6] = bytes((15, 28)); old[36:50] = b'\xFF' * 14  # Version 15 never owned these bytes.
        m.cpu.mem_write(EXT, bytes(old))
        self.assertEqual(m.call('Settings_Get', GRID, 0), 0)  # Spare score bytes are not cells.
        expected = bytearray(old); expected[4:6] = bytes((CURRENT, PAYLOAD)); expected[36:50] = bytes(14)
        self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE)), bytes(expected))
        self.assertEqual(tuple(m.call('Settings_Get', f, i) for f, i in ((14, 20), (30, 2), (30, 3), (14, 27), (11, 9))),
                         (12, 9, 4, 4, 1))

    def test_version_16_migration_keeps_every_byte_and_claims_only_costume_bytes(self):
        m = self.m
        m.call('Settings_Set', 14, 20, 12); m.call('Settings_Set', 30, 2, 9)
        m.call('Settings_Set', GRID, 3, 17); m.call('Settings_Set', GRID, 21, 30)
        old = bytearray(m.cpu.mem_read(EXT, REGION_SIZE))
        old[4:6] = bytes((16, 42)); old[50:68] = b'\x03' + b'\xFF' * 17  # Version 16 never owned these bytes.
        m.cpu.mem_write(EXT, bytes(old))
        self.assertEqual((m.call('Settings_Get', COSTUME, 0), m.call('Settings_Get', COSTUME, 1)), (0, 0))
        expected = bytearray(old); expected[4:6] = bytes((CURRENT, PAYLOAD)); expected[50:52] = bytes(2)
        expected[52:68] = EDGEGUARD_BYTES + EDGEGUARD2_BYTES
        self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE)), bytes(expected))
        self.assertEqual(tuple(m.call('Settings_Get', f, i) for f, i in ((14, 20), (30, 2), (GRID, 3), (GRID, 21))),
                         (12, 9, 17, 30))

    def test_version_17_migration_keeps_every_byte_and_claims_only_edgeguard_bytes(self):
        m = self.m
        m.call('Settings_Set', 14, 20, 12); m.call('Settings_Set', GRID, 21, 30)
        m.call('Settings_Set', COSTUME, 0, 5); m.call('Settings_Set', COSTUME, 1, 2)
        old = bytearray(m.cpu.mem_read(EXT, REGION_SIZE))
        old[4:6] = bytes((17, 44)); old[52:68] = b'\x07' * 16  # Version 17 never owned these bytes.
        m.cpu.mem_write(EXT, bytes(old))
        self.assertEqual(tuple(m.call('Settings_Get', EDGEGUARD, i) for i in range(EG_COUNT)), EDGEGUARD_DEFAULTS)
        expected = bytearray(old); expected[4:6] = bytes((CURRENT, PAYLOAD))
        expected[52:68] = EDGEGUARD_BYTES + EDGEGUARD2_BYTES
        self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE)), bytes(expected))
        self.assertEqual(tuple(m.call('Settings_Get', f, i) for f, i in ((14, 20), (GRID, 21), (COSTUME, 0), (COSTUME, 1))),
                         (12, 30, 5, 2))

    def test_version_18_migration_keeps_edgeguard_settings_and_claims_only_word_64(self):
        m = self.m
        customized = (150, 200, 30, 80, 120, 20, 300, 4, 33, 2, 63, 31, 15)
        for index, value in enumerate(customized):
            m.call('Settings_Set', EDGEGUARD, index, value)
        m.call('Settings_Set', COSTUME, 1, 2)
        old = bytearray(m.cpu.mem_read(EXT, REGION_SIZE))
        old[4:6] = bytes((18, 56)); old[64:68] = b'\x00\x00\x00\x00'  # Version 18 never owned word 64.
        m.cpu.mem_write(EXT, bytes(old))
        self.assertEqual(tuple(m.call('Settings_Get', EDGEGUARD, i) for i in range(EG_COUNT)),
                         customized + EDGEGUARD_DEFAULTS[EG_FIRST_NEW:])
        expected = bytearray(old); expected[4:6] = bytes((CURRENT, PAYLOAD)); expected[64:68] = EDGEGUARD2_BYTES
        self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE)), bytes(expected))
        self.assertEqual(m.call('Settings_Get', COSTUME, 1), 2)

    def test_version_18_records_read_new_recovery_defaults_without_writing(self):
        m = self.m
        old = bytearray(m.cpu.mem_read(EXT, REGION_SIZE))
        old[4:6] = bytes((18, 56)); old[64:68] = b'\xFF' * 4  # Spare score bytes are not masks.
        m.cpu.mem_write(EXT, bytes(old))
        values = [m.call('TMExtension_Read', EXT, EDGEGUARD, i) for i in range(EG_FIRST_NEW, EG_COUNT)]
        self.assertEqual(tuple(values), EDGEGUARD_DEFAULTS[EG_FIRST_NEW:])
        self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE)), bytes(old))

    def test_recovery_menu_defaults_match_saved_initial_masks(self):
        # settings_extension.c and recovery.c build into different DATs; keep
        # each saved mask's initial value equal to its menu's default rows.
        source = (ROOT / 'src' / 'recovery.c').read_text()
        menus = ('Fox', 'Falco', 'Sheik', 'Falcon', 'Marth', 'Yoshi', 'Peach', 'Luigi', 'DrMario',
                 'IceClimbers', 'Puff', 'Samus')
        for offset, menu in enumerate(menus):
            body = source.split('static EventOption RecOptions_%s[] = {' % menu, 1)[1].split('\n};', 1)[0]
            rows = body.split('.kind = OPTKIND_TOGGLE')[1:]
            mask = sum(1 << row for row, text in enumerate(rows) if '.val = 1' in text)
            index = 8 + offset
            self.assertEqual(mask, EDGEGUARD_DEFAULTS[index], menu)
            self.assertEqual(1 << len(rows), EDGEGUARD_COUNTS[index], menu)

    def test_edgeguard_preferences_round_trip_reject_and_repair_invalid_values(self):
        m = self.m
        core = m.record()
        self.assertEqual(tuple(m.call('Settings_Get', EDGEGUARD, i) for i in range(EG_COUNT)), EDGEGUARD_DEFAULTS)
        m.call('Settings_Set', COSTUME, 1, 4)  # The neighbor just before the Edgeguard words.
        values = list(EDGEGUARD_DEFAULTS)
        for index, count in enumerate(EDGEGUARD_COUNTS):
            for value in sorted({0, 1, count // 2, count - 1}):
                m.call('Settings_Set', EDGEGUARD, index, value)
                values[index] = value
                self.assertEqual(tuple(m.call('Settings_Get', EDGEGUARD, i) for i in range(EG_COUNT)), tuple(values))
            snapshot = bytes(m.cpu.mem_read(EXT, REGION_SIZE + 8))
            for value in (count, 0xFFFFFFFF):
                m.call('Settings_Set', EDGEGUARD, index, value)
                self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE + 8)), snapshot)
        snapshot = bytes(m.cpu.mem_read(EXT, REGION_SIZE + 8))
        for index in (EG_COUNT, 0xFFFFFFFF):
            m.call('Settings_Set', EDGEGUARD, index, 1)
        self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE + 8)), snapshot)
        self.assertEqual(m.call('Settings_Get', COSTUME, 1), 4)
        self.assertEqual(m.record(), core)
        self.assertEqual(bytes(m.cpu.mem_read(EXT + OWNED_SIZE, REGION_SIZE - OWNED_SIZE)), b'\xA5' * (REGION_SIZE - OWNED_SIZE))
        fresh = Machine(); fresh.put(m.record()); fresh.cpu.mem_write(EXT, bytes(m.cpu.mem_read(EXT, REGION_SIZE)))
        self.assertEqual(tuple(fresh.call('Settings_Get', EDGEGUARD, i) for i in range(EG_COUNT)), tuple(values))
        # A damaged Player Percent (999..1023) repairs to 0% and leaves its neighbors.
        word = int.from_bytes(m.cpu.mem_read(EXT + 56, 4), 'little') | 1023
        m.cpu.mem_write(EXT + 56, word.to_bytes(4, 'little'))
        self.assertEqual(m.call('Settings_Get', EDGEGUARD, 4), 0)
        self.assertEqual(tuple(m.call('Settings_Get', EDGEGUARD, i) for i in (5, 6)), tuple(values[5:7]))

    def test_edgeguard_player_position_reads_close_from_existing_saves_and_uses_only_word_60_top_bits(self):
        m = self.m
        self.assertEqual(m.call('Settings_Get', EDGEGUARD, EG_POSITION), 1)  # Close without a record.
        m.call('Settings_Set', COSTUME, 0, 1)  # Create the record.
        # A new record, like every record from before this setting, has bits 30-31 of word 60 clear.
        self.assertEqual(int.from_bytes(m.cpu.mem_read(EXT + 60, 4), 'little') >> 30, 0)
        self.assertEqual(m.call('Settings_Get', EDGEGUARD, EG_POSITION), 1)  # Close.
        before = bytearray(m.cpu.mem_read(EXT, REGION_SIZE))
        for position, stored in ((0, 3), (1, 0), (2, 1), (3, 2)):
            m.call('Settings_Set', EDGEGUARD, EG_POSITION, position)
            self.assertEqual(m.call('Settings_Get', EDGEGUARD, EG_POSITION), position)
            after = bytearray(m.cpu.mem_read(EXT, REGION_SIZE))
            word = int.from_bytes(after[60:64], 'little')
            self.assertEqual(word >> 30, stored)
            # Nothing else in the record changes, including Hit Strength and Marth in word 60.
            after[63] &= 0x3F
            self.assertEqual(bytes(after), bytes(before))
        self.assertEqual(tuple(m.call('Settings_Get', EDGEGUARD, i) for i in range(EG_POSITION)),
                         EDGEGUARD_DEFAULTS[:EG_POSITION])

    def test_costumes_round_trip_reject_and_repair_invalid_values(self):
        m = self.m
        core = m.record()
        m.call('Settings_Set', GRID, 21, 30)  # The last cell borders the costume bytes.
        m.call('Settings_Set', COSTUME, 0, 5); m.call('Settings_Set', COSTUME, 1, 2)
        self.assertEqual(bytes(m.cpu.mem_read(EXT + 50, 2)), b'\x05\x02')
        self.assertEqual(m.call('Settings_Get', GRID, 21), 30)
        self.assertEqual(m.record(), core)
        snapshot = bytes(m.cpu.mem_read(EXT, REGION_SIZE))
        for index, value in ((0, 6), (1, 0xFFFFFFFF), (2, 1)):
            m.call('Settings_Set', COSTUME, index, value)
        self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE)), snapshot)
        m.cpu.mem_write(EXT + 51, b'\x09')  # A damaged byte repairs to Default.
        self.assertEqual((m.call('Settings_Get', COSTUME, 0), m.call('Settings_Get', COSTUME, 1)), (5, 0))
        self.assertEqual(m.cpu.mem_read(EXT + 51, 1)[0], 0)

    def test_grid_cells_pack_five_bits_round_trip_and_reject_or_repair_invalid_values(self):
        m = self.m
        core = m.record()
        m.call('Settings_Set', 30, 2, 9)  # A neighbor in byte 35.
        values = [(slot * 7) % 31 for slot in range(22)]
        for slot, value in enumerate(values): m.call('Settings_Set', GRID, slot, value)
        self.assertEqual([m.call('Settings_Get', GRID, slot) for slot in range(22)], values)
        bits = int.from_bytes(m.cpu.mem_read(EXT + 36, 14), 'little')
        self.assertEqual([bits >> slot * 5 & 31 for slot in range(22)], values)
        self.assertEqual(bits >> 110, 0)  # The last two bits stay free.
        self.assertEqual(m.call('Settings_Get', 30, 2), 9)
        self.assertEqual(m.record(), core)
        snapshot = bytes(m.cpu.mem_read(EXT, REGION_SIZE))
        for slot, value in ((0, 31), (0, 0xFFFFFFFF), (22, 1), (0xFFFFFFFF, 1)):
            m.call('Settings_Set', GRID, slot, value)
        self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE)), snapshot)
        self.assertEqual(m.call('Settings_Get', GRID, 22), 0)
        fresh = Machine(); fresh.put(m.record()); fresh.cpu.mem_write(EXT, snapshot)
        self.assertEqual([fresh.call('Settings_Get', GRID, slot) for slot in range(22)], values)
        bits |= 31 << 5; m.cpu.mem_write(EXT + 36, bits.to_bytes(14, 'little'))  # A damaged cell.
        self.assertEqual(m.call('Settings_Get', GRID, 1), 0)
        self.assertEqual([m.call('Settings_Get', GRID, slot) for slot in (0, 2, 21)], [values[0], values[2], values[21]])

    def test_randall_overlay_editor_row_and_lab_share_the_global_flag(self):
        m = self.m
        fixtures.open_editor(m, 1)
        row = fixtures.editor_row(31)[1]
        label = 'Randall Overlay'
        self.assertEqual(''.join(chr(m.call('TestEditorLabelChar', row, i)) for i in range(len(label))), label)
        m.call('TestEditorInput', fixtures.MENU_A, row)
        self.assertEqual(m.call('Settings_Get', 11, 9), 1)
        m.call('TestEditorAnimate', row)
        m.call('TestSettingsEditorWrite', fixtures.PHYSICAL[row], m.call('TestEditorCache', row))
        self.assertEqual(m.call('Settings_Get', 11, 9), 1)
        self.assertEqual(m.call('TestSettingsEditorRow', fixtures.PHYSICAL[row]), 1)
        m.call('Settings_Set', 16, 31, 0)  # Logical row write, as the Lab and native hooks use.
        self.assertEqual(m.call('Settings_Get', 11, 9), 0)

    def test_dont_reset_timers_editor_row_writes_its_extension_flag(self):
        m = self.m
        fixtures.open_editor(m, 1)
        row = fixtures.editor_row(34)[1]
        label = "Don't Reset Timers"
        self.assertEqual(''.join(chr(m.call('TestEditorLabelChar', row, i)) for i in range(len(label))), label)
        m.call('TestEditorInput', fixtures.MENU_A, row)
        self.assertEqual(m.call('Settings_Get', 11, 10), 1)
        self.assertEqual(m.call('Settings_Get', 11, 9), 0)  # Randall is untouched.
        m.call('TestEditorAnimate', row)
        m.call('TestSettingsEditorWrite', fixtures.PHYSICAL[row], m.call('TestEditorCache', row))
        self.assertEqual(m.call('Settings_Get', 11, 10), 1)
        self.assertEqual(m.call('TestSettingsEditorRow', fixtures.PHYSICAL[row]), 1)
        self.assertEqual(m.call('Settings_Get', 16, 34), 1)
        m.call('Settings_Set', 16, 34, 0)  # Logical row write, as the native hooks use.
        self.assertEqual(m.call('Settings_Get', 11, 10), 0)

    def test_blast_zone_overlay_editor_row_writes_its_extension_flag(self):
        m = self.m
        fixtures.open_editor(m, 1)
        row = fixtures.editor_row(35)[1]
        label = "Blast Zone Overlay"
        self.assertEqual(''.join(chr(m.call('TestEditorLabelChar', row, i)) for i in range(len(label))), label)
        m.call('TestEditorInput', fixtures.MENU_A, row)
        self.assertEqual(m.call('Settings_Get', 11, BLAST_ZONES), 1)
        self.assertEqual((m.call('Settings_Get', 11, 9), m.call('Settings_Get', 11, 10)), (0, 0))  # Randall and timers are untouched.
        m.call('TestEditorAnimate', row)
        m.call('TestSettingsEditorWrite', fixtures.PHYSICAL[row], m.call('TestEditorCache', row))
        self.assertEqual(m.call('Settings_Get', 11, BLAST_ZONES), 1)
        self.assertEqual(m.call('TestSettingsEditorRow', fixtures.PHYSICAL[row]), 1)
        self.assertEqual(m.call('Settings_Get', 16, 35), 1)
        m.call('Settings_Set', 16, 35, 0)  # Logical row write, as the native hooks use.
        self.assertEqual(m.call('Settings_Get', 11, BLAST_ZONES), 0)

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
        self.assertEqual(bytes(m.cpu.mem_read(EXT + OWNED_SIZE, 96 - OWNED_SIZE)), b'\xA5' * (96 - OWNED_SIZE))
        self.assertEqual(bytes(m.cpu.mem_read(EXT, 8)), HEADER)

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
            for index in (PREF_COUNTS.get(field, len(counts)), 0xFFFFFFFF):
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
                         HEADER + b'\x85\x98\xFA\x87\x00\x72\x90\x00' + PS_DEFAULT_BYTES + SHIELD_DEFAULT_BYTES + COLOR_BYTES)
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
            self.assertEqual(after[4:6], bytes((CURRENT,PAYLOAD)))
            self.assertEqual(after[24:OWNED_SIZE], COLOR_BYTES)
            self.assertEqual(after[6:8], legacy[6:8])
            self.assertEqual(after[8], legacy[8] & ~NEW_BITS)  # Newly owned bits are cleared.
            self.assertEqual(after[9:11], legacy[9:11])
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
                tuple(6 if field == 28 and i == 3 else 0 if field == 28 and i in (7,9) else 2 if field == 28 and i == 8 else 0 if field == 30 and i >= 2 else n-1 for i,n in enumerate(counts)))
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
            expected = bytearray(old); expected[4:6] = bytes((CURRENT,PAYLOAD)); expected[20:24] = SHIELD_DEFAULT_BYTES; expected[24:OWNED_SIZE] = COLOR_BYTES; expected[19] = preset; expected[8] &= ~NEW_BITS
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
            expected = bytearray(old); expected[4:6] = bytes((CURRENT,PAYLOAD)); expected[20:24] = SHIELD_DEFAULT_BYTES; expected[24:OWNED_SIZE] = COLOR_BYTES; expected[8] &= ~NEW_BITS
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
        expected=bytearray(old);expected[4:6]=bytes((CURRENT,PAYLOAD));expected[20:24]=SHIELD_DEFAULT_BYTES;expected[24:OWNED_SIZE]=COLOR_BYTES; expected[8] &= ~NEW_BITS
        self.assertEqual(bytes(m.cpu.mem_read(EXT,REGION_SIZE)),bytes(expected))
        for field, counts, _ in FIELDS:
            self.assertEqual(tuple(m.call('Settings_Get',field,i) for i in range(len(counts))),tuple(0 if field == 30 and i >= 2 else n-1 for i,n in enumerate(counts)))  # Byte 35 is new in 15
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
            expected=bytearray(old);expected[4:6]=bytes((CURRENT,PAYLOAD)); expected[8] &= ~NEW_BITS; expected[35]=0  # Version 15 claims byte 35
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
            old=bytearray(m.cpu.mem_read(EXT,REGION_SIZE));old[4:6]=bytes((11,16))
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


    def test_low_laser_position_and_height_own_only_byte_35_of_version_15(self):
        m = self.m
        self.assertEqual((m.call('Settings_Get',30,2), m.call('Settings_Get',30,3)), (0,0))  # Center, Grounded.
        m.call('Settings_Set',30,1,1)  # Create a current extension through an existing field.
        before = bytearray(m.cpu.mem_read(EXT, REGION_SIZE))
        self.assertEqual(before[35], 0)
        for position in range(10):
            for height in range(5):
                m.call('Settings_Set',30,2,position); m.call('Settings_Set',30,3,height)
                self.assertEqual((m.call('Settings_Get',30,2), m.call('Settings_Get',30,3)), (position,height))
                expected = bytearray(before); expected[35] = position | height << 4
                self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE)), bytes(expected))
        m.call('Settings_Set',30,2,10); m.call('Settings_Set',30,3,5)  # Out of range is ignored.
        self.assertEqual((m.call('Settings_Get',30,2), m.call('Settings_Get',30,3)), (9,4))
        self.assertEqual((m.call('Settings_Get',30,0), m.call('Settings_Get',30,1)), (1,1))

    def test_low_laser_byte_35_invalid_values_read_and_repair_to_defaults(self):
        m = self.m
        m.call('Settings_Set',30,1,1)
        record = bytearray(m.cpu.mem_read(EXT, REGION_SIZE)); record[35] = 0xFF  # Position 15, height 7, spare bit 7.
        m.cpu.mem_write(EXT, bytes(record))
        self.assertEqual((m.call('TMExtension_Read',EXT,30,2), m.call('TMExtension_Read',EXT,30,3)), (0,0))
        m.call('Settings_Set',30,0,0)  # Any edit validates the record.
        after = bytes(m.cpu.mem_read(EXT, REGION_SIZE))
        self.assertEqual(after[35], 0x80)
        expected = bytearray(record); expected[35] = 0x80; expected[14] &= ~0x10
        self.assertEqual(after, bytes(expected))

    def test_under_fire_lava_overlay_lives_in_word_20_bit_21(self):
        m = self.m
        m.call('Settings_Set',29,0,0)  # Create the record.
        self.assertEqual(m.call('Settings_Get',29,2), 0)
        shield = [m.call('Settings_Get',f,i) for f, counts, _ in SHIELD_FIELDS for i in range(len(counts))]
        before = bytes(m.cpu.mem_read(EXT, REGION_SIZE))
        m.call('Settings_Set',29,2,1)
        after = bytes(m.cpu.mem_read(EXT, REGION_SIZE))
        expected = bytearray(before); expected[22] |= 0x20
        self.assertEqual(after, bytes(expected))
        self.assertEqual(m.call('Settings_Get',29,2), 1)
        self.assertEqual([m.call('Settings_Get',f,i) for f, counts, _ in SHIELD_FIELDS for i in range(len(counts))], shield)
        # Version 11..18 records own word 20, so the bit is read as is.
        old = bytearray(after); old[4:6] = bytes((18,56))
        m.cpu.mem_write(EXT, bytes(old))
        self.assertEqual(m.call('TMExtension_Read',EXT,29,2), 1)

    def test_records_before_version_11_never_read_the_lava_overlay_bit(self):
        for version, payload in ((10,12), (6,8)):
            m = Machine(); m.init()
            m.cpu.mem_write(EXT - 4, b'\xA5' * 100)
            m.call('Settings_Set',29,0,1)
            old = bytearray(m.cpu.mem_read(EXT, REGION_SIZE))
            old[4:6] = bytes((version,payload)); old[22] = 0xFF  # Valid-looking spare score bytes.
            m.cpu.mem_write(EXT, bytes(old))
            self.assertEqual(m.call('TMExtension_Read',EXT,29,2), 0)
            m.call('Settings_Set',29,0,0)  # Any edit migrates and claims word 20.
            self.assertEqual(m.cpu.mem_read(EXT, REGION_SIZE)[4], CURRENT)
            self.assertEqual(m.call('Settings_Get',29,2), 0)

    def test_sweetspot_double_jump_indicator_lives_in_word_20_bit_22(self):
        m = self.m
        m.call('Settings_Set',29,2,1)  # Lava Overlay: the neighboring bit 21.
        self.assertEqual(m.call('Settings_Get',SWEETSPOT_DJ,0), 0)
        shield = [m.call('Settings_Get',f,i) for f, counts, _ in SHIELD_FIELDS for i in range(len(counts))]
        before = bytes(m.cpu.mem_read(EXT, REGION_SIZE))
        m.call('Settings_Set',SWEETSPOT_DJ,0,1)
        after = bytes(m.cpu.mem_read(EXT, REGION_SIZE))
        expected = bytearray(before); expected[22] |= 0x40
        self.assertEqual(after, bytes(expected))
        self.assertEqual((m.call('Settings_Get',SWEETSPOT_DJ,0), m.call('Settings_Get',29,2)), (1, 1))
        self.assertEqual([m.call('Settings_Get',f,i) for f, counts, _ in SHIELD_FIELDS for i in range(len(counts))], shield)
        m.call('Settings_Set',SWEETSPOT_DJ,0,2)  # Out of range is ignored.
        m.call('Settings_Set',SWEETSPOT_DJ,1,0)  # So is any index but 0.
        self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE)), after)
        # Version 11..18 records own word 20, so the bit is read as is.
        old = bytearray(after); old[4:6] = bytes((18,56))
        m.cpu.mem_write(EXT, bytes(old))
        self.assertEqual(m.call('TMExtension_Read',EXT,SWEETSPOT_DJ,0), 1)
        m.cpu.mem_write(EXT, after)
        m.call('Settings_Set',SWEETSPOT_DJ,0,0)
        self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE)), before)

    def test_records_before_version_11_never_read_the_double_jump_indicator_bit(self):
        for version, payload in ((10,12), (6,8)):
            m = Machine(); m.init()
            m.cpu.mem_write(EXT - 4, b'\xA5' * 100)
            m.call('Settings_Set',29,0,1)
            old = bytearray(m.cpu.mem_read(EXT, REGION_SIZE))
            old[4:6] = bytes((version,payload)); old[22] = 0xFF  # Valid-looking spare score bytes.
            m.cpu.mem_write(EXT, bytes(old))
            self.assertEqual(m.call('TMExtension_Read',EXT,SWEETSPOT_DJ,0), 0)
            m.call('Settings_Set',29,0,0)  # Any edit migrates and claims word 20.
            self.assertEqual(m.cpu.mem_read(EXT, REGION_SIZE)[4], CURRENT)
            self.assertEqual(m.call('Settings_Get',SWEETSPOT_DJ,0), 0)

    def test_blast_zone_overlay_flag_lives_in_word_20_bit_23(self):
        m = self.m
        m.call('Settings_Set',29,2,1)  # Lava Overlay and the Double-Jump Indicator: bits 21 and 22.
        m.call('Settings_Set',SWEETSPOT_DJ,0,1)
        m.call('Settings_Set',11,10,1)  # Don't Reset Timers, the previous extension flag.
        self.assertEqual(m.call('Settings_Get',11,BLAST_ZONES), 0)
        core = m.record()
        before = bytes(m.cpu.mem_read(EXT, REGION_SIZE))
        m.call('Settings_Set',11,BLAST_ZONES,1)
        after = bytes(m.cpu.mem_read(EXT, REGION_SIZE))
        expected = bytearray(before); expected[22] |= 0x80
        self.assertEqual(after, bytes(expected))
        self.assertEqual(after[:8], HEADER)  # Still version 19.
        self.assertEqual(m.record(), core)
        self.assertEqual([m.call('Settings_Get',*key) for key in ((11,BLAST_ZONES), (29,2), (SWEETSPOT_DJ,0), (11,10), (11,9))], [1,1,1,1,0])
        m.call('Settings_Set',11,BLAST_ZONES,2)  # Out of range is ignored.
        self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE)), after)
        # Version 11..18 records own word 20, so the bit is read as is.
        old = bytearray(after); old[4:6] = bytes((18,56))
        m.cpu.mem_write(EXT, bytes(old))
        self.assertEqual(m.call('TMExtension_Read',EXT,11,BLAST_ZONES), 1)
        m.cpu.mem_write(EXT, after)
        m.call('Settings_Set',11,BLAST_ZONES,0)
        self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE)), before)

    def test_records_before_version_11_never_read_the_blast_zone_overlay_bit(self):
        for version, payload in ((10,12), (6,8)):
            m = Machine(); m.init()
            m.cpu.mem_write(EXT - 4, b'\xA5' * 100)
            m.call('Settings_Set',29,0,1)
            old = bytearray(m.cpu.mem_read(EXT, REGION_SIZE))
            old[4:6] = bytes((version,payload)); old[22] = 0xFF  # Valid-looking spare score bytes.
            m.cpu.mem_write(EXT, bytes(old))
            self.assertEqual(m.call('TMExtension_Read',EXT,11,BLAST_ZONES), 0)
            m.call('Settings_Set',29,0,0)  # Any edit migrates and claims word 20.
            self.assertEqual(m.cpu.mem_read(EXT, REGION_SIZE)[4], CURRENT)
            self.assertEqual(m.call('Settings_Get',11,BLAST_ZONES), 0)

    def test_older_records_never_read_byte_35_and_migration_claims_it_as_defaults(self):
        for version, payload in ((14,16), (10,12), (6,8)):
            m = Machine(); m.init()
            m.cpu.mem_write(EXT - 4, b'\xA5' * 100)
            m.call('Settings_Set',30,1,1)
            old = bytearray(m.cpu.mem_read(EXT, REGION_SIZE))
            old[4:6] = bytes((version,payload)); old[35] = 0x23  # Valid-looking spare score bytes.
            m.cpu.mem_write(EXT, bytes(old))
            self.assertEqual((m.call('TMExtension_Read',EXT,30,2), m.call('TMExtension_Read',EXT,30,3)), (0,0))
            self.assertEqual(bytes(m.cpu.mem_read(EXT, REGION_SIZE)), bytes(old))
            m.call('Settings_Set',30,2,5)
            after = bytes(m.cpu.mem_read(EXT, REGION_SIZE))
            self.assertEqual((after[4], after[35]), (CURRENT, 0x05))
            self.assertEqual(after[OWNED_SIZE:], bytes(old[OWNED_SIZE:]))
            self.assertEqual((m.call('Settings_Get',30,2), m.call('Settings_Get',30,3)), (5,0))

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

    def test_low_laser_position_and_height_roundtrip_in_optimized_production_dat(self):
        m = self.m
        m.init()
        m.call('DAT_Set', 30, 2, 9); m.call('DAT_Set', 30, 3, 4)
        saved = bytes(m.cpu.mem_read(EXT, REGION_SIZE))
        self.assertEqual(saved[35], 9 | 4 << 4)
        fixture = fixtures.RelocatedDATSettingsTests(); fixture.setUp()
        fixture.m.put(m.record()); fixture.m.cpu.mem_write(EXT, saved)
        self.assertEqual((fixture.m.call('DAT_Get', 30, 2), fixture.m.call('DAT_Get', 30, 3)), (9, 4))

    def test_v1_migration_uses_real_production_dat_relocations(self):
        m = self.m; m.init()
        legacy = b'TYEX\x01\x04\xA5\x5A\x06\x98\x7A\xA0'
        m.cpu.mem_write(EXT, legacy + b'\xE5' * (REGION_SIZE - len(legacy)))
        core = m.record()
        self.assertEqual(m.call('DAT_Get',14,30), 6)
        self.assertEqual(tuple(m.call('DAT_Get',27,i) for i in range(len(COUNTS))),
                         (8,1,0,2,2,2,3,1,7,0,0))
        self.assertEqual(tuple(m.call('DAT_Get',28,i) for i in range(len(LEDGE_COUNTS))), LEDGE_DEFAULTS)
        self.assertEqual(bytes(m.cpu.mem_read(EXT,8)), HEADER)
        self.assertEqual(bytes(m.cpu.mem_read(EXT+OWNED_SIZE,REGION_SIZE-OWNED_SIZE)),
                         b'\xE5'*(REGION_SIZE-OWNED_SIZE))
        self.assertEqual(m.record(),core)

if __name__ == '__main__': unittest.main()
