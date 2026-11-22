    # To be inserted at 8016e930
    # Falco Hop Peak Lines for vanilla NTSC 1.02 Melee / Slippi.
    # While Falco is in the match on a legal stage, draws an orange line
    # where the highest laser of Falco's short hop flies, and a teal one
    # for a full hop, over the main floor and each platform. Written by
    # scripts/hop_peak_lines.py from the ISO; the drawing is shared in
    # hop-peak-lines.inc.

    .set FIGHTER_KIND, 22

    .macro character_data
    .float 20.760109                # short hop: B on frame 2 shoots from 11.4400 up, laser 9.3201 over TopN
    .float 60.820107                # full hop: B on frame 14 shoots from 51.5000 up
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
    blr
    .endm

    .include "hop-peak-lines.inc"
