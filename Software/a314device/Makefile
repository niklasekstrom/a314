TARGET=a314.device
.PHONY: $TARGET
VBCC?=/opt/vbcc/
LN=vc
NDK=$(VBCC)/NDK_3.9
AS=vasmm68k_mot
NDK_INC=$(NDK)/Include/include_h
VBCC_TARGET=$(VBCC)/targets/m68k-amigaos
ASFLAGS= -Fhunk -I$(NDK)/Include/include_i -I../../include -I../
CC=vc +aos68k 
CFLAGS= -c99 -O3 -size -I$(NDK_INC)/libraries/ -I$(NDK_INC) -I. -maxoptpasses=20 -nostdlib
LN=vlink
LNFLAGS=-L$(VBCC)/targets/m68k-amigaos/lib -lamiga -lauto
all: $(TARGET)
$(TARGET): $(OBJS) 
	$(CC) romtag.asm a314driver.c int_server.asm $(CFLAGS) $(LNFLAGS) -o $(TARGET)
%.o: %.asm
	$(AS) $(ASFLAGS)  $< -o $(@) -L  $(@).lst
clean:
	rm -f *.o $(OBJS) *.adf *~ $(TARGET) *.lst
distclean: clean
