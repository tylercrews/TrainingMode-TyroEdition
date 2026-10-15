# Ledge returns: mechanics and feasibility

Investigation date: October 15, 2026. This covers Melee NTSC 1.02, the
Ledgechase CPU on Final Destination, and the Ledgedash event's new Help menu.

## Findings

A drop followed by an aerial jump is feasible across the playable roster.
It is different from pressing jump while hanging, which starts the native
ledge-jump animation. A ledgedash adds an air dodge into the stage floor.
The available inputs do not limit ledgedashing to a small character list;
the difficult parts are collision clearance, consistency and remaining
grounded actionable ledge intangibility (GALINT).

The implementation now offers both options as session-only percentages.
**Ledgedashing is experimental for every character.** Native input checks
and event sequencing have automated coverage; full animated returns have
not been certified in Dolphin. There is no measured per-character success
rate or maximum-GALINT timing table in this change. The conservative dash
may have no GALINT, and a miss uses the existing death/timeout retry.

## Native sequence

1. Finish the ledge-grab animation. `CliffCatch` has no input interrupt.
   `CliffWait` processes ledge options and arms a neutral-stick latch.
2. Reset the sticks, then press away or down to drop. Ledge release needs
   that latch; an early held input can fail. Away avoids introducing a down
   input that can cause a fastfall after release.
3. Jump on a subsequent simulation frame, holding inward. Jumping while
   still hanging gives the built-in ledge jump instead. The native aerial
   jump check verifies that a jump remains and dispatches by character.
4. For a hop, drift onto stage and fastfall once descending and safely
   inward. For a ledgedash, air dodge inward/down once there is clearance.
5. Let native collision and landing recovery finish before starting the
   selected grounded followup. Special landing cannot be interrupted by
   the event to manufacture extra actionable frames.

Relevant primary source: [CliffCatch and its grab effect](https://github.com/doldecomp/melee/blob/master/src/melee/ft/ftcliffcommon.c),
[CliffWait](https://github.com/doldecomp/melee/blob/master/src/melee/ft/kinds/ftCommon/ftCo_CliffWait.c),
[release latch](https://github.com/doldecomp/melee/blob/master/src/melee/ft/kinds/ftCommon/ftCo_CliffClimb.c),
[aerial jump dispatch](https://github.com/doldecomp/melee/blob/master/src/melee/ft/kinds/ftCommon/ftCo_JumpAerial.c),
[air dodge](https://github.com/doldecomp/melee/blob/master/src/melee/ft/kinds/ftCommon/ftCo_EscapeAir.c),
and [landing recovery](https://github.com/doldecomp/melee/blob/master/src/melee/ft/kinds/ftCommon/ftCo_Landing.c).
The source was also inspected in the adjacent local Melee decompilation;
the tests execute the corresponding native routines from `build/Start.dol`.

## Character handling

These groups account for every playable internal kind, including both Ice
Climbers and both Zelda/Sheik forms. All groups use native inputs and native
jump behavior. None has a verified full-game return success rate yet.

| Characters | Native jump behavior / implementation |
| --- | --- |
| Mario, Dr. Mario, Fox, Falco, Captain Falcon, Ganondorf, Donkey Kong, Bowser, Link, Young Link, Sheik, Popo, Nana, Pikachu, Pichu, Samus, Luigi, Marth, Roy, Zelda, Mr. Game & Watch | Ordinary aerial-jump entry uses each fighter's own attributes. Inward input and observed clearance replace a shared fixed frame delay. |
| Ness | Animation-driven aerial jump with its own horizontal physics callback. Let native animation supply the rise after one jump press. |
| Yoshi | Animation-driven jump and distinct animation/armor handling. Preserve native behavior; armor is not ledge intangibility. |
| Peach | Animation-driven jump with a delayed rise. Release jump immediately after acceptance to avoid triggering float. The generic return is not a parasol/ECB setup or a promise of useful GALINT. |
| Mewtwo | Animation-driven aerial jump. Wait for observed clearance rather than treating initial vertical velocity as ordinary jump height. |
| Kirby, Jigglypuff | Multijump states and later jumps that can accept a held jump button. Detect the consumed jump count, then release X to avoid consuming more jumps. |

The native source distinguishes these entry paths; this is not a roster
whitelist. Ice Climbers have independent return phases and clearance checks.
A sleeping Zelda/Sheik alternate remains excluded from CPU control.
See also the primary [multijump implementation](https://github.com/doldecomp/melee/blob/master/src/melee/ft/kinds/ftCommon/ftCo_JumpAerialF1.c).
Yoshi's [rising waveland guide](https://www.theyoshiguide.com/mechanics/jump-and-platform-mechanics/rising-waveland)
also demonstrates why one universal optimal air-dodge angle is inappropriate.
Animation-driven jump physics read the animation's vertical translation;
they do not need sustained jump-button input. In contrast, Peach can enter
float while holding jump when vertical velocity is nonpositive. The controller
therefore releases jump as soon as it observes acceptance for every fighter.
See [native animation-driven physics](https://github.com/doldecomp/melee/blob/master/src/melee/ft/ft_084E.c)
and [Peach float input checks](https://github.com/doldecomp/melee/blob/master/src/melee/ft/kinds/ftPeach/ftpeachfloat.c).

## Collision, protection and timing

The environment collision box (ECB) determines floor/wall contact; the
character's visible feet alone are not sufficient. Its bottom can be locked
temporarily, and the approach to ledge can affect the subsequent collision
state. This is why a fixed instruction such as "dodge N frames after jump"
cannot be assumed to work optimally for the whole roster and every regrab.

The current CPU controller uses a deliberately conservative 45-degree
inward/down air dodge. It checks the lower of the fighter origin and current
ECB bottom, relative to the ledge. Height must cover any remaining outward
distance plus a two-unit margin, and the fighter must be within ten units
outside the edge. Checking the origin too avoids relying entirely on a high
ECB bottom that may change on air-dodge entry. This is a geometric heuristic,
not a simulation of future animation/collision. It can sacrifice GALINT or
reach a normal landing before a dash is possible. It does not insert a dodge
after such a landing or silently replace the attempt with a native getup.

GALINT counts protected **grounded actionable** time. Ground contact alone
does not qualify: landing recovery must finish. Ledge protection continues
to expire during the sequence. Air-dodge intangibility is separate and does
not make a return continuously protected. Peach's conventional jump is a
particular obstacle to a useful invincible return; specialized recovery/ECB
setups are outside this controller. We intentionally do not publish exact
GALINT maxima or a list of "impossible" characters based on these tests.

## The circle around the grabbing hand

The ledge-grab effect appears at the ledge as the character grabs it. Watch
its expanding circle near the grabbing hand to learn the delay before
release, ideally with reduced game speed and the existing early-input Tips.
**Its first appearance is not permission to drop.** The grab animation still
has to finish. Use the circle and grab sound to learn a repeatable rhythm,
then practice the actual recovery/regrab setup you use in matches.

This cue is described in [Kadano's original frame-data investigation](https://smashboards.com/threads/kadanos-perfect-marth-class-advanced-frame-data-application.337035/).
The game source independently shows the effect being spawned at grab and
the lack of input interrupts during `CliffCatch`. We have not measured a
universal circle radius or animation frame that guarantees the earliest
release for every character. The in-game Help therefore avoids that claim.

## Validation and next steps

`tests/test_ledgechase.py` exercises the production event as PowerPC code and
an optimized DAT loaded through real MEX relocations. It covers six-way
selection/rebalancing, old presets, native release and jump/air-dodge input
checks for internal kinds 0..26, separate drop/jump frames, mirrored ledges,
consumed inputs between event ticks, hold behavior, clearance rejection,
fastfall, landing recovery, follower independence, freeze and reset.

Native animation/physics entry endpoints are fixtures in these tests. They
prove input acceptance and controller behavior, **not a completed physical
trajectory**. `test_event_trails.py` also inspects the relocated Ledgedash
Help submenu and its four-line guidance; existing HUD and exit checks remain
applicable. Font fit still needs a visual check in game.

For in-game verification, use an isolated test card:

1. In Ledgechase, choose the CPU and set Ledge Hop Chance to 100%. Use Safe
   player distance and CPU Action None so the player does not interrupt it.
2. Check both ledges at 0% and 100%, including repeated trials and Nana.
3. Repeat at Ledge Dash Chance 100%. Record drops, jump frames, dodge
   acceptance, actual waveland contact, first legal grounded action and GALINT.
   A grounded hop without an air dodge does not certify a ledgedash.
4. Prioritize Fox/Falco, Marth/Sheik/Falcon, the four animation-driven jumps,
   Kirby/Puff, and Ice Climbers before filling out the remaining roster.
5. Use the measured trajectories to tune character-specific angles or timing
   profiles. Keep failed/unverified profiles experimental. Real ledge regrabs
   must be tested separately from this event's directly initialized hang.

The seven original saved presets retain their old distributions and identities.
Seven appended presets now cover hop/dash-only, five-/six-way mixes, 75% dash
with 25% native ledge jump, and 50/50 hop/dash. Manual percentages remain
session-only. The version-9 settings extension also saves an independent
aerial attack for ledge jump/hop; see [Ledgechase Training](LEDGECHASE-TRAINING.md)
for timing, repeated Neutral B and migration details.
