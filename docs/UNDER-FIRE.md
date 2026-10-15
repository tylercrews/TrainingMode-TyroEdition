# Under Fire

The event keeps its native Brinstar lava challenge and automatically retries
after a hit or death. Use the pause menu's Exit to finish and save the session
best survival score. The best attempt appears beneath the timer.

| Setting | Default | Behavior |
| --- | --- | --- |
| Highlight Dropped Invincibility | On | Shared red tangibility highlight; overrides the global invincibility tint. |
| Hitbox Trails | Off | Event choice overrides global trails. All seven normal choices are available. |
| Ledge Side | Left | Left or Right; changing it restarts the attempt on that ledge. |
| Invincibility | On | Shows spare protected frames at regrab, or how many frames late the regrab was. |
| Disable Attack Particles | Off, saved | Hides attack/special effects, including Fox/Falco up-B flames; preserves the ledge-grab circle and double-jump rings. |
| Ledge Drop | On | Invincibility-cycle frame when the player drops from CliffWait. |
| Act/Jump | On | Invincibility-cycle frame - frames after drop/fastfall when an aerial action starts. |

D-pad Left/Right selects the corresponding ledge and restarts immediately.
The native restore resets lava height, the lava delay, player state and timer;
the camera also moves to the chosen ledge. Disable Attack Particles is saved;
the other Under Fire options are session-only.

Invincibility shows **green 0 at the perfect refresh boundary**, green +x
for earlier regrabs, and red -1, -2, etc. for additional late frames.
As described in the [Firefox ledgestall discussion](https://smashboards.com/threads/question-about-firefox-ledgestall.357655/),
the remaining intangibility and ledge-grab lockout both last 29 frames:
a perfectly executed stall regrabs as the old protection expires and renews
it, giving continuous total intangibility. This boundary previously read -1;
it now reads 0. This explanation also appears in the event menu's help text.
Every animation update samples protection before collision and
catch cleanup, including moves with no IASA callback. Native ledge protection
grants start the shared invincibility cycle; regrab uses the old sample before
CliffWait renews protection. Frozen updates do not advance the clock.

The three readouts share one horizontal row, left to right:
**Ledge Drop, Act/Jump, Invincibility**.
Ledge Drop counts from the start of that cycle, excluding the initial ledge-hold
frame that requires a neutral release before a drop is permitted.
The first legal drop is frame 1, shown in green; every later drop is red.
Act/Jump shows two white numbers, for example `4f - 3f`: the first uses the same
invincibility origin; the second counts after the drop. If already
fastfalling, it counts input opportunities after fastfall starts instead.
Both displays observe real input-driven state changes and ignore landings,
automatic ledge timeouts, collision transitions, hitlag and duplicate updates.
Each has its own toggle, independent of global OSD settings.

Particle suppression filters new fighter-owned models and loose particles
during attacks and specials, including late accessory effects. It explicitly
allows the ledge-grab circle even during the catch transition, and leaves free
movement effects such as double-jump rings visible. It does not blanket-delete
existing effects or clear the native effect-created flag each frame. This
prevents repeated charge/launch-ring spawns. Stage effects and move mechanics
remain active; turning the toggle off permits new attack effects normally.
