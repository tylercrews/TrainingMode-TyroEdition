    # To be inserted at 8016e7f4
    # Pichu Up-B Overlay for vanilla NTSC 1.02 Melee / Slippi.
    # While Pichu is in the match on a legal stage, draws the Up-B
    # Sweetspot event's targets at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 23

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 5.50 out (ECB front 2.00 + snap
    # 9, halved) and 6 down.
    shape GX_LINESTRIP, 10, 31, 0x1C6C94FF    # least tilt that aims, 73.16 to 16.84 degrees
    point 15.6580, 27.5655
    point 16.2071, 25.6057
    point 16.7502, 23.9427
    point 17.2867, 22.4979
    point 17.8159, 21.2166
    point 18.3374, 20.0603
    point 18.8505, 19.0006
    point 19.3547, 18.0164
    point 19.8494, 17.0915
    point 20.3342, 16.2135
    point 20.8727, 15.4625
    point 21.5676, 14.9473
    point 22.2453, 14.4097
    point 22.9050, 13.8501
    point 23.5460, 13.2692
    point 24.1676, 12.6676
    point 24.7692, 12.0460
    point 25.3501, 11.4050
    point 25.9097, 10.7453
    point 26.4473, 10.0676
    point 26.9625, 9.3727
    point 27.7135, 8.8342
    point 28.5915, 8.3494
    point 29.5164, 7.8547
    point 30.5006, 7.3505
    point 31.5603, 6.8374
    point 32.7166, 6.3159
    point 33.9979, 5.7867
    point 35.4427, 5.2502
    point 37.1057, 4.7071
    point 39.0655, 4.1580
    shape GX_LINESTRIP, 10, 31, 0x1C6C94FF    # least tilt that aims, -16.84 to -73.16 degrees
    point 39.0655, -16.1580
    point 37.1057, -16.7071
    point 35.4427, -17.2502
    point 33.9979, -17.7867
    point 32.7166, -18.3159
    point 31.5603, -18.8374
    point 30.5006, -19.3505
    point 29.5164, -19.8547
    point 28.5915, -20.3494
    point 27.7135, -20.8342
    point 26.9625, -21.3727
    point 26.4473, -22.0676
    point 25.9097, -22.7453
    point 25.3501, -23.4050
    point 24.7692, -24.0460
    point 24.1676, -24.6676
    point 23.5460, -25.2692
    point 22.9050, -25.8501
    point 22.2453, -26.4097
    point 21.5676, -26.9473
    point 20.8727, -27.4625
    point 20.3342, -28.2135
    point 19.8494, -29.0915
    point 19.3547, -30.0164
    point 18.8505, -31.0006
    point 18.3374, -32.0603
    point 17.8159, -33.2166
    point 17.2867, -34.4979
    point 16.7502, -35.9427
    point 16.2071, -37.6057
    point 15.6580, -39.5655
    shape GX_LINESTRIP, 10, 16, 0x30C8FFFF    # ring at the full travel 35.20, 73.16 to 16.84 degrees
    point 15.6960, 27.6910
    point 17.8805, 26.9509
    point 20.0119, 26.0694
    point 22.0810, 25.0501
    point 24.0789, 23.8976
    point 25.9970, 22.6167
    point 27.8271, 21.2129
    point 29.5613, 19.6923
    point 31.1923, 18.0613
    point 32.7129, 16.3271
    point 34.1167, 14.4970
    point 35.3976, 12.5789
    point 36.5501, 10.5810
    point 37.5694, 8.5119
    point 38.4509, 6.3805
    point 39.1910, 4.1960
    shape GX_LINESTRIP, 10, 16, 0x30C8FFFF    # ring at the full travel 35.20, -16.84 to -73.16 degrees
    point 39.1910, -16.1960
    point 38.4509, -18.3805
    point 37.5694, -20.5119
    point 36.5501, -22.5810
    point 35.3976, -24.5789
    point 34.1167, -26.4970
    point 32.7129, -28.3271
    point 31.1923, -30.0613
    point 29.5613, -31.6923
    point 27.8271, -33.2129
    point 25.9970, -34.6167
    point 24.0789, -35.8976
    point 22.0810, -37.0501
    point 20.0119, -38.0694
    point 17.8805, -38.9509
    point 15.6960, -39.6910
    shape GX_LINES, 12, 6, 0xFFFFFFFF    # straight across, up and down: hold straight here
    point 5.5000, 20.4000
    point 5.5000, 29.2000
    point 31.9000, -6.0000
    point 40.7000, -6.0000
    point 5.5000, -32.4000
    point 5.5000, -41.2000
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 3.0000, -8.5000
    point 8.0000, -3.5000
    point 3.0000, -3.5000
    point 8.0000, -8.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    lhz r3, 14(r31)
    .endm

    .include "up-b-overlay.inc"
