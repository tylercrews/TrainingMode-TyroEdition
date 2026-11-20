    # To be inserted at 8016e800
    # Zelda Up-B Overlay for vanilla NTSC 1.02 Melee / Slippi.
    # While Zelda is in the match on a legal stage, draws the Up-B
    # Sweetspot event's targets at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 19

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 6.50 out (ECB front 2.00 + snap
    # 11, halved) and 10 down.
    # Snap: the top of the box, 5.5 above that.
    shape GX_LINESTRIP, 10, 31, 0x1C6C94FF    # least tilt that aims, 73.16 to 16.84 degrees
    point 30.7363, 75.5852
    point 31.9844, 70.7257
    point 33.2187, 66.6124
    point 34.4380, 63.0483
    point 35.6408, 59.8973
    point 36.8259, 57.0626
    point 37.9920, 54.4731
    point 39.1379, 52.0761
    point 40.2623, 49.8313
    point 41.3641, 47.7074
    point 42.6026, 45.9044
    point 44.2346, 44.6945
    point 45.8261, 43.4318
    point 47.3754, 42.1176
    point 48.8807, 40.7534
    point 50.3406, 39.3406
    point 51.7534, 37.8807
    point 53.1176, 36.3754
    point 54.4318, 34.8261
    point 55.6945, 33.2346
    point 56.9044, 31.6026
    point 58.7074, 30.3641
    point 60.8313, 29.2623
    point 63.0761, 28.1379
    point 65.4731, 26.9920
    point 68.0626, 25.8259
    point 70.8973, 24.6408
    point 74.0483, 23.4380
    point 77.6124, 22.2187
    point 81.7257, 20.9844
    point 86.5852, 19.7363
    shape GX_LINESTRIP, 10, 31, 0x1C6C94FF    # least tilt that aims, -16.84 to -73.16 degrees
    point 86.5852, -34.2363
    point 81.7257, -35.4844
    point 77.6124, -36.7187
    point 74.0483, -37.9380
    point 70.8973, -39.1408
    point 68.0626, -40.3259
    point 65.4731, -41.4920
    point 63.0761, -42.6379
    point 60.8313, -43.7623
    point 58.7074, -44.8641
    point 56.9044, -46.1026
    point 55.6945, -47.7346
    point 54.4318, -49.3261
    point 53.1176, -50.8754
    point 51.7534, -52.3807
    point 50.3406, -53.8406
    point 48.8807, -55.2534
    point 47.3754, -56.6176
    point 45.8261, -57.9318
    point 44.2346, -59.1945
    point 42.6026, -60.4044
    point 41.3641, -62.2074
    point 40.2623, -64.3313
    point 39.1379, -66.5761
    point 37.9920, -68.9731
    point 36.8259, -71.5626
    point 35.6408, -74.3973
    point 34.4380, -77.5483
    point 33.2187, -81.1124
    point 31.9844, -85.2257
    point 30.7363, -90.0852
    shape GX_LINESTRIP, 10, 3, 0x1C6C94FF    # least tilt through the 90 degree mark
    point 4.3362, 57.4622
    point 6.5000, 57.5000
    point 8.6638, 57.4622
    shape GX_LINESTRIP, 10, 3, 0x1C6C94FF    # least tilt through the 0 degree mark
    point 68.4622, -7.8362
    point 68.5000, -10.0000
    point 68.4622, -12.1638
    shape GX_LINESTRIP, 10, 3, 0x1C6C94FF    # least tilt through the -90 degree mark
    point 8.6638, -71.9622
    point 6.5000, -72.0000
    point 4.3362, -71.9622
    shape GX_LINESTRIP, 10, 16, 0x30C8FFFF    # ring at the full travel 84.00, 73.16 to 16.84 degrees, around the snap
    point 30.8313, 75.8989
    point 36.0444, 74.1329
    point 41.1306, 72.0292
    point 46.0682, 69.5969
    point 50.8359, 66.8465
    point 55.4132, 63.7898
    point 59.7805, 60.4399
    point 63.9191, 56.8111
    point 67.8111, 52.9191
    point 71.4399, 48.7805
    point 74.7898, 44.4132
    point 77.8465, 39.8359
    point 80.5969, 35.0682
    point 83.0292, 30.1306
    point 85.1329, 25.0444
    point 86.8989, 19.8313
    shape GX_LINESTRIP, 10, 16, 0x30C8FFFF    # ring at the full travel 84.00, -16.84 to -73.16 degrees
    point 86.8989, -34.3313
    point 85.1329, -39.5444
    point 83.0292, -44.6306
    point 80.5969, -49.5682
    point 77.8465, -54.3359
    point 74.7898, -58.9132
    point 71.4399, -63.2805
    point 67.8111, -67.4191
    point 63.9191, -71.3111
    point 59.7805, -74.9399
    point 55.4132, -78.2898
    point 50.8359, -81.3465
    point 46.0682, -84.0969
    point 41.1306, -86.5292
    point 36.0444, -88.6329
    point 30.8313, -90.3989
    shape GX_LINESTRIP, 10, 3, 0x30C8FFFF    # ring through the 90 degree mark
    point 3.5684, 79.4488
    point 6.5000, 79.5000
    point 9.4316, 79.4488
    shape GX_LINESTRIP, 10, 3, 0x30C8FFFF    # ring through the 0 degree mark
    point 90.4488, -7.0684
    point 90.5000, -10.0000
    point 90.4488, -12.9316
    shape GX_LINESTRIP, 10, 3, 0x30C8FFFF    # ring through the -90 degree mark
    point 9.4316, -93.9488
    point 6.5000, -94.0000
    point 3.5684, -93.9488
    shape GX_LINES, 12, 6, 0xFFFFFFFF    # straight across, up and down: hold straight here
    point 6.5000, 57.5000
    point 6.5000, 79.5000
    point 68.5000, -10.0000
    point 90.5000, -10.0000
    point 6.5000, -72.0000
    point 6.5000, -94.0000
    shape GX_LINES, 12, 2, 0xFFFFFFFF    # the snap: coming down, the up-B grabs here
    point 4.0000, -4.5000
    point 9.0000, -4.5000
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 4.0000, -12.5000
    point 9.0000, -7.5000
    point 4.0000, -7.5000
    point 9.0000, -12.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    lis r3, -32697
    .endm

    .include "up-b-overlay.inc"
