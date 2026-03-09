; resload_unit.s — includes Resload.s data, provides RobNorthenLoader bridge

        ; External C functions (src/resload.c)
        xref    _RobNorthenLoader_C
        xref    _rnl_d1_return

        ; Include data section + _S_GetVBR
        include "../Resload.s"

        SECTION text,CODE

; ---------------------------------------------------------------------------
; RobNorthenLoader — register-convention bridge to C dispatcher
; Called from FloppyAsset_DispatchOp: d0=cmd, d1=arg1, a0=name, a1=buf
; Returns: d0=result, d1=return value (from _rnl_d1_return)
; ---------------------------------------------------------------------------

        xdef    RobNorthenLoader
RobNorthenLoader:
        move.l  a3,-(a7)                ; save a3 (as in original)
        movem.l d2-d7/a2-a6,-(sp)      ; save C ABI callee-saved regs
        move.l  a1,-(sp)               ; push buf
        move.l  a0,-(sp)               ; push name
        move.l  d1,-(sp)               ; push arg1
        move.l  d0,-(sp)               ; push cmd
        jsr     _RobNorthenLoader_C
        lea     16(sp),sp              ; clean args
        move.l  _rnl_d1_return,d1      ; d1 return value
        movem.l (sp)+,d2-d7/a2-a6
        move.l  (a7)+,a3
        rts
