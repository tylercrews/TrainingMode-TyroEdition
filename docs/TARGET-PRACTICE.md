# Target Practice

Select **Minigames → Target Practice** (the eighth event). Only Fox and Falco can play it. It opens the stage select, but only the six legal stages play: pick any other and the event restarts on Battlefield on its first frame, the way Up-B Sweetspot does. Targets come from the live collision lines. Hazards are off as in other events. There's no CPU.

A target is a small red and white bullseye, centered on its spot, with four ticks around it pointing to it so it's easy to find with the camera zoomed out (**Target Pointers**, On by default). It's drawn over everything (no depth test), so a platform never hides it. Shoot it and it breaks with a ring and a chime, the laser stops there, and the next target appears. The camera keeps the targets in view.

A timed minute counts your targets and saves your best, like Eggs-ercise. The first target appears at Go. **Free Practice** has no clock.

## Menu

- **Retry:** restart the event. Z while paused does the same.
- **Change Stage:** restart on another legal stage without going back through the character and stage select. Saved settings carry over.
- **Change Character:** a submenu with one row, **Player Character**: Left/Right picks Fox or Falco, X/Y the costume, and A restarts as them, skipping the character and stage select. It's `src/fighter_swap.h`, as in Grabs-ercise, limited to the two (`FighterSwap_Restrict`); there's no CPU row, since there's no CPU. Saved settings carry over.
- **Free Practice:** Off (default) is the timed minute; On has no clock. Saved. Changing it restarts the event.
- **SH Double Lasers:** Off (default) or On. Saved. On makes about half the targets come as [a pair](#sh-double-laser-pairs) off the main floor, a side platform or the top platform. Free Practice only, and Fox only: for Falco the row reads **SH Double Lasers (N/A)** and stays greyed out.
- **FH Multi Lasers:** Off (default), **All Combos**, or one of four [full hop combos](#fh-multi-laser-combos): **Hi-Hi-Low**, **High-MedHigh**, **High-Medium**, **High-Low** (the only one with a fastfall). Saved. All Combos makes about half the targets a combo, any of the four. One combo makes every target that combo, to drill it. Free Practice only, Fox and Falco both.
- **Hop Peak Lines:** Off (default) or On. Saved. [Lines](#hop-peak-lines) where the highest laser a short hop and a full hop can shoot flies, the one from the jump's peak, off the main floor and each platform. Timed runs and Free Practice alike. It shows or goes on the next frame, behind the paused menu too.
- **Short Hop Line Color** and **Full Hop Line Color:** each hop's [line color](#colors), one of the 15 OSD colors (White to Lavender, as the Global OSDs page lists them). Orange and Teal by default. Saved. The lines are that color over the side platforms, darker over the main floor and lighter over the top platform. A change shows on the next frame.
- **Double Jump Height:** Off (default) or On. Saved. While you're in the air with your double jump left, [a red line](#double-jump-height) where a laser shot at the peak of a double jump started now would fly. Once you double jump it stays at that jump's height until you land. Timed runs and Free Practice alike. It shows or goes on the next frame, behind the paused menu too.
- **Target Pointers:** On (default) or Off. Saved. The four ticks around each target, short black-edged white lines left, right, above and below it. Off draws just the bullseye. Timed runs and Free Practice alike. It shows or goes on the next frame, behind the paused menu too.
- **Counter Position:** where the count and the label under it go. It's Eggs-ercise's Egg Counter Position, shared with Grabs-ercise.
- **Reset Event Settings:** Free Practice, SH Double Lasers, FH Multi Lasers, Hop Peak Lines and Double Jump Height Off, the line colors back to Orange and Teal and Target Pointers On, then a restart in timed mode. Your best and the counter position stay.
- **Exit:** back to the event select.

## Where targets go

A laser flies flat, so a target's height is the height of the laser that hits it. Targets go:

- **Across:** anywhere over the main floor, at least 8 from its ends (`GroundEdges`, every floor line that isn't a platform).
- **Lowest:** the lowest laser you can shoot from the ground under the target's spot.
- **Highest:** a laser shot at the peak of a full hop and a double jump from the highest floor on the stage (platforms included, found from the live lines at each spawn, so Fountain's moving platforms count where they are).
- **Up and down:** evenly between those two.
- **Away from you:** at least 30 from your position and 25 from the last target, unless 35 tries in a row fail.

The heights come from the fighters' own skeletons in the ISO, by [scripts/laser_heights.py](../scripts/laser_heights.py):

- A shot starts at the `RThumbNb` joint's matrix times (0, 1.2325, 4.2636), with z zeroed (`ftFx_SpecialN_FtGetHoldJoint` in the decomp's `ftfoxspecialn.c`), on the frame the Loop script sets `cmd_vars[2]`: loop frame 5 for Fox, 8 for Falco. It fires in the animation proc, before that frame's physics, so it's from where the fighter ended the frame before.
- An airborne fighter lands once TopN plus the ECB bottom reaches the floor (`mpColl_LoadECB_JObj`). `Fighter_procMap` puts the skeleton at the new position before the collision callback, so the ECB comes from the current pose. In the aerial laser loop the legs are tucked, so the ECB bottom is well above TopN, and TopN can be that far under the floor while you can still shoot.
- The laser's first collision check (`itFoxlaser_UnkMotion1_Coll`) runs from the fighter's ECB center (`Item_InitSpawnPositionFromParent`) to the shot, so a shot that would start under the floor hits it and is gone at once.

| | Fox | Falco |
| --- | --- | --- |
| Aerial laser above TopN | 9.2829 | 3.7980 |
| Standing laser above the floor | 7.5740 | 2.7208 |
| ECB bottom, frame before the aerial shot | 4.4322 | 5.2207 |
| **Lowest laser above the floor** | **4.8507** | **0** (it hugs the floor) |
| Full hop + double jump | 31.28 + 40.204 = 71.484 | 51.5 + 41.778 = 93.278 |
| Highest target on Battlefield (top platform 54.4) | 135.2 | 151.5 |

The joints are all `JOBJ_CLASSICAL_SCALE`, so the arm bones' animated stretch (Fox's goes to 1.2) shears their children. The script poses them the way `HSD_JObjMakeMatrix` does.

### Jump physics

A jump off the ground skips gravity on its first frame, so it rises its whole speed that frame. `ftCo_Jump_Phys` (`Physics_JumpF`, `0x800CB474`) does nothing the first time it runs, when `mv.co.jump.x4` (`fighter + 0x2344`) is still false, as `ftCo_800CB110` leaves it, and calls `ft_80084DB0`, gravity first, from the second frame on. Fox's full hop peaks at 3.68 + 27.6 = 31.28, not 27.6.

B on that first airborne frame takes the skip away. The fighter's procs run anim (1), CPU (2), input (3) and physics (4): KneeBend's anim proc starts the jump, the jump's interrupt then takes the B, and the aerial laser's physics, `ft_80084DB0`, runs in place of the jump's. So a hop with B on frame 1 is the jump speed lower from its first frame on, and peaks a frame sooner. Every walk of a hop (`Jump`, given the press) does the same. B on frame 2 or later comes after the skipped frame, so its hop keeps the whole rise. A double jump never skips it (`ftCo_JumpAerial_Phys` calls `ft_80084DB0` at once).

| | Short hop | Full hop |
| --- | --- | --- |
| Fox, B on frame 1 | 8.55 | 27.6 |
| Fox, no B on frame 1 | 2.1 + 8.55 = 10.65 | 3.68 + 27.6 = 31.28 |
| Falco, B on frame 1 | 9.68 | 47.4 |
| Falco, no B on frame 1 | 1.9 + 9.68 = 11.58 | 4.1 + 47.4 = 51.5 |

### Target size

Each laser hitbox is 300 / 256 = 1.17 units across each way (the article's hitbox command times the 1/256 at `0x804DC790`). For both characters the standing laser is 2.72 above the lowest laser, so a target radius under 2.72 − 1.17 = 1.55 puts the lowest targets out of a standing laser's reach. The radius is **1.5**: a target breaks when a laser's center passes within 2.67 of its center, and a standing laser misses the lowest targets by 0.05.

## Hits

Each frame, every Fox or Falco laser item you own (`ITEM_FOXLASER`, `ITEM_FALCOLASER`, owner at `0x518`) is checked hitbox by hitbox. A hitbox sweeps from last frame's position to this frame's (`it_80272674`: state 1 is a new hitbox not placed yet, and 2 and on hold both ends), so a laser moving 5 to 7 units a frame can't skip a target. A swept capsule that comes within the target's radius plus the hitbox's live size breaks the target. The laser then ends the way it does on a wall: `Item_RemoveAllHitboxes` and a life of one frame (`Item_SetLifeTimer`). A laser breaks one target at most. Nothing is checked while paused. The think runs at priority 15, after the fighters' and items' updates.

## SH double laser pairs

The aerial laser fires 9 frames after the B press (Start's 5 frames and the Loop's fire frame, with the press as the first, which matches SmashWiki's aerial startup of 10), and again every Loop (Fox 10 frames, Falco 16) while B is pressed again in time (`ftFox_SpecialN_CheckLoopInput`). B can come on the first airborne frame: jumpsquat turns into the jump in the animation proc, and the jump's interrupt runs after it in the input proc.

`ShortHopPresses` walks a short hop with the live attributes and no fastfall (a fastfall only lands sooner) and counts the press frames whose second shot still comes before the landing. For Fox that's B on airborne frames 1 to 3. B on 1 keeps the hop's first frame's gravity ([Jump physics](#jump-physics)), so its hop peaks 2.1 lower: shots 17.83 and 5.48 above the surface, its peak and just before landing. B on 2 shoots 19.93 and 7.58, the higher hop's peak, and B on 3 shoots 19.73 and 5.08, the lowest second shot (its ECB is 0.23 off the floor the frame before). For Falco none fit: his short hop lands before a second shot, so there are no pairs. This matches SmashWiki, which lists the short hop double laser as Fox's.

With SH Double Lasers On in Free Practice, each spawn is a pair half the time. A pair is one of those three timings, at random. It stacks both targets at one x over the floor it's shot from, at that timing's heights above it, at least 8 from the floor's ends. The floor is one of three kinds, each as likely as the others where the stage has it:

| Stage | Main floor | Side platforms | Top platform |
| --- | --- | --- | --- |
| Final Destination | Yes | – | – |
| Battlefield, Dream Land, Yoshi's Story | Yes | Yes | Yes |
| Pokémon Stadium | Yes | Yes | – (it has none) |
| Fountain of Dreams | Yes | – (they move) | Yes |

- **Side** picks the left or right platform each try. The platforms are `FindPlatform`'s (`src/stage_points.h`, as Low Laser's **On Left/Top/Right Plat**): one-way lines at least 10 wide, the nearest center beyond x = ±10 for the sides and the highest across x = 0 for the top.
- **Fountain's** side platforms rise and fall, so the heights of a pair over one would be wrong a moment later. `PickPair` leaves them out on Fountain (`GRKINDEXT_IZUMI`). Its top platform is fixed at 42.75.
- **Keeping away.** The kind holds for the first half of the 40 tries, then each try picks again. A kind stays as likely as the others wherever you stand, and gives way only when no spot on it is 30 from you (standing on the top platform, say). The 25 from the last target still applies, so a platform too small to hold two spots 25 apart can't get two pairs in a row: in the tests, Fountain's top platform gets about a third of the pairs and the main floor two thirds.

A pair counts as two targets, and the next spawn waits until both are broken. Turning the setting off or on reaches the next spawn.

## FH multi laser combos

A full hop's lasers work the same way as a short hop's: B on airborne frame *p* shoots on *p* + 9 (Fox) or *p* + 12 (Falco), and again every Loop while B is pressed again in time. Each shot comes from where the fighter was the frame before. `FullHopCombos` walks a full hop with the live attributes, once, on the fighter's first frame: B on the first airborne frame keeps that frame's gravity, so its hop is the jump speed lower (3.68 for Fox, 4.1 for Falco) and peaks a frame sooner, and every later B rides the higher hop ([Jump physics](#jump-physics)). A shot counts only if the fighter is still in the air on the frame before it and the shot doesn't start under the floor. With no fastfall, every press's second shot comes after the apex for both fighters, so from press 2 on a later press makes a lower second shot (press 1's lower hop puts its second shot under press 2's), and the doubles run from press 1 to the last whose second shot comes before landing (Fox 17, Falco 24: second shots 5.82 and 1.79). The peak press is the one whose first shot comes from the apex, on the full hop's Hop Peak Line (`PeakPress`): Fox 8, Falco 14, second shots 30.21 and 32.50 with no fastfall.

Each combo is one B press, and High-Low adds a fastfall:

| Combo | Timing | Fox (frames: heights) | Falco (frames: heights) |
| --- | --- | --- | --- |
| **Hi-Hi-Low** | Three shots, the first two nearest the same height (either side of the apex) | B 3: 37.11, 38.26, 17.02 | B 6: 50.38, 49.34, 7.99 |
| **High-MedHigh** | No fastfall. Second shot nearest halfway from the peak press's up to the highest any double has (press 2's) | B 6: 39.87, 34.12 | B 10: 54.20, 42.28 |
| **High-Medium** | No fastfall. Second shot nearest halfway from the peak press's down to the last double's | B 13: 38.26, 17.02 | B 19: 52.85, 17.29 |
| **High-Low** | First shot from the apex, B again, and a fastfall: the lowest second shot after a peak laser | B 9, fastfall 18: 40.56, 6.56 | B 14, fastfall 27: 55.30, 2.65 |

The heights are above the floor the combo is shot from. The doubles' second lasers are each more than a target's reach (2.67) from the next one's, so each is its own timing.

High-Low's fastfall works because every aerial laser state's physics is `ft_80084DB0`, which runs `ftCommon_CheckFallFast` before gravity. The fastfall takes only once the fighter is falling (last frame's speed under 0), and from then on the fall is the fastfall speed every frame (Fox 3.4, Falco 3.5). `HighLow` tries every press and fastfall frame whose first shot comes from the apex and keeps the lowest second shot. A fastfall only lowers the frames from its own on, so it takes the presses whose first shot is the peak with no fastfall and tries each fastfall up to the second shot:

- **The press.** Fox's apex holds two frames, 16 and 17, at 31.28 (the speed on 17 is 2e-7 under 0), so B on 8 or 9 shoots from it, and 9 has a frame longer to fall. Falco's apex is one frame, 25.
- **The fastfall** is the first frame one can take, the first with the fighter falling (Fox 18, Falco 27): a frame earlier it doesn't take, and a frame later puts the second laser at 9.73 (Fox) or 5.83 (Falco), over the low target. For Fox that's the frame the first laser fires (18); for Falco it's the frame after (it fires on 26).
- **Other timings** near it break both targets too, each with its own fastfall frame: Fox B 10 and fastfall 19 (40.33, 6.33) or B 11 and 20 (39.87, 5.87); Falco B 15 and 28 (55.15, 2.33) or B 16 and 29 (54.83, 1.84).

Hi-Hi-Low's two high lasers both come out around the peak. Fox's full hop fits three shots from B on frames 1 to 7 and Falco's on 1 to 8, and `HiHiLow` takes the press whose first two shots come nearest the same height (a press within .01 of a better one loses to the earlier). That puts them either side of the apex: Fox's frames 3 and 4 tie, 1.15 apart either way round, so it's 3; Falco's is 6, 1.04 apart. With his 16-frame loop, Falco's two sit about 5.4 under his full hop line. Each high laser is within a target's reach of the other's target, so either breaks either, and a laser breaks the nearer one first. B a frame sooner (Fox's frame 2: 35.73, 39.18, 19.82) still breaks both high targets, but its third laser passes over the low one.

A combo stacks its targets at one x over a floor, the same way an SH pair does (`PickGroup`, which pairs use too): the main floor, a side platform or the top platform, each kind as likely as the others, never Fountain's side platforms, and keeping 30 from you and 25 from the last target. Hi-Hi-Low's two high targets would overlap there, so they sit side by side instead, 6 to the left and right of the low one (`HHL_SPREAD`, the `offsets` in `full_hops`): their ticks reach 5 from center, so a gap of 2 is left between them. The camera frames the targets' middle and widens by their spread.

- **All Combos:** each spawn is a single or a combo, half and half, and a combo is any of the five that fits the fighter.
- **One combo:** every spawn is that combo. There are no singles between, so you can drill it over and over.
- **With SH Double Lasers on too** (Fox): the spawns split evenly among the kinds that are on. All Combos gives a third singles, a third SH pairs and a third combos; one combo gives half SH pairs and half that combo.

A combo counts as two or three targets, and the next spawn waits until all of them are broken. Changing the row reaches the next spawn. Timed runs are always singles.

## Hop Peak Lines

Each floor gets two lines, at the highest laser a short hop and a full hop can shoot from it. `PeakPress` walks the jump from the live attributes (as SH Double Lasers' pairs are) for every B press and takes the highest first shot, the earliest press of any that tie. A shot comes from where the fighter was the frame before, so that's the shot from the jump's peak, plus the aerial laser's height over TopN (the Blaster table). A hop rises its whole speed on its first frame, but B on that frame takes that away ([Jump physics](#jump-physics)), so the line is a later B's. Falco's short hop peaks on airborne frame 12, which only B on frame 1 could shoot from, so his line is B on frame 2's shot, 0.14 under his peak:

| | Short hop: B on, from | Full hop: B on, from | Aerial laser | **Short hop line** | **Full hop line** |
| --- | --- | --- | --- | --- | --- |
| Fox | 2, 10.65 | 8, 31.28 | 9.2829 | **19.93** | **40.56** |
| Falco | 2, 11.44 | 14, 51.5 | 3.7980 | **15.24** | **55.30** |

Fox's short hop line is the height of SH Double Lasers' first shot with B on frame 2, and each full hop line is High-Low's first shot. A target on a line or just under it takes that jump's laser from that floor; one above it doesn't.

- **The floors** are found from the live lines every frame: the main floor at the middle (`FindSolidGround(0)`), then `FindPlatform`'s left, top and right platforms (as SH Double Lasers' pairs use them).
- **Across:** the main floor's and top platform's lines go from blast zone to blast zone. A side platform's go from its side's blast zone to the middle (x = 0) and stop, so the two sides read apart: Dream Land's right platform is 0.1 higher than its left, and its lines are that much over the left one's.
- **Fountain of Dreams:** the side platforms' lines ride them up and down. A platform sinking into the stage keeps its lines until it's less than 1 above the main floor; hidden, the game leaves it 0.375 up, inside the stage, so its lines go until it comes back.
- **Drawing:** 2 pixels wide, at alpha 230, with no depth test so the stage never hides them, under the targets. They start on the fighter's first frame, once its attributes are known.

### Colors

Each hop's lines have a base color, its row's: **Short Hop Line Color** (Orange, `FF9628`, by default) and **Full Hop Line Color** (Teal, `00B4A0`). The rows offer the OSD palette's 15 colors, White to Lavender, with the same RGB as the OSDs (`OSD_PaletteColor`). The shade goes with the floor, so the higher floors' lines read lighter:

| Floor | Shade | Orange | Teal |
| --- | --- | --- | --- |
| Main floor | Dark: 3/5 of each channel | `995A18` | `006C60` |
| Side platforms | The base color | `FF9628` | `00B4A0` |
| Top platform | Light: halfway to white | `FFCA93` | `7FD9CF` |

Each is premultiplied by alpha 230 as it's drawn (`PeakColor`). Final Destination has only the main floor's dark lines, and Pokémon Stadium no light ones. A color change shows on the next frame drawn.

The [Hop Peak Lines Gecko codes](../slippi-gecko-codes/hop-peak-lines/README.md) draw the same lines in any match, for Slippi, all in the default base colors.

## Double Jump Height

A double jump sets your vertical speed to the full hop's times `air_jump_v_multiplier`, whatever you were doing (`ftCo_JumpAerial_Enter_Basic` in the decomp's `ftCo_JumpAerial.c`), and then falls with gravity the way a jump does (`ft_80084DB0`, gravity before the move). So it rises the same height from anywhere: `Rise` of that speed, from the live attributes, the double jump half of the highest target's height. Unlike a jump off the ground, its first frame takes gravity (`ftCo_JumpAerial_Phys` calls `ft_80084DB0` at once), so there's no skipped frame to add.

| | Double jump speed | Gravity | Rise | Aerial laser | **The line over the feet** |
| --- | --- | --- | --- | --- | --- |
| Fox | 3.68 × 1.2 = 4.416 | 0.23 | 40.204 | 9.2829 | **49.49** |
| Falco | 4.1 × 0.94 = 3.854 | 0.17 | 41.778 | 3.7980 | **45.58** |

- **The line:** red, 12 across (`DJ_BAR`, 6 each side), at the rise plus the aerial laser's height over your feet (TopN): where a laser shot at the peak of a double jump started now flies, the highest laser your double jump can reach from here, as Hop Peak Lines mark a hop's. A target on it or just under it takes that laser. It's centered on the head's x (the `HeadN` bone's world position, from `Fighter_BoneLookup`; TopN's if there's no such bone).
- **Live:** while you're in the air with a jump left (`jumps_used` under `max_jumps`), it follows you each frame. It isn't there while the jump can't come: helpless (`FallSpecial`, F and B), air dodging (it ends helpless), on the ledge (`CliffCatch` through `CliffEscapeQuick`; a ledge jump keeps it), dead or on the revival platform, and during side-B and up-B (Fox's and Falco's states 347-359, `SpecialSStart` to `SpecialHiBound`), which end helpless or on the ground. It's there through a jump, falling, aerials, the aerial laser and the shine (you can jump out of it).
- **Held:** when you double jump, the line stays at the height it showed the frame before, which is that jump's own peak laser (the jump's first frame has already moved you by its first speed less gravity, the way `Rise` counts it). `TrackDoubleJump` sees it in the think, after the fighters move: a `JumpAerialF` or `JumpAerialB` state the frame after the jump could still come. The height holds through the rest of the jump, the fall, aerials, lasers and even helpless, while the line keeps following the head across, so your peak laser comes out along it. Landing, the ledge, or a KO ends it. If the jump comes back while it's held (a ledge grab gives it back), the line goes live again. Up-B also spends the jump (`ftFx_SpecialHi` sets `jumps_used` to the max on launch), but that's no double jump, so the line just goes.
- **Drawing:** one `GX_LINES` batch, 2 pixels wide (`PEAK_LINE_WIDTH`), red at alpha 230 premultiplied, with no depth test, over the Hop Peak Lines and under the targets. It starts on the fighter's first frame, once its attributes are known, in timed runs and Free Practice alike, and it's drawn behind the paused menu too.

## Persistence

`TM_SETTING_TARGETS` (field 53) is `TYEX` word 80's bits 10-19, with no version bump: the timed best in bits 10-17 (0 to 255, `TM_TARGETS_BEST_MAX`), Free Practice in 18 and SH Double Lasers in 19. Hop Peak Lines, appended as `TM_TARGETS_LINES`, is bit 29 the same way, and Double Jump Height, appended after FH Multi Lasers as `TM_TARGETS_DOUBLE_JUMP`, bit 30. Every migration to version 23 zeroes word 80, and every default is 0, so each is stored as itself. Only a version 23 or later record owns them, so older records read no best, timed mode and Off. Every value fills its bits, so nothing needs repairing. The best isn't a score slot (`Events_ScoreIndex` gives page 0 event 7 none), so Reset Records doesn't clear it. See the [save map](../notes/SAVE-DATA-MEMORY-MAP.md).

FH Multi Lasers, appended as `TM_TARGETS_FULL_HOP`, had no room in word 80. It's word 12's bits 6-8, which held Ledgechase's preset in versions 2-8 and haven't been read since. Those versions' migrations copied the preset out but never cleared the bits, so `TYEX` version 24 zeroes all six of word 12's retired bits (6-8 and 15-17) in every record it migrates, and only a version 24 or later record owns the row. It's stored in `TMTargetsFullHop`'s order, Off 0, All Combos 1, Hi-Hi-Low 2, High-MedHigh 3, High-Low 4 and High-Medium 5, so the cleared bits read as Off. The menu lists High-Medium before High-Low (`FullHopToSaved` and `FullHopFromSaved` in `src/targets.c`). High-Low took the 4 of Peak-Low, the no-fastfall peak press it replaced. A stored 6 was High-Lowest, the last no-fastfall press, and reads as High-Low; a stored 7 isn't a choice and reads as Off. Neither is written back until you pick a row.

The line colors, appended as `TM_TARGETS_SH_COLOR` and `TM_TARGETS_FH_COLOR`, had no room left anywhere in the record, so `TYEX` version 25 appends word 84, the last 4 free bytes of the score region, and they're its bits 0-3 and 4-7. A row's value is the palette color less one (White 0 to Lavender 14, `TM_TARGETS_COLOR_COUNT` 15), stored as its distance from its default (`(value + 15 - default) % 15`), so the zeros every migration to 25 writes read as Orange and Teal. A stored 15 isn't a color and reads as the default. Only a version 25 record owns the word: older records read Orange and Teal. Migrating version 24 claims word 84 alone and keeps FH Multi Lasers' bits (only migrations from 23 and older clear the retired bits).

Target Pointers, appended after the colors as `TM_TARGETS_POINTERS`, is word 84's bit 8, with no version bump. It's On by default, so it's stored the same way as its distance from On: 0 is On and 1 Off, and the zeros every migration to 25 writes read as On. Older records read On. Word 84's bits 9-31 are free.

## Tests

`tests/test_targets.py` runs the production event through `tests/targets_probe.c` in Unicorn:

- **Constants:** the Blaster table against `scripts/laser_heights.py` on `melee.iso`, the laser hitbox's size and the DOL's 1/256 scale, the lowest target out of a standing laser's reach, the full hop and double jump from live attributes, and which press frames fit two lasers (Fox's frames 1 to 3, B on 1 keeping its hop's first frame's gravity, none for Falco).
- **FH combos:** each combo's press, fastfall frame and heights for both fighters against a float32 walk of the ISO's full hop by the rules above, and against the table by hand. High-Low's first laser on the full hop's Hop Peak Line, its second no lower than the lowest laser, its fastfall the first that takes (a frame earlier the fighter isn't falling yet) and a frame later passing over the low target (9.73 and 5.83 by hand), and the other timings above breaking both its targets. The doubles' second lasers each more than a target's reach apart, Hi-Hi-Low's high two within reach of each other and spread `HHL_SPREAD` either side (no other combo spread), and which presses fit three shots (Fox 1-7, Falco 1-8). B on the first frame's hop peaking lower (27.6 against 31.28, 47.4 against 51.5).
- **Placement**, on each legal stage's native collision lines from the ISO (Stadium's base layout, Yoshi's without Randall), plus Fountain with its platforms where the game starts them (its DAT holds them joint-local), for both characters: every single target between the lowest laser and the highest, spread across that range, inside the floor's ends, away from you and the last target. Fox's pairs: about half the spawns, both at one x, at one of the three short hop double lasers' heights (each turning up) over a floor of the kinds in the table and no other (never Fountain's side platforms, Stadium's missing top or FD's platforms), each kind at least half its even share with you far away and at stage center, and both side platforms used. Standing on Battlefield's top platform, every target keeps 30 from you and pairs come off the other floors. No pairs for Falco, in timed runs, or with the setting off. FH combos, for both fighters: All Combos makes about half the spawns a combo, every combo turning up, over the same kinds of floor as pairs; one combo comes every spawn; SH pairs and combos share the spawns as above; standing on Battlefield's top platform, combos keep 30 from you and come off the other floors; and none in timed runs or with the row Off.
- **Hop Peak Lines:** the heights from the ISO's jumps and lasers, and by hand. On each legal stage, for both characters, one batch of lines over everything, two a floor at the right heights and colors, the main floor's and top platform's from blast zone to blast zone and the side platforms' to the middle. Dream Land's sides apart, Fountain's lines following its side platforms and going while one is sunk, the live blast zones, the row saving and drawing at once in timed runs and Free Practice, nothing before the fighter's first frame or on other passes, and the lines under the targets. Colors: Orange's and Teal's three shades by hand, each floor's shade (dark, base, light, base on Battlefield), the color rows' names and RGB against `osd_style.h` and `osd_style.c`, picking colors saving and redrawing at once with no restart, and saved colors loading into the rows.
- **Double Jump Height:** the rise from the ISO's attributes and by hand (40.204 for Fox, 41.778 for Falco), and the full hop + double jump less it is the full hop. For both characters facing either way, one red line over everything, 12 across at the rise plus the aerial laser over the feet (49.487 and 45.576 by hand). It follows TopN and the head bone every frame, and TopN's x with no head bone. Live, it shows in a jump, falling, aerials, the aerial laser, the shine and a ledge jump, with both jumps or one left, and not on the ground, with the jump spent, helpless, air dodging, on the ledge, dead, on the revival platform or in side-B and up-B, with the states either side of each range checked. Held: for both characters and both JumpAerial states, at the frame before's height through the rise, the fall, an aerial, the laser and helpless, following the head, paused and with the row off and on, until landing, the ledge or a KO; the jump coming back shows it live again; up-B's spent jump and a JumpAerial state with no jump the frame before hold nothing, and a retry holds nothing. The row saving and drawing at once, loading into the row, timed runs, Free Practice and paused, nothing before the fighter's first frame or on other passes, and the line over the peak lines and under the targets.
- **Target Pointers:** On by default, a target drawn as one batch of four ticks on the axes through its center (black under white, from 2.5 to 5 out) and then its four discs; turning the row off saves it with no retry or restart and leaves just the discs at once, and back on brings the ticks back. A saved Off loads into the row in timed runs and Free Practice, the break ring still draws with it off, and Reset Event Settings turns it back on.
- **The event:** the first target waiting for control, breaking (score, chime, the laser ended, the ring's 12 frames, the next target), a sweep that passes through a target in one frame, the exact reach, other fighters' lasers, new hitboxes and other items never hitting, a standing laser missing the lowest target and a low laser hitting it, pairs waiting for both, Hi-Hi-Low's three targets side by side as above and waiting for all three, with the camera taking in their spread, the FH row's gating, loading, values and saving (each stored value loading into its row without a write, the old 6 as High-Low and 7 as Off, and each row saving its own value), the camera, pausing, the row's gating and (N/A), saving, the timed minute's end and best (capped at 255), Free Practice, Reset Event Settings (the colors too), Counter Position and the draw link.
- **Stages and characters:** each legal stage plays, and Peach's Castle and Kongo Jungle restart once on Battlefield with no target out. Change Character is its own one-row submenu with the swap's row hooks, offers Fox and Falco only, starts on the fighter in play with no CPU row, does nothing on A until a pick changes, and then restarts with the pick and costume and no CPU (-1).

`tests/test_settings_extension.py` covers the field's bits, its neighbors in word 80, bad writes and version 22 records, FH Multi Lasers (its bits in word 12 alone, a stored 7 reading as Off, version 23 records migrating, the retired bits and word 84 cleared, and read without writing), the line colors: word 84's bits 0-7 alone as distances from Orange and Teal, a stored 15 reading as the default, and version 24 records migrating (word 84 alone claimed, FH Multi Lasers kept) and read without writing whatever the spare bytes hold, and Target Pointers: word 84's bit 8 alone, 0 reading as On, its neighbors' writes keeping it, a bad write ignored, and On from version 23 and 24 records, migrated or not, whatever the spare bytes hold. `tests/test_ledgechase_catalogue.py` checks the descriptor (Fox and Falco only, stage select, no CPU or score slot), and `tests/test_score_storage.py` and `tests/test_menu_exit.py` the catalogue count and the DAT's exit callback.

## Still to check in Dolphin

None of the tests render or run the real laser code. Still to check in game:

- That a target looks right at its size, the ticks help spot it, and the break ring and chime (Laser Land Training's success sound) feel right. That Target Pointers Off takes the ticks away at once from the pause menu, leaves the bullseye and the ring, and stays off through a restart and a trip through the memory card.
- That the lowest targets really need a low laser (a standing laser passes over by only 0.05), and that a short hop low laser hits them, for both characters, especially Falco's floor-level targets.
- That the highest targets are reachable with a full hop and double jump laser off the top platform, and the camera shows them.
- Fox's pairs with a real short hop double laser (B on airborne frame 1, 2 or 3, no fastfall), from the main floor, the side platforms and the top platform, on each stage, and that Fountain never puts one over a side platform.
- Each FH combo with a real full hop for both fighters, at the table's frames: that the lasers break their targets and Hi-Hi-Low's three shots fit (B pressed again within each Loop). The combos assume the aerial laser keeps the jump's gravity and fall speed, as the SH pairs do.
- High-Low: that a fastfall during the laser takes on the table's frame (Fox 18, Falco 27), the first it can, and the second laser still comes out, low enough to break the low target (Fox 6.56, Falco 2.65 up), with the first along the full hop line. That the laser loop's ECB doesn't land you sooner while fastfalling.
- Hi-Hi-Low's side-by-side high targets: that they read as a pair at the peak, the spread (6 each side) looks right and neither hides the other, both high lasers break them from either side, and Falco's low target (7.99 up) is reachable.
- That picking a non-legal stage loads it, then restarts on Battlefield (the restart runs on the first frame, during Ready).
- Change Character from Fox to Falco and back, with a costume change: the new fighter and its sounds load, the SH Double Lasers row turns (N/A) for Falco, and saved settings carry over.
- The timed run's end, the new record voice, and the best saving to the card.
- Hop Peak Lines on each stage for both characters: a laser at a short hop's and a full hop's peak flies along their lines, off the floor and each platform, and B on the first airborne frame can't reach them (that hop peaks 2.1 lower for Fox's short hop and 3.68 for his full hop). Falco's short hop line is B on frame 2's, just under his peak. Check that the side platforms' lines stop at the middle, Dream Land's two sides sit apart, Fountain's follow its platforms and go while one is sunk, and the lines look right (width, under the targets).
- The line colors: that the dark, base and light shades read as one color getting lighter with height on each stage's background, the dark ones stay visible over the main floor (Final Destination's are all dark), the color rows recolor at once from the pause menu, and the picks survive a restart and a trip through the memory card. An older save should update to version 25 and keep every other setting, FH Multi Lasers included.
- Double Jump Height for both characters: that a laser shot at the peak of a double jump from anywhere in the air (rising, falling, after an aerial or laser) flies along the red line. Check that the line stays put in height once you double jump and your peak laser comes out on it, that it goes on landing, the ledge and a KO, that up-B, side-B and air dodge take it away, that it comes back live after landing, and that it centers well on the head (HeadN) without jittering badly in the poses you'd see.
