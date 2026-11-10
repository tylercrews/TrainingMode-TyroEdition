    # To be inserted at 8016e7b0
    # Zelda Up-B Overlay B0XX for vanilla NTSC 1.02 Melee / Slippi.
    # While Zelda is in the match on a legal stage, draws the Up-B
    # Sweetspot event's B0XX Angles at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 19

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 6.50 out (ECB front 2.00 + snap
    # 11, halved) and 10 down.
    shape GX_POINTS, 30, 20, 0x1C6C94FF    # B0XX: Mod X or Mod Y without B
    point 75.7801, -39.3560
    point 75.7801, 19.3560
    point 72.8198, -44.3442
    point 72.8198, 24.3442
    point 75.1606, -52.5042
    point 75.1606, 32.5042
    point 69.5745, -56.1796
    point 69.5745, 36.1796
    point 63.8203, -59.1317
    point 63.8203, 39.1317
    point 35.8560, -79.2801
    point 35.8560, 59.2801
    point 40.8442, -76.3198
    point 40.8442, 56.3198
    point 49.0042, -78.6606
    point 49.0042, 58.6606
    point 52.6796, -73.0745
    point 52.6796, 53.0745
    point 60.2068, -74.2376
    point 60.2068, 54.2376
    shape GX_POINTS, 30, 25, 0x30C8FFFF    # B0XX: Mod X or Mod Y, with B
    point 75.6500, -10.0000
    point 6.5000, -73.6500
    point 6.5000, 53.6500
    point 83.4678, -42.6849
    point 83.4678, 22.6849
    point 80.5715, -48.0939
    point 80.5715, 28.0939
    point 77.9319, -54.1197
    point 77.9319, 34.1197
    point 71.2757, -57.2094
    point 71.2757, 37.2094
    point 65.1309, -59.4339
    point 65.1309, 39.4339
    point 6.5000, -82.4500
    point 6.5000, 62.4500
    point 39.1849, -86.9678
    point 39.1849, 66.9678
    point 44.5939, -84.0715
    point 44.5939, 64.0715
    point 50.6197, -81.4319
    point 50.6197, 61.4319
    point 56.0264, -77.4402
    point 56.0264, 57.4402
    point 57.7973, -72.2116
    point 57.7973, 52.2116
    shape GX_POINTS, 30, 5, 0xFFFFFFFF    # B0XX: no modifier
    point 90.5000, -10.0000
    point 6.5000, -94.0000
    point 6.5000, 74.0000
    point 65.5843, -69.0843
    point 65.5843, 49.0843
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 4.0000, -12.5000
    point 9.0000, -7.5000
    point 4.0000, -7.5000
    point 9.0000, -12.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    li r3, 70
    .endm

    .include "up-b-overlay.inc"
