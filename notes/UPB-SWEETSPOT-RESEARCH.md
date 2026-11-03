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
- **Marth:** freeze his forward smash on its first active frame (frame 10), the way Side-B freezes the down-tilt, with only the tipper hitbox on, and place him from where that hitbox actually is at runtime.
- **Stages:** limiting to the six legal stages is cheap: keep the stage select and restart on Battlefield if the stage isn't legal ([below](#stages)). There's no need to cut it down to Battlefield and Final Destination.

## How the game decides a ledge grab

From `mpcoll.c` (`mpColl_80044164` and the air collision that calls it) and `ftcliffcommon.c`:

- **Box.** Relative to the fighter's position (TopN), facing the ledge: horizontally from the fighter out to `ecb.right + ledge_snap_x`, vertically `ledge_snap_y ± ledge_snap_height / 2`, swept over the frame's movement. The ledge must be inside it, and the ECB's bottom (the feet) must be below the ledge. TM already reads these as `coll_data.cliffgrab_width`, `cliffgrab_y_offset` and `cliffgrab_height` (Side-B's `LedgeGrabTop` uses them).
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
| Falcon | Falcon Dive | Rises 37.6 by frame 44, 11.9 forward | Drift during the dive | After the frame-13 flag |
| Ganondorf | Dark Dive | Rises 41.9 by frame 44, 13.2 forward | Drift | Same as Falcon |
| Peach | Parasol | Rises 36.8 by frame 33, 3.4 forward | Drift | Falling only |
| Kirby | Final Cutter | 23-frame windup, then rises 50.6 by frame 17 of the rise and drops | Little | Several of its states |

**Physics rise:** simulated from attributes.

| Fighter | Up-B | Path | Notes |
| --- | --- | --- | --- |
| Samus | Screw Attack | About 48 (2.5 up against 0.066 gravity) | Drift. Grabs only falling. Tether is a separate move. |
| Donkey Kong | Spinning Kong | Barely rises in the air (0.675 up, gravity 0.07); 1.4 sideways | Grabs either direction |
| Bowser | Whirling Fortress | Small rise, mostly sideways | |
| Link, Young Link | Spin Attack | Small rise (`specialhi_grav_mul`) | Grabs either direction. Tether is a separate move. |
| Game & Watch | Fire | A jump the stick can angle at the start: probably up to ±30° (`0x5C` = 0.52 rad) past a 0.25 stick (`0x58`). The decomp is unsure what both attributes do. | Grabs facing |
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

- **Before up-B: the sweetspot ring.** An arc of radius D (the travel distance) around T, drawn only offstage. Up-B anywhere on it, aimed at T, and the travel ends with the ledge in the box: the fastest grab.
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
- **Freezing.** Side-B already freezes Marth's down-tilt on its first active frame with `FrameSpeedChange(cpu, 0)` (its tipper is also hitbox 3). Do the same with forward smash on frame 10, and turn hitboxes 0 to 2 off so any hit is the tipper.
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

- **Start:** an angel platform 40 out and 15 above the ledge, placed the way Ledgedash places its Respawn Platform start. Unlike Ledgedash, the game's own `RebirthWait` interrupt still decides when you leave, so any action works, up-B included.
    - That exit grants respawn invincibility (`ftColl_8007B7A4`). The event sets the remaining frames to 1 once you're off, so the game ends it itself, flashing included.
- **Up-B detection:** each character's up-B action-state range, from the decompilation's motion-state enums (clones share their original's). Yoshi and Jigglypuff are left off the CSS.
- **Guides:**
    - Aimed up-Bs and fixed paths are drawn as described above. The physics group (DK, Bowser, Link, Young Link, Ness, Game & Watch, Ice Climbers) only gets the ledge target.
    - The aimed lines lock to the travel's real velocity on its first frame, not to the stick.
- **Marth:** forward smash starts when your up-B does and freezes on frame 10 with all four hitboxes out. Placement isn't computed ahead of time: the first freeze reads hitbox 3's position and slides Marth (and his hitboxes) so the tipper is right over the ledge, then later resets use that measured reach.
    - The first Dolphin test had only the tipper on, 12 inland. It was too far back, and the whole forward smash was wanted.
- **The white X** uses the ECB side facing the ledge (`ecb.right` facing right, `-ecb.left` facing left), like `mpColl_800443C4` and its mirror. The first version used `ecb.right` on both ledges.
- **Settings:** Enable Line Guides, Marth and Camera are saved to the memory card. The success rate is session-only, carried through Change Stage with `carry_store`.
- **Stages:** the first-frame guard from [Stages](#stages), option 1.

## Slippi Gecko codes

[slippi-gecko-codes/up-b-overlay](../slippi-gecko-codes/up-b-overlay/README.md) has one code per character with a guide. They draw the blue guides and white X with fixed numbers that [scripts/upb_overlay_numbers.py](../scripts/upb_overlay_numbers.py) reads from the ISO. The only number the ISO can't give directly is the ECB's front, which the event reads live on the platform:

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
7. Marth's tipper on frame 10, now right over the ledge with the whole forward smash out. If it's too high or low for a slight overshoot, change `MARTH_TIPPER_FRAME` (the hitboxes are active through frame 13) or `MARTH_TIPPER_INLAND`.
8. The white X (`LedgeTarget`): an up-B ending there should grab the ledge right away.
9. The Ice Climbers: Nana leaves her platform with Popo.
10. The Gecko codes: the six earliest hooks (`8016E7EC` to `8016E80C`, Kirby, Pichu, Roy, Zelda, Mewtwo and Ganondorf) run before StartMelee loads the stage. The overlay only reads the stage when it draws, so that should be fine, but check one of them shows. Each code's X should sit on the event's.
