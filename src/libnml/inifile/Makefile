# Makefile for the Operating System interface  modules of the
# RCS Library

# LIB_OBJS
LIB_OBJS =
include Makefile.lib
# make definitions.
include ../../../Makefile.inc

# Declare which files are to be released
HEADERS = \
	inifile.hh \

SRCS =	\
	_inifile.c \
	inifile.cc \
        inivar.c

OBJS := $(LIB_OBJS)

LIBS =

all: headers $(OBJS:%.o=$(TMP_DIR)/%.o) $(TMP_DIR)/inivar.o

# Generic rules.
include ../../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers
