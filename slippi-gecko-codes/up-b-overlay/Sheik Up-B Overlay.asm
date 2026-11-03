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
    shape GX_LINESTRIP, 10, 41, 0x30C8FFFF    # ring at the full travel, 40.00
    point 7.0350, 25.0000
    point 10.1734, 24.8767
    point 13.2924, 24.5075
    point 16.3728, 23.8948
    point 19.3957, 23.0423
    point 22.3423, 21.9552
    point 25.1946, 20.6403
    point 27.9349, 19.1056
    point 30.5464, 17.3607
    point 33.0129, 15.4162
    point 35.3193, 13.2843
    point 37.4512, 10.9779
    point 39.3957, 8.5114
    point 41.1406, 5.8999
    point 42.6753, 3.1596
    point 43.9902, 0.3073
    point 45.0773, -2.6393
    point 45.9298, -5.6622
    point 46.5425, -8.7426
    point 46.9117, -11.8616
    point 47.0350, -15.0000
    point 46.9117, -18.1384
    point 46.5425, -21.2574
    point 45.9298, -24.3378
    point 45.0773, -27.3607
    point 43.9902, -30.3073
    point 42.6753, -33.1596
    point 41.1406, -35.8999
    point 39.3957, -38.5114
    point 37.4512, -40.9779
    point 35.3193, -43.2843
    point 33.0129, -45.4162
    point 30.5464, -47.3607
    point 27.9349, -49.1056
    point 25.1946, -50.6403
    point 22.3423, -51.9552
    point 19.3957, -53.0423
    point 16.3728, -53.8948
    point 13.2924, -54.5075
    point 10.1734, -54.8767
    point 7.0350, -55.0000
    shape GX_LINESTRIP, 10, 41, 0x1C6C94FF    # dim ring at the least tilt, 30.00
    point 7.0350, 15.0000
    point 9.3888, 14.9075
    point 11.7280, 14.6307
    point 14.0384, 14.1711
    point 16.3055, 13.5317
    point 18.5155, 12.7164
    point 20.6547, 11.7302
    point 22.7100, 10.5792
    point 24.6686, 9.2705
    point 26.5184, 7.8122
    point 28.2482, 6.2132
    point 29.8472, 4.4834
    point 31.3055, 2.6336
    point 32.6142, 0.6750
    point 33.7652, -1.3803
    point 34.7514, -3.5195
    point 35.5667, -5.7295
    point 36.2061, -7.9966
    point 36.6657, -10.3070
    point 36.9425, -12.6462
    point 37.0350, -15.0000
    point 36.9425, -17.3538
    point 36.6657, -19.6930
    point 36.2061, -22.0034
    point 35.5667, -24.2705
    point 34.7514, -26.4805
    point 33.7652, -28.6197
    point 32.6142, -30.6750
    point 31.3055, -32.6336
    point 29.8472, -34.4834
    point 28.2482, -36.2132
    point 26.5184, -37.8122
    point 24.6686, -39.2705
    point 22.7100, -40.5792
    point 20.6547, -41.7302
    point 18.5155, -42.7164
    point 16.3055, -43.5317
    point 14.0384, -44.1711
    point 11.7280, -44.6307
    point 9.3888, -44.9075
    point 7.0350, -45.0000
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
