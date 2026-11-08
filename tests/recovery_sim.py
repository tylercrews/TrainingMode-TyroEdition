"""Closed-loop Edgeguard recoveries: production AI, modeled physics.

Each frame the production Recovery_Think (tests/edgeguard_probe.c) reads the
fighter and writes its buttons and stick, and this file moves the fighter. The
physics is the decompilation's, move by move, with the ISO's numbers; the stages
are the ISO's collision lines. It has no animation-driven ECB beyond the numbers
below, and no player. A pass here means the plan works against those rules, not
that it was seen working in Dolphin.

Run it to print the recovery tables in the notes:
    python tests/recovery_sim.py [samples]
"""
import math
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_settings as fixtures
from test_lowlaser import load_probe

FALL, DAMAGEFALL, JUMP_F, JUMP_B, FALL_SPECIAL, DAMAGE_FLY, KIND_FOX, KIND_FALCO, KIND_LUIGI, BUTTON_B, BUTTON_Y = range(11)
SPECIAL_BASE = 341
# Action states after the common ones.
FOX_SIDEB_START, FOX_SIDEB_DASH, FOX_SIDEB_END = 0x15E, 0x15F, 0x160
FOX_FF_CHARGE, FOX_FF_TRAVEL, FOX_FF_FALL = 0x162, 0x163, 0x165
# Fox's and Luigi's character states share numbers, so Luigi's carry 1000 here.
LG_MISSILE_START, LG_MISSILE_HOLD, LG_MISSILE_FLY, LG_MISSILE_END = 1349, 1350, 1353, 1352
LG_UPB, LG_CYCLONE = 1356, 1358

# Common attributes (jump_v, air_v, air_h, gravity, terminal, drift_mult,
# drift_base, drift_max, friction, fastfall, model_scale), the ledge snap, and
# the ECB: front, feet, side point height and top, standing and in the moves
# that meet walls.
FIGHTERS = {
    'Fox': dict(attrs=(3.68, 1.2, .9, .23, 2.8, .06, .02, .83, .02, 3.4, .96), snap=(11, 13, 9),
                front=2., bottom=2., side=6., top=11., dash=(3., 2.4, 5.5, 8.5), end=(3.7, 4.8, 6.8, 8.9), startup=20, step=78 / 4 * .96),
    'Falco': dict(attrs=(4.1, .94, .94, .17, 3.1, .05, .02, .83, .02, 3.5, 1.1), snap=(11, 13, 9),
                  front=2.3, bottom=2., side=6.5, top=12., dash=(3.8, 2.8, 6.05, 9.3), end=(4.3, 5., 7.2, 9.5), startup=16, step=60 / 4 * 1.1),
    'Luigi': dict(attrs=(2.4, .9, .9, .069, 1.6, .025, .015, .68, .01, 2., 1.25), snap=(12, 15, 10),
                  front=2.36, bottom=2., side=8., top=14.),
}
# Fox-style special attributes, from 0x24: side-B to 0x50, Firefox from 0x54.
SPECIALS = {
    'Fox': (15, 1.5, .05, 1 / 60, 4, .1, 2, .07, 5, .08, .4, 20, 15, 1.25, .02, .015, .5, 30, 0, 6, 3.8, .1, 1.5, 20, .8, .125, .4, 18, 20, 18),
    'Falco': (15, 1.5, .05, 1 / 60, 2, .1, 2, .07, 5, .08, .4, 20, 15, 1.25, .02, .016, .5, 22, 0, 4, 4.2, .17, 1.5, 20, .8, .125, .4, 18, 40, 18),
    'Luigi': (),
}
LUIGI_UPB_Y = (0, 0, .01, .01, .01, 0, 0, 0, 0, 8.89, 17.29, 22.67, 25.62, 28.41, 31.02, 33.45, 35.69, 37.73, 39.56,
               41.18, 42.58, 43.74, 44.67, 45.34, 45.76, 45.91, 45.92, 45.89, 45.82, 45.7, 45.51, 45.25, 44.91, 44.47,
               43.92, 43.26, 42.48, 41.56, 40.49, 39.27, 37.88)

# The right side of each legal stage, from the ISO: (ledge), bottom blast zone,
# then the walls and undersides down from the ledge. 4 right wall, 2 ceiling.
STAGES = {
    'Final Destination': ((85.566, 0.), -140., ((85.566, 0., 85.566, -10.5, 4), (85.566, -10.5, 65.799, -20.454, 4),
                                                (65.799, -20.454, 65.837, -31.344, 4), (65.837, -31.344, 61.419, -47.366, 4),
                                                (61.419, -47.366, 53.774, -54.258, 4), (53.774, -54.258, 47.456, -55.388, 2),
                                                (47.456, -55.388, -47.456, -55.388, 2))),
    'Battlefield': ((68.4, 0.), -108.8, ((68.4, 0., 64.98, -6., 4), (64.98, -6., 35.6, -18., 2),
                                         (35.6, -18., 38., -21.2, 4), (38., -21.2, 32.8, -24.8, 4),
                                         (32.8, -24.8, 30.4, -28.8, 4), (30.4, -28.8, 29.2, -34.8, 4),
                                         (29.2, -34.8, 10.4, -40., 2))),
    'Dream Land': ((77.271, .009), -123., ((77.271, .009, 76.339, -11.042, 4), (76.339, -11.042, 65.763, -35.756, 4),
                                           (65.763, -35.756, -65.763, -35.756, 2))),
    "Yoshi's Story": ((56., -3.5), -91., ((56., -3.5, 56., -6.65, 4), (56., -6.65, 54.95, -7.7, 4), (54.95, -7.7, 53.725, -10.5, 4),
                                          (53.725, -10.5, 52.675, -12.25, 4), (52.675, -12.25, 52.675, -26.95, 4),
                                          (52.675, -26.95, 53.725, -28., 4), (53.725, -28., 53.725, -29.75, 4),
                                          (53.725, -29.75, 52.675, -30.8, 4), (52.675, -30.8, 52.675, -46.2, 4),
                                          (52.675, -46.2, 53.725, -47.25, 4))),
    'Pokemon Stadium': ((87.75, 0.), -111., ((87.75, 0., 87.75, -4., 4), (87.75, -4., 73.75, -15., 4), (73.75, -15., 73.75, -17.5, 4),
                                             (73.75, -17.5, 60., -17.5, 2), (60., -17.5, 60., -38., 4), (60., -38., 15., -60., 2))),
    'Fountain of Dreams': ((63.348, .624), -146., ((63.348, .624, 63.259, -4.396, 4), (63.259, -4.396, 59.483, -14.664, 4),
                                                   (59.483, -14.664, 56.872, -19.553, 4), (56.872, -19.553, 54.927, -26.118, 4),
                                                   (54.927, -26.118, 51.907, -32.206, 4), (51.907, -32.206, 47.604, -37.482, 4))),
}
HEIGHTS = {'Very High': (51, 61), 'High': (23, 33), 'Level': (-5, 5), 'Low': (-33, -23), 'Very Low': (-61, -51)}
DISTANCES = {'Close': (20, 30), 'Near': (40, 50), 'Medium': (60, 70), 'Far': (80, 90)}
HEIGHT_ROWS = {'Very High': 1, 'High': 2, 'Level': 3, 'Low': 4, 'Very Low': 5}
DISTANCE_ROWS = {'Close': 1, 'Near': 2, 'Medium': 3, 'Far': 4}
DEADZONE = .28


built = False


def build():
    """Compile the probe once a run. Recompiling it for every test class failed
    once at random, in a run where nothing else had changed."""
    global built
    if built:
        return
    subprocess.run([
        'C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe', '-O2', '-w',
        '-ffreestanding', '-fno-builtin', '-msdata=none', '-mcpu=750', '-nostdlib',
        '-Wl,-Ttext=0x101000,-e,TestEdgeguardInit,--unresolved-symbols=ignore-all',
        'tests/edgeguard_probe.c', '-o', 'build/edgeguard-probe.elf'
    ], cwd=ROOT, check=True)
    built = True


def stick(value):
    axis = value / 127 if value > 0 else value / 128
    axis = max(-1., min(1., axis))
    return 0. if abs(axis) <= DEADZONE else axis


class Sim:
    """One fighter on one stage's right ledge, or its left with `side` -1."""

    def __init__(self, machine, name, stage, side=1):
        self.m, self.name, self.side = machine, name, side
        self.f = FIGHTERS[name]
        self.ledge, self.blast, self.lines = STAGES[stage]
        (self.jump_v, self.air_v, self.air_h, self.gravity, self.terminal, self.drift_mult, self.drift_base,
         self.drift_max, self.friction, self.fastfall, self.model) = self.f['attrs']
        self.snap_x, snap_y, snap_h = self.f['snap']
        self.box_lo, self.box_hi = snap_y - snap_h / 2, snap_y + snap_h / 2
        self.kind = machine.call('TestState', {'Fox': KIND_FOX, 'Falco': KIND_FALCO, 'Luigi': KIND_LUIGI}[name])
        self.states = {k: machine.call('TestState', k) for k in (FALL, DAMAGEFALL, JUMP_F, JUMP_B, FALL_SPECIAL, DAMAGE_FLY)}
        self.b, self.y_button = machine.call('TestState', BUTTON_B), machine.call('TestState', BUTTON_Y)
        self.at = {name: machine.symbols[name] for name in ('sim_in', 'sim_out', 'sim_attr', 'sim_special', 'sim_segs', 'sim_ledge', 'sim_blast_bottom')}
        self.write('sim_attr', '>14f', *self.f['attrs'], *self.f['snap'])
        special = SPECIALS[name]
        self.write('sim_special', '>%df' % len(special), *special)
        self.write('sim_ledge', '>4f', -self.ledge[0], self.ledge[1], self.ledge[0], self.ledge[1])
        self.write('sim_blast_bottom', '>f', self.blast)
        count = 0
        for sign in (1, -1):
            for ax, ay, bx, by, kind in self.lines:
                wall = kind if sign > 0 or kind == 2 else 8
                self.m.cpu.mem_write(self.at['sim_segs'] + count * 20, struct.pack('>5f', ax * sign, ay, bx * sign, by, wall))
                count += 1
        machine.call('TestSimConfigure', self.kind, count)
        self.specials = dict(zip(range(0x24, 0x24 + 4 * len(special), 4), special))

    def write(self, name, fmt, *values):
        self.m.cpu.mem_write(self.at[name], struct.pack(fmt, *values))

    # Geometry, in ledge-relative `out` (outside the ledge) and `h` (over it).
    def inset(self, depth):
        """How far inland the side of the stage is `depth` under the ledge."""
        best = 1000.
        for ax, ay, bx, by, _ in self.lines:
            ya, yb = ay - self.ledge[1], by - self.ledge[1]
            if min(ya, yb) <= depth <= max(ya, yb):
                t = 0 if ya == yb else (depth - ya) / (yb - ya)
                best = min(best, self.ledge[0] - (ax + (bx - ax) * t))
        return best

    def ceiling(self, out):
        """The underside's height over a point under the stage."""
        best = 1000.
        for ax, ay, bx, by, _ in self.lines:
            xa, xb = self.ledge[0] - ax, self.ledge[0] - bx
            if min(xa, xb) <= -out <= max(xa, xb):
                t = 0 if xa == xb else (-out - xa) / (xb - xa)
                best = min(best, ay + (by - ay) * t - self.ledge[1])
        return best

    def ecb(self):
        if self.state == FOX_SIDEB_END:
            return self.f['end']
        if self.state in (FOX_SIDEB_DASH, FOX_SIDEB_START):
            return self.f['dash']
        return self.f['front'], self.f['bottom'], self.f['side'], self.f['top']

    # The move rules.
    def begin(self, out, h, vx=0., vy=0., kbx=0., kby=0., hitstun=0, state=None, jumps=1):
        self.out, self.h, self.vx, self.vy, self.kbx, self.kby = out, h, vx, vy, kbx, kby
        self.hitstun, self.hitlag = hitstun, 7
        self.state = self.states[DAMAGE_FLY] if state is None else state
        self.frame = 0
        self.jumps, self.facing, self.held = jumps, 1., 0  # 1 is toward the stage.
        self.cyclone_used, self.flag2, self.var0 = 0, 0, False
        self.log = []
        self.cyclone_start = None
        self.cyclone_top = None
        self.mobility = None
        self.charge, self.delay, self.travel, self.missile_flight = 0, 0, 0, 0
        self.dash_h = self.upb_h = self.jump_h = None
        self.backflipped = False
        self.last_sx = self.now_sx = 0.

    def launch(self, height, distance, rand=.5, inland=15):
        """A hitstun launch, through the production ChooseLaunch."""
        m = self.m
        m.call('TestCPU', round(self.gravity * 1000), round(self.terminal * 1000))
        m.call('TestRows', HEIGHT_ROWS[height], DISTANCE_ROWS[distance])
        m.call('TestRand', round(rand * 1000), 0)
        m.call('TestLaunch', 1, inland)
        signed = lambda v: (v if v < 0x80000000 else v - 0x100000000) / 1000
        mag, angle, start = (signed(m.call('TestLaunchInfo', k)) for k in range(3))
        speed = mag * .03
        self.begin(-start, -self.ledge[1], kbx=-speed * math.cos(math.radians(angle)), kby=speed * math.sin(math.radians(angle)),
                   hitstun=int(.4 * mag))

    def think(self, opt_flags):
        at = self.at['sim_in']
        x = self.side * (self.ledge[0] + self.out)
        y = self.ledge[1] + self.h
        toward = -self.side  # World X of "toward the stage".
        self.m.cpu.mem_write(at, struct.pack(
            '>9i10f', self.state % 1000, self.frame, 1, int(self.hitlag > 0), self.jumps, self.flag2, self.cyclone_used,
            opt_flags, self.held, x, y, self.vx * toward, self.vy, self.kbx * toward, self.kby, float(self.hitstun),
            self.facing * toward, self.f['front'], self.f['bottom']))
        self.cyclone_used = self.m.call('TestSimThink')
        held, sx, sy = struct.unpack('>iii', bytes(self.m.cpu.mem_read(self.at['sim_out'], 12)))
        return held, stick(((sx + 128) % 256 - 128) * toward), stick((sy + 128) % 256 - 128)

    def enter(self, state):
        self.state, self.frame = state, 0

    def jump(self, sx):
        self.jumps += 1
        self.vx, self.vy = sx * self.air_h, self.jump_v * self.air_v
        back = sx * self.facing <= -.125
        self.backflipped = self.backflipped or back
        self.jump_h = self.h
        self.enter(self.states[JUMP_B if back else JUMP_F])

    def drift(self, sx, cap=None):
        if sx == 0.:
            self.vx = max(0., self.vx - self.friction) if self.vx > 0 else min(0., self.vx + self.friction)
            return
        accel = sx * self.drift_mult + (self.drift_base if sx > 0 else -self.drift_base)
        target = sx * self.drift_max
        if cap is not None:
            target = max(-cap, min(cap, target))
        if target > 0:
            self.vx = self.vx + accel if self.vx + accel <= target else max(self.vx - self.friction, target)
        else:
            self.vx = self.vx + accel if self.vx + accel >= target else min(self.vx + self.friction, target)

    def fall(self, gravity=None, terminal=None):
        self.vy = max(self.vy - (self.gravity if gravity is None else gravity), -(self.terminal if terminal is None else terminal))

    def actionable(self):
        return self.state in (self.states[FALL], self.states[DAMAGEFALL], self.states[JUMP_F], self.states[JUMP_B]) \
            or (self.state == self.states[DAMAGE_FLY] and self.hitstun == 0)

    def special(self, sx, sy):
        self.log.append((self.name, self.state, self.out, self.h, sx, sy))
        if sy >= .55:
            return self.up_b()
        if sy <= -.55:
            return self.down_b() if self.name == 'Luigi' else None
        if abs(sx) >= .6:
            if sx * self.facing < -.2:
                self.facing = -self.facing
            return self.side_b()

    def up_b(self):
        if self.name == 'Luigi':
            self.vy, self.vx = 0., 0.
            self.upb_h = self.h
            self.upb_out = self.out
            self.enter(LG_UPB)
        else:
            self.vx /= self.specials[0x58]
            self.vy = 0.
            self.delay = int(self.specials[0x54])
            self.upb_h = self.h
            self.enter(FOX_FF_CHARGE)
        self.jumps = 2 if self.name != 'Luigi' else self.jumps

    def down_b(self):
        self.vy = .1 - (0. if self.cyclone_used else .8)
        self.vx = max(-.8, min(.8, self.vx))
        self.var0 = False
        self.flag2 = 0
        self.cyclone_start = self.h
        self.cyclone_top = self.h
        self.enter(LG_CYCLONE)

    def side_b(self):
        if self.name == 'Luigi':
            self.charge = 20 if abs(self.last_sx) < .6 else 0  # the smash input's head start
            self.vx /= 1.25
            self.vy = 0.
            self.enter(LG_MISSILE_START)
        else:
            self.vy = 0.
            self.vx /= self.specials[0x28]
            self.delay = int(self.specials[0x24])
            self.jumps = 2
            self.enter(FOX_SIDEB_START)

    def side_b_end(self):
        self.vx, self.vy = self.specials[0x3C] * self.facing, 0.
        self.delay = int(self.specials[0x44])
        self.enter(FOX_SIDEB_END)

    def special_fall(self, mobility):
        self.mobility = mobility
        self.enter(self.states[FALL_SPECIAL])

    def can_grab(self):
        """Whether this state asks for a ledge this frame. Only the states that do."""
        if self.state in (FOX_SIDEB_START, FOX_SIDEB_DASH, FOX_SIDEB_END, FOX_FF_CHARGE, FOX_FF_TRAVEL, FOX_FF_FALL):
            return True
        if self.state == LG_UPB:
            return self.frame >= 9 and self.vy < 0.
        if self.state == LG_CYCLONE or self.state in (LG_MISSILE_START, LG_MISSILE_HOLD, LG_MISSILE_FLY, LG_MISSILE_END):
            return False
        return True

    def step(self, opt_flags):
        """One frame. Returns 'ledge', 'stage', 'dead' or None."""
        held, sx, sy = self.think(opt_flags)
        pressed = held & ~self.held
        self.held = held
        self.last_sx, self.now_sx = self.now_sx, sx
        if self.hitlag > 0:
            self.hitlag -= 1
            return None
        self.frame += 1
        state = self.state

        # Animation: scripts and state ends.
        if state == LG_CYCLONE:
            if self.frame == 6:
                self.flag2 = 1
            if self.frame == 44:
                self.flag2 = 0
                self.var0 = True
                self.cyclone_used = 1
            if self.frame >= 80:
                self.enter(self.states[FALL])
        elif state == LG_UPB and self.frame >= 40:
            self.special_fall(.6)
        elif state == LG_MISSILE_START and self.frame >= 18:
            self.enter(LG_MISSILE_HOLD)
        elif state == LG_MISSILE_HOLD and (not held & self.b or self.charge >= 90):
            self.enter(LG_MISSILE_FLY)
            self.vx = .017 * self.charge + 1.4
            self.vy = .4 + .4 * self.charge / 90
            self.frame = 4
        elif state == LG_MISSILE_FLY and self.frame >= 44:
            self.vx /= 1.5
            self.enter(LG_MISSILE_END)
        elif state == LG_MISSILE_END and self.frame >= 30:
            self.enter(self.states[FALL])
        elif state == FOX_SIDEB_START and self.frame >= self.f['startup']:
            self.dash_h = self.h
            self.enter(FOX_SIDEB_DASH)
        elif state == FOX_SIDEB_DASH and self.frame >= 4:
            self.side_b_end()
        elif state == FOX_SIDEB_END and self.frame >= 40:
            self.special_fall(self.specials[0x4C])
        elif state == FOX_FF_CHARGE and self.frame >= 43:
            self.ff_launch(sx, sy)
        elif state == FOX_FF_TRAVEL:
            self.travel -= 1
            if self.travel <= 0:
                self.enter(FOX_FF_FALL)
        elif state == FOX_FF_FALL and self.frame >= 20:
            self.special_fall(self.specials[0x8C])

        # Interrupts.
        if self.actionable():
            if pressed & self.b:
                self.special(sx, sy)
            elif pressed & self.y_button and self.jumps < 2:
                self.jump(sx)
        elif self.state == FOX_SIDEB_DASH and self.frame <= 2 and pressed & self.b:
            self.side_b_end()

        # Physics.
        state = self.state
        if self.hitstun > 0:
            self.hitstun -= 1
            self.fall()
        elif self.actionable():
            self.fall()
            self.drift(sx)
        elif state == self.states[FALL_SPECIAL]:
            self.fall()
            self.drift(sx, self.mobility)
        elif state == FOX_SIDEB_START:
            if self.delay:
                self.delay -= 1
            else:
                self.fall(self.specials[0x30])
            self.vx = max(0., self.vx - self.specials[0x2C]) if self.vx > 0 else min(0., self.vx + self.specials[0x2C])
        elif state == FOX_SIDEB_DASH:
            self.vx, self.vy = (self.f['step'] * self.facing if self.frame > 0 else 0.), 0.
        elif state == FOX_SIDEB_END:
            if self.delay:
                self.delay -= 1
            else:
                self.fall(self.specials[0x48])
            self.vx = max(0., self.vx - self.specials[0x40]) if self.vx > 0 else min(0., self.vx + self.specials[0x40])
        elif state == FOX_FF_CHARGE:
            if self.delay:
                self.delay -= 1
            else:
                self.fall(self.specials[0x60])
            self.vx = max(0., self.vx - self.specials[0x5C]) if self.vx > 0 else min(0., self.vx + self.specials[0x5C])
        elif state == FOX_FF_TRAVEL:
            self.ff_frames += 1
            if self.ff_frames >= self.specials[0x70]:
                self.vx -= self.facing * self.specials[0x78] * math.cos(self.ff_angle)
                self.vy -= self.specials[0x78] * math.sin(self.ff_angle)
        elif state == FOX_FF_FALL:
            self.fall()
            self.drift(sx)
        elif state == LG_UPB:
            if self.frame >= 9:
                index = min(self.frame, len(LUIGI_UPB_Y) - 1)
                self.vy = (LUIGI_UPB_Y[index] - LUIGI_UPB_Y[index - 1]) * .95
                self.vx = 0.
            else:
                self.fall(.05)
        elif state == LG_CYCLONE:
            if not self.cyclone_used and self.flag2 and pressed & self.b:
                self.vy = min(self.vy + .8, 1.4)
            self.fall()
            # Past frame 44 its speed limit is gone, and a target of 0 stops him.
            target = 0. if self.var0 else sx * .8
            if target == 0.:
                self.vx = 0.
            elif self.vx < target:
                self.vx = min(self.vx + abs(sx) * .08, target)
            else:
                self.vx = max(self.vx - abs(sx) * .08, target)
        elif state in (LG_MISSILE_START, LG_MISSILE_HOLD):
            if state == LG_MISSILE_HOLD:  # It hovers: the fall is behind a flag that's never set.
                self.charge += 1
            self.vx = max(0., self.vx - .02) if self.vx > 0 else min(0., self.vx + .02)
        elif state == LG_MISSILE_FLY:
            self.fall(.01, 1.2)
        elif state == LG_MISSILE_END:
            self.fall(.1)
            self.vx = max(0., self.vx - .05)

        # Position: own speed, then knockback, which decays.
        size = math.hypot(self.kbx, self.kby)
        if size <= .051:
            self.kbx = self.kby = 0.
        else:
            self.kbx -= .051 * self.kbx / size
            self.kby -= .051 * self.kby / size
        out0, h0 = self.out, self.h
        self.out -= self.vx + self.kbx
        self.h += self.vy + self.kby
        if self.cyclone_top is not None and state == LG_CYCLONE:
            self.cyclone_top = max(self.cyclone_top, self.h)
        return self.collide(out0, h0)

    def ff_launch(self, sx, sy):
        if abs(sx) + abs(sy) >= self.specials[0x64]:
            if abs(sx) > self.specials[0x88]:
                self.facing = 1. if sx > 0 else -1.
            self.ff_angle = math.atan2(sy, sx * self.facing)
        else:
            self.ff_angle = math.pi / 2
        self.vx = self.facing * self.specials[0x74] * math.cos(self.ff_angle)
        self.vy = self.specials[0x74] * math.sin(self.ff_angle)
        self.ff_frames = 0
        self.travel = int(self.specials[0x68])
        self.enter(FOX_FF_TRAVEL)

    def collide(self, out0, h0):
        front, bottom, side, top = self.ecb()
        # The stage's side stops the ECB's front point coming in from outside,
        # and pushes it back out when the ECB grows against it.
        tip = self.h + side
        hit_wall = False
        if tip < 0.:
            stop = front - self.inset(tip)
            if self.out < stop and out0 > -1. and stop > -500.:
                self.out = stop
                hit_wall = True
        elif self.out < front and out0 >= front and self.h + bottom < 0.:
            # The point is over the ledge but the feet aren't: the corner stops it.
            self.out = front
            hit_wall = True
        if hit_wall and self.state == LG_MISSILE_FLY:  # ftLg_SpecialAirS2_Coll
            self.vx /= 1.5
            self.enter(LG_MISSILE_END)
        # Under the stage the head meets the underside.
        if self.out < 0.:
            limit = self.ceiling(self.out)
            if self.h + top > limit and h0 + top <= limit + 3.:
                self.h = limit - top
                self.vy = min(self.vy, 0.)
        # Landing on the stage.
        if self.out <= 0. and h0 + bottom >= 0. and self.h + bottom < 0.:
            return 'stage'
        # The ledge: a frame that moved down, with the ledge in the swept box.
        grab = self.h < h0 and self.can_grab()
        if self.state in (self.states[FALL], self.states[DAMAGEFALL], self.states[JUMP_F], self.states[JUMP_B],
                          self.states[FALL_SPECIAL], LG_UPB, FOX_SIDEB_START, FOX_SIDEB_DASH, FOX_SIDEB_END, FOX_FF_CHARGE):
            grab = grab and self.facing > 0
        if grab and self.out > 0. and self.h + bottom < 0. \
                and min(self.out, out0) - front - self.snap_x <= 0. \
                and min(self.h, h0) + self.box_lo <= 0. and max(self.h, h0) + self.box_hi >= 0.:
            self.grab_state = self.state
            self.grab_frame = self.frame
            return 'ledge'
        if self.ledge[1] + self.h < self.blast or self.out > 260.:
            return 'dead'
        return None

    def run(self, opt_flags, frames=700):
        for _ in range(frames):
            result = self.step(opt_flags)
            if result:
                return result
        return 'timeout'


def machine():
    m = fixtures.Machine()
    load_probe(m, 'edgeguard-probe.elf', 'TestEdgeguardInit')
    return m


def spot_rate(m, name, stage, rows, height, distance, samples=8, side=1):
    """How often a set of rows recovers from a Knockback Height and Distance."""
    sim = Sim(m, name, stage, side)
    tally = {}
    for i in range(samples):
        sim.launch(height, distance, (i + .5) / samples)
        m.call('TestSeed', 1 + i * 7919)
        result = sim.run(rows)
        tally[result] = tally.get(result, 0) + 1
    return tally


SP_FF, SP_SWEETSPOT, SP_TO_LEDGE, SP_TO_STAGE, SP_SHORTEN = 2, 4, 5, 6, 8  # recovery.c's spacie_plan
LG_SWEETSPOT = 3  # recovery.c's luigi_plan
SPACIE_ROWS = (
    ('Ledge Sweetspot', 1 << 7, lambda s, m, r: m.call('TestPlan') == SP_SWEETSPOT and s.dash_h is not None and r == 'ledge'),
    ('To Ledge', 1 << 8, lambda s, m, r: m.call('TestPlan') == SP_TO_LEDGE and s.dash_h is not None and r == 'ledge'),
    ('To Main Stage', 1 << 9, lambda s, m, r: m.call('TestPlan') == SP_TO_STAGE and s.dash_h is not None and r == 'stage'),
    ('Shortest Shorten', 1 << 11, lambda s, m, r: m.call('TestPlan') == SP_SHORTEN and s.dash_h is not None and r == 'ledge'),
    ('Up-B To Ledge', 1 << 3, lambda s, m, r: m.call('TestPlan') == SP_FF and s.upb_h is not None and r == 'ledge'),
)
LUIGI_ROWS = (
    ('Defaults: a Cyclone', 7, lambda s, m, r: s.cyclone_start is not None),
    ('Defaults and Up B Sweetspot: a sweetspot', 7 | 1 << 4, lambda s, m, r: m.call('TestPlan') == LG_SWEETSPOT and s.upb_h is not None and r == 'ledge'),
    ('Defaults and Backflip: a backflip, no up-B', 7 | 1 << 5, lambda s, m, r: s.backflipped and s.upb_h is None and r == 'ledge'),
)


def table(m, name, stage, label, rows, did, samples):
    """Percent of attempts that did what the row says, then deaths on all six stages."""
    print(f'\n**{name}, {label}**\n')
    print('| | ' + ' | '.join(DISTANCES) + ' |')
    print('| --- |' + ' --- |' * len(DISTANCES))
    cpu = Sim(m, name, stage)
    for height in HEIGHTS:
        cells = []
        for distance in DISTANCES:
            done = 0
            for i in range(samples):
                cpu.launch(height, distance, (i + .5) / samples)
                m.call('TestSeed', 1 + i * 7919)
                done += bool(did(cpu, m, cpu.run(rows)))
            cells.append(str(round(100 * done / samples)))
        print(f'| {height} | ' + ' | '.join(cells) + ' |')
    died = attempts = 0
    for each in STAGES:
        cpu = Sim(m, name, each)
        for height in HEIGHTS:
            for distance in DISTANCES:
                died += sum(n for k, n in spot_rate(m, name, each, rows, height, distance, 2).items() if k not in ('ledge', 'stage'))
                attempts += 2
    print(f'\nDied: {died} of {attempts} on the six legal stages.')


def main():
    build()
    samples = int(sys.argv[1]) if len(sys.argv) > 1 else 8
    m = machine()
    for name in ('Fox', 'Falco'):
        for label, rows, did in SPACIE_ROWS:
            table(m, name, 'Battlefield', label, rows, did, samples)
    for label, rows, did in LUIGI_ROWS:
        table(m, 'Luigi', 'Battlefield', label, rows, did, samples)


if __name__ == '__main__':
    main()
