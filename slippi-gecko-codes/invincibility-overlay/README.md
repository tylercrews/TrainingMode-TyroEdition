# Invincibility Overlay (Slippi Gecko code)

A standalone version of Tyro Edition's **Invincibility Overlay** for vanilla NTSC 1.02 Melee, including Slippi netplay. While a fighter is invincible or intangible, it's tinted, so you can see exactly when the protection from a roll, spotdodge, ledge grab, respawn platform or invincible move ends.

It's built to replace Aitch's **Change Colour While Invulnerable** code, which tints by port. In doubles that can put your teammate in the same color as the other team. Here, teammates each get their own shade of the team color.

## Colors

**Free-for-all:** the port color, the same as the Tyro hitbox trails. CPUs are lavender, since the trails' gray is hard to see.

| P1 | P2 | P3 | P4 | CPU |
|---|---|---|---|---|
| Red | Blue | Yellow | Green | Lavender |

**Teams:** a team can have at most three players (3 v 1), and each one gets a different shade. Players are numbered within their team in port order, so on blue team, the lowest port is blue, the next is lavender and the next is cyan. CPUs get their team's shades too.

| Team | 1st | 2nd | 3rd |
|---|---|---|---|
| Red | Red | Orange | Pink |
| Blue | Blue | Lavender | Cyan |
| Green | Green | Lime | Forest green |

Lavender only shows up on blue team in teams mode, so it can't be mistaken for a CPU.

## Install

1. Open Slippi Launcher → Settings → Netplay → Manage Gecko Codes → Add.
2. Use `Invincibility Overlay (Slippi NTSC 1.02)` as the name. Paste the hex lines from [invincibility-overlay.txt](invincibility-overlay.txt) into the code field, then save and enable it. The `$` title and `*` description lines are metadata, not hex code.
3. **Turn off Change Colour While Invulnerable** if you have it. With both on, they would each tint the same fighters.
4. Keep Slippi's standard required codes enabled and restart the game.

It only changes how fighters are drawn, never game state, so it can't cause a desync. Your opponent doesn't need it.

It hooks `80080E18`, so it can run alongside [Actionable Yellow-Green](../actionable/README.md) (`800805C8` and `8006D9EC`), the [Randall Overlay](../randall-overlay/README.md) (`8016E8C8`), the [Blast Zone Overlay](../blast-zone-overlay/README.md) (`8016E8C0`) and the [Double-Jump Height Indicators](../dj-height-indicator/README.md) (`8016E8E0` and `8016E8E8`). With Actionable Yellow-Green, its yellow and green win over the invincibility tint, the same as in Training Mode.

## What counts as invincible

The same as Training Mode's overlay:

- Invincibility or intangibility from the move or from the game: dodges, ledge grabs, respawns, getup options, and invincible moves like the start of Bowser's up-B. This is the game's own hurt status (`8007B868`).
- Moves that make every hurtbox intangible without setting that status. A single protected limb doesn't count.
- Yoshi's double-jump armor.
- Marth's and Roy's counter, while it's active.

Dead and sleeping fighters are never tinted.

## How it works

- **Hook:** `80080E18`, the first instruction (`mflr r0`) of the fighter draw callback that every fighter uses. The code wraps the whole callback: it sets the tint, runs the original callback from `80080E1C`, then puts the color data back. Slippi Online hooks `80080E80` inside that callback; that hook still runs, since the original code runs unchanged.
- **Tint:** the same as Training Mode. It turns off all three of the fighter's color overlays, sets the one the game draws (`800C0658` picks it) to the color at alpha 112, draws, and restores all three byte-for-byte. Flashes like a smash attack charge come back as soon as the protection ends.
- **Teams:** the teams flag is MatchInfo's `0x24D0` byte (`8046DB70`), which the game reads when it picks HUD colors. Each port's team is its player block's `0x47` byte (player blocks start at `80453080`, `0xE90` apart). Empty ports don't count toward the numbering.

The colors come from `TMCue_InvincibilityColor` in [src/action_cues.c](../../src/action_cues.c), the same function Training Mode uses, so both always match. The source is [overlay.c](overlay.c) and [entry.S](entry.S). The C2 payload is position independent, with a one-time relocation of its data pointers, like Actionable Yellow-Green's. Only a C2 code type is used, since Slippi's bootloader doesn't run Gecko conditionals.

## Rebuild the text code

With devkitPPC installed, from the repository root:

```sh
python scripts/build_invincibility_gecko.py
```

It writes `invincibility-overlay.txt` and `invincibility-overlay.gct`. Compiler scratch files stay under `build/invincibility-gecko`.

## Tests

[tests/test_invincibility_gecko.py](../../tests/test_invincibility_gecko.py) runs the built code in a PowerPC emulator, installed at two unrelated addresses the way the Gecko handler installs it. It checks:

- The free-for-all colors, lavender CPUs, and that team bytes are ignored outside teams.
- Every teams layout's shades in port order, CPU teammates, empty ports and unknown teams.
- Every protection source, and that dead, sleeping and unprotected fighters draw untouched.
- That all three color overlays are restored byte-for-byte, with either one selected, and that registers the callback must preserve are preserved.
- Together with the built Actionable Yellow-Green code, that its yellow wins and both restore everything.
- When `melee.iso` is at the repo root, that the hook, the hurt-status function and the player block and teams flag addresses match the disc.

The renderer itself is stubbed, so it checks what tint the game is asked to draw, not the picture.

```sh
python tests/test_invincibility_gecko.py -v
```

## Known limits

- **Untested in Dolphin and on Slippi.** It passes the emulator tests, but hasn't been run in the real game yet.
- **Possible hook conflict.** Another custom code that injects at `80080E18` would conflict.
