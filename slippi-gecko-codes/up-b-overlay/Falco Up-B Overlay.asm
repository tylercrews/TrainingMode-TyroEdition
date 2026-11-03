    # To be inserted at 8016e89c
    # Falco Up-B Overlay for vanilla NTSC 1.02 Melee / Slippi.
    # While Falco is in the match on a legal stage, draws the Up-B
    # Sweetspot event's targets at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 22

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 6.65 out (ECB front 2.30 + snap
    # 11, halved) and 13 down.
    shape GX_LINESTRIP, 10, 41, 0x30C8FFFF    # ring at the full travel, 60.10
    point 6.6500, 47.1000
    point 11.3654, 46.9147
    point 16.0517, 46.3600
    point 20.6801, 45.4394
    point 25.2219, 44.1585
    point 29.6493, 42.5251
    point 33.9348, 40.5495
    point 38.0522, 38.2437
    point 41.9759, 35.6219
    point 45.6818, 32.7004
    point 49.1471, 29.4971
    point 52.3504, 26.0318
    point 55.2719, 22.3259
    point 57.8937, 18.4022
    point 60.1995, 14.2848
    point 62.1751, 9.9993
    point 63.8085, 5.5719
    point 65.0894, 1.0301
    point 66.0100, -3.5983
    point 66.5647, -8.2846
    point 66.7500, -13.0000
    point 66.5647, -17.7154
    point 66.0100, -22.4017
    point 65.0894, -27.0301
    point 63.8085, -31.5719
    point 62.1751, -35.9993
    point 60.1995, -40.2848
    point 57.8937, -44.4022
    point 55.2719, -48.3259
    point 52.3504, -52.0318
    point 49.1471, -55.4971
    point 45.6818, -58.7004
    point 41.9759, -61.6219
    point 38.0522, -64.2437
    point 33.9348, -66.5495
    point 29.6493, -68.5251
    point 25.2219, -70.1585
    point 20.6801, -71.4394
    point 16.0517, -72.3600
    point 11.3654, -72.9147
    point 6.6500, -73.1000
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 4.1500, -15.5000
    point 9.1500, -10.5000
    point 4.1500, -10.5000
    point 9.1500, -15.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    addi r3, r3, -18784
    .endm

    .include "up-b-overlay.inc"
