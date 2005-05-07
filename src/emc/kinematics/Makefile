# Makefile for compiling EMC kinematics code

include ../../Makefile.inc

ifeq ($(KERNELRELEASE),2.4)

RT_SRCS = \
	cubic.c \
	mmxavg.c \
	tc.c \
	tp.c \
	trivkins.c

OBJS =

RT_OBJS  = \
	cubic.o \
	mmxavg.o \
	tc.o \
	tp.o \
	trivkins.o

endif

HEADERS = \
	cubic.h \
	kinematics.h \
	mmxavg.h \
	tc.h \
	tp.h

all: headers $(OBJS:%.o=$(TMP_DIR)/%.o) $(RT_OBJS:%.o=$(RTTMP_DIR)/%.o)

# Generic rules.
include ../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers
