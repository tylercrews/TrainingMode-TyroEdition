# Invincibility Stoplight (Slippi Gecko code)

A standalone version of Tyro Edition's **Invincibility Stoplight** for vanilla NTSC 1.02 Melee, including Slippi netplay. On a fighter's last eight invincible frames they turn red for four frames, then yellow for four, and on the next frame they can be hit. It's for learning exactly when a roll, spotdodge, tech, ledge grab or respawn stops protecting someone.

It's the same red and yellow for every fighter, a little more opaque than the [Invincibility Overlay](../invincibility-overlay/README.md)'s tint. It works on its own or together with that code: with both, a fighter shows their Overlay color until the last eight frames.

## Install

1. Open Slippi Launcher → Settings → Netplay → Manage Gecko Codes → Add.
2. Use `Invincibility Stoplight (Slippi NTSC 1.02)` as the name. Paste the hex lines from [invincibility-stoplight.txt](invincibility-stoplight.txt) into the code field, then save and enable it. The `$` title and `*` description lines are metadata, not hex code.
3. Keep Slippi's standard required codes enabled and restart the game.

It only changes how fighters are drawn, never game state, so it can't cause a desync. Your opponent doesn't need it.

It hooks `80080E1C`, so it can run alongside the [Invincibility Overlay](../invincibility-overlay/README.md) (`80080E18`), [Actionable Yellow-Green](../actionable/README.md) (`800805C8` and `8006D9EC`), the [Randall Overlay](../randall-overlay/README.md), the [Blast Zone Overlay](../blast-zone-overlay/README.md), the [Double-Jump Height Indicators](../dj-height-indicator/README.md) and the [Up-B Overlays](../up-b-overlay/README.md). The order is the same as in Training Mode: Actionable Yellow-Green's yellow and green show over the stoplight, and the stoplight shows over the Invincibility Overlay.

## What it covers

The tint has to start before the invincibility ends, so the code works out how many invincible frames are left by running the game's own countdown eight frames ahead, on a copy:

- **The game's timers:** ledge invincibility, respawn invincibility and the starman. These count down every frame, hitlag included.
- **The move's own script:** dodges, techs, getups and invincible moves switch invincibility off with a body-state command, either for the whole body or by making every hurtbox intangible. The code reads ahead in the script for that command. A move's script doesn't run during hitlag, so neither does its countdown.

When it can't tell, it shows nothing rather than a wrong color:

- Marth's and Roy's counter, and Yoshi's double-jump armor. The Invincibility Overlay tints these, but nothing counts down to their end.
- Invincibility that ends because the action ends rather than by a command, like an air dodge landing into a waveland.
- A move's invincibility while that fighter is in hitlag. It picks up again once the hitlag ends.

Every dodge, tech and getup, from the floor or the ledge, is invincible for at least 11 frames, so those always show the whole stoplight. A few specials and attacks are invincible for eight frames or fewer (Marth's up-B for one frame, a shine for two, Donkey Kong's and Peach's ledge attacks under 100% for six and seven), and those start partway through: all yellow, or only some of the red.

Dead and sleeping fighters are never tinted.

## How it works

- **Hook:** `80080E1C`, the second instruction (`stw r0,4(r1)`) of the fighter draw callback that every fighter uses. The first one, `mflr r0`, has already put the callback's return address in `r0`, so the code can wrap the rest of the callback: it sets the tint, runs the displaced `stw` and the original code from `80080E20`, then puts the color data back. The Invincibility Overlay code wraps the callback from `80080E18` the same way and jumps to `80080E1C` to draw, so with both codes on, the stoplight's tint replaces the Overlay's for that draw and each code puts back what it found. Slippi Online hooks `80080E80` inside that callback; that hook still runs.
- **Countdown:** the timers are replayed as PlayerThink_Animation (`8006A360`) runs them, before its hitlag check. `0x1990` (ledge) and `0x1994` (respawn) clear the game's hurt status at 0 unless bit `0x80` of `0x2221` holds it, and the starman bit (`0x221D` bit `0x02`) clears when `0x2004` reaches 0. Training Mode also checks that each timer went down by one since the last frame, because one of its events renews a timer every frame. Vanilla Melee only ever sets these timers and lets them count down, so this code skips that check. The script lookahead is Training Mode's read-only copy of the command interpreter, which also runs Actionable Yellow-Green's interrupt lookahead. It applies commands `0x68` (the script's hurt status), `0x6C` (every hurtbox) and `0x70` (one bone's hurtbox), whose handlers are entries 16-18 of the command table at `803C06E8`.
- **Tint:** the same as Training Mode. It turns off all three of the fighter's color overlays, sets the one the game draws (`800C0658` picks it) to red or yellow at alpha 150, draws, and restores all three byte-for-byte.

The countdown and colors come from `TMCue_ProtectionLeft` and `TM_CUE_ENDING_*_COLOR` in [src/action_cues.c](../../src/action_cues.c) and [src/action_cues.h](../../src/action_cues.h), the same ones Training Mode uses, so both always match. The source is [overlay.c](overlay.c) and [entry.S](entry.S). The C2 payload is position independent, with a one-time relocation of its data pointers, like the Invincibility Overlay's. Only a C2 code type is used, since Slippi's bootloader doesn't run Gecko conditionals.

## Rebuild the text code

With devkitPPC installed, from the repository root:

```sh
python scripts/build_invincibility_stoplight_gecko.py
```

It writes `invincibility-stoplight.txt` and `invincibility-stoplight.gct`. Compiler scratch files stay under `build/invincibility-stoplight-gecko`.

## Tests

[tests/test_invincibility_stoplight_gecko.py](../../tests/test_invincibility_stoplight_gecko.py) runs the built code in a PowerPC emulator, installed at two unrelated addresses the way the Gecko handler installs it. It checks:

- The ledge countdown: nothing, then four red, four yellow, then nothing once the fighter can be hit.
- With the Invincibility Overlay code, the port color (or a CPU's lavender) until the last eight frames.
- Respawn and ledge timers together, the `0x2221` hold, the hurt status set with no timer, and the starman.
- A roll's script commands, a synchronous timer still to run, and the whole-body hurtbox commands.
- That hitlag holds the script's countdown but not the timers.
- That an unreadable script, counters, Yoshi's armor and dead or sleeping fighters show no stoplight.
- That all three color overlays are restored byte-for-byte, with or without the Overlay code, and that registers the callback must preserve are preserved.
- With the Overlay and Actionable Yellow-Green codes, that Actionable's yellow wins and everything is restored.
- When `melee.iso` is at the repo root, that the hook, the timer code, the command table and the three body-state handlers match the disc.

The renderer itself is stubbed, so it checks what tint the game is asked to draw, not the picture.

```sh
python tests/test_invincibility_stoplight_gecko.py -v
```

## Known limits

- **Untested in Dolphin and on Slippi.** It passes the emulator tests, but hasn't been run in the real game yet.
- **Possible hook conflict.** Another custom code that injects at `80080E1C` would conflict.
