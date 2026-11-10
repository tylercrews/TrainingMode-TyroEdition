    # To be inserted at 8016e91c
    # Pikachu Up-B Overlay for vanilla NTSC 1.02 Melee / Slippi.
    # While Pikachu is in the match on a legal stage, draws the Up-B
    # Sweetspot event's targets at both ledges. Written by
    # scripts/upb_overlay_numbers.py from the ISO; the drawing is shared
    # in up-b-overlay.inc.

    .set FIGHTER_KIND, 12

    # Points are (out from the right ledge, up from it); the left ledge
    # mirrors them.
    .macro character_data
    # Target: the ledge mid-grab-box, 5.50 out (ECB front 2.00 + snap
    # 9, halved) and 11 down.
    shape GX_LINESTRIP, 10, 31, 0x1C6C94FF    # least tilt that aims, 73.16 to 16.84 degrees
    point 19.7949, 36.2353
    point 20.7154, 33.9133
    point 21.6257, 31.9189
    point 22.5249, 30.1627
    point 23.4120, 28.5831
    point 24.2860, 27.1361
    point 25.1460, 25.7898
    point 25.9911, 24.5202
    point 26.8203, 23.3093
    point 27.6329, 22.1430
    point 28.5009, 21.1125
    point 29.5406, 20.3417
    point 30.5545, 19.5372
    point 31.5416, 18.6999
    point 32.5006, 17.8308
    point 33.4307, 16.9307
    point 34.3308, 16.0006
    point 35.1999, 15.0416
    point 36.0372, 14.0545
    point 36.8417, 13.0406
    point 37.6125, 12.0009
    point 38.6430, 11.1329
    point 39.8093, 10.3203
    point 41.0202, 9.4911
    point 42.2898, 8.6460
    point 43.6361, 7.7860
    point 45.0831, 6.9120
    point 46.6627, 6.0249
    point 48.4189, 5.1257
    point 50.4133, 4.2154
    point 52.7353, 3.2949
    shape GX_LINESTRIP, 10, 31, 0x1C6C94FF    # least tilt that aims, -16.84 to -73.16 degrees
    point 52.7353, -25.2949
    point 50.4133, -26.2154
    point 48.4189, -27.1257
    point 46.6627, -28.0249
    point 45.0831, -28.9120
    point 43.6361, -29.7860
    point 42.2898, -30.6460
    point 41.0202, -31.4911
    point 39.8093, -32.3203
    point 38.6430, -33.1329
    point 37.6125, -34.0009
    point 36.8417, -35.0406
    point 36.0372, -36.0545
    point 35.1999, -37.0416
    point 34.3308, -38.0006
    point 33.4307, -38.9307
    point 32.5006, -39.8308
    point 31.5416, -40.6999
    point 30.5545, -41.5372
    point 29.5406, -42.3417
    point 28.5009, -43.1125
    point 27.6329, -44.1430
    point 26.8203, -45.3093
    point 25.9911, -46.5202
    point 25.1460, -47.7898
    point 24.2860, -49.1361
    point 23.4120, -50.5831
    point 22.5249, -52.1627
    point 21.6257, -53.9189
    point 20.7154, -55.9133
    point 19.7949, -58.2353
    shape GX_LINESTRIP, 10, 3, 0x1C6C94FF    # least tilt through the 90 degree mark
    point 4.1215, 28.4759
    point 5.5000, 28.5000
    point 6.8785, 28.4759
    shape GX_LINESTRIP, 10, 3, 0x1C6C94FF    # least tilt through the 0 degree mark
    point 44.9759, -9.6215
    point 45.0000, -11.0000
    point 44.9759, -12.3785
    shape GX_LINESTRIP, 10, 3, 0x1C6C94FF    # least tilt through the -90 degree mark
    point 6.8785, -50.4759
    point 5.5000, -50.5000
    point 4.1215, -50.4759
    shape GX_LINESTRIP, 10, 16, 0x30C8FFFF    # ring at the full travel 49.50, 73.16 to 16.84 degrees
    point 19.8381, 36.3779
    point 22.9101, 35.3372
    point 25.9073, 34.0976
    point 28.8170, 32.6643
    point 31.6265, 31.0435
    point 34.3239, 29.2422
    point 36.8975, 27.2681
    point 39.3362, 25.1297
    point 41.6297, 22.8362
    point 43.7681, 20.3975
    point 45.7422, 17.8239
    point 47.5435, 15.1265
    point 49.1643, 12.3170
    point 50.5976, 9.4073
    point 51.8372, 6.4101
    point 52.8779, 3.3381
    shape GX_LINESTRIP, 10, 16, 0x30C8FFFF    # ring at the full travel 49.50, -16.84 to -73.16 degrees
    point 52.8779, -25.3381
    point 51.8372, -28.4101
    point 50.5976, -31.4073
    point 49.1643, -34.3170
    point 47.5435, -37.1265
    point 45.7422, -39.8239
    point 43.7681, -42.3975
    point 41.6297, -44.8362
    point 39.3362, -47.1297
    point 36.8975, -49.2681
    point 34.3239, -51.2422
    point 31.6265, -53.0435
    point 28.8170, -54.6643
    point 25.9073, -56.0976
    point 22.9101, -57.3372
    point 19.8381, -58.3779
    shape GX_LINESTRIP, 10, 3, 0x30C8FFFF    # ring through the 90 degree mark
    point 3.7725, 38.4698
    point 5.5000, 38.5000
    point 7.2275, 38.4698
    shape GX_LINESTRIP, 10, 3, 0x30C8FFFF    # ring through the 0 degree mark
    point 54.9698, -9.2725
    point 55.0000, -11.0000
    point 54.9698, -12.7275
    shape GX_LINESTRIP, 10, 3, 0x30C8FFFF    # ring through the -90 degree mark
    point 7.2275, -60.4698
    point 5.5000, -60.5000
    point 3.7725, -60.4698
    shape GX_LINES, 12, 6, 0xFFFFFFFF    # straight across, up and down: hold straight here
    point 5.5000, 28.5000
    point 5.5000, 38.5000
    point 45.0000, -11.0000
    point 55.0000, -11.0000
    point 5.5000, -50.5000
    point 5.5000, -60.5000
    shape GX_LINES, 12, 4, 0xFFFFFFFF    # the target X
    point 3.0000, -13.5000
    point 8.0000, -8.5000
    point 3.0000, -8.5000
    point 8.0000, -13.5000
    .long 0                          # end
    .endm

    .macro original_instruction
    lwz r0, 36(r1)
    .endm

    .include "up-b-overlay.inc"
