
LIB_OBJS =
include Makefile.lib

include ../../../Makefile.inc

# Declare which files are to be released
HEADERS = \
	inifile.hh

SRCS =	\
	inifile.cc \
        inivar.cc

OBJS := $(LIB_OBJS) \
	inivar.o

LIBS =

all: headers $(OBJS:%.o=$(TMP_DIR)/%.o) $(BIN_DIR)/inivar

# Generic rules.
include ../../../Make.rules

$(BIN_DIR)/inivar: $(TMP_DIR)/inivar.o $(TMP_DIR)/inivar.o $(TMP_DIR)/inifile.o
	$(CXX) $^ -o $@

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers
