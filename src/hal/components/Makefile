# Makefile for HAL Components

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../../Makefile.inc

# Public headers
HEADERS =

# Private headers
PRIVHDRS =

SRCS = \
encoder.c \
stepgen.c \
pid.c

LIBS =

OBJS = \
$(RTLIB_DIR)/encoder.o \
$(RTLIB_DIR)/stepgen.o \
$(RTLIB_DIR)/pid.o

all : headers $(OBJS) $(LIBS)

# Generic rules
include $(EMC2_HOME)/Make.rules

headers: def_headers
indent: def_indent
clean :  def_clean

.PHONY: all headers depend indent install clean

