    # To be inserted at 8016e7bc
    # Sheik Up-B Overlay B0XX for vanilla NTSC 1.02 Melee / Slippi.
    # While Sheik is in the match on a legal stage, draws the Up-B
    # Sweetspot event's B0XX Angles at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 7

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 7.04 out (ECB front 3.07 + snap
    # 11, halved) and 15 down.
    # Snap: the top of the box, 5.5 above that.
    shape GX_POINTS, 30, 16, 0x1C6C94FF    # B0XX: Mod X or Mod Y without B
    point 40.2000, -29.0530
    point 40.2000, 4.5530
    point 38.7949, -31.4471
    point 38.7949, 6.9471
    point 39.7903, -35.2771
    point 39.7903, 10.7771
    point 21.0880, -48.1650
    point 21.0880, 23.6650
    point 23.4821, -46.7599
    point 23.4821, 22.2599
    point 27.3121, -47.7553
    point 27.3121, 23.2553
    point 29.0998, -45.1373
    point 29.0998, 20.6373
    point 32.6134, -45.5938
    point 32.6134, 21.0938
    shape GX_POINTS, 30, 25, 0x30C8FFFF    # B0XX: Mod X or Mod Y, with B
    point 40.2850, -15.0000
    point 7.0350, -45.7500
    point 7.0350, 21.2500
    point 43.6939, -30.5675
    point 43.6939, 6.0675
    point 42.3208, -33.1470
    point 42.3208, 8.6470
    point 41.0510, -36.0099
    point 41.0510, 11.5099
    point 37.9479, -37.5297
    point 37.9479, 13.0297
    point 35.0754, -38.6419
    point 35.0754, 14.1419
    point 7.0350, -49.7500
    point 7.0350, 25.2500
    point 22.6025, -51.6589
    point 22.6025, 27.1589
    point 25.1820, -50.2858
    point 25.1820, 25.7858
    point 28.0449, -49.0160
    point 28.0449, 24.5160
    point 30.6232, -47.1201
    point 30.6232, 22.6201
    point 31.5086, -44.6808
    point 31.5086, 20.1808
    shape GX_POINTS, 30, 5, 0xFFFFFFFF    # B0XX: no modifier
    point 47.0350, -15.0000
    point 7.0350, -55.0000
    point 7.0350, 30.5000
    point 35.1771, -43.1421
    point 35.1771, 18.6421
    shape GX_LINES, 12, 2, 0xFFFFFFFF    # the snap: coming down, the up-B grabs here
    point 4.5350, -9.5000
    point 9.5350, -9.5000
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 4.5350, -17.5000
    point 9.5350, -12.5000
    point 4.5350, -12.5000
    point 9.5350, -17.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    mr r3, r31
    .endm

    .include "up-b-overlay.inc"
