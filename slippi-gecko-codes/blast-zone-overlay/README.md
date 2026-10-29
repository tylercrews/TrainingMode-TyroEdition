# Blast Zone Overlay (Slippi Gecko code)

A standalone version of Tyro Edition's **Blast Zone Overlay** for vanilla NTSC 1.02 Melee, including Slippi netplay. On every stage it draws a crisp red line exactly on the blast zones, with a red glow that fades out over a short distance past them. The line is where the game checks for KOs.

It only draws: it reads the stage's blast zones and never changes game state, so it can't cause a desync. Your opponent doesn't need it.

## Install

1. Open Slippi Launcher → Settings → Netplay → Manage Gecko Codes → Add.
2. Use `Blast Zone Overlay (Slippi NTSC 1.02)` as the name. Paste the hex lines from [blast-zone-overlay.txt](blast-zone-overlay.txt) into the code field, then save and enable it. The `$` title and `*` description lines are metadata, not hex code.
3. Keep Slippi's standard required codes enabled and restart the game.

The camera rarely shows the blast zones during normal play. You'll see the line when the camera zooms out to follow someone far offstage.

It hooks `8016E8C0`, so it can run alongside the [Randall Overlay](../randall-overlay/README.md) (`8016E8C8`), the [Double-Jump Height Indicators](../dj-height-indicator/README.md) (`8016E8E0` and `8016E8E8`) and [Actionable Yellow-Green](../actionable/README.md) (`800805C8` and `8006D9EC`).

## How it works

- **Hook:** `8016E8C0` in StartMelee, which runs once per match, creates one GObj with a GX link 5 draw callback. The replaced instruction is `li r3, 255`. Slippi's own codes use `8016E748`, `8016E74C` and `8016E904` in StartMelee, not this one.
- **Blast zones:** each frame the callback reads the stage's blast zones from `stc_stage` (`8049E6C8`, offsets `0x74..0x80`) and adds the stage's camera offset (`0x10` and `0x14`). That's exactly what the game's `Stage_GetBlastzoneLeft/Right/Top/Bottom` return, and what its KO check (`Player_CheckForDeath`) compares each character's position against. Reading them every frame means the overlay follows any stage that moves them.
- **Drawing:** four bands, red on the line and clear 24 units out, mitered at the corners. Then the line itself, 3 pixels wide in opaque red, and a 3-pixel point on each corner to fill the notch GX leaves where two wide lines meet. Depth testing is off through `HSD_StateSetZMode` (the same way as the Randall Overlay), so nothing hides it.

The source is [Blast Zone Overlay.asm](Blast%20Zone%20Overlay.asm). This folder sits outside `ASM/`, so the Tyro ISO build doesn't include it; Tyro has its own C version (`src/stage_overlay_game.c`).

## Rebuild the text code

With devkitPPC installed and `DEVKITPPC` set, from the repository root:

```sh
python -I scripts/build_blast_zone_gecko.py
```

It assembles the code with `bin/hgecko` into `build/blast-zone-overlay.gct`, then writes `blast-zone-overlay.txt`.

## Tests

[tests/test_blast_zone_gecko.py](../../tests/test_blast_zone_gecko.py) runs the text code in a PowerPC emulator, installed the way the Gecko handler installs it. It checks that:

- The hook creates one GObj, restores every register and still runs `li r3, 255`.
- The glow, line and corners land on the blast zones, including with a camera offset.
- Empty or inverted blast zones and other render passes draw nothing.
- When `melee.iso` is at the repo root, the game's four getters are exactly the sum the code computes, and the KO check calls them.

The game functions are stubbed and the GX writes are recorded, so it checks what the code asks to draw, not the picture.

```sh
python tests/test_blast_zone_gecko.py -v
```

## Known limits

- **Untested in Dolphin and on Slippi.** It passes the emulator tests, but hasn't been run in the real game yet.
- **Possible hook conflict.** Another custom code that also injects at `8016E8C0` would conflict.
