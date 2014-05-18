#
#  Protobuf support for machinekit
#
#  generate Python modules, and C++ bindings from .proto files

# see http://www.cmcrossroads.com/ask-mr-make/6535-tracing-rule-execution-in-gnu-make
# to trace make execution of make in more detail:
#     make VV=1
ifeq ("$(origin VV)", "command line")
    OLD_SHELL := $(SHELL)
    SHELL = $(warning Building $@$(if $<, (from $<))$(if $?, ($? newer)))$(OLD_SHELL)
endif

ifeq ("$(origin V)", "command line")
  BUILD_VERBOSE = $(V)
endif
ifndef BUILD_VERBOSE
  BUILD_VERBOSE = 0
endif
ifeq ($(BUILD_VERBOSE),1)
  Q =
else
  Q = @
endif
ECHO := @echo

# all protobuf definitions live here
PROTODIR := proto

# generated C++ headers + source files
CXXGEN   := generated

# generated Python files
PYGEN    := python

# the set of all proto specs generated files depend on
PROTO_SPECS := $(wildcard $(PROTODIR)/*.proto)

# the protobuf compiler
PROTOC := $(shell which protoc)

# the directory where descriptor.proto lives:
GPBINCLUDE :=  $(shell pkg-config --variable=includedir protobuf)
DESCDIR    :=  $(GPBINCLUDE)/google/protobuf

# object files generated during dependency resolving
OBJDIR := objects

# search path for .proto files
# see note on PBDEP_OPT below
vpath %.proto  $(PROTODIR):$(GPBINCLUDE):$(DESCDIR)/compiler

# machinetalk/proto/*.proto derived Python bindings
PROTO_PY_TARGETS += $(subst $(PROTODIR)/, \
	$(PYGEN)/, \
	$(patsubst %.proto, %_pb2.py, $(PROTO_SPECS)))

# generated C++ includes
PROTO_CXX_INCS := $(subst $(PROTODIR)/, \
	$(CXXGEN)/,  \
	$(patsubst %.proto, %.pb.h, $(PROTO_SPECS)))

# generated C++ sources
PROTO_CXX_SRCS  :=  $(subst $(PROTODIR)/, \
	$(CXXGEN)/, \
	$(patsubst %.proto, %.pb.cc, $(PROTO_SPECS)))


# ---- generate dependcy files for .proto files
#
# the list of .d dep files for .proto files:
PROTO_DEPS :=  $(patsubst %,$(OBJDIR)/%,$(patsubst %.proto,%.d,$(PROTO_SPECS)))
#
# options to the dependency generator protoc plugin
PBDEP_OPT :=
#PBDEP_OPT += --debug
PBDEP_OPT += --cgen=$(CXXGEN)
PBDEP_OPT += --pygen=$(PYGEN)
# this path must match the vpath arrangement exactly or the deps will be wrong
# unfortunately there is no way to extract the proto path in the code
# generator plugin
PBDEP_OPT += --vpath=$(PROTODIR)
PBDEP_OPT += --vpath=$(GPBINCLUDE)
PBDEP_OPT += --vpath=$(DESCDIR)/compiler

$(OBJDIR)/$(PROTODIR)/%.d: $(PROTODIR)/%.proto
	$(ECHO) "protoc create dependencies for $<"
	@mkdir -p $(OBJDIR)/$(PROTODIR)
	$(Q)$(PROTOC) \
	--plugin=protoc-gen-depends=scripts/protoc-gen-depends \
	--proto_path=$(PROTODIR)/ \
	--proto_path=$(GPBINCLUDE)/ \
	--depends_out="$(PBDEP_OPT)":$(OBJDIR)/$(PROTODIR)/ \
	 $<

#---------- C++ rules -----------
#
# generate .cc/.h from proto files
# for command.proto, generated files are: command.pb.cc	command.pb.h
$(CXXGEN)/%.pb.cc $(CXXGEN)/%.pb.h: %.proto
	$(ECHO) "protoc create $@ from $<"
	@mkdir -p $(CXXGEN)
	$(Q)$(PROTOC) $(PROTOCXX_FLAGS) \
	--proto_path=$(PROTODIR)/ \
	--proto_path=$(GPBINCLUDE)/ \
	--cpp_out=$(CXXGEN)/ \
	$<

# ------------- Python rules ------------
#
# this is for the stock protobuf Python bindings -
# adapt here if using one of the accelerated methods
#
# generate Python modules from proto files
$(PYGEN)/%_pb2.py: %.proto
	$(ECHO) "protoc create $@ from $<"
	@mkdir -p $(PYGEN)
	$(Q)$(PROTOC) $(PROTOC_FLAGS) \
	--proto_path=$(PROTODIR)/ \
	--proto_path=$(GPBINCLUDE)/ \
	--python_out=$(PYGEN)/ \
	$<

GENERATED += $(PROTO_PY_TARGETS) \
	$(PROTO_CXX_SRCS)\
	$(PROTO_CXX_INCS)

# force create of %.proto-dependent files and their deps
Makefile: $(GENERATED) $(PROTO_DEPS)
-include $(PROTO_DEPS)

ios_replace: 
	sh scripts/ios-replace.sh $(CXXGEN)

all:  $(PROTO_DEPS) $(GENERATED)

clean:
	rm -rf $(OBJDIR) $(CXXGEN) $(PYGEN)
