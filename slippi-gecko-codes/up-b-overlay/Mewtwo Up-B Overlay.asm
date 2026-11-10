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
    shape GX_LINESTRIP, 10, 31, 0x1C6C94FF    # least tilt that aims, 73.16 to 16.84 degrees
    point 21.5097, 33.7139
    point 22.4458, 31.3868
    point 23.3716, 29.3869
    point 24.2860, 27.6247
    point 25.1881, 26.0386
    point 26.0769, 24.5847
    point 26.9515, 23.2309
    point 27.8109, 21.9533
    point 28.6542, 20.7340
    point 29.4806, 19.5588
    point 30.3620, 18.5190
    point 31.4149, 17.7384
    point 32.4417, 16.9237
    point 33.4412, 16.0759
    point 34.4124, 15.1958
    point 35.3543, 14.2843
    point 36.2658, 13.3424
    point 37.1459, 12.3712
    point 37.9937, 11.3717
    point 38.8084, 10.3449
    point 39.5890, 9.2920
    point 40.6288, 8.4106
    point 41.8040, 7.5842
    point 43.0233, 6.7409
    point 44.3009, 5.8815
    point 45.6547, 5.0069
    point 47.1086, 4.1181
    point 48.6947, 3.2160
    point 50.4569, 2.3016
    point 52.4568, 1.3758
    point 54.7839, 0.4397
    shape GX_LINESTRIP, 10, 31, 0x1C6C94FF    # least tilt that aims, -16.84 to -73.16 degrees
    point 54.7839, -28.4397
    point 52.4568, -29.3758
    point 50.4569, -30.3016
    point 48.6947, -31.2160
    point 47.1086, -32.1181
    point 45.6547, -33.0069
    point 44.3009, -33.8815
    point 43.0233, -34.7409
    point 41.8040, -35.5842
    point 40.6288, -36.4106
    point 39.5890, -37.2920
    point 38.8084, -38.3449
    point 37.9937, -39.3717
    point 37.1459, -40.3712
    point 36.2658, -41.3424
    point 35.3543, -42.2843
    point 34.4124, -43.1958
    point 33.4412, -44.0759
    point 32.4417, -44.9237
    point 31.4149, -45.7384
    point 30.3620, -46.5190
    point 29.4806, -47.5588
    point 28.6542, -48.7340
    point 27.8109, -49.9533
    point 26.9515, -51.2309
    point 26.0769, -52.5847
    point 25.1881, -54.0386
    point 24.2860, -55.6247
    point 23.3716, -57.3869
    point 22.4458, -59.3868
    point 21.5097, -61.7139
    shape GX_LINESTRIP, 10, 4, 0x1C6C94FF    # least tilt through the 90 degree mark
    point 4.2797, 25.9026
    point 6.1392, 25.9892
    point 8.0008, 25.9892
    point 9.8603, 25.9026
    shape GX_LINESTRIP, 10, 4, 0x1C6C94FF    # least tilt through the 0 degree mark
    point 46.9726, -11.2097
    point 47.0592, -13.0692
    point 47.0592, -14.9308
    point 46.9726, -16.7903
    shape GX_LINESTRIP, 10, 4, 0x1C6C94FF    # least tilt through the -90 degree mark
    point 9.8603, -53.9026
    point 8.0008, -53.9892
    point 6.1392, -53.9892
    point 4.2797, -53.9026
    shape GX_LINESTRIP, 10, 16, 0x30C8FFFF    # ring at the full travel 50.00, 73.16 to 16.84 degrees
    point 21.5529, 33.8565
    point 24.6559, 32.8053
    point 27.6835, 31.5531
    point 30.6225, 30.1053
    point 33.4604, 28.4682
    point 36.1850, 26.6487
    point 38.7846, 24.6547
    point 41.2480, 22.4947
    point 43.5647, 20.1780
    point 45.7247, 17.7146
    point 47.7187, 15.1150
    point 49.5382, 12.3904
    point 51.1753, 9.5525
    point 52.6231, 6.6135
    point 53.8753, 3.5859
    point 54.9265, 0.4829
    shape GX_LINESTRIP, 10, 16, 0x30C8FFFF    # ring at the full travel 50.00, -16.84 to -73.16 degrees
    point 54.9265, -28.4829
    point 53.8753, -31.5859
    point 52.6231, -34.6135
    point 51.1753, -37.5525
    point 49.5382, -40.3904
    point 47.7187, -43.1150
    point 45.7247, -45.7146
    point 43.5647, -48.1780
    point 41.2480, -50.4947
    point 38.7846, -52.6547
    point 36.1850, -54.6487
    point 33.4604, -56.4682
    point 30.6225, -58.1053
    point 27.6835, -59.5531
    point 24.6559, -60.8053
    point 21.5529, -61.8565
    shape GX_LINESTRIP, 10, 4, 0x30C8FFFF    # ring through the 90 degree mark
    point 3.5822, 35.8782
    point 5.9066, 35.9865
    point 8.2334, 35.9865
    point 10.5578, 35.8782
    shape GX_LINESTRIP, 10, 4, 0x30C8FFFF    # ring through the 0 degree mark
    point 56.9482, -10.5122
    point 57.0565, -12.8366
    point 57.0565, -15.1634
    point 56.9482, -17.4878
    shape GX_LINESTRIP, 10, 4, 0x30C8FFFF    # ring through the -90 degree mark
    point 10.5578, -63.8782
    point 8.2334, -63.9865
    point 5.9066, -63.9865
    point 3.5822, -63.8782
    shape GX_LINES, 12, 6, 0xFFFFFFFF    # straight across, up and down: hold straight here
    point 7.0700, 26.0000
    point 7.0700, 36.0000
    point 47.0700, -14.0000
    point 57.0700, -14.0000
    point 7.0700, -54.0000
    point 7.0700, -64.0000
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
