# Makefile for the CMS (Communication Management System) modules of the
# RCS Library

# LIB_OBJS
LIB_OBJS =
include Makefile.lib
# make rules.
include ../../../Makefile.inc

SRCS =	\
	locmem.cc \
	memsem.cc \
	phantom.cc \
	physmem.cc \
	shmem.cc \

HEADERS = \
	locmem.hh \
	memsem.hh \
	phantom.hh \
	physmem.hh \
	rem_msg.hh \
	shmem.hh \


OBJS := $(CMS_OBJS)

all: $(HEADERS) $(OBJS:%.o=$(TMP_DIR)/%.o)

# Generic rules.
include ../../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers
