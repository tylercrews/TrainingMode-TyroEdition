"""Exercise the optimized menu adapter and the original native attack routine."""
import re
import struct
import subprocess
import sys
import unittest
from unicorn import UC_HOOK_CODE
import test_settings as fixtures
from test_settings import ROOT, gpr, reg


def build():
    # Test-only exports expose the production entrypoints without changing the
    # shipped 50-slot ABI. Use exactly the shared module's production sources.
    table = ROOT / 'build/shield-training-exports.txt'
    table.write_text((ROOT / 'MexTK/tmFunction.txt').read_text().rstrip() +
        '\nShieldTraining_Init\nShieldTraining_Control\nActionCues_Clear\nActionCues_MatchStart\nActionCues_SceneChange\n')
    sources = re.search(r'queue_mex_build "tmFunction" "build/eventMenu.dat" "([^"]+)"',
        (ROOT / 'build.sh').read_text())[1].split()
    hmex='hmex.exe' if sys.platform=='win32' else 'hmex_macos_arm64' if sys.platform=='darwin' else 'hmex'
    subprocess.run([str(ROOT / 'bin' / hmex), '-q', '-l', 'MexTK/melee.link', '-f', '-O2 -w',
        '-s', 'tmFunction', '-t', str(table), '-o', 'build/shield-training-test.dat',
        '-i', *sources, '-dat', 'dats/eventMenu.dat'], cwd=ROOT, check=True)


class ShieldTrainingTests(unittest.TestCase):
    dat_path = ROOT / 'build/shield-training-test.dat'

    def setUp(self):
        fixtures.RelocatedDATSettingsTests.setUp(self)
        m = self.m
        m.init(); m.call('TestCueInit')
        dat = self.dat_path.read_bytes()
        _, size, reloc, roots, refs = struct.unpack_from('>5I', dat)
        nodes = 32 + size + reloc*4
        strings = nodes + (roots+refs)*8
        for i in range(roots):
            at, name = struct.unpack_from('>II', dat, nodes+i*8)
            if dat[strings+name:].split(b'\0',1)[0] == b'tmFunction':
                _, _, _, exports, count, _ = struct.unpack_from('>6I', dat, 32+at)
                break
        names = {1:'Desc', 20:'FileLoad', 50:'Init', 51:'Control', 52:'Clear', 53:'MatchStart', 54:'SceneChange'}
        for i in range(count):
            slot, offset = struct.unpack_from('>II', dat, 32+exports+i*8)
            if slot in names: m.symbols[names[slot]] = self.base+offset
        # The first direct call in Under Fire's initializer is EventMenu_Init.
        start = m.symbols['DAT_UnderFire']
        for pc in range(start,start+1024,4):
            word = self.word(pc)
            if word & 0xFC000003 == 0x48000001:
                delta = word & 0x03FFFFFC
                if delta & 0x02000000: delta -= 0x04000000
                self.create_menu = pc+delta
                break
        self.procs = {}
        self.native_calls = []
        self.native_bound = {}
        self.first_frame=0
        self.platform=88
        self.cue_objects=[m.call('TestCueObject',slot) for slot in range(12)]
        self.cpu_obj = m.call('TestCueObject',2)
        self.cpu_ft = m.call('TestCueData',2)
        self.human_obj = m.call('TestCueObject',0)
        self.human_ft = m.call('TestCueData',0)
        self.hook = m.cpu.hook_add(UC_HOOK_CODE,self.dependencies)
        m.call('FileLoad',0)

    def tearDown(self): self.m.cpu.hook_del(self.hook)
    def word(self, at): return struct.unpack('>I',self.m.cpu.mem_read(at,4))[0]
    def put(self, at, value): self.m.cpu.mem_write(at,struct.pack('>I',value & 0xFFFFFFFF))
    def string(self, at): return bytes(self.m.cpu.mem_read(at,100)).split(b'\0',1)[0].decode()
    def option(self, row): return self.options + row*44
    def value(self, row): return struct.unpack('>h',self.m.cpu.mem_read(self.option(row)+6,2))[0]
    def edit(self, row, value):
        self.m.cpu.mem_write(self.option(row)+6,struct.pack('>h',value))
        self.m.symbols['Change'] = self.word(self.option(row)+36)
        self.m.call('Change',0,value)
    def init(self, reversal=0):
        self.m.call('Init',reversal)
        self.options = self.word(self.menu+8)
    def dependencies(self,cpu,address,size,user):
        r = lambda n: cpu.reg_read(gpr(n))
        if address == self.create_menu:
            self.menu = r(3); cpu.reg_write(gpr(3),0x80407600)
        elif address == 0x80380358: cpu.reg_write(gpr(3),0) # Archive lookup
        elif address == 0x803901F0: cpu.reg_write(gpr(3),0x80407700)
        elif address == 0x8038FD54: self.procs[r(5)] = r(4)
        elif address == 0x8003418C:
            slot=r(3)*2+r(4)
            cpu.reg_write(gpr(3),self.cue_objects[slot] if slot<12 else 0)
        elif address in self.native_bound:
            name=self.native_bound[address]; self.native_calls.append(name)
            if name=='GetAllPlayerPointers':
                for n,val in enumerate((self.human_obj,self.human_ft,self.cpu_obj,self.cpu_ft),3):
                    cpu.reg_write(gpr(n),val)
            elif name=='StoreCPUTypeAndZeroInputs': self.put(self.cpu_ft+0x1A88,0)
            elif name=='CheckIfFirstFrame': cpu.reg_write(gpr(3),self.first_frame)
            elif name=='IsAnyoneDead': cpu.reg_write(gpr(3),0)
            elif name=='AdjustResetDistance': cpu.reg_write(gpr(3),0xFFFFFFFF)
            elif name=='StageGetGroundID_Main': cpu.reg_write(gpr(3),42)
            elif name=='StageGetGroundID_Platform': cpu.reg_write(gpr(3),self.platform)
            elif name=='PlacePlayersCenterStage':
                self.placed_on=r(3)
                cpu.reg_write(reg.UC_PPC_REG_PC,0x100000)
                return
        else: return
        cpu.reg_write(reg.UC_PPC_REG_PC,cpu.reg_read(reg.UC_PPC_REG_LR))
    def contact(self, tick, ft=None, state=181):
        self.m.call('TestCueTick',tick)
        self.m.call('DAT_UnderFireTiming',self.cpu_ft if ft is None else ft,2,state)

    def test_saved_attack_menu_mapping_and_combined_changes(self):
        m=self.m
        for attack in range(14):
            m.call('DAT_Set',33,0,attack); self.init(1)
            self.assertEqual(self.value(0),attack)
            native=m.call('Control',0,0)
            self.assertEqual(m.cpu.mem_read(native+0xA,1)[0],attack)
        self.edit(0,11);self.edit(1,3);self.edit(2,1);self.edit(3,1);self.edit(4,1)
        native=m.call('Control',0,0)
        self.assertEqual(bytes(m.cpu.mem_read(native+0xA,4)),bytes((11,1,1,1)))
        self.assertEqual(bytes(m.cpu.mem_read(native+0x28,4)),bytes((1,1,1,1)))
        self.assertEqual(tuple(m.call('DAT_Get',33,i) for i in range(5)),(11,3,1,1,1))
        self.assertEqual(m.cpu.mem_read(self.menu+4,1)[0],6)
        exit_row=self.option(5)
        self.assertEqual(self.word(exit_row+40),self.word(self.menu+48))

    def test_event_labels_use_muted_cyan_flag_and_init_bridge_selects_menu(self):
        for event, command, name in ((6,3,'Attack on Shield'),(7,4,'Reversal Training')):
            desc=self.m.call('Desc',1,event)
            self.assertEqual(self.m.cpu.mem_read(desc+0x14,1)[0]&0x30,0x10)
            self.m.call('Control',command,0)
            self.assertEqual(self.string(self.word(self.menu)),name)

    def test_distances_preserve_platform_center_height_sides_and_followers(self):
        m=self.m
        saved=0x80C00000
        bodies=[0x80C01000+i*0x3000 for i in range(4)]
        for reversal in (0,1):
            self.init(reversal)
            for option,distance in enumerate((10,20,30,45)):
                self.edit(1 if reversal else 3,option)
                for side in (-1,1):
                    for i,body in enumerate(bodies):
                        x=10+(-side if i<2 else side)*8+(i%2)*2
                        m.cpu.mem_write(body+0xB0,struct.pack('>3f',x,25,0))
                    m.cpu.mem_write(saved,struct.pack('>4I',*bodies))
                    m.call('Control',1,saved)
                    positions=[struct.unpack('>3f',m.cpu.mem_read(body+0xB0,12)) for body in bodies]
                    self.assertEqual(abs(positions[0][0]-positions[2][0]),distance)
                    self.assertEqual((positions[0][0]+positions[2][0])/2,10 if reversal else 0)
                    self.assertEqual(positions[1][0]-positions[0][0],2)
                    self.assertEqual(positions[3][0]-positions[2][0],2)
                    self.assertTrue(all(p[1]==25 for p in positions))

    def test_one_to_five_hits_multihit_reentry_and_reset_clear(self):
        m=self.m;self.init()
        for required in range(1,6):
            self.edit(1,required)
            for count in range(1,required+1):
                self.contact(count)
                self.contact(count) # Duplicate entry in the same simulation frame.
                if count<required: self.assertEqual(m.call('Control',2,0),0)
            self.assertEqual(m.call('Control',2,0),1)
            self.assertEqual(m.call('Control',2,0),0)
        self.edit(1,2)
        self.contact(20,self.human_ft); self.contact(21,state=179)
        self.assertEqual(m.call('Control',2,0),0)
        self.contact(22);m.call('Clear');self.contact(23)
        self.assertEqual(m.call('Control',2,0),0)
        self.edit(0,5) # Editing response also discards a partial sequence.
        self.contact(24)
        self.assertEqual(m.call('Control',2,0),0)
        self.contact(25)
        self.assertEqual(m.call('Control',2,0),1)

    def test_local_shield_on_and_off_override_global_and_detach(self):
        m=self.m
        m.call('MatchStart')
        self.init()
        m.symbols['Early']=self.procs[0]
        for global_value,local_value,expected in ((0,1,60),(1,0,8),(1,1,60),(0,0,8)):
            m.call('DAT_Set',11,5,global_value);self.edit(2,local_value)
            for slot in range(12):
                ft=m.call('TestCueData',slot)
                m.cpu.mem_write(ft+0x1998,struct.pack('>f',8))
            m.call('Early',0)
            for slot in range(12):
                ft=m.call('TestCueData',slot)
                self.assertEqual(struct.unpack('>f',m.cpu.mem_read(ft+0x1998,4))[0],expected)
            self.assertEqual(m.call('DAT_Get',11,5),global_value)
        m.call('SceneChange');m.call('MatchStart')
        m.call('DAT_Set',11,5,1)
        m.symbols['Early']=self.procs[0];m.call('Early',0)
        self.assertEqual(struct.unpack('>f',m.cpu.mem_read(self.cpu_ft+0x1998,4))[0],60)

    def load_native(self):
        m=self.m
        elf=(ROOT/'build/under-fire-native-test.elf').read_bytes()
        offset=struct.unpack_from('>I',elf,0x20)[0]
        stride,count=struct.unpack_from('>HH',elf,0x2E)
        sections=[struct.unpack_from('>10I',elf,offset+i*stride) for i in range(count)]
        symbols={}
        for s in sections:
            _,kind,flags,at,start,size,link,_,_,entry=s
            if flags&2 and size and kind!=8:m.cpu.mem_write(at,elf[start:start+size])
            if kind==2:
                names=sections[link];strings=elf[names[4]:names[4]+names[5]]
                for i in range(0,size,entry):
                    name,value,_,_,_,section=struct.unpack_from('>IIIBBH',elf,start+i)
                    if section:symbols[strings[name:].split(b'\0',1)[0].decode()]=value
        self.native_bound={symbols[name]:name for name in ('GetAllPlayerPointers',
            'StoreCPUTypeAndZeroInputs','CheckIfFirstFrame','IsAnyoneDead','SaveState_Load',
            'StageGetGroundID_Main','StageGetGroundID_Platform','PlacePlayersCenterStage','GiveFullShields')}
        m.symbols['NativeThink']=symbols['AttackOnShieldThink']+4
        m.symbols['NativeReversal']=symbols['ReversalThink']+4
        self.put(0x80407200+0x2C,0x80407400)
        self.native_state=0x80407400

    def test_native_reversal_honors_saved_platform_on_entry_and_falls_back_to_ground(self):
        m=self.m;self.load_native();self.first_frame=1
        for preference,available,expected in ((0,88,42),(1,88,88),(1,0xFFF,42)):
            m.call('DAT_Set',33,4,preference);self.init(1);self.platform=available
            m.call('NativeReversal',0x80407200)
            self.assertEqual(self.placed_on,expected)

    def test_native_cpu_waits_for_selected_hit_then_uses_original_grab(self):
        m=self.m;self.init();self.load_native()
        self.edit(1,3)
        m.call('Control',0,0) # Acknowledge the menu edit before the sequence.
        for count in range(1,4):
            self.contact(count)
            self.put(self.cpu_ft+0x10,179) # GuardWait after shieldstun
            m.cpu.mem_write(self.cpu_ft+0x23FC,struct.pack('>H',181))
            m.call('NativeThink',0x80407200)
            self.assertEqual(self.word(self.cpu_ft+0x1A88),0x1C0 if count==3 else 0xC0)
            self.assertEqual(self.word(self.native_state+4),48 if count==3 else 0)


if __name__=='__main__':
    build()
    unittest.main()
