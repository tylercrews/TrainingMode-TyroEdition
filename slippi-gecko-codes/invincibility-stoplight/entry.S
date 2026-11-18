/* One relocatable C2 body at the fighter GX callback's second instruction
 * (0x80080E1C, stw r0,4(r1)), wrapping the rest of the fighter draw. The
 * callback's mflr r0 at 0x80080E18 has already run, so r0 and LR hold its
 * return address, whether the game called it or the Invincibility Overlay
 * code's own wrapper at 0x80080E18 did. Only GOT/data pointers need relocating. */
.section .entry,"ax"
.global blob_start, reloc_done, reloc_offset, reloc_count, native_draw
blob_start:
    b draw_entry
reloc_done: .long 0
reloc_offset: .long 0
reloc_count: .long 0

draw_entry:
    stw r0,4(r1)
    stwu r1,-16(r1)
    bl relocate
    bl gecko_draw
    lwz r0,20(r1)
    addi r1,r1,16
    mtlr r0
    blr

relocate:
    mflr r12
    bl 1f
1:  mflr r11
    addi r11,r11,-(1b-blob_start)
    lwz r10,reloc_done-blob_start(r11)
    cmpwi r10,0
    bne 3f
    lwz r10,reloc_offset-blob_start(r11)
    add r10,r10,r11
    lwz r9,reloc_count-blob_start(r11)
    cmpwi r9,0
    beq 2f
    mtctr r9
0:  lwz r9,0(r10)
    add r9,r9,r11
    lwz r8,0(r9)
    add r8,r8,r11
    stw r8,0(r9)
    addi r10,r10,4
    bdnz 0b
2:  li r10,1
    stw r10,reloc_done-blob_start(r11)
3:  mtlr r12
    blr

/* The displaced mflr/stw, then the unmodified rest of the fighter GX callback,
 * which still calls 0x800805C8 (where Actionable Yellow-Green hooks) inside. */
native_draw:
    mflr r0
    stw r0,4(r1)
    lis r12,0x8008
    ori r12,r12,0x0e20
    mtctr r12
    bctr
