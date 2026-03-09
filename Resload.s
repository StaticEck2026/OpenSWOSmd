;
; Resload.s — Data section + _S_GetVBR
; All code ported to src/resload.c
;

    SECTION text,CODE

; Supervisor helper: MOVEC VBR,D0 ; RTE (must stay in asm — encoded instruction)
        xdef    _S_GetVBR
_S_GetVBR:
        dc.l    $4E7A0801               ; MOVEC VBR,D0
        rte

; ---------------------------------------------------------------------------
; Data
; ---------------------------------------------------------------------------

        xdef    S_SavedSP
        xdef    S_SavedInt1
        xdef    S_SavedInt2
        xdef    S_SavedInt3
        xdef    S_SavedInt4
        xdef    S_SavedInt5
        xdef    S_VBR
        xdef    S_DosBase
        xdef    S_GfxBase
        xdef    S_FromWB
        xdef    S_WBMsg
        xdef    S_SavedView
        xdef    S_SavedDmaCon
        xdef    S_SavedIntEna
        xdef    S_SavedAdkCon
        xdef    size
        xdef    g_quitToDos
        xdef    _keyexit

S_DosVer     dc.w    39
S_DosHandle: dc.l    0
S_DosBase:   dc.l    0
S_GfxBase:   dc.l    0
S_GfxName:   dc.b    "graphics.library",0
S_DosName:   dc.b    "dos.library",0
S_FromWB:    dc.w    0
S_WBMsg:     dc.l    0
S_VBR:       dc.l    0
S_SavedView:    dc.l 0
S_SavedDmaCon:  dc.w 0
S_SavedIntEna:  dc.w 0
S_SavedAdkCon:  dc.w 0
S_DetachedDisplay: dc.w 0
S_SavedSP:    dc.l 0
S_SavedInt1:  dc.l 0
S_SavedInt2:  dc.l 0
S_SavedInt3:  dc.l 0
S_SavedInt4:  dc.l 0
S_SavedInt5:  dc.l 0
ErrorString1:
          dc.b "Error - Could not open file: ",0
NewLine:
          dc.b $a,0
size:     dc.l  0
_resload: dc.l  0
_keyexit: dc.b  $46
g_quitToDos:
        dc.w    0

        even
