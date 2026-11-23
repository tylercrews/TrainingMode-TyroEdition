"""All Events (Alphabetized): the fifth Event Select page, shown left of Minigames.

python tests/test_all_events.py -v

Runs the relocated shared DAT and the actual Event Select Screen gecko codes
that tests/run_settings_tests.sh builds, against build/Start.dol. The native
routines the page switch calls to draw are stubbed, and the stubs record what
they're asked to draw.
"""
import struct
import unittest
from test_score_storage import NativeMachine, gpr
from unicorn import UC_HOOK_CODE
from unicorn import ppc_const as reg

ALL_EVENTS = 4  # TM_EVENT_PAGE_ALL
NAME = 'All Events (Alphabetized)'
PAGE, SELECTION = 2, 26  # TM_SETTING_EVENT_PAGE, TM_SETTING_EVENT_SELECTION
LEFT, RIGHT = 0x40, 0x80  # Inputs_GetPlayerRapidInputs' left and right
ROW_HEIGHT = -1.32  # The cursor's Y per row, as the X and Y jumps set it.
ESS = 0x80600000  # The Event Select Screen's data, at -0x4A40(r13).
CURSOR, CURSOR_JOBJ = ESS + 0x100, ESS + 0x200  # Its row (byte) and scroll (word at 4); the cursor's JObj.
STUBS = {
    0x801A36C0: 'input',        # Inputs_GetPlayerRapidInputs
    0x803A70A0: 'page_name',    # Text_UpdateSubtextContents
    0x8024D15C: 'row',          # Draw one row: (row, event)
    0x8024D7E0: 'description',  # (data, event)
    0x8024D5B0: 'score',
    0x80011E24: 'find_jobj',    # Stores the cursor's JObj at r4
    0x803732E8: 'dirty',        # HSD_JObjSetMtxDirtySub
    0x80024030: 'sfx',          # SFX_MenuCommonSound
}


class AllEventsMachine:
    def __init__(self):
        self.m = NativeMachine()
        self.f = self.m.load_services()

    def word(self, pointer):
        return struct.unpack('>I', self.m.cpu.mem_read(pointer, 4))[0]

    def text(self, pointer):
        return bytes(self.m.cpu.mem_read(pointer, 160)).split(b'\0', 1)[0].decode()

    def get(self, field, index=0):
        return self.m.call(self.f[27], field, index)

    def set(self, field, index, value):
        self.m.call(self.f[28], field, index, value)

    def page(self, page):
        return self.word(self.f[0] + page * 4)  # EventPages[page]

    def events(self, page):
        page = self.page(page)
        return [self.word(self.word(page + 8) + i * 4) for i in range(self.word(page + 4))]

    def name(self, descriptor):
        return self.text(self.word(descriptor)).strip()

    def home(self, descriptor):
        for page in range(ALL_EVENTS):
            if descriptor in self.events(page):
                return page, self.events(page).index(descriptor)
        raise AssertionError(f'{self.name(descriptor)} is on no other page')


class CatalogueTests(unittest.TestCase):
    def setUp(self):
        self.a = AllEventsMachine()

    def test_it_is_the_last_page_and_lists_every_event_once_by_name(self):
        a = self.a
        self.assertEqual(a.m.call(a.f[9]), ALL_EVENTS)  # GetPageNum: the last page's index.
        self.assertEqual(a.text(a.word(a.page(ALL_EVENTS))), NAME)
        self.assertEqual(a.text(a.m.call(a.f[4], ALL_EVENTS)), NAME)
        listed = a.events(ALL_EVENTS)
        home = [d for page in range(ALL_EVENTS) for d in a.events(page)]
        self.assertEqual(len(home), 38)
        # The same descriptors, not copies: each event once.
        self.assertEqual(sorted(listed), sorted(home))
        self.assertEqual(len(set(listed)), len(listed))
        names = [a.name(d) for d in listed]
        self.assertEqual(names, sorted(names, key=str.lower))
        self.assertEqual((names[0], names[-1]), ('Amsah-Tech Training', 'Waveshine SDI'))
        self.assertEqual(a.m.call(a.f[5], ALL_EVENTS), 37)  # GetPageEventNum: the last event's index.
        for index, descriptor in enumerate(listed):
            self.assertEqual(a.m.call(a.f[1], ALL_EVENTS, index), descriptor)  # GetEventDesc

    def test_each_event_is_its_own_page_s_with_its_score_slot(self):
        a = self.a
        scored = 0
        for index, descriptor in enumerate(a.events(ALL_EVENTS)):
            page, event = a.home(descriptor)
            with self.subTest(event=a.name(descriptor)):
                # GetCSSType, GetIsSelectStage, GetCPUFighter, GetStage, GetEventFile, GetCSSFile, GetScoreType.
                for function in (10, 11, 12, 13, 14, 15, 22):
                    self.assertEqual(a.m.call(a.f[function], ALL_EVENTS, index), a.m.call(a.f[function], page, event))
                a.set(PAGE, 0, ALL_EVENTS)
                shared = a.m.call(a.f[43], index)  # Settings_EventScoreSlot
                a.set(PAGE, 0, page)
                self.assertEqual(shared, a.m.call(a.f[43], event))
                scored += shared != 0
        self.assertGreaterEqual(scored, 20)

    def test_doubles_events_still_open_the_team_select(self):
        a, css = self.a, 0x80497758
        memcard = a.word(a.m.cpu.reg_read(gpr(13)) - 30656)
        names = [a.name(d) for d in a.events(ALL_EVENTS)]
        for name, kinds in (('Grab Punish Practice', (0, 1, 1, 3)), ('Passing Practice', (0, 1, 1, 3)),
                            ('Ledgechase Training', (3, 3, 3, 3))):
            with self.subTest(event=name):
                a.set(PAGE, 0, ALL_EVENTS)
                a.m.cpu.mem_write(memcard + 0x535, bytes((names.index(name),)))
                a.m.cpu.mem_write(css, struct.pack('>H', 1))
                for door in range(4):
                    a.m.cpu.mem_write(css + 0x70 + door * 36, bytes((2, 3)))
                a.m.call(a.f[39], 2)  # Settings_EventCharacters(TM_CHARACTERS_OPEN)
                self.assertEqual(tuple(a.m.cpu.mem_read(css + 0x71 + door * 36, 1)[0] for door in range(4)), kinds)


class PageSwitchTests(unittest.TestCase):
    """Custom ESS Button Actions.asm's L/R page switch, with its drawing stubbed."""

    def setUp(self):
        a = self.a = AllEventsMachine()
        cpu, r13 = a.m.cpu, a.m.cpu.reg_read(gpr(13))
        cpu.mem_write(r13 - 0x4A40, struct.pack('>I', ESS))
        cpu.mem_write(ESS + 0x2C, struct.pack('>I', CURSOR))
        cpu.mem_write(ESS + 0x28, struct.pack('>I', ESS + 0x300))
        cpu.mem_write(r13 - 0x4EB4, struct.pack('>I', ESS + 0x400))  # The page name's text.
        a.m.install_hook(0x8024D92C, 0x80620000)
        self.pressed, self.calls = 0, []
        for address, name in STUBS.items():
            cpu.mem_write(address, struct.pack('>I', 0x4E800020))  # blr
            cpu.hook_add(UC_HOOK_CODE, self.stub, name, address, address)
        self.assertEqual(a.get(PAGE), 0)  # A new save starts on Minigames here.

    def stub(self, uc, address, size, name):
        if name == 'input':
            uc.reg_write(gpr(3), self.pressed)
        elif name == 'find_jobj':
            uc.mem_write(uc.reg_read(gpr(4)), struct.pack('>I', CURSOR_JOBJ))
        self.calls.append((name, uc.reg_read(gpr(3)), uc.reg_read(gpr(4)), uc.reg_read(gpr(5))))

    def press(self, button, row=0, scroll=0):
        """One frame on the screen with the cursor at row and scroll, pressing button."""
        a = self.a
        a.m.cpu.mem_write(CURSOR, bytes((row, 0, 0, 0)) + struct.pack('>I', scroll))
        a.m.cpu.mem_write(CURSOR_JOBJ + 0x3C, struct.pack('>f', 99))
        self.pressed, self.calls = button, []
        a.m.call(0x8024D92C, stop=0x8024D930, count=5000000)
        calls = lambda kind: [call for call in self.calls if call[0] == kind]
        self.assertEqual([(r3, r4) for _, r3, r4, _ in calls('row')], [(i, self.scroll() + i) for i in range(9)])
        self.assertEqual(len(calls('sfx')), 1)
        return {
            'page': a.get(PAGE), 'selection': a.get(SELECTION),
            'name': a.text(calls('page_name')[0][3]),
            'cursor': (self.row(), self.scroll()),
            'described': calls('description')[0][2],
            'scored': calls('score')[0][2],  # The high score shown, for this event.
            'y': round(struct.unpack('>f', a.m.cpu.mem_read(CURSOR_JOBJ + 0x3C, 4))[0], 4),
        }

    def row(self):
        return self.a.m.cpu.mem_read(CURSOR, 1)[0]

    def scroll(self):
        return self.a.word(CURSOR + 4)

    def assertShows(self, screen, page, selection, cursor):
        names = ('Minigames', 'General Tech', 'Character-specific Tech', 'Doubles', NAME)
        self.assertEqual((screen['page'], screen['name'], screen['selection'], screen['cursor'],
                          screen['described'], screen['scored']),
                         (page, names[page], selection, cursor, selection, selection))
        self.assertEqual(screen['y'], round(cursor[0] * ROW_HEIGHT, 4))

    def test_pages_wrap_with_all_events_left_of_minigames(self):
        for button, page in ((LEFT, 4), (LEFT, 3), (LEFT, 2), (LEFT, 1), (LEFT, 0), (LEFT, 4),
                             (RIGHT, 0), (RIGHT, 1), (RIGHT, 2), (RIGHT, 3), (RIGHT, 4), (RIGHT, 0)):
            self.assertShows(self.press(button), page, 0, (0, 0))

    def test_each_list_keeps_its_own_cursor(self):
        # Leaving Minigames' sixth event for All Events, which starts at the top.
        self.assertShows(self.press(LEFT, row=5), ALL_EVENTS, 0, (0, 0))
        # All Events' 31st event, scrolled to the bottom row, back to Minigames' sixth.
        self.assertShows(self.press(RIGHT, row=8, scroll=22), 0, 5, (5, 0))
        # And back: 30 tops the window but 29 is the last scroll, so it's row 1.
        self.assertShows(self.press(LEFT, row=5), ALL_EVENTS, 30, (1, 29))
        # Doubles wasn't the page All Events was entered from: the top.
        self.assertShows(self.press(LEFT, row=1, scroll=29), 3, 0, (0, 0))
        self.assertShows(self.press(RIGHT), ALL_EVENTS, 30, (1, 29))
        # Nor, now, is Minigames.
        self.assertShows(self.press(RIGHT, row=1, scroll=29), 0, 0, (0, 0))
        # A cursor short of the last nine tops the window: leave All Events at 12.
        self.assertShows(self.press(LEFT), ALL_EVENTS, 30, (1, 29))
        self.assertShows(self.press(LEFT, row=0, scroll=12), 3, 0, (0, 0))
        self.assertShows(self.press(RIGHT), ALL_EVENTS, 12, (0, 12))
        # The categorized pages still start at the top when you move between them.
        self.assertShows(self.press(RIGHT), 0, 0, (0, 0))
        self.assertShows(self.press(RIGHT, row=3), 1, 0, (0, 0))
        self.assertShows(self.press(LEFT, row=4, scroll=2), 0, 0, (0, 0))

    def test_a_saved_cursor_past_the_last_event_starts_at_the_top(self):
        a = self.a
        self.press(LEFT); self.press(RIGHT)  # Makes the extension record.
        word_80 = 0x8045A6C0 + 0x1AE0 + 80  # TYEX's words are little-endian.
        word = int.from_bytes(a.m.cpu.mem_read(word_80, 4), 'little') & ~(63 << 22) | 50 << 22
        a.m.cpu.mem_write(word_80, word.to_bytes(4, 'little'))
        self.assertShows(self.press(LEFT), ALL_EVENTS, 0, (0, 0))

    def test_both_arrows_always_show(self):
        a = self.a
        cpu, r13 = a.m.cpu, a.m.cpu.reg_read(gpr(13))
        a.m.install_hook(0x8024D84C, 0x80630000)
        for page in (0, 3, ALL_EVENTS):
            a.set(PAGE, 0, page)
            for offset, text in ((-0x4EB0, ESS + 0x500), (-0x4EAC, ESS + 0x600)):
                cpu.mem_write(r13 + offset, struct.pack('>I', text))
                cpu.mem_write(text + 0x4D, b'\x01')
            a.m.call(0x8024D84C, stop=0x8024D850)
            self.assertEqual((cpu.mem_read(ESS + 0x54D, 1)[0], cpu.mem_read(ESS + 0x64D, 1)[0]), (0, 0))


class PlayedFlagTests(unittest.TestCase):
    """The select screen shows a high score only for a played event, and the
    game marks an event played by its index on the page it's played from. Both
    go by the event's index on its own page, so All Events agrees with it."""

    def setUp(self):
        self.a = AllEventsMachine()
        self.index = [self.a.name(d) for d in self.a.events(ALL_EVENTS)].index

    def test_an_event_played_from_all_events_is_played_on_its_own_page(self):
        a = self.a
        a.m.install_hook(0x8015CEB4, 0x80640000)  # Events_SetEventAsPlayed
        flags = 0x8045A6C0 + 0x1A68  # 64 bits, event 0 the lowest.
        for page, event, home in ((ALL_EVENTS, self.index('Slalom'), 1),
                                  (ALL_EVENTS, self.index('Ledgechase Training'), 16),
                                  (ALL_EVENTS, self.index('Eggs-ercise'), 0),
                                  (1, 16, 16), (0, 4, 4)):
            with self.subTest(page=page, event=event):
                a.set(PAGE, 0, page)
                a.m.cpu.mem_write(flags, bytes(8))
                a.m.call(0x8015CEB4, event)
                self.assertEqual(int.from_bytes(a.m.cpu.mem_read(flags, 8), 'big'), 1 << home)

    def test_the_high_score_checks_the_played_bit_of_its_own_page(self):
        a, cpu = self.a, self.a.m.cpu
        a.m.install_hook(0x8024D644, 0x80650000)  # In the high score routine, after its score type.
        keep = [n for n in range(4, 32) if n not in (13, 27)]
        for page, event, home in ((ALL_EVENTS, self.index('Slalom'), 1),
                                  (ALL_EVENTS, self.index('Training Lab'), 0), (2, 9, 9)):
            a.set(PAGE, 0, page)
            for score_type, cr0 in ((0, 0x2), (1, 0x4)):  # EQ for a KO count, GT for a time.
                with self.subTest(page=page, event=event, score_type=score_type):
                    for n in keep:
                        cpu.reg_write(gpr(n), 100 + n)
                    cpu.reg_write(gpr(3), score_type)
                    cpu.reg_write(gpr(27), event)
                    cpu.reg_write(reg.UC_PPC_REG_CR, 0x08765432)
                    cpu.emu_start(0x8024D644, 0x8024D648, count=1000000)
                    self.assertEqual(cpu.reg_read(reg.UC_PPC_REG_PC), 0x8024D648)
                    self.assertEqual((cpu.reg_read(gpr(27)), cpu.reg_read(gpr(3))), (home, score_type))
                    self.assertEqual([cpu.reg_read(gpr(n)) for n in keep], [100 + n for n in keep])
                    self.assertEqual(cpu.reg_read(reg.UC_PPC_REG_CR), cr0 << 28 | 0x08765432 & 0x0FFFFFFF)


class OpenTests(unittest.TestCase):
    """The screen opening on All Events, through the native cursor initializer and its page hooks."""

    def test_the_screen_opens_where_all_events_was_left(self):
        a = AllEventsMachine()
        a.set(PAGE, 0, ALL_EVENTS)
        a.set(SELECTION, 0, 30)
        a.m.install_hook(0x8024E858, 0x80610000)  # Remember Selected Event
        a.m.call(0x8024E858, 0, 0, stop=0x8024E85C)
        self.assertEqual(a.m.cpu.reg_read(gpr(30)), 30)
        a.m.install_hook(0x8024E438, 0x80612000)
        a.m.install_hook(0x8024CFFC, 0x80614000)
        cursor = 0x8050A000
        for event, row, scroll in ((30, 1, 29), (37, 8, 29), (3, 0, 3), (0, 0, 0)):
            a.m.call(0x8024E420, cursor, event)
            self.assertEqual((a.m.cpu.mem_read(cursor, 1)[0], a.word(cursor + 4)), (row, scroll))


if __name__ == '__main__':
    unittest.main(verbosity=2)
