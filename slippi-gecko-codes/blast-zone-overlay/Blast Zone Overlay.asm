    # To be inserted at 8016e8c0
    # Blast Zone Overlay for vanilla NTSC 1.02 Melee / Slippi (standalone Gecko code).
    # On every stage, draws a crisp red line on the blast zones and a red glow
    # fading out past them. Visual only: it never writes game state, so it
    # cannot desync.
    #
    # Match start (StartMelee) creates one GObj with a GX link 5 callback, the
    # same link the Randall Overlay uses. Each frame the callback reads the live
    # blast zones the way the game's Stage_GetBlastzone* getters do (the stage's
    # bounds plus its camera offset), which is what the KO check compares
    # against. Depth testing is off (HSD_StateSetZMode keeps HSD's state cache
    # in sync), so nothing hides the overlay.

    .macro branchl reg, address
    lis \reg, \address @h
    ori \reg, \reg, \address @l
    mtctr \reg
    bctrl
    .endm

    .set GObj_Create, 0x803901F0
    .set GObj_AddGXLink, 0x8039069C
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

    .set Stage, 0x8049E6C8           # stc_stage
    .set OffsetX, 0x10               # camera offset, added by every getter
    .set OffsetY, 0x14
    .set BlastLeft, 0x74
    .set BlastRight, 0x78
    .set BlastTop, 0x7C
    .set BlastBottom, 0x80
    .set LINE_WIDTH, 18              # 3 pixels, in sixths

    # Data table offsets
    .set D_ZERO, 0x00
    .set D_GLOW, 0x04                # how far past the line the glow fades out
    .set D_GLOW_COLOR, 0x08
    .set D_LINE_COLOR, 0x0C

    # Draw's frame: the four blast zone corners counterclockwise from bottom
    # left, then the glow's outer corners in the same order.
    .set INNER, 0x10
    .set OUTER, 0x30

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
    stwu r1, -0x80(r1)
    stmw r24, 0x60(r1)

    bl DataTable
    mflr r31

    lis r3, Stage @h
    ori r3, r3, Stage @l
    lfs f0, OffsetX(r3)
    lfs f1, BlastLeft(r3)
    fadds f1, f1, f0                 # left
    lfs f2, BlastRight(r3)
    fadds f2, f2, f0                 # right
    lfs f0, OffsetY(r3)
    lfs f3, BlastTop(r3)
    fadds f3, f3, f0                 # top
    lfs f4, BlastBottom(r3)
    fadds f4, f4, f0                 # bottom
    fcmpo cr0, f2, f1                # Nothing to draw unless right > left
    ble DrawExit
    fcmpo cr0, f3, f4                # and top > bottom.
    ble DrawExit

    lfs f5, D_GLOW(r31)
    fsubs f6, f1, f5
    fadds f7, f2, f5
    fadds f8, f3, f5
    fsubs f9, f4, f5
    addi r24, r1, INNER
    stfs f1, 0x00(r24)               # bottom left
    stfs f4, 0x04(r24)
    stfs f2, 0x08(r24)               # bottom right
    stfs f4, 0x0C(r24)
    stfs f2, 0x10(r24)               # top right
    stfs f3, 0x14(r24)
    stfs f1, 0x18(r24)               # top left
    stfs f3, 0x1C(r24)
    stfs f6, 0x20(r24)               # and the glow's outer corners
    stfs f9, 0x24(r24)
    stfs f7, 0x28(r24)
    stfs f9, 0x2C(r24)
    stfs f7, 0x30(r24)
    stfs f8, 0x34(r24)
    stfs f6, 0x38(r24)
    stfs f8, 0x3C(r24)

    lis r29, 0xCC00
    ori r29, r29, 0x8000             # GX FIFO

    # Glow: one band per side, red at the line and clear at its outer edge.
    li r3, 0x80                      # GX_QUADS
    li r4, 16
    li r5, 0
    bl Begin
    lfs f0, D_ZERO(r31)
    lwz r27, D_GLOW_COLOR(r31)
    li r28, 0                        # corner i, times 8
Glow:
    addi r25, r28, 8
    andi. r25, r25, 0x18             # corner i+1 (wrapping), times 8
    add r3, r24, r28
    mr r4, r27
    bl Vertex
    add r3, r24, r25
    mr r4, r27
    bl Vertex
    add r3, r24, r25
    addi r3, r3, OUTER - INNER
    li r4, 0
    bl Vertex
    add r3, r24, r28
    addi r3, r3, OUTER - INNER
    li r4, 0
    bl Vertex
    addi r28, r28, 8
    cmpwi r28, 0x20
    blt Glow

    # The line, closed back to the first corner.
    li r3, 0xB0                      # GX_LINESTRIP
    li r4, 5
    li r5, LINE_WIDTH
    bl Begin
    lfs f0, D_ZERO(r31)
    lwz r27, D_LINE_COLOR(r31)
    li r28, 0
Line:
    andi. r3, r28, 0x18
    add r3, r24, r3
    mr r4, r27
    bl Vertex
    addi r28, r28, 8
    cmpwi r28, 0x28
    blt Line

    # Wide lines end square at their vertices; a point as wide fills each corner.
    li r3, 0xB8                      # GX_POINTS
    li r4, 4
    li r5, LINE_WIDTH
    bl Begin
    lfs f0, D_ZERO(r31)
    li r28, 0
Corner:
    add r3, r24, r28
    mr r4, r27
    bl Vertex
    addi r28, r28, 8
    cmpwi r28, 0x20
    blt Corner

DrawExit:
    lmw r24, 0x60(r1)
    lwz r0, 0x84(r1)
    addi r1, r1, 0x80
    mtlr r0
    blr

########################################################
## Vertex(r3 point, r4 color, r29 FIFO, f0 0): leaf.
########################################################
Vertex:
    lfs f1, 0x0(r3)
    lfs f2, 0x4(r3)
    stfs f1, 0(r29)
    stfs f2, 0(r29)
    stfs f0, 0(r29)
    stw r4, 0(r29)
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
    .float 24.0                      # BLAST_ZONE_GLOW in src/stage_overlay_game.c
    .long 0x78000078                 # glow: red at alpha 120, premultiplied
    .long 0xFF0000FF                 # line: opaque red

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
    li r3, 255                       # Original instruction
