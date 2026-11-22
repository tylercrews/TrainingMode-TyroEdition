    # To be inserted at 8016e740
    # Fox Hop Peak Lines for vanilla NTSC 1.02 Melee / Slippi.
    # While Fox is in the match on a legal stage, draws an orange line
    # where the highest laser of Fox's short hop flies, and a teal one
    # for a full hop, over the main floor and each platform. Written by
    # scripts/hop_peak_lines.py from the ISO; the drawing is shared in
    # hop-peak-lines.inc.

    .set FIGHTER_KIND, 1

    .macro character_data
    .float 19.932854                # short hop: B on frame 2 shoots from 10.6500 up, laser 9.2829 over TopN
    .float 40.562855                # full hop: B on frame 8 shoots from 31.2800 up
    .endm

    # External stage ID, platform lines (NO_LINE: none), main floor y at x = 0.
    .macro stage_data
    stage 2, 0x00, 0x01, 0x02, 0.002775             # Fountain of Dreams
    stage 3, 0x23, 0x24, 0xFF, 0.000000             # Pokemon Stadium
    stage 8, 0x01, 0x04, 0x05, 0.000000             # Yoshi's Story
    stage 28, 0x00, 0x01, 0x02, 0.008800            # Dream Land
    stage 31, 0x02, 0x03, 0x04, 0.000000            # Battlefield
    stage 32, 0xFF, 0xFF, 0xFF, 0.000000            # Final Destination
    .endm

    .macro original_instruction
    mr r31, r3
    .endm

    .include "hop-peak-lines.inc"
