# This Makefile uses five compiler flags.
# -DNO_AA means the interpreter should exclude the A axis.
# -DNO_BB means the interpreter should exclude the B axis.
# -DNO_CC means the interpreter should exclude the C axis.
# -DAXIS_ERROR means reading NC code referencing an axis the interpreter
#    cannot handle should cause the interpreter to signal an error.
#    Without this flag, if the interpreter cannot handle an axis, any
#    syntactially valid NC code word starting with the axis letter is
#    read and ignored.
# -DALL_AXES means the interpreter should print canonical commands
#    that include all six axes, regardless of whether the interpreter
#    can handle those axes. Values put into the canonical command calls
#    for non-handled axes are always zero.

# This Makefile includes code for making six executables out of a
# possible 29. The others can be made similarly.

# Makefile for compiling RS-274-NGC interpreter code for the EMC
# set the name of the module directory (this directory's name)
MODULE = rs274ngc_new

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
	rs274ngc_errors.cc \
	rs274ngc_pre.cc

HEADERS = canon.hh \
	interp_internal.hh \
	rs274ngc.hh \
	rs274ngc_return.hh

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
	rs274ngc_errors.o \
	rs274ngc_pre.o

all: headers $(SRCS:%.cc=$(TMP_DIR)/%.o) $(TMP_DIR)/rs274.o

include ../../Make.rules

# this set of flags is for normal mill interp
LOCAL_CXXFLAGS = -Wno-unused -DDEBUG_EMC
# this set of flags is for lathe interp
#LOCAL_CXXFLAGS = -Wno-unused -DLATHE

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

.PHONY: all clean install indent depend headers kclean
