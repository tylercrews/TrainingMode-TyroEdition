"""Execute the production shared menu handler and inspect every event's DAT wiring."""
import re
import struct
import subprocess
import unittest
from pathlib import Path
from test_menu import Machine, ROOT, UC_HOOK_CODE, reg

A, START, L, R = 0x100, 0x1000, 0x40, 0x20
COMBO = A | START | L | R


def build():
    compiler = Path('C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe')
    exports = re.findall(r'\b(TestMenuExit\w+)\(', (ROOT / 'tests/menu_exit_probe.c').read_text())
    subprocess.run([str(compiler) if compiler.exists() else 'powerpc-eabi-gcc',
        '-O2', '-fno-inline', '-w', '-ffreestanding', '-fno-builtin',
        '-msdata=none', '-mcpu=750', '-nostdlib', '-ffunction-sections', '-fdata-sections',
        '-Wl,-Ttext=0x101000,-e,TestMenuExitInit,--gc-sections,--unresolved-symbols=ignore-all',
        *['-Wl,--undefined=' + name for name in exports],
        'tests/menu_exit_probe.c', 'src/menu.c', 'src/events.c',
        '-o', 'build/menu-exit-test.elf'], cwd=ROOT, check=True)


class MenuExitTests(unittest.TestCase):
    def setUp(self):
        self.m = Machine('menu-exit-test.elf', 'TestMenuExitInit')
        # Rendering/navigation are outside this input test. Pause/unpause and
        # custom-menu destruction execute their real production functions.
        def skip(cpu, address, size, user):
            cpu.reg_write(reg.UC_PPC_REG_PC, cpu.reg_read(reg.UC_PPC_REG_LR))
        for name in ('EventMenu_UpdateText', 'EventMenu_MenuThink'):
            address = self.m.symbols[name]
            self.m.cpu.hook_add(UC_HOOK_CODE, skip, begin=address, end=address)

    def state(self, paused=0, nested=0, custom=0, timer=10):
        self.m.call('TestMenuExitState', paused, nested, custom, timer)

    def pad(self, port=0, held=COMBO, down=START, left=0, right=0):
        self.m.call('TestMenuExitPad', port, held, down, left << 8 | right)

    def update(self):
        self.m.call('TestMenuExitUpdate')
        return [self.m.call('TestMenuExitResult', i) for i in range(6)]

    def test_every_port_gameplay_nested_and_custom_menus(self):
        for port in range(4):
            for paused, nested, custom in ((0, 0, 0), (1, 0, 0), (1, 1, 0), (1, 1, 1)):
                with self.subTest(port=port, paused=paused, nested=nested, custom=custom):
                    self.setUp()
                    self.state(paused, nested, custom)
                    self.pad(port)
                    exits, saves, destroyed, thinks, mode, _ = self.update()
                    self.assertEqual((exits, saves, destroyed, thinks, mode), (1, 1, custom, 0, 0))

    def test_analog_mixed_and_held_chord_without_new_start(self):
        for held, left, right in ((A | START, 43, 43), (A | START | L, 0, 43),
                                  (A | START | R, 43, 0), (COMBO, 0, 0)):
            with self.subTest(held=held):
                self.setUp()
                self.pad(held=held, down=0, left=left, right=right)
                self.assertEqual(self.update()[:2], [1, 1])

    def test_missing_buttons_and_below_threshold_do_not_exit(self):
        for held, left, right in [(COMBO & ~button, 0, 0) for button in (A, START, L, R)] + [
                (A | START, 42, 43), (A | START, 43, 42)]:
            with self.subTest(held=held, left=left, right=right):
                self.setUp()
                self.pad(held=held, down=0, left=left, right=right)
                self.assertEqual(self.update()[:2], [0, 0])

    def test_buttons_cannot_be_combined_across_ports(self):
        self.pad(0, A | START, 0)
        self.pad(1, L | R, 0)
        self.assertEqual(self.update()[:2], [0, 0])

    def test_start_still_pauses_and_unpauses_with_original_entry_guard(self):
        self.state(timer=0)
        self.pad(2, START, START)
        self.assertEqual(self.update()[4], 0)
        self.state(timer=10)
        self.assertEqual(self.update()[4:], [1, 2])
        self.state(paused=1, timer=0)
        result = self.update()
        self.assertEqual((result[0], result[4]), (0, 0))

    def test_every_production_event_reuses_its_exit_option_callback(self):
        names = ('lab', 'lcancel', 'ledgedash', 'wavedash', 'powershield', 'dthrowknee',
                 'edgeguard', 'fc', 'sweetspot', 'lowlaser', 'laserland', 'eggs',
                 'techchase', 'ledgechase', 'slalom', 'eventMenu')
        for name in names:
            with self.subTest(event=name):
                dat = (ROOT / 'build' / (name + '.dat')).read_bytes()
                _, size, relocations, roots, refs = struct.unpack_from('>5I', dat)
                nodes = 32 + size + relocations * 4
                strings = nodes + (roots + refs) * 8
                for i in range(roots):
                    offset, label = struct.unpack_from('>II', dat, nodes + i * 8)
                    if dat[strings + label:].split(b'\0', 1)[0] == (b'tmFunction' if name == 'eventMenu' else b'evFunction'):
                        code, table, count, exports, export_count, payload_size = struct.unpack_from('>6I', dat, 32 + offset)
                        break
                payload = bytearray(dat[32 + code:32 + code + payload_size])
                # Only pointer relocations are needed to inspect these menus.
                pointer_relocations = set()
                for i in range(count):
                    word, target = struct.unpack_from('>II', dat, 32 + table + i * 8)
                    if word >> 24 == 1:
                        offset = word & 0xFFFFFF
                        pointer_relocations.add(offset)
                        struct.pack_into('>I', payload, offset, target)
                public = dict(struct.unpack_from('>II', dat, 32 + exports + i * 8) for i in range(export_count))
                def word(at): return struct.unpack_from('>I', payload, at)[0]
                def callback_pointer(at):
                    target = word(at)
                    # A function at code offset zero becomes a non-null pointer
                    # when the native loader relocates the DAT into MEM1.
                    if at in pointer_relocations and target & 0xF0000000 != 0x80000000:
                        return 0x80700000 + target
                    return target
                if name == 'eventMenu':
                    title = payload.index(b'Under Fire - Ledgestall Training\0')
                    menus = [at for at in range(0, len(payload) - 52, 4)
                             if word(at) == title and payload[at + 4] == 7]
                    self.assertEqual(len(menus), 1)
                    root = menus[0]
                else:
                    root = word(public[3])
                callback = callback_pointer(root + 48)
                self.assertNotEqual(callback, 0)
                options = word(root + 8)
                exits = []
                for i in range(payload[root + 4]):
                    option = options + i * 44
                    text = payload[word(option + 12):].split(b'\0', 1)[0]
                    if text == b'Exit': exits.append(callback_pointer(option + 40))
                self.assertEqual(exits, [callback])

    def test_legacy_gameplay_and_pause_exit_on_every_port(self):
        for paused in (0, 1, 2):
            for port in range(4):
                with self.subTest(paused=paused, port=port):
                    self.setUp()
                    self.m.call('TestMenuExitLegacyInit', paused)
                    self.pad(port, A | START, 0, left=43, right=43)
                    self.m.call('TestMenuExitLegacyUpdate')
                    self.assertEqual(self.m.call('TestMenuExitResult', 6), 1)
                    self.assertEqual(self.m.call('TestMenuExitResult', 7), 3)
                    self.assertEqual(self.m.call('TestMenuExitResult', 1), 1)
                    self.assertEqual(self.m.call('TestMenuExitResult', 8), 0)
                    # A held combo must not end the match or save twice.
                    self.m.call('TestMenuExitLegacyUpdate')
                    self.assertEqual(self.m.call('TestMenuExitResult', 6), 1)
                    self.assertEqual(self.m.call('TestMenuExitResult', 1), 1)

    def test_legacy_incomplete_chords_leave_gameplay_and_develop_controls_active(self):
        self.m.call('TestMenuExitLegacyInit', 0)
        for button in (A, START, L, R):
            self.pad(held=COMBO & ~button)
            self.m.call('TestMenuExitLegacyUpdate')
        self.assertEqual(self.m.call('TestMenuExitResult', 6), 0)
        self.assertEqual(self.m.call('TestMenuExitResult', 1), 0)
        self.assertEqual(self.m.call('TestMenuExitResult', 8), 4)

    def test_every_legacy_event_load_installs_the_shared_handler(self):
        globals_text = (ROOT / 'ASM/Globals.s').read_text()
        jump_table = globals_text.split('.macro EventJumpTable', 1)[1].split('.endm', 1)[0]
        legacy_events = re.findall(r'^\s+bl (\w+)', jump_table, re.M)
        assembly = (ROOT / 'ASM/training-mode/Custom Events/Custom Event Code - Rewrite.asm').read_text()
        self.assertEqual(len(legacy_events), 15)  # Includes Under Fire's menu-backed legacy event.
        for event in legacy_events:
            with self.subTest(event=event):
                self.assertRegex(assembly, event + r'Load:\s+blrl\s+backup\s+rtocbl r12, TM_StartOSDs')


if __name__ == '__main__':
    build()
    unittest.main(verbosity=2)
