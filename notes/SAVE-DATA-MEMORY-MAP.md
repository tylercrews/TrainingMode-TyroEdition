# Save data memory map

What Tyro stores in the memory card save, where it lives, and how much room is left. Current as of T6 (core settings format 3, `TYEX` extension version 19).

Everything here is inside Melee's existing main save entry, so it uses the game's own save, checksum and encryption. Tyro never adds a new file or grows the save. Offsets are from the start of the game-data object (`stc_memcard`), not raw GCI offsets.

## Summary

| Area | Offsets | Size | Used | Free |
| --- | --- | --- | --- | --- |
| [Core settings record](#core-settings-record-0x1f240x1f4f) | `0x1F24..0x1F4F` | 44 bytes | 44 bytes | **1 bit** (byte 37 bit 7) |
| [Event-played flags](#event-score-area-0x1a680x1b3f) (native) | `0x1A68..0x1A6F` | 8 bytes | native | none |
| [Event high scores](#event-score-area-0x1a680x1b3f), slots 0..27 | `0x1A70..0x1ADF` | 112 bytes | 28 scores | none |
| [`TYEX` extension record](#tyex-extension-record-0x1ae00x1b23) | `0x1AE0..0x1B23` | 68 bytes | 68 bytes | **40 loose bits** inside the record |
| **Unallocated spare score slots** | `0x1B24..0x1B37` | **20 bytes** | nothing | **20 bytes (160 bits)** |
| Ledgechase high score (slot 50) | `0x1B38..0x1B3B` | 4 bytes | 1 score | none |
| Native completion word | `0x1B3C..0x1B3F` | 4 bytes | native | not ours |

**Bottom line:** 20 whole bytes are free, all in one block at `0x1B24..0x1B37`, plus scattered single bits. The core record is full.

The `TYEX` record and the free block are carved out of 88 bytes (`0x1AE0..0x1B37`) that used to be unused event score slots 28..49. See the [score storage audit](SCORE-STORAGE-AUDIT.md) for how that space was proven safe. In particular, "Reset records" keeps these bytes on Tyro saves.

### Growth of the `TYEX` record

| Version | Tyro build | Size | Added |
| --- | --- | --- | --- |
| 1..7 | T4 | 12 → 16 bytes | A-Smash Release color, Techchase, Ledgechase, Under Fire, Low Laser toggles |
| 8..10 | T4 | 20 bytes | Powershield, more Ledgechase |
| 11..12 | T4 | 24 bytes | Reversal and Attack on Shield |
| 13..14 | T5 | 24 bytes | Randall Overlay, Act OoAttack color (in spare bits) |
| 15 | T5 | 36 bytes | 8 extra OSD colors, Low Laser Puff position/height |
| 16 | T6 | 50 bytes | Fixed Grid OSD positions |
| 17 | T6 | 52 bytes | Player/CPU costume colors |
| 18 | T6 | 64 bytes | Edgeguard settings and per-CPU recovery options |
| 19 | T6 | 68 bytes | Recovery options of the CPUs added to Edgeguard (Yoshi to Samus) |

## Core settings record (`0x1F24..0x1F4F`)

The original Training Mode settings bytes, repacked by Tyro (format 3). Code: [src/settings.c](../src/settings.c). Byte numbers are relative to `0x1F24`.

| Byte | Bits | Setting |
| --- | --- | --- |
| 0..3 | 32 | OSD on/off mask, one bit per native OSD ID (big-endian). Bits 27 and 30 are kept in sync for older builds, but the real values live in `TYEX`. |
| 4 | 8 | OSD position (HUD / Sides / Top Left / Top Right) |
| 5 | 0-1 | Event menu page |
| 5 | 2-7 | Selected event on that page |
| 6 | 0 | "Recommended OSDs" flag (legacy) |
| 6 | 1-5 | Saved **Player character** (ID + 1, 0 = none) |
| 6 | 6-7 | Marker `10` = the character fields are valid |
| 7 | 0-3 / 4-7 | Lab frame advance button / frame decrement button |
| 8 | 0-3 / 4-7 | Lab D-Pad Up / D-Pad Down |
| 9 | 0-3 / 4-7 | Lab D-Pad Left / D-Pad Right |
| 10 | 0-5 | Flags: Override All OSDs Off, Hitbox Trails Very Fast, Hitbox Trails Instant, Missed L-Cancel, Actionable Yellow>Green, Infinite Shields |
| 10 | 6-7 | Format version (3) |
| 11 | 8 | Lab CPU input display |
| 12..29 | 18 × 8 | 18 character overlays: Player choice in the low nibble, CPU choice in the high nibble |
| 30..37 | 57 (to byte 37 bit 0) | Title colors of the 19 core OSDs, 3 bits each |
| 37 | 1-5 | Saved **CPU character** (ID + 1, 0 = none) |
| 37 | 6 | Ledgedash success criterion, high bit |
| 37 | **7** | **Free** |
| 38..39 | 16 | Signature `TY` |
| 40 | 0-2 | Flags: Run Turnaround, Invincibility Overlay, Override CPU OSDs Off |
| 40 | 3 | Event preferences initialized |
| 40 | 4-5 | Ledgedash reset delay |
| 40 | 6 | Ledgedash tips |
| 40 | 7 | Side-B Sweetspot line guides Off (inverted, so 0 = On) |
| 41 | 0-2 / 3-5 / 6-7 | Ledgedash starting position / reset / success criterion (low bits) |
| 42 | 8 | Eggs-ercise damage threshold |
| 43 | 0-1 | Eggs-ercise scale |
| 43 | 2 / 3 / 4 | Eggs-ercise spawn velocity / fighter collision / Free Practice |
| 43 | 5-6 | OSD layout (Recent / Fixed Grid / Practice Panel) |
| 43 | 7 | Side-B Sweetspot camera (Normal / Zoomed Out) |

> `MexTK/include/memcard.h` still names bytes 41..43 `TM_SettingsReserved[3]`. That name is out of date: those bytes are in use.

## Event score area (`0x1A68..0x1B3F`)

Each high score is a 4-byte slot at `0x1A70 + slot × 4`. Score slots don't follow menu order, so adding or moving a menu row never moves an old score.

| Slot | Offset | Event |
| --- | --- | --- |
| 0 | `0x1A70` | Eggs-ercise |
| 1 | `0x1A74` | Slalom |
| 2 | `0x1A78` | Shined Blind |
| 3 | `0x1A7C` | Reaction Test |
| 4 | `0x1A80` | Under Fire (perfect-refresh count) |
| 5 | `0x1A84` | Training Lab |
| 6 | `0x1A88` | L-Cancel Training |
| 7 | `0x1A8C` | Ledgedash Training |
| 8 | `0x1A90` | Wavedash Training |
| 9 | `0x1A94` | Techchase Training |
| 10 | `0x1A98` | Combo Training |
| 11 | `0x1A9C` | Attack on Shield |
| 12 | `0x1AA0` | Reversal Training |
| 13 | `0x1AA4` | SDI Training |
| 14 | `0x1AA8` | Powershield Training |
| 15 | `0x1AAC` | Ledgetech Training |
| 16 | `0x1AB0` | Amsah-Tech Training |
| 17 | `0x1AB4` | Shield Drop Training |
| 18 | `0x1AB8` | Waveshine SDI |
| 19 | `0x1ABC` | Slide-Off Training |
| 20 | `0x1AC0` | Grab Mash Training |
| 21 | `0x1AC4` | Ledgetech Marth Counter |
| 22 | `0x1AC8` | Edgeguard Training |
| 23 | `0x1ACC` | Side-B Sweetspot |
| 24 | `0x1AD0` | Laser Land Training |
| 25 | `0x1AD4` | Escape Sheik Techchase |
| 26 | `0x1AD8` | Escape DThrow Knee |
| 27 | `0x1ADC` | Float Cancel Training |
| 28..44 | `0x1AE0..0x1B23` | `TYEX` record (next section) |
| 45..49 | `0x1B24..0x1B37` | **Free (20 bytes)** |
| 50 | `0x1B38` | Ledgechase Training |
| — | — | Low Laser Puff Practice is free practice and has no score |

## `TYEX` extension record (`0x1AE0..0x1B23`)

Tyro's own versioned record. Code: [src/settings_extension.c](../src/settings_extension.c). Byte numbers are relative to `0x1AE0`. Multi-byte words are little-endian, and "word N" means the 4 bytes starting at byte N.

### Byte layout

| Bytes | Owner | Contents | Free inside |
| --- | --- | --- | --- |
| 0..7 | Header | `TYEX`, version (19), payload size (60), marker `A5 5A` | — |
| 8 | Global OSDs / overlays | bits 0-2 A-Smash Release color, bit 3 Randall Overlay, bits 4-6 Act OoAttack color | bit 7 |
| 9..11 | Techchase | word 9 bits 0-14 and 16-22 (table below) | bit 15 (byte 10 bit 7), bit 23 (byte 11 bit 7) |
| 12..15 | Ledgechase, Under Fire, Low Laser | word 12 (table below) | bits 6-8, 15-17 (retired) |
| 16..19 | Powershield, Ledgechase | word 16, fully used | — |
| 20..23 | Reversal, Attack on Shield, Under Fire | word 20 bits 0-21 | bits 22-31 (10 bits) |
| 24..34 | Global OSD colors | one 4-bit "extra color" nibble per OSD category (21 categories): 0 = use the core 3-bit color, 1..8 = Orange..Lavender | byte 34 high nibble |
| 35 | Low Laser Puff Practice | bits 0-3 Puff position, bits 4-6 Puff height | bit 7 |
| 36..49 | Fixed Grid OSD positions | 22 × 5-bit cells (21 OSDs + JC Shine): 0 = Auto, 1..30 = grid cell | byte 49 bits 6-7 |
| 50 | Character select | **Player costume** (0..5) | top 5 bits |
| 51 | Character select | **CPU costume** (0..5) | top 5 bits |
| 52..63 | Edgeguard | words 52, 56 and 60 (tables below) | bits 30-31 of word 56 |
| 64..67 | Edgeguard | word 64: recovery options of the later CPUs (table below) | — |

Free bits inside the record: 1 + 2 + 6 + 10 + 4 + 1 + 2 + 10 + 2 = **38 bits**. The 6 "retired" bits in word 12 held the Ledgechase preset and action in versions 2..8. They're no longer read, but older versions migrate out of them, so reuse them only with care.

### Bit fields in the words

**Word 9: Techchase**

| Bits | Setting |
| --- | --- |
| 0-3 | Preset |
| 4 | Reaction OSD |
| 5 | Timing OSD |
| 6-7 | Reset |
| 8-9 | Invisibility |
| 10-11 | Camera |
| 12-13 | Location |
| 14 | Spotdodge |
| 15 | *free* |
| 16-18 | CPU invincibility color |
| 19-20 | Height |
| 21-22 | Distance |
| 23 | *free* |

**Word 12: Ledgechase, Under Fire, Low Laser**

| Bits | Setting |
| --- | --- |
| 0-1 | Ledgechase side |
| 2 | Ledgechase CPU 100% |
| 3-5 | Ledgechase player percent |
| 6-8 | *retired (old preset)* |
| 9-11 | Ledgechase distance |
| 12-14 | Ledgechase CPU invincibility color |
| 15-17 | *retired (old action)* |
| 18 | Under Fire: disable attack particles |
| 19 | Under Fire: score is a refresh count (format marker) |
| 20 | Low Laser: event OSDs |
| 21 | Low Laser: suppress global OSDs |
| 22-23 | Ledgechase entry |
| 24-29 | Ledgechase CPU action |
| 30-31 | Ledgechase hop height |

**Word 16: Powershield, Ledgechase**

| Bits | Setting |
| --- | --- |
| 0-1 | Powershield fire speed |
| 2-7 | Powershield delay min |
| 8-13 | Powershield delay max |
| 14-16 | Powershield height |
| 17 | Powershield direction |
| 18-19 | Powershield movement |
| 20 | Powershield Nana |
| 21 | Powershield OSDs |
| 22 | Powershield suppress global OSDs |
| 23 | Powershield overlay |
| 24-27 | Ledgechase preset |
| 28-31 | Ledgechase aerial attack |

**Word 20: Reversal, Attack on Shield, Under Fire**

| Bits | Setting |
| --- | --- |
| 0-3, 20 | Reversal attack (bit 20 is the 5th bit) |
| 4-5 | Reversal distance |
| 6 | Reversal player facing |
| 7 | Reversal CPU facing |
| 8 | Reversal position (ground / platform) |
| 9-12 | Attack on Shield action |
| 13-15 | Attack on Shield hits |
| 16 | Attack on Shield infinite shield |
| 17-18 | Attack on Shield distance |
| 19 | Attack on Shield shield DI |
| 21 | Under Fire: Lava Overlay (no version bump: every record that owns word 20 zeroed it) |
| 22-31 | *free* |

**Word 52: Edgeguard Custom Hit Strength** (fully used)

| Bits | Setting |
| --- | --- |
| 0-8 | Knockback min (0..300, default 100) |
| 9-17 | Knockback max (0..300, default 120) |
| 18-24 | Angle min (0..90, default 45) |
| 25-31 | Angle max (0..90, default 60) |

**Word 56: Edgeguard percents**

| Bits | Setting |
| --- | --- |
| 0-9 | Player percent (0..998, default 0) |
| 10-19 | CPU percent min (0..999, default 50) |
| 20-29 | CPU percent max (0..999, default 80) |
| 30-31 | *free* |

**Word 60: Edgeguard Hit Strength, recovery options and Player Position** (fully used)

Each recovery mask has one bit per row of that CPU's Recovery Options menu, in row order ([src/recovery.c](../src/recovery.c)). Defaults match the menu's defaults.

| Bits | Setting |
| --- | --- |
| 0-2 | Hit Strength (Weak / Normal / Hard / Shallow / Custom, default Normal) |
| 3-8 | Fox recovery (6 rows, default Firefox Low) |
| 9-14 | Falco recovery (6 rows, default Firebird Low) |
| 15-20 | Sheik recovery (6 rows, default the three Vanish rows) |
| 21-25 | Captain Falcon recovery (5 rows, default Falcon Dive + Drift Back) |
| 26-29 | Marth recovery (4 rows, default Dolphin Slash Early + Double Jump) |
| 30-31 | Player Position (T7), stored as an offset from Close: 0 Close (default), 1 Halfway, 2 Mid Stage, 3 At Ledge. Every record that owned word 60 had these bits clear, so older saves read as Close with no version bump. |

**Word 64: recovery options of the later Edgeguard CPUs** (fully used)

Same layout as word 60's masks. These CPUs' recoveries are described in [the Edgeguard notes](EDGEGUARD-NEW-CHARACTERS.md).

| Bits | Setting |
| --- | --- |
| 0-4 | Yoshi (5 rows, default Jump High + Jump Low + Air Dodge to Ledge) |
| 5-9 | Peach (5 rows, default Float to Stage + Float Low + Parasol) |
| 10-14 | Luigi (5 rows, default Double Jump + Cyclone + Green Missile) |
| 15-19 | Dr. Mario (5 rows, default Double Jump + Dr. Tornado) |
| 20-22 | Ice Climbers (3 rows, default Belay + Squall Hammer) |
| 23-26 | Jigglypuff (4 rows, all on) |
| 27-31 | Samus (5 rows, all on but Screw Attack High) |

## Not saved

These live in RAM only and reset when the game closes:

- The game's character-select backup (`stc_memcard + 0xD10`, 3344). Tyro restores it at boot from the saved character (core bytes 6/37) and costume (`TYEX` bytes 50/51).
- The Global Settings page, the manual OSD page, Techchase/Ledgechase manual chances, and custom camera coordinates.
- All Training Lab menu options. Saving them was estimated at about 100 bytes, more than the 20 left.

## Adding something new

1. **A few bits:** use one of the free bits above, if the setting is small and fits next to related settings. Old records must already have 0 there, or the migration must clear it.
2. **Whole bytes:** append to `TYEX`, taking bytes from the free 20-byte block in order. Bump `TM_EXTENSION_VERSION` and `TM_EXTENSION_SIZE`, add a `versionN()` check for the previous version, and zero the new bytes in `TMExtension_Prepare` when migrating. The static assert in [src/settings_extension.h](../src/settings_extension.h) stops the build if the record outgrows the 88-byte region.
3. **More than 20 bytes:** would mean taking over real high-score slots, or a separate save file. Neither has been designed.
