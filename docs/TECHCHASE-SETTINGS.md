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
| Getup Attack Only | 0 | 0 | 0 | 100 |
| Place / Toward | 50 | 0 | 50 | 0 |
| Away / Toward | 0 | 50 | 50 | 0 |
| Tyro Setting | 60 | 15 | 15 | 10 |

Getup Attack Only (T10) is the one preset that also sets the getups: Stand,
Roll Away and Roll Toward 0, Getup Attack 100. Miss Tech Wait is left alone.
The other presets keep this session's getup edits, except that leaving Getup
Attack Only puts the getups back to the even 25s. It is listed after Miss Tech
Only but saved as preset 9, after Tyro Setting (`TC_PresetToSaved`), so a save
from before it still loads In Place / Toward, Away / Toward or Tyro Setting.
The 4-bit saved field already held 16 values, so there's no version bump.

Reaction Frame OSD, Timing OSD, Reset, Tech Invisibility, Camera mode,
Starting Location, CPU Drop Height, Player Distance, Player Side, CPU Followup
Action and CPU Invincibility Overlay are also saved. Camera coordinates,
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

## CPU Followup Action

CPU Followup Action (T10, formerly the Spotdodge Punish toggle) is what the CPU
does as soon as its tech or missed-tech getup ends, so you practice punishing
that instead of the tech:

| Choice | The CPU |
| --- | --- |
| None (default) | stands, as before: the reset timer starts on its first Wait frame |
| Spotdodge | spotdodges |
| Roll Away | rolls away from stage center (x = 0) |
| Roll In | rolls toward stage center |
| Shine | Fox and Falco only: shines, jumps out of it and wavelands |
| Random | picks one of the others again every reset, Shine only for Fox and Falco |

Each one waits through the whole tech or getup and starts on the first Wait
frame. Every step supplies this frame's processed input (and the raw CPU input
the next frame decodes) and calls the native IASA, so the game's own
eligibility checks decide each transition, and nothing is shortened:

- **Spotdodge** is unchanged: a fresh down/R input, with the native processed
  shoulder flag, through the Wait IASA.
- **Rolls**: Wait can't roll, only shield, so the first frame is a held R
  (`Interrupt_Shield`), and the next frame, still holding, a fresh full smash
  to the side out of GuardOn (`Interrupt_Roll`). The shield is never a fresh
  digital press, so it's GuardOn, not GuardReflect's powershield. Right at x = 0
  "away" means away from the player. Rolls stop at a floor's edge natively.
- **Shine**: Down B from Wait (through the cached special classification, so it
  can't come out as another special), B held through the shine's start, a fresh
  X in its loop (`Interrupt_Jump_Grounded`), X let go in jumpsquat (a short hop),
  and on the jump's first airborne frame a fresh R at 45 degrees down
  (`Interrupt_EscapeAir_Airdodge`): a waveland. It wavelands away from the
  player, but a waveland slides, so if the CPU's floor (its platform, or the
  solid ground) ends within 30 units (`TC_WAVELAND_ROOM`) that way it goes back
  toward the player, and with no room either way, straight down.

The followup is punishable from its first state until the CPU stands again.
Then it's intangible, as at the end of a tech (Spotdodge Punish used to leave
it hittable there), and the configured Fast/Slow reset delay starts. Reset
None continues to disable automatic resets. A Timing OSD "Early" counts against
the dodge, the roll or the waveland's landing lag. A hit earlier in the shine,
in its jump or its air dodge, shows no timing.

Shine reads "Shine (N/A)" for any other CPU, and stepping through the row skips
it, as Starting Location skips missing platforms. A saved Shine reads None for
another CPU, for the current event, without overwriting the saved choice.

The setting is stored without a version bump. Spotdodge Punish's one bit, word
9 bit 14, stays its low bit, so a saved Off or On reads as None or Spotdodge.
Its other two bits are word 80's spare bits 4-5, which every migration to
version 23 zeroes, and only a version 23 record reads them: an older record
reads the low bit alone, which is all it ever held. A damaged 6 or 7 repairs to
None without touching the rest of either word.

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
and eligibility instructions. The followup tests run the local DOL's own
Wait, special, shield, GuardOn, roll, shine-loop, jump and air dodge checks
with PlCo.dat's thresholds from the base ISO; only the state entries are
replaced, by recorders. Visual spacing and timing on all six stages, and every
followup in game (each roll's direction, Fox's and Falco's shine, jump and
waveland), still require gameplay verification in Dolphin.
