    # To be inserted at 8016e804
    # Mewtwo Up-B Overlay for vanilla NTSC 1.02 Melee / Slippi.
    # While Mewtwo is in the match on a legal stage, draws the Up-B
    # Sweetspot event's targets at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 16

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 7.07 out (ECB front 3.14 + snap
    # 11, halved) and 14 down.
    shape GX_LINESTRIP, 10, 41, 0x30C8FFFF    # ring at the full travel, 50.00
    point 7.0700, 36.0000
    point 10.9930, 35.8459
    point 14.8917, 35.3844
    point 18.7423, 34.6185
    point 22.5208, 33.5528
    point 26.2042, 32.1940
    point 29.7695, 30.5503
    point 33.1949, 28.6320
    point 36.4593, 26.4508
    point 39.5424, 24.0203
    point 42.4253, 21.3553
    point 45.0903, 18.4724
    point 47.5208, 15.3893
    point 49.7020, 12.1249
    point 51.6203, 8.6995
    point 53.2640, 5.1342
    point 54.6228, 1.4508
    point 55.6885, -2.3277
    point 56.4544, -6.1783
    point 56.9159, -10.0770
    point 57.0700, -14.0000
    point 56.9159, -17.9230
    point 56.4544, -21.8217
    point 55.6885, -25.6723
    point 54.6228, -29.4508
    point 53.2640, -33.1342
    point 51.6203, -36.6995
    point 49.7020, -40.1249
    point 47.5208, -43.3893
    point 45.0903, -46.4724
    point 42.4253, -49.3553
    point 39.5424, -52.0203
    point 36.4593, -54.4508
    point 33.1949, -56.6320
    point 29.7695, -58.5503
    point 26.2042, -60.1940
    point 22.5208, -61.5528
    point 18.7423, -62.6185
    point 14.8917, -63.3844
    point 10.9930, -63.8459
    point 7.0700, -64.0000
    shape GX_LINESTRIP, 10, 41, 0x1C6C94FF    # dim ring at the least tilt, 40.00
    point 7.0700, 26.0000
    point 10.2084, 25.8767
    point 13.3274, 25.5075
    point 16.4078, 24.8948
    point 19.4307, 24.0423
    point 22.3773, 22.9552
    point 25.2296, 21.6403
    point 27.9699, 20.1056
    point 30.5814, 18.3607
    point 33.0479, 16.4162
    point 35.3543, 14.2843
    point 37.4862, 11.9779
    point 39.4307, 9.5114
    point 41.1756, 6.8999
    point 42.7103, 4.1596
    point 44.0252, 1.3073
    point 45.1123, -1.6393
    point 45.9648, -4.6622
    point 46.5775, -7.7426
    point 46.9467, -10.8616
    point 47.0700, -14.0000
    point 46.9467, -17.1384
    point 46.5775, -20.2574
    point 45.9648, -23.3378
    point 45.1123, -26.3607
    point 44.0252, -29.3073
    point 42.7103, -32.1596
    point 41.1756, -34.8999
    point 39.4307, -37.5114
    point 37.4862, -39.9779
    point 35.3543, -42.2843
    point 33.0479, -44.4162
    point 30.5814, -46.3607
    point 27.9699, -48.1056
    point 25.2296, -49.6403
    point 22.3773, -50.9552
    point 19.4307, -52.0423
    point 16.4078, -52.8948
    point 13.3274, -53.5075
    point 10.2084, -53.8767
    point 7.0700, -54.0000
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 4.5700, -16.5000
    point 9.5700, -11.5000
    point 4.5700, -11.5000
    point 9.5700, -16.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    addi r30, r3, -18784
    .endm

    .include "up-b-overlay.inc"
