# Makefile for HAL

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../Makefile.inc

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

ifneq ($(MODULE_EXT),.ko)

RT_SRCS = \
hal_lib.c

LIBS = \
$(RTLIB_DIR)/hal_lib.o 

endif

LIBS += \
$(LIB_DIR)/hal_lib.o

all : headers $(LIBS)
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)

# Generic rules
include $(SRC_DIR)/Make.rules

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

