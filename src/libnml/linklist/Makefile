
# make rules.
include ../../Makefile.inc

# LIB_OBJS
LIB_OBJS =
include Makefile.lib

HEADERS = linklist.hh

SRCS =	linklist.cc

OBJS := $(LIB_OBJS)

LIBS =

BINS= 

all: headers $(OBJS:%.o=$(TMP_DIR)/%.o)

LOCAL_CFLAGS = -fPIC
LOCAL_CXXFLAGS = -fPIC

# Generic rules.
include ../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers
