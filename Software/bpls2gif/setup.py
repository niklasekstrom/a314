#!/usr/bin/python
# -*- coding: utf-8 -*-

from distutils.core import setup, Extension

setup(  name            = "bpls2gif",
        version         = "1.0",
        description     = "Encode Amiga bitplanes as GIF file",
        author          = "Niklas Ekström",
        url             = "http://github.com/niklasekstrom/a314",
        ext_modules     = [Extension("bpls2gif", ["bpls2gif.c"])]
)
