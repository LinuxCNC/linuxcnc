#
# Makefile for some basic support functions..
#
# LIB_OBJS
LIB_OBJS =
include Makefile.lib
# make rules.
include ../../../Makefile.inc

# Declare which files are to be released
HEADERS = \
	linklist.hh \
	rcs.hh \
	rcs_prnt.hh

SRCS =	linklist.cc \
	rcs_prnt.cc

OBJS := $(LIB_OBJS)

LIBS =

all: headers $(OBJS:%.o=$(TMP_DIR)/%.o)

# Generic rules.
include ../../../Make.rules

clean: def_clean

indent: def_indent

.PHONY: all clean install indent depend headers
