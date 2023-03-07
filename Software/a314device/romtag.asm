    XDEF    _debug_process_seglist
    XREF    _debug_process_run

RTC_MATCHWORD:	equ	$4afc
RTF_AUTOINIT:	equ	(1<<7)
RTF_COLDSTART:	equ	(1<<0)
NT_DEVICE:	equ	3
VERSION:	equ	1
PRIORITY:	equ	-3

		section	code,code

		moveq	#-1,d0
		rts

romtag:
		dc.w	RTC_MATCHWORD
		dc.l	romtag
		dc.l	endcode
		dc.b	RTF_AUTOINIT | RTF_COLDSTART
		dc.b	VERSION
		dc.b	NT_DEVICE
		dc.b	PRIORITY
		dc.l	_device_name
		dc.l	_id_string
		dc.l	_auto_init_tables
endcode:

        cnop    0,4

        dc.l    16
_debug_process_seglist:
        dc.l    0
        jmp     _debug_process_run
