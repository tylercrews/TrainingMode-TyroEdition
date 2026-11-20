# Up-B Sweetspot Practice: research

**Status (November 3, 2026):** a first version of the event is in T9 ([src/upbsweetspot.c](../src/upbsweetspot.c)), not yet tested in Dolphin; see [what's implemented](#implemented-in-t9). The idea is an event like Side-B Sweetspot, but for every character's up-B: an overlay that shows where to up-B from (and, for aimed up-Bs, the angle to hold), a live line for the angle you're holding while the up-B charges, Normal/Zoomed Out camera like Edgeguard, and a Marth on the stage spaced to tipper forward smash you if you overshoot.

Sources: the Melee decompilation (doldecomp/melee, `src/melee/ft/kinds/*/ft*specialhi.c`, `src/melee/mp/mpcoll.c`) for the move logic, and the vanilla ISO for the numbers. Root motion comes from [`scripts/fighter_move_data.py`](../scripts/fighter_move_data.py) (e.g. `python -I scripts/fighter_move_data.py melee.iso PlMs SpecialAirHi`); attribute values were read from each `PlXx.dat`'s special attributes (`ftData + 0x04`). Distances are world units.

## Short answer

- **Nothing has to be authored per stage.** Every guide can be drawn from the live ledge position (the same ledge table Side-B and Edgeguard use) and the fighter's live data: the ledge-grab box in `coll_data`, the move's attributes in `special_attributes`, and the stick in `input.lstick`. Only the stage's underside differs, and that only matters for very low up-Bs (see [Stages](#stages)).
- **One rule decides every sweetspot:** the game only looks for a ledge on frames where the fighter **moved down** (`cur_pos.y < prev_pos.y`, `mpcoll.c`). A sweetspot is wherever the descending part of an up-B passes the ledge-grab box over the ledge. Rising into the ledge never grabs it.
- **Characters fall into four groups** ([table](#every-character)):
    - **Aimed** (Fox, Falco, Sheik, Zelda, Mewtwo, Pikachu, Pichu; Ness indirectly): a full circle of directions. The sweetspot is a **ring** around the ledge at the up-B's travel distance. These get the angle overlay.
    - **Fixed path** (Mario, Dr. Mario, Luigi, Marth, Roy, Falcon, Ganondorf, Peach, Kirby): an animated path with a little stick angle or drift. The sweetspot is a **curve** below and outside the ledge (the path upside down).
    - **Physics rise** (Samus, Donkey Kong, Bowser, Link, Young Link, Game & Watch, Ice Climbers): simulated from attributes, with drift. Also a curve.
    - **No recovery up-B** (Yoshi, Jigglypuff): leave them out of the CSS, or give them a different guide later.
- **The held angle can be shown exactly.** Each aimed up-B reads `input.lstick` on one known frame with a simple formula ([below](#showing-the-angle-youre-holding)). Re-running that formula every frame of the charge gives the exact direction and distance you'd get if the up-B locked in now.
- **Camera:** reuse Edgeguard's setup unchanged: [`zoom_camera.h`](../src/zoom_camera.h) with the bottom blast zone in view (`ZoomCamera_Init(value, 1)` plus `ZoomCamera_Follow` every frame).
- **Marth:** freeze his forward smash on its last active frame (frame 13), with the sword all the way down, the way Side-B freezes the down-tilt, and place him from where the tipper hitbox actually is at runtime.
- **Stages:** limiting to the six legal stages is cheap: keep the stage select and restart on Battlefield if the stage isn't legal ([below](#stages)). There's no need to cut it down to Battlefield and Final Destination.

## How the game decides a ledge grab

From `mpcoll.c` (`mpColl_80044164` and the air collision that calls it) and `ftcliffcommon.c`:

- **Box.** Relative to the fighter's position (TopN), facing the ledge: horizontally from the fighter out to `ecb.right + ledge_snap_x`, vertically `ledge_snap_y ± ledge_snap_height / 2`, swept over the frame's movement. The ledge must be inside it, and the ECB's bottom (the feet) must be below the ledge. TM already reads these as `coll_data.cliffgrab_width`, `cliffgrab_y_offset` and `cliffgrab_height` (Side-B's `LedgeGrabTop` uses them). In `mpColl_80044164` the sweep is the box from the lower of the frame's two positions to the higher, so moving down it grabs on the first frame your position is `ledge_snap_y − ledge_snap_height / 2` or less below the ledge (Fox and Falco: 8.5), with the ECB's bottom still outside the ledge ([Coming down](#coming-down-where-the-ledge-snaps-t10)).
- **Moving down.** The check only runs when `cur_pos.y < prev_pos.y`. A perfectly level travel frame doesn't count either.
- **Holding down** past the drop threshold (`input.lstick.y <= -x480`) turns ledge grabs off.
- **The move must ask for it.** Each up-B's collision callback decides when, and in which direction:
    - **Fox and Falco's travel** checks **both** directions (`CLIFFCATCH_BOTH`). The charge and the fall after it check the facing direction only.
    - **Marth and Roy:** only once `cmd_var0` is set (frame 6 for Marth, 10 for Roy) and velocity is downward. Marth also **skips the first falling frame** (`cmd_vars[1]` latch).
    - **Luigi, Mario and Dr. Mario:** `cmd_var0` (frame 9, 7 and 4) and falling.
    - **Falcon and Ganondorf:** a flag set during the dive (script frame 13).
    - **The teleports** (Sheik, Zelda, Mewtwo) and Quick Attack check the facing direction during the travel and the reappear.
    - **Donkey Kong, Link and Young Link** check both directions.
- **Ledge-grab box sizes** (unscaled, `ftData + 0x44`; the game multiplies them by the fighter's scale):

| Fighter | snap x / y / height | Fighter | snap x / y / height |
| --- | --- | --- | --- |
| Mario, Dr. Mario | 12 / 15 / 10 | Fox, Falco | 11 / 13 / 9 |
| Luigi | 12 / 15 / 10 | Falcon, Ganondorf | 9 / 17 / 11 |
| Marth, Roy | 12 / 17 / 11 | Sheik | 11 / 15 / 11 |
| Zelda | 11 / 10 / 11 | Mewtwo | 11 / 14 / 12 |
| Pikachu | 9 / 11 / 10 | Pichu | 9 / 6 / 6 |
| Peach | 10 / 15 / 11 | Kirby | 10 / 8.5 / 8 |
| Samus | 10 / 10 / 9 | DK | 15 / 15 / 10 |
| Bowser | 13 / 12 / 12 | Link, Young Link | 11 / 16 / 10 |
| Game & Watch | 12 / 15 / 10 | Ice Climbers | 12 / 13 / 10 |
| Yoshi | 13 / 10 / 11 | Jigglypuff | 9 / 10 / 9 |

"Grabbing faster" falls out of this. The fastest sweetspot is the one whose first grab-capable frame already has the ledge in the box. Start lower and you fall short or hit the stage's side. Start higher or closer and you either rise over the ledge and land on the stage (Marth's tipper) or fall back down to it later.

## Every character

**Aimed:** a full circle of directions.

| Fighter | Up-B | Path and numbers (ISO) | Aim and when it locks | Grabs |
| --- | --- | --- | --- | --- |
| Fox | Firefox | Charge: 43-frame animation. Entering zeroes Y speed and divides X by 1.25. No gravity for 15 frames, then 0.015 a frame (about 6 lower by launch); X slows 0.02 a frame. No drift. Travel: 30 frames at 3.8, slowing 0.1 a frame from frame 6, **81.5** total, ending at 1.3 a frame. | Locks on the charge's last frame. If \|x\|+\|y\| ≥ 0.5, angle = atan2(y, x·facing), turning around first if \|x\| > 0.125. Otherwise straight up. | Travel only while angled down, either direction. After travel, the fall grabs facing. The charge can also grab. |
| Falco | Firebird | Same charge (gravity 0.016 after 15 frames). Travel: 22 frames at 4.2, slowing 0.17 a frame from frame 4, **60.1** total. | Same as Fox | Same as Fox |
| Sheik | Vanish | Startup: 36-frame animation, slow fall. Travel: 20 frames (invisible) at 1.0·m + 1.0, where m is stick magnitude: **30** at the 0.5 minimum to **40** at full tilt. | Locks at the end of startup. If m > 0.5, angle = atan2(y, x·facing), facing the stick. Otherwise straight up at full distance. | Travel and reappear, facing |
| Zelda | Farore's Wind | Startup: 34-frame animation. Travel: 20 frames at 2.2·m + 2.0: **62** to **84**. | Same rule (m > 0.5) | Same as Sheik |
| Mewtwo | Teleport | Startup: 8-frame animation. Travel: 10 frames at 2.0·m + 3.0: **40** to **50**. | Same rule (m ≥ 0.5) | Travel and reappear, facing |
| Pikachu | Quick Attack | Startup: 14-frame animation. First zip: 5 frames at 4.0·m + 5.9: **39.5** to **49.5**. Second zip: 0.9× as fast, if its angle differs from the first by more than 38°. | Each zip locks when it starts (m ≥ 0.5, else straight up) | Both zips |
| Pichu | Agility | First zip: 8 frames at 2.2·m + 2.2: **26.4** to **35.2**. Second zip at the same speed, if it differs by more than 5°. | Same | Both zips |
| Ness | PK Thunder 2 | You steer the thunder. Ness flies away from where it hits him. | The direction is from the thunder to Ness at contact | (not checked) |

**Fixed path:** animated (TransN root motion, from the ISO).

| Fighter | Up-B | Path | Stick control | Grabs from |
| --- | --- | --- | --- | --- |
| Mario | Super Jump Punch | Rises 40.4 by frame 24, 17.6 forward | Before frame 7: \|x\| > 0.625 tilts it up to 18°; \|x\| > 0.25 turns around | Frame 7, falling |
| Dr. Mario | Super Jump Punch | Same path | Same, until frame 4 | Frame 4, falling |
| Luigi | Super Jump Punch | Rises 45.9 by frame 26, straight up | About 2° | Frame 9, falling |
| Marth | Dolphin Slash | Rises 47.3 by frame 21, 10.2 forward | Before frame 6: \|x\| > 0.5 tilts it up to 20°; \|x\| > 0.25 turns around | Frame 6, falling, after skipping one frame |
| Roy | Blazer | Rises 44.5 by frame 30, 12.3 forward | Same, up to 40° (attribute `0x38`), until frame 10 | Frame 10, falling |
| Falcon | Falcon Dive | Rises 37.6 by frame 44, 11.9 forward | No stick angle. The frame-13 flag only turns him around; drift steers the whole rise, up to 0.95 a frame (about 42 over 44 frames) | After the frame-13 flag |
| Ganondorf | Dark Dive | Rises 41.9 by frame 44, 13.2 forward | Same as Falcon, up to 0.66 a frame (about 29) | Same as Falcon |
| Peach | Parasol | Rises 36.8 by frame 33, 3.4 forward | Before frame 6: \|x\| > 0.625 tilts it up to 18°, the same rule as Mario | Falling only |
| Kirby | Final Cutter | 23-frame windup, then rises 50.6 by frame 17 of the rise and drops | Little | Several of its states |

**Physics rise:** simulated from attributes.

| Fighter | Up-B | Path | Notes |
| --- | --- | --- | --- |
| Samus | Screw Attack | About 48 (2.5 up against 0.066 gravity) | Drift. Grabs only falling. Tether is a separate move. |
| Donkey Kong | Spinning Kong | Barely rises in the air (0.675 up, gravity 0.07); 1.4 sideways | Grabs either direction |
| Bowser | Whirling Fortress | Small rise, mostly sideways | |
| Link, Young Link | Spin Attack | Small rise (`specialhi_grav_mul`) | Grabs either direction. Tether is a separate move. |
| Game & Watch | Fire | Animated, not physics: rises 75.6 by frame 30, 9.1 forward. The motion is on joint 2, not TransN's usual joint 1, which is why `fighter_move_data.py` shows none. The first stick past 0.25 (`0x58`) before frame 8 locks the angle at (\|x\| - 0.25) × 0.52 rad (`0x5C`), up to 22.5°, and turns him that way. | Grabs facing, falling, after frame 4 |
| Ice Climbers | Belay / Squall Hammer | Belay depends on where Nana is (60 range); Squall is mashed | A fixed guide only fits Squall. See the [Edgeguard notes](EDGEGUARD-NEW-CHARACTERS.md). |

**No recovery up-B:** Yoshi (Egg Toss gives no lift in the air) and Jigglypuff (Sing). Leave them out of the event's CSS.

## The overlay, per group

All of these are drawn in world space with the match camera, like Side-B's `Draw_Lines` (`CObj_SetCurrent(*stc_matchcam_cobj)`, `GFX_Start` / `GFX_AddVtx`), so they stay on the ledge as the camera moves. The target point T is the ledge minus the grab-box offset: where the fighter's position must be for the box to just hold the ledge.

### Aimed (Fox as the example)

```
              stage
   ───────────────●  ledge
          .  ´    T  ← end here: the box holds the ledge
       ´          |
     ´   too close: |  ← inside the ring, aiming at T rises through it
    ´    overshoot  |    and lands on the stage (Marth)
    |   onto stage  |
     `            ´
        `  .  ´  ring: radius = travel (Fox 81.5, Falco 60.1)
```

- **Before up-B: the sweetspot ring.** An arc of radius D (the travel distance) around T, drawn only offstage, except for Fox and Falco, who can also come up from under the stage: their ring carries on under the stage to 2/3 of a circle, stopping 30 degrees short of level so it doesn't stick up into the stage (and never over the top of it), drawn over the stage, with a white tick where it's level with T: up-B there and hold straight toward the stage. Up-B anywhere on it, aimed at T, and the travel ends with the ledge in the box: the fastest grab.
    - Above the ledge, the whole inside of the ring works, angled down at the ledge (the travel grabs on the way, in either direction).
    - Below it, closer than D overshoots onto the stage.
    - Ending travel anywhere in the column just outside the ledge also works, only slower (you fall to it).
    - This is the "crescent circle" idea from the README's investigate list.
- **During the charge: two lines from the launch point.** Fox and Falco can't drift while charging, so the launch point is known when the up-B starts (simulate the charge's fall). From it, draw:
    - **Needed angle:** a white line to T.
    - **Held angle:** the line for the angle you're holding, D long, with a dot where the travel would end. Color it green if that end grabs right away, yellow if you'd fall to the ledge, red if you'd land on the stage or come up short.
- **Teleports and Quick Attack:** stick magnitude changes the distance, so the ring becomes a band from the 0.5-tilt distance out to full. The held line's length follows the stick, so tilt and angle are shown together. They can drift during startup, so recompute the launch point every frame.
- **After lock:** keep the held line where it locked (capture it on the state change, the way Side-B captures `range_origin`). The Trails API can draw the real path over it.
- **Ness:** draw the needed direction from Ness, plus where the thunder has to be to send him that way: the same line, extended through him on the other side.

### Fixed path and physics rise

- Generate each character's path ahead of time: frame-by-frame root-motion offsets in a table in the event's source, made by a script (like `sweetspot_knockback_presets.py`). For the physics moves, simulate in the event from the live attributes.
- **Sweetspot curve.** For each frame t the move can grab on, the start point S(t) = T − path(t), mirrored for facing. Those points form a curve hanging below and outside the ledge: the path flipped upside down.
    - Its top end is the fastest grab: the first frame it can grab, at about the peak.
    - Draw it as a line with a marked dot for the fastest spot, like Side-B's numbered dots.
- **Stick control** (Marth, Roy, Mario, Dr. Mario, Game & Watch): while the angle can still change, draw the predicted path for the stick you're holding, tilted the same way the game will tilt it.
- **Drift** (Falcon, Ganondorf, Peach, Samus): draw where full drift toward the stage and no drift would end.

## Showing the angle you're holding

- **Input.** `input.lstick` (FighterData `0x620`) is the processed stick the move code reads, with Melee's deadzone (an axis under 0.2875 reads 0) and the 1/80 steps already applied. Every frame of the charge, run the same formula as the move (table above), then draw the result.
- **Charge states.** Fox and Falco charge in `SpecialHiHoldAir`, state `0x162`, which Side-B and Edgeguard already check for. The aimed launch reads the stick on the charge's last frame, so a dot or tick on the charge's frame count can show how many frames are left to line it up.
- **HUD.** The Wavedash and Ledgedash events already draw a stick icon in the HUD panel with `HUD_DrawInfoPanelWithStick`. Rows like **Held 31° / Need 34°** could use it. For numbers in world space, Side-B's seven-segment `DrawRangeNumber` works.

## Camera

Same as Edgeguard:

- `ZoomCamera_Init(saved_value, 1)` in `Event_Init`.
- A GObj with `ZoomCamera_Restore` as its cleanup.
- `ZoomCamera_Follow((hmn_x + cpu_x) / 2)` every frame.
- `ZoomCamera_Apply` from the menu row's OnChange.

It needs its own saved Camera setting (1 bit).

Zoomed Out adds 80 on each side, about one Fox ring. On Normal, the far side of Fox's and Zelda's rings can be off-screen when you start far out.

## Enemy Marth: tipper forward smash

From `PlMs.dat` (`AttackS4`, the forward smash's subaction script):

| Hitbox | Bone | Damage | Size | Notes |
| --- | --- | --- | --- | --- |
| 0 | 76 (sword) | 14 | 3.91 | |
| 1 | 71 | 14 | 3.12 | |
| 2 | 4 | 14 | 3.52 | |
| **3 (tipper)** | **76 (sword)** | **20** | **3.91** | **5.08 further along the blade than 0** |

- **Shared values:** all four are active frames 10 to 13 (cleared on 14), with angle 361 (45° on an airborne target). The tipper has base knockback 80 and growth 70; the others 60 and 70.
- **Scale:** sizes and offsets are multiplied by Marth's 1.15 scale. The tipper's radius is about 4.5.
- **Freezing.** Side-B already freezes Marth's down-tilt on its first active frame with `FrameSpeedChange(cpu, 0)` (its tipper is also hitbox 3). Do the same with forward smash, on the last frame before its script clears the hitboxes.
- **Placement at runtime.** The blade's angle comes from the animation, so read where hitbox 3 actually is once Marth is frozen. Then move him so the tipper sits where a slight overshoot comes down: a little inland of the ledge and low.
    - Where that is depends on the character. Take it from the same path data: run the path from a launch point just inside the sweetspot and see where it lands.
    - Make it a setting (Off / On, maybe a spacing) so the overlay can be practiced alone.
- **A reacting Marth** that starts the forward smash 10 frames before you'd land would look more natural. It needs the landing prediction to be right, so do the frozen version first.

## Stages

- **Per-stage work.** None for the overlay: everything is ledge-relative, and the ledge table in [`sweetspot.c`](../src/sweetspot.c) covers every stage. What changes per stage is the underside:
    - Battlefield's undercut, Yoshi's Story's and Final Destination's sloped sides, and Pokémon Stadium's straight walls.
    - Very low launch points can hit those before reaching the ledge (the game's own bonk shows it).
    - The overlay could mark a blocked path by raycasting the predicted path against the stage's lines, but that can wait.
- **Restricting to the six legal stages.** `event_vars->restart_on_stage` only works for events with `.stage = -1` (`Events_RestartOnStage`, [`events.c`](../src/events.c)). Options, simplest first:
    1. **(Recommended)** Keep the stage select (`.stage = -1`). On the first Think frame, if `Stage_GetExternalID()` isn't one of `StageSwap_Stages`, call `restart_on_stage(GRKINDEXT_BATTLE)`. The [Change Stage](../src/stage_swap.h) menu already offers only the six. No ASM.
    2. **Fixed stage.** Give the event a fixed stage, skipping the select, and let `Events_RestartOnStage` accept it through a new `EventDesc` flag, so Change Stage still works.
    3. **Hide the other icons** on the stage select. That needs new SSS hooks; only `Load SSS For Certain Events.asm` exists today, and it only picks whether the select is shown.
- **Battlefield and Final Destination only** would only save Dolphin test time, not code.

## Open questions for the event itself

- **Starting position.** Side-B launches you with knockback presets. Here it's probably better to place you offstage in a fall, at spots relative to your own sweetspot: on it, too high, too low, too far, or random.
- **Success.** A ledge catch straight out of the up-B. The overlay knows the fastest possible grab, so the HUD could show **Grabbed in N frames (best M)**. A Sweetspot + Ledgedash criterion could reuse Side-B's.
- **Save data.** About 1 byte: camera, overlay, Marth, start position, criterion. The [memory map](SAVE-DATA-MEMORY-MAP.md) had 20 free bytes as of T6.
- **Which characters first.** Fox and Falco (the angle overlay, and the README's crescent idea), then Marth, Roy, Falcon, Ganondorf, Mario, Dr. Mario and Luigi (path tables), then the rest.

## Implemented in T9

[src/upbsweetspot.c](../src/upbsweetspot.c) follows this note, with these choices:

- **Start:** an angel platform 40 out and 15 above the ledge, placed the way Ledgedash places its Respawn Platform start. Unlike Ledgedash, the game's own `RebirthWait` interrupt still decides when you leave, so any action works, up-B included. Since T10 each character has its own spot ([Start spots](#start-spots-t10)).
    - That exit grants respawn invincibility (`ftColl_8007B7A4`). The event sets the remaining frames to 1 once you're off, so the game ends it itself, flashing included.
- **Up-B detection:** each character's up-B action-state range, from the decompilation's motion-state enums (clones share their original's). Yoshi and Jigglypuff are left off the CSS.
- **Guides:**
    - Aimed up-Bs and fixed paths are drawn as described above. Game & Watch is a fixed path too. DK, Bowser, Link, Young Link, Ness and the Ice Climbers only get the ledge target.
    - The aimed lines lock to the travel's real velocity on its first frame, not to the stick.
    - Fixed paths get a yellow line at the angle the up-B set out at: taken once, when it has risen `PATH_ANGLE_RISE` (6), from where it started to where it is, and drawn the length of `UpB_Info`'s path. It never moves after that, since following the fighter to the top wiggled with Falcon's drift. Only rising counts, because Mario and Peach drop first. It's the [measured](#air-up-bs-and-the-animation-numbers) angle, not one worked out from `lstick_angle` and `UpB_Info`, which would be wrong for two reasons: the air rise isn't the animation's, and Falcon and Ganondorf have no angle to lock.
- **Marth:** his attack starts as each attempt does and freezes with all four hitboxes out, on frame 13 for the forward smash and frame 7 for the down-tilt (`AttackLw3`, hitbox 3 is its 10% tipper), while you're still on the platform, so a hit doesn't depend on when you up-B. `AttackS4`'s script creates its hitboxes at 10 and clears them at 14, and `Fighter_IncrementStateFrameInfo` leaves `state.frame` on the next animation frame before the script runs (80073240), so a command at time t runs when `state.frame` reaches t and 13 is the last frame with them out. The freeze is set again every frame, as Side-B's is; nothing in the game is known to undo it, but that was the only difference left between the two. The forward smash's placement isn't computed ahead of time: its first freeze reads hitbox 3's position and slides Marth (and his hitboxes) so the tipper is right over the ledge, then later resets use that measured reach. The down-tilt stands 15 in from the ledge, Side-B's spot, so its tip reaches past the ledge the same way.
    - The first Dolphin test had only the tipper on, 12 inland. It was too far back, and the whole forward smash was wanted.
- **The white X** uses the ECB side facing the ledge (`ecb.right` facing right, `-ecb.left` facing left), like `mpColl_800443C4` and its mirror. The first version used `ecb.right` on both ledges.
- **Settings:** Enable Line Guides, Marth and Camera are saved to the memory card, and since T10 B0XX Angles and Landing Line (Fox and Falco; greyed out for everyone else). The success rate is session-only, carried through Change Stage with `carry_store`.
- **Stages:** the first-frame guard from [Stages](#stages), option 1.

## The stick's dead zones and B0XX angles (T10)

**How the game reads the stick.** Every aimed up-B takes its angle as `atan2(lstick.y, lstick.x * facing)` from `input.lstick`: Fox and Falco in `ftFx_SpecialAirHi_Enter`, Sheik in `ftSk_SpecialHi_80113A30`, and Zelda's, Mewtwo's and Pikachu's air launches the same way. That stick is:

- **In 80ths, inside a circle.** `HSD_PadClamp` (`controller.c`) runs with `gmmain.c`'s settings: a circle clamp of radius 80, no inner dead zone, scaled by 80. A stick past the circle is scaled back onto it, each axis rounded toward 0.
- **Zeroed near each axis.** `Fighter_procInput` (`fighter.c`) sets an axis to 0 when it's at or under `horizontal_stick_deadzone` or `vertical_stick_deadzone` (PlCo.dat `ftLoadCommonData` 0x0 and 0x4, both **0.28**). So an axis reads 0 or at least 23/80 (0.2875). Hold the stick straight left and move it up or down a little, and it still reads exactly left until the other axis passes 22/80, about 16 degrees of stick at full tilt.

So the shallowest angle off a straight direction is `atan2(23, 76)`, **16.84 degrees**: 23 is the least the other axis can read, and 76 the most the first can then read inside the circle (77² + 23² is past 80²). It's the same limit as the Wavedash OSD's shallowest angle. Nothing strictly between straight and 16.84 degrees off it can be aimed, either side of straight across, up and down. That's the game, not the controller: notches only make the angles it does read easier to hit.

**What the guides draw.**

- Each quarter of the ring between straight directions runs from 16.84 degrees past one to 16.84 short of the next. A white tick across the ring on each straight direction marks where holding straight sweetspots (Fox's level tick was the first), with 2 degrees of ring either side through it (`STRAIGHT_ARC`), so the ring still reads as a circle and the tick shows its distance.
- The teleports and Quick Attack go farther with more tilt. At φ degrees off the nearest straight direction, the off-axis part of a tilt t is t·sin φ, so the least tilt that aims there is max(0.5, 0.2875 / sin φ). The dim edge follows it: it meets the ring at 16.84 degrees, where only a near-full tilt reads off-axis, and is the 0.5 threshold from asin(0.2875 / 0.5) = 35.1 degrees on. Straight works at any tilt past 0.5, so their white marks run from the dim edge to the ring, with 2 degrees of the dim edge (at the 0.5 threshold) and of the ring through their ends.
- The event reads the deadzones live from `*stc_ftcommon`; the Gecko codes take them from the ISO.

**The B0XX.** HayBox's Melee mode ([`src/modes/Melee20Button.cpp`](https://github.com/JonnyHaystack/HayBox/blob/master/src/modes/Melee20Button.cpp)) is, by its README, "up to date with B0XX V3 specifications", so its coordinates are the ones used. Its comments give the B0XX's design angles: 22.96 to 67.04 degrees, 4.41 apart, with 45 in the middle. In 80ths, toward the stage and up, for one quadrant (the other three mirror it):

| Input | Without B | Degrees | With B | Degrees |
| --- | --- | --- | --- | --- |
| Straight across, straight up, diagonal | 80, 0 / 0, 80 / 56, 56 | 0 / 90 / 45 | the same | |
| Mod X across, up | 53, 0 / 0, 43 | 0 / 90 | the same | |
| Mod Y across, up | 27, 0 / 0, 59 | (too little: straight up) / 90 | 80, 0 / 0, 59 | 0 / 90 |
| Mod X diagonal | 59, 25 | 22.96 | 73, 31 | 23.01 |
| Mod X diagonal + C-Down | 56, 29 | 27.38 | 70, 36 | 27.22 |
| Mod X diagonal + C-Left | 63, 39 | 31.76 | 68, 42 | 31.70 |
| Mod X diagonal + C-Up | 56, 41 | 36.21 | 59, 43 | 36.09 |
| Mod X diagonal + C-Right | 49, 42 | 40.60 | 51, 43 | 40.13 |
| Mod Y diagonal | 25, 59 | 67.04 | 31, 73 | 66.99 |
| Mod Y diagonal + C-Down | 29, 56 | 62.62 | 36, 70 | 62.78 |
| Mod Y diagonal + C-Left | 39, 63 | 58.24 | 42, 68 | 58.30 |
| Mod Y diagonal + C-Up | 41, 56 | 53.79 | 47, 64 | 53.71 |
| Mod Y diagonal + C-Right | 51, 61 | 50.10 | 47, 57 | 50.50 |

- Every axis there is past the deadzone, so the game reads them as sent. Mod Y across (27/80) is under Fox's and Falco's 0.5 of \|x\| + \|y\| and the teleports' 0.5 tilt, so it goes straight up. With B, Mod Y across is a full tilt (HayBox's "turnaround neutral B nerf").
- B0XX firmware has changed: the v1.00 manual's Mod X diagonal is 0.7375, 0.2875 (21.3 degrees), and its longer angles were on L, not B.
- Fox and Falco go the same distance at any tilt, so B moves their angles under half a degree (0.67 units on Fox's ring, less than a dot): they get one dot per angle. The teleports and Quick Attack get both: with B nearly at the full travel, without B closer in. Sheik's Mod X diagonal is 36.0 away without B (0.80 of a tilt) and 39.8 with it. Mod Y + C-Right is the exception: without B it's the longer one.
- Each dot is where up-B with that stick has to start to end at the X: the stick's direction reversed, at the distance its tilt travels. Under the stage, Fox's and Falco's are from aiming up and away from it, from 31.7 degrees (Mod X + C-Left) up: 23 and 27 would start past the ring's end, 30 degrees short of level.
- Spots under a unit apart are one dot: white first, then blue, then dim.

## Start spots (T10)

**The problem.** Donkey Kong and Bowser couldn't get to anywhere they could sweetspot from on the old platform (40 out, 15 up). Their air up-Bs barely rise but go a long way sideways, and they keep the drift you carry into them:

- **Donkey Kong** (`ftDk_SpecialAirHi`): 0.675 up, under 0.07 of his gravity until `cmd_var0` at frame 27, then full gravity. That's a peak of about 16.5 at frame 31. Sideways he accelerates 0.05 a frame toward 1.4 (attributes `0x4C` to `0x60`), starting from your drift capped at 0.85. Held toward the stage from a standstill, he's 51 across when he's back to the height he started at (frame 50) and 100 across by the end (frame 85). Drift carried in adds about 16.
- **Bowser** (`ftKp_SpecialAirHi`): 1.78 up against 0.05 gravity (max fall 1.0) until frame 50, then FallSpecial. Sideways he accelerates 0.02 toward 1.6 (`0x54` to `0x6C`), and his drift is kept, not capped. That's a peak of about 31 at frame 35. From a standstill he's back to his start height 39 across (frame 65). Carrying 0.8 of drift in, that's 87.
- **Link and Young Link** (`ftLk_SpecialAirHi`): 2.3 (2.31) up against half gravity for 80 frames, keeping half your drift, toward 0.6 (0.8) of their drift. That's a peak of about 47 at frame 41.

From 40 out, holding toward the stage as you fall (what you'd naturally do), any up-B overshoots onto the stage. The only grabs left are after falling 23 to 34 frames, using the stage's wall, in a window of about 3 frames.

**The rule now** (`Start_Spots` in the event; Ness and the Ice Climbers keep the old spot):

- **Path up-Bs:** 20 straight above the blue dot (the fastest grab), and at least 15 out so the platform clears the stage's wall. Luigi, Kirby, Samus and Peach go straight up, so they drift in a little on the way down.
- **Aimed up-Bs:** 20 above the white X, the full travel out: fall a little, up-B, and hold straight toward the stage. Fox's and Falco's charge drops about 6, so they up-B 6 above the X.
- **Donkey Kong, Bowser, Link and Young Link** have no guide, so their spots come from a simulation: drop off the platform holding toward the stage, up-B after k frames, and keep holding toward the stage. See "The physics up-Bs' second try" below.
- **Blast zones:** the platform is never within 35 of the bottom blast zone or 20 of a side one. Game & Watch is moved on Yoshi's Story, where his dot is below the blast zone anyway, and so is Bowser (by 10).

| Fighter | Out, up from the ledge | From |
| --- | --- | --- |
| Mario, Dr. Mario | 25, −35 | dot 24.7, −55.4 |
| Luigi | 15, −41 | dot 7.2, −60.9 |
| Marth | 17, −44 | dot 17.2, −64.3 |
| Roy | 19, −42 | dot 19.4, −61.5 |
| Captain Falcon | 18, −35 | dot 18.4, −54.6 |
| Ganondorf | 21, −39 | dot 20.5, −58.9 |
| Peach | 15, −32 | dot 9.4, −51.8 |
| Kirby | 15, −41 | dot 6.0, −60.7 |
| Samus | 15, −37 | dot 6.0, −57.3 |
| Game & Watch | 16, −71 | dot 16.1, −90.6 |
| Fox | 88, 7 | X 6.5, −13, plus 81.5 |
| Falco | 67, 7 | X 6.65, −13, plus 60.1 |
| Sheik | 47, 5 | X 7.0, −15, plus 40 |
| Zelda | 91, 10 | X 6.5, −10, plus 84 |
| Mewtwo | 57, 6 | X 7.1, −14, plus 50 |
| Pikachu | 55, 9 | X 5.5, −11, plus 49.5 |
| Pichu | 41, 14 | X 5.5, −6, plus 35.2 |
| Donkey Kong | 105, 35 | see below |
| Bowser | 110, 15 | |
| Link | 85, 35 | |
| Young Link | 100, 35 | |
| Ness, Ice Climbers | 40, 15 | the old spot |

The dots and X's come from `scripts/upb_overlay_numbers.py`'s numbers. The physics up-Bs' second spots haven't been checked in Dolphin. If they still overshoot, move them out along the band (about 10 out for every 15 up); if they come up short, move them in.

### The physics up-Bs' second try

The first spots (DK 70, 10; Bowser 60, 0; Link 55, 0; Young Link 65, −5) failed in Dolphin: DK and Bowser had to jump backwards before up-B to get anywhere they could sweetspot from, and DK drifting in got hit by Marth. The simulation behind them was too kind in two ways:

- **It let the wall catch them.** It stopped anyone who passed ledge height a few units out against Final Destination's wall, from where they slid down into the ledge. In the game, the ECB clips the stage's corner there, and they land on the stage.
- **It guessed the ECBs.** `scripts/fighter_ecb_pose.py` on each `SpecialAirHi` (medians over the move):

  | Fighter | ECB bottom above the feet | Half width |
  | --- | --- | --- |
  | Donkey Kong | 0 | 4.5 |
  | Bowser | 2.0 (0 to 5.5) | 4.7 |
  | Link | 5.8 | 2.2 |
  | Young Link | 4.6 | 2.0 |

  A raised bottom lands them on the stage from that much lower, and they can't grab until it's below the ledge.

The simulation now counts a grab only when the ledge is in the box, they're moving down and the ECB's bottom is below the ledge. It counts as a failure when they pass ledge height over the stage (land), or within half a width plus 2 of the ledge (corner), or reach the stage's side below it (wall). No wall to slide down.

With that, the old spots work for an up-B straight off the platform, but drifting in for 3 to 6 frames overshoots, which is what happened in Dolphin. The spots that sweetspot form a narrow diagonal band, about 10 across, farther out the higher you start, and drifting in carries you across it. So the platform now sits just beyond the band, where falling with the stick toward the stage takes you into it and keeps you there. Dolphin showed the up-Bs go farther than the simulation says, so each spot is picked to still work if they really go 5 or 10 farther:

| Fighter | Spot | Up-B after this many frames of drifting in | If the up-B really goes 10 farther, 5 farther, as simulated, 5 shorter |
| --- | --- | --- | --- |
| Donkey Kong | 105, 35 | 2 to 37 | 41, 40, 36, 25 frames |
| Bowser | 110, 15 | 11 to 35 | 19, 29, 25, 18 |
| Link | 85, 35 | 1 to 57 | 18, 58, 57, 41 |
| Young Link | 100, 35 | 5 to 61 | 30, 58, 53, 38 |

Bowser keeps all the drift he carries into Whirling Fortress, so his window opens only once he has some (about 10 frames). Up-B straight off the platform comes up short. Dropping without drifting comes up short for all four.

## Coming down: where the ledge snaps (T10)

**The problem.** The white X is the middle of the ledge-grab box, so an up-B that ends there grabs. That's right for one that ends level or rising: the box is only checked moving down, so it grabs as it starts to fall, and the middle leaves the most room either way. Coming down, the travel grabs on its way to the X instead, the first frame the box holds the ledge. Aiming at the X then aims lower than the grab, and the ring above the X, at the travel's length from it, is closer in than the farthest you can start from and still grab in the travel.

**The snap.** Moving down, the box holds the ledge once your position is at the top of its range: `ledge_snap_y − ledge_snap_height / 2` below the ledge, from the ledge out to the box's reach (`mpColl_80044164`; the sweep covers the frame's whole drop, so a fast travel can't skip it). The snap is that height, over the X:

| Fighter | X, down from the ledge | Snap | Fighter | X | Snap |
| --- | --- | --- | --- | --- | --- |
| Fox, Falco | 13 | 8.5 (4.5 above the X) | Sheik | 15 | 9.5 |
| Zelda | 10 | 4.5 | Mewtwo | 14 | 8 |
| Pikachu | 11 | 6 | Pichu | 6 | 3 |

The ECB's bottom has to be below the ledge too, and at the snap it is for all seven: posed through their travels, it's at most 3.5 above the position (Zelda's, against her box's 4.5) and Pichu's at most 2.5 (against 3).

**What the guides draw.**

- A **white bar** over the X, as wide as it, at the snap.
- The ring's quarter from straight above the X out to level, its mark straight above, and the B0XX dots for the angles that come down are around the snap. Up-B from one, aim at the snap, and the travel grabs as it ends. Closer grabs sooner. A little farther ends just above the box, and you fall into the grab a frame or two later.
- Level and rising are still around the X. The step between the two is inside the gap beside level, which isn't drawn.
- During the charge, the white line aims at the snap when you launch from above it, else at the X, and Held / Need follows it. The live line is green when, coming down, it crosses the box (worked out against its rectangle), and yellow when it ends just above it.

The X stays the grab spot for the fixed-path up-Bs: they grab as they start to fall from their peak, like a level travel.

## The landing line (T10)

**What it is.** For Fox and Falco, the **Landing Line** setting (and an add-on Gecko code each) draws a pink line for the lowest you can up-B from and still land on the stage. For each angle the stick can aim up toward the stage, and straight across, it's where up-B has to start for the travel, aimed back the other way, to cross the ledge's x just high enough to clear the corner. Start above it and aim just over the ledge, and the travel carries you onto the stage. Below it, the ledge is all that's in reach.

**The corner.** Rising, the travel doesn't land on or grab anything until it's past the ledge, so what matters is that the ECB, a diamond around Fox's position, doesn't touch the corner on the way over. Moving along a straight line `rise` degrees above level, the corner stays clear of the diamond as long as the line passes it by the diamond's extent across the line. The two points that can reach it first are the bottom and the side facing the stage. As the travel crosses the ledge's x, the position has to be at least

    max(−bottom, side_x · tan(rise) − side_y)

above the ledge: the bottom's height for shallow angles, and for steep ones, where the side reaches the wall first, the side's. The farthest start is then the travel's length back along the angle from there. Ending exactly over the ledge's x is enough: the travel's last speed (1.3 a frame for Fox) carries on into SpecialHiFall, inward.

**The ECB during the travel.** `ftFox_SpecialHi_RotateModel` pitches the whole skeleton at XRotN to the travel's angle, so the ECB depends on the angle. Posing SpecialHi with XRotN turned `2π − angle` (a scratch version of `scripts/fighter_ecb_pose.py`), over the travel's last 5 frames:

| Rise | Fox bottom / side x / side y | Falco bottom / side x / side y |
| --- | --- | --- |
| 0 | 2.49 / 4.01 / 4.06 | 2.98 / 4.07 / 5.38 |
| 16.84 | 1.96 / 3.81 / 4.22 | 2.31 / 3.57 / 5.54 |
| 30 | 1.89 / 3.42 / 4.62 | 2.19 / 3.06 / 5.88 |
| 45 | 2.21 / 2.76 / 5.38 | 2.50 / 2.60 / 6.47 |
| 60 | 2.51 / 2.00 / 6.13 | 3.11 / 2.00 / 7.18 |
| 73.16 | 2.86 / 2.00 / 6.79 | 3.42 / 2.00 / 7.62 |

From 60 degrees up the sides are at the game's 2-unit minimum, which is also where they start to decide it. So the guides use two numbers each: bottom 2.2 and side height 6.8 for Fox, 2.5 and 7.6 for Falco (`LandingECB` in the event, `LANDING_ECB` in the script), with the side 2 out. That line is within 0.3 (Fox) and 0.6 (Falco) of what each angle's own ECB gives. Crossing the ledge's x, the position is then 2.2 below the ledge for Fox up to about 60 degrees, rising to 0.2 below at 73.16; Falco 2.5, rising to 1.0.

**Left out, so the line is the safe one.**

- **What's left of your speed after the travel.** It carries on up and in for a few frames, so you might make it from a unit or two lower. That depends on gravity and drift, so the line doesn't count on it.
- **Wallriding.** Aimed into the stage's side below the ledge, Firefox can ride up the wall and over (`ftFx_SpecialAirHi_Coll` doesn't end the travel on a wall; the decompilation marks its match as fake, so the details aren't settled). That depends on each stage's wall and underside (Battlefield's undercut, Final Destination's and Yoshi's Story's slopes), so it isn't drawn. It's on the README's investigate list.
- **Coming down onto the stage.** Starting above the line isn't the lowest, so the line stops at level.

The other aimed up-Bs don't get it yet: their travels don't turn the model the same way, so their ECBs would need their own posing.

## Air up-Bs and the animation numbers

`UpB_Info`'s rise and forward are the animation's whole root motion, and the air versions don't all follow it. Mario, Dr. Mario, Luigi, Marth, Roy and Peach only apply root motion once `cmd_var0` sets. Before that they fall (Mario, Dr. Mario and Peach at 0.5 a frame) and the root motion is lost, and after it the speed is scaled (0.95, or 1.1 for Marth and Roy). Falcon, Ganondorf and Game & Watch apply it from frame 1, unscaled, so their numbers are right.

Simulated from the ISO's attributes and root motion, with no Dolphin check yet:

| Fighter | Table rise, forward | Simulated rise, forward |
| --- | --- | --- |
| Mario | 40.4, 17.6 | 30.3, 16.7 |
| Dr. Mario | 40.4, 17.6 | 35.4, 16.7 |
| Luigi | 45.9, 0 | 41.8, 0 |
| Marth | 47.3, 10.2 | 50.7, 7.7 |
| Roy | 44.5, 12.3 | 43.8, 10.2 |
| Peach | 36.8, 3.4 | 29.0, 3.3 |

If Dolphin agrees, those characters' blue dots, and their Gecko codes, sit up to 10 units from where the up-B really reaches the X. Mario's and Peach's would be too low to reach it. This hasn't been changed.

## Zelda and Sheik

`Fighter_GetGObj` and `Fighter_GetSubcharGObj` read through `isTransformed`, and `Fighter_Transform` swaps it. `Savestate_Load_v1` used to look its fighters up first and restore `isTransformed` afterward, so after a transform each form loaded the other's data, and the event then placed the wrong one. It now restores `isTransformed` for every player before anything looks a fighter up.

An event's reset keeps you in the form you're in: `Savestate_KeepForms` calls `Fighter_Transform` right after the load, before the event places anyone (see [DEVELOPMENT.md](../DEVELOPMENT.md#zelda-and-sheik-t9)). The form is picked on the character select with Z, and holding A through a match load (`Playerblock_Initialize`, 8016D97C) no longer swaps it in events.

## Slippi Gecko codes

[slippi-gecko-codes/up-b-overlay](../slippi-gecko-codes/up-b-overlay/README.md) has one code per character with a guide, and since T10 a B0XX code for each aimed up-B, which draws B0XX Angles' dots, and a Landing Line add-on for Fox and Falco (hooks `8016E754` and `8016E758`). They draw the blue guides, white X and bar with fixed numbers that [scripts/upb_overlay_numbers.py](../scripts/upb_overlay_numbers.py) reads from the ISO. The numbers the ISO can't give directly are the ECB's front, which the event reads live on the platform, and the travel's ECB behind the landing line ([above](#the-landing-line-t10)):

- **How the game sizes it** (`mpColl_LoadECB_JObj`): the box around six joints from `ftData + 0x44`, relative to TopN. If that box is narrower than 10 (the minimum `10 × scale`), it's re-centered at half its width. Each side is then at least 2 out. (The +2 padding only applies when the ledge-grab flag is off. `mpColl_LoadECB` uses 6 and grounded collision 5, which both have it.)
- **Measured** by posing each skeleton in Wait1 (`RebirthWait`'s motion) across its loop, with the melee-decomp-animations tools, at the model scale (common attribute 0x8C) and facing the ledge:

| Fighter | ECB front | Fighter | ECB front |
| --- | --- | --- | --- |
| Mario | 2.15 | Dr. Mario | 2.14 |
| Luigi | 2.36 | Marth | 2.00 |
| Roy | 2.21 | Captain Falcon | 4.07 |
| Ganondorf | 5.67 | Peach | 2.00 |
| Kirby | 2.00 | Samus | 2.03 |
| Fox | 2.00 | Falco | 2.30 |
| Sheik | 3.07 | Zelda | 2.00 |
| Mewtwo | 3.14 | Pikachu | 2.00 |
| Pichu | 2.00 | | |

Most characters are at the 2 minimum. Since the X is half of (ECB front + ledge snap x) out, a unit of difference in the ECB moves the X half a unit.

## Check in Dolphin before building on these numbers

1. Fox and Falco travel ends 81.5 and 60.1 from the launch point (no collision), and the charge drops about 6.
2. A level (0°) Firefox into the ledge doesn't grab during travel, and a slightly downward one does.
3. Sheik's Vanish distance: 40 at full tilt seems short, so check the 36-frame startup and 20-frame travel.
4. Roy's 40° angle range against Marth's 20°.
5. Pikachu's 5-frame first zip (the earlier Edgeguard note estimated about 50 at full tilt, which agrees).
6. The event's angel platform: every character can leave it in every way, the platform shows, and Marth's tipper hits right after you leave.
7. Marth's tipper on frame 13, now right over the ledge with the sword all the way down. If it's too far for a slight overshoot, change `MARTH_FSMASH_FRAME` (the hitboxes are active from frame 10) or `MARTH_TIPPER_INLAND`.
8. The white X (`LedgeTarget`): an up-B ending there should grab the ledge right away.
9. The Ice Climbers: Nana leaves her platform with Popo.
10. The [air rises](#air-up-bs-and-the-animation-numbers): up-B from Mario's blue dot and see if it reaches the X. The yellow line shows the angle you set out at.
11. Marth's down-tilt: that it starts cleanly from Wait, freezes with its hitboxes out, and that the tipper reaches the ledge.
12. Game & Watch's blue and dim dots.
13. The Gecko codes: the six earliest hooks (`8016E7EC` to `8016E80C`, Kirby, Pichu, Roy, Zelda, Mewtwo and Ganondorf) run before StartMelee loads the stage. The overlay only reads the stage when it draws, so that should be fine, but check one of them shows. Each code's X should sit on the event's. The B0XX codes' hooks (`8016E760` to `8016E7D8`) run earlier still, just after Slippi's two at `8016E748` and `8016E74C`.
14. The ring's gaps: during Fox's charge, hold the stick a little off straight across, up or down. Held should stay at 0 or 90, and the live line straight, until the stick is about 16 degrees off.
15. B0XX Angles with a B0XX (or a box on HayBox's Melee mode): up-B from a dot, hold its input, and the live line should end on the X and grab. Mod X with a diagonal should read 23 in Held / Need. For Sheik, check a blue dot with B held through the startup and a dim one without.
16. [Start spots](#start-spots-t10): Donkey Kong, Link and Young Link should sweetspot if they drop holding toward the stage and up-B anywhere in the first 30 frames or so, and Bowser if he up-Bs after 10 to 30 frames of it, without jumping backwards first. Game & Watch on Yoshi's Story starts 35 above the blast zone, and Bowser there 20 inside the side one.
17. [The snap](#coming-down-where-the-ledge-snaps-t10): a Firefox from the ring above the X, aimed at the bar, grabs as the travel ends (the live line green); from a few units farther it ends just over the box and grabs a frame or two later (yellow). The ledge should snap when Fox is about 8.5 below it, not at the X's 13.
18. [The landing line](#the-landing-line-t10): Fox and Falco, from a little above the pink line, aimed just over the ledge, land on the stage without touching its side; from a little below, they hit the corner or the wall. Check a steep angle (Mod Y up) as well as a shallow one, and straight across from the tick. If the line is a couple of units low everywhere, the travel's leftover speed is helping more than counted; if it's off only at steep angles, check `side_y`.
19. The Landing Line codes' hooks, `8016E754` and `8016E758`, run right after Slippi's two and the Randall Timer's (`8016E750`), earlier than any other Up-B code: check one shows.
