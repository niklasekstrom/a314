
	XDEF	_VBlankServer
	CODE

;struct VBlankData
;{
;	struct Task *task;
;	ULONG signal;
;};

	; a1 points to VBlankData structure
_VBlankServer:
	move.l	$4.w,a6
	move.l	4(a1),d0	; signal
	move.l	0(a1),a1	; task
	jsr	-324(a6)
	moveq	#0,d0
	rts
