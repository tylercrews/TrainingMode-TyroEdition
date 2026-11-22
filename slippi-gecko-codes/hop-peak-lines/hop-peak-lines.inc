    # Shared body of the Fox and Falco Hop Peak Lines codes. Each character's
    # .asm sets FIGHTER_KIND, defines character_data (its two heights),
    # stage_data (the legal stages) and original_instruction (the instruction
    # its hook replaces), then includes this file. scripts/hop_peak_lines.py
    # writes the .asm files from the ISO.
    #
    # Match start (StartMelee) creates one GObj with a GX link 5 callback. Each
    # frame, while a fighter of FIGHTER_KIND is in the match on a legal stage,
    # the callback draws two lines over the main floor and over each platform:
    # orange where the highest laser of a short hop flies, teal where a full
    # hop's does. The main floor's and the top platform's lines run from
    # blast zone to blast zone, and a side platform's from its side's blast zone
    # to the middle (x = 0), so the two sides read apart. Platforms come from
    # their live collision lines, so Fountain's lines follow its side platforms
    # and go while one is sunk into the stage, and a platform whose line the
    # game has turned off (Stadium's, during a transformation) has none. Depth
    # testing is off (HSD_StateSetZMode keeps HSD's state cache in sync).
    # Visual only: it never writes game state, so it cannot desync. The same
    # drawing as Target Practice's Hop Peak Lines in src/targets.c.

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
    .set Stage_GetBlastzoneLeft, 0x80224B50
    .set Stage_GetBlastzoneRight, 0x80224B38
    .set GrColl_CheckIfLineEnabled, 0x80054ED8  # enabled and not hidden
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

    .set CollLines, -0x51E4          # r13: CollLine[] (8 bytes each, its CollLineDesc first)
    .set CollVerts, -0x51E8          # r13: CollVert[] (0x18 bytes each, pos_curr at +8)
    .set GX_LINES, 0xA8
    .set LINE_WIDTH, 12              # 2 pixels, in sixths
    .set PLAYER_COUNT, 6
    .set STAGE_COUNT, 6
    .set STAGE_SIZE, 8
    .set PLATFORMS, 3
    .set NO_LINE, 0xFF

    # stage_data: external ID, PLATFORMS platform line IDs, main floor y.
    .macro stage id, first, second, third, main
    .byte \id, \first, \second, \third
    .float \main
    .endm

    # Data table offsets
    .set D_ZERO, 0x00
    .set D_HALF, 0x04
    .set D_SUNK, 0x08                # a platform closer than this to the main floor is sunk
    .set D_KIND, 0x0C
    .set D_COLORS, 0x10              # the short hop's line, then the full hop's
    .set D_HEIGHTS, 0x18             # character_data, in the same order
    .set D_STAGES, 0x20              # stage_data

    # Draw's frame: each floor's line, then the blast zones and the main floor.
    .set FLOORS, 0x10                # 1 + PLATFORMS x (left x, right x, y)
    .set FLOOR_SIZE, 0xC
    .set LEFT, 0x40
    .set RIGHT, 0x44
    .set MAIN_Y, 0x48

#############################
## Match start: create GObj
#############################
    stwu r1, -0x90(r1)
    stw r0, 0x8(r1)                  # Keep every register for the native code,
    mflr r0                          # r0, LR and CR included: one hook is the
    stw r0, 0xC(r1)                  # function's last blr.
    mfcr r0
    stw r0, 0x10(r1)
    stmw r3, 0x14(r1)

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
    stwu r1, -0x80(r1)
    stmw r24, 0x60(r1)

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
    addi r30, r30, STAGE_SIZE
    subic. r4, r4, 1
    bgt FindStage
    b DrawExit
FoundStage:

    branchl r12, Stage_GetBlastzoneLeft
    stfs f1, LEFT(r1)
    branchl r12, Stage_GetBlastzoneRight
    stfs f1, RIGHT(r1)

    # The main floor's lines go across.
    addi r29, r1, FLOORS             # r29 = the next floor
    li r25, 1                        # r25 = floors
    lfs f1, LEFT(r1)
    lfs f2, RIGHT(r1)
    lfs f3, 0x4(r30)
    stfs f3, MAIN_Y(r1)
    stfs f1, 0x0(r29)
    stfs f2, 0x4(r29)
    stfs f3, 0x8(r29)
    addi r29, r29, FLOOR_SIZE

    # Then each platform whose line is on, from its live vertices.
    li r28, 1                        # r28 = its byte in the stage's entry
Platform:
    lbzx r27, r30, r28               # r27 = its line
    cmpwi r27, NO_LINE
    beq NextPlatform
    mr r3, r27
    branchl r12, GrColl_CheckIfLineEnabled
    cmpwi r3, 0
    beq NextPlatform
    lwz r3, CollLines(r13)
    lwz r4, CollVerts(r13)
    cmpwi r3, 0
    beq NextPlatform
    cmpwi r4, 0
    beq NextPlatform
    slwi r5, r27, 3
    lwzx r5, r3, r5                  # CollLineDesc *
    cmpwi r5, 0
    beq NextPlatform
    lha r6, 0x0(r5)                  # its two vertices
    lha r7, 0x2(r5)
    mulli r6, r6, 0x18
    mulli r7, r7, 0x18
    add r6, r6, r4
    add r7, r7, r4
    lfs f1, 0x8(r6)                  # each end's x and y, where it is now
    lfs f2, 0xC(r6)
    lfs f3, 0x8(r7)
    lfs f4, 0xC(r7)
    fadds f5, f2, f4
    lfs f0, D_HALF(r31)
    fmuls f5, f5, f0                 # y at its middle
    lfs f0, MAIN_Y(r1)
    lfs f6, D_SUNK(r31)
    fadds f0, f0, f6
    fcmpo cr0, f5, f0
    blt NextPlatform                 # Sunk into the stage (Fountain's, hidden).
    lfs f0, D_ZERO(r31)
    lfs f6, LEFT(r1)                 # Across the middle (the top platform): blast zone
    lfs f7, RIGHT(r1)                # to blast zone.
    fcmpo cr0, f1, f0
    fcmpo cr1, f3, f0
    bge cr0, 0f
    bge cr1, StoreFloor
    fmr f7, f0                       # Both ends left of the middle: up to it.
    b StoreFloor
0:  ble cr0, StoreFloor
    ble cr1, StoreFloor
    fmr f6, f0                       # Both right of it: from it.
StoreFloor:
    stfs f6, 0x0(r29)
    stfs f7, 0x4(r29)
    stfs f5, 0x8(r29)
    addi r29, r29, FLOOR_SIZE
    addi r25, r25, 1
NextPlatform:
    addi r28, r28, 1
    cmpwi r28, PLATFORMS
    ble Platform

    # Two lines a floor, at its height plus each peak laser.
    slwi r4, r25, 2
    li r3, GX_LINES
    li r5, LINE_WIDTH
    bl Begin
    lis r26, 0xCC00
    ori r26, r26, 0x8000             # GX FIFO
    lfs f0, D_ZERO(r31)
    addi r29, r1, FLOORS
Floor:
    li r28, 0                        # 0 the short hop's line, 4 the full hop's
Hop:
    addi r3, r31, D_COLORS
    lwzx r4, r3, r28
    addi r3, r31, D_HEIGHTS
    lfsx f3, r3, r28
    lfs f2, 0x8(r29)
    fadds f2, f2, f3
    lfs f1, 0x0(r29)
    stfs f1, 0(r26)
    stfs f2, 0(r26)
    stfs f0, 0(r26)
    stw r4, 0(r26)
    lfs f1, 0x4(r29)
    stfs f1, 0(r26)
    stfs f2, 0(r26)
    stfs f0, 0(r26)
    stw r4, 0(r26)
    addi r28, r28, 4
    cmpwi r28, 8
    blt Hop
    addi r29, r29, FLOOR_SIZE
    subic. r25, r25, 1
    bgt Floor

DrawExit:
    lmw r24, 0x60(r1)
    lwz r0, 0x84(r1)
    addi r1, r1, 0x80
    mtlr r0
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
    .float 0.5
    .float 1.0                       # SUNK_PLATFORM in src/targets.c
    .long FIGHTER_KIND
    .long 0xE68724E6                 # orange: Training Mode's FF9628 at alpha 230, premultiplied
    .long 0x00A290E6                 # teal: 00B4A0 at alpha 230
    character_data
    stage_data

GetDraw:
    mflr r4                          # Draw
    li r5, 5                         # GX link 5: world overlays
    li r6, 0
    branchl r12, GObj_AddGXLink      # r3 still holds the new GObj

StartExit:
    lmw r3, 0x14(r1)
    lwz r0, 0x10(r1)
    mtcr r0
    lwz r0, 0xC(r1)
    mtlr r0
    lwz r0, 0x8(r1)
    addi r1, r1, 0x90
    original_instruction
