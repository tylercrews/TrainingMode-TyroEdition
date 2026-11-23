# Training Lab Fountain of Dreams platforms

On Fountain of Dreams, the Training Lab pause menu's Stage Options has **Left Platform Height** and **Right Platform Height**. Each one picks:

- **Random:** the platform moves as it does in a match.
- **Hidden:** the platform is sunk, the same as when the game hides it.
- **12.375 to 27.375**, in steps of 0.375: the platform stays at that height. The number is the Y you stand at on it, the same Y that Info Display shows.

The heights are the game's own range, so you can't pick a height the game never uses. The left platform starts at 16.125, the right one at 22.125, and both come back up at 19.875 after sinking. All three are on the list, and so are the lowest (12.375) and highest (27.375).

Changing a row moves the platform right away, so you can see it behind the paused menu. The choices last for the session only: they start at Random each match, Reset puts them back, and Change Stage doesn't carry them.

Going back to Random hands the platform to the game where it is. About 4 seconds (255 frames) later, the game decides what to do next as in a match: move, sink or wait. A hidden platform comes back up instead.

## How the game moves them

Each side platform is a map gobj 4 whose think is `Stages_FoDPlatformThink` (801CC358). `Stages_Fountain_SidePlatformSpawn` (801CCBDC) creates the left one first, on line group 0, and the right one on line group 1. The fountain (map gobj 3, 801CBE64) gives them its joints 1 and 2.

| Ground data | Use |
| --- | --- |
| +0x04, +0x14 | Back pointer to the gobj, and map ID 4 |
| +0xC4 | State: 0 pick a wait, 1 wait, 2 move to the target, 3 sunk, 4 hidden |
| +0xC6 | Timer |
| +0xC8 | Line group: 0 left, 1 right |
| +0xCC | The platform's joint in the fountain model |
| +0xD0, +0xD4 | Height and target, in joint units |
| +0xD8 | Height of the water column under the platform at scale 1 |
| +0xDC | Height it comes back up to |

The numbers come from GrIz.dat's `yakumono_param`:

| Offset | Value | Use |
| --- | --- | --- |
| 0x00, 0x08 | 20, 28 | Left and right starting heights |
| 0x0C | 25 | Height both come back up to |
| 0x18–0x1C | 5–10 | Size of a move |
| 0x20, 0x24 | 35, 15 | Highest and lowest target. Every move is clamped to these. |
| 0x28, 0x2C | 0.15, 0.1 | Rise and fall per frame |
| 0x30, 0x34 | 0.25, 0.25 | Chance a move goes away from 25, from below and from above |
| 0x38–0x3C | 600–1080 | Frames to wait |
| 0x40, 0x44, 0x48 | 4, 10, 8 | Weights for sink, move and wait again |
| 0x4C–0x50 | 480–1680 | Frames to stay hidden |

When it waits out the timer, the platform sinks, moves or waits again. Sinking aims at −1. Once the platform is below 15, the next frame hides the water column (`JOBJ_HIDDEN`, with its animations removed) and leaves the joint 1 below the column's root. Coming back up clears the flag, restarts the column's animation (`Stage_SetMapJOBJAnim(gobj, 0, 7, 0, 0, 1)`) and rises to 25. Whenever the height changes, the column scales with it: Y = max(height / column height, 0.01) and X = 0.5 × Y + 0.5. Every frame, the think also updates its line group's collision from the joint.

### Joint height to stage units

The map gobj creator (`Ground_801C14D0`) puts each map model under a root scaled by `grGroundParam` (0.75), and the platform line is at Y 1.5 in its joint. A joint at height h puts the surface at **0.75 × (h + 1.5)**, so 15 to 35 becomes 12.375 to 27.375. The top platform's joint is fixed at 55.5, which works out the same way to the top platform's known height of 42.75.

## How the Lab holds them

[src/fountain.c](../src/fountain.c) runs from `Event_PostThink` every frame, after the stage's think, and again when a row changes. It finds both side platforms on the map p_link by their map ID and back pointer. It doesn't rely on list order.

For a held height, it writes the state the game would be in if it were waiting at that height: state 1, height and target both set to that height. It also sets the joint and the water column's scale the way the game's own update does. The timer is reset to 255 every frame, so the wait never runs out. For Hidden, it writes the game's hidden state instead (state 4 at −1) and hides the column and stops its animation once. Picking a height while the platform is hidden brings it back up the same way the game does. The game's think keeps running the whole time, so collision follows the joint as usual. Random writes nothing.

The earlier version, from Community Edition, had seven presets: Random, Hidden, Lowest, Left Default, Average, Right Default and Highest. It only set the joint. The water column kept its old size, and Hidden put the joint at −3.25 instead of using the game's hidden state.

## Validation

`python tests/test_fountain.py` runs the production code against the DOL's `Stages_FoDPlatformThink`, with GrIz.dat's real parameters from `melee.iso`. Joints, the GObj list, the collision update and the animation calls are fixtures. It checks that:

- The choices are the game's clamp range and include both starting heights and the height they come back up to. The labels are the surface heights worked out from GrIz.dat's ground scale and platform line.
- A held platform stays put for 3000 frames while the game's think keeps updating its collision. Its water column is the size the game draws at that height.
- Hidden matches the game's own sink (state, joint, flags and column) and stops the animation only once.
- A height picked while the platform is hidden brings it back up with the same flag change and animation call the game uses.
- Random leaves the game's data byte-for-byte alone. Switching to Random from a held height or from Hidden picks up without a jump.
- Other stages, a map-4 lookalike, unknown line groups and out-of-range choices are left alone.

Still to check in Dolphin:

- Standing on a held platform, Info Display's Y matches the menu (for example 16.125 on the left platform's start).
- The water column looks right at 12.375 and 27.375, and Hidden looks like the game's own sink.
- Changing a row while paused moves the platform behind the menu.
- Switching from a height or Hidden back to Random, the platform moves or comes back up about 4 seconds later.
