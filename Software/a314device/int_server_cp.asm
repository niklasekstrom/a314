	XDEF	_IntServer
	CODE

CLOCK_PORT	equ	$d80001

SIGB_INT	equ	14
SIGF_INT	equ	(1<<SIGB_INT)

		; a1 points to driver task
_IntServer:	lea.l	CLOCK_PORT+4,a5

		move.b	(a5),d0		; read cp irq
		bne.b	should_signal

		moveq	#0,d0
		rts

should_signal:
		move.b	#1,(a5)		; clear cp irq

		move.l	$4.w,a6
		move.l	#SIGF_INT,d0
		; a1 = pointer to driver task
		jsr	-324(a6)	; Signal()

		moveq	#1,d0
		rts
