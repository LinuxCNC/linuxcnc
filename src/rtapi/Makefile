# Makefile for real-time and user-level API

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../Makefile.inc

# Public headers (will be copied to INC_DIR)
HEADERS = \
rtapi.h \
rtapi_app.h \
rtapi_common.h \
procfs_macros.h \
rtapi_proc.h \
vsnprintf.h

# Non-Realtime Sources - will be compiled to '*.o'
SRCS = $(RTPREFIX)_ulapi.c

# Realtime Sources - will be compiled to '*.o'
RT_SRCS = $(RTPREFIX)_rtapi.c

# Libraries to be created
LIBS = $(RTLIB_DIR)/$(RTPREFIX)_rtapi.o \
$(LIB_DIR)/$(RTPREFIX)_ulapi.o

# list of all the non-realtime '*.o' files
OBJS = $(RTPREFIX)_ulapi.o

# list of all the realtime '*.o' files
RT_OBJS = $(RTPREFIX)_rtapi.o


all: headers $(LIBS)

# Generic rules
include ../../Make.rules

# explicit rules to make libraries and binaries

$(RTLIB_DIR)/$(RTPREFIX)_rtapi.o : $(RTTMP_DIR)/$(RTPREFIX)_rtapi.o
	- \install -p -m0664 $< $(RTLIB_DIR)/rtapi.o

$(LIB_DIR)/$(RTPREFIX)_ulapi.o : $(TMP_DIR)/$(RTPREFIX)_ulapi.o
	- \install -p -m0664 $< $(LIB_DIR)/ulapi.o

clean: clean_up
	- (rm -f $(LIB_DIR)/ulapi.o)
	- (rm -f $(RTLIB_DIR)/rtapi.o)

.PHONY: all clean depend headers indent install

