    # To be inserted at 8015ef14
    .include "../../../../Globals.s"

    # Replace the native final memzero(base + 0x1A68, 216). The shared helper
    # clears played flags, assigned scores and the adjacent 0x1B3C word while
    # preserving the unused score extension only for the Tyro save identity.
    # Earlier per-fighter record clears remain owned by the native routine.
    SettingsBackup
    lwz r3, MemcardData(r13)
    rtocbl r12, TM_SettingsResetEventRecords
    SettingsRestore
