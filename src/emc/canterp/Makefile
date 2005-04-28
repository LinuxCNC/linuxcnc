# Makefile for compiling the canonical file interpreter

include ../../Makefile.inc

SRCS = canterp.cc

HEADERS = 

BINS =

OBJS = canterp.o

all: $(TMP_DIR)/canterp.o

include ../../Make.rules
LOCAL_CXXFLAGS = -Wno-unused

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers
