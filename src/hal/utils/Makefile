# Makefile for HAL Utilities

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../../Makefile.inc

# Set local flags with the GTK flags.
LOCAL_CFLAGS = $(GTK_CFLAGS)

# Public headers
HEADERS =

# Private headers
PRIVHDRS = \
miscgtk.h \
scope_shm.h \
scope_rt.h \
scope_usr.h

SRCS = \
halcmd.c \
miscgtk.c \
meter.c \
scope.c \
scope_horiz.c \
scope_vert.c \
scope_trig.c \
scope_disp.c

RT_SRCS = \
scope_rt.c

LIBS = \
$(RTLIB_DIR)/scope_rt.o

ifdef GTK_VERSION
GTK_BINS = \
$(BIN_DIR)/halmeter \
$(BIN_DIR)/halscope
endif

BINS = \
$(BIN_DIR)/halcmd \
$(GTK_BINS)

all : headers $(LIBS) $(BINS)

# Generic rules
include $(EMC2_HOME)/Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean :  def_clean

# rule to link halcmd
$(BIN_DIR)/halcmd : $(TMP_DIR)/halcmd.o $(LIB_DIR)/ulapi.o \
		$(LIB_DIR)/hal_lib.o $(LIB_DIR)/hal_cmds.o
	@ echo "Linking $@"
	$(CC) $(CFLAGS) $(ULFLAGS) $^ -o $@

# rule to link halmeter

HALMETER_OBJS = \
$(TMP_DIR)/meter.o \
$(TMP_DIR)/miscgtk.o \
$(LIB_DIR)/ulapi.o \
$(LIB_DIR)/hal_lib.o

$(BIN_DIR)/halmeter : $(HALMETER_OBJS)
	@ echo "Linking $@"
	$(CC) $(GTK_LIBS) $(CFLAGS) $(ULFLAGS) $^ -o $@

# rule to link halscope

HALSCOPE_OBJS = \
$(TMP_DIR)/scope.o \
$(TMP_DIR)/scope_horiz.o \
$(TMP_DIR)/scope_vert.o \
$(TMP_DIR)/scope_trig.o \
$(TMP_DIR)/scope_disp.o \
$(TMP_DIR)/miscgtk.o \
$(LIB_DIR)/ulapi.o \
$(LIB_DIR)/hal_lib.o

$(BIN_DIR)/halscope : $(HALSCOPE_OBJS)
	@ echo "Linking $@"
	$(CC) $(GTK_LIBS) $(CFLAGS) $(ULFLAGS) $^ -o $@

.PHONY: all headers depend indent install clean

