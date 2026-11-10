    # To be inserted at 8016e7d0
    # Falco Up-B Overlay B0XX for vanilla NTSC 1.02 Melee / Slippi.
    # While Falco is in the match on a legal stage, draws the Up-B
    # Sweetspot event's B0XX Angles at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 22

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 6.65 out (ECB front 2.30 + snap
    # 11, halved) and 13 down.
    shape GX_POINTS, 30, 28, 0x30C8FFFF    # B0XX: Mod X or Mod Y, B held or not
    point 61.9687, -36.4915
    point 61.9687, 10.4915
    point 60.0962, -40.4866
    point 60.0962, 14.4866
    point 57.7829, -44.5821
    point 57.7829, 18.5821
    point -44.4829, -44.5821
    point 55.2194, -48.3980
    point 55.2194, 22.3980
    point -41.9194, -48.3980
    point 52.5978, -51.7403
    point 52.5978, 25.7403
    point -39.2978, -51.7403
    point 30.1415, -68.3187
    point 30.1415, 42.3187
    point -16.8415, -68.3187
    point 34.1366, -66.4462
    point 34.1366, 40.4462
    point -20.8366, -66.4462
    point 38.2321, -64.1329
    point 38.2321, 38.1329
    point -24.9321, -64.1329
    point 42.2237, -61.4408
    point 42.2237, 35.4408
    point -28.9237, -61.4408
    point 44.8845, -59.3695
    point 44.8845, 33.3695
    point -31.5845, -59.3695
    shape GX_POINTS, 30, 6, 0xFFFFFFFF    # B0XX: no modifier
    point 66.7500, -13.0000
    point 6.6500, -73.1000
    point 6.6500, 47.1000
    point 49.1471, -55.4971
    point 49.1471, 29.4971
    point -35.8471, -55.4971
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 4.1500, -15.5000
    point 9.1500, -10.5000
    point 4.1500, -10.5000
    point 9.1500, -15.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    li r3, 0
    .endm

    .include "up-b-overlay.inc"
