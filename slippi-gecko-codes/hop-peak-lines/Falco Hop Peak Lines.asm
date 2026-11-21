    # To be inserted at 8016e930
    # Falco Hop Peak Lines for vanilla NTSC 1.02 Melee / Slippi.
    # While Falco is in the match on a legal stage, draws a teal line where
    # a laser shot at the peak of Falco's short hop flies, and a purple one
    # for a full hop, over the main floor and each platform. Written by
    # scripts/hop_peak_lines.py from the ISO; the drawing is shared in
    # hop-peak-lines.inc.

    .set FIGHTER_KIND, 22

    .macro character_data
    .float 13.477983                # short hop: rises 9.6800, laser 3.7980 over TopN
    .float 51.197981                # full hop: rises 47.4000
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
