    # Shared body of the Up-B Overlay codes. Each character's .asm sets
    # FIGHTER_KIND, defines character_data (its shapes) and
    # original_instruction (the instruction its hook replaces), then includes
    # this file. scripts/upb_overlay_numbers.py writes the .asm files.
    #
    # Match start (StartMelee) creates one GObj with a GX link 5 callback. Each
    # frame, while a fighter of FIGHTER_KIND is in the match on a legal stage,
    # the callback finds the two ledges with the line IDs Tyro's events use and
    # draws the character's shapes at each one. Every point is stored for the
    # right ledge as (out from the ledge, up from it) and mirrored for the left.
    # Depth testing is off (HSD_StateSetZMode keeps HSD's state cache in sync).
    # Visual only: it never writes game state, so it cannot desync.

    .macro branchl reg, address
    lis \reg, \address @h
    ori \reg, \reg, \address @l
    mtctr \reg
    bctrl
    .endm

    .set GObj_Create, 0x803901F0
    .set GObj_AddGXLink, 0x8039069C
    .set Player_GetEntity, 0x80034110  # the slot's active fighter GObj, or 0
    .set Stage_GetExternalID, 0x80225194
    .set Stage_GetLeftOfLineCoordinates, 0x80053ECC
    .set Stage_GetRightOfLineCoordinates, 0x80053DA4
    .set HSD_ClearVtxDesc, 0x8036C244
    .set GXSetCurrentMtx, 0x80341510
    .set COBJ_GetCurrent, 0x8036A288
    .set COBJ_GetViewingMtx, 0x803695F0
    .set GXLoadPosMtxImm, 0x80341494
    .set HSD_StateSetLineWidth, 0x80361A74
    .set HSD_StateSetPointSize, 0x80361C60
    .set HSD_SetupRenderMode, 0x8036199C
    .set HSD_StateSetZMode, 0x80361BB8
    .set GXSetVtxAttrFmt, 0x8033C414
    .set GXSetVtxDesc, 0x8033BF00
    .set HSD_StateSetCullMode, 0x80361AD8
    .set GXBegin, 0x8033D0DC

    .set GX_LINES, 0xA8
    .set GX_LINESTRIP, 0xB0
    .set GX_POINTS, 0xB8

    # character_data: shape records, each followed by its points, then a zero word.
    .macro shape kind, width, count, color
    .byte \kind, \width, \count, 0   # GX shape, line width / point size, points
    .long \color
    .endm

    .macro point x, y
    .float \x, \y
    .endm

    .set PLAYER_COUNT, 6
    .set STAGE_COUNT, 6

    # Data table offsets
    .set D_ZERO, 0x00
    .set D_LEFT, 0x04                # outward from the left ledge
    .set D_RIGHT, 0x08               # and from the right one
    .set D_KIND, 0x0C
    .set D_STAGES, 0x10              # STAGE_COUNT x (external ID, left line, right line, pad)
    .set D_SHAPES, 0x28              # character_data

    # Draw's frame: each ledge's (x, y, z), then which way is out from it.
    .set LEDGES, 0x10
    .set LEDGE_SIZE, 0x10
    .set LEDGE_OUT, 0xC

#############################
## Match start: create GObj
#############################
    stwu r1, -0x90(r1)
    stw r0, 0x8(r1)                  # Keep r0, LR and r3-r31 for the native code:
    mflr r0                          # some hooks sit where r0 is live.
    stw r0, 0xC(r1)
    stmw r3, 0x10(r1)

    li r3, 0
    li r4, 7
    li r5, 0
    branchl r12, GObj_Create
    cmpwi r3, 0
    beq StartExit
    bl GetDraw                       # LR = Draw; GetDraw continues to StartExit.

#########################################
## GX callback: void Draw(GOBJ *, int pass)
#########################################
Draw:
    cmpwi r4, 2
    bnelr
    mflr r0
    stw r0, 0x4(r1)
    stwu r1, -0x50(r1)
    stmw r26, 0x38(r1)

    bl DataTable
    mflr r31

    # Only while this character is in the match.
    li r30, 0
FindFighter:
    mr r3, r30
    branchl r12, Player_GetEntity
    cmpwi r3, 0
    beq NextPlayer
    lwz r3, 0x2C(r3)                 # FighterData
    cmpwi r3, 0
    beq NextPlayer
    lwz r3, 0x4(r3)                  # kind
    lwz r4, D_KIND(r31)
    cmpw r3, r4
    beq FoundFighter
NextPlayer:
    addi r30, r30, 1
    cmpwi r30, PLAYER_COUNT
    blt FindFighter
    b DrawExit
FoundFighter:

    # r30 = this stage's entry, or nothing to draw.
    branchl r12, Stage_GetExternalID
    addi r30, r31, D_STAGES
    li r4, STAGE_COUNT
FindStage:
    lbz r5, 0x0(r30)
    cmpw r5, r3
    beq FoundStage
    addi r30, r30, 4
    subic. r4, r4, 1
    bgt FindStage
    b DrawExit
FoundStage:

    lbz r3, 0x1(r30)
    addi r4, r1, LEDGES
    branchl r12, Stage_GetLeftOfLineCoordinates
    lbz r3, 0x2(r30)
    addi r4, r1, LEDGES + LEDGE_SIZE
    branchl r12, Stage_GetRightOfLineCoordinates
    lfs f0, D_LEFT(r31)
    stfs f0, LEDGES + LEDGE_OUT(r1)
    lfs f0, D_RIGHT(r31)
    stfs f0, LEDGES + LEDGE_SIZE + LEDGE_OUT(r1)

    lis r26, 0xCC00
    ori r26, r26, 0x8000             # GX FIFO
    addi r29, r1, LEDGES             # r29 = this ledge
Ledge:
    addi r28, r31, D_SHAPES          # r28 = this shape
Shape:
    lbz r3, 0x0(r28)
    cmpwi r3, 0
    beq NextLedge
    lbz r27, 0x2(r28)                # r27 = points left
    mr r4, r27
    lbz r5, 0x1(r28)
    bl Begin
    lwz r4, 0x4(r28)                 # color
    addi r28, r28, 8
    lfs f0, D_ZERO(r31)
    lfs f3, 0x0(r29)
    lfs f4, 0x4(r29)
    lfs f5, LEDGE_OUT(r29)
Point:
    lfs f1, 0x0(r28)
    lfs f2, 0x4(r28)
    fmadds f1, f1, f5, f3            # ledge x + out * outward
    fadds f2, f2, f4                 # ledge y + up
    stfs f1, 0(r26)
    stfs f2, 0(r26)
    stfs f0, 0(r26)
    stw r4, 0(r26)
    addi r28, r28, 8
    subic. r27, r27, 1
    bgt Point
    b Shape
NextLedge:
    addi r29, r29, LEDGE_SIZE
    addi r3, r1, LEDGES + 2 * LEDGE_SIZE
    cmpw r29, r3
    blt Ledge

DrawExit:
    lmw r26, 0x38(r1)
    lwz r0, 0x54(r1)
    addi r1, r1, 0x50
    mtlr r0
    blr

########################################################
## Begin(r3 shape, r4 count, r5 line width and point size):
## GFX_Start with depth testing off.
########################################################
Begin:
    mflr r0
    stw r0, 0x4(r1)
    stwu r1, -0x60(r1)
    stmw r29, 0x54(r1)
    mr r31, r3
    mr r30, r4
    mr r29, r5
    branchl r12, HSD_ClearVtxDesc
    li r3, 0
    branchl r12, GXSetCurrentMtx
    branchl r12, COBJ_GetCurrent
    addi r4, r1, 0x20
    branchl r12, COBJ_GetViewingMtx
    addi r3, r1, 0x20
    li r4, 0
    branchl r12, GXLoadPosMtxImm
    mr r3, r29
    li r4, 5
    branchl r12, HSD_StateSetLineWidth
    mr r3, r29
    li r4, 5
    branchl r12, HSD_StateSetPointSize
    lis r3, 0x6800
    ori r3, r3, 0x0002
    branchl r12, HSD_SetupRenderMode
    li r3, 0                         # compare off
    li r4, 7                         # GX_ALWAYS
    li r5, 0                         # no depth update
    branchl r12, HSD_StateSetZMode
    li r3, 0
    li r4, 9                         # GX_VA_POS
    li r5, 1                         # GX_POS_XYZ
    li r6, 4                         # GX_F32
    li r7, 0
    branchl r12, GXSetVtxAttrFmt
    li r3, 0
    li r4, 11                        # GX_VA_CLR0
    li r5, 1                         # GX_CLR_RGBA
    li r6, 5                         # GX_RGBA8
    li r7, 0
    branchl r12, GXSetVtxAttrFmt
    li r3, 9
    li r4, 1                         # GX_DIRECT
    branchl r12, GXSetVtxDesc
    li r3, 11
    li r4, 1
    branchl r12, GXSetVtxDesc
    li r3, 0
    branchl r12, HSD_StateSetCullMode
    mr r3, r31
    li r4, 0                         # GX_VTXFMT0
    mr r5, r30
    branchl r12, GXBegin
    lmw r29, 0x54(r1)
    lwz r0, 0x64(r1)
    addi r1, r1, 0x60
    mtlr r0
    blr

DataTable:
    blrl
    .float 0.0
    .float -1.0
    .float 1.0
    .long FIGHTER_KIND
    .byte 2, 0x03, 0x07, 0           # Fountain of Dreams
    .byte 3, 0x33, 0x36, 0           # Pokemon Stadium
    .byte 8, 0x02, 0x06, 0           # Yoshi's Story
    .byte 28, 0x03, 0x05, 0          # Dream Land
    .byte 31, 0x00, 0x05, 0          # Battlefield
    .byte 32, 0x00, 0x02, 0          # Final Destination
    character_data

GetDraw:
    mflr r4                          # Draw
    li r5, 5                         # GX link 5: world overlays
    li r6, 0
    branchl r12, GObj_AddGXLink      # r3 still holds the new GObj

StartExit:
    lmw r3, 0x10(r1)
    lwz r0, 0xC(r1)
    mtlr r0
    lwz r0, 0x8(r1)
    addi r1, r1, 0x90
    original_instruction
