#
# Makefile for the RCS C++ Storage Class Utilities
#

# LIB_OBJS
LIB_OBJS =
include Makefile.lib
# make rules.
include ../../../Makefile.inc

# Declare which files are to be released
HEADERS = \
	rcs.hh \
	rcsversion.h \
	rcs_exit.hh \
	rcs_print.hh

SRCS =	rcs_print.cc
	

OBJS := $(LIB_OBJS)

LIBS =

all: headers $(OBJS:%.o=$(TMP_DIR)/%.o)

# Generic rules.
include ../../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers
