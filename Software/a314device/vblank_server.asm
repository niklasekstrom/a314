
	XDEF	_VBlankServer
	CODE

; #define R2A_MASK	0x01
; #define A2R_MASK	0x02
;
; struct VBlankData
; {
; 	struct ComArea *ca;
; 	struct Task *task;
; 	ULONG signal;
; 	UBYTE reqs;
; 	UBYTE acks;
; 	UBYTE max_len;
; 	UBYTE padding;
; };

; struct ComArea
; {
; 	UBYTE a2r_tail;
; 	UBYTE r2a_head;
; 	UBYTE r2a_tail;
; 	UBYTE a2r_head;
; 	UBYTE a2r_buffer[256];
; 	UBYTE r2a_buffer[256];
; };

	; a1 points to VBlankData structure
_VBlankServer:
	move.b	12(a1),d0
	move.b	13(a1),d1
	eor.b	d1,d0		; d0 = reqs ^ acks = bit set if was requested
	beq	done

	move.l	(a1),a0		; a0 = com area
	move.l	d2,a5
	clr	d1

	btst	#0,d0
	beq	not_r2a

	move.b	2(a0),d2
	sub.b	1(a0),d2	; d2 = (tail - head) & 255 = length of R2A
	beq	not_r2a

	or.b	#1,d1

not_r2a:
	btst	#1,d0
	beq	not_a2r

	clr	d2
	move.b	0(a0),d2
	sub.b	3(a0),d2	; d2 = (tail - head) & 255 = length of A2R
	clr	d0
	move.b	14(a1),d0	; d0 = max_len
	cmp	d0,d2
	bgt	not_a2r

	or.b	#2,d1

not_a2r:
	tst.b	d1
	beq	no_int

	eor.b	d1,13(a1)	; update acks

	move.l	$4.w,a6
	move.l	8(a1),d0	; signal
	move.l	4(a1),a1	; task
	jsr	-324(a6)

no_int:
	move.l	a5,d2
done:
	moveq	#0,d0
	rts
