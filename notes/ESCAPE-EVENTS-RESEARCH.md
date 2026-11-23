# Escape events research (T10, T11)

The numbers behind **Escape Samus Up-B** ([src/escapesamus.c](../src/escapesamus.c)) and **Escape IC Grab (Beta)** ([src/escapeic.c](../src/escapeic.c)), and the Ice Climbers' handoffs: the down throw handoff the IC event does since T11, and the other IC regrabs, with how to escape each.

Move data comes from the vanilla NTSC 1.02 ISO's `PlSs.dat` and `PlPp.dat` subaction scripts. Common constants come from `PlCo.dat`'s `ftLoadCommonData`. Native behavior was read from the disassembled `Start.dol` (devkitPPC's `powerpc-eabi-objdump` on the DOL's text sections) and, for T11, from the Melee decomp ([doldecomp/melee](https://github.com/doldecomp/melee)). Frame numbers in the move tables are the scripts' own timer counts. The events never depend on how those map to "frame 1" conventions: they identify hits by their properties, not by frame.

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
- **The C-stick can't SDI in Melee.** `8008E4F0` reads only the control stick (`fp + 0x620`, `0x624`) and its tilt timers, so the event's SDI counter rightly ignores the C-stick. Being able to SDI with both sticks is a Brawl thing.
- **ASDI is `8008E714`,** installed as `cb.ExitHitlag`. When hitlag ends it moves you PlCo `0x4BC` (3.0) along the stick, or along the C-stick when that's past its threshold (`800DF608`), which wins over the control stick. It then applies trajectory DI (`8008E5A4`, at most PlCo `0x1A8`, 18 degrees).
- **The TM SDI OSD** hooks `8008E52C` inside the check, and counts only while the SDI OSD is on. Its counters share a union with the Wavedash OSD's data.

### How the event counts

- **SDI inputs.** Each frame, before the physics proc, and again after the frame's damage, the event replaces your `cb.EveryHitlag` with a wrapper whenever it's the native `8008E4F0`. Each hit's damage entry puts the native one back. The wrapper calls the native function and counts the call if your position changed, because the native check only moves you when it takes an SDI. That's every input the game took, OSD or not.
- **The last hit.** An s_link 15 proc runs after hit detection and damage. A hit on you that frame shows up as a rise in `dmg.percent + dmg.percent_temp`, and Samus's hitboxes are still the ones that hit. The launcher is her only Screw Attack hitbox with set knockback (`wdsk` 80), so a hit while one is active is the last hit.
- **The result.** The attempt resolves once Samus's animation is past frame 33 (every hitbox is gone by 30) or she leaves the up-B. Launched if the last hit landed, Escaped if any other hit did, Samus Missed if none did. A miss isn't counted.
- **Holding you still.** Until her first hit you can't act. In the air you're held in Fall the way Passing Practice floats its enemy. On the ground, Wait's interrupt callback is cleared every frame. Your stick still feeds the tilt timers, so SDIing the first hit works as it does in a match. Locking your controller instead would have handed you a free SDI on the first hit if you were holding a direction.

### Goal: Punish

- **Her helpless fall.** Samus's up-B ends through `Fighter_EnterSpecialFall` (`80096900`), from the grounded version at `8012A8A0` and the aerial one at `8012A948`. Both pass `can_fastfall` 1 and take the drift multiplier and landing lag from her special attributes `0x48` (0.66) and `0x50` (24). That's 24 frames of LandingFallSpecial before she can act. The code is hers: the aerial up-B's entry at `8012A7C8` sets her rise speed from attribute `0x44`, the 2.5 above.
- **Fast falling.** FallSpecial's physics (`80096B44`) starts with the common fast fall check (`8007D528`). It needs her falling (`self_vel.Y` < 0), the stick at least PlCo `0x88` (0.6625) down, and the y tilt timer under PlCo `0x8C` (4), so a fresh flick. With `can_fastfall` set she then falls at her fast fall speed. The event flicks her CPU stick down on each falling frame after one with the stick inside PlCo `0x8`, so she fast falls on her first falling frame.
- **The window.** It's open from her up-B until she can act: Wait, a Fall, Landing, DamageFall or a DownWait, out of hitlag and hitstun. After her up-B that's Wait, on the frame her landing lag ends. Her animation runs before the event's think and hits come after it, so she's invincible from her first free frame. Damage to her or a grab (a Capture state other than CaptureCut and CaptureJump, or a Thrown state) while it's open is a punish. After a punish, the window stays open, so your combo plays out, until she can act again.
- **Invincible.** `hurt.kind_game` 1, as respawning sets it, and `Fighter_SetAllHurtboxState(1)`. The game only rewrites `kind_game` when a ledge or respawn timer runs out (`8006A420` on), so it lasts until the reset puts it back to 0. Hit detection skips a fighter whose `kind_script` or `kind_game` is set (`800770A4`). She flashes with Lab's intangibility color animation (10).
- **Counting.** Every counted attempt is an escape attempt. A punish attempt counts when it's decided: launched, punished or too slow. So a D-pad restart in the window leaves it out, and a miss is never counted.

### To check in Dolphin

- Punish: Samus fast falls as soon as she starts falling, lands with 24 frames of lag, and turns invincible (flashing) the frame she can act. Hits in her landing lag count; hits after don't.
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

- **The capture tick.** CaptureWait's animation callback (`800DB908`, in the state table at `803C440C`; CaptureWaitLw's `800DBD10` calls it) takes PlCo `0x3A4` (1) off the timer each frame and adds 1 to its frame count (`state_var1`, zeroed when the grab connects at `800DA9E8`). Then it calls `Fighter_CheckGrabBreakout` (`8007DC08`) with PlCo `0x3A8` (6). It lets go when the timer reaches 0 or less, through `800DA698`. CaptureDamage's callbacks (`800DC2E4`, `800DC468`) run the same tick through `800DB8A4` but never let go, and go back to CaptureWait at their animation's end (20 frames). Both store whether you mashed that frame (`fp + 0x2348`). (Before T10's Actionable Flash these notes had CapturePulled running `800DB8A4`. It doesn't: the state table entries for 223 and 226 have empty callbacks.)
- **The pull doesn't count.** CapturePulledHi and Lw (223, 226) have empty animation and interrupt callbacks (`800DAC70`, `800DB004`), so nothing ticks and no mash counts until you're in CaptureWait. The grabber's CatchPull (213) keeps playing Catch's animation and script from the connect frame (`800D9E00`, with the script-catch-up flag `0x4000`) and ends at the animation's end, or as soon as the script sets `fp + 0x2210` bit `0x10` (`800D9E80`). Its CatchWait entry moves you to CaptureWait (`800DA294` → `800DB6C8`). The `0x50` command with value 0 sets that bit (`800718A4`, opcode 20 of the table at `803C06E8`). Popo's Catch script puts his grab boxes out on frame 6 and runs `40000000 50000000` on frame 8, so the pull lasts a frame or two. Your stick memory, zeroed at the connect, is still zero on CaptureWait's first frame, so a stick already held past halfway counts then. A button pressed during the pull and still held doesn't.
- **What counts.** `8007DC08` takes 6 for any fresh press in `input.down & 0x80000F00`: A, B, X, Y, and bit 31, the shield bit. The fighter input build (`8006B070`..`8006B0D8`) sets bit 31 for digital L or R, for an analog press past the trigger deadzone, and for Z, which also sets A. So L, R and Z count, and L and R count as one button. It takes 6 more when the control stick's direction changes: each axis keeps a sticky -1/0/+1, set past PlCo `0x308` either way, and a change on either axis counts. That's at most one button and one stick input a frame, so 13 off a frame at the most.
- **What doesn't count.** `8007DC08` reads only `input.down` and the control stick (`fp + 0x620`, `0x624`). The input build fills those from the pad's main stick (pad `+0x20`, `+0x24`) and never mixes in the C-stick (`+0x28`, `+0x2C` go to `fp + 0x638`, `0x63C`). So the C-stick, D-pad and Start do nothing for a grab escape in Melee. Some players believe otherwise, but SmashWiki says it's Brawl where the C-stick counts double.
- **Pummels.** A pummel's hitlag freezes you: `Fighter_UpdateAnim` skips the whole animation update, callback included, while `fp + 0x2219` bit `0x04` is set (`8006AA64`). That's set entering hitlag (`8006D044`) and cleared by `Fighter_ExitHitlag` (`8006D10C`). So the timer stops and mashing does nothing during the hit's hitlag. The rest of CaptureDamage's flinch ticks and counts. But only CaptureWait lets go, so a timer that runs out in the flinch lets go once you're back in CaptureWait. That's the mechanism behind wobbling (below).

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

Because the tick is an animation callback (s_link 1), the event measures in an s_link 15 proc. On the frame the grab connects it sees the full timer the grab set. It counts from the first frame the timer drops, after the pull, and from then on it sees the timer after each frame's tick: the drop is 1 + 6 x that frame's inputs, rounded and capped at 2 inputs. The frame you get out is counted too, so not mashing at 0% reads exactly 75. (Before T10's Actionable Flash it counted the pull's frame or two as well.) Popo presses Z on the setup's second frame, as Grab Punish's teammate does. A missed grab is tried again from the next of four gaps (8, 5, 11, 14). You can't act until the grab connects (Wait's interrupt callback is cleared), so mashing early can't dodge it. Both climbers run with no AI: Nana stands still, unless a handoff has the event press her buttons (see the handoffs below). Each later hold, after a regrab, is counted the same way from its own first drop.

### Ground and air releases

When CaptureWait lets go (`800DB974`), Popo goes to CatchCut through `800DA698`, and you get one of two releases:

- **Air release, CaptureJump (230, `800DC070`),** if either:
    - You pressed X or Y (`input.down & 0xC00`) while the frame count was under PlCo `0x3AC` (16). CaptureWait's interrupt callbacks (`800DBA4C`, `800DBD30`) call `800DC014`, which sets `fp + 0x234C` for it. The grab zeroes that flag when it connects. The count runs only in CaptureWait and CaptureDamage, so it's the first 16 frames after the pull.
    - The control stick is up at least PlCo `0x70` (0.6625) on the frame you get out (`800DC044`). Only that frame's stick counts, so waggling up and down can give either release.
- **Ground release, CaptureCut (229, `800DC750`),** otherwise. (Releases that aren't a mash out have callers of their own. Not checked here.)

What each does:

- **CaptureCut** pushes you back along the ground at PlCo `0x370` (1) with friction. It has no interrupt (`800DC82C` is a `blr`). At its animation's end `Fighter_EnterFallOrWait` (`8007D92C`) puts you in Wait, or Fall if you're airborne. It's 30 frames for every character but DK, whose is 20 (`PlXxAJ.dat`).
- **CaptureJump** sets you moving back at PlCo `0x374` (1) and up at `0x378` (2), with gravity and aerial drift (`800DC224`). Its animation callback counts `state_var1` up from 0 (`800DC110`). From 30 (PlCo `0x3B8`) its interrupt check (`800DC160`) runs the same aerial interrupts as Fall's (`800CCAAC`: specials, items, attacks, air dodge, jump). Its animation is 50 frames for everyone, then Fall (`800CC730`). The interrupt check runs in the input proc (`8006B7F4`), after the animation proc that counted the frame, so you can act on the 30th frame after the release.

### The Actionable Flash

The event's **Actionable Flash** (T10, default On) is the global Actionable Yellow>Green cue, on you, for the release after you mash out. `ActionCues_Remaining` now times both releases. CaptureCut is timed to its animation's end. CaptureJump is timed to whichever comes first, its 30th counted frame or its end. The event reaches that function through `event_vars->cue_remaining`. It runs `TMCue_Update` from `action_cues.c`, compiled into the event, so its yellow and green are the global cue's. It shows them through the timing overlay, as Powershield does. It's armed when you get out. It ends once the green is over, or when you leave the release for a state the cue doesn't time: a jump's squat, an air dodge or a special. It's saved in the core record's last free bit, byte 37 bit 7, inverted so 0 is On.

### To check in Dolphin

- Popo's grab connects from 8 for a range of characters (the retries cover the rest).
- Not mashing gives exactly the No-Mash Hold (75 at 0%), and the count matches a slow, countable mash.
- Popo's pull lasts a frame or two: a mash on the grab's first frame doesn't count, and a stick held through the pull counts once.
- An X or Y press in the first 16 frames, or holding up as you get out, gives an air release. Neither gives a ground release. Both still end the attempt.
- The Actionable Flash: yellow on the last two frames of either release, green on your first two actionable frames (Wait after a ground release, frame 30 of an air release), and nothing during the grab or after the green.
- With Handoff on None, Nana doesn't follow Popo or act while he holds you.

<a id="ice-climbers-handoffs-research-for-a-later-version"></a>

## Ice Climbers handoffs (T11)

### What a handoff is

A **handoff** is the Ice Climbers' chain grab: one climber throws, and the other, acting separately (desynced), regrabs you before you can act. Handoffs became the Ice Climbers' main grab game after most tournaments banned wobbling (from 2019-2020). In Melee the other climber can act during grabs and throws, so the desync comes free with the grab ([SmashWiki: Desyncing](https://www.ssbwiki.com/Desyncing)). The basic handoff is Popo's down throw into Nana's regrab. "Re-grabbing is capable from both directions": when Nana holds you and down throws, Popo regrabs the same way.

### Popo's grab and throws, from `PlPp.dat`

| Move | Damage | Angle | KB growth | Base KB | Script frames (release) | Length | Speed |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Grab (Catch) | 0% | - | - | - | grab boxes on 6-7 | 30 | fixed |
| Dash grab (CatchDash) | 0% | - | - | - | grab boxes on 6-7 | 40 | fixed |
| Pummel (CatchAttack) | 3% | 80 | 100 | set KB 30 | hit on 12 | 25 | fixed |
| Forward throw | 5% | 45 | 60 | 70 | releases on 28; a 6% kick on 24 | 50 | fixed |
| Back throw | 7% | 60 | 30 | 80 | turns on 8, releases on 17 | 40 | your weight |
| Up throw | 5% | 88 | 28 | 110 | releases on 31; 6% kicks on 25 | 60 | fixed |
| Down throw | 6% | 80 | 40 | 70 | releases on 36 | 50 | your weight |
| Down air | 9% | 361 | 110 | set KB 36 | one hitbox from 3 to 53 | 66 | fixed |

The grab boxes are a 4.49 sphere on the hand (bone 26) and a 3.13 one on bone 22, and they catch grounded and airborne fighters alike (the hitbox command's ground and air bits). Posing the skeleton on Catch's script frames 6 and 7 puts the hand about 6.7 forward and 7.8 up from Popo's feet, then 6.1 and 5.8 (CatchDash: 7.9 and 7.1, then 7.5 and 6.0). Hit and hurt radii scale with the player's size (`x34_scale`, 1 normally), not the model's 1.15. Every throw's release command is the `0x50` command with value 0, which sets the flag `ftCo_800DD724` releases you on (the same one that ends CatchPull). Nana's archive (`PlNn.dat`) has only her specials: she runs Popo's common moves.

**Throw speed.** `ftCo_800DD4B0` plays a throw at `1 / (your weight x PlCo 0x37C)`, and `0x37C` is 0.01, unless the thrower's weight-independent mask (`ftData` attributes `+0x180`) has that throw's bit. Popo's mask is `0x05`, forward and up throw, so his down and back throws run at 100 / your weight frames a frame. In the game's float steps, a down throw releases you `ceil(36 x weight / 100)` frames after it starts: Pichu 20, Jigglypuff 22, Fox 27, Falco and Pikachu 29, Marth and the Ice Climbers 32, Sheik 33, Mario 36, Falcon 38, Ganondorf 40, Bowser 43. That's the release column of Nicki's data sheet for every character it lists.

**Throw knockback.** The release (`ftCo_800DDDE4`) works out knockback with PlCo `0x10C` as your weight, and `0x10C` is 100: every character takes the same knockback from a throw. The down throw at 0% is 78.5 (from 3% on, 80 or more), the back throw's is 86.7. A down throw's release (`ftCo_800DE7C0` with `motion_id == 222`) also forces knockback level 3 and DamageFlyTop, a tumble state, whatever the knockback.

**DI, and no SDI.** The release puts you at the thrower's TransN2 bone, sets your knockback, and applies trajectory DI on the spot (`ftCo_8008E5A4`, right after `ftCo_8008DCE0`). A throw has no hitlag, so there's no SDI or ASDI on it: only the DI angle, up to 18 degrees, from your stick as of the release frame.

**Grabbable again at once.** Being held or thrown sets your grab immunity mask (`x1A6A`) to `0x1FF`, and the release's state change clears it (`Fighter_ChangeMotionState`, `fighter.c`). The grab check (`ftColl_80078A2C`) needs that mask clear of the grabber's bit, the grab box's ground or air bit for your state, no intangibility, and a grabbable hurtbox in the sphere. So the other climber can catch you the frame you're released.

### The down throw handoff, frame by frame

Nicki's data sheet ("Ice Climbers Information and Data Sheet", section 1.2) lists, for each character, the release frame and "on which of the following frames they can be regrabbed by the other climber (when the other climber's grab hitbox needs to be active)", for DI in and DI away, at about 50%. Every window starts on the release frame, for both DIs:

| Character | Release | Window DI in / away |
| --- | --- | --- |
| Jigglypuff | 22 | 3 / 3 |
| Kirby | 26 | 4 / 4 |
| Fox | 27 | 4 / 3 |
| Falco | 29 | 3 / 2 |
| Pikachu | 29 | 5 / 4 |
| Mewtwo | 31 | 5 / 6 |
| Marth | 32 | 2 / 3 |
| Sheik | 33 | 5 / 4 |
| Mario | 36 | 3 / 3 |
| Captain Falcon | 38 | 3 / 4 |
| Ganondorf | 40 | 3 / 4 |
| Bowser | 43 | 6 / 7 |

Nicki's notes: "handoff windows often become 1 frame smaller as an opponent gains more percent", and are larger at 0%. Against characters whose window is smaller DI'd away, like Falco, Nana can walk forward a little during the throw. Against Marth or Luigi she has to be right on top of Popo, or DI in can't be regrabbed. Pichu "only works when walking forward with Nana".

So a climber standing on top of the thrower, with its grab out on the release frame, catches every DI at low and mid percents. DI only beats a late regrab, by a frame for the lighter characters DI'd away (Fox: 3 frames instead of 4).

### Nana's grab, from the decomp

When Nana holds someone, her AI's grab mode (`x18 = 9`, from `ftCo_IsGrabbing` in `ftCo_0A01.c`) runs the CPU grab routine `ftCo_800B683C` (`ftcpuattack.c`). Nothing in it is Nana's own:

1. It pummels if `HSD_Randf() x (1 + pummels so far) < 0.5`: half the time first, then a quarter, then a sixth.
2. Otherwise, if she's within 30 of either edge of the floor under her (`mpIsland_8005AC14` and its extents), she pushes the stick toward that edge: forward if she faces it, back if not. Off the ledge this is what makes her throw predictable.
3. Otherwise she throws up, down, forward or back at 1 in 4 each (`HSD_Randf()` against 0.25, 0.5 and 0.75).

The throw input is the stick at neutral for a frame, then pushed (`CpuCmd_WaitFor 1`), so her throw starts a frame after the decision. Her AI level is Popo's percent / 20 (`ftCo_800B101C`), which this routine doesn't use. HDR's frame data page lists 50% up, 20% down, 15% forward and 15% back for Nana's random throw. The decomp's routine has no such weights, so the event uses 1 in 4.

### What the event does (Handoff: Down Throw)

- Nana stands on top of Popo, facing you.
- Popo down throws on his first CatchWait frame (stick down past PlCo `0xB0`, -0.25, from neutral: CatchWait's IASA, `ftCo_800DD1E4`), before a mash can get you out.
- Each frame of the throw, the event steps Popo's animation frame by his rate, in floats as the game does, to the frame the script releases you (36). With 6 frames to go, Nana presses Z (A and the shield bit, through Wait's own IASA), so her grab boxes come out on your release frame and the one after.
- Nana then holds you the way her AI would (above), with the event pressing her buttons and no AI running.
- If she down throws, Popo, now on top of her, regrabs it the same way, and down throws again. Any other throw ends the chain, and so does mashing out, or a regrab that misses.
- Each new hold is normalized as if tied with handicap off, as Popo's first grab is.

### The techniques

1. **Down throw handoff.** Above. One handoff is always possible, while a second is "possible but iffy" ([Smashboards Q&A](https://smashboards.com/threads/general-ics-q-a-thread.199633/page-106)): the second depends on Nana's random throw.
2. **Ledge handoffs and the cookhold.** Near the edge Nana always throws toward it, so the chain becomes predictable. SmashWiki calls handoffs "only a true infinite when done near the edge of the stage (due to the CPU AI's throw behavior)" ([SmashWiki: Wobbling](https://www.ssbwiki.com/Wobbling)). The cookhold is a harder variation, pioneered by CookBook ([SmashWiki: CookBook](https://www.ssbwiki.com/Smasher:CookBook)), with a [video](https://www.youtube.com/watch?v=2c23FZJK6Hs&t=128s) linked from the grab page. No step-by-step description was found. Nicki's sheet lists back throw handoffs that "work when synced" against Marth, the Ice Climbers, Sheik, Ness, Falcon, Peach, Zelda, the Marios, Pikachu, Samus, Link, Young Link, Ganondorf, DK, Bowser and Mewtwo, with an unexplained "L" on some.
3. **Down throw, Nana down air, regrab.** Nana's down air during Popo's down throw holds you down and forces a stand-up that can be regrabbed. It's escapable with SDI ([SmashWiki: Ice Climbers' grab](https://www.ssbwiki.com/Ice_Climbers_(SSBM)/Grab)). The down air is one long hitbox (frames 3 to 53) with set knockback, so its hit gives 6 frames of hitlag to SDI in.
4. **Down throw, Nana forward air, Popo regrab.** Grab and down throw with Popo, short hop forward air (a meteor) with Nana, then regrab with Popo. A Smashboards tech thread says it's "significantly harder to smash DI than d-throw dair" (seen only in a search summary).
5. **Back throw regrab ("pitching").** Popo back throws and Nana wavedashes back to regrab, on fast fallers, to about 120% (Smashboards tech thread, the author's own testing).
6. **Down throw into Nana's smash attacks.** At kill percents, Nana's up smash, forward smash or down smash as you leave the throw. Nicki times the smash "so that it hits them right as they leave your grab".
7. **Wobbling (banned).** Popo pummels while Nana forward-tilts or down-tilts in rhythm (about 200 BPM), so every hit puts you back in the grab flinch. The DOL agrees: each hit's hitlag freezes the timer, CaptureDamage ticks and counts your mash, but only CaptureWait lets go, and every hit starts the 20-frame flinch again (see Mashing above). **Blizzobbling** does the same with Blizzard.
8. **Popo's solo down throw chain grab.** Even without Nana, Popo can chain down throw Sheik, Ganondorf and Roy until about 50% ([SmashWiki: Chain grab](https://www.ssbwiki.com/Chaingrab)). Nicki's sheet has percent bands for each DI (No DI, DI In, DI Away) with Popo's answer to each (standing, walk or dash jump-canceled grab, walk turn, dash, wavedash), and "Jump Out" bands.
9. **The freeze glitch.** Banned, and fixed in PAL ([SmashWiki: Ice Climbers](https://www.ssbwiki.com/Ice_Climbers_(SSBM))).

### Getting out, as the victim

These are the hints in the event's Help, with where each comes from.

| Technique | How to get out | Source |
| --- | --- | --- |
| Down throw handoff | DI doesn't stop a well-timed regrab at low and mid percents. It only beats a late one, by a frame for lighter characters DI'd away. Mash in Nana's grab: she pummels before throwing half the time, and a pummel's flinch gives a fast masher time to get out at low percent. Away from the ledge 3 of her 4 throws end the chain. | Nicki's windows; the decomp's grab routine; the DOL's mash rules |
| At the ledge | Her throw is always toward the ledge there. Mash in her grab, and DI her throw up and in to make it back. | The decomp; general DI |
| Down air regrab | SDI down and away with quarter circles on the control stick while holding down on the C-stick. | [Dignitas](https://dignitas.gg/articles/blogs/smash/13619/fox-vs-ice-climbers-common-mistakes-to-avoid-as-fox), SmashWiki |
| Forward air regrab | SDI it like the down air, though players say its timing makes it much harder to SDI. | Smashboards (search summary) |
| Back throw regrab | You come down in tumble: DI changes where you land, so tech or act as soon as you can. Which DI escapes isn't documented for Melee. A fandom wiki that may be about Project+ says to DI it up. | Physics above; unverified |
| Popo alone | Only a few characters, at low percents, and he has to react to your DI. At some percents you can jump out. | SmashWiki, Nicki's sheet |
| Wobbling | It can't be escaped once it's going. Mash before it starts. | SmashWiki, the DOL |
| Down throw, up smash | DI away, and be ready to tech the landing, or you'll most likely be jab reset. | Dignitas |
| Blizzard | SDI up and away in the air, down and away on the ground. | Dignitas |

### What the other handoffs would take

The event has only the down throw handoff because it's the one whose timing is fully known: the release frame from your weight, Nana on top of Popo, her grab on the release frame. The others need things no source documents:

1. **Back and forward throw regrabs** need Nana to move to where you'll come down and grab you there. That means predicting your flight (Passing Practice's `PredictEnemy` does that), the grab geometry above, and a wavedash or dash for her. How a real Nana moves for "pitching" isn't written down, and the escape advice depends on it.
2. **Down and forward air regrabs** need Nana's short hop aerial timed to the release (like Passing Practice's aerials), and Popo's regrab after his throw ends. What the down air does to you next ("forces a stand-up") needs checking in Dolphin.
3. **Popo alone** needs Popo to react to your DI after his own throw ends: walking, dashing, turning and jump-canceled grabs, per Nicki's percent bands.
4. **Ledge handoffs** need Popo to regrab Nana's back throw toward the ledge, which is where Nicki's back throw handoff list comes in.

### To check in Dolphin

- On Down Throw, Nana's grab catches you on the frame Popo's down throw lets you go, for a light, a middle and a heavy character (Pichu, Fox, Mario, Bowser), with no DI, DI in and DI away. The event's timing matches Nicki's release frames exactly, so a miss would mean the grab box doesn't reach where you're released, not a wrong frame.
- Pichu: Nicki's sheet says the handoff only works if Nana walks forward. Nana standing on top of Popo may miss Pichu, which would be correct.
- At high percents, whether the regrab still connects, and from what percent it stops (Nicki: the window shrinks as you take damage).
- Nana's pummels and throws: about half her first decisions are pummels, and her throws spread evenly over up, down, forward and back.
- How quickly Nana acts after her grab: the event decides on her first CatchWait frame. The real AI may take a frame or two longer.
- Mashing out of Nana's grab during a pummel at 0% to 20%.
- Popo regrabs Nana's down throw, and the chain goes on until a throw other than down.
- Nana's AI level is Popo's percent / 20, and the event leaves Popo at 0%. Nothing in the grab routine reads the level.

## Sources

- [SmashWiki: Ice Climbers (SSBM)](https://www.ssbwiki.com/Ice_Climbers_(SSBM)): hand-offs, wobbling, the freeze glitch, desyncs, Blizzard.
- [SmashWiki: Ice Climbers (SSBM)/Grab](https://www.ssbwiki.com/Ice_Climbers_(SSBM)/Grab): the grab's frames and hitboxes, and the handoff, cookhold and down throw down air regrab after the ban.
- [SmashWiki: Ice Climbers (SSBM)/Down throw](https://www.ssbwiki.com/Ice_Climbers_(SSBM)/Down_throw): 6%, base 70, growth 40 (matches the ISO).
- [SmashWiki: Wobbling](https://www.ssbwiki.com/Wobbling): the rhythm, the moves, the grab flinch, handoffs near the edge.
- [SmashWiki: Chain grab](https://www.ssbwiki.com/Chaingrab): handoffs, Nana's down air during the down throw, Popo's solo chain grab.
- [SmashWiki: Desyncing](https://www.ssbwiki.com/Desyncing): Melee's partner acts during grabs and throws.
- [SmashWiki: SDI](https://www.ssbwiki.com/SDI): 6 units per input in Melee, from the 2nd hitlag frame, quarter-circle rules, ASDI of 3.
- [SmashWiki: C-Stick](https://www.ssbwiki.com/C-Stick): two-stick SDI and the C-stick counting double in grab escapes are both Brawl.
- [Smashboards: mechanics of mashing out of grab](https://smashboards.com/threads/mechanics-of-mashing-out-of-grab.441693/): A, B, X, Y, L, R and Z count, with L and R as one button; C-stick, D-pad and Start don't (matches `8007DC08`).
- [Smashboards: C-stick ASDI](https://smashboards.com/threads/proper-way-to-sdi-out-of-uair.398521/post-18959077): the C-stick only does ASDI, and its ASDI replaces the control stick's.
- [Dignitas: Fox vs Ice Climbers](https://dignitas.gg/articles/blogs/smash/13619/fox-vs-ice-climbers-common-mistakes-to-avoid-as-fox): how to SDI and DI out of the common follow-ups.
- Smashboards threads on [handoff timing](https://smashboards.com/threads/hand-off-timing-w-pictures-frames.394226/), [grabs and desyncs](https://smashboards.com/threads/ice-climbers-grabs-and-desynces.344412/page-3) and the [IC Q&A](https://smashboards.com/threads/general-ics-q-a-thread.199633/page-106). Smashboards blocks automated reading, so only search summaries of these were seen.
- T11: Nicki's [Ice Climbers Information and Data Sheet](https://docs.google.com/document/d/1e2WfuNH1I86On59O53FubxYzA-zYMZKZVIrCPNVEqhk/edit) (linked from SmashWiki's grab page, majority of its information compiled by Nopia): the down throw handoff's release frames and regrab windows for DI in and away, back throw handoffs, Popo's chain grab by percent and DI, in-grab combos and frame data.
- T11: the Melee decomp, [doldecomp/melee](https://github.com/doldecomp/melee): `ftcpuattack.c` (`ftCo_800B683C`, the CPU grab routine), `ftCo_0A01.c` (Nana's AI, its grab mode), `ftCo_Throw.c` and `ftCo_Thrown.c` (throw speed, release, knockback, DI), `ftcoll.c` (`ftColl_80078A2C`, the grab check), `ftaction.c` and `lb/types.h` (the subaction commands' bitfields), `ft/types.h` (the attributes and the weight-independent throw mask).
- T11: [SmashWiki: back throw](https://www.ssbwiki.com/Ice_Climbers_(SSBM)/Back_throw), [forward throw](https://www.ssbwiki.com/Ice_Climbers_(SSBM)/Forward_throw) and [up throw](https://www.ssbwiki.com/Ice_Climbers_(SSBM)/Up_throw): their frame data, and which throws depend on your weight (matches the ISO). [SmashWiki: CookBook](https://www.ssbwiki.com/Smasher:CookBook): he pioneered the cookhold.
- T11: [Smashboards: New Ice Climbers Tech (20IC)](https://smashboards.com/threads/new-ice-climbers-tech-20ic.483823/): the down throw, forward air, regrab, "pitching" (back throw into Nana's wavedash regrab, fast fallers to about 120%), and Nana's throws being random but at the ledge. Seen only in search summaries.
- T11: the [HDR frame data page](https://hdr-development.github.io/hdr-website/characters/melee/popo.html) gives Nana's random throw as 50% up, 20% down, 15% forward, 15% back. The decomp's grab routine has 1 in 4 each, which the event uses.
- T11: the [Ice Climber Plus wiki's Hand-offs page](https://ice-climber-plus.fandom.com/wiki/Hand-offs) couldn't be read (paywalled). Search summaries of it give "DI as far up as possible" against a back throw regrab, but the wiki may be about Project+, so the event's Help doesn't use it.
