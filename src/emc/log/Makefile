# Makefile for compiling EMC logging code

include ../../Makefile.inc

SRCS = emcmotlog.c

HEADERS = emcmotlog.h

BINS =

OBJS =  emcmotlog.o

all: headers $(OBJS:%.o=$(TMP_DIR)/%.o) $(OBJS:%.o=$(RTTMP_DIR)/%.o)

# Generic rules.
include ../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers
