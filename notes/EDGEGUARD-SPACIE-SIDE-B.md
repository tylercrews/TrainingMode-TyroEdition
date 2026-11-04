# Fox and Falco side-B recoveries in Edgeguard Training

**Status (November 3, 2026, T9):** implemented in [src/recovery.c](../src/recovery.c) (`Think_Spacies` and the `Spacie*` functions above it). The rows have been tried in Dolphin, but **the To Main Stage rework and the Knockback settings came after and haven't been**. [tests/test_edgeguard.py](../tests/test_edgeguard.py) runs To Main Stage's plan in the production code. The success rates below come from a Python model of the same logic, not from the game.

## The options

Fox's and Falco's Recovery Options replace the old Illusion / Phantasm row with six side-B rows. Each attempt (it starts when hitstun ends) the CPU picks one checked side-B row it can still reach, or its up-B as one more choice when a Firefox / Firebird row is checked.

| Row | What the CPU does | Side-B height (feet, above the ledge) | Ends how |
| --- | --- | --- | --- |
| Instant | Side-B toward the stage the moment hitstun ends | wherever it is | anything |
| Ledge Sweetspot | Jump up and side-B into the ledge | 10 to 16 below | grabs during the dash, after 1, 2 or 3 steps |
| To Ledge | Side-B short of the ledge; the endlag drifts in | 9 to 14.5 below | grabs in the endlag, 5+ frames in (aims for 6 to 12) |
| To Main Stage | Side-B right above the ledge onto the main stage, through you if you're standing near the ledge | 1 to 5 above, the dash's own height | lands on the stage |
| To Side Plat | Jump high and side-B onto the side platform | 3 to 9 above the platform | lands on the platform, aimed at its middle |
| Shortest Shorten | Jump up close to the ledge, side-B, B again at once | 6 to 15 below | grabs in the endlag |

- The side-B rows double jump on their own; the Double Jump row is only for the up-B logic. With Fast Fall checked they also fast fall when they're more than 15 above their height.
- Ledge Sweetspot picks 1, 2 or 3 dash steps, and To Ledge a whole or an at-once-shortened dash, at random among the ones the CPU can still drift to.
- To Main Stage aims near where the CPU already is, up to 6 past a full dash, since the endlag carries it the rest of the way. If you're standing between the ledge and about 46 inland (Fox) it aims through you, but only when it can get there: slashing you never costs it the main stage.
- To Side Plat uses the one-way platform nearest the ledge on that half of the stage, read live (Fountain's move). Without one (Final Destination) it's To Main Stage.
- When a checked row can't make it: the up-B if a Firefox row is checked; otherwise the first of Ledge Sweetspot, To Main Stage, Shortest Shorten and To Ledge that can, then a double jump toward the stage and a side-B at its top.

## Side-B physics

From the decompiled move ([ftfoxspecials.c](https://github.com/doldecomp/melee/blob/master/src/melee/ft/kinds/ftFox/ftfoxspecials.c)) and the NTSC 1.02 special attributes (the same for both, read live by the code):

| Part | Fox | Falco | Notes |
| --- | --- | --- | --- |
| Startup | 20 frames (19 of physics) | 16 (15) | Y speed 0; X speed / 1.5 (0x28), then 0.05 (0x2C) slower a frame. No fall for 15 frames (0x24), then 1/60 a frame (0x30): Fox drops 1/6, Falco 0 |
| Dash | 3 × 18.72 = 56.16 | 3 × 16.5 = 49.5 | 78/4 or 60/4 times the model scale (0.96 / 1.10), at a fixed height. B on its first two frames: no step (see [sweetspot.c](../src/sweetspot.c)) |
| Endlag | 40 frames | 40 frames | 2.0 forward (0x3C), 0.07 slower a frame (0x40): 27.58 in all. No fall for 5 frames (0x44), then 0.08 a frame (0x48) |
| Then | special fall | special fall | mobility 0.4 (0x4C), landing lag 20 (0x50) |

All three parts can grab the ledge (`ftCliffCommon_80081298` in each collision callback). The grab box ([mpcoll.c](https://github.com/doldecomp/melee/blob/master/src/melee/mp/mpcoll.c)) spans 13 ± 4.5 above the feet (8.5 to 17.5) and reaches 11 past the front of the ECB, sweeping the frame's whole move. The ECB's bottom must still be outside the ledge and under it, and holding the stick down refuses the grab.

## How the CPU decides

Every actionable frame, a side-B plan:

1. **Side-Bs** if its feet are in the plan's height band (To Main Stage: if the dash will be, see below) and a frame-by-frame simulation of a side-B started now (startup, dash, endlag, with the knockback still carrying it) ends the way the plan wants. A dash step that would jump past the ledge while under it counts as a miss: it goes into the wall or under the stage.
2. Otherwise **drifts** toward the plan's aim: where the dash should start, less the startup's drift and the knockback still to come.
3. Under the band, **double jumps** at a random planned height if the jump's rise is long enough to drift the rest of the way, or at the last height the jump still reaches the band.
4. **Falls through** to another plan when it can't reach the band any more, or can't drift to the aim in time even saving the jump for last. The height it can reach counts the knockback still lifting it.

**Why To Main Stage goes by the dash's height.** The other rows have a result narrow enough to pin the height: a grab only happens with the ledge in the box. "Lands on the stage" isn't. The side-B zeroes the CPU's own Y speed, but not its knockback, and when hitstun ends there's `0.0096 * knockback` of speed left, which keeps lifting it through the 19 frames of startup. Testing the height at the press let the dash run from 3 to 26 above the ledge in the model, and Battlefield's side platform is 27 above it. The other two complaints about this row came from falling back when it couldn't reach: to Ledge Sweetspot, or the last-ditch side-B at the top of a double jump.

The earliest shorten presses B on the first frame the CPU is seen in the dash. Edgeguard's and the Lab's think run at the same priority (3) as `Fighter_procInput`, and first: procs of one priority run in `p_link` order, and the event's GObj is 7, fighters 8. So a press takes effect the same frame: this B lands on the dash's first frame, and a side-B's first startup frame is the frame it's pressed, which is where the simulation starts.

## Modeled success rates

Battlefield, 120 attempts per cell, launched as [the knockback notes](EDGEGUARD-KNOCKBACK.md) describe, each row alone and no Firefox row checked: how often the CPU did that row's side-B and it ended as planned (for To Main Stage, with the dash 1 to 5 above the ledge). The rest are mostly another side-B or, from Very Low and Far, a last-ditch side-B. Fox / Falco, in percent.

**Ledge Sweetspot**

| | Close | Near | Medium | Far |
| --- | --- | --- | --- | --- |
| Very High | 100 / 100 | 100 / 100 | 100 / 100 | 100 / 100 |
| High | 100 / 100 | 100 / 100 | 100 / 100 | 98 / 70 |
| Level | 100 / 100 | 100 / 100 | 100 / 100 | 42 / 6 |
| Low | 100 / 100 | 100 / 100 | 100 / 97 | 0 / 0 |
| Very Low | 41 / 55 | 71 / 83 | 89 / 53 | 0 / 0 |

**To Ledge**

| | Close | Near | Medium | Far |
| --- | --- | --- | --- | --- |
| Very High | 100 / 100 | 100 / 100 | 100 / 100 | 100 / 100 |
| High | 100 / 100 | 100 / 100 | 100 / 100 | 100 / 100 |
| Level | 100 / 100 | 100 / 100 | 100 / 100 | 100 / 100 |
| Low | 100 / 100 | 48 / 88 | 100 / 100 | 100 / 97 |
| Very Low | 25 / 38 | 0 / 2 | 75 / 99 | 84 / 46 |

**To Main Stage**

| | Close | Near | Medium | Far |
| --- | --- | --- | --- | --- |
| Very High | 100 / 100 | 100 / 100 | 100 / 100 | 100 / 78 |
| High | 100 / 100 | 100 / 100 | 100 / 100 | 55 / 10 |
| Level | 100 / 100 | 100 / 100 | 100 / 100 | 0 / 0 |
| Low | 100 / 100 | 100 / 100 | 94 / 58 | 0 / 0 |
| Very Low | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 |

**To Side Plat**

| | Close | Near | Medium | Far |
| --- | --- | --- | --- | --- |
| Very High | 100 / 100 | 100 / 100 | 100 / 99 | 0 / 0 |
| High | 93 / 100 | 100 / 100 | 81 / 36 | 0 / 0 |
| Level | 22 / 96 | 100 / 100 | 5 / 0 | 0 / 0 |
| Low | 0 / 0 | 0 / 3 | 0 / 0 | 0 / 0 |
| Very Low | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 |

**Shortest Shorten**

| | Close | Near | Medium | Far |
| --- | --- | --- | --- | --- |
| Very High | 100 / 100 | 100 / 100 | 3 / 5 | 0 / 0 |
| High | 100 / 100 | 100 / 100 | 0 / 0 | 0 / 0 |
| Level | 100 / 100 | 46 / 58 | 0 / 0 | 0 / 0 |
| Low | 100 / 100 | 0 / 0 | 0 / 0 | 0 / 0 |
| Very Low | 29 / 42 | 0 / 0 | 0 / 0 | 0 / 0 |

The zeros are physics. One double jump rises about 40, so from Very Low (51 or more under the ledge) the CPU can't get back up to the ledge's height for To Main Stage or To Side Plat. Shortest Shorten only has the endlag's drift, about 25, on top of the grab box, so it needs the CPU to start close. Far ends around the end of a full side-B: drifting in takes time, and only a start high enough gives it that.

## Testing in Dolphin

- [ ] Each row alone on Battlefield at a few Height and Distance spots: does the CPU do what the tables say?
- [ ] Ledge Sweetspot: the grab happens mid-dash, not in the wall. Try 1-step sweetspots (the CPU starts about 25 out).
- [ ] To Ledge: the CPU floats into the ledge slowly for a few frames.
- [ ] Shortest Shorten: the dash really shortens (no step). If it takes a step, the B press is a frame late.
- [ ] To Main Stage: the dash runs right above the ledge, every time, at High, Level and Low from Near and Medium. It never reaches side platform height, and it never goes for the ledge unless it can't get above it. Stand near the ledge: the dash goes through you.
- [ ] To Side Plat on Battlefield, Yoshi's Story, Dream Land, Pokemon Stadium and Fountain of Dreams (moving platforms); To Main Stage instead on Final Destination.
- [ ] Ledgehog: with you on the ledge, the ledge options should fail like a real Fox's would.
- [ ] Old saves: a save with Illusion checked comes back with Illusion Instant checked, and Fast Fall where it was.
- [ ] Training Lab: a recovering Fox or Falco CPU uses the same rows.
