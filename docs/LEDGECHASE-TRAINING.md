# Ledgechase Training

General Tech #17 uses selectable human and CPU fighters on Final Destination,
skipping stage selection. Each attempt brings the CPU to the selected ledge
using the chosen entry method. The player starts grounded on
stage, facing the CPU. Both damage values use the native percent HUD.

The event saves these ten choices:

| Setting | Choices | Default |
| --- | --- | --- |
| Ledge Option Chances | Fifteen presets and session-only percentage edits | All Normal Options |
| Ledge Side | Left, Right, Swap on Success, Random | Left |
| CPU 100% | Off / On; sets exactly 0% / 100% | Off |
| Player Percent | 0%, 25%, 50%, 75%, 100%, 125%, 150% | 0% |
| Player Distance | Edge, Close, Mid, Far, Safe | Close |
| CPU Invincibility Overlay | Off, Red, Blue, Yellow, Green, Cyan, Magenta, Lavender | Lavender |
| CPU Action (Grounded) | Training Lab ground counter actions, without Play Slots or Random Advanced | None |
| CPU Action (Jumping) | Five aerials, Neutral B (Repeat), Up B, Side B Toward/Away, Down B, Random | None |
| Ledge Setup / Get to Ledge | Spawn on Ledge, Fall to Ledge, Jump to Ledge | Jump to Ledge |
| Ledge Setup / Ledge Hop Height | Low (Auto), Medium (Auto), Original | Low (Auto) |

Ledge Setup also has a session-only **Hop Drop Adjustment**, from -12 to +12
stage units. Positive drops deeper; negative raises the attack arc.

Fall to Ledge starts 24 units above the lip, outside the stage, and uses
native falling/collision to grab it. Jump to Ledge starts below the lip at a
character-dependent depth and requests one native aerial jump. Standard jumps
aim about 5 units above the lip; animation-driven jumps use a 16-unit starting
depth. Jump depth remains bounded to 12..70 units. Both steer
gently toward an outside holding position and wait for the actual catch and
hang states. The ledge-option clock begins only after every active CPU body
finishes grabbing; approach time does not consume the punish window. A failed
approach retries after a landing/death or a 240-frame timeout. Spawn on Ledge
retains the original direct placement. Recovery-move entry is a future-work
comment in the source, with no menu placeholder or special-move script.

Native ledge-option selection switches to the slow variants at **100%**.
The CPU percent toggle sets this value exactly. Player-distance steps are
4, 12, 24, 40 and 80 stage units inward from the ledge. Safe places the player
near center stage with a generous margin from ledge attacks; exact spacing
and hurtbox coverage still need live roster verification.

| Preset | Getup | Jump | Roll | Attack | Hop | Dash |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| All Normal Options | 25 | 25 | 25 | 25 | 0 | 0 |
| Getup Only | 100 | 0 | 0 | 0 | 0 | 0 |
| Jump Only | 0 | 100 | 0 | 0 | 0 | 0 |
| Roll Only | 0 | 0 | 100 | 0 | 0 | 0 |
| Attack Only | 0 | 0 | 0 | 100 | 0 | 0 |
| Tyro setting | 60 | 10 | 15 | 15 | 0 | 0 |
| Getup / Roll | 50 | 0 | 50 | 0 | 0 | 0 |
| Ledge Hop Only | 0 | 0 | 0 | 0 | 100 | 0 |
| Ledge Dash Only | 0 | 0 | 0 | 0 | 0 | 100 |
| Regular Four + Hop | 20 | 20 | 20 | 20 | 20 | 0 |
| Regular Four + Dash | 20 | 20 | 20 | 20 | 0 | 20 |
| All Six Even | 1/6 | 1/6 | 1/6 | 1/6 | 1/6 | 1/6 |
| Dash 75 / Jump 25 | 0 | 25 | 0 | 0 | 0 | 75 |
| Hop / Dash | 0 | 0 | 0 | 0 | 50 | 50 |
| Dash 50 / Others 10 | 10 | 10 | 10 | 10 | 10 | 50 |

Changing an individual percentage rebalances the remaining choices to 100%.
Only the selected preset is saved. Ledge Hop Chance and Ledge Dash Chance now
participate in the same six-way rebalance. The seven original presets retain
their distributions and identities. All Six Even samples each option with
exactly 1/6 probability; its integer rows display 17/17/17/17/16/16. Editing
any chance switches to the displayed whole-percent distribution for this
session. Reselect the preset to restore exact equality.

Ledge Hop releases with an outward input, fastfalls to its selected low-hop
depth, double jumps inward, then drifts and fastfalls onto stage. Original
height or None aerial action keeps the immediate double jump. It uses each character's native
aerial jump, including animation-driven and multijump characters. It is
separate from Jump Chance, which still selects the built-in ledge jump.

Ledge Dash is **experimental**: it uses the same release/jump, then requests
an inward/down air dodge after a conservative origin/ECB clearance check.
This is not an optimized GALINT script or a verified roster success matrix.
Characters can return without useful GALINT, land before dodging, or miss;
the existing retry rules still apply. Native input/sequence tests cover all
playable internal kinds, but full animated collision behavior needs in-game
verification. See [the mechanics investigation](LEDGEDASH-MECHANICS.md) for
character handling, sources, limitations and a roster verification procedure.

Seventy percent of attempts choose the first legal ledge-action frame. The
remaining attempts spread their delay through the available ledge protection
and at most five vulnerable frames. Normal getup respects the native neutral
stick-release latch, so its first legal input can be later than the other
button-driven options. Manual release also respects that latch. The event supplies processed CPU inputs to native IASA; it
does not replace ledge animations or shorten their recovery.
The supplied inputs also refresh Melee's cached special-move classifications,
so grounded Down B reaches the native special check instead of only crouching.
See the native [input counter update](https://github.com/doldecomp/melee/blob/master/src/melee/ft/fighter.c).

With CPU Action (Jumping) set to None, ledge jumps fastfall on the first native-eligible descending frame. The
selected CPU action follows the ledge option when normal native action checks
allow it. The action catalogue and input sequences are shared with Training Lab, including
specials, tilts, aerials, hops, wavedashes, directional rolls and dashes. Random
uses Training Lab's ground random-action pool and chooses once per attempt.
Each followup runs once, finishing its complete input sequence before releasing
its inputs. Ledge-jump fastfall ends when the grounded followup begins, so it
does not interfere with a subsequent hop, aerial or wavedash. None leaves the CPU idle after its ledge option.
A 30-frame punish window follows completion of that action. Hits or
grabs start a 15-frame retry delay; deaths and an attempt timeout also retry.
Swap on Success changes sides only after a successful hit or grab. D-pad Left
retries without counting success. Exit flushes pending preferences.

CPU Action (Jumping) is independent of CPU Action (Grounded). It
borrows the aerial/special input commands from Training Lab without issuing
another jump. The native ledge jump must finish both uninterruptible stages
before attacking. A ledge hop waits until rising, at least one unit inside
the edge and four units above it, with its ECB bottom above the floor. This
avoids deliberately cancelling a rising animation-driven jump below the lip;
it is a conservative heuristic rather than a guarantee for every move.
Ordinary aerials and directional specials are requested once per attempt.
Random chooses one attack per active CPU body at reset. Ledge Dash and the
other ledge options ignore this setting. Low Fox/Falco lasers have a separate
below-lip startup gate described below.

Low/Medium attack hops fastfall **before** the aerial jump. For ordinary jumps,
the controller predicts vertical rise from native jump velocity and gravity;
multijump fighters use their first aerial-jump velocity. The low target apex,
measured at the fighter origin above ledge height, is 10 units for Fox/Falco
Neutral B, 22 for Down Air, 16 for Forward Air, 10 for Up Air, and 14 for other
choices. Medium adds 12 units. Hop Drop Adjustment subtracts from that target,
bounded to 8..40 units. Jump depth is bounded at 70 units below the lip.

Ness/Yoshi/Peach/Mewtwo have animation-driven jumps rather than that ballistic
trajectory. Their initial extra drops are conservatively 4/6/2/4 units,
halved for Medium and bounded to 0..12 after manual adjustment. These are
starting tuning profiles, not measured optimal attack heights. The input
waits until the target depth, requests the native jump, and lets native jump
entry clear fastfall. No position or velocity is edited during the drop.

Neutral B pulses repeatedly while airborne. For Fox/Falco, the controller
feeds their native blaster repeat window, watches the first loop restart into
the second, then releases B to avoid requesting a third loop. Fastfall is
delayed through the second loop and resumes in blaster endlag. This delay
applies after the jump; the setup drop still fastfalls. In Low/Medium mode,
Fox/Falco may begin blaster startup up to 14 units below the lip and within
eight units outside it, while rising, if a six-frame gravity projection clears
the lip by two units. Their jump continues during blaster, allowing the first
laser to start lower without cancelling the rise. The six-frame gate is a
clearance heuristic, not an exact shot/hitbox simulator. For Falco
double-laser practice, choose **Ledge Hop Only** and **Neutral B (Repeat)**.
The tests verify native repeat acceptance, input release and fastfall timing;
full projectile counts and survival for each character/setup need in-game
verification. Eye-level contact varies with opponent height, move startup,
hitboxes, ECB and the ledge grab; use the manual adjustment to tune a matchup.
Other characters' repeated Neutral B follows their own native
charge/loop/cancel rules. Specials can alter drift or recovery substantially.

The CPU overlay shares the global invincibility detector and render-time
composition. Its local color overrides the global CPU protection tint,
including Off. Human global colors remain active. The same event-local
palette is available in Techchase Training. Scene cleanup detaches the menu
option pointer before the event module unloads.

`tests/test_ledgechase.py` covers production event logic as PowerPC code;
`tests/test_ledgechase_catalogue.py` checks the actual relocated shared DAT's
General Tech #17 entry, selectable CPU, fixed stage and native HUD contract.
The score-storage and extension tests cover all original scores, the new
fixed score slot and migration from the earlier T4 settings extension.

The 20-byte settings extension is version 10. Byte 19 stores the expanded preset
in bits 0..3 and the aerial selector in bits 4..7. The earlier preset bits remain
untouched. Versions 2..8 migrate their original seven preset choices and start
aerial attacks at None; version 8's Powershield settings are preserved. The
ground action selector still uses six bits in byte 15, retaining the earlier
action migrations. Byte 14 bits 6..7 save entry; byte 15 bits 6..7 save hop
height. Versions 1..9 default these newly claimed fields to Jump to Ledge and
Low (Auto), preserving version 9's fourteen presets and aerial selection.
Existing version 10 saves retain their selected entry method.
Resetting damage clears pending
damage rather than copying the starting percent into it, preventing a second
application of damage on the next native update.
