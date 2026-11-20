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
    # Snap: the top of the box, 5 above that.
    shape GX_LINESTRIP, 10, 31, 0x1C6C94FF    # least tilt that aims, 73.16 to 16.84 degrees
    point 19.7949, 41.2353
    point 20.7154, 38.9133
    point 21.6257, 36.9189
    point 22.5249, 35.1627
    point 23.4120, 33.5831
    point 24.2860, 32.1361
    point 25.1460, 30.7898
    point 25.9911, 29.5202
    point 26.8203, 28.3093
    point 27.6329, 27.1430
    point 28.5009, 26.1125
    point 29.5406, 25.3417
    point 30.5545, 24.5372
    point 31.5416, 23.6999
    point 32.5006, 22.8308
    point 33.4307, 21.9307
    point 34.3308, 21.0006
    point 35.1999, 20.0416
    point 36.0372, 19.0545
    point 36.8417, 18.0406
    point 37.6125, 17.0009
    point 38.6430, 16.1329
    point 39.8093, 15.3203
    point 41.0202, 14.4911
    point 42.2898, 13.6460
    point 43.6361, 12.7860
    point 45.0831, 11.9120
    point 46.6627, 11.0249
    point 48.4189, 10.1257
    point 50.4133, 9.2154
    point 52.7353, 8.2949
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
    point 4.1215, 33.4759
    point 5.5000, 33.5000
    point 6.8785, 33.4759
    shape GX_LINESTRIP, 10, 3, 0x1C6C94FF    # least tilt through the 0 degree mark
    point 44.9759, -9.6215
    point 45.0000, -11.0000
    point 44.9759, -12.3785
    shape GX_LINESTRIP, 10, 3, 0x1C6C94FF    # least tilt through the -90 degree mark
    point 6.8785, -50.4759
    point 5.5000, -50.5000
    point 4.1215, -50.4759
    shape GX_LINESTRIP, 10, 16, 0x30C8FFFF    # ring at the full travel 49.50, 73.16 to 16.84 degrees, around the snap
    point 19.8381, 41.3779
    point 22.9101, 40.3372
    point 25.9073, 39.0976
    point 28.8170, 37.6643
    point 31.6265, 36.0435
    point 34.3239, 34.2422
    point 36.8975, 32.2681
    point 39.3362, 30.1297
    point 41.6297, 27.8362
    point 43.7681, 25.3975
    point 45.7422, 22.8239
    point 47.5435, 20.1265
    point 49.1643, 17.3170
    point 50.5976, 14.4073
    point 51.8372, 11.4101
    point 52.8779, 8.3381
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
    point 3.7725, 43.4698
    point 5.5000, 43.5000
    point 7.2275, 43.4698
    shape GX_LINESTRIP, 10, 3, 0x30C8FFFF    # ring through the 0 degree mark
    point 54.9698, -9.2725
    point 55.0000, -11.0000
    point 54.9698, -12.7275
    shape GX_LINESTRIP, 10, 3, 0x30C8FFFF    # ring through the -90 degree mark
    point 7.2275, -60.4698
    point 5.5000, -60.5000
    point 3.7725, -60.4698
    shape GX_LINES, 12, 6, 0xFFFFFFFF    # straight across, up and down: hold straight here
    point 5.5000, 33.5000
    point 5.5000, 43.5000
    point 45.0000, -11.0000
    point 55.0000, -11.0000
    point 5.5000, -50.5000
    point 5.5000, -60.5000
    shape GX_LINES, 12, 2, 0xFFFFFFFF    # the snap: coming down, the up-B grabs here
    point 3.0000, -6.0000
    point 8.0000, -6.0000
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
