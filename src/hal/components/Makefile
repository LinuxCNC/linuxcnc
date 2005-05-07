# Makefile for HAL Components

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../Makefile.inc

# Public headers
HEADERS =

# Private headers
PRIVHDRS =

ifeq ($(KERNELRELEASE),2.4)

RT_SRCS = \
blocks.c \
debounce.c \
encoder.c \
encoder_ratio.c \
stepgen.c \
freqgen.c \
siggen.c \
pid.c \
supply.c

LIBS = \
$(RTLIB_DIR)/blocks.o \
$(RTLIB_DIR)/debounce.o \
$(RTLIB_DIR)/encoder.o \
$(RTLIB_DIR)/encoder_ratio.o \
$(RTLIB_DIR)/stepgen.o \
$(RTLIB_DIR)/freqgen.o \
$(RTLIB_DIR)/siggen.o \
$(RTLIB_DIR)/pid.o \
$(RTLIB_DIR)/supply.o

endif

all : headers $(LIBS)

# Generic rules
include $(SRC_DIR)/Make.rules

ifeq ($(KERNELRELEASE),2.4)

# specific rule to link siggen with libm
$(RTLIB_DIR)/siggen.o: $(RTTMP_DIR)/siggen.o
	@ echo "Linking kernel module $@"
	ld -r -static -S -O2 $^ $(MATHLIB) -o $@

endif

headers: def_headers
indent: def_indent
depend: def_depend
clean :  def_clean

.PHONY: all headers depend indent install clean

