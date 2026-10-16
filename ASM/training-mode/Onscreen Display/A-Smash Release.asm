    # To be inserted at 800df0e8
    .include "../../Globals.s"
    .include "../../m-ex/Header.s"

    # Fighter_procInput has updated native buttons and excluded hitlag.
    # r3 is still the fighter GOBJ; r31 is its userdata. Observe before native
    # PreCharge/Charging changes its state for a manual A release.
    SettingsBackup
    rtocbl r12, TM_ASmashInput
    SettingsRestore

    lwz r0, 0x2114(r31)
