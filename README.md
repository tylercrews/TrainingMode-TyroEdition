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

<img width="740" height="606" alt="tm-te settings fixed order rearranged" src="https://github.com/user-attachments/assets/ea6c4031-5125-4923-9773-09ea5a0e6329" />

So here's what I have turned on as an aspiring fox player:

<img width="747" height="587" alt="tm-te settings pg 1" src="https://github.com/user-attachments/assets/93f3f656-36ba-43ca-abdb-62522e2b907e" /> 

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
 
<img width="899" height="740" alt="tm-te settings pg 2" src="https://github.com/user-attachments/assets/3eed3447-bb99-4df0-a1a8-1296d8c8c5f1" />

- Page 2, Overlays:
    - **Hitbox Trails Very Fast -** I think this might be the most useful setting for a new player of anything. Being able to see where moves are hitting at all times gives you a much better idea of spacing, of placing where you're hitting, and visibility into how you're making mistakes. You also notice little quirks of the game better, like when you're running around on FD upsmashing eggs in eggsercise, seeing how much narrower the running upsmash hitbox is compared to standing hitbox, and where it actually hits on the ground is so important. I could not recommend this setting more highly. I made it a global so that you can play friends locally in vs mode and every player slot has their own hitbox trail color.
    - **Missed L Cancel -** tried to make this work like the stock slippi overlay. just a classic.
    - **Run Turnaround -** this one blinks you red when you go into turnaround, I think it's just good to have the negative reinforcement similar to the missed L Cancel to remind yourself that you messed up that dashdance/whatever.
    - **Actionable Yellow>Green -** This was one of my favorite ideas, and I'm really glad I got it working. There's even slippi code in here to add it to netplay if you end up liking it. At the end of any time that you are inactionable the last two frames will be yellow, and then the first two frames you are actionable you turn green. Kind of like a drag race go signal: it's time to MOVE. And it's just a blink so you don't have to deal with the constant green overlay when your character is standing around, I feel like that didn't really help me much. This one helps you get an idea of when you need to start pushing your stick again after wavedashing and L canceling. It doesn't do hitstun though, I feel like that's a slightly different thing that shouldn't be bundled together with this, but I'll probably do that eventually too.
    - **Invincibility Overlay -** I think the biggest problem smash players have is that they can't count to 3. Internalizing how long invincibility lasts when your opponent comes off the angel platform is crucial, knowing how long before they're damageable from roll, from spotdodge, from the ledge, it's important stuff. I'm also trying to learn how yoshi's power armor works so that's baked in here, and it also covers some moves like marth+roy's counter, and ofc it covers actual invincible moves like the startup of Bowser UpB.
    -  **Infinite Shields -** I suck at shield drops and I'm working on them now. Recently a friend was trying to help me out showing me in vs mode, but I'm so slow that it was driving me crazy trying to wait for my shield to regenerate so we could keep practicing. So this fixes that.
    - **Blast Zone Overlay -** Gives a nice clear line where your character is going to blow up. I think it could be useful if the camera ever tilts far enough in that direction for some reason. 
    -  **Randall Overlay -** I created this to mark where Randall is when he's behind the stage on yoshi's. I'm quite bad at knowing the timing so I want to start paying attention to it more. This setting will not work in training mode if you leave the Hazards Off setting in character select (and the timer counts up instead of down in training lab anyway so it's less useful), but it works great in vs mode so that's why I made it a global.
    -  **Don't Reset Timers -** When a savestate loads it resets the timer back to whatever time it was on when the savestate was made. That could be helpful if you're trying to time a sequence out, so I wanted it to still be accessible. But I prefer to see how long I have been playing so that I can keep track of how long I'm practicing things, and makes it easy to know when to switch sides so my practice is balanced.

<img width="745" height="611" alt="tm-te settings pg 3" src="https://github.com/user-attachments/assets/6f6c1bb3-7118-4c61-8ace-60afbc2a2920" />

- Page 3, OSD Positions
    - I like a little gap on row 2 in the middle so that event-specific OSDs can go there.
    - I like the wavedash underneath because I can check it easier when I'm scooting around.

So hopefully that gives you a better idea of how I practice! 
Right now my favorite events are infinite mode Eggsercise and the Ledgedash training events. But I've done a ton of ledgedashing without much refresh practice, so I put a ton of effort into making the Under Fire event more fun so I could practice ledge refreshes instead of my fake double jump ones and help my edge play.

## BUGS/FEATURES TO INVESTIGATE WORKING IN T10
- improve up-b overlay and sweetspot event with indicators on angled characters's arcs where the fixed angle spots are for boxx players
- new objects in eggsercise. practice hitting in other spots other than on the floor
- expand the edgeguarding event, i want at least doc and game and watch if I didn't already do doc. get some more in there like dk and all the mid tiers
- test pikachu's recovery in both the up b sweet spot event and the recovery event. It might be too much for me to try to build out as a non-pika player, but would be nice to expand
- set up the Training Lab settings to save now that we have the memory available to use. XX Actually, there are about 100 settings in Training Lab, and so that would take 100 bytes, but we only have 52 remaining. We can either start cannibalizing the event high scores or leave as is
- Similar Actionable X>Green for hitstun ending?
- change name of low laser puff practice to frozen puff (low laser?) practice
- Under Fire split high score between left and right side?? Idt you can have 2 ko high scores though. 
- Edgeguard Training Beta Changes:
    - Luigi feels like they reset a little early

## Tyro branch changelog

This section summarizes implemented Tyro-specific changes, newest version first. Keep one concise bullet per major feature within each version, updating that bullet for related adjustments. Changes to an earlier feature may receive a new bullet in a later version. Proposed changes stay in the relevant [OSD investigation](notes/OSD-GLOBAL-SETTINGS-INVESTIGATION.md) or [Training Lab menu investigation](notes/TRAINING-LAB-MENU-INVESTIGATION.md) until implemented.

### T9 (V1.4.1T9) - 2026-11-02

- **Program changes**
    - New [scripts/build_invincibility_gecko.py](scripts/build_invincibility_gecko.py) builds the Invincibility Overlay Gecko code, and [tests/test_invincibility_gecko.py](tests/test_invincibility_gecko.py) runs it in an emulator, on its own and together with the Actionable Yellow-Green code.
    - New [scripts/sweetspot_knockback_presets.py](scripts/sweetspot_knockback_presets.py) reads Fox's and Falco's falling speeds and the game's knockback rules from a vanilla ISO, and prints where each Side-B Sweetspot Knockback preset ends hitstun and whether you can fall into the side-B from there.
    - Added [research for an Up-B Sweetspot Practice event](notes/UPB-SWEETSPOT-RESEARCH.md): how the game decides a ledge grab, every character's up-B path, aim and ledge-grab frames from the ISO and the decompilation, an overlay design for each kind of up-B (including a live line for the angle you're holding), Edgeguard's camera, a Marth frozen in a tipper forward smash, and limiting the event to the six legal stages. Nothing is implemented yet.
    - New [scripts/upb_overlay_numbers.py](scripts/upb_overlay_numbers.py) reads each character's up-B and ledge-grab box from a vanilla ISO and writes the Up-B Overlay Gecko codes' shapes. [scripts/build_upb_overlay_gecko.py](scripts/build_upb_overlay_gecko.py) builds the codes, and [tests/test_upb_overlay_gecko.py](tests/test_upb_overlay_gecko.py) runs all 17 together in an emulator.
    - Events can now use the VS character select (any number of CPUs, team colors), for 2v1 Grab Punish Practice. Its **Rules** button does nothing in events, since there's no Rules screen there and pressing it would have started the event. The General Tech page has 18 events; the new one is unscored, so no score slots move.
    - The save record (`TYEX`) grows by 4 bytes, from 68 to 72 (version 20), to save the 2v1 Grab Punish and Up-B Sweetspot settings. Saves from older Tyro builds update on their own and keep every setting; the two events start at their defaults. 16 bytes of spare save space are left. As with every earlier `TYEX` version bump, an older Tyro build can't read the new record: it leaves it alone and uses defaults for everything stored there (like event settings, the extra OSD colors, Fixed Grid positions and costumes) until you're back on T9.
    - Working copies now use LF line endings everywhere through [.gitattributes](.gitattributes), so `git add` stops warning that LF will be replaced by CRLF. `.bat` files stay CRLF because cmd.exe needs it, [VS Code](.vscode/settings.json) creates new files as LF, and the Actionable and Invincibility Gecko build scripts always write LF. The game itself doesn't change.
    - **Fixed:** `build.sh` sometimes stopped with `Could not read file 'build/ledgedash_logic.o'`. The Ledgedash and Sweetspot events both compile that file to the same place, so building them at the same time could collide. They now build one after the other.
    - Low Laser's ground and platform lookups moved into [src/stage_points.h](src/stage_points.h), so Ledgegrab Speedrun can share them. Low Laser plays the same.
    - `GFX_Start` has a new `no_depth` option, so an event can draw over the stage without its own copy of the setup. Nothing else changes; Ledgegrab Speedrun's ledge markers are the first to use it.
- **Globals and all events**
    - **Invincibility Overlay** in teams matches now gives each teammate their own shade of the team color, so in doubles your invincible teammate no longer looks like the other team. Teammates are numbered in port order (a team has at most three, 3 v 1): red team is red, orange, pink; blue team is blue, lavender, cyan; green team is green, lime, forest green. CPUs on a team get its shades too. Free-for-all still uses port colors, with CPUs lavender. Lavender itself is now a little deeper and less white everywhere the overlay uses it, including CPUs and the CPU Invincibility Overlay's Lavender choice. The CPU Invincibility Overlay choice in Techchase and Ledgechase still comes first. The colors live in one function shared with the Gecko code, so the two always match.
        - New [Slippi Gecko code](slippi-gecko-codes/invincibility-overlay/README.md) with the same colors and the same idea of invincible as Tyro's overlay, to replace Aitch's Change Colour While Invulnerable code. It only changes how fighters are drawn, never game state, and runs alongside the other Tyro Gecko codes; with Actionable Yellow-Green, yellow and green win, like in Tyro. It passes emulator tests but hasn't been tested in Dolphin yet.
    - **Act Oo OSDs** (Act OoWait, Act OoAttack, Act OoShield, Act OoHitstun and Act OoJump) now show a blank `--` result when you don't act within 20 frames, so Fixed Grid and Practice Panel no longer leave an older attempt's frame up as if it were the latest. The 20 frames count from where each OSD starts counting: your first actionable frame after a landing (after normal landing lag), ledge getup or roll, tech or getup, or attack or throw; the end of shield stun or hitstun; or a jump, double jump, platform drop or fall (like dropping off the ledge). Hitlag doesn't count. Act OoWait and Act OoAttack keep their source row, like `L-cancel` or `Uthrow`. Act OoShield, Act OoHitstun and Act OoJump still show the frame of an action after 20 frames, as before. In the Practice Panel, `--` goes into the history like FAIL. Recent (pop-up) mode doesn't add an empty pop-up; it clears out that OSD's older pop-up instead. Jump Out Of Shine is unchanged and still shows FAIL after 15 frames.
    - **Fixed:** Zelda and Sheik turned permanently invisible after transforming while any overlay that tints fighters was on (Invincibility Overlay, Actionable Yellow>Green, Missed L Cancel, Run Turnaround and the event tints). The game swaps which form counts as the player's second fighter on every transform, and the overlays tracked fighters that way, so each form got the other's slot and lost its draw. They now track each form by its own fighter object, which never changes. The Gecko codes weren't affected.
- **Event-specific changes**
    - New **Ledgegrab Speedrun** event, last on the Minigames page: how few frames can you take to reach a ledge from a set starting spot? The D-pad controls, ledge markers and wider camera haven't been tested in Dolphin yet.
        - **Characters:** everyone, with no CPU. **Stages:** the six legal stages, from the stage select or **Change Stage** in the menu. Pick any other stage and the event restarts on Battlefield.
        - **Goal Ledge:** Left or Right, first in the menu. D-pad down switches it.
        - **Start Position:** At Ledge, At Far Ledge, Under Close Plat, Under Far Plat, Center Stage, Top Plat, Close Plat or Far Plat. D-pad left/right cycles through them, wrapping around. Close and far are relative to the goal ledge, so switching ledges mirrors the start. Platforms a stage doesn't have show as (N/A) and are skipped. Fountain's side platforms keep moving, so those starts vary a little from run to run.
        - **Facing:** Toward Ledge or Away From Ledge, meaning the goal ledge. D-pad up switches it, and doesn't taunt here.
        - Every D-pad press restarts right away, and a run you're still in the middle of doesn't count. The controls are in the menu's **Help**.
        - **Ledge markers:** a big red X with a black edge over the wrong ledge, and a large green exclamation point above the goal ledge, high enough to clear a fighter standing there. Both draw over the stage and fighters. The camera now keeps both ledges in frame, since a lone fighter's camera zooms in too far to show them from across the stage.
        - **The count:** a large frame counter, white while it runs. It starts on your first move, so your reaction to the restart doesn't count, and stops when you grab the goal ledge. Grabbing the other ledge doesn't stop it.
        - **Results** stay up for a second before the restart: cyan for a new or tied Session Best, green within 4 frames of it, yellow within 10, and red when further off.
        - **DNF:** still going after 30 seconds (1800 frames) is a red DNF, which also stays up for a second before the restart. Dying is a DNF too, but restarts right away.
        - **Session Best** and **Previous** sit under the counter, styled like Under Fire's. Each start and facing keeps its own, and both ledges share them because they're the same run mirrored. Previous changes at the restart, so while a result is up it still shows the run before.
        - Nothing is saved to the memory card, since times depend on your character and stage. Session times clear when you change stage or leave the event, and all of them together take 64 bytes of RAM. Your menu choices carry over a stage change.
        - The Minigames page has 6 events now. This one is unscored, so no score slots move.
    - New **2v1 Grab Punish Practice** event, last on the General Tech page. Your teammate CPU grabs the enemy CPU, and you punish the enemy (like with a JC up-smash) before they mash out, without hitting your teammate. The grab setup, the countdown and the percents work in Dolphin; the cleaned-up panel, the release countdown, the red overlay and the big countdown below haven't been tested there yet.
        - **Fixed:** in the first Dolphin test, everyone stood stuck in place (your inputs only changed what your fighter was doing), the enemy showed double their Enemy Percent, and Start didn't open the menu. Setting the enemy's percent also queued the same amount again as new damage, so the event took it for your hit and set the grab up over every other frame. Each of those resets rewound the event timer, and the menu only opens a few frames after it starts. The percent is now set like Ledgechase's, and trying the grab again only moves the two CPUs, so you're never put back mid-setup.
        - **Character select:** the VS select, always in Teams mode. Pick your character, then set two ports to CPU and pick the teammate and the enemy. Exactly one CPU is always on your team, and that's your teammate; the other CPU is the enemy and can't be on your team. Changing your color brings your teammate along, and changing your teammate's color brings you along. Putting the enemy on your color swaps them: it becomes your teammate and the old teammate becomes the enemy. Everyone wears their team's costume. Any other players sit out. Then pick any stage.
        - Every reset puts your teammate next to the enemy, and it grabs them with a real standing grab (Z), retried with a different spacing if it misses (only the two CPUs move for a retry; your teammate goes back to 0%). You can't be grabbed until it connects.
        - New **HUD panel**, three rows:
            - **Grab Time Left** counts down the frames until the enemy breaks free at the current mashing, then keeps going as **Release** and the frames until they can act: 30 after the grab ends, 20 for Donkey Kong, which nothing can cut short. Then it shows the result: **Grab Hit** or **Release Hit** with the frames that were left when you hit them, **Escaped** (no hit), or **Let Go** (your hit on your teammate freed them, and you never hit them).
            - **Enemy Hits** and **Ally Hits**: the attempts you hit each one, as hits/attempts and the percentage. An attack that hits both counts for both. An attack that hits your teammate first still counts as a Grab Hit if it lands on the enemy within 12 frames.
        - The same frames show **big under the match timer**: white for the grab, yellow for the release. They stay up after a hit, so you can see what you had left.
        - The enemy pulses **red**, the same red as Missed L Cancel and Run Turnaround, from the first frame they can act after getting away without a hit, until the next reset. It shows whether or not those global overlays are on, and over any other overlay.
        - New **Count Release Hits** setting, under Teammate Facing (Off by default, saved). Off, only a hit in the grab is an Enemy Hit; a hit during the release shows as a Release Hit but isn't counted. On counts those too. Changing it resets your totals. It uses a spare bit of the save record, so the save format doesn't change.
        - After the result, the event keeps going until the enemy has been out of hitstun for 45 frames (at least 1 second, at most 8, or 1 second after a KO), so you can keep punishing with an up-tilt or whatever follows. D-pad left/right resets without counting the attempt.
        - **Grab Location** and **Your Location**: Random (the default), Side Plat Left, Side Plat Right, Center Stage, Under Left Plat, Under Right Plat, Left Corner or Right Corner. Without that platform, like on Final Destination, the ground where it usually is gets used. If your spot is the grab's spot, or within 22 of the pair on the same surface, you start beside them instead.
        - **Enemy Percent**: 0%, 20%, 40%, 60% (the default), 80% or 100%.
        - **CPU Mashing**: No Mash, Light Mash (the default; a button every 6 frames), Heavy Mash (a button and a stick direction, each every 4 frames) or Inhuman Mash (a new button and stick direction every frame).
        - **Teammate Facing**: Random (the default), Toward You (the enemy is between you and your teammate) or Away From You (your teammate is in front).
        - **Mashing Info** in the menu, and the top of [src/grabpunish.c](src/grabpunish.c), explain how mashing changes the hold. The game holds a grab for 30 + 1.6 × percent frames, plus 8 per handicap step below 9 (handicap off is 9) and 15 for each place the grabbed player is above fourth. Every frame takes 1 off, and every fresh A/B/X/Y press and every new stick direction takes 6 more, so mashing can take 13 a frame at most. This event holds every grab as if the match were tied with handicap off, 75 + 1.6 × percent frames, so your KOs don't shorten later grabs. At 60% that's 171 frames with no mash, 86 light, 43 heavy and 14 inhuman; at 100%, 235, 118, 59 and 19.
        - All six settings (Grab Location, Your Location, Enemy Percent, CPU Mashing, Teammate Facing and Count Release Hits) are saved to the memory card as you change them.
    - New **Up-B Sweetspot** event, right under Side-B Sweetspot on the Character-specific Tech page. You start on an angel platform off a ledge and recover to it with your up-B, while Marth waits on the stage to punish an overshoot. It's been tried once in Dolphin; the Marth change and the fix below came after and haven't been. The research behind it is in [the up-B sweetspot notes](notes/UPB-SWEETSPOT-RESEARCH.md).
        - **Characters:** everyone with a recovery up-B, so not Yoshi or Jigglypuff. **Stages:** the six legal stages. Pick any other stage and the event restarts on Battlefield.
        - **Angel platform:** 40 out from the ledge and 15 above it, close enough for every character to recover. Leave it any way the game allows: fall, jump, or up-B straight off it. The platform's respawn invincibility ends as soon as you leave, so Marth can hit you. With the Ice Climbers, Nana gets her own platform beside Popo.
        - **Marth:** when your up-B starts, he forward smashes and freezes on its first active frame, so the whole forward smash lingers like Side-B's down-tilt, with the tipper (20%) centered right over the ledge. The first swing measures where the tipper is and moves Marth to line it up; later resets start him in the right spot. **Marth: Off** leaves him standing.
            - Changed after the first Dolphin test: the whole forward smash now stays out instead of only the tipper, and the tipper sits right over the ledge instead of 12 inland, which was too far back.
        - **Resets:** right away when you die, 30 frames after you're hit or land on the stage, and 60 frames after you grab the ledge. **D-pad left/right** resets on the left or right ledge without counting the attempt. The automatic resets stay on the ledge you were on.
        - **HUD panel** like Side-B's:
            - **Last Attempt:** Sweetspot (with the frames from your up-B to the ledge), Got Hit, Fell, Landed, or No Up-B (you reached the ledge or stage without one).
            - **Success Rate:** successes / attempts and the percentage.
            - **Held / Need**, for Fox, Falco, Sheik, Zelda, Mewtwo, Pikachu and Pichu: the angle you're holding and the angle to the ledge, in degrees above level toward the stage.
        - **Enable Line Guides** (on by default) draws a line out from each ledge and a white X where your position grabs the ledge: the middle of your own ledge-grab box. Then, per character:
            - **Aimed up-Bs** (Fox, Falco, Sheik, Zelda, Mewtwo, Pikachu, Pichu): a blue ring around the X at the up-B's travel distance. Up-B on it and aim at the X.
                - The teleports and Quick Attack go farther the more you tilt the stick, so their dim inner ring is the least tilt.
                - While the up-B charges, a white line runs from where the travel will start to the X. A second line shows where the angle you're holding would send you: green if it reaches the ledge, yellow if close, red if not. Both freeze when the up-B locks in.
            - **Fixed paths** (Mario, Dr. Mario, Luigi, Marth, Roy, Captain Falcon, Ganondorf, Peach, Kirby, Samus): a blue dot where to up-B from for the fastest grab, at the top of the up-B, and a line up from it to where it still grabs, more slowly.
                - Mario, Dr. Mario, Marth and Roy also get a dim dot and line for the stick held toward the stage.
                - A yellow X marks where your up-B started.
            - Donkey Kong, Bowser, Link, Young Link, Ness, Game & Watch and the Ice Climbers only get the ledge X for now.
            - **Fixed:** on the right ledge, the X measured your ledge-grab box from the back of your ECB instead of the front. It now uses the side facing the ledge, as the game does. That only moves the X a little, and only for the widest characters, like Ganondorf and Bowser.
            - New [Slippi Gecko codes](slippi-gecko-codes/up-b-overlay/README.md), one for each of the 17 characters with a guide, that draw the same blue guides and white X at both ledges on the six legal stages in any match, while that character is in the match. All 17 can be on at once. They leave out the live parts: the angle lines, the Held / Need row and the yellow X. They pass emulator tests but haven't been tested in Dolphin yet.
        - **Camera:** Normal or Zoomed Out, like Edgeguard's; Zoomed Out keeps the bottom blast zone in view. **Change Stage** restarts on another legal stage.
        - Enable Line Guides, Marth and Camera are saved to the memory card as you change them. The success rate lasts for the session and carries over through Change Stage.
        - The event is unscored, so the Character-specific Tech events after it keep their score slots.
    - **Side-B Sweetspot**
        - New **HUD panel** that counts your attempts, like the Ledgedash event's. **Last Attempt** says what happened: Sweetspot, Got Hit (Marth's down-tilt), Fell, Landed (on the stage after side-B), Up-B or Air Dodge, or with Sweetspot + Ledgedash, Ledgedash with its GALINT, No GALINT, Ledge Option, Regrabbed or Too Slow (3 seconds after letting go). **Success Rate** shows successes / attempts and the percentage. **Criterion** and **Knockback** show what this attempt is using. D-pad resets don't count as attempts, and the counts last for the session.
        - New **Success Criteria** setting (saved), second in the menu: **Sweetspot** (the default, as before: grab the ledge with side-B) or **Sweetspot + Ledgedash** (after the sweetspot, ledgedash onto the stage with ledge intangibility left). The ledgedash is judged like the Ledgedash event's Any GALINT, with the same code. Changing it resets the success rate.
        - New **Knockback** setting (saved), first in the menu. **Random** (the default) is the original mix, which always ends hitstun 6-36 below the ledge, so you have to double jump. The six presets are named for where you can act again when hitstun ends, if you don't DI or hold the stick:
            - **Above, Close** and **Above, Far** end 17-32 above the ledge, high enough to drift in and fall into the side-B without a double jump.
            - **Level, Close** and **Level, Far** end at about ledge height (5 below to 8 above), so you side-B right away or double jump.
            - **Below, Close** and **Below, Far** end 24-31 below, so you need the double jump.
            - **Close** is 39-50 out from the ledge. **Far** is 68-85 out, around the end of a full side-B.
            - **Random Preset** picks one of the six each reset.
            - Each preset varies a little every attempt (±2 knockback, ±1.5°). The numbers come from [scripts/sweetspot_knockback_presets.py](scripts/sweetspot_knockback_presets.py) and are the same for Fox and Falco, except that Falco ends up to 7 lower.
        - Each attempt now starts 6 above the ledge instead of at a height of 6, so every preset ends in the same place on every stage. That only moves the start on Yoshi's Story (3.5 lower) and Fountain of Dreams (0.6 higher).
        - Both settings use 4 spare bits of the save record (`TYEX` word 20 bits 26-29), so the save format doesn't change.
    - **Edgeguard Training**
        - Fox's and Falco's **Recovery Options** replace the Illusion / Phantasm row with six side-B rows, so you can practice against one way of side-B recovering at a time. Each attempt the CPU picks one checked side-B row it can still reach, or its up-B as one more choice when a Firefox / Firebird row is checked. It's been tried in Dolphin; the To Main Stage rework and the Knockback settings below came after and haven't been. The details and a test list are in [the spacie side-B notes](notes/EDGEGUARD-SPACIE-SIDE-B.md).
            - **Instant:** side-B toward the stage the moment hitstun ends, without jumping.
            - **Ledge Sweetspot:** jump up and side-B into the ledge, grabbing it during the dash, after one, two or three of its steps.
            - **To Ledge:** side-B short of the ledge, so the end of the move drifts into the ledge over several frames, lingering at the edge. When the CPU is too close for a whole side-B, it shortens at once instead.
            - **To Main Stage:** side-B right above the ledge onto the main stage, with the dash 1 to 5 above it, through you when you're standing near the ledge and it can get there. (This was To Stage. Knockback left over from the hit kept lifting the CPU through the startup, so the dash could run anywhere from 3 to 26 above the ledge, up at side platform height. It now goes by the dash's height, not the height it pressed B at.)
            - **To Side Plat:** jump high and side-B onto the side platform, landing near its middle. Without one, like on Final Destination, it goes to the stage.
            - **Shortest Shorten:** jump up close to the ledge, then side-B and press B again at once, so the end of the move grabs the ledge.
        - Before it side-Bs, the CPU simulates the side-B frame by frame (startup, dash, end, the ledge-grab box, the stage and the platform), with Fox's and Falco's numbers from the decompiled move, and only goes when it ends the way the row says. Until then it drifts toward where that side-B starts and double jumps when it needs to, even with Double Jump off (that row is for the up-B now). With Fast Fall on, it also fast falls down to its side-B height.
        - Where hitstun ends decides what's possible, and the Knockback settings below let you choose it. One double jump rises about 40, so To Main Stage and To Side Plat can't work from Very Low, and from Far they need the CPU to start high. To Side Plat works from Level or higher, short of Far. Shortest Shorten works from Close, or Near at Level or higher. When a checked row can't make it, the CPU uses its up-B if a Firefox / Firebird row is checked, or else the closest side-B row that can (Ledge Sweetspot first). The notes have a table of every row on every spot.
        - **Fast Fall** moved up a row, above the side-B rows. Saved settings carry over, with the old Illusion / Phantasm row as Instant. The five new rows of each use 10 spare bits of the save record (`TYEX` word 68 bits 19-28), so the save format doesn't change.
        - Fox and Falco CPUs now set their buttons fresh every frame, like the newer Edgeguard CPUs, which the shorten needs. The Training Lab's Recovery Options for a Fox or Falco CPU have the same rows (not saved there, as before).
        - **Knockback Height** and **Knockback Distance** (saved) replace Hit Strength, so you choose where the CPU ends up instead of how hard it's hit. They name the spot where the CPU can act again, and it's the same spot for every CPU. The details and a test list are in [the knockback notes](notes/EDGEGUARD-KNOCKBACK.md).
            - **Height**, from the ledge: **Very High** (51-61 above), **High** (23-33 above), **Level** (5 below to 5 above), **Low** (23-33 below), **Very Low** (51-61 below).
            - **Distance**, out from the ledge: **Close** (20-30), **Near** (40-50), **Medium** (60-70), **Far** (80-90).
            - **Random** on either row picks one of its choices each reset, so you can fix one and vary the other, or vary both. Each attempt also lands somewhere random inside the spot.
            - **Custom** on Height uses **Customize Knockback** (the old Customize Hit Strength, with your values) and greys out Distance. Changing a custom value selects it, as before.
            - The launch is worked out each reset from the CPU's gravity and falling speed, the stage's ledge and where the CPU starts. A launch that would touch the stage starts just off the ledge instead. Floaty CPUs need that for Low and Very Low, because they don't fall far enough in hitstun: they get sent flat or downward off the ledge.
            - The default, Low and Near, is the closest spot to the old Normal. Saved choices carry over: Weak is now Low, Close; Normal is Low, Near; Hard is High, Medium; Shallow is Very Low, Far; Custom stays Custom. The CPU's percent is 50-80 on every spot, Normal's range. Height uses Hit Strength's saved field and Distance 3 spare bits of the save record (`TYEX` word 68 bits 29-31), so the save format doesn't change.
    - **Low Laser Puff Practice**
        - For characters other than Fox and Falco, the timing readout is now **B Press Start** instead of Neutral-B Start, and it times your first aerial B press of any kind, so things like Samus's side-B missiles count. It reads the button, so a press that doesn't start a move (like during an aerial) counts too. Fox and Falco keep Laser Start, which still only times the laser.

For T9 I was working with Claude Opus 5.5 Extra High

### T8 (V1.4.1T8) - 2026-10-28

- **Program changes**
    - New [scripts/sweetspot_dj_platforms.py](scripts/sweetspot_dj_platforms.py) reads Fox's and Falco's double jump, side-B and ledge-grab numbers and the legal stages' ledges from a vanilla ISO, and prints where the Double-Jump Height Indicator draws on each legal stage. [scripts/build_dj_height_gecko.py](scripts/build_dj_height_gecko.py) rebuilds its Gecko codes.
    - New [scripts/build_blast_zone_gecko.py](scripts/build_blast_zone_gecko.py) rebuilds the Blast Zone Overlay Gecko code, and [tests/test_blast_zone_gecko.py](tests/test_blast_zone_gecko.py) runs it in an emulator.
- **Globals and all events**
    - New **Don't Reset Timers** setting on the Global Overlays & Gameplay page, below Randall Overlay after a blank row (Off by default, saved). Normally, loading a savestate puts the match timer back to where it was when you saved, and events that reset to a savestate, like Edgeguard Training, restart their timer every attempt. With it On, the timer keeps counting through loads and resets, so you can see how long you've been practicing (and, say, switch sides after a few minutes). Off keeps the old behavior for timing something from a savestate. Everything else still rewinds: event logic and Lab recordings follow the savestate as before. It uses the spare bit 7 of `TYEX` byte 8, so the save format doesn't change.
    - New **Blast Zone Overlay** setting on the Global Overlays & Gameplay page, the first stage overlay, above Randall Overlay (Off by default, saved). Randall Overlay and Don't Reset Timers each move down a row. On every stage, it draws a crisp red line exactly on the blast zones, with red fading out for a short distance past them. It reads the blast zones every frame, the same numbers the game checks for KOs. It uses a spare bit of the save record (`TYEX` word 20 bit 23), so the save format doesn't change.
        - New [Slippi Gecko code](slippi-gecko-codes/blast-zone-overlay/README.md) that draws the same line and glow on every stage in any match. It passes emulator tests but hasn't been tested in Dolphin yet.
- **Event-specific changes**
    - **Side-B Sweetspot**
        - New **Double-Jump Height Indicator** setting (Off by default, saved). Under each ledge it draws a short line and an X at ledge height above the line's middle. Double jump before your feet fall below the line, drift to the X, and side-B at the peak to sweetspot. Orange for Fox, sky blue for Falco.
            - **The line** is the lowest your feet can be when you double jump and still sweetspot: the double jump's rise (40.2 for Fox, 41.8 for Falco) plus the top of your ledge-grab box (17.5), so 57.5 under the ledge for Fox and 59.3 for Falco. Double jump from it and the blue guide line peaks level with the white one; from below it you can't make it. Fox's line is 1/6 higher because he drops that much during side-B startup.
            - It's two thirds as wide as the double jump can drift either way by its peak: ±10.6 for Fox (the most is 15.9, in 19 frames) and ±12.3 for Falco (18.5, in 22 frames).
            - **The X** is where a full side-B (the range guide's white **0** dot) just reaches the ledge. It draws on top of the white ledge line.
            - It works with the Line Guides on or off, and works out its numbers from your character's live data. It uses a spare bit of the save record, so the save format doesn't change.
        - New **Change Stage** menu: restart on any of the six tournament-legal stages without going back through the character and stage select, like Eggs-ercise, Low Laser and Training Lab. The current stage is greyed out. All of the event's settings are saved, so they carry over.
        - New [Slippi Gecko codes](slippi-gecko-codes/dj-height-indicator/README.md), one for Fox and one for Falco, that draw the same line and X on the six legal stages in any match. They don't check who is playing, and both can be on at once. Their README lists where the marks land on each stage. They pass emulator tests but haven't been tested in Dolphin yet.
    - **Edgeguard Training**
        - New **Side** setting (saved), second in the menu under Recovery Options: **Random** (the default, as before), **Left** or **Right**. On Left or Right, every attempt starts at that ledge, and the CPU is always launched off that side. It takes effect on the next reset. It uses 2 spare bits of the save record (`TYEX` word 20 bits 24-25), so the save format doesn't change.
        - New **Change Stage** menu, under Change Characters: restart on any of the six tournament-legal stages without going back through the character and stage select. Your characters and saved settings carry over, including the CPU's recovery options.
        - New **Blast Zone Overlay** toggle, under Customize Hit Strength. It's the same setting as the global one, so turning it on here turns it on everywhere.
        - New **Camera** setting, under Blast Zone Overlay: **Normal** (the default) or **Zoomed Out**, the same zoom as Side-B Sweetspot's Camera setting, to see farther offstage. Here, Zoomed Out also always keeps the bottom blast zone in view, 20 units below the line, so you can see how close both characters are to it (turn on Blast Zone Overlay to see the line). The camera still follows the fighters side to side. Normally the camera only goes down as far as the lowest fighter, so the blast zone stays off-screen until someone is about to die. It's saved separately from Side-B Sweetspot's, in a spare bit of the Edgeguard save word, so the save format doesn't change.

For T8 I was working with Claude Opus 5.5 Extra High
<img width="460" height="365" alt="tm-te t8 example" src="https://github.com/user-attachments/assets/3ea9e6be-db4d-411d-ab7b-06e4c6668c42" />

### T7 (V1.4.1T7) - 2026-10-21

- **Globals and all events**
    - **Wavedash OSD** angle now uses the same colors as the timing OSDs: cyan up to 20.0° (nearly perfect; the shallowest angle you can get is about 16.8°), green from 20.1° to 25.0°, yellow from 25.1° to 30.0°, and white from 30.1° up. Before, it was green from 16.8° to 23.6°, yellow up to 30.5°, and white otherwise. A 1f wavedash no longer turns the whole first row green, so the `Wavedash` title keeps the color you picked for the OSD; the frame count is still colored like the other timing OSDs.
    - **Actionable Yellow>Green** now works on all four ledge options, at both under and over 100%: ledge getup, ledge roll and ledge attack go yellow for the last two frames and green when you're back in Wait. Ledge jump goes yellow at the end of its second (airborne) stage and green on your first actionable frame in the air.
        - The [Slippi Gecko code](slippi-gecko-codes/actionable/README.md) for this overlay is updated to match, so the ledge options also work in Slippi.
    - **Act OoWait** now reports after a ledge getup or a ledge roll, labeled `Ledge getup` or `Ledge roll`.
    - **Act OoAttack** now reports after a ledge attack, labeled `Ledge attack`. Like throws, a ledge attack only counts as Act OoAttack, not Act OoWait.
    - **Invincibility Overlay** now tints CPUs lavender instead of gray, which was hard to see. In Techchase and Ledgechase, the **CPU Invincibility Overlay** choice **Gray** is now **Lavender** (still the default; saved choices carry over). CPU hitbox trails stay gray.
- **Event-specific changes**
    - **Edgeguard Training**
        - New **Player Position** setting (saved) for where you start each attempt: **At Ledge** (5 units in), **Close** (15 units in, the original spot and the default), **Halfway** (halfway from the ledge to the middle of the stage) or **Mid Stage** (the middle). Halfway and Mid Stage scale with the stage's size. The CPU is still launched from its usual spot, except on At Ledge, where it's launched from your spot so your bair still sends it out in front of you. It uses 2 spare bits of the Edgeguard save word, so the save format doesn't change.
        - **Samus** now reels in after her grapple connects. Before, she never pressed A again, so she hung on the wall until she dropped into tumble. She also misses it less: she only grapples when her hand will be just under the ledge and outside the stage wall. Before, she could fire with her hand above the ledge (the grapple flew over the stage) or from so close that the game cancelled it. Aimed this way, the reel should end with her on the ledge. The details are in [the Edgeguard character notes](notes/EDGEGUARD-NEW-CHARACTERS.md#samus).

For T7 I was working with Claude Opus 5.5 Extra High

### T6 (V1.4.1T6) - 2026-10-19

- **Program changes**
    - New reusable character selector module ([src/fighter_swap.h](src/fighter_swap.h)) and menu support for custom rows (extra buttons, colored values, stock icons), so other events can get the same Player/CPU Character rows later. Details are in [the mid-event swap notes](notes/MID-EVENT-CHARACTER-STAGE-SWAP.md#implemented-in-t6-player-and-cpu-character).
    - Global Settings saves version 19 of its extension record. Fixed Grid positions use 14 of the 52 spare bytes, the saved Player/CPU costumes use 2 more and the Edgeguard settings use 16, leaving 20. Older saves update automatically with every OSD on Auto, default costumes and default Edgeguard settings.
    - Reading a saved setting no longer re-checks the fields that can't hold a bad value (like the recovery toggles), so every settings read does less work.
    - The character selector can now limit one slot to a list of characters, for events that only support some CPUs.
    - **BETA, relatively untested -** New CPU recoveries for Yoshi, Peach, Luigi, Dr. Mario, the Ice Climbers, Jigglypuff and Samus ([src/recovery.c](src/recovery.c)). They press buttons the way a person does: fresh presses, and mashing at about 10 presses a second (15 with Fast Mashing). The research, the numbers behind each one and a Dolphin test list are in [the Edgeguard character notes](notes/EDGEGUARD-NEW-CHARACTERS.md); [scripts/fighter_move_data.py](scripts/fighter_move_data.py) reads the move heights and timings from the ISO.
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
    - **Under Fire - Ledgestall Training**
        - New **Lava Overlay** setting (Off by default, saved). It hides the lava's model and draws a see-through orange overlay at the same height on top of the stage and your character, so you can still see the hand grab through it on the ledge after the lava rises. The lava still rises and hurts exactly as before. It uses a spare bit of the save record, so the save format doesn't change.

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
