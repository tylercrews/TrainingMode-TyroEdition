/* One relocatable C2 body owns code/data. The simulation hook calls entry + 4.
 * Only GOT/data pointers need relocating; executable branches remain relative. */
.section .entry,"ax"
.global blob_start, reloc_done, reloc_offset, reloc_count, native_draw
blob_start:
    b draw_entry
    b tick_entry
reloc_done: .long 0
reloc_offset: .long 0
reloc_count: .long 0

.macro entry name, function
\name:
    mflr r0
    stw r0,4(r1)
    stwu r1,-16(r1)
    bl relocate
    bl \function
    lwz r0,20(r1)
    addi r1,r1,16
    mtlr r0
    blr
.endm
entry draw_entry, gecko_draw
entry tick_entry, gecko_tick

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

/* Displaced mflr plus return into the unmodified lower-level renderer.
 * The normal fighter draw uses this function; FoD's reflection is separate. */
native_draw:
    mflr r0
    lis r12,0x8008
    ori r12,r12,0x05cc
    mtctr r12
    bctr
