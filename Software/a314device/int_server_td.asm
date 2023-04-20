	XDEF	_IntServer
	CODE

R_EVENTS_ADDRESS	equ	12
R_ENABLE_ADDRESS	equ	13
A_EVENTS_ADDRESS	equ	14
A_ENABLE_ADDRESS	equ	15

A_EVENT_R2A_TAIL        equ	1
A_EVENT_A2R_HEAD        equ	2

INTENA		equ	$dff09a
CLOCK_PORT	equ	$dc0000

SIGB_INT	equ	14
SIGF_INT	equ	(1<<SIGB_INT)

		; a1 points to driver task
_IntServer:	lea.l	CLOCK_PORT,a5

		move	#$4000,INTENA

		move.b	$37(a5),d1
		moveq	#8,d0
		or.b	d1,d0
		move.b	d0,$37(a5)	; Switch to CMEM
		move.b	$3b(a5),d0	; A_EVENTS
		and.b	$3f(a5),d0	; A_ENABLE
		and.b	#$f,d0
		bne.s	should_signal

		move.b	d1,$37(a5)	; Switch to RTC

		move	#$c000,INTENA
		moveq	#0,d0
		rts

should_signal:
		move.b	#0,$3f(a5)	; A_ENABLE
		move.b	d1,$37(a5)	; Switch to RTC

		move	#$c000,INTENA

		move.l	$4.w,a6
		move.l	#SIGF_INT,d0
		; a1 = pointer to driver task
		jsr	-324(a6)	; Signal()

		moveq	#0,d0
		rts
