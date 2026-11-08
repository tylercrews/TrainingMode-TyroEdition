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
    shape GX_LINESTRIP, 10, 61, 0x30C8FFFF    # ring at the full travel, 81.50
    point 6.5000, 68.5000
    point 12.1852, 68.3015
    point 17.8426, 67.7069
    point 23.4448, 66.7191
    point 28.9645, 65.3429
    point 34.3746, 63.5850
    point 39.6490, 61.4540
    point 44.7619, 58.9602
    point 49.6884, 56.1159
    point 54.4045, 52.9349
    point 58.8872, 49.4326
    point 63.1147, 45.6262
    point 67.0663, 41.5342
    point 70.7229, 37.1764
    point 74.0666, 32.5742
    point 77.0811, 27.7500
    point 79.7517, 22.7273
    point 82.0655, 17.5304
    point 84.0111, 12.1849
    point 85.5791, 6.7166
    point 86.7619, 1.1523
    point 87.5536, -4.4809
    point 87.9504, -10.1557
    point 87.9504, -15.8443
    point 87.5536, -21.5191
    point 86.7619, -27.1523
    point 85.5791, -32.7166
    point 84.0111, -38.1849
    point 82.0655, -43.5304
    point 79.7517, -48.7273
    point 77.0811, -53.7500
    point 74.0666, -58.5742
    point 70.7229, -63.1764
    point 67.0663, -67.5342
    point 63.1147, -71.6262
    point 58.8872, -75.4326
    point 54.4045, -78.9349
    point 49.6884, -82.1159
    point 44.7619, -84.9602
    point 39.6490, -87.4540
    point 34.3746, -89.5850
    point 28.9645, -91.3429
    point 23.4448, -92.7191
    point 17.8426, -93.7069
    point 12.1852, -94.3015
    point 6.5000, -94.5000
    point 0.8148, -94.3015
    point -4.8426, -93.7069
    point -10.4448, -92.7191
    point -15.9645, -91.3429
    point -21.3746, -89.5850
    point -26.6490, -87.4540
    point -31.7619, -84.9602
    point -36.6884, -82.1159
    point -41.4045, -78.9349
    point -45.8872, -75.4326
    point -50.1147, -71.6262
    point -54.0663, -67.5342
    point -57.7229, -63.1764
    point -61.0666, -58.5742
    point -64.0811, -53.7500
    shape GX_LINES, 12, 2, 0xFFFFFFFF    # tick: hold straight toward the stage from here
    point 82.0000, -13.0000
    point 94.0000, -13.0000
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
