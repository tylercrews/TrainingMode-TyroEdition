# A-Smash Release

The existing OSD producers had no A-button release measurement. The new shared
producer displays the number of unfrozen input opportunities between entering
an A-input smash and releasing A. Releasing at the first following input is
`1f`; holding through it and releasing on the next is `2f`. Releases before the
character reaches its charge point also produce a result. This measures the
player's A hold rather than a character-specific animation frame or accumulated
damage multiplier.

The setting uses category 30 and message kind 70. It uses the existing tagged
message ABI, global palette, master suppression, CPU suppression and layout
service. The timing target is 1, giving the existing cyan/green/white/red timing
colors for 1/2/3/4+ frames. Existing OSD identities and their saved palette bytes
are unchanged; the setting's palette belongs to the separate save extension.

## Native entry and input hooks

The local NTSC 1.02 DOL confirms the following addresses and register contracts:

- `0x800693EC`, inside Fighter_ChangeMotionState: the existing ShiftStates hook
  calls `OSD_ASmashEnter(r26, r24)` before native `state_id` changes. `r26` is the
  fighter data, `r24` is the requested state, and `r27` is the model JOBJ.
- `0x800DF0E8`, inside the common smash input handler: `OSD_ASmashInput(r3)` sees
  the updated native buttons before charge-entry/release logic changes them.
  `r3` is still the fighter GOBJ and `r31` is its fighter data. The displaced
  native instruction remains `lwz r0, 0x2114(r31)`.

The A-origin test reproduces native left-stick thresholds and timers. It requires
the actual A press and rejects C-stick-only smashes, including a simultaneous A
press with an unrelated left-stick direction. Dash forward smashes and jump-squat
up smashes retain their native relaxed-timer behavior. Common smashes, Game &
Watch's forward smash, Peach's three forward smashes and Ness's custom smashes are
included. Ness's charge-state transitions keep the initial episode.

Native charge state 3 retires the episode before an A-release result, so a smash
that automatically releases at its limit cannot report a subsequent A release.
Ness's separate release states retire it too. State changes, death, respawn,
rewinds and the shared restore/scene clear also retire pending episodes. Frozen
or repeated same-frame callbacks do not increase the count. Subfighters do not
produce duplicate results.

The native counter at `0x2118` is a float, although the SDK calls it an integer.
The producer leaves it untouched and uses its own twelve-entry runtime table
(288 bytes). Neither hook changes charge behavior, input or animation rate.

Primary decompilation references are the
[common charge/input handlers](https://github.com/doldecomp/melee/blob/master/src/melee/ft/ft_0DF0.c)
and the
[forward-smash input branches](https://github.com/doldecomp/melee/blob/master/src/melee/ft/kinds/ftCommon/ftCo_AttackS4.c).
The local executable, rather than decompilation names alone, supplies the hook
addresses and register verification.

## Verification

`tests/test_asmash.py` has thirteen focused PowerPC regressions. They execute the
actual native charge/input instructions from `build/Start.dol` with the assembled
hooks, verify first-opportunity and delayed releases, test automatic release at
the native float counter's limit, and check hook GPR/FPR/CR/CTR/XER preservation.
They also execute the optimized production DAT callbacks after native MEX
relocation, covering the loader path that an ELF-only test cannot verify.

Run `tests/run_settings_tests.sh` to regenerate the ELF, optimized DAT and hook
fixtures, then `python tests/test_asmash.py -v`. Dolphin gameplay and display
verification remain separate from these native-code regressions.
