# Makefile for compiling EMC motion code

include ../../../Makefile.inc

SRCS = \
	command.c \
	control.c \
	emcmotglb.c \
	emcmotutil.c \
	emcpid.c \
	motion.c \
	usrmotintf.c

HEADERS = \
	emcmotcfg.h \
	emcmotglb.h \
	emcpid.h \
	motion.h \
	usrmotintf.h

PRIVHDRS = \
	mot_priv.h

OBJS = \
	emcmotglb.o \
	emcmotutil.o \
	usrmotintf.o

RT_OBJS  = \
	command.o \
	control.o \
	cubic.o \
	emcmotutil.o \
	emcpid.o \
	mmxavg.o \
	motion.o \
	tc.o \
	tp.o

RTLIBS = \
	motmod.o

BINS =

all: headers $(OBJS:%.o=$(TMP_DIR)/%.o) $(RT_OBJS:%.o=$(RTTMP_DIR)/%.o) \
	$(BINS:%=$(BIN_DIR)/%) $(RTLIBS:%.o=$(RTLIB_DIR)/%.o)

# Generic rules.
include ../../../Make.rules

# Explicit rules.

# Side note: MATHLIB defines the linking and lib options for
# libm. Linking to the glibc math lib may lead to unresolved
# symbols without an additional stub module. RTAI provides a
# kernel module, so RTLinux builds are the only ones at risk.
#
# An alternative would be to use ulibc's libm - Often packaged as
# part of dietlibc. 
$(RTLIB_DIR)/motmod.o: \
	$(RTTMP_DIR)/cubic.o \
	$(RTTMP_DIR)/motion.o \
	$(RTTMP_DIR)/command.o \
	$(RTTMP_DIR)/control.o \
	$(RTTMP_DIR)/exthalmot.o \
	$(RTTMP_DIR)/emcmotglb.o \
	$(RTTMP_DIR)/emcmotlog.o \
	$(RTTMP_DIR)/emcmotutil.o \
	$(RTTMP_DIR)/emcpid.o \
	$(RTTMP_DIR)/mmxavg.o \
	$(RTTMP_DIR)/tc.o \
	$(RTTMP_DIR)/tp.o \
	$(RTTMP_DIR)/trivkins.o
	ld -r -static -S -O2 $^ $(RTLIB_DIR)/libpm.a $(MATHLIB) -o $@

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers
