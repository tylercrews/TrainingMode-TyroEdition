# Low Laser Puff Practice

Select **Character-specific Tech → Low Laser Puff Practice** (the eighth event). Only Fox and Falco are available. The event uses Final Destination and a fixed Jigglypuff CPU, with percent HUDs enabled.

Puff holds frame 2 of crouch startup (`Squat`), with animation speed zero and its animation/interrupt callbacks disabled. It never enters idle crouch (`SquatWait`). Its normal hurtboxes and native percent accumulation remain enabled; knockback, pushing, CPU inputs and grabs are disabled. Position and velocity are held without restoring a fighter savestate or resetting its damage after hits. The pose uses the game's [native startup crouch state](https://github.com/doldecomp/melee/blob/master/src/melee/ft/kinds/ftCommon/ftCo_Squat.c).

The crouch transition passes `-1` for animation blending so the startup pose is applied immediately, followed by resolving the joint matrices. This happens only on placement or restore. The ongoing position lock runs through Puff's native physics callback at priority 4, before Melee's collision and model passes. The event's later priority-16 callback measures timing without rewriting or invalidating Puff's visual transforms. Previously it invalidated those transforms on every active update; opening the menu stopped that late work, which matched the user's report that Puff appeared correctly only while paused or after repositioning. Disabling animation blending alone did not resolve that reported issue.

The pause menu contains:

- **Puff Position:** Center, Far Left, Center Left, Center Right, Far Right. These use X = 0, -70, -35, 35 and 70 on FD's main floor. The player starts on the opposite side, facing Puff. Repositioning preserves Puff's percent.
- **Event OSDs:** defaults **On**, saved in memory-card settings.
- **Suppress Global OSDs:** defaults **Off**, saved independently. This hides global OSDs only within this event and preserves their saved global choices.
- **Exit:** return to event selection and save pending settings.
- **Reset %:** reset only Puff's damage and percent HUD to zero.

The two white timing readouts form a centered pair at the same height and scale as Under Fire's timing displays:

- **Laser Start:** the first aerial neutral-B startup of the jump. Frame 1 is the first airborne frame; jumpsquat, hitlag and paused updates do not add frames. A new jump clears both results; landing leaves the last results visible.
- **Fastfall:** the regular Fastfall OSD measurement, read from the same `TM.can_fastfall_frames` counter used by `Display FF Timing.asm`. It counts eligible fastfall frames, so 1 means the first possible fastfall frame. It does not count from takeoff. Repeated updates during the same fastfall retain its activation result.

Neither readout applies timing-result colors or the global OSD palette. Both hide during the event pause menu. Event OSDs and global suppression work independently.

## Persistence and validation

The two toggles use bits 20–21 of the existing extension word at bytes 12–14. `TYEX` version 6 remains 16 bytes and migrates versions 1–5, initializing only the newly owned fields and retaining Under Fire's score-format marker. The event is unscored; its catalogue entry does not claim a legacy score or the preference region.

`tests/test_lowlaser.py` executes the production PowerPC event and its optimized relocated DAT. It covers the startup pose and callback locks, position changes, retained damage, Reset %, airborne timing, frozen/paused clocks, independent saved toggles, neutral colors, restricted character selection and score isolation. Update-order regressions run physics/model preparation before the late event callback and check that it leaves the prepared matrices valid during play, pause/resume and damage. Native DOL tests verify blend-argument semantics and the regular fastfall routine with its existing counter hook. Settings tests cover migration, fresh-service reload, field isolation and native score/reset boundaries. These tests do not render GPU output; visual gameplay validation of the update-order correction remains outstanding. UI checks stay disabled at the user's request.
