    # To be inserted at 80261704
    .include "../../../Globals.s"
    .include "../../../m-ex/Header.s"

    # The VS CSS's Rules button sets the CSS exit kind to 3 (Rules) when A is
    # pressed over it. Events using the VS CSS (2v1 Grab Punish Practice) have
    # no Rules screen, and the event CSS treats every exit but Back as Start,
    # so in event mode the button does nothing.
    # cr0 holds the A test (rlwinm. r0, r28, 0, 23, 23) for the next beq,
    # which skips the exit; r3 is free after the original store.

    sth r3, 0x8(r31)    # original: the cursor is over the button

    load r3, SceneController
    lbz r3, Scene.CurrentMajor(r3)
    cmpwi r3, Scene.EventMode
    beq Exit            # event mode: eq is set, so the exit is skipped

    rlwinm. r0, r28, 0, 23, 23  # otherwise, test A again for the beq

Exit:
