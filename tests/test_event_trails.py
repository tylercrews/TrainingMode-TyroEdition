"""Event trail precedence, paused edits, lifecycle and actual Ledgedash DAT wiring."""
import struct
import unittest
from unicorn import UC_HOOK_CODE
import test_settings as fixtures
from test_settings import Machine, ROOT


class TrailOverrideTests(unittest.TestCase):
    def setUp(self):
        self.m=Machine();self.m.init();self.m.call('TestTrailInit')
        self.option=0x80407000
        self.m.call('Settings_Set',11,1,1)
        self.m.call('Settings_Set',11,2,1)

    def choose(self, choice, bind=True):
        self.m.cpu.mem_write(self.option,struct.pack('>h',choice))
        if bind:self.m.call('Trails_SetOverride',self.option)

    def test_every_local_profile_overrides_both_globals_without_changing_preferences(self):
        m=self.m;before=m.record()
        for choice in range(7):
            self.choose(choice)
            m.call('TestTrailHit',1);m.call('TestTrailTick',10)
            self.assertEqual(m.call('TestTrailDraw'),int(choice!=0))
            m.call('TestTrailHit',0);m.call('TestTrailTick',11)
            self.assertEqual(m.call('TestTrailDraw'),int(choice not in (0,4)))
        self.assertEqual(m.record(),before)

    def test_paused_off_discards_old_history_and_overrides_legacy_local_configuration(self):
        m=self.m
        self.choose(1);m.call('TestTrailHit',1);m.call('TestTrailTick',10)
        self.assertEqual(m.call('TestTrailDraw'),1)
        self.choose(0,False)  # A paused menu edit: no setter or simulation tick.
        m.call('Trails_Configure',1,5)
        self.assertEqual(m.call('TestTrailDraw'),0)
        self.choose(1,False)
        self.assertEqual(m.call('TestTrailDraw'),0)
        m.call('TestTrailTick',11)
        self.assertEqual(m.call('TestTrailDraw'),1)

    def test_retry_keeps_off_and_scene_change_releases_it(self):
        m=self.m
        self.choose(0);m.call('Trails_Clear')
        m.call('TestTrailHit',1);m.call('TestTrailTick',10)
        self.assertEqual(m.call('TestTrailDraw'),0)
        m.call('Trails_SetOverride',0)
        m.call('TestTrailTick',11)
        self.assertEqual(m.call('TestTrailDraw'),1)
        self.choose(0);m.call('Trails_SceneChange')
        self.assertEqual(m.call('TestTrailDraw'),0)
        m.call('Trails_MatchStart');m.call('TestTrailTick',12)
        self.assertEqual(m.call('TestTrailDraw'),1)

    def test_invalid_choices_are_off(self):
        for choice in (-1,7,255):
            self.choose(choice)
            self.m.call('TestTrailHit',1);self.m.call('TestTrailTick',10)
            self.assertEqual(self.m.call('TestTrailDraw'),0)


class LedgedashTrailMenuTests(unittest.TestCase):
    dat_path=ROOT/'build/ledgedash-trails-test.dat'
    dat_root=b'evFunction'
    def setUp(self):
        fixtures.RelocatedDATSettingsTests.setUp(self)
        self.m.init();self.m.call('TestTrailInit')
        dat=self.dat_path.read_bytes()
        _,size,relocs,roots,refs=struct.unpack_from('>5I',dat)
        nodes=32+size+relocs*4;strings=nodes+(roots+refs)*8
        for i in range(roots):
            at,name=struct.unpack_from('>II',dat,nodes+i*8)
            if dat[strings+name:].split(b'\0',1)[0]==self.dat_root:
                _,_,_,exports,count,_=struct.unpack_from('>6I',dat,32+at);break
        self.exports=dict(struct.unpack_from('>II',dat,32+exports+i*8) for i in range(count))
        self.menu=self.word(self.base+self.exports[3])
        options=self.word(self.menu+8)
        self.row=None
        for i in range(self.m.cpu.mem_read(self.menu+4,1)[0]):
            option=options+44*i
            if self.text(self.word(option+12))=='Hitbox Trails':self.row=option
        self.assertIsNotNone(self.row)

    def word(self, address):return struct.unpack('>I',self.m.cpu.mem_read(address,4))[0]
    def text(self, address):return bytes(self.m.cpu.mem_read(address,90)).split(b'\0',1)[0].decode()

    def test_real_menu_callback_overrides_global_and_off(self):
        m=self.m
        self.assertEqual(bytes(m.cpu.mem_read(self.row+6,4)),bytes(4))
        self.assertEqual(struct.unpack('>H',m.cpu.mem_read(self.row+4,2))[0],7)
        names=self.word(self.row+32)
        self.assertEqual([self.text(self.word(names+4*i)) for i in range(7)],
                         ['Off','Normal','Fast','Very Fast','Instant','Slow','No Fade'])
        m.symbols['DAT_ChangeTrails']=self.word(self.row+36)
        m.call('Settings_Set',11,1,1);before=m.record()
        for choice in (0,4,1,6,0):
            m.cpu.mem_write(self.row+6,struct.pack('>h',choice))
            m.call('DAT_ChangeTrails',0,choice)
            m.call('TestTrailHit',1);m.call('TestTrailTick',10)
            self.assertEqual(m.call('TestTrailDraw'),int(choice!=0))
        self.assertEqual(m.record(),before)

    def test_ledgedash_help_is_a_submenu_with_release_cue_and_actionable_guidance(self):
        options = self.word(self.menu + 8)
        rows = [options + 44*i for i in range(self.m.cpu.mem_read(self.menu+4, 1)[0])]
        help_row = next(row for row in rows if self.text(self.word(row+12)) == 'Help')
        self.assertEqual(self.m.cpu.mem_read(help_row, 1)[0], 0)  # OPTKIND_MENU.
        submenu = self.word(help_row+32)
        self.assertEqual(self.text(self.word(submenu)), 'Ledgedash Help')
        count = self.m.cpu.mem_read(submenu+4, 1)[0]
        self.assertEqual(count, 9)
        help_options = self.word(submenu+8)
        descriptions = []
        for i in range(count):
            row = help_options+44*i
            lines = [self.text(self.word(row+16+4*j)) for j in range(4)]
            self.assertTrue(all(0 < len(line) <= 60 for line in lines))
            descriptions.extend(lines)
        text = ' '.join(descriptions)
        for guidance in ('grabbing hand', 'before you can drop', 'both sticks to neutral',
                         'collision box', 'landing lag', 'zero GALINT', 'Peach', 'Tips'):
            self.assertIn(guidance, text)

    def test_init_binds_event_override_before_gameplay_dependencies(self):
        m=self.m;m.call('Settings_Set',11,1,1)
        # Event_Init loads its existing preferences, installs overrides, then
        # allocates cleanup/HUD objects. Stop at the first allocation boundary.
        m.symbols['DAT_InitLedgedash']=self.base+self.exports[0]
        reached=[]
        def stop(cpu,address,size,user):reached.append(address);cpu.emu_stop()
        link=(ROOT/'MexTK/melee.link').read_text()
        for name in ('sprintf','strcpy'):
            native=int(next(line.split(':')[0] for line in link.splitlines() if line.endswith(':'+name)),16)
            target=m.symbols[name]
            m.cpu.mem_write(native,struct.pack('>4I',0x3D800000|(target>>16),
                0x618C0000|(target&65535),0x7D8903A6,0x4E800420))
        create=int(next(line.split(':')[0] for line in link.splitlines() if line.endswith(':GObj_Create')),16)
        hook=m.cpu.hook_add(UC_HOOK_CODE,stop,begin=create,end=create)
        m.cpu.reg_write(fixtures.gpr(1),0x80400000)
        m.cpu.reg_write(fixtures.gpr(3),0x80407300)
        m.cpu.reg_write(fixtures.reg.UC_PPC_REG_LR,fixtures.STOP)
        try:m.cpu.emu_start(m.symbols['DAT_InitLedgedash'],fixtures.STOP,count=300000)
        finally:m.cpu.hook_del(hook)
        self.assertEqual(reached,[create])
        m.call('TestTrailHit',1);m.call('TestTrailTick',10)
        self.assertEqual(m.call('TestTrailDraw'),0)


if __name__=='__main__':unittest.main(verbosity=2)
