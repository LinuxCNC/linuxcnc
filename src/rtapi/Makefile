# Makefile for real-time and user-level API

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../Makefile.inc

# Public headers
HEADERS = \
procfs_macros.h \
rtapi.h \
rtapi_app.h

# Private headers
PRIVHDRS = \
rtapi_common.h \
rtapi_proc.h \
vsnprintf.h

# Sources
SRCS = \
$(RTPREFIX)_ulapi.c

RT_SRCS = \
$(RTPREFIX)_rtapi.c \
mathstubs.c

# Libraries to be created
LIBS = \
$(RTLIB_DIR)/rtapi.o \
$(LIB_DIR)/ulapi.o \
$(RTTMP_DIR)/mathstubs.o

all: headers $(LIBS)
	@ echo "Set up RTOS symlinks"
	@ if [ -f $(EMC2_HOME)/scripts/load_$(RTPREFIX) ] ; \
	then \
	  ln -fs $(EMC2_HOME)/scripts/load_$(RTPREFIX) \
	    $(EMC2_HOME)/scripts/load_rtos ; \
	else \
	  echo "ERROR: $(EMC2_HOME)/scripts/load_$(RTPREFIX) not found" ; \
	fi
	@ if [ -f $(EMC2_HOME)/scripts/unload_$(RTPREFIX) ] ; \
	then \
	  ln -fs $(EMC2_HOME)/scripts/unload_$(RTPREFIX) \
	    $(EMC2_HOME)/scripts/unload_rtos ; \
	else \
	  echo "ERROR: $(EMC2_HOME)/scripts/unload_$(RTPREFIX) not found" ; \
	fi

# Generic rules
include ../../Make.rules

examples:
	(cd examples; make all)

headers: def_headers
	(cd examples; make $@)

indent: def_indent
	(cd examples; make $@)

depend: def_depend
	(cd examples; make $@)

clean: def_clean
	@ echo "Remove RTOS symlinks"
	- rm $(EMC2_HOME)/scripts/load_realtime_base 2> /dev/null
	- rm $(EMC2_HOME)/scripts/unload_realtime_base 2> /dev/null
	# note that we always clean the examples, even if not
	# configured to make them
	(cd examples; make $@)

# explicit rules to make libs from objs

$(RTLIB_DIR)/rtapi.o : $(RTTMP_DIR)/$(RTPREFIX)_rtapi.o
	@ echo "Install rtapi.o in rtlib"
	- \install -p -m0664 $< $(RTLIB_DIR)/rtapi.o


$(LIB_DIR)/ulapi.o : $(TMP_DIR)/$(RTPREFIX)_ulapi.o
	@ echo "Install ulapi.o in lib"
	- \install -p -m0664 $< $(LIB_DIR)/ulapi.o

.PHONY: all examples headers depend indent install clean

