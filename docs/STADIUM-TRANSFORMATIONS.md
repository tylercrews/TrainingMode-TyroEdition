# Training Lab Stadium transformations

On Pokémon Stadium, open the Training Lab pause menu's Stage Options and choose **Normal, Fire, Grass, Rock, or Water**. Resume play to run the transition. The selected form stays in place until another is chosen, with **Hazards On or Off**. The selection is session-local and starts at Normal each match.

Changing between two transformed forms returns through Normal first. This is intentional: the native game frees the previous transformation's model and texture buffer during that return. Loading a second transformation into the same buffer while the first is still live can corrupt its assets. Changes made during a transition are queued; the latest selection wins once the current transition finishes.

## Fix

The old setting returned immediately for Normal, depended on the Stadium manager proc removed by Hazards Off, changed shared timing parameters, and patched a native RNG instruction globally. It also indexed Rock and Water against a differently ordered native random-selection table.

`src/stadium.c` now replaces only Training Lab's Stadium manager proc. It explicitly starts the native asynchronous file load and then lets the original `grStadium_801D4548` state machine perform the warning, terrain swap, model scaling, collision relinking and cleanup. It calls the state machine only during a requested transition, so its random idle timer cannot change the held form. Moving collision and wind updates remain scheduled, without duplicating the hazard-free wind fallback.

The file mapping is Fire → `GrPs1.dat`, Grass → `GrPs2.dat`, Rock → `GrPs4.dat`, Water → `GrPs3.dat`. No native instructions or shared Stadium parameters are overwritten, and other events and ordinary Stadium matches retain their native controller.

The native manager layout, file loading callback and transition lifecycle are documented in the [Melee decompilation's Stadium controller](https://github.com/doldecomp/melee/blob/master/src/melee/gr/grpstadium.c). This implementation targets the bundled NTSC 1.02 DOL.

## Validation

`python tests/test_stadium.py` executes the production controller against the actual DOL's transition state machine. Rendering, terrain allocation and DVD completion are fixtures, including a delayed load. Coverage includes all four file mappings, returning to Normal, switching through Normal, holding forms beyond the native timeout, changing choices during a transition, invalid values, other-stage isolation, and both hazard settings. The existing Lab menu tests also validate its relocated production DAT.

The user confirmed that the Stadium transformations work correctly in gameplay. Automated checks cover both hazard settings and the transition cases above.
