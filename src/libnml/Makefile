# Makefile for rcslib/src directory
# This makefile calls makes in subdirectories and includes the target info to
# create the library from the object files.

# include Makefile.lib
# make rules.
include ../../Makefile.inc

all: headers

all headers clean indent install:
	(cd linklist; make $@)
	(cd inifile; make $@)

.PHONY: all clean depend headers indent install
