# Makefile for HAL Drivers

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../../Makefile.inc

# Public headers
HEADERS =

# Private headers
PRIVHDRS =

SRCS = \
parport.c

LIBS =

OBJS = \
$(RTLIB_DIR)/parport.o

all : headers $(OBJS) $(LIBS)

# Generic rules
include $(EMC2_HOME)/Make.rules

headers: def_headers
indent: def_indent
depend: def_depend
clean :  def_clean


.PHONY: all headers depend indent install clean

