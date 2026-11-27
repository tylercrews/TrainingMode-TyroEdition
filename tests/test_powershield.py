"""Production event plus real native laser/reflection paths and shared DAT."""
from pathlib import Path
import struct
import subprocess
import unittest
import test_settings as fixtures  # Puts build/test-deps on sys.path first.
from unicorn import UC_HOOK_CODE
from test_score_storage import NativeMachine

ROOT = Path(__file__).resolve().parents[1]
YELLOW, GREEN = 0xFFF000DC, 0x50FF5ADC


def build():
    subprocess.run(['C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe', '-O2', '-w',
        '-ffreestanding', '-fno-builtin', '-msdata=none', '-mcpu=750', '-nostdlib',
        '-Wl,-Ttext=0x101000,-e,TestPSInit,--unresolved-symbols=ignore-all',
        'tests/powershield_probe.c', '-o', 'build/powershield-probe.elf'], cwd=ROOT, check=True)


def load_probe(m):
    data = (ROOT / 'build/powershield-probe.elf').read_bytes()
    at = struct.unpack_from('>I', data, 0x20)[0]
    stride, count = struct.unpack_from('>HH', data, 0x2E)
    sections = [struct.unpack_from('>10I', data, at + i * stride) for i in range(count)]
    for _, kind, flags, address, offset, size, link, _, _, entry in sections:
        if flags & 2 and size:
            m.cpu.mem_write(address, bytes(size) if kind == 8 else data[offset:offset+size])
        if kind == 2:
            strings = sections[link]
            names = data[strings[4]:strings[4]+strings[5]]
            for i in range(0, size, entry):
                name, value, *_ = struct.unpack_from('>IIIBBH', data, offset+i)
                m.symbols[names[name:].split(b'\0',1)[0].decode()] = value
    m.call('TestPSInit')


def load_event_dat(m):
    data=(ROOT/'build/powershield-test.dat').read_bytes()
    _,size,relocs,roots,refs=struct.unpack_from('>5I',data)
    nodes=32+size+relocs*4;strings=nodes+(roots+refs)*8
    for i in range(roots):
        offset,name=struct.unpack_from('>II',data,nodes+i*8)
        if data[strings+name:].split(b'\0',1)[0]==b'evFunction':
            code,table,count,exports,num_exports,size=struct.unpack_from('>6I',data,32+offset)
            break
    else:raise AssertionError('Event DAT missing exports')
    base=0x80900000;payload=bytearray(data[32+code:32+code+size])
    for i in range(count):
        word,target=struct.unpack_from('>II',data,32+table+i*8)
        kind,offset=word>>24,word&0xFFFFFF
        if target&0xF0000000!=0x80000000:target+=base
        if kind==1:struct.pack_into('>I',payload,offset,target)
        elif kind==4:struct.pack_into('>H',payload,offset,target&65535)
        elif kind==6:struct.pack_into('>H',payload,offset,((target+0x8000)>>16)&65535)
        elif kind==10:
            old=struct.unpack_from('>I',payload,offset)[0]
            struct.pack_into('>I',payload,offset,old|((target-base-offset)&0x3FFFFFC))
        elif kind==26:struct.pack_into('>I',payload,offset,(target-base-offset)&0xFFFFFFFF)
        else:raise AssertionError(f'Unexpected relocation {kind}')
    m.cpu.mem_write(base,bytes(payload))
    return {index:base+offset for index,offset in
            (struct.unpack_from('>II',data,32+exports+i*8) for i in range(num_exports))}


class EventTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()
    def setUp(self):
        self.m = fixtures.Machine()
        load_probe(self.m)
    def info(self, key):
        value = self.m.call('TestPSInfo', key)
        return value if value < 0x80000000 else value - 0x100000000
    def color(self): return self.info(2) & 0xFFFFFFFF
    def sample(self, tick, state=87, stun=0, frozen=0):
        self.m.call('TestPSSample',tick,state,stun,frozen)
        self.m.call('TestPSTiming',4,0)
    def phase(self, phase, next=0): self.m.call('TestPSTiming',phase,next)
    def contact(self, kind=0, frame=0, reason=4, slot=0):
        self.m.call('TestPSContact',kind,frame,reason,slot)
    def label(self, which, row):
        return bytes(self.m.call('TestPSLabel',which,row,i) for i in range(80)).split(b'\0',1)[0].decode()

    def test_laser_stun_two_yellow_two_green_with_immediate_action(self):
        self.contact()
        colors = []
        for tick, stun in enumerate([5,4,3,2,1,0],1):
            self.sample(tick,87,stun); colors.append(self.color())
        self.phase(0); self.phase(2,20)
        self.assertEqual(self.info(0),1)
        self.sample(7,20); colors.append(self.color())
        self.sample(8,20); colors.append(self.color())
        self.assertEqual(colors,[0,0,0,YELLOW,YELLOW,GREEN,GREEN,0])
        self.assertEqual(self.label(0,2),'Laser hit')

    def test_hitlag_pause_and_duplicate_native_callbacks_do_not_advance(self):
        self.contact()
        self.sample(1,87,2)
        for _ in range(3): self.phase(4)
        self.assertEqual(self.color(),YELLOW)
        self.sample(2,87,2,1); self.phase(0); self.phase(2,20)
        self.assertEqual((self.color(),self.info(0),self.info(3)),(0,-1,0))
        self.sample(3,87,1); self.sample(4,87,0)
        self.m.call('TestPSPause',1)
        self.assertEqual((self.info(4),self.info(5)),(1,1))
        self.m.call('TestPSPause',0)
        self.phase(0); self.phase(0)
        self.assertEqual(self.info(3),1)
        self.phase(2,24)
        self.assertEqual(self.info(0),1)

    def test_shield_hit_and_powershield_have_their_own_recovery_sources(self):
        for kind, reason in [(1,2),(2,0)]:
            with self.subTest(kind=kind):
                self.m.call('TestPSRestore')
                self.contact(kind,2,reason)
                self.sample(10,181 if kind==1 else 182,0,1)
                self.assertEqual(self.color(),0)
                self.sample(11,181 if kind==1 else 182)
                self.phase(0)
                if kind == 1:
                    self.assertEqual(self.info(3),0)
                    self.sample(12,179)
                    self.phase(0)
                self.phase(2,24)
                self.assertEqual(self.info(0),1)
                self.assertEqual(self.label(0,2), 'Shield hit' if kind==1 else 'Powershield')
                self.assertEqual(self.color(),GREEN)

    def test_shieldstun_cue_uses_scaled_animation_then_two_recovery_frames(self):
        for yoshi in (False,True):
            for rate in (5,10,23):
                with self.subTest(yoshi=yoshi,rate=rate):
                    self.m.call('TestPSRestore')
                    if yoshi: self.m.call('TestPSYoshi')
                    self.contact(1,5,2)
                    colors=[]
                    for tick,left in enumerate((4,3,2,1),1):
                        self.m.call('TestPSAnimation',300-left*rate,rate,300)
                        self.sample(tick,344 if yoshi else 181)
                        colors.append(self.color()); self.phase(0)
                        self.assertEqual(self.info(3),0)
                    self.sample(5,342 if yoshi else 179); colors.append(self.color())
                    self.phase(0); self.phase(2,24)  # Immediate jump retains green.
                    self.assertEqual(self.info(0),1)
                    self.sample(6,24); colors.append(self.color())
                    self.sample(7,24); colors.append(self.color())
                    self.assertEqual(colors,[0,0,YELLOW,YELLOW,GREEN,GREEN,0])

    def test_shield_cue_freeze_duplicates_toggle_and_new_contact(self):
        self.contact(1,5,2)
        self.m.call('TestPSAnimation',290,10,300)
        self.sample(1,181); self.assertEqual(self.color(),YELLOW)
        self.sample(2,181,0,1); self.assertEqual(self.color(),0)
        self.sample(3,181); self.assertEqual(self.color(),YELLOW)
        self.sample(4,179); self.assertEqual(self.color(),GREEN)
        for _ in range(3): self.phase(4)
        self.sample(5,179,0,1); self.assertEqual(self.color(),0)
        self.sample(6,179); self.assertEqual(self.color(),GREEN)
        self.m.call('TestPSChoice',9,0); self.assertEqual(self.color(),0)
        self.m.call('TestPSChoice',9,1); self.assertEqual(self.color(),GREEN)
        self.contact(2,1,0); self.assertEqual(self.color(),0)
        self.sample(7,182); self.assertEqual(self.color(),GREEN)
        self.sample(8,182); self.assertEqual(self.color(),GREEN)
        self.sample(9,179); self.assertEqual(self.color(),GREEN)
        self.sample(10,179); self.assertEqual(self.color(),GREEN)
        self.sample(11,179); self.assertEqual(self.color(),0)

    def test_reflection_green_lasts_four_unfrozen_frames_after_immediate_action(self):
        self.contact(2,1,0)
        self.sample(1,182); self.assertEqual(self.color(),GREEN)
        self.phase(0); self.phase(2,24); self.assertEqual(self.info(0),1)
        self.sample(2,24); self.assertEqual(self.color(),GREEN)
        self.sample(3,24,0,1); self.assertEqual(self.color(),0)
        self.sample(4,24); self.assertEqual(self.color(),GREEN)
        for _ in range(3): self.phase(4)
        self.sample(5,24); self.assertEqual(self.color(),GREEN)
        self.sample(6,24); self.assertEqual(self.color(),0)

    def test_invalid_shield_animation_does_not_invent_a_yellow_window(self):
        for frame,rate,end in ((290,0,300),(290,-10,300),(0,10,0),(-10,10,300)):
            with self.subTest(frame=frame,rate=rate,end=end):
                self.m.call('TestPSRestore'); self.contact(1)
                self.m.call('TestPSAnimation',frame,rate,end)
                self.sample(1,181); self.assertEqual(self.color(),0)
                self.sample(2,179); self.assertEqual(self.color(),GREEN)

    def test_automatic_transitions_do_not_report_actions_and_late_action_counts(self):
        self.contact()
        self.sample(1,87,1)
        self.phase(2,14)  # Native animation recovery, outside IASA.
        self.assertEqual(self.info(0),-1)
        for tick in (2,3,4):
            self.sample(tick,14); self.phase(0); self.phase(1)
        self.sample(5,14); self.phase(0); self.phase(2,20)
        self.assertEqual(self.info(0),4)
        self.assertEqual(self.info(7)&0xFFFFFFFF,0xFF5050FF)

    def test_shielding_and_walking_keep_counting_until_another_action(self):
        for kind in (0,1,2):
            with self.subTest(kind=kind):
                self.m.call('TestPSRestore'); self.contact(kind)
                self.sample(1,14)
                # Shield startup, hold, release, stun, reflect, walking speeds,
                # turnaround and stopping must all leave the attempt pending.
                for tick, target in enumerate((178,179,180,181,182,15,16,17,18,14),1):
                    self.phase(0); self.phase(2,target); self.phase(1)
                    self.assertEqual(self.info(0),-1)
                    self.sample(tick+1,target)
                self.phase(0); self.phase(2,24)  # Jump finally completes it.
                self.assertEqual(self.info(0),11)
                self.assertEqual(self.info(1),kind)

    def test_yoshi_shield_states_do_not_complete_an_attempt(self):
        # Use the production enum rather than assuming Yoshi's fighter index.
        self.m.call('TestPSYoshi')
        self.contact(2,1,0); self.sample(1,14)
        for tick,target in enumerate(range(341,346),1):
            self.phase(0); self.phase(2,target); self.phase(1)
            self.assertEqual(self.info(0),-1)
            self.sample(tick+1,target)
        self.phase(0); self.phase(2,24)
        self.assertEqual(self.info(0),6)

    def test_act_out_of_laser_timing_colors(self):
        for frame,color in enumerate((0x00FFFFFF,0x8DFF6EFF,0xFFF000FF,0xFF5050FF,0xFF5050FF),1):
            with self.subTest(frame=frame):
                self.m.call('TestPSRestore'); self.contact()
                self.assertEqual(self.info(7)&0xFFFFFFFF,0xFFFFFFFF)
                for tick in range(1,frame+1):
                    self.sample(tick,14); self.phase(0)
                    if tick < frame: self.phase(1)
                self.phase(2,20)
                self.assertEqual(self.info(0),frame)
                self.assertEqual(self.info(7)&0xFFFFFFFF,color)

    def test_rolling_or_spotdodging_out_of_laser_stun_fails_in_shines_fail_color(self):
        for which,label in enumerate(('ROLL','ROLL','DODGE')):
            for kind in (0,1,2):
                with self.subTest(which=which,kind=kind):
                    self.m.call('TestPSRestore'); self.contact(kind)
                    self.sample(1,178 if kind else 14); self.phase(0)
                    self.phase(2,self.m.call('TestPSEscape',which))
                    self.assertEqual(self.label(0,1),label)
                    self.assertEqual(self.info(7)&0xFFFFFFFF,0xFFA2BAFF)
                    self.assertEqual(self.info(0),1)  # Still counted as the action.

    def test_the_next_laser_clears_a_fail(self):
        self.contact(); self.sample(1,14); self.phase(0)
        self.phase(2,self.m.call('TestPSEscape',2))
        self.contact()
        self.assertEqual(self.label(0,1),'-')
        self.assertEqual(self.info(7)&0xFFFFFFFF,0xFFFFFFFF)
        self.sample(2,14); self.phase(0); self.phase(2,20)
        self.assertEqual(self.info(0),1)
        self.assertEqual(self.info(7)&0xFFFFFFFF,0x00FFFFFF)

    def test_new_contact_replaces_pending_and_cpu_or_nana_never_owns_human_result(self):
        self.contact();self.sample(1,87,1)
        self.contact(2,1,0,1);self.contact(2,1,0,2)
        self.assertEqual(self.label(0,2),'Laser hit')
        self.contact(1,8,2)
        self.assertEqual(self.label(0,2),'Shield hit')
        self.assertEqual(self.label(1,2),'Reflect ended')
        self.sample(2,181);self.phase(0);self.phase(2,24)
        self.assertEqual(self.info(0),-1)

    def test_all_menu_values_reload_defaults_and_bounds_are_saved(self):
        m=self.m
        defaults=(0,0,20,0,0,0,1,1,0,1,0)
        self.assertEqual(tuple(m.call('TestPSPreference',i) for i in range(11)),defaults)
        # Each saved preference's menu row; Samus Pattern (10, T11) sits under the height.
        self.assertEqual(tuple(m.call('TestPSPrefRow',i) for i in range(11)),(2,3,4,5,7,8,9,10,11,12,6))
        for index,value in [(0,3),(1,50),(2,12),(3,4),(4,1),(5,3),(6,0),(7,0),(8,1),(9,0),(10,2)]:
            m.call('TestPSChoice',index,value)
        saved=(3,12,12,4,1,3,0,0,1,0,2)
        self.assertEqual(tuple(m.call('TestPSPreference',i) for i in range(11)),saved)
        m.call('TestPSReenter')
        options=self.info(15)
        self.assertEqual(tuple(struct.unpack('>h',m.cpu.mem_read(options+m.call('TestPSPrefRow',i)*44+6,2))[0]
                               for i in range(11)),saved)
        self.assertEqual((self.info(4),self.info(5),self.info(6)),(1,1,1))
        self.assertEqual(m.call('TestPSDelay',12,12),1)  # Never HSD_Randi(0).
        self.assertEqual(m.call('TestPSDelay',0,60),61)  # Both endpoints available.

    def test_osds_overlay_and_global_suppression_are_independent_and_exit_detaches(self):
        m=self.m
        self.contact();self.sample(1,87,2)
        m.call('TestPSChoice',7,0)
        self.assertEqual((self.info(4),self.info(5),self.info(6)),(1,1,0))
        self.assertEqual(self.color(),YELLOW)
        m.call('TestPSChoice',8,1)
        self.assertEqual(self.info(6),1)
        m.call('TestPSChoice',9,0)
        self.assertEqual(self.color(),0)
        m.call('TestPSChoice',9,1)
        self.assertEqual(self.color(),YELLOW)
        m.call('TestPSExit')
        self.assertEqual((self.info(6),self.info(10),self.info(11),self.info(12),self.color()),(-1,0,1,1,0))

    def test_restore_rewind_and_spawn_discard_timing_and_results(self):
        for reset in ('restore','rewind','spawn'):
            self.m.call('TestPSRestore');self.contact(2,1,0)
            self.sample(10,182);self.phase(0);self.phase(2,24)
            self.assertEqual(self.info(0),1)
            if reset=='restore':self.m.call('TestPSRestore')
            elif reset=='rewind':self.sample(2,182)
            else:
                ft=self.info(17);self.m.cpu.mem_write(ft+8,struct.pack('>I',2));self.sample(11,14)
            self.assertEqual((self.info(0),self.info(9),self.color()),(-1,0,0))


LEGAL = [('GrNLa.dat', 32, None), ('GrNBa.dat', 31, None), ('GrOp.dat', 28, None),
         ('GrSt.dat', 8, {1, 2, 3, 4, 5, 6}), ('GrPs.dat', 3, {34, 35, 36, 51, 52, 53, 54})]
FD, FOUNTAIN, OTHER_STAGE = 32, 2, 4  # GRKINDEXT_FD, _IZUMI, and Kongo Jungle.
CKIND_FALCO, CKIND_SAMUS = 0x14, 0x10
FD_EDGE = 85.5656967
# Saved preferences (TM_PS_*) and their choices.
SPEED, HEIGHT, DIRECTION, PATTERN = 0, 3, 4, 10
FAST, RIGHT, LEFT = 3, 0, 1
SHORT_HOP, PLATFORM, RANDOM = 0, 1, 2
# Missile height above the floor below for Very Low, Low, Mid and High, and
# how far the model's frame timing may move one. The rising missile comes out
# where the full hop is 16 frames after B on its first airborne frame.
TARGETS = {1: 10.0, 2: 13.0, 3: 16.5, 4: 20.5}
TOLERANCE = .75
RISING = 34.54
# Menu rows.
ROW_CHARACTERS, ROW_STAGE, ROW_HEIGHT, ROW_PATTERN, ROW_MOVEMENT, ROW_OVERLAY = 0, 1, 5, 6, 8, 12


class SamusTests(unittest.TestCase):
    """The production event's CPU against a frame model of Samus (see the probe)
    on each legal stage's native collision lines from the ISO."""
    @classmethod
    def setUpClass(cls):
        build()
        import test_techchase
        cls.floors = {name: test_techchase.native_floors(name) for name, _, _ in LEGAL}

    def setUp(self):
        self.m = fixtures.Machine()
        load_probe(self.m)

    def sim(self, key):
        value = self.m.call('TestPSSim', key)
        return value if value < 0x80000000 else value - 0x100000000
    def row(self, row, key):
        value = self.m.call('TestPSRow', row, key)
        return value if value < 0x80000000 else value - 0x100000000
    def text(self, address):
        return bytes(self.m.cpu.mem_read(address, 96)).split(b'\0', 1)[0].decode()
    def label(self, which, row):
        return bytes(self.m.call('TestPSLabel', which, row, i) for i in range(80)).split(b'\0', 1)[0].decode()
    def line(self, ax, ay, bx, by, flags=0, enabled=1):
        return self.m.call('TestPSAddLine', *[round(v * 1000) for v in (ax, ay, bx, by)], flags, enabled)
    def stage(self, filename, stage, normal):
        self.m.call('TestPSStage', stage)
        lines = []
        for index, ax, ay, bx, by, flags in self.floors[filename]:
            enabled = normal is None or index in normal
            self.line(ax, ay, bx, by, flags, enabled)
            if enabled:
                a, b = sorted(((ax, ay), (bx, by)))
                lines.append((a[0], a[1], b[0], b[1], bool(flags & 0x100)))
        return lines
    def fountain(self):
        """Fountain where the game starts it: the main floor, the side platforms
        at 16.125 and 22.125, and the top at 42.75 (notes/LEGAL-STAGE-PLATFORMS.md)."""
        self.m.call('TestPSStage', FOUNTAIN)
        self.line(-63.3476, 0, 63.3476, 0)
        left = self.line(-49.5, 16.125, -21, 16.125, 0x100)
        right = self.line(21, 22.125, 49.5, 22.125, 0x100)
        self.line(-14.25, 42.75, 14.25, 42.75, 0x100)
        lines = [(-63.3476, 0, 63.3476, 0, False), (-49.5, 16.125, -21, 16.125, True),
                 (21, 22.125, 49.5, 22.125, True), (-14.25, 42.75, 14.25, 42.75, True)]
        return lines, left, right
    @staticmethod
    def edges(lines):
        return min(l[0] for l in lines if not l[4]), max(l[2] for l in lines if not l[4])
    @staticmethod
    def side_platform(lines, side):
        platforms = [l for l in lines if l[4] and (l[0] + l[2]) / 2 * side > 10]
        return min(platforms, key=lambda l: abs(l[0] + l[2])) if platforms else None
    def match(self, samus=True, pattern=SHORT_HOP, height=1, direction=RIGHT, speed=FAST):
        for index, value in ((SPEED, speed), (HEIGHT, height), (DIRECTION, direction), (PATTERN, pattern)):
            self.m.call('TestPSSetPreference', index, value)
        self.m.call('TestPSMatch', int(samus))
    def run_frames(self, frames):
        while frames > 0:  # A few frames a call, under the machine's instruction limit.
            self.m.call('TestPSRun', min(frames, 4))
            frames -= 4
    def missiles(self):
        keys = ('height', 'x', 'dir', 'smash', 'descending', 'from_platform', 'frame')
        out = []
        for i in range(self.sim(0)):
            values = [self.m.call('TestPSMissile', i, k) for k in range(len(keys))]
            values = [v - 0x100000000 if v >= 0x80000000 else v for v in values]
            item = dict(zip(keys, values))
            item['height'] /= 1000; item['x'] /= 1000
            out.append(item)
        return out
    def clean(self):
        self.assertEqual((self.sim(1), self.sim(2)), (0, 0))  # No wrong move or fastfall.

    def test_falco_keeps_final_destinations_start_and_mirrors_for_left(self):
        lines = self.stage(*LEGAL[0])
        self.match(samus=False)
        self.run_frames(1)
        self.assertAlmostEqual(self.sim(8) / 1000, -60, places=2)  # You, where FD's spawn put you,
        self.assertAlmostEqual(self.sim(6) / 1000, 60, places=2)   # and Falco where his did.
        self.assertEqual((self.sim(9), self.sim(7)), (0, 0))
        self.assertEqual((self.sim(10), self.sim(11)), (-1, -1))
        self.assertEqual(self.sim(15), 1)  # Kept as the event's savestate.
        self.m.call('TestPSChoice', DIRECTION, LEFT)
        self.assertEqual(self.sim(16) & 4, 4)  # A mirrored load,
        self.assertAlmostEqual(self.sim(8) / 1000, 60, places=2)
        self.assertAlmostEqual(self.sim(6) / 1000, -60, places=2)

    def test_falco_starts_on_every_stages_scaled_spot(self):
        for filename, stage, normal in LEGAL[1:]:
            with self.subTest(stage=filename):
                lines = self.stage(filename, stage, normal)
                left, right = self.edges(lines)
                self.match(samus=False)
                self.run_frames(1)
                self.assertAlmostEqual(self.sim(6) / 1000, 60 * right / FD_EDGE, places=2)
                self.assertAlmostEqual(self.sim(8) / 1000, -60 * -left / FD_EDGE, places=2)

    def test_short_hop_missiles_fly_at_each_height_on_every_legal_stage(self):
        for filename, stage, normal in LEGAL:
            for height, target in TARGETS.items():
                with self.subTest(stage=filename, height=height):
                    lines = self.stage(filename, stage, normal)
                    self.match(height=height)
                    self.run_frames(400)
                    self.clean()
                    shots = self.missiles()
                    self.assertGreaterEqual(len(shots), 4)
                    for shot in shots:
                        self.assertTrue(shot['smash'])        # Super missiles,
                        self.assertTrue(shot['descending'])   # on the way down,
                        self.assertEqual(shot['dir'], -1)     # at you,
                        self.assertFalse(shot['from_platform'])
                        self.assertLess(abs(shot['height'] - target), TOLERANCE)
                    self.assertEqual(self.sim(3), 0)  # never landing on a platform.
                    x = self.sim(6) / 1000
                    for l in lines:  # She stands clear of any platform her short hop reaches.
                        if l[4] and l[1] < 29:
                            self.assertFalse(l[0] - 7.99 < x < l[2] + 7.99, (x, l))

    def test_random_height_picks_each_of_the_four(self):
        self.stage(*LEGAL[0])
        self.m.call('TestPSRandom', 4, 0, 1, 2, 3)
        self.match(height=0)
        self.run_frames(500)
        self.clean()
        heights = sorted(round(s['height']) for s in self.missiles())
        for target in TARGETS.values():
            self.assertTrue(any(abs(h - target) < TOLERANCE + .5 for h in heights), (target, heights))

    def test_platform_pattern_rises_to_the_side_platform_then_drops_and_fires_on_the_way_down(self):
        for filename, stage, normal in LEGAL[1:]:
            for height in (1, 4):
                with self.subTest(stage=filename, height=height):
                    lines = self.stage(filename, stage, normal)
                    platform = self.side_platform(lines, 1)
                    self.match(pattern=PLATFORM, height=height)
                    self.run_frames(1)
                    self.assertAlmostEqual(self.sim(6) / 1000, (platform[0] + platform[2]) / 2, places=2)
                    self.run_frames(700)
                    self.clean()
                    shots = self.missiles()
                    rising = [s for s in shots if not s['from_platform']]
                    falling = [s for s in shots if s['from_platform']]
                    self.assertGreaterEqual(len(rising), 3)
                    self.assertGreaterEqual(len(falling), 3)
                    self.assertGreaterEqual((self.sim(3), self.sim(4)), (3, 3))  # Platform landings, drops.
                    for shot in rising:
                        self.assertTrue(shot['smash'])
                        self.assertFalse(shot['descending'])
                        self.assertLess(abs(shot['height'] - RISING), 2.5)
                    # A drop can't fire higher than pressing on its first frame:
                    # 16 frames from -0.5 under gravity, plus the cannon.
                    floor = max(ay + (by - ay) * ((platform[0] + platform[2]) / 2 - ax) / (bx - ax)
                                for ax, ay, bx, by, plat in lines
                                if not plat and ax <= (platform[0] + platform[2]) / 2 <= bx)
                    highest = platform[1] - floor - 16 * .5 - .066 * 136 + 9.92
                    for shot in falling:
                        self.assertTrue(shot['smash'] and shot['descending'])
                        self.assertLess(abs(shot['height'] - min(TARGETS[height], highest)), TOLERANCE)
                    # They alternate: up, then down from the platform.
                    order = [s['from_platform'] for s in shots]
                    self.assertTrue(all(a != b for a, b in zip(order, order[1:])), order)
                    self.assertTrue(all(s['dir'] == -1 for s in shots))

    def test_direction_left_mirrors_samus_under_the_left_platform(self):
        lines = self.stage(*LEGAL[1])  # Battlefield
        platform = self.side_platform(lines, -1)
        self.match(pattern=PLATFORM, direction=LEFT)
        self.run_frames(1)
        self.assertAlmostEqual(self.sim(6) / 1000, (platform[0] + platform[2]) / 2, places=2)
        self.run_frames(500)
        self.clean()
        shots = self.missiles()
        self.assertGreaterEqual(len(shots), 4)
        self.assertTrue(all(s['dir'] == 1 for s in shots))

    def test_random_pattern_mixes_short_hops_and_the_platform(self):
        self.stage(*LEGAL[1])
        self.m.call('TestPSRandom', 2, 0, 1, 0, 0)
        self.match(pattern=RANDOM)
        self.run_frames(900)
        self.clean()
        shots = self.missiles()
        self.assertTrue(any(s['from_platform'] for s in shots))
        self.assertTrue(any(not s['from_platform'] and s['descending'] for s in shots))  # A short hop,
        self.assertTrue(any(not s['from_platform'] and not s['descending'] for s in shots))  # and a rise.

    def test_final_destination_has_no_platform_so_its_patterns_short_hop(self):
        self.stage(*LEGAL[0])
        for pattern in (PLATFORM, RANDOM):
            with self.subTest(pattern=pattern):
                self.match(pattern=pattern)
                self.assertEqual(self.text(self.m.call('TestPSRowValue', ROW_PATTERN, 1)), 'Platform (N/A)')
                self.assertEqual(self.text(self.m.call('TestPSRowValue', ROW_PATTERN, 2)), 'Random (N/A)')
                self.run_frames(300)
                self.clean()
                shots = self.missiles()
                self.assertGreaterEqual(len(shots), 3)
                self.assertTrue(all(s['descending'] and not s['from_platform'] for s in shots))
                self.assertEqual(self.sim(4), 0)
                self.assertAlmostEqual(self.sim(6) / 1000, 60, places=2)

    def test_fountain_platforms_that_move_or_sink(self):
        lines, left, right = self.fountain()
        # Short Hop stands clear of the side platform even at its lowest.
        self.match()
        self.run_frames(1)
        x = self.sim(6) / 1000
        self.assertFalse(21 - 8 < x < 49.5 + 8, x)
        # Platform: under the right platform, which rises, sinks and goes.
        self.match(pattern=PLATFORM, height=2)
        self.run_frames(1)
        self.assertAlmostEqual(self.sim(6) / 1000, 35.25, places=2)
        for dy, enabled in ((5000, 1), (-12000, 1), (0, 0), (0, 1)):
            with self.subTest(dy=dy, enabled=enabled):
                self.m.call('TestPSMoveLine', right, dy, enabled)
                before = len(self.missiles())
                self.run_frames(300)
                self.clean()
                shots = self.missiles()[before:]
                self.assertGreaterEqual(len(shots), 2)
                for shot in shots:
                    self.assertTrue(shot['smash'])
                    if shot['from_platform']:
                        self.assertTrue(shot['descending'])
                        self.assertLess(shot['height'], TARGETS[2] + TOLERANCE)
                if not enabled:  # No platform: full hops back to the ground.
                    self.assertFalse(any(s['from_platform'] for s in shots))

    def test_another_stage_restarts_on_final_destination(self):
        self.m.call('TestPSStage', OTHER_STAGE)
        self.line(-50, 0, 50, 0)
        self.match()
        self.run_frames(5)
        self.assertEqual(self.sim(12), FD)
        self.assertEqual((self.sim(14), self.sim(0), self.sim(15)), (0, 0, 0))  # No inputs, no setup.
        self.assertEqual((self.sim(18), self.sim(19)), (0, -1))  # Detached, as on Exit.

    def test_rows_follow_the_cpu(self):
        self.stage(*LEGAL[1])
        self.match(samus=False)
        self.assertEqual((self.row(ROW_PATTERN, 0), self.row(ROW_MOVEMENT, 0)), (1, 0))
        self.assertEqual(self.text(self.row(ROW_HEIGHT, 2)), 'Laser Height')
        self.assertEqual(self.text(self.row(ROW_OVERLAY, 2)), 'Laser Recovery Cue')
        self.assertEqual(self.label(0, 0), 'Act out of Laser')
        self.match(samus=True)
        self.assertEqual((self.row(ROW_PATTERN, 0), self.row(ROW_MOVEMENT, 0)), (0, 1))
        self.assertEqual(self.text(self.row(ROW_HEIGHT, 2)), 'Missile Height')
        self.assertEqual(self.text(self.row(ROW_OVERLAY, 2)), 'Missile Recovery Cue')
        self.assertEqual(self.text(self.m.call('TestPSRowValue', ROW_PATTERN, 1)), 'Platform')
        self.assertEqual(self.label(0, 0), 'Act out of Missile')
        self.m.call('TestPSContact', 0, 0, 4, 0)
        self.assertEqual(self.label(0, 2), 'Missile hit')
        self.m.call('TestPSContact', 2, 1, 0, 0)
        self.assertEqual(self.label(1, 2), 'Reflected missile')

    def test_change_characters_offers_falco_and_samus_and_change_stage_the_legal_six(self):
        self.stage(*LEGAL[1])
        self.match(samus=False)
        self.assertEqual(self.row(ROW_CHARACTERS, 5), self.m.call('TestPSSwap', 6))
        self.assertEqual(self.row(ROW_STAGE, 5), self.m.call('TestPSSwap', 5))
        self.assertEqual(self.m.call('TestPSSwap', 0), 2)
        self.assertEqual((self.text(self.m.call('TestPSSwap', 1)), self.text(self.m.call('TestPSSwap', 2))), ('Falco', 'Samus'))
        self.assertEqual((self.m.call('TestPSSwap', 3), self.m.call('TestPSSwap', 4), self.m.call('TestPSSwap', 9)), (0, 0, 0))
        self.assertEqual(self.m.call('TestPSSwap', 7), 1 << 1)  # Battlefield is current.
        self.m.call('TestPSSwapCPU', 1)
        self.assertEqual(self.m.call('TestPSSwap', 8), CKIND_SAMUS)
        self.assertEqual((self.sim(18), self.sim(19)), (0, -1))  # Let go before the restart.

    def test_samus_pattern_saves_and_moves_her(self):
        lines = self.stage(*LEGAL[1])
        platform = self.side_platform(lines, 1)
        self.match()
        self.run_frames(1)
        spot = self.sim(6) / 1000
        self.m.call('TestPSChoice', PATTERN, PLATFORM)
        self.assertEqual(self.m.call('TestPSPreference', PATTERN), PLATFORM)
        self.assertAlmostEqual(self.sim(6) / 1000, (platform[0] + platform[2]) / 2, places=2)
        self.m.call('TestPSChoice', PATTERN, SHORT_HOP)
        self.assertAlmostEqual(self.sim(6) / 1000, spot, places=2)


class NativePowershieldTests(unittest.TestCase):
    def setUp(self):
        self.m=NativeMachine();self.functions=self.m.load_services()
        self.m.call(self.functions[28],14,9,1)  # Enable the global Powershield OSD.
        self.item,self.hit,self.ft=0x805A0000,0x805B0000,0x805C0000
        self.obj=0x805D0000
        self.m.cpu.mem_write(self.ft,struct.pack('>I',self.obj))
        self.m.cpu.mem_write(self.obj+0x2c,struct.pack('>I',self.ft))
        self.m.cpu.mem_write(self.ft+0x10,struct.pack('>I',182))
        self.m.cpu.mem_write(self.ft+0x1A30,struct.pack('>I',60))
        self.m.cpu.mem_write(self.ft+0x2218,b'\x10')
        self.m.cpu.mem_write(self.hit+0xC,struct.pack('>f',3))
        self.m.cpu.mem_write(self.ft+0x2354,struct.pack('>f',1))
        # SDK double pointer to common data, matching the original global.
        self.m.cpu.mem_write(0x804DB6A0-0x514C,struct.pack('>I',0x805E1000))
        self.m.cpu.mem_write(0x805E1000+0x2A4,struct.pack('>f',1))
        self.results=[]
        self.hook=self.m.cpu.hook_add(UC_HOOK_CODE,self.dependencies)
        self.m.install_hook(0x8007758C,0x806A0000)
    def dependencies(self,cpu,address,size,_):
        r=lambda n:cpu.reg_read(fixtures.gpr(n))
        if address==self.functions[21]:
            fmt=bytes(cpu.mem_read(r(6),100)).split(b'\0',1)[0].decode()
            self.results.append((r(3),r(4),r(5),fmt,r(7),r(8)))
            cpu.reg_write(fixtures.gpr(3),0)
        elif address!=0x8026FAC4:return
        cpu.reg_write(fixtures.reg.UC_PPC_REG_PC,cpu.reg_read(fixtures.reg.UC_PPC_REG_LR))
    def tearDown(self):self.m.cpu.hook_del(self.hook)
    def test_all_four_native_shield_spawns_use_the_renderer_owner_and_material_layout(self):
        m=self.m
        effect,obj,root=0x805E2000,0x805E3000,0x805E4000
        def write(address,value):m.cpu.mem_write(address,struct.pack('>I',value))
        write(effect+4,obj);write(effect+8,self.obj);write(obj+0x28,root)
        write(obj+0x2C,effect);write(root+0x10,root+0x100)
        tevs=[]
        for node in range(7):
            joint=root+0x100+node*0x100
            dobj,mobj,tobj,tev=0x805E6000+node*0x100,0x805E7000+node*0x100,0x805E8000+node*0x200,0x805EA000+node*0x100
            if node<6:write(joint+8,joint+0x100)
            write(joint+0x18,dobj);write(dobj+8,mobj);write(mobj+8,tobj)
            write(tobj+8,tobj+0x100)
            for texture in range(2):
                write(tobj+texture*0x100+0xA8,tev+texture*0x40)
                tevs.append(tev+texture*0x40)
        models=[]
        def allocate(cpu,address,size,_):
            if address!=0x8005C1B4:return
            models.append(cpu.reg_read(fixtures.gpr(3)))
            cpu.reg_write(fixtures.gpr(3),effect)
            cpu.reg_write(fixtures.reg.UC_PPC_REG_PC,cpu.reg_read(fixtures.reg.UC_PPC_REG_LR))
        hook=m.cpu.hook_add(UC_HOOK_CODE,allocate)
        try:
            for gfx,model in ((1047,11),(1048,12),(1049,13),(1050,14)):
                with self.subTest(gfx=gfx):
                    for tev in tevs:m.cpu.mem_write(tev,bytes(32))
                    write(effect+0x10,0)
                    m.call(0x8005FDDC,gfx,self.obj,root,0xFF5050)
                    self.assertEqual(models[-1],model)
                    self.assertEqual(struct.unpack('>I',m.cpu.mem_read(effect+0x10,4))[0],0x8005FAB8)
                    self.assertEqual(struct.unpack('>I',m.cpu.mem_read(effect+8,4))[0],self.obj)
                    colored=tevs[1:2] if gfx==1047 else tevs[::2] if gfx==1049 else tevs[:1]
                    for tev in colored:
                        self.assertEqual(bytes(m.cpu.mem_read(tev+0x10,3)),b'\xff\xff\xff')
                        self.assertEqual(bytes(m.cpu.mem_read(tev+0x14,3)),b'\xff\x50\x50')
        finally:m.cpu.hook_del(hook)
    def test_native_projectile_reflection_does_not_grant_physical_shield_drop_cancels(self):
        m=self.m
        m.call(0x80077464,self.item,self.hit,self.ft)
        self.assertEqual(struct.unpack('>I',m.cpu.mem_read(self.ft+0x235C,4))[0],0)
        # Intercept only the input checks called by the real GuardOff IASA.
        # With no inputs accepted, reflection gets just the ordinary checks.
        checks=set()
        for address in range(0x80092CFC,0x80092E10,4):
            word=struct.unpack('>I',m.cpu.mem_read(address,4))[0]
            if word & 0xFC000003 == 0x48000001:
                offset=word & 0x03FFFFFC
                if offset & 0x02000000: offset-=0x04000000
                checks.add(address+offset)
        calls=[]
        def input_check(cpu,address,size,_):
            if address not in checks:return
            calls.append(address)
            cpu.reg_write(fixtures.gpr(3),0)
            cpu.reg_write(fixtures.reg.UC_PPC_REG_PC,cpu.reg_read(fixtures.reg.UC_PPC_REG_LR))
        hook=m.cpu.hook_add(UC_HOOK_CODE,input_check)
        try:
            m.call(0x80092CFC,self.obj)
            self.assertEqual(calls,[0x8009980C,0x800CB024])
            # The physical powershield collision path calls this grant routine.
            m.cpu.mem_write(0x805E1000+0x2B8,struct.pack('>I',3))
            m.call(0x80094138,self.ft)
            self.assertEqual(struct.unpack('>I',m.cpu.mem_read(self.ft+0x235C,4))[0],3)
            calls.clear();m.call(0x80092CFC,self.obj)
            self.assertGreater(len(calls),2)
            self.assertEqual(calls[-2:],[0x8009980C,0x800CB024])
        finally:m.cpu.hook_del(hook)
    def test_real_reflection_acceptance_reports_success_and_overdamage_and_shine_do_not(self):
        m=self.m
        # Actual native function chooses its accepted reflection block.
        m.call(0x80077464,self.item,self.hit,self.ft)
        self.assertEqual(len(self.results),1)
        self.assertEqual(self.results[-1][2:5],(1,'Powershield\nSuccess: %df',1))
        self.assertEqual(struct.unpack('>I',m.cpu.mem_read(self.item+0xC64,4))[0],self.obj)
        self.results.clear()
        m.cpu.mem_write(self.hit+0xC,struct.pack('>f',61))
        m.call(0x80077464,self.item,self.hit,self.ft)
        self.assertEqual(self.results,[])
        m.cpu.mem_write(self.hit+0xC,struct.pack('>f',3))
        m.cpu.mem_write(self.ft+0x10,struct.pack('>I',360))
        m.call(0x80077464,self.item,self.hit,self.ft)
        self.assertEqual(self.results,[])
    def test_both_native_reflect_frames_are_successes_and_geometry_failure_has_honest_label(self):
        m=self.m
        for left,frame in [(1,1),(0,2)]:
            m.cpu.mem_write(self.ft+0x2354,struct.pack('>f',left))
            m.call(0x80077464,self.item,self.hit,self.ft)
            self.assertEqual(self.results[-1][4],frame)
        m.call(self.functions[49],self.ft,6,self.item)
        self.assertEqual(self.results[-1][3], 'Missed Powershield\n%s\nShield frame: %df')
        reason=bytes(m.cpu.mem_read(self.results[-1][4],80)).split(b'\0',1)[0].decode()
        self.assertEqual(reason,'Reflect active / shield hit')
        self.assertNotIn('Early',reason)
        m.cpu.mem_write(self.ft+0x10,struct.pack('>I',178))
        m.cpu.mem_write(self.ft+0x2218,b'\0')
        m.call(self.functions[49],self.ft,6,self.item)
        self.assertEqual(self.results[-1][3],'Missed Powershield\nNo hard press')

    def test_relocated_event_receives_all_three_real_collision_hooks_and_native_iasa_timing(self):
        m=self.m
        event_functions=load_event_dat(m)
        event,menu,menu_data,event_data=0x805D0800,0x805D1000,0x805D2000,0x805D3000
        m.cpu.mem_write(event+0x2C,struct.pack('>I',event_data))
        m.cpu.mem_write(menu+0x2C,struct.pack('>I',menu_data))
        texts=[];updates=[];subtexts={}
        def string(address):return bytes(m.cpu.mem_read(address,96)).split(b'\0',1)[0].decode()
        def engine(cpu,address,size,_):
            r=lambda n:cpu.reg_read(fixtures.gpr(n))
            if address==0x80380358:cpu.reg_write(fixtures.gpr(3),0x805F8000)
            elif address==0x80034110:cpu.reg_write(fixtures.gpr(3),self.obj)
            elif address==0x803A611C:cpu.reg_write(fixtures.gpr(3),7)
            elif address==0x803A6754:
                text=0x80600000+len(texts)*0x1000;texts.append(text);subtexts[text]=0
                cpu.reg_write(fixtures.gpr(3),text)
            elif address==0x803A6B98:
                row=subtexts[r(3)];subtexts[r(3)]+=1;cpu.reg_write(fixtures.gpr(3),row)
            elif address==0x803A70A0:
                fmt=string(r(5));value=string(r(6)) if fmt=='%s' else r(6)
                updates.append((r(3),r(4),fmt,value))
            elif address not in (0x803A74F0,0x803A7548):return
            cpu.reg_write(fixtures.reg.UC_PPC_REG_PC,cpu.reg_read(fixtures.reg.UC_PPC_REG_LR))
        hook=m.cpu.hook_add(UC_HOOK_CODE,engine)
        try:
            m.call(self.functions[20],0x805F8000)  # Real OnFileLoad binds EventVars.
            variables=struct.unpack('>I',m.cpu.mem_read(0x803D7054,4))[0]
            m.cpu.mem_write(variables+8,struct.pack('>II',event,menu))
            m.call(event_functions[0],event)
            self.assertEqual(len(texts),2)
            m.cpu.mem_write(self.item+0x10,struct.pack('>I',55))  # Falco laser.
            m.call(0x80077464,self.item,self.hit,self.ft)
            m.call(event_functions[1])
            self.assertIn((texts[1],1,'Success: %df',1),updates)
            self.assertIn((texts[0],2,'%s','Powershield'),updates)
            m.cpu.mem_write(0x8046B6A0+0x254,struct.pack('>I',10))
            m.call(self.functions[49],self.ft,4,0)
            m.call(self.functions[49],self.ft,0,0)
            m.call(self.functions[49],self.ft,2,24)
            m.call(self.functions[49],self.ft,1,0)
            m.call(event_functions[1])
            self.assertIn((texts[0],1,'%df',1),updates)
            # Run each other actual hook and displaced instruction, with the
            # original live registers at its injection point.
            for address,allocation,registers,label in [
                (0x800776A0,0x806A2000,{3:self.item,4:self.hit,5:self.ft},'Shield hit'),
                (0x800780E0,0x806A4000,{26:self.item,28:self.ft},'Laser hit')]:
                m.install_hook(address,allocation)
                for register,value in registers.items():m.cpu.reg_write(fixtures.gpr(register),value)
                m.call(address,stop=address+4)
                m.call(event_functions[1])
                self.assertEqual([u[3] for u in updates if u[:3]==(texts[0],2,'%s')][-1],label)
            # A Fox laser is excluded even when it hits the same human.
            m.cpu.mem_write(self.item+0x10,struct.pack('>I',54))
            m.call(self.functions[49],self.ft,7,self.item)
            m.call(event_functions[1])
            self.assertEqual([u[3] for u in updates if u[:3]==(texts[0],2,'%s')][-1],'Laser hit')
        finally:m.cpu.hook_del(hook)


class TimingRenderTests(unittest.TestCase):
    def setUp(self):
        self.m=fixtures.Machine();self.m.init();self.m.call('TestCueInit')
    def tint(self, color):
        m=self.m
        obj=m.call('TestCueObject',0)
        address=0x80403F00;m.cpu.mem_write(address,struct.pack('>I',color))
        m.call('ActionCues_SetTimingOverlay',obj,address if color else 0)
    def materials(self, which=0):
        m=self.m
        address=m.call('TestShieldMaterials',which)
        return bytes(m.cpu.mem_read(address,7*2*32))
    def test_shield_tint_covers_every_piece_and_white_flash_and_preserves_alpha(self):
        m=self.m
        m.call('TestCueState',0,182,0,40,100)
        m.call('TestShieldInit',0,0,1)
        original=self.materials()
        for color in (YELLOW,GREEN):
            with self.subTest(color=color):
                self.tint(color)
                self.assertEqual(m.call('TestShieldWrapped',0),1)
                m.call('TestShieldDraw',0)
                for node in range(7):
                    for texture in range(2):
                        self.assertEqual(m.call('TestShieldColor',0,node,texture,0),(color&0xFFFFFF00)|(90+node))
                        self.assertEqual(m.call('TestShieldColor',0,node,texture,1),(color&0xFFFFFF00)|(40+texture))
                self.assertEqual(self.materials(),original)
        self.tint(0);m.call('TestShieldDraw',0)
        self.assertEqual(m.call('TestShieldColor',0,0,0,0),0xFFFFFF5A)
        self.assertEqual(m.call('TestShieldColor',0,0,0,1),0xFF505028)
    def test_shield_owner_filtering_new_effects_and_scene_cleanup(self):
        m=self.m
        m.call('TestCueState',0,182,0,40,100)
        m.call('TestCueState',2,179,0,40,100)
        m.call('TestShieldInit',0,0,1)
        m.call('TestShieldInit',1,2,1)  # Opponent shield stays native.
        m.call('TestShieldInit',2,0,0)  # Other effects on the human stay native.
        self.tint(GREEN)
        self.assertEqual([m.call('TestShieldWrapped',i) for i in range(3)],[1,0,0])
        m.call('TestShieldInit',3,0,1)  # Effect born after the timing update.
        m.call('TestCueTick',1)
        self.assertEqual(m.call('TestShieldWrapped',3),1)
        for i in range(4):m.call('TestShieldDraw',i)
        self.assertEqual(m.call('TestShieldColor',0,0,0,1),0x50FF5A28)
        self.assertEqual(m.call('TestShieldColor',1,0,0,1),0xFF505028)
        self.assertEqual(m.call('TestShieldColor',2,0,0,1),0xFF505028)
        m.call('ActionCues_SceneChange')
        self.assertEqual([m.call('TestShieldWrapped',i) for i in range(4)],[0,0,0,0])
    def test_shield_shared_material_restore_missing_parts_and_spawn_reset(self):
        m=self.m
        m.call('TestCueState',0,182,0,40,100)
        m.call('TestShieldInit',0,0,1);m.call('TestShieldSharedMaterial',0)
        original=self.materials();self.tint(GREEN);m.call('TestShieldDraw',0)
        self.assertEqual(self.materials(),original)
        m.call('TestShieldMissingMaterial',0);m.call('TestShieldDraw',0)
        self.assertEqual(self.materials(),original)
        m.call('TestCueSpawn',0,2,0);m.call('TestShieldDraw',0)
        self.assertEqual(m.call('TestShieldColor',0,0,0,1),0xFF505028)
        self.assertEqual(self.materials(),original)
        self.assertEqual(m.call('TestShieldDrawCount',0),3)
    def test_shield_tint_clears_on_restore_and_death(self):
        m=self.m
        m.call('TestCueState',0,182,0,40,100);m.call('TestShieldInit',0,0,1)
        self.tint(GREEN);m.call('ActionCues_Clear');m.call('TestShieldDraw',0)
        self.assertEqual(m.call('TestShieldColor',0,0,0,1),0xFF505028)
        self.tint(YELLOW);m.call('TestCueSpawn',0,0,1);m.call('TestShieldDraw',0)
        self.assertEqual(m.call('TestShieldColor',0,0,0,1),0xFF505028)
    def test_event_timing_wins_over_global_cues_and_restores_native_colors(self):
        m=fixtures.Machine();m.init();m.call('TestCueInit')
        m.call('Settings_Set',11,4,1)
        m.call('Settings_Set',11,7,1)
        obj=m.call('TestCueObject',0);ft=m.call('TestCueData',0)
        m.call('TestCueState',0,70,3,5,100);m.call('TestCueTick',1)
        original=bytes(m.cpu.mem_read(ft+0x408,0x180))
        color=0x80403F00;m.cpu.mem_write(color,struct.pack('>I',GREEN))
        m.call('ActionCues_SetTimingOverlay',obj,color)
        m.call('TestCueDraw',0)
        self.assertEqual(m.call('TestCueDrawColor',0),GREEN)
        self.assertEqual(bytes(m.cpu.mem_read(ft+0x408,0x180)),original)
        m.call('Settings_Set',11,0,1)  # Text suppression does not affect cues.
        m.call('TestCueDraw',0)
        self.assertEqual(m.call('TestCueDrawColor',0),GREEN)
        m.call('ActionCues_SetTimingOverlay',obj,0)
        m.call('TestCueDraw',0)
        self.assertEqual(m.call('TestCueDrawColor',0),YELLOW)
        self.assertEqual(bytes(m.cpu.mem_read(ft+0x408,0x180)),original)


if __name__=='__main__':unittest.main()
