"""Trace one Passing Practice return in the probe, frame by frame.

python tests/passing_trace.py MATE ENEMY AERIAL SPEED ANGLE

MATE and ENEMY index the probe's fighters (0 Fox, 1 Marth, 2 Peach, 3
Jigglypuff, 4 Bowser, 5 Pikachu), AERIAL the menu's (0 Neutral, 1 Forward,
2 Back, 3 Up, 4 Down Air). After the serve, once the enemy reaches you and
your teammate can act, your hit sends it back at SPEED units a frame and ANGLE
degrees. Each row shows the enemy, your teammate, its state and category, the
enemy's predicted flight (frames, lands, where), whether it's coming down
nearer your teammate than you (pass: only then is a plan used), and the plan
the event would pick on that frame: valid, wait, full hop, hit frame,
prediction frame, feet x, or the nearest out-of-reach plan's stick and x. The
plan is worked out before the frame's hitlag countdown, so it can be a frame
off from the event's own.
"""
import sys
from test_passing import *

mate, enemy, aerial = (int(arg) for arg in sys.argv[1:4])
speed, angle = float(sys.argv[4]), float(sys.argv[5])
scripts = {FAIR: (4, 7, 12, 8, 6), NAIR: (3, 20, 3, 6, 7)}
build()
e = Event(mate, enemy, aerial, scripts.get(aerial))
print('serve', e.serve(), 'reach', e.until_reachable())
e.you_hit(speed, angle)
print('you at', e.you_x(), 'teammate', e.mate(), 'toward', e.toward(), 'facing', e.info(33))
debug = lambda key: signed(e.m.call('TestPassDebug', key))
for f in range(120):
    e.m.call('TestPassPlanNow')
    print(f, 'phase', e.phase(), 'enemy %.1f %.1f' % e.enemy(), 'mate %.1f %.1f' % e.mate(), 'state', e.info(14),
          'cat', debug(10), 'path', debug(0), debug(1), 'end %.1f' % (debug(13) / 10), 'pass', debug(16),
          'plan', debug(2), 'w', debug(3), 'full', debug(4), 'j', debug(5), 'T', debug(6), 'x %.1f' % (debug(7) / 10),
          'near', debug(8), 's %.2f' % (debug(9) / 100), 'nx %.1f' % (debug(11) / 10),
          'aerial', debug(15), 'returns', e.mate_hits())
    e.step()
    if e.phase() != VOLLEY:
        print('phase', e.phase())
        break
