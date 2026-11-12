# Techchase settings

The first row of Tech Chances selects a preset. Both the parent option and
submenu title use this name. Only the preset choice is saved;
editing any individual percentage remains available for the current session.
Selecting another preset reapplies its four tech probabilities. Starting the
event restores the saved preset and the original defaults for getup choices.

| Preset | Tech in place | Tech away | Tech toward | Miss tech |
| --- | ---: | ---: | ---: | ---: |
| Original | 40 | 30 | 30 | 0 |
| Even | 25 | 25 | 25 | 25 |
| In Place Only | 100 | 0 | 0 | 0 |
| Away Only | 0 | 100 | 0 | 0 |
| Toward Only | 0 | 0 | 100 | 0 |
| Miss Tech Only | 0 | 0 | 0 | 100 |
| Place / Toward | 50 | 0 | 50 | 0 |
| Away / Toward | 0 | 50 | 50 | 0 |
| Tyro Setting | 60 | 15 | 15 | 10 |

Reaction Frame OSD, Timing OSD, Reset, Tech Invisibility, Camera mode,
Starting Location, CPU Drop Height, Player Distance, Player Side, Spotdodge
Punish and CPU Invincibility Overlay are also saved. Camera coordinates,
manually moved fighters and individual percentages remain session-only.

Starting Location offers Center Stage, Left Plat, Right Plat and Top Plat.
The legal-stage platform resolver supports Yoshi's Story, Fountain of Dreams,
Final Destination, Pokemon Stadium, Battlefield and Dream Land. It reads
enabled live collision lines on every reset, following Fountain's moving
platforms. CPU Drop Height offers Normal (27 units, default), Short (15), Tall
(40), and Double (54), measured above the selected surface. Player Distance
offers Right Next To (10 units, default), Close (20), Medium (30), Past Tech
Roll (45) and Random. Random rolls a new spacing on every reset, uniformly
from 10 through 30, so any spot from Right Next To to Medium, not only those
three. These distances are independent of Ledgechase's presets. Reversal and
Attack on Shield share the four fixed ones (`TC_FixedDistance`), not Random.

Player Side (Left, default; Right; Random) picks which side of the CPU the
player starts on, on solid ground and facing it. Random flips again on every
reset. With defaults, Center Stage retains the original x=-5/player and
x=5/CPU positions. Right mirrors Center Stage and Top Plat, so the CPU drops at
x=-5 and the player stands to its right. On the side platforms the side is
literal: the old toward-center layout is Left Plat with Right, or Right Plat
with Left. When the spacing would leave the stage, the player stops 5 units in
from the ledge (`TC_LEDGE_MARGIN`, Edgeguard's At Ledge spacing). Yoshi's side
platforms overhang its ledges, so left of the left platform the player is
7.25 units from the CPU at every distance.

D-Pad left/right picks Left/Right and restarts. Like the menu row, it saves the
choice and restores automatic positioning. Pressing the side already chosen
only restarts, as D-Pad left always has, so manually moved fighters stay put.
From Random, either press picks a fixed side. The press is read from the master
pad, which savestates don't rewind.

Unavailable platforms are labeled N/A and skipped when changing the selector.
Final Destination has no platform choices; Stadium has no top platform.
An unavailable saved choice falls back to Center Stage for the current event
without overwriting the saved preference. Selecting a location restores
automatic positioning after manual fighter movement. Changing height,
distance or side also restores automatic positioning and restarts the attempt.

Both new selectors use two bits each; Disable Attack Particles in Under Fire
uses one bit. All five bits occupy padding within the existing 16-byte save
extension. Version 4 migrates versions 1–3, retaining existing preferences and
initializing the new choices to their defaults. No global OSD storage changes.

Random distance and Player Side (T10) take three more bits with no version
bump. Random is distance value 4, whose top bit is word 9's spare bit 15 (byte
10 bit 7). No version ever wrote that bit, so saved distances keep their
meaning, and migrating a version 1 record clears it. Player Side takes byte 50
bits 3–4, next to the human costume, which is now read through bits 0–2.
Costumes only go to 5, and every costume write and repair kept bits 3–4 clear.
Records before version 17 don't own byte 50 and read Left. A damaged side (3)
or distance (5–7) repairs to its default without touching its neighbors.

Spotdodge Punish waits through the chosen tech or missed-tech getup. On the
first Wait frame, it supplies a fresh down/R input and calls the native Wait
IASA, allowing normal spotdodge eligibility to decide the transition. It
includes the native processed shoulder flag; no tech animation is shortened.
The configured Fast/Slow reset delay starts after the spotdodge finishes.
Reset None continues to disable automatic resets.

CPU Invincibility Overlay selects Off, Red, Blue, Yellow, Green, Cyan, Magenta
or Lavender (default). It uses the shared natural-protection detector, including
move and dodge invulnerability. This event-local choice overrides the global
protection tint for CPU fighters, including when set to Off. Human coloring
continues to follow Global Settings. The same setting is available in Ledgechase.

`tests/test_techchase.py` executes the production C menu, positioning and
punish logic as PowerPC code. It checks every preset, session-only edits,
saved preferences, live geometry on both sides (including the ledge stop),
Random's rolls, the D-Pad, missing-platform handling and punish
transitions. Its spotdodge test executes the local DOL's actual Wait IASA
and eligibility instructions. Visual spacing and timing on all six stages
still require gameplay verification in Dolphin.
