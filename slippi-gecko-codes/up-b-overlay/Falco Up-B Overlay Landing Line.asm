    # To be inserted at 8016e758
    # Falco Up-B Overlay Landing Line for vanilla NTSC 1.02 Melee / Slippi.
    # While Falco is in the match on a legal stage, draws the Up-B
    # Sweetspot event's Landing Line at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 22

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Travel 60.10; the ECB at its end: bottom 2.5 up, sides
    # 2 out at 7.6 up. Each point crosses the ledge's x just clear of it.
    shape GX_LINESTRIP, 10, 16, 0xFF60E0FF    # Landing Line, -16.84 to -73.16 degrees: up-B from above it to land
    point 57.5235, -19.9084
    point 56.2599, -23.6383
    point 54.7548, -27.2774
    point 53.0146, -30.8101
    point 51.0467, -34.2213
    point 48.8597, -37.4962
    point 46.4629, -40.6209
    point 43.8666, -43.5820
    point 41.0820, -46.3666
    point 38.1209, -48.9629
    point 34.9962, -51.3597
    point 31.7213, -53.5467
    point 28.3101, -55.5146
    point 24.7774, -57.2548
    point 21.1383, -58.5369
    point 17.4084, -58.5148
    shape GX_LINESTRIP, 10, 3, 0xFF60E0FF    # Landing Line through level
    point 60.0634, -0.4025
    point 60.1000, -2.5000
    point 60.0634, -4.5975
    shape GX_LINES, 12, 2, 0xFF60E0FF    # straight across: from this high, hold straight to land
    point 54.1000, -2.5000
    point 66.1000, -2.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    addi r4, r4, -17484
    .endm

    .include "up-b-overlay.inc"
