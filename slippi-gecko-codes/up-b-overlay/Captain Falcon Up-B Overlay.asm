    # To be inserted at 8016e8cc
    # Captain Falcon Up-B Overlay for vanilla NTSC 1.02 Melee / Slippi.
    # While Captain Falcon is in the match on a legal stage, draws the Up-B
    # Sweetspot event's targets at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 2

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 6.54 out (ECB front 4.07 + snap
    # 9, halved) and 17 down.
    shape GX_LINES, 12, 2, 0x30C8FFFF    # stick held up: fastest grab to slowest
    point 18.4350, -54.6000
    point 26.7350, -30.1000
    shape GX_POINTS, 42, 1, 0x30C8FFFF    # stick held up: up-B here
    point 18.4350, -54.6000
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 4.0350, -19.5000
    point 9.0350, -14.5000
    point 4.0350, -14.5000
    point 9.0350, -19.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    cmplwi r12, 0
    .endm

    .include "up-b-overlay.inc"
