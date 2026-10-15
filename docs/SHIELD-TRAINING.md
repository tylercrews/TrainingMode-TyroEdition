# Attack on Shield and Reversal Training

Both events use the shared pause menu, with descriptions and an Exit option.
L + R + A + Start exits from gameplay or the menu and saves edited preferences.
Their event-select names use opaque RGB `(0, 180, 180)`: cyan dimmed by the same
180/255 factor as the legacy gray-white names.

Attack on Shield saves its OoS Option, Hits Before Reaction (1–5), Infinite
Shields and Player Distance. The CPU keeps shielding until the requested number
of shield hits, then uses the original response at the first shield-wait
opportunity. Separate hits in a multihit count through native shieldstun entries;
duplicate notifications in one frame count once. Restoring an attempt or editing
the menu clears the partial count. None keeps the CPU shielding. Infinite
Shields defaults On and applies to both fighters; Off allows normal shield
damage even when the global setting is On. It never edits the global preference.

Reversal saves CPU Attack (all fourteen existing choices), Player Distance,
Player Facing, CPU Facing and Position. A saved platform choice applies on
entry, with the existing ground fallback if no platform is available. D-pad
Left/Right still fine-tunes spacing for the session. Choosing another distance
or position reapplies the preset.

Both events share `TC_PlayerDistance` with Techchase:

| Preset | Separation in stage units |
| --- | ---: |
| Right Next To (default) | 10 |
| Close | 20 |
| Medium | 30 |
| Past Tech Roll | 45 |

Spacing is between fighter origins. Reversal preserves the selected surface's
center and height; Attack on Shield centers the pair on FD and retains its
randomized sides on retries. Followers move by the same offset as their main
fighter. Existing attack commands, random attack blacklists, timings and manual
Reversal spacing logic remain in the native assembly routines.

The saved preferences occupy four appended bytes in `TYEX` version 11 (24 bytes
total). Versions 1–10 migrate without changing their previously owned choices.
Unknown/foreign records are preserved. See [the storage audit](../SCORE-STORAGE-AUDIT.md).

Run `bash tests/run_settings_tests.sh` with Unicorn on `PYTHONPATH`; it includes
the optimized relocated-DAT tests in `tests/test_shield_training.py` and executes
the assembled Attack on Shield reaction gate. Tests cover saved menu mapping,
one to five hits, duplicate/multihit entries, reset clearing, distances and
followers, the local/global shield combinations, and saved platform entry.
Dolphin validation remains necessary for visual layout, live controller feel,
roster attack trajectories and an actual memory-card save/reload.
