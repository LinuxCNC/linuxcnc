# Makefile for rcslib/src directory
# This makefile calls makes in subdirectories and includes the target info to
# create the library from the object files.

# include Makefile.lib
# make rules.
include ../../Makefile.inc

SUBDIRS=linklist rcs inifile posemath os_intf buffer cms nml

all: headers

all headers clean depend indent install:
	@@for subdir in $(SUBDIRS); \
	do \
		echo "Making $@ in $$subdir"; \
		make -C $$subdir $@ ; \
	done

	(make -f Makefile.lib $@)

.PHONY: all clean depend headers indent install
