# Makefile for src/emc directory
# This makefile calls makes in subdirectories and includes the target info to
# create the library from the object files.

# include Makefile.lib
# make rules.

all: headers

all headers kclean clean depend indent install:
	(cd rs274ngc; make $@)
	(cd nml_intf; make $@)
	(cd drivers; make $@)
	(cd drivers/ppmc; make $@)
	(cd drivers/simio; make $@)
	(cd drivers/vital; make $@)
	(cd motion; make $@)
	(cd iotask; make $@)
	(cd task; make $@)

.PHONY: all clean depend headers indent install kclean
