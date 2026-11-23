    # To be inserted at 8016e754
    # Fox Up-B Overlay Landing Line for vanilla NTSC 1.02 Melee / Slippi.
    # While Fox is in the match on a legal stage, draws the Up-B
    # Sweetspot event's Landing Line at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 1

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Travel 81.50; the ECB at its end: bottom 2.2 up, sides
    # 2 out at 6.8 up. Each point crosses the ledge's x just clear of it.
    shape GX_LINESTRIP, 10, 16, 0xFF60E0FF    # Landing Line, -16.84 to -73.16 degrees: up-B from above it to land
    point 78.0061, -25.8071
    point 76.2926, -30.8651
    point 74.2516, -35.8000
    point 71.8917, -40.5906
    point 69.2231, -45.2164
    point 66.2574, -49.6575
    point 63.0071, -53.8948
    point 59.4864, -57.9102
    point 55.7102, -61.6864
    point 51.6948, -65.2071
    point 47.4575, -68.4574
    point 43.0164, -71.4231
    point 38.3906, -74.0917
    point 33.6000, -76.4516
    point 28.6651, -77.7696
    point 23.6071, -78.1974
    shape GX_LINESTRIP, 10, 3, 0xFF60E0FF    # Landing Line through level
    point 81.4504, 0.6443
    point 81.5000, -2.2000
    point 81.4504, -5.0443
    shape GX_LINES, 12, 2, 0xFF60E0FF    # straight across: from this high, hold straight to land
    point 75.5000, -2.2000
    point 87.5000, -2.2000
    .long 0                          # end
    .endm

    .macro original_instruction
    addi r3, r3, -17676
    .endm

    .include "up-b-overlay.inc"
