"""Execute the production Passing Practice event against a small engine, frame by frame.

tests/passing_probe.c supplies the teammate's jumpsquat, jump, drift and
aerial (one scripted hitbox), knockback that decays like the game's, Final
Destination's floor, hitlag and L-cancels, around the real Event_Think.
Your hits are scripted: the enemy launched at a given speed and angle.
"""
from pathlib import Path
import math
import subprocess
import unittest
from unicorn import UC_HOOK_CODE
import test_settings as fixtures
from test_lowlaser import load_probe

ROOT = Path(__file__).resolve().parents[1]
SERVE, VOLLEY, RESULT = range(3)
NONE, MATE, YOU = range(3)
AERIAL_ROW, PERCENT_ROW, MODE_ROW, HITSTUN_ROW, DISTANCE_ROW, SIDE_ROW, FACING_ROW = range(7)
NAIR, FAIR, BAIR, UAIR, DAIR = range(5)
FROZEN, RISING = range(2)
OFF, ON = range(2)
LEFT, RIGHT, RANDOM = range(3)
TOWARD, AWAY = range(2)
FOX, MARTH, PEACH, PUFF, BOWSER, PIKACHU = range(6)
STARTUP = {FOX: 3, MARTH: 4, PEACH: 5, PUFF: 5, BOWSER: 8, PIKACHU: 3}
DPAD_LEFT, DPAD_RIGHT, DPAD_DOWN = 1, 2, 4
WAIT, RUNBRAKE = 14, 23  # Grounded and able to act.
SERVE_DELAY, RESULT_FRAMES = 30, 45
WHITE, CYAN, RED = 0xFFFFFF, 0x00FFFF, 0xFF2828
NEW_BEST_SFX = 303
DISTANCES = (20, 30, 42, 55, 70)
# Defaults: Back Air, 40%, Frozen %, Off, Medium, Left, Toward.
DEFAULTS = (BAIR, 4, FROZEN, OFF, 2, LEFT, TOWARD)


def build():
    subprocess.run([
        'C:/devkitPro/devkitPPC/bin/powerpc-eabi-gcc.exe', '-O2', '-w',
        '-ffreestanding', '-fno-builtin', '-msdata=none', '-mcpu=750', '-nostdlib',
        '-Wl,-Ttext=0x101000,-e,TestPassInit,--unresolved-symbols=ignore-all',
        'tests/passing_probe.c', '-o', 'build/passing-probe.elf'
    ], cwd=ROOT, check=True)


def signed(value):
    return value - (1 << 32) if value & 0x80000000 else value


class Event:
    """One run of the event: the probe's teammate and enemy, and your controls."""
    def __init__(self, mate=FOX, enemy=MARTH, aerial=None, script=None):
        self.m = fixtures.Machine()
        load_probe(self.m, 'passing-probe.elf', 'TestPassInit')
        # load_probe runs the init with no fighters picked; pick them.
        self.m.call('TestPassInit', mate, enemy)
        if aerial is not None:
            self.choose(AERIAL_ROW, aerial)
        if script:
            self.script(*script)
        self.step()

    def info(self, key): return signed(self.m.call('TestPassInfo', key))
    def step(self, frames=1):
        # One frame a call: a planning frame is a good share of the emulator's budget.
        for _ in range(frames):
            self.m.call('TestPassThink', 1)
    def text(self, row):
        return bytes(self.m.cpu.mem_read(self.m.call('TestPassText', row), 48)).split(b'\0', 1)[0].decode()
    def choose(self, row, value): self.m.call('TestPassChoose', row, value)
    def script(self, first, last, x, y, r):
        self.m.call('TestPassScript', first, last, int(x * 10) & 0xFFFFFFFF, int(y * 10) & 0xFFFFFFFF, int(r * 10))
    def press(self, buttons):
        self.m.call('TestPassPad', buttons)
        self.step()
        self.m.call('TestPassPad', 0)
    def phase(self): return self.info(0)
    def hits(self): return self.info(1)
    def last(self): return self.info(2)
    def enemy(self): return self.info(7) / 10, self.info(8) / 10
    def mate(self): return self.info(12) / 10, self.info(13) / 10
    def you_x(self): return self.info(19) / 10
    def toward(self): return self.info(17)
    def mate_hits(self): return self.info(15)
    def mate_ready(self): return WAIT <= self.info(14) <= RUNBRAKE
    def preferences(self): return tuple(signed(self.m.call('TestPassPreference', row)) for row in range(7))
    def you_facing(self): return self.info(41)
    def hud(self): return [self.text(row) for row in range(3)]

    def serve(self, limit=150):
        """Until your teammate's serve hits the enemy."""
        for _ in range(limit):
            self.step()
            if self.phase() == VOLLEY:
                return True
        return False

    def until_reachable(self, limit=150, ready=True):
        """After your teammate's touch: until the enemy is near you or about to
        land, and with ready, your teammate is on the ground and can act."""
        prev_y = self.enemy()[1]
        for _ in range(limit):
            self.step()
            if self.phase() != VOLLEY:
                return False
            x, y = self.enemy()
            near = abs(x - self.you_x()) < 10
            landing = y < 8 and y < prev_y and self.info(34)
            prev_y = y
            if ((near and y < 30) or landing) and (not ready or self.mate_ready()):
                return True
        return False

    def you_hit(self, speed, angle, damage=8, hitstun=None):
        """Your hit sends the enemy toward your teammate, with Melee's hitstun
        for that launch speed unless it's given: floor(0.4 * knockback), and
        the speed is 0.03 * knockback."""
        vx = -self.toward() * speed * math.cos(math.radians(angle))
        vy = speed * math.sin(math.radians(angle))
        if hitstun is None:
            hitstun = int(speed / .03 * .4)
        self.m.call('TestPassYouHit', int(vx * 100) & 0xFFFFFFFF, int(vy * 100) & 0xFFFFFFFF, damage, hitstun)

    def wait_for_return(self, limit=150):
        """After your hit: True once your teammate hits the enemy back."""
        before = self.mate_hits()
        for _ in range(limit):
            self.step()
            if self.mate_hits() > before:
                return True
            if self.phase() != VOLLEY:
                return False
        return False

    def pass_back(self, speed=2.2, angle=45):
        """One pass: wait for the enemy, hit it, and see it returned."""
        if not self.until_reachable():
            return False
        self.you_hit(speed, angle)
        return self.wait_for_return()

    def until(self, phase, limit=300):
        for _ in range(limit):
            if self.phase() == phase:
                return True
            self.step()
        return self.phase() == phase


class PassingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): build()

    # Where everyone starts.
    def test_you_and_your_teammate_start_the_distance_apart_on_your_sides(self):
        e = Event()
        self.assertEqual(e.toward(), -1)  # You on the left.
        self.assertAlmostEqual(e.you_x(), -21, places=1)
        self.assertAlmostEqual(e.mate()[0], 21, places=1)
        for distance, value in enumerate(DISTANCES):
            e.choose(DISTANCE_ROW, distance)
            e.choose(SIDE_ROW, RIGHT)
            e.step()
            self.assertEqual(e.toward(), 1)
            self.assertAlmostEqual(e.you_x(), value / 2, places=1)
            self.assertAlmostEqual(e.mate()[0], -value / 2, places=1)

    def test_your_teammate_faces_away_for_back_air_and_toward_you_for_the_rest(self):
        e = Event()
        for aerial in (NAIR, FAIR, BAIR, UAIR, DAIR):
            e.choose(AERIAL_ROW, aerial)
            e.step()
            facing = e.info(33)
            self.assertEqual(facing, -e.toward() if aerial == BAIR else e.toward(), aerial)

    def test_random_side_picks_again_each_volley(self):
        e = Event()
        e.choose(SIDE_ROW, RANDOM)
        sides = set()
        for value in (0, 1, 0, 1):
            self.m_random(e, value)
            e.press(DPAD_DOWN)
            sides.add(e.toward())
        self.assertEqual(sides, {-1, 1})

    def m_random(self, e, value): e.m.call('TestPassRandom', value)

    # The serve.
    def test_the_serve_sends_the_enemy_toward_you(self):
        e = Event()
        self.assertTrue(e.info(25))  # Floating, waiting for the serve.
        e.step(SERVE_DELAY - 3)
        self.assertEqual(e.phase(), SERVE)
        self.assertTrue(e.serve())
        self.assertEqual((e.last(), e.hits(), e.mate_hits()), (MATE, 0, 1))
        self.assertGreater(e.info(9) * e.toward(), 0)  # Knockback toward you.
        self.assertFalse(e.info(25))

    def test_the_serve_waits_on_the_floor_for_its_delay(self):
        e = Event()
        e.step(SERVE_DELAY - 3)
        self.assertEqual(e.mate()[1], 0)
        self.assertEqual(e.info(14), WAIT)

    def test_the_jumpsquat_is_measured_on_the_serve(self):
        for mate in (FOX, MARTH, PEACH, BOWSER):
            e = Event(mate, MARTH)
            self.assertTrue(e.serve())
            self.assertEqual(e.info(24), STARTUP[mate], mate)

    def test_the_first_serve_measures_the_aerial_and_later_enemies_wait_in_its_hitbox(self):
        e = Event()
        self.assertEqual((e.info(20), e.info(21)), (0, 0))
        self.assertTrue(e.serve())
        self.assertEqual((e.info(20), e.info(21)), (4, 1))  # The script's first frame.
        e.press(DPAD_DOWN)
        e.step(SERVE_DELAY - 4)
        spot = (e.info(22), e.info(23))  # Settled, just before the jump.
        for _ in range(30):
            e.step()
            if e.phase() != SERVE:
                break
            self.assertEqual((e.info(22), e.info(23)), spot)  # It never has to move.
        self.assertEqual(e.phase(), VOLLEY)

    def test_a_far_off_guess_still_serves_by_moving_the_enemy_into_the_hitbox(self):
        e = Event(script=(4, 12, -18, 18, 4))
        spot = (e.info(22), e.info(23))
        self.assertTrue(e.serve())
        self.assertNotEqual((e.info(22), e.info(23)), spot)
        self.assertEqual(e.last(), MATE)

    def test_a_serve_that_misses_tries_again(self):
        # A hitbox that only lasts its first frame: the move into it comes too late.
        e = Event(script=(4, 4, -18, 18, 4))
        self.assertFalse(e.serve(limit=SERVE_DELAY + 25))
        self.assertTrue(e.serve(limit=120))  # Now it waits right there.

    # The volley.
    def test_your_hits_count_once_for_each_pass(self):
        e = Event()
        self.assertTrue(e.serve())
        for n in range(1, 5):
            self.assertTrue(e.pass_back(), n)
            self.assertEqual(e.hits(), n)
        # Hitting it twice before your teammate does is still one pass.
        self.assertTrue(e.until_reachable())
        e.you_hit(1.2, 80)
        e.step(6)
        self.assertEqual(e.hits(), 5)
        e.you_hit(2.2, 45)
        e.step(2)
        self.assertEqual((e.hits(), e.last()), (5, YOU))

    def test_your_teammate_waits_while_you_pass_to_yourself_then_returns_your_pass(self):
        # An up tilt you'll back air: it comes down nearer you, so it's still yours.
        e = Event(FOX, MARTH)
        self.assertTrue(e.serve())
        self.assertTrue(e.until_reachable())
        home = e.mate()[0]
        e.you_hit(1.6, 88)
        prev_y = e.enemy()[1]
        for _ in range(80):
            e.step()
            self.assertEqual(e.mate()[1], 0)  # No jump for the up tilt.
            y = e.enemy()[1]
            if y < prev_y and y < 14:
                break
            prev_y = y
        self.assertEqual((e.phase(), e.hits()), (VOLLEY, 1))
        self.assertLessEqual(abs(e.mate()[0] - home), 2)
        # Then your back air passes it: still one hit, and it comes back.
        e.you_hit(2.2, 45)
        e.step()
        self.assertEqual((e.hits(), e.last()), (1, YOU))
        self.assertTrue(e.wait_for_return())
        e.step()  # The event sees the hit's damage the frame after.
        self.assertEqual((e.hits(), e.last()), (1, MATE))

    def test_your_teammate_returns_passes_that_come_down_near_it(self):
        setups = [(FOX, FOX, BAIR, None), (FOX, MARTH, BAIR, None), (MARTH, MARTH, FAIR, (4, 7, 12, 8, 6)),
                  (PEACH, PEACH, NAIR, (3, 20, 3, 6, 7)), (BOWSER, PIKACHU, FAIR, (10, 14, 10, 9, 7))]
        near = returned = 0
        report = []
        for mate, enemy, aerial, script in setups:
            e = Event(mate, enemy, aerial, script)
            for speed in (1.8, 2.4, 3.0, 3.6):
                for angle in (20, 40, 60, 80):
                    e.press(DPAD_DOWN)
                    if not e.serve() or not e.until_reachable():
                        continue
                    e.you_hit(speed, angle)
                    e.m.call('TestPassPlanNow')
                    # Where it would land untouched: past your teammate is positive.
                    end = signed(e.m.call('TestPassDebug', 13)) / 10
                    beyond = (end - e.mate()[0]) * -e.toward()
                    ok = e.wait_for_return()
                    if -15 <= beyond <= 15:
                        near += 1
                        returned += ok
                        if not ok:
                            report.append((mate, enemy, aerial, speed, angle, round(beyond, 1)))
        print(f'\nreturned {returned} of {near} passes landing within 15 of your teammate; missed {report}')
        self.assertGreaterEqual(near, 20)
        self.assertGreaterEqual(returned / near, .8)

    def test_your_teammate_walks_back_to_its_spot_and_turns_after_a_pass(self):
        e = Event(FOX, MARTH)
        self.assertTrue(e.serve())
        self.assertTrue(e.until_reachable())
        e.you_hit(3.4, 25)  # Long: your teammate drifts back for it.
        self.assertTrue(e.wait_for_return())
        for _ in range(240):
            e.step()
            if e.mate_ready() and abs(e.mate()[0] - 21) <= 2 and e.info(33) == 1:
                break
        self.assertLessEqual(abs(e.mate()[0] - 21), 2)
        self.assertEqual(e.info(33), 1)  # Back Air: facing away from you, on the left.

    def test_your_teammate_l_cancels_its_aerials(self):
        e = Event(FOX, MARTH)
        self.assertTrue(e.serve())
        self.assertTrue(e.until_reachable())
        self.assertGreaterEqual(e.info(40), 1)

    # Drops and records.
    def test_a_drop_ends_the_volley_then_restarts_with_previous_and_session_best(self):
        e = Event()
        self.assertEqual(e.hud(), ['Hits: 0', 'Session Best Frozen %: --', 'Previous: --'])
        self.assertTrue(e.serve())
        self.assertTrue(e.pass_back())
        self.assertTrue(e.pass_back())
        loads = e.info(16)
        self.assertTrue(e.until(RESULT))  # Nobody hits it this time.
        self.assertEqual(e.hud(), ['Hits: 2', 'Session Best Frozen %: 2', 'Previous: 2'])
        self.assertEqual((e.info(31), e.info(32)), (CYAN, NEW_BEST_SFX))
        e.step(RESULT_FRAMES - 1)
        self.assertEqual(e.phase(), RESULT)
        e.step()
        self.assertEqual((e.phase(), e.info(16)), (SERVE, loads + 1))
        self.assertEqual(e.hud(), ['Hits: 0', 'Session Best Frozen %: 2', 'Previous: 2'])
        self.assertEqual(e.info(31), WHITE)
        # A shorter volley is red and only changes Previous.
        self.assertTrue(e.serve())
        self.assertTrue(e.pass_back())
        self.assertTrue(e.until(RESULT))
        self.assertEqual(e.hud(), ['Hits: 1', 'Session Best Frozen %: 2', 'Previous: 1'])
        self.assertEqual(e.info(31), RED)

    def test_a_volley_with_no_hits_records_zero(self):
        e = Event()
        self.assertTrue(e.serve())
        self.assertTrue(e.until(RESULT))
        self.assertEqual(e.hud()[1:], ['Session Best Frozen %: 0', 'Previous: 0'])
        self.assertEqual(e.info(31), RED)

    def test_the_enemy_catching_the_ledge_ends_the_volley(self):
        e = Event()
        self.assertTrue(e.serve())
        self.assertTrue(e.pass_back())
        loads = e.info(16)
        e.m.call('TestPassEnemyOnLedge')  # Still airborne, as on the real ledge.
        e.step()
        self.assertEqual(e.phase(), RESULT)
        self.assertEqual(e.hud(), ['Hits: 1', 'Session Best Frozen %: 1', 'Previous: 1'])
        e.step(RESULT_FRAMES)
        self.assertEqual((e.phase(), e.info(16)), (SERVE, loads + 1))

    # Reset If Hitstun Ends.
    def test_reset_if_hitstun_ends_has_the_enemy_jump_away_and_ends_the_volley(self):
        e = Event()
        e.choose(PERCENT_ROW, 0)  # A serve with little hitstun, which runs out well before it lands.
        e.choose(HITSTUN_ROW, ON)
        e.step()
        self.assertEqual(e.preferences()[HITSTUN_ROW], ON)
        self.assertTrue(e.serve())
        loads = e.info(16)
        for _ in range(60):
            e.step()
            if e.phase() != VOLLEY:
                break
        self.assertEqual(e.phase(), RESULT)
        self.assertEqual((e.info(42), e.info(34)), (0, 1))  # Out of hitstun, in the air.
        x = e.enemy()[0]
        nearer = e.you_x() if abs(x - e.you_x()) < abs(x - e.mate()[0]) else e.mate()[0]
        away = 1 if x > nearer else -1
        # A double jump away from whichever of you is nearer.
        self.assertEqual((e.info(43), e.info(45)), (1, away))
        self.assertGreater(e.info(44) * away, 100)
        self.assertEqual(e.hud(), ['Hits: 0', 'Session Best Frozen %: 0', 'Previous: 0'])
        e.step(RESULT_FRAMES)
        self.assertEqual((e.phase(), e.info(16), e.info(43)), (SERVE, loads + 1, 1))

    def test_without_reset_if_hitstun_ends_the_enemy_never_acts(self):
        e = Event()
        e.choose(PERCENT_ROW, 0)
        e.step()
        self.assertTrue(e.serve())
        for _ in range(60):
            e.step()
            if not e.info(42) or e.phase() != VOLLEY:
                break
        self.assertEqual((e.phase(), e.info(42), e.info(34)), (VOLLEY, 0, 1))  # Out of hitstun, still falling.
        e.step(3)
        self.assertEqual((e.phase(), e.info(43)), (VOLLEY, 0))

    def test_your_own_pass_has_to_keep_it_in_hitstun_too(self):
        e = Event()
        e.choose(HITSTUN_ROW, ON)
        e.step()
        self.assertTrue(e.serve())
        self.assertTrue(e.until_reachable())
        e.you_hit(1.6, 88, hitstun=6)  # A weak up tilt: it can act long before it's back down.
        for _ in range(20):
            e.step()
            if e.phase() != VOLLEY:
                break
        self.assertEqual((e.phase(), e.hits(), e.info(43)), (RESULT, 1, 1))
        self.assertEqual(e.hud()[1:], ['Session Best Frozen %: 1', 'Previous: 1'])

    def test_each_percent_mode_keeps_its_own_session_best_and_previous(self):
        e = Event()
        self.assertTrue(e.serve())
        self.assertTrue(e.pass_back())
        self.assertTrue(e.pass_back())
        self.assertTrue(e.until(RESULT))
        e.choose(MODE_ROW, RISING)
        e.step()
        self.assertEqual(e.hud(), ['Hits: 0', 'Session Best Rising %: --', 'Previous: --'])
        # Rising percents fly farther each pass, so one is enough here.
        self.assertTrue(e.serve())
        self.assertTrue(e.pass_back())
        self.assertTrue(e.until(RESULT))
        self.assertEqual(e.hud()[1:], ['Session Best Rising %: 1', 'Previous: 1'])
        self.assertEqual((e.info(3), e.info(4), e.info(5), e.info(6)), (2, 2, 1, 1))
        e.choose(MODE_ROW, FROZEN)
        e.step()
        self.assertEqual(e.hud()[1:], ['Session Best Frozen %: 2', 'Previous: 2'])

    # Percents.
    def test_frozen_percent_puts_the_enemy_back_after_every_hit(self):
        e = Event()
        e.choose(PERCENT_ROW, 7)
        e.step()
        self.assertEqual((e.info(10), e.info(11)), (70, 70))
        self.assertTrue(e.serve())
        e.step()
        self.assertEqual((e.info(10), e.info(11)), (70, 70))
        self.assertTrue(e.pass_back())
        e.step()
        self.assertEqual((e.info(10), e.info(11)), (70, 70))

    def test_rising_percent_adds_up_until_the_next_volley(self):
        e = Event()
        e.choose(MODE_ROW, RISING)
        e.step()
        self.assertEqual(e.info(10), 40)
        self.assertTrue(e.serve())
        e.step()
        self.assertEqual(e.info(10), 52)  # The probe's aerial does 12.
        self.assertTrue(e.pass_back())
        e.step()
        # Your 8, then 12 more. The game's own HUD shows damage; the event only
        # sets it when it sets the percent.
        self.assertEqual(e.info(10), 72)
        self.assertTrue(e.until(RESULT))
        self.assertTrue(e.until(SERVE))
        self.assertEqual(e.info(10), 40)

    def test_percents_go_from_0_to_200_in_tens(self):
        e = Event()
        for value in (0, 5, 20):
            e.choose(PERCENT_ROW, value)
            e.step()
            self.assertEqual(e.info(10), value * 10)

    # Controls.
    def test_you_start_facing_your_teammate_or_facing_away(self):
        e = Event()
        self.assertEqual(e.you_facing(), -e.toward())  # Toward your teammate.
        e.choose(FACING_ROW, AWAY)
        e.step()
        self.assertEqual((e.you_facing(), e.preferences()[FACING_ROW]), (e.toward(), AWAY))
        e.choose(SIDE_ROW, RIGHT)
        e.step()
        self.assertEqual((e.toward(), e.you_facing()), (1, 1))

    def test_dpad_down_flips_your_facing_saves_it_and_restarts_without_counting_the_volley(self):
        e = Event()
        self.assertTrue(e.serve())
        self.assertTrue(e.pass_back())
        loads = e.info(16)
        e.press(DPAD_DOWN)
        self.assertEqual((e.phase(), e.hits(), e.info(16)), (SERVE, 0, loads + 1))
        self.assertEqual(e.hud()[1:], ['Session Best Frozen %: --', 'Previous: --'])
        self.assertEqual((e.you_facing(), e.preferences()[FACING_ROW]), (e.toward(), AWAY))
        e.press(DPAD_DOWN)
        self.assertEqual((e.you_facing(), e.preferences()[FACING_ROW]), (-e.toward(), TOWARD))

    def test_dpad_left_and_right_pick_your_side_and_save_it(self):
        e = Event()
        e.press(DPAD_RIGHT)
        self.assertEqual((e.toward(), e.preferences()[SIDE_ROW]), (1, RIGHT))
        self.assertGreater(e.you_x(), 0)
        e.press(DPAD_LEFT)
        self.assertEqual((e.toward(), e.preferences()[SIDE_ROW]), (-1, LEFT))
        self.assertLess(e.you_x(), 0)

    def test_menu_changes_are_saved_and_start_a_new_volley(self):
        e = Event()
        self.assertEqual(e.preferences(), DEFAULTS)
        self.assertTrue(e.serve())
        loads = e.info(16)
        e.choose(AERIAL_ROW, FAIR)
        e.choose(PERCENT_ROW, 9)
        e.choose(DISTANCE_ROW, 4)
        e.choose(HITSTUN_ROW, ON)
        self.assertEqual(e.preferences(), (FAIR, 9, FROZEN, ON, 4, LEFT, TOWARD))
        e.step()
        self.assertEqual((e.phase(), e.info(16)), (SERVE, loads + 1))
        self.assertAlmostEqual(e.mate()[0] - e.you_x(), 70, places=1)

    def test_a_new_aerial_is_measured_again(self):
        e = Event()
        self.assertTrue(e.serve())
        self.assertEqual(e.info(21), 1)
        e.choose(AERIAL_ROW, NAIR)
        e.step()
        self.assertEqual((e.info(20), e.info(21)), (0, 0))
        self.assertTrue(e.serve())
        self.assertEqual(e.info(21), 1)

    # Cost: the planner runs every frame your teammate goes for a pass.
    def test_the_busiest_think_stays_cheap(self):
        e = Event(PEACH, PEACH, NAIR, (3, 20, 3, 6, 7))
        self.assertTrue(e.serve())
        self.assertTrue(e.until_reachable())
        e.you_hit(2.6, 45)
        count, worst = [0], 0
        def hook(uc, address, size, data): count[0] += 1
        for _ in range(60):
            handle = e.m.cpu.hook_add(UC_HOOK_CODE, hook)
            count[0] = 0
            e.m.call('TestPassPlanNow')
            e.m.cpu.hook_del(handle)
            worst = max(worst, count[0])
            e.step()
            if e.phase() != VOLLEY:
                break
        print(f'\nbusiest planning frame: {worst} instructions')
        self.assertLess(worst, 200000)


if __name__ == '__main__':
    unittest.main(verbosity=2)
