    XREF    _library_name
    XREF    _id_string
    XREF    _auto_init_tables

RTC_MATCHWORD:  equ     $4afc
RTF_AUTOINIT:   equ     (1<<7)
NT_LIBRARY:     equ     9
VERSION:        equ     1
PRIORITY:       equ     0

    section CODE,CODE

    moveq   #-1,d0
    rts

romtag:
    dc.w    RTC_MATCHWORD
    dc.l    romtag
    dc.l    endcode
    dc.b    RTF_AUTOINIT
    dc.b    VERSION
    dc.b    NT_LIBRARY
    dc.b    PRIORITY
    dc.l    _library_name
    dc.l    _id_string
    dc.l    _auto_init_tables
endcode:
