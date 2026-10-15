# Ledgechase Training

General Tech #17 uses selectable human and CPU fighters on Final Destination,
skipping stage selection. Each attempt starts the CPU holding the selected
ledge with fresh native ledge invulnerability. The player starts grounded on
stage, facing the CPU. Both damage values use the native percent HUD.

The event saves these eight choices:

| Setting | Choices | Default |
| --- | --- | --- |
| Ledge Option Chances | Fourteen presets and session-only percentage edits | Even |
| Ledge Side | Left, Right, Swap on Success, Random | Left |
| CPU 100% | Off / On; sets exactly 0% / 100% | Off |
| Player Percent | 0%, 25%, 50%, 75%, 100%, 125%, 150% | 0% |
| Player Distance | Edge, Close, Mid, Far, Safe | Close |
| CPU Invincibility Overlay | Off, Red, Blue, Yellow, Green, Cyan, Magenta, Gray | Gray |
| CPU Action | Training Lab ground counter actions, without Play Slots or Random Advanced | None |
| Aerial Attack When Jumping | Five aerials, Neutral B (Repeat), Up B, Side B Toward/Away, Down B, Random | None |

Native ledge-option selection switches to the slow variants at **100%**.
The CPU percent toggle sets this value exactly. Player-distance steps are
4, 12, 24, 40 and 80 stage units inward from the ledge. Safe places the player
near center stage with a generous margin from ledge attacks; exact spacing
and hurtbox coverage still need live roster verification.

| Preset | Getup | Jump | Roll | Attack | Hop | Dash |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Even | 25 | 25 | 25 | 25 | 0 | 0 |
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

Changing an individual percentage rebalances the remaining choices to 100%.
Only the selected preset is saved. Ledge Hop Chance and Ledge Dash Chance now
participate in the same six-way rebalance. The seven original presets retain
their distributions and identities. All Six Even samples each option with
exactly 1/6 probability; its integer rows display 17/17/17/17/16/16. Editing
any chance switches to the displayed whole-percent distribution for this
session. Reselect the preset to restore exact equality.

Ledge Hop releases with an outward input, double jumps inward on a later
frame, then drifts and fastfalls onto stage. It uses each character's native
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

With Aerial Attack When Jumping set to None, ledge jumps fastfall on the first native-eligible descending frame. The
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

Aerial Attack When Jumping is independent of the grounded CPU Action. It
borrows the aerial/special input commands from Training Lab without issuing
another jump. The native ledge jump must finish both uninterruptible stages
before attacking. A ledge hop waits until rising, at least one unit inside
the edge and four units above it, with its ECB bottom above the floor. This
avoids deliberately cancelling a rising animation-driven jump below the lip;
it is a conservative heuristic rather than a guarantee for every move.
Ordinary aerials and directional specials are requested once per attempt.
Random chooses one attack per active CPU body at reset. Ledge Dash and the
other ledge options ignore this setting.

Neutral B pulses repeatedly while airborne. For Fox/Falco, the controller
feeds their native blaster repeat window, watches the first loop restart into
the second, then releases B to avoid requesting a third loop. Fastfall is
delayed through the second loop and resumes in blaster endlag. For Falco
double-laser practice, choose **Ledge Hop Only** and **Neutral B (Repeat)**.
The tests verify native repeat acceptance, input release and fastfall timing;
full projectile counts and survival for each character/setup need in-game
verification. Other characters' repeated Neutral B follows their own native
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

The 20-byte settings extension is version 9. Byte 19 stores the expanded preset
in bits 0..3 and the aerial selector in bits 4..7. The earlier preset bits remain
untouched. Versions 2..8 migrate their original seven preset choices and start
aerial attacks at None; version 8's Powershield settings are preserved. The
ground action selector still uses six bits in byte 15, retaining the earlier
action migrations. Resetting damage clears pending
damage rather than copying the starting percent into it, preventing a second
application of damage on the next native update.
