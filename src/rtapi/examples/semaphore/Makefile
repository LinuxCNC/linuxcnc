# Makefile for semaphore example

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../../Makefile.inc

# Public headers
HEADERS =

# Private headers
PRIVHDRS = common.h

# Sources
SRCS =

ifneq ($(MODULE_EXT),.ko)

RT_SRCS = \
master.c \
slave.c

# Libraries to be created
LIBS = \
$(RTLIB_DIR)/master.o \
$(RTLIB_DIR)/slave.o

endif

# Binaries to be created
BINS =


all : headers $(LIBS) $(BINS)

# Generic rules
include $(SRC_DIR)/Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean kclean

kclean:
	- rm -f .*.cmd *.o *.mod.* *.ko
	- rm -fR .tmp_versions

.PHONY: all headers depend indent install clean kclean
