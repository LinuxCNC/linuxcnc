# Some useful targets:
#
#     default (if no make target is specified): Build the software and
#         the docs (if enabled via configure).
#
#     build-software: Build all the realtime and non-realtime software
#         libraries & executables.
#
#     docs: Build all the documentation that's enabled through configure.
#
#
#     install: Install everything (realtime software, non-realtime
#         software, and docs).
#
#     install-software: Install all architecture-specific things (realtime
#         and non-realtime libraries and executables).
#
#     install-docs: Install all enabled documentation into $(DESTDIR).
#

SHELL:=/bin/bash

default: build-software

install: install-software install-docs

install-software: install-kernel-dep install-kernel-indep install-menu

ifeq ($(origin KERNELRELEASE), undefined)
#MAKEFLAGS += --warn-undefined-variables
endif

EXTRAFLAGS :=
EXTRA_DEBUG ?=

ifndef DESTDIR
DESTDIR :=
endif

# see http://www.cmcrossroads.com/ask-mr-make/6535-tracing-rule-execution-in-gnu-make
# to trace make execution of make in more detail:
# make VV=1
ifeq ("$(origin VV)", "command line")
    OLD_SHELL := $(SHELL)
    SHELL = $(warning Building $@$(if $<, (from $<))$(if $?, ($? newer)))$(OLD_SHELL)
endif

# Delete the default suffix rules
.SUFFIXES:
.PHONY: default build-software userspace modules clean modclean depclean install install-software install-docs python pythonclean cscope cscopeclean

# A "trivial build" is one which should not include dependency information
# either because it should be usable before dependency information can be
# generated or when it is invalid (clean, docclean) or when running as root
# when the user must guarantee in advance that everything is built
# (setuid, install)
ifeq ($(MAKECMDGOALS),)
TRIVIAL_BUILD=no
else
ifeq ($(filter-out docclean clean setuid install tags swish,$(MAKECMDGOALS)),)
TRIVIAL_BUILD=yes
else
TRIVIAL_BUILD=no
endif
endif

# Used to enable x86 only HAL parts
ifeq ($(shell uname -m),i686)
X86ARCH = true
endif
ifeq ($(shell uname -m),x86_64)
X86ARCH = true
endif

# Beautify output
# ---------------------------------------------------------------------------
#
# A simple variant is to prefix commands with $(Q) - that's useful
# for commands that shall be hidden in non-verbose mode.
#
#	$(Q)ln $@ :<
#
# If BUILD_VERBOSE equals 0 then the above command will be hidden.
# If BUILD_VERBOSE equals 1 then the above command is displayed.

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

ifeq "$(findstring s,$(filter-out --%, $(MAKEFLAGS)))" ""
ECHO=@echo
VECHO=echo
else
ECHO=@true
VECHO=true
endif

ifeq ($(BASEPWD),)
BASEPWD := $(shell pwd)
export BASEPWD
include Makefile.inc
ifeq ($(origin PYTHONPATH),undefined)
PYTHONPATH:=$(EMC2_HOME)/lib/python
else
PYTHONPATH:=$(EMC2_HOME)/lib/python:$(PYTHONPATH)
endif
export PYTHONPATH
else
include $(BASEPWD)/Makefile.inc
endif
ifeq ($(RTPREFIX),)
$(error Makefile.inc must specify RTPREFIX and other variables)
endif

ifeq ($(TOOL_NML),yes)
  TOOL_NML_FLAG := -DTOOL_NML
  $(info TOOL_NML_FLAG =$(TOOL_NML_FLAG))
else
endif

cc-option = $(shell if $(CC) $(CFLAGS) $(1) -S -o /dev/null -xc /dev/null \
	     > /dev/null 2>&1; then echo "$(1)"; fi ;)
cxx-option = $(shell if $(CXX) $(CXXFLAGS) $(1) -S -o /dev/null -xc++ /dev/null \
            > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi ;)

ifeq ($(origin KERNELRELEASE),undefined)
# When KERNELRELEASE is not defined, this is the userspace build.
# The "modules" target is the gateway to the kernel module build.
build-software: userspace modules
ifeq ($(RUN_IN_PLACE),yes)
	@if [ -f ../bin/gomc-server ]; then if ! getcap ../bin/gomc-server 2>/dev/null | grep -q cap_sys_nice; then $(VECHO) "You now need to run 'sudo make setuid' in order to run in place with access to hardware."; fi; fi
endif

VERSION=$(shell cat ../VERSION)
dist:
	mkdir ../linuxcnc-$(VERSION)
	(cd ../ && git archive master) | (cd ../linuxcnc-$(VERSION) && tar xf -)
	(cd ../linuxcnc-$(VERSION) && ./debian/configure)
	(cd ../linuxcnc-$(VERSION)/src && ./autogen.sh)
	(cd .. && tar zcf linuxcnc-$(VERSION).tar.gz linuxcnc-$(VERSION))
	rm -rf ../linuxcnc-$(VERSION)

# Print 'entering' all the time
MAKEFLAGS += w

# Create the variables with := so that subsequent += alterations keep it
# as a "substitute at assignment time" variable
TARGETS :=
PYTARGETS :=
GENERATED_MANPAGES :=

# Submakefiles from each of these directories will be included if they exist
SUBDIRS := \
    libnml/inifile libnml/posemath libnml \
    \
    gmi/lib \
    \
    rtapi \
    \
    hal/components hal/drivers hal/drivers/mesa-hostmot2 \
    hal/user_comps/devices hal/user_comps/mb2hal \
    hal/user_comps hal/user_comps/vismach hal/user_comps/vfs11_vfd hal/utils hal \
    hal/user_comps/vfdb_vfd hal/user_comps/wj200_vfd \
    hal/user_comps/pi500_vfd \
    hal/user_comps/huanyang-vfd \
	hal/user_comps/xhc-whb04b-6 \
	hal/hal-ads-server \
	hal/drivers/ethercat \
    \
    emc/usr_intf/axis emc/usr_intf/touchy emc/usr_intf/stepconf emc/usr_intf/pncconf \
    emc/usr_intf/gremlin emc/usr_intf/gscreen emc/usr_intf/pyui emc/usr_intf/qtvcp \
    emc/usr_intf/gmoccapy emc/usr_intf/qtplasmac emc/usr_intf/mdro\
    emc/usr_intf/gmcui \
    emc/usr_intf emc/nml_intf emc/task emc/iotask emc/kinematics emc/tp emc/canterp \
    emc/motion emc/ini emc/rs274ngc emc/sai \
    emc \
    \
    gomc \
    gmi/codegen \
    webapp \
    \
    po \
    \
    ../docs/src ../docs/src/source-highlight \
    \
    ../tests \



ULAPISRCS := rtapi/$(RTPREFIX)_ulapi.c

# Each item in INCLUDES is transformed into a -I directive later on
# The top directory is always included
INCLUDES := .

USERSRCS :=
PROGRAMS :=

# When used like $(call TOxxx, ...) these turn a list of source files
# into the corresponding list of object files, dependency files,
# or both.  When a source file has to be compiled with special flags,
# TOOBJSDEPS is used.  Confusingly, TOOBJSDEPS includes preprocessed source
# file names, but this is what allows 'make src.i' to produce proper
# preprocessed source when src.c needs a customized compile flag.
# See Submakefile.skel for an example.
TOOBJS = $(patsubst %.cc,objects/%.o,$(patsubst %.c,objects/%.o,$(1)))
TODEPS = $(patsubst %.cc,objects/%.d,$(patsubst %.c,objects/%.d,$(1)))
TOOBJSDEPS = $(call TOOBJS,$(1)) $(call TODEPS, $(1)) $(patsubst %.cc,%.ii,$(patsubst %.c,%.i,$(1)))

SUBMAKEFILES := $(patsubst %,%/Submakefile,$(SUBDIRS))
-include $(wildcard $(SUBMAKEFILES))

# This checks that all the things listed in USERSRCS are either C files
# or C++ files
ASSERT_EMPTY = $(if $(1), $(error "Should be empty but is not: $(1)"))
$(call ASSERT_EMPTY,$(filter-out %.c %.cc, $(USERSRCS)))

$(call TOOBJS,$(PYSRCS)) : EXTRAFLAGS += -fPIC -fno-strict-aliasing
USERSRCS += $(PYSRCS)

# Find the list of object files for each type of source file
CUSERSRCS := $(filter %.c,$(USERSRCS))
CXXUSERSRCS := $(filter %.cc,$(USERSRCS))
CUSEROBJS := $(call TOOBJS,$(CUSERSRCS))
CXXUSEROBJS += $(call TOOBJS,$(CXXUSERSRCS))

ifeq ($(TRIVIAL_BUILD),no)
# Find the dependency filenames, then include them all
DEPS := $(sort $(patsubst %.o,%.d,$(CUSEROBJS) $(CXXUSEROBJS)))
READ_DEPS = $(wildcard $(DEPS))
$(shell $(VECHO) 1>&2 Reading $(words $(READ_DEPS))/$(words $(DEPS)) dependency files)
-include $(READ_DEPS)
UNREAD_DEPS = $(filter-out $(READ_DEPS), $(DEPS))
$(shell $(VECHO) 1>&2 Done reading dependencies)
endif

# Each directory in $(INCLUDES) is passed as a -I directory when compiling.
INCLUDE := $(patsubst %,-I%, $(INCLUDES)) -I$(RTDIR)/include
INCLUDE += $(PYTHON_CPPFLAGS)
INCLUDE += $(LIBTIRPC_CFLAGS)

# Compilation options.	Perhaps some of these should come from Makefile.inc? (CXXFLAGS now does)
INTEGER_OVERFLOW_FLAGS := -fwrapv
OPT := -Os $(INTEGER_OVERFLOW_FLAGS)
DEBUG := $(DEBUG) -g -Wall -D_FORTIFY_SOURCE=2
CFLAGS := $(INCLUDE) $(OPT) $(DEBUG) $(EXTRA_DEBUG) -D_GNU_SOURCE -std=gnu11 -Werror=implicit-function-declaration $(CFLAGS) $(CPPFLAGS)
CXXFLAGS := $(INCLUDE) $(EXTRA_DEBUG) $(DEBUG) $(OPT) -Werror=overloaded-virtual $(CXXFLAGS) $(CPPFLAGS)
CXXFLAGS += -std=gnu++17
# In Debian 11, any inclusion of <boost/python.hpp> leads to several
# diagnostics from included headers about deprecated features. LinuxCNC does
# not directly use these deprecated features, but it does use boost::python.
# Silence the warnings just when they occur in files using boost/python.hpp by
# adding SILENCE_BOOST_INTERNAL_DIAGNOSTICS_FLAGS to the object's EXTRAFLAGS
SILENCE_BOOST_INTERNAL_DIAGNOSTICS_FLAGS = -DBOOST_ALLOW_DEPRECATED_HEADERS=1 -DBOOST_BIND_GLOBAL_PLACEHOLDERS=1
CFLAGS   += $(TOOL_NML_FLAG)
CXXFLAGS += $(TOOL_NML_FLAG)

ifeq ($(RUN_IN_PLACE),yes)
LDFLAGS := -L$(LIB_DIR) -Wl,-rpath,$(LIB_DIR) $(LIBTIRPC_LIBS) $(LDFLAGS)
else
LDFLAGS := -Wl,-rpath-link,../lib $(LIBTIRPC_LIBS) $(LDFLAGS)
endif

# Rules to make .o (object) files
$(sort $(CUSEROBJS)) : objects/%.o: %.c
	$(ECHO) Compiling $<
	@mkdir -p $(dir $@)
	@rm -f $@
	$(Q)$(CC) -c $(CFLAGS) $(EXTRAFLAGS) \
		-MP -MD -MF "${@:.o=.d}" -MT "$@" \
		$< -o $@

$(sort $(CXXUSEROBJS)) : objects/%.o: %.cc
	$(ECHO) Compiling $<
	@mkdir -p $(dir $@)
	@rm -f $@
	$(Q)$(CXX) -c $(CXXFLAGS) $(EXTRAFLAGS) \
		-MP -MD -MF "${@:.o=.d}" -MT "$@" \
		$< -o $@

# Rules to make .i (preprocessed) files
$(sort $(patsubst %.c,%.i,$(CUSERSRCS))): %.i: %.c
	$(ECHO) Preprocessing $< to $@
	$(Q)$(CC) -dD $(CFLAGS) $(EXTRAFLAGS) -E $< -o $@

$(sort $(patsubst %.cc,%.ii,$(CXXUSERSRCS))): %.ii: %.cc
	$(ECHO) Preprocessing $< to $@
	$(Q)$(CXX) -dD $(CXXFLAGS) $(EXTRAFLAGS) -E $< -o $@

ifeq ($(TRIVIAL_BUILD),no)
configure: configure.ac
	AUTOGEN_TARGET=configure ./autogen.sh

config.h.in: configure.ac
	AUTOGEN_TARGET=config.h.in ./autogen.sh

config.status: configure
	if [ -f config.status ]; then ./config.status --recheck; else \
	    echo 1>&2 "*** linuxcnc is not configured.	Run './configure' with appropriate flags."; \
	    exit 1; \
	fi
endif

Makefile: config.h
config.h: config.h.in config.status
	@./config.status -q --header=$@

INFILES = \
        ../scripts/linuxcnc \
	../scripts/linuxcnc_info ../scripts/setup_designer \
	../scripts/haltcl ../scripts/rtapi.conf Makefile.inc Makefile.modinc \
	../tcl/linuxcnc.tcl ../scripts/rip-environment \
	../scripts/linuxcncmkdesktop \
	../lib/python/nf.py \
	../share/desktop-directories/linuxcnc-doc.directory \
	../share/desktop-directories/linuxcnc-ref.directory \
	../share/desktop-directories/linuxcnc-cnc.directory \
	../share/menus/CNC.menu \
	../share/applications/linuxcnc.desktop \
	../share/applications/linuxcnc-latency.desktop \
	../share/applications/linuxcnc-latency-histogram.desktop \
	../share/applications/linuxcnc-pncconf.desktop \
	../share/applications/linuxcnc-stepconf.desktop \
	../scripts/linuxcnc_var \
	../scripts/runtests \

$(INFILES): %: %.in config.status
	@./config.status --file=$@

build-software: $(INFILES)

# For each file to be copied to ../include, its location in the source tree
# is listed here.  Note that due to $(INCLUDE), defined above, the include
# files in the source tree are the ones used when building linuxcnc.  The copy
# in ../include is used when building external components of linuxcnc.
SRCHEADERS := \
    emc/linuxcnc.h \
    emc/kinematics/cubic.h \
    emc/kinematics/kinematics.h \
    emc/kinematics/genhexkins.h \
    emc/kinematics/genserkins.h \
    emc/kinematics/pentakins.h \
    emc/kinematics/pumakins.h \
    emc/tp/tc.h \
    emc/tp/tc_types.h \
    emc/tp/tcq.h \
    emc/tp/tp_types.h \
    emc/tp/spherical_arc.h \
    emc/tp/blendmath.h \
    emc/motion/emcmotcfg.h \
    emc/motion/motion.h \
    emc/motion/homing.h \
    emc/motion/simple_tp.h \
    emc/motion/state_tag.h \
    emc/motion/usrmotintf.h \
    emc/motion/axis.h \
    emc/nml_intf/canon.hh \
    emc/nml_intf/canon_position.hh \
    emc/nml_intf/emctool.h \
    emc/nml_intf/emccfg.h \
    emc/nml_intf/emcglb.h \
    emc/nml_intf/emcpos.h \
    emc/nml_intf/emcpose.h \
    emc/nml_intf/interp_return.hh \
    emc/nml_intf/motion_types.h \
    emc/nml_intf/debugflags.h \
    emc/rs274ngc/interp_internal.hh \
    emc/rs274ngc/interp_fwd.hh \
    emc/rs274ngc/interp_base.hh \
    emc/rs274ngc/modal_state.hh \
    emc/rs274ngc/rs274ngc.hh \
    emc/rs274ngc/saicanon.hh \
    hal/hal.h \
    hal/drivers/mesa-hostmot2/hostmot2-serial.h \
    libnml/inifile/inifile.h \
    libnml/inifile/inifile.hh \
    libnml/posemath/posemath.h \
    libnml/posemath/gotypes.h \
    libnml/posemath/gomath.h \
    libnml/posemath/sincos.h \
    rtapi/rtapi.h \
    rtapi/rtapi_app.h \
    rtapi/rtapi_atomic.h \
    rtapi/rtapi_bitops.h \
    rtapi/rtapi_bool.h \
    rtapi/rtapi_byteorder.h \
    rtapi/rtapi_firmware.h \
    rtapi/rtapi_io.h \
    rtapi/rtapi_limits.h \
    rtapi/rtapi_list.h \
    rtapi/rtapi_math.h \
    rtapi/rtapi_math64.h \
    rtapi/rtapi_mutex.h \
    rtapi/rtapi_parport.h \
    rtapi/rtapi_pci.h \
    rtapi/rtapi_stdint.h \
    rtapi/rtapi_ctype.h \
    rtapi/rtapi_errno.h \
    rtapi/rtapi_string.h

# the "headers" target installs all the header files in ../include
.PHONY: headers
HEADERS := $(patsubst %,../include/%,$(foreach h,$(SRCHEADERS),$(notdir $h)))
headers: $(HEADERS)

# install header files as part of the build
TARGETS += headers

# Add converting of %.po files
TARGETS += $(patsubst po/%.po, ../share/locale/%/LC_MESSAGES/linuxcnc.mo, $(wildcard po/*.po))
TARGETS += $(patsubst po/gmoccapy/%.po, ../share/locale/%/LC_MESSAGES/gmoccapy.mo, $(wildcard po/gmoccapy/*.po))
TARGETS += $(patsubst po/%.po, objects/%.msg, $(wildcard po/*.po))

# ngcgui as standalone app:
TARGETS += ../bin/ngcgui
../bin/ngcgui: Makefile
	$(Q)rm -f $@
	$(Q)(echo "#!$(TCLSH)")>|$@
	$(Q)(echo "source $(EMC2_TCL_LIB_DIR)/ngcgui.tcl")>>$@
	$(Q)(echo "::ngcgui::standalone_ngcgui")>>$@
	$(Q)chmod +x $@

# pyngcgui as standalone app:
TARGETS += ../bin/pyngcgui
../bin/pyngcgui: Makefile
	$(Q)rm -f $@
	$(Q)(echo "#!$(PYTHON)")>|$@
	$(Q)(echo "import pyngcgui")>>$@
	$(Q)(echo "pyngcgui.standalone_pyngcgui()")>>$@
	$(Q)chmod +x $@

# gremlin_view as standalone app:
TARGETS += ../bin/gremlin_view
../bin/gremlin_view: Makefile
	$(Q)rm -f $@
	$(Q)(echo "#!$(PYTHON)")>|$@
	$(Q)(echo "import gremlin_view")>>$@
	$(Q)(echo "gremlin_view.standalone_gremlin_view()")>>$@
	$(Q)chmod +x $@

# halshow: now a gmcui web app (symlink created in gmcui/Submakefile)

# And make userspace depend on $(TARGETS)
userspace: $(TARGETS)

pythonclean:
	rm -f $(PYTARGETS)
	find ../lib/python -name '*.so' -exec rm {} +
	find . -name __pycache__ | xargs -r rm -r
python: $(PYTARGETS)
userspace: python
inclean:
	$(RM) $(INFILES)
headersclean:
	$(RM) $(HEADERS)
clean: docclean pythonclean cscopeclean headersclean
distclean: clean inclean

# These rules clean things up.
# 'clean' cleans everything but dependency files, and 'depclean' cleans them
# too.
modclean:
	-rm -f ../cmod/*.so

depclean:
	-rm -rf depends

clean: genclean depclean modclean
genclean:
	find . -name '*.o' |xargs rm -f
	-rm -rf objects
	-rm -f $(TARGETS)
	-rm -f $(GENERATED_MANPAGES)
	-rm -f hal/components/conv_*.comp

pycheck-python-files:
	@echo 'Checking *.py files for python3 compatibility...'
	@for d in ./hal; do \
		filelist=$(shell mktemp) && \
		find $$d -name '*.py' -type f > $$filelist && \
		cat $$filelist | xargs /usr/bin/env python3 -m py_compile && \
		rm -f $$filelist; \
	done

pycheck-python-script:
	@echo 'Checking "text/x-python" and "text/x-script.python" (by MIME type) files python3 compatibility...'
	@for d in ../tests; do \
		filelist=$(shell mktemp) && \
		find $$d -type f -exec file -F ':' -i {} \; | grep -e 'text/x-python' -e 'text/x-script.python' | awk -F ':' '{print $$1}' > $$filelist && \
		cat $$filelist | xargs /usr/bin/env python3 -m py_compile && \
		rm -f $$filelist; \
	done

pycheck: pycheck-python-files pycheck-python-script

# So that nothing is built as root, this rule does not depend on the touched
# files (Note that files in depends/ might be rebuilt, and there's little that
# can be done about it)
# Capabilities required by gomc-server:
#   cap_net_admin  - EtherCAT: manage network interfaces and settings
#   cap_net_raw    - EtherCAT: send/receive raw Ethernet frames
#   cap_ipc_lock   - lock shared memory (mlock) for realtime operation
#   cap_sys_resource - raise resource limits (e.g. RLIMIT_MEMLOCK, RLIMIT_RTPRIO)
#   cap_sys_nice   - set realtime scheduling priorities (SCHED_FIFO/SCHED_RR)
#   cap_sys_rawio  - direct hardware I/O port access (ioperm/iopl for parport, PCI BARs)
setuid:
	setcap cap_net_admin,cap_net_raw,cap_ipc_lock,cap_sys_resource,cap_sys_nice,cap_sys_rawio=ep ../bin/gomc-server

# These rules allows a header file from this directory to be installed into
# ../include.  A pair of rules like these will exist in the Submakefile
# of each file that contains headers.
$(patsubst %,../include/%,$(wildcard *.h)): ../include/%.h: %.h
	$(Q)-cp $^ $@
$(patsubst %,../include/%,$(wildcard *.hh)): ../include/%.hh: %.hh
	$(Q)-cp $^ $@

INSTALL=install -o root
DIR=$(INSTALL) -d -m 0755
FILE=$(INSTALL) -m 0644
TREE=cp -dR
CONFIGFILE=$(INSTALL) -m 0644
EXE=$(INSTALL) -m 0755
SETUID=$(INSTALL) -m 4755
GLOB=$(wildcard $(1))

XDGDESKTOP = \
    ../share/applications/linuxcnc.desktop \
    ../share/applications/linuxcnc-latency.desktop \
    ../share/applications/linuxcnc-latency-histogram.desktop \
    ../share/applications/linuxcnc-pncconf.desktop \
    ../share/applications/linuxcnc-stepconf.desktop \

ifeq ($(RUN_IN_PLACE),yes)
define ERROR_MESSAGE
You configured run-in-place, but are trying to install.
For an installable version, run configure without --enable-run-in-place
and rebuild
endef
install install-software install-docs:
	$(error $(ERROR_MESSAGE))

MENUS = ../share/menus/CNC.menu \
    ../share/desktop-directories/cnc.directory
rip-install-menus rip-install-menu: $(MENUS) $(XDGDESKTOP)
	mkdir -p $(HOME)/.config/menus/applications-merged
	cp $< $(HOME)/.config/menus/applications-merged
else

DOCS=README.adoc INSTALL.adoc AUTHORS

# FIXME figure out how to install translated docs in subdirs
DOCS_HELP=$(call GLOB,../docs/help/*.adoc)

NC_FILES=$(filter-out %/butterfly.ngc,$(call GLOB,../nc_files/*))
TCL=$(call GLOB,../tcl/*.tcl)
TCL_BIN=$(call GLOB,../tcl/bin/*.tcl) ../tcl/bin/popimage

install-docs:
	$(DIR) $(DESTDIR)$(docsdir)
	$(FILE) $(addprefix ../docs/,$(DOCS)) $(DESTDIR)$(docsdir)
	$(FILE) $(DOCS_HELP) $(DESTDIR)$(docsdir)
	$(FILE) emc/usr_intf/axis/etc/axis_light_background $(DESTDIR)$(docsdir)
	$(FILE) emc/usr_intf/axis/README $(DESTDIR)$(docsdir)/README.axis

install-dirs:
	$(DIR) $(DESTDIR)$(EMC2_GOMOD_DIR) \
		$(DESTDIR)$(sysconfdir)/linuxcnc $(DESTDIR)$(bindir) $(DESTDIR)$(libdir) \
		$(DESTDIR)/lib/linuxcnc $(DESTDIR)$(includedir)/linuxcnc \
		$(DESTDIR)$(docsdir) $(DESTDIR)$(ncfilesdir) \
		$(DESTDIR)/etc/X11/app-defaults $(DESTDIR)$(tcldir)/bin \
		$(DESTDIR)$(tcldir)/scripts \
		$(DESTDIR)$(mandir)/man1 \
		$(DESTDIR)$(mandir)/man3 \
		$(DESTDIR)$(mandir)/man9 \
		$(DESTDIR)$(tcldir)/msgs \
		$(DESTDIR)$(localedir)/de/LC_MESSAGES \
		$(DESTDIR)$(datadir)/axis/images \
		$(DESTDIR)$(datadir)/axis/tcl \
		$(DESTDIR)$(datadir)/gscreen/images \
		$(DESTDIR)$(datadir)/gscreen/skins \
		$(DESTDIR)$(datadir)/qtvcp/images \
		$(DESTDIR)$(datadir)/qtvcp/screens \
		$(DESTDIR)$(datadir)/qtvcp/panels \
		$(DESTDIR)$(datadir)/qtvcp/widgets_ui \
		$(DESTDIR)$(datadir)/gmoccapy/icons \
		$(DESTDIR)$(datadir)/glade/catalogs \
		$(DESTDIR)$(datadir)/glade/pixmaps/hicolor/22x22/actions \
		$(DESTDIR)$(datadir)/gtksourceview-4/language-specs \
		$(DESTDIR)$(datadir)/linuxcnc/stepconf \
		$(DESTDIR)$(datadir)/linuxcnc/pncconf \
		$(DESTDIR)$(datadir)/linuxcnc/pncconf/pncconf-help \
		$(DESTDIR)$(datadir)/linuxcnc/hallib

install-kernel-indep: install-dirs
	$(FILE) ../docs/man/man1/*.1 $(DESTDIR)$(mandir)/man1
	$(FILE) $(filter-out %/skeleton.3hal, $(wildcard ../docs/man/man3/*.3hal)) $(DESTDIR)$(mandir)/man3
	$(FILE) $(filter-out %/skeleton.3rtapi, $(wildcard ../docs/man/man3/*.3rtapi)) $(DESTDIR)$(mandir)/man3
	$(FILE) $(filter-out %/skeleton.9, $(wildcard ../docs/man/man9/*.9)) $(DESTDIR)$(mandir)/man9
	$(FILE) objects/*.msg $(DESTDIR)$(tcldir)/msgs
	$(EXE) ../scripts/haltcl $(DESTDIR)$(bindir)
	$(EXE) ../scripts/simulate_probe $(DESTDIR)$(bindir)
	$(EXE) ../scripts/sim_pin $(DESTDIR)$(bindir)
	$(FILE) ../*.png ../*.gif $(DESTDIR)$(datadir)/linuxcnc
	$(FILE) ../lib/hallib/*   $(DESTDIR)$(datadir)/linuxcnc/hallib
	$(FILE) ../tcl/*.png $(DESTDIR)$(tcldir)

	# install all the sample configs, including subdirs (tar is required on debian systems, and common on others)
	$(DIR) $(DESTDIR)$(sampleconfsdir)
	((cd ../configs && tar --exclude CVS --exclude .cvsignore --exclude .gitignore -cf - .) | (cd $(DESTDIR)$(sampleconfsdir) && tar -xf -))

	$(EXE) $(filter-out ../bin/test_rtapi_vsnprintf, $(filter ../bin/%,$(TARGETS))) $(DESTDIR)$(bindir)
	$(EXE) ../scripts/linuxcnc $(DESTDIR)$(bindir)
	$(EXE) ../scripts/linuxcnc_info $(DESTDIR)$(bindir)
	$(EXE) ../scripts/setup_designer $(DESTDIR)$(bindir)
	$(EXE) ../scripts/linuxcnc_var $(DESTDIR)$(bindir)
	$(EXE) ../scripts/monitor-xhc-hb04 $(DESTDIR)$(bindir)
	$(EXE) ../scripts/latency-test $(DESTDIR)$(bindir)
	$(EXE) ../scripts/latency-plot $(DESTDIR)$(bindir)
	$(EXE) ../scripts/latency-histogram $(DESTDIR)$(bindir)
	$(EXE) ../scripts/moveoff_gui $(DESTDIR)$(bindir)
	$(EXE) ../scripts/hal-histogram $(DESTDIR)$(bindir)
	$(EXE) ../scripts/xhc-hb04-accels $(DESTDIR)$(bindir)
	$(EXE) ../scripts/pyvcp_demo $(DESTDIR)$(bindir)
	$(EXE) ../scripts/gladevcp_demo $(DESTDIR)$(bindir)
	$(EXE) ../scripts/linuxcncmkdesktop $(DESTDIR)$(bindir)
	$(EXE) ../bin/update_ini $(DESTDIR)$(bindir)
	$(EXE) ../scripts/halreport $(DESTDIR)$(bindir)
	$(FILE) $(filter ../lib/%.a ../lib/%.so.0,$(TARGETS)) $(DESTDIR)$(libdir)
	cp --no-dereference $(wildcard ../lib/*.so) $(DESTDIR)$(libdir)
	-ldconfig $(DESTDIR)$(libdir)
	$(FILE) ../lib/linuxcnc/canterp.so $(DESTDIR)$(libdir)/linuxcnc
	$(FILE) $(HEADERS) $(DESTDIR)$(includedir)/linuxcnc/
	$(TREE) $(NC_FILES) $(DESTDIR)$(ncfilesdir)
	$(EXE) ../nc_files/M101 $(DESTDIR)$(ncfilesdir)
	$(FILE) ../tcl/TkLinuxCNC $(DESTDIR)/etc/X11/app-defaults
	$(FILE) Makefile.modinc $(DESTDIR)$(datadir)/linuxcnc
	$(EXE) $(TCL) $(DESTDIR)$(tcldir)
	$(FILE) ../tcl/linuxcnc.so $(DESTDIR)$(tcldir)
	$(FILE) ../tcl/pkgIndex.tcl $(DESTDIR)$(tcldir)
	$(EXE) $(TCL_BIN) $(DESTDIR)$(tcldir)/bin
	$(FILE) ../tcl/scripts/balloon.tcl ../tcl/scripts/emchelp.tcl $(DESTDIR)$(tcldir)/scripts
	$(EXE) ../tcl/scripts/Set_Coordinates.tcl $(DESTDIR)$(tcldir)/scripts
	$(FILE) ../share/linuxcnc/*.glade $(DESTDIR)$(prefix)/share/linuxcnc
	$(FILE) ../share/linuxcnc/stepconf/*.glade $(DESTDIR)$(prefix)/share/linuxcnc/stepconf
	$(FILE) ../share/linuxcnc/touchy.glade $(DESTDIR)$(prefix)/share/linuxcnc
	$(FILE) ../share/linuxcnc/gscreen.glade $(DESTDIR)$(prefix)/share/linuxcnc
	$(FILE) ../share/linuxcnc/gscreen2.glade $(DESTDIR)$(prefix)/share/linuxcnc
	$(FILE) ../share/gmoccapy/gmoccapy.glade $(DESTDIR)$(prefix)/share/gmoccapy
	$(FILE) ../share/linuxcnc/pncconf/*.glade $(DESTDIR)$(prefix)/share/linuxcnc/pncconf
	$(FILE) ../share/linuxcnc/gremlin_view.ui $(DESTDIR)$(prefix)/share/linuxcnc
	$(FILE) ../share/linuxcnc/popupkeyboard.ui $(DESTDIR)$(prefix)/share/linuxcnc
	$(FILE) ../configs/common/linuxcnc.nml     $(DESTDIR)$(prefix)/share/linuxcnc
	$(FILE) ../configs/common/linuxcnc_big.nml $(DESTDIR)$(prefix)/share/linuxcnc
	$(FILE) ../src/emc/usr_intf/pncconf/pncconf-help/*.txt $(DESTDIR)$(prefix)/share/linuxcnc/pncconf/pncconf-help
	$(FILE) ../src/emc/usr_intf/pncconf/pncconf-help/*.png $(DESTDIR)$(prefix)/share/linuxcnc/pncconf/pncconf-help

	$(FILE) ../lib/python/gladevcp/hal_python.xml $(DESTDIR)$(datadir)/glade/catalogs/
	$(FILE) ../share/glade/pixmaps/hicolor/22x22/actions/widget*.png  $(DESTDIR)$(datadir)/glade/pixmaps/hicolor/22x22/actions/

	$(FILE) ../share/gtksourceview-4/language-specs/*.lang  $(DESTDIR)$(datadir)/gtksourceview-4/language-specs/

	$(FILE) ../src/hal/drivers/mesa-hostmot2/modbus/*.tmpl  $(DESTDIR)$(prefix)/share/linuxcnc/

install-kernel-indep: install-python
install-python: install-dirs
	$(DIR) $(DESTDIR)$(SITEPY) $(DESTDIR)$(SITEPY)/rs274
	$(DIR) $(DESTDIR)$(SITEPY)/touchy
	$(DIR) $(DESTDIR)$(SITEPY)/gscreen
	$(DIR) $(DESTDIR)$(SITEPY)/qtvcp
	$(DIR) $(DESTDIR)$(SITEPY)/qtvcp/designer/
	$(DIR) $(DESTDIR)$(SITEPY)/qtvcp/lib
	$(DIR) $(DESTDIR)$(SITEPY)/qtvcp/lib/writer
	$(DIR) $(DESTDIR)$(SITEPY)/qtvcp/lib/writer/ext
	$(DIR) $(DESTDIR)$(SITEPY)/qtvcp/lib/qtplasmac
	$(DIR) $(DESTDIR)$(SITEPY)/qtvcp/lib/qt_vismach
	$(DIR) $(DESTDIR)$(SITEPY)/qtvcp/lib/qt_ngcgui
	$(DIR) $(DESTDIR)$(SITEPY)/qtvcp/lib/qt_pdf
	$(DIR) $(DESTDIR)$(SITEPY)/qtvcp/lib/gcode_utility
	$(DIR) $(DESTDIR)$(SITEPY)/qtvcp/plugins
	$(DIR) $(DESTDIR)$(SITEPY)/qtvcp/widgets
	$(DIR) $(DESTDIR)$(SITEPY)/qtvcp/widgets/widget_icons
	$(DIR) $(DESTDIR)$(SITEPY)/gmoccapy
	$(DIR) $(DESTDIR)$(SITEPY)/gladevcp
	$(DIR) $(DESTDIR)$(SITEPY)/stepconf
	$(DIR) $(DESTDIR)$(SITEPY)/pncconf
	$(DIR) $(DESTDIR)$(SITEPY)/pyui
	$(DIR) $(DESTDIR)$(SITEPY)/plasmac
	$(DIR) $(DESTDIR)$(SITEPY)/gmi
	$(FILE) ../lib/python/*.py ../lib/python/*.so $(DESTDIR)$(SITEPY)
	$(FILE) ../lib/python/rs274/*.py $(DESTDIR)$(SITEPY)/rs274
	$(FILE) ../lib/python/touchy/*.py $(DESTDIR)$(SITEPY)/touchy
	$(FILE) ../lib/python/gscreen/*.py $(DESTDIR)$(SITEPY)/gscreen
	$(FILE) ../lib/python/qtvcp/*.{py,ui,txt} $(DESTDIR)$(SITEPY)/qtvcp
	$(FILE) ../lib/python/qtvcp/designer/*.txt $(DESTDIR)$(SITEPY)/qtvcp/designer
	$(FILE) ../lib/python/qtvcp/widgets/*.py $(DESTDIR)$(SITEPY)/qtvcp/widgets
	$(FILE) ../lib/python/qtvcp/widgets/widget_icons/*.{png,gif} $(DESTDIR)$(SITEPY)/qtvcp/widgets/widget_icons
	$(FILE) ../lib/python/qtvcp/plugins/*.py $(DESTDIR)$(SITEPY)/qtvcp/plugins
	$(TREE) ../lib/python/qtvcp/lib/* $(DESTDIR)$(SITEPY)/qtvcp/lib
	$(FILE) ../lib/python/gmoccapy/*.py $(DESTDIR)$(SITEPY)/gmoccapy
	$(FILE) ../lib/python/gladevcp/*.{py,png,glade} $(DESTDIR)$(SITEPY)/gladevcp
	$(FILE) ../lib/python/stepconf/*.py $(DESTDIR)$(SITEPY)/stepconf
	$(FILE) ../lib/python/pncconf/*.py $(DESTDIR)$(SITEPY)/pncconf
	$(FILE) ../lib/python/pyui/*.py $(DESTDIR)$(SITEPY)/pyui
	$(FILE) ../lib/python/plasmac/*.py $(DESTDIR)$(SITEPY)/plasmac
	$(FILE) ../lib/python/gmi/*.py $(DESTDIR)$(SITEPY)/gmi
	$(EXE) ../bin/stepconf $(DESTDIR)$(bindir)
	$(EXE) ../bin/pncconf $(DESTDIR)$(bindir)
	$(EXE) ../bin/pyui $(DESTDIR)$(bindir)
	$(EXE) ../bin/pyvcp $(DESTDIR)$(bindir)
	$(EXE) ../bin/gladevcp $(DESTDIR)$(bindir)
	$(EXE) ../bin/axis $(DESTDIR)$(bindir)
	$(EXE) ../bin/axis-remote $(DESTDIR)$(bindir)
	$(EXE) ../bin/debuglevel $(DESTDIR)$(bindir)
	$(EXE) ../bin/linuxcnctop $(DESTDIR)$(bindir)
	$(EXE) ../bin/mdi $(DESTDIR)$(bindir)
	$(EXE) ../bin/manualtoolchange_ui $(DESTDIR)$(bindir)
	$(EXE) ../bin/image-to-gcode $(DESTDIR)$(bindir)
	$(EXE) ../bin/touchy $(DESTDIR)$(bindir)
	$(EXE) ../bin/gscreen $(DESTDIR)$(bindir)
	$(EXE) ../bin/qtvcp $(DESTDIR)$(bindir)
	$(EXE) ../lib/python/qtvcp/designer/install_script $(DESTDIR)$(SITEPY)/qtvcp/designer/
	$(EXE) ../bin/gmoccapy $(DESTDIR)$(bindir)
	$(EXE) ../bin/teach-in $(DESTDIR)$(bindir)
	$(EXE) $(patsubst %.py,../bin/%,$(VISMACH_PY)) $(DESTDIR)$(bindir)
	$(EXE) $(patsubst %.py,../bin/%,$(QTPLASMAC_PY)) $(DESTDIR)$(bindir)
	$(FILE) ../share/linuxcnc/linuxcnc-wizard.gif $(DESTDIR)$(prefix)/share/linuxcnc
	$(FILE) ../share/axis/images/*.{png,gif,xbm,ngc} $(DESTDIR)$(datadir)/axis/images
	$(FILE) ../share/axis/tcl/*.tcl $(DESTDIR)$(datadir)/axis/tcl
	$(FILE) ../share/gscreen/images/*.gif $(DESTDIR)$(datadir)/gscreen/images
	$(TREE) ../share/gscreen/skins/* $(DESTDIR)$(datadir)/gscreen/skins
	$(TREE) ../share/qtvcp/images/* $(DESTDIR)$(datadir)/qtvcp/images
	$(TREE) ../share/qtvcp/screens/* $(DESTDIR)$(datadir)/qtvcp/screens
	$(TREE) ../share/qtvcp/panels/* $(DESTDIR)$(datadir)/qtvcp/panels
	$(TREE) ../share/qtvcp/widgets_ui/* $(DESTDIR)$(datadir)/qtvcp/widgets_ui
	$(TREE) ../share/gmoccapy/icons/* $(DESTDIR)$(datadir)/gmoccapy/icons

install-kernel-dep:
	$(DIR)	$(DESTDIR)$(moduledir)/linuxcnc \
		$(DESTDIR)$(bindir) \
		$(DESTDIR)$(sysconfdir)/linuxcnc
	$(FILE) ../cmod/*.so $(DESTDIR)$(EMC2_GOMOD_DIR)
	$(FILE) ../scripts/rtapi.conf $(DESTDIR)$(sysconfdir)/linuxcnc
endif # RUN_IN_PLACE

CONF=../configs
COMMON=$(CONF)/common
CONFILES=$(addsuffix /$(1), $(filter-out $(COMMON) $(CONF),\
                                         ${shell find ${CONF} -type d -print}))
install-menu: $(XDGDESKTOP)
	$(DIR) $(DESTDIR)/usr/share/applications/
	for d in $(XDGDESKTOP); do \
	    $(FILE) $$d $(DESTDIR)/usr/share/applications/ ; \
	done

endif # userspace

# ---------------------------------------------------------------------------
# GMI home module (cmod)
# ---------------------------------------------------------------------------

HOMEMOD_CMOD_SRCS := \
    emc/motion/homing.c

HOMEMOD_CMOD_CFLAGS := \
    -fPIC -Os -Wall -Wextra \
    -DUSPACE -D_GNU_SOURCE -Drealtime \
    -fno-strict-aliasing -fwrapv \
    -I. -I../include \
    -Igomc/pkg/cmodule \
    -Igomc/generated/gmi/home \
    -Igomc/generated/gmi/mot \
    -Iemc/motion

HOMEMOD_CMOD_DEPS := \
    gomc/generated/gmi/home/home_api.h \
    gomc/generated/gmi/mot/mot_api.h \
    $(wildcard gomc/pkg/cmodule/gomc_*.h) \
    $(wildcard emc/motion/homing.h) \
    $(wildcard emc/motion/motion.h)

../cmod/homemod.so: $(HOMEMOD_CMOD_SRCS) $(HOMEMOD_CMOD_DEPS) | headers
	$(ECHO) "Compiling cmod homemod.so"
	@mkdir -p $(dir $@)
	$(Q)$(CC) -shared $(HOMEMOD_CMOD_CFLAGS) \
	    -o $@ $(HOMEMOD_CMOD_SRCS) \
	    $(CMOD_LDFLAGS) -lm

TARGETS += ../cmod/homemod.so
modules: ../cmod/homemod.so

# ---------------------------------------------------------------------------
# GMI trajectory planner module (cmod)
# ---------------------------------------------------------------------------

TPMOD_CMOD_SRCS := \
    emc/tp/tp.c \
    emc/tp/tc.c \
    emc/tp/tcq.c \
    emc/tp/spherical_arc.c \
    emc/tp/blendmath.c \
    emc/nml_intf/emcpose.c

TPMOD_CMOD_CFLAGS := \
    -fPIC -Os -Wall -Wextra \
    -DUSPACE -D_GNU_SOURCE -Drealtime \
    -fno-builtin-sin -fno-builtin-cos -fno-builtin-sincos \
    -fno-strict-aliasing -fwrapv \
    -I. -I../include \
    -Igomc/pkg/cmodule \
    -Igomc/generated/gmi/tp \
    -Igomc/generated/gmi/mot \
    -Iemc/motion \
    -Iemc/tp \
    -Ilibnml/posemath

TPMOD_CMOD_DEPS := \
    gomc/generated/gmi/tp/tp_api.h \
    gomc/generated/gmi/mot/mot_api.h \
    $(wildcard gomc/pkg/cmodule/gomc_*.h) \
    $(wildcard emc/tp/*.h) \
    $(wildcard emc/motion/motion.h)

../cmod/tpmod.so: $(TPMOD_CMOD_SRCS) $(TPMOD_CMOD_DEPS) | headers ../lib/libposemath.so
	$(ECHO) "Compiling cmod tpmod.so"
	@mkdir -p $(dir $@)
	$(Q)$(CC) -shared $(TPMOD_CMOD_CFLAGS) \
	    -o $@ $(TPMOD_CMOD_SRCS) \
	    $(CMOD_LDFLAGS) -L../lib -lposemath -lm

TARGETS += ../cmod/tpmod.so
modules: ../cmod/tpmod.so

# Phony so that it is always rebuilt when requested, not because it
# shouldn't exist as a file
.PHONY: tags
tags:
	ctags-exuberant \
		--extra=+fq \
		--exclude=depends --exclude=objects --exclude=.mod.c \
		'--langmap=make:+(Submakefile),make:+(Makefile.inc),c:+.comp' \
		-I EXPORT_SYMBOL+,RTAPI_MP_INT+,RTAPI_MP_LONG+,RTAPI_MP_STRING+ \
		-I RTAPI_MP_ARRAY_INT+,RTAPI_MP_ARRAY_LONG+,RTAPI_MP_ARRAY_STRING+ \
		-I MODULE_AUTHOR+,MODULE_DESCRIPTION+,MODULE_LICENSE+ \
		-R . ../tcl ../scripts ../share/axis/tcl
	rm -f TAGS
	find . -type f -name '*.[ch]' -printf '%P\0' |xargs -0 etags -l c --append
	find . -type f -name '*.cc' -printf '%P\0' |xargs -0 etags -l c++ --append
	find . -type f -name '*.hh' -printf '%P\0' |xargs -0 etags -l c++ --append

etags:
	etags	--extra=+fq \
		--exclude=depends --exclude=objects --exclude=.mod.c \
		'--langmap=make:+(Submakefile),make:+(Makefile.inc),c:+.comp' \
		-I EXPORT_SYMBOL+,RTAPI_MP_INT+,RTAPI_MP_LONG+,RTAPI_MP_STRING+ \
		-I RTAPI_MP_ARRAY_INT+,RTAPI_MP_ARRAY_LONG+,RTAPI_MP_ARRAY_STRING+ \
		-I MODULE_AUTHOR+,MODULE_DESCRIPTION+,MODULE_LICENSE+ \
		-R . ../tcl ../scripts ../share/axis/tcl
	find . -type f -name '*.[ch]' |xargs etags --language-force=C --append
	find . -type f -name '*.cc' |xargs etags --language-force=C++ --append
	find . -type f -name '*.hh' |xargs etags --language-force=C++ --append

.PHONY: swish
swish:
	swish-e -c .swish_config -v 0 -i $(BASEPWD) \
		$(dir $(BASEPWD))tcl \
		$(dir $(BASEPWD))share/axis/tcl \
		$(dir $(BASEPWD))lib/python \
		$(dir $(BASEPWD))scripts \
		$(dir $(BASEPWD))configs \
		$(dir $(BASEPWD))docs/src \
		$(dir $(BASEPWD))docs/man/man1

# When you depend on objects/var-ZZZ you are depending on the contents of the
# variable ZZZ, which is assumed to depend on a Makefile, a Submakefile, or
# Makefile.inc
objects/var-%: Makefile $(wildcard $(SUBMAKEFILES)) Makefile.inc
	@mkdir -p $(dir $@)
	@echo $($*) > $@.tmp
	@sh move-if-change $@.tmp $@

../lib/%.so: ../lib/%.so.0
	ln -sf $(notdir $<) $@

cscope:
	cscope -Rb

cscopeclean:
	bash -c 'for f in `find ./ -name "cscope.*out"`;do rm $$f;done'
# vim:ts=8:sts=8:sw=8:noet:
