# Makefile for HAL Drivers
#
# NOTE: To avoid possible confusion or contention with
# normal Linux drivers, all HAL drivers should begin with
# the string "hal_"
#
# The default rules in this file and Make.rules will properly
# build an driver that consists of a single source file, linked
# to the RTAPU/ULAPI and HAL libraries only.  All you need to
# do to add a new driver is to add <name>.c to SRCS and
# $(BIN_DIR)/<name> to BINS (for a user space driver), and/or
# add <name>.c to RT_SCRS and $(RTLIB_DIR)/<name>.o to LIBS (for
# a realtime driver).
#
# FIXME - can we change the default rules so they properly link
# modules that require the math library, but do _not_ link the
# math lib if it isn't needed?  If not, then modules that require
# the math lib will also require explicit rules ;-(
#

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../../Makefile.inc

# Public headers
HEADERS =

# Private headers
PRIVHDRS =

SRCS = \
hal_parport.c \
hal_skeleton.c

RT_SRCS = \
hal_parport.c \
hal_tiro.c \
hal_skeleton.c

LIBS = \
$(RTLIB_DIR)/hal_parport.o \
$(RTLIB_DIR)/hal_tiro.o \
$(RTLIB_DIR)/hal_skeleton.o

BINS = \
$(BIN_DIR)/hal_parport \
$(BIN_DIR)/hal_skeleton


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

