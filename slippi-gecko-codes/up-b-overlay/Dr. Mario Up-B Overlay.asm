    # To be inserted at 8016e92c
    # Dr. Mario Up-B Overlay for vanilla NTSC 1.02 Melee / Slippi.
    # While Dr. Mario is in the match on a legal stage, draws the Up-B
    # Sweetspot event's targets at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 21

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 7.07 out (ECB front 2.14 + snap
    # 12, halved) and 15 down.
    shape GX_LINES, 12, 2, 0x1C6C94FF    # stick toward the stage (18 degrees): fastest grab to slowest
    point 36.2929, -47.9840
    point 37.2373, -40.2117
    shape GX_POINTS, 42, 1, 0x1C6C94FF    # stick toward the stage (18 degrees): up-B here
    point 36.2929, -47.9840
    shape GX_LINES, 12, 2, 0x30C8FFFF    # stick held up: fastest grab to slowest
    point 24.6700, -55.4000
    point 27.9700, -48.3000
    shape GX_POINTS, 42, 1, 0x30C8FFFF    # stick held up: up-B here
    point 24.6700, -55.4000
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 4.5700, -17.5000
    point 9.5700, -12.5000
    point 4.5700, -12.5000
    point 9.5700, -17.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    mtlr r0
    .endm

    .include "up-b-overlay.inc"
