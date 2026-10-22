;
; whdload_api.i -- the parts of WHDLoad's Slave interface OpenSWOS's slave uses.
;
; Written for OpenSWOS from WHDLoad's published interface (its autodoc): the layout
; of the Slave header, the offsets of the resload functions it calls, and the
; values of the flags and tags it passes. Only interface numbers, which any Slave
; must use; nothing here is taken from WHDLoad's own include files.
;

; ---- the Slave header (ws_Version 17) -----------------------------------------
;	ws_Security	moveq #-1,d0 / rts	(started from the Shell: does nothing)
;	ws_ID		"WHDLOADS"
;	ws_Version	.w	ws_Flags	.w
;	ws_BaseMemSize	.l	ws_ExecInstall	.l (0)
;	ws_GameLoader	.w	ws_CurrentDir	.w	ws_DontCache	.w (offsets)
;	ws_keydebug	.b	ws_keyexit	.b
;	ws_ExpMem	.l	(WHDLoad writes the address of the memory here)
;	ws_name		.w	ws_copy		.w	ws_info		.w (offsets)
;	ws_kickname	.w	ws_kicksize	.l	ws_kickcrc	.w (no Kickstart: 0)
;	ws_config	.w

; ws_Flags
WHDLF_NoError	= 1<<1		; abort on any resload error (not used: the game
				; handles a missing file itself)
WHDLF_ClearMem	= 1<<12		; BaseMem and ExpMem start zeroed
WHDLF_Req68020	= 1<<4
WHDLF_ReqAGA	= 1<<5

; resload functions: offsets from the base WHDLoad passes in A0. D0-D1/A0-A1 are
; scratch, everything else is kept; A5/A6 are never touched while interrupts run.
resload_Abort		= $04	; (a7) reason, (4,a7) primary, (8,a7) secondary; JMP to it
resload_LoadFile	= $08	; a0 name, a1 address -> d0 size, d1 dos error
resload_SaveFile	= $0c	; d0 size, a0 name, a1 address -> d0 success, d1 error
resload_ListFiles	= $14	; d0 buffer size, a0 directory, a1 buffer -> d0 count
resload_FlushCache	= $20
resload_GetFileSize	= $24	; a0 name -> d0 size (0: no such file)
resload_Relocate	= $50	; a0 executable (in place), a1 tags -> d0 size left there
resload_DeleteFile	= $58	; a0 name -> d0 success
resload_SetCPU		= $60	; d0 properties, d1 mask -> d0 old properties

; resload_Abort reasons
TDREASON_OK		= -1
TDREASON_FAILMSG	= 43	; primary = the message to show

; resload_Relocate tags (TAG_USER+$8100000 onwards)
WHDLTAG_CHIPPTR		= $88100000	; where the MEMF_CHIP hunks go
WHDLTAG_ALIGN		= $88100002	; round every hunk to this (8: as AllocMem)

; resload_SetCPU properties
WCPUF_Base_NC		= 1		; BaseMem (chip) not cached
WCPUF_Exp_CB		= 12		; ExpMem cached, copyback
WCPUF_Slave_CB		= 48		; the Slave cached, copyback
WCPUF_IC		= 1<<8		; instruction cache
WCPUF_DC		= 1<<9		; data cache
WCPUF_SB		= 1<<11		; store buffer (68060)
WCPUF_BC		= 1<<12		; branch cache (68060)
WCPUF_SS		= 1<<13		; superscalar dispatch (68060)
WCPUF_All		= $7f3f		; every property, the mask that sets them all
