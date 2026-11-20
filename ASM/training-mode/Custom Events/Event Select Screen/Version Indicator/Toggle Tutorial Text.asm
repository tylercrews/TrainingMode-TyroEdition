    # To be inserted at 8024d84c
    .include "../../../../Globals.s"
    .include "../../../../m-ex/Header.s"

    .set LeftArrowTextOffset, -0x4EB0
    .set RightArrowTextOffset, -0x4EAC

    backup

    # Pages wrap (All Events (Alphabetized) is left of Minigames), so there's
    # always a page each way: show both arrows.
    li r4, 0
    lwz r3, LeftArrowTextOffset (r13)
    stb r4, 0x4D(r3)
    lwz r3, RightArrowTextOffset (r13)
    stb r4, 0x4D(r3)

end:
    restore
    lwz r0, 0x0024(sp)
