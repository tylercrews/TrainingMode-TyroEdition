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
    shape GX_LINESTRIP, 10, 31, 0x1C6C94FF    # least tilt that aims, 73.16 to 16.84 degrees
    point 30.7363, 70.0852
    point 31.9844, 65.2257
    point 33.2187, 61.1124
    point 34.4380, 57.5483
    point 35.6408, 54.3973
    point 36.8259, 51.5626
    point 37.9920, 48.9731
    point 39.1379, 46.5761
    point 40.2623, 44.3313
    point 41.3641, 42.2074
    point 42.6026, 40.4044
    point 44.2346, 39.1945
    point 45.8261, 37.9318
    point 47.3754, 36.6176
    point 48.8807, 35.2534
    point 50.3406, 33.8406
    point 51.7534, 32.3807
    point 53.1176, 30.8754
    point 54.4318, 29.3261
    point 55.6945, 27.7346
    point 56.9044, 26.1026
    point 58.7074, 24.8641
    point 60.8313, 23.7623
    point 63.0761, 22.6379
    point 65.4731, 21.4920
    point 68.0626, 20.3259
    point 70.8973, 19.1408
    point 74.0483, 17.9380
    point 77.6124, 16.7187
    point 81.7257, 15.4844
    point 86.5852, 14.2363
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
    shape GX_LINESTRIP, 10, 4, 0x1C6C94FF    # least tilt through the 90 degree mark
    point 2.1751, 51.8490
    point 5.0573, 51.9832
    point 7.9427, 51.9832
    point 10.8249, 51.8490
    shape GX_LINESTRIP, 10, 4, 0x1C6C94FF    # least tilt through the 0 degree mark
    point 68.3490, -5.6751
    point 68.4832, -8.5573
    point 68.4832, -11.4427
    point 68.3490, -14.3249
    shape GX_LINESTRIP, 10, 4, 0x1C6C94FF    # least tilt through the -90 degree mark
    point 10.8249, -71.8490
    point 7.9427, -71.9832
    point 5.0573, -71.9832
    point 2.1751, -71.8490
    shape GX_LINESTRIP, 10, 16, 0x30C8FFFF    # ring at the full travel 84.00, 73.16 to 16.84 degrees
    point 30.8313, 70.3989
    point 36.0444, 68.6329
    point 41.1306, 66.5292
    point 46.0682, 64.0969
    point 50.8359, 61.3465
    point 55.4132, 58.2898
    point 59.7805, 54.9399
    point 63.9191, 51.3111
    point 67.8111, 47.4191
    point 71.4399, 43.2805
    point 74.7898, 38.9132
    point 77.8465, 34.3359
    point 80.5969, 29.5682
    point 83.0292, 24.6306
    point 85.1329, 19.5444
    point 86.8989, 14.3313
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
    shape GX_LINESTRIP, 10, 4, 0x30C8FFFF    # ring through the 90 degree mark
    point 0.6405, 73.7954
    point 4.5454, 73.9773
    point 8.4546, 73.9773
    point 12.3595, 73.7954
    shape GX_LINESTRIP, 10, 4, 0x30C8FFFF    # ring through the 0 degree mark
    point 90.2954, -4.1405
    point 90.4773, -8.0454
    point 90.4773, -11.9546
    point 90.2954, -15.8595
    shape GX_LINESTRIP, 10, 4, 0x30C8FFFF    # ring through the -90 degree mark
    point 12.3595, -93.7954
    point 8.4546, -93.9773
    point 4.5454, -93.9773
    point 0.6405, -93.7954
    shape GX_LINES, 12, 6, 0xFFFFFFFF    # straight across, up and down: hold straight here
    point 6.5000, 52.0000
    point 6.5000, 74.0000
    point 68.5000, -10.0000
    point 90.5000, -10.0000
    point 6.5000, -72.0000
    point 6.5000, -94.0000
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
