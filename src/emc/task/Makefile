# Makefile for compiling EMC task code

# get all the platform and application definitions
include ../../Makefile.inc

SRCS =	emccanon.cc \
	emcsvr.cc \
	emctask.cc \
	emctaskmain.cc \
	iotaskintf.cc \
	taskintf.cc \

HEADERS =

BINS =  \
	emcsvr \
	milltask \
	milltaskcanon

OBJS = emccanon.o \
	emcsvr.o \
	emctask.o \
	emctaskmain.o \
	iotaskintf.o \
	taskintf.o

all: headers $(OBJS:%.o=$(TMP_DIR)/%.o) $(BINS:%=$(BIN_DIR)/%)
# $(OBJS:%.o=$(TMP_DIR)/%.o)

# Generic rules.
include ../../Make.rules

$(BIN_DIR)/emcsvr: $(TMP_DIR)/emcsvr.o \
	$(LIB_DIR)/libemc.a \
	$(LIB_DIR)/libnml.a
	@ echo "Linking $@"
	$(CXX) $^ $(ULFLAGS) -o $@

$(BIN_DIR)/milltask: \
	$(TMP_DIR)/emcmotglb.o \
	$(TMP_DIR)/emctask.o \
	$(TMP_DIR)/emccanon.o \
	$(TMP_DIR)/emctaskmain.o \
	$(LIB_DIR)/librs274.a \
	$(TMP_DIR)/usrmotintf.o \
	$(TMP_DIR)/emcmotutil.o \
	$(TMP_DIR)/taskintf.o \
	$(TMP_DIR)/iotaskintf.o \
	$(LIB_DIR)/libemc.a \
	$(LIB_DIR)/libnmld.a \
	$(LIB_DIR)/ulapi.o
	@ echo "Linking $@"
	$(CXX) $^ $(CXXFLAGS) -o $@

$(BIN_DIR)/milltaskcanon: \
	$(TMP_DIR)/emcmotglb.o \
	$(TMP_DIR)/canterp.o \
	$(TMP_DIR)/emccanon.o \
	$(TMP_DIR)/emctaskmain.o \
	$(TMP_DIR)/usrmotintf.o \
	$(TMP_DIR)/emcmotutil.o \
	$(TMP_DIR)/taskintf.o \
	$(TMP_DIR)/iotaskintf.o \
	$(LIB_DIR)/libemc.a \
	$(LIB_DIR)/libnmld.a \
	$(LIB_DIR)/ulapi.o
	@ echo "Linking $@"
	$(CXX) $^ $(CXXFLAGS) -o $@

headers: def_headers

indent: def_indent

depend: def_depend

clean: def_clean
	rm -f $(BINS:%=$(BIN_DIR)/%)


.PHONY: all clean install indent depend headers
