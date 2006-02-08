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
	$(MAKE) -C linklist $@
	$(MAKE) -C rcs $@
	$(MAKE) -C inifile $@
	$(MAKE) -C posemath $@
	$(MAKE) -C os_intf $@
	$(MAKE) -C buffer $@
	$(MAKE) -C cms $@
	$(MAKE) -C nml $@
	($(MAKE) -f Makefile.lib $@)

.PHONY: all clean depend headers indent install
