# Makefile for rcslib/src directory
# This makefile calls makes in subdirectories and includes the target info to
# create the library from the object files.

# include Makefile.lib
# make rules.

all: headers

SUBDIRS= rs274ngc log ini nml_intf kinematics motion iotask task usr_intf

all headers clean depend indent install:

	@@for subdir in $(SUBDIRS); \
	do \
		echo "Making $@ in $$subdir"; \
		make -C $$subdir $@ ; \
	done

.PHONY: all clean depend headers indent install
