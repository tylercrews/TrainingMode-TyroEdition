    # To be inserted at 8016e7f8
    # Roy Up-B Overlay for vanilla NTSC 1.02 Melee / Slippi.
    # While Roy is in the match on a legal stage, draws the Up-B
    # Sweetspot event's targets at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 26

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 7.11 out (ECB front 2.21 + snap
    # 12, halved) and 17 down.
    shape GX_LINES, 12, 2, 0x1C6C94FF    # stick toward the stage (40 degrees): fastest grab to slowest
    point 45.1314, -43.1827
    point 37.8899, -33.4636
    shape GX_POINTS, 42, 1, 0x1C6C94FF    # stick toward the stage (40 degrees): up-B here
    point 45.1314, -43.1827
    shape GX_LINES, 12, 2, 0x30C8FFFF    # stick held up: fastest grab to slowest
    point 19.4050, -61.5000
    point 20.1050, -49.4000
    shape GX_POINTS, 42, 1, 0x30C8FFFF    # stick held up: up-B here
    point 19.4050, -61.5000
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 4.6050, -19.5000
    point 9.6050, -14.5000
    point 4.6050, -14.5000
    point 9.6050, -19.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    li r4, 0
    .endm

    .include "up-b-overlay.inc"
