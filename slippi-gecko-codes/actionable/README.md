# Actionable Yellow-Green for Slippi

[actionable.txt](actionable.txt) is the copy/paste Gecko code. It targets a normal
**Melee NTSC-U 1.02 (GALE01, revision 2)** ISO running in Slippi, including live
online play. It does not require the Training Mode ISO or its DAT files.

The standalone payload passes PowerPC emulator tests. **Actual live netplay and
visual rendering have not yet been verified in Dolphin.** No inherent netplay
blocker was found in the source review; this is a candidate for testing, not a
confirmed desync-free release.

## Install

1. Open Slippi Launcher → Settings → Netplay → Manage Gecko Codes → Add.
2. Use `Actionable Yellow-Green (Slippi NTSC 1.02)` as the name. Paste the hex
   lines from `actionable.txt` into the code field, then save and enable it.
   The `$` title and `*` description lines are metadata, not hex code.
3. Keep Slippi's standard **Required: General Codes**, **Required: Slippi
   Recording**, and **Required: Slippi Online** enabled. Restart the game.

"Slippi Recording" is Slippi's standard live-game instrumentation code. This
overlay reads the frame counter it maintains during live matches. You can turn
off saving replay files; watching or recording a replay is not necessary.

The tint appears on **all fighters**, including opponents and Ice Climbers'
subfighters. Yellow lasts the final two unfrozen recovery frames; green confirms
recovery for two frames. Normal/autocancel landing, special landing, aerial
landing lag at its actual L-cancel rate, ordinary grounded/aerial attacks and
ordinary throws (F/B/U/D) follow the Training Mode overlay. Game & Watch's custom normal attacks and
Kirby's dash attack are included. Specials, rapid-jab startup/loops, animation
loops and other unsupported recovery types do not get guessed timing cues.
FoD's separate floor reflection keeps its native appearance.

Do not combine this with another custom code that hooks `0x800805C8` or
`0x8006D9EC`. These hooks do not overlap the inspected stock Slippi netplay
codeset. Other custom codes need their own compatibility check.

## Build and validation

From the repository root, with devkitPPC installed:

```powershell
python scripts/build_actionable_gecko.py
python -m pip install --target build/actionable-gecko/deps unicorn==2.1.4
python tests/test_actionable_gecko.py -v
```

Outputs are `actionable.txt` and `actionable.gct`. Compiler scratch files remain
under `build/actionable-gecko`. Neither building nor testing edits the ISO,
Training Mode sources, or Dolphin settings.

The code reuses `src/action_cues.c` for the bounded, read-only native animation
script lookahead and two-frame pulse model. A simulation hook samples each
fighter after collision processing. Sixteen samples per fighter reconstruct
history from the immediately preceding tick, so resimulation replaces discarded
future results instead of advancing a render-frame timer. Fighter identity and
spawn changes break the history chain; the first Slippi tick (-123) starts fresh.
Rendering wraps the existing lower-level fighter renderer, temporarily replaces
the selected tint, and restores all three native color structures afterward.
No fighter fields are extended, no callbacks are replaced, and no runtime heap
allocation is added by the payload. Slippi allocates the enabled codeset itself.

The C2 payload is position independent, with a one-time relocation of its local
data pointers. The second C2 resolves the first hook's branch target, so neither
code needs a fixed free-memory address. Only C2 codetypes are used because
Slippi's bootloader installs a supported subset of Gecko commands and does not
evaluate standard Gecko conditional commands.

Tests execute the distributed C2 bodies at two unrelated allocations and check
yellow/green timing, native-script IASA lookahead, scaled landing rates, corrected
rollback futures, repeated ticks, hitlag, respawns, ring wrap, full injection
register preservation, and byte-for-byte restoration of fighter color data.
When `melee.iso` is present, another check executes the disc's actual native
animation-length query through both skeleton paths. Other tests intercept that
query, and all tests intercept the GX renderer; emulator success does not
establish actual networking or graphical behavior.

References inspected:

- [Gecko writing guide](https://github.com/AlexanderHarrison/HOW-TO-WRITE-GECKO-CODES)
- [Slippi netplay codes](https://github.com/project-slippi/slippi-ssbm-asm/blob/master/netplay.json)
- [Slippi bootloader](https://github.com/project-slippi/slippi-ssbm-asm/blob/master/Bootloader/main.asm)
- [Slippi Gecko parser](https://github.com/project-slippi/slippi-ssbm-asm/blob/master/Common/Gecko/ProcessCodeList.asm)
- [Slippi rollback memory regions](https://github.com/project-slippi/Ishiiruka/blob/slippi/Source/Core/Core/Slippi/SlippiSavestate.cpp)
