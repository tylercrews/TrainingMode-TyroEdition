# Randall Timer

**Randall Timer** is a global setting on the Global Overlays & Gameplay page, right under Randall Overlay. It's also in the Training Lab's Stage Options on Yoshi's Story, under Randall Overlay. It's Off by default and saved, and both places change the same setting.

On Yoshi's Story, it draws a small map of Randall's loop through the match timer at the top of the screen:

- Two **dotted lines** run straight through the timer's numbers and past both sides of it, joined at each end, so the track is a long loop around the timer.
- A **red arrow** rides the track where Randall is, pointing the way it's moving: right along the bottom line, up the right end, left along the top line and down the left end.
- **The timer is the stage.** The part of the track behind the numbers stands for the stage between its walls, and the track past the timer's sides for where Randall comes out. The arrow is behind the numbers, and much more see-through, exactly while Randall is behind the stage, and solid on the track past the timer exactly while Randall is out on that side.

It follows the real Randall every frame, so it starts where Randall starts (the bottom-left corner, heading right) and moves at Randall's speed: about 5 seconds behind the numbers, then about 5 seconds out on a side, every 20-second lap.

It works in any match on Yoshi's Story with a timer, counting up (events, the Training Lab) or down (a timed VS match). It doesn't show on any other stage, in a match with no timer, while the game hides the timer, in a timed match's last 5 seconds (the game swaps the timer for its big countdown), or when hazards are off (Randall isn't there). It's separate from Randall Overlay, so either one can be on without the other.

The standalone [Slippi Gecko code](../slippi-gecko-codes/randall-timer/README.md) is still the first version, which tinted the timer instead. It hasn't been ported to the map.

## Randall's loop

Randall is map GObj 2 in `GrSt.dat`. Its root joint's first child, the cloud, rides a path animation (`HSD_A_J_PATH`), and the root's second child is the spline it follows. The animation's path value goes linearly from 0 to 1 over 1200 frames and loops, and the spline is 13 points joined by straight lines with their lengths normalized, so Randall moves at a constant speed and every lap takes exactly 20 seconds.

In stage units (the spline's joint turns it 180 degrees and moves it by (75.25, -101.5), and the map root is scaled by `grGroundParam`, 0.7), the loop is a long rectangle with cut corners, counter-clockwise from the bottom-left corner, where the cloud starts:

| Part of the loop | Cloud joint |
| --- | --- |
| Bottom, moving right | Y −38.5, X −95.725 to 95.725 |
| Right end, rising | X out to 97.475, Y −38.5 to −18.9 |
| Top, moving left | Y −18.9, X 95.725 to −95.725 |
| Left end, falling | X out to −97.475, Y −18.9 to −38.5 |

Most of the bottom and top runs are inside the stage. The stage's walls at both heights are at X ±75.25 in the DAT, ±52.675 in stage units. So on the right, Randall leaves the wall low, swings out and up, and slides back in high. On the left it's the mirror: it leaves high and goes back in low.

Randall counts as out while the cloud joint is past ±52.675. From the DAT, over one loop (frame 0 is the first frame of the stage's animation):

| Frames | Randall |
| --- | --- |
| 122–418 | Behind the stage (297 frames) |
| 419–721 | Out on the right (303 frames, about 5 seconds) |
| 722–1018 | Behind the stage (297 frames) |
| 1019–1199, 0–121 | Out on the left (303 frames) |

`tests/test_randall_timer.py` recomputes this from `melee.iso`.

## Drawing the map

**Where it draws.** The game's HUD camera (`InitHUD`, `802F390C`) is a perspective camera 64 units from its plane, which shows about 59 units across and 48.5 up. It draws GX links 8, 10 (its light) and 11 (`gxlink_prios` `0xD00`), and no other camera draws link 8. The timer is a link 11 GObj, so the map is a link 8 GObj, drawn in render pass 0: the camera runs each pass over all its links in order, so link 8's pass 0 comes before any pass of the timer's joints, and the numbers cover the map. Depth testing is off. Since it's the game's own HUD camera, it works in VS matches as well as events.

**Where the timer is.** `HUD_Timer_Initialize` (`802F446C`) stores the timer's GObj at `stc_matchhud->timer_gobj` (`804A1078`), or 0 when the match has no timer; with 5 seconds left in a timed match, the game frees it for its countdown. It's IfAll's `ScInfTim` model, placed at ScInfDmg's joint 13, (0, 17.65): a root with ten joints, in the order the game keeps them, the hour and minute separators, then two digits each of hours, minutes, centiseconds and seconds. The hours and their separator are hidden until an hour has passed. Each frame the map reads the world position of every joint that's showing and adds half its quad's width: ±0.6016 for a separator, ±1.3008 for a digit, ±0.9883 for the smaller centiseconds. Normally that's the minutes' tens to the centiseconds' ones, X −7.10 to 9.04. It reads the timer's height from its root.

**The map.** The stage between its walls (X ±52.675) becomes the timer's width, with the same scale past them, so the arrow's time behind the numbers and past each side is Randall's time behind the stage and out. Randall's two heights become two lines 0.6 above and below the timer's middle, through the big digits (±1.3). The track is Randall's 13 path points, mapped the same way, with dots every 0.8 HUD units: a 2-pixel light dot on a 4-pixel dark one, dimmer behind the numbers. The arrow is at the cloud joint's live world position, mapped, and points along the path segment nearest it (on a corner, the segment leaving it). It's red with a dark outline, and about 40% opaque behind the numbers.

## Saved setting

`TM_FLAG_RANDALL_TIMER` (index 14 of the flag field) is `TYEX` word 80 bit 20 (byte 82, `0x10`), with no version bump: every migration to version 23 zeroes word 80, and Off is 0. Older records read Off. See the [save data map](../notes/SAVE-DATA-MEMORY-MAP.md).
