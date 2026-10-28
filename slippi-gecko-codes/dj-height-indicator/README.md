# Double-Jump Height Indicator (Slippi Gecko codes)

Standalone versions of the **Double-Jump Height Indicator** from Tyro Edition's Side-B Sweetspot event, for vanilla NTSC 1.02 Melee, including Slippi netplay. There is one code for Fox and one for Falco. Each one draws its character's marks on the six legal stages, whoever is playing. Enable the one for the character you play, or both.

Under each ledge, a code draws a short line and an X at ledge height above the line's middle. The line is the lowest your feet can fall before your double jump can no longer get you back up to sweetspot. Double jump before you drop below it, drift so you peak at the X, and side-B there: a full-length side-B sweetspots the ledge.

They only draw: they read the stage's ledges and never change game state, so they can't cause a desync. Your opponent doesn't need them.

## Reading the marks

- **The line** is the lowest your feet can be when you double jump and still sweetspot: 57.5 under the ledge for Fox and 59.3 for Falco. That's the double jump's rise (40.2 for Fox, 41.8 for Falco) plus 17.5, the top of your ledge-grab box above your feet. Double jump from the line and the top of the box peaks level with the ledge, just enough to grab it. In the event, that's the blue guide line meeting the white one. Double jump from higher and you have more room. Fox's line is 1/6 higher because he drops that much during side-B startup.
- **The X** is at ledge height, where a full side-B (no second B press to shorten it) just reaches the ledge: 83.74 out for Fox and 77.08 for Falco. This is the white **0** dot of the event's range guide. Peak on the X and side-B.
- **The line's width** is two thirds of how far the double jump can drift either way before its peak: ±10.6 for Fox (the most is 15.9, in 19 frames) and ±12.3 for Falco (18.5, in 22 frames). From anywhere on the line you can drift under the X by the peak without holding the stick all the way.
- **Color:** orange for Fox, sky blue for Falco.

## Install

1. Open Slippi Launcher → Settings → Netplay → Manage Gecko Codes → Add.
2. For Fox, use `Fox Double-Jump Height Indicator (Slippi NTSC 1.02)` as the name and paste the hex lines from [fox-dj-height-indicator.txt](fox-dj-height-indicator.txt). For Falco, use `Falco Double-Jump Height Indicator (Slippi NTSC 1.02)` and [falco-dj-height-indicator.txt](falco-dj-height-indicator.txt). Save and enable it. The `$` title and `*` description lines are metadata, not hex code.
3. Keep Slippi's standard required codes enabled and restart the game.

On other stages they do nothing. The Fox code hooks `8016E8E0` and the Falco code `8016E8E8`, so both can be on at once, alongside the [Randall Overlay](../randall-overlay/README.md) (`8016E8C8`) and [Actionable Yellow-Green](../actionable/README.md) (`800805C8` and `8006D9EC`).

## Where they draw

From [scripts/sweetspot_dj_platforms.py](../../scripts/sweetspot_dj_platforms.py), which reads the stages and characters from a vanilla ISO. Every stage is symmetric, so each row covers both sides (left side negative).

| Stage | Ledges (x, y) | Fox line (x from..to, y) | Fox X | Falco line (x from..to, y) | Falco X |
| --- | --- | --- | --- | --- | --- |
| Fountain of Dreams | ±63.35, 0.62 | ±136.51..157.66, -56.91 | ±147.09, 0.62 | ±128.09..152.77, -58.65 | ±140.43, 0.62 |
| Pokémon Stadium | ±87.75, 0.00 | ±160.92..182.06, -57.54 | ±171.49, 0.00 | ±152.49..177.17, -59.28 | ±164.83, 0.00 |
| Yoshi's Story | ±56.00, -3.50 | ±129.17..150.31, -61.04 | ±139.74, -3.50 | ±120.74..145.42, -62.78 | ±133.08, -3.50 |
| Dream Land | ±77.27, 0.01 | ±150.44..171.58, -57.53 | ±161.01, 0.01 | ±142.01..166.69, -59.27 | ±154.35, 0.01 |
| Battlefield | ±68.40, 0.00 | ±141.57..162.71, -57.54 | ±152.14, 0.00 | ±133.14..157.82, -59.28 | ±145.48, 0.00 |
| Final Destination | ±85.57, 0.00 | ±158.73..179.88, -57.54 | ±169.31, 0.00 | ±150.31..174.99, -59.28 | ±162.65, 0.00 |

Everything is inside the blast zones. The tightest is Yoshi's Story: its side blast zones are more than 20 past the outer end of Fox's line, and its bottom one (-91) is about 28 under Falco's.

## How it works

- **Hooks:** each code replaces one StartMelee instruction, which runs once per match: Fox `8016E8E0` (`mr r30, r3`) and Falco `8016E8E8` (`cmplwi r30, 0`). Each creates its own GObj with a GX link 5 draw callback. Slippi's own codes use `8016E748`, `8016E74C` and `8016E904` in StartMelee, not these.
- **Ledges:** each frame the callback calls the game's `Stage_GetLeftOfLineCoordinates` and `Stage_GetRightOfLineCoordinates` with the line IDs the Tyro event uses. The game walks to the end of the floor and returns the live ledge, already scaled for stages like Battlefield and Yoshi's Story.
- **Drawing:** the line and X under both ledges, as 12 line vertices, with depth testing off through `HSD_StateSetZMode` (the same way as the Randall Overlay), so nothing hides them.
- **Numbers:** each character's full side-B, line width and depth are fixed vanilla values, printed by the script. The Tyro event works them out from the live character data instead.

The sources are [Fox Double-Jump Height Indicator.asm](Fox%20Double-Jump%20Height%20Indicator.asm) and [Falco Double-Jump Height Indicator.asm](Falco%20Double-Jump%20Height%20Indicator.asm). Each holds only its character's numbers and hook, and includes the shared drawing code in [dj-height-indicator.inc](dj-height-indicator.inc). This folder sits outside `ASM/`, so the Tyro ISO build doesn't include it.

## Rebuild the text codes

With devkitPPC installed and `DEVKITPPC` set, from the repository root:

```sh
python -I scripts/build_dj_height_gecko.py
```

It assembles both codes with `bin/hgecko` into `build/dj-height-indicator.gct`, then writes `fox-dj-height-indicator.txt` and `falco-dj-height-indicator.txt`.

## Tests

[tests/test_dj_height_gecko.py](../../tests/test_dj_height_gecko.py) runs both text codes in a PowerPC emulator, installed together the way the Gecko handler installs them. It checks that:

- Each hook creates its own GObj and restores every register, and Falco's still sets the flags its replaced compare sets.
- Each legal stage draws the right line and X under both ledges.
- Other stages and render passes draw nothing.
- When `melee.iso` is at the repo root, the fixed numbers match the ISO.

The game functions are stubbed and the GX writes are recorded, so it checks what the codes ask to draw, not the picture.

```sh
python tests/test_dj_height_gecko.py -v
```

## Known limits

- **Untested in Dolphin and on Slippi.** They pass the emulator tests, but haven't been run in the real game yet.
- **Full side-B only.** A side-B shortened with a second B press goes less far, so start closer than the X for those.
- **Startup drift isn't counted.** If you're still drifting when you press side-B, the startup carries you a little farther toward where you were drifting. The event's range guide ignores this too.
- **Vanilla only.** The numbers are fixed, so a mod that changes Fox's or Falco's jumps or side-B would need them rebuilt.
- **Possible hook conflict.** Another custom code that also injects at `8016E8E0` or `8016E8E8` would conflict.
