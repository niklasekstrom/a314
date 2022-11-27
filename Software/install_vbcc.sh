#!/bin/sh
DEST=/opt/vbcc
WGET=wget
TAR_OPTS=xzf

install_assembler() {
    rm -rf vasm.tar.gz vasm
    ${WGET} http://sun.hasenbraten.de/vasm/release/vasm.tar.gz
    tar ${TAR_OPTS} vasm.tar.gz
    cd vasm
    make CPU=m68k SYNTAX=mot
    cp vasmm68k_mot vobjdump $DEST
    cd ..
    rm -rf vasm.tar.gz vasm
}

install_linker() {
    rm -rf vlink.tar.gz vlink
    ${WGET} http://sun.hasenbraten.de/vlink/release/vlink.tar.gz
    tar ${TAR_OPTS} vlink.tar.gz
    cd vlink
    make
    cp vlink $DEST
    cd ..
    rm -rf vlink.tar.gz vlink
}

install_compiler() {
    rm -rf vbcc.tar.gz vbcc
    ${WGET} http://www.ibaug.de/vbcc/vbcc.tar.gz
    tar ${TAR_OPTS} vbcc.tar.gz
    cd vbcc
    mkdir bin
    make TARGET=m68k
    make TARGET=m68ks
    cp bin/vbcc* bin/vc bin/vprof $DEST
    cd ..
    rm -rf vbcc.tar.gz vbcc
}

install_target_amigaos() {
    rm -rf vbcc_target_m68k-amigaos*
    ${WGET} http://phoenix.owl.de/vbcc/2022-05-22/vbcc_target_m68k-amigaos.lha
    lha x vbcc_target_m68k-amigaos.lha > /dev/null
    mkdir -p $DEST/targets
    mv vbcc_target_m68k-amigaos/targets/* $DEST/targets/
    rm -rf vbcc_target_m68k-amigaos*
}

install_target_kick13() {
    rm -rf vbcc_target_m68k-kick13*
    ${WGET} http://phoenix.owl.de/vbcc/2017-08-14/vbcc_target_m68k-kick13.lha
    #${WGET} http://phoenix.owl.de/vbcc/2022-05-22/vbcc_target_m68k-kick13.lha
    lha x vbcc_target_m68k-kick13.lha > /dev/null
    echo "
--- vbcc_target_m68k-kick13/targets/m68k-kick13/include/clib/expansion_protos.h	2017-01-11 10:31:32.000000000 +0100
+++ /opt/vbcc.niklas/targets/m68k-kick13/include/clib/expansion_protos.h	2021-05-15 22:24:17.000000000 +0200
@@ -4,7 +4,7 @@
 #endif
 
 /* CPTR */
-#ifndef #ifndef EXEC_TYPES_H 
+#ifndef EXEC_TYPES_H
 #include <exec/types.h>
 #endif
" | patch -p0
    mkdir -p $DEST/targets
    mv vbcc_target_m68k-kick13/targets/* $DEST/targets/
    rm -rf vbcc_target_m68k-kick13*
}

install_ndk() {
    rm -rf NDK3.2.lha
    ${WGET} http://aminet.net/dev/misc/NDK3.2.lha
    mkdir NDK32
    cd NDK32
    lha x ../NDK3.2.lha > /dev/null
    cd ..
    mv NDK32 $DEST/../
    rm -rf NDK3.2.lha
}

install_config() {
    rm -rf vbcc_unix_config.tar.gz
    ${WGET} https://server.owl.de/~frank/vbcc/2019-10-04/vbcc_unix_config.tar.gz
    tar ${TAR_OPTS} vbcc_unix_config.tar.gz
    mv config $DEST
    echo "-cc=vbccm68k -c99 -quiet -hunkdebug %s -o= %s %s -O=%ld -no-cpp-warn -I$VBCC/targets/m68k-kick13/include -I$VBCC/../NDK32/Include_H
-ccv=vbccm68k -c99 -hunkdebug %s -o= %s %s -O=%ld -no-cpp-warn -I$VBCC/targets/m68k-kick13/include -I$VBCC/../NDK32/Include_H
-as=vasmm68k_mot -quiet -Fhunk -kick1hunks -phxass -nowarn=62 %s -o %s
-asv=vasmm68k_mot -Fhunk -kick1hunks -phxass -nowarn=62 %s -o %s
-rm=rm -f %s
-rmv=rm %s
-ld=vlink -bamigahunk -x -Bstatic -Cvbcc -nostdlib -Z -mrel $VBCC/targets/m68k-kick13/lib/startup.o %s %s -L$VBCC/targets/m68k-kick13/lib -lvc -o %s
-l2=vlink -bamigahunk -x -Bstatic -Cvbcc -nostdlib -Z -mrel %s %s -L$VBCC/targets/m68k-kick13/lib -o %s
-ldv=vlink -bamigahunk -t -x -Bstatic -Cvbcc -nostdlib -Z -mrel $VBCC/targets/m68k-kick13/lib/startup.o %s %s -L$VBCC/targets/m68k-kick13/lib -lvc -o %s
-l2v=vlink -bamigahunk -t -x -Bstatic -Cvbcc -nostdlib -Z -mrel %s %s -L$VBCC/targets/m68k-kick13/lib -o %s
-ldnodb=-s
-ul=-l%s
-cf=-F%s
-ml=1000
" > $DEST/config/vc.config
    rm -rf vbcc_unix_config.tar.gz
}


mkdir -p $DEST
install_assembler
install_linker
install_compiler
install_target_amigaos
install_target_kick13
install_ndk
install_config

echo -------------------------------------
echo Add the follow lines to your .profile
echo export VBCC=\"$DEST\"
echo PATH=\"\$VBCC:\$PATH\"
