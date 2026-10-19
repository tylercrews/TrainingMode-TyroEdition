# Ordering Global OSDs

**Status: design chosen (October 19, 2026), not implemented.** Custom placement applies only to the **Fixed Grid** layout. Each OSD gets its own saved grid cell, so cells can be left blank. It's edited on a third Global Settings page, **Fixed Grid OSD Positions**. The earlier ranking designs and the Z order mode are kept under [Rejected options](#rejected-options) for reference.

## Goal

Let users choose where each OSD appears in **Fixed Grid** (10 cells per page, 5 columns × 2 rows), including leaving cells empty so related OSDs can be grouped. **Practice Panel** and **Recent** keep the default order.

## Current behavior

- The order is the category slot order from `TMSettings_OSDCategoryID(slot)` (`src/settings.h`): the 19 core OSD IDs from `TMSettings_OSDIDs`, then A-Smash Release, then Act OoAttack. That makes `TM_OSD_CATEGORIES` = 21.
- `TMOSD_Cell` (`src/osd_layout.c`) gives each player an owner block. Inside the block, an OSD's index is the count of *enabled* slots before it, so turning one off shifts everything after it. Pages are `index / capacity`, and L/R in the pause menu flips pages (`src/menu.c`). Owner blocks run on from each other, so a CPU's block can start mid-page.
- When a player is Fox or Falco (`sync_split_owners` in `src/osd_layout_game.c`), Fighter-specific Tech takes two cells. Its own cell becomes **Jump Out Of Shine** (side-B shorten shares it), and the runtime-only **JC Shine** child (`slot == TM_OSD_CATEGORIES`) gets the cell right after it. Other characters get one cell.
- Rendering doesn't need contiguous cells. `TMOSD_Cell` returns a page and cell that set x/y. The `--` placeholder is drawn only for cells some enabled OSD maps to, and `hide_empty()` hides the rest. A cell nothing maps to is already blank.

## Design: assigned cells

### Saved data

Each of the 22 placeable OSDs stores one 5-bit value: the 21 categories plus JC Shine.

| Value | Meaning |
|---|---|
| 0 | **Auto** |
| 1–30 | A cell: `page * 10 + row * 5 + column + 1`, pages 1–3, rows Top/Bot, columns 1–5 |
| 31 | Unused |

All Auto is today's order, so old saves don't change.

### Placement in a match (Fixed Grid only)

1. **An OSD with an assigned cell** is always drawn in that cell. If it's turned off, the cell is left blank and nothing moves into it.
2. **Auto OSDs** fill the cells after the *highest assigned cell*, in default slot order, packed together like today. The highest assigned cell counts every OSD with an assigned cell, whether it's on or off, so Auto OSDs don't move when an OSD with an assigned cell is turned on or off. Unassigned cells before that point stay blank. That's how gaps are made.
3. **JC Shine** has its own assigned cell. It's drawn only while the player is Fox or Falco. Otherwise its cell is blank, or if it's on Auto, nothing is drawn for it. It is **not** tied to "the cell after Fighter-specific Tech", because that cell may be assigned to another OSD or Fighter-specific Tech may be in column 5.
4. **Extra owner blocks** (CPU OSDs when the CPU override is off) start on a fresh page after the previous owner's last used cell and use the same layout. When every OSD is Auto, keep today's run-on blocks so nothing changes for users who never touch the page.
5. **Page count** for pause-menu paging is the last page any owner's drawn OSD lands on, plus one.

### Editor: page 3, "Fixed Grid OSD Positions (3/3)"

- Titles become `Global OSDs (1/3)`, `Global Overlays & Gameplay (2/3)` and `Fixed Grid OSD Positions (3/3)`. **R** goes forward and **L** goes back. Today both go forward.
- The OSD rows are in the same spots as on page 1. The two OVERRIDE rows in the bottom-right corner are replaced by **JC Shine (Fox/Falco)** and **Reset All to Auto**.
- Rows read `Name: Value`, e.g. `Wavedash Info: Pg1 Top 1` or `L-Cancel: Auto`. The value is at most 9 characters, close to the longest color name, so the current 0.78 row scaling still fits. It uses **Pg** instead of **P1**, because P1 reads as Player 1.
- The row color matches page 1: the OSD's own color if it's on, grey if it's off. An OSD that's off can still be placed. Its cell stays blank until it's turned on.
- The checkbox shows whether the OSD is on, read-only. A/B never toggle it on this page.
- **A** moves the row to the next free cell and **B** to the previous one, in the order Auto → Pg1 Top 1…5 → Pg1 Bot 1…5 → Pg2 … → Pg3 Bot 5 → Auto, wrapping both ways. Cells held by another OSD are skipped, so two OSDs can never share a cell. To swap two OSDs, move one to a free cell or Auto first. With 22 OSDs and 30 cells, there's always a free cell.
- **Reset All to Auto**: the first A changes the row to `Reset All to Auto: A again`, and a second A resets every position. Moving off the row cancels.
- **Start** saves and exits like the other pages. X/Y still change OSD Display. The positions only matter when the display is Fixed Grid.

## Cost

### Save memory

The core record has only `TM_SettingsReserved[3]` (24 bits) free, so the positions go in the `TYEX` extension (`src/settings_extension.h`). Its region runs from `0x1AE0` to `0x1B38` (88 bytes), and version 15 uses 36 of them (byte 35 is Low Laser's).

| Encoding | Size | Extension after |
|---|---|---|
| 22 × 5 bits (chosen) | 14 bytes (110 bits) | 50 of 88 |
| 22 × 6 bits (4 pages, 40 cells) | 17 bytes | 53 of 88 |
| One byte per OSD | 22 bytes | 58 of 88 |

This needs extension version 16. The migration from 15 zero-fills the new bytes, which is all Auto. These bytes come out of the same 52 free bytes that saving Training Lab settings would need (see the README's T7 list).

### RAM

| Item | Size |
|---|---|
| Unpacked positions in `TMOSDMap` | 22 bytes |
| Reset confirm flag | 1 byte |

There's no scratch copy, because the page has no cancel mode. Code is a few hundred bytes.

## Grid size and memory (checked October 19, 2026)

**Decision: don't increase memory.** No 7th row, because it needs a sixth bit per OSD. Don't raise the text pool either. The grid stays at 30 cells, which keeps positions at 5 bits (14 bytes). Those 30 cells can be either 3 pages of 2 rows or one page of 6 rows. Which one is still open.

### Screen space

- **Camera:** the OSD camera is `hud_cobjdesc` in `dats/eventMenu.dat`. It's a perspective camera (fov 41.539°, aspect 1.2167, eye at z = 64 looking at the origin). At z = 0 the visible height is y = ±24.27, about 9.9 px per unit.
- **Clip line:** `Message_Init` sets `scissor_bottom = 400`, so OSDs are cut off below y ≈ −16.18 to keep them off the percent HUD. That leaves 40.4 units (400 px) of usable height.
- **Box height:** a grid box (joint scale Y = 3) is about 5.15 units tall. Recent stacks the same boxes exactly 5.15 apart.
- **Today:** rows at y = 18 and 11 leave about 37 px above the top box and about 18 px between the rows.

| Rows | Row spacing | Gap between rows | Positions |
|---|---|---|---|
| 2 (today) | 7 | ~18 px | — |
| 6 | ~6.75, top row at ~21.3 | ~16 px | 30 cells, 5 bits, 14 bytes |
| 7 | ~5.75, top row at ~21.3 | ~6 px | 35 cells, **6 bits, 17 bytes** (rejected) |

- **6 rows at today's spacing:** they need 40.15 units, so they only fit pushed flush against both edges. At y = 18, 11 … −17, the sixth row is clipped.
- **7 rows:** moving up reclaims only about 3.3 units, so the spacing has to shrink until the boxes nearly touch.
- **CPU tags:** with tighter spacing, the per-cell CPU tags (drawn above the box) would overlap the row above.

### Text pool

All text in a match comes from Melee's text-library pool, not the main heap. That includes OSD results, placeholders, CPU tags, Info Display and the pause menu.

- **Size:** Melee picks the size per scene in `Heap_UpdatePersistentHeapsOnSceneChange` (`0x801a3f48`, calling `0x803a6048`); in-game it's 18 KB (`0x4800`). `ASM/training-mode/Misc/Increase In-Game SIS Text Heap.asm` raises it to **28 KB** (`0x7000`).
- **Failure:** the allocator (`0x803a5798`) is best-fit, and it calls `OSPanic`, which halts the game, when nothing fits. It also panics if the remaining block (the large unused block at the end of the pool) would be left with less than 12 bytes.
- **Cost per text:** a Text costs 160 + 16 + 128 bytes, each with a 12-byte block header, so about 340 bytes before it has any text. The string buffer grows in 128-byte steps, allocating the new buffer before freeing the old one.
- **Fragmentation:** freed blocks only merge back if they sit next to the remaining block, so the pool can fragment over a long session.
- **Addresses:** pool size at `0x804D7968`, free list head at `0x804D7970`, used list at `0x804D7974`. Each block header is `{next, data, size}`.

What Fixed Grid keeps in the pool:

| Text | Kept | Approx. cost |
|---|---|---|
| OSD results | The latest per player and OSD, **on every page**. Hidden pages still hold them, because native producers edit the text after `Message_Display` and flipping pages has to show them. | ~0.4 KB each |
| `--` placeholders | Per cell index, shared across pages, created when first needed and kept for the match. 9 subtexts, because the code is shared with the Practice Panel. | ~470 B each |
| CPU tags | Per cell index, shared across pages | ~340 B each |
| Practice Panel footer/timing | Freed when off-page. Fixed Grid creates none. | — |

- **Page layout doesn't change result memory.** Result cost depends on how many OSDs have fired and for how many players, not on how many cells or pages there are.
- **Estimated totals:** one player with everything on is about 9 KB, and adding a CPU with Override CPU OSDs Off turned off makes it about 18 KB. Four humans in VS with everything on could reach about 35 KB, more than the pool, with or without this feature. All of these are estimates that still need measuring.

### Plan to keep text memory flat

- **One placeholder text:** draw all Fixed Grid `--` placeholders from one shared Text with 2 subtexts per cell, instead of one 9-subtext Text per cell. That's about 2 KB for a 30-cell grid, less than today's ten placeholders (about 4.6 KB).
- **One CPU label:** CPU OSDs get their own page with assigned cells, so a single "CPU" page label replaces the per-cell tags.
- **Dev readout:** walk the free list and show free space and the lowest free space seen. Stress-test it: Fox vs a Falco CPU with CPU OSDs shown, everything on, Info Display on, the Training Lab menu open while flipping tabs, a long session, and a 4-player VS match.
- **Guard (proposed):** before an OSD result is created, check that the remaining block has at least a set reserve. The readout decides the reserve, which must leave room for the pause menu and Info Display, since those aren't guarded. Producers dereference the object `Message_Display` returns, so the guard can't return null:
    - When a player and OSD already have a result, free the old one *before* creating the new one, so replacing a result never grows the pool.
    - When a first result arrives while memory is low, hand back one pre-made hidden message, sized for 3 × 32 characters so it never grows, and don't draw it.
    - Cost: a few hundred bytes of code, about 8 bytes of statics, about 0.5 KB of pool for the hidden message (part of the reserve) and nothing in the save.

## Implementation notes

- **Storage index for JC Shine.** JC Shine's runtime slot is `TM_OSD_CATEGORIES`, which moves when a category is appended. Give it a fixed storage index, for example entry 0 with categories at `1 + slot`, so a new OSD can be appended later without reading JC Shine's position as its own.
- **Layout.** Add the unpacked positions to `TMOSDMap`, and fill them where `sync_split_owners` already updates the map. `TMOSD_Cell` uses the assigned cell if there is one. Otherwise it uses `highest assigned + 1 +` the count of enabled Auto OSDs before it. Only Fixed Grid reads the positions. Practice Panel keeps the current index code. Change `TMOSD_PageCount` to use the highest drawn cell instead of `count / capacity`.
- **Editor rows.** `TM_EDITOR_PAGES` becomes 3, with a third row table in `TMSettings_EditorIDs`. JC Shine and Reset All need row IDs above 31, so `row_names[32]` grows. `refresh_row` formats the position when the page is 3.
- **Native row Booleans.** `switch_page` and the native Start exit save `TM_SETTING_NATIVE_ROW` by row ID, and page 3 reuses the OSD IDs. Page 3's row bytes and `RSS_SELECTED` have to mirror each OSD's on/off state so that saving them changes nothing. The JC Shine and Reset rows must be skipped.
- **Input.** `OSD_EditorInput` on page 3 steps `TM_SETTING_OSD_POSITION`, skipping cells held by other OSDs. The L/R direction change is in `switch_page`.
- **Tests.** `TMOSD_Cell` is host-testable. Cover all-Auto matching today's order (including run-on CPU blocks), an assigned OSD that's off leaving a blank, Auto OSDs after the highest assigned cell, JC Shine with and without the Fox/Falco split, page-aligned CPU blocks, page count with gaps, the free-cell stepping, and migration from version 15.

## Rejected options

- **Order the enabled OSDs (Z "Select OSD Order" mode).** Turning an OSD off dropped it from the list, so toggling OSDs changed the order. The mode also depended on two unknowns: whether Z reaches `OSD_EditorInput` at all, and whether Start can be kept from exiting through `Start Exits Custom RSS.asm`. The page 3 design uses the existing L/R paging and A/B stepping, so it needs neither.
- **Global priority list (rank all 21 once).** It fixed the toggling problem but still packed OSDs left to right, so there were no gaps. 14 bytes at 5 bits, or 9 as a permutation index.
- **Ranked list with blank entries.** A blank moves whenever an OSD before it is turned off. 19 bytes (30 entries × 5 bits).
- **Cell map (each cell stores its OSD).** Same result as assigned cells, but 19 bytes.
- **Separate Practice Panel positions.** A second set of assignments for the Panel's 9 rows, +14 bytes. Dropped: the Panel is a list, and gaps don't help there.
