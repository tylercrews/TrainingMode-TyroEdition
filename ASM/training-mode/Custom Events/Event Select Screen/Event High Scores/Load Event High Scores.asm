    # To be inserted at 8015cf60
    .include "../../../../Globals.s"

    # r3 = event within the saved page. The shared accessor validates page,
    # event and flattened score index before returning an owned score slot.
    # Invalid indices must never expose the spare score preference region.
    SettingsBackup
    rtocbl r12, TM_SettingsEventScoreSlot
    cmpwi r3, 0
    beq InvalidScore
    lwz r3, 0(r3)
    b ScoreReady
InvalidScore:
    li r3, 0
ScoreReady:
    stw r3, 0x30(r1)
    SettingsRestore
    blr
