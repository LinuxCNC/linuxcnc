# Makefile for timer example

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../../../Makefile.inc

# Public headers
HEADERS =

# Private headers
PRIVHDRS =

# Sources
SRCS =

RT_SRCS = \
timertask.c

# Libraries to be created
LIBS = \
$(RTLIB_DIR)/timertask.o

# Binaries to be created
BINS =


all : headers $(LIBS) $(BINS)

# Generic rules
include ../../../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all headers depend indent install clean

