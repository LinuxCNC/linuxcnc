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
LOCAL_CXXFLAGS = -Wno-unused -DLATHE

$(TMP_DIR)/rs274.o: $(TMP_DIR)/rs274ngc_pre.o
	cp $< $@

headers: def_headers

# We don't use indent on these sources as it messes up the formatting
# of some of the arrays - It makes them harder to read !
# indent: def_indent

depend: def_depend

clean: def_clean
	rm -f $(TMP_DIR)/rs274.o

.PHONY: all clean install indent depend headers
