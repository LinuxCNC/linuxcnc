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

# FMP took out SUB_DIR since who wants those rs274ngc_new subdirs in
# plat/XXX/include,src,lib anyway?
# SUB_DIR=rs274ngc_new

include ../../Makefile.inc

HEADERS = canon.hh \
	rs274ngc.hh \
	rs274ngc_return.hh

SRCS = driver.cc \
	rs274ngc_errors.c \
	rs274ngc_pre.cc

OBJS = driver.o \
	rs274ngc_errors.o \
	rs274ngc_pre.o

RT_OBJS =

BINS =

LOCAL_CXXFLAGS= -Wno-unused

all: headers $(OBJS:%.o=$(TMP_DIR)/%.o) $(RT_OBJS:%.o=$(RTTMP_DIR)/%.o) \
	$(BINS:%=$(BIN_DIR)/%) $(RTLIBS:%.o=$(RTLIB_DIR)/%.o) \
	$(TMP_DIR)/rs274abc.o

foo:
	echo '$(OBJS)'

# Generic rules.
include ../../Make.rules

$(TMP_DIR)/rs274abc.o:  $(TMP_DIR)/driver.o \
			$(TMP_DIR)/rs274ngc_errors.o \
			$(TMP_DIR)/rs274ngc_pre.o
	rm -f $@
	ar rc $@ $^
	ranlib $@


headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean depend headers indent install kclean
