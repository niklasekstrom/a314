	XDEF	_IntServer
	CODE

		; these offsets must be in sync with A314Device struct
CA_OFFSET	equ	40
TASK_OFFSET	equ	44

SIGB_INT	equ	14
SIGF_INT	equ	(1 << SIGB_INT)

		; a1 points to device struct
_IntServer:	move.l	CA_OFFSET(a1),a5	; com area pointer

		move	2(a5),d0
		and	#3,d0
		beq.s	spurious

		move	#$0300,2(a5)		; clear enable bits

		move.l	$4.w,a6
		move.l	#SIGF_INT,d0
		lea.l	TASK_OFFSET(a1),a1	; pointer to driver task
		jsr	-324(a6)		; Signal()

spurious:	moveq	#0,d0
		rts
