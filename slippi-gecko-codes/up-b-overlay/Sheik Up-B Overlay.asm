    # To be inserted at 8016e8a4
    # Sheik Up-B Overlay for vanilla NTSC 1.02 Melee / Slippi.
    # While Sheik is in the match on a legal stage, draws the Up-B
    # Sweetspot event's targets at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 7

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 7.04 out (ECB front 3.07 + snap
    # 11, halved) and 15 down.
    shape GX_LINESTRIP, 10, 31, 0x1C6C94FF    # least tilt that aims, 73.16 to 16.84 degrees
    point 18.5782, 23.1426
    point 19.2022, 20.9156
    point 19.8194, 19.0258
    point 20.4290, 17.3839
    point 21.0304, 15.9280
    point 21.6229, 14.6140
    point 22.2060, 13.4098
    point 22.7789, 12.2913
    point 23.3412, 11.2403
    point 23.8920, 10.2427
    point 24.5040, 9.3892
    point 25.2937, 8.8038
    point 26.0638, 8.1928
    point 26.8134, 7.5569
    point 27.5418, 6.8968
    point 28.2482, 6.2132
    point 28.9318, 5.5068
    point 29.5919, 4.7784
    point 30.2278, 4.0288
    point 30.8388, 3.2587
    point 31.4242, 2.4690
    point 32.2777, 1.8570
    point 33.2753, 1.3062
    point 34.3263, 0.7439
    point 35.4448, 0.1710
    point 36.6490, -0.4121
    point 37.9630, -1.0046
    point 39.4189, -1.6060
    point 41.0608, -2.2156
    point 42.9506, -2.8328
    point 45.1776, -3.4568
    shape GX_LINESTRIP, 10, 31, 0x1C6C94FF    # least tilt that aims, -16.84 to -73.16 degrees
    point 45.1776, -26.5432
    point 42.9506, -27.1672
    point 41.0608, -27.7844
    point 39.4189, -28.3940
    point 37.9630, -28.9954
    point 36.6490, -29.5879
    point 35.4448, -30.1710
    point 34.3263, -30.7439
    point 33.2753, -31.3062
    point 32.2777, -31.8570
    point 31.4242, -32.4690
    point 30.8388, -33.2587
    point 30.2278, -34.0288
    point 29.5919, -34.7784
    point 28.9318, -35.5068
    point 28.2482, -36.2132
    point 27.5418, -36.8968
    point 26.8134, -37.5569
    point 26.0638, -38.1928
    point 25.2937, -38.8038
    point 24.5040, -39.3892
    point 23.8920, -40.2427
    point 23.3412, -41.2403
    point 22.7789, -42.2913
    point 22.2060, -43.4098
    point 21.6229, -44.6140
    point 21.0304, -45.9280
    point 20.4290, -47.3839
    point 19.8194, -49.0258
    point 19.2022, -50.9156
    point 18.5782, -53.1426
    shape GX_LINESTRIP, 10, 4, 0x1C6C94FF    # least tilt through the 90 degree mark
    point 4.9423, 14.9269
    point 6.3369, 14.9919
    point 7.7331, 14.9919
    point 9.1277, 14.9269
    shape GX_LINESTRIP, 10, 4, 0x1C6C94FF    # least tilt through the 0 degree mark
    point 36.9619, -12.9073
    point 37.0269, -14.3019
    point 37.0269, -15.6981
    point 36.9619, -17.0927
    shape GX_LINESTRIP, 10, 4, 0x1C6C94FF    # least tilt through the -90 degree mark
    point 9.1277, -44.9269
    point 7.7331, -44.9919
    point 6.3369, -44.9919
    point 4.9423, -44.9269
    shape GX_LINESTRIP, 10, 16, 0x30C8FFFF    # ring at the full travel 40.00, 73.16 to 16.84 degrees
    point 18.6213, 23.2852
    point 21.1037, 22.4442
    point 23.5258, 21.4425
    point 25.8770, 20.2843
    point 28.1473, 18.9745
    point 30.3270, 17.5190
    point 32.4067, 15.9237
    point 34.3774, 14.1958
    point 36.2308, 12.3424
    point 37.9587, 10.3717
    point 39.5540, 8.2920
    point 41.0095, 6.1123
    point 42.3193, 3.8420
    point 43.4775, 1.4908
    point 44.4792, -0.9313
    point 45.3202, -3.4137
    shape GX_LINESTRIP, 10, 16, 0x30C8FFFF    # ring at the full travel 40.00, -16.84 to -73.16 degrees
    point 45.3202, -26.5863
    point 44.4792, -29.0687
    point 43.4775, -31.4908
    point 42.3193, -33.8420
    point 41.0095, -36.1123
    point 39.5540, -38.2920
    point 37.9587, -40.3717
    point 36.2308, -42.3424
    point 34.3774, -44.1958
    point 32.4067, -45.9237
    point 30.3270, -47.5190
    point 28.1473, -48.9745
    point 25.8770, -50.2843
    point 23.5258, -51.4425
    point 21.1037, -52.4442
    point 18.6213, -53.2852
    shape GX_LINESTRIP, 10, 4, 0x30C8FFFF    # ring through the 90 degree mark
    point 4.2447, 24.9026
    point 6.1042, 24.9892
    point 7.9658, 24.9892
    point 9.8253, 24.9026
    shape GX_LINESTRIP, 10, 4, 0x30C8FFFF    # ring through the 0 degree mark
    point 46.9376, -12.2097
    point 47.0242, -14.0692
    point 47.0242, -15.9308
    point 46.9376, -17.7903
    shape GX_LINESTRIP, 10, 4, 0x30C8FFFF    # ring through the -90 degree mark
    point 9.8253, -54.9026
    point 7.9658, -54.9892
    point 6.1042, -54.9892
    point 4.2447, -54.9026
    shape GX_LINES, 12, 6, 0xFFFFFFFF    # straight across, up and down: hold straight here
    point 7.0350, 15.0000
    point 7.0350, 25.0000
    point 37.0350, -15.0000
    point 47.0350, -15.0000
    point 7.0350, -45.0000
    point 7.0350, -55.0000
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 4.5350, -17.5000
    point 9.5350, -12.5000
    point 4.5350, -12.5000
    point 9.5350, -17.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    cmplwi r12, 0
    .endm

    .include "up-b-overlay.inc"
