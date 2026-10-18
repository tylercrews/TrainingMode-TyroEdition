# Ordering Global OSDs

**Status: deferred design (October 17, 2026).** Nothing here is implemented. The feature is on hold because it costs RAM and about 21 bytes of save memory. This note records the design so it can be picked up later without redoing the investigation.

## Goal

Let users choose the order OSDs appear in the layouts that place them in fixed positions: **Fixed Grid** (10 cells, 5×2, per page) and **Practice Panel** (9 rows per page). **Recent** isn't affected because it orders by time.

## Current behavior

- The order is the category slot order from `TMSettings_OSDCategoryID(slot)` (`src/settings.h`): the 19 core OSD IDs from `TMSettings_OSDIDs`, then A-Smash Release, then Act OoAttack. That makes `TM_OSD_CATEGORIES` = 21.
- `TMOSD_Cell` (`src/osd_layout.c`) gives each player an owner block. Inside the block, an OSD's index is the count of *enabled* slots before it. Pages are `index / capacity`, and L/R in the pause menu flips pages (`src/menu.c`).
- The runtime-only JC Shine child slot (`slot == TM_OSD_CATEGORIES`) is placed immediately after Fighter-specific Tech (slot 4, setting 8) when that player has it split.

## Option A: order the enabled OSDs (Z "Select OSD Order" mode)

This mode lives in the Global OSDs page of the native-RSS editor (`src/osd_editor_game.c`).

| Input | Behavior |
|---|---|
| **Z** (outside the mode) | Enter order mode. Copy the saved order into a scratch buffer so it can be restored on cancel. |
| **A** | Give the cursor row's OSD the next number. Rows for OSDs that are turned off (and page 2 rows, gaps and the OVERRIDE rows) are greyed out and can't be picked. A plays the error sound on them. |
| **B** | Backspace: remove the most recent number. |
| **Start** | Save the order, leave order mode and return to the normal Global OSDs view. It does **not** leave the screen. |
| **Z** (inside the mode) | Cancel: restore the scratch order and leave order mode. |
| **L/R** | Ignored while in order mode, because every OSD is on page 1. |

Rows show the pick number instead of the color while in the mode, e.g. `Wavedash Info: 3` or `Wavedash Info: —`.

### Decisions

1. **OSDs left unnumbered** follow the numbered ones in default slot order. Numbering just your top three and pressing Start is a valid use.
2. **Turning an OSD off removes it from the saved list.** If it's turned back on, it goes to the end, and the user re-orders if they care.
3. **An on-screen hint is required.** For example, in the title area: `Select OSD Order — A: pick  B: undo  Start: save  Z: cancel`. `refresh_title` already scales that text when it's long.

Decision 2 is why Option B came up. Option A ties the order to the on/off state, so turning things on and off changes the order.

## Option B: global priority list (preferred direction)

The user ranks **all 21 OSDs once**, whether or not they're turned on. The on/off toggles then only choose which OSDs are drawn, and the drawn ones always follow the global priority. Turning an OSD off and on again never loses its place.

Differences from Option A:

- In order mode every OSD row can be picked, not only the enabled ones.
- Turning an OSD on or off never changes the list, so decision 2 no longer applies.
- Unranked OSDs still fall back to default slot order after the ranked ones.
- `TMOSD_Cell` walks the priority list and skips OSDs that are disabled or don't belong to this owner. The rest of the layout code doesn't change.

The editor controls stay the same as Option A.

## Cost

### Save memory

The core record has only `TM_SettingsReserved[3]` (24 bits) free, which is too little. The `TYEX` extension (`src/settings_extension.h`) is the right home. Its region runs from `0x1AE0` to `0x1B38` (88 bytes), and version 15 uses 36 of them.

| Encoding | Size | Notes |
|---|---|---|
| One byte per slot, `0xFF` terminator | 21 bytes | Simplest. Extension size goes from 36 to 57. |
| 5 bits per slot | 14 bytes | Bit packing, with 31 as the terminator. |
| Lehmer code (permutation index) | 9 bytes (66 bits) | Full order of all 21 only, which suits Option B. More code and harder to debug. |

Whichever encoding is used, this means extension version 16 plus a migration. An empty or all-zero list means default order, so old saves behave as they do today. Slots are append-only (new runtime categories get added at the end), so a stored slot index stays valid when new OSDs are added later. Unknown or new slots fall back to default order after the list.

### RAM

| Item | Size |
|---|---|
| Saved list (runtime copy) | 21 bytes |
| Scratch copy for Z cancel | 21 bytes |
| Pick count and mode flag | 2 bytes |

The total is about 45 bytes plus a few hundred bytes of code. The scratch copy is only needed while the editor is open, so it could reuse editor-only memory.

## Implementation notes

- **Z input (main unknown).** `OSD_EditorInput` receives remapped RSS menu bits, not pad bits. The known ones are A `0x200`, B `0x20`, L `0x40`, R `0x80` and Start `0x100`. It hasn't been confirmed whether Z produces a menu bit, or whether the native think calls the hook at all when only Z is pressed. If it doesn't, read `HSD_TRIGGER_Z` from the master pad in a per-frame path on this screen. **Check this in Dolphin first.** Dev builds also use L/R+Z to toggle the console.
- **Start.** `ASM/training-mode/Onscreen Display/Toggle UI/Start Exits Custom RSS.asm` treats Start as the exit test before the C hook runs. In order mode it must not exit. Add a check on an order-mode byte so C handles Start instead.
- **Row text.** `refresh_row` already formats `"%s: %s"` and sets the row color. Order mode replaces the color name with the pick number and greys out rows that can't be picked (`0xB4B4B4`). Whether to hide the checkbox child (`pose_checkbox`) during the mode is still open.
- **Layout.** Keep the JC Shine child directly after Fighter-specific Tech whatever the order. The order applies within each player's owner block, not across players.
- **Tests.** `TMOSD_Cell` is host-testable. Cover default order, partial lists, disabled entries (Option B), the shine split and migration from version 15.
