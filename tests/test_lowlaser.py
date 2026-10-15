"""Execute production Low Laser event, native Fastfall hooks and DAT catalogue."""
from pathlib import Path
import struct
import subprocess
import unittest
import test_settings as fixtures
from test_score_storage import NativeMachine

ROOT = Path(__file__).resolve().parents[1]


def build():
    subprocess.run([
        'C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe', '-O2', '-w',
        '-ffreestanding', '-fno-builtin', '-msdata=none', '-mcpu=750', '-nostdlib',
        '-Wl,-Ttext=0x101000,-e,TestLaserInit,--unresolved-symbols=ignore-all',
        'tests/lowlaser_probe.c', '-o', 'build/lowlaser-probe.elf'
    ], cwd=ROOT, check=True)


def load_probe(machine):
    elf = (ROOT / 'build/lowlaser-probe.elf').read_bytes()
    at = struct.unpack_from('>I', elf, 0x20)[0]
    stride, count = struct.unpack_from('>HH', elf, 0x2E)
    sections = [struct.unpack_from('>10I', elf, at + i * stride) for i in range(count)]
    for _, kind, flags, address, offset, size, link, _, _, entry in sections:
        if flags & 2 and size and kind != 8: machine.cpu.mem_write(address, elf[offset:offset+size])
        if flags & 2 and size and kind == 8: machine.cpu.mem_write(address, bytes(size))
        if kind == 2:
            strings = sections[link]
            names = elf[strings[4]:strings[4]+strings[5]]
            for i in range(0, size, entry):
                name, value, *_ = struct.unpack_from('>IIIBBH', elf, offset+i)
                machine.symbols[names[name:].split(b'\0',1)[0].decode()] = value
    machine.call('TestLaserInit')


class LowLaserTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    def setUp(self):
        self.m = fixtures.Machine()
        load_probe(self.m)
        self.step()

    def step(self): self.m.call('TestLaserThink')
    def info(self, key):
        value = self.m.call('TestLaserInfo', key)
        return value if value < 0x80000000 else value - 0x100000000
    def sample(self, state=25, airborne=1, fastfall=0, eligible=0, frozen=0):
        self.m.call('TestLaserSample', state, airborne, fastfall, eligible, frozen)
        self.step()

    def test_crouch_is_startup_pose_with_no_animation_iasa_knockback_nudge_or_grabs(self):
        self.assertEqual([self.info(k) for k in (4,5,6,7,8,9,10)], [0,-50,39,200,0,1,1])
        for _ in range(180): self.step()
        self.assertEqual([self.info(k) for k in (6,7,8)], [39,200,0])

    def test_placement_uses_immediate_frozen_pose(self):
        self.assertEqual((self.info(21),self.info(22),self.info(23)),(0,0,0))
        for _ in range(10): self.step()
        self.assertEqual((self.info(21),self.info(22)),(0,0))
        for position in (1,0,4,0):
            self.m.call('TestLaserChoice',0,position)
            self.assertEqual((self.info(21),self.info(22),self.info(23)),(0,0,0))

    def test_active_ticks_do_not_invalidate_model_after_native_model_update(self):
        self.step()
        self.assertEqual((self.info(24),self.info(25)),(0,0))
        self.m.call('TestLaserPause',1); self.step()
        self.assertEqual((self.info(24),self.info(25)),(0,0))
        self.m.call('TestLaserPause',0); self.step()
        self.assertEqual((self.info(24),self.info(25)),(0,0))
        self.m.call('TestLaserHit',8); self.step()
        self.assertEqual((self.info(4),self.info(3),self.info(18)),(0,8,1))
        self.assertEqual((self.info(24),self.info(25)),(0,0))

    def test_native_action_state_blend_zero_uses_default_and_minus_one_applies_immediately(self):
        m=NativeMachine()
        defaults=0x80500000
        m.cpu.mem_write(defaults,b'\x06')
        m.cpu.reg_write(fixtures.gpr(29),defaults)
        def floating(index,value):
            m.cpu.reg_write(getattr(fixtures.reg,'UC_PPC_REG_FPR'+str(index)),
                            struct.unpack('>Q',struct.pack('>d',float(value)))[0])
        for requested,expected in ((0,6),(-1,0),(3,3)):
            floating(3,0); floating(29,2); floating(30,0); floating(31,requested)
            # Actual nonzero-start-frame branch immediately before ApplyAnimation.
            m.call(0x80069D40,stop=0x80069D88)
            result=struct.unpack('>d',struct.pack('>Q',m.cpu.reg_read(fixtures.reg.UC_PPC_REG_FPR3)))[0]
            self.assertEqual(result,expected)

    def test_hits_accumulate_percent_without_moving_target_and_reset_only_percent(self):
        self.m.call('TestLaserHit', 7); self.step()
        self.m.call('TestLaserHit', 11); self.step()
        self.assertEqual([self.info(k) for k in (3,4,18)], [18,0,1])
        for index, x in enumerate((0,-70,-35,35,70)):
            self.m.call('TestLaserChoice',0,index)
            self.assertEqual((self.info(4),self.info(5),self.info(3)),(x,50 if x<0 else -50,18))
        position = (self.info(4), self.info(5))
        self.m.call('TestLaserChoice',4,0)
        self.assertEqual((self.info(3), self.info(16)),(0,0))
        self.assertEqual((self.info(4),self.info(5)),position)

    def test_laser_counts_first_airborne_frame_and_fastfall_reuses_eligible_counter(self):
        for _ in range(3): self.sample()
        self.sample(344)
        self.assertEqual(self.info(0),4)
        self.sample(345,fastfall=1,eligible=2)
        self.assertEqual((self.info(1),self.info(2)),(2,5))
        self.sample(345,fastfall=1,eligible=3)
        self.assertEqual(self.info(1),2)  # Keep the activation result.
        self.sample(14,airborne=0)
        self.assertEqual((self.info(19),self.info(20)),(4,2))
        self.sample(344)  # Next jump, immediate aerial neutral-B.
        self.assertEqual((self.info(0),self.info(1)),(1,-1))

    def test_grounded_shots_hitlag_and_pause_do_not_advance_airborne_clock(self):
        self.sample(341,airborne=0)
        self.assertEqual(self.info(0),-1)
        self.sample(); self.sample(frozen=1)
        self.assertEqual(self.info(2),1)
        self.m.call('TestLaserPause',1)
        self.sample(344,frozen=0)
        self.assertEqual((self.info(2),self.info(12)),(1,1))
        self.m.call('TestLaserPause',0); self.sample(344)
        self.assertEqual((self.info(0),self.info(12)),(2,0))

    def test_actual_native_fastfall_routine_and_existing_counter_hook_feed_event_result(self):
        m = self.m
        dol = (ROOT/'build/Start.dol').read_bytes()
        for i in range(18):
            offset, base, size = [struct.unpack_from('>I',dol,at+i*4)[0] for at in (0,0x48,0x90)]
            if offset and size: m.cpu.mem_write(base,dol[offset:offset+size])
        m.cpu.reg_write(fixtures.gpr(2),0x804DF9E0)
        m.cpu.reg_write(fixtures.gpr(13),0x804DB6A0)
        m.call('TestLaserPrepare'); m.call('TestLaserInit'); self.step()
        m.install_hook(0x8007D550)
        m.symbols['NativeFastfall'] = 0x8007D528
        # Only the sound output is replaced. Eligibility and the counter are native.
        link = (ROOT/'MexTK/melee.link').read_text().splitlines()
        sound = int(next(line.split(':')[0] for line in link if line.endswith(':Fighter_PlaySFX')),16)
        m.cpu.mem_write(sound,bytes.fromhex('4e800020'))
        self.sample()
        for _ in range(4):
            m.call('TestLaserNativeInput',0)
            self.assertEqual(m.call('NativeFastfall',m.call('TestLaserFighter',0)),0)
        m.call('TestLaserNativeInput',1)
        self.assertEqual(m.call('NativeFastfall',m.call('TestLaserFighter',0)),1)
        self.step()
        self.assertEqual(self.info(1),5)

    def test_saved_toggles_are_independent_and_timing_is_always_white(self):
        self.assertEqual((self.info(11),self.info(12)),(0,0))
        self.m.call('TestLaserChoice',2,1); self.step()
        self.assertEqual((self.info(11),self.info(12)),(1,0))
        self.m.call('TestLaserChoice',1,0); self.step()
        self.assertEqual((self.info(11),self.info(12)),(1,1))
        self.m.call('TestLaserReenter'); self.step()
        self.assertEqual((self.info(11),self.info(12),self.info(15)),(1,1,2))
        self.m.call('TestLaserChoice',1,1)
        self.sample(344,fastfall=1,eligible=1)
        self.assertEqual((self.info(13),self.info(14)),(-1,-1))
        self.m.call('TestLaserChoice',3,0)
        self.assertEqual((self.info(11),self.info(17)),(0,1))

    def test_real_relocated_dat_initializes_places_and_measures(self):
        fixture = fixtures.RelocatedDATSettingsTests()
        fixture.dat_path = ROOT/'build/lowlaser-test.dat'; fixture.dat_root = b'evFunction'
        fixture.setUp(); m = fixture.m
        load_probe(m); m.call('TestLaserPrepare')
        dat = fixture.dat_path.read_bytes()
        _,size,relocs,roots,refs = struct.unpack_from('>5I',dat)
        nodes=32+size+relocs*4; strings=nodes+(roots+refs)*8
        for i in range(roots):
            at,name=struct.unpack_from('>II',dat,nodes+i*8)
            if dat[strings+name:].split(b'\0',1)[0]==b'evFunction':
                _,_,_,exports,count,_=struct.unpack_from('>6I',dat,32+at); break
        entries=dict(struct.unpack_from('>II',dat,32+exports+i*8) for i in range(count))
        base=fixture.base
        for index,name in ((0,'DAT_Init'),(1,'DAT_Update'),(2,'DAT_Think')): m.symbols[name]=base+entries[index]
        # Production DAT calls native addresses; substitute the same fixture endpoints.
        for line in (ROOT/'MexTK/melee.link').read_text().splitlines():
            if ':' not in line: continue
            address,name=line.split(':',1)
            if name in m.symbols and name not in ('Event_Init','Event_Think','Event_Update'):
                target=m.symbols[name]
                m.cpu.mem_write(int(address,16),struct.pack('>4I',0x3D800000|(target>>16),
                    0x618C0000|(target&65535),0x7D8903A6,0x4E800420))
        event=m.call('TestLaserObject')
        m.call('DAT_Init',event); m.call('DAT_Think',event)
        self.assertEqual(tuple(m.call('TestLaserInfo',k) for k in (21,22,23)),(0,0,0))
        for paused in (0,1,0,0):
            m.call('TestLaserPause',paused); m.call('TestLaserEngineTick')
            m.call('DAT_Think',event)
            self.assertEqual(tuple(m.call('TestLaserInfo',k) for k in (24,25)),(0,0))
        m.call('TestLaserSample',344,1,1,3,0); m.call('DAT_Think',event); m.call('DAT_Update')
        self.assertEqual((m.call('TestLaserInfo',0),m.call('TestLaserInfo',1)),(1,3))
        menu_ptr=struct.unpack('>I',m.cpu.mem_read(base+entries[3],4))[0]
        rows=struct.unpack('>I',m.cpu.mem_read(menu_ptr+8,4))[0]
        self.assertEqual(m.cpu.mem_read(menu_ptr+4,1)[0],5)
        reset_name=struct.unpack('>I',m.cpu.mem_read(rows+4*44+12,4))[0]
        self.assertEqual(bytes(m.cpu.mem_read(reset_name,8)).split(b'\0',1)[0],b'Reset %')


class LowLaserCatalogueTests(unittest.TestCase):
    def test_eighth_character_event_limits_css_and_does_not_claim_preference_score_bytes(self):
        m=NativeMachine(); functions=m.load_services()
        desc=m.call(functions[1],2,7)
        word=lambda at: struct.unpack('>I',m.cpu.mem_read(at,4))[0]
        self.assertEqual(bytes(m.cpu.mem_read(word(desc),80)).split(b'\0',1)[0],b'Low Laser Puff Practice\n')
        self.assertEqual(bytes(m.cpu.mem_read(word(desc+8),12)).split(b'\0',1)[0],b'lowlaser')
        whitelist=m.call(functions[25],7,2)
        self.assertEqual(word(whitelist),(1<<9)|(1<<10))
        self.assertEqual(m.call(functions[12],2,7),15)  # External Jigglypuff ID.
        self.assertEqual(m.call(functions[13],2,7),32)  # FD.
        m.call(functions[28],2,0,2)
        self.assertEqual(m.call(functions[43],7),0)


if __name__=='__main__': unittest.main(verbosity=2)
