    # To be inserted at 8005FDDC (efSync_Spawn entry).
    .include "../../Globals.s"

    # Every effect path, including loose particles and late accessory models,
    # supplies its owner in r4. A suppressed spawn returns the native NULL.
    SettingsBackup
    mr r5, r3
    mr r3, r4
    li r4, 2
    rtocbl r12, TM_UnderFireControl
    cmpwi r3, 0
    beq AllowSpawn
    SettingsRestore
    li r3, 0
    blr
AllowSpawn:
    SettingsRestore
    mflr r0
