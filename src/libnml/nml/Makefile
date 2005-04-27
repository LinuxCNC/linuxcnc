
LIB_OBJS =
include Makefile.lib
# make rules.
include ../../Makefile.inc

# Declare which files are to be released
HEADERS = cmd_msg.hh \
	nml.hh \
	nml_oi.hh \
	nml_srv.hh \
	nmldiag.hh \
	nmlmsg.hh \
	stat_msg.hh \

SRCS =	cmd_msg.cc \
	nml.cc \
	nml_oi.cc \
	nml_srv.cc \
	nmldiag.cc \
	nmlmsg.cc \
	stat_msg.cc \

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
