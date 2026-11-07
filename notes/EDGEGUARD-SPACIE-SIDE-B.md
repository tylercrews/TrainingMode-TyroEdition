# Fox and Falco side-B recoveries in Edgeguard Training

**Status (November 6, 2026, T9):** implemented in [src/recovery.c](../src/recovery.c) (`Think_Spacies` and the `Spacie*` functions above it). The rows have been tried in Dolphin, but **the To Main Stage rework, the Knockback settings, the ledge rules below and Firefox To Ledge came after and haven't been**. [tests/recovery_sim.py](../tests/recovery_sim.py) runs the production AI frame by frame against physics written from the decompilation, on the ISO's stage lines, and [tests/test_edgeguard.py](../tests/test_edgeguard.py) runs a small sample of it. The success rates below come from that, not from the game.

## The options

Fox's and Falco's Recovery Options replace the old Illusion / Phantasm row with six side-B rows. Each attempt (it starts when hitstun ends) the CPU picks one checked side-B row it can still reach, or Firefox To Ledge, or the old up-B logic as one more choice when a Firefox / Firebird row is checked.

| Row | What the CPU does | Side-B height (feet, above the ledge) | Ends how |
| --- | --- | --- | --- |
| Instant | Side-B toward the stage the moment hitstun ends | wherever it is | anything |
| Ledge Sweetspot | Jump up and side-B right into the ledge | 6.5 to 13.5 below | grabs on the endlag's first falling frames, 6 or 7 |
| To Ledge | Side-B short of the ledge; the endlag drifts in | 9 to 13.5 below | grabs in the endlag, 8+ frames in (aims for 9 to 14) |
| To Main Stage | Side-B right above the ledge onto the main stage, through you if you're standing near the ledge | 1 to 5 above, the dash's own height | lands on the stage |
| To Side Plat | Jump high and side-B onto the side platform | 3 to 9 above the platform | lands on the platform, aimed at its middle |
| Shortest Shorten | Jump up close to the ledge, side-B, B again at once | 6 to 15 below | grabs in the endlag |
| Firefox To Ledge | Up-B at the ledge as soon as it can, from wherever it is | none | grabs in flight, or falling in from over the ledge |

- The side-B rows double jump on their own. Firefox To Ledge does too when the ledge is out of range, and with the Double Jump row on, on half its attempts anyway. Otherwise that row is for the older up-B logic. With Fast Fall checked the side-B rows also fast fall when they're more than 15 above their height.
- Ledge Sweetspot aims 1, 2 or 3 dash steps from the ledge, and To Ledge a whole or an at-once-shortened dash, at random among the ones the CPU can still drift to.
- To Main Stage aims near where the CPU already is, up to 6 past a full dash, since the endlag carries it the rest of the way. If you're standing between the ledge and about 46 inland (Fox) it aims through you, but only when it can get there: slashing you never costs it the main stage.
- To Side Plat uses the one-way platform nearest the ledge on that half of the stage, read live (Fountain's move). Without one (Final Destination) it's To Main Stage.
- When a checked row can't make it: Firefox To Ledge if that row is checked; the old up-B logic if another Firefox row is; otherwise the first of Ledge Sweetspot, To Main Stage, Shortest Shorten and To Ledge that can, and otherwise Firefox To Ledge anyway. That last one replaced a double jump and a side-B at its top, which from there only went under the stage.

## Side-B physics

From the decompiled move ([ftfoxspecials.c](https://github.com/doldecomp/melee/blob/master/src/melee/ft/kinds/ftFox/ftfoxspecials.c)) and the NTSC 1.02 special attributes (the same for both, read live by the code):

| Part | Fox | Falco | Notes |
| --- | --- | --- | --- |
| Startup | 20 frames (19 of physics) | 16 (15) | Y speed 0; X speed / 1.5 (0x28), then 0.05 (0x2C) slower a frame. No fall for 15 frames (0x24), then 1/60 a frame (0x30): Fox drops 1/6, Falco 0 |
| Dash | 3 × 18.72 = 56.16 | 3 × 16.5 = 49.5 | 78/4 or 60/4 times the model scale (0.96 / 1.10), at a fixed height. All three steps run unless something stops them. B on its first two frames: no step (see [sweetspot.c](../src/sweetspot.c)) |
| Endlag | 40 frames | 40 frames | 2.0 forward (0x3C), 0.07 slower a frame (0x40): 27.58 in all. No fall for 5 frames (0x44), then 0.08 a frame (0x48) |
| Then | special fall | special fall | mobility 0.4 (0x4C), landing lag 20 (0x50) |

**When it can grab.** The grab box ([mpcoll.c](https://github.com/doldecomp/melee/blob/master/src/melee/mp/mpcoll.c)) spans 13 ± 4.5 above the feet (8.5 to 17.5) and reaches 11 past the front of the ECB, sweeping the frame's whole move. The ECB's bottom must still be outside the ledge and under it, and holding the stick down refuses the grab.

But the game only looks for a ledge on a frame the fighter **moved down** (`cur_pos.y < prev_pos.y`). The dash has no Y track, so its speed is exactly 0 (`ft_80085134`), and the endlag doesn't fall for 5 frames. **So the dash never grabs.** Fox's startup can, on its last few frames. Out of a dash, the earliest grab is the endlag's sixth frame.

**What stops the dash.** The ECB's front point, posed from the ISO's skeletons in the dash (`SpecialAirS`, with [scripts/fighter_ecb_pose.py](../scripts/fighter_ecb_pose.py)): 5.5 over Fox's feet and 3 ahead of them, 6.05 and 3.8 for Falco. The endlag's are larger, about 6.8 and 3.7, and push him back out of a wall. At the front point's height the side of the stage is what matters, measured down from the ledge:

| Stage | The side of the stage |
| --- | --- |
| Battlefield | Sloped wall for only **6**, then a ceiling running inland. A dash below that goes under the stage |
| Pokémon Stadium | Straight for 4, then cutting 14 inland over the next 11 |
| Final Destination | Straight for 10.5, then cutting 20 inland over 10 |
| Dream Land | Nearly straight for 11, then sloping inland |
| Yoshi's Story | Straight for 3, then 3.3 inland by 9 down, and straight again |
| Fountain of Dreams | Nearly straight for 5, then sloping gently inland |

`SpacieWallInset` reads this from the stage's live lines (walls and undersides, not one-way platforms), so nothing here is per-stage.

## Why Ledge Sweetspot suicided

It took a dash reaching the box for a grab, and took the grab to end the dash. Neither is so. Its plan aimed 1, 2 or 3 steps from the ledge at 10 to 16 below, with the height tested at the first frame the simulation agreed, so a rising CPU went at the band's low end. On Battlefield a two-step aim at 16 below called its second step a grab, and the third step went on under the lip. A wall that stops the dash at the top of its height fixes it, which is why Final Destination's tall wall hardly ever showed it: run through the same physics, the old plan died 113 of 240 times on Battlefield, 101 on Pokémon Stadium and 3 on Final Destination.

Now the simulation only grabs on frames that moved down, stops the dash at the wall from the stage's lines, and calls a dash with no wall a miss if it ends within 2 of the ledge. A wall that leaves the feet less than 0.5 outside the ledge doesn't count as one: it slopes, and the fall slides them under. So on Battlefield the CPU goes from 6.5 to about 10 below, where the wall stops it, or from down to 13.5 below only with a whole dash that ends short of the ledge.

## How the CPU decides

Every actionable frame, a side-B plan:

1. **Side-Bs** if its feet are in the plan's height band (To Main Stage: if the dash will be, see below) and a frame-by-frame simulation of a side-B started now (startup, dash, endlag, with the knockback still carrying it, and the wall) ends the way the plan wants.
2. Otherwise **drifts** toward the plan's aim: where the dash should start, less the startup's drift and the knockback still to come.
3. Under the band, **double jumps** at a random planned height if the jump's rise is long enough to drift the rest of the way, or at the last height the jump still reaches the band.
4. **Falls through** to another plan when it can't reach the band any more, or can't drift to the aim in time even saving the jump for last. The height it can reach counts the knockback still lifting it.

**Why To Main Stage goes by the dash's height.** The other rows have a result narrow enough to pin the height: a grab only happens with the ledge in the box. "Lands on the stage" isn't. The side-B zeroes the CPU's own Y speed, but not its knockback, and when hitstun ends there's `0.0096 * knockback` of speed left, which keeps lifting it through the 19 frames of startup. Testing the height at the press let the dash run from 3 to 26 above the ledge in the model, and Battlefield's side platform is 27 above it.

The earliest shorten presses B on the first frame the CPU is seen in the dash. Edgeguard's and the Lab's think run at the same priority (3) as `Fighter_procInput`, and first: procs of one priority run in `p_link` order, and the event's GObj is 7, fighters 8. So a press takes effect the same frame: this B lands on the dash's first frame, and a side-B's first startup frame is the frame it's pressed, which is where the simulation starts.

## Firefox To Ledge

The CPU up-Bs as soon as a shot can reach the ledge, from wherever it is, so it comes in from a different place and angle every attempt. (The first version waited to be exactly a full travel from the ledge, so it fell to the same spot every time.)

The travel goes a fixed distance: 81.5 for Fox (30 frames at 3.8, slowing 0.1 a frame from frame 6), 60.1 for Falco (22 at 4.2, 0.17 from frame 4). Nothing ends it early but a grab or a landing: walls don't bounce it, and only the ground can (`ftFx_SpecialAirHi_Coll`). And only a frame that moved down can grab. So there are two shots, both for a spot 9 outside the ledge and 13 under it, the middle of the box:

- **From over the spot:** straight at it. It's angled down, so it grabs on the way, in either direction, however much travel it had left. This is most attempts.
- **From under it:** a rise can't grab, and a shot at the ledge itself would carry on over it onto the stage, which is Firefox Low. So it aims straight over the ledge, as high as the travel goes, and falls in. The fall after the travel (`SpecialHiFall`) grabs in either direction and can drift, and the CPU steers by where it'd stop, since a flat shot ends fast. From exactly a full travel away, this is a shot that ends right at the ledge.

- **Timing:** each frame it simulates the charge (42 frames, no drift, 6 of fall) to find the launch point, and up-Bs if the spot's in range. Out of range and over the spot, it drifts in, since falling brings it closer. Out of range under the spot, it double jumps, or up-Bs anyway with no jump left.
- **Double Jump row:** half the attempts jump first, from under 30 over the ledge. Any jump then gets a random 1/3 to all of its rise before the up-B, which is most of the variety: it's what gets a CPU that starts under the ledge up over it, for a shot down.
- **Aim:** stick values inside the 0.28 deadzone read 0, on either axis, so a shot is dead on an axis or about 16° off it. A shot down that's too flat takes the 16° if that still reaches the box, and otherwise it's a shot up. A shot up that's close to vertical tilts only if it still ends off the stage. A shot down that has to be dead straight waits to be in reach of the ledge first.

## Modeled success rates

Battlefield, 12 attempts per cell, launched as [the knockback notes](EDGEGUARD-KNOCKBACK.md) describe, each row alone: how often the CPU did what the row says and it ended that way. `python tests/recovery_sim.py` prints these. The rest are nearly always another plan that recovers: over the six legal stages and both ledges, Ledge Sweetspot died 0 of 960 times for each of them, To Ledge 0 for Fox and 6 for Falco, and Firefox To Ledge 0 for each, and with Double Jump on too. Fox / Falco, in percent.

**Ledge Sweetspot**

| | Close | Near | Medium | Far |
| --- | --- | --- | --- | --- |
| Very High | 100 / 100 | 83 / 100 | 92 / 100 | 92 / 100 |
| High | 92 / 100 | 92 / 100 | 100 / 100 | 100 / 100 |
| Level | 67 / 100 | 75 / 100 | 75 / 100 | 100 / 83 |
| Low | 75 / 92 | 100 / 100 | 100 / 100 | 33 / 8 |
| Very Low | 0 / 17 | 33 / 58 | 75 / 92 | 0 / 0 |

**To Ledge**

| | Close | Near | Medium | Far |
| --- | --- | --- | --- | --- |
| Very High | 100 / 100 | 100 / 100 | 100 / 100 | 100 / 100 |
| High | 100 / 100 | 100 / 100 | 100 / 100 | 100 / 100 |
| Level | 100 / 100 | 100 / 92 | 100 / 100 | 100 / 100 |
| Low | 75 / 92 | 67 / 100 | 100 / 100 | 100 / 100 |
| Very Low | 8 / 17 | 0 / 0 | 67 / 92 | 75 / 58 |

**To Main Stage**

| | Close | Near | Medium | Far |
| --- | --- | --- | --- | --- |
| Very High | 100 / 100 | 100 / 100 | 100 / 100 | 100 / 92 |
| High | 100 / 100 | 100 / 100 | 100 / 100 | 58 / 0 |
| Level | 100 / 100 | 100 / 100 | 100 / 100 | 0 / 0 |
| Low | 75 / 92 | 100 / 100 | 100 / 67 | 0 / 0 |
| Very Low | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 |

**To Side Plat** (from the earlier Python model, which had no walls. The simulator has no platforms yet. This row lands, so the ledge rules don't touch it.)

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
| Very High | 92 / 75 | 100 / 100 | 0 / 0 | 0 / 0 |
| High | 100 / 92 | 100 / 100 | 0 / 0 | 0 / 0 |
| Level | 100 / 100 | 17 / 33 | 0 / 0 | 0 / 0 |
| Low | 75 / 83 | 0 / 0 | 0 / 0 | 0 / 0 |
| Very Low | 8 / 8 | 0 / 0 | 0 / 0 | 0 / 0 |

**Firefox To Ledge**

| | Close | Near | Medium | Far |
| --- | --- | --- | --- | --- |
| Very High | 100 / 100 | 100 / 100 | 100 / 100 | 100 / 100 |
| High | 100 / 100 | 100 / 100 | 100 / 100 | 100 / 100 |
| Level | 100 / 100 | 100 / 100 | 100 / 100 | 100 / 100 |
| Low | 75 / 92 | 100 / 100 | 100 / 100 | 100 / 100 |
| Very Low | 8 / 17 | 100 / 100 | 100 / 100 | 100 / 100 |

The low cells at Close are launches that have to start just off the ledge (`LAUNCH_OFF_LEDGE`), so the CPU's already under it: a side-B or an up-B has nothing to do but the fall in, and it often grabs the ledge just falling. One double jump rises about 40, so from Very Low (51 or more under the ledge) the CPU can't get back up to the ledge's height for To Main Stage or To Side Plat. Shortest Shorten only has the endlag's drift, about 25, on top of the grab box, so it needs the CPU to start close.

## Testing in Dolphin

- [ ] Ledge Sweetspot on **Battlefield and Pokémon Stadium**, the two stages it suicided on: the CPU stops at the wall and grabs. It should never fly under the stage. The heights to watch are in `SP_SWEETSPOT_LO` and `SP_SWEETSPOT_HI`.
- [ ] The ECB numbers: if a dash at the top of Battlefield's wall passes under the lip, `SP_FOX_DASH_ECB_Y` / `FRONT` or `SP_WALL_MARGIN` are off.
- [ ] Firefox To Ledge from each Height and Distance: it up-Bs right after hitstun, from a different place each time. From over the ledge it grabs in flight. From under it, it flies up over the ledge and falls in, which leaves it high and helpless for a moment. If shots down land on the stage or hit the wall, change `SP_FF_OUT` / `SP_FF_H`. With Double Jump on, about half the attempts should jump first.
- [ ] Each other row alone on Battlefield at a few Height and Distance spots: does the CPU do what the tables say?
- [ ] To Ledge: the CPU floats into the ledge slowly for a few frames.
- [ ] Shortest Shorten: the dash really shortens (no step). If it takes a step, the B press is a frame late.
- [ ] To Main Stage: the dash runs right above the ledge, every time, at High, Level and Low from Near and Medium. It never reaches side platform height, and it never goes for the ledge unless it can't get above it. Stand near the ledge: the dash goes through you.
- [ ] To Side Plat on Battlefield, Yoshi's Story, Dream Land, Pokemon Stadium and Fountain of Dreams (moving platforms); To Main Stage instead on Final Destination.
- [ ] Ledgehog: with you on the ledge, the ledge options should fail like a real Fox's would.
- [ ] Old saves: a save with Illusion checked comes back with Illusion Instant checked, and Fast Fall where it was.
- [ ] Training Lab: a recovering Fox or Falco CPU uses the same rows.
