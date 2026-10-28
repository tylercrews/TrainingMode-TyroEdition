# Double-Jump Height Indicator (Slippi Gecko code)

A standalone version of the **Double-Jump Height Indicator** from Tyro Edition's Side-B Sweetspot event, for vanilla NTSC 1.02 Melee, including Slippi netplay. On the six legal stages it draws a short line under each ledge for every Fox and Falco in the match. Double jump with your feet on the line, side-B at the top of the jump, and a full-length side-B sweetspots the ledge.

It only draws: it reads the stage's ledges and which characters are playing, and never changes game state, so it can't cause a desync. Your opponent doesn't need it.

## Reading the line

- **Color:** orange for Fox, sky blue for Falco. If both are in the match, both lines show.
- **Height:** 53.0 under the ledge for Fox and 54.8 for Falco. That's the double jump's rise (40.2 for Fox, 41.8 for Falco) plus 13, so at the top of the jump the middle of your ledge-grab box is level with the ledge. The box is 9 tall, so you have 4.5 of slack either way. Fox's line is 1/6 higher because he drops that much during side-B startup.
- **Middle:** where a full side-B (no second B press to shorten it) just reaches the ledge: 83.74 out for Fox and 77.08 for Falco. This is the white **0** dot of the event's range guide.
- **Width:** how far the double jump can drift either way before its peak, with the stick held all the way: 15.9 for Fox (19 frames to the peak) and 18.5 for Falco (22 frames). From anywhere on the line, you can drift to the middle by the peak.

## Install

1. Open Slippi Launcher → Settings → Netplay → Manage Gecko Codes → Add.
2. Use `Double-Jump Height Indicator (Slippi NTSC 1.02)` as the name. Paste the hex lines from [dj-height-indicator.txt](dj-height-indicator.txt) into the code field, then save and enable it. The `$` title and `*` description lines are metadata, not hex code.
3. Keep Slippi's standard required codes enabled and restart the game.

On other stages it does nothing. It can run alongside the [Randall Overlay](../randall-overlay/README.md) (hook `8016E8C8`) and [Actionable Yellow-Green](../actionable/README.md) (hooks `800805C8` and `8006D9EC`); this one hooks `8016E8E0`.

## Where it draws

From [scripts/sweetspot_dj_platforms.py](../../scripts/sweetspot_dj_platforms.py), which reads the stages and characters from a vanilla ISO. Every stage is symmetric, so each row covers both sides (left side negative).

| Stage | Ledges (x, y) | Fox line (x from..to, y) | Falco line (x from..to, y) |
| --- | --- | --- | --- |
| Fountain of Dreams | ±63.35, 0.62 | ±131.23..162.95, -52.41 | ±121.92..158.94, -54.15 |
| Pokémon Stadium | ±87.75, 0.00 | ±155.63..187.35, -53.04 | ±146.32..183.34, -54.78 |
| Yoshi's Story | ±56.00, -3.50 | ±123.88..155.60, -56.54 | ±114.57..151.59, -58.28 |
| Dream Land | ±77.27, 0.01 | ±145.15..176.87, -53.03 | ±135.84..172.86, -54.77 |
| Battlefield | ±68.40, 0.00 | ±136.28..168.00, -53.04 | ±126.97..163.99, -54.78 |
| Final Destination | ±85.57, 0.00 | ±153.45..185.17, -53.04 | ±144.14..181.16, -54.78 |

Every line is inside the blast zones and far above the bottom ones. The tightest is Yoshi's Story, where the side blast zones are about 20 past the outer end of Fox's line.

## How it works

- **Hook:** `8016E8E0` (StartMelee) runs once per match and creates one GObj with a GX link 5 draw callback. The replaced instruction is `mr r30, r3`. Slippi's own codes use `8016E748`, `8016E74C` and `8016E904` in StartMelee, not this one.
- **Ledges:** each frame it calls the game's `Stage_GetLeftOfLineCoordinates` and `Stage_GetRightOfLineCoordinates` with the line IDs the Tyro event uses. The game walks to the end of the floor and returns the live ledge, already scaled for stages like Battlefield and Yoshi's Story.
- **Characters:** it walks the fighter GObj list and draws Fox's lines if any fighter is Fox, and Falco's if any is Falco.
- **Drawing:** two lines per character, with depth testing off through `HSD_StateSetZMode` (the same way as the Randall Overlay), so nothing hides them.
- **Numbers:** the three per-character numbers are fixed vanilla values, printed by the script. The Tyro event works them out from the live character data instead.

The source is [Double-Jump Height Indicator.asm](Double-Jump%20Height%20Indicator.asm). This folder sits outside `ASM/`, so the Tyro ISO build doesn't include it.

## Rebuild the text code

Run from the repository root in devkitPro's MSYS2 shell:

```sh
./bin/hgecko.exe slippi-gecko-codes/dj-height-indicator build/dj-height-indicator.gct
```

Then convert the `.gct` to text lines: drop the 8-byte `00D0C0DE00D0C0DE` header and the `F000000000000000` footer, and print the rest as `XXXXXXXX XXXXXXXX` pairs under the `$Double-Jump Height Indicator (Slippi NTSC 1.02)` name line.

## Tests

[tests/test_dj_height_gecko.py](../../tests/test_dj_height_gecko.py) runs the code from `dj-height-indicator.txt` in a PowerPC emulator, installed the way the Gecko handler installs it. It checks that match start creates the GObj and restores every register, and that each legal stage draws the right lines for Fox, Falco, both or neither. It also checks that other stages and render passes draw nothing, and, when `melee.iso` is at the repo root, that the fixed numbers match the ISO. The game functions are stubbed and the GX writes are recorded, so it checks what the code asks to draw, not the picture.

```sh
python tests/test_dj_height_gecko.py -v
```

## Known limits

- **Untested in Dolphin and on Slippi.** It passes the emulator tests, but hasn't been run in the real game yet.
- **Full side-B only.** A side-B shortened with a second B press goes less far, so start closer than the line for those.
- **Startup drift isn't counted.** If you're still drifting when you press side-B, the startup carries you a little farther toward where you were drifting. The event's range guide ignores this too.
- **Vanilla only.** The numbers are fixed, so a mod that changes Fox's or Falco's jumps or side-B would need them rebuilt.
- **Possible hook conflict.** Another custom code that also injects at `8016E8E0` would conflict.
