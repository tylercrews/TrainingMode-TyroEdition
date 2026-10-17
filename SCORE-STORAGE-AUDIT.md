# Spare event-score storage audit

**Current capacity (October 15, 2026):** `TYEX` version 12 owns 24 of the audited 88 bytes, leaving **64 contiguous bytes unallocated**, plus eighteen unused bits within its allocated payload. See the [version-12 addendum](#version-12-shield-di-and-expanded-reversal-actions); earlier version-specific totals below are historical.

Verified October 14, 2026 against the current event catalogue, `build/Start.dol`,
the optimized shared event DAT and the compiled native hooks. The original
44-byte settings format remains at `0x1F24..0x1F4F`.

The original candidate was 92 bytes at `0x1AE0..0x1B3B`. With Ledgechase,
**88 bytes / 704 bits at game-data offsets `0x1AE0..0x1B37`** remain reserved
for preferences. These are score slots 28 through 49, not raw GCI offsets.
The three pages now contain 5, 17 and 7 events. The original 28 four-byte scores
retain `0x1A70..0x1ADF`; the new General Tech event 17 uses fixed score slot 50
at `0x1B38..0x1B3B`. Adding the menu row does not shift any old score. Separate
event-played flags at `0x1A68..0x1A6F` and the native word beginning at `0x1B3C`
are excluded.

The current version-7 extension owns **16 bytes at `0x1AE0..0x1AEF`**, with an eight-byte
`TYEX`, version, size and marker header plus eight payload bytes. **72 bytes
remain unallocated**. Native high scores have not moved or been removed, and
the 44-byte OSD settings record has not been repacked. See
`src/settings_extension.h` and `src/settings_extension.c` for the current
payload; the audit establishes the boundary independently of that payload.
Version 7 uses byte 15 bits 0..5 for the expanded Ledgechase CPU action; the
record stays at 16 bytes, preserving 72 unallocated bytes. Versions 2..6 map
the old action choices into the new catalogue without changing neighboring
event settings. Version 7 also accepts the appended 50/50 Getup / Roll preset.

## Native save and checksum coverage

The native manifest at `0x803BAB74` declares its main data entry as `0x1790`
bytes. The getter at `0x8015CC40` returns game-data base plus `0x1868`, so the
candidate lies at logical payload offsets `0x278..0x2CF`, and the new score at
`0x2D0..0x2D3`, safely inside the
main serialized entry. No manifest or memory-card file size change is needed.

`tests/test_score_storage.py` executes the actual native block writer at
`0x803ACFC0`, allowing its real `memcpy`, padding, block-header generation,
checksum and encryption to run. It intercepts `CARDWrite` only after the
completed sector exists in memory. The native decoder at `0x803B31CC` restores
the complete original payload, including all candidate bytes and both
neighboring words. Executing the native checksum routine at `0x803B2B20` after
changing the first, middle or last candidate byte changes the checksum.
Changing ciphertext in the candidate causes the native decoder to reject it.

This verifies native serialization and checksum coverage without accessing a
real card. Dolphin save/reload, card failure and no-card interaction still need
live verification before release confidence can include those paths.

## Access and clear paths

| Native path | Verified behavior | Protection or intended handling |
| --- | --- | --- |
| Getter `0x8015CF5C` / setter `0x8015CF70` | The original indexed functions can reach all 51 score slots. Earlier mod hooks added page offsets without checking bounds. | Getter hook `0x8015CF60` and setter hook `0x8015CF74` now call the bounded shared score resolver. Invalid indices read zero and write nothing. |
| Event records reset `0x8015EEC8` | Clears 25 fighter-record subranges, then 216 bytes at `0x1A68..0x1B3F`. The original final clear erases the entire candidate. | Hook `0x8015EF14` replaces only that final clear. On recognized `TMTE01` formats, clear the 120-byte played/assigned-score prefix and the final eight bytes (Ledgechase score plus adjacent word), preserving all 88 preference bytes. Unknown core/extension formats retain the original 92-byte protected region. |
| New/missing/corrupt main-save initialization `gmMainLib_8015F600(1, ...)` | Native clear at `0x8015F77C..0x8015F788` clears `0x448` bytes beginning at `0x1868`, including the candidate. | Intentional fresh-save behavior; extension defaults are reconstructed. This path must not preserve stale preferences from another card/save. |
| Boot initialization `0x8015FBA4` | Zeroes the entire native game-data object before initialization/loading. | Intentional RAM initialization; subsequent card loading restores serialized bytes. |
| Normal card read/write | Copies full manifest entries through native card jobs. | Candidate bytes travel with the existing main entry and native checksum/encryption. No separate file or job is introduced. |
| Played-flag helpers `0x8015CEB4` / `0x8015CEFC` | Access `0x1A68..0x1A6F`. | Separate flags retained; excluded from the extension. |
| Adjacent completion helpers `0x8015CF84` / `0x8015CF94` | Read/write the byte at `0x1B3C`. | Excluded from the extension and retained in records-reset semantics. |

The archived native source in `build/investigation/melee-source.zip` confirms
the same field accesses and reset scopes. Searches of that source found no
other native game-save field reader/writer referring to the numeric score
array; the DOL references corroborate the getter, setter and bounded clear.
Unrelated fighter structures also have fields named `x1A68` / `x1A70`; they do
not use the save-data base and do not overlap this storage.

The protected paths are tested using the optimized, actually relocated shared
DAT and the compiled native C2 hooks. Tests cover every current score on every
page, out-of-range and negative-as-unsigned event indices, the entire spare
region as canaries, both boundary words, and GPR/FPR/CR/CTR preservation across
the added service calls. The native records-reset test executes the complete
native routine, including its earlier fighter-record clears.

## Ownership, growth and downgrade policy

`Events_ScoreIndex` validates the page and event against the actual page arrays.
The catalogue has a compile-time guard requiring the mapped counts 5/17/7.
Minigames retain score slots 0..4, the original General Tech events retain
5..20, character-specific events retain 21..27, and Ledgechase uses slot 50.
Further catalogue changes require an explicit mapping instead of silently
overlapping preferences or changing old score identities. Keep the storage
boundary fixed when removing events; do not slide persisted preferences
according to current count.

Records-reset preservation checks the complete six-byte identity `TMTE01`.
The tests verify that `GTME01`, `GALE01`, the former `TYRE01` and even `TMTE02` keep the original
216-byte native clear. The separate Tyro save namespace prevents ordinary
upstream/vanilla use from accessing a Tyro save automatically.

The extension has independent format recognition. Foreign identity, unsupported
core format and unsupported extension versions use private runtime defaults;
unknown persisted bytes are retained. Because older unknown extensions were
allowed to own all 92 bytes, new Ledgechase score access reads zero and ignores
writes when the core or extension format is unsupported. Erasing event records
also preserves that complete opaque 92-byte region, including slot 50's four
bytes. Recognized versions 1, 2, current version 3 and absent extensions permit
Ledgechase's fixed slot 50. Validation during records erasure uses private
copies and does not migrate or otherwise rewrite preferences.

The owned version-3 record is bounded to 16 bytes; unrelated preference bytes
remain untouched. An exact version-1 header (`TYEX`, version 1, payload size 4,
markers `A5 5A`) migrates to version 3, payload size 8. A-Smash's saved color,
all eight earlier Techchase preferences and unclaimed padding bits survive.
The new Techchase CPU protection color defaults to Gray. Ledgechase defaults to
Left, CPU 100% Off, player 0%, Even preset, Close distance, Gray protection color
and None follow-up. Version 2 also migrates to version 3: the four existing
CPU actions retain their meaning as menu indices shift by one. The fifth
choice uses one old padding bit without expanding the record. Manual
per-option percentages remain session-only.
Migration marks the existing native save dirty once; it does not change the
44-byte core record, prior event scores, slot 50 or bytes beyond the owned
16-byte record. Unknown versions, sizes or markers remain untouched.

Unmodified older Tyro executables cannot implement all these protections.
Earlier version-aware builds treat version 3 as unsupported and preserve it;
its original 28 score writes do not touch the preference candidate. Builds
predating the score-region reset protection still clear preferences through
their native **Erase Event Records** action. Fresh-save creation,
explicit deletion of a save, and card replacement intentionally reset settings.
Do not claim that arbitrary older executables preserve this extension. A missing
or invalid extension header must continue to yield valid defaults rather than
interpreting stale score words as settings.

## Repeating the audit

Rebuild the optimized shared DAT and assembly fixtures with the settings runner,
then run the audit with the existing Unicorn dependency available:

```sh
bash tests/run_settings_tests.sh
PYTHONPATH=build/tangibility-test-deps python tests/test_score_storage.py -v
```

The audit has **12 passing tests**, plus **10 passing extension codec/migration
tests** and two passing catalogue tests. It performs no physical card writes and
does not edit the input DOL, DAT or GCT. `CARDWrite` is intercepted in the emulator.


## Version 8: Powershield preferences (historical allocation)

The current extension owns **20 bytes** at 0x1AE0..0x1AF3, leaving **68 bytes** of the existing 88-byte region unallocated. This supersedes the historical 16-byte ownership figures above. Bytes 16..19 contain Fire Speed, Min/Max Fire Delay, Laser Height, Direction, Movement, Nana, Event OSDs, Suppress Global OSDs and Laser Recovery Cue. Their word uses bits 0..23; bits 24..31 remain reserved. Defaults serialize as `00 14 B0 00`.

Migration from the exact version-7 header (payload length 8) changes only version/length and the four newly owned bytes, preserving all earlier settings and their padding. Versions 1..6 retain their established migrations. The core remains 44 bytes / format 3, and the file manifest, checksums, score identities, slot-50 protections and 88-byte records-reset preservation are unchanged. Tests include version-7 migration canaries and recognized version-8 records reset through native code. Unknown future extensions and foreign identities cannot be claimed by Powershield settings. See [event documentation](docs/POWERSHIELD-TRAINING.md).

## Version 9: Ledgechase presets and aerial attacks (historical allocation)

The extension still owns 20 bytes, with 68 bytes unallocated in the audited region. The formerly reserved byte 19 now holds a four-bit Ledgechase preset (14 choices) and a four-bit aerial selector (12 choices, default None). Powershield's lower 24 bits remain unchanged. Versions 2..8 copy their old three-bit preset into the new slot, retaining its original bytes, and clear the new aerial selector. Version 1 initializes both to defaults. Version-8 migration changes only the version header and byte 19, preserving every Powershield choice and surrounding canary. Unknown future formats retain private defaults and their original bytes. Core settings, native card checksums, score identities, and records-reset boundaries are unchanged.

## Version 10: Ledge entry and hop height

Ownership remains 20 bytes. Byte 14 bits 6..7 now hold Get to Ledge (Spawn/Fall/Jump, default Jump), and byte 15 bits 6..7 hold Ledge Hop Height (Low/Medium/Original, default Low). Versions 1..9 initialize these formerly reserved bits rather than interpreting their old padding. Version 9 keeps all fourteen valid preset IDs and its aerial selection; invalid old preset 14 does not become the new 50%-dash preset. The preset slot now accepts fifteen choices. Powershield, all previous owned preferences, core settings and the surrounding score region remain intact. Hop Drop Adjustment is session-only. Migration and native records-reset tests cover the exact version-10 header as well as earlier formats.


## Version 11: Attack on Shield and Reversal

The extension now owns **24 bytes** at `0x1AE0..0x1AF7`, leaving **64 bytes** unallocated in the same 88-byte region. The first 20 bytes retain all version-10 preferences, apart from the version/length header. Bytes 20..23 form a little-endian word: bits 0..3 Reversal attack (14 choices), 4..5 distance, 6 player facing, 7 CPU facing, 8 ground/platform, 9..12 Attack on Shield response (10 choices), 13..15 hit count minus one (0..4), 16 infinite shields (default On), 17..18 distance; bits 19..31 remain reserved. The default word is `00 00 01 00`.

Versions 1..10 initialize the new word, retaining all previously supported preferences and padding. Missing records remain read-only until an edit. Unsupported and foreign records keep their bytes and use private runtime preferences. New tests cover exhaustive value/invalid-write isolation, fresh-service reload, exact version-10 migration, production DAT relocation, and native records erasure across versions 1..11. Native score boundaries, save serialization and checksums are unchanged.

<a id="availability-addendum-2026-10-15"></a>

## Availability addendum — October 15, 2026

Version-11 snapshot; the [version-12 addendum](#version-12-shield-di-and-expanded-reversal-actions) below accounts for two subsequently allocated padding bits.

Attack on Shield and Reversal consume **four additional bytes**, growing the extension from 20 to 24 bytes. Current settings ownership is 44 core bytes + 24 extension bytes = **68 bytes**, without growing the native save file or allocating another card block.

| Extension region | Game-data offsets | Bytes |
| --- | --- | ---: |
| Version/identity header | `0x1AE0..0x1AE7` | 8 |
| Allocated preference payload | `0x1AE8..0x1AF7` | 16 |
| Unallocated contiguous tail | `0x1AF8..0x1B37` | **64** |
| Total audited preference region | `0x1AE0..0x1B37` | **88** |

The new word uses nineteen of its thirty-two bits, leaving thirteen unassigned. Together with seven earlier padding bits, the current extension has **twenty unassigned bits inside its allocated payload**: byte 8 bits 3–7, byte 10 bit 7, byte 11 bit 7, byte 22 bits 3–7, and byte 23 bits 0–7. These twenty bits are additional to the 64-byte tail. Six preserved legacy Ledgechase encoding bits (the word at byte 12, bits 6–8 and 15–17) are excluded from free capacity.

The original 44-byte record separately has **one unused bit**, byte 37 bit 7 (`0x1F49`, mask `0x80`), and no unused whole bytes. Thus the contiguous expansion budget is **64 bytes / 512 bits**, with another twenty extension padding bits and one core bit available only through explicit field allocation and compatible initialization. About 73% of the audited extension region remains unallocated. This does not establish that any other part of the save is available.

Recomputed production field masks show 102 live payload bits + six preserved legacy bits + twenty unassigned bits = 128 payload bits. The header adds 64 bits, giving the current 192-bit / 24-byte record. All totals agree with `TM_EXTENSION_SIZE=24`, `TM_EXTENSION_VERSION=11` and the fixed end boundary at `0x1B38`. Existing native serialization, checksum, score-identity and records-reset protections continue to apply. For the detailed core reserve history and the new events' per-field budget, see [memory investigation addendum 54](OSD-GLOBAL-SETTINGS-INVESTIGATION.md#save-memory-addendum-2026-10-15).

## Version 12: Shield DI and expanded Reversal actions

Ownership stays **24 bytes at `0x1AE0..0x1AF7`**, leaving **64 contiguous bytes at `0x1AF8..0x1B37`** unallocated. Shield DI consumes bit 19 of the word at extension bytes 20..23 (In=0, Out=1). Bit 20 supplies Reversal's fifth attack bit, while its original four bits remain at 0..3. Attack on Shield's fifteen actions fit the existing four-bit slot at 9..12, so that expansion costs no new bits. Reversal's seventeen choices include Grab, Run Up JC Grab and Spotdodge Shine.

The event word now uses 21 bits, leaving bits 21..31 unused. Total unused payload capacity is **18 bits**: extension byte 8 bits 3..7, byte 10 bit 7, byte 11 bit 7, byte 22 bits 5..7, and byte 23. The sixteen-byte payload reconciles as **104 live preference bits + six preserved legacy bits + eighteen unused bits = 128 bits**. The original core separately retains one unused bit at byte 37 bit 7. Total settings ownership remains 68 bytes; native save-file size, card block count and score boundaries do not change.

**Version 13 (T5, October 17, 2026):** the global Randall Overlay flag claims extension byte 8 bit 3, beside A-Smash Release's palette in bits 0..2. Every migration into version 13 (including version 12) clears that bit, so old padding cannot enable it; all other bytes keep their values. Unused payload capacity is now **17 bits**: byte 8 bits 4..7, byte 10 bit 7, byte 11 bit 7, byte 22 bits 5..7, and byte 23. Size, the 64-byte tail, the core's one unused bit and native save boundaries are unchanged.

Version-11 migration preserves valid old choices and existing unrelated padding, clears the newly owned bits 19/20, and repairs formerly invalid action values to zero. It does not reinterpret invalid old Reversal 14/15 or Attack on Shield 10..15 as newly supported choices. Version-12 reads/writes reconstruct and mask Reversal's split five-bit index explicitly, preserving adjacent distance/facing fields. Prior migrations and foreign/unknown-format protections remain. Tests cover all legal/invalid action writes, split-bit round trips, version-11 migration with padding canaries and native records erasure for versions 1..12. Runtime input/approach state and pause-screen suppression consume no save bits. See [memory investigation addendum 55](OSD-GLOBAL-SETTINGS-INVESTIGATION.md#save-memory-shield-actions-v12).
