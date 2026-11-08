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
    shape GX_LINESTRIP, 10, 41, 0x30C8FFFF    # ring at the full travel, 35.20
    point 5.5000, 29.2000
    point 8.2618, 29.0915
    point 11.0065, 28.7666
    point 13.7173, 28.2274
    point 16.3774, 27.4772
    point 18.9705, 26.5206
    point 21.4805, 25.3634
    point 23.8919, 24.0129
    point 26.1900, 22.4774
    point 28.3606, 20.7663
    point 30.3902, 18.8902
    point 32.2663, 16.8606
    point 33.9774, 14.6900
    point 35.5129, 12.3919
    point 36.8634, 9.9805
    point 38.0206, 7.4705
    point 38.9772, 4.8774
    point 39.7274, 2.2173
    point 40.2666, -0.4935
    point 40.5915, -3.2382
    point 40.7000, -6.0000
    point 40.5915, -8.7618
    point 40.2666, -11.5065
    point 39.7274, -14.2173
    point 38.9772, -16.8774
    point 38.0206, -19.4705
    point 36.8634, -21.9805
    point 35.5129, -24.3919
    point 33.9774, -26.6900
    point 32.2663, -28.8606
    point 30.3902, -30.8902
    point 28.3606, -32.7663
    point 26.1900, -34.4774
    point 23.8919, -36.0129
    point 21.4805, -37.3634
    point 18.9705, -38.5206
    point 16.3774, -39.4772
    point 13.7173, -40.2274
    point 11.0065, -40.7666
    point 8.2618, -41.0915
    point 5.5000, -41.2000
    shape GX_LINESTRIP, 10, 41, 0x1C6C94FF    # dim ring at the least tilt, 26.40
    point 5.5000, 20.4000
    point 7.5713, 20.3186
    point 9.6299, 20.0750
    point 11.6630, 19.6706
    point 13.6580, 19.1079
    point 15.6028, 18.3904
    point 17.4853, 17.5226
    point 19.2940, 16.5097
    point 21.0175, 15.3580
    point 22.6454, 14.0747
    point 24.1676, 12.6676
    point 25.5747, 11.1454
    point 26.8580, 9.5175
    point 28.0097, 7.7940
    point 29.0226, 5.9853
    point 29.8904, 4.1028
    point 30.6079, 2.1580
    point 31.1706, 0.1630
    point 31.5750, -1.8701
    point 31.8186, -3.9287
    point 31.9000, -6.0000
    point 31.8186, -8.0713
    point 31.5750, -10.1299
    point 31.1706, -12.1630
    point 30.6079, -14.1580
    point 29.8904, -16.1028
    point 29.0226, -17.9853
    point 28.0097, -19.7940
    point 26.8580, -21.5175
    point 25.5747, -23.1454
    point 24.1676, -24.6676
    point 22.6454, -26.0747
    point 21.0175, -27.3580
    point 19.2940, -28.5097
    point 17.4853, -29.5226
    point 15.6028, -30.3904
    point 13.6580, -31.1079
    point 11.6630, -31.6706
    point 9.6299, -32.0750
    point 7.5713, -32.3186
    point 5.5000, -32.4000
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
