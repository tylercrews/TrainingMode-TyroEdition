<p align="center"><img src="Logos/Training-Mode-banner.png"  alt=""  width="300"/></p>

# Training Mode - Tyro Edition

Training Mode - Tyro Edition is an augmented version of Aitch's Training Mode - Community Edition, which is an extension of UnclePunch's training modpack for Super Smash Bros. Melee. It corrects what I feel are shortcomings to the program, making improvements based solely on my personal experiences as a frequent user and what I think would be useful for my own practice. I have developed these features with the use of AI and have no plans to recreate any of my work without the use of AI, so for that reason they will not be eligible for merging into the Community Edition and should remain exclusive to this version of the program. Please do not use any work from this repository in part or whole as a contribution to Aitch's TM-CE.

I don't have my own community and I don't intend to spread this mod in a way to compete with community edition, I'm just going to continue making features that I think will be useful for myself.
So if you would like to access Aitch's original repo and discord you can go here. It is a great community and Aitch is an incredible dev:
* To download the regular Community Edition ISO, click [here](https://github.com/AlexanderHarrison/TrainingMode-CommunityEdition/releases/latest).
* Join the Community Edition [discord](https://discord.gg/2Khb8CVP7A) to discuss changes, new features, or ask for assistance.

## Development
Please read [DEVELOPMENT.md](DEVELOPMENT.md).

The Tyro version and stable game/save identity live in [version.h](version.h). Update `TM_VERSION` for a new release; keep `TM_GAME_ID` stable. Run `./build.sh --version` to see the version, game ID, and output filename.

## My Settings
<img width="747" height="587" alt="tm-te settings pg 1" src="https://github.com/user-attachments/assets/93f3f656-36ba-43ca-abdb-62522e2b907e" /> 
<img width="749" height="615" alt="tm-te settings pg 2" src="https://github.com/user-attachments/assets/75197645-145d-495b-8f2f-efd4fcae1f79" />
<img width="745" height="619" alt="tm-te settings osds 2" src="https://github.com/user-attachments/assets/1fb33b7a-d1ba-448a-bbf7-82d14302563b" />


So here's what I have turned on as an aspiring fox player:

- OSD Display is set to Fixed Grid, which is a display that I came up with where all of the OSDs are permanently in a fixed position at the top so you don't have to figure out which is which when they're popping up, and you can always look to the same place to see the one you're currently working on.
- Page 1, OSDs:
    - **Wavedash - Teal -** still trying to make sure that I'm getting short hops when I wavedash instead of full hops.
    - **Fastfall Timing - Blue -** as a newbie fox this is integral in my shffl practice, just trying to get my short hop nairs to fastfall, especially in eggsercise to see how well I'm fastfalling after hitlag
    - **Fighter Specific Tech - Cyan -** cyan for shine, I split it into two visible OSDs, and then a third for the spaceys (side b shorten) doesn't come up as much so I don't mind that one sharing the shine one as much. But acting out of shine is a must of course
    - **A-Smash Release - Yellow -** There are sometimes where I like JC Upsmash with the cstick and sometimes I like it with A. But I've noticed glowey frames when I put out my upsmashes so I needed to make this OSD to be sure. Going into slow-mo I saw that glow starts on frame 3, but once I made this it turned out I was really doing it more often at frame 5 or frame 4, which was a real eye opener. This is a new change but I think it's going to be a permanent mainstay for me for my run up upsmash practice.
    - **Act OoWait - Magenta -** The Act Out of Wait is crucial because it pairs with the Actionable Yellow -> Green Overlay. The overlay tells you when to go and then the act out of wait tells you how well you did at that. I'm always trying to act faster out of landing (with and without aerials) and out of wavedash back in particular.
    - **Act OoAttack - Red -** I created this one to be similar to Act OoWait, but for attacks. It'll let you see how quickly you start moving again after a smash attack, but the real reason I made it is to see how quickly you start jumping after you upthrow a character. Gotta get that tight, and it pairs extremely well with the overlay to see how well you're doing.
    - **Act OoJump - White -** This one will tell you how quickly you're putting out your aerials, trying to hit those F1 nairs, but this one is a more recent addition for me tbh. I'm not a zumper so I haven't been trying to force the f1 upair tech.
    - **OVERRIDE CPU OSDS OFF -** I don't really care about seeing when the CPUs do their actions, I don't see the use in it and it clutters the screen. So I created this override so that you only see your own actions.
- Page 2, Overlays:
    - **Hitbox Trails Very Fast -** I think this might be the most useful setting for a new player of anything. Being able to see where moves are hitting at all times gives you a much better idea of spacing, of placing where you're hitting, and visibility into how you're making mistakes. You also notice little quirks of the game better, like when you're running around on FD upsmashing eggs in eggsercise, seeing how much narrower the running upsmash hitbox is compared to standing hitbox, and where it actually hits on the ground is so important. I could not recommend this setting more highly. I made it a global so that you can play friends locally in vs mode and every player slot has their own hitbox trail color.
    - **Missed L Cancel -** tried to make this work like the stock slippi overlay. just a classic.
    - **Run Turnaround -** this one blinks you red when you go into turnaround, I think it's just good to have the negative reinforcement similar to the missed L Cancel to remind yourself that you messed up that dashdance/whatever.
    - **Actionable Yellow>Green -** This was one of my favorite ideas, and I'm really glad I got it working. There's even slippi code in here to add it to netplay if you end up liking it. At the end of any time that you are inactionable the last two frames will be yellow, and then the first two frames you are actionable you turn green. Kind of like a drag race go signal: it's time to MOVE. And it's just a blink so you don't have to deal with the constant green overlay when your character is standing around, I feel like that didn't really help me much. This one helps you get an idea of when you need to start pushing your stick again after wavedashing and L canceling. It doesn't do hitstun though, I feel like that's a slightly different thing that shouldn't be bundled together with this, but I'll probably do that eventually too.
    - **Invincibility Overlay -** I think the biggest problem smash players have is that they can't count to 3. Internalizing how long invincibility lasts when your opponent comes off the angel platform is crucial, knowing how long before they're damageable from roll, from spotdodge, from the ledge, it's important stuff. I'm also trying to learn how yoshi's power armor works so that's baked in here, and it also covers some moves like marth+roy's counter, and ofc it covers actual invincible moves like the startup of Bowser UpB.
    -  **Infinite Shields -** I suck at shield drops and I'm working on them now. Recently a friend was trying to help me out showing me in vs mode, but I'm so slow that it was driving me crazy trying to wait for my shield to regenerate so we could keep practicing. So this fixes that.
    -  **Randall Overlay -** I created this to mark where Randall is when he's behind the stage on yoshi's. I'm quite bad at knowing the timing so I want to start paying attention to it more. This setting will not work in training mode if you leave the Hazards Off setting in character select (and the timer counts up instead of down in training lab anyway so it's less useful), but it works great in vs mode so that's why I made it a global.

So hopefully that gives you a better idea of how I practice! 
Right now my favorite events are infinite mode Eggsercise and the Ledgedash training events. But I've done a ton of ledgedashing without much refresh practice, so I put a ton of effort into making the Under Fire event more fun so I could practice ledge refreshes instead of my fake double jump ones and help my edge play.

## BUGS/FEATURES TO INVESTIGATE WORKING IN T7
- set up the Training Lab settings to save now that we have the memory available to use. XX Actually, there are about 100 settings in Training Lab, and so that would take 100 bytes, but we only have 52 remaining. We can either start cannibalizing the event high scores or leave as is
- Similar Actionable X>Green for hitstun ending?
- change name of low laser puff practice to frozen puff (low laser?) practice
- Under Fire split high score between left and right side?? Idt you can have 2 ko high scores though. 
- Under Fire make a setting where you can see the hand grab through the lava if you don't want to go by timing forever. But by the time the lava comes up if you want to succeed you do just need perfect rhythm. Just not sure if taking away the visual cue is good or not.
- Edgeguard Training Beta Changes:
    - Samus often misses tether, and doesn't "pull up" after hitting tether, just hangs there.
    - Luigi feels like they reset a little early

## Tyro branch changelog

This section summarizes implemented Tyro-specific changes, newest version first. Keep one concise bullet per major feature within each version, updating that bullet for related adjustments. Changes to an earlier feature may receive a new bullet in a later version. Proposed changes stay in the relevant [OSD investigation](notes/OSD-GLOBAL-SETTINGS-INVESTIGATION.md) or [Training Lab menu investigation](notes/TRAINING-LAB-MENU-INVESTIGATION.md) until implemented.

### T6 (V1.4.1T6) - 2026-10-19

- **Program changes**
    - New reusable character selector module ([src/fighter_swap.h](src/fighter_swap.h)) and menu support for custom rows (extra buttons, colored values, stock icons), so other events can get the same Player/CPU Character rows later. Details are in [the mid-event swap notes](notes/MID-EVENT-CHARACTER-STAGE-SWAP.md#implemented-in-t6-player-and-cpu-character).
    - Global Settings saves version 19 of its extension record. Fixed Grid positions use 14 of the 52 spare bytes, the saved Player/CPU costumes use 2 more and the Edgeguard settings use 16, leaving 20. Older saves update automatically with every OSD on Auto, default costumes and default Edgeguard settings.
    - Reading a saved setting no longer re-checks the fields that can't hold a bad value (like the recovery toggles), so every settings read does less work.
    - The character selector can now limit one slot to a list of characters, for events that only support some CPUs.
    - New CPU recoveries for Yoshi, Peach, Luigi, Dr. Mario, the Ice Climbers, Jigglypuff and Samus ([src/recovery.c](src/recovery.c)). They press buttons the way a person does: fresh presses, and mashing at about 10 presses a second (15 with Fast Mashing). The research, the numbers behind each one and a Dolphin test list are in [the Edgeguard character notes](notes/EDGEGUARD-NEW-CHARACTERS.md); [scripts/fighter_move_data.py](scripts/fighter_move_data.py) reads the move heights and timings from the ISO.
    - New [save data memory map](notes/SAVE-DATA-MEMORY-MAP.md): tables showing every saved byte Tyro uses, what owns it, and what's still free.
    - Global Settings pages now go forward with R and back with L. Previously both went forward.
- **Globals and all events**
    - **Change Stage** (Eggs-ercise, Low Laser Puff Practice, Training Lab) now reloads every file for the new match, like the character change does. The load takes a little longer, but changing the stage after changing characters can't crash.
    - **Fastfall Timing OSD** now has three rows: `Fastfall`, then `input #f` (white), the jump frame where your down input started, then `Nf (Mf)`. N is which frame of the fastfall window you acted on, where 1 is the first frame your y velocity is below 0, so a frame-1 aerial that changes your gravity is already counted. M is the jump frame the fastfall took effect on. `(Mf)` also shows in the Practice Panel layout and stays when a hitlag prefix is added. Fastfalls that don't come from a jump, like walking off a ledge or Kirby/Puff multi-jumps, keep the old two-row display.
    - New **Fixed Grid OSD Positions** page in Global Settings (the third page: R twice or L once from Global OSDs). It lists the same OSDs as Global OSDs and lets you put each one in any cell of a 6-row × 5-column grid, shown as `R1 C1` (row 1, column 1).
        - A steps to the next free cell and B to the previous one, through `Auto`. Two OSDs can never share a cell.
        - A placed OSD always uses its cell, even when other OSDs are turned off, and cells you leave empty stay blank, so you can group OSDs with gaps between them.
        - **JC Shine (Fox/Falco)** has its own row. Its cell stays empty when you're not playing Fox or Falco.
        - **Reset All to Auto** puts every OSD back on Auto. Press A twice; B or moving off the row cancels.
    - **Fixed Grid** now fits six rows: the grid moved up a little and the rows are slightly closer. OSDs on Auto still fill only the top two rows, ten per page, so if you don't place anything the grid looks the same as before, just higher. With cells placed, Auto OSDs start after your last placed OSD in the top two rows, and CPU OSDs start on their own page, labeled **CPU OSDs** instead of tagging every cell. The page label moved between rows 2 and 3.
    - The character select now remembers the Player and CPU **costume colors** across restarts, along with the characters it already remembered. A saved color comes back only with the character it was picked for; restricted events that force a different character still use its default color.
    - Empty `--` Fixed Grid cells use a smaller placeholder that's freed once the OSD shows a result, so the bigger grid doesn't use more of Melee's text memory.
- **Event-specific changes**
    - **Training Lab**
        - New **Player Character** and **CPU Character** rows at the top of the **Session** tab can let you hot swap characters in the middle of a training lab session. Left/Right picks a character in its default color, in my tier list order (Fox, Marth, Jigglypuff, Falco, Peach, Sheik, ...). X/Y cycles that character's costume colors. The name is drawn in the costume's color, with its stock icon next to it. Press A to restart the match with both picks without going back through the character select.
        - Your Lab settings carry over like they do for Change Stage, except the CPU's recovery options (kept only if the CPU stays the same) and Character RNG (kept only if both characters stay the same). Recordings and saved positions are cleared.
        - The two characters can't end up in the same costume: picking the other slot's character skips its color.
        - Picking Zelda as the CPU now gives you Zelda. (The character select still turns a Zelda CPU into Sheik.)
        - A recovering CPU playing Yoshi, Peach, Luigi, Dr. Mario, the Ice Climbers, Jigglypuff or Samus now uses the new Edgeguard recoveries instead of the game's own CPU, and the **Recovery Options** menu works for them.
    - **Edgeguard Training**
        - The event name is now cyan on the event select screen.
        - Settings are saved: Player Percent, Hit Strength, all six Custom Hit Strength values, and the Recovery Options of each CPU (every CPU keeps its own).
        - New **Change Characters** menu with the same Player Character and CPU Character rows as the Training Lab. The player can be anyone; the CPU list only has the characters the event supports.
        - **BETA, very unrefined and mostly untested -** Seven new CPUs: **Yoshi** (double jump high or low, air dodge to the ledge or stage, egg toss), **Peach** (float to the stage or low by the ledge, Parasol, Bomber, float aerials), **Luigi** (mashed Cyclone, Green Missile with the game's own misfires, up B), **Dr. Mario** (mashed Dr. Tornado, Cape stall, up B), the **Ice Climbers** (Belay when Nana is close, mashed Squall Hammer otherwise), **Jigglypuff** (jumps to stay high or low, Pound, aerials at you) and **Samus** (bomb jumps, grapple, Screw Attack). Each has its own Recovery Options. These haven't been tested in Dolphin yet; see [the notes](notes/EDGEGUARD-NEW-CHARACTERS.md#testing-in-dolphin).
        - Ice Climbers: resets no longer switch off Nana's AI, and she now follows Popo instead of you after a reset.

For T6 I was working with Claude Opus 5.5 (mostly on Medium, with Extra High use for the character swapping and OSD layout, and Max for attempting to add new characters to Edgeguarding)

### T5 (V1.4.1T5) - 2026-10-18

- **Program changes**
    - Moved the investigation, audit and guide notes from the repo root into [notes/](notes/), and added [a deferred design for custom OSD ordering](notes/ORDERING-GLOBAL-OSDS.md).
    - Added [an investigation of swapping characters or stages mid-event](notes/MID-EVENT-CHARACTER-STAGE-SWAP.md): a quick restart straight back into the event works for any character or stage; an instant in-match swap is only practical for one pair (e.g. Fox↔Falco) using Melee's Zelda/Sheik transform.
- **Globals and all events**
    - Fixed Grid and Practice Panel OSD cells that belong to a CPU now have a small grey **CPU** tag, so a CPU's block of cells (which never updates if the CPU doesn't wavedash, shine, etc.) can't be mistaken for frozen duplicates of your own OSDs. Turning on **Override CPU OSDs Off** still hides them entirely.
    - Every match now starts on OSD page 1. Previously the page chosen in the global OSD menu (or left over from the last match) carried into the next match, which could hide your Wavedash/character-specific cells on a page you weren't looking at.
    - Split the Global Settings menu into two pages, **Global OSDs** and **Global Overlays & Gameplay**, switched with L/R. The OSD page is regrouped (movement/tech, Act Oo*, shield/escape, punish, ledge/items) with the two OVERRIDE rows in the bottom-right corner below a blank line.
    - New Global Settings controls: **A** steps a setting forward, **B** steps it backward, **Start** saves and exits, X/Y still change the OSD display. (Previously Start stepped backward and B exited, and R didn't actually turn the page.)
    - Renamed the **Act OoLag** and **Act OoAirborne** menu entries to **Act OoWait** and **Act OoJump**, matching how they show up in game.
    - Added a **Randall Overlay** (Global Overlays & Gameplay page, and Training Lab > Stage Options on Yoshi's Story). It draws Randall's platform and a cloud outline on top of everything, so you can always see where he is on his track, even while he's behind the stage. The platform line comes from Randall's live collision, so it matches exactly where you can land. It will not work with stage hazards off.
    - Randall Overlay is also available as a standalone **Slippi Gecko code** for netplay/vanilla Melee: see [slippi-gecko-codes/randall-overlay](slippi-gecko-codes/randall-overlay/README.md). It's visual only, so it can't desync.
    - Fixed Global Settings checkboxes keeping the other page's ON/OFF state after an L/R page switch (for example Randall Overlay showing ON next to "Off" until first pressed). Every checkbox is now set from its own setting whenever the page is drawn.
    - The Global Overlays & Gameplay page is grouped into **character overlays**, **character gameplay** (Infinite Shields) and **stage overlays** (Randall Overlay), separated by blank rows.
    - The yellow/green actionable overlay now also covers throws: after your forward/back/up/down throw, you get the yellow warning and green flash at the exact frame the throw animation ends and you can move again (weight-dependent throw speeds included). Character-special throws like DK's cargo carry and Kirby's swallow aren't covered. The Actionable Slippi Gecko code got the same change.
    - New **Act OoAttack** OSD (Global OSDs page next to Act OoWait, and Training Lab > Action Timing OSDs). After a grounded attack (jab, tilts, smashes, dash attack) or a throw, it shows the attack and how many frames after becoming actionable you started your next action: **1f** cyan, **2f** green, **3f** yellow, **4f+** red, with the attack's hitlag in front when it hit (e.g. `4hl->2f`). It counts acting out of IASA and out of the Wait/crouch after the animation ends, including a buffered frame of walk/turn/crouch like Act OoWait does. Every throw is covered, including ones like Fox's up-throw that end in Wait; throws were removed from Act OoWait so they only show up here. Starts Off; its color is saved in the extension save block.
    - OSD colors went from 7 to 15: added **Orange, Pink, Purple, Teal, Lime, Gold, Sky Blue and Lavender** after Magenta, in the Global OSDs page and Training Lab. Existing color choices are unchanged. If you open the save in an older Tyro build, a new color shows as the closest original one (e.g. Teal as Cyan).
    - The angle diagram in the Wavedash OSD and the Wavedash/Ledgedash HUDs is now the bottom half of the octagon, drawn bigger, since a wavedash always points down and the top half never got used.
- **Event-specific changes**
    - **Eggs-ercise**
        - Free Practice's count-up timer now always starts at 0:00, whether you start the event in Free Practice or switch to it from the menu. Switching mid-challenge restarts the match, which also fixes the timer getting stuck on a big **5** if you switched during the final countdown.
        - New **Change Stage** menu (under Retry): restart on any of the six tournament-legal stages without going back through the character and stage select. A timed run starts over; Free Practice stays on with its saved egg settings.
    - **Under Fire - Ledgestall Training**
        - Renamed the **Under Fire** event to **Under Fire - Ledgestall Training** on the event select screen and in its pause menu, so it's clear the event is about refreshing ledge invincibility.
    - **Low Laser Puff Practice** ([details](docs/LOW-LASER-PUFF.md))
        - Every character can play it now, not just Fox and Falco. For characters other than Fox/Falco, the timing readout is named **Neutral-B Start** and times your first aerial neutral-B.
        - It opens the stage select instead of always using FD. Positions are worked out from each stage's own floor and platforms, and they're built for the six legal stages (FD, Battlefield, Dream Land, Yoshi's Story, Fountain of Dreams, Pokémon Stadium).
        - **Puff Position** adds **On Left/Top/Right Plat** and **Under Left/Right Plat**. Platforms a stage doesn't have show as (N/A) and get skipped. Far/Center Left/Right scale to the stage's width (FD keeps its exact old spots), and on Fountain, Puff rides the moving platform.
        - New **Puff Height** setting: **Grounded** (default), **Short Hop**, **Full Hop**, **Short Hop + Double Jump**, **Full Hop + Double Jump**. It lifts Puff above whichever position you picked, using heights worked out from Puff's own jump values.
        - Puff Position and Puff Height are both saved to the memory card.
        - Saved the on/under-platform coordinates for each legal stage as a reference in [notes/LEGAL-STAGE-PLATFORMS.md](notes/LEGAL-STAGE-PLATFORMS.md), with a script to regenerate them from the ISO.
        - New **Change Stage** menu: restart on any of the six legal stages without going back through the character and stage select. Puff Position, Puff Height and the OSD toggles carry over (a platform position the new stage doesn't have uses Center for that match); Puff's percent resets.
    - **Training Lab**
        - New **Change Stage** row in the **Stage/RNG** tab: restart on any of the six legal stages without going back through the character and stage select. Your Lab settings come with you: CPU behavior, DI/SDI, tech and getup chances, counters (including Advanced Counter), shields, percents and locks, camera/HUD/model/collision displays, game speed, Info Display, Hitbox Trails, Custom TDI, Custom OSDs, Action Log, Character RNG and recovery options. Recordings and saved positions are cleared because they belong to the old stage, and the old stage's own options (Stadium transformation, Fountain platform heights) don't carry.
        - New **Reset to Default Settings** in the **Session** and **Exit** tabs: puts every unsaved Lab setting back to how a fresh Lab starts, without leaving the match. Recordings, saved positions and saved preferences (Controls, overlays, OSD colors, Input Display) are kept.

For T5 I was working with Claude Opus 5.5 (mostly on Medium, with Extra High use for some research on character and stage swapping)

### T4 (V1.4.1T4) - 2026-10-15

- **Program changes**
    - Change the game/save ID from `TYRE01` to `TMTE01`, for Training Mode Tyro Edition :3
    - Made use of some unutilized event-storage available on the memory card slot to expand preference storage for events, without cannibalizing any pre-existing memory or event-score slots.
    - Highlighted events with Tyro-specific changes in cyan on the event select screen. The dark cyan ones I haven't put quite as much effort into making improvements.
- **Globals and all events**
    - Added **L + R + A + Start** shortcut to exit any event during gameplay or from menus, with normal cleanup and score saving; analog triggers are supported.
    - Added a global **A-Smash Release** OSD so that you can get timing feedback for how quickly you're able to let go of A when doing a smash attack.
    - Added an octagonal stick-direction diagram beside the angle in the global Wavedash OSD and Wavedash/Ledgedash HUDs. The display makes it much easier to understand what the degree means at a glance.
    - Set up logic so that in-event OSD overrides and overlays can disable the global settings you have so you don't have to exit and go back to the global menu to turn helpful/conflicting osds/overlays on/off. 
    - Made every event that I worked on (except Training Lab) remember settings that you use via the memory card.
    - Fixed the Powershield OSD, which wasn't accurately reading what frame you shielded on and wouldn't tell you if you got a successful powershield.
- **Event-specific changes**
    - **Training Lab** ([details](docs/STADIUM-TRANSFORMATIONS.md))
        - Got the Pokémon Stadium transformation selection working without having to turn Hazards On. Selected stages will go through the transitions to get to them and then stay frozen until you change settings.
    - **Attack on Shield / Reversal** ([details](docs/SHIELD-TRAINING.md))
        - Created an updated menu, adding Techchase distance presets.
        - Attack on Shield: Add a 1–5 shield-hit reaction count, Infinite Shields override, Shield DI In/Out, all five short-hop aerials and Spotdodge Shine.
        - Reversal: Remember the selected attack; add Grab, Run Up JC Grab that tracks the player, and Spotdodge Shine.
    - **Ledgedash** ([mechanics notes](docs/LEDGEDASH-MECHANICS.md))
        - Rename GALINT to **Any GALINT**; added **4 / 7 / 10 GALINT** minimums as alternate success criteria in case you want to be more strict on yourself.
        - Improved the HELP descriptions, detailing input, hand-circle timing and GALINT.
        - Add local Hitbox Trails and replaced my invincibility highlighting with **Highlight Dropped Invincibility** overlay, a red pulse that shows you when you take too long and become tangible again.
    - **Techchase**
        - Creates a new list of nine saved chance presets so you can easily swap around without having to fine tune every single percentage every time.
        - Added a new mode where you can have the CPU drop onto a side or top platform instead of on the ground with you, so you can practice platform techchasing too. Gave a drop height setting too so hopefully any character can take advantage of it.
        - Added a Spotdodge Punish setting that makes the CPU do an instant spotdodge after their tech option so you can practice punishing the spot dodge instead of the actual tech.
        - Add a local CPU Invincibility Overlay color to make it easier to see when they are invincible; overrides the global CPU invincibility overlay.
        - Saves preexisting settings like OSD, reset, invisibility and camera-mode choices, but manual percentages and camera coordinates remain session-only.
    - **Ledgechase** ([mechanics notes](docs/LEDGEDASH-MECHANICS.md))
        - An all new event that lets you choose a cpu opponent to try to fight their options from ledge.
        - Randomizes the CPU between regular get up from ledge, roll up from ledge, ledge jump, ledge attack, and even included experimental ledge hop and LedgeDash options.
        - Has a CPU 100% option so that you can see the slower animations, too
        - Tried to make flexible options for what the CPU does after their ledge option so they'll do hopefully anything you want to do. Doing attacks from ledge and trying to do falco ledgehop double laser are experimental and not finely tuned, especially not across the entire roster. There are separate settings for aerial and grounded actions, so you can have something like a falco mixing you up between ledge hop dair and ledgedash shine.
        - Added a local CPU Invincibility Overlay color so you can see what frame to punish a little easier; overrides the global CPU invincibility overlay.
    - **Powershield** ([details](docs/POWERSHIELD-TRAINING.md))
        - Added Yellow>Green overlay that shows the first frame you are actionable after laser hitstun/shieldstun/powershield.
        - Added OSD to see exactly what frame you Act out of Laser (ignoring shielding late or walking out of laser), as well as the newly corrected Powershield OSD. Also works for shieldstun so you can see how quickly you're wavedashing out of blocking the laser too.
    - **Side-B Sweetspot**
        - Semi-bugfix qol change - Reset the event after landing on stage or platforms following Side-B, instead of you needing to realize the event wasn't going to reset and jump off stage to kill yourself.
        - Created a camera zoom setting so that you can see yourself off the sides of the stage more easily on small stages like Yoshi's, instead of having to gauge where you were through the spyglass.
    - **Low Laser Puff Practice** ([details](docs/LOW-LASER-PUFF.md))
        - An all new Character-specific Tech event for Fox/Falco. Just plants a fixed startup-crouch Puff to practice shooting the lowest of low lasers without the fake-gratification of hitting the puff-crouch-idle (which is a little taller and has an easier timing)
        - Includes neutral laser/fastfall OSDs so you can see what timing you're hitting for the lasers
        - Might end up adding more characters/flavors to this event, but with these OSDs I think it sets up the best kind of groundwork to practice any laser heights.
    - **Under Fire**
        - low key totally overhauled this event to make it actually useful and worth playing and actually do something unique to practice over just ledgedash event.
        - Event now restarts automatically after hits/deaths instead of spitting you back to the event menu.
        - Changed high score from being timer-based to being count-based for how many perfect ledge refreshes you can do, so the "startup" of the event doesn't feel like wasted time.
        - Retains Session Best score that displays on screen through resets and updates/saves your high score when you exit the event.
        - Gave the event a proper menu, with options to turn off local Hitbox Trails so they're not in the way of seeing your model, and a custom overlay to highlight Dropped Invincibility which will override the global character invincibility overlay to make your mistakes more clear.
        - Created a setting to hide move animations, so that when you're trying to refresh with Fox/Falco you can see your character model more clearly instead of the flames.
        - You can now switch from the left side ledge to the right side ledge! It wasn't possible to practice refreshing on both sides in this event before. Works like ledgedash event where you can either change the setting manually or use the Dpad left-right. Also works as a nice shortcut to reset the event faster.
        - Added two event-specific OSDs to show what frame you drop off ledge (telling you if you hit the frame-perfect you need), and show you how close you were to getting a perfect refresh.

For T4 I was working with ChatGPT Sol 6.1 (mostly on High, some Medium and Ultra use)

### T3 (V1.4.1T3) - 2026-10-09

- Remember the selected event and page across sessions, restoring the cursor and scroll position on boot.
- Add numbered Fox/Falco Side-B Sweetspot distance guides that include drift, with a saved Enable Line Guides toggle defaulting to On.
- Extend the Invincibility Overlay to Marth and Roy's active grounded and aerial counter windows.
- Rework the Training Lab menu with eight category tabs, clearer option rows, descriptions and submenu previews, and apply/cancel value pickers; retain parent selections, shortcuts and existing saves.

### T2 (V1.4.1T2) - 2026-10-07

- Add saved Fixed Grid and Practice Panel OSD layouts with reserved feature positions, retained results and manual paging; Fixed Grid shows ten OSDs per page, while Practice Panel includes three previous timing results.
- Add saved OSD title colors and previews in both editors, plus separate CPU and all-OSD overrides in reorganized Global Settings menus.
- Simplify timing OSDs with compact frame counts, smaller arrows, colored hitlag/turnaround context and best-frame-relative colors; extend Act OoWait to landing and direct recovery exits.
- Replace Fox/Falco Act OoShine predictions with confirmed Jump Out Of Shine results, turnaround context and failure detection; give Jump Out Of Shine and JC Shine separate layout slots and histories, with corrected hitlag handling.
- Clarify Wavedash feedback with explicit hop types and duration-based colors, and add explicit L-cancel Success/Missed feedback.
- Fix corrupted, duplicated and overlapping OSD text by correcting native color encoding, row replacement and text sizing, with regression coverage.
- Add global Very Fast and Instant hitbox trails with player/team colors, translucent history, subfighter/projectile support and reliable cleanup across resets, restores and scene changes.
- Add a player-colored Invincibility Overlay for ledge, respawn, dodges, moves and Yoshi's double-jump armor, with reliable protection detection and rendering across respawns.
- Add Actionable Yellow>Green recovery cues and red Missed L Cancel / Run Turnaround diagnostics, with corrected detection and cue priority.
- Add full-health Infinite Shields for main fighters and subfighters.
- Expand Ledgedash with Attack/Dash goals, ledge selection, Protection Highlight, four success criteria and criterion-aware statistics; add configurable ground/platform egg targets and preserve reset behavior and randomized targets through regrabs.
- Save Ledgedash and Eggs-ercise event settings and add Reset Event Settings to both events; also correct Ledgedash's frame-advance button lookup.
- Remember human and CPU character choices across unrestricted events and saved sessions, using local fallbacks for events with required fighters.
- Consolidate saved settings into the existing 44-byte record, preserving scores, OSD choices and colors while saving every Lab overlay condition and migrating earlier Tyro settings.
- Fix memory-card crashes during settings saves and save creation by deferring saves to the initialized card service, guarding native save handling and correcting game-ID relocation.
- Centralize version metadata and give Tyro a separate disc/save identity, save caption, versioned release filenames, cross-platform patchers and matching banner/symbol-map names.
- Make partial builds rebuild modules after version/identity changes and stop ISO updates when a C module fails to compile.

### T1 (v1.4.1 T1) - 2026-09-25

- Add Eggs-ercise hitbox trails and Very Fast / Instant decay options in Eggs-ercise and Training Lab.
- Add Tyro Edition names, version labels and banners, with corrected in-game banner colors.
- Remove the restriction on AI-assisted contributions from the development guidelines.

## Changes From the Original
- New Training Lab Features:
    - Recording:
        - Reworked recording UI. Allows re-saveing existing recordings with different percents or positioning.
        - Savestates now require holding DPad right, preventing accidental savestates.
        - Set chances for slots during random playback.
        - Option to auto-restore state when the CPU performs a counter action.
        - Takeover HMN or CPU playback at any point.
        - Press DPad left/right when browsing savestates to quickly change pages.
        - Savestates can now always be saved. (May cause crashes if you save during special moves).
        - Option for the CPU to counter on hit or recording end during playback.
        - Play a recording slot as a counter action.
        - Resave Positions option will start the recording from a new position, without removing inputs.
        - Prune Positions option will start the recording from a new position, truncating inputs.
        - Re-Record mode will allow your to record over a previously recorded slot using playback takeover.
        - Start Paused option will only start the replay on your first input.
        - Add random additional percent when loading a state.
        - Slot Management menu - delete, copy, and edit slots.
    - CPU Options:
        - Random custom DI option. The CPU will pick a random option from your custom TDI.
        - CPU Shield angling options.
        - Added Wavedash counter actions.
        - Tech animations can be set invisible after they are distinguishable.
        - Added usmash OoS and all specials moves as counter actions.
        - All special moves can be used as counter actions.
        - Neutral jump action, set as the default.
        - SDI and ASDI options.
        - SDI is now set by number of inputs rather than by chance.
        - SDI and mashing are set to none by default.
        - Move CPU option.
        - Choose a port to control the CPU option.
        - CPU will use random TDI after custom TDI ends.
        - Option to reverse Custom TDI if on the other side of the player.
        - Tech lockout option. This prevents the CPU from teching in quick succession.
        - Tech trap option. This prevents the CPU from teching for a short window after being hit.
        - Added dash through and dash back counter actions.
        - Option to force shielded projectiles to always be powershielded.
    - Other Changes:
        - Alter set OSDs with the OSD menu in the lab.
        - Alter character RNG - choose misfire, nana throws, peach pulls and fsmash, and GnW hammer.
        - Added advanced counter actions - manually choose counter actions for each specific hit.
        - Added "Freeze CPU" option to freeze a CPU's hitboxes in place.
        - Set a chance to wait in miss tech.
        - Added shield health option.
        - Added new shortcut system, currently only supporting frame advance (Press Y then A in the menu).
        - Hitboxes are colored by ID and sorted by priority.
        - Game speed option.
        - Color overlays.
        - Lock percents.
        - Hazard toggle.
        - Hide stage model.
        - Item grab range display.
        - Show input and info displays for both HMN and CPU.
        - Mirror recordings.
        - The overlay, taunt, and input display options are saved to the memcard.
        - R can be used as a frame advance button.
        - Stage options to control stadium transformations and FOD platform heights.
        - Custom action state OSDs.
    - Import Menu Changes:
        - Fixed glitches when importing using a port other than port 1.
        - Fixed Sheik/Zelda transformations when loading.
        - Recordings are now filtered by the selected HMN character.
        - Cursor can now wrap.
        - Deleting replays too fast will no longer crash.
- Ledgedash Event Changes:
    - Colours have been updated to be colourblind friendly.
    - The airdodge angle is now consistent with other events.
    - Invincible grab hitboxes show like other hitboxes.
    - Added reset delay option.
    - Camera is now consistent across reset options.
    - HUD now updates and displays every frame.
    - Game speed option.
    - Swap sides on auto reset option.
    - Option to always maintain full ledge invincibility while on the ledge.
    - The GALINT frame is now correct if ledgedashing without refreshing.
    - Will no longer immediately reset on aerial.
    - Option to show current state as an overlay.
    - Sopo is used instead of both climbers.
- New Edgeguard Event:
    - Replaces Armada Shine event.
    - Learn the basics of edgeguarding Fox, Falco, Marth, Sheik, and Falcon!
    - Adjust the options the opponent uses to change the difficulty or practice specific situations.
    - Choose between preset values or manually adjust hit angle, knockback, and damage.
- OSD Changes:
    - Act OoWait OSD will trigger even with intermediate buffered actions such as a frame of walk.
    - Removed Max OSDs and Recommended OSDs options, replacing with a new OSD Position option.
    - Wavedash OSD now shows if it was a short hop or full hop.
    - Removed broken OSDs, rewriting the most important ones.
    - Added new glide toss, aerial out of double jump, special out of jump, and double jump out of jump OSDs.
    - Added new lockout timer OSDs.
    - Added new Fighter-Specific Tech OSDs with:
        - Spacies: Act OoShine and Shorten Frame.
        - Peach: Act OoFloat.
        - Yoshi: Egg Toss angle and strength.
- Bugfixes/Small Changes:
    - **Fixed cpu acting too late out of sakurai angle and other non-knockdown hits (such as fox drill).**
    - Updated to UCF 0.84 (Allows practicing with dashback out of crouch).
    - All trigger-based functionality can now be performed with analog-only triggers.
    - Nametags will now show up in C events.
    - Slow down advanced camera with R.
    - DIDraw will now show for sheik and after throws.
    - System inputs in info display will now work for ports other than port 1.
    - The lab now saves a minor savestate on boot.
    - Act OoHitstun now works after being hit by falco laser.
    - The powershield event has been rewritten and given a new laser height option.
    - Adjustable timing in Amsah tech event.
    - Jump actions no longer make the CPU self-destruct.
    - Various OSDs have been fixed.
    - Lightshield now works in recordings.
    - Added successful counters to the LCancel and Wavedash events.
    - Fix CPU DI on back throws and moves that send backwards.
    - Can now use lightshield L with DPad to adjust percents.
    - CPUs now DI DK cargo throw.
    - Samus homing missiles will target the CPU.
    - Nana will not drop shield when Popo's shield is hit.
    - Added the polling drift fix.
    - Deleting replays too fast will no longer crash.
    - Every character can be used in Amsah Tech training.
    - Removed the maximum distance in Reversal training.
    - Added getup attacks and dash attack to Reversal training.
    - Added option to move to the platform in Reversal training.
    - Infinite shields now applies to nana.
- Work in progress:
    - Reaction Tech Chase Event
    - Improving the savestate format
- Developer Features:
    - Simple and easily reproducible builds on Windows and Linux.
    - Simple to add new events - no need to touch ASM.
    - Fast recompilation on Linux using make.
    - Simplified and performant [tool](https://github.com/AlexanderHarrison/gc_fst) to extract and rebuild ISOs.
