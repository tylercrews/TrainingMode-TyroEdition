"""Execute Under Fire's relocated shared DAT, regrab hook and native reset."""
import struct
import unittest
from unicorn import UC_HOOK_CODE
import test_settings as fixtures
from test_settings import ROOT, gpr, reg


class UnderFireTests(unittest.TestCase):
    dat_path = fixtures.RelocatedDATSettingsTests.dat_path

    def setUp(self):
        fixtures.RelocatedDATSettingsTests.setUp(self)
        m = self.m
        m.init(); m.call('TestCueInit')
        self.fighter = m.call('TestCueObject', 0)
        self.ft = m.call('TestCueData', 0)
        self.menu_obj, self.menu_data = 0x80407600, 0x80407800
        m.cpu.mem_write(self.menu_obj + 0x2C, struct.pack('>I', self.menu_data))
        self.paused = 0
        self.texts, self.rows, self.colors, self.effects, self.stage_calls = [], [], [], [], []
        self.subtexts = {}
        start = m.symbols['DAT_UnderFire']
        for pc in range(start, start + 512, 4):
            word = self.word(pc)
            if word & 0xFC000003 == 0x48000001:
                delta = word & 0x03FFFFFC
                if delta & 0x02000000: delta -= 0x04000000
                self.create_menu = pc + delta
                break
        else: self.fail('Menu initialization call missing')
        self.hook = m.cpu.hook_add(UC_HOOK_CODE, self.dependencies)
        m.call('DAT_UnderFire')
        self.options = self.word(self.menu + 8)
        # Run the actual native effect-removal helper, bounding only the effect
        # manager. This verifies clearing the move-effect spawn flag as well.
        dol = (ROOT / 'build/Start.dol').read_bytes()
        for i in range(18):
            off, base, size = [struct.unpack_from('>I', dol, at + 4*i)[0] for at in (0,0x48,0x90)]
            if base <= 0x8007DB24 < base + size:
                pos = off + 0x8007DB24 - base
                m.cpu.mem_write(0x8007DB24, dol[pos:pos+0x34]); break
        for slot, name in ((46,'DAT_UnderFireControl'), (47,'DAT_UnderFireRegrab'), (48,'DAT_UnderFireCamera')):
            m.cpu.mem_write(0x80300000 - 200 + slot*4, struct.pack('>I', m.symbols[name]))

    def tearDown(self): self.m.cpu.hook_del(self.hook)
    def word(self, address): return struct.unpack('>I', self.m.cpu.mem_read(address, 4))[0]
    def string(self, address): return bytes(self.m.cpu.mem_read(address,100)).split(b'\0',1)[0].decode()
    def option(self, row, value=None):
        address = self.options + row*44
        if value is not None: self.m.cpu.mem_write(address+6, struct.pack('>h', value))
        return address

    def dependencies(self, cpu, address, size, user):
        r = lambda n: cpu.reg_read(gpr(n))
        if address == self.create_menu:
            self.menu = r(3); cpu.reg_write(gpr(3), self.menu_obj)
        elif address == 0x8003418C: cpu.reg_write(gpr(3), 0)
        elif address == 0x801A45E8: cpu.reg_write(gpr(3), self.paused)
        elif address == 0x803A611C: cpu.reg_write(gpr(3), 7)
        elif address == 0x803A6754:
            text = 0x80409000 + len(self.texts)*0x400
            self.texts.append(text); self.subtexts[text] = 0
            cpu.reg_write(gpr(3), text)
        elif address == 0x803A6B98:
            row = self.subtexts[r(3)]; self.subtexts[r(3)] += 1
            cpu.reg_write(gpr(3), row)
        elif address == 0x803A70A0:
            value = r(6); value = value - 0x100000000 if value & 0x80000000 else value
            self.rows.append((r(3), r(4), self.string(r(5)), value))
        elif address == 0x803A74F0:
            self.colors.append((r(3), r(4), bytes(cpu.mem_read(r(5),4))))
        elif address == 0x8005B880: self.effects.append(r(3))
        elif address in (0x80053ECC, 0x80053DA4):
            self.stage_calls.append((address,r(3)))
            x = -100 if address == 0x80053ECC else 100
            cpu.mem_write(r(4), struct.pack('>3f',x,10,0))
        else: return
        cpu.reg_write(reg.UC_PPC_REG_PC, cpu.reg_read(reg.UC_PPC_REG_LR))

    def control(self, reset=0): return self.m.call('DAT_UnderFireControl', self.fighter, reset)
    def protection(self, frames): self.m.cpu.mem_write(self.ft+0x1990, struct.pack('>i',frames))

    def test_defaults_menu_and_dpad_share_side_and_full_reset_request(self):
        m = self.m; core = m.record()
        self.assertEqual(m.cpu.mem_read(self.menu+4,1)[0],6)
        for row, name, value in ((2,'Ledge Side',0),(3,'Ledge Invincibility',1),(4,'Disable Move Particles',0)):
            at = self.option(row)
            self.assertEqual(self.string(self.word(at+12)),name)
            self.assertEqual(bytes(m.cpu.mem_read(at+6,4)),struct.pack('>hh',value,value))
        self.assertEqual(self.control(),0)
        m.cpu.mem_write(self.ft+0x668,struct.pack('>I',2))
        self.assertEqual(self.control(),3)  # Right plus restart request.
        self.assertEqual(bytes(m.cpu.mem_read(self.option(2)+6,4)),b'\0\1\0\1')
        m.cpu.mem_write(self.ft+0x668,bytes(4))
        self.assertEqual(self.control(),3)  # Pending until the native spawn.
        self.assertEqual(self.control(1),1)
        self.assertEqual(self.control(),1)
        self.option(2,0)
        m.symbols['ChangeSide'] = self.word(self.option(2)+36)
        m.call('ChangeSide',self.menu_obj,0)
        self.assertEqual(self.control(),2)
        self.assertEqual(self.control(1),0)
        self.paused = 2
        m.cpu.mem_write(self.ft+0x668,struct.pack('>I',2))
        self.assertEqual(self.control(),0)  # Paused menu navigation cannot switch ledges.
        self.assertEqual(m.record(),core)

    def test_camera_mirrors_with_native_brinstar_ledge_ids(self):
        m = self.m; box = 0x80408000
        m.call('DAT_UnderFireCamera',box)
        self.assertEqual(self.stage_calls[-1],(0x80053ECC,5))
        self.assertEqual(struct.unpack('>3f',m.cpu.mem_read(box+16,12)),(-65,30,0))
        self.option(2,1); self.control(1)
        self.assertEqual(self.stage_calls[-1],(0x80053DA4,17))
        self.assertEqual(struct.unpack('>3f',m.cpu.mem_read(box+16,12)),(65,30,0))

    def test_signed_regrab_result_counts_tangible_frames_and_respects_toggle(self):
        m = self.m
        self.protection(7); self.control()
        m.call('DAT_UnderFireRegrab',self.fighter)
        self.assertEqual(self.rows[-1][2:],('%+df',7))
        self.assertEqual(self.colors[-1][2],bytes((80,255,90,255)))
        self.protection(0)
        # No earlier tangible frame: this regrab is the first late frame.
        m.call('DAT_UnderFireRegrab',self.fighter)
        self.assertEqual(self.rows[-1][3],-1)
        for _ in range(4): self.control()
        m.call('DAT_UnderFireRegrab',self.fighter)
        self.assertEqual(self.rows[-1][3],-5)
        self.assertEqual(self.colors[-1][2],bytes((255,40,40,255)))
        count = len(self.rows); self.option(3,0)
        m.call('DAT_UnderFireRegrab',self.fighter)
        self.assertEqual(len(self.rows),count)
        self.option(3,1); self.control(1); self.protection(0)
        m.call('DAT_UnderFireRegrab',self.fighter)
        self.assertEqual(self.rows[-1][3],-1)  # Retry clears timing history.

    def test_native_regrab_hook_runs_when_global_ledge_osd_is_off(self):
        m = self.m
        m.call('Settings_Set',14,26,0)
        m.cpu.reg_write(gpr(13),0x804DB6A0)
        m.cpu.mem_write(0x804DB6A0-0x77C0,struct.pack('>I',0x8045A6C0))
        self.protection(12)
        hook = m.install_hook(0x80081370,0x80600000)
        m.cpu.reg_write(gpr(3),self.fighter)
        m.cpu.reg_write(gpr(30),self.fighter)
        m.cpu.reg_write(gpr(31),self.ft)
        m.cpu.emu_start(hook,0x80081374,count=300000)
        self.assertEqual(m.cpu.reg_read(reg.UC_PPC_REG_PC),0x80081374)
        self.assertEqual(self.rows[-1][2:],('%+df',12))
        self.assertEqual(m.cpu.reg_read(gpr(30)),self.fighter)
        self.assertEqual(m.cpu.reg_read(gpr(3)),self.fighter)

    def test_native_right_shortcut_runs_complete_restore_and_selected_spawn(self):
        m = self.m
        elf = (ROOT/'build/under-fire-native-test.elf').read_bytes()
        shoff = struct.unpack_from('>I',elf,0x20)[0]
        stride,count = struct.unpack_from('>HH',elf,0x2E)
        sections = [struct.unpack_from('>10I',elf,shoff+i*stride) for i in range(count)]
        symbols = {}
        for _,kind,flags,address,offset,size,link,_,_,entry in sections:
            if flags & 2 and size and kind != 8: m.cpu.mem_write(address,elf[offset:offset+size])
            if kind == 2:
                names = sections[link]
                table = elf[names[4]:names[4]+names[5]]
                for i in range(0,size,entry):
                    name,value,_,_,_,section = struct.unpack_from('>IIIBBH',elf,offset+i)
                    if section: symbols[table[name:].split(b'\0',1)[0].decode()] = value
        m.symbols['NativeThink'] = symbols['LedgeStallThink']+4
        event,state = 0x80407200,0x80407400
        m.cpu.mem_write(event+0x2C,struct.pack('>I',state))
        m.cpu.mem_write(state,struct.pack('>BBHIf',0,0,123,0,-100))
        calls = []
        bound = {symbols[n]:n for n in ('GetAllPlayerPointers','CheckIfFirstFrame','SaveState_Load',
            'PlaceOnLedge','UpdateCameraBox','IntToFloat','Ledgestall_UpdateLavaPosition')}
        def native(cpu,address,size,user):
            name = bound.get(address)
            if not name: return
            if name == 'GetAllPlayerPointers':
                for n,v in ((3,self.fighter),(4,self.ft),(5,0),(6,0)):cpu.reg_write(gpr(n),v)
            elif name == 'CheckIfFirstFrame': cpu.reg_write(gpr(3),0)
            elif name == 'IntToFloat':
                v = cpu.reg_read(gpr(3));v = v-0x100000000 if v & 0x80000000 else v
                cpu.reg_write(reg.UC_PPC_REG_FPR1,struct.unpack('>Q',struct.pack('>d',float(v)))[0])
            else:
                calls.append((name,cpu.reg_read(gpr(4))))
                if name == 'SaveState_Load':cpu.mem_write(self.ft+0x668,bytes(4))
            cpu.reg_write(reg.UC_PPC_REG_PC,cpu.reg_read(reg.UC_PPC_REG_LR))
        hook = m.cpu.hook_add(UC_HOOK_CODE,native)
        try:
            m.cpu.mem_write(self.ft+0x668,struct.pack('>I',2))
            m.call('NativeThink',event)
            self.assertEqual([n for n,_ in calls],['SaveState_Load','PlaceOnLedge','UpdateCameraBox',
                                                  'Ledgestall_UpdateLavaPosition'])
            self.assertEqual(calls[1],('PlaceOnLedge',1))
            self.assertEqual(bytes(m.cpu.mem_read(state,12)),struct.pack('>BBHIf',0,0,123,0,-100))
            self.assertEqual(bytes(m.cpu.mem_read(0x8046B6C8,6)),bytes(6))
            self.assertEqual(self.control(),1)  # No repeat restore after the shortcut.
        finally:m.cpu.hook_del(hook)

    def test_move_particles_default_off_and_native_cleanup_when_enabled(self):
        m = self.m; original = bytes(m.cpu.mem_read(self.ft+0x10,4))
        self.control(); self.assertEqual(self.effects,[])
        self.option(4,1)
        for kind in (1,22):  # Fox and Falco, using the same native FX cleanup.
            m.cpu.mem_write(self.ft+4,struct.pack('>I',kind))
            m.cpu.mem_write(self.ft+0x2219,b'\x80')  # Native move-effect-created flag.
            self.control()
            self.assertEqual(self.effects[-1],self.fighter)
            self.assertEqual(bytes(m.cpu.mem_read(self.ft+0x2219,1)),b'\0')
        count = len(self.effects); self.option(4,0); self.control()
        self.assertEqual(len(self.effects),count)
        self.assertEqual(bytes(m.cpu.mem_read(self.ft+0x10,4)),original)


if __name__ == '__main__': unittest.main(verbosity=2)
