# Makefile for compiling EMC IO controller

# get all the platform and application definitions
include ../../../Makefile.inc

SRCS =	simIoControl.cc

HEADERS =

BINS =  simio

OBJS = simIoControl.o

all: headers $(OBJS:%.o=$(TMP_DIR)/%.o) $(BINS:%=$(BIN_DIR)/%)

# Generic rules.
include ../../../Make.rules


$(BIN_DIR)/simio: $(TMP_DIR)/simIoControl.o \
	$(LIB_DIR)/libemc.a \
	$(LIB_DIR)/libnml.a \
	$(LIB_DIR)/ulapi.o
	@ echo "Linking $@"
	$(CXX) $^ $(CFLAGS) $(ULFLAGS) -o $@

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean
	rm -f $(BINS:%=$(BIN_DIR)/%)


.PHONY: all clean install indent depend headers
