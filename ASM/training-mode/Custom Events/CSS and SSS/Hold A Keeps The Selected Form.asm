    # To be inserted at 8016d97c
    .include "../../../Globals.s"
    .include "../../../m-ex/Header.s"

    # Holding A through a match load swaps Zelda and Sheik, either way. Events
    # pick the form on the character select, so a Retry pressed with A, or A
    # still held from the stage select, mustn't turn it into the other one.
    rlwinm. r0, r0, 0, 23, 23                   # Original: is A held?
    beq Exit
    load r3, SceneController
    lbz r3, Scene.CurrentMajor(r3)
    cmpwi r3, Scene.EventMode                   # Equal skips the swap, like A being up.

Exit:
