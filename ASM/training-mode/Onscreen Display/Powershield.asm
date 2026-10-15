    # To be inserted at 800776a0
    # A projectile touched the normal shield collider. Success is a separate path.
    .include "../../Globals.s"
    .include "../../m-ex/Header.s"
    SettingsBackup
    mr r6, r3                 # ItemData
    mr r3, r5                 # FighterData
    li r4, 6                  # Contact: ordinary shield
    mr r5, r6
    rtocbl r12, TM_EventTiming
    SettingsRestore
    mr r28, r4                # Original instruction
