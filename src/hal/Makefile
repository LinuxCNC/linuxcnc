# Makefile for HAL

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../Makefile.inc

# Set local flags with the GTK flags.
LOCAL_CFLAGS = $(GTK_CFLAGS)

# Public headers
HEADERS = \
hal.h

# Private headers
PRIVHDRS = \
hal_priv.h

SRCS = \
hal_lib.c

RT_SRCS = \
hal_lib.c

LIBS = \
$(RTLIB_DIR)/hal_lib.o \
$(LIB_DIR)/hal_lib.o

# Generic rules
include ../../Make.rules

all : TARGET_DIRS headers $(LIBS)
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)


headers: def_headers
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)

indent: def_indent
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)

depend: def_depend
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)

clean :  def_clean
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)

.PHONY: all headers depend indent install clean

