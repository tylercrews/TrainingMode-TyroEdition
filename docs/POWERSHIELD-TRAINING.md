# Powershield Training

General Tech **#10** has its own laser recovery cue and two retained OSDs,
arranged as the same centered pair used in Under Fire. The pause menu's **Help**
submenu explains shielding, the reflection window, geometry and both readouts.

Since T11 the CPU is **Falco** or **Samus**, picked on the event's character
select (it starts on Falco) or with **Change Characters**, and the stage is any
of the six legal stages, from the stage select or **Change Stage**. See
[Characters, stages and Samus](#characters-stages-and-samus).

| Saved option | Default |
| --- | --- |
| Fire Speed | Random |
| Min / Max Fire Delay | 0 / 20, inclusive |
| Laser Height (Missile Height for Samus) | Random |
| Samus Pattern | Short Hop; available only for Samus |
| Direction | Right |
| Movement | In-Place; available only for Falco |
| Nana | On; available only for Ice Climbers |
| Event OSDs | On; controls both event readouts |
| Suppress Global OSDs | Off |
| Laser Recovery Cue | On |

Direction shortcuts also save the selected direction. Changing one random-delay
bound adjusts the other when necessary and saves both. Equal bounds are valid;
the generator never calls `HSD_Randi(0)`. Exit queues the existing native save
path. Global suppression leaves saved global preferences intact and does not
hide the event readouts. Event OSDs and Laser Recovery Cue are independent.

## Act out of Laser

The first readout shows **1f** when an input-driven action begins on the first
unfrozen opportunity after the latest laser interaction. Its source line is
**Laser hit**, **Shield hit**, or **Powershield**. Ordinary damage hitstun and
shieldstun block the clock. A powershield starts counting after any native freeze
ends. Shielding, walking and waiting continue the count until another action
changes the fighter's state during the native input/IASA pass. Shield startup,
hold, release and reflection, all walking speeds, walking turns and stops are
excluded, including Yoshi's egg-shield states.

Frame 1 is cyan, frame 2 green, frame 3 yellow, and frame 4 onward red.
Hitlag, pause, duplicate callbacks, automatic
recovery transitions and CPU/subfighter actions do not advance or complete the
measurement. State-preserving inputs such as shield tilt are not completed
actions. A new laser replaces an unfinished attempt. Reset, restore, death,
spawn changes and clock rewind clear the episode and retained results.

**Laser Recovery Cue** (**Missile Recovery Cue** for Samus) turns the human
yellow for the final two unfrozen frames of the CPU's laser or missile damage
hitstun or shieldstun, then green for two unfrozen
frames after it ends, including immediate next actions. Shieldstun uses the
native animation length and playback rate; green waits for the actual exit
from GuardSetOff (or Yoshi's GuardDamage). An accepted reflection has no
shieldstun, so it goes directly green on the first unfrozen update and stays
green for **four unfrozen frames** to make the confirmation easier to see. A new
contact replaces the previous cue. It works with global Actionable Yellow>Green
Off and takes precedence over ordinary global body tints. Rendering restores
all three native color structures. The same cue also colors the player's
shield, including the white reflection flash: a draw wrapper on the owning
fighter's four native shield effects temporarily colors their texture TEV
registers, preserving animated alpha and restoring every changed register
after drawing. Opponent shields and unrelated effects keep their native
colors. Cue Off, restore, death and respawn discard the tint; scene changes
detach the shield draw wrappers. Damage, stun and action timing remain native.

## What can I do after a powershield?

Green means the laser's stun has ended. While you are still shielding, the
normal restrictions on shield actions apply; it does not promise an immediate
dash or grounded attack.

| Interaction | Recovery and follow-ups |
| --- | --- |
| Reflected laser | No shieldstun from the reflection. Use normal out-of-shield options. Releasing shield still has normal shield-drop recovery before a dash or ordinary grounded attack. |
| Ordinary laser shield hit | Wait through hitlag and shieldstun, then use normal out-of-shield options. |
| Physical attack powershield | Still has shieldstun. Afterward, a separate cancel window allows attacks during shield release; it does not add a shield-release dash cancel. |

For most characters, jump into an aerial, wavedash to reposition, grab, roll,
spotdodge, or jump-cancel into up-smash/up-B. A wavedash bypasses shield-drop
recovery, but still costs jumpsquat and airdodge landing recovery, so the best
follow-up depends on character, spacing and your goal. It is a useful movement
option after reflecting a laser, rather than expecting an instant shield-drop
dash. Character-specific shield rules still apply.

The [native shield routines](https://github.com/doldecomp/melee/blob/master/src/melee/ft/kinds/ftCommon/ftCo_Guard.c)
keep these mechanics separate: GuardSetOff has no IASA actions, GuardReflect
uses the normal shield input checks, and GuardOff only checks the expanded
attack list when `guard.x1C` is nonzero. The physical powershield collision path
calls `ftCo_80094138` to grant that timer; the accepted projectile-reflection
path in [ftcoll.c](https://github.com/doldecomp/melee/blob/master/src/melee/ft/ftcoll.c)
does not. Neither GuardOff input list includes a dash check.

## Powershield OSD investigation and repair

The former hook at `0x800776A0` ran only inside the normal projectile/shield
collision path. It never observed accepted projectile reflection, so it could
not report success. It labeled an active reflector **Early**, although a laser
can intersect the normal shield while missing the reflector. Its number came
from shield startup variable `+0x2340`, with different arbitrary offsets in its
failure branches. That variable can retain analog startup before a digital
click opens reflection. The old late branch also wrote through an unrelated
live register.

New native hooks observe:

- **Accepted reflection**, `0x8007758C`: after the reflector's damage-cap test.
  GuardReflect, including Yoshi's matching egg state, identifies a powershield;
  Shine and other reflectors are excluded.
- **Normal shield contact**, `0x800776A0`: a shield hit, never inferred to be a
  reflected projectile from the action-state name alone.
- **Accepted body hit**, `0x800780E0`: after native hit logging; phantoms bypass
  this point.

The native [shield implementation](https://github.com/doldecomp/melee/blob/master/src/melee/ft/kinds/ftCommon/ftCo_Guard.c)
and [projectile collision implementation](https://github.com/doldecomp/melee/blob/master/src/melee/ft/ftcoll.c)
distinguish the projectile reflector from the physical powershield flag. The
local unmodified NTSC 1.02 `PlCo.dat` initializes the reflector timer
(`ftCommon + 0x2A4`) to 1. Startup has timer 1, the next frame timer 0, and native
animation disables it only after it becomes negative: **frames 1 and 2 both
reflect**. The separate physical flag alone does not prove laser reflection.

The repaired global and event OSDs report **Success: 1f / 2f** for accepted
reflection. Both are green successes. The number counts from eligible digital
shield startup, excluding frozen frames, rather than from Falco firing.
Failed event contacts report:

- **Reflect active**: the normal shield took the hit during the reflection
  window. Check spacing, posture and laser height; early input is not proven.
- **Reflect ended**: contact after the tracked reflector window. The shield
  frame continues counting beyond frame 2.
- **No hard press**: no tracked eligible digital startup; no frame is invented.
- **No shield**: the laser hit the player's body; no shield frame is shown.

Click L/R fully just before impact. Analog light shield alone does not create
the reflector, and a slow squeeze can miss the eligible click. Different
shield/reflector geometry means a shield hit during the window can require a
spacing or height adjustment. The Help submenu repeats these points in game.

The global Powershield category retains its identity and works outside this
event. Event callbacks receive only Falco-laser and Samus-missile contacts
(`ITEM_FALCOLASER`, `ITEM_SAMUSMISSILE`, which both of her missiles share) on the main human,
independent of global OSD settings. Shared export 49 dispatches timing phases
0..4 and contact phases 5..7; the **50-slot function table does not grow**.
Scene cleanup detaches event callbacks before its module unloads.

## Characters, stages and Samus

The event uses the training character select. The CPU door offers only Falco
and Samus, and starts on Falco unless your saved CPU is one of them
(`EventDesc.cpuDefault`). The stage select follows. In the pause menu,
**Change Characters** has **Player Character** and **CPU Character** (Falco or
Samus; Left/Right picks, X/Y the costume, A restarts), and **Change Stage** the
six legal stages, the current one greyed out. Both restart the match straight
into the event with your saved settings.

Any other stage, which the stage select's Random can pick, restarts on Final
Destination on the first frame, the event's original stage. The other
stage-select events restart on Battlefield.

### Where everyone stands

Direction **Right** is the savestate: you on the left at FD's −60, facing left
as before, and the CPU on the right at +60, facing you. On another stage each
is scaled by that side's edge over FD's (85.5657), so FD's spots are exact.
Direction **Left** mirrors it. Both stand on the live floor. The CPU's spot moves
off any platform its short hop could land on: one within 29 of Samus's floor
(her 22.75 apex plus her airborne ECB's bottom) or 18 of Falco's, with
Fountain's side platforms taken at their lowest, 12.375. Falco's ±70 limits on
his approach and retreat scale the same way. Samus's Platform and Random
patterns stand her under the right side platform instead.

| Stage | You | Falco | Samus: Short Hop | Samus: Platform / Random |
| --- | --- | --- | --- | --- |
| Final Destination | −60 | 60 | 60 | 60 (no platform) |
| Battlefield | −48 | 48 | 12 | 38.8 |
| Dream Land | −54.2 | 54.2 | 54.2 (its platforms are out of reach) | 47.4 |
| Yoshi's Story | −39.3 | 39.3 | 20 | 43.75 |
| Pokémon Stadium | −61.5 | 61.5 | 63 | 40 |
| Fountain of Dreams | −44.4 | 13 | 13 | 35.25 |

### Samus's numbers

From `PlSs.dat` and `PlCo.dat` in the ISO (and the [decomp's side-B](https://github.com/doldecomp/melee/blob/master/src/melee/ft/kinds/ftSamus/ftsamusspecials.c)):

- A 3-frame jumpsquat into a 1.7 short hop (22.75 high) or 2.1 full hop
  (34.46), gravity 0.066 to a terminal 1.4. Entering the aerial missile clears
  a fastfall, and she never fastfalls.
- Side-B is B with the stick at least 0.6 sideways and less than 0.55 up or
  down. It's a **super missile** if the stick crossed the smash threshold fewer
  than 3 frames before (special attribute 0x28), so every press comes after a
  frame with the stick neutral.
- The aerial super missile (motion state 352) lasts 50 frames. The missile
  spawns on its animation frame 15 (its 16th frame) at joint 56, her cannon,
  9.92 above her feet, and flies flat: 0.6 a frame, speeding up to 2.2, for 75
  frames, about 130 units. Landing during the move goes to Landing, and before
  the spawn it cancels the missile.
- Dropping through a platform is a smash down (0.66 within 6 frames) from
  Wait: Squat, then Pass 2 frames later at −0.5.

### Missile Height

**Very Low**, **Low**, **Mid** and **High** are 10, 13, 16.5 and 20.5 above
the floor below her; **Random** picks one each time. Every missile but
Platform's rising one comes on the way down, on the frame that comes closest:
the event predicts where she'll be 16 frames after a press (it's read the next
frame, then the move's 15) under gravity to terminal speed, plus the cannon.
Each comes out within half a frame's fall, 0.7. Very Low spawns with her feet
within that of the floor, while her ECB's bottom is 3.25 over them, so she
can't land first.

A missile after dropping from a platform can't be higher than one pressed on
the drop's first frame, about 7 under the platform: Battlefield 20.1, Dream Land
23.2, Yoshi's Story 17.3 (its platforms are 24.4 over the floor under them),
Pokémon Stadium 17.9, and Fountain 5 to 20 as its platforms move. There, High
is as high as the drop allows. From Fountain's lowest platforms she lands before
the missile comes out.

### Samus Pattern

- **Short Hop** (the default): a short hop from the ground with a missile on
  the way down.
- **Platform**: from under the side platform she full hops straight up with a
  super missile on its first airborne frame, about 34.5 over her floor (a little
  higher in game, where the jump has already risen its first frame), and lands
  on the platform. After the Fire Speed delay she drops through it and fires on
  the way down, at the Missile Height, then lands and goes again. That rising
  missile flies over you on the ground, but 4 to 10 over the other side
  platform: stand there to shield it. If the platform isn't there (one of
  Fountain's sinks), the full hop just lands back on the ground.
- **Random**: each time she can act on the ground, Short Hop or Platform.

Final Destination has no platforms: Platform and Random show **(N/A)** and play
short hops. Fire Speed and the delay bounds set her wait on the ground and on a
platform, and Direction mirrors her. **Movement** is Falco's only. With Samus,
the rows and readouts say **Missile Height**, **Missile Recovery Cue**, **Act
out of Missile**, **Missile hit** and **Reflected missile**.

### Tests and what Dolphin has to show

`tests/test_powershield.py`'s `SamusTests` run the production event against a
frame model of Samus in the probe, built from the numbers above, on each legal
stage's native collision lines from the ISO and a Fountain with moving and
disappearing platforms. They cover every stage's start spots for both CPUs (FD's
exactly −60 and 60) and Direction Left's mirror, each Missile Height's short hop
missiles on every stage, Random's four heights, the Platform pattern's
alternating rising and falling missiles on every stage with platforms, Random's
mix, FD's N/A patterns, Fountain, the restart on FD from another stage, the rows
each CPU greys out and names, Change Characters' Falco and Samus and Change
Stage's six, and Samus Pattern saving and moving her. Each run checks for no
wrong move (a homing missile, a bomb or up-B, B on the ground) and no fastfall.
The model isn't Dolphin. Still to check in game:

- Falco starts on the right on FD with you on the left, as before. The stage's
  spawns (slot 0 at −60, slot 1 at +60) say so; the code used to keep them.
- The missiles are super missiles and their heights look like the table. If the
  CPU's raw input is read the same frame instead of the next, each comes a frame
  earlier: up to 1.4 higher.
- The drop through each platform, the full hop landing on each, and Fountain's
  moving platforms.
- The character select's CPU door shows only Falco and Samus and starts on
  Falco; a Random stage restarts on FD.
- Reflected missiles read **Success**, and the readouts' labels.

## Persistence and verification

Append-only settings field 31 owns ten preferences in four new extension bytes.
`TYEX` **version 8** owns 20 bytes (`0x1AE0..0x1AF3`), up from 16. It stays within
the audited 88-byte preference area, leaving **68 bytes unallocated**. The
44-byte core, original scores and Ledgechase's slot 50 remain unchanged.
Versions 1..7 retain their preferences through migration; new bytes initialize
to Powershield defaults. Missing records read defaults without writing until an
edit. Foreign identities and unknown formats preserve persisted bytes.

Tests execute the production event, optimized relocated DAT, shared service and
all three emitted collision hooks. They check real native reflection acceptance
and rejection, both successful frames, source filtering, IASA action counts,
two-yellow/two-green hitstun and shieldstun cues at multiple animation rates,
four-frame reflection recovery, physical versus projectile shield-release cancels,
immediate actions, hitlag/pause/duplicate callbacks,
cleanup, saved settings and delay bounds. Render tests check tint priority and
native color restoration. Shield tests cover the native setup paths for all
four effects, white-flash replacement, multipart and shared materials,
alpha preservation, owner filtering and cleanup. Wider regressions cover migration, record reset and
native checksum/serialization. Real Dolphin layout and physical memory-card
save/reload remain manual checks.
