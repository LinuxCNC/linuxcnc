# Makefile for compiling ini code

# get all the platform and application definitions
include ../../../Makefile.inc

SRCS = \
	iniaux.cc \
	iniaxis.cc \
	inicool.cc \
	inilube.cc \
	inispin.cc \
	initool.cc \
	initraj.cc

HEADERS =  \
	iniaux.hh \
	iniaxis.hh \
	inicool.hh \
	inilube.hh \
	inispin.hh \
	initool.hh \
	initraj.hh \


BINS =

OBJS =  \
	iniaux.o \
	iniaxis.o \
	inicool.o \
	inilube.o \
	inispin.o \
	initool.o \
	initraj.o


all: headers $(OBJS:%.o=$(TMP_DIR)/%.o) $(BINS:%=$(BIN_DIR)/%)

# Generic rules.
include ../../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers
