# Target Practice

Select **Minigames → Target Practice** (the eighth event). Only Fox and Falco can play it. It opens the stage select, but only the six legal stages play: pick any other and the event restarts on Battlefield on its first frame, the way Up-B Sweetspot does. Targets come from the live collision lines. Hazards are off as in other events. There's no CPU.

A target is a small red and white bullseye, centered on its spot, with four ticks around it so it's easy to find with the camera zoomed out. It's drawn over everything (no depth test), so a platform never hides it. Shoot it and it breaks with a ring and a chime, the laser stops there, and the next target appears. The camera keeps the targets in view.

A timed minute counts your targets and saves your best, like Eggs-ercise. The first target appears at Go. **Free Practice** has no clock.

## Menu

- **Retry:** restart the event. Z while paused does the same.
- **Change Stage:** restart on another legal stage without going back through the character and stage select. Saved settings carry over.
- **Change Character:** a submenu with one row, **Player Character**: Left/Right picks Fox or Falco, X/Y the costume, and A restarts as them, skipping the character and stage select. It's `src/fighter_swap.h`, as in Grabs-ercise, limited to the two (`FighterSwap_Restrict`); there's no CPU row, since there's no CPU. Saved settings carry over.
- **Free Practice:** Off (default) is the timed minute; On has no clock. Saved. Changing it restarts the event.
- **SH Double Lasers:** Off (default) or On. Saved. On makes about half the targets come as [a pair](#sh-double-laser-pairs) off the main floor, a side platform or the top platform. Free Practice only, and Fox only: for Falco the row reads **SH Double Lasers (N/A)** and stays greyed out.
- **Hop Peak Lines:** Off (default) or On. Saved. [Lines](#hop-peak-lines) where a laser shot at the peak of a short hop (teal) or a full hop (purple) flies, off the main floor and each platform. Timed runs and Free Practice alike. It shows or goes on the next frame, behind the paused menu too.
- **Counter Position:** where the count and the label under it go. It's Eggs-ercise's Egg Counter Position, shared with Grabs-ercise.
- **Reset Event Settings:** Free Practice, SH Double Lasers and Hop Peak Lines Off, then a restart in timed mode. Your best and the counter position stay.
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
| Full hop + double jump | 27.6 + 40.204 = 67.804 | 47.4 + 41.778 = 89.178 |
| Highest target on Battlefield (top platform 54.4) | 131.5 | 147.4 |

The joints are all `JOBJ_CLASSICAL_SCALE`, so the arm bones' animated stretch (Fox's goes to 1.2) shears their children. The script poses them the way `HSD_JObjMakeMatrix` does.

### Target size

Each laser hitbox is 300 / 256 = 1.17 units across each way (the article's hitbox command times the 1/256 at `0x804DC790`). For both characters the standing laser is 2.72 above the lowest laser, so a target radius under 2.72 − 1.17 = 1.55 puts the lowest targets out of a standing laser's reach. The radius is **1.5**: a target breaks when a laser's center passes within 2.67 of its center, and a standing laser misses the lowest targets by 0.05.

## Hits

Each frame, every Fox or Falco laser item you own (`ITEM_FOXLASER`, `ITEM_FALCOLASER`, owner at `0x518`) is checked hitbox by hitbox. A hitbox sweeps from last frame's position to this frame's (`it_80272674`: state 1 is a new hitbox not placed yet, and 2 and on hold both ends), so a laser moving 5 to 7 units a frame can't skip a target. A swept capsule that comes within the target's radius plus the hitbox's live size breaks the target. The laser then ends the way it does on a wall: `Item_RemoveAllHitboxes` and a life of one frame (`Item_SetLifeTimer`). A laser breaks one target at most. Nothing is checked while paused. The think runs at priority 15, after the fighters' and items' updates.

## SH double laser pairs

The aerial laser fires 9 frames after the B press (Start's 5 frames and the Loop's fire frame, with the press as the first, which matches SmashWiki's aerial startup of 10), and again every Loop (Fox 10 frames, Falco 16) while B is pressed again in time (`ftFox_SpecialN_CheckLoopInput`). B can come on the first airborne frame: jumpsquat turns into the jump in the animation proc, and the jump's interrupt runs after it in the input proc.

`ShortHopPresses` walks a short hop with the live attributes and no fastfall (a fastfall only lands sooner) and counts the press frames whose second shot still comes before the landing. For Fox that's only B on the first airborne frame: shots 17.83 and 5.48 above the surface, the short hop's peak and just before landing. For Falco none fit: his short hop lands before a second shot, so there are no pairs. This matches SmashWiki, which lists the short hop double laser as Fox's.

With SH Double Lasers On in Free Practice, each spawn is a pair half the time. A pair stacks both targets at one x over the floor it's shot from, at those heights above it, at least 8 from the floor's ends. The floor is one of three kinds, each as likely as the others where the stage has it:

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

## Hop Peak Lines

A jump's highest laser is the one shot at its peak: a shot comes from where the fighter was the frame before, and the peak is the highest the fighter gets. So each floor gets two lines at that height above it, a jump's rise (`Rise`, from the live attributes, as SH Double Lasers' are) plus the aerial laser's height over TopN (the Blaster table):

| | Short hop rise | Full hop rise | Aerial laser | **Teal (short hop)** | **Purple (full hop)** |
| --- | --- | --- | --- | --- | --- |
| Fox | 8.55 | 27.6 | 9.2829 | **17.83** | **36.88** |
| Falco | 9.68 | 47.4 | 3.7980 | **13.48** | **51.20** |

Fox's teal line is the height of SH Double Lasers' first shot. A target on a line or just under it takes that jump's laser from that floor; one above it doesn't.

- **The floors** are found from the live lines every frame: the main floor at the middle (`FindSolidGround(0)`), then `FindPlatform`'s left, top and right platforms (as SH Double Lasers' pairs use them).
- **Across:** the main floor's and top platform's lines go from blast zone to blast zone. A side platform's go from its side's blast zone to the middle (x = 0) and stop, so the two sides read apart: Dream Land's right platform is 0.1 higher than its left, and its lines are that much over the left one's.
- **Fountain of Dreams:** the side platforms' lines ride them up and down. A platform sinking into the stage keeps its lines until it's less than 1 above the main floor; hidden, the game leaves it 0.375 up, inside the stage, so its lines go until it comes back.
- **Drawing:** 2 pixels wide, Training Mode's palette Purple and Teal at alpha 230, with no depth test so the stage never hides them, under the targets. They start on the fighter's first frame, once its attributes are known.

The [Hop Peak Lines Gecko codes](../slippi-gecko-codes/hop-peak-lines/README.md) draw the same lines in any match, for Slippi.

## Persistence

`TM_SETTING_TARGETS` (field 53) is `TYEX` word 80's bits 10-19, with no version bump: the timed best in bits 10-17 (0 to 255, `TM_TARGETS_BEST_MAX`), Free Practice in 18 and SH Double Lasers in 19. Hop Peak Lines, appended as `TM_TARGETS_LINES`, is bit 29 the same way. Every migration to version 23 zeroes word 80, and every default is 0, so each is stored as itself. Only a version 23 record owns them, so older records read no best, timed mode and Off. Every value fills its bits, so nothing needs repairing. The best isn't a score slot (`Events_ScoreIndex` gives page 0 event 7 none), so Reset Records doesn't clear it. See the [save map](../notes/SAVE-DATA-MEMORY-MAP.md).

## Tests

`tests/test_targets.py` runs the production event through `tests/targets_probe.c` in Unicorn:

- **Constants:** the Blaster table against `scripts/laser_heights.py` on `melee.iso`, the laser hitbox's size and the DOL's 1/256 scale, the lowest target out of a standing laser's reach, the full hop and double jump from live attributes, and which press frames fit two lasers (Fox's first frame only, none for Falco).
- **Placement**, on each legal stage's native collision lines from the ISO (Stadium's base layout, Yoshi's without Randall), plus Fountain with its platforms where the game starts them (its DAT holds them joint-local), for both characters: every single target between the lowest laser and the highest, spread across that range, inside the floor's ends, away from you and the last target. Fox's pairs: about half the spawns, both at one x, at the short hop double laser's heights over a floor of the kinds in the table and no other (never Fountain's side platforms, Stadium's missing top or FD's platforms), each kind at least half its even share with you far away and at stage center, and both side platforms used. Standing on Battlefield's top platform, every target keeps 30 from you and pairs come off the other floors. No pairs for Falco, in timed runs, or with the setting off.
- **Hop Peak Lines:** the heights from the ISO's jumps and lasers, and by hand. On each legal stage, for both characters, one batch of lines over everything, two a floor at the right heights and colors, the main floor's and top platform's from blast zone to blast zone and the side platforms' to the middle. Dream Land's sides apart, Fountain's lines following its side platforms and going while one is sunk, the live blast zones, the row saving and drawing at once in timed runs and Free Practice, nothing before the fighter's first frame or on other passes, and the lines under the targets.
- **The event:** the first target waiting for control, breaking (score, chime, the laser ended, the ring's 12 frames, the next target), a sweep that passes through a target in one frame, the exact reach, other fighters' lasers, new hitboxes and other items never hitting, a standing laser missing the lowest target and a low laser hitting it, pairs waiting for both, the camera, pausing, the row's gating and (N/A), saving, the timed minute's end and best (capped at 255), Free Practice, Reset Event Settings, Counter Position and the draw link.
- **Stages and characters:** each legal stage plays, and Peach's Castle and Kongo Jungle restart once on Battlefield with no target out. Change Character is its own one-row submenu with the swap's row hooks, offers Fox and Falco only, starts on the fighter in play with no CPU row, does nothing on A until a pick changes, and then restarts with the pick and costume and no CPU (-1).

`tests/test_settings_extension.py` covers the field's bits, its neighbors in word 80, bad writes and version 22 records. `tests/test_ledgechase_catalogue.py` checks the descriptor (Fox and Falco only, stage select, no CPU or score slot), and `tests/test_score_storage.py` and `tests/test_menu_exit.py` the catalogue count and the DAT's exit callback.

## Still to check in Dolphin

None of the tests render or run the real laser code. Still to check in game:

- That a target looks right at its size, the ticks help spot it, and the break ring and chime (Laser Land Training's success sound) feel right.
- That the lowest targets really need a low laser (a standing laser passes over by only 0.05), and that a short hop low laser hits them, for both characters, especially Falco's floor-level targets.
- That the highest targets are reachable with a full hop and double jump laser off the top platform, and the camera shows them.
- Fox's pairs with a real short hop double laser (B on the first airborne frame, no fastfall), from the main floor, the side platforms and the top platform, on each stage, and that Fountain never puts one over a side platform.
- That picking a non-legal stage loads it, then restarts on Battlefield (the restart runs on the first frame, during Ready).
- Change Character from Fox to Falco and back, with a costume change: the new fighter and its sounds load, the SH Double Lasers row turns (N/A) for Falco, and saved settings carry over.
- The timed run's end, the new record voice, and the best saving to the card.
- Hop Peak Lines on each stage for both characters: a laser at a short hop's and a full hop's peak flies along the teal and purple lines, off the floor and each platform. Check that the side platforms' lines stop at the middle, Dream Land's two sides sit apart, Fountain's follow its platforms and go while one is sunk, and the lines look right (width, colors, under the targets).
