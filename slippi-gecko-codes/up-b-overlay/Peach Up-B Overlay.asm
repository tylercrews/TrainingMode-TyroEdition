    # To be inserted at 8016e908
    # Peach Up-B Overlay for vanilla NTSC 1.02 Melee / Slippi.
    # While Peach is in the match on a legal stage, draws the Up-B
    # Sweetspot event's targets at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 9

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 6.00 out (ECB front 2.00 + snap
    # 10, halved) and 15 down.
    shape GX_LINES, 12, 2, 0x30C8FFFF    # stick held up: fastest grab to slowest
    point 9.4000, -51.8000
    point 10.1000, -49.4000
    shape GX_POINTS, 42, 1, 0x30C8FFFF    # stick held up: up-B here
    point 9.4000, -51.8000
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 3.5000, -17.5000
    point 8.5000, -12.5000
    point 3.5000, -12.5000
    point 8.5000, -17.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    rlwinm. r0, r0, 29, 31, 31
    .endm

    .include "up-b-overlay.inc"
