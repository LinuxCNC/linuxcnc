# Makefile for compiling EMC NML interface code

include ../../../Makefile.inc

SRCS =	\
	emc.cc \
	emcargs.cc \
	emcglb.c \
	emcops.cc \
	inerpl.cc

HEADERS = \
	emc.hh \
	emccfg.h \
	emcglb.h \
	emcpos.h \
	interpl.hh

BINS =

OBJS = 	\
	emc.o \
	emcargs.o \
	emcglb.o \
	emcops.o \
	interpl.o

LIBS =	libemc.a

all: headers $(OBJS:%.o=$(TMP_DIR)/%.o) $(LIBS:%=$(LIB_DIR)/%)

# Generic rules.
include ../../../Make.rules

libemc.a : $(LIB_DIR)/libemc.a

$(LIB_DIR)/libemc.a:	\
	$(TMP_DIR)/emc.o \
	$(TMP_DIR)/emcargs.o \
	$(TMP_DIR)/emcops.o \
	$(TMP_DIR)/emcglb.o \
	$(TMP_DIR)/emcmotlog.o \
	$(TMP_DIR)/iniaux.o \
	$(TMP_DIR)/iniaxis.o \
	$(TMP_DIR)/inicool.o \
	$(TMP_DIR)/inilube.o \
	$(TMP_DIR)/inispin.o \
	$(TMP_DIR)/initool.o \
	$(TMP_DIR)/initraj.o \
	$(TMP_DIR)/interpl.o
	$(AR) rc $@ $^
	ranlib $@

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers
