# Makefile for the CMS (Communication Management System) modules of the
# RCS Library

# LIB_OBJS
LIB_OBJS =
include Makefile.lib
# make rules.
include ../../Makefile.inc

SRCS =	\
	memsem.cc \
	physmem.cc \
	shmem.cc \

HEADERS = memsem.hh \
	physmem.hh \
	shmem.hh \


OBJS := $(CMS_OBJS)

all: $(HEADERS) $(OBJS:%.o=$(TMP_DIR)/%.o)

# Generic rules.
include ../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers
