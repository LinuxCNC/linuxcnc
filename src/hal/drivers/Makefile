# Makefile for HAL Drivers

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../../Makefile.inc

# Public headers
HEADERS =

# Private headers
PRIVHDRS =

SRCS = \
parport.c

RT_SRCS = \
parport.c

LIBS = \
$(RTLIB_DIR)/parport.o

BINS = \
$(BIN_DIR)/parport


all : headers $(OBJS) $(LIBS) $(BINS)

# Generic rules
include $(EMC2_HOME)/Make.rules

headers: def_headers
indent: def_indent
depend: def_depend
clean :  def_clean

# rule to link single module binaries (user space versions of drivers)
$(BIN_DIR)/% : $(TMP_DIR)/%.o $(LIB_DIR)/ulapi.o $(LIB_DIR)/hal_lib.o
	@ echo "Linking user space driver $@"
	$(CC) $(CFLAGS) $(ULFLAGS) $^ -o $@


.PHONY: all headers depend indent install clean

