# Techchase settings

The first row of Option Chances selects a preset. Only that choice is saved;
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
Starting Location and Spotdodge Punish are also saved. Camera coordinates,
manually moved fighters and individual percentages remain session-only.

Starting Location offers Center Stage, Left Plat, Right Plat and Top Plat.
The legal-stage platform resolver supports Yoshi's Story, Fountain of Dreams,
Final Destination, Pokemon Stadium, Battlefield and Dream Land. It reads
enabled live collision lines on every reset, following Fountain's moving
platforms. The CPU drops from 27 units above the selected surface; the player
starts on the solid ground 10 horizontal units toward stage center, facing
the CPU. Center Stage retains the original x=-5/player and x=5/CPU positions.

Unavailable platforms are labeled N/A and skipped when changing the selector.
Final Destination has no platform choices; Stadium has no top platform.
An unavailable saved choice falls back to Center Stage for the current event
without overwriting the saved preference. Selecting a location restores
automatic positioning after manual fighter movement.

Spotdodge Punish waits through the chosen tech or missed-tech getup. On the
first Wait frame, it supplies a fresh down/R input and calls the native Wait
IASA, allowing normal spotdodge eligibility to decide the transition. It
includes the native processed shoulder flag; no tech animation is shortened.
The configured Fast/Slow reset delay starts after the spotdodge finishes.
Reset None continues to disable automatic resets.

`tests/test_techchase.py` executes the production C menu, positioning and
punish logic as PowerPC code. It checks every preset, session-only edits,
saved preferences, live geometry, missing-platform handling and punish
transitions. Its spotdodge test executes the local DOL's actual Wait IASA
and eligibility instructions. Visual spacing and timing on all six stages
still require gameplay verification in Dolphin.
