# Edgeguard Knockback Height and Distance

**Status (November 3, 2026, T9):** implemented in [src/edgeguard.c](../src/edgeguard.c) (`ChooseLaunch`, `SolveLaunch`, `LaunchClearsStage`, `LoadKnockback`) and [src/edgeguard.h](../src/edgeguard.h) (the two tables). [tests/test_edgeguard.py](../tests/test_edgeguard.py) runs the production code for all 13 CPUs on every spot and replays each launch frame by frame, but **it hasn't been watched in Dolphin yet**.

## The settings

Hit Strength said how hard the CPU was hit, so you had to learn where each choice ended up, and the answer changed with the CPU. These two rows say where it ends up. Both name the spot where the CPU can act again when hitstun ends: its feet relative to the ledge.

| Knockback Height | Feet relative to the ledge |
| --- | --- |
| Very High | 51 to 61 above |
| High | 23 to 33 above |
| Level | 5 below to 5 above |
| Low | 23 to 33 below |
| Very Low | 51 to 61 below |

| Knockback Distance | Out from the ledge |
| --- | --- |
| Close | 20 to 30 |
| Near | 40 to 50 |
| Medium | 60 to 70 |
| Far | 80 to 90 |

That's 20 spots from two short rows. **Random** on either row picks one of that row's choices each reset, so one axis can stay fixed while the other varies, without choosing specific combinations. Each attempt also lands somewhere random inside the spot's ranges.

**Custom** is on Knockback Height. It uses the Customize Knockback submenu (the old Customize Hit Strength: knockback, angle and percent ranges) and greys out Knockback Distance, which it doesn't use. Changing a custom value selects Custom, as before.

The default, Low and Near, is the spot closest to the old default, Normal. The CPU's percent is 50-80 on every spot, Normal's range.

## How a spot becomes a launch

A fixed table can't do this. Over 45 frames of hitstun, Fox falls about 65 farther than Jigglypuff (gravity 0.23 and top speed 2.8 against 0.064 and 1.3), so the same knockback ends in very different places. Side-B Sweetspot gets away with one table because it only launches Fox and Falco.

So `SolveLaunch` works it out for the CPU, stage and launch spot at each reset. From [`Fighter_procUpdate`](https://github.com/doldecomp/melee/blob/master/src/melee/ft/fighter.c), with the CPU launched straight from a standstill:

- Hitstun lasts `floor(0.4 * knockback)` frames.
- Knockback speed starts at `0.03 * knockback` and drops 0.051 a frame along its angle, so after N frames it has carried the CPU `N * 0.03 * knockback - 0.051 * N * (N + 1) / 2`.
- The CPU's own fall adds on top, by its gravity up to its top speed, with no air drift in hitstun.

For each length of hitstun, the fall says where the knockback must carry the CPU, which gives one distance, one angle and one knockback. The right length is the first one that knockback doesn't outlast. Hitstun is a whole number of frames, so the CPU can end up to 1.8 from the exact spot, well inside the spot's own ±5.

The numbers come from the game (`force_applied_to_kb_mag_multiplier`, `x154`, `kb_frameDecay`, `attr.gravity`, `attr.terminal_velocity`). Launches start at y = 0, so the spot is measured from the ledge's real height: Yoshi's Story and Fountain of Dreams end in the same place as everywhere else.

**The CPU's DI does nothing here.** The game applies DI in `ftCo_Damage_OnExitHitlag`, a `post_hitlag_cb` that only real hits set (`ftCo_Damage.c`). `Reset_CPU` writes `kb_vel` directly and changing state clears the callback, so the launch angle is exactly what's written. `DIIn` still runs in hitlag and has no effect on this launch. A real hit from the player later is DI'd as usual.

## Launching off the ledge

The CPU normally launches from 15 inland, or from the player's spot on At Ledge (5 inland). Some spots can't be reached from there. The CPU would have to go nearly flat or downward, which would hit the stage, and landing in hitstun is a knockdown.

`LaunchClearsStage` replays the part of the launch over the stage. If the feet get within 1 of the ground, the launch starts 2 off the ledge instead, where a flat or downward angle is safe. The angle can go as low as -63 degrees on the floatiest CPUs at Very Low, Close.

Which spots need it (from 15 inland, with C, N, M and F for the distances):

| CPU | Launches off the ledge on |
| --- | --- |
| Fox, Falco | Very Low C N |
| Captain Falcon | Low C, Very Low C N M |
| Marth, Yoshi | Low C N M, Very Low all |
| Sheik | Low C N, Very Low all |
| Zelda, Peach, Luigi, Dr. Mario, Ice Climbers, Samus | Low all, Very Low all |
| Jigglypuff | Level C, Low all, Very Low all |

Floaty CPUs need it most because they barely fall during hitstun, so the only way to end low is to be sent there. It reads as a spike off the ledge, away from the player's bair. The launch is already hand-placed, so nothing depends on the bair reaching the CPU.

## Saves

No version bump. Knockback Height took over Hit Strength's field (word 60 bits 0-2), and Knockback Distance uses word 68's spare bits 29-31, stored plus 1. Those bits are 0 in every older record, so 0 means the height field still holds Hit Strength, and `LoadKnockback` converts it. The first save writes both new values.

| Old Hit Strength | Height, Distance | Why |
| --- | --- | --- |
| Weak | Low, Close | ended 9 to 40 out, 28 to 41 under (Fox) |
| Normal | Low, Near | 28 to 64 out, 21 to 40 under |
| Hard | High, Medium | 37 to 118 out, 7 under to 87 over |
| Shallow | Very Low, Far | 57 to 109 out, 44 to 89 under |
| Custom | Custom | the custom values never moved |

## Testing in Dolphin

- [ ] Each Height with Fox and with Jigglypuff at Near: does the CPU act at about the same height on both?
- [ ] Each Distance at Level: Close should be just off the ledge, Far around the end of a full Fox side-B.
- [ ] Very Low on Jigglypuff, Samus and Peach: the launch off the ledge shouldn't catch the wall under the ledge. Try stages with different undersides (Dream Land, Yoshi's Story, Pokemon Stadium).
- [ ] Yoshi's Story and Fountain of Dreams: Level still ends at the ledge.
- [ ] Player Position At Ledge: launches start at your spot and still end on the chosen spot.
- [ ] Random on one row only: the other axis stays put.
- [ ] Custom greys out Distance, editing a custom value selects Custom, and leaving Custom restores Distance.
- [ ] Old saves: each old Hit Strength comes back as the table says, then stays on what you pick.
