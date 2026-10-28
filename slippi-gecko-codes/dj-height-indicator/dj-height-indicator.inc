    # Shared body of the Fox and Falco Double-Jump Height Indicator codes. Each
    # character's .asm defines character_data (its numbers and color) and
    # original_instruction (the instruction its hook replaces), then includes
    # this file.
    #
    # Match start (StartMelee) creates one GObj with a GX link 5 callback. Each
    # frame on a legal stage, the callback finds the two ledges with the line
    # IDs Tyro's Side-B Sweetspot event uses, then under each ledge draws the
    # double-jump platform and, at ledge height, the X where to side-B. Depth
    # testing is off (HSD_StateSetZMode keeps HSD's state cache in sync). Visual
    # only: it never writes game state, so it cannot desync.

    .macro branchl reg, address
    lis \reg, \address @h
    ori \reg, \reg, \address @l
    mtctr \reg
    bctrl
    .endm

    .set GObj_Create, 0x803901F0
    .set GObj_AddGXLink, 0x8039069C
    .set Stage_GetExternalID, 0x80225194
    .set Stage_GetLeftOfLineCoordinates, 0x80053ECC
    .set Stage_GetRightOfLineCoordinates, 0x80053DA4
    .set HSD_ClearVtxDesc, 0x8036C244
    .set GXSetCurrentMtx, 0x80341510
    .set COBJ_GetCurrent, 0x8036A288
    .set COBJ_GetViewingMtx, 0x803695F0
    .set GXLoadPosMtxImm, 0x80341494
    .set HSD_StateSetLineWidth, 0x80361A74
    .set HSD_SetupRenderMode, 0x8036199C
    .set HSD_StateSetZMode, 0x80361BB8
    .set GXSetVtxAttrFmt, 0x8033C414
    .set GXSetVtxDesc, 0x8033BF00
    .set HSD_StateSetCullMode, 0x80361AD8
    .set GXBegin, 0x8033D0DC

    .set STAGE_COUNT, 6
    .set LINE_WIDTH, 16

    # Data table offsets
    .set D_ZERO, 0x00
    .set D_TARGET, 0x04              # half the X's width and height
    .set D_STAGES, 0x08              # STAGE_COUNT x (external ID, left line, right line, pad)
    .set D_FULL, 0x20                # character_data: full side-B,
    .set D_WIDTH, 0x24               # platform half-width,
    .set D_DEPTH, 0x28               # platform depth under the ledge,
    .set D_COLOR, 0x2C               # and color.

#############################
## Match start: create GObj
#############################
    mflr r0
    stw r0, 0x4(r1)
    stwu r1, -0x80(r1)
    stmw r3, 0x8(r1)                 # Keep every live register for the native code.

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
    stwu r1, -0x40(r1)
    stmw r30, 0x38(r1)

    bl DataTable
    mflr r31

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
    addi r4, r1, 0x10
    branchl r12, Stage_GetLeftOfLineCoordinates
    lbz r3, 0x2(r30)
    addi r4, r1, 0x20
    branchl r12, Stage_GetRightOfLineCoordinates

    li r3, 0xA8                      # GX_LINES
    li r4, 12
    li r5, LINE_WIDTH
    bl Begin
    lis r3, 0xCC00
    ori r3, r3, 0x8000
    lwz r4, D_COLOR(r31)
    lfs f0, D_ZERO(r31)
    lfs f5, D_FULL(r31)
    lfs f6, D_WIDTH(r31)
    lfs f7, D_DEPTH(r31)
    lfs f8, D_TARGET(r31)
    lfs f1, 0x10(r1)                 # Left ledge: offstage is -X.
    fsubs f1, f1, f5
    lfs f2, 0x14(r1)
    bl Mark
    lfs f1, 0x20(r1)                 # Right ledge: offstage is +X.
    fadds f1, f1, f5
    lfs f2, 0x24(r1)
    bl Mark

DrawExit:
    lmw r30, 0x38(r1)
    lwz r0, 0x44(r1)
    addi r1, r1, 0x40
    mtlr r0
    blr

    .macro vertex x, y
    stfs \x, 0(r3)
    stfs \y, 0(r3)
    stfs f0, 0(r3)
    stw r4, 0(r3)
    .endm

########################################################
## Mark(r3 FIFO, r4 color, f0 0, f1 target x, f2 ledge y,
## f6 half-width, f7 depth, f8 X half-size): leaf. The
## platform at f2 - f7, then the X centered at (f1, f2).
########################################################
Mark:
    fsubs f3, f2, f7
    fsubs f4, f1, f6
    vertex f4, f3
    fadds f4, f1, f6
    vertex f4, f3
    fsubs f4, f1, f8                 # X: bottom left to top right,
    fsubs f3, f2, f8
    vertex f4, f3
    fadds f4, f1, f8
    fadds f3, f2, f8
    vertex f4, f3
    fsubs f4, f1, f8                 # then top left to bottom right.
    vertex f4, f3
    fadds f4, f1, f8
    fsubs f3, f2, f8
    vertex f4, f3
    blr

########################################################
## Begin(r3 shape, r4 count, r5 line width): GFX_Start
## with depth testing off.
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
    .float 2.5                       # X half-size, DJ_TARGET_SIZE in src/sweetspot.c
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
    lmw r3, 0x8(r1)
    lwz r0, 0x84(r1)
    addi r1, r1, 0x80
    mtlr r0
    original_instruction
