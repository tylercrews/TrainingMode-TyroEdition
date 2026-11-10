    # To be inserted at 8016e898
    # Fox Up-B Overlay for vanilla NTSC 1.02 Melee / Slippi.
    # While Fox is in the match on a legal stage, draws the Up-B
    # Sweetspot event's targets at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 1

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 6.50 out (ECB front 2.00 + snap
    # 11, halved) and 13 down.
    shape GX_LINESTRIP, 10, 16, 0x30C8FFFF    # ring at the full travel 81.50, 73.16 to 16.84 degrees
    point 30.1071, 65.0061
    point 35.1651, 63.2926
    point 40.1000, 61.2516
    point 44.8906, 58.8917
    point 49.5164, 56.2231
    point 53.9575, 53.2574
    point 58.1948, 50.0071
    point 62.2102, 46.4864
    point 65.9864, 42.7102
    point 69.5071, 38.6948
    point 72.7574, 34.4575
    point 75.7231, 30.0164
    point 78.3917, 25.3906
    point 80.7516, 20.6000
    point 82.7926, 15.6651
    point 84.5061, 10.6071
    shape GX_LINESTRIP, 10, 16, 0x30C8FFFF    # ring at the full travel 81.50, -16.84 to -73.16 degrees
    point 84.5061, -36.6071
    point 82.7926, -41.6651
    point 80.7516, -46.6000
    point 78.3917, -51.3906
    point 75.7231, -56.0164
    point 72.7574, -60.4575
    point 69.5071, -64.6948
    point 65.9864, -68.7102
    point 62.2102, -72.4864
    point 58.1948, -76.0071
    point 53.9575, -79.2574
    point 49.5164, -82.2231
    point 44.8906, -84.8917
    point 40.1000, -87.2516
    point 35.1651, -89.2926
    point 30.1071, -91.0061
    shape GX_LINESTRIP, 10, 12, 0x30C8FFFF    # ring at the full travel 81.50, -106.84 to -150.00 degrees
    point -17.1071, -91.0061
    point -22.3898, -89.2078
    point -27.5371, -87.0522
    point -32.5247, -84.5495
    point -37.3294, -81.7112
    point -41.9287, -78.5509
    point -46.3008, -75.0832
    point -50.4255, -71.3245
    point -54.2832, -67.2923
    point -57.8560, -63.0056
    point -61.1271, -58.4844
    point -64.0811, -53.7500
    shape GX_LINESTRIP, 10, 4, 0x30C8FFFF    # ring through the 90 degree mark
    point 0.8148, 68.3015
    point 4.6036, 68.4780
    point 8.3964, 68.4780
    point 12.1852, 68.3015
    shape GX_LINESTRIP, 10, 4, 0x30C8FFFF    # ring through the 0 degree mark
    point 87.8015, -7.3148
    point 87.9780, -11.1036
    point 87.9780, -14.8964
    point 87.8015, -18.6852
    shape GX_LINESTRIP, 10, 4, 0x30C8FFFF    # ring through the -90 degree mark
    point 12.1852, -94.3015
    point 8.3964, -94.4780
    point 4.6036, -94.4780
    point 0.8148, -94.3015
    shape GX_LINES, 12, 6, 0xFFFFFFFF    # straight across, up and down: hold straight here
    point 6.5000, 62.5000
    point 6.5000, 74.5000
    point 82.0000, -13.0000
    point 94.0000, -13.0000
    point 6.5000, -88.5000
    point 6.5000, -100.5000
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 4.0000, -15.5000
    point 9.0000, -10.5000
    point 4.0000, -10.5000
    point 9.0000, -15.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    lis r3, -32697
    .endm

    .include "up-b-overlay.inc"
