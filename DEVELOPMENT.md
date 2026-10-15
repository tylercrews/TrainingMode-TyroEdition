# Development

Thanks for considering contributing to TM-CE!
I really appreciate all the help.
Melee is a pretty complicated game, but that doesn't make TM-CE hard to contribute to!
Here are a few things you should know before contributing.

- [Join the discord here](https://discord.gg/2Khb8CVP7A).
- Ping me in the dev-discussion channel before starting a contribution, I will guide you in the right direction.
- Keep contributions small and focused.

If you have any other questions, feel free to ping me (Aitch) in the dev-discussion channel.

## Compilation

### Windows
1. [Install DevKitPro](https://github.com/devkitPro/installer/releases/latest). Install the Gamecube (aka PPC or PowerPC) package.
2. Run the 'windows_setup.bat' file.
3. Run the command `./build.sh path-to-melee.iso` in the console.

### Linux / MacOS / WSL / MSYS2
1. [Install DevKitPro](https://devkitpro.org/wiki/Getting_Started#Unix-like_platforms). Install the Gamecube (gamecube-dev) package.
2. Install xdelta3. This should be simple to install through your package manager.
3. Run the command `./build.sh path-to-melee.iso` in the console.
    - If the provided binaries fail (possibly due to libc issues), you can compile your own binaries from my repos:
[gc_fst](https://github.com/AlexanderHarrison/gc_fst), [hmex](https://github.com/AlexanderHarrison/cdat), [hgecko](https://github.com/AlexanderHarrison/hgecko)

### NixOS
1. Make sure you have the [Flakes](https://nixos.wiki/wiki/Flakes) feature enabled.
2. Enter the development shell by running `nix develop` to load all the necessary dependencies.
3. Run the command `./build.sh path-to-melee.iso` in the console.

Alternatively, if you have [Direnv](https://nixos.wiki/wiki/Direnv) installed, you can run `direnv allow` to enter the development shell automatically when you enter this project directory.

### Build Mode
The build script takes an optional additional mode argument called the mode - `build.sh iso [mode]`.
This allows building an optimized release, or fine-grained recompilation.
Examples:
- `build.sh iso`: debug build from scratch. **You probably want to use this**.
- `build.sh iso release`: release build from scratch.
- `build.sh iso build/codes.gct`: only rebuild asm.
- `build.sh iso build/edgeguard.dat`: only rebuild edgeguard event. You can use any dat file here.

## Tyro Version and Save Identity

Edit **[version.h](version.h)** at the repository root to update `TM_VERSION`, for example `V1.4.1T2`. This one value supplies the in-game short/long labels, disc/banner titles, memory-card caption, ISO filename, release folder/ZIP, and packaged patchers. Keep the newest version's implemented changes in the [README changelog](README.md#tyro-branch-changelog).

`TM_GAME_ID` is Tyro's stable identity, **`TYRE01`**. Keep it unchanged between releases. Its `TYRE` game code differs from upstream's `GTME`, so Tyro uses its own native save namespace. Tyro starts with a fresh save by default; there is no automatic overwrite, retagging, or migration of the legacy upstream save. Existing `GTME` recordings also remain in their old namespace until an explicit compatible importer is implemented. Per-game Dolphin settings associated with the old ID may need deliberate copying.

Run builds from the repository root:

```sh
./build.sh --version
./build.sh melee.iso
./build.sh melee.iso release
```

Outputs use `TM-Tyro-${TM_VERSION}.iso` and, for releases, `TM-Tyro-${TM_VERSION}.zip`. Release contents are staged under `build/releases/` and include patchers with generated filename configuration and the Tyro symbol map. Windows packaging uses `zip` when installed, or built-in PowerShell compression otherwise. Generated banner metadata is written to `build/opening.bnr`; root `opening.bnr` remains the source artwork/credits.

Changing version/identity metadata forces a full rebuild even when a single-module build was requested. Assembly identity constants and the save caption are generated in `build/tyro-identity.s` before compilation; use the build script to generate this include before invoking `hgecko` manually. `clean.sh` removes build scratch and the current Tyro version's ISO/ZIP, preserving legacy `TM-CE.iso` and other versioned outputs.

## Saved Settings and Migration

`src/settings.c` is the portable byte codec; `src/settings_game.c` owns the shared runtime service. Event modules use `TM_GetSetting` / `TM_SetSetting` through `EventVars.settings`. Native hooks use the register-preserving `SettingsRead`, `SettingsWrite`, and `SettingsToggle` macros in `ASM/Globals.s`; macro register arguments are numeric register indices (or absolute numeric aliases), not `r3`-style register tokens. Do not write the saved fields directly.

The record stays at `0x1F24..0x1F4F`. Byte 5 stores the events page in its low two bits and the highlighted event in its high six bits. Existing saves start at event zero. Older Tyro builds may reset this combined preference to General Tech/event zero; other settings are unaffected. It uses explicit big-endian mask bytes, control/actor nibbles, three-bit OSD palette slots, `TY` signature bytes at `0x1F4A`, and version 3 in the high two bits of `0x1F2E`. Record byte 40 contains Run Turnaround (bit 0), protection (bit 1), CPU OSD suppression (bit 2), event-preference initialization (bit 3), Ledgedash reset delay (bits 4-5), Tips (bit 6), and Side-B Sweetspot guides Off (bit 7). Byte 41 contains Ledgedash start/reset/criterion; byte 42 contains Eggs-ercise damage threshold; byte 43's low five bits contain scale, spawn velocity, collision and infinite mode, bits 5-6 contain the OSD layout, and bit 7 is free: **one explicit reserved bit remains**, plus one unused palette-padding bit. Byte 37 bit 6 now holds Ledgedash criterion bit 2; its original low two bits remain in byte 41, preserving the first four criterion IDs. Versions 1/2 initialize the newer global flags Off, clear the event initialization marker, default the layout to Recent and Sweetspot guides On while preserving TurnRun where supported. Version-aware older Tyro builds use private defaults for unsupported core formats without overwriting them. `src/settings.h` defines stable fields/IDs and counts; C/assembly ABI-header changes invalidate partial builds.

Validation runs lazily after data is available. Only the exact running Tyro game/maker ID may migrate the record. Missing-signature legacy Tyro lists are copied before repacking; disabled/invalid pairs are skipped and duplicate valid groups retain the last value. Existing enable bits and valid controls are preserved, enabled OSDs receive White, and new flags default Off. Changes mark the native save as dirty and use its existing serialization/checksum path. Future format versions must retain the `TY` signature. An unsupported version or foreign identity leaves all 44 persisted bytes untouched and uses mutable in-memory defaults instead.

Earlier unversioned `TYRE01` builds from step 0 cannot understand this packed record. Once migrated, avoid using those builds with the same Tyro save; the separate upstream/T1 `GTME01` namespace remains unaffected. Version-aware newer builds preserve unsupported settings, but this does not make arbitrary older game executables or recording formats compatible. No automatic upstream-save copy/import is implemented.

T4 adds an independent `TYEX` extension at `0x1AE0`, inside unused event-score slots. Version 3 owns 16 bytes and migrates the earlier 12-byte version 1 and 16-byte version 2 while retaining A-Smash Release's palette and all existing Techchase choices. It adds Techchase's CPU invincibility color and seven Ledgechase choices: side, CPU 100%, player percent step, chance preset, distance, CPU invincibility color and followup action. The new None default uses one formerly unused padding bit; version 2 actions migrate to the corresponding shifted menu choices. The original 44-byte record and its 19 OSD palettes retain their format. Manual percentages and custom camera coordinates remain session-only. Reads of a missing extension reconstruct defaults without initializing it; an explicit edit initializes it. Unknown formats retain their bytes and use private runtime preferences. See [the score-storage audit](SCORE-STORAGE-AUDIT.md) for native serialization/checksum tests, reset protection, catalogue limits and downgrade behavior.

`TYEX` version 6 still owns 16 bytes. It adds Low Laser Puff Practice's saved Event OSDs (On) and Suppress Global OSDs (Off) at bits 20?21 of the word spanning bytes 12?14. Versions 1?5 initialize these fields while preserving previously owned settings, including version 5's Under Fire count-record marker. The eighth Character-specific Tech event is unscored and maps to no persisted score slot. See [Low Laser Puff Practice](docs/LOW-LASER-PUFF.md) for the fixed crouch target, timing and test coverage.

`TYEX` version 7 keeps the 16-byte size. Ledgechase appends a 50/50 Getup / Roll preset and shares Training Lab's ground counter catalogue and action input tables through `src/cpu_actions.h` and `src/cpu_ground_actions.h`, excluding Play Slots and Random Advanced. Its expanded action index uses byte 15 bits 0..5; versions 2..6 migrate the old actions while preserving the old bits and other event settings. Byte 15 bits 6..7 remain free.

Event score identities are stable independently of menu order: the original 28 events keep slots 0..27 and Ledgechase (General Tech #17) uses slot 50. The extension region is now `0x1AE0..0x1B37` (88 bytes), leaving 72 bytes unallocated after its 16-byte record. Unknown older formats may occupy the former 92-byte region: new-score access and records erasure preserve their final word too. Further catalogue changes require updating the explicit mapping and its compile-time guards; never derive persisted score identities from the new page lengths.

Runtime OSD category count is separate from the original saved palette count. Category 30 is A-Smash Release, appended to runtime layouts. Both global menus place it above the CPU/all-OSD overrides; the editor's display-to-physical bindings handle the reordered rows without moving saved bits. Existing categories keep their identities and palette bits. See [A-Smash Release](docs/A-SMASH-RELEASE.md) for its input timing and native hook contracts, and [Techchase settings](docs/TECHCHASE-SETTINGS.md) for presets, positioning and punish behavior.

Ledgechase is an independent event module; its fixed-FD, selectable-CPU and native percent-HUD contracts live in `src/events.c`. EventVars appends `set_cpu_invincibility_overlay`, which borrows a menu option pointer until scene cleanup. It uses the shared protection detector and render-time color composition, including paused edits and CPU subfighters. Local Off suppresses only the CPU protection tint, while the human keeps its global setting. The binding survives restore clears and detaches before event modules unload. See [Ledgechase Training](docs/LEDGECHASE-TRAINING.md).

Run the actual PowerPC codec, runtime bridge and native-hook tests from the repository root:

```sh
python -m pip install --target build/test-deps unicorn==2.1.4
bash tests/run_settings_tests.sh
```

The runner compiles both a freestanding big-endian PowerPC ELF and a fresh optimized production DAT. Tests apply native MEX relocation rules to that DAT and emulate the actual exported settings accessors, as well as the ELF fixtures. They cover legacy/duplicate/invalid migration, all overlay slots, cross-byte palettes, flags/control nibbles, surrounding canaries, reserved bytes, future/foreign ownership, save-dirty behavior, and hook GPR/FPR/CR/CTR/XER preservation. This is independent of the native card's on-disk checksums; save/reload and downgrade behavior should also be checked in Dolphin on a test card before distributing the format change.

Keep the settings byte-comparison helper non-inlined. Inlining comparisons between fixed low-memory addresses and module strings can emit symbol-minus-fixed-address addends that hmex/native relocation cannot preserve. ELF-only checks can pass while the loaded DAT reads a wrapped low address. The save-creation regressions therefore require the optimized DAT path.

## OSD Identities and Styling

The Wavedash and Ledgedash training HUDs use the appended `EventVars.HUD_DrawInfoPanelWithStick` callback to replace their Angle label with the same octagonal diagram beside the unchanged-size number. Other info-panel callers retain their existing layout. Signed attempt vectors are appended to each event's data and published with the numeric angle, so a new attempt or retry cannot replace the diagram while the old number remains visible. The helper respects the HUD camera's hide flag and completes the panel's GX quad batch before drawing lines. Egg Toss retains its numeric launch-angle display. Run `tests/test_stick_hud.py` through the settings runner for production event snapshot/retry and native GX vertex/font checks.

`src/osd_style.c` defines the pure message-tag decoder, palette, replacement-key rule and best-frame timing colors; `src/osd_style_game.c` applies the style at GX time after legacy callers recolor their subtexts. Configurable messages explicitly supply a settings identity. Untagged event feedback keeps its own colors and visibility. The GOBJ return contract stays valid when a category is Off; no caller is given a NULL pointer.

Native producers use `Message_DisplayOSD id, arg, line, inline, best` in `ASM/Globals.s`. The original low-byte kind is retained for queue behavior; metadata separately identifies the setting, the first/second/third integer vararg to snapshot, its text line, an optional Wavedash first-line layout, and the best displayed frame (default 1). C producers use `OSD_MessageTag`. Queue replacement matches both kind and settings ID, preserving alternate IDs and the -1 always-new convention. Do not infer identities or timing semantics by parsing formatted strings.

Timing color compares the unchanged displayed measurement with the declared best frame: best=Cyan, best+1=Green, best+2=White, earlier/later=Red. Instant double jump retains its original best interval of five and still displays five; it is not relabeled Frame 1. No-timing metadata leaves success rates, GALINT, advantage, SDI counts, angle/hop quality, early/late diagnostics, lockout state and shortening-window outcomes under their existing rules. L-cancel adds a separate outcome line so its input timing does not redefine whether the cancel succeeded.

Wavedash uses one normal-size first-row subtext for its title and frame value. Row two draws an octagonal stick gate beside the angle number; optional Hop remains row three. Message creation snapshots the full signed airdodge direction from the owning fighter before later attempts overwrite it. The diagram follows the text's transform in Recent, Fixed Grid and Practice Panel, including queue/deletion animations and live visibility settings. `OSD_StickTip` intersects the direction with the gate, and `OSD_DrawStickDiagram` draws native GX lines without a texture or font glyph. The native combo-end callback finds the canonical Combo Counter in the shared C queue, rather than looking up an obsolete native kind-13 text array. MsgData's original prefix/text pointer offset and existing function-export indices are preserved; extensions are appended and ABI fingerprints force full builds.

Both editors select Off, White, Red, Green, Blue, Yellow, Cyan and Magenta. In the L-button menu, B cycles forward and Z backward; boolean global rows toggle with either button. Stick/D-pad navigation, X/Y position selection and A/Start exit remain available. Each row shows its value and a color preview. Lab changes only the selected category through its existing eight-choice rows.

Both menus expose OVERRIDE OSDS OFF (On hides messages, Off restores normal display). It uses flag bit 0; native row ID 6 maps to that flag rather than enable-mask bit 6. Suppression is applied to tagged configurable messages at GX time, including existing text and backgrounds while paused. Individual palette choices and trail flags remain intact; untagged event feedback is unaffected. Message lifetimes continue normally, so expired messages do not return when suppression is lifted. Master suppression itself uses no extra saved bytes; after step 6 the shared record is format 3 with 30 reserved bits remaining.

`src/osd_editor_game.c` maps native cursor order to sparse settings IDs and updates the existing row subtexts. The native hook intercepts B/Z before the RSS Boolean toggle path. It updates the selected Boolean in the rules struct while leaving the row snapshot unchanged until native animation copies it; the exit save then preserves the selected nonzero palette. Untitled/reserved rows cannot be edited. Editor exports are appended after existing function indices and ABI fingerprints force a full rebuild. PowerPC tests cover palette cycling/wrapping, snapshot saves, untouched reserved bits, master/trail independence, future-format fallback and fresh-service round trips, in addition to styling tests. Font placement, native menu clicks and real memory-card save/reload require Dolphin verification.

## Shared Recovery Cues, Flashes and Shields

`src/action_cues.c` owns the pulse model and a read-only, bounded two-frame native script lookahead; `src/action_cues_game.c` samples all twelve possible main/subfighter slots in a shared match service. Both OSD editors expose Actionable Yellow/Green, Run Turnaround, Missed L Cancel and Infinite Shields. The text master switch does not disable them.

Yellow marks the last two unfrozen simulation frames before supported recovery; green is a two-frame completion confirmation, including immediate next ordinary actions. Boundaries cover normal/autocancel landing, special/air-dodge landing, all five aerial landing states at their actual rates, and ordinary aerial/grounded attacks using native IASA or animation completion. Game & Watch's custom normal attacks and Kirby's dash attack are normalized explicitly; Kirby's airborne dash continuation has no IASA and uses completion only. Rapid-jab start/loop, animation loops, charge holds and character specials/other recovery categories are excluded. See investigation section 30 for the exact scope and remaining runtime matrix.

The landing hook records missed-cancel timing before the input timer ages, independently of text enable. Run Turnaround triggers only on TurnRun (19). Each red diagnostic repeats a eight-frame opacity pulse throughout its corresponding action state, with pause/hitlag holding its phase. Timing colors replace red/local body color layers during the original fighter GX callback; all three native color structures are copied back afterward. Invisibility is respected. No persistent native colanim slot is cleared. Restore/reposition callbacks clear transient history through the appended `EventVars.clear_action_cues` API; match/scene/death/clock changes also clear it.

Infinite Shields refills native full health before fighter processing and after event writes, so Lab's local shield-health choices cannot override it while On. It applies to humans, CPUs and subfighters. It does not create immunity to a single over-full-health shield hit. Off resumes event-local policy. The overlay tracker uses 624 bytes for twelve entries plus 16 bytes of service state and GOBJ/process overhead; drawing uses a temporary 384-byte stack snapshot.

PowerPC adapter/model tests validate two-frame windows, rates/IASA, priority and byte-for-byte colanim restoration, all slots/shields, freeze/pause/restore, flashes, new editor rows and version migration. The optimized release build is also required. Actual Dolphin frame stepping, menu fit, camera passes and native card I/O remain separate runtime verification.

The shared GX wrapper also provides Invincibility Overlay: effective native whole-body hurt status plus Yoshi's positive native double-jump armor. It uses trail RGB accents (CPU Gray) with alpha 112/255. Actionable Yellow/Green wins over protection; protection wins over red diagnostics/local colors. Draw-time reads make toggles, pause, restores and protection expiry immediate without a new history buffer. Native color state is restored after drawing. The SDK's `Fighter_GetIntangibleFrames` returns effective hurt status, despite its name. See investigation section 31 for coverage, format-3 migration and the current 30-bit reserve ledger.

## Project Structure
### Shared hitbox trails

`src/trails.c` owns the bounded trail model, fade curves, profile priority and player palette. `src/trails_game.c` captures/draws once through the common match-start service; Lab and Eggs-ercise configure it through `EventVars.trails`. The bank holds 128 forty-byte samples (5,120 bytes), plus twelve bytes of indices/clock state. This is runtime RAM; the two global preferences use their existing flag bits, with no extra allocation for trails; the shared record currently has 30 reserved bits left.

Ledgedash and Under Fire use the appended `EventVars.trails->set_override` callback with a borrowed menu-option pointer. Their Hitbox Trails selector offers Off, Normal, Fast, Very Fast, Instant, Slow and No Fade. An attached selector takes priority over global and legacy local profiles, including Off; NULL releases the override. Rendering reads the current value while paused and clears history when the effective mode changes. Restore clears retain the choice, while scene cleanup detaches the pointer before unloading the event. Under Fire resets its selector to Off on entry. These are session choices and allocate no save bits; Lab and Eggs-ercise retain their existing global-priority rules.

Under Fire uses exports 40/41 and 46..49 for its menu, refresh counter, native ledge control, CliffCatch timing, camera subject and diagnostic sampling. Left/Right menu choices and D-pad shortcuts share the complete native restore path. The native timer is hidden; a large Perfect Refreshes counter adds one for each exact-zero regrab after a tracked ledge departure, independent of readout visibility. Initial placement, duplicate catches, close misses and hit/death feedback never score. Retry resets the visible count while retaining the session maximum for Exit. The descriptor uses integer score formatting. Save extension v5 adds one record-format bit; old survival-frame records stay hidden and untouched until a positive count replaces them, preserving all preferences and the 16-byte extension size. Ledge Refresh OSTs defaults On and controls a centered Ledge Drop / Invincibility pair. First legal drop is green 1; 2/3 are magenta; 4+ are red. Perfect regrab is green 0, -1/-2 magenta, later failures red. Act is reduced to one white cycle frame and retained only in disabled code. Suppress Global OSTs reuses its runtime option, defaults On, and gates global results plus grid/panel placeholders without writing saved global settings; Exit and scene cleanup detach the override. Disable Attack Particles remains saved/default Off, preserving ledge circle 1052 and free-movement effects while filtering attack/special models and sprites. See [Under Fire](docs/UNDER-FIRE.md). The 50-slot shared function table remains full; no new exports are added.

The global Very Fast/Instant toggles override event-local Enable/Decay; both On selects the Very Fast union once. Local menus display the active global override. Local decay choices are Normal, Fast, Very Fast, Instant, Slow and Off (no fading); Very Very Fast was removed. Samples retain player RGB and encode base-damage opacity in their alpha byte: 64 minimum, 20 per damage point, 240 maximum; history caps scale up to 96. The fade endpoints retain the preset's lifetime. Off under Decay means no fading, not disabled trails. A dense match can overwrite samples before their full nominal lifetime.

Colors use player-block accent indices (including teams), with CPU kind explicitly gray. Item ownership is checked against live fighter objects; unmatched owners are neutral gray. Lab body overlays no longer replace trail colors. The shared callback collects all six slots and both subfighter indices, after fighter/item/event updates, and uses a simulation tick with the native match-frame key for pause/double-capture/rewind detection. Native DOL inspection confirms the frame-key field and its pause/frame-advance gates. Scene changes, successful common savestate loads and Ledgedash repositioning clear the bank. Stationary identical samples are refreshed instead of repeatedly blended.

Native L-menu row IDs 2 and 4 now map explicitly to the Very Fast/Instant flags through `TM_SETTING_NATIVE_ROW`; row 6 maps to master-off; rows 7, 11, 17 and 23 map to missed L-cancel, recovery cues, Run Turnaround and Infinite Shields respectively. Row 25 maps to the protection overlay. Other rows map to ordinary OSD enables. The two rows do not use bits 2/4 of the enable mask. `SettingsReadIndexed` preserves the hook register context while supplying the native row index. The existing PowerPC test runner now also covers profile union/priority, alpha endpoints, palette mapping, pause/deduplication/rewind, ring bounds and native row mapping.

Native row constants and both labels live in `src/settings.h`. The build generates their ASM label macros alongside the identity include, so L-menu and Lab labels stay consistent. Lab's two change callbacks update only the selected flag, then refresh both menu values from the shared service. Local menus distinguish Off, Very Fast, Instant and Both On; Both On retains Very Fast's single-render union. Tests round-trip every combination into a new emulated service instance and verify that row edits preserve the sibling flag, OSD colors/mask, overlays and other global flags.

Runtime checks should cover both global toggles, all local decays, paused/frame-advanced/slowed play, recordings and restores, Nana, owned/reflected/neutral items, team colors, dense projectile loads, and native hitbox/body-overlay combinations. The model tests and build do not verify visual brightness or callback timing in Dolphin.

There are a few important directories to know about:
1. `src/`: this directory contains the source for the C events, as well as some setup code for the event in `events.c`.
2. `MexTK/`:
    - The `include/` subdirectory contains headers for internal melee functions. Calling these will call native ssbm code.
    - The `melee.link` file maps native melee symbols to addresses.
    - The `.txt` files contain symbols that we want called by the m-ex system.
        For example, C events will want their `Event_Init` and `Event_Think` functions called.
        We only use the evFunction and cssFunction modes.
3. `ASM/`: This huge directory contains gecko codes for various things like UCF, old events, OSDs, etc.
Every file has a injection address at the start.
When the game boots up, it will overwrite the instruction at that address and replace it with a branch to the asm contained in the file.
The `.asm` files will be injected and run, while the `.s` files contain include macros and will not be assembled by hgecko.
4. `dats/`: The dat file format is used by SSBM for storing data, such as models and animations.
This directory contains HUD models and animations for events.
You will need to use [HSDRawViewer](https://github.com/Ploaj/HSDLib) to view these.
5. `bin/`: This directory contains binaries used in compilation.
    - `hgecko` is a reimplementation of Fizzi's `gecko` tool for better performance. This compiles asm files into gecko codes.
    - `hmex` is a reimplementation of Ploaj's `MexTK` tool for better performance. This compiles C code into DLLs run by m-ex.
    - `gc_fst` allows modifying the ISO filesystem.

## Melee Stuff

### HSDRaw and Dat Files

[A dat file, or an HSD_Archive](https://github.com/doldecomp/melee/blob/master/src/sysdolphin/baselib/archive.c) is the file format for data in ssbm.
Everything is stored in dat files - models, animations, code, textures, etc. Only cutscenes and music are stored differently.

[You can open, view, and edit dat files with HSDRawViewer](https://github.com/Ploaj/HSDLib).

The `dats/` directory contains some of these files.
They contain event specific objects, mostly menu models with some random other data.

### Objects

- **GOBJ** - game object. This is a very generic object.
They can have a model, an update function (think function in melee), pointer to arbitrary data, etc.
Most everything is a GOBJ.
- **JOBJ/JOBJDesc** - joint object (models).
Each JOBJ has a sibling and a child, forming a tree of joints.
Each joint can have a DOBJ, forming a large tree of models.
Technically, HSDRaw only deals with JOBJDescs, as JOBJs are only created at runtime from a JOBJDesc.
However, it calls them JOBJs for whatever reason. So JOBJDescs in training mode are JOBJs in HSDraw.
This same pattern holds for a lot (but not all!) HSDRaw objects.
Almost every object in the training mode dat files are JOBJDescs (node will be 0x40 in length).
You'll need to right click on the node -> Open As -> JOBJ in HSDRaw in order to view the model.
- **DOBJ** - display object. These contain meshes, textures, a material, etc.
- **MOBJ** - material object. Lots of stuff here, but I don't know much about them.
- **HSD_Material**. This contains colouring information. Most objects are coloured by setting the diffuse field in these.
- **TOBJ** - texture object. Images that will be displayed on a mesh.
- **POBJ** - polygon object. Contains a mesh.

## How To Do Things

- If you want to alter an event written in C (easy):
    - The training lab, lcancel, ledgedash, wavedash, edgeguard, and powershield events are written in c.
    This makes them much easier to modify than the other events. Poke around in their source in `src/`.
- If you want to alter an event written in asm (big knowledge check):
    - You will need to know a bit of Power PC asm.
    - Read `ASM/Readme.md`
    - Go to `ASM/training-mode/Custom Events/Custom Event Code - Rewrite.asm` and search for the event you want to modify.
    - These will A LOT trickier to modify than the other events. Prefer making a new event or modifying the lab.
    - There are a lot of random loads from random offsets there. Grep for that address in MexTK/include to see where it points.
    Feel free to put a comment there indicating the source!
- If you want to make a new event (tricky, but super flexible):
    - Add a file and header to the `src/`.
    - Add the `EventDesc` and `EventMatchData` structs to `events.c` and add a reference to them in the `General_Events`, `Minigames_Events`, or `Spacie_Events` array.
    - Implement the `Event_Init`, `Event_Update`, `Event_Think` methods and `Event_Menu` pointer in your c file.
    - Add the required compilation steps in `build.sh`. Follow the same structure as the other events. You can skip the dat copy if you don't have any models attached to the event (you won't), like the powershield event.
Poke around the other events to figure out how to implement these.
The powershield event is the simplest and easiest to learn from.
- If you want to create a new OSD (simple-ish):
    - Add your function logic to `src/osds.c`.
    - Add the OSD to the OSD list in `ASM/training-mode/Globals.s`. OSD ids are weird, I don't know exactly how to do this.
    - Add your OSD to the corresponding slot in `ASM/training-mode/Onscreen Display/Toggle UI/Load Alt Text When Loaded With L.asm`.
- If you want to draw graphics (easy):
    - Use the `GFX_Start` and `GFX_AddVtx` functions. Search around to see the specific usage.
    - You can use the `HUD_*` functions for higher level drawing.
    - Drawing from ASM is currently difficult. Check out `ASM/.../Custom ESS Button Actions.asm` for a possible method.

## Debugging Tips

### Training Lab controller menu (T3)

Lab's root opts into the tabbed renderer through `EventMenu.tabs`. `src/lab_menu.h` defines referenced views of the original `EventOption` arrays; never copy mutable options or reorder serialized value choices to change presentation. `EventMenu_GetOption` resolves view/page rows to their canonical option. Palette/global callbacks identify that option directly rather than using a presentation row index.

`src/menu_controller_ui.h`, included by `menu.c`, implements the native tabs, two panels, previews and A/B value pickers. `src/menu_controller.h` contains portable cursor/value/viewport rules. Menus without tabs retain the legacy renderer. The menu headers participate in the shared ABI build fingerprint, forcing a full rebuild after layout changes. No save-format change or new shared function export is required.

`src/menu_controller_layout.h` sets the right panel's glyph-safe baseline spacing and bottom-bounded preview capacity. Use 32-unit stock glyph cells when checking height; the initial 17-unit preview pitch overlapped at scale .65. Current descriptions use 30-unit pitch and previews 32 at .84 scale. Unopened submenu previews page with Left/Right when needed; tabs and editor page selectors retain their own Left/Right behavior. The root's optional `unavailable_reason` callback supplies Lab recording-state context without a new shared export. See the [recording guide](TRAINING-LAB-RECORDING-GUIDE.md) and investigation section 13 for font options and current recording semantics.

Run `python tests/test_menu.py` after a full optimized build; it uses the existing Unicorn dependency under `build/test-deps`. The probe checks bounded menus, canonical identity, all OSD destinations, signed values and large frame pickers. It also loads `build/lab.dat` with native MEX relocation rules and executes the initial view/page setup before any fighter dependency. Run the existing `tests/run_settings_tests.sh` suite too. Use devkitPro's MSYS2 Bash on Windows; Git Bash's MINGW identifier does not select this build script's Windows tools. Live font fit, controller input and custom editor/card flows still require Dolphin.
- Development builds enable logging! Call `TMLOG(...)` to print to the dolphin console and the onscreen console. L/R+Z toggles console visibility.
- **Use the dolphin debugger!** Make sure you have the latest version of dolphin for debugging.
    - To set a breakpoint, use the `bp()` fn call in C or the `SetBreakpoint` macro in ASM (which will clobber r3). Then when you boot up dolphin, put a breakpoint on the `bp` symbol.
    - **For Tyro, load `build/TYRE01.map` with Symbols->Load Other Map File.** Or copy it to the Maps/ directory in the Dolphin data directory. The release ZIP includes this map; `GTME01.map` remains its source symbol map in the repository.


## Grouped Global Settings Editor

The native title and Lab entry/title are `Global Settings`. Read native rows down the left column then the right: nineteen OSDs, blank row 19, `OVERRIDE OSDS OFF` at row 20, blank row 21, then seven shared controls at rows 22-28. The checkbox subtrees for gaps are hidden without walking into sibling rows. Lab follows OSDs/master/shared controls in its scrollable menu.

`TM_SETTING_EDITOR_ROW` (field 17) translates native physical RSS IDs to `TMSettings_EditorIDs`; the load/save hooks use that field. Do not change serialized OSD IDs, palette slots, logical-row bindings (field 16) or flag positions when moving UI rows. Tests exercise initialization, both gap rows, all native exit writes, macro ABI and preservation of custom-event bits. The packed format remains 3 with 30 reserved bits.

Missed-cancel results stay latched until aerial landing exits; Run Turnaround is active only in TurnRun. Their opacity pulse lasts the state, holds during pause/hitlag, and is overridden by recovery/protection cues. Trail base damage changes captured RGB toward a matching alternate independently for each hitbox; current/history alpha stays 200/72 with existing fade timing. Hue differences prevent stationary deduplication from erasing strong-phase history. Source inspection found the old trail used damage-dependent RGB rather than a dedicated hard-hit classification. See investigation sections 33 and 36 for layout, pulse behavior, current trail hue mapping and runtime checks.


## Renderer Lifecycle and Ledgedash

Keep a fighter's original GX callback when a spawn number changes on the same GOBJ. Clear cue/local history, not renderer ownership. All-Off mode restores the native callback. Shared protection checks aggregate engine/script status and all protected capsules; the latter covers native roll/move paths missed by aggregate-only checks. The common L-cancel window at `ftCommon + 0xE4` is an integer, not float. Landing entry is captured by the hook with an independent late-entry fallback.

Ledgedash uses `src/ledgedash_logic.c` for criteria/lifecycle/surface checks and `EventVars.set_local_overlay` for color composition. It adds Left/Right ledge choice, four criteria, a bright protection highlight, and an Egg Targets submenu with enable, Ground/Platform/Random targeting, fixed/random distance bounds and pop-damage controls. Restore the original ResetThink-before-HUDThink ordering and failure checks, including airdodge frame 9 during a Falling start; completed attempts use the original completion delay for both verdicts. Hard criteria alone preserve their landed GALINT opportunity. Statistics/audio resolve once, and harder criteria retain their protected opportunity. Menu options are event-local and cost no save bytes.

Keep egg pointers/context outside the copied event block. `EventVars.get_restore_serial` changes on successful common restores; Ledgedash reconciles its target. Cleanup must restore item callbacks and remove active/retiring targets before unload. The verified egg-rest entry at 0x80288E6C and supporting collision line avoid delayed falling spawns; surface validity and live platform motion remain runtime verification points. See investigation section 35 for precise behavior, remaining checks and the unchanged 30-bit reserve ledger.


## Native Color Selection and Timing Context

The native color chooser at `0x800C0658` reads `color[0].colanim` (fighter `+0x430`): nonzero selects slot 0, zero selects slot 1. Apply temporary tints to that selected slot; disabling its color flag does not change selection. Preserve all three ColorOverlay structures around the original draw callback. Tests execute the real seven-instruction native chooser, including perfect protected wavelands and local Ledgedash highlights.

`OSDContext_Tick` is appended export 33 and runs at `0x8006AB78` before the existing freeze early return; `OSD_ActOutWait` is export 34. Preserve volatile/nonvolatile registers and floating-point state with SettingsBackup/Restore. The twelve generation-keyed contexts occupy 864 static RAM bytes, outside FighterData and the memory-card record. Native pause/frame duplicate, rewind, respawn and common restore/reposition cleanup must not leak previous contact counts.

Timed messages snapshot their relevant own-fighter hitlag once. A white `Nhl ->` subtext precedes the original measured frame (including `/7` for L-cancel); frame color still compares that measurement with its technique's best frame. Wavedash shrinks both first-row runs to 70%, or uses a 55% prefix/result alongside the title when needed. Do not convert untimed outcome/angle/advantage text into actionable Frame 1. MsgData retains its original prefix and now occupies 56 bytes.

Act OoWait uses actual normal-landing lag, accepts direct aerial/special landing exits, and labels supported recovery sources. It retains the native six-history-slot/13-opportunity scope; extending long recovery chains and adding actual `hs`/`ss` duration are tracked in the plan's top to-do list. Imported recording shield policy, shieldstun recovery cues, real card validation and live performance checks remain there as well.


## Compact OSDs and CPU Suppression (October 6, second round)

Numeric frame strings use Nf; ratios include both units (2f/7f). Hitlag uses Nhl->Nf because the native font dictionary has no arrow glyph. Do not feed UTF-8 to Text_ConvertToMenuText. The formatter converts its bounded ASCII prefix/result, reads native menu-font kerning and positions the runs adjacently. White prefix and result timing colors stay separate; timing colors are now Cyan/Green/Yellow/Red relative to each technique's original best interval.

Act OoWait prints title/source/result. Tag bit 22 (TM_OSD_POINTER_FIRST) declares the leading source string, so the timing helper skips it with va_arg(const char *) before reading the integer; do not read a pointer as an int or parse formatted strings for timing metadata. Wavedash's third-row policy is implemented by shared export 35 and its native caller: Short Hop at 1f cyan, other Short Hop green, Full Hop red.

MsgData.queue_num identifies a message's player owner. CPU suppression is checked at GX time using live Playerblock.p_kind, preserving valid GOBJ/text contracts and supporting paused toggles. Untagged CPU-owned messages are included; general queue 6 and human messages remain available. Lab's independently drawn CPU info panel checks the same flag. The ALL override keeps its existing configurable-message scope.

The editor has one gap at native display row 19, ALL override row 20, CPU override row 21, then the seven shared controls. CPU logical row 29 maps to flag 8 and physical RSS row 18; never use enable-mask bit 29 for this preference. Lab insertion shifts every affected initialization/callback offset. Format 3 remains 44 bytes; byte 40 bit 2 is CPU suppression, leaving mask 0xF8 plus bytes 41-43 unassigned (29 bits). Older formats 1/2 clear the newly allocated bit during migration.

LdshEggPlacement caches mode/distance in the savestate-copied event block. Reroll only from actual Fighter_PlaceOnLedge reset/reposition; regrabs, restore reconciliation and vanished-support retries reuse the cached selection and consume no placement randomness. Item identities/callback ownership stay outside the copied block. Trail current/history alpha is now 216/84; TM_TRAIL_FADE_BASE remains 200 to keep existing profile endpoints, including Instant.


## Single-Row Colored Timing and Confirmed Shine Outcomes

The old separate-subtext kerning estimates are superseded. OSD_FormatTiming creates one centered native row with inline colors; Wavedash uses normal 1.0 subtext scale and retains angle/hop rows. The shared ASCII converter recognizes ESC + 8 uppercase hex digits and writes native opcode 0x0C + exactly three RGB bytes. The input alpha pair is consumed but never emitted as a fourth payload byte. Bounded generated strings use this syntax; ordinary text keeps the existing converter path. Title header color remains dynamically configurable while inline commands color hitlag/turn/result components. Do not reapply a whole timing-row color when MsgData.timing_encoded is set. There are no independent centered prefix/result objects to overlap.

Wavedash hop coloring must use the value actually supplied to its third-row format (r9), saved in r25 across Message_Display. It must not use wavedash timing from TM_FramesinOneASAgo. Native producer regression tests deliberately make those values differ.

OSD_ShineBeforeIASA (export 36) samples at Generic Per Frame before the native IASA callback. OSD_ShineAfterIASA (37) observes after that callback at 0x8006B80C; the hook restores the original epilogue instruction. Both preserve registers/FPRs using SettingsBackup/Restore. Only actual jump/turn states count as outcomes; remove the old input-predicted Act OoShine display. First turn is pending context; second turn is terminal and always red. Each result is emitted once per episode.

Document but do not print the normal two-step shine turnaround recovery: turn IASA is empty, so no opportunity counter advances there. Resume at 1 on the first restored loop opportunity. Preserve ground/air transfers; do not count airborne time with no jump remaining as jump delay. Hitlag, turn timing and the post-turn jump timing are separate quantities. A fresh shine, generation change, rewind or common restore/reposition clears transient state.

TMShineEpisode adds 52 bytes per context; twelve entries total 1,488 bytes. MsgData is 72 bytes with previous offsets preserved. Save format stays 3/44 bytes with 29 free reserved bits. The test runner builds fresh ASM and executes emitted C2 bodies with the native DOL converter under Unicorn, in addition to model/adapter, Wavedash producer and relocated-DAT tests. These checks complement live Dolphin appearance/frame-step validation.


## Native Text Color Payload Safety

The native width parser at 0x803A8314 and renderer at 0x803A8D7C consume exactly three bytes after color opcode 0x0C. Emitting RGBA leaves alpha in the command stream; 0xFF is then interpreted as a glyph prefix and can index an invalid SIS font pointer at 0x803A8368. It also corrupts subtext traversal and can make timing content, recovery sources or titles overlap/disappear. Eight ASCII hex input digits remain supported, but the converter emits only RGB.

A converter-byte test alone does not validate that stream. The regressions now execute the native width parser (0x803A8134) and the same native subtext locator (0x803A6FEC) used by text editing. Initialize the text opcode-history buffer and native constants/SIS kerning fixture when running these functions under Unicorn. Tests include an old malformed RGBA stream that must fail, and corrected RGB streams that must preserve all following row boundaries.


## Replacing Already-Colored Native Text

The native subtext locator's optional old-body-length iterator at 0x803A7068 originally recognized glyph pairs, spacing (0x0A) and tracking (0x0B), but stopped at inline color (0x0C). SetText therefore treated an existing colored row as an empty/truncated old body. Inserting a replacement and writing its closing 0x0F could orphan the old RGB payload, turning color bytes into bogus glyphs and leaving duplicated/blank content. The new C2 hook counts 0x0C + RGB as four bytes while preserving other commands.

Shine callers set TM_OSD_DEFER_FORMAT (tag bit 28). Message_Display creates their unformatted result row, then the caller attaches finalized hitlag/turn/outcome metadata and formats once. The shared category, best-frame bits, CPU/master suppression and existing queue API stay unchanged.

Do not validate this path solely by converting strings or manually assembling a finished text buffer. The regression now invokes real native SetText, Position, Scale, Color and subtext traversal on the same buffer through repeated growing/shrinking updates. Its format fixture copies already-expanded ASCII, matching the timing builder; native conversion and buffer mutation remain real. Without the old-length fix, the regression detects retained/orphaned data; with it, title/detail/angle/hop rows and buffer boundaries remain intact.


## Final Global Settings Order

Use the shared Actionable Yellow>Green label. The native editor orders the original nineteen OSDs, A-Smash Release, CPU override, ALL override, then seven visual/gameplay controls. Display rows 19/20/21 bind to logical IDs 30/29/6; physical RSS rows 16/17/18 map to A-Smash/CPU/all. Lab's canonical array uses the same indices; its Global Settings view presents A-Smash immediately above both overrides. Trail/cue initialization and callbacks keep offsets 22 onward. Do not alter serialized IDs/bits to move UI rows. Reserve counts in later sections supersede the historical counts above.

## Reserved-bit event preferences

This supersedes earlier reserve counts above. The core remains **44 bytes / format 3**, with **four free bits**. Appended settings fields 18/19 select stable Ledgedash/Eggs-ercise preference IDs; field 20 is an event-scoped reset command (`value=1`, index `TM_EVENT_LEDGEDASH` or `TM_EVENT_EGGS`). No new function-export slot or EventVars member is needed.

Ledgedash persists Starting Position, Reset, Success Criteria, Reset Delay and Tips. Eggs-ercise persists damage threshold, scale, spawn velocity, collision display and Free Practice/infinite mode. Local Egg trails and all other Ledgedash choices remain session choices. Never save live RNG, automatic ledge swaps, attempt counters or object pointers as preferences.

An older format-3 record with byte 40 bit 3 clear reads immutable event defaults without changing its payload or dirty state. The first valid explicit preference write initializes both small blocks and publishes the marker last. Invalid writes do not initialize anything. Initialized values are validated individually, and both free masks (`0x80` at byte 40 and `0xE0` at byte 43) survive edits, validation and resets. Older format-3 readers preserve these previously reserved bits; unversioned prototypes remain incompatible as described above.

Load choices before initial placement/spawning and set both `val` and `val_prev`. The Ledgedash criterion label and Pop Egg enable dependency are reconciled before setup. Egg infinite mode restores the count-up clock and Free Practice gating; timed mode uses challenge defaults for gameplay even if staged menu choices differ. A reset restarts Eggs-ercise with a fresh timed clock. Menu callbacks mark the native record dirty; shared menu close and explicit exit/retry flush through `Memcard_SaveIfChanged`, with no per-frame or automatic-placement save polling. The existing native card/checksum machinery writes the same record.

The compiled regression suite covers all valid preference choices, bad indices/ranges, individual repairs, first-write defaults, per-event reset isolation, four-bit preservation, hint/infinite-mode reload, foreign/unsupported private fallback and optimized DAT relocation. Live card failure/reload and actual menu/placement checks remain Dolphin validation.

## Stable OSD layouts

`src/osd_layout.c` defines canonical player/category keys, reserved row-major cells, append-only owner order, page capacity and a bounded three-result timing history. `src/osd_layout_game.c` routes finalized result objects into that bank. Fixed Grid is three columns by three rows; Practice Panel is six compact rows. Every selected category reserves a cell even before its first result. Expiry changes the footer from `new` to `last`, retaining text and geometry without slide/delete/lifetime-bar animation.

Native Message_Display callers still receive valid objects and edit their original subtext indices. The original MsgData prefix is unchanged; seven appended fields grow it **72 -> 100 bytes**. Player/status/history text is a separate Text object, created after caller edits. Inline-colored timing remains one native glyph stream. Recent continues using its original kind/category replacement rules and eight-message queues; essential untagged event feedback remains in those queues in every mode. Switching to a stable style imports existing queued results without overwriting a newer native result. Switching to Recent restores centered geometry and normal queue expiry.

Settings field **21** stores `Recent=0 / Fixed=1 / Panel=2` in byte 43 bits 5-6. Value 3 is invalid/reserved. Field **22** exposes the six composite native editor choices (four Recent anchors plus Fixed/Panel) without widening position byte 4 beyond 0-3. Field **23** is a shared RAM-only page. Both remaining free masks are `0x80` in bytes 40 and 43. Event resets preserve layout; layout edits preserve hints/infinite mode and all event preferences. No export index or EventVars member moves.

Native X/Y cycles display choices, and L/R selects a primary-player preview page; the L press opening the menu is ignored for pagination. Lab appends OSD Display after existing global rows, so palette/override/overlay row offsets remain unchanged. Its style and Recent-position choices use the shared accessors. Common pause-menu L/R changes actual match pages (unless a custom submenu owns input). Pages never rotate with events or expiry; an out-of-range explicit choice is clamped. The page footer appears only for multi-page selections. Page changes do not dirty card data.

The bank has **114 possible keys**, but creates result objects only on emission and at most nine placeholder Texts, nine visible footer Texts and one page Text. It retains emitted off-page result objects to preserve native text/caller safety. Static mapping/history/lifecycle storage is **5,180 bytes**; actual result Text/JOBJ/engine heap use is additional and needs live profiling. Per-key replacement destroys the previous object once. Stock/generation changes, common restores and rewind clear stale results/history while retaining the selected layout. Automatic ordinary practice actions do not rotate pages.

The suite now has **109 PowerPC tests**, including actual native text parser/setter checks and fresh optimized DAT relocation. Layout coverage verifies positions across repetition/expiry, all 114 keys, manual pagination, no duplicate history callbacks, independent player/CPU ownership, reset/stock/rewind behavior, safe Recent imports (including same-update ordering), transitions, and persistence/free-bit isolation. Native on-screen font/background fit and dense-match heap/render cost still require Dolphin validation.

### Compact layout and shine deadline refinement

This supersedes the initial layout dimensions above. Fixed Grid uses **five columns / ten cells per page**. Practice Panel uses **nine rows per page** with pitch **3.4** (previously 5.3), a title wrapped into at most two smaller lines, a vertical separator, a **2.2x** latest-result subtext, and three previous results vertically at the right. The history bank keeps latest plus three previous entries. Ownership/recency is smaller and hitlag/turn context stays on a separate row beneath the large result. Native source details such as Wavedash angle/hop retain their original colors. Source rows remain valid for native callers; replaced title/timing rows are positioned outside the viewport with aspect fitting disabled, rather than deleting/reindexing them. Lifetime-bar hiding includes child joints.

MsgData appends the failure flag and bounded original title, growing **100 -> 140 bytes**. Layout/history storage grows **5,180 -> 6,140 static bytes**. No save bits are added: format 3/44 bytes still has two reserve bits. Native editor pagination uses the shared capacity helper, so ESS and runtime use the same five-column/nine-row capacities.

Shine episodes append two bounded counters, growing **52 -> 60 bytes**, with twelve native context entries totaling **1,584 bytes** (previously 1,488). An unfrozen loop opportunity advances the fifteen-update deadline; mandatory startup/turn/reflection recovery and hitlag do not. Confirmed jump or second turnaround on the boundary takes priority. Holding shine times out once as red `FAIL`; an early B release keeps its pending deadline through the end/wait state so it also fails after fifteen counted updates. A new shine, death or restore/rewind cancels old context. The deadline spans both sides of a first turnaround instead of resetting when the loop resumes. Failure metadata preserves hitlag/first-turn prefix and is also retained as a typed red FAIL in panel history.

**114 compiled PowerPC regressions pass**, including deadline/boundary/freeze/release cases, native failure formatting/parsing and compact panel metrics/history columns. The optimized release is warning-free and packaged modules are checked separately. Native visual fit remains a Dolphin check.

## Card autosave resource lifecycle

Do not assume that `Memcard_SaveIfChanged` can start a write from a match scene. Native global polling calls it too. The reported PC **0x8001C868** is an indexed icon-table read through **0x80433318 + 0x5C**; when the archive is unloaded it reads NULL + 4. Continuing after that warning reaches the native `_p(work_area)` assertion in `lbcardnew.c`.

New C2 guards at **0x8001CC84** (save poll) and **0x8001CDB4** (synchronous drain) return before the original routine if the card archive enable/icon table or work/transfer buffers are unavailable. They preserve the dirty flag. Guarding the synchronous drain also prevents spinning forever on a deferred native dirty flag. Initialized resources continue into the original native code and its error/state/checksum handling.

`Settings_Set` and migration/repair queue a private RAM pending flag. They set native dirty only when the card service is ready, avoiding invalid autosave during paused settings edits. Archive teardown may reset native dirty, so the pending flag is independent. Appended export **38, Settings_CommitPending**, is called from the Event Select think hook with full register/FPR preservation. It requeues dirty after native card initialization and acknowledges only a request that was actually queued/accepted; an unrelated in-flight request cannot acknowledge a match edit. No card file is opened or retagged by the settings service. Return to Event Select after editing to let normal autosave complete.

`MemcardState.memcard_changed` (+0xC) and `enable` (+0x18) are **four-byte native flags**; the SDK's byte `bool` declarations were wrong. They are now `int`, with assertions for widths/offsets and unchanged +0x5C table placement. The record still occupies **44 bytes / format 3**, with two reserve bits, and upstream identity isolation remains unchanged.

**121 PowerPC tests pass**. New tests execute the real DOL card routines, reproduce the exact old invalid read, execute both patched guard bodies, preserve edits across archive teardown, exercise ready/missing/error states and async request acknowledgement, and verify the ESS export after optimized DAT relocation. Device request/time/language leaves are bounded fixtures; no real user card is written. Live Dolphin editing, Return-to-ESS saves, no-card/error prompts and reload remain runtime checks.

## Character-specific layout children

Fixed Grid and Practice Panel now reserve separate adjacent **Jump Out Of Shine / JC Shine** cells for Fox and Falco. Both read category **8** for title palette and suppression, while raw message kind **64** selects the JC child. The child has no new saved OSD setting. Other characters keep one ordinary character-specific cell. Recent still uses its existing independent kinds 8/64.

Runtime keys have **20 slots per owner**, with the JC child at internal slot 19. `TMOSD_MessageKey` chooses a key from category plus kind; `TMOSD_Category` maps either child back to setting 8. The cell map inserts the active child directly after its parent in display order, before the next canonical category. `split_owners` records live Fox/Falco owners, so it reserves the child before either action emits. Per-owner counts/page ranks, replacement/import, history, placeholder names and stock/disable cleanup all use the new mapping. Both cells disappear when the single character-specific setting is disabled. Native ESS preview allows the extra possible child before CSS; actual match pagination uses the live roster.

The bank grows **6,140 -> 6,456 static bytes**, with at most **120 possible keys**. MsgData and the saved record are unchanged: **44 bytes / format 3 / two reserve bits**. Latest/history/footer objects remain bounded as documented above.

The native JC Shine producer preserves **r9's printed hop duration in r25** across Message_Display, rather than using r7's JC Shine timing. Its old short/full-only color table is replaced by the existing shared `OSD_WavedashHopColor` export 35. Its local short=0/full=1 flag is inverted to the helper's short=1/full=0 convention. The temporary color uses free stack space outside backupall's saved GPR region. Short Hop 1f is cyan, other Short Hops green, Full Hop red at every timing.

**124 PowerPC regressions pass**, including independent child replacement/history, both layouts and characters, early reservation, non-Fox/Falco exclusion, shared disabling, 120-key mapping, and execution of the actual assembled JC producer with mismatched shine/hop counts and preserved r25. Native visual confirmation remains a Dolphin check.

## Panel readability refinement

Practice Panel title scales increase **0.72 -> 1.05** with greater separation between its two wrapped lines. Native detail viewport scale increases **0.019 -> 0.027**; visible details are counted independently of hidden source timing/title rows, so Wavedash angle/hop or landing-source information uses the freed status space. Empty placeholders use the same enlarged title geometry. The 3.4-unit row pitch and independent shine cells remain.

Both stable layouts remove player/New/Last/waiting status text. Fixed Grid needs no supplemental footer Text. Practice Panel keeps its title/history/separator presentation, but uses a separate **single timing row** containing the full colored `Nhl->Ntrn->Nf`/FAIL stream. All parts share scale 2.2 and fit uniformly inside the timing column; there is no tiny prefix row or independently centered fragment. Title sizing is isolated from timing fitting. History keeps three previous entries. Actor ownership and suppression remain encoded in the bank despite removing visible ownership status.

MsgData adds one timing Text pointer, **140 -> 144 bytes**. That Text is allocated only for visible timed panel results and destroyed on replacement, page hiding, clearing or transition to Fixed/Recent. Maximum supplemental presentation/timing Texts is two per visible panel row; placeholders/page remain bounded. The 44-byte save format, two remaining reserve bits and safe pending-card lifecycle are unchanged.

**126 PowerPC tests pass**, including full-stream native parsing/replacement, larger title scale, missing status/prefix rows, transitions and bounded repeated timing-Text allocation. The release and packaged payloads are verified separately. Final spacing needs native Dolphin visual confirmation.


## Global character persistence

`TM_SETTING_CHARACTER` is append-only field 24, index 0 human / 1 CPU. Values are external IDs 0-25, or UINT32_MAX for unset. Byte 6 retains the recommended flag in bit 0; bits 1-5 store human ID+1 and bits 6-7 mark the extension as `10`. Byte 37 bit 0 belongs to the last title palette; bits 1-5 store CPU ID+1. Marker initialization clears unowned CPU padding before exposing it. Codec validation bounds codes and preserves adjacent palette/settings bytes. Older 0/1 recommended readers may discard this extension when downgrading, so retaining characters through an older Tyro build is not guaranteed.

The record is still 44 bytes, format 3. The feature uses twelve previously unused bits; two palette bits plus the two explicit reserved bits remain (four available audited bits). No score region or save-file growth is used.

`Settings_EventCharacters` appends export 39. CSS prepare/confirm hooks preserve all GPR/FPR/control state around it. The wrapper validates event/page IDs, obtains the current whitelist and CPU-selectability, and calls `Settings_Characters` to synchronize RAM-only CSS backups with the shared record. Restricted events do not overwrite general preferences. Their human selection is no longer cleared after the restore service selects an allowed fighter. Native forced-opponent preload/match rules remain effective. Deferred dirty/commit semantics stay intact; no in-match raw card write is added.

132 PowerPC tests pass, including all 676 pairs through fresh record reloads, adjacent palette and recommended edits, malformed/unset/foreign data, restricted fallback, costume reuse/reset, and actual emitted native CSS hooks. Real card writes, cold boot, controller-port selection and gameplay round trips need Dolphin verification.


## Compact OSD arrows and JC Shine hitlag

The native ASCII converter now transforms contiguous `->` into an eleven-byte balanced run: `0E FFFF FFFF`, original native `0B 20FC 2100` glyph/control bytes, then `0F`. The private scale marker means 75% of the enclosing X/Y scale. Width hook `0x803A824C` and draw hook `0x803A8DB8` apply the same relative multiplier after the native scale push. Ordinary scale commands retain the native absolute handling. Native scale pop restores the enclosing state; this works when Practice Panel uses scale 2.2. Current font scaling is preserved rather than assuming a 1.0 row.

The existing SetText old-length hook at `0x803A7068` skips the complete eleven-byte private run. Its inner `0F` must not terminate the old row. Color parsing still consumes opcode plus exactly three RGB bytes. The source text remains ASCII `->` for all producers; standalone hyphen/greater-than glyphs are untouched, and all users of the shared converter gain the compact pair.

Message creation skips hitlag capture for JC Shine kind 64. `OSD_TimingText` independently suppresses that prefix for retained/imported JC messages. Jump Out Of Shine kind 8 retains hitlag. No data layout, export order, save format or storage budget changes: 44 bytes, format 3, four available audited bits.

135 PowerPC tests pass. New tests execute native conversion, width, nested scales, native replacement/row-location and the actual assembled draw hook plus native scale push/pop. They verify relative scaling at multiple row sizes, restored size for following numbers, unchanged standalone glyphs and JC/Jump Out prefix differences. Final readability remains a Dolphin check.


## Marth/Roy counter-active overlay

`ActionCues_IsProtected` includes Marth/Roy counter stance IDs 369/371 only when `flags.shield_enable` and `flags.shield_x40` are both set. Native shield initialization at 0x8007B1B8 enables mask 0x80 in byte 0x221B; shared ground/air counter animation callbacks at 0x80138AA8/0x80138B64 add mask 0x40. They clear mask 0x80 when their command variable closes the window, leaving 0x40 latched. Never treat the latter alone as active protection. Retaliation states 370/372 and other fighters do not satisfy this exception; existing hurt status/whole-body/Yoshi detection is preserved.

The common renderer keeps player-colored translucent tint and its existing priority/toggle logic. No ABI, exports, combat logic or persistence allocation changes. Save storage remains 44 bytes / format 3 with four available audited bits.

137 PowerPC tests pass. The new native-callback test executes the real ground/air counter callbacks and shield initialization against fixture collision data; only the animation-end query is stubbed. Additional rendering checks cover active flags, recovery, state/kind exclusion and live global disabling. Cold Dolphin frame-step validation remains a visual check.


## Side-B Sweetspot distance guides

Side-B Sweetspot schedules a 30-frame retry after an attempted aerial Side-B enters Landing (42), LandingFallSpecial (43), or grounded Side-B startup/dash/end (347-349 with ground flag). The check runs before CPU AI, independently of Marth's current animation, and restores his animation speed so platform/onstage landings cannot strand a frozen opponent. Existing pending resets are not replaced: death/manual repositioning and the original 60-frame ledge-catch retry retain their behavior. Optimized relocated-event regressions execute the actual state machine to check landings before/after the freeze, countdown completion, no-attempt/airborne exclusions, immediate death and ledge-delay preservation.

`Enable Line Guides` uses the original ledge-grab height and success/failure colors, with a finite line and numbered endpoints including endlag drift. It faces the travel direction and anchors at the launch position during an attempt. Frame labels 1/2 share the shortest endpoint, followed by 3, 4 and 0 (unshortened). World-space numerals use 65% of their initial size. The former underside guide is commented out under `Extra Underside Distance Markings, Totally Unnecessary`, including its menu row, drawing function and hooks. Vanilla Fox/Falco drift-inclusive ranges and the unused shorter dash-only ranges are preserved in comments in `src/sweetspot.c`.

Appended settings field 25, index 0, saves only Enable Line Guides. Byte 40 bit 7 stores its inverse: zero means On, so fresh and existing zero-padded format-3 records default On without an initialization write. Versions 1/2 explicitly clear the formerly unowned bit during migration. An explicit toggle changes only this bit; Ledgedash/Eggs initialization and resets preserve it. Loading sets both `val` and `val_prev`, and menu close/Exit use the existing deferred native memory-card save path. The commented-out underside option has no saved setting.

Side-B Sweetspot's **Camera** menu offers **Normal** (default) and **Zoomed Out**. The latter keeps normal tracking of both fighters, multiplies the stage's minimum/maximum camera distances by 1.5, and expands camera tracking bounds by 80 units horizontally and 40 vertically. Original values are captured per event, restored before every mode change, and restored by the drawing object's cleanup callback on scene teardown. Practice savestates do not restore these stage fields, so resets retain the selected camera. Native field meanings follow [StageCameraInfo](https://github.com/doldecomp/melee/blob/master/src/melee/gr/types.h) and the [normal camera's bounds/distance calculations](https://github.com/doldecomp/melee/blob/master/src/melee/cm/camera.c).

Appended field **32**, `TM_SETTING_SWEETSPOT_CAMERA`, index 0, stores Normal/Zoomed Out in byte 43 bit 7. The core remains 44 bytes / format 3 with one unused bit remaining (byte 37 bit 7); the extension is unchanged. Existing zero-padded format-3 saves default Normal without a write, and versions 1/2 clear the previously unowned camera bit during migration. Camera edits preserve guides, Eggs, Ledgedash, OSD layout and all other settings. Init loads both `val` and `val_prev`; menu close/Exit follow the existing deferred card-save path. PowerPC codec/service and relocated DAT tests cover reloads, invalid/future/foreign records, sibling preference edits, actual menu callbacks, noncompounding camera changes, saved initialization and scene cleanup. Visual framing on both ledges and real memory-card I/O still require Dolphin verification.

The record remains 44 bytes, format 3, with three audited free bits (byte 37 bits 6-7 and byte 43 bit 7); this supersedes earlier reserve counts. PowerPC and optimized relocated-DAT regressions cover default On, Off/On round trips, invalid writes, sibling-setting preservation, older-format migration and fresh-service reload. Visual rendering and real memory-card persistence still require Dolphin checks.

## Event-local dropped-invincibility highlight and Under Fire practice loop

Ledgedash's existing option 11/value slot is now **Highlight Dropped Invincibility**, retaining default On. `EventVars.set_tangibility_overlay` borrows the event's `s16` Boolean option; the shared renderer reads it at draw time, including while paused. It inverts `ActionCues_IsProtected`, uses the same RGB and eight-frame opacity pulse as the red diagnostics, and takes priority over timing/local body tints while vulnerable. While enabled it suppresses the global invincibility tint without writing its saved preference. Restore clears pulse history while preserving the event option; match/scene transitions discard the borrowed pointer. No global row, saved field or persistence bytes are added.

Under Fire retains its native assembly lava/ledge logic. Appended tmFunction export 40, `UnderFire_InitMenu`, creates its event-local C pause menu, defaults the highlight On, and installs a menu update callback. Hit/death handling retains the 30-frame feedback delay, then reloads the original fighter state, replaces the fighter on the selected ledge, resets lava height and its full 123-frame delay, and zeroes the visible refresh count. The native timer is hidden. It does not pause for success/failure or destroy its think GOBJ after a hit.

Export 41, `UnderFire_UpdateScore`, now accepts a perfect-refresh success signal from the pre-catch diagnostic, with no survival-time feed. Each valid exact-zero refresh increments the visible count, while a session maximum survives retries and includes the current attempt. A large centered gold **Perfect Refreshes: N** counter has a smaller **Session Best: N** count directly beneath it, both with black outlines and both hidden while paused. Session Best retains the highest count across retries and resets only on event initialization. On menu Exit, a positive session maximum replaces a lower count record, or replaces an incompatible old time record after setting the v5 format marker; the existing deferred memory-card save then runs. The event ID is captured at initialization. The marker uses one padding bit, and event-local suppression uses the retired Act runtime option; no settings bytes or completion screens are added. Native and relocated production-DAT tests cover scoring, duplicate/initial-catch exclusions, close misses, hidden readouts, retries, record migration, ties/lower scores, suppression and saving before Exit. Visual placement and a real card round trip remain Dolphin checks.


## Powershield Training recovery feedback

General Tech #10 now owns a laser-hitstun/shieldstun Yellow>Green cue, paired Act out of Laser / Powershield readouts, and a Help submenu. Yellow uses the final two unfrozen hitstun frames or the native shieldstun animation boundary and rate; green lasts the first two unfrozen recovery updates. Accepted projectile reflection skips shieldstun and goes directly green for four unfrozen updates. Green identifies recovery from the laser, while normal shield-action restrictions still apply. Help distinguishes projectile reflection from physical powershields: only the physical collision grants the GuardOff attack-cancel timer, and neither gives a shield-release dash cancel. Accepted reflection (0x8007758C), ordinary shield contact (0x800776A0), and accepted body contact (0x800780E0) feed a shared observer. Success requires the native accepted reflection branch, rather than the physical powershield bit or an action-state name. Native PlCo's reflector timer starts at 1 and expires below 0, producing two successful frames. Shield contact while it is active is labeled honestly, without guessing Early. The old failure-only OSD and unrelated-register write are removed.

Timing overlays also wrap the owning fighter's shield-effect renderer. Native effects 1047..1050 (models 11..14) share the unique alpha callback at 0x8005FAB8 and carry the fighter in Effect.parent. Only matching owner effects with the standard GXLink_Common callback are wrapped. All material TEV constant/tev0 RGB channels, including the powershield white flash, take the cue color during rendering; original RGB and animated alpha survive byte-for-byte. Shared materials are saved once. The four original EfCoData shield models have at most eight textures, within the 16-register snapshot capacity. Cue changes work while paused, newly spawned effects bind after simulation, restore/death/respawn remove tint, and scene cleanup detaches wrappers. No native hooks, exports or persistence bytes are added.

Export 49 now names `EventTiming_Observe` and retains the `TM_UnderFireTiming` / `TM_EventTiming` assembly offset. Timing phases 0..4 fan out to Under Fire and an optional event observer; phases 5..7 carry native projectile contacts. The shared table stays at 50 slots. EventVars appends observer registration and a timing-overlay setter. ABI stamps include `powershield_osd.h`; all modules rebuild together. The event's borrowed callbacks detach before module unload, and restore clears their episode. The independent timing tint takes precedence over normal global colors while preserving the native three-slot render snapshot.

Act out of Laser counts unfrozen IASA opportunities after hitstun/shieldstun, with frame 1 as the earliest action. Automatic transitions, shielding (including Yoshi's egg), walking, walking turns and stops do not complete it; the count continues until another action. Its timing colors are cyan for 1f, green for 2f, yellow for 3f and red for 4+f. Its source identifies Laser hit, Shield hit or Powershield. Event OSDs defaults On, global suppression Off, and Laser Recovery Cue On; the latter covers all three laser interactions. Fire speed, both inclusive random delay bounds, laser height, direction/D-pad selection, movement and Nana also persist. Equal delay bounds call HSD_Randi(1), never zero.

`TYEX` version 8 owns 20 bytes at 0x1AE0..0x1AF3. Version 7's complete first 16 bytes retain their settings (apart from the version/length header); bytes 16..19 initialize ten Powershield preferences, using bits 0..23 of their little-endian word. Versions 1..6 keep their earlier migrations and also initialize this appended word. Default word bytes are `00 14 B0 00`. The core is still 44 bytes / format 3, the audited extension region is 88 bytes with 68 unallocated, and scores/foreign or unknown records remain protected. See [Powershield Training](docs/POWERSHIELD-TRAINING.md) for the full measurements, hook contracts and validation scope.
