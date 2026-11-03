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
    shape GX_LINESTRIP, 10, 41, 0x30C8FFFF    # ring at the full travel, 49.50
    point 5.5000, 38.5000
    point 9.3837, 38.3474
    point 13.2435, 37.8906
    point 17.0555, 37.1323
    point 20.7963, 36.0773
    point 24.4428, 34.7320
    point 27.9725, 33.1048
    point 31.3637, 31.2057
    point 34.5954, 29.0463
    point 37.6477, 26.6401
    point 40.5018, 24.0018
    point 43.1401, 21.1477
    point 45.5463, 18.0954
    point 47.7057, 14.8637
    point 49.6048, 11.4725
    point 51.2320, 7.9428
    point 52.5773, 4.2963
    point 53.6323, 0.5555
    point 54.3906, -3.2565
    point 54.8474, -7.1163
    point 55.0000, -11.0000
    point 54.8474, -14.8837
    point 54.3906, -18.7435
    point 53.6323, -22.5555
    point 52.5773, -26.2963
    point 51.2320, -29.9428
    point 49.6048, -33.4725
    point 47.7057, -36.8637
    point 45.5463, -40.0954
    point 43.1401, -43.1477
    point 40.5018, -46.0018
    point 37.6477, -48.6401
    point 34.5954, -51.0463
    point 31.3637, -53.2057
    point 27.9725, -55.1048
    point 24.4428, -56.7320
    point 20.7963, -58.0773
    point 17.0555, -59.1323
    point 13.2435, -59.8906
    point 9.3837, -60.3474
    point 5.5000, -60.5000
    shape GX_LINESTRIP, 10, 41, 0x1C6C94FF    # dim ring at the least tilt, 39.50
    point 5.5000, 28.5000
    point 8.5991, 28.3782
    point 11.6792, 28.0137
    point 14.7211, 27.4086
    point 17.7062, 26.5667
    point 20.6160, 25.4932
    point 23.4326, 24.1948
    point 26.1387, 22.6793
    point 28.7175, 20.9562
    point 31.1532, 19.0360
    point 33.4307, 16.9307
    point 35.5360, 14.6532
    point 37.4562, 12.2175
    point 39.1793, 9.6387
    point 40.6948, 6.9326
    point 41.9932, 4.1160
    point 43.0667, 1.2062
    point 43.9086, -1.7789
    point 44.5137, -4.8208
    point 44.8782, -7.9009
    point 45.0000, -11.0000
    point 44.8782, -14.0991
    point 44.5137, -17.1792
    point 43.9086, -20.2211
    point 43.0667, -23.2062
    point 41.9932, -26.1160
    point 40.6948, -28.9326
    point 39.1793, -31.6387
    point 37.4562, -34.2175
    point 35.5360, -36.6532
    point 33.4307, -38.9307
    point 31.1532, -41.0360
    point 28.7175, -42.9562
    point 26.1387, -44.6793
    point 23.4326, -46.1948
    point 20.6160, -47.4932
    point 17.7062, -48.5667
    point 14.7211, -49.4086
    point 11.6792, -50.0137
    point 8.5991, -50.3782
    point 5.5000, -50.5000
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
