# Makefile for rcslib/src directory
# This makefile calls makes in subdirectories and includes the target info to
# create the library from the object files.

# include Makefile.lib
# make rules.

all: headers

all headers clean depend indent install:
	(cd rs274ngc; make $@)
	(cd log; make $@)
	(cd ini; make $@)
	(cd nml_intf; make $@)
	(cd hal_intf; make $@)
	(cd kinematics; make $@)
	(cd motion; make $@)
	(cd iotask; make $@)
	(cd task; make $@)
	(cd usr_intf; make $@)

.PHONY: all clean depend headers indent install
