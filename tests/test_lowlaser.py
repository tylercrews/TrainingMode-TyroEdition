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
        self.m.call('TestLaserChoice',5,0)
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
        self.m.call('TestLaserChoice',3,1); self.step()
        self.assertEqual((self.info(11),self.info(12)),(1,0))
        self.m.call('TestLaserChoice',2,0); self.step()
        self.assertEqual((self.info(11),self.info(12)),(1,1))
        self.m.call('TestLaserReenter'); self.step()
        self.assertEqual((self.info(11),self.info(12),self.info(15)),(1,1,2))
        self.m.call('TestLaserChoice',2,1)
        self.sample(344,fastfall=1,eligible=1)
        self.assertEqual((self.info(13),self.info(14)),(-1,-1))
        self.m.call('TestLaserChoice',4,0)
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
        self.assertEqual(m.cpu.mem_read(menu_ptr+4,1)[0],6)
        reset_name=struct.unpack('>I',m.cpu.mem_read(rows+5*44+12,4))[0]
        self.assertEqual(bytes(m.cpu.mem_read(reset_name,8)).split(b'\0',1)[0],b'Reset %')


FD_EDGE = struct.unpack('>f', struct.pack('>f', 85.5656967))[0]
MARIO, FOX = 0, 1
# Expected apex rises: gravity before movement, from Puff's PlPr.dat values.
def rise(velocity, gravity=.064):
    height = 0
    while True:
        velocity -= gravity
        if velocity <= 0: return height
        height += velocity
HEIGHTS = (0, rise(1.05), rise(1.6), rise(1.05) + rise(1.65), rise(1.6) + rise(1.65))
LEGAL = [('GrNLa.dat', 32, None), ('GrNBa.dat', 31, None), ('GrOp.dat', 28, None),
         ('GrSt.dat', 8, {1, 2, 3, 4, 5, 6}), ('GrPs.dat', 3, {34, 35, 36, 51, 52, 53, 54})]


class LowLaserStageTests(unittest.TestCase):
    """Production placement against each legal stage's native collision lines."""
    @classmethod
    def setUpClass(cls):
        build()
        import test_techchase
        cls.floors = {name: test_techchase.native_floors(name) for name, _, _ in LEGAL}

    def setUp(self):
        self.m = fixtures.Machine()
        load_probe(self.m)
        self.m.call('TestLaserThink')

    def info(self, key):
        value = self.m.call('TestLaserInfo', key)
        return value if value < 0x80000000 else value - 0x100000000
    def puff(self): return (self.info(26) / 1000, self.info(27) / 1000)
    def player(self): return (self.info(28) / 1000, self.info(29) / 1000)
    def line(self, ax, ay, bx, by, flags=0, enabled=1):
        return self.m.call('TestLaserAddLine', *[round(v * 1000) for v in (ax, ay, bx, by)], flags, enabled)
    def stage(self, filename, stage, normal):
        self.m.call('TestLaserStage', stage)
        lines = []
        for index, ax, ay, bx, by, flags in self.floors[filename]:
            enabled = normal is None or index in normal
            self.line(ax, ay, bx, by, flags, enabled)
            if enabled:
                a, b = sorted(((round(ax * 1000) / 1000, round(ay * 1000) / 1000), (round(bx * 1000) / 1000, round(by * 1000) / 1000)))
                lines.append((a[0], a[1], b[0], b[1], flags & 0x100))
        return lines
    def choose(self, row, value):
        self.m.call('TestLaserChoice', row, value)
        self.m.call('TestLaserThink')

    @staticmethod
    def ground_y(lines, x):
        return max(ay + (by - ay) * (x - ax) / (bx - ax) for ax, ay, bx, by, platform in lines
                   if not platform and ax <= x <= bx)

    def test_every_legal_stage_places_ground_and_platform_positions_from_live_geometry(self):
        for filename, stage, normal in LEGAL:
            with self.subTest(stage=filename):
                lines = self.stage(filename, stage, normal)
                left = min(l[0] for l in lines if not l[4]); right = max(l[2] for l in lines if not l[4])
                platforms = [((l[0] + l[2]) / 2, l[1]) for l in lines if l[4]]
                sides = {-1: [p for p in platforms if p[0] < -10], 1: [p for p in platforms if p[0] > 10],
                         0: [p for p in platforms if abs(p[0]) < 10]}
                self.choose(0, 0)
                available = self.info(39)
                self.assertEqual(available, {'GrNLa.dat': 0x1F, 'GrPs.dat': 0x3BF}.get(filename, 0x3FF))
                for choice in range(10):
                    if not available >> choice & 1: continue
                    self.choose(0, choice)
                    self.assertEqual(self.info(32), choice)
                    self.assertEqual(self.info(34), choice)  # Saved.
                    puff = self.puff()
                    if choice < 5:
                        fd_x = (0, -70, -35, 35, 70)[choice]
                        x = fd_x * ((-left if fd_x < 0 else right) / FD_EDGE)
                        self.assertAlmostEqual(puff[0], x, delta=.002)
                        self.assertAlmostEqual(puff[1], self.ground_y(lines, x), delta=.002)
                    else:
                        side = (-1, 0, 1, -1, 1)[choice - 5]
                        target = max(sides[side], key=lambda p: p[1]) if side == 0 else min(sides[side], key=lambda p: abs(p[0]))
                        self.assertAlmostEqual(puff[0], target[0], delta=.002)
                        y = target[1] if choice < 8 else self.ground_y(lines, target[0])
                        self.assertAlmostEqual(puff[1], y, delta=.002)
                    self.assertEqual((self.info(42), self.info(30)), (1, 0))  # Grounded Squat.
                    hmn = self.player()
                    on_right = puff[0] < -1
                    x = 50 * (right / FD_EDGE) if on_right else -50 * (-left / FD_EDGE)
                    self.assertAlmostEqual(hmn[0], x, delta=.002)
                    self.assertAlmostEqual(hmn[1], self.ground_y(lines, x), delta=.002)
                    self.assertEqual(self.info(38), -1 if on_right else 1)  # Faces Puff.
                    self.assertEqual(self.info(43), 0)
                if filename == 'GrNLa.dat':
                    # FD keeps the original exact coordinates.
                    for choice, x in enumerate((0, -70, -35, 35, 70)):
                        self.choose(0, choice)
                        self.assertEqual((self.puff()[0], self.player()[0]), (x, 50 if x < 0 else -50))

    def test_missing_platforms_are_skipped_in_selector_direction_and_marked(self):
        self.stage('GrNLa.dat', 32, None)
        self.choose(0, 4); self.choose(0, 5)  # Right past all five platform choices wraps to Center.
        self.assertEqual((self.info(32), self.info(34)), (0, 0))
        self.choose(0, 9)  # Left from Center skips back to Far Right.
        self.assertEqual(self.info(32), 4)
        menu = self.m.symbols['positions_display']
        names = [bytes(self.m.cpu.mem_read(struct.unpack('>I', self.m.cpu.mem_read(menu + 4 * i, 4))[0], 24)).split(b'\0')[0]
                 for i in (4, 5, 9)]
        self.assertEqual(names, [b'Far Right', b'On Left Plat (N/A)', b'Under Right Plat (N/A)'])
        self.stage('GrPs.dat', 3, {34, 35, 36, 51, 52, 53, 54})
        self.choose(0, 5); self.choose(0, 6)  # Stadium has no top platform.
        self.assertEqual(self.info(32), 7)

    def test_saved_platform_missing_on_this_stage_uses_center_without_overwriting_save(self):
        self.stage('GrPs.dat', 3, {34, 35, 36, 51, 52, 53, 54})
        self.m.call('TestLaserSetPreference', 2, 6)  # On Top Plat, saved on another stage.
        self.m.call('TestLaserSetPreference', 3, 2)
        self.m.call('TestLaserReenter'); self.m.call('TestLaserThink')
        self.assertEqual((self.info(32), self.info(33), self.info(34), self.info(35)), (0, 2, 6, 2))
        self.assertAlmostEqual(self.puff()[0], 0, delta=.002)
        self.assertAlmostEqual(self.puff()[1], HEIGHTS[2], delta=.01)
        self.stage('GrNBa.dat', 31, None)
        self.m.call('TestLaserReenter'); self.m.call('TestLaserThink')
        self.assertEqual(self.info(32), 6)
        self.assertAlmostEqual(self.puff()[1], 54.4 + HEIGHTS[2], delta=.01)

    def test_heights_raise_puff_above_every_position_and_are_saved(self):
        self.assertEqual([round(h, 3) for h in HEIGHTS], [0, 8.096, 19.2, 28.546, 39.65])
        self.stage('GrNBa.dat', 31, None)
        for choice in range(10):
            self.choose(1, 0); self.choose(0, choice)
            base = self.puff()
            for height, rise_units in enumerate(HEIGHTS):
                self.choose(1, height)
                self.assertEqual((self.info(33), self.info(35)), (height, height))
                self.assertAlmostEqual(self.info(40) / 1000, rise_units, delta=.002)
                for _ in range(30): self.m.call('TestLaserThink')  # Held in place.
                puff = self.puff()
                self.assertAlmostEqual(puff[0], base[0], delta=.002)
                self.assertAlmostEqual(puff[1], base[1] + rise_units, delta=.002)
                airborne = height > 0
                self.assertEqual((self.info(30), self.info(41), self.info(42), self.info(31)),
                                 (1, 1, 0, 1) if airborne else (0, 0, 1, 0))
                self.assertEqual(self.info(8), 0)  # Pose stays frozen.
        self.choose(1, 4)
        self.m.call('TestLaserHit', 9); self.m.call('TestLaserThink')
        self.assertEqual((self.info(3), self.info(18)), (9, 1))
        self.assertAlmostEqual(self.puff()[1], base[1] + HEIGHTS[4], delta=.002)
        self.m.call('TestLaserRestore'); self.m.call('TestLaserThink')
        self.assertEqual((self.info(30), self.info(41)), (1, 1))

    def test_fountain_platform_carries_puff_and_falls_back_to_ground_while_gone(self):
        self.m.call('TestLaserStage', 2)
        ground = self.line(-63.35, 0, 63.35, 0)
        left = self.line(-49, 11, -21, 11, 0x100)
        self.line(21, 35, 49, 35, 0x100)
        self.line(-14, 43, 14, 43, 0x100)
        self.choose(0, 5)
        self.assertEqual(self.puff(), (-35, 11))
        self.assertEqual(self.info(37), left)
        self.m.call('TestLaserMoveLine', left, 5000, 1); self.m.call('TestLaserThink')
        self.assertAlmostEqual(self.puff()[1], 16, delta=.002)
        self.choose(1, 2)
        self.assertAlmostEqual(self.puff()[1], 16 + HEIGHTS[2], delta=.002)
        self.m.call('TestLaserMoveLine', left, -3000, 1); self.m.call('TestLaserThink')
        self.assertAlmostEqual(self.puff()[1], 13 + HEIGHTS[2], delta=.002)
        self.m.call('TestLaserMoveLine', left, 0, 0); self.m.call('TestLaserThink')
        self.assertAlmostEqual(self.puff()[1], HEIGHTS[2], delta=.002)  # Main floor while the platform is gone.
        self.m.call('TestLaserMoveLine', left, 0, 1); self.m.call('TestLaserThink')
        self.assertAlmostEqual(self.puff()[1], 13 + HEIGHTS[2], delta=.002)
        self.choose(1, 0)
        self.assertEqual(self.info(37), left)
        self.m.call('TestLaserMoveLine', left, 0, 0); self.m.call('TestLaserThink')
        self.assertEqual((self.puff()[1], self.info(37)), (0, ground))
        self.m.call('TestLaserMoveLine', left, 0, 1); self.m.call('TestLaserThink')
        self.assertAlmostEqual(self.puff()[1], 13, delta=.002)
        self.assertEqual(self.info(37), left)
        self.choose(0, 8)  # Under Left Plat stays on the main floor.
        self.assertEqual(self.puff(), (-35, 0))

    def test_every_character_times_first_aerial_neutral_b_by_native_move_transition(self):
        m = self.m
        def sample(state, airborne=1):
            m.call('TestLaserSample', state, airborne, 0, 0, 0); m.call('TestLaserThink')
        m.call('TestLaserMove', MARIO, 1); m.call('TestLaserReenter'); m.call('TestLaserThink')
        self.assertEqual(self.info(36), 1)  # Neutral-B Start label for non-blasters.
        sample(360, 0)
        m.call('TestLaserMove', MARIO, 18); sample(361, 0)  # Grounded neutral-B is ignored.
        self.assertEqual(self.info(0), -1)
        for _ in range(3): sample(25)
        sample(362)
        self.assertEqual(self.info(0), 4)
        sample(14, 0); sample(25)  # atk_kind still 18 after the move: not a new neutral-B.
        self.assertEqual(self.info(0), -1)
        m.call('TestLaserMove', MARIO, 20); sample(363)  # Up-B.
        self.assertEqual(self.info(0), -1)
        m.call('TestLaserMove', MARIO, 18); sample(363)  # No state transition.
        self.assertEqual(self.info(0), -1)
        sample(364)
        self.assertEqual(self.info(0), 4)
        m.call('TestLaserMove', FOX, 0); m.call('TestLaserReenter'); m.call('TestLaserThink')
        self.assertEqual(self.info(36), 0)  # Fox and Falco keep Laser Start.


class LowLaserCatalogueTests(unittest.TestCase):
    def test_eighth_character_event_allows_every_character_and_stage_select_without_score_bytes(self):
        m=NativeMachine(); functions=m.load_services()
        desc=m.call(functions[1],2,7)
        word=lambda at: struct.unpack('>I',m.cpu.mem_read(at,4))[0]
        self.assertEqual(bytes(m.cpu.mem_read(word(desc),80)).split(b'\0',1)[0],b'Low Laser Puff Practice\n')
        self.assertEqual(bytes(m.cpu.mem_read(word(desc+8),12)).split(b'\0',1)[0],b'lowlaser')
        whitelist=m.call(functions[25],7,2)
        self.assertEqual(word(whitelist),0xFFFFFFFF)  # Every human character.
        self.assertEqual(m.call(functions[12],2,7),15)  # External Jigglypuff ID.
        self.assertEqual(m.call(functions[13],2,7)&0xFFFF,0xFFFF)  # -1: stage select.
        m.call(functions[28],2,0,2)
        self.assertEqual(m.call(functions[43],7),0)


if __name__=='__main__': unittest.main(verbosity=2)
