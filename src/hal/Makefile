# Makefile for HAL

# get the defs for CC, CFLAGS, LIB_DIR, etc.
include ../../Makefile.inc

# Set local flags with the GTK flags.
LOCAL_CFLAGS = $(GTK_CFLAGS)

# Public headers
HEADERS = \
hal.h

# Private headers
PRIVHDRS = \
hal_priv.h \
miscgtk.h \
halsc_shm.h \
halsc_rt.h \
halsc_usr.h

SRCS = \
hal_lib.c \
halcmd.c \
miscgtk.c \
halmeter.c \
halscope.c \
halsc_horiz.c \

#halsc_vert.c # FIX ME This file needs committing.

RT_SRCS = \
hal_lib.c \
halscope_rt.c

LIBS = \
$(RTLIB_DIR)/hal_lib.o \
$(LIB_DIR)/hal_lib.o \
$(RTLIB_DIR)/halscope_rt.o

ifdef GTK_VERSION
GTK_BINS = \
$(BIN_DIR)/halmeter
#$(BIN_DIR)/halscope
endif

BINS = \
$(BIN_DIR)/halcmd \
$(GTK_BINS)

all : headers $(LIBS) $(BINS)
	(cd drivers; make $@)
	(cd components; make $@)

# Generic rules
include $(EMC2_HOME)/Make.rules

headers: def_headers
	(cd drivers; make $@)
	(cd components; make $@)

indent: def_indent
	(cd drivers; make $@)
	(cd components; make $@)

depend: def_depend
	(cd drivers; make $@)
	(cd components; make $@)

clean :  def_clean
	(cd drivers; make $@)
	(cd components; make $@)

# rule to link halcmd
$(BIN_DIR)/halcmd : $(TMP_DIR)/halcmd.o $(LIB_DIR)/ulapi.o $(LIB_DIR)/hal_lib.o
	@ echo "Linking $@"
	$(CC) $(CFLAGS) $(ULFLAGS) $^ -o $@

# rule to link halmeter

HALMETER_OBJS = \
$(TMP_DIR)/halmeter.o \
$(TMP_DIR)/miscgtk.o \
$(LIB_DIR)/ulapi.o \
$(LIB_DIR)/hal_lib.o

$(BIN_DIR)/halmeter : $(HALMETER_OBJS)
	@ echo "Linking $@"
	$(CC) $(GTK_LIBS) $(CFLAGS) $(ULFLAGS) $^ -o $@

# rule to link halscope

HALSCOPE_OBJS = \
$(TMP_DIR)/halscope.o \
$(TMP_DIR)/halsc_horiz.o \
$(TMP_DIR)/halsc_vert.o \
$(TMP_DIR)/miscgtk.o \
$(LIB_DIR)/ulapi.o \
$(LIB_DIR)/hal_lib.o

$(BIN_DIR)/halscope : $(HALSCOPE_OBJS)
	@ echo "Linking $@"
	$(CC) $(GTK_LIBS) $(CFLAGS) $(ULFLAGS) $^ -o $@

.PHONY: all headers depend indent install clean

