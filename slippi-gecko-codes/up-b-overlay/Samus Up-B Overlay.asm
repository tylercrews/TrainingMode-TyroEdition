    # To be inserted at 8016e920
    # Samus Up-B Overlay for vanilla NTSC 1.02 Melee / Slippi.
    # While Samus is in the match on a legal stage, draws the Up-B
    # Sweetspot event's targets at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 13

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 6.01 out (ECB front 2.03 + snap
    # 10, halved) and 10 down.
    shape GX_LINES, 12, 2, 0x30C8FFFF    # stick held up: fastest grab to slowest
    point 6.0150, -57.3000
    point 6.0150, -54.0000
    shape GX_POINTS, 42, 1, 0x30C8FFFF    # stick held up: up-B here
    point 6.0150, -57.3000
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 3.5150, -12.5000
    point 8.5150, -7.5000
    point 3.5150, -7.5000
    point 8.5150, -12.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    lwz r31, 28(r1)
    .endm

    .include "up-b-overlay.inc"
