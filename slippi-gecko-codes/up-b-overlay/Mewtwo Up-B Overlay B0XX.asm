    # To be inserted at 8016e76c
    # Mewtwo Up-B Overlay B0XX for vanilla NTSC 1.02 Melee / Slippi.
    # While Mewtwo is in the match on a legal stage, draws the Up-B
    # Sweetspot event's B0XX Angles at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 16

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 7.07 out (ECB front 3.14 + snap
    # 11, halved) and 14 down.
    shape GX_POINTS, 30, 16, 0x1C6C94FF    # B0XX: Mod X or Mod Y without B
    point 49.4426, -31.9545
    point 49.4426, 3.9545
    point 47.7098, -35.0456
    point 47.7098, 7.0456
    point 48.3280, -39.5406
    point 48.3280, 11.5406
    point 25.0245, -56.3726
    point 25.0245, 28.3726
    point 28.1156, -54.6398
    point 28.1156, 26.6398
    point 32.6106, -55.2580
    point 32.6106, 27.2580
    point 35.0422, -52.2059
    point 35.0422, 24.2059
    point 39.0626, -52.2657
    point 39.0626, 24.2657
    shape GX_POINTS, 30, 25, 0x30C8FFFF    # B0XX: Mod X or Mod Y, with B
    point 50.3200, -14.0000
    point 7.0700, -54.7500
    point 7.0700, 26.7500
    point 52.9333, -33.4762
    point 52.9333, 5.4762
    point 51.2486, -36.7204
    point 51.2486, 8.7204
    point 49.5939, -40.2648
    point 49.5939, 12.2648
    point 46.0643, -42.4196
    point 46.0643, 14.4196
    point 42.7557, -44.0879
    point 42.7557, 16.0879
    point 7.0700, -58.7500
    point 7.0700, 30.7500
    point 26.5462, -59.8633
    point 26.5462, 31.8633
    point 29.7904, -58.1786
    point 29.7904, 30.1786
    point 33.3348, -56.5239
    point 33.3348, 28.5239
    point 36.5773, -54.1801
    point 36.5773, 26.1801
    point 37.9054, -51.3962
    point 37.9054, 23.3962
    shape GX_POINTS, 30, 5, 0xFFFFFFFF    # B0XX: no modifier
    point 57.0700, -14.0000
    point 7.0700, -64.0000
    point 7.0700, 36.0000
    point 42.2832, -49.2132
    point 42.2832, 21.2132
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 4.5700, -16.5000
    point 9.5700, -11.5000
    point 4.5700, -11.5000
    point 9.5700, -16.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    li r3, 1
    .endm

    .include "up-b-overlay.inc"
