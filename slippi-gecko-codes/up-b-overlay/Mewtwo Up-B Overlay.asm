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
    # Snap: the top of the box, 6 above that.
    shape GX_LINESTRIP, 10, 31, 0x1C6C94FF    # least tilt that aims, 73.16 to 16.84 degrees
    point 21.5097, 39.7139
    point 22.4458, 37.3868
    point 23.3716, 35.3869
    point 24.2860, 33.6247
    point 25.1881, 32.0386
    point 26.0769, 30.5847
    point 26.9515, 29.2309
    point 27.8109, 27.9533
    point 28.6542, 26.7340
    point 29.4806, 25.5588
    point 30.3620, 24.5190
    point 31.4149, 23.7384
    point 32.4417, 22.9237
    point 33.4412, 22.0759
    point 34.4124, 21.1958
    point 35.3543, 20.2843
    point 36.2658, 19.3424
    point 37.1459, 18.3712
    point 37.9937, 17.3717
    point 38.8084, 16.3449
    point 39.5890, 15.2920
    point 40.6288, 14.4106
    point 41.8040, 13.5842
    point 43.0233, 12.7409
    point 44.3009, 11.8815
    point 45.6547, 11.0069
    point 47.1086, 10.1181
    point 48.6947, 9.2160
    point 50.4569, 8.3016
    point 52.4568, 7.3758
    point 54.7839, 6.4397
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
    shape GX_LINESTRIP, 10, 3, 0x1C6C94FF    # least tilt through the 90 degree mark
    point 5.6740, 31.9756
    point 7.0700, 32.0000
    point 8.4660, 31.9756
    shape GX_LINESTRIP, 10, 3, 0x1C6C94FF    # least tilt through the 0 degree mark
    point 47.0456, -12.6040
    point 47.0700, -14.0000
    point 47.0456, -15.3960
    shape GX_LINESTRIP, 10, 3, 0x1C6C94FF    # least tilt through the -90 degree mark
    point 8.4660, -53.9756
    point 7.0700, -54.0000
    point 5.6740, -53.9756
    shape GX_LINESTRIP, 10, 16, 0x30C8FFFF    # ring at the full travel 50.00, 73.16 to 16.84 degrees, around the snap
    point 21.5529, 39.8565
    point 24.6559, 38.8053
    point 27.6835, 37.5531
    point 30.6225, 36.1053
    point 33.4604, 34.4682
    point 36.1850, 32.6487
    point 38.7846, 30.6547
    point 41.2480, 28.4947
    point 43.5647, 26.1780
    point 45.7247, 23.7146
    point 47.7187, 21.1150
    point 49.5382, 18.3904
    point 51.1753, 15.5525
    point 52.6231, 12.6135
    point 53.8753, 9.5859
    point 54.9265, 6.4829
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
    shape GX_LINESTRIP, 10, 3, 0x30C8FFFF    # ring through the 90 degree mark
    point 5.3250, 41.9695
    point 7.0700, 42.0000
    point 8.8150, 41.9695
    shape GX_LINESTRIP, 10, 3, 0x30C8FFFF    # ring through the 0 degree mark
    point 57.0395, -12.2550
    point 57.0700, -14.0000
    point 57.0395, -15.7450
    shape GX_LINESTRIP, 10, 3, 0x30C8FFFF    # ring through the -90 degree mark
    point 8.8150, -63.9695
    point 7.0700, -64.0000
    point 5.3250, -63.9695
    shape GX_LINES, 12, 6, 0xFFFFFFFF    # straight across, up and down: hold straight here
    point 7.0700, 32.0000
    point 7.0700, 42.0000
    point 47.0700, -14.0000
    point 57.0700, -14.0000
    point 7.0700, -54.0000
    point 7.0700, -64.0000
    shape GX_LINES, 12, 2, 0xFFFFFFFF    # the snap: coming down, the up-B grabs here
    point 4.5700, -8.0000
    point 9.5700, -8.0000
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
