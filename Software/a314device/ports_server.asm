
	XDEF	_PortsServer
	CODE

; struct PortsData
; {
; 	struct Task *task;
; 	ULONG signal;
; };

	; a1 points to PortsData structure
_PortsServer:
	lea.l	$dc0000,a0
	move.b	#8,$37(a0)
	move.b	$3b(a0),d0
	move.b	#0,$37(a0)

	and	#$f,d0
	beq	done

	move.l	$4.w,a6
	move.l	4(a1),d0	; signal
	move.l	(a1),a1		; task
	jsr	-324(a6)

done:
	moveq	#0,d0
	rts
