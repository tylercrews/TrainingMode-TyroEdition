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
    shape GX_LINESTRIP, 10, 41, 0x30C8FFFF    # ring at the full travel, 84.00
    point 6.5000, 74.0000
    point 13.0906, 73.7411
    point 19.6405, 72.9658
    point 26.1094, 71.6791
    point 32.4574, 69.8887
    point 38.6454, 67.6059
    point 44.6352, 64.8445
    point 50.3899, 61.6218
    point 55.8740, 57.9574
    point 61.0536, 53.8741
    point 65.8970, 49.3970
    point 70.3741, 44.5536
    point 74.4574, 39.3740
    point 78.1218, 33.8899
    point 81.3445, 28.1352
    point 84.1059, 22.1454
    point 86.3887, 15.9574
    point 88.1791, 9.6094
    point 89.4658, 3.1405
    point 90.2411, -3.4094
    point 90.5000, -10.0000
    point 90.2411, -16.5906
    point 89.4658, -23.1405
    point 88.1791, -29.6094
    point 86.3887, -35.9574
    point 84.1059, -42.1454
    point 81.3445, -48.1352
    point 78.1218, -53.8899
    point 74.4574, -59.3740
    point 70.3741, -64.5536
    point 65.8970, -69.3970
    point 61.0536, -73.8741
    point 55.8740, -77.9574
    point 50.3899, -81.6218
    point 44.6352, -84.8445
    point 38.6454, -87.6059
    point 32.4574, -89.8887
    point 26.1094, -91.6791
    point 19.6405, -92.9658
    point 13.0906, -93.7411
    point 6.5000, -94.0000
    shape GX_LINESTRIP, 10, 41, 0x1C6C94FF    # dim ring at the least tilt, 62.00
    point 6.5000, 52.0000
    point 11.3645, 51.8089
    point 16.1989, 51.2367
    point 20.9736, 50.2869
    point 25.6591, 48.9655
    point 30.2264, 47.2805
    point 34.6474, 45.2424
    point 38.8949, 42.8637
    point 42.9427, 40.1591
    point 46.7658, 37.1452
    point 50.3406, 33.8406
    point 53.6452, 30.2658
    point 56.6591, 26.4427
    point 59.3637, 22.3949
    point 61.7424, 18.1474
    point 63.7805, 13.7264
    point 65.4655, 9.1591
    point 66.7869, 4.4736
    point 67.7367, -0.3011
    point 68.3089, -5.1355
    point 68.5000, -10.0000
    point 68.3089, -14.8645
    point 67.7367, -19.6989
    point 66.7869, -24.4736
    point 65.4655, -29.1591
    point 63.7805, -33.7264
    point 61.7424, -38.1474
    point 59.3637, -42.3949
    point 56.6591, -46.4427
    point 53.6452, -50.2658
    point 50.3406, -53.8406
    point 46.7658, -57.1452
    point 42.9427, -60.1591
    point 38.8949, -62.8637
    point 34.6474, -65.2424
    point 30.2264, -67.2805
    point 25.6591, -68.9655
    point 20.9736, -70.2869
    point 16.1989, -71.2367
    point 11.3645, -71.8089
    point 6.5000, -72.0000
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
