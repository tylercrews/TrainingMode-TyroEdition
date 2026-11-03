    # To be inserted at 8016e8a0
    # Marth Up-B Overlay for vanilla NTSC 1.02 Melee / Slippi.
    # While Marth is in the match on a legal stage, draws the Up-B
    # Sweetspot event's targets at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 18

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 7.00 out (ECB front 2.00 + snap
    # 12, halved) and 17 down.
    shape GX_LINES, 12, 2, 0x1C6C94FF    # stick toward the stage (20 degrees): fastest grab to slowest
    point 32.7624, -57.9589
    point 29.6062, -45.4862
    shape GX_POINTS, 42, 1, 0x1C6C94FF    # stick toward the stage (20 degrees): up-B here
    point 32.7624, -57.9589
    shape GX_LINES, 12, 2, 0x30C8FFFF    # stick held up: fastest grab to slowest
    point 17.2000, -64.3000
    point 18.5000, -51.5000
    shape GX_POINTS, 42, 1, 0x30C8FFFF    # stick held up: up-B here
    point 17.2000, -64.3000
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 4.5000, -19.5000
    point 9.5000, -14.5000
    point 4.5000, -14.5000
    point 9.5000, -19.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    lwz r12, 9472(r3)
    .endm

    .include "up-b-overlay.inc"
