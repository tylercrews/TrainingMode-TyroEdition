# Attack on Shield and Reversal Training

Both events use the shared pause menu, with descriptions and an Exit option.
Their match initializers disable native pausing and hide its HUD, so the original
pause screen and rotating camera do not activate behind this menu. Native debug
camera hotkeys are skipped while the event menu is open.
L + R + A + Start exits from gameplay or the menu and saves edited preferences.
Their event-select names use opaque RGB `(0, 180, 180)`: cyan dimmed by the same
180/255 factor as the legacy gray-white names.

Attack on Shield saves CPU Action, Hits Before Reaction (1–5), Infinite
Shields, Player Distance and Shield DI (In/Out). The CPU keeps shielding until the requested number
of shield hits, then uses the selected response at the first shield-wait
opportunity. Separate hits in a multihit count through native shieldstun entries;
duplicate notifications in one frame count once. Restoring an attempt or editing
the menu clears the partial count. None keeps the CPU shielding. Infinite
Shields defaults On and applies to both fighters; Off allows normal shield
damage even when the global setting is On. It never edits the global preference.

CPU Action includes Grab, Spotdodge, Roll Away/Towards, Up B, Up Smash,
Wavedash Away, Short Hop Nair/Fair/Dair/Bair/Uair, Shine Out of Shield,
Spotdodge Shine and None. Short hops release jump during jumpsquat; Fair/Bair
follow CPU facing. Spotdodge Shine observes a real spotdodge before sending
down-B after recovery. Shine choices are intended for Fox/Falco; other fighters
receive their native down-B input. Shield DI defaults In and holds toward the
player during shieldstun/hitlag; Out holds away. Direction follows current
positions on either side at each shield hit and stays fixed through that hit.
Holding the direction preserves native SDI timing and its consumed-input marker;
it does not generate a fresh SDI input on every hitlag frame. Native ASDI still
applies when hitlag ends. The stick returns to neutral before shield-wait
responses, preventing the DI setting from causing an unintended roll.

Reversal saves CPU Attack (the original fourteen plus Grab, Run Up JC Grab and
Spotdodge Shine), Player Distance,
Player Facing, CPU Facing and Position. A saved platform choice applies on
entry, with the existing ground fallback if no platform is available. D-pad
Left/Right still fine-tunes spacing for the session. Choosing another distance
or position reapplies the preset.

Run Up JC Grab recomputes the player's horizontal position each update. It runs
toward that position, turns toward the player if necessary, and requests jump
when within 12 stage units. During the actual jumpsquat it sends grab, which the
native jumpsquat/standing-grab checks accept as Catch. It uses normal movement
and collision, without moving the CPU directly or extending grab range. The
12-unit threshold is a shared approach heuristic; roster spacing and moving or
airborne targets can still make the grab miss. The usual attempt timer bounds
the chase. All new action phases clear on retry, restore or a menu edit. Both
legacy native restore paths explicitly call the adapter's reset command; they
do not rely on the newer `Savestate_v1` observer cleanup.

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
fighter. Original Reversal attacks, random attack blacklists, reset timing and
manual spacing logic remain in assembly; the shared C adapter supplies the new
sequences and Attack on Shield inputs. Inputs update raw/processed controls and
grounded-special caches before invoking the fighter's native IASA callback.

The saved preferences occupy four appended bytes in `TYEX` version 12 (24 bytes
total). Shield DI claims bit 19 of this word; bit 20 extends Reversal's attack
index to five bits while retaining its original low four bits. The expanded
Attack on Shield catalogue still fits its existing four-bit field. Version 11
keeps valid old choices, clears the two newly owned padding bits, and repairs old
invalid action codes instead of interpreting them as new actions. Versions 1–10
retain their established migrations and receive the event defaults.
Unknown/foreign records are preserved. See [the storage audit](../notes/SCORE-STORAGE-AUDIT.md).

Run `bash tests/run_settings_tests.sh` with Unicorn on `PYTHONPATH`; it includes
the optimized relocated-DAT tests in `tests/test_shield_training.py` and executes
the assembled Attack on Shield reaction gate and Reversal dispatch. Tests cover saved menu mapping,
one to five hits, duplicate/multihit entries, reset clearing, distances and
followers, the local/global shield combinations, saved platform entry, native
pause flags and suppressed camera hotkeys, every aerial/response input, dynamic
chase direction and actual native jumpsquat/standing-grab acceptance. Native
shield SDI/ASDI callbacks verify that 1, 5 and 20 hitlag updates produce the same
displacement for a held direction. Both events' actual Exit callbacks run through
menu A-selection and gameplay/paused LRAS paths, checking unpause, match-ending
state and suppression of repeated exits from a held chord.
Dolphin validation remains necessary for visual layout, live controller feel,
roster attack trajectories and an actual memory-card save/reload.
