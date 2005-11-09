# Makefile for compiling RS-274-NGC interpreter code for the EMC

include ../../Makefile.inc


SRCS = \
	interp_arc.cc \
	interp_array.cc \
	interp_check.cc \
	interp_convert.cc \
	interp_cycles.cc \
	interp_execute.cc \
	interp_find.cc \
	interp_internal.cc \
	interp_inverse.cc \
	interp_read.cc \
	interp_write.cc \
	interp_o_word.cc \
	rs274ngc_errors.cc \
	rs274ngc_pre.cc

HEADERS = \
	rs274ngc.hh \
	interp_internal.hh

BINS =

OBJS = \
	interp_arc.o \
	interp_array.o \
	interp_check.o \
	interp_convert.o \
	interp_cycles.o \
	interp_execute.o \
	interp_find.o \
	interp_internal.o \
	interp_inverse.o \
	interp_read.o \
	interp_write.o \
	interp_o_word.o \
	rs274ngc_errors.o \
	rs274ngc_pre.o

all: headers $(SRCS:%.cc=$(TMP_DIR)/%.o) $(TMP_DIR)/rs274.o

include ../../Make.rules

# this set of flags is for normal mill interp
LOCAL_CXXFLAGS = -Wno-unused
# this set of flags is for lathe interp
#LOCAL_CXXFLAGS = -Wno-unused -DLATHE
# FIXME - need a way to make both

$(TMP_DIR)/rs274.o: $(OBJS)
	rm -f $@
	$(AR) $(ARFLAGS) $@ $^

headers: def_headers

# We don't use indent on these sources as it messes up the formatting
# of some of the arrays - It makes them harder to read !
# indent: def_indent

depend: def_depend

clean: def_clean
	rm -f $(TMP_DIR)/rs274.o

.PHONY: all clean install indent depend headers
