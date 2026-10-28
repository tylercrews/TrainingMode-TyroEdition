    # To be inserted at 8016e8e0
    # Fox Double-Jump Height Indicator for vanilla NTSC 1.02 Melee / Slippi.
    # On the six legal stages, draws Fox's double-jump platform and side-B
    # target X under each ledge, in orange. The drawing is shared with the
    # Falco code in dj-height-indicator.inc; the numbers come from
    # scripts/sweetspot_dj_platforms.py.

    .macro character_data
    .float 83.740                    # full side-B
    .float 10.573                    # platform half-width: 2/3 of the 15.860 drift
    .float 57.537                    # platform depth: lowest feet height to double jump from
    .long 0xFF8C00FF                 # orange
    .endm

    .macro original_instruction
    mr r30, r3
    .endm

    .include "dj-height-indicator.inc"
