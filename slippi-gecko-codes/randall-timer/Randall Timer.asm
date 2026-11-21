    # To be inserted at 8016e750
    # Randall Timer for vanilla NTSC 1.02 Melee / Slippi (standalone Gecko code).
    # On Yoshi's Story, tints the match timer purple while Randall is out past
    # the stage's left side and teal while it's out past the right. Visual only:
    # it changes the timer HUD's material colors, which the game never reads
    # back, so it cannot desync.
    #
    # Match start (StartMelee) creates one GObj with a GX link 5 callback and
    # forgets the last match's timer. Each frame the callback reads where
    # Randall's cloud joint is (the first child of GrSt.dat's root, which the
    # game puts under a scale joint as map GObj 2's JObj; the cloud rides the
    # stage's path animation). When the side it's out on changes, it sets every
    # timer material's diffuse color to the material's own color times the tint,
    # or back to its own color. The digits are textures multiplied by that color.
    # The same logic as Training Mode's src/stage_overlay_game.c.

    .macro branchl reg, address
    lis \reg, \address @h
    ori \reg, \reg, \address @l
    mtctr \reg
    bctrl
    .endm

    # One channel of r4 (the material's color) times r28's (the tint's), over
    # r8 (255), into r5. rot brings the channel to the low byte.
    .macro channel rot, first, last
    rlwinm r6, r4, \rot, 24, 31
    rlwinm r7, r28, \rot, 24, 31
    mullw r6, r6, r7
    divwu r6, r6, r8
    rlwimi r5, r6, 32 - \rot, \first, \last
    .endm

    .set GObj_Create, 0x803901F0
    .set GObj_AddGXLink, 0x8039069C
    .set Stage_GetExternalID, 0x80225194
    .set Stage_ExternalToInternal, 0x8022519C
    .set Stage_GetMapGObj, 0x801C2BA4
    .set JOBJ_GetWorldPosition, 0x8000B1CC

    .set GRKIND_STORY, 10
    .set RANDALL_MAP_GOBJ, 2
    .set TimerGObj, 0x804A1078       # stc_matchhud->timer_gobj (0x804A0FD8 + 0xA0); 0 with no timer
    .set NOT_A_DOBJ, 0x4020          # JOBJ_SPLINE | JOBJ_PTCL: the joint's union isn't a DObj
    .set PARTS, 16                   # The vanilla timer has 10 materials.

    # Data table offsets
    .set D_OUT_X, 0x00               # Out past X = +/-52.675
    .set D_LEFT, 0x04                # Purple
    .set D_RIGHT, 0x08               # Teal
    .set D_ROOT, 0x0C                # The timer whose colors are saved (its root joint)
    .set D_SIDE, 0x10                # The side the timer shows: 0, -1 left, 1 right
    .set D_COUNT, 0x14               # Materials saved
    .set D_PARTS, 0x18               # PARTS x (material, its own diffuse color)

#############################
## Match start: create GObj
#############################
    stwu r1, -0x90(r1)
    stw r0, 0x8(r1)                  # Keep every register for the native code,
    mflr r0                          # r0, LR and CR included.
    stw r0, 0xC(r1)
    mfcr r0
    stw r0, 0x10(r1)
    stmw r3, 0x14(r1)

    bl DataTable
    mflr r3
    li r0, 0
    stw r0, D_ROOT(r3)               # A new match: forget the last one's timer,
    stw r0, D_SIDE(r3)               # even if this one's lands at the same address.
    stw r0, D_COUNT(r3)

    li r3, 0
    li r4, 7
    li r5, 0
    branchl r12, GObj_Create
    cmpwi r3, 0
    beq StartExit
    bl GetUpdate                     # LR = Update; GetUpdate continues to StartExit.

###########################################
## GX callback: void Update(GOBJ *, int pass)
###########################################
Update:
    cmpwi r4, 2
    bnelr
    mflr r0
    stw r0, 0x4(r1)
    stwu r1, -0x40(r1)
    stmw r26, 0x20(r1)

    bl DataTable
    mflr r31

    # The match's timer, if it has one.
    lis r3, TimerGObj@ha
    lwz r3, TimerGObj@l(r3)
    cmpwi r3, 0
    beq 0f
    lwz r3, 0x28(r3)                 # Its root joint
0:  mr r30, r3
    lwz r4, D_ROOT(r31)
    cmpw r4, r30
    beq 1f
    stw r30, D_ROOT(r31)             # A new timer, or none: the old one's materials went with it.
    li r0, 0
    stw r0, D_SIDE(r31)
    stw r0, D_COUNT(r31)
1:  cmpwi r30, 0
    beq UpdateExit

    # Randall's side: 0 unless its cloud is out past a side of Yoshi's Story.
    li r29, 0
    branchl r12, Stage_GetExternalID
    branchl r12, Stage_ExternalToInternal
    cmpwi r3, GRKIND_STORY
    bne Decide
    li r3, RANDALL_MAP_GOBJ
    branchl r12, Stage_GetMapGObj    # Null once hazards remove Randall.
    cmpwi r3, 0
    beq Decide
    lwz r4, 0x2C(r3)                 # MapData
    cmpwi r4, 0
    beq Decide
    lwz r4, 0x14(r4)                 # MapData index
    cmpwi r4, RANDALL_MAP_GOBJ
    bne Decide
    lwz r3, 0x28(r3)                 # The game's scale joint (801C1768)
    cmpwi r3, 0
    beq Decide
    lwz r3, 0x10(r3)                 # Its child: GrSt.dat's root, which never moves
    cmpwi r3, 0
    beq Decide
    lwz r3, 0x10(r3)                 # Its first child: the cloud
    cmpwi r3, 0
    beq Decide
    li r4, 0
    addi r5, r1, 0x10
    branchl r12, JOBJ_GetWorldPosition
    lfs f1, 0x10(r1)                 # X
    lfs f0, D_OUT_X(r31)
    fcmpo cr0, f1, f0
    ble 2f                           # (Not a number reads as behind the stage.)
    li r29, 1                        # Out past the right side
    b Decide
2:  fneg f0, f0
    fcmpo cr0, f1, f0
    bge Decide
    li r29, -1                       # Out past the left side

Decide:
    lwz r3, D_SIDE(r31)
    cmpw r3, r29
    beq UpdateExit                   # Unchanged, or untinted and staying so: leave it alone.
    lwz r3, D_COUNT(r31)
    cmpwi r3, 0
    bne 3f
    mr r3, r30
    bl Save                          # Still untinted, so these are the timer's own colors.
3:  stw r29, D_SIDE(r31)

    # Each material: its own color, times the tint while Randall is out.
    lwz r28, D_RIGHT(r31)
    cmpwi r29, 0
    bgt 4f
    lwz r28, D_LEFT(r31)
4:  li r27, 0
    addi r26, r31, D_PARTS
    li r8, 255
TintLoop:
    lwz r3, D_COUNT(r31)
    cmpw r27, r3
    bge UpdateExit
    lwz r3, 0x0(r26)                 # Material
    lwz r4, 0x4(r26)                 # Its own diffuse color
    cmpwi r29, 0
    beq 5f
    mr r5, r4                        # Keeps its alpha.
    channel 8, 0, 7                  # Red
    channel 16, 8, 15                # Green
    channel 24, 16, 23               # Blue
    mr r4, r5
5:  stw r4, 0x4(r3)                  # material->diffuse
    addi r26, r26, 8
    addi r27, r27, 1
    b TintLoop

UpdateExit:
    lmw r26, 0x20(r1)
    lwz r0, 0x44(r1)
    addi r1, r1, 0x40
    mtlr r0
    blr

################################################################
## Save(r3 = a joint): appends the material of every DObj in that
## joint, its siblings and their children, in tree order, up to
## PARTS. r31 is the data table.
################################################################
Save:
    mflr r0
    stw r0, 0x4(r1)
    stwu r1, -0x20(r1)
    stw r30, 0x18(r1)
    stw r29, 0x14(r1)
    mr r30, r3
SaveJoint:
    cmpwi r30, 0
    beq SaveExit
    lwz r0, 0x14(r30)                # Flags
    andi. r0, r0, NOT_A_DOBJ
    bne SaveChild
    lwz r29, 0x18(r30)               # DObj
SaveDObj:
    cmpwi r29, 0
    beq SaveChild
    lwz r3, 0x8(r29)                 # MObj
    cmpwi r3, 0
    beq SaveNext
    lwz r3, 0xC(r3)                  # Material
    cmpwi r3, 0
    beq SaveNext
    lwz r4, D_COUNT(r31)
    cmpwi r4, PARTS
    bge SaveNext
    slwi r5, r4, 3
    add r5, r5, r31
    stw r3, D_PARTS(r5)
    lwz r6, 0x4(r3)                  # Diffuse
    stw r6, D_PARTS + 4(r5)
    addi r4, r4, 1
    stw r4, D_COUNT(r31)
SaveNext:
    lwz r29, 0x4(r29)                # Next DObj
    b SaveDObj
SaveChild:
    lwz r3, 0x10(r30)                # Child
    bl Save
    lwz r30, 0x8(r30)                # Sibling
    b SaveJoint
SaveExit:
    lwz r29, 0x14(r1)
    lwz r30, 0x18(r1)
    lwz r0, 0x24(r1)
    addi r1, r1, 0x20
    mtlr r0
    blr

DataTable:
    blrl
    .float 52.675                    # RANDALL_TIMER_OUT_X in src/stage_overlay_game.c
    .long 0xA064FFFF                 # RANDALL_TIMER_LEFT: Training Mode's purple
    .long 0x00B4A0FF                 # RANDALL_TIMER_RIGHT: its teal
    .long 0                          # D_ROOT
    .long 0                          # D_SIDE
    .long 0                          # D_COUNT
    .fill PARTS * 2, 4, 0            # D_PARTS

GetUpdate:
    mflr r4                          # Update
    li r5, 5                         # GX link 5, with the other overlays
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
    lis r4, 0x8017                   # Original instruction
