    # To be inserted at 8016e89c
    # Falco Up-B Overlay for vanilla NTSC 1.02 Melee / Slippi.
    # While Falco is in the match on a legal stage, draws the Up-B
    # Sweetspot event's targets at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 22

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 6.65 out (ECB front 2.30 + snap
    # 11, halved) and 13 down.
    shape GX_LINESTRIP, 10, 16, 0x30C8FFFF    # ring at the full travel 60.10, 73.16 to 16.84 degrees
    point 24.0584, 44.5235
    point 27.7883, 43.2599
    point 31.4274, 41.7548
    point 34.9601, 40.0146
    point 38.3713, 38.0467
    point 41.6462, 35.8597
    point 44.7709, 33.4629
    point 47.7320, 30.8666
    point 50.5166, 28.0820
    point 53.1129, 25.1209
    point 55.5097, 21.9962
    point 57.6967, 18.7213
    point 59.6646, 15.3101
    point 61.4048, 11.7774
    point 62.9099, 8.1383
    point 64.1735, 4.4084
    shape GX_LINESTRIP, 10, 16, 0x30C8FFFF    # ring at the full travel 60.10, -16.84 to -73.16 degrees
    point 64.1735, -30.4084
    point 62.9099, -34.1383
    point 61.4048, -37.7774
    point 59.6646, -41.3101
    point 57.6967, -44.7213
    point 55.5097, -47.9962
    point 53.1129, -51.1209
    point 50.5166, -54.0820
    point 47.7320, -56.8666
    point 44.7709, -59.4629
    point 41.6462, -61.8597
    point 38.3713, -64.0467
    point 34.9601, -66.0146
    point 31.4274, -67.7548
    point 27.7883, -69.2599
    point 24.0584, -70.5235
    shape GX_LINESTRIP, 10, 12, 0x30C8FFFF    # ring at the full travel 60.10, -106.84 to -150.00 degrees
    point -10.7584, -70.5235
    point -14.6540, -69.1974
    point -18.4497, -67.6078
    point -22.1277, -65.7622
    point -25.6708, -63.6692
    point -29.0624, -61.3387
    point -32.2865, -58.7816
    point -35.3281, -56.0098
    point -38.1729, -53.0364
    point -40.8076, -49.8753
    point -43.2198, -46.5412
    point -45.3981, -43.0500
    shape GX_LINESTRIP, 10, 3, 0x30C8FFFF    # ring through the 90 degree mark
    point 4.5525, 47.0634
    point 6.6500, 47.1000
    point 8.7475, 47.0634
    shape GX_LINESTRIP, 10, 3, 0x30C8FFFF    # ring through the 0 degree mark
    point 66.7134, -10.9025
    point 66.7500, -13.0000
    point 66.7134, -15.0975
    shape GX_LINESTRIP, 10, 3, 0x30C8FFFF    # ring through the -90 degree mark
    point 8.7475, -73.0634
    point 6.6500, -73.1000
    point 4.5525, -73.0634
    shape GX_LINES, 12, 6, 0xFFFFFFFF    # straight across, up and down: hold straight here
    point 6.6500, 41.1000
    point 6.6500, 53.1000
    point 60.7500, -13.0000
    point 72.7500, -13.0000
    point 6.6500, -67.1000
    point 6.6500, -79.1000
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 4.1500, -15.5000
    point 9.1500, -10.5000
    point 4.1500, -10.5000
    point 9.1500, -15.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    addi r3, r3, -18784
    .endm

    .include "up-b-overlay.inc"
