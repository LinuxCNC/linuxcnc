# Makefile for compiling the interface between HAL and emc

include ../../Makefile.inc

HEADERS = \
	extintf.h

SRCS = \
	exthalmot.c \

BINS =

RT_OBJS = \
	exthalmot.o \

all: headers $(RT_OBJS:%.o=$(RTTMP_DIR)/%.o)

# Generic rules.
include ../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers
