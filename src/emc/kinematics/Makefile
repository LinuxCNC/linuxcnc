# Makefile for compiling EMC kinematics code

include ../../Makefile.inc

ifneq ($(MODULE_EXT),.ko)

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

clean: def_clean kclean

kclean:
	- rm -f .*.cmd *.o *.mod.* *.ko
	- rm -fR .tmp_versions 

.PHONY: all clean kclean install indent depend headers
