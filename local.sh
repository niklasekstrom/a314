#!/bin/bash

VBCC=/opt/vbcc
NDK32=/opt/ndk32

PATH=$PATH:$VBCC/bin

make -j -C Software
VBCC=$VBCC NDK32=$NDK32 make -j -C Software -f Makefile-amiga -j
