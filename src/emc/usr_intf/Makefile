# Makefile for compiling EMC user interface code

# get all the platform and application definitions
include ../../Makefile.inc

SRCS =	\
	emcsh.cc \
	iosh.cc \
	usrmot.c

HEADERS =

BINS =  emcsh \
	iosh \
	usrmot \

OBJS = \
	emcsh.o \
	iosh.o \
	usrmot.o

all: headers $(OBJS:%.o=$(TMP_DIR)/%.o) $(BINS:%=$(BIN_DIR)/%)
# $(OBJS:%.o=$(TMP_DIR)/%.o)

# Generic rules.
include ../../Make.rules

# Tcl emcsh
$(TMP_DIR)/emcsh.o: emcsh.cc
	@ echo "Compile emcsh.cc to tmp dir using TCL flags"
	$(CXX) emcsh.cc -c $(ULFLAGS) $(TCL_CFLAGS) -o $@

$(TMP_DIR)/iosh.o: iosh.cc
	@ echo "Compile iosh.cc to tmp dir using TCL flags"
	$(CXX) iosh.cc -c $(ULFLAGS)  $(TCL_CFLAGS) -o $@

$(BIN_DIR)/emcsh: $(TMP_DIR)/emcsh.o \
	$(LIB_DIR)/libemc.a \
	$(LIB_DIR)/libnml.a
	@ echo "Linking $@"
	$(CXX) $^ $(ULFLAGS)  $(TCL_CFLAGS) $(TCL_LIBS) -o $@

$(BIN_DIR)/iosh: $(TMP_DIR)/iosh.o \
	$(TMP_DIR)/usrmotintf.o \
	$(TMP_DIR)/emcmotglb.o \
	$(TMP_DIR)/emcmotutil.o \
	$(LIB_DIR)/libemc.a \
	$(LIB_DIR)/libnml.a \
	$(LIB_DIR)/ulapi.o
	@ echo "Linking $@"
	$(CXX) $^ $(ULFLAGS)  $(TCL_CFLAGS) $(TCL_LIBS) -o $@

$(BIN_DIR)/usrmot : \
	$(TMP_DIR)/usrmotintf.o \
	$(TMP_DIR)/usrmot.o \
	$(TMP_DIR)/emcmotglb.o \
	$(TMP_DIR)/emcmotlog.o \
	$(TMP_DIR)/emcmotutil.o \
	$(LIB_DIR)/ulapi.o \
	$(LIB_DIR)/libnml.a
	@ echo "Linking $@"
	$(CXX) $(CFLAGS) $(ULFLAGS) $^ -o $@

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean
	rm -f $(BINS:%=$(BIN_DIR)/%)


.PHONY: all clean install indent depend headers
