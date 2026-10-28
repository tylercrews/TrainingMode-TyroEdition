    # To be inserted at 8016e8e8
    # Falco Double-Jump Height Indicator for vanilla NTSC 1.02 Melee / Slippi.
    # On the six legal stages, draws Falco's double-jump platform and side-B
    # target X under each ledge, in sky blue. The drawing is shared with the
    # Fox code in dj-height-indicator.inc; the numbers come from
    # scripts/sweetspot_dj_platforms.py. This hook is two instructions after
    # Fox's, so both codes can be on at once.

    .macro character_data
    .float 77.080                    # full side-B
    .float 12.340                    # platform half-width: 2/3 of the 18.510 drift
    .float 59.278                    # platform depth: lowest feet height to double jump from
    .long 0x30C8FFFF                 # sky blue
    .endm

    .macro original_instruction
    cmplwi r30, 0                    # Sets cr0 for the native beq that follows.
    .endm

    .include "dj-height-indicator.inc"
