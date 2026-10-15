    # To be inserted at 8007758c
    # Accepted reflection: over-damage rejection bypasses this block entirely.
    .include "../../Globals.s"
    .include "../../m-ex/Header.s"
    SettingsBackup
    mr r3, r31                # FighterData
    li r4, 7                  # Contact: accepted reflection
    mr r5, r30                # ItemData (service excludes other reflectors)
    rtocbl r12, TM_EventTiming
    SettingsRestore
    lwz r0, 0x2070(r31)       # Original instruction
