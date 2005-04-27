
LIB_OBJS =
include Makefile.lib
# make definitions.
include ../../Makefile.inc

# Declare which files are to be released
HEADERS = \
	_sem.h \
	_shm.h \
	_timer.h \
	sem.hh \
	shm.hh \
	timer.hh


SRCS =	\
	_sem.c \
	_shm.c \
	_timer.c \
	sem.cc \
	shm.cc \
	timer.cc \


OBJS := $(LIB_OBJS)

LIBS =

all: headers $(OBJS:%.o=$(TMP_DIR)/%.o)

# Generic rules.
include ../../Make.rules

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers
