
LIB_OBJS =
include Makefile.lib

include ../../Makefile.inc

# Declare which files are to be released
HEADERS = \
	inifile.h \
	inifile.hh

SRCS =	\
	_inifile.c \
	inifile.cc \
        inivar.cc

OBJS := $(LIB_OBJS) 

LIBS =

LOCAL_CFLAGS = -fPIC
LOCAL_CXXFLAGS = -fPIC

all: headers $(SRCS:%.cc=$(TMP_DIR)/%.o) $(SRCS:%.c=$(TMP_DIR)/%.o)

# Generic rules.
include ../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers
