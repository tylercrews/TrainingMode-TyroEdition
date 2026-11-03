    # To be inserted at 8016e80c
    # Ganondorf Up-B Overlay for vanilla NTSC 1.02 Melee / Slippi.
    # While Ganondorf is in the match on a legal stage, draws the Up-B
    # Sweetspot event's targets at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 25

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 7.33 out (ECB front 5.67 + snap
    # 9, halved) and 17 down.
    shape GX_LINES, 12, 2, 0x30C8FFFF    # stick held up: fastest grab to slowest
    point 20.5350, -58.9000
    point 29.8350, -31.6000
    shape GX_POINTS, 42, 1, 0x30C8FFFF    # stick held up: up-B here
    point 20.5350, -58.9000
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 4.8350, -19.5000
    point 9.8350, -14.5000
    point 4.8350, -14.5000
    point 9.8350, -19.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    cmpwi r3, 0
    .endm

    .include "up-b-overlay.inc"
