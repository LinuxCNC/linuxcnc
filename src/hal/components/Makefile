# Makefile for HAL Components

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../../Makefile.inc

# Public headers
HEADERS =

# Private headers
PRIVHDRS =

RT_SRCS = \
encoder.c \
stepgen.c \
siggen.c \
pid.c \
supply.c

LIBS = \
$(RTLIB_DIR)/encoder.o \
$(RTLIB_DIR)/stepgen.o \
$(RTLIB_DIR)/siggen.o \
$(RTLIB_DIR)/pid.o \
$(RTLIB_DIR)/supply.o

all : headers $(LIBS)

# Generic rules
include $(EMC2_HOME)/Make.rules

# specific rule to link siggen with libm
$(RTLIB_DIR)/siggen.o: $(RTTMP_DIR)/siggen.o
	@ echo "Linking kernel module $@"
	ld -r -static -S -O2 $^ $(MATHLIB) -o $@

headers: def_headers
indent: def_indent
depend: def_depend
clean :  def_clean

.PHONY: all headers depend indent install clean

