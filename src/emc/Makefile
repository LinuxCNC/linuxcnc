# Makefile for rcslib/src directory
# This makefile calls makes in subdirectories and includes the target info to
# create the library from the object files.

# include Makefile.lib
# make rules.

all: headers

all headers clean indent install:
	(cd rs274ngc; make $@)
	(cd log; make $@)
	(cd ini; make $@)
	(cd nml_intf; make $@)
	(cd motion; make $@)
	(cd task; make $@)

.PHONY: all clean depend headers indent install
