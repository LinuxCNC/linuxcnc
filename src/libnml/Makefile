# Makefile for rcslib/src directory
# This makefile calls makes in subdirectories and includes the target info to
# create the library from the object files.

# include Makefile.lib
# make rules.
include ../Makefile.inc

LOCAL_CXXFLAGS = $(LIBFLAGS)
LOCAL_FLAGS = $(LIBFLAGS)

all: headers

all headers clean depend indent install:
	(cd linklist; make $@)
	(cd rcs; make $@)
	(cd inifile; make $@)
	(cd os_intf; make $@)
	(cd buffer; make $@)
	(cd cms; make $@)
	(cd nml; make $@)
	(make -f Makefile.lib $@)

.PHONY: all clean depend headers indent install
