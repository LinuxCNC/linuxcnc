# Makefile for rcslib/src directory
# This makefile calls makes in subdirectories and includes the target info to
# create the library from the object files.

# include Makefile.lib
# make rules.
include ../Makefile.inc

all: headers

all headers clean depend indent install:
	@ (if [ ! -d "$(LIB_DIR)" ] ; then echo "Create directory '$(LIB_DIR)'" ; mkdir -p $(LIB_DIR) ; fi)
	@ (if [ ! -d "$(BIN_DIR)" ] ; then echo "Create directory '$(BIN_DIR)'" ; mkdir -p $(BIN_DIR) ; fi)
#	@ (if [ ! -d "$(RTLIB_DIR)" ] ; then echo "Create directory '$(RTLIB_DIR)'" ; mkdir -p $(RTLIB_DIR) ; fi)
	(cd linklist; make $@)
	(cd rcs; make $@)
	(cd inifile; make $@)
	(cd posemath; make $@)
	(cd os_intf; make $@)
	(cd buffer; make $@)
	(cd cms; make $@)
	(cd nml; make $@)
	(make -f Makefile.lib $@)

.PHONY: all clean depend headers indent install
