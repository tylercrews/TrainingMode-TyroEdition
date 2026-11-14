# Escape events research (T10)

The numbers behind **Escape Samus Up-B** ([src/escapesamus.c](../src/escapesamus.c)) and **Escape IC Grab (Beta)** ([src/escapeic.c](../src/escapeic.c)), and research on the Ice Climbers' handoffs for a later version of the IC event.

Move data comes from the vanilla NTSC 1.02 ISO's `PlSs.dat` and `PlPp.dat` subaction scripts. Common constants come from `PlCo.dat`'s `ftLoadCommonData`. Native behavior was read from the disassembled `Start.dol` (devkitPPC's `powerpc-eabi-objdump` on the DOL's text sections). Frame numbers in the move tables are the scripts' own timer counts. The events never depend on how those map to "frame 1" conventions: they identify hits by their properties, not by frame.

## Escape Samus Up-B

### Screw Attack, from `PlSs.dat`

Samus's model scale is 0.88, which scales every size and offset below in game. The hitboxes sit on bone 0 in two columns, 5.86 to either side of her, one at her feet and one 9.38 up.

**Grounded (SpecialHi, state 353)**

| Frames | Hits | Damage | Angle (low / high) | KB growth | Base KB (low / high) | Set KB | Size | Element |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 4 | opener | 2% | 100 / 100 | 100 | 130 / 100 | 0 | 3.12 | normal |
| 6, 8, 10 | 3 | 1% | 105 / 100 | 100 | 110 / 70 | 0 | 2.73 | normal |
| 12 to 26, every 2 | 8 | 1% | 110 / 200 | 100 | 70 / 40 | 0 | 2.34 | normal |
| 28-29 | launcher | 1% | 70 | 100 | 0 | 80 (weight-dependent) | 7.81, at (1.56, 5.08) | electric |

She doesn't rise until frame 5. Then her root motion carries her up fast: 1.1 at frame 5, 4.2 at 6, 14.2 at 8, 26.2 at 10, 36.7 at 12 and 40.1 at 13. She hangs at 42 to 44.5 from frame 14 to past 30, so the eight-hit loop and the launcher happen at the top.

**Aerial (SpecialAirHi, state 354)**

| Frames | Hits | Damage | Angle (low / high) | KB growth | Base KB (low / high) | Set KB | Element |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 4, 6, 8 | 3 | 1% | 110 / 140 | 0 | 100 / 25 | 0 | normal |
| 10 to 24, every 2 | 8 | 1% | 110 / 140 | 0 | 80 / 25 | 0 | normal |
| 26-27 | launcher | 1% | 70 | 100 | 0 | 80 | electric |

The aerial loop's knockback growth is 0, so its hits knock you the same distance at any percent: that version traps you the same way at every percent. The grounded loop's growth is 100, so higher percents knock you farther out of it. The aerial version rises by physics (2.5 up against 0.066 gravity), not root motion. The event uses the grounded version, as when Samus up-Bs out of shield.

**Why it pulls you in.** A hit's horizontal direction comes from the two fighters' positions, not the attacker's facing (`ftColl_8007A06C`, see [the Passing Practice notes](../DEVELOPMENT.md#passing-practice-t10)). Angles past 90 point back toward the attacker. So the loop's 100-110 degrees lift you and pull you slightly toward Samus, and the high column's 200 degrees in the second loop push you down and toward her. The launcher's 70 degrees sends you up and away. Its hitbox is about three times as wide as the loop's (6.9 at her scale) and centered 1.4 in front of her, so it still catches you at the edge of a column.

**Hitlag.** In Melee a hit's hitlag is `floor(damage / 3 + 3)` frames, so every Screw Attack hit gives 3 frames. Electric hits multiply it by 1.5, so the launcher gives 4.

### Smash DI in the DOL

- **The check is `8008E4F0`.** Damage entry (`8008E2A8`) installs it as the victim's `cb.EveryHitlag` (`fp + 0x21D0`). `Fighter_UpdatePhys` (`8006B82C`) calls it each hitlag frame, at `8006BE00`. It runs only while the fighter is in hitlag (`fp + 0x221A` bit `0x20`). If the stick is at least PlCo `0x4B0` long (0.7) and either axis's tilt timer (`fp + 0x670`, `0x671`) is under PlCo `0x4B4` (4), it adds the stick times PlCo `0x4B8` (6.0) to your position. Then it sets both timers to 254 and adds the distance to your match stats (`800401F0`, which accumulates at `+0xCDC`/`+0xCE0`).
- **The tilt timers.** The input proc (`8006B140` on) resets an axis's timer to 0 on a fresh crossing of PlCo `0x8` (inside last frame, past it now). It counts up to 254 while the axis stays past, and reads 254 while the axis is inside. So an SDI needs a fresh smash: moving from straight to a diagonal crosses the other axis, but moving from a diagonal to straight doesn't. That's why quarter-circle SDI works.
- **No SDI from a held stick.** Damage entry also sets both timers to 254 (`8008E2C4`, `8008E2D0`), so a stick held from before the hit never SDIs.
- **ASDI is `8008E714`,** installed as `cb.ExitHitlag`. When hitlag ends it moves you PlCo `0x4BC` (3.0) along the stick, or along the C-stick when that's held. It then applies trajectory DI (`8008E5A4`, at most PlCo `0x1A8`, 18 degrees).
- **The TM SDI OSD** hooks `8008E52C` inside the check, and counts only while the SDI OSD is on. Its counters share a union with the Wavedash OSD's data.

### How the event counts

- **SDI inputs.** Each frame, before the physics proc, and again after the frame's damage, the event replaces your `cb.EveryHitlag` with a wrapper whenever it's the native `8008E4F0`. Each hit's damage entry puts the native one back. The wrapper calls the native function and counts the call if your position changed, because the native check only moves you when it takes an SDI. That's every input the game took, OSD or not.
- **The last hit.** An s_link 15 proc runs after hit detection and damage. A hit on you that frame shows up as a rise in `dmg.percent + dmg.percent_temp`, and Samus's hitboxes are still the ones that hit. The launcher is her only Screw Attack hitbox with set knockback (`wdsk` 80), so a hit while one is active is the last hit.
- **The result.** The attempt resolves once Samus's animation is past frame 33 (every hitbox is gone by 30) or she leaves the up-B. Launched if the last hit landed, Escaped if any other hit did, Samus Missed if none did. A miss isn't counted.
- **Holding you still.** Until her first hit you can't act. In the air you're held in Fall the way Passing Practice floats its enemy. On the ground, Wait's interrupt callback is cleared every frame. Your stick still feeds the tilt timers, so SDIing the first hit works as it does in a match. Locking your controller instead would have handed you a free SDI on the first hit if you were holding a direction.

### To check in Dolphin

- Samus's up-B comes out on the setup's last frame, from the B-and-up CPU press, on Battlefield at every distance, both sides and both positions.
- The SDI count matches the global SDI OSD's "successful" count (turn it on to compare).
- Holding a direction through the setup gives no SDI on the first hit.
- The launcher detection: a hit by her last hit says Launched, and falling out says Escaped.
- The Air position (feet 6 up) is caught by her opener for a few characters, tall and short.
- Ice Climbers players get solo Popo (`force_sopo`).

## Escape IC Grab (Beta)

### The grab timer

When a grab connects (`800DA8E4`), the game sets the grabbed fighter's timer through `8007DBCC` to:

    timer = 30 + 8 x (9 - handicap) + 15 x (4 - (place + 1)) + 1.6 x percent

These are PlCo `0x354` (30), `0x358` (8), `0x35C` (9), `0x360` (15), `0x364` (4) and `0x368` (1.6). Place counts from 0 for first. `8007DBCC` also zeroes the stick-direction bytes (`fp + 0x1A50`, `0x1A51`), so your first tilt in any direction counts. The event holds every grab as if the match were tied with handicap off, as Grab Punish Practice does: **75 + 1.6 x percent** frames without mashing. For multiples of 10% that's always a whole number of frames: the native fused multiply-add rounds `1.6 x percent` to an exact integer. That gives 75 at 0%, then 16 more each step up to 395 at 200%.

### Mashing

- **The capture tick.** CapturePulled's and CaptureWait's animation callbacks (`800DB8A4`, `800DB908`, in the state table at `803C440C`) take PlCo `0x3A4` (1) off the timer each frame. Then they call `Fighter_CheckGrabBreakout` (`8007DC08`) with PlCo `0x3A8` (6). Only CaptureWait lets go, when the timer reaches 0 or less, through `800DA698`. Both callbacks also store whether you mashed that frame (`fp + 0x2348`).
- **What counts.** `8007DC08` takes 6 for any fresh press in `input.down & 0x80000F00` (A, B, X, Y). It takes 6 more when the stick's direction changes: each axis keeps a sticky -1/0/+1, set past PlCo `0x308` either way, and a change on either axis counts. That's at most one button and one stick input a frame, so 13 off a frame at the most.
- **Pummels freeze it.** CaptureDamage, the flinch when you're hit while held, runs neither callback, so the timer doesn't move and mashing does nothing. That's the mechanism behind wobbling (below).

Escape time in frames, by mash speed:

| Percent | No mash | 10 inputs/s | 20 inputs/s | 30 inputs/s | Fastest possible |
| --- | --- | --- | --- | --- | --- |
| 0% | 75 | 38 | 25 | 19 | 6 |
| 50% | 155 | 78 | 52 | 39 | 12 |
| 100% | 235 | 118 | 79 | 59 | 19 |
| 150% | 315 | 158 | 105 | 79 | 25 |
| 200% | 395 | 198 | 132 | 99 | 31 |

An input every 6 frames (10 a second) takes 2 a frame on average. 20 a second takes 3, 30 takes 4. The fastest possible is 13 a frame: a fresh button every frame (alternating) and a new stick direction every frame.

### How the event counts

Because the tick is an animation callback (s_link 1), the event measures in an s_link 15 proc. On the frame the grab connects it sees the full timer the grab set. On every frame after, it sees the timer after that frame's tick: the drop is 1 + 6 x that frame's inputs, rounded and capped at 2 inputs. The frame you get out is counted too, so not mashing at 0% reads exactly 75. Popo presses Z on the setup's second frame, as Grab Punish's teammate does. A missed grab is tried again from the next of four gaps (8, 5, 11, 14). You can't act until the grab connects (Wait's interrupt callback is cleared), so mashing early can't dodge it. Both climbers run with no AI, so Nana stands still.

### To check in Dolphin

- Popo's grab connects from 8 for a range of characters (the retries cover the rest).
- Not mashing gives exactly the No-Mash Hold (75 at 0%), and the count matches a slow, countable mash.
- The CaptureJump release (holding up as it ends) still ends the attempt.
- Nana doesn't follow Popo or act while he holds you.

## Ice Climbers handoffs (research for a later version)

### What a handoff is

A **handoff** is a chain grab: one climber throws, and the other, acting separately (desynced), regrabs you before you can act. Handoffs became the Ice Climbers' main grab game after most tournaments banned wobbling (from 2019-2020). The other climber can act during grabs and throws in Melee, so the desync comes free with the grab.

### Popo's grab and throws, from `PlPp.dat`

| Move | Damage | Angle | KB growth | Base KB | Notes |
| --- | --- | --- | --- | --- | --- |
| Grab (Catch) | 0% | - | - | - | out frames 6-7: radius 4.49 on the hand (bone 26), 3.12 on bone 22 |
| Pummel | 3% | 80 | 100 | 0 | set KB 30, frame 12 |
| Forward throw | 5% | 45 | 60 | 70 | plus a 6% hitbox on frame 24 |
| Back throw | 7% | 60 | 30 | 80 | |
| Up throw | 5% | 88 | 28 | 110 | plus 6% hitboxes on frame 25 |
| Down throw | 6% | 80 | 40 | 70 | puts you right in front of them |
| Release | 3% | 361 | 100 | 40 | the throws' release hitbox |

Melee plays a throw faster or slower with the victim's weight, so its release frame depends on who's thrown.

### The techniques

1. **Down throw, Nana regrab.** This is the basic handoff. The down throw pops you low and almost straight up (80 degrees), and Nana, a step behind, grabs you once you land. Players describe the cue as "once they hit the ground, regrab", with a small walk forward first. One handoff is always possible, while a second is "possible but iffy" ([Smashboards Q&A](https://smashboards.com/threads/general-ics-q-a-thread.199633/page-106), [Smashboards](https://smashboards.com/threads/ice-climbers-grabs-and-desynces.344412/page-3)).
2. **Handoff chains near the ledge.** Popo down throws (sometimes forward throws) and Nana regrabs. Nana's throw is chosen by her AI, and near the edge she always throws toward it, so the chain becomes predictable. SmashWiki calls handoffs "only a true infinite when done near the edge of the stage (due to the CPU AI's throw behavior)" ([SmashWiki: Wobbling](https://www.ssbwiki.com/Wobbling), [SmashWiki: Chain grab](https://www.ssbwiki.com/Chaingrab)).
3. **The cookhold.** SmashWiki lists it as a variation of handoffs that, like them, needs the climbers close to the ledge so Nana's throw is predictable ([SmashWiki: Ice Climbers' grab](https://www.ssbwiki.com/Ice_Climbers_(SSBM)/Grab)). I couldn't find a step-by-step description.
4. **Down throw, Nana down air, regrab.** Nana's down air during Popo's down throw holds you on the ground and forces a stand-up that can be regrabbed. It's escapable with SDI ([SmashWiki: Ice Climbers' grab](https://www.ssbwiki.com/Ice_Climbers_(SSBM)/Grab), [SmashWiki: Chain grab](https://www.ssbwiki.com/Chaingrab)).
5. **Down throw into Nana's smash attacks.** At kill percents, Popo down throws and Nana up-smashes, forward-smashes or down-smashes you as you land (the "down throw forward smash" idea).
6. **Wobbling (banned).** Popo pummels while Nana forward-tilts or down-tilts in rhythm (about 200 BPM), so every hit puts you back in the grab flinch. You "cannot get out of the grab while they are in a special form of grab hitstun" ([SmashWiki: Wobbling](https://www.ssbwiki.com/Wobbling)). The DOL agrees: CaptureDamage never runs the capture tick above, so the timer stops and mashing does nothing. **Blizzobbling** does the same with Blizzard.
7. **Popo's solo down throw chain grab.** Even without Nana, Popo can chain down throw Sheik, Ganondorf and Roy until about 50% ([SmashWiki: Chain grab](https://www.ssbwiki.com/Chaingrab)).
8. **The freeze glitch.** Banned, and fixed in PAL. It freezes the opponent in place ([SmashWiki: Ice Climbers](https://www.ssbwiki.com/Ice_Climbers_(SSBM))).

### Getting out, as the victim

From a Fox-against-Ice-Climbers guide ([Dignitas](https://dignitas.gg/articles/blogs/smash/13619/fox-vs-ice-climbers-common-mistakes-to-avoid-as-fox)):

- **Down throw, down air:** hold down on the C-stick, and quarter-circle SDI down and away on the control stick.
- **Down throw, up-smash:** DI away, and be ready to tech the landing, or you'll most likely be jab reset.
- **Blizzard:** SDI up and away in the air, down and away on the ground.
- In general, DI away from the follow-up they're setting up, and tech on landing. Mashing only helps before the throw: a handoff throws you before you can mash out.

### What adding a handoff would take

The event already runs both climbers with no AI, so a handoff means scripting them frame by frame:

1. **The throw.** Popo throws (the stick down in CatchWait for a down throw) on a chosen frame of the hold, before a fast masher gets out (at 0%, the fastest possible mash takes 6 frames).
2. **Your path.** Where and when you land depends on your weight (throw speed), your percent, your DI and the throw's knockback. Passing Practice already predicts a launched fighter's flight (`PredictEnemy` in [src/passing.c](../src/passing.c): gravity, knockback decay, hitlag), and that would carry over.
3. **Nana's regrab.** Nana has to stand near where you'll land and press Z so her grab, out on frames 6-7, meets your first frames where you can be grabbed. That's a window of a few frames, and it moves with every character and DI. She'd need to re-plan every frame, the way Passing Practice's teammate does.
4. **The loop and the lesson.** Nana throws, Popo regrabs, and so on. The anti-handoff version from the to-do list would show which way to DI each throw.

**Confidence.** Moderate for one down throw regrab, low for full chains, until it's tried in Dolphin. The pieces exist in this codebase, but the windows are a few frames wide and depend on the character, the percent and your DI. None of that can be tuned offline. Down Throw Regrab is the one to build first: Popo down throws at a set hold frame and Nana regrabs where you land. Test it in Dolphin on a few characters (light, heavy, fast faller) before adding Nana's throws, the down air, smash attacks or the ledge chain.

## Sources

- [SmashWiki: Ice Climbers (SSBM)](https://www.ssbwiki.com/Ice_Climbers_(SSBM)): hand-offs, wobbling, the freeze glitch, desyncs, Blizzard.
- [SmashWiki: Ice Climbers (SSBM)/Grab](https://www.ssbwiki.com/Ice_Climbers_(SSBM)/Grab): the grab's frames and hitboxes, and the handoff, cookhold and down throw down air regrab after the ban.
- [SmashWiki: Ice Climbers (SSBM)/Down throw](https://www.ssbwiki.com/Ice_Climbers_(SSBM)/Down_throw): 6%, base 70, growth 40 (matches the ISO).
- [SmashWiki: Wobbling](https://www.ssbwiki.com/Wobbling): the rhythm, the moves, the grab flinch, handoffs near the edge.
- [SmashWiki: Chain grab](https://www.ssbwiki.com/Chaingrab): handoffs, Nana's down air during the down throw, Popo's solo chain grab.
- [SmashWiki: Desyncing](https://www.ssbwiki.com/Desyncing): Melee's partner acts during grabs and throws.
- [SmashWiki: SDI](https://www.ssbwiki.com/SDI): 6 units per input in Melee, from the 2nd hitlag frame, quarter-circle rules, ASDI of 3.
- [Dignitas: Fox vs Ice Climbers](https://dignitas.gg/articles/blogs/smash/13619/fox-vs-ice-climbers-common-mistakes-to-avoid-as-fox): how to SDI and DI out of the common follow-ups.
- Smashboards threads on [handoff timing](https://smashboards.com/threads/hand-off-timing-w-pictures-frames.394226/), [grabs and desyncs](https://smashboards.com/threads/ice-climbers-grabs-and-desynces.344412/page-3) and the [IC Q&A](https://smashboards.com/threads/general-ics-q-a-thread.199633/page-106). Smashboards blocks automated reading, so only search summaries of these were seen.
