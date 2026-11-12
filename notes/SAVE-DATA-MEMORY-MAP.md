# Save data memory map

What Tyro stores in the memory card save, where it lives, and how much room is left. Current as of T10 (core settings format 3, `TYEX` extension version 21).

Everything here is inside Melee's existing main save entry, so it uses the game's own save, checksum and encryption. Tyro never adds a new file or grows the save. Offsets are from the start of the game-data object (`stc_memcard`), not raw GCI offsets.

## Summary

| Area | Offsets | Size | Used | Free |
| --- | --- | --- | --- | --- |
| [Core settings record](#core-settings-record-0x1f240x1f4f) | `0x1F24..0x1F4F` | 44 bytes | 44 bytes | **1 bit** (byte 37 bit 7) |
| [Event-played flags](#event-score-area-0x1a680x1b3f) (native) | `0x1A68..0x1A6F` | 8 bytes | native | none |
| [Event high scores](#event-score-area-0x1a680x1b3f), slots 0..27 | `0x1A70..0x1ADF` | 112 bytes | 28 scores | none |
| [`TYEX` extension record](#tyex-extension-record-0x1ae00x1b2b) | `0x1AE0..0x1B2B` | 76 bytes | 76 bytes | **9 loose bits** (6 of them retired, in word 12), including **1 bit** in word 72 |
| **Unallocated spare score slots** | `0x1B2C..0x1B37` | **12 bytes** | nothing | **12 bytes (96 bits)** |
| Ledgechase high score (slot 50) | `0x1B38..0x1B3B` | 4 bytes | 1 score | none |
| Native completion word | `0x1B3C..0x1B3F` | 4 bytes | native | not ours |

**Bottom line:** 12 whole bytes are free, all in one block at `0x1B2C..0x1B37`, plus 1 bit in word 72 and scattered single bits. The core record is full.

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
| 20 | T9 | 72 bytes | 2v1 Grab Punish and Up-B Sweetspot settings |
| 21 | T9 | 76 bytes | 2v1 Grab Punish's P3 and P4 characters |

## Core settings record (`0x1F24..0x1F4F`)

The original Training Mode settings bytes, repacked by Tyro (format 3). Code: [src/settings.c](../src/settings.c). Byte numbers are relative to `0x1F24`.

| Byte | Bits | Setting |
| --- | --- | --- |
| 0..3 | 32 | OSD on/off mask, one bit per native OSD ID (big-endian). Bits 27 and 30 are kept in sync for older builds, but the real values live in `TYEX`. |
| 4 | 8 | OSD position (HUD / Sides / Top Left / Top Right) |
| 5 | 0-1 | Event menu page: Minigames, General Tech, Character-specific Tech, Doubles. Since T10 all four values are pages, so nothing here is ever invalid; the Event Select Screen resets a selection past the page's last event |
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
| 28..46 | `0x1AE0..0x1B2B` | `TYEX` record (next section) |
| 46..49 | `0x1B2C..0x1B37` | **Free (12 bytes)** |
| 50 | `0x1B38` | Ledgechase Training |
| — | — | Low Laser Puff Practice is free practice and has no score |

## `TYEX` extension record (`0x1AE0..0x1B2B`)

Tyro's own versioned record. Code: [src/settings_extension.c](../src/settings_extension.c). Byte numbers are relative to `0x1AE0`. Multi-byte words are little-endian, and "word N" means the 4 bytes starting at byte N.

### Byte layout

| Bytes | Owner | Contents | Free inside |
| --- | --- | --- | --- |
| 0..7 | Header | `TYEX`, version (21), payload size (68), marker `A5 5A` | — |
| 8 | Global OSDs / overlays / gameplay | bits 0-2 A-Smash Release color, bit 3 Randall Overlay, bits 4-6 Act OoAttack color, bit 7 Don't Reset Timers (T8, no version bump: every record zeroed byte 8 and nothing ever set bit 7) | — |
| 9..11 | Techchase, Under Fire | word 9 bits 0-23, fully used (table below). Bit 15 (byte 10 bit 7) is Player Distance's top bit since T10 | — |
| 12..15 | Ledgechase, Under Fire, Low Laser | word 12 (table below) | bits 6-8, 15-17 (retired) |
| 16..19 | Powershield, Ledgechase | word 16, fully used | — |
| 20..23 | Reversal, Attack on Shield, Under Fire, Side-B Sweetspot, Blast Zone Overlay, Edgeguard Side, 2v1 Grab Punish | word 20, fully used | — |
| 24..34 | Global OSD colors, Ledgegrab Speedrun, 2v1 Grab Punish | one 4-bit "extra color" nibble per OSD category (21 categories): 0 = use the core 3-bit color, 1..8 = Orange..Lavender. Byte 34 bits 4-5 are Ledgegrab Speedrun's Reset Speed (T9), stored as an offset from the default: 0 Short, 1 Linger, 2 Instant, and an invalid 3 reads as Short. Byte 34 bits 6-7 are the top bits of Grab Punish's Enemy Percent and Teammate Percent (T9). No version bump: every migration to version 15 zeroed this nibble and the color code keeps it. Read only from version 15 on, and Grab Punish's only from version 20 on | — |
| 35 | Low Laser Puff Practice, 2v1 Grab Punish | bits 0-3 Puff position, bits 4-6 Puff height, bit 7 the top bit of Grab Punish's CPU Mashing (T9, no version bump: nothing else ever wrote it) | — |
| 36..49 | Fixed Grid OSD positions, Eggs-ercise | 22 × 5-bit cells (21 OSDs + JC Shine): 0 = Auto, 1..30 = grid cell. Byte 49 bits 6-7 are Eggs Under Platforms and Eggs In The Air (T9), Off by default. No version bump: every migration to version 16 zeroed them and the grid cells keep them. Read only from version 17 on, like the Egg Counter Position | — |
| 50 | Character select, 2v1 Grab Punish | bits 0-2 **Player costume** (0..5), bits 5-7 the low bits of Teammate Percent (T9): 0% to 200% in steps of 20, default 0%, with its top bit in byte 34 bit 7. No version bump, for the same reason as the Egg Counter Position below. Bits 3-4 are Techchase's **Player Side** (T10): 0 Left (default), 1 Right, 2 Random, and an invalid 3 repairs to Left. No version bump: every costume write stored 0..5 and every repair cleared the low 5 bits, so they're clear in every record that owns the byte. Read only from version 17 on. Since T10 the costume is read and repaired through the low 3 bits (5 before) | — |
| 51 | Character select, Eggs-ercise | bits 0-2 **CPU costume** (0..5), bits 5-7 Eggs-ercise Egg Counter Position (T9): 0 Top Left (default), 1 Top Right, 2 Bottom Left, 3 Bottom Right, 4 Under Timer. No version bump: a costume is at most 5 and a higher byte always repaired to 0, so these bits are clear in every record that owns the byte. Read only from records that do (version 17 on). The costume is read and repaired through the low 3 bits (5 before T10), so a damaged costume still repairs to Default and keeps the position and bits 3-4 | bits 3-4 |
| 52..63 | Edgeguard, Up-B Sweetspot | words 52, 56 and 60 (tables below) | — |
| 64..67 | Edgeguard | word 64: recovery options of the later CPUs (table below) | — |
| 68..71 | 2v1 Grab Punish, Up-B Sweetspot, Edgeguard (Fox's and Falco's side-B rows, Knockback Distance) | word 68 (table below) | — |
| 72..75 | Grab Punish's team select, Edgeguard's newer recovery rows, Passing Practice, Up-B Sweetspot | word 72 (table below) | bit 31 |

Free bits inside the record: 6 + 2 + 1 = **9 bits**: word 12's six retired bits, byte 51 bits 3-4 and word 72 bit 31. Techchase took word 9 bit 15 and byte 50 bits 3-4 in T10. The 6 "retired" bits in word 12 held the Ledgechase preset and action in versions 2..8. They're no longer read, but older versions migrate out of them, so reuse them only with care.

### Bit fields in the words

**Word 9: Techchase, Under Fire**

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
| 15 | Distance's top bit (T10), so Random is 4. No version bump: no version ever wrote this bit, so saved distances keep their meaning; migrating a version 1 record clears it |
| 16-18 | CPU invincibility color |
| 19-20 | Height |
| 21-22 | Distance, low bits: 0 Right Next To, 1 Close, 2 Medium, 3 Past Tech Roll, 4 Random (T10, with bit 15). A stored 5-7 repairs to Right Next To |
| 23 | Under Fire: Remove Fleshy Pillars (T9). No version bump: every record started with it zero and migrations only keep it |

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

**Word 20: Reversal, Attack on Shield, Under Fire, Side-B Sweetspot, Blast Zone Overlay, Edgeguard Side, 2v1 Grab Punish**

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
| 22 | Side-B Sweetspot: Double-Jump Height Indicator (no version bump, same reason) |
| 23 | Blast Zone Overlay, a global flag (T8, no version bump, same reason) |
| 24-25 | Edgeguard Side (T8): 0 Random (default), 1 Left, 2 Right. No version bump. Migrations keep this padding as it was, so only choosing a Side writes it, and an invalid 3 reads as Random. Read only from records that own the Edgeguard words |
| 26 | Side-B Sweetspot: Success Criteria (T9): 0 Sweetspot (default), 1 Sweetspot + Ledgedash. No version bump: every record that owns word 20 zeroed it and nothing else ever wrote it |
| 27-29 | Side-B Sweetspot: Knockback (T9): 0 Random (default), 1..6 the presets (Above, Close to Below, Far), 7 Random Preset. No version bump, same reason |
| 30-31 | 2v1 Grab Punish: the enemy's team in your last match (T9): 0 Red, 1 Blue, 2 Green. No version bump. Like Edgeguard Side, migrations keep this padding, so only a match writes it, and an invalid 3 reads as Red. The character select moves the enemy off your team if they match, so a fresh save is Red vs Blue |

**Word 52: Edgeguard Custom Knockback** (fully used)

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
| 30 | Camera (T8): 0 Normal (default), 1 Zoomed Out. Every record that owned word 56 had this bit clear, so older saves read as Normal with no version bump. |
| 31 | Up-B Sweetspot: Marth's top bit (T9), see word 68. No version bump: nothing else ever wrote it, so it's clear in every record |

**Word 60: Edgeguard Knockback Height, recovery options and Player Position** (fully used)

Each recovery mask has one bit per row of that CPU's Recovery Options menu, in row order ([src/recovery.c](../src/recovery.c)). Defaults match the menu's defaults.

| Bits | Setting |
| --- | --- |
| 0-2 | Knockback Height (T9): 0 Random, 1 Very High, 2 High, 3 Level, 4 Low, 5 Very Low, 6 Custom. While word 68's Knockback Distance is still 0, this field is the Hit Strength it replaced (0 Weak, 1 Normal, 2 Hard, 3 Shallow, 4 Custom, default Normal), which `LoadKnockback` converts ([the knockback notes](EDGEGUARD-KNOCKBACK.md)) |
| 3-8 | Fox recovery, first six bits (default Firefox Low). Since T9 these are Firefox Low, Mid and High (rows 1-3), Double Jump (row 5), Illusion Instant (row 7, the old Illusion bit), then Fast Fall (row 6); the other side-B rows are in word 68, and Firefox To Ledge (row 4) in word 72 |
| 9-14 | Falco recovery, first six bits (default Firebird Low), laid out like Fox's |
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
| 10-14 | Luigi's first five rows, Fast Mashing in bit 14 though it's now the last row (default Double Jump + Cyclone + Green Missile); Up B Sweetspot and Backflip Jump Sweetspot are in word 72 |
| 15-19 | Dr. Mario (5 rows, default Double Jump + Dr. Tornado) |
| 20-22 | Ice Climbers (3 rows, default Belay + Squall Hammer) |
| 23-26 | Jigglypuff (4 rows, all on) |
| 27-31 | Samus (5 rows, all on but Screw Attack High) |

**Word 68: 2v1 Grab Punish, Up-B Sweetspot and Edgeguard** (T9, version 20)

Migrating to version 20 zeroes this word and then writes the defaults, so the spare bits are zero in every record that owns it. Any event can grow into its spare bits with no version bump if the new setting's default is 0, which is how Edgeguard took bits 19-31. Version 19 and older records read the defaults.

| Bits | Setting |
| --- | --- |
| 0-2 | Grab Punish: Grab Location (8 choices, default Random) |
| 3-5 | Grab Punish: Your Location (8 choices, default Random) |
| 6-8 | Grab Punish: Enemy Percent, low bits (0% to 200% in steps of 20, default 60%), with its top bit in byte 34 bit 6 |
| 9-10 | Grab Punish: CPU Mashing, low bits (No / Light / Medium / Heavy / Very Heavy / Inhuman, default Light), with its top bit in byte 35 bit 7 |
| 11-12 | Grab Punish: Teammate Facing (Random / Toward You / Away From You, default Random) |
| 13 | Grab Punish: Release Hit = Success, stored inverted: 0 On (default), 1 Off, so a save that never touched it reads On |
| 14-15 | Grab Punish: your team in your last match (T9): 0 Red (default), 1 Blue, 2 Green. These are this event's own, apart from the normal costumes in bytes 50-51 |
| 16 | Up-B Sweetspot: Enable Line Guides (default On) |
| 17 | Up-B Sweetspot: Marth's low bit, with its top bit in word 56 bit 31. Stored 0 Forward Smash (default), 1 Off, 2 Down Tilt, so a save from when this row was Tipper / Off keeps its meaning. The menu order is Forward Smash, Down Tilt, Off |
| 18 | Up-B Sweetspot: Camera (Normal / Zoomed Out, default Normal). Its B0XX Angles (T10) is word 72 bit 28 |
| 19-23 | Edgeguard: Fox's side-B rows 8-12 (Ledge Sweetspot, To Ledge, To Main Stage, To Side Plat, Shortest Shorten), default all off |
| 24-28 | Edgeguard: Falco's side-B rows 8-12, the same way |
| 29-31 | Edgeguard: Knockback Distance plus 1: 1 Random, 2 Close, 3 Near, 4 Medium, 5 Far. 0 means no choice saved yet, so word 60 bits 0-2 still hold Hit Strength |

**Word 72: Grab Punish's team select, Edgeguard's newer recovery rows, Passing Practice and Up-B Sweetspot** (T9, version 21)

Every migration to version 21 zeroes this word, so its spare bits are zero in every record that owns it, and any event can grow into them with no version bump if the new setting's default is 0. Version 20 and older records read no character saved.

| Bits | Setting |
| --- | --- |
| 0-4 | P3's last character: 0 none yet (Fox), otherwise the external ID + 1, so Zelda and Sheik are told apart |
| 5-9 | P4's, the same way |
| 10 | Edgeguard: Firefox To Ledge, default off. No version bump |
| 11 | Edgeguard: Firebird To Ledge, the same way |
| 12-13 | Edgeguard: Luigi's Up B Sweetspot and Backflip Jump Sweetspot, default off, the same way |
| 14-16 | Passing Practice (T10): Teammate Aerial (Neutral / Forward / Back / Up / Down Air, default Back Air). No version bump |
| 17-21 | Passing Practice: Enemy Percent (0% to 200% in steps of 10, default 40%) |
| 22 | Passing Practice: Percent Mode (Frozen % / Rising %, default Frozen %) |
| 23-25 | Passing Practice: Distance (Very Close / Close / Medium / Far / Very Far, default Medium) |
| 26-27 | Passing Practice: Your Side (Left / Right / Random, default Left) |
| 28 | Up-B Sweetspot (T10): B0XX Angles, default Off. No version bump. Its other rows are in word 68; the menu lists it second, and `TMUpBSweetspotPreference` follows the menu |
| 29 | Passing Practice (T10): Reset If Hitstun Ends (Off / On, default Off). It's fourth in the menu, and `TMPassingPreference` follows the menu |
| 30 | Passing Practice (T10): Your Facing (Toward / Away from your teammate, default Toward), last in the menu |
| 31 | Spare |

Passing Practice stores each row as its distance from its default, `(value + count - default) % count`, so the zeros every record has there read as the defaults. A stored value past the row's choices reads as invalid and is repaired to the default. Version 20 and older records read the defaults. Its Session Best and Previous counts are RAM-only (below).

P1 and P2 use the normal saved characters (core bytes 6 and 37), which already tell Zelda and Sheik apart.

## Not saved

These live in RAM only and reset when the game closes:

- The game's character-select backup (`stc_memcard + 0xD10`, 3344). Tyro restores it at boot from the saved character (core bytes 6/37) and costume (`TYEX` bytes 50/51).
- Which form each character select door has on the Zelda icon (`TM_SETTING_DOOR_ZELDA`). It comes from the saved characters each time the select opens.
- The Global Settings page, the manual OSD page, Techchase/Ledgechase manual chances, and custom camera coordinates.
- Passing Practice's Session Best and Previous counts for each Percent Mode, and what it has measured of the teammate's aerial and jumps. The event's `.dat` reloads with every match, which clears them.
- All Training Lab menu options. Saving them was estimated at about 100 bytes, more than the 16 left.

## Adding something new

1. **A few bits:** use one of the free bits above, if the setting is small and fits next to related settings. Old records must already have 0 there, or the migration must clear it.
2. **Whole bytes:** append to `TYEX`, taking bytes from the free 12-byte block in order. Bump `TM_EXTENSION_VERSION` and `TM_EXTENSION_SIZE`, add a `versionN()` check for the previous version, and zero the new bytes in `TMExtension_Prepare` when migrating. The static assert in [src/settings_extension.h](../src/settings_extension.h) stops the build if the record outgrows the 88-byte region.
3. **More than 12 bytes:** would mean taking over real high-score slots, or a separate save file. Neither has been designed.
