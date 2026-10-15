    # To be inserted at 80081370 (native CliffCatch entry).
    .include "../../Globals.s"

    # Sample the previous ledge protection before any catch/state-transition
    # logic runs. Preserve the complete incoming native call context.
    SettingsBackup
    rtocbl r12, TM_UnderFireRegrab
    SettingsRestore

    mflr r0
