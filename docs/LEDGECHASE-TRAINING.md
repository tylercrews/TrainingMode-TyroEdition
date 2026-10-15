# Ledgechase Training

General Tech #17 uses selectable human and CPU fighters on Final Destination,
skipping stage selection. Each attempt starts the CPU holding the selected
ledge with fresh native ledge invulnerability. The player starts grounded on
stage, facing the CPU. Both damage values use the native percent HUD.

The event saves these seven choices:

| Setting | Choices | Default |
| --- | --- | --- |
| Ledge Option Chances | Seven presets and session-only percentage edits | Even |
| Ledge Side | Left, Right, Swap on Success, Random | Left |
| CPU 100% | Off / On; sets exactly 0% / 100% | Off |
| Player Percent | 0%, 25%, 50%, 75%, 100%, 125%, 150% | 0% |
| Player Distance | Edge, Close, Mid, Far, Safe | Close |
| CPU Invincibility Overlay | Off, Red, Blue, Yellow, Green, Cyan, Magenta, Gray | Gray |
| CPU Action | Training Lab ground counter actions, without Play Slots or Random Advanced | None |

Native ledge-option selection switches to the slow variants at **100%**.
The CPU percent toggle sets this value exactly. Player-distance steps are
4, 12, 24, 40 and 80 stage units inward from the ledge. Safe places the player
near center stage with a generous margin from ledge attacks; exact spacing
and hurtbox coverage still need live roster verification.

| Preset | Getup | Jump | Roll | Attack |
| --- | ---: | ---: | ---: | ---: |
| Even | 25 | 25 | 25 | 25 |
| Getup Only | 100 | 0 | 0 | 0 |
| Jump Only | 0 | 100 | 0 | 0 |
| Roll Only | 0 | 0 | 100 | 0 |
| Attack Only | 0 | 0 | 0 | 100 |
| Tyro setting | 60 | 10 | 15 | 15 |
| Getup / Roll | 50 | 0 | 50 | 0 |

Changing an individual percentage rebalances the remaining choices to 100%.
Only the selected preset is saved. Ledge Hop Chance and Ledge Dash Chance now
participate in the same six-way rebalance. The existing presets give both new
options 0%; set either chance to 100% to isolate it for testing. Both manual
chances remain session-only.

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

Ledge jumps fastfall on the first native-eligible descending frame. The
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

The 16-byte settings extension is version 7. The action selector uses six bits
in byte 15; its former three bits remain untouched. Versions 2 through 6 migrate
the old actions to their corresponding new choices. The 50/50 preset is appended
so existing preset identities remain stable. Resetting damage clears pending
damage rather than copying the starting percent into it, preventing a second
application of damage on the next native update.
