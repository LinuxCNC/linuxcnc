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
hal_priv.h

SRCS = \
hal_lib.c \
hal_cmds.c \
hal_procfs.c \
avltree.c

RT_SRCS = \
hal_lib.c \
avltree.c

LIBS = \
$(RTLIB_DIR)/hal_lib.o \
$(LIB_DIR)/hal_lib.o \
$(RTLIB_DIR)/avltree.o \
$(LIB_DIR)/avltree.o \
$(LIB_DIR)/hal_cmds.o \
$(LIB_DIR)/hal_procfs.o


all : headers $(LIBS)
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)

# Generic rules
include $(EMC2_HOME)/Make.rules

headers: def_headers
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)

indent: def_indent
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)

depend: def_depend
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)

clean :  def_clean
	(cd utils; make $@)
	(cd drivers; make $@)
	(cd components; make $@)

# rule to make avltree (test program)
avltest : avltree.c avltree.h
	@ echo "Compiling avltree test program to $(BIN_DIR)/avltree"
	$(CC) $(CFLAGS) $(ULFLAGS) -DBUILD_TEST avltree.c -o $(BIN_DIR)/avltree



.PHONY: all headers depend indent install clean avltest

