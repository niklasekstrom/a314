	XDEF	_IntServer
	CODE

CPA_OFFSET	equ	40
TASK_OFFSET	equ	44

SIGB_INT	equ	14
SIGF_INT	equ	(1<<SIGB_INT)

		; a1 points to device struct
_IntServer:	move.l	CPA_OFFSET(a1),a5

		move.b	4096(a5),d0		; read cp irq
		beq.b	spurious

		move.b	#1,4096(a5)		; clear cp irq

		move.l	$4.w,a6
		move.l	#SIGF_INT,d0
		lea.l	TASK_OFFSET(a1),a1	; pointer to driver task
		jsr	-324(a6)		; Signal()

spurious:	moveq	#0,d0
		rts
