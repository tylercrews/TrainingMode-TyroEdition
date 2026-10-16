"""Execute event angle capture/publication and shared HUD GX output on PowerPC."""
from pathlib import Path
import struct
import subprocess
import unittest

from test_settings import Machine, ROOT, gpr, reg
from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_WRITE


def build(kind):
    compiler = Path('C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe')
    subprocess.run([str(compiler) if compiler.exists() else 'powerpc-eabi-gcc',
        '-O2', '-w', '-ffreestanding', '-fno-builtin', '-msdata=none', '-mcpu=750', '-nostdlib',
        '-ffunction-sections', '-fdata-sections', '-DSTICK_HUD_' + kind,
        '-Wl,-Ttext=0x101000,-e,TestHUDInit,--gc-sections,--unresolved-symbols=ignore-all',
        *['-Wl,--undefined=' + name for name in ('TestHUDInit', 'TestHUDDraw',
          *(('TestHUDSample', 'TestHUDPublish', 'TestHUDRetry', 'TestHUDDirection', 'TestHUDRow')
            if kind != 'SHARED' else ()))],
        'tests/stick_hud_probe.c', 'src/osd_style.c', 'src/ledgedash_logic.c',
        '-o', 'build/stick-hud-' + kind.lower() + '.elf'], cwd=ROOT, check=True)


def machine(kind):
    m = Machine()
    data = (ROOT / ('build/stick-hud-' + kind.lower() + '.elf')).read_bytes()
    at = struct.unpack_from('>I', data, 0x20)[0]
    stride, count = struct.unpack_from('>HH', data, 0x2E)
    sections = [struct.unpack_from('>10I', data, at + i * stride) for i in range(count)]
    for _, section_kind, flags, address, offset, size, link, _, _, entry in sections:
        if flags & 2 and size:
            m.cpu.mem_write(address, bytes(size) if section_kind == 8 else data[offset:offset+size])
        if section_kind == 2:
            strings = sections[link]
            names = data[strings[4]:strings[4]+strings[5]]
            for i in range(0, size, entry):
                name, value, *_ = struct.unpack_from('>IIIBBH', data, offset+i)
                m.symbols[names[name:].split(b'\0', 1)[0].decode()] = value
    m.call('TestHUDInit')
    return m


class EventStickHUDTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for kind in ('WAVEDASH', 'LEDGEDASH', 'SHARED'): build(kind)

    def test_both_events_keep_signed_snapshot_paired_with_published_number_across_retries(self):
        for kind, row in (('WAVEDASH', 1), ('LEDGEDASH', 0)):
            for x, y in ((95,-29), (-95,-29), (95,29), (-95,29)):
                with self.subTest(event=kind, x=x, y=y):
                    m = machine(kind)
                    def drawn():
                        m.call('TestHUDDraw', 0, 0)
                        return tuple(v if v < 0x80000000 else v-0x100000000 for v in
                                     (m.call('TestHUDDirection', 0), m.call('TestHUDDirection', 1)))
                    self.assertEqual(drawn(), (0,0))
                    m.call('TestHUDSample', x, y)
                    self.assertEqual(drawn(), (0,0))  # Number still belongs to the prior result.
                    m.call('TestHUDPublish')
                    self.assertEqual(drawn(), (x,y))
                    self.assertEqual(m.call('TestHUDRow'), row)
                    m.call('TestHUDRetry'); m.call('TestHUDSample', -x, -y)
                    self.assertEqual(drawn(), (x,y))
                    m.call('TestHUDPublish')
                    self.assertEqual(drawn(), (-x,-y))

    def test_shared_renderer_preserves_font_size_draws_after_panel_and_respects_hidden_hud(self):
        m = machine('SHARED'); m.cpu.mem_map(0xCC008000, 0x1000)
        batches, labels, values = [], [], []
        def boundary(cpu, address, size, user):
            if address == m.symbols['GFX_Start']:
                batches.append(cpu.reg_read(gpr(3)))
            elif address == m.symbols['HUD_DrawTextEx']:
                pointer = cpu.reg_read(gpr(3))
                text = bytes(cpu.mem_read(pointer, 32)).split(b'\0',1)[0]
                labels.append((text, cpu.reg_read(reg.UC_PPC_REG_FPR1)))
            else: return
            cpu.reg_write(reg.UC_PPC_REG_PC, cpu.reg_read(reg.UC_PPC_REG_LR))
        def fifo(cpu, access, address, size, value, user):
            if size == 4: values.append(struct.unpack('>f', struct.pack('>I', value))[0])
        m.cpu.hook_add(UC_HOOK_CODE, boundary)
        m.cpu.hook_add(UC_HOOK_MEM_WRITE, fifo, begin=0xCC008000, end=0xCC008003)
        for row in (0,1):
            batches.clear(); labels.clear(); values.clear()
            m.call('TestHUDDraw', row, 0)
            self.assertEqual(batches, [64,16,2])  # Finish every panel vertex before starting the icon.
            self.assertEqual(len(labels), 5)
            self.assertEqual([text for text,_ in labels],
                [b'20.00',b'Timing',b'Perfect',b'Rate',b'50%'] if row == 0 else
                [b'Angle',b'20.00',b'Perfect',b'Rate',b'50%'])
            self.assertTrue(all(size == struct.unpack('>Q',struct.pack('>d',.5))[0] for _,size in labels))
            cx,cy,_ = values[-6:-3]; tx,ty,_ = values[-3:]
            self.assertAlmostEqual(cx,19.65,places=4)
            self.assertAlmostEqual(cy,11.5-row*5.5,places=4)
            self.assertLess(tx,cx); self.assertLess(ty,cy)
        batches.clear(); labels.clear(); values.clear()
        m.call('TestHUDDraw', -1, 0)
        self.assertEqual(batches,[64]); self.assertEqual(len(labels),6)
        batches.clear(); labels.clear(); values.clear()
        m.call('TestHUDDraw', 0, 1)
        self.assertEqual((batches,labels,values),([],[],[]))


if __name__ == '__main__': unittest.main(verbosity=2)
