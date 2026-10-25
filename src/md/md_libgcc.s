| 68000-safe replacements for the libgcc routines the build needs.
|
| The m68k cross gcc a distro ships (m68k-linux-gnu) carries a libgcc built for 68020+:
| its __divsi3 uses bsr.l, which a 68000 does not have. These are the same entry points in
| plain 68000 code. Calling convention: arguments on the stack, result in d0, d0-d1/a0-a1
| scratch.

        .text

| __mulsi3(a, b) = a * b (low 32 bits)
        .globl  __mulsi3
__mulsi3:
        move.w  4(sp), d0       | a.hi
        mulu.w  10(sp), d0      | a.hi * b.lo
        move.w  8(sp), d1       | b.hi
        mulu.w  6(sp), d1       | b.hi * a.lo
        add.w   d1, d0
        swap    d0
        clr.w   d0
        move.w  6(sp), d1
        mulu.w  10(sp), d1      | a.lo * b.lo
        add.l   d1, d0
        rts

| udivmod: d0 = n, d1 = d  ->  d0 = n / d, d2 = n % d. Clobbers d1 only through d2/d0.
udivmod:
        cmp.l   #0x10000, d1
        bcc.s   .Lbig
        | divisor fits 16 bits: two divu steps (a zero divisor traps, vector 5)
        move.l  d0, d2
        clr.w   d2
        swap    d2              | d2 = n.hi
        divu.w  d1, d2          | d2 = r.hi : q.hi
        swap    d0              | d0 = n.lo : n.hi
        move.w  d2, d0          | d0 = n.lo : q.hi
        swap    d0              | d0 = q.hi : n.lo
        move.w  d0, d2          | d2 = r.hi : n.lo
        divu.w  d1, d2          | d2 = r : q.lo
        move.w  d2, d0          | d0 = q.hi : q.lo
        clr.w   d2
        swap    d2              | d2 = r
        rts
.Lbig:
        | divisor >= 65536: the quotient is under 65536; shift and subtract
        move.l  d3, -(sp)
        moveq   #0, d2
        moveq   #31, d3
1:      add.l   d0, d0
        addx.l  d2, d2
        bcs.s   3f
        cmp.l   d1, d2
        bcs.s   2f
3:      sub.l   d1, d2
        addq.l  #1, d0
2:      dbra    d3, 1b
        move.l  (sp)+, d3
        rts

        .globl  __udivsi3
__udivsi3:
        move.l  d2, -(sp)
        move.l  8(sp), d0
        move.l  12(sp), d1
        bsr.s   udivmod
        move.l  (sp)+, d2
        rts

        .globl  __umodsi3
__umodsi3:
        move.l  d2, -(sp)
        move.l  8(sp), d0
        move.l  12(sp), d1
        bsr.s   udivmod
        move.l  d2, d0
        move.l  (sp)+, d2
        rts

        .globl  __divsi3
__divsi3:
        move.l  d2, -(sp)
        move.l  d3, -(sp)
        moveq   #0, d3
        move.l  12(sp), d0
        bpl.s   1f
        neg.l   d0
        not.b   d3
1:      move.l  16(sp), d1
        bpl.s   2f
        neg.l   d1
        not.b   d3
2:      bsr     udivmod
        tst.b   d3
        beq.s   3f
        neg.l   d0
3:      move.l  (sp)+, d3
        move.l  (sp)+, d2
        rts

        .globl  __modsi3
__modsi3:
        move.l  d2, -(sp)
        move.l  d3, -(sp)
        moveq   #0, d3
        move.l  12(sp), d0
        bpl.s   1f
        neg.l   d0
        not.b   d3              | the remainder takes the dividend's sign
1:      move.l  16(sp), d1
        bpl.s   2f
        neg.l   d1
2:      bsr     udivmod
        move.l  d2, d0
        tst.b   d3
        beq.s   3f
        neg.l   d0
3:      move.l  (sp)+, d3
        move.l  (sp)+, d2
        rts

        .section .note.GNU-stack,"",@progbits
