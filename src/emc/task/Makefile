# Makefile for compiling EMC task code

# get all the platform and application definitions
include ../../../Makefile.inc

SRCS =	emccanon.cc \
	emcsh.cc \
	emcsvr.cc \
	emctask.cc \
	emctaskmain.o \
	iosh.cc \
	minimilltaskintf.cc

HEADERS =

BINS =  emcsh \
	emcsvr \
	iosh \
	minimilltask

OBJS = emccanon.o \
	emcsh.o \
	emcsvr.o \
	emctask.o \
	emctaskmain.o \
	iosh.o \
	minimilltaskintf.o

all: headers $(OBJS:%.o=$(TMP_DIR)/%.o) $(BINS:%=$(BIN_DIR)/%)
# $(OBJS:%.o=$(TMP_DIR)/%.o)

# Generic rules.
include ../../../Make.rules

# Tcl emcsh
$(TMP_DIR)/emcsh.o: emcsh.cc
	$(CXX) $^ -c $(ULFLAGS)  $(TCL_CFLAGS) -o $@

$(TMP_DIR)/iosh.o: iosh.cc
	$(CXX) $^ -c $(ULFLAGS)  $(TCL_CFLAGS) -o $@

$(BIN_DIR)/emcsh: $(TMP_DIR)/emcsh.o \
	$(LIB_DIR)/libemc.a \
	$(LIB_DIR)/libnml.a
	$(CXX) $^ $(ULFLAGS)  $(TCL_CFLAGS) $(TCL_LIBS) -o $@

$(BIN_DIR)/iosh: $(TMP_DIR)/iosh.o \
	$(TMP_DIR)/usrmotintf.o \
	$(TMP_DIR)/emcmotglb.o \
	$(TMP_DIR)/emcmotutil.o \
	$(LIB_DIR)/libemc.a \
	$(LIB_DIR)/libnml.a \
	$(LIB_DIR)/ulapi.o
	$(CXX) $^ $(ULFLAGS)  $(TCL_CFLAGS) $(TCL_LIBS) -o $@

$(BIN_DIR)/emcsvr: $(TMP_DIR)/emcsvr.o \
	$(LIB_DIR)/libemc.a \
	$(LIB_DIR)/libnml.a
	$(CXX) $^ $(ULFLAGS) -o $@

$(BIN_DIR)/minimilltask:	\
	$(TMP_DIR)/emcmotglb.o \
	$(TMP_DIR)/emctask.o \
	$(TMP_DIR)/emccanon.o \
	$(TMP_DIR)/emctaskmain.o \
	$(TMP_DIR)/rs274.o \
	$(TMP_DIR)/usrmotintf.o \
	$(TMP_DIR)/emcmotlog.o \
	$(TMP_DIR)/emcmotutil.o \
	$(TMP_DIR)/minimilltaskintf.o \
	$(LIB_DIR)/libemc.a \
	$(LIB_DIR)/libnmld.a \
	$(LIB_DIR)/ulapi.o
	$(CXX) $^ $(CXXFLAGS) -o $@

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean

.PHONY: all clean install indent depend headers
