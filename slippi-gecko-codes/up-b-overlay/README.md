# Up-B Overlay (Slippi Gecko codes)

Standalone versions of the line guides from Tyro Edition's **Up-B Sweetspot** event, for vanilla NTSC 1.02 Melee, including Slippi netplay. There is one code for each of the 17 characters the event has a guide for, and a **B0XX** code for each of the 7 that aim their up-B, which draws the event's **B0XX Angles** instead. Each draws its character's targets at both ledges on the six legal stages, while that character is in the match (any port). Enable the ones for the characters you play, the regular code or the B0XX one. All 24 can be on at once, and each stays hidden until its character is in the match, but a character's two codes draw over each other.

They only draw: they read the stage's ledges and which characters are playing, and never change game state, so they can't cause a desync. Your opponent doesn't need them.

## Reading the marks

All of them are measured from the **white X**: where you are when the ledge sits in the middle of your ledge-grab box. Get there at the end of your up-B and you grab the ledge.

- **Aimed up-Bs** (Fox, Falco, Sheik, Zelda, Mewtwo, Pikachu, Pichu): a **blue ring** around the X at the up-B's travel distance. Up-B from on the ring and aim at the X. The ring is the offstage half, from straight above the X to straight below it, except for Fox's and Falco's, which carry on under the stage to 2/3 of a circle, because they can also recover from under it. It stops 30 degrees short of level under the stage, so it doesn't stick up into it, and leaves out the quarter over the stage. Everything draws over the stage.
  - **Gaps:** the ring leaves out the 16.8 degrees either side of straight across, up and down. Melee reads a stick axis at or under 0.28 as 0, so near straight reads exactly straight, and the shallowest angle off it is 23/80 of the stick across with 76/80 along. No stick or controller can aim into a gap. A **white tick** on each straight direction marks where holding straight sweetspots, with 4 degrees of the ring either side running through it at the right distance.
  - The teleports and Quick Attack go farther the more you tilt the stick, so they also get a **dim edge** at the least tilt that still aims each way. That's the 0.5 threshold, curving out to meet the ring at the gaps, where only a near-full tilt reads off straight. Up-B from between the two and tilt the stick less the closer you are. Their white marks run from the dim edge to the ring, since straight works at any tilt that aims, with 4 degrees of each through their ends.
  - Fox and Falco go the same distance at any tilt, so they only get the one ring.
- **B0XX codes** (the same seven): the white X, and a **dot** where each of the B0XX's up-B angles sweetspots, instead of the ring. Up-B from a dot and hold its input. **White** dots have no modifier: straight, or a diagonal. **Blue** ones are Mod X or Mod Y on a diagonal, alone or with a C-button, which step 4.4 degrees at a time from 23 to 67, plus Mod X or Mod Y straight. The teleports and Quick Attack go less far without B held, so their blue dots are with B (the B0XX's extended up-B angles), and they also get a **dim** dot for each without B, closer in. The coordinates are the B0XX V3's, as [HayBox's Melee mode](https://github.com/JonnyHaystack/HayBox/blob/master/src/modes/Melee20Button.cpp) has them.
- **Fixed paths** (Mario, Dr. Mario, Luigi, Marth, Roy, Captain Falcon, Ganondorf, Peach, Kirby, Samus): a **blue dot** where to up-B from for the fastest grab, at the top of the up-B, and a line up from it to where it still grabs, more slowly, on the way back down.
  - Mario, Dr. Mario, Marth and Roy also get a **dim dot and line** for holding the stick toward the stage during the up-B.

What the event shows on top of this, and these codes leave out: the line out from each ledge, the lines for the angle you're holding and the angle you need, the Held / Need row, and the yellow X where your up-B started.

## The codes

Every point is relative to the ledge, so the marks are the same on all six stages. "Out" is away from the stage, and "down" is below the ledge.

| Character | Code | Hook | White X | Guide |
| --- | --- | --- | --- | --- |
| Fox | [fox-up-b-overlay.txt](fox-up-b-overlay.txt) | `8016E898` | 6.50 out, 13 down | 2/3 ring 81.5, gaps, ticks |
| Falco | [falco-up-b-overlay.txt](falco-up-b-overlay.txt) | `8016E89C` | 6.65 out, 13 down | 2/3 ring 60.1, gaps, ticks |
| Marth | [marth-up-b-overlay.txt](marth-up-b-overlay.txt) | `8016E8A0` | 7.00 out, 17 down | dot 17.2 out, 64.3 down; dim (20°) 32.8 out, 58.0 down |
| Sheik | [sheik-up-b-overlay.txt](sheik-up-b-overlay.txt) | `8016E8A4` | 7.04 out, 15 down | ring 40.0, least 30.0, gaps |
| Captain Falcon | [falcon-up-b-overlay.txt](falcon-up-b-overlay.txt) | `8016E8CC` | 6.54 out, 17 down | dot 18.4 out, 54.6 down |
| Peach | [peach-up-b-overlay.txt](peach-up-b-overlay.txt) | `8016E908` | 6.00 out, 15 down | dot 9.4 out, 51.8 down |
| Pikachu | [pikachu-up-b-overlay.txt](pikachu-up-b-overlay.txt) | `8016E91C` | 5.50 out, 11 down | ring 49.5, least 39.5, gaps |
| Samus | [samus-up-b-overlay.txt](samus-up-b-overlay.txt) | `8016E920` | 6.01 out, 10 down | dot 6.0 out, 57.3 down |
| Luigi | [luigi-up-b-overlay.txt](luigi-up-b-overlay.txt) | `8016E924` | 7.18 out, 15 down | dot 7.2 out, 60.9 down |
| Mario | [mario-up-b-overlay.txt](mario-up-b-overlay.txt) | `8016E928` | 7.08 out, 15 down | dot 24.7 out, 55.4 down; dim (18°) 36.3 out, 48.0 down |
| Dr. Mario | [drmario-up-b-overlay.txt](drmario-up-b-overlay.txt) | `8016E92C` | 7.07 out, 15 down | dot 24.7 out, 55.4 down; dim (18°) 36.3 out, 48.0 down |
| Ganondorf | [ganondorf-up-b-overlay.txt](ganondorf-up-b-overlay.txt) | `8016E80C` | 7.33 out, 17 down | dot 20.5 out, 58.9 down |
| Mewtwo | [mewtwo-up-b-overlay.txt](mewtwo-up-b-overlay.txt) | `8016E804` | 7.07 out, 14 down | ring 50.0, least 40.0, gaps |
| Zelda | [zelda-up-b-overlay.txt](zelda-up-b-overlay.txt) | `8016E800` | 6.50 out, 10 down | ring 84.0, least 62.0, gaps |
| Roy | [roy-up-b-overlay.txt](roy-up-b-overlay.txt) | `8016E7F8` | 7.11 out, 17 down | dot 19.4 out, 61.5 down; dim (40°) 45.1 out, 43.2 down |
| Pichu | [pichu-up-b-overlay.txt](pichu-up-b-overlay.txt) | `8016E7F4` | 5.50 out, 6 down | ring 35.2, least 26.4, gaps |
| Kirby | [kirby-up-b-overlay.txt](kirby-up-b-overlay.txt) | `8016E7EC` | 6.00 out, 8.5 down | dot 6.0 out, 60.7 down |
| Fox B0XX | [fox-b0xx-up-b-overlay.txt](fox-b0xx-up-b-overlay.txt) | `8016E7D8` | 6.50 out, 13 down | 34 dots |
| Falco B0XX | [falco-b0xx-up-b-overlay.txt](falco-b0xx-up-b-overlay.txt) | `8016E7D0` | 6.65 out, 13 down | 34 dots |
| Sheik B0XX | [sheik-b0xx-up-b-overlay.txt](sheik-b0xx-up-b-overlay.txt) | `8016E7BC` | 7.04 out, 15 down | 46 dots |
| Zelda B0XX | [zelda-b0xx-up-b-overlay.txt](zelda-b0xx-up-b-overlay.txt) | `8016E7B0` | 6.50 out, 10 down | 50 dots |
| Mewtwo B0XX | [mewtwo-b0xx-up-b-overlay.txt](mewtwo-b0xx-up-b-overlay.txt) | `8016E76C` | 7.07 out, 14 down | 46 dots |
| Pikachu B0XX | [pikachu-b0xx-up-b-overlay.txt](pikachu-b0xx-up-b-overlay.txt) | `8016E764` | 5.50 out, 11 down | 46 dots |
| Pichu B0XX | [pichu-b0xx-up-b-overlay.txt](pichu-b0xx-up-b-overlay.txt) | `8016E760` | 5.50 out, 6 down | 46 dots |

There are no codes for Donkey Kong, Bowser, Link, Young Link, Ness, Game & Watch or the Ice Climbers, because the event has no guide for them yet. Yoshi and Jigglypuff don't recover with up-B. The B0XX codes' dot counts are after merging spots under a unit apart: holding B moves Fox's and Falco's angles under half a degree, so they get one dot per angle.

## Install

1. Open Slippi Launcher → Settings → Netplay → Manage Gecko Codes → Add.
2. Use the code's first line, without the `$`, as the name (for example `Fox Up-B Overlay (Slippi NTSC 1.02)` or `Fox Up-B Overlay B0XX (Slippi NTSC 1.02)`), and paste the hex lines from its text file into the code field. Save and enable it. The `$` title and `*` description lines are metadata, not hex code. Repeat for each character you want.
3. Keep Slippi's standard required codes enabled and restart the game.

On other stages they do nothing. Every code has its own hook, so they run alongside each other and alongside the other codes in this folder's parent: the [Randall Overlay](../randall-overlay/README.md) (`8016E8C8`), the [Blast Zone Overlay](../blast-zone-overlay/README.md) (`8016E8C0`), the [Double-Jump Height Indicators](../dj-height-indicator/README.md) (`8016E8E0` and `8016E8E8`), [Actionable Yellow-Green](../actionable/README.md) and the [Invincibility Overlay](../invincibility-overlay/README.md).

## How it works

- **Hooks:** each code replaces one instruction in StartMelee, which runs once per match, and creates its own GObj with a GX link 5 draw callback. The 24 hooks are unconditional instructions that aren't branches, with no float register live across them: 11 near the end of StartMelee and 13 before it loads the stage, the B0XX codes' 7 earliest (`8016E760` to `8016E7D8`). Slippi's own codes use `8016E748`, `8016E74C` and `8016E904`, not these. The hook keeps every register the native code might still need, including `r0` and the link register, then runs the instruction it replaced.
- **Who's playing:** each frame the callback asks the game for each port's active fighter (`Player_GetEntity`, which follows Zelda and Sheik as they transform) and draws only if one of them is its character.
- **Ledges:** it calls the game's `Stage_GetLeftOfLineCoordinates` and `Stage_GetRightOfLineCoordinates` with the line IDs the Tyro events use. The game walks to the end of the floor and returns the live ledge, already scaled for stages like Battlefield and Yoshi's Story.
- **Drawing:** each code holds its shapes as a list of points for the right ledge, (out, up), and mirrors them for the left one. Depth testing is off through `HSD_StateSetZMode` (the same way as the Randall Overlay), so nothing hides them. Line widths, dot size and colors are the event's.
- **Numbers:** [scripts/upb_overlay_numbers.py](../../scripts/upb_overlay_numbers.py) reads them from a vanilla ISO, the same way the event reads the live character data:
  - **White X:** out by half of (the ECB's front + the ledge-grab box's reach), down by the box's height above the feet. The event reads the ECB live while you stand on the platform. The codes use a fixed value, from posing each character's skeleton in its standing animation the way the game sizes the ECB. For most characters that's the game's 2-unit minimum, so the X lands within about a unit of the event's.
  - **Rings:** the up-B's travel distance from its special attributes. `UNDER_STAGE` in the script lists who gets the 2/3 ring: Fox and Falco, as in the event.
  - **Gaps:** from the stick deadzones in PlCo.dat (`ftLoadCommonData` 0x0 and 0x4), the way the game reads the stick: in 80ths inside a circle of radius 80, with an axis at or under its deadzone read as 0. The dim edge is the least tilt with both axes past their deadzones and past the up-B's threshold.
  - **B0XX dots:** the B0XX's sticks (`B0XX_PLAIN`, `B0XX_MOD_B` and `B0XX_MOD` in the script) run through each up-B's own rule, as the event does: Fox and Falco go straight up below 0.5 of |x| + |y|, the rest below a 0.5 tilt, and the teleports' distance grows with the tilt.
  - **Dots:** the up-B's root motion, the same table as the event's.

The sources are one `.asm` per character, which the script writes, and the shared drawing code in [up-b-overlay.inc](up-b-overlay.inc). This folder sits outside `ASM/`, so the Tyro ISO build doesn't include it.

## Rebuild the text codes

With devkitPPC installed and `DEVKITPPC` set, from the repository root:

```sh
python -I scripts/upb_overlay_numbers.py melee.iso --write   # only when the shapes change
python -I scripts/build_upb_overlay_gecko.py
```

The first rewrites the `.asm` files from the ISO. The second assembles them with `bin/hgecko` into `build/up-b-overlay.gct`, then writes one text file per character.

## Tests

[tests/test_upb_overlay_gecko.py](../../tests/test_upb_overlay_gecko.py) runs all 24 text codes in a PowerPC emulator, installed together the way the Gecko handler installs them. It checks that:

- Every hook creates its own GObj and then leaves every register as the instruction it replaced would have, compares included.
- Each legal stage draws the code's shapes at both ledges, mirrored for the left, with Fox's ring, gaps and ticks, Fox's B0XX dots and Mario also worked out by hand.
- A code draws nothing until its character is in the match, and nothing on other stages or render passes.
- When `melee.iso` is at the repo root, each hook replaces exactly the instruction the vanilla DOL has there, the `.asm` files match what the ISO gives, and the stick deadzones are vanilla's 0.28.

[tests/test_upbsweetspot.py](../../tests/test_upbsweetspot.py) checks that the event draws the same shapes as the script, gaps and B0XX dots included.

The game functions are stubbed and the GX writes are recorded, so it checks what the codes ask to draw, not the picture.

```sh
python tests/test_upb_overlay_gecko.py -v
```

## Known limits

- **Untested in Dolphin and on Slippi.** They pass the emulator tests, but haven't been run in the real game yet.
- **A fixed X.** The event moves the X with your ECB; these use a standing value, which can be a unit off for the widest characters.
- **B0XX V3 coordinates.** The B0XX codes use the B0XX V3's (HayBox's Melee mode). Older B0XX firmware, or a box with other coordinates, would need its own added to the script.
- **Vanilla only.** The numbers are fixed, so a mod that changes a character's up-B or ledge grab would need them rebuilt.
- **Possible hook conflict.** Another custom code that injects at one of these hooks would conflict with that character's code.
