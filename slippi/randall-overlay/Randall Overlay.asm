    # To be inserted at 8016e8c8
    # Randall Overlay for vanilla NTSC 1.02 Melee / Slippi (standalone Gecko code).
    # Draws Randall's live platform and a cloud outline over everything on
    # Yoshi's Story, including while the cloud is behind the stage. Visual only:
    # it reads stage collision and never writes game state, so it cannot desync.
    #
    # Match start (StartMelee) creates one GObj with a GX link 5 callback, the
    # same link the hitbox/trail overlays use. The callback finds the collision
    # group whose joint belongs to Randall (map GObj 2) and draws its floor lines
    # plus three translucent puffs sized to the group's vertex bounds. Depth
    # testing is disabled through HSD's cached state (HSD_StateSetZMode).

    .macro branchl reg, address
    lis \reg, \address @h
    ori \reg, \reg, \address @l
    mtctr \reg
    bctrl
    .endm

    .set GObj_Create, 0x803901F0
    .set GObj_AddGXLink, 0x8039069C
    .set Stage_GetExternalID, 0x80225194
    .set Stage_ExternalToInternal, 0x8022519C
    .set Stage_GetMapGObj, 0x801C2BA4
    .set JOBJ_GetWorldPosition, 0x8000B1CC
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

    .set GRKIND_STORY, 10
    .set RANDALL_MAP_GOBJ, 2
    .set FirstCollGroup, -0x51DC     # r13: CollGroup *
    .set CollLines, -0x51E4          # r13: CollLine[] (8 bytes each)
    .set CollVerts, -0x51E8          # r13: CollVert[] (0x18 bytes, pos_curr at +8)
    .set SEGMENTS, 16

    # Data table offsets
    .set D_FILL, 0x00
    .set D_EDGE, 0x04
    .set D_TOP, 0x08
    .set D_ZERO, 0x0C
    .set D_PUFF, 0x10                # 0.24 of the platform width
    .set D_BASE, 0x14                # 0.55 puff below the platform
    .set D_SIDE, 0x18                # side puffs 0.95 puff in from each end
    .set D_FLAT, 0x1C                # 0.8 vertical squash
    .set D_BIG, 0x20                 # middle puff 1.25x
    .set D_DROP, 0x24                # middle puff 0.25 puff lower
    .set D_HALF, 0x28
    .set D_FALLBACK, 0x2C            # fallback circle radius
    .set D_CIRCLE, 0x30              # SEGMENTS+1 (cos, sin) pairs

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
    stwu r1, -0x100(r1)
    stmw r20, 0x8(r1)

    bl DataTable
    mflr r27

    branchl r12, Stage_GetExternalID
    branchl r12, Stage_ExternalToInternal
    cmpwi r3, GRKIND_STORY
    bne DrawExit
    li r3, RANDALL_MAP_GOBJ
    branchl r12, Stage_GetMapGObj    # Null once hazards remove Randall.
    cmpwi r3, 0
    beq DrawExit
    lwz r4, 0x2C(r3)                 # MapData
    cmpwi r4, 0
    beq DrawExit
    lwz r4, 0x14(r4)                 # MapData index
    cmpwi r4, RANDALL_MAP_GOBJ
    bne DrawExit
    lwz r31, 0x28(r3)                # Root joint
    cmpwi r31, 0
    beq DrawExit

    # Find the collision group whose joint is Randall's root or a descendant.
    lwz r30, FirstCollGroup(r13)
FindGroup:
    cmpwi r30, 0
    beq Fallback
    lwz r3, 0x20(r30)                # group->jobj
WalkUp:
    cmpwi r3, 0
    beq NextGroup
    cmpw r3, r31
    beq FoundGroup
    lwz r3, 0x0C(r3)                 # jobj->parent
    b WalkUp
NextGroup:
    lwz r30, 0x0(r30)
    b FindGroup

Fallback:
    mr r3, r31
    li r4, 0
    addi r5, r1, 0x80
    branchl r12, JOBJ_GetWorldPosition
    lfs f1, 0x80(r1)
    lfs f2, 0x84(r1)
    lfs f3, D_FALLBACK(r27)
    fmr f4, f3
    mr r3, r27
    bl Ellipse
    b DrawExit

FoundGroup:
    lwz r29, 0x4(r30)                # CollGroupDesc
    cmpwi r29, 0
    beq Fallback
    lwz r28, CollVerts(r13)
    cmpwi r28, 0
    beq Fallback
    lhz r26, 0x26(r29)               # vert_num
    cmpwi r26, 0
    beq Fallback
    lhz r25, 0x24(r29)               # vert_start
    mulli r25, r25, 0x18
    add r25, r25, r28
    addi r25, r25, 0x8               # &verts[start].pos_curr
    lfs f5, 0x0(r25)                 # left
    fmr f6, f5                       # right
    lfs f7, 0x4(r25)                 # high
Bounds:
    lfs f1, 0x0(r25)
    lfs f2, 0x4(r25)
    fcmpo cr0, f1, f5
    bge 0f
    fmr f5, f1
0:  fcmpo cr0, f1, f6
    ble 1f
    fmr f6, f1
1:  fcmpo cr0, f2, f7
    ble 2f
    fmr f7, f2
2:  addi r25, r25, 0x18
    subic. r26, r26, 1
    bgt Bounds

    # width, puff and base, kept on the stack across the drawing calls.
    fsubs f8, f6, f5                 # width
    lfs f0, D_ZERO(r27)
    fcmpo cr0, f8, f0
    ble DrawExit
    lfs f0, D_PUFF(r27)
    fmuls f9, f8, f0                 # puff
    lfs f0, D_BASE(r27)
    fnmsubs f10, f9, f0, f7          # base = high - puff*0.55
    stfs f5, 0x60(r1)
    stfs f6, 0x64(r1)
    stfs f9, 0x68(r1)
    stfs f10, 0x6C(r1)

    # Left puff
    lfs f0, D_SIDE(r27)
    fmadds f1, f9, f0, f5            # left + puff*0.95
    fmr f2, f10
    fmr f3, f9
    lfs f0, D_FLAT(r27)
    fmuls f4, f9, f0
    mr r3, r27
    bl Ellipse
    # Right puff
    lfs f9, 0x68(r1)
    lfs f6, 0x64(r1)
    lfs f0, D_SIDE(r27)
    fnmsubs f1, f9, f0, f6           # right - puff*0.95
    lfs f2, 0x6C(r1)
    fmr f3, f9
    lfs f0, D_FLAT(r27)
    fmuls f4, f9, f0
    mr r3, r27
    bl Ellipse
    # Middle puff
    lfs f5, 0x60(r1)
    lfs f6, 0x64(r1)
    lfs f9, 0x68(r1)
    fadds f1, f5, f6
    lfs f0, D_HALF(r27)
    fmuls f1, f1, f0                 # center x
    lfs f0, D_DROP(r27)
    lfs f2, 0x6C(r1)
    fnmsubs f2, f9, f0, f2           # base - puff*0.25
    lfs f0, D_BIG(r27)
    fmuls f3, f9, f0
    fmr f4, f9
    mr r3, r27
    bl Ellipse

    # Floor lines last, brightest.
    lwz r24, CollLines(r13)
    cmpwi r24, 0
    beq DrawExit
    lhz r26, 0x2(r29)                # floor_num
    lhz r25, 0x0(r29)                # floor_start
FloorLoop:
    cmpwi r26, 0
    beq DrawExit
    slwi r3, r25, 3
    lwzx r23, r24, r3                # CollLineDesc *
    cmpwi r23, 0
    beq FloorNext
    li r3, 0xA8                      # GX_LINES
    li r4, 2
    li r5, 18
    bl Begin
    lis r22, 0xCC00
    ori r22, r22, 0x8000
    lwz r21, D_TOP(r27)
    lfs f0, D_ZERO(r27)
    lha r3, 0x0(r23)                 # vert_prev
    mulli r3, r3, 0x18
    add r3, r3, r28
    lfs f1, 0x8(r3)
    lfs f2, 0xC(r3)
    stfs f1, 0(r22)
    stfs f2, 0(r22)
    stfs f0, 0(r22)
    stw r21, 0(r22)
    lha r3, 0x2(r23)                 # vert_next
    mulli r3, r3, 0x18
    add r3, r3, r28
    lfs f1, 0x8(r3)
    lfs f2, 0xC(r3)
    stfs f1, 0(r22)
    stfs f2, 0(r22)
    stfs f0, 0(r22)
    stw r21, 0(r22)
FloorNext:
    addi r25, r25, 1
    subi r26, r26, 1
    b FloorLoop

DrawExit:
    lmw r20, 0x8(r1)
    lwz r0, 0x104(r1)
    addi r1, r1, 0x100
    mtlr r0
    blr

########################################################
## Ellipse(r3 = data, f1 cx, f2 cy, f3 rx, f4 ry): fill + rim
########################################################
Ellipse:
    mflr r0
    stw r0, 0x4(r1)
    stwu r1, -0x40(r1)
    stmw r28, 0x30(r1)
    mr r31, r3
    stfs f1, 0x10(r1)
    stfs f2, 0x14(r1)
    stfs f3, 0x18(r1)
    stfs f4, 0x1C(r1)
    lis r30, 0xCC00
    ori r30, r30, 0x8000

    li r3, 0xA0                      # GX_TRIANGLEFAN
    li r4, SEGMENTS + 2
    li r5, 0
    bl Begin
    lwz r29, D_FILL(r31)
    lfs f0, D_ZERO(r31)
    lfs f1, 0x10(r1)
    lfs f2, 0x14(r1)
    stfs f1, 0(r30)
    stfs f2, 0(r30)
    stfs f0, 0(r30)
    stw r29, 0(r30)
    bl Rim

    li r3, 0xB0                      # GX_LINESTRIP
    li r4, SEGMENTS + 1
    li r5, 6
    bl Begin
    lwz r29, D_EDGE(r31)
    bl Rim

    lmw r28, 0x30(r1)
    lwz r0, 0x44(r1)
    addi r1, r1, 0x40
    mtlr r0
    blr

    # Leaf helper on Ellipse's frame: emits SEGMENTS+1 rim points in color r29.
Rim:
    addi r3, r31, D_CIRCLE
    li r4, SEGMENTS + 1
    lfs f0, D_ZERO(r31)
    lfs f5, 0x10(r1)
    lfs f6, 0x14(r1)
    lfs f7, 0x18(r1)
    lfs f8, 0x1C(r1)
RimLoop:
    lfs f1, 0x0(r3)
    lfs f2, 0x4(r3)
    fmadds f1, f1, f7, f5            # cx + cos*rx
    fmadds f2, f2, f8, f6            # cy + sin*ry
    stfs f1, 0(r30)
    stfs f2, 0(r30)
    stfs f0, 0(r30)
    stw r29, 0(r30)
    addi r3, r3, 8
    subic. r4, r4, 1
    bgt RimLoop
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
    .long 0x5C5C5C5C                 # fill: white at alpha 92, premultiplied
    .long 0xE6E6E6E6                 # rim
    .long 0xFFFFFFFF                 # platform line
    .float 0.0
    .float 0.24
    .float 0.55
    .float 0.95
    .float 0.8
    .float 1.25
    .float 0.25
    .float 0.5
    .float 6.0
    .float 1.0, 0.0
    .float 0.9238795, 0.3826834
    .float 0.7071068, 0.7071068
    .float 0.3826834, 0.9238795
    .float 0.0, 1.0
    .float -0.3826834, 0.9238795
    .float -0.7071068, 0.7071068
    .float -0.9238795, 0.3826834
    .float -1.0, 0.0
    .float -0.9238795, -0.3826834
    .float -0.7071068, -0.7071068
    .float -0.3826834, -0.9238795
    .float 0.0, -1.0
    .float 0.3826834, -0.9238795
    .float 0.7071068, -0.7071068
    .float 0.9238795, -0.3826834
    .float 1.0, 0.0

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
    lwz r12, 0x0044(r31)             # Original instruction
