# Makefile for HAL Drivers

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../../Makefile.inc

# Public headers
HEADERS =

# Private headers
PRIVHDRS =

RT_SRCS = \
parport.c

LIBS = \
$(RTLIB_DIR)/parport.o

all : headers $(LIBS)

# Generic rules
include $(EMC2_HOME)/Make.rules

headers: def_headers
indent: def_indent
depend: def_depend
clean :  def_clean


.PHONY: all headers depend indent install clean

