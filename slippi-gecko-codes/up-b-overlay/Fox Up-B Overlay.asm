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
    shape GX_LINESTRIP, 10, 81, 0x30C8FFFF    # ring at the full travel, 81.50
    point 6.5000, 68.5000
    point 12.8944, 68.2488
    point 19.2494, 67.4966
    point 25.5258, 66.2482
    point 31.6849, 64.5111
    point 37.6887, 62.2962
    point 43.5002, 59.6171
    point 49.0836, 56.4902
    point 54.4045, 52.9349
    point 59.4300, 48.9731
    point 64.1292, 44.6292
    point 68.4731, 39.9300
    point 72.4349, 34.9045
    point 75.9902, 29.5836
    point 79.1171, 24.0002
    point 81.7962, 18.1887
    point 84.0111, 12.1849
    point 85.7482, 6.0258
    point 86.9966, -0.2506
    point 87.7488, -6.6056
    point 88.0000, -13.0000
    point 87.7488, -19.3944
    point 86.9966, -25.7494
    point 85.7482, -32.0258
    point 84.0111, -38.1849
    point 81.7962, -44.1887
    point 79.1171, -50.0002
    point 75.9902, -55.5836
    point 72.4349, -60.9045
    point 68.4731, -65.9300
    point 64.1292, -70.6292
    point 59.4300, -74.9731
    point 54.4045, -78.9349
    point 49.0836, -82.4902
    point 43.5002, -85.6171
    point 37.6887, -88.2962
    point 31.6849, -90.5111
    point 25.5258, -92.2482
    point 19.2494, -93.4966
    point 12.8944, -94.2488
    point 6.5000, -94.5000
    point 0.1056, -94.2488
    point -6.2494, -93.4966
    point -12.5258, -92.2482
    point -18.6849, -90.5111
    point -24.6887, -88.2962
    point -30.5002, -85.6171
    point -36.0836, -82.4902
    point -41.4045, -78.9349
    point -46.4300, -74.9731
    point -51.1292, -70.6292
    point -55.4731, -65.9300
    point -59.4349, -60.9045
    point -62.9902, -55.5836
    point -66.1171, -50.0002
    point -68.7962, -44.1887
    point -71.0111, -38.1849
    point -72.7482, -32.0258
    point -73.9966, -25.7494
    point -74.7488, -19.3944
    point -75.0000, -13.0000
    point -74.7488, -6.6056
    point -73.9966, -0.2506
    point -72.7482, 6.0258
    point -71.0111, 12.1849
    point -68.7962, 18.1887
    point -66.1171, 24.0002
    point -62.9902, 29.5836
    point -59.4349, 34.9045
    point -55.4731, 39.9300
    point -51.1292, 44.6292
    point -46.4300, 48.9731
    point -41.4045, 52.9349
    point -36.0836, 56.4902
    point -30.5002, 59.6171
    point -24.6887, 62.2962
    point -18.6849, 64.5111
    point -12.5258, 66.2482
    point -6.2494, 67.4966
    point 0.1056, 68.2488
    point 6.5000, 68.5000
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
