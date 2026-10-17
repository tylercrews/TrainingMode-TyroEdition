    # To be inserted at 80235fb4
    .include "../../../Globals.s"
    .include "../../../m-ex/Header.s"

    # Global Settings exits (and saves) on Start; B steps a choice backward
    # in TM_OSDEditorInput instead. r3 holds RSS menu bits: 0x20 back, 0x100 Start.
    # r4 is free here; only CR0 must carry the result to the native beq.
    load r4, 0x804a04f0
    lbz r4, 0x0011(r4)
    cmpwi r4, 0x2
    blt original

    rlwinm. r0, r3, 0, 23, 23   # Start
    b exit

original:
    rlwinm. r0, r3, 0, 26, 26   # Back

exit:
