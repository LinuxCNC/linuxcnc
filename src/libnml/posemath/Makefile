# Makefile for compiling posemath library

# Note: posemath.cc and _posemath.c implement the C++ and C versions of
# the posemath.h definitions, respectively.
# sincos.h and sincos.c are C only.
# mathprnt.cc and _mathprnt.c implement the C++ and C versions of the
# mathprnt.h definitions, respectively.
#
# Debug can be compiled in to the posemath.cc and _posemath.c files.
#
# The resulting .o files are linked into libpm.a (no debug), or libpmdb.a
# (with debug).

# make rules.
include ../../Makefile.inc
# LIB_OBJS
LIB_OBJS =
include Makefile.lib

HEADERS = \
	posemath.h \
	sincos.h

SRCS = \
	_posemath.c \
	sincos.c \
	posemath.cc

LIBS =	$(LIB_OBJS:%.o=$(TMP_DIR)/%.o) \
	$(LIB_DIR)/libpm.a

BINS =

ifeq ($(KERNELRELEASE),2.4)
RT_SRCS = \
	_posemath.c \
	sincos.c

RT_OBJS = _posemath.o \
	sincos.o

LIBS +=	$(RTLIB_DIR)/libpm.a

endif


LOCAL_RTFLAGS = -ffast-math
#LOCAL_CXXFLAGS = -Wno-deprecated

all: headers $(RT_OBJS:%.o=$(RTTMP_DIR)/%.o) $(LIBS)

LOCAL_CFLAGS = -fPIC
LOCAL_CXXFLAGS = -fPIC

# Generic rules.
include ../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

# explicit rules to make libraries and binaries
$(RTLIB_DIR)/libpm.a: $(RTTMP_DIR)/_posemath.o $(RTTMP_DIR)/sincos.o
	$(AR) $(ARFLAGS) $@ $^
	ranlib $@

$(LIB_DIR)/libpm.a: \
	$(TMP_DIR)/_posemath.o $(TMP_DIR)/posemath.o $(TMP_DIR)/sincos.o
	$(AR) $(ARFLAGS) $@ $^
	ranlib $@

.PHONY: all clean install indent depend headers
