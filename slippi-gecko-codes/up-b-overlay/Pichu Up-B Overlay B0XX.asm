    # To be inserted at 8016e760
    # Pichu Up-B Overlay B0XX for vanilla NTSC 1.02 Melee / Slippi.
    # While Pichu is in the match on a legal stage, draws the Up-B
    # Sweetspot event's B0XX Angles at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 23

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 5.50 out (ECB front 2.00 + snap
    # 9, halved) and 6 down.
    shape GX_POINTS, 30, 16, 0x1C6C94FF    # B0XX: Mod X or Mod Y without B
    point 34.6852, -18.3666
    point 34.6852, 6.3666
    point 33.4487, -20.4734
    point 33.4487, 8.4734
    point 34.3247, -23.8438
    point 34.3247, 11.8438
    point 17.8666, -35.1852
    point 17.8666, 23.1852
    point 19.9734, -33.9487
    point 19.9734, 21.9487
    point 23.3438, -34.8247
    point 23.3438, 22.8247
    point 24.9170, -32.5208
    point 24.9170, 20.5208
    point 28.0090, -32.9225
    point 28.0090, 20.9225
    shape GX_POINTS, 30, 25, 0x30C8FFFF    # B0XX: Mod X or Mod Y, with B
    point 34.7600, -6.0000
    point 5.5000, -33.0600
    point 5.5000, 21.0600
    point 37.7598, -19.6994
    point 37.7598, 7.6994
    point 36.5515, -21.9693
    point 36.5515, 9.9693
    point 35.4340, -24.4887
    point 35.4340, 12.4887
    point 32.7033, -25.8262
    point 32.7033, 13.8262
    point 30.1756, -26.8049
    point 30.1756, 14.8049
    point 5.5000, -36.5800
    point 5.5000, 24.5800
    point 19.1994, -38.2598
    point 19.1994, 26.2598
    point 21.4693, -37.0515
    point 21.4693, 25.0515
    point 23.9887, -35.9340
    point 23.9887, 23.9340
    point 26.2576, -34.2657
    point 26.2576, 22.2657
    point 27.0368, -32.1191
    point 27.0368, 20.1191
    shape GX_POINTS, 30, 5, 0xFFFFFFFF    # B0XX: no modifier
    point 40.7000, -6.0000
    point 5.5000, -41.2000
    point 5.5000, 29.2000
    point 30.2651, -30.7651
    point 30.2651, 18.7651
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 3.0000, -8.5000
    point 8.0000, -3.5000
    point 3.0000, -3.5000
    point 8.0000, -8.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    lis r3, -32734
    .endm

    .include "up-b-overlay.inc"
