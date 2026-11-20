    # To be inserted at 8016e7d8
    # Fox Up-B Overlay B0XX for vanilla NTSC 1.02 Melee / Slippi.
    # While Fox is in the match on a legal stage, draws the Up-B
    # Sweetspot event's B0XX Angles at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 1

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 6.50 out (ECB front 2.00 + snap
    # 11, halved) and 13 down.
    # Snap: the top of the box, 4.5 above that.
    shape GX_POINTS, 30, 28, 0x30C8FFFF    # B0XX: Mod X or Mod Y, B held or not
    point 81.5162, -44.8562
    point 81.5162, 23.3562
    point 78.9770, -50.2739
    point 78.9770, 28.7739
    point 75.8401, -55.8277
    point 75.8401, 34.3277
    point -62.8401, -55.8277
    point 72.3637, -61.0024
    point 72.3637, 39.5024
    point -59.3637, -61.0024
    point 68.8086, -65.5347
    point 68.8086, 44.0347
    point -55.8086, -65.5347
    point 38.3562, -88.0162
    point 38.3562, 66.5162
    point -25.3562, -88.0162
    point 43.7739, -85.4770
    point 43.7739, 63.9770
    point -30.7739, -85.4770
    point 49.3277, -82.3401
    point 49.3277, 60.8401
    point -36.3277, -82.3401
    point 54.7406, -78.6894
    point 54.7406, 57.1894
    point -41.7406, -78.6894
    point 58.3488, -75.8805
    point 58.3488, 54.3805
    point -45.3488, -75.8805
    shape GX_POINTS, 30, 6, 0xFFFFFFFF    # B0XX: no modifier
    point 88.0000, -13.0000
    point 6.5000, -94.5000
    point 6.5000, 73.0000
    point 64.1292, -70.6292
    point 64.1292, 49.1292
    point -51.1292, -70.6292
    shape GX_LINES, 12, 2, 0xFFFFFFFF    # the snap: coming down, the up-B grabs here
    point 4.0000, -8.5000
    point 9.0000, -8.5000
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 4.0000, -15.5000
    point 9.0000, -10.5000
    point 4.0000, -10.5000
    point 9.0000, -15.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    li r3, 31
    .endm

    .include "up-b-overlay.inc"
