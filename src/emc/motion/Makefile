# Makefile for compiling EMC motion code

include ../../../Makefile.inc

SRCS = \
	emcmotglb.c \
	emcmotutil.c \
	usrmotintf.c

HEADERS =  \
	emcmotcfg.h \
	emcmotglb.h \
	emcmot.h \
	usrmotintf.h

BINS =

OBJS =  \
	emcmotglb.o \
	emcmotutil.o \
	usrmotintf.o


all: headers $(OBJS:%.o=$(TMP_DIR)/%.o) $(BINS:%=$(BIN_DIR)/%)

# Generic rules.
include ../../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers
