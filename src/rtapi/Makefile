# Makefile for real-time and user-level API

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../Makefile.inc

# Public headers
HEADERS = \
rtapi.h \
rtapi_app.h

# Private headers
PRIVHDRS = \
rtapi_common.h \
procfs_macros.h \
rtapi_proc.h \
vsnprintf.h

# Sources
SRCS = \
$(RTPREFIX)_ulapi.c

RT_SRCS = \
$(RTPREFIX)_rtapi.c

# Libraries to be created
LIBS = \
$(RTLIB_DIR)/rtapi.o \
$(LIB_DIR)/ulapi.o

# Object files to be created
OBJS = \
$(TMP_DIR)/$(RTPREFIX)_ulapi.o \
$(RTTMP_DIR)/$(RTPREFIX)_rtapi.o

all: headers $(OBJS) $(LIBS)

# Generic rules
include ../../Make.rules

headers: def_headers
indent: def_indent
depend: def_depend
clean: def_clean

# explicit rules to make libs from objs

$(RTLIB_DIR)/rtapi.o : $(RTTMP_DIR)/$(RTPREFIX)_rtapi.o
	@ echo "Install rtapi.o in rtlib"
	- \install -p -m0664 $< $(RTLIB_DIR)/rtapi.o

$(LIB_DIR)/ulapi.o : $(TMP_DIR)/$(RTPREFIX)_ulapi.o
	@ echo "Install ulapi.o in lib"
	- \install -p -m0664 $< $(LIB_DIR)/ulapi.o


.PHONY: all headers depend indent install clean

