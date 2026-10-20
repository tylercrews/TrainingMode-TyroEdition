# Ordering Global OSDs

**Status: implemented in T6 (October 19, 2026); not yet checked in Dolphin.** Custom placement applies only to the **Fixed Grid** layout. The grid is one page of 6 rows × 5 columns, and each OSD can be given its own cell, so cells can be left blank. Cells are set on a third Global Settings page, **Fixed Grid OSD Positions**. The earlier ranking designs and the Z order mode are kept under [Rejected options](#rejected-options) for reference. Code-level details are in [DEVELOPMENT.md](../DEVELOPMENT.md#fixed-grid-positions-t6).

## Goal

Let users choose where each OSD appears in **Fixed Grid**, including leaving cells empty so related OSDs can be grouped. **Practice Panel** and **Recent** keep the default order.

## Before T6's grid change

- The order was the category slot order from `TMSettings_OSDCategoryID(slot)` (`src/settings.h`): the 19 core OSD IDs from `TMSettings_OSDIDs`, then A-Smash Release, then Act OoAttack. That makes `TM_OSD_CATEGORIES` = 21.
- `TMOSD_Cell` (`src/osd_layout.c`) gives each player an owner block. Inside the block, an OSD's index is the count of *enabled* slots before it, so turning one off shifts everything after it. Fixed Grid had 10 cells per page (5 × 2). L/R in the pause menu flips pages (`src/menu.c`). Owner blocks run on from each other, so a CPU's block can start mid-page.
- When a player is Fox or Falco (`sync_split_owners` in `src/osd_layout_game.c`), Fighter-specific Tech takes two cells. Its own cell becomes **Jump Out Of Shine** (side-B shorten shares it), and the runtime-only **JC Shine** child (`slot == TM_OSD_CATEGORIES`) gets the cell right after it. Other characters get one cell.
- Rendering doesn't need contiguous cells. `TMOSD_Cell` returns a page and cell that set x/y. The `--` placeholder is drawn only for cells some enabled OSD maps to, so a cell nothing maps to is already blank.

## Design: assigned cells

### Saved data

Each of the 22 placeable OSDs stores one 5-bit value: the 21 categories plus JC Shine.

| Value | Meaning |
|---|---|
| 0 | **Auto** |
| 1–30 | A cell: `row * 5 + column + 1`, rows 1–6 from the top, columns 1–5 from the left |
| 31 | Unused (repairs to Auto) |

All Auto is the original order, so old saves don't change.

### Placement in a match (Fixed Grid only)

1. **An OSD with an assigned cell** is always drawn in that cell. If it's turned off, the cell is left blank and nothing moves into it.
2. **Auto OSDs** only use the **top two rows**, so a user who never assigns a cell sees the original grid: ten per page, then the next page. On a player's first page they start after the *highest assigned cell in the top two rows*, in default order. That cell counts every OSD with an assigned cell, whether it's on or off, so turning an assigned OSD on or off never moves Auto OSDs. Unassigned cells before that point stay blank, which is how gaps are made. Auto OSDs still pack among themselves.
3. **JC Shine** has its own cell. It's drawn only while the player is Fox or Falco; otherwise its cell is blank. On Auto it follows Fighter-specific Tech. It is **not** tied to "the cell after Fighter-specific Tech", because that cell may be assigned to another OSD or Fighter-specific Tech may be in column 5.
4. **Extra owner blocks** (CPU OSDs when the CPU override is off) start on a fresh page and use the same layout. With every OSD on Auto, blocks run on as before.
5. **CPU labels:** with cells assigned, a CPU's page is labeled `CPU OSDs 2/2 - paused L/R: page` instead of tagging every cell.
6. **Page count** is the sum of each owner's pages: one for its assigned cells and first Auto cells, plus one per ten overflowing Auto OSDs.
7. **Bad saves:** a repeated or out-of-range cell falls back to Auto (the earlier slot keeps the cell).

### Editor: page 3, "Fixed Grid OSD Positions (3/3)"

- Titles are `Global OSDs (1/3)`, `Global Overlays & Gameplay (2/3)` and `Fixed Grid OSD Positions (3/3)`. **R** goes forward and **L** goes back (before this change both went forward).
- The OSD rows are in the same spots as on page 1. The two OVERRIDE rows in the bottom-right corner are replaced by **JC Shine (Fox/Falco)** and **Reset All to Auto**.
- Rows read `Name: Value`, e.g. `Wavedash Info: R1 C1` (row 1, column 1) or `L-Cancel: Auto`.
- The row color matches page 1: the OSD's own color if it's on, grey if it's off. An OSD that's off can still be placed; its cell stays blank until it's turned on. JC Shine shows Fighter-specific Tech's color.
- The checkbox shows whether the OSD is on, read-only. A/B never toggle it on this page. Reset All has no checkbox.
- **A** moves the row to the next free cell and **B** to the previous one, in the order Auto → R1 C1…C5 → R2 C1 … → R6 C5 → Auto, wrapping both ways. Cells held by another OSD are skipped, so two OSDs can never share a cell. To swap two OSDs, move one to a free cell or Auto first. With 22 OSDs and 30 cells, there's always a free cell.
- **Reset All to Auto**: the first A changes the row to `Press A Again`, and a second A puts every OSD back on Auto. B or moving off the row cancels.
- **Start** saves and exits like the other pages. X/Y still change OSD Display. The positions only matter when the display is Fixed Grid.

## Cost

### Save memory

The core record has only `TM_SettingsReserved[3]` (24 bits) free, so the positions are in the `TYEX` extension (`src/settings_extension.h`). Its region runs from `0x1AE0` to `0x1B38` (88 bytes). Version 15 used 36 of them (byte 35 is Low Laser's); version 16 adds the cells in bytes 36..49.

| Encoding | Size | Extension after |
|---|---|---|
| 22 × 5 bits (used) | 14 bytes (110 bits) | 50 of 88 |
| 22 × 6 bits (4 pages, 40 cells) | 17 bytes | 53 of 88 |
| One byte per OSD | 22 bytes | 58 of 88 |

The migration from 15 keeps bytes 0..35 and zero-fills the new bytes, which is all Auto. These bytes came out of the same 52 free bytes that saving Training Lab settings would need (see the README's investigate list); 38 are left.

### RAM

| Item | Size |
|---|---|
| Cells in `TMOSDMap` | 23 bytes |
| Per-cell placeholder/tag arrays (10 → 30 cells) | +320 bytes |
| Editor cell names and Reset flag | about 180 bytes |

Code is a few hundred bytes. Text-pool use goes down; see [Plan to keep text memory flat](#plan-to-keep-text-memory-flat).

## Grid size and memory (checked October 19, 2026)

**Decision: don't increase memory.** No 7th row, because it needs a sixth bit per OSD. Don't raise the text pool either. The grid stays at 30 cells, which keeps positions at 5 bits (14 bytes). T6 uses one page of 6 rows rather than 3 pages of 2 rows.

### Screen space

- **Camera:** the OSD camera is `hud_cobjdesc` in `dats/eventMenu.dat`. It's a perspective camera (fov 41.539°, aspect 1.2167, eye at z = 64 looking at the origin). At z = 0 the visible height is y = ±24.27, about 9.9 px per unit.
- **Clip line:** `Message_Init` sets `scissor_bottom = 400`, so OSDs are cut off below y ≈ −16.18 to keep them off the percent HUD. That leaves 40.4 units (400 px) of usable height.
- **Box height:** a grid box (joint scale Y = 3) is about 5.15 units tall. Recent stacks the same boxes exactly 5.15 apart.
- **Before T6's grid change:** rows at y = 18 and 11 left about 37 px above the top box and about 18 px between the rows.
- **T6:** rows at y = 20.7 − row × 6.65 (20.7, 14.05, 7.4, 0.75, −5.9, −12.55) leave about 10 px above row 1, about 15 px between rows and about 10 px above the clip line. The pause-menu page label moved into the gap between rows 2 and 3, because its old spot (y = −12) is now row 6.

| Rows | Row spacing | Gap between rows | Positions |
|---|---|---|---|
| 2 (before T6's grid change) | 7 | ~18 px | — |
| 6 (T6) | 6.65, top row at 20.7 | ~15 px | 30 cells, 5 bits, 14 bytes |
| 7 | ~5.75, top row at ~21.3 | ~6 px | 35 cells, **6 bits, 17 bytes** (rejected) |

- **6 rows at the old spacing:** they need 40.15 units, so they only fit pushed flush against both edges. At y = 18, 11 … −17, the sixth row is clipped.
- **7 rows:** moving up reclaims only about 3.3 units, so the spacing has to shrink until the boxes nearly touch.
- **CPU tags:** with tighter spacing, the per-cell CPU tags (drawn above the box) would overlap the row above.

### Text pool

All text in a match comes from Melee's text-library pool, not the main heap. That includes OSD results, placeholders, CPU tags, Info Display and the pause menu.

- **Size:** Melee picks the size per scene in `Heap_UpdatePersistentHeapsOnSceneChange` (`0x801a3f48`, calling `0x803a6048`); in-game it's 18 KB (`0x4800`). `ASM/training-mode/Misc/Increase In-Game SIS Text Heap.asm` raises it to **28 KB** (`0x7000`).
- **Failure:** the allocator (`0x803a5798`) is best-fit, and it calls `OSPanic`, which halts the game, when nothing fits. It also panics if the remaining block (the large unused block at the end of the pool) would be left with less than 12 bytes.
- **Cost per text:** a Text costs 160 + 16 + 128 bytes, each with a 12-byte block header, so about 340 bytes before it has any text. The string buffer grows in 128-byte steps, allocating the new buffer before freeing the old one.
- **Fragmentation:** freed blocks only merge back if they sit next to the remaining block, so the pool can fragment over a long session. A buffer that keeps growing leaves every old size behind as a separate free block.
- **Addresses:** pool size at `0x804D7968`, free list head at `0x804D7970`, used list at `0x804D7974`. Each block header is `{next, data, size}`.

What Fixed Grid keeps in the pool:

| Text | Kept | Approx. cost |
|---|---|---|
| OSD results | The latest per player and OSD, **on every page**. Hidden pages still hold them, because native producers edit the text after `Message_Display` and flipping pages has to show them. | ~0.4 KB each |
| `--` placeholders | T6: one per shown cell without a result, freed when the result arrives or the cell leaves the page. 2 subtexts in Fixed Grid (9 in the Panel). Before T6's grid change: kept for the match, 9 subtexts. | ~340 B each (was ~470 B) |
| CPU tags | Per cell index, shared across pages. With cells assigned, replaced by the page label. | ~340 B each |
| Practice Panel footer/timing | Freed when off-page. Fixed Grid creates none. | — |

- **Page layout doesn't change result memory.** Result cost depends on how many OSDs have fired and for how many players, not on how many cells or pages there are.
- **Estimated totals:** one player with everything on is about 9 KB, and adding a CPU with Override CPU OSDs Off turned off makes it about 18 KB. Four humans in VS with everything on could reach about 35 KB, more than the pool, with or without this feature. All of these are estimates that still need measuring.

### Plan to keep text memory flat

- **Done in T6 — freed placeholders:** each cell holds at most one Text (its result or its placeholder), so the grid uses no more text memory than before T6's grid change for the same OSDs. The first plan, one shared placeholder Text with 2 subtexts per cell, was dropped: growing one buffer to about 3 KB in 128-byte steps would leave roughly 35 KB of unmergeable free blocks, more than the pool.
- **Done in T6 — one CPU label:** with cells assigned, CPU OSDs get their own page, so the page label replaces the per-cell tags.
- **Open — dev readout:** walk the free list and show free space and the lowest free space seen. Stress-test it: Fox vs a Falco CPU with CPU OSDs shown, everything on, Info Display on, the Training Lab menu open while flipping tabs, a long session, and a 4-player VS match.
- **Not planned for now — guard:** before an OSD result is created, check that the remaining block has at least a set reserve. The readout would decide the reserve, which must leave room for the pause menu and Info Display, since those aren't guarded. Producers dereference the object `Message_Display` returns, so the guard can't return null:
    - When a player and OSD already have a result, free the old one *before* creating the new one, so replacing a result never grows the pool.
    - When a first result arrives while memory is low, hand back one pre-made hidden message, sized for 3 × 32 characters so it never grows, and don't draw it.
    - Cost: a few hundred bytes of code, about 8 bytes of statics, about 0.5 KB of pool for the hidden message (part of the reserve) and nothing in the save.

## Still to check in Dolphin

- Row 1 at the very top and row 6 just above the percent display, on stages and in VS mode with a timer at the top.
- The page label in the gap between rows 2 and 3.
- Row text width on page 3 (`Item Throw Interrupts: R6 C5` is about as long as the longest color row).
- That leaving page 3 brings back the checkbox on page 1's `OVERRIDE ALL OSDS OFF`. Page 3 hides Reset All's checkbox the same way gap rows are hidden, which the native page reload has always undone.

## Rejected options

- **Order the enabled OSDs (Z "Select OSD Order" mode).** Turning an OSD off dropped it from the list, so toggling OSDs changed the order. The mode also depended on two unknowns: whether Z reaches `OSD_EditorInput` at all, and whether Start can be kept from exiting through `Start Exits Custom RSS.asm`. The page 3 design uses the existing L/R paging and A/B stepping, so it needs neither.
- **Global priority list (rank all 21 once).** It fixed the toggling problem but still packed OSDs left to right, so there were no gaps. 14 bytes at 5 bits, or 9 as a permutation index.
- **Ranked list with blank entries.** A blank moves whenever an OSD before it is turned off. 19 bytes (30 entries × 5 bits).
- **Cell map (each cell stores its OSD).** Same result as assigned cells, but 19 bytes.
- **Three pages of 2 rows.** Same 30 cells and save cost as 6 rows, but you'd have to flip pages to see your own OSDs.
- **Separate Practice Panel positions.** A second set of assignments for the Panel's 9 rows, +14 bytes. Dropped: the Panel is a list, and gaps don't help there.
