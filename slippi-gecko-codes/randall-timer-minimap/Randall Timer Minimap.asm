    # To be inserted at 8016e750
    # Randall Timer Minimap for vanilla NTSC 1.02 Melee / Slippi (standalone Gecko code).
    # On Yoshi's Story, draws a small map of Randall's loop through the match
    # timer: two dotted lines through the numbers, joined past each side, and a
    # red arrow where Randall is, pointing the way he's moving. The timer is the
    # stage: the arrow is behind the numbers, and see-through, exactly while
    # Randall is behind the stage. Visual only: it reads the stage, the timer and
    # the match clock and never writes game state, so it cannot desync.
    #
    # Match start (StartMelee) creates one GObj with a GX link 8 callback. The
    # game's HUD camera draws links 8, 10 and 11, and the timer's joints are in
    # 11, so on render pass 0 the map draws before the numbers, which cover it.
    # Each frame the callback reads Randall's cloud joint (the first child of
    # GrSt.dat's root, which the game puts under a scale joint as map GObj 2's
    # JObj) or, with no Randall, puts him where the timer's time has him. It
    # measures the timer from its digits' joints and maps the stage between its
    # walls (X +/-52.675) onto the timer's width.
    # The same drawing as Training Mode's src/stage_overlay_game.c (RandallTimer_GX).

    .macro branchl reg, address
    lis \reg, \address @h
    ori \reg, \reg, \address @l
    mtctr \reg
    bctrl
    .endm

    # One vertex to the GX FIFO (r29): position, then its RGBA8 color.
    .macro vertex x, y, z, color
    stfs \x, 0(r29)
    stfs \y, 0(r29)
    stfs \z, 0(r29)
    stw \color, 0(r29)
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
    .set HSD_StateSetPointSize, 0x80361C60
    .set HSD_SetupRenderMode, 0x8036199C
    .set HSD_StateSetZMode, 0x80361BB8
    .set GXSetVtxAttrFmt, 0x8033C414
    .set GXSetVtxDesc, 0x8033BF00
    .set HSD_StateSetCullMode, 0x80361AD8
    .set GXBegin, 0x8033D0DC

    .set GRKIND_STORY, 10
    .set RANDALL_MAP_GOBJ, 2
    .set TimerGObj, 0x804A1078       # stc_matchhud->timer_gobj; 0 with no timer or in the last 5 seconds
    .set Match, 0x8046B6A0           # stc_match
    .set M_SECONDS, 0x28             # time_seconds
    .set M_MS, 0x2C                  # time_ms (u16), counting up 0-59 within each second
    .set M_TIMER, 0x24C8             # match.timer: the byte's low two bits
    .set M_START_SECONDS, 0x24D8     # match.timer_seconds
    .set M_START_MS, 0x24DC          # match.timer_subseconds (u8)
    .set MATCH_TIMER_COUNTDOWN, 2
    .set RANDALL_TIMER_OFFSET, 125   # Randall's animation frame, ahead of the timer's
    .set RANDALL_LOOP_FRAMES, 1200
    .set JOBJ_HIDDEN, 0x10
    .set DIGITS, 10                  # ScInfTim's joints
    .set PATH_POINTS, 13
    .set HALF_POINTS, 8
    .set DOTS_MAX, 256
    .set DOT_SIZE, 12                # Points, in sixths of a pixel: 2 pixels...
    .set DOT_EDGE, 24                # ...over a 4-pixel dark one.
    .set GX_TRIANGLES, 0x90
    .set GX_POINTS, 0xB8
    .set FIFO, 0xCC008000

    # Data table offsets
    .set D_OUT_X, 0x00               # The stage's walls at Randall's heights
    .set D_MID_Y, 0x04               # Halfway between Randall's two heights
    .set D_SCALE_Y, 0x08             # HUD units per stage unit, up
    .set D_ZERO, 0x0C
    .set D_HALF, 0x10
    .set D_ONE, 0x14
    .set D_THREE, 0x18
    .set D_WIDE, 0x1C                # The timer's edges start this far out
    .set D_FAR, 0x20                 # A real position is nearer than this
    .set D_PAIR, 0x24                # Two dots' spacing
    .set D_PAIRS_MAX, 0x28           # DOTS_MAX / 4
    .set D_LAP, 0x2C                 # RANDALL_LOOP_FRAMES
    .set D_CORNER, 0x30
    .set D_NUDGE, 0x34
    .set D_HUGE, 0x38
    .set D_DOT_COLORS, 0x3C          # Dark dot past the timer, behind it; light dot past, behind
    .set D_ARROW_COLORS, 0x4C        # Outline past, behind; fill past, behind
    .set D_WIDTHS, 0x5C              # DIGITS half widths
    .set D_PATH, 0x84                # PATH_POINTS (x, y)
    .set D_HALVES, 0xEC              # 2 x HALF_POINTS (x, y): the right half, then the left
    .set D_ARROW, 0x16C              # 2 x 9 (along, across): the outline, then the fill

    # Update's stack frame
    .set FRAME, 0xD0
    .set SCRATCH, 0x08               # A double, for int and float conversions
    .set POS, 0x10                   # A Vec3
    .set STEPS, 0x20                 # The path's 12 steps, or the right half's 7 and its length...
    .set RIGHT_LENGTH, STEPS + 0x1C
    .set LEFT_STEPS, STEPS + 0x20    # ...then the left half's
    .set LEFT_LENGTH, STEPS + 0x3C
    .set GPRS, 0x60                  # r22-r31
    .set FPRS, 0x90                  # f24-f31

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

    li r3, 0
    li r4, 7
    li r5, 0
    branchl r12, GObj_Create
    cmpwi r3, 0
    beq StartExit
    bl GetUpdate                     # LR = Update; GetUpdate continues to StartExit.

###########################################
## GX callback: void Update(GOBJ *, int pass)
## Registers kept across calls: r31 the data table, r29 the FIFO, f31/f30
## Randall (stage units, then the arrow's center on the map), f29/f28 the
## timer's middle, f27/f26 HUD units per stage unit across and up.
###########################################
Update:
    cmpwi r4, 0
    bnelr                            # Pass 0 only: before the numbers
    stwu r1, -FRAME(r1)
    mflr r0
    stw r0, FRAME + 4(r1)
    stmw r22, GPRS(r1)
    stfd f24, FPRS + 0x00(r1)
    stfd f25, FPRS + 0x08(r1)
    stfd f26, FPRS + 0x10(r1)
    stfd f27, FPRS + 0x18(r1)
    stfd f28, FPRS + 0x20(r1)
    stfd f29, FPRS + 0x28(r1)
    stfd f30, FPRS + 0x30(r1)
    stfd f31, FPRS + 0x38(r1)
    bl DataTable
    mflr r31
    lis r29, FIFO@h
    ori r29, r29, FIFO@l

    branchl r12, Stage_GetExternalID
    branchl r12, Stage_ExternalToInternal
    cmpwi r3, GRKIND_STORY
    bne Exit

    # Randall, if he's there: map GObj 2, its scale joint, GrSt.dat's root, the cloud.
    li r3, RANDALL_MAP_GOBJ
    branchl r12, Stage_GetMapGObj    # Null once hazards remove Randall.
    cmpwi r3, 0
    beq ByTimer
    lwz r4, 0x2C(r3)                 # MapData
    cmpwi r4, 0
    beq ByTimer
    lwz r4, 0x14(r4)                 # MapData index
    cmpwi r4, RANDALL_MAP_GOBJ
    bne ByTimer
    lwz r3, 0x28(r3)                 # The game's scale joint (801C1768)
    cmpwi r3, 0
    beq ByTimer
    lwz r3, 0x10(r3)                 # Its child: GrSt.dat's root, which never moves
    cmpwi r3, 0
    beq Exit
    lwz r3, 0x10(r3)                 # Its first child: the cloud
    cmpwi r3, 0
    beq Exit
    li r4, 0
    addi r5, r1, POS
    branchl r12, JOBJ_GetWorldPosition
    lfs f31, POS(r1)
    lfs f30, POS + 4(r1)
    lfs f0, D_FAR(r31)
    fabs f1, f31
    fcmpo cr0, f1, f0
    bnl Exit                         # Not a real position (or not a number)
    fabs f1, f30
    fcmpo cr0, f1, f0
    bnl Exit
    b MeasureTimer

    # No Randall: where a match has him at the timer's time. His frame is the
    # frames the timer shows as run plus the offset, around his 1200-frame
    # loop, and he rides his path at a constant speed from its first point.
ByTimer:
    lis r5, Match@h
    ori r5, r5, Match@l
    lwz r3, M_SECONDS(r5)
    lhz r4, M_MS(r5)
    mulli r6, r3, 60
    add r6, r6, r4                   # The time it shows, counting up
    lbz r0, M_TIMER(r5)
    clrlwi r0, r0, 30
    cmpwi r0, MATCH_TIMER_COUNTDOWN
    bne TimerFrame
    lbz r7, M_START_MS(r5)           # Counting down, ms still counts up, from 59 at the start.
    li r8, 59
    cmpwi r7, 0
    beq 0f
    subi r8, r7, 1
0:  lwz r7, M_START_SECONDS(r5)
    mulli r7, r7, 60
    addi r7, r7, 59
    subf r7, r8, r7                  # Frames left at the start
    mulli r6, r3, 60
    addi r6, r6, 59
    subf r6, r4, r6                  # Frames left now
    subf r6, r6, r7                  # Frames run
TimerFrame:
    addi r6, r6, RANDALL_TIMER_OFFSET
    li r7, RANDALL_LOOP_FRAMES
    divwu r8, r6, r7
    mullw r8, r8, r7
    subf r3, r8, r6
    bl IntToFloat
    fmr f8, f1                       # His frame
    addi r24, r31, D_PATH
    addi r23, r1, STEPS
    li r22, PATH_POINTS - 1
    lfs f7, D_ZERO(r31)              # The path's length
PathStep:
    lfs f1, 0(r24)
    lfs f2, 4(r24)
    lfs f3, 8(r24)
    lfs f4, 12(r24)
    fsubs f1, f3, f1
    fsubs f2, f4, f2
    fmuls f2, f2, f2
    fmadds f1, f1, f1, f2
    bl Sqrt
    stfs f1, 0(r23)
    fadds f7, f7, f1
    addi r24, r24, 8
    addi r23, r23, 4
    subic. r22, r22, 1
    bne PathStep
    fmuls f1, f7, f8
    lfs f0, D_LAP(r31)
    fdivs f1, f1, f0                 # How far along it he is
    addi r3, r31, D_PATH
    addi r4, r1, STEPS
    li r5, PATH_POINTS
    bl Walk
    fmr f31, f1
    fmr f30, f2

    # The timer: its root's height, and the edges of the joints it shows.
MeasureTimer:
    lis r3, TimerGObj@ha
    lwz r3, TimerGObj@l(r3)
    cmpwi r3, 0
    beq Exit
    lwz r28, 0x28(r3)                # Its root joint
    cmpwi r28, 0
    beq Exit
    lwz r0, 0x14(r28)
    andi. r0, r0, JOBJ_HIDDEN
    bne Exit                         # The game hides it.
    lfs f25, D_WIDE(r31)             # Left edge
    fneg f24, f25                    # Right edge
    lwz r27, 0x10(r28)               # First joint
    li r26, 0
DigitLoop:
    cmpwi r27, 0
    beq DigitsDone
    cmpwi r26, DIGITS
    bge DigitsDone
    lwz r0, 0x14(r27)
    andi. r0, r0, JOBJ_HIDDEN
    bne DigitNext                    # The hours, until an hour has passed
    mr r3, r27
    li r4, 0
    addi r5, r1, POS
    branchl r12, JOBJ_GetWorldPosition
    lfs f1, POS(r1)
    slwi r3, r26, 2
    add r3, r3, r31
    lfs f2, D_WIDTHS(r3)
    fsubs f3, f1, f2
    fcmpo cr0, f3, f25
    bnl DigitRight
    fmr f25, f3
DigitRight:
    fadds f3, f1, f2
    fcmpo cr0, f3, f24
    bng DigitNext
    fmr f24, f3
DigitNext:
    lwz r27, 0x8(r27)                # Sibling
    addi r26, r26, 1
    b DigitLoop
DigitsDone:
    fcmpo cr0, f24, f25
    bng Exit                         # No joint showing
    mr r3, r28
    li r4, 0
    addi r5, r1, POS
    branchl r12, JOBJ_GetWorldPosition
    lfs f28, POS + 4(r1)
    lfs f0, D_HALF(r31)
    fadds f29, f25, f24
    fmuls f29, f29, f0               # The timer's middle
    fsubs f27, f24, f25
    fmuls f27, f27, f0
    lfs f0, D_OUT_X(r31)
    fdivs f27, f27, f0               # The walls land on its edges.
    lfs f26, D_SCALE_Y(r31)          # Randall's heights land 0.6 above and below its middle.

    # The track: each half of the loop, from the bottom line's middle around an
    # end to the top line's middle, with its steps' lengths on the map.
    li r25, 0                        # 0 the right half, 1 the left
HalfLoop:
    slwi r3, r25, 6
    add r24, r31, r3
    addi r24, r24, D_HALVES
    slwi r3, r25, 5
    add r23, r1, r3
    addi r23, r23, STEPS
    lfs f7, D_ZERO(r31)              # Its length
    li r22, HALF_POINTS - 1
HalfStep:
    lfs f1, 0(r24)
    lfs f2, 4(r24)
    bl MapPoint
    fmr f5, f1
    fmr f6, f2
    lfs f1, 8(r24)
    lfs f2, 12(r24)
    bl MapPoint
    fsubs f1, f1, f5
    fsubs f2, f2, f6
    fmuls f2, f2, f2
    fmadds f1, f1, f1, f2
    bl Sqrt
    stfs f1, 0(r23)
    fadds f7, f7, f1
    addi r24, r24, 8
    addi r23, r23, 4
    subic. r22, r22, 1
    bne HalfStep
    stfs f7, 0(r23)
    addi r25, r25, 1
    cmpwi r25, 2
    blt HalfLoop

    # The same even number of dots on each half, from the right half's length,
    # at the same fractions of each half: centered, so the timer's middle and
    # each end's middle fall between two dots.
    lfs f0, D_ZERO(r31)
    lfs f1, RIGHT_LENGTH(r1)
    fcmpo cr0, f1, f0
    bng Arrow
    lfs f2, LEFT_LENGTH(r1)
    fcmpo cr0, f2, f0
    bng Arrow
    lfs f0, D_PAIR(r31)
    fdivs f1, f1, f0
    lfs f0, D_HALF(r31)
    fadds f1, f1, f0                 # Pairs of dots, rounded
    li r24, DOTS_MAX / 2
    lfs f0, D_PAIRS_MAX(r31)
    fcmpo cr0, f1, f0
    bnl PerHalfMin
    fctiwz f1, f1
    stfd f1, SCRATCH(r1)
    lwz r24, SCRATCH + 4(r1)
    slwi r24, r24, 1
PerHalfMin:
    cmpwi r24, 2
    bge PerHalfDone
    li r24, 2
PerHalfDone:
    mr r3, r24
    bl IntToFloat
    fmr f25, f1                      # Dots a half

    # A dark dot under each light one, dimmer behind the numbers.
    li r23, 0                        # 0 the dark dots, 1 the light ones
DotPass:
    li r3, GX_POINTS
    slwi r4, r24, 1
    li r5, DOT_EDGE
    cmpwi r23, 0
    beq DotBegin
    li r5, DOT_SIZE
DotBegin:
    bl Begin
    li r22, 0
DotLoop:
    mr r3, r22
    bl IntToFloat
    lfs f0, D_HALF(r31)
    fadds f1, f1, f0
    fdivs f7, f1, f25                # (i + 0.5) / dots a half
    lfs f1, RIGHT_LENGTH(r1)
    fmuls f1, f7, f1
    addi r3, r31, D_HALVES
    addi r4, r1, STEPS
    li r5, HALF_POINTS
    bl Walk
    bl Dot
    lfs f1, LEFT_LENGTH(r1)
    fmuls f1, f7, f1
    addi r3, r31, D_HALVES + HALF_POINTS * 8
    addi r4, r1, LEFT_STEPS
    li r5, HALF_POINTS
    bl Walk
    bl Dot
    addi r22, r22, 1
    cmpw r22, r24
    blt DotLoop
    addi r23, r23, 1
    cmpwi r23, 2
    blt DotPass

    # The arrow points along the path segment nearest Randall (on a corner,
    # the one leaving it).
Arrow:
    li r22, 0                        # Nearest segment
    lfs f8, D_HUGE(r31)              # Its distance, squared
    li r23, 0
    addi r24, r31, D_PATH
Segment:
    lfs f1, 0(r24)
    lfs f2, 4(r24)
    lfs f3, 8(r24)
    lfs f4, 12(r24)
    fsubs f3, f3, f1
    fsubs f4, f4, f2
    fsubs f5, f31, f1
    fsubs f6, f30, f2
    fmuls f6, f6, f4
    fmadds f5, f5, f3, f6
    fmuls f6, f4, f4
    fmadds f6, f3, f3, f6
    fdivs f5, f5, f6                 # How far along it he's nearest, 0 to 1
    lfs f0, D_ZERO(r31)
    fcmpo cr0, f5, f0
    bnl SegmentHigh
    fmr f5, f0
SegmentHigh:
    lfs f0, D_ONE(r31)
    fcmpo cr0, f5, f0
    bng SegmentNear
    fmr f5, f0
SegmentNear:
    fmadds f1, f3, f5, f1
    fsubs f1, f1, f31
    fmadds f2, f4, f5, f2
    fsubs f2, f2, f30
    fmuls f2, f2, f2
    fmadds f1, f1, f1, f2
    lfs f0, D_CORNER(r31)
    fcmpo cr0, f5, f0
    bng SegmentBest
    lfs f0, D_NUDGE(r31)             # At its end: the next segment wins.
    fadds f1, f1, f0
SegmentBest:
    fcmpo cr0, f1, f8
    bnl SegmentNext
    fmr f8, f1
    mr r22, r23
SegmentNext:
    addi r24, r24, 8
    addi r23, r23, 1
    cmpwi r23, PATH_POINTS - 1
    blt Segment

    slwi r3, r22, 3
    add r24, r31, r3
    addi r24, r24, D_PATH
    lfs f1, 0(r24)
    lfs f2, 4(r24)
    bl MapPoint
    fmr f5, f1
    fmr f6, f2
    lfs f1, 8(r24)
    lfs f2, 12(r24)
    bl MapPoint
    fsubs f7, f1, f5
    fsubs f8, f2, f6
    fmuls f1, f8, f8
    fmadds f1, f7, f7, f1
    bl Sqrt
    lfs f0, D_ZERO(r31)
    fcmpo cr0, f1, f0
    bng Exit
    fdivs f24, f7, f1                # Its direction on the map
    fdivs f25, f8, f1
    fabs f3, f31
    lfs f0, D_OUT_X(r31)
    li r28, 0
    fcmpo cr0, f3, f0
    cror 2, 0, 2                     # <=
    bne ArrowCenter
    li r28, 4                        # Behind the stage: the see-through colors
ArrowCenter:
    fmr f1, f31
    fmr f2, f30
    bl MapPoint
    fmr f31, f1
    fmr f30, f2

    li r23, 0                        # 0 the outline, 1 the fill on it
ArrowPass:
    li r3, GX_TRIANGLES
    li r4, 9
    li r5, 0
    bl Begin
    mulli r3, r23, 72
    add r24, r31, r3
    addi r24, r24, D_ARROW
    slwi r3, r23, 3
    add r3, r3, r28
    add r3, r3, r31
    lwz r25, D_ARROW_COLORS(r3)
    lfs f9, D_ZERO(r31)
    li r22, 9
ArrowVertex:
    lfs f1, 0(r24)                   # Along the direction
    lfs f2, 4(r24)                   # Across it, to the left
    fmadds f3, f24, f1, f31
    fnmsubs f3, f25, f2, f3
    fmadds f4, f25, f1, f30
    fmadds f4, f24, f2, f4
    vertex f3, f4, f9, r25
    addi r24, r24, 8
    subic. r22, r22, 1
    bne ArrowVertex
    addi r23, r23, 1
    cmpwi r23, 2
    blt ArrowPass

Exit:
    lfd f24, FPRS + 0x00(r1)
    lfd f25, FPRS + 0x08(r1)
    lfd f26, FPRS + 0x10(r1)
    lfd f27, FPRS + 0x18(r1)
    lfd f28, FPRS + 0x20(r1)
    lfd f29, FPRS + 0x28(r1)
    lfd f30, FPRS + 0x30(r1)
    lfd f31, FPRS + 0x38(r1)
    lmw r22, GPRS(r1)
    lwz r0, FRAME + 4(r1)
    mtlr r0
    addi r1, r1, FRAME
    blr

########################################################
## Leaf helpers, on Update's frame and data table (r31).
########################################################

    # MapPoint(f1, f2 stage units) -> f1, f2 on the map. Uses f0.
MapPoint:
    fmadds f1, f1, f27, f29
    lfs f0, D_MID_Y(r31)
    fsubs f2, f2, f0
    fmadds f2, f2, f26, f28
    blr

    # Sqrt(f1) -> f1, as the game's sqrtf (8000D5BC). Uses f0, f2-f4.
Sqrt:
    lfs f0, D_ZERO(r31)
    fcmpo cr0, f1, f0
    blelr
    frsqrte f2, f1
    lfs f4, D_HALF(r31)
    lfs f3, D_THREE(r31)
    fmul f0, f2, f2
    fmul f2, f4, f2
    fnmsub f0, f1, f0, f3
    fmul f2, f2, f0
    fmul f0, f2, f2
    fmul f2, f4, f2
    fnmsub f0, f1, f0, f3
    fmul f2, f2, f0
    fmul f0, f2, f2
    fmul f2, f4, f2
    fnmsub f0, f1, f0, f3
    fmul f0, f2, f0
    fmul f0, f1, f0
    frsp f1, f0
    blr

    # Walk(r3 points, r4 their steps' lengths, r5 points, f1 how far) -> f1, f2:
    # that far along them. Uses r0, r3-r6, f0-f6.
Walk:
    li r6, 1
WalkStep:
    lfs f3, 0(r4)
    fcmpo cr0, f1, f3
    cror 2, 0, 2                     # <=
    beq WalkHere
    addi r0, r6, 1
    cmpw r0, r5
    beq WalkHere                     # The last step
    fsubs f1, f1, f3
    addi r3, r3, 8
    addi r4, r4, 4
    addi r6, r6, 1
    b WalkStep
WalkHere:
    lfs f0, D_ZERO(r31)
    fmr f4, f0
    fcmpo cr0, f3, f0
    bng WalkClamp
    fdivs f4, f1, f3
WalkClamp:
    fcmpo cr0, f4, f0
    bnl WalkHigh
    fmr f4, f0
WalkHigh:
    lfs f0, D_ONE(r31)
    fcmpo cr0, f4, f0
    bng WalkPoint
    fmr f4, f0
WalkPoint:
    lfs f5, 0(r3)
    lfs f6, 8(r3)
    fsubs f6, f6, f5
    fmadds f1, f6, f4, f5
    lfs f5, 4(r3)
    lfs f6, 12(r3)
    fsubs f6, f6, f5
    fmadds f2, f6, f4, f5
    blr

    # Dot(f1, f2 stage units, r23 the pass): one dot, dimmer behind the
    # numbers. Uses r3, f0-f3.
Dot:
    fabs f3, f1
    lfs f0, D_OUT_X(r31)
    slwi r3, r23, 3
    fcmpo cr0, f3, f0
    cror 2, 0, 2                     # <=
    bne DotColor
    addi r3, r3, 4
DotColor:
    add r3, r3, r31
    lwz r3, D_DOT_COLORS(r3)
    fmadds f1, f1, f27, f29
    lfs f0, D_MID_Y(r31)
    fsubs f2, f2, f0
    fmadds f2, f2, f26, f28
    lfs f0, D_ZERO(r31)
    vertex f1, f2, f0, r3
    blr

    # IntToFloat(r3) -> f1. Uses r0, f0 and Update's scratch double.
IntToFloat:
    lis r0, 0x4330
    stw r0, SCRATCH(r1)
    lis r0, 0x8000
    stw r0, SCRATCH + 4(r1)
    lfd f0, SCRATCH(r1)
    xoris r3, r3, 0x8000
    stw r3, SCRATCH + 4(r1)
    lfd f1, SCRATCH(r1)
    fsub f1, f1, f0
    frsp f1, f1
    blr

########################################################
## Begin(r3 shape, r4 count, r5 point size and line width):
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
    .float 52.675                    # D_OUT_X: RANDALL_TIMER_OUT_X in src/stage_overlay_game.c
    .float -28.7                     # D_MID_Y: (-18.9 + -38.5) / 2
    .float 0.0612244904              # D_SCALE_Y: 0.6 / ((-18.9 - -38.5) / 2)
    .float 0.0                       # D_ZERO
    .float 0.5                       # D_HALF
    .float 1.0                       # D_ONE
    .float 3.0                       # D_THREE
    .float 1e9                       # D_WIDE
    .float 1000.0                    # D_FAR
    .float 1.6                       # D_PAIR: 2 x RANDALL_TIMER_DOT_SPACING (0.8)
    .float 64.0                      # D_PAIRS_MAX
    .float 1200.0                    # D_LAP
    .float 0.999                     # D_CORNER
    .float 1e-4                      # D_NUDGE
    .float 1e30                      # D_HUGE
    # Premultiplied RGBA: past the timer, then behind it (see RANDALL_TIMER_*_COLOR).
    .long 0x000000A0, 0x00000037     # Dark dot: black at 160, at 55 behind
    .long 0xD3D3D3E6, 0x49494950     # Light dot: 235 gray at 230, at 80 behind
    .long 0x000000DC, 0x00000037     # Arrow outline: black at 220, at 55 behind
    .long 0xEB2828FF, 0x8A171796     # Arrow: red (235, 40, 40), at 150 behind
    # D_WIDTHS: half of each of ScInfTim's quads, in ifTime's joint order: the
    # hour and minute separators, then hours, minutes, centiseconds and seconds.
    .float 0.6015625, 0.6015625
    .float 1.30078125, 1.30078125, 1.30078125, 1.30078125
    .float 0.98828125, 0.98828125
    .float 1.30078125, 1.30078125
    # D_PATH: Randall's path in stage units, from the bottom-left corner where
    # the cloud starts, counter-clockwise and back.
    .float -95.725, -38.5, 95.725, -38.5, 96.95, -37.975, 97.475, -36.75
    .float 97.475, -20.475, 96.95, -19.25, 95.725, -18.9, -95.725, -18.9
    .float -96.95, -19.425, -97.475, -20.475, -97.475, -36.75, -96.95, -37.8
    .float -95.725, -38.5
    # D_HALVES: the bottom line's middle, around the right end (path points
    # 1-6) to the top line's middle; then around the left end (0, 11-7).
    .float 0.0, -38.5, 95.725, -38.5, 96.95, -37.975, 97.475, -36.75
    .float 97.475, -20.475, 96.95, -19.25, 95.725, -18.9, 0.0, -18.9
    .float 0.0, -38.5, -95.725, -38.5, -96.95, -37.8, -97.475, -36.75
    .float -97.475, -20.475, -96.95, -19.425, -95.725, -18.9, 0.0, -18.9
    # D_ARROW: a head and a shaft, 1.3 tail to tip, head 0.65 long and 0.96
    # wide, shaft 0.34 wide, as three triangles; first grown 0.13 all round
    # for the dark outline.
    .float 0.868839324, 0.0, -0.13, 0.737604499, -0.13, -0.737604499
    .float -0.78, -0.3, -0.13, 0.3, -0.78, 0.3
    .float -0.78, -0.3, -0.13, -0.3, -0.13, 0.3
    .float 0.65, 0.0, 0.0, 0.48, 0.0, -0.48
    .float -0.65, -0.17, 0.0, 0.17, -0.65, 0.17
    .float -0.65, -0.17, 0.0, -0.17, 0.0, 0.17

GetUpdate:
    mflr r4                          # Update
    li r5, 8                         # GX link 8: the HUD camera's, under the timer
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
