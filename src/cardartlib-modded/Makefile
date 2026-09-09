# -*- makefile -*-

include ../Makefile-common

# Input files
BASEDIR = ..
SUBDIRS = src
LIBS = gdiplus boost_filesystem-$(BOOST_SUFFIX) boost_system-$(BOOST_SUFFIX)

# Output files
OUTPUT = CardArtLib.dll

include ../Makefile-targets

install: ../../Cardartlib.dll

../../Cardartlib.dll: $(OUTPUT)
	/bin/cp -f CardArtLib.dll CardArtLib.dbg ../..
