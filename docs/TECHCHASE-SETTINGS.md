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
Starting Location, CPU Drop Height, Player Distance, Spotdodge Punish and CPU
Invincibility Overlay are also saved. Camera coordinates,
manually moved fighters and individual percentages remain session-only.

Starting Location offers Center Stage, Left Plat, Right Plat and Top Plat.
The legal-stage platform resolver supports Yoshi's Story, Fountain of Dreams,
Final Destination, Pokemon Stadium, Battlefield and Dream Land. It reads
enabled live collision lines on every reset, following Fountain's moving
platforms. CPU Drop Height offers Normal (27 units, default), Short (15), Tall
(40), and Double (54), measured above the selected surface. Player Distance
offers Right Next To (10 units, default), Close (20), Medium (30), and Past
Tech Roll (45). The player starts on solid ground toward stage center, facing
the CPU. With defaults, Center Stage retains the original x=-5/player and
x=5/CPU positions. These distances are independent of Ledgechase's presets.

Unavailable platforms are labeled N/A and skipped when changing the selector.
Final Destination has no platform choices; Stadium has no top platform.
An unavailable saved choice falls back to Center Stage for the current event
without overwriting the saved preference. Selecting a location restores
automatic positioning after manual fighter movement. Changing height or
distance also restores automatic positioning and restarts the attempt.

Both new selectors use two bits each; Disable Attack Particles in Under Fire
uses one bit. All five bits occupy padding within the existing 16-byte save
extension. Version 4 migrates versions 1–3, retaining existing preferences and
initializing the new choices to their defaults. No global OSD storage changes.

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
saved preferences, live geometry, missing-platform handling and punish
transitions. Its spotdodge test executes the local DOL's actual Wait IASA
and eligibility instructions. Visual spacing and timing on all six stages
still require gameplay verification in Dolphin.
