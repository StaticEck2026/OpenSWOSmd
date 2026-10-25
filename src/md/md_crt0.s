| OpenSWOS Mega Drive: vector table, ROM header and start-up.
|
| The vector table and header are what a Mega Drive (and TMSS) expect at 0x000000 and
| 0x000100; the code after them puts the hardware in a known state, copies .data from ROM
| to work RAM, clears .bss and calls main(). See docs/MEGADRIVE.md.
|
| GNU as, Motorola mnemonics, % register prefix optional (Makefile.md passes
| --register-prefix-optional).

        .section .vectors,"ax"
        .globl  _md_vectors
_md_vectors:
        .long   __stack_top             | 0  initial SSP
        .long   _md_start               | 1  initial PC
        .long   _md_exc_bus             | 2  bus error
        .long   _md_exc_addr            | 3  address error
        .long   _md_exc_ill             | 4  illegal instruction
        .long   _md_exc_zdiv            | 5  zero divide
        .long   _md_exc_other           | 6  CHK
        .long   _md_exc_other           | 7  TRAPV
        .long   _md_exc_priv            | 8  privilege
        .long   _md_exc_other           | 9  trace
        .long   _md_exc_ill             | 10 line A
        .long   _md_exc_ill             | 11 line F
        .rept   12
        .long   _md_exc_other           | 12-23 reserved
        .endr
        .long   _md_exc_other           | 24 spurious
        .long   _md_int_null            | 25 level 1
        .long   _md_int_ext             | 26 level 2 (external, pad TH)
        .long   _md_int_null            | 27 level 3
        .long   _md_int_hblank          | 28 level 4 (HBlank)
        .long   _md_int_null            | 29 level 5
        .long   _md_int_vblank          | 30 level 6 (VBlank)
        .long   _md_int_null            | 31 level 7
        .rept   16
        .long   _md_exc_other           | 32-47 TRAP #n
        .endr
        .rept   16
        .long   _md_exc_other           | 48-63 reserved
        .endr

        .section .header,"a"
        .ascii  "SEGA MEGA DRIVE "                                  | 0x100 system
        .ascii  "(C)OSWS 2026.OCT"                                  | 0x110 copyright
        .ascii  "SENSIBLE WORLD OF SOCCER  OPENSWOS              " | 0x120 domestic name (48)
        .ascii  "SENSIBLE WORLD OF SOCCER  OPENSWOS              " | 0x150 overseas name (48)
        .ascii  "GM OSWS0001-00"                                    | 0x180 serial
        .word   0                                                   | 0x18E checksum (scripts/md/fix_header.py)
        .ascii  "J6              "                                  | 0x190 I/O: 3 and 6 button pads
        .long   0x000000                                            | 0x1A0 ROM start
        .long   0x3FFFFF                                            | 0x1A4 ROM end (fix_header.py)
        .long   0xFF0000                                            | 0x1A8 RAM start
        .long   0xFFFFFF                                            | 0x1AC RAM end
        .ascii  "RA"                                                | 0x1B0 backup RAM: odd bytes, battery
        .byte   0xF8, 0x20
        .long   0x200001                                            | 0x1B4 SRAM start
        .long   0x203FFF                                            | 0x1B8 SRAM end (8 KB on odd bytes)
        .ascii  "            "                                      | 0x1BC modem
        .ascii  "                                        "          | 0x1C8 notes (40)
        .ascii  "JUE             "                                  | 0x1F0 regions

        .text
        .globl  _md_start
_md_start:
        move.w  #0x2700, sr
        | A cold boot leaves the pad port controls zero; a soft reset does not, and then
        | the hardware has been set up already -- still redo all of it below.
        | TMSS: write 'SEGA' if the console has the security chip (version != 0).
        move.b  0xA10001, d0
        andi.b  #0x0F, d0
        beq.s   1f
        move.l  #0x53454741, 0xA14000
1:
        | Read the VDP status once (clears a pending write state after reset).
        move.w  0xC00004, d0

        | Z80: take the bus, hold it in reset, load "di; jr $" so it idles, then let it run
        | until the sound driver (md_sound.c) gives it something to do.
        move.w  #0x0100, 0xA11100
        move.w  #0x0100, 0xA11200
2:      btst    #0, 0xA11100
        bne.s   2b
        lea     0xA00000, a0
        move.w  #0x1FFF, d1
3:      clr.b   (a0)+
        dbra    d1, 3b
        lea     0xA00000, a0
        move.b  #0xF3, (a0)+            | di
        move.b  #0x18, (a0)+            | jr $
        move.b  #0xFE, (a0)+
        move.w  #0x0000, 0xA11200       | reset
        moveq   #20, d1
4:      dbra    d1, 4b
        move.w  #0x0100, 0xA11200       | out of reset
        move.w  #0x0000, 0xA11100       | give back the bus

        | PSG: all four channels silent.
        lea     0xC00011, a0
        move.b  #0x9F, (a0)
        move.b  #0xBF, (a0)
        move.b  #0xDF, (a0)
        move.b  #0xFF, (a0)

        | Clear all of work RAM (the stack included).
        lea     0xFF0000, a0
        moveq   #0, d0
        move.w  #0x3FFF, d1
5:      move.l  d0, (a0)+
        dbra    d1, 5b

        | .data: ROM image -> RAM.
        lea     __data_load, a0
        lea     __data_start, a1
        move.l  #__data_end, d1
        sub.l   a1, d1
        lsr.l   #1, d1
        beq.s   7f
        subq.l  #1, d1
6:      move.w  (a0)+, (a1)+
        dbra    d1, 6b
        | dbra counts 16 bits; .data is under 64 KB anyway (work RAM is).
7:
        move.l  #__stack_top, sp
        jsr     main
8:      bra.s   8b

| ---- interrupts --------------------------------------------------------------------

        .globl  _md_int_vblank
_md_int_vblank:
        movem.l d0-d1/a0-a1, -(sp)
        jsr     md_vblank_irq
        movem.l (sp)+, d0-d1/a0-a1
        rte

_md_int_hblank:
        rte
_md_int_ext:
        rte
_md_int_null:
        rte

| ---- exceptions: md_panic shows the vector, PC and SR on screen --------------------
| Group 0 (bus/address error) pushes 8 bytes of access info before SR/PC.
_md_exc_bus:
        moveq   #2, d0
        bra.s   _md_exc_g0
_md_exc_addr:
        moveq   #3, d0
_md_exc_g0:
        move.l  2(sp), d1               | access address
        move.l  10(sp), a0              | PC
        move.w  8(sp), d2               | SR
        bra.s   _md_exc_common
_md_exc_ill:
        moveq   #4, d0
        bra.s   _md_exc_g1
_md_exc_zdiv:
        moveq   #5, d0
        bra.s   _md_exc_g1
_md_exc_priv:
        moveq   #8, d0
        bra.s   _md_exc_g1
_md_exc_other:
        moveq   #-1, d0
_md_exc_g1:
        moveq   #0, d1
        move.l  2(sp), a0
        move.w  (sp), d2
_md_exc_common:
        move.w  #0x2700, sr
        move.l  #__stack_top, sp
        move.l  d2, -(sp)               | sr
        move.l  d1, -(sp)               | access address
        move.l  a0, -(sp)               | pc
        move.l  d0, -(sp)               | vector
        jsr     md_exception
9:      bra.s   9b

        .section .note.GNU-stack,"",@progbits
