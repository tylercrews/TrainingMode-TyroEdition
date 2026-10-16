# Powershield Training

General Tech **#10** has its own laser recovery cue and two retained OSDs,
arranged as the same centered pair used in Under Fire. The pause menu's **Help**
submenu explains shielding, the reflection window, geometry and both readouts.

| Saved option | Default |
| --- | --- |
| Fire Speed | Random |
| Min / Max Fire Delay | 0 / 20, inclusive |
| Laser Height | Random |
| Direction | Right |
| Movement | In-Place |
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

**Laser Recovery Cue** turns the human yellow for the final two unfrozen frames
of a Falco laser's damage hitstun or shieldstun, then green for two unfrozen
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
event. Event callbacks receive only Falco-laser contacts on the main human,
independent of global OSD settings. Shared export 49 dispatches timing phases
0..4 and contact phases 5..7; the **50-slot function table does not grow**.
Scene cleanup detaches event callbacks before its module unloads.

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
