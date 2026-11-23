# Randall Timer Minimap

**Randall Timer Minimap** is a global setting on the Global Overlays & Gameplay page, right under Randall Overlay. It's also in the Training Lab's Stage Options on Yoshi's Story, under Randall Overlay. It's Off by default and saved, and both places change the same setting.

On Yoshi's Story, it draws a small map of Randall's loop through the match timer at the top of the screen:

- Two **dotted lines** run straight through the timer's numbers and past both sides of it, joined at each end, so the track is a long loop around the timer.
- A **red arrow** rides the track where Randall is, pointing the way it's moving: right along the bottom line, up the right end, left along the top line and down the left end.
- **The timer is the stage.** The part of the track behind the numbers stands for the stage between its walls, and the track past the timer's sides for where Randall comes out. The arrow is behind the numbers, and more see-through, exactly while Randall is behind the stage, and solid on the track past the timer exactly while Randall is out on that side.

It follows the real Randall every frame, so it starts where Randall starts (the bottom-left corner, heading right) and moves at Randall's speed: about 5 seconds behind the numbers, then about 5 seconds out on a side, every 20-second lap.

It works in any match on Yoshi's Story with a timer, counting up (events, the Training Lab) or down (a timed VS match). With hazards off, where Randall isn't there, it shows where he would be at the timer's time (see [Without Randall](#without-randall)). It doesn't show on any other stage, in a match with no timer, while the game hides the timer, or in a timed match's last 5 seconds (the game swaps the timer for its big countdown). It's separate from Randall Overlay, so either one can be on without the other.

The standalone [Slippi Gecko code](../slippi-gecko-codes/randall-timer-minimap/README.md) draws the same map in any match on Yoshi's Story, for vanilla Melee and Slippi netplay. Its emulator tests check it draws exactly what this version draws.

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

**The map.** The stage between its walls (X ±52.675) becomes the timer's width, with the same scale past them, so the arrow's time behind the numbers and past each side is Randall's time behind the stage and out. Randall's two heights become two lines 0.6 above and below the timer's middle, through the big digits (±1.3). The track is Randall's 13 path points, mapped the same way, with dots about every 0.8 HUD units: a 2-pixel light dot on a 4-pixel dark one, dimmer behind the numbers. The dots are laid out the same on both ends. Each half of the loop, from the bottom line's middle around an end to the top line's middle, gets an even number of them, centered on the half, at the same fractions of each half's length. So the timer's middle falls between two dots on each line, and each end has two dots on its straight, one above and one below its middle. (Spaced from Randall's start instead, the left end had one dot in the middle and the right end two.) The arrow is at the cloud joint's live world position, mapped, and points along the path segment nearest it (on a corner, the segment leaving it). It's red with a dark outline, and about 60% opaque behind the numbers.

**Finding the cloud.** The game doesn't use `GrSt.dat`'s root as the map GObj's joint: `Ground_GetStageGObj` (`801C1768`) puts the file's tree under a scale joint (`grGroundParam`'s 0.7) and gives the GObj that one. So the cloud is two levels down, the first child of the file's root, which is what the game's `Stage_GetMapGObjJObj` (`801C3FA4`) returns for index 1, and what `grStory_801E3370` keeps as Randall's joint for its puffs. The overlay asks for it the same way. The file's root itself never moves, so reading the GObj's joint's first child instead parks the arrow at the stage's middle, above the track and pointing left. The cloud's world matrix already has the path in it: `HSD_JObjMakeMatrix` puts a path-animated joint at the spline joint's world matrix times its translation, so the world position is the stage units the map expects.

## Without Randall

With hazards off, events remove Randall's map GObj (`Hazards_Disable`), so there's no cloud to read. The map then puts the arrow where a match would have him at the timer's time: his animation frame is the frames the timer shows as run plus `RANDALL_TIMER_OFFSET` (125), modulo 1200, and he rides his path at a constant speed (its lengths are normalized), so that frame is that fraction of the way around the loop from the bottom-left corner. "The frames the timer shows as run" (`Match_TimerFrames` in `events.h`) is its time counting up, or its start less what's left counting down. Counting down, the game keeps `time_ms` counting up from 59 within each second and shows `99 - ms * 99 / 59` as the hundredths (`8016AF0C`).

**The offset.** In a VS match on Yoshi's Story with an 8 minute timer, the timer read 7:50.31 (582 frames in) with the arrow, drawn from the live cloud, at X 57.9 on the top line heading left: frame 706.7 of Randall's loop, so 124.7 frames ahead of the timer, measured from the arrow's flat tail and its outline to about a frame. That's within a frame or two of Slippi's numbering, where a match's first frame is -123 and the timer starts at frame 0. 125 is the constant. A frame is a third of a stage unit of Randall's path, well under a pixel on the map.

`tests/test_randall_timer.py` drives the map frame by frame through Randall's real path from `melee.iso` with the map GObj gone, and the arrow lands on his path within a third of a pixel every frame.

## In the Training Lab

**Randall stays.** The Lab's Hazards Off removes the Shy Guys but keeps Randall (`Hazards_Disable(keep_randall)`). Its Stage Options on Yoshi's Story start with a **Randall** row, On by default and not saved (Reset to Default Settings turns it back On; a stage or character change starts it On). Off hides the cloud joint (map joint 1, the only one in his tree with a model), switches off his collision group with the game's own `Stage_DisableLineGroup` (`mpLib_80057BC0`, back on with `Stage_EnableLineGroup`, `mpJointListAdd`), and holds his puff timer (`s16` at his `MapData` + 0xC4, `801E366C`) so no puffs appear. He keeps moving, so the Randall Timer Minimap keeps following him. The Randall Overlay doesn't draw his platform while the cloud is hidden.

**In step with the timer.** The Lab hides READY and GO, so on its own its Randall would be a different number of frames ahead of the timer than in a match, and a savestate rewinds the timer but not the stage. So every frame, after the stage's animation (proc priority 1) and before Randall's think (4), the Lab compares his cloud's animation frame with the timer's plus 125. If they're more than a frame and a half apart, it requests the timer's next frame on his animation (`HSD_AObjReqAnim`), and the stage's next animation step plays that frame without advancing. He starts the Lab in step (behind the stage at the bottom left), and a savestate that rewinds the timer takes him back to where he was at that time. With Don't Reset Timers, neither moves back. Frame advance and the pause stop and step both together.

**Timer 8m Inf Loop.** A Session row, Off by default and not saved. The match timer keeps counting up, and a proc on the timer's own GObj link (16, which runs while paused) at priority 18, right after the game's timer think (`TimerThink`, 17), points the digits at an 8 minute countdown of the same moment with `ifTime_SetTime` (`802F3AE8`, `Match_SetTimerDigits`) and applies them (the timer's only animations are its digits'). It counts down as a VS match's timer does, 0:00.00 at 28800 frames, and the next frame starts again at 7:59.99. Since the match itself never counts down, the game never swaps in its 5 second countdown, beeps or ends the match. Both timers show the same moment, so switching never moves the timer or Randall, and 28800 frames are exactly 24 of Randall's laps, so at every time on the countdown he's where an 8 minute match has him.

## Saved setting

`TM_FLAG_RANDALL_TIMER` (index 14 of the flag field) is `TYEX` word 80 bit 20 (byte 82, `0x10`), with no version bump: every migration to version 23 zeroes word 80, and Off is 0. Older records read Off. See the [save data map](../notes/SAVE-DATA-MEMORY-MAP.md).
