# Makefile for compiling RS-274-NGC interpreter code for the EMC

include ../../../Makefile.inc


SRCS = \
	rs274ngc_pre.cc \

HEADERS = \
	canon.hh \
	rs274ngc.hh \
	rs274ngc_return.hh

BINS =

OBJS = rs274.o \

all: headers $(SRCS:%.cc=$(TMP_DIR)/%.o) $(OBJS:%=$(TMP_DIR)/%)

include ../../../Make.rules
LOCAL_CXXFLAGS = -Wno-unused

$(TMP_DIR)/rs274.o: $(TMP_DIR)/rs274ngc_pre.o
	cp $< $@

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean
	rm -f $(TMP_DIR)/rs274.o

.PHONY: all clean install indent depend headers
