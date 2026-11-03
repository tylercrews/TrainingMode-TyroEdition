# Fox and Falco side-B recoveries in Edgeguard Training

**Status (November 3, 2026, T9):** implemented in [src/recovery.c](../src/recovery.c) (`Think_Spacies` and the `Spacie*` functions above it). It compiles and the save wiring is tested, but **it hasn't been watched in Dolphin yet**. The success rates below come from a Python model of the same logic, not from the game.

## The options

Fox's and Falco's Recovery Options replace the old Illusion / Phantasm row with six side-B rows. Each attempt (it starts when hitstun ends) the CPU picks one checked side-B row it can still reach, or its up-B as one more choice when a Firefox / Firebird row is checked.

| Row | What the CPU does | Side-B height (feet, above the ledge) | Ends how |
| --- | --- | --- | --- |
| Instant | Side-B toward the stage the moment hitstun ends | wherever it is | anything |
| Ledge Sweetspot | Jump up and side-B into the ledge | 10 to 16 below | grabs during the dash, after 1, 2 or 3 steps |
| To Ledge | Side-B short of the ledge; the endlag drifts in | 9 to 14.5 below | grabs in the endlag, 5+ frames in (aims for 6 to 12) |
| To Stage | Side-B onto the stage, through you if you're standing near the ledge | 3 to 10 above | lands on the stage |
| To Side Plat | Jump high and side-B onto the side platform | 3 to 9 above the platform | lands on the platform, aimed at its middle |
| Shortest Shorten | Jump up close to the ledge, side-B, B again at once | 6 to 15 below | grabs in the endlag |

- The side-B rows double jump on their own; the Double Jump row is only for the up-B logic. With Fast Fall checked they also fast fall when they're more than 15 above their height.
- Ledge Sweetspot picks 1, 2 or 3 dash steps, and To Ledge a whole or an at-once-shortened dash, at random among the ones the CPU can still drift to.
- To Stage aims near where the CPU already is, and through you when you're standing between the ledge and about 46 inland (Fox).
- To Side Plat uses the one-way platform nearest the ledge on that half of the stage, read live (Fountain's move). Without one (Final Destination) it's To Stage.
- When a checked row can't make it: the up-B if a Firefox row is checked; otherwise the first of Ledge Sweetspot, To Stage, Shortest Shorten and To Ledge that can, then a double jump toward the stage and a side-B at its top.

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

1. **Side-Bs** if its feet are in the plan's height band and a frame-by-frame simulation of a side-B started now (startup, dash, endlag, with the knockback still carrying it) ends the way the plan wants. A dash step that would jump past the ledge while under it counts as a miss: it goes into the wall or under the stage.
2. Otherwise **drifts** toward the plan's aim: where the dash should start, less the startup's drift and the knockback still to come.
3. Under the band, **double jumps** at a random planned height if the jump's rise is long enough to drift the rest of the way, or at the last height the jump still reaches the band.
4. **Falls through** to another plan when it can't reach the band any more, or can't drift to the aim in time even saving the jump for last.

The earliest shorten presses B on the first frame the CPU is seen in the dash. Edgeguard's think runs at the same priority (3) as `Fighter_procInput`, so the press lands on the dash's first or second frame. Both give no step. A one- or two-step shorten would depend on which, so the CPU never uses one.

## Modeled success rates

Battlefield, 400 attempts per cell, each row alone and no Firefox row checked: how often the CPU did that row's side-B and it ended as planned. The rest are mostly another side-B (Ledge Sweetspot, or To Ledge) or, on Shallow, a last-ditch side-B. Fox / Falco.

| Row | Weak | Normal | Hard | Shallow |
| --- | --- | --- | --- | --- |
| Ledge Sweetspot | 100% / 100% | 100% / 100% | 96% / 85% | 12% / 8% |
| To Ledge | 100% / 100% | 55% / 83% | 100% / 100% | 27% / 23% |
| To Stage | 84% / 94% | 82% / 73% | 64% / 40% | 0% |
| To Side Plat | 0% | 0% | 50% / 28% | 0% |
| Shortest Shorten | 95% / 97% | 12% / 16% | 8% / 4% | 0% |

Where hitstun ends decides it. Weak ends 9 to 40 out and 28 to 41 under the ledge, Normal 28 to 64 out and 21 to 40 under, Hard 37 to 118 out and 7 under to 87 over, Shallow 57 to 109 out and 44 to 89 under. One double jump rises about 40, so To Side Plat (a platform 25 to 30 above the ledge) needs Hard or a Custom Hit Strength that ends high, and Shortest Shorten needs the CPU to end close.

## Testing in Dolphin

- [ ] Each row alone on Battlefield, Weak, Normal and Hard: does the CPU do what the table says?
- [ ] Ledge Sweetspot: the grab happens mid-dash, not in the wall. Try 1-step sweetspots (the CPU starts about 25 out).
- [ ] To Ledge: the CPU floats into the ledge slowly for a few frames.
- [ ] Shortest Shorten: the dash really shortens (no step). If it takes a step, the B press is a frame late.
- [ ] To Stage: the dash goes through you when you stand near the ledge, and it lands on the stage.
- [ ] To Side Plat on Battlefield, Yoshi's Story, Dream Land, Pokemon Stadium and Fountain of Dreams (moving platforms); To Stage instead on Final Destination.
- [ ] Ledgehog: with you on the ledge, the ledge options should fail like a real Fox's would.
- [ ] Old saves: a save with Illusion checked comes back with Illusion Instant checked, and Fast Fall where it was.
- [ ] Training Lab: a recovering Fox or Falco CPU uses the same rows.
