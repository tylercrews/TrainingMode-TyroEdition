    # To be inserted at 8024d644
    .include "../../../../Globals.s"

    # The high score routine (8024d5b0) shows a score only if the event's
    # played bit is set, and r27 is the event on the page shown. The score and
    # its type are already read. On All Events, check the bit the event has on
    # its own page, as that page does (Played Flag Uses Home Page.asm).
    SettingsReadIndexed SettingsField_EventHome, 27, 27
    clrlwi. r0, r3, 24
