
LIB_OBJS =
include Makefile.lib
# make rules.
include ../../Makefile.inc

# Declare which files are to be released
SRCS =	nml.cc \
	nmlmsg.cc \
	stat_msg.cc

HEADERS = nml.hh \
	nmlmsg.hh \
	stat_msg.hh

OBJS := $(LIB_OBJS)

LIBS =

all: headers $(OBJS:%.o=$(TMP_DIR)/%.o)

# Generic rules.
include ../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers
