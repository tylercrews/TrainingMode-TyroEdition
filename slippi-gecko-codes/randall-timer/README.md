# Randall Timer (Slippi Gecko code)

A standalone version of Tyro Edition's **Randall Timer** for vanilla NTSC 1.02 Melee, including Slippi netplay. On Yoshi's Story, the match timer turns **purple** while Randall is out past the left side of the stage and **teal** while it's out past the right. While Randall is behind the stage, the timer is its normal color.

Randall's loop takes exactly 20 seconds and starts with the match, so in a normal match each side's turn lands at the same times on the clock every game. Watching the colors against the numbers is a quick way to learn them. Randall is out about 5 seconds a side, with about 5 seconds behind the stage between them.

It only recolors the timer, which the game never reads back, so it can't cause a desync. Your opponent doesn't need it.

## Install

1. Open Slippi Launcher → Settings → Netplay → Manage Gecko Codes → Add.
2. Use `Randall Timer (Slippi NTSC 1.02)` as the name. Paste the hex lines from [randall-timer.txt](randall-timer.txt) into the code field, then save and enable it. The `$` title and `*` description lines are metadata, not hex code.
3. Keep Slippi's standard required codes enabled and restart the game.

On other stages it does nothing, and it does nothing in a match with no timer. It hooks `8016E750`, so it can run alongside the [Randall Overlay](../randall-overlay/README.md) (`8016E8C8`), the [Blast Zone Overlay](../blast-zone-overlay/README.md) (`8016E8C0`), the [Double-Jump Height Indicators](../dj-height-indicator/README.md) (`8016E8E0` and `8016E8E8`), the [Up-B Overlays](../up-b-overlay/README.md), [Actionable Yellow-Green](../actionable/README.md) and the invincibility codes.

## How it works

- **Hook:** `8016E750` in StartMelee, which runs once per match, creates one GObj with a GX link 5 callback and forgets the last match's timer. The replaced instruction is `lis r4, 0x8017`: unconditional, not a branch, and no float register is live across it. Slippi's own codes use `8016E748`, `8016E74C` and `8016E904` in StartMelee, not this one.
- **Where Randall is:** each frame on Yoshi's Story, the callback looks up map GObj 2 and reads the world position of its root's first child, the cloud joint. That joint rides a path animation that loops every 1200 frames, counter-clockwise: on the right, Randall leaves the stage's wall low, swings out, rises and slides back in high, and the left side is the mirror of that. The stage's walls at both of Randall's heights are at X ±75.25 in `GrSt.dat`, which the stage's scale (0.7) puts at **±52.675**. Randall is out while the cloud is past that.
- **The timer:** `stc_matchhud->timer_gobj` (`804A1078`) is the timer's GObj, or 0 with no timer. Its digits are textures multiplied by each material's constant diffuse color, and nothing animates that color, so a new color stays until something changes it. Before the first tint, the code saves each of the timer's materials (10 in vanilla) with its own color. When Randall's side changes, it sets every material to its own color times the tint, channel by channel (the gray separators stay a shade darker than the digits), or back to its own color. Alpha is unchanged.
- **Colors:** `A064FF` purple and `00B4A0` teal, Training Mode's palette Purple and Teal.

The source is [Randall Timer.asm](Randall%20Timer.asm). This folder sits outside `ASM/`, so the Tyro ISO build doesn't include it; Tyro has its own C version (`src/stage_overlay_game.c`), and both follow the same rules.

## Rebuild the text code

With devkitPPC installed and `DEVKITPPC` set, from the repository root:

```sh
python -I scripts/build_randall_timer_gecko.py
```

It assembles the code with `bin/hgecko` into `build/randall-timer.gct`, then writes `randall-timer.txt`.

## Tests

[tests/test_randall_timer_gecko.py](../../tests/test_randall_timer_gecko.py) runs the text code in a PowerPC emulator, installed the way the Gecko handler installs it. It checks that:

- The hook creates one GObj, restores every register (`r0`, the link register and CR included) and still runs `lis r4, 0x8017`.
- Past -52.675 the timer is purple, past 52.675 teal, and in between untinted. White digits become exactly the tint, and the gray separators keep their shading and alpha.
- It writes only when the side changes, and puts the timer's own colors back when Randall goes behind the stage or is gone (hazards off).
- On other stages, with no timer, or on render passes other than the last, it does nothing.
- A new timer, or a new match with a timer at the same address, gets its own colors saved again.
- Its threshold and colors are the same numbers as the C version's.
- With `build/Start.dol`, the hook replaces exactly that instruction, and the game's own lookup turns Yoshi's Story's stage ID into internal stage 10.

[tests/test_randall_timer.py](../../tests/test_randall_timer.py) checks the research against `melee.iso`: Randall's path and the walls in `GrSt.dat`, and the timer's materials in `IfAll.usd`.

```sh
python tests/test_randall_timer_gecko.py -v
```

## Known limits

- **Untested in Dolphin and on Slippi.** It passes the emulator tests, but hasn't been run in the real game yet.
- **Possible hook conflict.** Another custom code that also injects at `8016E750` would conflict.
- **Another code coloring the timer.** A code that also sets the timer's material colors would fight with this one while Randall is out.
