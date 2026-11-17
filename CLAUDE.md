# Training Mode - Tyro Edition

A fork of Training Mode - Community Edition (a Super Smash Bros. Melee training modpack). Events are C in
`src/`, compiled by `hmex` into `.dat` modules, plus PowerPC gecko codes in `ASM/`. Read
[DEVELOPMENT.md](DEVELOPMENT.md) for implementation contracts; each feature's details are in `docs/` and `notes/`.

## After every change, rebuild the ISO

The user debugs every change in Dolphin straight from the built ISO. Once a change's tests pass, build it
without asking:

    ./build.sh melee.iso

It patches `TM-Tyro-<TM_VERSION>.iso` in the repo root in place, which is expected. Say whether it built.
`./build.sh melee.iso build/<module>.dat` rebuilds one module, and a change to a shared header forces a full
build on its own (see DEVELOPMENT.md, "Build Mode").

On Windows, run it from devkitPro's MSYS2 bash. Git Bash reports `MINGW64`, so `build.sh` picks the Linux
binaries in `bin/` and fails with "gc_fst: cannot execute binary file". From Git Bash:

    MSYSTEM=MSYS /c/devkitPro/msys2/usr/bin/bash.exe -lc 'cd "$1" && export PATH="$DEVKITPPC/bin:$PATH" && ./build.sh melee.iso' _ "$(pwd)"

## Tests

- In Git Bash, `hmex` and the PowerPC tests need
  `export PATH="/c/devkitPro/devkitPPC/bin:$PATH" DEVKITPRO=C:/devkitPro DEVKITPPC=C:/devkitPro/devkitPPC`.
- Unicorn goes in `build/test-deps`: `python -m pip install --target build/test-deps unicorn==2.1.4`.
- `python tests/test_<event>.py` builds `tests/<event>_probe.c`, which `#include`s the production event
  source, and runs it as PowerPC code. Some tests also run native routines from `build/Start.dol` and read
  data from `melee.iso`.
- `bash tests/run_settings_tests.sh` rebuilds the settings ELF and test DATs, then runs every suite. Run it
  after touching save code, shared headers or anything several events use.
- `tests/test_menu.py` needs a full release build first, and `tests/test_menu_exit.py` a fresh build.
- Tests don't replace Dolphin. After a change, say what still needs checking in game.

## Saved settings

The save is almost full. [notes/SAVE-DATA-MEMORY-MAP.md](notes/SAVE-DATA-MEMORY-MAP.md) lists every bit and
what's free; update it with any change.

- Read and write through `TM_GetSetting` / `TM_SetSetting`. Never write save bytes directly.
- The field and preference enums in `src/settings.h` are append-only (`ASM/Globals.s` mirrors the fields).
- Never reorder a saved row's stored values or its serialized IDs to change a menu. The menu order can differ
  from the stored order (see `TC_PresetToSaved` in `src/techchase_logic.h`).
- A new setting in spare bits needs those bits to be 0 in every record that owns them, or a migration that
  clears them. Store it so 0 reads as its default, and it needs no version bump. Whole new bytes: see the
  map's "Adding something new".
- Never save live RNG, attempt counters or pointers. Load saved choices in `Event_Init` before placing
  anything, and set both `val` and `val_prev`.
- Score slot identities are fixed (`Events_ScoreIndex`). Never derive them from page lengths.

## Driving a CPU

- For a CPU to act on the frame an event's think decides it, set both the raw `cpu.*` input (which the next
  frame decodes) and the processed `input.*`, then call the current state's native IASA (`cb.IASA`), so the
  game's own checks decide. See `QueueInput` in `src/ledgechase.c`, `src/shield_training.c` and
  `src/techchase.c`. (Some comments say the think runs after CPU input decoding, Grabs-ercise's say before
  the input proc; this pattern works either way.)
- `input.down` is buttons newly pressed this frame. A fresh digital L/R from Wait or in shield starts a
  powershield (GuardReflect) instead of a plain shield.
- Native research: `GTME01.map` names the game's functions; disassemble `build/Start.dol` with
  `powerpc-eabi-objdump`; the Melee decomp is at github.com/doldecomp/melee; `PlCo.dat`'s `ftLoadCommonData`
  in `melee.iso` holds the input thresholds.

## Docs

- Update the event's doc in `docs/` (or its research in `notes/`), and DEVELOPMENT.md for a new
  implementation contract.
- Don't change `version.h` unless asked. `TM_GAME_ID` never changes.

## README changelog

Every user-facing change goes in the README changelog, under the current version's section (`TM_VERSION` in
`version.h`; newest version first).

- Write brief descriptions of features, organized by event, the way the existing sections are: **New
  events**, **Globals and all events**, **Event changes** (a sub-list per event) and **Program changes**.
- One concise bullet per feature. A later adjustment updates that feature's bullet instead of adding another.
- No bugfix bullets for an event that's new in the current version. Fix it and, if what it does changed,
  update its description so the entry says what the event does now.
- Don't rewrite an older version's entries. A change to a feature from an earlier version gets a new bullet
  under that event in the current version.
- Take an item off "BUGS/FEATURES TO INVESTIGATE" once it's done.

## Debugging

Development builds log with `TMLOG(...)` to the Dolphin and on-screen consoles (L/R+Z toggles it). `bp()` in C
gives a breakpoint symbol. Load `build/TMTE01.map` in Dolphin (Symbols > Load Other Map File).
