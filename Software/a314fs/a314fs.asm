	idnt	"a314fs.c"
	opt	0
	opt	NQLPSMRBT
	;section	"CODE",code
	public	_MyNewList
	cnop	0,4
_MyNewList
	movem.l	l3,-(a7)
	move.l	(4+l5,a7),a1
	move.l	a1,a0
	addq.l	#4,a0
	move.l	a0,(a1)
	move.l	#0,(4,a1)
	move.l	a1,(8,a1)
l1
l3	reg
l5	equ	0
	rts
; stacksize=0
	opt	0
	opt	NQLPSMRBT
	public	_MyCreatePort
	cnop	0,4
_MyCreatePort
	movem.l	l15,-(a7)
	move.l	(8+l17,a7),d3
	move.l	(4+l17,a7),a3
	moveq	#-1,d0
	move.l	_SysBase,a6
	jsr	-330(a6)
	move.l	d0,d1
	move.l	d1,d2
	moveq	#-1,d0
	cmp.l	d2,d0
	bne	l9
l8
	moveq	#0,d0
	bra	l6
l9
	move.l	#65537,d1
	moveq	#34,d0
	move.l	_SysBase,a6
	jsr	-198(a6)
	move.l	d0,a0
	move.l	a0,a2
	move.l	a2,d0
	bne	l11
l10
	move.l	d2,d0
	move.l	_SysBase,a6
	jsr	-336(a6)
	moveq	#0,d0
	bra	l6
l11
	move.l	a3,(10,a2)
	move.b	d3,(9,a2)
	move.b	#4,(8,a2)
	move.b	#0,(14,a2)
	move.b	d2,(15,a2)
	move.l	#0,a1
	move.l	_SysBase,a6
	jsr	-294(a6)
	move.l	d0,(16,a2)
	move.l	a3,d0
	beq	l13
	move.l	a2,a1
	move.l	_SysBase,a6
	jsr	-354(a6)
	bra	l14
l13
	pea	(20,a2)
	jsr	_MyNewList
	addq.w	#4,a7
l14
	move.l	a2,d0
l6
l15	reg	a2/a3/a6/d2/d3
	movem.l	(a7)+,a2/a3/a6/d2/d3
l17	equ	20
	rts
	opt	0
	opt	NQLPSMRBT
	public	_MyCreateExtIO
	cnop	0,4
_MyCreateExtIO
	movem.l	l24,-(a7)
	move.l	(8+l26,a7),d2
	move.l	(4+l26,a7),a3
	move.l	a3,d0
	bne	l21
l20
	moveq	#0,d0
	bra	l18
l21
	move.l	#65537,d1
	move.l	d2,d0
	move.l	_SysBase,a6
	jsr	-198(a6)
	move.l	d0,a0
	move.l	a0,a2
	move.l	a2,d0
	bne	l23
l22
	moveq	#0,d0
	bra	l18
l23
	move.b	#5,(8,a2)
	move.w	d2,(18,a2)
	move.l	a3,(14,a2)
	move.l	a2,d0
l18
l24	reg	a2/a3/a6/d2
	movem.l	(a7)+,a2/a3/a6/d2
l26	equ	16
	rts
	opt	0
	opt	NQLPSMRBT
	public	_dbg_init
	cnop	0,4
_dbg_init
	movem.l	l29,-(a7)
l27
l29	reg
l31	equ	0
	rts
; stacksize=0
	opt	0
	opt	NQLPSMRBT
	public	_dbg
	cnop	0,4
_dbg
	movem.l	l34,-(a7)
l32
l34	reg
l36	equ	0
	rts
; stacksize=0
	opt	0
	opt	NQLPSMRBT
	public	_DosAllocMem
	cnop	0,4
_DosAllocMem
	movem.l	l39,-(a7)
	move.l	(4+l41,a7),d2
	move.l	#65537,d1
	moveq	#4,d0
	add.l	d2,d0
	move.l	_SysBase,a6
	jsr	-198(a6)
	move.l	d0,a0
	move.l	a0,a2
	addq.l	#4,a2
	move.l	d2,(a0)
	move.l	a2,d0
l37
l39	reg	a2/a6/d2
	movem.l	(a7)+,a2/a6/d2
l41	equ	12
	rts
	opt	0
	opt	NQLPSMRBT
	public	_DosFreeMem
	cnop	0,4
_DosFreeMem
	movem.l	l44,-(a7)
	move.l	(4+l46,a7),a3
	move.l	a3,a2
	move.l	-(a2),d2
	move.l	d2,d0
	move.l	a2,a1
	move.l	_SysBase,a6
	jsr	-210(a6)
l42
l44	reg	a2/a3/a6/d2
	movem.l	(a7)+,a2/a3/a6/d2
l46	equ	16
	rts
	opt	0
	opt	NQLPSMRBT
	public	_reply_packet
	cnop	0,4
_reply_packet
	movem.l	l49,-(a7)
	move.l	(4+l51,a7),a2
	move.l	(4,a2),a3
	move.l	_mp,(4,a2)
	move.l	(a2),a1
	move.l	a3,a0
	move.l	_SysBase,a6
	jsr	-366(a6)
l47
l49	reg	a2/a3/a6
	movem.l	(a7)+,a2/a3/a6
l51	equ	12
	rts
	opt	0
	opt	NQLPSMRBT
	public	_a314_cmd_wait
	cnop	0,4
_a314_cmd_wait
	movem.l	l54,-(a7)
	move.l	(12+l56,a7),d3
	move.w	(6+l56,a7),d2
	move.l	(8+l56,a7),a2
	move.l	_a314_ior,a0
	move.w	d2,(28,a0)
	move.l	_a314_ior,a0
	move.b	#0,(31,a0)
	move.l	_a314_ior,a0
	move.l	_socket,(32,a0)
	move.l	_a314_ior,a0
	move.l	a2,(36,a0)
	move.l	_a314_ior,a0
	move.w	d3,(40,a0)
	move.l	_a314_ior,a1
	move.l	_SysBase,a6
	jsr	-456(a6)
l52
l54	reg	a2/a6/d2/d3
	movem.l	(a7)+,a2/a6/d2/d3
l56	equ	16
	rts
	opt	0
	opt	NQLPSMRBT
	public	_a314_connect
	cnop	0,4
_a314_connect
	sub.w	#12,a7
	movem.l	l59,-(a7)
	move.l	(16+l61,a7),a2
	lea	(0+l61,a7),a0
	move.l	a0,d1
	move.l	_DOSBase,a6
	jsr	-192(a6)
	move.l	(8+l61,a7),_socket
	move.l	a2,a0
	inline
	move.l	a0,d0
.l1
	tst.b	(a0)+
	bne	.l1
	sub.l	a0,d0
	not.l	d0
	einline
	move.l	d0,-(a7)
	move.l	a2,-(a7)
	move.l	#9,-(a7)
	jsr	_a314_cmd_wait
	add.w	#12,a7
l57
l59	reg	a2/a6
	movem.l	(a7)+,a2/a6
l61	equ	8
	add.w	#12,a7
	rts
	opt	0
	opt	NQLPSMRBT
	public	_a314_read
	cnop	0,4
_a314_read
	movem.l	l64,-(a7)
	move.l	(8+l66,a7),d2
	move.l	(4+l66,a7),a2
	move.l	d2,-(a7)
	move.l	a2,-(a7)
	move.l	#10,-(a7)
	jsr	_a314_cmd_wait
	add.w	#12,a7
l62
l64	reg	a2/d2
	movem.l	(a7)+,a2/d2
l66	equ	8
	rts
	opt	0
	opt	NQLPSMRBT
	public	_a314_write
	cnop	0,4
_a314_write
	movem.l	l69,-(a7)
	move.l	(8+l71,a7),d2
	move.l	(4+l71,a7),a2
	move.l	d2,-(a7)
	move.l	a2,-(a7)
	move.l	#11,-(a7)
	jsr	_a314_cmd_wait
	add.w	#12,a7
l67
l69	reg	a2/d2
	movem.l	(a7)+,a2/d2
l71	equ	8
	rts
	opt	0
	opt	NQLPSMRBT
	public	_a314_eos
	cnop	0,4
_a314_eos
	movem.l	l74,-(a7)
	move.l	#0,-(a7)
	move.l	#0,-(a7)
	move.l	#12,-(a7)
	jsr	_a314_cmd_wait
	add.w	#12,a7
l72
l74	reg
l76	equ	0
	rts
	opt	0
	opt	NQLPSMRBT
	public	_a314_reset
	cnop	0,4
_a314_reset
	movem.l	l79,-(a7)
	move.l	#0,-(a7)
	move.l	#0,-(a7)
	move.l	#13,-(a7)
	jsr	_a314_cmd_wait
	add.w	#12,a7
l77
l79	reg
l81	equ	0
	rts
	opt	0
	opt	NQLPSMRBT
	public	_create_and_add_volume
	cnop	0,4
_create_and_add_volume
	movem.l	l84,-(a7)
	move.l	#44,-(a7)
	jsr	_DosAllocMem
	move.l	d0,_my_volume
	move.l	#_default_volume_name,d0
	lsr.l	#2,d0
	move.l	_my_volume,a0
	move.l	d0,(40,a0)
	move.l	_my_volume,a0
	moveq	#2,d0
	move.l	d0,(4,a0)
	move.l	_my_volume,a0
	move.l	_mp,(8,a0)
	move.l	_my_volume,a0
	move.l	#858862592,(32,a0)
	move.l	_my_volume,a0
	add.l	#16,a0
	move.l	a0,d1
	move.l	_DOSBase,a6
	jsr	-192(a6)
	move.l	_DOSBase,a0
	move.l	(34,a0),a3
	move.l	(24,a3),d0
	lsl.l	#2,d0
	move.l	d0,a2
	move.l	_SysBase,a6
	jsr	-132(a6)
	move.l	_my_volume,a1
	move.l	(4,a2),(a1)
	move.l	_my_volume,d0
	lsr.l	#2,d0
	move.l	d0,(4,a2)
	move.l	_SysBase,a6
	jsr	-138(a6)
	addq.w	#4,a7
l82
l84	reg	a2/a3/a6
	movem.l	(a7)+,a2/a3/a6
l86	equ	12
	rts
	opt	0
	opt	NQLPSMRBT
	public	_startup_fs_handler
	cnop	0,4
_startup_fs_handler
	movem.l	l106,-(a7)
	move.l	(4+l108,a7),a2
	move.l	(20,a2),d0
	lsl.l	#2,d0
	move.l	d0,a3
	move.l	(24,a2),d0
	lsl.l	#2,d0
	move.l	d0,a5
	move.l	(28,a2),d0
	lsl.l	#2,d0
	move.l	d0,a4
	moveq	#0,d0
	move.b	(a3),d0
	addq.l	#1,d0
	move.l	d0,d2
	move.l	a3,a1
	lea	_device_name,a0
	inline
	move.l	a0,d0
	cmp.l	#16,d2
	blo	.l5
	moveq	#1,d1
	and.b	d0,d1
	beq	.l1
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
.l1
	move.l	a1,d1
	and.b	#1,d1
	beq	.l3
	cmp.l	#$10000,d2
	blo	.l5
.l2
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
	bne	.l2
	bra	.l7
.l3
	moveq	#3,d1
	and.l	d2,d1
	sub.l	d1,d2
.l4
	move.l	(a1)+,(a0)+
	subq.l	#4,d2
	bne	.l4
	move.w	d1,d2
.l5
	subq.w	#1,d2
	blo	.l7
.l6
	move.b	(a1)+,(a0)+
	dbf	d2,.l6
.l7
	einline
	moveq	#0,d0
	move.b	(a3),d0
	addq.l	#1,d0
	lea	_device_name,a0
	move.b	#0,(0,a0,d0.l)
	move.l	_mp,(8,a4)
	moveq	#-1,d0
	move.l	d0,(12,a2)
	move.l	#0,(16,a2)
	move.l	a2,-(a7)
	jsr	_reply_packet
	jsr	_dbg_init
	pea	l89
	jsr	_dbg
	move.l	#_device_name,d0
	move.l	d0,-(a7)
	pea	l90
	jsr	_dbg
	move.l	a5,d0
	move.l	d0,-(a7)
	pea	l91
	jsr	_dbg
	move.l	a4,d0
	move.l	d0,-(a7)
	pea	l92
	jsr	_dbg
	move.l	#0,-(a7)
	move.l	#0,-(a7)
	jsr	_MyCreatePort
	move.l	d0,_timer_mp
	move.l	#40,-(a7)
	move.l	_timer_mp,-(a7)
	jsr	_MyCreateExtIO
	move.l	d0,_tr
	moveq	#0,d1
	move.l	_tr,a1
	moveq	#0,d0
	lea	l95,a0
	move.l	_SysBase,a6
	jsr	-444(a6)
	move.l	d0,d2
	add.w	#48,a7
	beq	l94
l93
	pea	l96
	jsr	_dbg
	addq.w	#4,a7
	bra	l87
l94
	move.l	#0,-(a7)
	move.l	#0,-(a7)
	jsr	_MyCreatePort
	move.l	d0,_a314_mp
	move.l	#42,-(a7)
	move.l	_a314_mp,-(a7)
	jsr	_MyCreateExtIO
	move.l	d0,_a314_ior
	moveq	#0,d1
	move.l	_a314_ior,a1
	moveq	#0,d0
	lea	l99,a0
	move.l	_SysBase,a6
	jsr	-444(a6)
	move.l	d0,d2
	add.w	#16,a7
	beq	l98
l97
	pea	l100
	jsr	_dbg
	addq.w	#4,a7
	bra	l87
l98
	move.l	_a314_ior,a0
	move.l	(20,a0),_A314Base
	pea	l103
	jsr	_a314_connect
	addq.w	#4,a7
	tst.l	d0
	beq	l102
l101
	pea	l104
	jsr	_dbg
	addq.w	#4,a7
	bra	l87
l102
	move.l	#128,d1
	move.l	#256,d0
	move.l	_SysBase,a6
	jsr	-198(a6)
	move.l	d0,a0
	move.l	a0,_request_buffer
	move.l	#128,d1
	move.l	#4096,d0
	move.l	_SysBase,a6
	jsr	-198(a6)
	move.l	d0,a0
	move.l	a0,_data_buffer
	jsr	_create_and_add_volume
	pea	l105
	jsr	_dbg
	addq.w	#4,a7
l87
l106	reg	a2/a3/a4/a5/a6/d2
	movem.l	(a7)+,a2/a3/a4/a5/a6/d2
l108	equ	24
	rts
	cnop	0,4
l96
	dc.b	70
	dc.b	97
	dc.b	116
	dc.b	97
	dc.b	108
	dc.b	32
	dc.b	101
	dc.b	114
	dc.b	114
	dc.b	111
	dc.b	114
	dc.b	58
	dc.b	32
	dc.b	117
	dc.b	110
	dc.b	97
	dc.b	98
	dc.b	108
	dc.b	101
	dc.b	32
	dc.b	116
	dc.b	111
	dc.b	32
	dc.b	111
	dc.b	112
	dc.b	101
	dc.b	110
	dc.b	32
	dc.b	116
	dc.b	105
	dc.b	109
	dc.b	101
	dc.b	114
	dc.b	46
	dc.b	100
	dc.b	101
	dc.b	118
	dc.b	105
	dc.b	99
	dc.b	101
	dc.b	10
	dc.b	0
	cnop	0,4
l100
	dc.b	70
	dc.b	97
	dc.b	116
	dc.b	97
	dc.b	108
	dc.b	32
	dc.b	101
	dc.b	114
	dc.b	114
	dc.b	111
	dc.b	114
	dc.b	58
	dc.b	32
	dc.b	117
	dc.b	110
	dc.b	97
	dc.b	98
	dc.b	108
	dc.b	101
	dc.b	32
	dc.b	116
	dc.b	111
	dc.b	32
	dc.b	111
	dc.b	112
	dc.b	101
	dc.b	110
	dc.b	32
	dc.b	97
	dc.b	51
	dc.b	49
	dc.b	52
	dc.b	46
	dc.b	100
	dc.b	101
	dc.b	118
	dc.b	105
	dc.b	99
	dc.b	101
	dc.b	10
	dc.b	0
	cnop	0,4
l104
	dc.b	70
	dc.b	97
	dc.b	116
	dc.b	97
	dc.b	108
	dc.b	32
	dc.b	101
	dc.b	114
	dc.b	114
	dc.b	111
	dc.b	114
	dc.b	58
	dc.b	32
	dc.b	117
	dc.b	110
	dc.b	97
	dc.b	98
	dc.b	108
	dc.b	101
	dc.b	32
	dc.b	116
	dc.b	111
	dc.b	32
	dc.b	99
	dc.b	111
	dc.b	110
	dc.b	110
	dc.b	101
	dc.b	99
	dc.b	116
	dc.b	32
	dc.b	116
	dc.b	111
	dc.b	32
	dc.b	97
	dc.b	51
	dc.b	49
	dc.b	52
	dc.b	102
	dc.b	115
	dc.b	32
	dc.b	111
	dc.b	110
	dc.b	32
	dc.b	114
	dc.b	97
	dc.b	115
	dc.b	112
	dc.b	10
	dc.b	0
	cnop	0,4
l89
	dc.b	65
	dc.b	67
	dc.b	84
	dc.b	73
	dc.b	79
	dc.b	78
	dc.b	95
	dc.b	83
	dc.b	84
	dc.b	65
	dc.b	82
	dc.b	84
	dc.b	85
	dc.b	80
	dc.b	10
	dc.b	0
	cnop	0,4
l90
	dc.b	32
	dc.b	32
	dc.b	100
	dc.b	101
	dc.b	118
	dc.b	105
	dc.b	99
	dc.b	101
	dc.b	95
	dc.b	110
	dc.b	97
	dc.b	109
	dc.b	101
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	83
	dc.b	10
	dc.b	0
	cnop	0,4
l91
	dc.b	32
	dc.b	32
	dc.b	102
	dc.b	115
	dc.b	115
	dc.b	109
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l92
	dc.b	32
	dc.b	32
	dc.b	110
	dc.b	111
	dc.b	100
	dc.b	101
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l95
	dc.b	116
	dc.b	105
	dc.b	109
	dc.b	101
	dc.b	114
	dc.b	46
	dc.b	100
	dc.b	101
	dc.b	118
	dc.b	105
	dc.b	99
	dc.b	101
	dc.b	0
	cnop	0,4
l99
	dc.b	97
	dc.b	51
	dc.b	49
	dc.b	52
	dc.b	46
	dc.b	100
	dc.b	101
	dc.b	118
	dc.b	105
	dc.b	99
	dc.b	101
	dc.b	0
	cnop	0,4
l103
	dc.b	97
	dc.b	51
	dc.b	49
	dc.b	52
	dc.b	102
	dc.b	115
	dc.b	0
	cnop	0,4
l105
	dc.b	83
	dc.b	116
	dc.b	97
	dc.b	114
	dc.b	116
	dc.b	117
	dc.b	112
	dc.b	32
	dc.b	115
	dc.b	117
	dc.b	99
	dc.b	99
	dc.b	101
	dc.b	115
	dc.b	115
	dc.b	102
	dc.b	117
	dc.b	108
	dc.b	10
	dc.b	0
	opt	0
	opt	NQLPSMRBT
	public	_wait_for_response
	cnop	0,4
_wait_for_response
	movem.l	l118,-(a7)
	moveq	#0,d2
l111
	move.l	_request_buffer,a0
	tst.b	(a0)
	beq	l116
	move.l	d2,-(a7)
	pea	l117
	jsr	_dbg
	addq.w	#8,a7
	bra	l109
l116
	move.l	_tr,a0
	move.w	#9,(28,a0)
	move.l	_tr,a0
	move.l	_timer_mp,(14,a0)
	move.l	_tr,a0
	move.l	#0,(32,a0)
	move.l	_tr,a0
	add.l	#32,a0
	move.l	#1000,(4,a0)
	move.l	_tr,a1
	move.l	_SysBase,a6
	jsr	-456(a6)
l114
	addq.l	#1,d2
	bra	l111
l113
l109
l118	reg	a6/d2
	movem.l	(a7)+,a6/d2
l120	equ	8
	rts
	cnop	0,4
l117
	dc.b	45
	dc.b	45
	dc.b	71
	dc.b	111
	dc.b	116
	dc.b	32
	dc.b	114
	dc.b	101
	dc.b	115
	dc.b	112
	dc.b	111
	dc.b	110
	dc.b	115
	dc.b	101
	dc.b	32
	dc.b	97
	dc.b	102
	dc.b	116
	dc.b	101
	dc.b	114
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	32
	dc.b	115
	dc.b	108
	dc.b	101
	dc.b	101
	dc.b	112
	dc.b	115
	dc.b	10
	dc.b	0
	opt	0
	opt	NQLPSMRBT
	public	_write_req_and_wait_for_res
	cnop	0,4
_write_req_and_wait_for_res
	subq.w	#8,a7
	movem.l	l123,-(a7)
	move.l	(12+l125,a7),d2
	move.l	_request_buffer,a0
	move.l	_A314Base,a6
	jsr	-42(a6)
	move.l	d0,(0+l125,a7)
	move.l	d2,(4+l125,a7)
	move.l	#8,-(a7)
	lea	(4+l125,a7),a0
	move.l	a0,-(a7)
	jsr	_a314_write
	move.l	#8,-(a7)
	lea	(12+l125,a7),a0
	move.l	a0,-(a7)
	jsr	_a314_read
	add.w	#16,a7
l121
l123	reg	a6/d2
	movem.l	(a7)+,a6/d2
l125	equ	8
	addq.w	#8,a7
	rts
	opt	0
	opt	NQLPSMRBT
	public	_create_and_add_file_lock
	cnop	0,4
_create_and_add_file_lock
	movem.l	l128,-(a7)
	move.l	(8+l130,a7),d3
	move.l	(4+l130,a7),d2
	move.l	#20,-(a7)
	jsr	_DosAllocMem
	move.l	d0,a2
	move.l	d2,(4,a2)
	move.l	d3,(8,a2)
	move.l	_mp,(12,a2)
	move.l	_my_volume,d0
	lsr.l	#2,d0
	move.l	d0,(16,a2)
	move.l	_SysBase,a6
	jsr	-132(a6)
	move.l	_my_volume,a0
	move.l	(12,a0),(a2)
	move.l	a2,d0
	lsr.l	#2,d0
	move.l	_my_volume,a0
	move.l	d0,(12,a0)
	move.l	_SysBase,a6
	jsr	-138(a6)
	move.l	a2,d0
	addq.w	#4,a7
l126
l128	reg	a2/a6/d2/d3
	movem.l	(a7)+,a2/a6/d2/d3
l130	equ	16
	rts
	opt	0
	opt	NQLPSMRBT
	public	_action_locate_object
	cnop	0,4
_action_locate_object
	movem.l	l150,-(a7)
	move.l	(4+l152,a7),a2
	move.l	(20,a2),d0
	lsl.l	#2,d0
	move.l	d0,a5
	move.l	(24,a2),d0
	lsl.l	#2,d0
	move.l	d0,a6
	move.l	(28,a2),d3
	pea	l133
	jsr	_dbg
	move.l	a5,-(a7)
	pea	l134
	jsr	_dbg
	move.l	a6,-(a7)
	pea	l135
	jsr	_dbg
	add.w	#20,a7
	moveq	#-2,d0
	cmp.l	d3,d0
	bne	l137
l136
	lea	l139,a0
	bra	l138
l137
	lea	l140,a0
l138
	move.l	a0,-(a7)
	pea	l141
	jsr	_dbg
	move.l	_request_buffer,a3
	move.w	#0,(a3)
	move.w	(10,a2),d0
	move.w	d0,(2,a3)
	addq.w	#8,a7
	move.l	a5,d0
	bne	l143
l142
	moveq	#0,d0
	bra	l144
l143
	move.l	(4,a5),d0
l144
	move.l	d0,(4,a3)
	move.w	d3,(8,a3)
	moveq	#0,d4
	move.b	(a6),d4
	moveq	#1,d0
	add.l	d4,d0
	move.l	d0,d2
	move.l	a6,a1
	lea	(10,a3),a0
	inline
	move.l	a0,d0
	cmp.l	#16,d2
	blo	.l5
	moveq	#1,d1
	and.b	d0,d1
	beq	.l1
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
.l1
	move.l	a1,d1
	and.b	#1,d1
	beq	.l3
	cmp.l	#$10000,d2
	blo	.l5
.l2
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
	bne	.l2
	bra	.l7
.l3
	moveq	#3,d1
	and.l	d2,d1
	sub.l	d1,d2
.l4
	move.l	(a1)+,(a0)+
	subq.l	#4,d2
	bne	.l4
	move.w	d1,d2
.l5
	subq.w	#1,d2
	blo	.l7
.l6
	move.b	(a1)+,(a0)+
	dbf	d2,.l6
.l7
	einline
	moveq	#11,d0
	add.l	d4,d0
	move.l	d0,-(a7)
	jsr	_write_req_and_wait_for_res
	move.l	_request_buffer,a4
	addq.w	#4,a7
	tst.w	(2,a4)
	bne	l146
l145
	move.w	(4,a4),d0
	ext.l	d0
	move.l	d0,-(a7)
	pea	l147
	jsr	_dbg
	move.l	#0,(12,a2)
	move.w	(4,a4),d0
	ext.l	d0
	move.l	d0,(16,a2)
	addq.w	#8,a7
	bra	l148
l146
	move.l	d3,-(a7)
	move.l	(6,a4),-(a7)
	jsr	_create_and_add_file_lock
	move.l	d0,d5
	move.l	d5,-(a7)
	pea	l149
	jsr	_dbg
	move.l	d5,d0
	lsr.l	#2,d0
	move.l	d0,(12,a2)
	move.l	#0,(16,a2)
	add.w	#16,a7
l148
	move.l	a2,-(a7)
	jsr	_reply_packet
	addq.w	#4,a7
l131
l150	reg	a2/a3/a4/a5/a6/d2/d3/d4/d5
	movem.l	(a7)+,a2/a3/a4/a5/a6/d2/d3/d4/d5
l152	equ	36
	rts
	cnop	0,4
l147
	dc.b	32
	dc.b	32
	dc.b	70
	dc.b	97
	dc.b	105
	dc.b	108
	dc.b	101
	dc.b	100
	dc.b	44
	dc.b	32
	dc.b	101
	dc.b	114
	dc.b	114
	dc.b	111
	dc.b	114
	dc.b	32
	dc.b	99
	dc.b	111
	dc.b	100
	dc.b	101
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l149
	dc.b	32
	dc.b	32
	dc.b	82
	dc.b	101
	dc.b	116
	dc.b	117
	dc.b	114
	dc.b	110
	dc.b	105
	dc.b	110
	dc.b	103
	dc.b	32
	dc.b	108
	dc.b	111
	dc.b	99
	dc.b	107
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l133
	dc.b	65
	dc.b	67
	dc.b	84
	dc.b	73
	dc.b	79
	dc.b	78
	dc.b	95
	dc.b	76
	dc.b	79
	dc.b	67
	dc.b	65
	dc.b	84
	dc.b	69
	dc.b	95
	dc.b	79
	dc.b	66
	dc.b	74
	dc.b	69
	dc.b	67
	dc.b	84
	dc.b	10
	dc.b	0
	cnop	0,4
l134
	dc.b	32
	dc.b	32
	dc.b	112
	dc.b	97
	dc.b	114
	dc.b	101
	dc.b	110
	dc.b	116
	dc.b	32
	dc.b	108
	dc.b	111
	dc.b	99
	dc.b	107
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l135
	dc.b	32
	dc.b	32
	dc.b	110
	dc.b	97
	dc.b	109
	dc.b	101
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	83
	dc.b	10
	dc.b	0
	cnop	0,4
l139
	dc.b	83
	dc.b	72
	dc.b	65
	dc.b	82
	dc.b	69
	dc.b	68
	dc.b	95
	dc.b	76
	dc.b	79
	dc.b	67
	dc.b	75
	dc.b	0
	cnop	0,4
l140
	dc.b	69
	dc.b	88
	dc.b	67
	dc.b	76
	dc.b	85
	dc.b	83
	dc.b	73
	dc.b	86
	dc.b	69
	dc.b	95
	dc.b	76
	dc.b	79
	dc.b	67
	dc.b	75
	dc.b	0
	cnop	0,4
l141
	dc.b	32
	dc.b	32
	dc.b	109
	dc.b	111
	dc.b	100
	dc.b	101
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	115
	dc.b	10
	dc.b	0
	opt	0
	opt	NQLPSMRBT
	public	_action_free_lock
	cnop	0,4
_action_free_lock
	movem.l	l165,-(a7)
	move.l	(4+l167,a7),a4
	move.l	(20,a4),d2
	move.l	(20,a4),d0
	lsl.l	#2,d0
	move.l	d0,a3
	pea	l155
	jsr	_dbg
	move.l	a3,-(a7)
	pea	l156
	jsr	_dbg
	add.w	#12,a7
	move.l	a3,d0
	beq	l158
l157
	move.l	_request_buffer,a5
	move.w	#0,(a5)
	move.w	(10,a4),d0
	move.w	d0,(2,a5)
	move.l	(4,a3),(4,a5)
	move.l	#8,-(a7)
	jsr	_write_req_and_wait_for_res
	move.l	_SysBase,a6
	jsr	-132(a6)
	move.l	_my_volume,a0
	addq.w	#4,a7
	cmp.l	(12,a0),d2
	bne	l160
l159
	move.l	_my_volume,a0
	move.l	(a3),(12,a0)
	bra	l161
l160
	move.l	_my_volume,a0
	move.l	(12,a0),d0
	lsl.l	#2,d0
	move.l	d0,a2
	bra	l163
l162
	move.l	(a2),d0
	lsl.l	#2,d0
	move.l	d0,a2
l163
	cmp.l	(a2),d2
	bne	l162
l164
	move.l	(a3),(a2)
l161
	move.l	_SysBase,a6
	jsr	-138(a6)
	move.l	a3,-(a7)
	jsr	_DosFreeMem
	addq.w	#4,a7
l158
	moveq	#-1,d0
	move.l	d0,(12,a4)
	move.l	#0,(16,a4)
	move.l	a4,-(a7)
	jsr	_reply_packet
	addq.w	#4,a7
l153
l165	reg	a2/a3/a4/a5/a6/d2
	movem.l	(a7)+,a2/a3/a4/a5/a6/d2
l167	equ	24
	rts
	cnop	0,4
l155
	dc.b	65
	dc.b	67
	dc.b	84
	dc.b	73
	dc.b	79
	dc.b	78
	dc.b	95
	dc.b	70
	dc.b	82
	dc.b	69
	dc.b	69
	dc.b	95
	dc.b	76
	dc.b	79
	dc.b	67
	dc.b	75
	dc.b	10
	dc.b	0
	cnop	0,4
l156
	dc.b	32
	dc.b	32
	dc.b	108
	dc.b	111
	dc.b	99
	dc.b	107
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	opt	0
	opt	NQLPSMRBT
	public	_action_copy_dir
	cnop	0,4
_action_copy_dir
	movem.l	l180,-(a7)
	move.l	(4+l182,a7),a2
	move.l	(20,a2),d0
	lsl.l	#2,d0
	move.l	d0,a4
	pea	l170
	jsr	_dbg
	move.l	a4,-(a7)
	pea	l171
	jsr	_dbg
	add.w	#12,a7
	move.l	a4,d0
	bne	l173
l172
	move.l	#0,(12,a2)
	move.l	#0,(16,a2)
	bra	l174
l173
	move.l	_request_buffer,a5
	move.w	#0,(a5)
	move.w	(10,a2),d0
	move.w	d0,(2,a5)
	move.l	(4,a4),(4,a5)
	move.l	#8,-(a7)
	jsr	_write_req_and_wait_for_res
	move.l	_request_buffer,a3
	addq.w	#4,a7
	tst.w	(2,a3)
	bne	l176
l175
	move.w	(4,a3),d0
	ext.l	d0
	move.l	d0,-(a7)
	pea	l177
	jsr	_dbg
	move.l	#0,(12,a2)
	move.w	(4,a3),d0
	ext.l	d0
	move.l	d0,(16,a2)
	addq.w	#8,a7
	bra	l178
l176
	move.l	(8,a4),-(a7)
	move.l	(6,a3),-(a7)
	jsr	_create_and_add_file_lock
	move.l	d0,a6
	move.l	a6,-(a7)
	pea	l179
	jsr	_dbg
	move.l	a6,d0
	lsr.l	#2,d0
	move.l	d0,(12,a2)
	move.l	#0,(16,a2)
	add.w	#16,a7
l178
l174
	move.l	a2,-(a7)
	jsr	_reply_packet
	addq.w	#4,a7
l168
l180	reg	a2/a3/a4/a5/a6
	movem.l	(a7)+,a2/a3/a4/a5/a6
l182	equ	20
	rts
	cnop	0,4
l177
	dc.b	32
	dc.b	32
	dc.b	70
	dc.b	97
	dc.b	105
	dc.b	108
	dc.b	101
	dc.b	100
	dc.b	44
	dc.b	32
	dc.b	101
	dc.b	114
	dc.b	114
	dc.b	111
	dc.b	114
	dc.b	32
	dc.b	99
	dc.b	111
	dc.b	100
	dc.b	101
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l179
	dc.b	32
	dc.b	32
	dc.b	82
	dc.b	101
	dc.b	116
	dc.b	117
	dc.b	114
	dc.b	110
	dc.b	105
	dc.b	110
	dc.b	103
	dc.b	32
	dc.b	108
	dc.b	111
	dc.b	99
	dc.b	107
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l170
	dc.b	65
	dc.b	67
	dc.b	84
	dc.b	73
	dc.b	79
	dc.b	78
	dc.b	95
	dc.b	67
	dc.b	79
	dc.b	80
	dc.b	89
	dc.b	95
	dc.b	68
	dc.b	73
	dc.b	82
	dc.b	10
	dc.b	0
	cnop	0,4
l171
	dc.b	32
	dc.b	32
	dc.b	108
	dc.b	111
	dc.b	99
	dc.b	107
	dc.b	32
	dc.b	116
	dc.b	111
	dc.b	32
	dc.b	100
	dc.b	117
	dc.b	112
	dc.b	108
	dc.b	105
	dc.b	99
	dc.b	97
	dc.b	116
	dc.b	101
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	opt	0
	opt	NQLPSMRBT
	public	_action_parent
	cnop	0,4
_action_parent
	movem.l	l198,-(a7)
	move.l	(4+l200,a7),a2
	move.l	(20,a2),d0
	lsl.l	#2,d0
	move.l	d0,a5
	pea	l185
	jsr	_dbg
	move.l	a5,-(a7)
	pea	l186
	jsr	_dbg
	add.w	#12,a7
	move.l	a5,d0
	bne	l188
l187
	move.l	#0,(12,a2)
	move.l	#211,(16,a2)
	bra	l189
l188
	move.l	_request_buffer,a4
	move.w	#0,(a4)
	move.w	(10,a2),d0
	move.w	d0,(2,a4)
	move.l	(4,a5),(4,a4)
	move.l	#8,-(a7)
	jsr	_write_req_and_wait_for_res
	move.l	_request_buffer,a3
	addq.w	#4,a7
	tst.w	(2,a3)
	bne	l191
l190
	move.w	(4,a3),d0
	ext.l	d0
	move.l	d0,-(a7)
	pea	l192
	jsr	_dbg
	move.l	#0,(12,a2)
	move.w	(4,a3),d0
	ext.l	d0
	move.l	d0,(16,a2)
	addq.w	#8,a7
	bra	l193
l191
	tst.l	(6,a3)
	bne	l195
l194
	move.l	#0,(12,a2)
	move.l	#0,(16,a2)
	bra	l196
l195
	move.l	#-2,-(a7)
	move.l	(6,a3),-(a7)
	jsr	_create_and_add_file_lock
	move.l	d0,a6
	move.l	a6,-(a7)
	pea	l197
	jsr	_dbg
	move.l	a6,d0
	lsr.l	#2,d0
	move.l	d0,(12,a2)
	move.l	#0,(16,a2)
	add.w	#16,a7
l196
l193
l189
	move.l	a2,-(a7)
	jsr	_reply_packet
	addq.w	#4,a7
l183
l198	reg	a2/a3/a4/a5/a6
	movem.l	(a7)+,a2/a3/a4/a5/a6
l200	equ	20
	rts
	cnop	0,4
l192
	dc.b	32
	dc.b	32
	dc.b	70
	dc.b	97
	dc.b	105
	dc.b	108
	dc.b	101
	dc.b	100
	dc.b	44
	dc.b	32
	dc.b	101
	dc.b	114
	dc.b	114
	dc.b	111
	dc.b	114
	dc.b	32
	dc.b	99
	dc.b	111
	dc.b	100
	dc.b	101
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l197
	dc.b	32
	dc.b	32
	dc.b	82
	dc.b	101
	dc.b	116
	dc.b	117
	dc.b	114
	dc.b	110
	dc.b	105
	dc.b	110
	dc.b	103
	dc.b	32
	dc.b	108
	dc.b	111
	dc.b	99
	dc.b	107
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l185
	dc.b	65
	dc.b	67
	dc.b	84
	dc.b	73
	dc.b	79
	dc.b	78
	dc.b	95
	dc.b	80
	dc.b	65
	dc.b	82
	dc.b	69
	dc.b	78
	dc.b	84
	dc.b	10
	dc.b	0
	cnop	0,4
l186
	dc.b	32
	dc.b	32
	dc.b	108
	dc.b	111
	dc.b	99
	dc.b	107
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	opt	0
	opt	NQLPSMRBT
	public	_action_examine_object
	cnop	0,4
_action_examine_object
	movem.l	l213,-(a7)
	move.l	(4+l215,a7),a4
	move.l	(20,a4),d0
	lsl.l	#2,d0
	move.l	d0,a5
	move.l	(24,a4),d0
	lsl.l	#2,d0
	move.l	d0,a3
	pea	l203
	jsr	_dbg
	move.l	a5,-(a7)
	pea	l204
	jsr	_dbg
	move.l	a3,-(a7)
	pea	l205
	jsr	_dbg
	move.l	_request_buffer,a6
	move.w	#0,(a6)
	move.w	(10,a4),d0
	move.w	d0,(2,a6)
	add.w	#20,a7
	move.l	a5,d0
	bne	l207
l206
	moveq	#0,d0
	bra	l208
l207
	move.l	(4,a5),d0
l208
	move.l	d0,(4,a6)
	move.l	#8,-(a7)
	jsr	_write_req_and_wait_for_res
	move.l	_request_buffer,a2
	addq.w	#4,a7
	tst.w	(2,a2)
	bne	l210
l209
	move.w	(4,a2),d0
	ext.l	d0
	move.l	d0,-(a7)
	pea	l211
	jsr	_dbg
	move.l	#0,(12,a4)
	move.w	(4,a2),d0
	ext.l	d0
	move.l	d0,(16,a4)
	addq.w	#8,a7
	bra	l212
l210
	moveq	#0,d0
	move.b	(30,a2),d0
	move.l	d0,d3
	moveq	#1,d0
	add.l	d3,d0
	move.l	d0,d2
	lea	(30,a2),a0
	move.l	a0,a1
	lea	(8,a3),a0
	inline
	move.l	a0,d0
	cmp.l	#16,d2
	blo	.l5
	moveq	#1,d1
	and.b	d0,d1
	beq	.l1
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
.l1
	move.l	a1,d1
	and.b	#1,d1
	beq	.l3
	cmp.l	#$10000,d2
	blo	.l5
.l2
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
	bne	.l2
	bra	.l7
.l3
	moveq	#3,d1
	and.l	d2,d1
	sub.l	d1,d2
.l4
	move.l	(a1)+,(a0)+
	subq.l	#4,d2
	bne	.l4
	move.w	d1,d2
.l5
	subq.w	#1,d2
	blo	.l7
.l6
	move.b	(a1)+,(a0)+
	dbf	d2,.l6
.l7
	einline
	moveq	#1,d0
	add.l	d3,d0
	move.b	#0,(8,a3,d0.l)
	move.w	(6,a2),d0
	ext.l	d0
	move.l	d0,(a3)
	move.w	(8,a2),d0
	ext.l	d0
	move.l	d0,(4,a3)
	move.w	(8,a2),d0
	ext.l	d0
	move.l	d0,(120,a3)
	move.l	(14,a2),(116,a3)
	move.l	(10,a2),(124,a3)
	move.l	#511,d0
	add.l	(10,a2),d0
	moveq	#9,d1
	asr.l	d1,d0
	move.l	d0,(128,a3)
	move.l	(18,a2),(132,a3)
	lea	(18,a2),a0
	lea	(132,a3),a1
	move.l	(4,a0),(4,a1)
	lea	(18,a2),a0
	lea	(132,a3),a1
	move.l	(8,a0),(8,a1)
	move.b	#0,(144,a3)
	moveq	#-1,d0
	move.l	d0,(12,a4)
	move.l	#0,(16,a4)
l212
	move.l	a4,-(a7)
	jsr	_reply_packet
	addq.w	#4,a7
l201
l213	reg	a2/a3/a4/a5/a6/d2/d3
	movem.l	(a7)+,a2/a3/a4/a5/a6/d2/d3
l215	equ	28
	rts
	cnop	0,4
l211
	dc.b	32
	dc.b	32
	dc.b	70
	dc.b	97
	dc.b	105
	dc.b	108
	dc.b	101
	dc.b	100
	dc.b	44
	dc.b	32
	dc.b	101
	dc.b	114
	dc.b	114
	dc.b	111
	dc.b	114
	dc.b	32
	dc.b	99
	dc.b	111
	dc.b	100
	dc.b	101
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l203
	dc.b	65
	dc.b	67
	dc.b	84
	dc.b	73
	dc.b	79
	dc.b	78
	dc.b	95
	dc.b	69
	dc.b	88
	dc.b	65
	dc.b	77
	dc.b	73
	dc.b	78
	dc.b	69
	dc.b	95
	dc.b	79
	dc.b	66
	dc.b	74
	dc.b	69
	dc.b	67
	dc.b	84
	dc.b	10
	dc.b	0
	cnop	0,4
l204
	dc.b	32
	dc.b	32
	dc.b	108
	dc.b	111
	dc.b	99
	dc.b	107
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l205
	dc.b	32
	dc.b	32
	dc.b	102
	dc.b	105
	dc.b	98
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	opt	0
	opt	NQLPSMRBT
	public	_action_examine_next
	cnop	0,4
_action_examine_next
	movem.l	l228,-(a7)
	move.l	(4+l230,a7),a4
	move.l	(20,a4),d0
	lsl.l	#2,d0
	move.l	d0,a6
	move.l	(24,a4),d0
	lsl.l	#2,d0
	move.l	d0,a2
	pea	l218
	jsr	_dbg
	move.l	a6,-(a7)
	pea	l219
	jsr	_dbg
	move.l	a2,-(a7)
	pea	l220
	jsr	_dbg
	move.l	_request_buffer,a5
	move.w	#0,(a5)
	move.w	(10,a4),d0
	move.w	d0,(2,a5)
	add.w	#20,a7
	move.l	a6,d0
	bne	l222
l221
	moveq	#0,d0
	bra	l223
l222
	move.l	(4,a6),d0
l223
	move.l	d0,(4,a5)
	move.w	(2,a2),(8,a5)
	move.l	#10,-(a7)
	jsr	_write_req_and_wait_for_res
	move.l	_request_buffer,a3
	addq.w	#4,a7
	tst.w	(2,a3)
	bne	l225
l224
	move.w	(4,a3),d0
	ext.l	d0
	move.l	d0,-(a7)
	pea	l226
	jsr	_dbg
	move.l	#0,(12,a4)
	move.w	(4,a3),d0
	ext.l	d0
	move.l	d0,(16,a4)
	addq.w	#8,a7
	bra	l227
l225
	moveq	#0,d0
	move.b	(30,a3),d0
	move.l	d0,d3
	moveq	#1,d0
	add.l	d3,d0
	move.l	d0,d2
	lea	(30,a3),a0
	move.l	a0,a1
	lea	(8,a2),a0
	inline
	move.l	a0,d0
	cmp.l	#16,d2
	blo	.l5
	moveq	#1,d1
	and.b	d0,d1
	beq	.l1
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
.l1
	move.l	a1,d1
	and.b	#1,d1
	beq	.l3
	cmp.l	#$10000,d2
	blo	.l5
.l2
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
	bne	.l2
	bra	.l7
.l3
	moveq	#3,d1
	and.l	d2,d1
	sub.l	d1,d2
.l4
	move.l	(a1)+,(a0)+
	subq.l	#4,d2
	bne	.l4
	move.w	d1,d2
.l5
	subq.w	#1,d2
	blo	.l7
.l6
	move.b	(a1)+,(a0)+
	dbf	d2,.l6
.l7
	einline
	moveq	#1,d0
	add.l	d3,d0
	move.b	#0,(8,a2,d0.l)
	move.w	(6,a3),d0
	ext.l	d0
	move.l	d0,(a2)
	move.w	(8,a3),d0
	ext.l	d0
	move.l	d0,(4,a2)
	move.w	(8,a3),d0
	ext.l	d0
	move.l	d0,(120,a2)
	move.l	(14,a3),(116,a2)
	move.l	(10,a3),(124,a2)
	move.l	#511,d0
	add.l	(10,a3),d0
	moveq	#9,d1
	asr.l	d1,d0
	move.l	d0,(128,a2)
	move.l	(18,a3),(132,a2)
	lea	(18,a3),a0
	lea	(132,a2),a1
	move.l	(4,a0),(4,a1)
	lea	(18,a3),a0
	lea	(132,a2),a1
	move.l	(8,a0),(8,a1)
	move.b	#0,(144,a2)
	moveq	#-1,d0
	move.l	d0,(12,a4)
	move.l	#0,(16,a4)
l227
	move.l	a4,-(a7)
	jsr	_reply_packet
	addq.w	#4,a7
l216
l228	reg	a2/a3/a4/a5/a6/d2/d3
	movem.l	(a7)+,a2/a3/a4/a5/a6/d2/d3
l230	equ	28
	rts
	cnop	0,4
l226
	dc.b	32
	dc.b	32
	dc.b	70
	dc.b	97
	dc.b	105
	dc.b	108
	dc.b	101
	dc.b	100
	dc.b	44
	dc.b	32
	dc.b	101
	dc.b	114
	dc.b	114
	dc.b	111
	dc.b	114
	dc.b	32
	dc.b	99
	dc.b	111
	dc.b	100
	dc.b	101
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l218
	dc.b	65
	dc.b	67
	dc.b	84
	dc.b	73
	dc.b	79
	dc.b	78
	dc.b	95
	dc.b	69
	dc.b	88
	dc.b	65
	dc.b	77
	dc.b	73
	dc.b	78
	dc.b	69
	dc.b	95
	dc.b	78
	dc.b	69
	dc.b	88
	dc.b	84
	dc.b	10
	dc.b	0
	cnop	0,4
l219
	dc.b	32
	dc.b	32
	dc.b	108
	dc.b	111
	dc.b	99
	dc.b	107
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l220
	dc.b	32
	dc.b	32
	dc.b	102
	dc.b	105
	dc.b	98
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	opt	0
	opt	NQLPSMRBT
	public	_action_findxxx
	cnop	0,4
_action_findxxx
	movem.l	l254,-(a7)
	move.l	(4+l256,a7),a2
	move.l	(20,a2),d0
	lsl.l	#2,d0
	move.l	d0,a3
	move.l	(24,a2),d0
	lsl.l	#2,d0
	move.l	d0,a6
	move.l	(28,a2),d0
	lsl.l	#2,d0
	move.l	d0,d4
	cmp.l	#1004,(8,a2)
	bne	l234
l233
	pea	l235
	jsr	_dbg
	addq.w	#4,a7
	bra	l236
l234
	cmp.l	#1005,(8,a2)
	bne	l238
l237
	pea	l239
	jsr	_dbg
	addq.w	#4,a7
	bra	l240
l238
	cmp.l	#1006,(8,a2)
	bne	l242
l241
	pea	l243
	jsr	_dbg
	addq.w	#4,a7
l242
l240
l236
	move.l	a3,-(a7)
	pea	l244
	jsr	_dbg
	move.l	a6,-(a7)
	pea	l245
	jsr	_dbg
	move.l	d4,-(a7)
	pea	l246
	jsr	_dbg
	move.l	_request_buffer,a4
	move.w	#0,(a4)
	move.w	(10,a2),d0
	move.w	d0,(2,a4)
	add.w	#24,a7
	move.l	a6,d0
	bne	l248
l247
	moveq	#0,d0
	bra	l249
l248
	move.l	(4,a6),d0
l249
	move.l	d0,(4,a4)
	move.l	d4,a0
	moveq	#0,d3
	move.b	(a0),d3
	moveq	#1,d0
	add.l	d3,d0
	move.l	d0,d2
	move.l	d4,a1
	lea	(8,a4),a0
	inline
	move.l	a0,d0
	cmp.l	#16,d2
	blo	.l5
	moveq	#1,d1
	and.b	d0,d1
	beq	.l1
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
.l1
	move.l	a1,d1
	and.b	#1,d1
	beq	.l3
	cmp.l	#$10000,d2
	blo	.l5
.l2
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
	bne	.l2
	bra	.l7
.l3
	moveq	#3,d1
	and.l	d2,d1
	sub.l	d1,d2
.l4
	move.l	(a1)+,(a0)+
	subq.l	#4,d2
	bne	.l4
	move.w	d1,d2
.l5
	subq.w	#1,d2
	blo	.l7
.l6
	move.b	(a1)+,(a0)+
	dbf	d2,.l6
.l7
	einline
	moveq	#9,d0
	add.l	d3,d0
	move.l	d0,-(a7)
	jsr	_write_req_and_wait_for_res
	move.l	_request_buffer,a5
	addq.w	#4,a7
	tst.w	(2,a5)
	bne	l251
l250
	move.w	(4,a5),d0
	ext.l	d0
	move.l	d0,-(a7)
	pea	l252
	jsr	_dbg
	move.l	#0,(12,a2)
	move.w	(4,a5),d0
	ext.l	d0
	move.l	d0,(16,a2)
	addq.w	#8,a7
	bra	l253
l251
	move.l	(6,a5),(36,a3)
	move.l	_mp,(8,a3)
	move.l	#0,(4,a3)
	moveq	#-1,d0
	move.l	d0,(12,a2)
	move.l	#0,(16,a2)
l253
	move.l	a2,-(a7)
	jsr	_reply_packet
	addq.w	#4,a7
l231
l254	reg	a2/a3/a4/a5/a6/d2/d3/d4
	movem.l	(a7)+,a2/a3/a4/a5/a6/d2/d3/d4
l256	equ	32
	rts
	cnop	0,4
l252
	dc.b	32
	dc.b	32
	dc.b	70
	dc.b	97
	dc.b	105
	dc.b	108
	dc.b	101
	dc.b	100
	dc.b	44
	dc.b	32
	dc.b	101
	dc.b	114
	dc.b	114
	dc.b	111
	dc.b	114
	dc.b	32
	dc.b	99
	dc.b	111
	dc.b	100
	dc.b	101
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l235
	dc.b	65
	dc.b	67
	dc.b	84
	dc.b	73
	dc.b	79
	dc.b	78
	dc.b	95
	dc.b	70
	dc.b	73
	dc.b	78
	dc.b	68
	dc.b	85
	dc.b	80
	dc.b	68
	dc.b	65
	dc.b	84
	dc.b	69
	dc.b	10
	dc.b	0
	cnop	0,4
l239
	dc.b	65
	dc.b	67
	dc.b	84
	dc.b	73
	dc.b	79
	dc.b	78
	dc.b	95
	dc.b	70
	dc.b	73
	dc.b	78
	dc.b	68
	dc.b	73
	dc.b	78
	dc.b	80
	dc.b	85
	dc.b	84
	dc.b	10
	dc.b	0
	cnop	0,4
l243
	dc.b	65
	dc.b	67
	dc.b	84
	dc.b	73
	dc.b	79
	dc.b	78
	dc.b	95
	dc.b	70
	dc.b	73
	dc.b	78
	dc.b	68
	dc.b	79
	dc.b	85
	dc.b	84
	dc.b	80
	dc.b	85
	dc.b	84
	dc.b	10
	dc.b	0
	cnop	0,4
l244
	dc.b	32
	dc.b	32
	dc.b	102
	dc.b	105
	dc.b	108
	dc.b	101
	dc.b	32
	dc.b	104
	dc.b	97
	dc.b	110
	dc.b	100
	dc.b	108
	dc.b	101
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l245
	dc.b	32
	dc.b	32
	dc.b	108
	dc.b	111
	dc.b	99
	dc.b	107
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l246
	dc.b	32
	dc.b	32
	dc.b	110
	dc.b	97
	dc.b	109
	dc.b	101
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	83
	dc.b	10
	dc.b	0
	opt	0
	opt	NQLPSMRBT
	public	_action_read
	cnop	0,4
_action_read
	movem.l	l276,-(a7)
	move.l	(4+l278,a7),a4
	move.l	(20,a4),d6
	move.l	(24,a4),a5
	move.l	(28,a4),d4
	pea	l259
	jsr	_dbg
	move.l	d6,-(a7)
	pea	l260
	jsr	_dbg
	move.l	d4,-(a7)
	pea	l261
	jsr	_dbg
	add.w	#20,a7
	tst.l	d4
	bne	l263
l262
	moveq	#-1,d0
	move.l	d0,(12,a4)
	move.l	#211,(16,a4)
	move.l	a4,-(a7)
	jsr	_reply_packet
	addq.w	#4,a7
	bra	l257
l263
	moveq	#0,d5
	bra	l265
l264
	move.l	d4,d3
	cmp.l	#4096,d3
	ble	l268
l267
	move.l	#4096,d3
l268
	move.l	_request_buffer,a3
	move.w	#0,(a3)
	move.w	(10,a4),d0
	move.w	d0,(2,a3)
	move.l	d6,(4,a3)
	move.l	_data_buffer,a0
	move.l	_A314Base,a6
	jsr	-42(a6)
	move.l	d0,(8,a3)
	move.l	d3,(12,a3)
	move.l	#16,-(a7)
	jsr	_write_req_and_wait_for_res
	move.l	_request_buffer,a2
	addq.w	#4,a7
	tst.w	(2,a2)
	bne	l270
l269
	move.w	(4,a2),d0
	ext.l	d0
	move.l	d0,-(a7)
	pea	l271
	jsr	_dbg
	moveq	#-1,d0
	move.l	d0,(12,a4)
	move.w	(4,a2),d0
	ext.l	d0
	move.l	d0,(16,a4)
	addq.w	#8,a7
	bra	l257
l270
	tst.l	(6,a2)
	beq	l273
	move.l	(6,a2),d2
	move.l	_data_buffer,a1
	move.l	a5,a0
	inline
	move.l	a0,d0
	cmp.l	#16,d2
	blo	.l5
	moveq	#1,d1
	and.b	d0,d1
	beq	.l1
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
.l1
	move.l	a1,d1
	and.b	#1,d1
	beq	.l3
	cmp.l	#$10000,d2
	blo	.l5
.l2
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
	bne	.l2
	bra	.l7
.l3
	moveq	#3,d1
	and.l	d2,d1
	sub.l	d1,d2
.l4
	move.l	(a1)+,(a0)+
	subq.l	#4,d2
	bne	.l4
	move.w	d1,d2
.l5
	subq.w	#1,d2
	blo	.l7
.l6
	move.b	(a1)+,(a0)+
	dbf	d2,.l6
.l7
	einline
	move.l	a5,a1
	add.l	(6,a2),a1
	move.l	a1,a5
	move.l	d5,d0
	add.l	(6,a2),d0
	move.l	d0,d5
	move.l	d4,d0
	sub.l	(6,a2),d0
	move.l	d0,d4
l273
	cmp.l	(6,a2),d3
	ble	l275
l274
	bra	l266
l275
l265
	tst.l	d4
	bne	l264
l266
	move.l	d5,(12,a4)
	move.l	#0,(16,a4)
	move.l	a4,-(a7)
	jsr	_reply_packet
	addq.w	#4,a7
l257
l276	reg	a2/a3/a4/a5/a6/d2/d3/d4/d5/d6
	movem.l	(a7)+,a2/a3/a4/a5/a6/d2/d3/d4/d5/d6
l278	equ	40
	rts
	cnop	0,4
l271
	dc.b	32
	dc.b	32
	dc.b	70
	dc.b	97
	dc.b	105
	dc.b	108
	dc.b	101
	dc.b	100
	dc.b	44
	dc.b	32
	dc.b	101
	dc.b	114
	dc.b	114
	dc.b	111
	dc.b	114
	dc.b	32
	dc.b	99
	dc.b	111
	dc.b	100
	dc.b	101
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l259
	dc.b	65
	dc.b	67
	dc.b	84
	dc.b	73
	dc.b	79
	dc.b	78
	dc.b	95
	dc.b	82
	dc.b	69
	dc.b	65
	dc.b	68
	dc.b	10
	dc.b	0
	cnop	0,4
l260
	dc.b	32
	dc.b	32
	dc.b	97
	dc.b	114
	dc.b	103
	dc.b	49
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l261
	dc.b	32
	dc.b	32
	dc.b	108
	dc.b	101
	dc.b	110
	dc.b	103
	dc.b	116
	dc.b	104
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	opt	0
	opt	NQLPSMRBT
	public	_action_write
	cnop	0,4
_action_write
	movem.l	l296,-(a7)
	move.l	(4+l298,a7),a4
	move.l	(20,a4),d6
	move.l	(24,a4),a5
	move.l	(28,a4),d4
	pea	l281
	jsr	_dbg
	move.l	d6,-(a7)
	pea	l282
	jsr	_dbg
	move.l	d4,-(a7)
	pea	l283
	jsr	_dbg
	moveq	#0,d5
	add.w	#20,a7
	bra	l285
l284
	move.l	d4,d3
	cmp.l	#4096,d3
	ble	l288
l287
	move.l	#4096,d3
l288
	move.l	d3,d2
	move.l	a5,a1
	move.l	_data_buffer,a0
	inline
	move.l	a0,d0
	cmp.l	#16,d2
	blo	.l5
	moveq	#1,d1
	and.b	d0,d1
	beq	.l1
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
.l1
	move.l	a1,d1
	and.b	#1,d1
	beq	.l3
	cmp.l	#$10000,d2
	blo	.l5
.l2
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
	bne	.l2
	bra	.l7
.l3
	moveq	#3,d1
	and.l	d2,d1
	sub.l	d1,d2
.l4
	move.l	(a1)+,(a0)+
	subq.l	#4,d2
	bne	.l4
	move.w	d1,d2
.l5
	subq.w	#1,d2
	blo	.l7
.l6
	move.b	(a1)+,(a0)+
	dbf	d2,.l6
.l7
	einline
	move.l	_request_buffer,a3
	move.w	#0,(a3)
	move.w	(10,a4),d0
	move.w	d0,(2,a3)
	move.l	d6,(4,a3)
	move.l	_data_buffer,a0
	move.l	_A314Base,a6
	jsr	-42(a6)
	move.l	d0,(8,a3)
	move.l	d3,(12,a3)
	move.l	#16,-(a7)
	jsr	_write_req_and_wait_for_res
	move.l	_request_buffer,a2
	addq.w	#4,a7
	tst.w	(2,a2)
	bne	l290
l289
	move.w	(4,a2),d0
	ext.l	d0
	move.l	d0,-(a7)
	pea	l291
	jsr	_dbg
	move.l	d5,(12,a4)
	move.w	(4,a2),d0
	ext.l	d0
	move.l	d0,(16,a4)
	addq.w	#8,a7
	bra	l279
l290
	tst.l	(6,a2)
	beq	l293
	move.l	a5,a1
	add.l	(6,a2),a1
	move.l	a1,a5
	move.l	d5,d0
	add.l	(6,a2),d0
	move.l	d0,d5
	move.l	d4,d0
	sub.l	(6,a2),d0
	move.l	d0,d4
l293
	cmp.l	(6,a2),d3
	ble	l295
l294
	bra	l286
l295
l285
	tst.l	d4
	bne	l284
l286
	move.l	d5,(12,a4)
	move.l	#0,(16,a4)
	move.l	a4,-(a7)
	jsr	_reply_packet
	addq.w	#4,a7
l279
l296	reg	a2/a3/a4/a5/a6/d2/d3/d4/d5/d6
	movem.l	(a7)+,a2/a3/a4/a5/a6/d2/d3/d4/d5/d6
l298	equ	40
	rts
	cnop	0,4
l291
	dc.b	32
	dc.b	32
	dc.b	70
	dc.b	97
	dc.b	105
	dc.b	108
	dc.b	101
	dc.b	100
	dc.b	44
	dc.b	32
	dc.b	101
	dc.b	114
	dc.b	114
	dc.b	111
	dc.b	114
	dc.b	32
	dc.b	99
	dc.b	111
	dc.b	100
	dc.b	101
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l281
	dc.b	65
	dc.b	67
	dc.b	84
	dc.b	73
	dc.b	79
	dc.b	78
	dc.b	95
	dc.b	87
	dc.b	82
	dc.b	73
	dc.b	84
	dc.b	69
	dc.b	10
	dc.b	0
	cnop	0,4
l282
	dc.b	32
	dc.b	32
	dc.b	97
	dc.b	114
	dc.b	103
	dc.b	49
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l283
	dc.b	32
	dc.b	32
	dc.b	108
	dc.b	101
	dc.b	110
	dc.b	103
	dc.b	116
	dc.b	104
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	opt	0
	opt	NQLPSMRBT
	public	_action_seek
	cnop	0,4
_action_seek
	movem.l	l309,-(a7)
	move.l	(4+l311,a7),a2
	move.l	(20,a2),d2
	move.l	(24,a2),d3
	move.l	(28,a2),d4
	pea	l301
	jsr	_dbg
	move.l	d2,-(a7)
	pea	l302
	jsr	_dbg
	move.l	d3,-(a7)
	pea	l303
	jsr	_dbg
	move.l	d4,-(a7)
	pea	l304
	jsr	_dbg
	move.l	_request_buffer,a3
	move.w	#0,(a3)
	move.w	(10,a2),d0
	move.w	d0,(2,a3)
	move.l	d2,(4,a3)
	move.l	d3,(8,a3)
	move.l	d4,(12,a3)
	move.l	#16,-(a7)
	jsr	_write_req_and_wait_for_res
	move.l	_request_buffer,a4
	add.w	#32,a7
	tst.w	(2,a4)
	bne	l306
l305
	move.w	(4,a4),d0
	ext.l	d0
	move.l	d0,-(a7)
	pea	l307
	jsr	_dbg
	moveq	#-1,d0
	move.l	d0,(12,a2)
	move.w	(4,a4),d0
	ext.l	d0
	move.l	d0,(16,a2)
	addq.w	#8,a7
	bra	l308
l306
	move.l	(6,a4),(12,a2)
	move.l	#0,(16,a2)
l308
	move.l	a2,-(a7)
	jsr	_reply_packet
	addq.w	#4,a7
l299
l309	reg	a2/a3/a4/d2/d3/d4
	movem.l	(a7)+,a2/a3/a4/d2/d3/d4
l311	equ	24
	rts
	cnop	0,4
l307
	dc.b	32
	dc.b	32
	dc.b	70
	dc.b	97
	dc.b	105
	dc.b	108
	dc.b	101
	dc.b	100
	dc.b	44
	dc.b	32
	dc.b	101
	dc.b	114
	dc.b	114
	dc.b	111
	dc.b	114
	dc.b	32
	dc.b	99
	dc.b	111
	dc.b	100
	dc.b	101
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l301
	dc.b	65
	dc.b	67
	dc.b	84
	dc.b	73
	dc.b	79
	dc.b	78
	dc.b	95
	dc.b	83
	dc.b	69
	dc.b	69
	dc.b	75
	dc.b	10
	dc.b	0
	cnop	0,4
l302
	dc.b	32
	dc.b	32
	dc.b	97
	dc.b	114
	dc.b	103
	dc.b	49
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l303
	dc.b	32
	dc.b	32
	dc.b	110
	dc.b	101
	dc.b	119
	dc.b	95
	dc.b	112
	dc.b	111
	dc.b	115
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l304
	dc.b	32
	dc.b	32
	dc.b	109
	dc.b	111
	dc.b	100
	dc.b	101
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	opt	0
	opt	NQLPSMRBT
	public	_action_end
	cnop	0,4
_action_end
	movem.l	l316,-(a7)
	move.l	(4+l318,a7),a2
	move.l	(20,a2),d2
	pea	l314
	jsr	_dbg
	move.l	d2,-(a7)
	pea	l315
	jsr	_dbg
	move.l	_request_buffer,a3
	move.w	#0,(a3)
	move.w	(10,a2),d0
	move.w	d0,(2,a3)
	move.l	d2,(4,a3)
	move.l	#8,-(a7)
	jsr	_write_req_and_wait_for_res
	moveq	#-1,d0
	move.l	d0,(12,a2)
	move.l	#0,(16,a2)
	move.l	a2,-(a7)
	jsr	_reply_packet
	add.w	#20,a7
l312
l316	reg	a2/a3/d2
	movem.l	(a7)+,a2/a3/d2
l318	equ	12
	rts
	cnop	0,4
l314
	dc.b	65
	dc.b	67
	dc.b	84
	dc.b	73
	dc.b	79
	dc.b	78
	dc.b	95
	dc.b	69
	dc.b	78
	dc.b	68
	dc.b	10
	dc.b	0
	cnop	0,4
l315
	dc.b	32
	dc.b	32
	dc.b	97
	dc.b	114
	dc.b	103
	dc.b	49
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	opt	0
	opt	NQLPSMRBT
	public	_action_delete_object
	cnop	0,4
_action_delete_object
	movem.l	l331,-(a7)
	move.l	(4+l333,a7),a2
	move.l	(20,a2),d0
	lsl.l	#2,d0
	move.l	d0,a4
	move.l	(24,a2),d0
	lsl.l	#2,d0
	move.l	d0,a5
	pea	l321
	jsr	_dbg
	move.l	a4,-(a7)
	pea	l322
	jsr	_dbg
	move.l	a5,-(a7)
	pea	l323
	jsr	_dbg
	move.l	_request_buffer,a3
	move.w	#0,(a3)
	move.w	(10,a2),d0
	move.w	d0,(2,a3)
	add.w	#20,a7
	move.l	a4,d0
	bne	l325
l324
	moveq	#0,d0
	bra	l326
l325
	move.l	(4,a4),d0
l326
	move.l	d0,(4,a3)
	moveq	#0,d3
	move.b	(a5),d3
	moveq	#1,d0
	add.l	d3,d0
	move.l	d0,d2
	move.l	a5,a1
	lea	(8,a3),a0
	inline
	move.l	a0,d0
	cmp.l	#16,d2
	blo	.l5
	moveq	#1,d1
	and.b	d0,d1
	beq	.l1
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
.l1
	move.l	a1,d1
	and.b	#1,d1
	beq	.l3
	cmp.l	#$10000,d2
	blo	.l5
.l2
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
	bne	.l2
	bra	.l7
.l3
	moveq	#3,d1
	and.l	d2,d1
	sub.l	d1,d2
.l4
	move.l	(a1)+,(a0)+
	subq.l	#4,d2
	bne	.l4
	move.w	d1,d2
.l5
	subq.w	#1,d2
	blo	.l7
.l6
	move.b	(a1)+,(a0)+
	dbf	d2,.l6
.l7
	einline
	moveq	#9,d0
	add.l	d3,d0
	move.l	d0,-(a7)
	jsr	_write_req_and_wait_for_res
	move.l	_request_buffer,a6
	addq.w	#4,a7
	tst.w	(2,a6)
	bne	l328
l327
	move.w	(4,a6),d0
	ext.l	d0
	move.l	d0,-(a7)
	pea	l329
	jsr	_dbg
	move.l	#0,(12,a2)
	move.w	(4,a6),d0
	ext.l	d0
	move.l	d0,(16,a2)
	addq.w	#8,a7
	bra	l330
l328
	moveq	#-1,d0
	move.l	d0,(12,a2)
	move.l	#0,(16,a2)
l330
	move.l	a2,-(a7)
	jsr	_reply_packet
	addq.w	#4,a7
l319
l331	reg	a2/a3/a4/a5/a6/d2/d3
	movem.l	(a7)+,a2/a3/a4/a5/a6/d2/d3
l333	equ	28
	rts
	cnop	0,4
l329
	dc.b	32
	dc.b	32
	dc.b	70
	dc.b	97
	dc.b	105
	dc.b	108
	dc.b	101
	dc.b	100
	dc.b	44
	dc.b	32
	dc.b	101
	dc.b	114
	dc.b	114
	dc.b	111
	dc.b	114
	dc.b	32
	dc.b	99
	dc.b	111
	dc.b	100
	dc.b	101
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l321
	dc.b	65
	dc.b	67
	dc.b	84
	dc.b	73
	dc.b	79
	dc.b	78
	dc.b	95
	dc.b	68
	dc.b	69
	dc.b	76
	dc.b	69
	dc.b	84
	dc.b	69
	dc.b	95
	dc.b	79
	dc.b	66
	dc.b	74
	dc.b	69
	dc.b	67
	dc.b	84
	dc.b	10
	dc.b	0
	cnop	0,4
l322
	dc.b	32
	dc.b	32
	dc.b	108
	dc.b	111
	dc.b	99
	dc.b	107
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l323
	dc.b	32
	dc.b	32
	dc.b	110
	dc.b	97
	dc.b	109
	dc.b	101
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	83
	dc.b	10
	dc.b	0
	opt	0
	opt	NQLPSMRBT
	public	_action_rename_object
	cnop	0,4
_action_rename_object
	movem.l	l351,-(a7)
	move.l	(4+l353,a7),a2
	move.l	(20,a2),d0
	lsl.l	#2,d0
	move.l	d0,a4
	move.l	(24,a2),d0
	lsl.l	#2,d0
	move.l	d0,a6
	move.l	(28,a2),d0
	lsl.l	#2,d0
	move.l	d0,d7
	move.l	(32,a2),d0
	lsl.l	#2,d0
	move.l	d0,d5
	pea	l336
	jsr	_dbg
	move.l	a4,-(a7)
	pea	l337
	jsr	_dbg
	move.l	a6,-(a7)
	pea	l338
	jsr	_dbg
	move.l	a4,-(a7)
	pea	l339
	jsr	_dbg
	move.l	d5,-(a7)
	pea	l340
	jsr	_dbg
	move.l	_request_buffer,a3
	move.w	#0,(a3)
	move.w	(10,a2),d0
	move.w	d0,(2,a3)
	add.w	#36,a7
	move.l	a4,d0
	bne	l342
l341
	moveq	#0,d0
	bra	l343
l342
	move.l	(4,a4),d0
l343
	move.l	d0,(4,a3)
	tst.l	d7
	bne	l345
l344
	moveq	#0,d0
	bra	l346
l345
	move.l	d7,a0
	move.l	(4,a0),d0
l346
	move.l	d0,(8,a3)
	moveq	#0,d3
	move.b	(a6),d3
	move.l	d5,a0
	moveq	#0,d4
	move.b	(a0),d4
	move.b	d3,(12,a3)
	move.b	d4,(13,a3)
	lea	(13,a3),a0
	lea	(1,a0),a5
	move.l	d3,d2
	lea	(1,a6),a0
	move.l	a0,a1
	move.l	a5,a0
	inline
	move.l	a0,d0
	cmp.l	#16,d2
	blo	.l5
	moveq	#1,d1
	and.b	d0,d1
	beq	.l1
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
.l1
	move.l	a1,d1
	and.b	#1,d1
	beq	.l3
	cmp.l	#$10000,d2
	blo	.l5
.l2
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
	bne	.l2
	bra	.l7
.l3
	moveq	#3,d1
	and.l	d2,d1
	sub.l	d1,d2
.l4
	move.l	(a1)+,(a0)+
	subq.l	#4,d2
	bne	.l4
	move.w	d1,d2
.l5
	subq.w	#1,d2
	blo	.l7
.l6
	move.b	(a1)+,(a0)+
	dbf	d2,.l6
.l7
	einline
	add.l	d3,a5
	move.l	d4,d2
	move.l	d5,a0
	addq.l	#1,a0
	move.l	a0,a1
	move.l	a5,a0
	inline
	move.l	a0,d0
	cmp.l	#16,d2
	blo	.l5
	moveq	#1,d1
	and.b	d0,d1
	beq	.l1
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
.l1
	move.l	a1,d1
	and.b	#1,d1
	beq	.l3
	cmp.l	#$10000,d2
	blo	.l5
.l2
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
	bne	.l2
	bra	.l7
.l3
	moveq	#3,d1
	and.l	d2,d1
	sub.l	d1,d2
.l4
	move.l	(a1)+,(a0)+
	subq.l	#4,d2
	bne	.l4
	move.w	d1,d2
.l5
	subq.w	#1,d2
	blo	.l7
.l6
	move.b	(a1)+,(a0)+
	dbf	d2,.l6
.l7
	einline
	moveq	#14,d0
	add.l	d3,d0
	add.l	d4,d0
	move.l	d0,-(a7)
	jsr	_write_req_and_wait_for_res
	move.l	_request_buffer,d6
	move.l	d6,a0
	addq.w	#4,a7
	tst.w	(2,a0)
	bne	l348
l347
	move.l	d6,a0
	move.w	(4,a0),d0
	ext.l	d0
	move.l	d0,-(a7)
	pea	l349
	jsr	_dbg
	move.l	#0,(12,a2)
	move.l	d6,a0
	move.w	(4,a0),d0
	ext.l	d0
	move.l	d0,(16,a2)
	addq.w	#8,a7
	bra	l350
l348
	moveq	#-1,d0
	move.l	d0,(12,a2)
	move.l	#0,(16,a2)
l350
	move.l	a2,-(a7)
	jsr	_reply_packet
	addq.w	#4,a7
l334
l351	reg	a2/a3/a4/a5/a6/d2/d3/d4/d5/d6/d7
	movem.l	(a7)+,a2/a3/a4/a5/a6/d2/d3/d4/d5/d6/d7
l353	equ	44
	rts
	cnop	0,4
l349
	dc.b	32
	dc.b	32
	dc.b	70
	dc.b	97
	dc.b	105
	dc.b	108
	dc.b	101
	dc.b	100
	dc.b	44
	dc.b	32
	dc.b	101
	dc.b	114
	dc.b	114
	dc.b	111
	dc.b	114
	dc.b	32
	dc.b	99
	dc.b	111
	dc.b	100
	dc.b	101
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l336
	dc.b	65
	dc.b	67
	dc.b	84
	dc.b	73
	dc.b	79
	dc.b	78
	dc.b	95
	dc.b	82
	dc.b	69
	dc.b	78
	dc.b	65
	dc.b	77
	dc.b	69
	dc.b	95
	dc.b	79
	dc.b	66
	dc.b	74
	dc.b	69
	dc.b	67
	dc.b	84
	dc.b	10
	dc.b	0
	cnop	0,4
l337
	dc.b	32
	dc.b	32
	dc.b	108
	dc.b	111
	dc.b	99
	dc.b	107
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l338
	dc.b	32
	dc.b	32
	dc.b	110
	dc.b	97
	dc.b	109
	dc.b	101
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	83
	dc.b	10
	dc.b	0
	cnop	0,4
l339
	dc.b	32
	dc.b	32
	dc.b	116
	dc.b	97
	dc.b	114
	dc.b	103
	dc.b	101
	dc.b	116
	dc.b	32
	dc.b	100
	dc.b	105
	dc.b	114
	dc.b	101
	dc.b	99
	dc.b	116
	dc.b	111
	dc.b	114
	dc.b	121
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l340
	dc.b	32
	dc.b	32
	dc.b	110
	dc.b	101
	dc.b	119
	dc.b	32
	dc.b	110
	dc.b	97
	dc.b	109
	dc.b	101
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	83
	dc.b	10
	dc.b	0
	opt	0
	opt	NQLPSMRBT
	public	_action_create_dir
	cnop	0,4
_action_create_dir
	movem.l	l367,-(a7)
	move.l	(4+l369,a7),a2
	move.l	(20,a2),d0
	lsl.l	#2,d0
	move.l	d0,a5
	move.l	(24,a2),d0
	lsl.l	#2,d0
	move.l	d0,a6
	pea	l356
	jsr	_dbg
	move.l	a5,-(a7)
	pea	l357
	jsr	_dbg
	move.l	a6,-(a7)
	pea	l358
	jsr	_dbg
	move.l	_request_buffer,a3
	move.w	#0,(a3)
	move.w	(10,a2),d0
	move.w	d0,(2,a3)
	add.w	#20,a7
	move.l	a5,d0
	bne	l360
l359
	moveq	#0,d0
	bra	l361
l360
	move.l	(4,a5),d0
l361
	move.l	d0,(4,a3)
	moveq	#0,d3
	move.b	(a6),d3
	moveq	#1,d0
	add.l	d3,d0
	move.l	d0,d2
	move.l	a6,a1
	lea	(8,a3),a0
	inline
	move.l	a0,d0
	cmp.l	#16,d2
	blo	.l5
	moveq	#1,d1
	and.b	d0,d1
	beq	.l1
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
.l1
	move.l	a1,d1
	and.b	#1,d1
	beq	.l3
	cmp.l	#$10000,d2
	blo	.l5
.l2
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
	bne	.l2
	bra	.l7
.l3
	moveq	#3,d1
	and.l	d2,d1
	sub.l	d1,d2
.l4
	move.l	(a1)+,(a0)+
	subq.l	#4,d2
	bne	.l4
	move.w	d1,d2
.l5
	subq.w	#1,d2
	blo	.l7
.l6
	move.b	(a1)+,(a0)+
	dbf	d2,.l6
.l7
	einline
	moveq	#9,d0
	add.l	d3,d0
	move.l	d0,-(a7)
	jsr	_write_req_and_wait_for_res
	move.l	_request_buffer,a4
	addq.w	#4,a7
	tst.w	(2,a4)
	bne	l363
l362
	move.w	(4,a4),d0
	ext.l	d0
	move.l	d0,-(a7)
	pea	l364
	jsr	_dbg
	move.l	#0,(12,a2)
	move.w	(4,a4),d0
	ext.l	d0
	move.l	d0,(16,a2)
	addq.w	#8,a7
	bra	l365
l363
	move.l	#-2,-(a7)
	move.l	(6,a4),-(a7)
	jsr	_create_and_add_file_lock
	move.l	d0,d4
	move.l	d4,-(a7)
	pea	l366
	jsr	_dbg
	move.l	d4,d0
	lsr.l	#2,d0
	move.l	d0,(12,a2)
	move.l	#0,(16,a2)
	add.w	#16,a7
l365
	move.l	a2,-(a7)
	jsr	_reply_packet
	addq.w	#4,a7
l354
l367	reg	a2/a3/a4/a5/a6/d2/d3/d4
	movem.l	(a7)+,a2/a3/a4/a5/a6/d2/d3/d4
l369	equ	32
	rts
	cnop	0,4
l364
	dc.b	32
	dc.b	32
	dc.b	70
	dc.b	97
	dc.b	105
	dc.b	108
	dc.b	101
	dc.b	100
	dc.b	44
	dc.b	32
	dc.b	101
	dc.b	114
	dc.b	114
	dc.b	111
	dc.b	114
	dc.b	32
	dc.b	99
	dc.b	111
	dc.b	100
	dc.b	101
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l366
	dc.b	32
	dc.b	32
	dc.b	82
	dc.b	101
	dc.b	116
	dc.b	117
	dc.b	114
	dc.b	110
	dc.b	105
	dc.b	110
	dc.b	103
	dc.b	32
	dc.b	108
	dc.b	111
	dc.b	99
	dc.b	107
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l356
	dc.b	65
	dc.b	67
	dc.b	84
	dc.b	73
	dc.b	79
	dc.b	78
	dc.b	95
	dc.b	67
	dc.b	82
	dc.b	69
	dc.b	65
	dc.b	84
	dc.b	69
	dc.b	95
	dc.b	68
	dc.b	73
	dc.b	82
	dc.b	10
	dc.b	0
	cnop	0,4
l357
	dc.b	32
	dc.b	32
	dc.b	108
	dc.b	111
	dc.b	99
	dc.b	107
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l358
	dc.b	32
	dc.b	32
	dc.b	110
	dc.b	97
	dc.b	109
	dc.b	101
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	83
	dc.b	10
	dc.b	0
	opt	0
	opt	NQLPSMRBT
	public	_action_set_protect
	cnop	0,4
_action_set_protect
	movem.l	l383,-(a7)
	move.l	(4+l385,a7),a2
	move.l	(24,a2),d0
	lsl.l	#2,d0
	move.l	d0,a4
	move.l	(28,a2),d0
	lsl.l	#2,d0
	move.l	d0,a5
	move.l	(32,a2),d3
	pea	l372
	jsr	_dbg
	move.l	a4,-(a7)
	pea	l373
	jsr	_dbg
	move.l	a5,-(a7)
	pea	l374
	jsr	_dbg
	move.l	d3,-(a7)
	pea	l375
	jsr	_dbg
	move.l	_request_buffer,a3
	move.w	#0,(a3)
	move.w	(10,a2),d0
	move.w	d0,(2,a3)
	add.w	#28,a7
	move.l	a4,d0
	bne	l377
l376
	moveq	#0,d0
	bra	l378
l377
	move.l	(4,a4),d0
l378
	move.l	d0,(4,a3)
	move.l	d3,(8,a3)
	moveq	#0,d4
	move.b	(a5),d4
	moveq	#1,d0
	add.l	d4,d0
	move.l	d0,d2
	move.l	a5,a1
	lea	(12,a3),a0
	inline
	move.l	a0,d0
	cmp.l	#16,d2
	blo	.l5
	moveq	#1,d1
	and.b	d0,d1
	beq	.l1
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
.l1
	move.l	a1,d1
	and.b	#1,d1
	beq	.l3
	cmp.l	#$10000,d2
	blo	.l5
.l2
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
	bne	.l2
	bra	.l7
.l3
	moveq	#3,d1
	and.l	d2,d1
	sub.l	d1,d2
.l4
	move.l	(a1)+,(a0)+
	subq.l	#4,d2
	bne	.l4
	move.w	d1,d2
.l5
	subq.w	#1,d2
	blo	.l7
.l6
	move.b	(a1)+,(a0)+
	dbf	d2,.l6
.l7
	einline
	moveq	#13,d0
	add.l	d4,d0
	move.l	d0,-(a7)
	jsr	_write_req_and_wait_for_res
	move.l	_request_buffer,a6
	addq.w	#4,a7
	tst.w	(2,a6)
	bne	l380
l379
	move.w	(4,a6),d0
	ext.l	d0
	move.l	d0,-(a7)
	pea	l381
	jsr	_dbg
	move.l	#0,(12,a2)
	move.w	(4,a6),d0
	ext.l	d0
	move.l	d0,(16,a2)
	addq.w	#8,a7
	bra	l382
l380
	moveq	#-1,d0
	move.l	d0,(12,a2)
	move.l	#0,(16,a2)
l382
	move.l	a2,-(a7)
	jsr	_reply_packet
	addq.w	#4,a7
l370
l383	reg	a2/a3/a4/a5/a6/d2/d3/d4
	movem.l	(a7)+,a2/a3/a4/a5/a6/d2/d3/d4
l385	equ	32
	rts
	cnop	0,4
l381
	dc.b	32
	dc.b	32
	dc.b	70
	dc.b	97
	dc.b	105
	dc.b	108
	dc.b	101
	dc.b	100
	dc.b	44
	dc.b	32
	dc.b	101
	dc.b	114
	dc.b	114
	dc.b	111
	dc.b	114
	dc.b	32
	dc.b	99
	dc.b	111
	dc.b	100
	dc.b	101
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l372
	dc.b	65
	dc.b	67
	dc.b	84
	dc.b	73
	dc.b	79
	dc.b	78
	dc.b	95
	dc.b	83
	dc.b	69
	dc.b	84
	dc.b	95
	dc.b	80
	dc.b	82
	dc.b	79
	dc.b	84
	dc.b	69
	dc.b	67
	dc.b	84
	dc.b	10
	dc.b	0
	cnop	0,4
l373
	dc.b	32
	dc.b	32
	dc.b	108
	dc.b	111
	dc.b	99
	dc.b	107
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l374
	dc.b	32
	dc.b	32
	dc.b	110
	dc.b	97
	dc.b	109
	dc.b	101
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	83
	dc.b	10
	dc.b	0
	cnop	0,4
l375
	dc.b	32
	dc.b	32
	dc.b	109
	dc.b	97
	dc.b	115
	dc.b	107
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	opt	0
	opt	NQLPSMRBT
	public	_action_set_comment
	cnop	0,4
_action_set_comment
	movem.l	l399,-(a7)
	move.l	(4+l401,a7),a2
	move.l	(24,a2),d0
	lsl.l	#2,d0
	move.l	d0,a5
	move.l	(28,a2),d0
	lsl.l	#2,d0
	move.l	d0,a6
	move.l	(32,a2),d0
	lsl.l	#2,d0
	move.l	d0,d5
	pea	l388
	jsr	_dbg
	move.l	a5,-(a7)
	pea	l389
	jsr	_dbg
	move.l	a6,-(a7)
	pea	l390
	jsr	_dbg
	move.l	d5,-(a7)
	pea	l391
	jsr	_dbg
	move.l	_request_buffer,a3
	move.w	#0,(a3)
	move.w	(10,a2),d0
	move.w	d0,(2,a3)
	add.w	#28,a7
	move.l	a5,d0
	bne	l393
l392
	moveq	#0,d0
	bra	l394
l393
	move.l	(4,a5),d0
l394
	move.l	d0,(4,a3)
	moveq	#0,d3
	move.b	(a6),d3
	move.l	d5,a0
	moveq	#0,d4
	move.b	(a0),d4
	move.b	d3,(8,a3)
	move.b	d4,(9,a3)
	lea	(9,a3),a0
	lea	(1,a0),a4
	move.l	d3,d2
	lea	(1,a6),a0
	move.l	a0,a1
	move.l	a4,a0
	inline
	move.l	a0,d0
	cmp.l	#16,d2
	blo	.l5
	moveq	#1,d1
	and.b	d0,d1
	beq	.l1
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
.l1
	move.l	a1,d1
	and.b	#1,d1
	beq	.l3
	cmp.l	#$10000,d2
	blo	.l5
.l2
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
	bne	.l2
	bra	.l7
.l3
	moveq	#3,d1
	and.l	d2,d1
	sub.l	d1,d2
.l4
	move.l	(a1)+,(a0)+
	subq.l	#4,d2
	bne	.l4
	move.w	d1,d2
.l5
	subq.w	#1,d2
	blo	.l7
.l6
	move.b	(a1)+,(a0)+
	dbf	d2,.l6
.l7
	einline
	add.l	d3,a4
	move.l	d4,d2
	move.l	d5,a0
	addq.l	#1,a0
	move.l	a0,a1
	move.l	a4,a0
	inline
	move.l	a0,d0
	cmp.l	#16,d2
	blo	.l5
	moveq	#1,d1
	and.b	d0,d1
	beq	.l1
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
.l1
	move.l	a1,d1
	and.b	#1,d1
	beq	.l3
	cmp.l	#$10000,d2
	blo	.l5
.l2
	move.b	(a1)+,(a0)+
	subq.l	#1,d2
	bne	.l2
	bra	.l7
.l3
	moveq	#3,d1
	and.l	d2,d1
	sub.l	d1,d2
.l4
	move.l	(a1)+,(a0)+
	subq.l	#4,d2
	bne	.l4
	move.w	d1,d2
.l5
	subq.w	#1,d2
	blo	.l7
.l6
	move.b	(a1)+,(a0)+
	dbf	d2,.l6
.l7
	einline
	moveq	#10,d0
	add.l	d3,d0
	add.l	d4,d0
	move.l	d0,-(a7)
	jsr	_write_req_and_wait_for_res
	move.l	_request_buffer,d6
	move.l	d6,a0
	addq.w	#4,a7
	tst.w	(2,a0)
	bne	l396
l395
	move.l	d6,a0
	move.w	(4,a0),d0
	ext.l	d0
	move.l	d0,-(a7)
	pea	l397
	jsr	_dbg
	move.l	#0,(12,a2)
	move.l	d6,a0
	move.w	(4,a0),d0
	ext.l	d0
	move.l	d0,(16,a2)
	addq.w	#8,a7
	bra	l398
l396
	moveq	#-1,d0
	move.l	d0,(12,a2)
	move.l	#0,(16,a2)
l398
	move.l	a2,-(a7)
	jsr	_reply_packet
	addq.w	#4,a7
l386
l399	reg	a2/a3/a4/a5/a6/d2/d3/d4/d5/d6
	movem.l	(a7)+,a2/a3/a4/a5/a6/d2/d3/d4/d5/d6
l401	equ	40
	rts
	cnop	0,4
l397
	dc.b	32
	dc.b	32
	dc.b	70
	dc.b	97
	dc.b	105
	dc.b	108
	dc.b	101
	dc.b	100
	dc.b	44
	dc.b	32
	dc.b	101
	dc.b	114
	dc.b	114
	dc.b	111
	dc.b	114
	dc.b	32
	dc.b	99
	dc.b	111
	dc.b	100
	dc.b	101
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l388
	dc.b	65
	dc.b	67
	dc.b	84
	dc.b	73
	dc.b	79
	dc.b	78
	dc.b	95
	dc.b	83
	dc.b	69
	dc.b	84
	dc.b	95
	dc.b	67
	dc.b	79
	dc.b	77
	dc.b	77
	dc.b	69
	dc.b	78
	dc.b	84
	dc.b	10
	dc.b	0
	cnop	0,4
l389
	dc.b	32
	dc.b	32
	dc.b	108
	dc.b	111
	dc.b	99
	dc.b	107
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l390
	dc.b	32
	dc.b	32
	dc.b	110
	dc.b	97
	dc.b	109
	dc.b	101
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	83
	dc.b	10
	dc.b	0
	cnop	0,4
l391
	dc.b	32
	dc.b	32
	dc.b	99
	dc.b	111
	dc.b	109
	dc.b	109
	dc.b	101
	dc.b	110
	dc.b	116
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	83
	dc.b	10
	dc.b	0
	opt	0
	opt	NQLPSMRBT
	public	_fill_info_data
	cnop	0,4
_fill_info_data
	movem.l	l404,-(a7)
	move.l	(4+l406,a7),a2
	moveq	#36,d2
	moveq	#0,d0
	move.l	a2,a0
	inline
	move.l	a0,a1
	cmp.l	#16,d2
	blo	.l3
	move.l	a0,d1
	and.b	#1,d1
	beq	.l1
	move.b	d0,(a0)+
	subq.l	#1,d2
.l1
	move.b	d0,d1
	lsl.w	#8,d0
	move.b	d1,d0
	move.w	d0,d1
	swap	d0
	move.w	d1,d0
	moveq	#3,d1
	and.l	d2,d1
	sub.l	d1,d2
.l2
	move.l	d0,(a0)+
	subq.l	#4,d2
	bne	.l2
	move.w	d1,d2
.l3
	subq.w	#1,d2
	blo	.l5
.l4
	move.b	d0,(a0)+
	dbf	d2,.l4
.l5
	move.l	a1,d0
	einline
	move.l	d0,a1
	moveq	#82,d0
	move.l	d0,(8,a2)
	move.l	#524288,(12,a2)
	moveq	#10,d0
	move.l	d0,(16,a2)
	move.l	#512,(20,a2)
	move.l	_my_volume,a0
	move.l	(32,a0),(24,a2)
	move.l	_my_volume,d0
	lsr.l	#2,d0
	move.l	d0,(28,a2)
	moveq	#-1,d0
	move.l	d0,(32,a2)
l402
l404	reg	a2/d2
	movem.l	(a7)+,a2/d2
l406	equ	8
	rts
	opt	0
	opt	NQLPSMRBT
	public	_action_info
	cnop	0,4
_action_info
	movem.l	l411,-(a7)
	move.l	(4+l413,a7),a2
	move.l	(20,a2),d0
	lsl.l	#2,d0
	move.l	d0,a3
	move.l	(24,a2),d0
	lsl.l	#2,d0
	move.l	d0,a4
	pea	l409
	jsr	_dbg
	move.l	a3,-(a7)
	pea	l410
	jsr	_dbg
	move.l	a4,-(a7)
	jsr	_fill_info_data
	moveq	#-1,d0
	move.l	d0,(12,a2)
	move.l	#0,(16,a2)
	move.l	a2,-(a7)
	jsr	_reply_packet
	add.w	#20,a7
l407
l411	reg	a2/a3/a4
	movem.l	(a7)+,a2/a3/a4
l413	equ	12
	rts
	cnop	0,4
l409
	dc.b	65
	dc.b	67
	dc.b	84
	dc.b	73
	dc.b	79
	dc.b	78
	dc.b	95
	dc.b	73
	dc.b	78
	dc.b	70
	dc.b	79
	dc.b	10
	dc.b	0
	cnop	0,4
l410
	dc.b	32
	dc.b	32
	dc.b	108
	dc.b	111
	dc.b	99
	dc.b	107
	dc.b	32
	dc.b	61
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	opt	0
	opt	NQLPSMRBT
	public	_action_disk_info
	cnop	0,4
_action_disk_info
	movem.l	l417,-(a7)
	move.l	(4+l419,a7),a2
	move.l	(20,a2),d0
	lsl.l	#2,d0
	move.l	d0,a3
	pea	l416
	jsr	_dbg
	move.l	a3,-(a7)
	jsr	_fill_info_data
	moveq	#-1,d0
	move.l	d0,(12,a2)
	move.l	#0,(16,a2)
	move.l	a2,-(a7)
	jsr	_reply_packet
	add.w	#12,a7
l414
l417	reg	a2/a3
	movem.l	(a7)+,a2/a3
l419	equ	8
	rts
	cnop	0,4
l416
	dc.b	65
	dc.b	67
	dc.b	84
	dc.b	73
	dc.b	79
	dc.b	78
	dc.b	95
	dc.b	68
	dc.b	73
	dc.b	83
	dc.b	75
	dc.b	95
	dc.b	73
	dc.b	78
	dc.b	70
	dc.b	79
	dc.b	10
	dc.b	0
	opt	0
	opt	NQLPSMRBT
	public	_start
	cnop	0,4
_start
	movem.l	l450,-(a7)
	move.l	a0,a4
	move.l	4,_SysBase
	moveq	#0,d0
	lea	l422,a1
	move.l	_SysBase,a6
	jsr	-552(a6)
	move.l	d0,a0
	move.l	a0,_DOSBase
	move.l	#0,-(a7)
	move.l	#0,-(a7)
	jsr	_MyCreatePort
	move.l	d0,_mp
	move.l	a4,-(a7)
	jsr	_startup_fs_handler
	add.w	#12,a7
l423
	move.l	_mp,a0
	move.l	_SysBase,a6
	jsr	-384(a6)
	move.l	_mp,a0
	move.l	_SysBase,a6
	jsr	-372(a6)
	move.l	d0,a3
	move.l	(10,a3),a2
	move.l	(8,a2),d0
	moveq	#8,d1
	cmp.l	d0,d1
	beq	l427
	move.l	d0,d1
	sub.l	#15,d1
	cmp.l	#14,d1
	bhi	l454
	lsl.l	#2,d1
	move.l	l453(pc,d1.l),a0
	jmp	(a0)
	cnop	0,4
l453
	dc.l	l428
	dc.l	l440
	dc.l	l441
	dc.l	l454
	dc.l	l429
	dc.l	l454
	dc.l	l443
	dc.l	l442
	dc.l	l431
	dc.l	l432
	dc.l	l445
	dc.l	l446
	dc.l	l454
	dc.l	l444
	dc.l	l430
l454
	moveq	#82,d1
	cmp.l	d0,d1
	beq	l436
	moveq	#87,d1
	cmp.l	d0,d1
	beq	l437
	cmp.l	#1004,d0
	beq	l433
	cmp.l	#1005,d0
	beq	l434
	cmp.l	#1006,d0
	beq	l435
	cmp.l	#1007,d0
	beq	l439
	cmp.l	#1008,d0
	beq	l438
	bra	l447
l427
	move.l	a2,-(a7)
	jsr	_action_locate_object
	addq.w	#4,a7
	bra	l426
l428
	move.l	a2,-(a7)
	jsr	_action_free_lock
	addq.w	#4,a7
	bra	l426
l429
	move.l	a2,-(a7)
	jsr	_action_copy_dir
	addq.w	#4,a7
	bra	l426
l430
	move.l	a2,-(a7)
	jsr	_action_parent
	addq.w	#4,a7
	bra	l426
l431
	move.l	a2,-(a7)
	jsr	_action_examine_object
	addq.w	#4,a7
	bra	l426
l432
	move.l	a2,-(a7)
	jsr	_action_examine_next
	addq.w	#4,a7
	bra	l426
l433
	move.l	a2,-(a7)
	jsr	_action_findxxx
	addq.w	#4,a7
	bra	l426
l434
	move.l	a2,-(a7)
	jsr	_action_findxxx
	addq.w	#4,a7
	bra	l426
l435
	move.l	a2,-(a7)
	jsr	_action_findxxx
	addq.w	#4,a7
	bra	l426
l436
	move.l	a2,-(a7)
	jsr	_action_read
	addq.w	#4,a7
	bra	l426
l437
	move.l	a2,-(a7)
	jsr	_action_write
	addq.w	#4,a7
	bra	l426
l438
	move.l	a2,-(a7)
	jsr	_action_seek
	addq.w	#4,a7
	bra	l426
l439
	move.l	a2,-(a7)
	jsr	_action_end
	addq.w	#4,a7
	bra	l426
l440
	move.l	a2,-(a7)
	jsr	_action_delete_object
	addq.w	#4,a7
	bra	l426
l441
	move.l	a2,-(a7)
	jsr	_action_rename_object
	addq.w	#4,a7
	bra	l426
l442
	move.l	a2,-(a7)
	jsr	_action_create_dir
	addq.w	#4,a7
	bra	l426
l443
	move.l	a2,-(a7)
	jsr	_action_set_protect
	addq.w	#4,a7
	bra	l426
l444
	move.l	a2,-(a7)
	jsr	_action_set_comment
	addq.w	#4,a7
	bra	l426
l445
	move.l	a2,-(a7)
	jsr	_action_disk_info
	addq.w	#4,a7
	bra	l426
l446
	move.l	a2,-(a7)
	jsr	_action_info
	addq.w	#4,a7
	bra	l426
l447
	move.l	(8,a2),-(a7)
	pea	l448
	jsr	_dbg
	move.l	#0,(12,a2)
	move.l	#209,(16,a2)
	move.l	a2,-(a7)
	jsr	_reply_packet
	add.w	#12,a7
l426
	bra	l423
l425
	pea	l449
	jsr	_dbg
	move.l	_DOSBase,a1
	move.l	_SysBase,a6
	jsr	-414(a6)
	addq.w	#4,a7
l420
l450	reg	a2/a3/a4/a6
	movem.l	(a7)+,a2/a3/a4/a6
l452	equ	16
	rts
	cnop	0,4
l448
	dc.b	85
	dc.b	110
	dc.b	107
	dc.b	110
	dc.b	111
	dc.b	119
	dc.b	110
	dc.b	32
	dc.b	97
	dc.b	99
	dc.b	116
	dc.b	105
	dc.b	111
	dc.b	110
	dc.b	58
	dc.b	32
	dc.b	36
	dc.b	108
	dc.b	10
	dc.b	0
	cnop	0,4
l422
	dc.b	100
	dc.b	111
	dc.b	115
	dc.b	46
	dc.b	108
	dc.b	105
	dc.b	98
	dc.b	114
	dc.b	97
	dc.b	114
	dc.b	121
	dc.b	0
	cnop	0,4
l449
	dc.b	83
	dc.b	104
	dc.b	117
	dc.b	116
	dc.b	116
	dc.b	105
	dc.b	110
	dc.b	103
	dc.b	32
	dc.b	100
	dc.b	111
	dc.b	119
	dc.b	110
	dc.b	10
	dc.b	0
	public	_default_volume_name
	;section	"DATA",data
	cnop	0,4
_default_volume_name
	dc.b	6
	dc.b	80
	dc.b	105
	dc.b	68
	dc.b	105
	dc.b	115
	dc.b	107
	dc.b	0
	public	_request_buffer
	cnop	0,4
_request_buffer
	dc.l	0
	public	_data_buffer
	cnop	0,4
_data_buffer
	dc.l	0
	public	_SysBase
	;section	"BSS",bss
	cnop	0,4
_SysBase
	ds.b	4
	public	_DOSBase
	cnop	0,4
_DOSBase
	ds.b	4
	public	_A314Base
	cnop	0,4
_A314Base
	ds.b	4
	public	_mp
	cnop	0,4
_mp
	ds.b	4
	public	_my_volume
	cnop	0,4
_my_volume
	ds.b	4
	public	_device_name
	cnop	0,4
_device_name
	ds.b	32
	public	_timer_mp
	cnop	0,4
_timer_mp
	ds.b	4
	public	_tr
	cnop	0,4
_tr
	ds.b	4
	public	_a314_mp
	cnop	0,4
_a314_mp
	ds.b	4
	public	_a314_ior
	cnop	0,4
_a314_ior
	ds.b	4
	public	_socket
	cnop	0,4
_socket
	ds.b	4
