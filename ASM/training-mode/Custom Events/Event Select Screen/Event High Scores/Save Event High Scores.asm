    # To be inserted at 8015cf74
    .include "../../../../Globals.s"

    # r3 = event within the saved page, r4 = score. Keep the incoming score
    # across the C call and refuse any slot outside the current catalogue.
    SettingsBackup
    rtocbl r12, TM_SettingsEventScoreSlot
    cmpwi r3, 0
    beq Exit
    SettingsSavedLoad r4, 4
    stw r4, 0(r3)
Exit:
    SettingsRestore
    blr
