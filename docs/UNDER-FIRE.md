# Under Fire

The event keeps its native Brinstar lava challenge and automatically retries
after a hit or death. Use the pause menu's Exit to finish and save the session
best survival score. The best attempt appears beneath the timer.

| Setting | Default | Behavior |
| --- | --- | --- |
| Highlight Dropped Invincibility | On | Shared red tangibility highlight; overrides the global invincibility tint. |
| Hitbox Trails | Off | Event choice overrides global trails. All seven normal choices are available. |
| Ledge Side | Left | Left or Right; changing it restarts the attempt on that ledge. |
| Ledge Invincibility | On | Shows remaining ledge invincibility at regrab, or how many frames late the regrab was. |
| Disable Move Particles | Off | Hides the player's move effects, including Fox/Falco up-B flames. |

D-pad Left/Right selects the corresponding ledge and restarts immediately.
The native restore resets lava height, the lava delay, player state and timer;
the camera also moves to the chosen ledge. These options are session-only.

The Ledge Invincibility display uses positive green frame counts while the
previous ledge protection remains and negative red counts once it expires.
It samples during CliffCatch, before CliffWait grants fresh protection. The
first tangible regrab frame is -1. Timing excludes hitlag/frozen updates and
resets with each attempt. This event display works with the global Ledge OSD
turned off and has its own toggle.

Particle suppression runs after fighter processing and removes fighter-owned
effects through native cleanup. It leaves the lava hazard and move mechanics
active. Turning it off permits new effects to appear normally.
