    # To be inserted at 8007B760 (native timed intangibility grant).
    .include "../../Globals.s"

    SettingsBackup
    mr r5, r4
    li r4, 3
    lwz r3, 0x2c(r3)
    rtocbl r12, TM_EventTiming
    SettingsRestore
    mflr r0
