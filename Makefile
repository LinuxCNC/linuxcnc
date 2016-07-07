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

DESTDIR := /usr/local
# all protobuf definitions live here
NAMESPACEDIR := machinetalk/protobuf
SRCDIR := src
SRCDIRINV := $(shell realpath --relative-to=$(SRCDIR) .)
PROTODIR := $(SRCDIR)/$(NAMESPACEDIR)

BUILDDIR := build

# generated C++ headers + source files
CXXGEN   := $(BUILDDIR)/cpp

# generated Python files
PYGEN    := $(BUILDDIR)/python

# generated Documentation files
# default to asciidoc template
# for mk-docs formatting, pass in TEMPLATE pointing to the mk-docs template
TEMPLATE := $(SRCDIRINV)/scripts/asciidoc.mustache
DOCFORMAT := asciidoc
DOCEXT := asciidoc
#DOCFORMAT := markdown
#DOCEXT := md

DOCGEN := $(BUILDDIR)/doc

# pkg-config
PKG_CONFIG := $(shell which pkg-config)

# the set of all proto specs generated files depend on
PROTO_SPECS := $(wildcard $(PROTODIR)/*.proto)

# the protobuf compiler
PROTOC := $(shell which protoc)

# the directory where descriptor.proto lives:
GPBINCLUDE :=  $(shell $(PKG_CONFIG) --variable=includedir protobuf)
DESCDIR    :=  $(GPBINCLUDE)/google/protobuf

# object files generated during dependency resolving
OBJDIR := $(BUILDDIR)/objects

# search path for .proto files
# see note on PBDEP_OPT below
vpath %.proto  $(PROTODIR):$(GPBINCLUDE):$(DESCDIR)/compiler

# machinetalk/proto/*.proto derived Python bindings
PROTO_PY_TARGETS := ${PROTO_SPECS:$(SRCDIR)/%.proto=$(PYGEN)/%_pb2.py}
PROTO_PY_EXTRAS := $(PYGEN)/setup.py $(PYGEN)/machinetalk/__init__.py $(PYGEN)/machinetalk/protobuf/__init__.py

# generated C++ includes
PROTO_CXX_INCS := ${PROTO_SPECS:$(SRCDIR)/%.proto=$(CXXGEN)/%.pb.h}

# generated C++ sources
PROTO_CXX_SRCS  :=  ${PROTO_SPECS:$(SRCDIR)/%.proto=$(CXXGEN)/%.pb.cc}

# generated doc file
DOC_TARGET := $(DOCGEN)/machinetalk-protobuf.$(DOCEXT)

# ---- generate dependcy files for .proto files
#
# the list of .d dep files for .proto files:
PROTO_DEPS :=  ${PROTO_SPECS:$(SRCDIR)/%.proto=$(OBJDIR)/%.d}

#
# options to the dependency generator protoc plugin
PBDEP_OPT :=
#PBDEP_OPT += --debug
PBDEP_OPT += --cgen=$(CXXGEN)
PBDEP_OPT += --pygen=$(PYGEN)
# this path must match the vpath arrangement exactly or the deps will be wrong
# unfortunately there is no way to extract the proto path in the code
# generator plugin
PBDEP_OPT += --vpath=$(SRCDIR)
PBDEP_OPT += --vpath=$(GPBINCLUDE)
PBDEP_OPT += --vpath=$(DESCDIR)/compiler


GENERATED += \
	$(PROTO_CXX_SRCS)\
	$(PROTO_CXX_INCS) \
	$(PROTO_PY_TARGETS) \
	$(PROTO_PY_EXTRAS)

$(OBJDIR)/%.d: $(SRCDIR)/%.proto
	$(ECHO) "protoc create dependencies for $<"
	@mkdir -p $(OBJDIR)/
	$(Q)$(PROTOC) \
		--plugin=protoc-gen-depends=scripts/protoc-gen-depends \
		--proto_path=$(SRCDIR)/ \
		--proto_path=$(GPBINCLUDE)/ \
		--depends_out="$(PBDEP_OPT)":$(OBJDIR)/ \
		 $<

#---------- C++ rules -----------
#
# generate .cc/.h from proto files
# for command.proto, generated files are: command.pb.cc	command.pb.h
$(CXXGEN)/%.pb.cc $(CXXGEN)/%.pb.h: $(SRCDIR)/%.proto
	$(ECHO) "protoc create $@ from $<"
	@mkdir -p $(CXXGEN)
	$(Q)$(PROTOC) $(PROTOCXX_FLAGS) \
	--proto_path=$(SRCDIR)/ \
	--proto_path=$(GPBINCLUDE)/ \
	--cpp_out=$(CXXGEN) \
	$<

# ------------- Python rules ------------
#
# this is for the stock protobuf Python bindings -
# adapt here if using one of the accelerated methods
#
# generate Python modules from proto files
$(PYGEN)/%_pb2.py: $(SRCDIR)/%.proto
	$(ECHO) "protoc create $@ from $<"
	@mkdir -p $(PYGEN)
	$(Q)$(PROTOC) $(PROTOC_FLAGS) \
		--proto_path=$(SRCDIR)/ \
		--proto_path=$(GPBINCLUDE)/ \
		--python_out=$(PYGEN)/ \
		$<

$(PYGEN)/%.py: python/%.py
	cp "$<" "$@"

# force create of %.proto-dependent files and their deps
Makefile: $(GENERATED) $(PROTO_DEPS)
-include $(PROTO_DEPS)

# ------------- protoc-gen-doc rules ------------
#
# see https://github.com/estan/protoc-gen-doc
#
# generate $(DOCFORMAT) files from proto files
$(DOC_TARGET): $(wildcard $(SRCDIR)/*.proto) $(TEMPLATE) Makefile
#doc_base:
	$(ECHO) "protoc create $@ from *.proto"
	@mkdir -p $(DOCGEN)
	$(Q)cd $(SRCDIR); \
	$(PROTOC) $(PROTOC_FLAGS) \
	--proto_path=./ \
	--proto_path=$(GPBINCLUDE)/ \
	--doc_out=$(TEMPLATE),$(SRCDIRINV)/$@:./ \
	$(NAMESPACEDIR)/*.proto

all: $(GENERATED) $(PROTO_DEPS)

ios_replace:
	sh scripts/ios-replace.sh $(CXXGEN)

docs: $(PROTO_DEPS) $(DOC_TARGET)

clean:
	rm -rf build

install_proto: $(PROTO_SPECS)
	mkdir -p $(DESTDIR)/include/$(NAMESPACEDIR)
	for proto in $(PROTO_SPECS); do \
		install -m 0644 $$proto $(DESTDIR)/include/$(NAMESPACEDIR); \
	done

install_cpp: $(PROTO_CXX_INCS)
	mkdir -p $(DESTDIR)/include/$(NAMESPACEDIR)
	for headerfile in $(PROTO_CXX_INCS); do \
		install -m 0644 $$headerfile $(DESTDIR)/include/$(NAMESPACEDIR); \
	done

install: install_proto install_cpp
