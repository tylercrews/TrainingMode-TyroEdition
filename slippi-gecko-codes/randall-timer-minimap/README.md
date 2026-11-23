# Randall Timer Minimap (Slippi Gecko code)

A standalone version of Tyro Edition's **Randall Timer Minimap** for vanilla NTSC 1.02 Melee, including Slippi netplay. On Yoshi's Story, it draws a small map of Randall's loop through the match timer:

- Two **dotted lines** run through the timer's numbers and past both sides of it, joined at each end, so the track is a long loop around the timer.
- A **red arrow** rides the track where Randall is, pointing the way he's moving: right along the bottom line, up the right end, left along the top line and down the left end.
- **The timer is the stage.** The arrow is behind the numbers, and see-through, exactly while Randall is behind the stage, and solid on the track past the timer exactly while he's out on that side.

Randall's loop takes exactly 20 seconds: about 5 seconds behind the stage, then about 5 seconds out on a side. The arrow follows the real Randall every frame, so it never drifts. It's the same drawing as Tyro's own (see [the doc](../../docs/RANDALL-TIMER-MINIMAP.md)).

It only draws, and never writes game state, so it can't cause a desync. Your opponent doesn't need it.

## Install

1. Open Slippi Launcher → Settings → Netplay → Manage Gecko Codes → Add.
2. Use `Randall Timer Minimap (Slippi NTSC 1.02)` as the name. Paste the hex lines from [randall-timer-minimap.txt](randall-timer-minimap.txt) into the code field, then save and enable it. The `$` title and `*` description lines are metadata, not hex code.
3. Keep Slippi's standard required codes enabled and restart the game.

If you installed the first version of this code, which tinted the timer purple and teal, remove it: this one uses the same hook.

It works in any match on Yoshi's Story with a timer, counting up or down. It doesn't show on other stages, in a match with no timer, while the game hides the timer, or in a timed match's last 5 seconds (the game swaps the timer for its big countdown). It hooks `8016E750`, so it can run alongside the [Randall Overlay](../randall-overlay/README.md) (`8016E8C8`), the [Blast Zone Overlay](../blast-zone-overlay/README.md) (`8016E8C0`), the [Double-Jump Height Indicators](../dj-height-indicator/README.md) (`8016E8E0` and `8016E8E8`), the [Hop Peak Lines](../hop-peak-lines/README.md), the [Up-B Overlays](../up-b-overlay/README.md), [Actionable Yellow-Green](../actionable/README.md) and the invincibility codes.

## How it works

- **Hook:** `8016E750` in StartMelee, which runs once per match, creates one GObj with a GX link 8 callback. The replaced instruction is `lis r4, 0x8017`: unconditional, not a branch, and no float register is live across it. Slippi's own codes use `8016E748`, `8016E74C` and `8016E904` in StartMelee, not this one.
- **Where it draws:** the game's HUD camera draws GX links 8, 10 and 11, and no other camera draws 8. The timer's joints are in link 11, so on render pass 0 the map draws first and the numbers cover it. Depth testing is off.
- **Where Randall is:** each frame on Yoshi's Story, the callback looks up map GObj 2 and reads the world position of the cloud joint. The game puts `GrSt.dat`'s tree under a scale joint and gives the GObj that one, so the cloud is the first child of its child (the file's root, which never moves). That joint rides a path animation that loops every 1200 frames. If map GObj 2 is gone (a code that removes stage hazards), the arrow goes where a match has Randall at the timer's time: the frames the timer shows as run, plus 125, around his loop at his constant speed. It reads the match clock (`stc_match`'s `time_seconds` and `time_ms`, and the timer's mode and start for a countdown).
- **The timer:** `stc_matchhud->timer_gobj` (`804A1078`) is the timer's GObj, or 0 with no timer (the game also clears it when it swaps in the last 5 seconds' countdown). The callback reads the world position of each of its ten joints that's showing and adds half its quad's width, which gives the timer's edges, and reads its height from its root.
- **The map:** the stage between its walls at Randall's heights (X ±52.675) becomes the timer's width, with the same scale past them, so the arrow's time behind the numbers and past each side is Randall's time behind the stage and out. Randall's two heights become lines 0.6 above and below the timer's middle. Each half of the loop gets the same even number of dots, about every 0.8 HUD units, at the same fractions of each half, so both ends look the same: a 2-pixel light dot on a 4-pixel dark one. The arrow points along the path segment nearest Randall.

The source is [Randall Timer Minimap.asm](Randall%20Timer%20Minimap.asm). This folder sits outside `ASM/`, so the Tyro ISO build doesn't include it; Tyro has its own C version (`RandallTimer_GX` in `src/stage_overlay_game.c`). The code's square roots are the game's own `sqrtf` (`8000D5BC`), done inline.

## Rebuild the text code

With devkitPPC installed and `DEVKITPPC` set, from the repository root:

```sh
python -I scripts/build_randall_timer_minimap_gecko.py
```

It assembles the code with `bin/hgecko` into `build/randall-timer-minimap.gct`, then writes `randall-timer-minimap.txt`.

## Tests

[tests/test_randall_timer_minimap_gecko.py](../../tests/test_randall_timer_minimap_gecko.py) runs the text code in a PowerPC emulator, installed the way the Gecko handler installs it, and records what it sends the GPU. It checks that:

- The hook creates one GObj in GX link 8, restores every register (`r0`, the link register and CR included) and still runs `lis r4, 0x8017`. The draw keeps every saved integer and float register.
- It draws the dark dots, the light dots, then the arrow's outline and fill, with depth testing off.
- The map puts the stage's walls on the timer's edges and Randall's heights on the two lines, follows the timer when it moves or an hour widens it, and caps the dots for a huge timer.
- The dots run evenly around the loop, the same on both ends, and dim behind the numbers. The arrow points the way Randall moves and is see-through exactly between the walls.
- With no Randall, the arrow is where the timer puts him, counting up or down.
- It draws nothing on other stages, with no timer or a hidden one, with no joint showing, without a real position for the cloud, or on render passes other than 0.
- With devkitPPC and `build/Start.dol`, it builds Tyro's own version and checks the code draws the same vertices, sizes and colors all around Randall's path, by the timer without Randall, and (with `melee.iso`) on every frame of his real lap.

```sh
python tests/test_randall_timer_minimap_gecko.py -v
```

[tests/test_randall_timer.py](../../tests/test_randall_timer.py) checks the research against `melee.iso`: Randall's path and the walls in `GrSt.dat`, the timer's joints and the HUD camera in `IfAll.usd`, and where the game keeps the timer.

## Known limits

- **Untested in Dolphin and on Slippi.** It passes the emulator tests, but hasn't been run in the real game yet.
- **Possible hook conflict.** Another custom code that also injects at `8016E750` would conflict, including this code's first version.
