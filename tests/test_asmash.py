"""Execute the A-Smash producer and hooked native charge/input handlers.

Uses the fresh production-source ELF/GCT built by run_settings_tests.sh.
The real DOL instruction paths decide charge entry, release and the maximum;
only animation-rate/audio/color helpers are replaced with observable stubs.
"""
import struct
import unittest

import test_settings as settings_test
from test_settings import Machine, ROOT, STOP, gpr, reg
from unicorn import UC_HOOK_CODE


class ASmashTests(unittest.TestCase):
    def setUp(self):
        self.m = Machine()
        self.m.init()
        self.m.call("TestCueInit")
        self.m.call("TestCueState", 0, 14, 0, 40, 100)
        self.m.call("Settings_Set", 14, 30, 6)
        self.m.call("ASMash_Clear")
        self.data = self.m.call("TestCueData", 0)
        self.obj = self.m.call("TestCueObject", 0)
        common = self.m.symbols["cue_common"]
        for offset, value in [(0x3C, .8), (0xCC, .8), (0xD4, -.8)]:
            self.m.cpu.mem_write(common + offset, struct.pack(">f", value))
        for offset in (0x40, 0xD0, 0xD8):
            self.m.cpu.mem_write(common + offset, struct.pack(">I", 4))
        self.m.call("TestWaitClear")

    def time(self, frame):
        self.m.call("TestContextTick", 0, frame)

    def buttons(self, down=0, held=0, x=0, y=0, timer=0):
        m = self.m
        m.cpu.mem_write(self.data + 0x620, struct.pack(">2f", x, y))
        m.cpu.mem_write(self.data + 0x65C, struct.pack(">I", held))
        m.cpu.mem_write(self.data + 0x668, struct.pack(">I", down))
        m.cpu.mem_write(self.data + 0x673, bytes([timer, timer]))

    def begin(self, state=60, kind=1, frame=10, old=14, held=0x100, timer=0):
        self.time(frame)
        self.m.call("TestCueKind", 0, kind)
        self.m.call("TestCueState", 0, old, 0, 40, 100)
        family = 2 if state in (63, 342) else 3 if state in (64, 345) else 1
        self.buttons(0x100, held, x=1 if family == 1 else 0,
                     y=1 if family == 2 else -1 if family == 3 else 0, timer=timer)
        self.m.call("OSD_ASmashEnter", self.data, state)
        self.m.call("TestCueState", 0, state, 0, 40, 100)
        self.m.cpu.mem_write(self.data + 0x2114, struct.pack(">I", 0))

    def input(self, frame, held=0x100):
        self.time(frame)
        self.m.cpu.mem_write(self.data + 0x65C, struct.pack(">I", held))
        self.m.call("OSD_ASmashInput", self.obj)

    def result(self):
        return self.m.call("TestWaitFrame")

    def test_first_early_release_all_directions_and_custom_smash_entries(self):
        for kind, states in [(1, range(58,65)), (24, [346,63,64]),
                             (9, [349,350,351,63,64]), (8,[341,342,345])]:
            for state in states:
                with self.subTest(kind=kind, state=state):
                    self.m.call("TestWaitClear")
                    self.begin(state, kind)
                    self.input(11, 0)
                    self.assertEqual(self.result(), 1)
                    tag = self.m.call("TestWaitTag")
                    self.assertEqual(self.m.call("OSD_MessageSettings", tag), 30)
                    self.assertEqual(self.m.call("OSD_MessageKind", tag), 70)
                    self.assertEqual(self.m.call("OSD_MessageBestFrame", tag), 1)
                    self.assertEqual(self.m.call("OSD_MessageArgument", tag), 1)

    def test_counts_held_opportunities_once_and_ignores_frozen_updates(self):
        self.begin()
        self.input(10, 0)  # State-entry frame is the A-press frame.
        self.assertEqual(self.result(), 0)
        self.input(11)
        self.input(11)
        self.m.call("TestCueFrozen", 0, 1)
        self.input(12, 0)
        self.input(13, 0)
        self.m.call("TestCueFrozen", 0, 0)
        self.input(14, 0)
        self.assertEqual(self.result(), 2)
        self.m.call("TestWaitClear")
        self.input(15, 0)
        self.assertEqual(self.result(), 0)  # One result per attempt.

    def test_cstick_only_simultaneous_wrong_axis_and_stale_lstick_are_excluded(self):
        for down, x, y, timer in [(0,1,0,0), (0x100,0,1,0), (0x100,1,0,5)]:
            self.time(10)
            self.m.call("TestCueState", 0, 14, 0, 40, 100)
            self.buttons(down, 0x100, x, y, timer)
            self.m.cpu.mem_write(self.data+0x638, struct.pack(">2f", 1,0))
            self.m.call("OSD_ASmashEnter", self.data, 60)
            self.m.call("TestCueState", 0, 60, 0, 40, 100)
            self.input(11,0)
            self.assertEqual(self.result(),0)

    def test_native_dash_and_jump_cancel_smashes_accept_relaxed_stick_timers(self):
        self.begin(old=20,timer=255)
        self.input(11,0)
        self.assertEqual(self.result(),1)
        self.m.call("TestWaitClear")
        self.begin(state=63,old=24,timer=255)
        self.input(11,0)
        self.assertEqual(self.result(),1)

    def test_cancel_restore_rewind_death_spawn_and_auto_release_retire_attempt(self):
        for retire in ["cancel", "restore", "rewind", "death", "spawn", "maximum"]:
            with self.subTest(retire=retire):
                self.m.call("TestWaitClear")
                self.begin()
                self.input(11)
                if retire == "cancel":
                    self.m.call("OSD_ASmashEnter", self.data, 14)
                    self.m.call("TestCueState", 0, 14, 0, 40,100)
                elif retire == "restore": self.m.call("ActionCues_Clear")
                elif retire == "rewind": self.input(5)
                elif retire == "death": self.m.call("TestCueSpawn",0,1,1)
                elif retire == "spawn": self.m.call("TestCueSpawn",0,2,0)
                else: self.m.cpu.mem_write(self.data+0x2114,struct.pack(">I",3))
                self.input(12,0)
                self.assertEqual(self.result(),0)
                self.m.call("TestCueSpawn",0,0,0)

    def test_ness_charge_transition_keeps_count_and_automatic_release_does_not_report(self):
        self.begin(state=342,kind=8)
        self.input(11)
        self.m.call("OSD_ASmashEnter", self.data,343)
        self.m.call("TestCueState",0,343,12,40,0)
        self.m.cpu.mem_write(self.data+0x2114,struct.pack(">I",4))
        self.input(12,0)
        self.assertEqual(self.result(),2)
        self.m.call("TestWaitClear")
        self.begin(state=345,kind=8)
        self.input(11)
        self.m.call("OSD_ASmashEnter",self.data,346)
        self.m.call("TestCueState",0,346,12,40,0)
        self.m.call("OSD_ASmashEnter",self.data,347)
        self.m.call("TestCueState",0,347,13,40,100)
        self.input(12,0)
        self.assertEqual(self.result(),0)

    def test_setting_off_and_subfighter_do_not_produce_results(self):
        self.m.call("Settings_Set",14,30,0)
        self.begin()
        self.input(11,0)
        self.assertEqual(self.result(),0)
        self.m.call("Settings_Set",14,30,6)
        sub=self.m.call("TestCueData",1)
        self.m.cpu.mem_write(sub+0x620,struct.pack(">2f",1,0))
        self.m.cpu.mem_write(sub+0x668,struct.pack(">I",0x100))
        self.m.call("OSD_ASmashEnter",sub,60)
        self.m.call("TestCueState",1,60,0,40,100)
        self.time(15)
        self.m.call("OSD_ASmashInput",self.m.call("TestCueObject",1))
        self.assertEqual(self.result(),0)

    def test_standard_timing_colors_apply_without_changing_the_release_count(self):
        for frame, color in [(1,0x00FFFFFF),(2,0x8DFF6EFF),(3,0xFFF000FF),(4,0xFFA2BAFF)]:
            with self.subTest(frame=frame):
                self.m.call("TestStyleInit",30,-1,1,1)
                self.m.call("Settings_Set",14,30,2)
                self.begin()
                for held_frame in range(1,frame): self.input(10+held_frame)
                self.input(10+frame,0)
                self.m.call("TestStyleDraw")
                self.assertEqual(self.result(),frame)
                self.assertEqual(self.m.call("TestStyleColor",1),color)
                self.assertEqual(self.m.call("TestStyleColor",0),0xFF4646FF)
    def native(self):
        m = self.m
        dol = (ROOT / "build/Start.dol").read_bytes()
        for a,s in [(0x800DEE84,0x344)]:
            for i in range(18):
                offset,base,length=[struct.unpack_from(">I",dol,j+i*4)[0] for j in (0,0x48,0x90)]
                if base<=a and a+s<=base+length:
                    m.cpu.mem_write(a,dol[offset+a-base:offset+a-base+s]);break
            else: self.fail("Native smash handlers missing from input DOL")
        m.cpu.reg_write(gpr(13),0x804D6D5C)
        for slot,name in [(44,"OSD_ASmashEnter"),(45,"OSD_ASmashInput")]:
            m.cpu.mem_write(0x80300000-200+slot*4,struct.pack(">I",m.symbols[name]))
        m.install_hook(0x800DF0E8)
        m.symbols["NativeSmashInput"]=0x800DF0D0
        m.symbols["NativeSmashCharge"]=0x800DEF38
        shake=0x80407000
        m.cpu.mem_write(shake,struct.pack(">2I",0,4))
        m.cpu.mem_write(0x804D1BE4,struct.pack(">I",shake))
        m.cpu.mem_write(0x80300000-0x68B0,struct.pack(">f",0))
        m.cpu.mem_write(0x80300000-0x68AC,struct.pack(">f",1))
        m.cpu.mem_write(self.data+0x2128,struct.pack(">I",0x7B))
        m.cpu.mem_write(self.data+0x2130,struct.pack(">I",1))
        helpers={0x8006F190,0x800C0200,0x8007ECD4,0x8007EBAC,0x800BFFD0,0x80088148}
        for a in helpers: m.cpu.mem_write(a,bytes.fromhex("4e800020"))
        def helper(cpu,address,size,user):
            if address == 0x8006F190:
                bits=cpu.reg_read(reg.UC_PPC_REG_FPR1)
                rate=struct.unpack(">d",struct.pack(">Q",bits))[0]
                cpu.mem_write(self.data+0x89C,struct.pack(">f",rate))
        self.helper_hook=m.cpu.hook_add(UC_HOOK_CODE,helper)

    def test_hooked_native_precharge_early_release_and_held_charge_release(self):
        self.native()
        self.begin()
        self.m.cpu.mem_write(self.data+0x2114,struct.pack(">I",1))
        self.time(11);self.buttons(held=0)
        self.m.call("NativeSmashInput",self.obj)
        self.assertEqual(self.result(),1)
        self.assertEqual(bytes(self.m.cpu.mem_read(self.data+0x2114,4)),bytes(4))
        self.m.call("TestWaitClear")
        self.begin()
        self.m.cpu.mem_write(self.data+0x2114,struct.pack(">I",1))
        self.time(11)
        self.m.call("NativeSmashInput",self.obj)
        self.assertEqual(struct.unpack(">I",self.m.cpu.mem_read(self.data+0x2114,4))[0],2)
        self.assertEqual(struct.unpack(">f",self.m.cpu.mem_read(self.data+0x89C,4))[0],0)
        self.time(12);self.buttons(held=0)
        self.m.call("NativeSmashInput",self.obj)
        self.assertEqual(self.result(),2)
        self.assertEqual(struct.unpack(">I",self.m.cpu.mem_read(self.data+0x2114,4))[0],3)
        self.assertEqual(struct.unpack(">f",self.m.cpu.mem_read(self.data+0x89C,4))[0],1)

    def test_native_maximum_charge_float_counter_auto_release_is_excluded(self):
        self.native()
        self.begin()
        self.input(11)
        self.m.cpu.mem_write(self.data+0x2114,struct.pack(">I3f",2,59,60,1.4))
        self.m.cpu.mem_write(self.data+0x2124,struct.pack(">f",1))
        self.time(12)
        self.m.call("NativeSmashCharge",self.obj)
        self.assertEqual(struct.unpack(">If",self.m.cpu.mem_read(self.data+0x2114,8)),(3,60))
        self.buttons(held=0)
        self.m.call("NativeSmashInput",self.obj)
        self.assertEqual(self.result(),0)

    def test_input_hook_preserves_registers_and_displaced_native_state_read(self):
        m=self.m
        self.begin()
        self.time(11);self.buttons(held=0)
        m.cpu.mem_write(self.data+0x2114,struct.pack(">I",2))
        m.cpu.mem_write(0x80300000-200+45*4,struct.pack(">I",m.symbols["OSD_ASmashInput"]))
        m.symbols["NativeInputHook"]=m.install_hook(0x800DF0E8)
        m.cpu.mem_write(0x800DF0EC,bytes.fromhex("4e800020"))
        for i in range(3,32): m.cpu.reg_write(gpr(i),0x55000000+i)
        m.cpu.reg_write(gpr(3),self.obj);m.cpu.reg_write(gpr(31),self.data)
        before=[m.cpu.reg_read(gpr(i)) for i in range(3,32)]
        fprs=[getattr(reg,f"UC_PPC_REG_FPR{i}") for i in range(32)]
        for i,r in enumerate(fprs): m.cpu.reg_write(r,0x3FF0000000000000+i)
        m.call("TestSeedXER")
        # The XER seeding helper only clobbers r3.
        m.cpu.reg_write(gpr(3),self.obj)
        m.cpu.reg_write(reg.UC_PPC_REG_CR,0x13579BDF)
        m.cpu.reg_write(reg.UC_PPC_REG_CTR,0x80407200)
        m.call("NativeInputHook")
        self.assertEqual([m.cpu.reg_read(gpr(i)) for i in range(3,32)],before)
        self.assertEqual([m.cpu.reg_read(r) for r in fprs],[0x3FF0000000000000+i for i in range(32)])
        self.assertEqual(m.cpu.reg_read(reg.UC_PPC_REG_CR),0x13579BDF)
        self.assertEqual(m.cpu.reg_read(reg.UC_PPC_REG_CTR),0x80407200)
        self.assertEqual(m.cpu.reg_read(gpr(0)),2)
        self.assertEqual(m.call("TestReadXER"),0xA0000000)
        self.assertEqual(self.result(),1)

    def test_entry_asm_uses_new_state_in_r24_and_preserves_native_snapshot(self):
        m=self.m
        self.time(10);self.buttons(0x100,0x100,x=1)
        m.cpu.mem_write(0x80300000-200+44*4,struct.pack(">I",m.symbols["OSD_ASmashEnter"]))
        m.symbols["NativeSmashEnter"]=m.install_hook(0x800693EC)
        m.cpu.mem_write(0x800693F0,bytes.fromhex("4e800020"))
        m.cpu.reg_write(gpr(26),self.data)
        m.cpu.reg_write(gpr(24),60)
        m.cpu.reg_write(gpr(27),0x80407000)  # JOBJ value must not become the state ID.
        m.cpu.mem_write(self.data+0x23F8,struct.pack(">H",17))
        m.call("NativeSmashEnter")
        self.assertEqual(bytes(m.cpu.mem_read(self.data+0x23FC,2)),struct.pack(">H",14))
        self.assertEqual(bytes(m.cpu.mem_read(self.data+0x2408,2)),struct.pack(">H",17))
        self.assertEqual(bytes(m.cpu.mem_read(self.data+0x23F8,2)),bytes(2))
        m.call("TestCueState",0,60,0,40,100)
        self.input(11,0)
        self.assertEqual(self.result(),1)


class RelocatedASmashTests(unittest.TestCase):
    def test_optimized_native_dat_exports_report_release_after_real_relocation(self):
        loader=settings_test.RelocatedDATSettingsTests("test_new_save_identity_and_repeated_reads_after_real_dat_relocation")
        loader.setUp()
        m=loader.m
        dat=(ROOT/"build/settings-relocated-test.dat").read_bytes()
        _,size,relocs,roots,refs=struct.unpack_from(">5I",dat)
        nodes=32+size+relocs*4
        strings=nodes+(roots+refs)*8
        for i in range(roots):
            offset,name=struct.unpack_from(">II",dat,nodes+i*8)
            if dat[strings+name:].split(b"\0",1)[0]==b"tmFunction":
                _,_,_,exports,count,_=struct.unpack_from(">6I",dat,32+offset)
                break
        else: self.fail("Production tmFunction root missing")
        addresses=dict(struct.unpack_from(">II",dat,32+exports+i*8) for i in range(count))
        for slot,name in [(44,"DAT_ASmashEnter"),(45,"DAT_ASmashInput")]:
            self.assertIn(slot,addresses)
            m.symbols[name]=loader.base+addresses[slot]
        # Preserve the production producer's actual tag/varargs; bound only
        # the text-allocation dependency with the existing observed PPC fixture.
        target=m.symbols["Message_Display"]
        m.cpu.mem_write(loader.base+addresses[21],struct.pack(">4I",
            0x3D800000|(target>>16),0x618C0000|(target&65535),0x7D8903A6,0x4E800420))
        m.init();m.call("TestCueInit");m.call("TestCueState",0,14,0,40,100);m.call("DAT_Set",14,30,6)
        data=m.call("TestCueData",0);obj=m.call("TestCueObject",0)
        common=m.symbols["cue_common"]
        m.cpu.mem_write(common+0x3C,struct.pack(">fI",.8,4))
        m.call("TestContextTick",0,10)
        m.cpu.mem_write(data+0x620,struct.pack(">2f",1,0))
        m.cpu.mem_write(data+0x668,struct.pack(">I",0x100))
        m.call("DAT_ASmashEnter",data,60)
        m.call("TestCueState",0,60,0,40,100)
        m.call("TestContextTick",0,11)
        m.call("DAT_ASmashInput",obj)
        self.assertEqual(m.call("TestWaitFrame"),1)
        tag=m.call("TestWaitTag")
        self.assertEqual(m.call("OSD_MessageSettings",tag),30)
        self.assertEqual(m.call("OSD_MessageKind",tag),70)


if __name__ == "__main__":
    unittest.main()
