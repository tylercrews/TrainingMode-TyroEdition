    # To be inserted at 8016e924
    # Luigi Up-B Overlay for vanilla NTSC 1.02 Melee / Slippi.
    # While Luigi is in the match on a legal stage, draws the Up-B
    # Sweetspot event's targets at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 17

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 7.18 out (ECB front 2.36 + snap
    # 12, halved) and 15 down.
    shape GX_LINES, 12, 2, 0x30C8FFFF    # stick held up: fastest grab to slowest
    point 7.1800, -60.9000
    point 7.1800, -52.9000
    shape GX_POINTS, 42, 1, 0x30C8FFFF    # stick held up: up-B here
    point 7.1800, -60.9000
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 4.6800, -17.5000
    point 9.6800, -12.5000
    point 4.6800, -12.5000
    point 9.6800, -17.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    lwz r30, 24(r1)
    .endm

    .include "up-b-overlay.inc"
