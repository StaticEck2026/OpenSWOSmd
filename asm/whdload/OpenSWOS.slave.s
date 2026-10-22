;
; OpenSWOS.slave.s -- OpenSWOS's own WHDLoad Slave.
;
; Starts the AGA program, SWOS2, without a Kickstart: no ROM image, no boot, no
; startup-sequence. WHDLoad gives it BaseMem (chip, from address 0) and ExpMem; the
; Slave
;   1. turns the CPU caches on for ExpMem (WHDLoad's default leaves both off on a
;      68020 without an MMU, i.e. a stock A1200) -- the program runs from there;
;   2. loads SWOS2 into ExpMem and relocates it over itself with resload_Relocate,
;      which does what dos.LoadSeg does when it is started from Workbench, with its
;      MEMF_CHIP hunk (the screens, sprites and samples) put in BaseMem at CHIP_AT;
;   3. finds the program's WHDLoad hook (whd_hook in src/resload.c: "OSWSWHDL") in
;      what it relocated and fills it in -- WHDLoad's resload base, and the ExpMem
;      left over, which the program allocates from;
;   4. jumps to it, in supervisor mode with interrupts on, as the original SWOS slave
;      did. The program does its file access through resload (its data/, grafs/,
;      sound/ and save/ are in the Slave's ws_CurrentDir, "data"), and quits with
;      resload_Abort.
;
; Built by Makefile.amiga beside SWOS2: the memory sizes come from SWOS2's own hunk
; header (scripts/amiga/whdload_sizes.py -> obj/whdload_sizes.i) and are checked again
; here at run time.
;
	INCLUDE	"whdload_api.i"
	INCLUDE	"whdload_sizes.i"

CHIP_AT		= $1000			; above the exception vectors
STACK_ROOM	= $4000			; left free at the top of BaseMem for the SSP
HEAP_SIZE	= $100000		; ExpMem past the program, for its allocations
BASEMEM		= (CHIP_AT+SWOS2_CHIP+STACK_ROOM+$fff)&~$fff
EXPMEM		= (SWOS2_FAST+HEAP_SIZE+$fff)&~$fff

	IFGT	SWOS2_FILE-EXPMEM
	FAIL	"SWOS2 does not fit where the Slave loads it"
	ENDC
	IFGT	BASEMEM-$200000
	FAIL	"SWOS2's chip hunks are larger than an A1200's chip RAM"
	ENDC

;==============================================================================

base:
	moveq	#-1,d0			; ws_Security: run from the Shell, do nothing
	rts
	dc.b	"WHDLOADS"		; ws_ID
	dc.w	17			; ws_Version
	dc.w	WHDLF_ClearMem|WHDLF_Req68020|WHDLF_ReqAGA	; ws_Flags
	dc.l	BASEMEM			; ws_BaseMemSize
	dc.l	0			; ws_ExecInstall
	dc.w	start-base		; ws_GameLoader
	dc.w	dir-base		; ws_CurrentDir
	dc.w	0			; ws_DontCache
	dc.b	0			; ws_keydebug
	dc.b	$46			; ws_keyexit: Del, as the original SWOS slave
expmem:	dc.l	EXPMEM			; ws_ExpMem (WHDLoad writes its address here)
	dc.w	name-base		; ws_name
	dc.w	copy-base		; ws_copy
	dc.w	info-base		; ws_info
	dc.w	0			; ws_kickname: no Kickstart
	dc.l	0			; ws_kicksize
	dc.w	0			; ws_kickcrc
	dc.w	0			; ws_config

dir:	dc.b	"data",0
name:	dc.b	"OpenSWOS (AGA)",0
copy:	dc.b	"2026 OpenSWOS, from Sensible World of Soccer (1994-97 Sensible Software)",0
info:	dc.b	"github.com/benbaker76/OpenSWOS",0
prog:	dc.b	"SWOS2",0
nofile:	dc.b	"SWOS2 is not in the data folder",0
nochip:	dc.b	"SWOS2's chip hunks do not fit in the chip memory asked for",0
nofast:	dc.b	"SWOS2 does not fit in the expansion memory asked for",0
nohook:	dc.b	"SWOS2 has no WHDLoad hook: it is older than this Slave",0
	EVEN

resload:	dc.l	0
chiptags:	dc.l	WHDLTAG_ALIGN,8
chiplen:	dc.l	WHDLTAG_CHIPPTR,CHIP_AT
		dc.l	0		; TAG_DONE

;==============================================================================

start:					; A0 = resload base
	lea	resload(pc),a1
	move.l	a0,(a1)
	move.l	a0,a2

	; the caches, for ExpMem (the program) and the Slave; chip is never cached
	move.l	#WCPUF_Base_NC|WCPUF_Exp_CB|WCPUF_Slave_CB|WCPUF_IC|WCPUF_DC|WCPUF_SB|WCPUF_BC|WCPUF_SS,d0
	move.l	#WCPUF_All,d1
	jsr	(resload_SetCPU,a2)

	; SWOS2, loaded to the start of ExpMem
	lea	prog(pc),a0
	move.l	expmem(pc),a1
	jsr	(resload_LoadFile,a2)
	lea	nofile(pc),a0
	tst.l	d0
	beq	fail

	; relocated over itself; the chip hunk to CHIP_AT. D0 = what stayed in ExpMem,
	; the tag's data = the length put at CHIP_AT.
	move.l	expmem(pc),a0
	lea	chiptags(pc),a1
	jsr	(resload_Relocate,a2)
	move.l	d0,d7			; D7 = the program's size in ExpMem
	lea	nochip(pc),a0
	move.l	chiplen+4(pc),d0
	cmp.l	#BASEMEM-CHIP_AT-STACK_ROOM,d0
	bhi	fail
	lea	nofast(pc),a0
	cmp.l	#EXPMEM-$1000,d7
	bhs	fail

	; the hook: "OSWS","WHDL", then resload, heap start, heap end (longwords)
	move.l	expmem(pc),a3
	move.l	a3,a4
	add.l	d7,a4			; A4 = end of the program
.scan:	cmp.l	#"OSWS",(a3)
	bne.b	.next
	cmp.l	#"WHDL",(4,a3)
	beq.b	.found
.next:	addq.l	#2,a3
	cmp.l	a4,a3
	blo.b	.scan
	lea	nohook(pc),a0
	bra	fail
.found:	move.l	a2,(8,a3)		; resload
	move.l	a4,d0
	addq.l	#7,d0
	and.w	#~7,d0
	move.l	d0,(12,a3)		; heap start
	move.l	expmem(pc),d0
	add.l	#EXPMEM,d0
	move.l	d0,(16,a3)		; heap end

	jsr	(resload_FlushCache,a2)
	move.w	#$2000,sr		; supervisor, interrupts on
	move.l	expmem(pc),a0
	jmp	(a0)			; SWOS2's first hunk: its entry

; A0 = the message
fail:	clr.l	-(a7)			; secondary
	move.l	a0,-(a7)		; primary: the message
	pea	TDREASON_FAILMSG
	move.l	resload(pc),a0
	jmp	(resload_Abort,a0)
