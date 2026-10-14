# Spare event-score storage audit

Verified October 14, 2026 against the current event catalogue, `build/Start.dol`,
the optimized shared event DAT and the compiled native hooks. The original
44-byte settings format remains at `0x1F24..0x1F4F`.

The usable candidate is **92 bytes / 736 bits at game-data offsets
`0x1AE0..0x1B3B`**. These are score slots 28 through 50, not raw GCI offsets.
The three pages contain 5, 16 and 7 events; their 28 four-byte scores occupy
`0x1A70..0x1ADF`. The separate event-played flags at `0x1A68..0x1A6F` and the
native word beginning at `0x1B3C` are excluded.

The new extension owns only the first **12 bytes at `0x1AE0..0x1AEB`**, with a
separate `TYEX`, version, size and marker header. **80 bytes remain unallocated**.
Native high scores have not moved or been removed, and OSD settings have not
been repacked. See `src/settings_extension.h` and `src/settings_extension.c`
for the current payload; the audit establishes the boundary independently of
that payload.

## Native save and checksum coverage

The native manifest at `0x803BAB74` declares its main data entry as `0x1790`
bytes. The getter at `0x8015CC40` returns game-data base plus `0x1868`, so the
candidate lies at logical payload offsets `0x278..0x2D3`, safely inside the
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
| Event records reset `0x8015EEC8` | Clears 25 fighter-record subranges, then 216 bytes at `0x1A68..0x1B3F`. The original final clear erases the entire candidate. | Hook `0x8015EF14` replaces only that final clear. On `TYRE01`, clear the 120-byte played/assigned-score prefix and the four-byte adjacent word separately, preserving all 92 candidate bytes. |
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
The catalogue has a compile-time guard requiring at most 28 scores. Adding a
29th event therefore requires an explicit storage relocation/migration instead
of silently overlapping extension bytes. Keep the storage boundary fixed when
removing events; do not slide persisted preferences according to current count.

Records-reset preservation checks the complete six-byte identity `TYRE01`.
The tests verify that `GTME01`, `GALE01` and even `TYRE02` keep the original
216-byte native clear. The separate Tyro save namespace prevents ordinary
upstream/vanilla use from accessing a Tyro save automatically.

The extension has independent format recognition. Foreign identity, unsupported
core format and unsupported extension versions use private runtime defaults;
unknown persisted bytes are retained. An event-records reset preserves all 92
candidate bytes on Tyro, including formats this build cannot understand.
The owned payload is bounded to 12 bytes; unrelated spare bytes remain untouched.

Unmodified older Tyro executables cannot implement these protections: their
normal current-catalogue score writes do not touch the candidate, but their
native **Erase Event Records** action still clears it. Fresh-save creation,
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

The audit has **10 passing tests**. It performs no physical card writes and
does not edit the input DOL, DAT or GCT. `CARDWrite` is intercepted in the emulator.
