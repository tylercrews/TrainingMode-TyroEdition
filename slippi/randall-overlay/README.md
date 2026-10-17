# Randall Overlay (Slippi Gecko code)

A standalone version of Tyro Edition's **Randall Overlay** for vanilla NTSC 1.02 Melee, including Slippi netplay. On Yoshi's Story it draws Randall's platform as a bright line, with a translucent cloud outline under it. Both stay visible while Randall passes behind the stage.

It only draws: it reads the stage's collision data and never changes game state, so it can't cause a desync. Your opponent doesn't need it.

## Install (Slippi Dolphin)

1. In Slippi Dolphin, right-click your Melee ISO and choose **Properties**, then the **Gecko Codes** tab.
2. Click **Add New Code**, set the name to `Randall Overlay`, and paste every line of [randall-overlay.txt](randall-overlay.txt) **except** the first `$Randall Overlay [TM Tyro]` line into the code box.
3. Tick the new code's checkbox and close the window.

On other stages it does nothing.

## How it works

- **Hook:** `8016e8c8` (StartMelee) runs once per match and creates one GObj with a GX link 5 draw callback. The replaced instruction is `lwz r12, 0x44(r31)`.
- **Finding Randall:** each frame on Yoshi's Story, the callback looks up map GObj 2. It finds the collision group whose joint is Randall's root joint or one of its descendants.
- **Drawing:** it draws that group's floor lines, and three ellipses sized to the group's vertex bounds. Depth testing is disabled through `HSD_StateSetZMode`, so HSD's state cache stays consistent.
- **Fallback:** if no collision group matches, it draws a circle at the cloud's root joint.

The source is [Randall Overlay.asm](Randall%20Overlay.asm). This folder sits outside `ASM/`, so the Tyro ISO build doesn't include it; Tyro has its own C version (`src/stage_overlay_game.c`).

## Rebuild the text code

Run from the repository root in devkitPro's MSYS2 shell:

```sh
./bin/hgecko.exe slippi/randall-overlay build/randall-overlay.gct
```

Then convert the `.gct` to text lines: drop the 8-byte `00D0C0DE00D0C0DE` header and the `F000000000000000` footer, and print the rest as `XXXXXXXX XXXXXXXX` pairs under the `$Randall Overlay [TM Tyro]` name line.

## Known limits

- **Untested on Slippi.** It was built against the vanilla DOL, but hasn't been run there yet.
- **Possible hook conflict.** Another code that also injects at `8016e8c8` would conflict. Slippi's own codes are not known to use this address.
