# Under Fire

The event keeps its native Brinstar lava challenge and automatically retries
after a hit or death. A large **Perfect Refreshes** counter replaces the timer,
with a smaller **High Score: N** above it and **Session Best: N** directly beneath
it. High Score shows the saved perfect-refresh record loaded on event entry;
an absent record or an old survival-time record displays zero. Each perfect
ledge refresh (Invincibility = 0) adds
one, including before the lava reaches the ledge. Close misses do not score.
Retries and side changes reset the current count; Session Best keeps the
highest count across those attempts. Both start at zero on entering the event
and all three lines hide while paused. Use the pause menu's Exit or
L + R + A + Start to save that best count. A newly saved High Score appears on
the next entry; Session Best tracks improvements during the current session.

The native timer models are hidden during event updates, including after
unpausing restores the HUD. The score counters remain visible during play.
The shared menu owns pausing. Under Fire disables the native pause menu and
its HUD, and skips native camera hotkeys while the shared menu is open. Exit
unpauses before ending the match. The high-score display reads the existing
record and adds no save fields. Lava Overlay uses one spare bit of the
existing settings record (see below).

Event Select displays an integer count. Old survival-time records are hidden
until the first positive count replaces them, rather than treating a frame
total as a count. A single format-marker bit in save-extension version 5
distinguishes the two; existing preferences survive migration and the extension
stays 16 bytes. Other event records are unchanged.

| Setting | Default | Behavior |
| --- | --- | --- |
| Highlight Dropped Invincibility | On | Shared red tangibility highlight; overrides the global invincibility tint. |
| Hitbox Trails | Off | Event choice overrides global trails. All seven normal choices are available. |
| Ledge Side | Left | Left or Right; changing it restarts the attempt on that ledge. |
| Disable Attack Particles | Off, saved | Hides attack/special effects, including Fox/Falco up-B flames; preserves the ledge-grab circle and double-jump rings. |
| Lava Overlay | Off, saved | Hides the lava model and draws a translucent stand-in at its height over the stage and fighter, so ledge grabs stay visible through it. The lava's rise and hitbox are unchanged. |
| Ledge Refresh OSTs | On | Toggles both Ledge Drop and Invincibility. Counting still works when Off. |
| Suppress Global OSTs | On | Hides global OSD results and layout placeholders only during Under Fire. Saved global choices stay unchanged. |

D-pad Left/Right selects the corresponding ledge and restarts immediately.
The native restore resets lava height, the lava delay, player state and counter;
the camera also moves to the chosen ledge. Disable Attack Particles and Lava
Overlay are saved; the other Under Fire options are session-only.

Invincibility shows **green 0 at the perfect refresh boundary**, green +x
for earlier regrabs, magenta -1 or -2 for close misses, and red -3 or later.
As described in the [Firefox ledgestall discussion](https://smashboards.com/threads/question-about-firefox-ledgestall.357655/),
the remaining intangibility and ledge-grab lockout both last 29 frames:
a perfectly executed stall regrabs as the old protection expires and renews
it, giving continuous total intangibility. This boundary previously read -1;
it now reads 0. This explanation also appears in the event menu's help text.
Every animation update samples protection before collision and
catch cleanup, including moves with no IASA callback. Native ledge protection
grants start the shared invincibility cycle; regrab uses the old sample before
CliffWait renews protection. Frozen updates do not advance the clock.

The two readouts share a centered horizontal row: **Ledge Drop, Invincibility**.
Ledge Drop counts from the start of that cycle, excluding the initial ledge-hold
frame that requires a neutral release before a drop is permitted.
The first legal drop is frame 1, shown in green; frames 2 and 3 are magenta,
and frame 4 onward is red. The drop diagnostic observes real input-driven
state changes and ignores landings, automatic timeouts and collision
transitions. Frozen updates and duplicate catch callbacks cannot add counts.
Scoring requires a previous protection cycle and ledge departure, so initial
placement never scores; hit/death feedback cannot add to a failed attempt.

Act/Jump was renamed **Act**, reduced to its single invincibility-cycle frame,
then disabled in a retained `#if 0` code block. It has no live menu option or
Text allocation. Its former runtime option storage now serves Suppress Global
OSTs, which releases its override on Exit and scene changes. Global suppression
does not affect the event's counter, refresh readouts or body overlays.

Particle suppression filters new fighter-owned models and loose particles
during attacks and specials, including late accessory effects. It explicitly
allows the ledge-grab circle even during the catch transition, and leaves free
movement effects such as double-jump rings visible. It does not blanket-delete
existing effects or clear the native effect-created flag each frame. This
prevents repeated charge/launch-ring spawns. Stage effects and move mechanics
remain active; turning the toggle off permits new attack effects normally.

Lava Overlay is event-local code in [src/events.c](../src/events.c), not a
global overlay.
While On, it sets `JOBJ_HIDDEN` on every joint of Brinstar's lava (map GObj 8)
that was visible, remembering only those joints so Off restores the model
exactly. Its GOBJ draws in the in-game effects GX link (after the stage and
fighters) with depth testing off: a translucent orange quad from the lava
surface down, plus a bright surface line, over everything. The fighter and the
ledge grab stay visible through it. (Earlier attempts: the end of the stage
link never appeared, and drawing behind the fighter in the fighter link hid the
ledge.) The surface is the Y of
the joint Under Fire's assembly moves (map joint 0, not the tree's root, which
stays put) minus 55, the offset the assembly adds when it moves that joint. Only
per-match instance flags change, never the stage file's shared data, so later
Brinstar matches are unaffected. Exit releases it, and scene changes forget the
old joints. The setting is saved in `TYEX` word 20 bit 21 (former padding that
every record owning word 20 zeroed), so the extension stays version 19.
