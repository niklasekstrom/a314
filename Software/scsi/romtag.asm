    XREF    _device_name
    XREF    _id_string
    XREF    _auto_init_tables
    XREF    _cli_main

RTC_MATCHWORD:  equ    $4afc
RTF_AUTOINIT:   equ    (1<<7)
RTF_COLDSTART:  equ    (1<<0)
NT_DEVICE:      equ    3
VERSION:        equ    1
PRIORITY:       equ    -5

        section    CODE,code

        jsr     _cli_main
        rts

romtag:
        dc.w    RTC_MATCHWORD
        dc.l    romtag
        dc.l    endcode
        dc.b    RTF_AUTOINIT|RTF_COLDSTART
        dc.b    VERSION
        dc.b    NT_DEVICE
        dc.b    PRIORITY
        dc.l    _device_name
        dc.l    _id_string
        dc.l    _auto_init_tables
endcode:
