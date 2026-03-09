; resload_unit.s — includes Resload.s data, provides bridges for asm callers

        ; External C functions (src/resload.c)
        xref    _SafeStart
        xref    _DetachOSDisplay
        xref    _LoadDosLibrary
        xref    _CloseDosLibrary
        xref    _AllocMemAny
        xref    _ExitToDos
        xref    _FixSnoopBug
        xref    _FixMemoryRoutine
        xref    _KBHook
        xref    _RobNorthenLoader_C
        xref    _rnl_d1_return

        ; External asm functions (SWOS2.s)
        xref    Int2KeyboardHandler

        ; Include data section + _S_GetVBR
        include "../Resload.s"

        SECTION text,CODE

; ---------------------------------------------------------------------------
; Bridge stubs — forward asm-visible names to C implementations
; ---------------------------------------------------------------------------

        xdef    SafeStart
SafeStart:
        jmp     _SafeStart

        xdef    DetachOSDisplay
DetachOSDisplay:
        jmp     _DetachOSDisplay

        xdef    LoadDosLibrary
LoadDosLibrary:
        jmp     _LoadDosLibrary

        xdef    CloseDosLibrary
CloseDosLibrary:
        jmp     _CloseDosLibrary

        xdef    ExitToDos
ExitToDos:
        jmp     _ExitToDos

        xdef    FixSnoopBug
FixSnoopBug:
        jmp     _FixSnoopBug

        xdef    FixMemoryRoutine
FixMemoryRoutine:
        jmp     _FixMemoryRoutine

        xdef    KBHook
KBHook:
        jmp     _KBHook

        xdef    Access
Access:
        adda.l  $18(a5),a1
        move.l  a1,d1
        and.l   #$00ffffff,d1
        move.l  d1,a1
        moveq   #0,d1
        rts

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

; ---------------------------------------------------------------------------
; resload_FlushCache — no-op stub (for any asm callers)
; ---------------------------------------------------------------------------
        xdef    resload_FlushCache
resload_FlushCache:
        rts

; ---------------------------------------------------------------------------
; Stubs for resload_* entry points (in case any asm code references them)
; These are dead but must exist for linking.
; ---------------------------------------------------------------------------
        xdef    resload_GetFileSize
resload_GetFileSize:
        rts

        xdef    resload_LoadFile
resload_LoadFile:
        rts

        xdef    resload_LoadFileDEC
resload_LoadFileDEC:
        rts

        xdef    resload_Deletefile
resload_Deletefile:
        rts

        xdef    resload_SaveFile
resload_SaveFile:
        rts

        xdef    resload_ListFiles
resload_ListFiles:
        rts

        xdef    resload_MakeDataPath
resload_MakeDataPath:
        rts

; Wrapper stubs (dead code paths may reference these)
        xdef    _GetFileSize
_GetFileSize:
        rts

        xdef    _LoadFile
_LoadFile:
        rts

        xdef    _SaveFile
_SaveFile:
        rts

        xdef    _Deletefile
_Deletefile:
        rts

        xdef    _ListFiles
_ListFiles:
        rts
