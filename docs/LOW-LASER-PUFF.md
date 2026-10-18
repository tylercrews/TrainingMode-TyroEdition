# Low Laser Puff Practice

Select **Character-specific Tech → Low Laser Puff Practice** (the eighth event). Every character is available. The event opens the stage select and is designed for the six legal stages: Final Destination, Battlefield, Dream Land, Yoshi's Story, Fountain of Dreams and Pokémon Stadium. The CPU is always a frozen Jigglypuff, and percent HUDs are enabled. Hazards are disabled as in other events, so Stadium stays in its base layout and Randall and Dream Land's wind are removed. Fountain's platforms still move.

Grounded Puff holds frame 2 of crouch startup (`Squat`), with animation speed zero and its animation/interrupt callbacks disabled. It never enters idle crouch (`SquatWait`). Its normal hurtboxes and native percent accumulation remain enabled; knockback, pushing, CPU inputs and grabs are disabled. Position and velocity are held without restoring a fighter savestate or resetting its damage after hits. The pose uses the game's [native startup crouch state](https://github.com/doldecomp/melee/blob/master/src/melee/ft/kinds/ftCommon/ftCo_Squat.c).

The crouch transition passes `-1` for animation blending so the startup pose is applied immediately, followed by resolving the joint matrices. This happens only on placement or restore. The ongoing position lock runs through Puff's native physics callback at priority 4, before Melee's collision and model passes. The event's later priority-16 callback measures timing without rewriting or invalidating Puff's visual transforms. Previously it invalidated those transforms on every active update; opening the menu stopped that late work, which matched the user's report that Puff appeared correctly only while paused or after repositioning. Disabling animation blending alone did not resolve that reported issue.

The pause menu contains:

- **Puff Position:** Center, Far Left, Center Left, Center Right, Far Right, On Left Plat, On Top Plat, On Right Plat, Under Left Plat, Under Right Plat. Saved on the memory card. Repositioning preserves Puff's percent.
- **Puff Height:** Grounded (default), Short Hop, Full Hop, Short Hop + Double Jump, Full Hop + Double Jump. Saved on the memory card.
- **Change Stage:** restart on Final Destination, Battlefield, Dream Land, Yoshi's Story, Fountain of Dreams or Pokémon Stadium without going back through the character and stage select. The current stage is greyed out. The saved settings carry over; Puff's percent resets with the new match. This uses the shared restart described in [notes/MID-EVENT-CHARACTER-STAGE-SWAP.md](../notes/MID-EVENT-CHARACTER-STAGE-SWAP.md).
- **Event OSDs:** defaults **On**, saved in memory-card settings.
- **Suppress Global OSDs:** defaults **Off**, saved independently. This hides global OSDs only within this event and preserves their saved global choices.
- **Exit:** return to event selection and save pending settings.
- **Reset %:** reset only Puff's damage and percent HUD to zero.

## Positions

Static reference coordinates for every legal stage's platforms are in [notes/LEGAL-STAGE-PLATFORMS.md](../notes/LEGAL-STAGE-PLATFORMS.md). Positions come from the stage's live collision lines: the current vertices of enabled floor lines, the same approach Techchase's Starting Location uses. Nothing is a fixed per-stage coordinate.

- **Ground positions** scale FD's original X values (0, ±35, ±70) by each side's main-floor edge divided by FD's edge (85.5657). FD therefore keeps its exact old coordinates. Puff stands on the highest solid floor at that X.
- **On Left/Top/Right Plat** places Puff at the center of the matching one-way platform (native platform flag `0x0100`). Left and right are the side platforms nearest center (center beyond ±10). Top is the highest platform spanning X = 0. Platform positions are offered only on the six legal stages.
- **Under Left/Right Plat** places Puff on the main floor below that platform's center.
- The player starts on the main floor on the other side of Puff, facing it: X = +50 or −50, scaled the same way. The player goes to the right whenever Puff is left of X = −1.

Choices a stage lacks show as **(N/A)**. For example, FD has no platforms and Stadium has no top platform. The selector skips these in the direction you were scrolling. If the saved position isn't available on the current stage, the event uses Center for that match but leaves the saved value unchanged, so it returns on a stage that has that platform.

On a platform, Puff follows the platform's live line every physics frame, so it rides Fountain's moving platforms. If that line is disabled or no longer under Puff, Puff uses the main floor below until the platform returns. If you change a menu option while the platform is gone, Puff is re-placed and falls back to Center.

## Heights

Puff Height lifts Puff straight up from the chosen position's surface. A raised Puff stays above a moving platform as the platform moves. Each height is a jump's apex rise. It is computed from Puff's live attributes using the native order, where gravity is applied before each frame's movement: `rise(v) = Σ (v − k·g)` for k = 1, 2, … while that term is positive. The double-jump velocity comes from the first entry of Jigglypuff's multi-jump table (special attributes `+0x14`), because its generic aerial-jump multiplier is 0. With the ISO values (gravity 0.064, short hop 1.05, full hop 1.6, first aerial jump 1.65), the heights are:

| Height | Units above the surface |
| --- | --- |
| Short Hop | 8.096 |
| Full Hop | 19.2 |
| Short Hop + Double Jump | 28.546 |
| Full Hop + Double Jump | 39.65 |

A raised Puff is airborne in a frozen frame-0 `Fall` pose, not a crouch. Its collision callback is a no-op, so it never lands or grabs a ledge while it's held. The event uses a no-op instead of NULL because one native caller doesn't null-check that callback. Grounded Puff restores the native Squat callbacks. Raised Puffs below a platform can overlap it; the position is still exact.

## Timing readouts

The two white timing readouts form a centered pair at the same height and scale as Under Fire's timing displays:

- **Laser Start** (Fox and Falco) / **Neutral-B Start** (everyone else): the first aerial neutral-B startup of the jump. Frame 1 is the first airborne frame; jumpsquat, hitlag and paused updates do not add frames. A new jump clears both results; landing leaves the last results visible.
  - Fox and Falco use their shared `SpecialAirNStart` state (344), unchanged from earlier versions.
  - Other characters count the frame they enter a character-specific state (341+) whose native move ID is neutral-B (18). Native `ActionStateChange` copies the new state's move ID into `atk_kind` (0x2068) through `0x800890D0`, but skips that write for non-attack states (ID 1). The value therefore lingers after a move, so only a state transition counts.
- **Fastfall:** the regular Fastfall OSD measurement, read from the same `TM.can_fastfall_frames` counter used by `Display FF Timing.asm`. It counts eligible fastfall frames, so 1 means the first possible fastfall frame. It does not count from takeoff. Repeated updates during the same fastfall retain its activation result.

Neither readout applies timing-result colors or the global OSD palette. Both hide during the event pause menu. Event OSDs and global suppression work independently.

## Persistence and validation

Event OSDs and Suppress Global OSDs use bits 20–21 of the extension word at bytes 12–14, introduced in `TYEX` version 6. Puff Position (bits 0–3) and Puff Height (bits 4–6) use byte 35, the last byte of the version-15 record. Every migration to version 15 already zeroed that byte, so existing saves read as Center and Grounded, and no version bump was needed. Reads of older, unmigrated records return the defaults without looking at byte 35. Out-of-range values repair to the defaults. The color-nibble size check now stops at byte 34. The event is unscored; its catalogue entry does not claim a legacy score or the preference region.

`tests/test_lowlaser.py` executes the production PowerPC event and its optimized relocated DAT. It covers:

- the startup pose and callback locks, position changes, retained damage and Reset %
- airborne timing, frozen/paused clocks, independent saved toggles and neutral colors
- all-character and stage-select catalogue entries, and score isolation
- Change Stage: the current stage greyed out and labelled, each row's stage ID, and OSD suppression released before the restart

`LowLaserStageTests` loads each legal stage's real collision lines from the ISO (Stadium's base group, Yoshi's without Randall) and checks every position: Puff's surface point, the player's side, ground height and facing, the availability masks and the saved value. FD keeps its exact old coordinates. Other tests cover direction-aware skipping and (N/A) labels, falling back from a saved missing platform without overwriting the save, every height on every Battlefield position (state, airborne flag and no-op collision included), and Fountain-style moving and disappearing platforms. The neutral-B test covers grounded shots, a lingering `atk_kind`, up-B and the no-transition case.

Update-order regressions run physics/model preparation before the late event callback and check that it leaves the prepared matrices valid during play, pause/resume and damage. Native DOL tests verify blend-argument semantics and the regular fastfall routine with its existing counter hook. Settings tests cover migration, byte-35 ownership and repair, older records ignoring byte 35, fresh-service and relocated-DAT reloads, field isolation and native score/reset boundaries.

These tests do not render GPU output or run Fountain's real platform animation. Still to check in Dolphin: Puff's look in the frozen Fall pose, Fountain riding, and the stage-select flow. UI checks stay disabled at the user's request.
