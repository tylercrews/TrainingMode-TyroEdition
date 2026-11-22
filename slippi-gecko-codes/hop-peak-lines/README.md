# Hop Peak Lines (Slippi Gecko codes)

Standalone versions of the **Hop Peak Lines** from Tyro Edition's Target Practice event, for vanilla NTSC 1.02 Melee, including Slippi netplay. There's one code for Fox and one for Falco. Each one draws its character's lines on the six legal stages while that character is in the match (any port). Enable the one for the character you play, or both.

Over the main floor and each platform, a code draws two lines: **orange** where the highest laser a short hop can shoot flies, and **teal** where a full hop's does. That's the laser shot at the jump's peak (for Falco's short hop, as near it as a laser gets). A laser shot on the way up or down goes lower.

They only draw: they read the stage's collision lines and which characters are playing, and never change game state, so they can't cause a desync. Your opponent doesn't need them.

## Reading the lines

| | Short hop: B on, from | Full hop: B on, from | Laser over the feet | **Orange (short hop)** | **Teal (full hop)** |
| --- | --- | --- | --- | --- | --- |
| Fox | frame 2, 10.65 | frame 8, 31.28 | 9.28 | **19.93** | **40.56** |
| Falco | frame 2, 11.44 | frame 14, 51.5 | 3.80 | **15.24** | **55.30** |

The heights are above the floor you jump from. "B on" is the airborne frame you press B on for that laser (the first airborne frame is 1), and "from" is how high you are on the frame before it fires, where the shot comes from. A jump off the ground skips gravity on its first frame, so it rises its whole jump speed then, unless you press B on that frame: the laser's physics takes over and applies gravity at once, and the hop peaks lower. That's why B on frame 1 can't reach the line, and why Falco's short hop line is 0.14 under his peak, which only B on frame 1 could shoot from.

- **The main floor's and top platform's lines** go all the way across, from blast zone to blast zone.
- **A side platform's lines** go from its side's blast zone to the middle of the stage and stop. Each side's lines sit over that side, so you can compare them: Dream Land's right platform is 0.1 higher than its left, and its lines are that much higher too.
- **Fountain of Dreams:** the side platforms' lines follow them up and down. When a platform sinks into the stage, its lines go until it comes back up.
- **Pokémon Stadium:** while the stage is transformed, its two platforms are gone, and so are their lines.
- Final Destination has only the main floor's two lines.

## Install

1. Open Slippi Launcher → Settings → Netplay → Manage Gecko Codes → Add.
2. For Fox, use `Fox Hop Peak Lines (Slippi NTSC 1.02)` as the name and paste the hex lines from [fox-hop-peak-lines.txt](fox-hop-peak-lines.txt). For Falco, use `Falco Hop Peak Lines (Slippi NTSC 1.02)` and [falco-hop-peak-lines.txt](falco-hop-peak-lines.txt). Save and enable it. The `$` title and `*` description lines are metadata, not hex code.
3. Keep Slippi's standard required codes enabled and restart the game.

On other stages they do nothing. The Fox code hooks `8016E740` and the Falco code `8016E930`, so both can be on at once, alongside every other code in this folder's parent: the [Up-B Overlays](../up-b-overlay/README.md), the [Double-Jump Height Indicators](../dj-height-indicator/README.md) (`8016E8E0` and `8016E8E8`), the [Randall Overlay](../randall-overlay/README.md) (`8016E8C8`), the [Randall Timer Minimap](../randall-timer-minimap/README.md) (`8016E750`), the [Blast Zone Overlay](../blast-zone-overlay/README.md) (`8016E8C0`), [Actionable Yellow-Green](../actionable/README.md) and the invincibility codes.

## How it works

- **Hooks:** each code replaces one instruction in StartMelee, which runs once per match, and creates its own GObj with a GX link 5 draw callback. Every other unconditional, non-branch instruction in StartMelee with no float register live across it is taken by the codes above or by Slippi's own (`8016E748`, `8016E74C` and `8016E904`), and Training Mode's frame counter uses `8016E744`. That leaves `8016E740` (`mr r31,r3`, just after the prologue) for Fox and the function's last instruction, `8016E930` (`blr`), for Falco. The Falco code ends with that `blr` itself, so it returns from StartMelee as the game would. Each hook keeps every register the native code might still need, `r0`, the link register and CR included, then runs the instruction it replaced.
- **Who's playing:** each frame the callback asks the game for each port's active fighter (`Player_GetEntity`) and draws only if one of them is its character.
- **Floors:** the main floor's height at the middle is a fixed number for each stage. Each platform is a fixed collision line ID. Every frame the code asks the game whether the line is on (`GrColl_CheckIfLineEnabled`, which Stadium's transformations turn off), then reads where its two ends are right now. That's how Fountain's moving platforms are followed. A line spanning the middle (the top platform) draws across, and one on one side draws from that side's blast zone to the middle. One less than 1 above the main floor is sunk (a hidden Fountain platform sits 0.375 up, inside the stage) and gets no lines.
- **Blast zones:** the game's `Stage_GetBlastzoneLeft` and `Stage_GetBlastzoneRight`.
- **Drawing:** one batch of 2-pixel lines, Training Mode's palette Orange (`FF9628`) and Teal (`00B4A0`) at alpha 230. Depth testing is off through `HSD_StateSetZMode`, the same way as the Randall Overlay, so nothing hides them.
- **Numbers:** [scripts/hop_peak_lines.py](../../scripts/hop_peak_lines.py) reads them from a vanilla ISO: each jump's highest laser, walking the jump from the character's jump speed and gravity for every B press (`laser_heights.peak_laser`: a jump off the ground skips gravity on its first frame unless B comes on it, and gravity comes before the move after that), and the aerial laser's height from the character's skeleton, the same way Target Practice does ([scripts/laser_heights.py](../../scripts/laser_heights.py)). It also finds each stage's platform lines and main floor height. The Tyro event works the heights out from the live character instead.

| Stage | Main floor (y at x = 0) | Platform lines |
| --- | --- | --- |
| Fountain of Dreams | 0.0028 | 0, 1, 2 (they move) |
| Pokémon Stadium | 0 | 35, 36 (base layout) |
| Yoshi's Story | 0 | 1, 4, 5 (not Randall's) |
| Dream Land | 0.0088 | 0, 1, 2 |
| Battlefield | 0 | 2, 3, 4 |
| Final Destination | 0 | none |

The sources are [Fox Hop Peak Lines.asm](Fox%20Hop%20Peak%20Lines.asm) and [Falco Hop Peak Lines.asm](Falco%20Hop%20Peak%20Lines.asm), which the script writes. Each holds only its character's numbers, the stage table and its hook, and includes the shared drawing code in [hop-peak-lines.inc](hop-peak-lines.inc). This folder sits outside `ASM/`, so the Tyro ISO build doesn't include it.

## Rebuild the text codes

With devkitPPC installed and `DEVKITPPC` set, from the repository root:

```sh
python -I scripts/hop_peak_lines.py melee.iso --write   # only when the numbers change
python -I scripts/build_hop_peak_lines_gecko.py
```

The first rewrites the `.asm` files from the ISO. The second assembles them with `bin/hgecko` into `build/hop-peak-lines.gct`, then writes `fox-hop-peak-lines.txt` and `falco-hop-peak-lines.txt`.

## Tests

[tests/test_hop_peak_lines_gecko.py](../../tests/test_hop_peak_lines_gecko.py) runs both text codes in a PowerPC emulator, installed together the way the Gecko handler installs them, with each stage's collision lines in memory where the game keeps them. It checks that:

- Each hook is free and creates its own GObj, then leaves every register as the instruction it replaced would, and Falco's still returns from StartMelee.
- Each legal stage draws two lines per floor at the right heights and colors, worked out by hand for Fox on Battlefield and Falco on Final Destination. The main floor's and top platform's lines reach the live blast zones, and the side platforms' stop at the middle, Dream Land's included.
- Fountain's lines follow its side platforms and go while one is sunk, and a platform whose line is off has none.
- A code draws nothing until its character is in the match, and nothing on other stages (where it never asks about a line, since the game's check hangs on one the stage doesn't have) or render passes.
- The line width, colors and sunk height are Target Practice's (its default Orange and Teal; the event also darkens the main floor's lines and lightens the top platform's, and lets you pick the colors, which the codes don't).
- When `melee.iso` is at the repo root, the `.asm` files match what the script gives, the heights are Target Practice's, the platform lines are the stages', and each hook replaces exactly the instruction the vanilla DOL has there.

The game functions are stubbed and the GX writes are recorded, so it checks what the codes ask to draw, not the picture.

```sh
python tests/test_hop_peak_lines_gecko.py -v
```

## Known limits

- **Untested in Dolphin and on Slippi.** They pass the emulator tests, but haven't been run in the real game yet.
- **The peak, not the whole jump.** A line is the highest laser that jump can shoot. Lower targets take a laser shot earlier or later in the jump.
- **Vanilla only.** The numbers and line IDs are fixed, so a mod that changes Fox's or Falco's jumps or laser, or the legal stages' collision, would need them rebuilt.
- **Possible hook conflict.** Another custom code that also injects at `8016E740` or `8016E930` would conflict.
