# Randall Timer

**Randall Timer** is a global setting on the Global Overlays & Gameplay page, right under Randall Overlay. It's also in the Training Lab's Stage Options on Yoshi's Story, under Randall Overlay. It's Off by default and saved, and both places change the same setting.

On Yoshi's Story, the match timer turns:

- **purple** while Randall is out past the left side of the stage,
- **teal** while Randall is out past the right side,
- back to its normal color while Randall is behind the stage.

It does nothing on any other stage, in a match with no timer, or when hazards are off (Randall isn't there). It's separate from Randall Overlay, so either one can be on without the other. A standalone [Slippi Gecko code](../slippi-gecko-codes/randall-timer/README.md) does the same in any match.

## Randall's loop

Randall is map GObj 2 in `GrSt.dat`. Its root joint's first child, the cloud, rides a path animation (`HSD_A_J_PATH`), and the root's second child is the spline it follows. The animation's path value goes linearly from 0 to 1 over 1200 frames and loops, and the spline is 13 points joined by straight lines with their lengths normalized, so Randall moves at a constant speed and every lap takes exactly 20 seconds.

In stage units (the map root is scaled by `grGroundParam`, 0.7), the loop is a rectangle with rounded corners, counter-clockwise:

| Part of the loop | Cloud joint |
| --- | --- |
| Bottom, moving right | Y −38.5, X −95.7 to 95.7 |
| Right side, rising | X up to 97.475, Y −38.5 to −18.9 |
| Top, moving left | Y −18.9, X 95.7 to −95.7 |
| Left side, falling | X down to −97.475, Y −18.9 to −38.5 |

Most of the bottom and top runs are inside the stage. The stage's walls at both heights are at X ±75.25 in the DAT, ±52.675 in stage units. So on the right, Randall leaves the wall low, swings out and up, and slides back in high. On the left it's the mirror: it leaves high and goes back in low.

The overlay counts Randall as out while the cloud joint is past ±52.675. From the DAT, over one loop (frame 0 is the first frame of the stage's animation):

| Frames | Randall |
| --- | --- |
| 122–418 | Behind the stage (297 frames) |
| 419–721 | Out on the right (303 frames, about 5 seconds) |
| 722–1018 | Behind the stage (297 frames) |
| 1019–1199, 0–121 | Out on the left (303 frames) |

`tests/test_randall_timer.py` recomputes this from `melee.iso`.

## Coloring the timer

`HUD_Timer_Initialize` (`802F446C`) stores the timer's GObj at `stc_matchhud->timer_gobj` (`804A1078`), or 0 when the match has no timer. The timer is IfAll's `ScInfTim` model: a root with ten child joints of one DObj each, eight digits (white) and two separators (gray, `B3B3B3`). Each digit is a texture that `TimerThink` swaps by animation frame, multiplied by its material's constant diffuse color (`RENDER_CONSTANT`, `TEX_COLORMAP_MODULATE`, no active TEV override). The timer's material animations only change textures, never a color. So writing a material's diffuse color tints that part, and the color stays until something changes it.

Before the first tint, the overlay saves each material in the timer's joint tree with its own color (up to 16). When Randall's side changes, it sets every material to its own color times the tint, channel by channel (`own × tint / 255`), keeping alpha. That makes white digits exactly the tint and keeps the separators a shade darker. When Randall goes back behind the stage, or the setting is turned off, it writes each material's own color back. It writes only when the side changes, and it forgets the saved colors at match start, at a scene change, or when the timer GObj changes.

The colors are Training Mode's palette Purple (`A064FF`) and Teal (`00B4A0`).

## Saved setting

`TM_FLAG_RANDALL_TIMER` (index 14 of the flag field) is `TYEX` word 80 bit 20 (byte 82, `0x10`), with no version bump: every migration to version 23 zeroes word 80, and Off is 0. Older records read Off. See the [save data map](../notes/SAVE-DATA-MEMORY-MAP.md).
