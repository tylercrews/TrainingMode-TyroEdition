    # To be inserted at 8016e764
    # Pikachu Up-B Overlay B0XX for vanilla NTSC 1.02 Melee / Slippi.
    # While Pikachu is in the match on a legal stage, draws the Up-B
    # Sweetspot event's B0XX Angles at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 12

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 5.50 out (ECB front 2.00 + snap
    # 9, halved) and 11 down.
    shape GX_POINTS, 30, 16, 0x1C6C94FF    # B0XX: Mod X or Mod Y without B
    point 47.4122, -28.7594
    point 47.4122, 6.7594
    point 45.6958, -31.8157
    point 45.6958, 9.8157
    point 46.3328, -36.2775
    point 46.3328, 14.2775
    point 23.2594, -52.9122
    point 23.2594, 30.9122
    point 26.3157, -51.1958
    point 26.3157, 29.1958
    point 30.7775, -51.8328
    point 30.7775, 29.8328
    point 33.1768, -48.8025
    point 33.1768, 26.8025
    point 37.1719, -48.8821
    point 37.1719, 26.8821
    shape GX_POINTS, 30, 25, 0x30C8FFFF    # B0XX: Mod X or Mod Y, with B
    point 48.2500, -11.0000
    point 5.5000, -51.2500
    point 5.5000, 29.2500
    point 50.9031, -30.2808
    point 50.9031, 8.2808
    point 49.2340, -33.4918
    point 49.2340, 11.4918
    point 47.5985, -37.0020
    point 47.5985, 15.0020
    point 44.0902, -39.1251
    point 44.0902, 17.1251
    point 40.8034, -40.7656
    point 40.8034, 18.7656
    point 5.5000, -55.2500
    point 5.5000, 33.2500
    point 24.7808, -56.4031
    point 24.7808, 34.4031
    point 27.9918, -54.7340
    point 27.9918, 32.7340
    point 31.5020, -53.0985
    point 31.5020, 31.0985
    point 34.7113, -50.7771
    point 34.7113, 28.7771
    point 36.0174, -48.0104
    point 36.0174, 26.0104
    shape GX_POINTS, 30, 5, 0xFFFFFFFF    # B0XX: no modifier
    point 55.0000, -11.0000
    point 5.5000, -60.5000
    point 5.5000, 38.5000
    point 40.3597, -45.8597
    point 40.3597, 23.8597
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 3.0000, -13.5000
    point 8.0000, -8.5000
    point 3.0000, -8.5000
    point 8.0000, -13.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    addi r3, r3, 22356
    .endm

    .include "up-b-overlay.inc"
