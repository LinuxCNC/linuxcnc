# Makefile for compiling EMC motion code

include ../../../Makefile.inc

SRCS = \
	emcmotglb.c \
	emcmotutil.c \
	usrmotintf.c \
	emcpid.c \
	mmxavg.c \
	cubic.c \
	tc.c \
	tp.c

RT_SRCS = \
	emcmot.c

HEADERS = \
	emcmotcfg.h \
	emcmotglb.h \
	usrmotintf.h \
	emcpid.h \
	mmxavg.h \
	cubic.h \
	tc.h \
	tp.h \
	emcmot.h

BINS =

OBJS = \
	emcmotglb.o \
	emcmotutil.o \
	usrmotintf.o \
	emcpid.o \
	mmxavg.o \
	cubic.o \
	tc.o \
	tp.o

RT_OBJS  = \
	emcmot.o

LIBS = \

all: headers $(OBJS:%.o=$(TMP_DIR)/%.o) $(RT_OBJS:%.o=$(RTTMP_DIR)/%.o) $(BINS:%=$(BIN_DIR)/%) $(LIBS)

# Generic rules.
include ../../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers
