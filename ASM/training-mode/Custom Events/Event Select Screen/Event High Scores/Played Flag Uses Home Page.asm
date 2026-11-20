    # To be inserted at 8015ceb4
    .include "../../../../Globals.s"

    # Events_SetEventAsPlayed: r3 is the event on the page it was played from.
    # Set the bit of its index on its own page, so an event played from All
    # Events counts on its own page too, and the other way around.
    SettingsReadIndexed SettingsField_EventHome, 3, 3
    mflr r0
