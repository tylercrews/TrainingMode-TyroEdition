# Adding CPU Characters to Edgeguard Training

**Status (October 19, 2026):** seven new CPU recoveries are implemented: Yoshi, Peach, Luigi, Dr. Mario, the Ice Climbers, Jigglypuff and Samus. They compile and the save/menu wiring is tested, but **none of them has been watched in Dolphin yet**. Every threshold below is a starting point to tune. Pikachu is not implemented; it looks doable ([see below](#pikachu-not-implemented)).

Sources: the Melee decompilation (doldecomp/melee) for move logic and action state IDs, and the vanilla ISO for numbers. [`scripts/fighter_move_data.py`](../scripts/fighter_move_data.py) prints every move height and timing window used here.

## Short answer

| CPU | What it does | How sure | Main thing to check in Dolphin |
| --- | --- | --- | --- |
| Yoshi | Double jump high or low, then air dodge into the ledge or onto the stage; optional Egg Toss | High | Air dodge to ledge actually grabs (only the helpless fall after the dodge can) |
| Peach | Double jump and float onto the stage, float low by the ledge, Parasol to the ledge, optional Bomber and float aerials | Medium | Float starts at the top of the jump and ends over the stage |
| Luigi | Double jump, mash Cyclone, Green Missile across gaps (game's own 1 in 8 misfire), up B lined up under the ledge | Medium | Cyclone height at normal mashing; up B lining up |
| Dr. Mario | Double jump, mash Dr. Tornado, Cape stall, up B from about 15 out | Medium | Tornado then up B order; up B start distance |
| Ice Climbers | Double jump, Belay when Nana is close, mash Squall Hammer otherwise | Medium-low | Nana keeps up (she runs the game's own follower AI) |
| Jigglypuff | Holds a high or low height with her five jumps, Pound across gaps, back/forward/neutral air at the player | Medium | Jump spacing; aerials not sending her offstage |
| Samus | Double jump, bomb hops to stall and drift, grapple aimed at the wall just under the ledge (then reels in), Screw Attack | Medium | Grapple connects and the reel ends on the ledge |

## How these recoveries are built

Everything is in [src/recovery.c](../src/recovery.c), after Marth. They follow the existing pattern: a recovery menu of toggles per character, and a `Think_*` function that writes the CPU's stick and buttons every frame. A few things are new and shared:

- **Inputs are assigned, not added.** `Recovery_Think` clears the new characters' buttons and sticks every frame (`RecoveryAssignsInputs`). The game copies the CPU's buttons into the fighter's input each frame (`fighter.c`), and only acts on *presses*, so the AIs must control exactly when a button goes down and up.
- **`Press()`** makes a fresh press: if the button was already down last frame it is released for a frame first. A move that's requested every frame is then retried every other frame until the game takes it.
- **`MashB()`** presses B once every 6 frames, give or take one (about 10 presses a second), or every 4 with Fast Mashing (about 15, a very fast human). Presses are never on consecutive frames.
- **`DriftHome()`**: above the ledge-grab point, drift onto the stage; below it, line up where the up special should start instead of drifting under the stage.
- **Ledge reach**: a rise reaches the ledge if its top ends no more than `LEDGE_GRAB_REACH` (10) below the grab point. That margin comes from the existing Falcon and Marth AIs: their up B thresholds (48 and 60) against their real rises (37.6 and 47.3).
- **Tumble**: knockback states after hitstun and DamageFall allow jumps, specials, aerials and the tether, but no air dodge or float (`ftCo_DamageFall_IASA`). The AIs don't try those from tumble.

### Where the heights come from

Many recovery moves aren't driven by a velocity number at all. The game sets the fighter's velocity each frame to the change in the animation's **TransN** bone, times the model scale (`ftanim.c`, `ft_800851C0`). So a move's rise is the TransN curve in the character's `PlXxAJ.dat`. The script decodes those curves with a port of the game's own key interpreter (`sysdolphin/baselib/fobj.c`; the key data is little-endian). It also reads the subaction scripts for the frames that set `cmd_vars`, which the move code uses for windows like "mash counts now" or "may grab the ledge from here".

| Move | From the ISO | Used for |
| --- | --- | --- |
| Marth up B | rises 47.3 by frame 21 | Calibration (AI uses 60 to the grab point) |
| Falcon up B | rises 37.6 by frame 44 | Calibration (AI uses 48) |
| Yoshi double jump | rises 52.5 by frame 52, after an 8-frame dip | When to jump; no dodging while it rises |
| Peach double jump | rises 33.2 by frame 50, after an 8-frame dip; float check from frame 30 | Holding jump to float at the top |
| Peach Parasol | rises 36.8 by frame 33, 3.4 forward; ledge only on the way down | Parasol range |
| Luigi up B | rises 45.9 by frame 26, straight up; ledge from frame 9 when falling | Line up first |
| Luigi Cyclone | B counts frames 6 to 44; lift used up at 44 | Mash window |
| Dr. Mario up B | rises 40.4 by frame 24, 17.6 forward | Start about 15 outside the grab point |
| Dr. Tornado | B counts frames 0 to 38; lift used up at 38 | Mash window |
| Squall Hammer | B counts frames 0 to 54 (with Nana) or 57; then helpless | Mash window |
| Belay | checks Nana within 60 on frame 5 | Only Belay with Nana close |
| Jigglypuff jumps | next jump allowed from frame 28 of the last | Jump spacing |
| Pound | angle read on frame 12 | Hold the stick up to frame 12 |
| Samus Screw Attack | no root motion: 2.5 up against 0.066 gravity, about 48 | Screw Attack range |

## Mashing within human limits

The tornado, Cyclone and Squall Hammer lift on each **fresh** B press during their window. The lift amounts are from the decomp and `PlXx.dat`:

- **Dr. Tornado:** +1.2 per press, capped at 1.4, against gravity 0.095.
- **Cyclone:** +0.8 per press, capped at 1.4, against gravity 0.069.
- **Squall Hammer:** +0.67 per lift with Nana (+0.6 alone), at most one lift every 7 frames.

The tornado and Cyclone each lift only once. Dr. Mario's lift (`tornadoCharge`) comes back on landing. Luigi's (`cycloneCharge`) only comes back when a Cyclone touches down, the known Melee quirk; Edgeguard's reset restores both every attempt. Simulated over 40 random mash patterns (worst / average height gained):

| Presses a second | 7.5 | 8.6 | 10 (normal) | 12 | 15 (fast) |
| --- | --- | --- | --- | --- | --- |
| Dr. Tornado | 23 / 27 | 25 / 32 | 31 / 36 | 35 / 40 | 40 / 44 |
| Luigi Cyclone | 0 / 0 | 0 / 2 | 4 / 15 | 0 / 22 | 18 / 32 |
| Squall Hammer with Nana | 45 / 54 | 58 / 72 | 75 / 78 | 75 / 79 | 75 / 79 |
| Squall Hammer alone | 10 / 13 | 14 / 18 | 21 / 21 | 21 / 21 | 21 / 21 |

So Luigi only gets real height from fast mashing (true in Melee too), and Squall Hammer stops improving past one lift per 7 frames. By the end of the move a 10-a-second tornado or Cyclone can be up to 12 *lower* than it started, so the AIs only use them when an up B can still reach afterwards (`*_SAFE`), or as a last resort.

## Per character

### Yoshi

- **Game facts:** the double jump is animation-driven with armor (`ftYs_JumpAerial_Enter`). Egg Toss gives no lift in the air and returns to a normal fall. Air dodge: 3.1 speed decaying ×0.9 a frame (about 31 units). The dodge itself can't grab the ledge (`ftCo_EscapeAir_Coll` only checks for landing); the helpless fall after it can.
- **AI:** "Jump High" double jumps at or above ledge height and drifts in; "Jump Low" waits until below. It always jumps by 38 under the grab point. After the jump it air dodges toward a point 10 above the grab point so the fall catches the ledge, or onto the stage when the stage is within reach. It never dodges or attacks while the jump is rising.
- **Options:** Jump High, Jump Low, Air Dodge to Ledge (on); Air Dodge to Stage, Egg Toss (off).

### Peach

- **Game facts:** the float lasts 150 frames, once per airtime (`has_float` at fp+222C). It starts with down + jump held, or with jump held when a jump's rise ends. Holding jump or up keeps it going. Up B and float aerials work from the float. The Parasol grabs only on the way down. Bomber doesn't leave her helpless.
- **AI:** "Float to Stage" double jumps holding jump, floats at the top and drifts in, letting go once over the stage. "Float Low" floats near ledge height and drifts into the ledge. Parasol from below within 46.8; Bomber across long gaps; float aerials at a close player.
- **Options:** Float to Stage, Float Low, Parasol (on); Peach Bomber, Float Aerials (off).

### Luigi

- **Game facts:** the up B is straight up, with nearly no stick angle (2°). Green Missile charges while B is held (from a 20-frame head start on a smash input, up to 90), misfires 1 in 8 (`HSD_Randi(8)` in `ftLg_SpecialS_SetVars`), and doesn't leave him helpless.
- **AI:** double jump, Green Missile charged more for longer gaps, Cyclone when the up B can still reach afterwards, then up B lined up within 10 of the grab point. It forces the up B from about 46 under.
- **Options:** Double Jump, Cyclone, Green Missile (on); Up B Early, Fast Mashing (off).

### Dr. Mario

- **Game facts:** the up B goes 17.6 forward while rising, and grabs only on the way down. The first cape each airtime gives a 0.7 upward stall (`isCapeBoost`).
- **AI:** like Luigi, but lines the up B up about 15 outside the grab point. Optional cape stall to change the timing.
- **Options:** Double Jump, Dr. Tornado (on); Cape Stall, Up B Early, Fast Mashing (off).

### Ice Climbers

- **Game facts:** Belay checks for Nana within 60 on its fifth frame (`checkNanaInRange`). With her, Popo is pulled toward her at 1.2 + distance/12; alone it's a 1.8 hop (about 16). Squall Hammer ends helpless (30 frames landing lag), so it's Belay or Squall, not both.
- **AI:** Popo is the CPU. It double jumps, Belays when Nana is within 45 and not in hitstun, and otherwise mashes Squall Hammer. A solo Belay is only a last resort with Squall turned off.
- **Event fixes this needed** ([src/edgeguard.c](../src/edgeguard.c)):
    - Reset no longer turns Nana's AI off (`cpu.ai = 15`). Savestates don't restore the CPU block, so she would have stopped following Popo.
    - Nana's follow history now points at Popo, not the human (`Reset_Subchar(cpu_sub, cpu)`).
- **Options:** Belay, Squall Hammer (on); Fast Mashing (off).

### Jigglypuff

- **Game facts:** five midair jumps are her own states (341 to 345). The first needs a fresh jump press; the rest need jump held, each from frame 28 of the last (`ftCo_800D730C`). Pound's angle is up to 20° up from stick Y 0.1 to 0.5 (below the 0.55 up B threshold), read on frame 12. She isn't helpless after it.
- **AI:** each attempt picks a height to hold, above the ledge (Jump High) or level with it (Jump Low), and jumps back up to it while drifting in. Pound across gaps. Back air at a player behind her, forward or neutral air in front.
- **Options:** Jump High, Jump Low, Pound, Aerials (all on).

### Samus

- **Game facts:**
    - **Bombs:** laying one in the air sets 0.9 upward speed and multiplies air acceleration ×22 (`ftSs_SpecialAirLw_Enter`), so bomb hops travel fast. A bomb going off on her is a 1.7-speed launch at 90° ± up to 45° by her offset, and she can act during it (state 342).
    - **Grapple:** Z (L/R + A), facing the stage. Once per airtime (`used_tether`), and **a miss leaves her helpless** (`ftCo_800968C8`). From `ftCo_AirCatch.c`, `itsamusgrapple.c` and `PlSs.dat`:
        - The head leaves her hand on frame 7 of AirCatch, about 9 up and 9 forward (the arm's pose that frame), and flies **straight out** at 1.7 a frame. The air chain is 45 links of 0.88 (the base 30 × 1.0, times 1.5 for the air version and her 0.88 scale), so it reaches about 40 from the hand. It retracts from frame 40.
        - It only latches onto a **wall** (floors bounce it off), so it has to fly into the side of the stage, under the ledge. If the hand is already past the wall on frame 7, the game cancels it.
        - Once latched (AirCatchHit, 358) she **hangs until A is pressed**, then reels in. Without a press she drops into tumble after 180 frames (`ftSs_DatAttrs` xD0).
        - At the end of the reel she grabs the ledge if it's in her grab box, widened by 5 (`ftCo_800C3A14`): 7.3 to 20.3 above her feet, so the head hit 0 to 11 under the ledge. Otherwise she hops up like a ledge jump (`ftCo_8009B390`) and can still act.
    - **Screw Attack:** grabs only on the way down.
- **AI:** bomb hops toward the stage when far, holding still over the bomb when close so it launches her. It grapples only when facing the ledge, falling, and where the head will leave from (8 frames ahead) is 13 to 40 outside the ledge and 2 to 10 under it, so the reel ends on the ledge. Without a double jump it always takes the last frames of that window. Once latched it presses A 4 frames in to reel in. Otherwise it Screw Attacks.
- **Options:** Double Jump, Bomb Jumps, Grapple, Screw Attack (on); Screw Attack High (off).

## Pikachu (not implemented)

Quick Attack is more mechanical than it looks (`ftpikachuspecialhi.c`, `PlPk.dat`):

- **First zip:** its direction is the stick at the end of the start-up. Its speed is 4.0 × stick magnitude + 5.9 for 5 frames: about 50 units at full tilt, less with a softer stick. Below 0.5 it goes straight up at full speed.
- **Second zip:** happens on frame 9 of the end animation if the stick angle differs from the first by more than the 0xA8 attribute (stored as 38, presumably degrees). It's 0.9 times as fast.
- **Ledge:** both zips can grab it.

An AI would aim the first zip like Fox's Firefox and use the second to correct. The hard part is the **sweetspot**: real Pikachus zip into the ledge from below, and the exact angles need testing. It also needs saved toggles, and the new save word is full. A Pikachu menu needs another 4-byte word (20 bytes left).

## Other effects

- **Training Lab:** the Lab includes `recovery.c`, so these seven characters now use the new recoveries (and get a **Recovery Options** menu) when the CPU is set to recover. They used the game's own CPU AI before.
- **Save data:** the new characters' toggles are saved in `TYEX` version 19, a fourth Edgeguard word (bytes 64 to 67), leaving 20 spare bytes. See the [save data memory map](SAVE-DATA-MEMORY-MAP.md).

## Testing in Dolphin

For each character, run Edgeguard with the default options and **don't** hit the CPU. A good recovery AI should make it back most of the time from Normal hit strength.

1. **Yoshi:** does the air dodge to ledge grab? Does it ever dodge mid-jump?
2. **Peach:** does she float at the top of the double jump, and stop floating over the stage? Does Float Low drift into the ledge?
3. **Luigi / Dr. Mario:** does the up B start in the right place (Luigi under the ledge, Doc about 15 out)? With Fast Mashing off, does the Cyclone or tornado leave them able to up B?
4. **Ice Climbers:** does Nana follow Popo after a reset? Does Belay fire with her close and give full height?
5. **Jigglypuff:** do the jumps space out, or does she burn them early? Do the aerials hit a player standing at the ledge?
6. **Samus:** does the grapple connect, and does the reel end on the ledge? If it hits the wall but she hops up instead of grabbing, `SS_GRAPPLE_MAX_DEPTH` is too deep for that stage; if it flies over the stage, raise `SS_GRAPPLE_MIN_DEPTH` or `SS_GRAPPLE_HAND_Y`. If it never misses, `SS_GRAPPLE_MAX_X` can grow toward about 48.
7. **High recoveries:** with "high" options on (Jump High, Float to Stage, Up B Early, Screw Attack High), does the CPU land on the stage rather than on the ledge?
