/* Injected at Fighter_procCamera (0x8006D9EC), after collision processing.
 * Preserve the complete injection context, including floating-point status. */
.text
.global _start
_start:
    stwu r1,-0x120(r1)
    stw r0,8(r1)
    mflr r0
    stw r0,12(r1)
    mfcr r0
    stw r0,16(r1)
    mfctr r0
    stw r0,20(r1)
    mfxer r0
    stw r0,24(r1)
    stmw r3,28(r1)
    .set n,0
    .rept 14
    stfd n,144+8*n(r1)
    .set n,n+1
    .endr
    mffs f0
    stfd f0,256(r1)
    /* Decode the render C2's relative branch to discover its allocation. */
    lis r12,0x8008
    ori r12,r12,0x05c8
    lwz r11,0(r12)
    rlwinm r11,r11,0,6,29
    slwi r11,r11,6
    srawi r11,r11,6
    add r12,r12,r11
    addi r12,r12,4
    mtctr r12
    bctrl
    lfd f0,256(r1)
    mtfsf 255,f0
    .set n,0
    .rept 14
    lfd n,144+8*n(r1)
    .set n,n+1
    .endr
    lmw r3,28(r1)
    lwz r0,24(r1)
    mtxer r0
    lwz r0,20(r1)
    mtctr r0
    lwz r0,16(r1)
    mtcr r0
    lwz r0,12(r1)
    mtlr r0
    lwz r0,8(r1)
    addi r1,r1,0x120
    mflr r0 /* original instruction */
