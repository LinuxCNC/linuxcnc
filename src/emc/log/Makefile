# Makefile for compiling EMC logging code

include ../../Makefile.inc

SRCS = emcmotlog.c

HEADERS = emcmotlog.h

BINS =

OBJS =  emcmotlog.o

all: headers $(OBJS:%.o=$(TMP_DIR)/%.o)

# Generic rules.
include ../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean kclean

kclean:
	- rm -f .*.cmd *.o *.mod.* *.ko
	- rm -fR .tmp_versions 

.PHONY: all clean kclean install indent depend headers
