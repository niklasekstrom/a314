	XDEF	_check_a314_mapping
	CODE

CLOCK_PORT	equ	$dc0000

		; Must have executed Disable() and WaitBlit() before gets here.
		; Must be in CMEM bank (RegD or'ed with 8).

		; a0 = address
_check_a314_mapping:
		move.l	d2,-(a7)

		clr.l	d0
		lea.l	CLOCK_PORT,a1

		or.b	#$2,$2f(a1)	; Set autodetect bit in RegB
		or.w	d0,(a0)		; Read and write back word from/to (a0)
		and.b	#$d,$2f(a1)	; Clear autodetect bit in RegB

		lea.l	$2b(a1),a1
		move.b	#1,(a1)		; Copy autodetect address to RegA

		moveq	#5-1,d2
.loop:		ror.l	#4,d0
		moveq	#$f,d1
		and.b	(a1),d1
		or.b	d1,d0
		dbra	d2,.loop

		moveq	#20-4,d1
		rol.l	d1,d0

		move.l	(a7)+,d2
		rts
