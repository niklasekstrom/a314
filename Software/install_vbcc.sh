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
    patch -p0 < patch.vbcc_target_m68k-kick13_patch
    mkdir -p $DEST/targets
    mv vbcc_target_m68k-kick13/targets/* $DEST/targets/
    rm -rf vbcc_target_m68k-kick13*
}

install_ndk() {
    rm -rf NDK3.2.lha
    ${WGET} http://aminet.net/dev/misc/NDK3.2.lha
    mkdir NDK_3.2
	cd NDK_3.2
    lha x ../NDK3.2.lha > /dev/null
	cd ..
    mv NDK_3.2 $DEST/../
    rm -rf NDK3.2.lha
}

install_config() {
    rm -rf vbcc_unix_config.tar.gz
    ${WGET} https://server.owl.de/~frank/vbcc/2019-10-04/vbcc_unix_config.tar.gz
    tar ${TAR_OPTS} vbcc_unix_config.tar.gz
    mv config $DEST
    cp vc.config $DEST/config
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
