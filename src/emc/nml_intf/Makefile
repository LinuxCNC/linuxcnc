# Makefile for compiling EMC NML code

include ../../Makefile.inc
HEADERS = emc.hh \
	emccfg.h \
	emcglb.h \
	emcpos.h \
	getinput.h \
	iniaxis.hh \
	initraj.hh \
	initool.hh \
	inispin.hh \
	inicool.hh \
	inilube.hh \
	iniaux.hh \
	interpl.hh

SRCS =  emc.cc \
	emcglb.c \
	emcargs.cc \
	getinput.c \
	iniaxis.cc \
	initraj.cc \
	initool.cc \
	inispin.cc \
	inicool.cc \
	inilube.cc \
	iniaux.cc \
	interpl.cc \
	emcops.cc \

OBJS =  emc.o \
	emcglb.o \
	getinput.o \
	emcargs.o \
	iniaxis.o \
	initraj.o \
	initool.o \
	inispin.o \
	inicool.o \
	inilube.o \
	iniaux.o \
	interpl.o \
	emcops.o \

RT_OBJS =

BINS =

LIBS =	libemc.a


all: headers $(OBJS:%.o=$(TMP_DIR)/%.o) $(RT_OBJS:%.o=$(RTTMP_DIR)/%.o) \
	$(BINS:%=$(BIN_DIR)/%) $(RTLIBS:%.o=$(RTLIB_DIR)/%.o) $(LIB_DIR)/libemc.a

# Generic rules.
include ../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean
	rm -f $(LIB_DIR)/libemc.a

.PHONY: all clean depend headers indent install kclean

$(LIB_DIR)/libemc.a:	\
	$(TMP_DIR)/emc.o \
	$(TMP_DIR)/emcglb.o \
	$(TMP_DIR)/emcargs.o \
	$(TMP_DIR)/getinput.o \
	$(TMP_DIR)/iniaxis.o \
	$(TMP_DIR)/initraj.o \
	$(TMP_DIR)/initool.o \
	$(TMP_DIR)/inispin.o \
	$(TMP_DIR)/inicool.o \
	$(TMP_DIR)/inilube.o \
	$(TMP_DIR)/iniaux.o \
	$(TMP_DIR)/interpl.o \
	$(TMP_DIR)/emcops.o
	rm -f $@
	ar rc $@ $^
	ranlib $@
