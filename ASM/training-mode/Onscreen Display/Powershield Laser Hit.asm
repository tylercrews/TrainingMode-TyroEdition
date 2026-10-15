    # To be inserted at 800780e0
    # Accepted corporeal projectile hit, after hit logging; phantoms bypass it.
    .include "../../Globals.s"
    .include "../../m-ex/Header.s"
    SettingsBackup
    mr r3, r28                # FighterData
    li r4, 5                  # Contact: corporeal projectile hit
    mr r5, r26                # ItemData
    rtocbl r12, TM_EventTiming
    SettingsRestore
    lwz r3, 0xC34(r26)        # Original instruction
