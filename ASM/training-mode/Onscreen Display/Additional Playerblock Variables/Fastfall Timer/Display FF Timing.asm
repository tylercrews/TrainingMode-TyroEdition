    # To be inserted at 8007d57c
    .include "../../../../Globals.s"
    .include "../../../../m-ex/Header.s"

    .set entity, 31
    .set REG_FighterData, 31
    .set player, 30
    .set REG_TextColor, 29
    .set REG_Text, 28

##########################################################
## 804a1f5c -> 804a1fd4 = Static Stock Icon Text Struct ##
## Is 0x80 long and is zero'd at the start ##
## of every VS Match ##
## Store Text Info here ##
##########################################################

    backupall

    mr REG_FighterData, r3

    # CHECK IF ENABLED
    li r0, OSD.Fastfall             # PowerShield ID
    # lwz r4, -0xdbc(rtoc) #get frame data toggle bits
    lwz r4, MemcardData(r13)
    SettingsRead SettingsField_OSDMask, 4
    li r3, 1
    slw r0, r3, r0
    and. r0, r0, r4
    beq Exit

CheckForFollower:
    mr r3, REG_FighterData
    branchl r12, 0x80005510
    cmpwi r3, 0x1
    beq Exit

    # Check If Falling Off Ledge and Respawn Platform
    lhz r3, TM_OneASAgo(REG_FighterData)
    # cmpwi r3, 0xFD #CliffWait
    # beq Exit
    cmpwi r3, 0xD                   # RebirthWait
    beq Exit

    # TM_CanFastfallFrameCount is incremented by the native fastfall check
    # (8007d528) only on frames where y velocity < 0, so it already tracks the
    # real first fastfallable frame (e.g. frame-1 aerials changing gravity).

####################
## Get Jump Frame ##
####################

    # Sum frames back through the AS history until a jump state. The native
    # check runs after the AS frame counter ticks, so jump frame 1 reads as 1.
    lhz r9, TM_FramesinCurrentAS(REG_FighterData)
    lwz r3, 0x10(REG_FighterData)
    subi r3, r3, ASID_JumpF
    cmplwi r3, ASID_JumpAerialB - ASID_JumpF
    ble GetInputFrame
    li r10, 0
JumpWalkLoop:
    add r11, REG_FighterData, r10
    lhz r3, TM_FramesInPrevASStart(r11)
    add r9, r9, r3
    lhz r3, TM_PrevASStart(r11)
    subi r3, r3, ASID_JumpF
    cmplwi r3, ASID_JumpAerialB - ASID_JumpF
    ble GetInputFrame
    addi r10, r10, 2
    cmpwi r10, TM_PrevASSlots * 2
    blt JumpWalkLoop
    b PrintMessage_NoJump           # Not from a jump (walked off, multijump, etc.)

GetInputFrame:
    # 0x671 = frames since the stick left the Y deadzone. The native check
    # accepts it while below the smash window, which is the fastfall buffer.
    lbz r3, 0x671(REG_FighterData)
    sub r7, r9, r3

PrintMessage:
    li r3, 7                        # Message Kind
    lbz r4, 0xC(REG_FighterData)    # Message Queue
    li r5, MSGCOLOR_WHITE
    bl Fastfall_String
    mflr r6
    lhz r8, TM_CanFastfallFrameCount(REG_FighterData)
    Message_DisplayOSD 20, 2, 2, 0
    lwz r3, 0x2C(r3)
    lwz REG_Text, MsgData_Text(r3)
    li REG_TextColor, 2
    b CheckFrameOne

PrintMessage_NoJump:
    li r3, 7                        # Message Kind
    lbz r4, 0xC(REG_FighterData)    # Message Queue
    li r5, MSGCOLOR_WHITE
    bl Fastfall_String_NoJump
    mflr r6
    lhz r7, TM_CanFastfallFrameCount(REG_FighterData)
    Message_DisplayOSD 20, 1, 1, 0
    lwz r3, 0x2C(r3)
    lwz REG_Text, MsgData_Text(r3)
    li REG_TextColor, 1

CheckFrameOne:
    # Check if frame 1
    lhz r3, TM_CanFastfallFrameCount(REG_FighterData)
    cmpwi r3, 1
    bne Exit
    mr r3, REG_Text
    mr r4, REG_TextColor            # result row subtext
    bl Colors
    mflr r5
    branchl r12, Text_ChangeTextColor

    b Exit

###################
## TEXT CONTENTS ##
###################

Fastfall_String:
    blrl
    .string "Fastfall\ninput %df\n%df (%df)"
    .align 2

Fastfall_String_NoJump:
    blrl
    .string "Fastfall\n%df"
    .align 2

Colors:
    blrl
    .long 0x8dff6eff                # green

##############################

Exit:
    restoreall
    li r0, 1
