# Makefile for emc directory
# This makefile calls makes in subdirectories

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../Makefile.inc

# Public headers
HEADERS = \
global_defs.h


all: headers
	(cd log; make $@)
	(cd ini; make $@)
	(cd nml_intf; make $@)
	(cd rs274ngc; make $@)
	(cd canterp; make $@)
	#(cd hal_intf; make $@)
	(cd kinematics; make $@)
	(cd motion; make $@)
	(cd iotask; make $@)
	(cd task; make $@)
	(cd usr_intf; make $@)

# Generic rules
include $(SRC_DIR)/Make.rules

headers: def_headers
	(cd log; make $@)
	(cd ini; make $@)
	(cd nml_intf; make $@)
	(cd rs274ngc; make $@)
	(cd canterp; make $@)
	#(cd hal_intf; make $@)
	(cd kinematics; make $@)
	(cd motion; make $@)
	(cd iotask; make $@)
	(cd task; make $@)
	(cd usr_intf; make $@)

indent: def_indent
	(cd log; make $@)
	(cd ini; make $@)
	(cd nml_intf; make $@)
	(cd rs274ngc; make $@)
	(cd canterp; make $@)
	#(cd hal_intf; make $@)
	(cd kinematics; make $@)
	(cd motion; make $@)
	(cd iotask; make $@)
	(cd task; make $@)
	(cd usr_intf; make $@)

depend: def_depend
	(cd log; make $@)
	(cd ini; make $@)
	(cd nml_intf; make $@)
	(cd rs274ngc; make $@)
	(cd canterp; make $@)
	#(cd hal_intf; make $@)
	(cd kinematics; make $@)
	(cd motion; make $@)
	(cd iotask; make $@)
	(cd task; make $@)
	(cd usr_intf; make $@)

clean: def_clean
	(cd log; make $@)
	(cd ini; make $@)
	(cd nml_intf; make $@)
	(cd rs274ngc; make $@)
	(cd canterp; make $@)
	#(cd hal_intf; make $@)
	(cd kinematics; make $@)
	(cd motion; make $@)
	(cd iotask; make $@)
	(cd task; make $@)
	(cd usr_intf; make $@)

install:
	(cd log; make $@)
	(cd ini; make $@)
	(cd nml_intf; make $@)
	(cd rs274ngc; make $@)
	(cd canterp; make $@)
	#(cd hal_intf; make $@)
	(cd kinematics; make $@)
	(cd motion; make $@)
	(cd iotask; make $@)
	(cd task; make $@)
	(cd usr_intf; make $@)

.PHONY: all clean depend headers indent install
