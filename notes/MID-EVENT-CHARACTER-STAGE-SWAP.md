# Swapping Characters or Stages Mid-Event

**Status (October 18, 2026): Option A is implemented for stages** in Eggs-ercise, Training Lab and Low Laser Puff Practice ([see below](#implemented-in-t5-change-stage)). Character swaps are still only investigated. Sources: TM source, the event-mode ASM hooks, and a disassembly of `build/Start.dol` (devkitPPC `powerpc-eabi-objdump`) checked against `GTME01.map`. Nothing has been tried in Dolphin yet.

## Question

Could an event like Low Laser Puff Practice have a menu setting with a few characters (or stages) that swap in and out, or does every change need a full reload? This also covers the open README item about other target characters and poses (dashing Marth, dashing Fox).

## Short answer

| What | Instant, in the same match? | Restart straight back into the event? |
| --- | --- | --- |
| Stage | No. Melee builds the stage once per match scene. | **Yes.** |
| Any character, any number of choices (player or target CPU) | No (see Option C). | **Yes.** Small change, recommended. |
| One fixed pair per player slot (e.g. Fox↔Falco) | **Possible** by reusing Melee's Zelda↔Sheik transform. Medium/large, needs Dolphin testing. | Yes. |

"Restart" doesn't mean going back through the menus. The event can end its own match and come straight back with the new choice, skipping the CSS and SSS. That's the same thing Eggs-ercise does when you switch to Free Practice. The cost is that load screen, plus Puff's percent and the timing readouts reset.

## How an event gets its characters and stage today

- `EventInit` ([src/events.c:1706](../src/events.c#L1706)) builds the match:
    - Player: `preload->queued.fighters[0]` (line 1786).
    - CPU: the event's fixed `cpuKind` (Puff for Low Laser, line 1767). It reads `preload->queued.fighters[1]` only for `SLCHRKIND_TRAINING` events (lines 1769–1772).
    - Stage: `preload->queued.stage` when the event's stage is -1, which Low Laser's is (line 1811).
- `EventInit` runs from the hook in `ASM/training-mode/Custom Events/Custom Event Code - Rewrite.asm`, at `0x801bb128` inside `ScenePrep_EventMode_InGame`. That runs every time the in-match scene starts, **including retries**.
- `Retry()` ([src/eggs.c:49](../src/eggs.c#L49), same in [src/slalom.c:27](../src/slalom.c#L27)) sets `stc_match->end_kind = MATCHENDKIND_RETRY` and ends the match. `SceneDecide_EventMode_InGame` (`0x801bb758`) sees end kind 8 and writes minor scene 1, the in-match scene, as the next scene (`0x801bb830`). The CSS and SSS are skipped.
    - That retry path rewrites the preload table (`0x801bb804`) only when a vanilla multi-match event's round counter (memcard `+0x550`) is non-zero. TM events don't use it, and Eggs/Slalom already retry with any character.
- The event's own `.dat` is reloaded for every match (`EventLoad`), so statics in `lowlaser.c` don't survive a restart. The preload table is ordinary RAM and does. Saved settings (Puff Position, Height, OSDs) already reload from the memory card.

## Option A: restart into the new choice (recommended)

1. Add **Character** (player), **Target** (CPU) and/or **Stage** rows to the event's pause menu.
2. On change, write the choice into the preload table: `preload->queued.fighters[0].kind/.costume`, `.fighters[1]`, or `.stage` (external `GRKINDEXT_*` ID, as Lab's recording import writes). Call `Preload_Update()` during the next match prep, not mid-match, where it could evict the running stage; native event prep and the Salty Runback code call it between matches.
    - TM's `EventInit` hook jumps past the in-match prep's own preload/audio update (`EventMode_updateGameAndAudioCache` calls at `0x801bb688`–`0x801bb734`), so nothing else would request the new files.
3. Call the same `Retry` as Eggs.
4. For the target CPU only: let Low Laser's `EventInit` path read `fighters[1]` the way the Training-CSS path already does. When you enter the CSS, the "Load CSS + Preload CPU" hook already sets `fighters[1]` to the event's CPU (Puff), so the default stays Puff.

What it gets for free:

- **Sounds.** `StartMelee` calls `Audio_LoadStageAndCharAudio` (`0x8002785c`). That builds the sound-bank list from the live player blocks, so the new character's sounds load even though the CSS's "Load Character ssm Files" hook is skipped.
- **HUD, camera, collision, disabled hazards.** The new match rebuilds all of them. Low Laser's positions already come from live geometry, so a new stage needs nothing extra.

Watch out for:

- **Costumes.** Clamp the costume to 0 if the new character has fewer costumes than the old index. Avoid the CPU's costume matching the player's.
- **Zelda/ICs as the target.** `EventInit` turns a Zelda CPU into Sheik (line 1776), and an Ice Climbers target would bring Nana.
- **Load time.** It's the same load as the Eggs-ercise mode switch. A possible speed-up: the preload table has 8 fighter slots, and the event only uses 2. `Preload_CheckWhichGameFilesToPreload` (`0x80017960`) loads every filled slot, so putting the menu's choices in the spare slots should keep them in memory between restarts. **Unverified:** how much memory that leaves, and whether anything clears the spare slots.
- **CSS cursor.** On exit, the CSS still shows the character originally picked there, unless the CSS backup is updated too. Cosmetic.

## Option B: instant swap for one pair (borrow Zelda↔Sheik)

Melee already swaps characters mid-match, but only Zelda↔Sheik.

- **Character table** at `0x803BCDE0`: 3 bytes per external character (main internal ID, sub internal ID, flag).
    - Zelda = {Zelda, Sheik, 1} and Sheik = {Sheik, Zelda, 1}.
    - Ice Climbers = {Popo, Nana, 0}. Flag 0 means both fighters are active at once.
    - Every other character has no sub.
- **`LoadCharacter`** (`0x80031cb0`) loads files for both main and sub, so Sheik's files are in memory from the start of a Zelda match.
- **`SetupPlayerSlot`** (`0x80031ad0`) creates the main fighter (player block `+0xB0`). If the character has a sub, it creates a second fighter (`+0xB4`), which starts asleep when the flag is 1.
- **The transform** (`0x8007efc8`, MexTK `Fighter_Transform(gobj, enter_state_cb)`):
    - Swaps the slot's active index.
    - Copies percent, position, item effects (starman/metal/cloak/flower), camera box, ECB and ledge boxes to the other fighter.
    - Puts the old fighter to sleep (`AS_011`) and calls the callback to set the new one's state.
    - None of its calls are Zelda/Sheik-specific; the character-specific part is the transform move that calls it.
- **`Fighter_GetGObj(ply)`** (`0x80034110`) returns `gobj[isTransformed[0]]`, i.e. the active fighter. So event code that uses `Fighter_GetGObj(0)` follows the swap.
- **Savestates.** TM's savestates already save and restore both fighters in a slot ([src/savestate_v1.c:63](../src/savestate_v1.c#L63), [:222](../src/savestate_v1.c#L222)).

To get Fox↔Falco in Low Laser:

1. For this event only, change Fox's table entry to {Fox, Falco, 1} before the match is set up, and restore it on exit. Every mode reads this table. The preload pass and `SetupPlayerSlot` would then load Falco and create him asleep.
2. Request Falco's sound bank too. Zelda and Sheik share one bank (`0x08000000`), but Fox (`0x800`) and Falco (`0x400`) don't. TM's CSS ssm hook could OR it in.
3. Swap with `Fighter_Transform` and a callback that enters Wait (grounded) or Fall (airborne).
4. Refresh anything the event computed once per match from the character, such as Low Laser's "Laser Start" vs "Neutral-B Start" label.

Unknowns to check in Dolphin:

- Whether the HUD stock icon updates.
- Whether `PlayerBlock_SwapActiveTransformationID's` (`0x80034218`) asserts on a non-Zelda slot.
- Code that reads the player block's external ID would still say Fox while Falco is active.
- Memory. A 4-player match can hold Zelda+Sheik and Popo+Nana plus two other characters, so the fighter cache should have room for about six characters' files. Fox + Falco + Puff should fit.

The same trick works for the target slot (e.g. Puff↔Marth), but Option A covers that more simply.

## Option C: instant swap among 3+ characters (not recommended)

A player block holds only two fighters. Both ways around that are risky:

- **Extra player slots (ports 3–6)** for the alternates, created asleep and reading the human's controller. Each would get a HUD percent to hide. A lot of TM code (OSDs, input display, recording) assumes the human is player 0.
- **Creating and destroying fighters mid-match.** Melee never does this. Projectiles, grabs, hit-victim lists, the camera and savestates all keep pointers to fighters.

## Stages

Stages need Option A. The match scene builds the stage's models, collision, camera bounds, blast zones, lighting and music once. The only partial exception in Melee is Pokémon Stadium's transformations, which use a special extra file allocation. Low Laser already works on any stage from live geometry, so a **Stage** row would only need the six legal stages mapped to internal IDs.

## Recommendation

Build Option A first. It covers the open README item (other target characters), player swaps and stages, with a small change that uses paths the build already exercises: `Retry` from Eggs/Slalom, and `EventInit` reading the preload table. Pose choices (e.g. a frozen dash for Marth or Fox) are a separate setting that would work the same way after any swap, like Puff's frozen `Squat` frame. Try Option B only if restarting feels too slow for back-to-back Fox/Falco comparisons.

## Implemented in T5: Change Stage

All three events offer the same **Change Stage** submenu ([src/stage_swap.h](../src/stage_swap.h)): Final Destination, Battlefield, Dream Land, Yoshi's Story, Fountain of Dreams and Pokémon Stadium. The current stage is greyed out and labelled **Current**.

**Shared restart** ([src/events.c](../src/events.c), boot-loaded code):

- `EventVars.restart_on_stage(stage)` records the stage, saves pending settings, and ends the match as a retry, like Eggs-ercise's `Retry`. It does nothing for events with a fixed stage.
- During the next match prep, `EventInit` writes the stage into `preload->queued.stage` and calls `Preload_Update()`, as native event prep does. Nothing is loaded mid-match.
- `EventVars.carry_store`/`carry_take` lend a 2 KB buffer that survives the restart. Only the restarted event can take it; any other event start discards it.
- The new members are appended after `shield_training`, so the ASM's `0xAC` offset is unchanged.

**What each event keeps:**

- **Eggs-ercise and Low Laser** keep everything they already save on the memory card. Nothing else is carried. Low Laser releases its OSD-suppression pointer before the restart, as its Exit does.
- **Training Lab** has over 200 unsaved option rows, so it carries them in the buffer (see [Why Training Lab needs a carry](#why-training-lab-needs-a-carry)):
    - **Rows:** General (except the saved Input Display), CPU, Tech, both Info Displays, Hitbox Trails, Character RNG, the CPU's recovery menu, Advanced Counter and Action Log.
    - **Extra state:** Custom TDI values, Custom OSD states with their names, and Action Log states with their names.
    - **How it restores:** on the first frame, before the start savestate, every value is set first. Then each changed row's own callback runs (camera, HUD, model and collision displays, percents and locks, CPU intangibility, trails, RNG). Rows whose callbacks rewrite sibling rows are restored as plain values instead: the chance rebalancers, the Info Display presets, and the Advanced Counter and Action Log page selectors. Their derived state (which rows are disabled, which page is shown) is then rebuilt.
    - **Not carried:** recordings, saved positions, the old stage's options (Stadium transformation, Fountain platform heights), and a recording imported on the CSS. These belong to the old stage. The shared Character RNG block is cleared as on Exit; the carried rows reapply it.
- **Reset to Default Settings** (Lab's Session and Exit tabs) reuses the same restore code. `Event_Init` snapshots the fresh values, including the current stage's own options, and Reset applies that snapshot in place. It also clears Custom TDI, Custom OSDs and Action Log states. Recordings, positions and saved preferences stay.

**Tests:**

- `tests/test_stage_swap.py` runs the production `events.c`: retry hand-off and preload update, fixed-stage refusal, carry lifetime.
- `tests/test_lab_session.py` copies the carry buffer between two fresh Lab loads. It also checks Reset, and which rows the lists include.
- `tests/test_lowlaser.py` and `tests/test_menu.py` cover the menus.

**Still to check in Dolphin:** the actual reload time; Fountain's reflection and Stadium's transformation allocations after a stage change; camera/HUD state reapplied on the first frame; and the Lab's menu text after a Reset.

## Why Training Lab needs a carry

Lab's `.dat` is re-read from disc for every match (`LoadRELDAT.asm` calls `0x80016c64`, then relocates). Its statics therefore start from their compiled defaults, and `Event_Init` also copies default tables over Info Display, Advanced Counter and Action Log. Only settings loaded through `TM_GetSetting` survive a plain restart: Input Display, Controls, overlays, OSD colors/layout, trail flags and Randall. `eventMenu.dat` is different. It's loaded once at boot into its own heap (`Load eventMenu.dat.asm` at `0x803753b0`), so its statics, like the carry buffer, live for the whole session.

## Address reference

| Address | Name (GTME01.map) | Why it matters |
| --- | --- | --- |
| `0x801bad70` | `ScenePrep_EventMode_InGame` | TM's `EventInit` hook is at `0x801bb128`, then jumps to `0x801bb738`. |
| `0x801bb758` | `SceneDecide_EventMode_InGame` | End kind 8 (retry) → next minor scene 1, the match (`0x801bb7a0`–`0x801bb838`). |
| `0x801ba938` | `EventMode_updateGameAndAudioCache` | Skipped by TM's hooks for C events. |
| `0x80017960` | `Preload_CheckWhichGameFilesToPreload` | Loads every filled entry in the 8-slot fighter table. |
| `0x80031cb0` | `LoadCharacter` | Loads main + sub internal IDs from `0x803BCDE0`. |
| `0x80031ad0` | `SetupPlayerSlot` | Creates main (`+0xB0`) and sub (`+0xB4`) fighters. |
| `0x80034110` | `Fighter_GetGObj` | Returns the active (transformed) fighter. |
| `0x8007efc8` | `Sheik/Zelda_Transform_CopyInfoAndSleep` | Generic copy and sleep; called by both transform moves. |
| `0x8002785c` | `Audio_LoadStageAndCharAudio` | Called from `StartMelee`; per-character sound banks at `0x803BB3C8` (16 bytes each). |
