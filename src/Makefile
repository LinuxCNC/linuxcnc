# Delete the default suffix rules
.SUFFIXES:
.PHONY: default userspace modules clean modclean depclean install python pythonclean

ifeq "$(findstring s,$(MAKEFLAGS))" ""
ECHO=@echo
VECHO=echo
else
ECHO=@true
VECHO=true
endif

ifeq ($(BASEPWD),)
BASEPWD := $(shell pwd)
export BASEPWD
endif
include $(BASEPWD)/Makefile.inc
ifeq ($(RTPREFIX),)
$(error Makefile.inc must specify RTPREFIX and other variables)
endif

ifeq ($(DEPSTYLE),gcc3)
DEP = $(1) -MM -MG -MT "$(2)" $(4) -o $(3).tmp && mv -f "$(3)".tmp "$(3)"
else
DEP = bash depend.sh "$(1)" "$(2)" "$(3)" $(4)
endif

cc-option = $(shell if $(CC) $(CFLAGS) $(1) -S -o /dev/null -xc /dev/null \
             > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi ;)

ifeq ($(KERNELRELEASE),)
# When KERNELRELEASE is not defined, this is the userspace build.
# The "modules" target is the gateway to the kernel module build.
default: configs userspace modules
ifeq ($(RUN_IN_PLACE),yes)
ifneq ($(BUILD_SYS),sim)
	@if [ -f ../bin/emc_module_helper ]; then if ! [ `stat -c %u ../bin/emc_module_helper` -eq 0 -a -u ../bin/emc_module_helper ]; then $(VECHO) "You now need to run 'sudo make setuid' in order to run in place."; fi; fi
endif
endif

# Create the variables with := so that subsequent += alterations keep it
# as a "substitute at assignment time" variable
TARGETS :=
PYTARGETS := 

# Submakefiles from each of these directories will be included if they exist
SUBDIRS := \
    libnml/linklist libnml/cms libnml/rcs libnml/inifile libnml/os_intf \
    libnml/nml libnml/buffer libnml/posemath libnml \
    \
    rtapi/examples/timer rtapi/examples/semaphore rtapi/examples/shmem \
    rtapi/examples/extint rtapi/examples/fifo rtapi/examples rtapi \
    \
    hal/components hal/drivers hal/user_comps/vcp hal/user_comps/devices \
    hal/user_comps hal/classicladder hal/utils hal \
    \
    emc/usr_intf/axis \
    emc/usr_intf emc/nml_intf emc/task emc/iotask emc/kinematics emc/canterp \
    emc/motion emc/ini emc/rs274ngc emc \
    \
    module_helper \
    \
    po \
    \
    ../docs/src

ULAPISRCS := rtapi/$(RTPREFIX)_ulapi.c 

# Each item in INCLUDES is transformed into a -I directive later on
# The top directory is always included
INCLUDES := .

USERSRCS := 
PROGRAMS := 

# When used like $(call TOxxx, ...) these turn a list of source files
# into the corresponding list of object files, dependency files,
# or both.  When a source file has to be compiled with special flags,
# TOOBJSDEPS is used.  See Submakefile.skel for an example.
TOOBJS = $(patsubst %.cc,objects/%.o,$(patsubst %.c,objects/%.o,$(1)))
TODEPS = $(patsubst %.cc,depends/%.d,$(patsubst %.c,depends/%.d,$(1)))
TOOBJSDEPS = $(call TOOBJS,$(1)) $(call TODEPS,$(1) $(patsubst %.cc,%.i,$(patsubst %.c,%.i,$(1))))

SUBMAKEFILES := $(patsubst %,%/Submakefile,$(SUBDIRS))
-include $(wildcard $(SUBMAKEFILES))

# This checks that all the things listed in USERSRCS are either C files
# or C++ files
ASSERT_EMPTY = $(if $(1), $(error "Should be empty but is not: $(1)"))
$(call ASSERT_EMPTY,$(filter-out %.c %.cc, $(USERSRCS)))
$(call ASSERT_EMPTY,$(filter-out %.c, $(RTSRCS)))

ifeq ($(BUILD_PYTHON),yes)
$(call TOOBJSDEPS,$(PYSRCS)) : EXTRAFLAGS += -fPIC
USERSRCS += $(PYSRCS)
endif

# Find the list of object files for each type of source file
CUSERSRCS := $(filter %.c,$(USERSRCS))
CXXUSERSRCS := $(filter %.cc,$(USERSRCS))
CUSEROBJS := $(call TOOBJS,$(CUSERSRCS))
CXXUSEROBJS += $(call TOOBJS,$(CXXUSERSRCS))

ifneq ($(MAKECMDGOALS),clean)
# Find the dependency filenames, then include them all
DEPS := $(sort $(patsubst objects/%.o,depends/%.d,$(CUSEROBJS) $(CXXUSEROBJS)))
-include $(DEPS)
Makefile: $(DEPS)
endif

# Each directory in $(INCLUDES) is passed as a -I directory when compiling.
INCLUDE := $(patsubst %,-I%, $(INCLUDES)) -I$(RTDIR)/include
ifeq ($(BUILD_PYTHON),yes)
INCLUDE += -I$(INCLUDEPY)
endif

# Compilation options.  Perhaps some of these should come from Makefile.inc?
OPT := -O2 $(call cc-option,-fno-strict-aliasing)
DEBUG := -g -Wall
CFLAGS := $(INCLUDE) $(OPT) $(DEBUG) -DULAPI $(call cc-option, -Wdeclaration-after-statement) $(BITOPS_DEFINE)
CXXFLAGS := $(INCLUDE) $(OPT) $(DEBUG) -DULAPI $(BITOPS_DEFINE)

ifeq ($(RUN_IN_PLACE),yes)
LDFLAGS := -L$(LIB_DIR) -Wl,-rpath,$(LIB_DIR)
else
LDFLAGS := -L$(LIB_DIR) -Wl,-rpath,$(libdir)
endif

# Rules to make .d (dependency) files.
$(sort $(call TODEPS, $(filter %.cc,$(USERSRCS)))): depends/%.d: %.cc
	@mkdir -p $(dir $@)
	$(ECHO) Depending $<
	@$(call DEP,$(CXX),$@ $(patsubst depends/%.d,objects/%.o,$@),$@,$(CXXFLAGS) $(EXTRAFLAGS) $<)

$(sort $(call TODEPS, $(filter %.c,$(USERSRCS)))): depends/%.d: %.c 
	@mkdir -p $(dir $@)
	$(ECHO) Depending $<
	@$(call DEP,$(CC),$@ $(patsubst depends/%.d,objects/%.o,$@),$@,$(CFLAGS) $(EXTRAFLAGS) $<)

# Rules to make .o (object) files
$(sort $(CUSEROBJS)) : objects/%.o: %.c
	$(ECHO) Compiling $<
	@mkdir -p $(dir $@)
	@$(CC) -c $(CFLAGS) $(EXTRAFLAGS) $< -o $@

$(sort $(CXXUSEROBJS)) : objects/%.o: %.cc
	$(ECHO) Compiling $<
	@mkdir -p $(dir $@)
	@$(CXX) -c $(CXXFLAGS) $(EXTRAFLAGS) $< -o $@

# Rules to make .i (preprocessed) files
$(sort $(patsubst %.c,%.i,$(CUSERSRCS))): %.i: %.c
	@echo Preprocessing $< to $@
	@$(CC) -dD $(CFLAGS) $(EXTRAFLAGS) -E $< -o $@

$(sort $(patsubst %.cc,%.ii,$(CXXUSERSRCS))): %.ii: %.cc
	@echo Preprocessing $< to $@
	@$(CXX) -dD $(CXXFLAGS) $(EXTRAFLAGS) -E $< -o $@

config.status: configure
	if [ -f config.status ]; then ./config.status --recheck; else ./configure --enable-run-in-place; fi

config.h: config.h.in config.status
	@./config.status -q --header=$@

INFILES = \
	../docs/man/man1/emc.1 ../scripts/emc ../scripts/realtime \
	../scripts/rtapi.conf Makefile.inc depend.sh Makefile.modinc \
	../tcl/emc.tcl ../scripts/halrun ../scripts/emc-environment \
	../scripts/emcmkdesktop
$(INFILES): %: %.in config.status
	@./config.status --file=$@

default: $(INFILES)

# For each file to be copied to ../include, its location in the source tree
# is listed here.  Note that due to $(INCLUDE), defined above, the include
# files in the source tree are the ones used when building emc2.  The copy
# in ../include is used when building external components of emc2.
HEADERS := \
    config.h \
    emc/ini/iniaxis.hh \
    emc/ini/initool.hh \
    emc/ini/initraj.hh \
    emc/kinematics/cubic.h \
    emc/kinematics/kinematics.h \
    emc/kinematics/genhexkins.h \
    emc/kinematics/tc.h \
    emc/kinematics/tp.h \
    emc/motion/emcmotcfg.h \
    emc/motion/emcmotglb.h \
    emc/motion/motion.h \
    emc/motion/usrmotintf.h \
    emc/nml_intf/canon.hh \
    emc/nml_intf/emc.hh \
    emc/nml_intf/emccfg.h \
    emc/nml_intf/emcglb.h \
    emc/nml_intf/emcpos.h \
    emc/nml_intf/interp.hh \
    emc/nml_intf/interp_return.hh \
    emc/nml_intf/interpl.hh \
    emc/rs274ngc/interp_internal.hh \
    emc/rs274ngc/rs274ngc.hh \
    hal/hal.h \
    libnml/buffer/locmem.hh \
    libnml/buffer/memsem.hh \
    libnml/buffer/phantom.hh \
    libnml/buffer/physmem.hh \
    libnml/buffer/recvn.h \
    libnml/buffer/rem_msg.hh \
    libnml/buffer/sendn.h \
    libnml/buffer/shmem.hh \
    libnml/buffer/tcpmem.hh \
    libnml/cms/cms.hh \
    libnml/cms/cms_aup.hh \
    libnml/cms/cms_cfg.hh \
    libnml/cms/cms_dup.hh \
    libnml/cms/cms_srv.hh \
    libnml/cms/cms_up.hh \
    libnml/cms/cms_user.hh \
    libnml/cms/cms_xup.hh \
    libnml/cms/cmsdiag.hh \
    libnml/cms/tcp_opts.hh \
    libnml/cms/tcp_srv.hh \
    libnml/inifile/inifile.hh \
    libnml/linklist/linklist.hh \
    libnml/nml/cmd_msg.hh \
    libnml/nml/nml.hh \
    libnml/nml/nml_mod.hh \
    libnml/nml/nml_oi.hh \
    libnml/nml/nml_srv.hh \
    libnml/nml/nmldiag.hh \
    libnml/nml/nmlmsg.hh \
    libnml/nml/stat_msg.hh \
    libnml/os_intf/_sem.h \
    libnml/os_intf/sem.hh \
    libnml/os_intf/_shm.h \
    libnml/os_intf/shm.hh \
    libnml/os_intf/_timer.h \
    libnml/os_intf/timer.hh \
    libnml/posemath/posemath.h \
    libnml/posemath/sincos.h \
    libnml/rcs/rcs.hh \
    libnml/rcs/rcs_exit.hh \
    libnml/rcs/rcs_print.hh \
    libnml/rcs/rcsversion.h \
    rtapi/rtapi.h \
    rtapi/rtapi_app.h \
    rtapi/rtapi_bitops.h \
    rtapi/rtapi_math.h \
    rtapi/rtapi_math_i386.h \
    rtapi/rtapi_common.h \
    rtapi/rtapi_ctype.h \
    rtapi/rtapi_errno.h \
    rtapi/rtapi_string.h

# Make each include file a target
TARGETS += $(patsubst %,../include/%,$(foreach h,$(HEADERS),$(notdir $h)))

# Add converting of %.po files
TARGETS += $(patsubst %.po, %.mo, $(wildcard po/*_axis.po po/*_rs274_err.po))
ifeq "$(MSGFMT_TCL_WORKS)" "yes"
TARGETS += $(patsubst %.po, %.msg, $(wildcard po/??.po po/??_??.po))
endif

# And make userspace depend on $(TARGETS)
userspace: $(TARGETS)

ifeq ($(BUILD_PYTHON),yes)
pythonclean:
	rm -f $(PYTARGETS)
python: $(PYTARGETS)
userspace: python
clean: docclean pythonclean
endif

# This is the gateway into the crazy world of "kbuild", the linux 2.6 system
# for building kernel modules.  Other kernel module build styles need to be
# accomodated here.
ifeq ($(BUILD_SYS),kbuild)

# '-o $(KERNELDIR)/Module.symvers' silences warnings about that file being missing
modules:
	$(MAKE) -C $(KERNELDIR) SUBDIRS=`pwd` CC=$(CC) V=0 -o $(KERNELDIR)/Module.symvers modules 
	-cp *$(MODULE_EXT) ../rtlib/
endif

# These rules clean things up.  'modclean' cleans files generated by 'modules'
# (except that it doesn't remove the modules that were copied to rtlib)
# 'clean' cleans everything but dependency files, and 'depclean' cleans them
# too.
modclean:
	find -name '.*.cmd' -or -name '*.ko' -or -name '*.mod.c' -or -name '*.mod.o' | xargs rm -f
	-rm -rf .tmp_versions
	find . -name .tmp_versions |xargs rm -rf
	-rm -f ../rtlib/*.ko

depclean:
	-rm -rf depends

clean: depclean modclean
	find . -name '*.o' |xargs rm -f
	-rm -rf objects 
	-rm -f $(TARGETS)
	-rm -f $(COPY_CONFIGS)
	-rm -f ../rtlib/*.$(MODULE_EXT)
	-rm -f hal/components/conv_*.comp

# So that nothing is built as root, this rule does not depend on the touched
# files (Note that files in depends/ might be rebuilt, and there's little that
# can be done about it)
ifeq ($(BUILD_SYS),sim)
setuid:
	@echo "'make setuid' is not needed for the simulator"
else
setuid:
	chown root ../bin/emc_module_helper
	chmod 4750 ../bin/emc_module_helper
endif

# These rules allows a header file from this directory to be installed into
# ../include.  A pair of rules like these will exist in the Submakefile
# of each file that contains headers.
../include/%.h: %.h
	-cp $^ $@
../include/%.hh: %.hh
	-cp $^ $@

DIR=install -d -m 0755 -o root
FILE=install -m 0644 -o root
TREE=cp -dR
CONFIGFILE=install -m 0644
EXE=install -m 0755 -o root
SETUID=install -m 4755 -o root
NOCVS=$(filter-out %CVS,$(1))
GLOB=$(call NOCVS,$(wildcard $(1)))
CONFIGS=$(subst ../configs/,,$(call GLOB,../configs/*))

ifeq ($(RUN_IN_PLACE),yes)
define ERROR_MESSAGE
You configured run-in-place, but are trying to install.  
For an installable version, run configure without --enable-run-in-place 
and rebuild
endef
install:
	$(error $(ERROR_MESSAGE))
else

define INSTALL_CONFIG
$(DIR) $(DESTDIR)$(sysconfdir)/emc2/sample-configs/$(1)
$(FILE) $(call GLOB,../configs/$(1)/*) $(DESTDIR)$(sysconfdir)/emc2/sample-configs/$(1)

endef
INSTALL_CONFIGS=$(foreach config,$(CONFIGS), $(call INSTALL_CONFIG,$(config)))

DOCS=INSTALL NEWS README AUTHORS

DOCS_HELP=$(call GLOB,../docs/help/*)
NC_FILES=$(call GLOB,../nc_files/*)
TCL=$(call GLOB,../tcl/*.tcl)
TCL_BIN=$(call GLOB,../tcl/bin/*.tcl) ../tcl/bin/popimage

install: install-kernel-dep install-kernel-indep
	$(ECHO) "Installed in $(DESTDIR) with prefix $(prefix)"

install-kernel-indep:
	$(DIR) $(DESTDIR)$(initd_dir) $(DESTDIR)$(EMC2_RTLIB_DIR) \
		$(DESTDIR)$(sysconfdir)/emc2 $(DESTDIR)$(bindir) \
		$(DESTDIR)/usr/local/etc/emc2/configs \
		$(DESTDIR)$(libdir) $(DESTDIR)$(includedir)/emc2 \
		$(DESTDIR)$(docsdir)/help $(DESTDIR)$(ncfilesdir) \
		$(DESTDIR)/etc/X11/app-defaults $(DESTDIR)$(tcldir)/bin \
		$(DESTDIR)$(tcldir)/scripts \
		$(DESTDIR)$(mandir)/man1 \
		$(DESTDIR)$(mandir)/man3 \
		$(DESTDIR)$(mandir)/man9 \
		$(DESTDIR)$(tcldir)/msgs \
		$(DESTDIR)$(localedir)/de/LC_MESSAGES \
		$(DESTDIR)$(SITEPY) \
		$(DESTDIR)$(SITEPY)/rs274 \
		$(DESTDIR)$(datadir)/axis/images \
		$(DESTDIR)$(datadir)/axis/tcl		
	$(FILE) ../docs/man/man1/*.1 $(DESTDIR)$(mandir)/man1
	$(FILE) $(filter-out %/skeleton.3hal, $(wildcard ../docs/man/man3/*.3hal)) $(DESTDIR)$(mandir)/man3
	$(FILE) $(filter-out %/skeleton.3rtapi, $(wildcard ../docs/man/man3/*.3rtapi)) $(DESTDIR)$(mandir)/man3
	$(FILE) $(filter-out %/skeleton.9, $(wildcard ../docs/man/man9/*.9)) $(DESTDIR)$(mandir)/man9
	$(FILE) po/*.msg $(DESTDIR)$(tcldir)/msgs
	$(EXE) ../scripts/realtime $(DESTDIR)$(initd_dir)
	$(EXE) ../scripts/halrun $(DESTDIR)$(bindir)
	$(FILE) ../*.png ../*.gif $(DESTDIR)$(sysconfdir)/emc2
	$(INSTALL_CONFIGS)
	$(EXE) $(filter-out ../bin/emc_module_helper, $(filter ../bin/%,$(TARGETS))) $(DESTDIR)$(bindir)
	$(EXE) ../scripts/emc $(DESTDIR)$(bindir)
	$(FILE) $(filter ../lib/%.a ../lib/%.so,$(TARGETS)) $(DESTDIR)$(libdir)
	$(FILE) ../lib/python/*.py ../lib/python/*.so $(DESTDIR)$(SITEPY)
	$(FILE) ../lib/python/rs274/*.py $(DESTDIR)$(SITEPY)/rs274
	$(EXE) ../bin/axis ../bin/axis-remote ../bin/debuglevel ../bin/emctop ../bin/mdi ../bin/hal_manualtoolchange ../bin/image-to-gcode $(DESTDIR)$(bindir)
	$(FILE) emc/usr_intf/axis/etc/axis_light_background $(DESTDIR)$(docsdir)
	$(FILE) emc/usr_intf/axis/README $(DESTDIR)$(docsdir)/README.axis
	$(FILE) ../share/axis/images/*.gif ../share/axis/images/*.xbm ../share/axis/images/*.ngc $(DESTDIR)$(datadir)/axis/images
	$(FILE) ../share/axis/tcl/*.tcl $(DESTDIR)$(datadir)/axis/tcl
	-ldconfig $(DESTDIR)$(libdir)
	$(FILE) $(filter %.h %.hh,$(TARGETS)) $(DESTDIR)$(includedir)/emc2/
	$(FILE) $(addprefix ../docs/,$(DOCS)) $(DESTDIR)$(docsdir)
	$(FILE) $(DOCS_HELP) $(DESTDIR)$(docsdir)/help
	$(FILE) $(NC_FILES) $(DESTDIR)$(ncfilesdir)
	$(EXE) ../nc_files/M101 $(DESTDIR)$(ncfilesdir)
	$(FILE) ../tcl/TkEmc $(DESTDIR)/etc/X11/app-defaults
	$(FILE) ../app-defaults/XEmc $(DESTDIR)/etc/X11/app-defaults
	$(FILE) Makefile.modinc $(DESTDIR)/etc/emc2
	$(EXE) $(TCL) $(DESTDIR)$(tcldir)
	$(EXE) $(TCL_BIN) $(DESTDIR)$(tcldir)/bin
	$(FILE) ../tcl/scripts/balloon.tcl ../tcl/scripts/emchelp.tcl $(DESTDIR)$(tcldir)/scripts
	$(EXE) ../tcl/scripts/Set_Coordinates.tcl $(DESTDIR)$(tcldir)/scripts

install-kernel-dep:
	$(DIR)  $(DESTDIR)$(moduledir)/emc2 \
		$(DESTDIR)$(bindir) \
		$(DESTDIR)$(sysconfdir)/emc2
	$(FILE) ../rtlib/*$(MODULE_EXT) $(DESTDIR)$(EMC2_RTLIB_DIR)
ifneq "$(BUILD_SYS)" "sim"
	$(SETUID) ../bin/emc_module_helper $(DESTDIR)$(bindir)
endif
	$(FILE) ../scripts/rtapi.conf $(DESTDIR)$(sysconfdir)/emc2
endif # RUN_IN_PLACE

CONF=../configs
COMMON=$(CONF)/common
CONFILES=$(addsuffix /$(1), $(filter-out $(COMMON), $(wildcard $(CONF)/*)))
.PHONY: configs
COPY_CONFIGS := \
	$(patsubst %,../configs/%/emc.nml, demo_mazak halui_halvcp hexapod-sim max \
		univpwm univstep stepper-xyza demo_step_cl stepper motenc m5i20 stg \
		sim vti nist-lathe dallur-thc demo_sim_cl etch-servo ppmc lathe-pluto) \
	$(patsubst %,../configs/%/core_stepper.hal, demo_step_cl stepper demo_sim_cl) \
	$(patsubst %,../configs/%/core_servo.hal, motenc m5i20 stg vti) \
	$(patsubst %,../configs/%/core_sim.hal, halui_halvcp sim) \
	../configs/sim/axis_manualtoolchange.hal
configs: $(COPY_CONFIGS)
	
$(call CONFILES,axis_manualtoolchange.hal): %/axis_manualtoolchange.hal: ../configs/common/axis_manualtoolchange.hal
	-cp $< $@
$(call CONFILES,emc.nml): %/emc.nml: ../configs/common/emc.nml
	-cp $< $@
$(call CONFILES,core_stepper.hal): %/core_stepper.hal: ../configs/common/core_stepper.hal
	-cp $< $@
$(call CONFILES,core_servo.hal): %/core_servo.hal: ../configs/common/core_servo.hal
	-cp $< $@
$(call CONFILES,core_sim.hal): %/core_sim.hal: ../configs/common/core_sim.hal
	-cp $< $@
 
endif # userspace

ifneq ($(KERNELRELEASE),)
include $(BASEPWD)/hal/components/Submakefile
endif

# KERNELRELEASE is nonempty, therefore we are building modules using the
# "kbuild" system.  $(BASEPWD) is used here, instead of relative paths, because
# that's what kbuild seems to require

EXTRA_CFLAGS = $(RTFLAGS) -D__MODULE__ -I$(BASEPWD) -I$(BASEPWD)/libnml/linklist \
	-I$(BASEPWD)/libnml/cms -I$(BASEPWD)/libnml/rcs -I$(BASEPWD)/libnml/inifile \
	-I$(BASEPWD)/libnml/os_intf -I$(BASEPWD)/libnml/nml -I$(BASEPWD)/libnml/buffer \
	-I$(BASEPWD)/libnml/posemath -I$(BASEPWD)/rtapi -I$(BASEPWD)/hal \
	-I$(BASEPWD)/emc/nml_intf -I$(BASEPWD)/emc/kinematics -I$(BASEPWD)/emc/motion \
        -DSEQUENTIAL_SUPPORT -DHAL_SUPPORT -DDYNAMIC_PLCSIZE -DRT_SUPPORT

ifeq "$(USE_STUBS)" "1"
MATHSTUB := rtapi/mathstubs.o
endif

ifdef SEQUENTIAL_SUPPORT
EXTRA_CFLAGS += -DSEQUENTIAL_SUPPORT
endif

# For each module, there's an addition to obj-m or obj-$(CONFIG_foo)
# plus a definition of foo-objs, which contains the full path to the
# object file(s) that the module contains.  Unfortunately, this setup pollutes
# the source directory with object files and other temporaries, but I can't
# find a way around it.

# Subdirectory:  rtapi
obj-$(CONFIG_RTAPI) += rtapi.o
rtapi-objs := rtapi/$(RTPREFIX)_rtapi.o

# Subdirectory: rtapi/examples (unneeded?)

# Subdirectory: hal/components
obj-$(CONFIG_BLOCKS) += blocks.o
blocks-objs := hal/components/blocks.o $(MATHSTUB)
obj-$(CONFIG_DEBOUNCE) += debounce.o
debounce-objs := hal/components/debounce.o $(MATHSTUB)
obj-$(CONFIG_ENCODER) += encoder.o
encoder-objs := hal/components/encoder.o $(MATHSTUB)
obj-$(CONFIG_COUNTER) += counter.o
counter-objs := hal/components/counter.o $(MATHSTUB)
obj-$(CONFIG_ENCODER_RATIO) += encoder_ratio.o
encoder_ratio-objs := hal/components/encoder_ratio.o $(MATHSTUB)
obj-$(CONFIG_STEPGEN) += stepgen.o
stepgen-objs := hal/components/stepgen.o $(MATHSTUB)
obj-$(CONFIG_FREQGEN) += freqgen.o
freqgen-objs := hal/components/freqgen.o $(MATHSTUB)
obj-$(CONFIG_PWMGEN) += pwmgen.o
pwmgen-objs := hal/components/pwmgen.o $(MATHSTUB)
obj-$(CONFIG_SIGGEN) += siggen.o
siggen-objs := hal/components/siggen.o $(MATHSTUB)
obj-$(CONFIG_PID) += pid.o
pid-objs := hal/components/pid.o $(MATHSTUB)
obj-$(CONFIG_PID) += threads.o
threads-objs := hal/components/threads.o $(MATHSTUB)
obj-$(CONFIG_SUPPLY) += supply.o
supply-objs := hal/components/supply.o $(MATHSTUB)
obj-$(CONFIG_TIMEDELAY) += timedelay.o
timedelay-objs := hal/components/timedelay.o $(MATHSTUB)
obj-$(CONFIG_SIM_ENCODER) += sim_encoder.o
sim_encoder-objs := hal/components/sim_encoder.o $(MATHSTUB)
obj-$(CONFIG_WEIGHTED_SUM) += weighted_sum.o
weighted_sum-objs := hal/components/weighted_sum.o $(MATHSTUB)
obj-$(CONFIG_MODMATH) += modmath.o
modmath-objs := hal/components/modmath.o $(MATHSTUB)
obj-$(CONFIG_STREAMER) += streamer.o
streamer-objs := hal/components/streamer.o $(MATHSTUB)
obj-$(CONFIG_SAMPLER) += sampler.o
sampler-objs := hal/components/sampler.o $(MATHSTUB)

# Subdirectory: hal/drivers
ifneq ($(BUILD_SYS),sim)
obj-$(CONFIG_HAL_PARPORT) += hal_parport.o
hal_parport-objs := hal/drivers/hal_parport.o $(MATHSTUB)
obj-$(CONFIG_HAL_TIRO) += hal_tiro.o
hal_tiro-objs := hal/drivers/hal_tiro.o $(MATHSTUB)
obj-$(CONFIG_HAL_STG) += hal_stg.o
hal_stg-objs := hal/drivers/hal_stg.o $(MATHSTUB)
obj-$(CONFIG_HAL_VTI) += hal_vti.o
hal_vti-objs := hal/drivers/hal_vti.o $(MATHSTUB)
obj-$(CONFIG_HAL_EVOREG) += hal_evoreg.o
hal_evoreg-objs := hal/drivers/hal_evoreg.o $(MATHSTUB)
obj-$(CONFIG_HAL_MOTENC) += hal_motenc.o
hal_motenc-objs := hal/drivers/hal_motenc.o $(MATHSTUB)
obj-$(CONFIG_HAL_M5I20) += hal_m5i20.o
hal_m5i20-objs := hal/drivers/hal_m5i20.o $(MATHSTUB)
obj-$(CONFIG_HAL_AX521H) += hal_ax5214h.o
hal_ax5214h-objs := hal/drivers/hal_ax5214h.o $(MATHSTUB)
obj-$(CONFIG_HAL_PPMC) += hal_ppmc.o
hal_ppmc-objs := hal/drivers/hal_ppmc.o $(MATHSTUB)
obj-$(CONFIG_HAL_SPEAKER) += hal_speaker.o
hal_speaker-objs := hal/drivers/hal_speaker.o $(MATHSTUB)
obj-$(CONFIG_HAL_SKELETON) += hal_skeleton.o
hal_skeleton-objs := hal/drivers/hal_skeleton.o $(MATHSTUB)

ifneq "$(filter 2.6.%, $(kernelvers))" ""
obj-$(CONFIG_PROBE_PARPORT) += probe_parport.o
probe_parport-objs := hal/drivers/probe_parport.o $(MATHSTUB)
endif
endif

obj-$(CONFIG_CLASSICLADDER_RT) += classicladder_rt.o
classicladder_rt-objs := hal/classicladder/module_hal.o $(MATHSTUB)
classicladder_rt-objs += hal/classicladder/arithm_eval.o
classicladder_rt-objs += hal/classicladder/arrays.o
classicladder_rt-objs += hal/classicladder/calc.o
classicladder_rt-objs += hal/classicladder/calc_sequential.o
classicladder_rt-objs += hal/classicladder/manager.o
classicladder_rt-objs += hal/classicladder/symbols.o
classicladder_rt-objs += hal/classicladder/vars_access.o

ifdef SEQUENTIAL_SUPPORT
classicladder_rt-objs += hal/classicladder/calc_sequential_rt.o
endif

obj-m += scope_rt.o
scope_rt-objs := hal/utils/scope_rt.o $(MATHSTUB)

obj-m += hal_lib.o
hal_lib-objs := hal/hal_lib.o $(MATHSTUB)

obj-m += trivkins.o
trivkins-objs := emc/kinematics/trivkins.o

obj-m += rotatekins.o
rotatekins-objs := emc/kinematics/rotatekins.o

obj-m += tripodkins.o
tripodkins-objs := emc/kinematics/tripodkins.o

obj-m += genhexkins.o
genhexkins-objs := emc/kinematics/genhexkins.o

obj-$(CONFIG_MOTMOD) += motmod.o
motmod-objs := emc/kinematics/cubic.o 
motmod-objs += emc/kinematics/tc.o 
motmod-objs += emc/kinematics/tp.o 
motmod-objs += emc/motion/motion.o 
motmod-objs += emc/motion/command.o 
motmod-objs += emc/motion/control.o 
motmod-objs += emc/motion/emcmotglb.o 
motmod-objs += emc/motion/emcmotutil.o 
motmod-objs += libnml/posemath/_posemath.o
motmod-objs += libnml/posemath/sincos.o $(MATHSTUB)

TORTOBJS = $(foreach file,$($(patsubst %.o,%,$(1))-objs), objects/rt$(file))
ifeq ($(BUILD_SYS),sim)
EXTRA_CFLAGS += -fPIC -O2
RTOBJS := $(sort $(foreach mod,$(obj-m),$(call TORTOBJS,$(mod))))

RTDEPS := $(sort $(patsubst objects/%.o,depends/%.d, $(RTOBJS)))
modules: $(patsubst %.o,../rtlib/%.so,$(obj-m))
../rtlib/%.so:
	$(ECHO) Linking $@
	@$(CC) -shared -o $@ $^ -lm

$(sort $(RTDEPS)): depends/rt%.d: %.c
	@mkdir -p $(dir $@)
	$(ECHO) Depending $<
	@$(call DEP,$(CC),$@ $(patsubst depends/%.d,objects/%.o,$@),$@,$(OPT) $(DEBUG) -DSIM -DRTAPI $(EXTRA_CFLAGS) $<)

# Rules to make .o (object) files
$(sort $(RTOBJS)) : objects/rt%.o : %.c
	$(ECHO) Compiling realtime $<
	@mkdir -p $(dir $@)
	@$(CC) -c $(OPT) $(DEBUG) -DSIM -DSIMULATOR -DRTAPI $(EXTRA_CFLAGS) $< -o $@
endif

ifeq ($(BUILD_SYS),normal)
modules: $(patsubst %,../rtlib/%,$(obj-m))
RTOBJS := $(sort $(foreach mod,$(obj-m),$(call TORTOBJS,$(mod))))
RTDEPS := $(sort $(patsubst objects/%.o,depends/%.d, $(RTOBJS)))

$(sort $(RTDEPS)): depends/rt%.d: %.c
	@mkdir -p $(dir $@)
	$(ECHO) Depending $<
	@$(call DEP,$(CC),$@ $(patsubst depends/%.d,objects/%.o,$@),$@,$(EXTRA_CFLAGS) $<)


# Rules to make .o (object) files
$(sort $(RTOBJS)) : objects/rt%.o : %.c
	$(ECHO) Compiling realtime $<
	@mkdir -p $(dir $@)
	$(CC) -c -DRTAPI -nostdinc -isystem $(shell $(CC) -print-file-name=include) -I$(KERNELDIR)/include $(EXTRA_CFLAGS) $< -o $@

../rtlib/%.o:
	$(ECHO) Linking $@
	@ld -r -static -S -O2 -o $@ $^ $(EXTRALINK) $(MATHLIB)
endif

ifneq "$(filter normal sim,$(BUILD_SYS))" ""
ifneq "$(BUILD_SYS)" "sim"
../rtlib/rtapi$(MODULE_EXT): $(addprefix objects/rt,$(rtapi-objs))
endif
../rtlib/classicladder_rt$(MODULE_EXT): $(addprefix objects/rt,$(classicladder_rt-objs))
../rtlib/blocks$(MODULE_EXT): $(addprefix objects/rt,$(blocks-objs))
../rtlib/debounce$(MODULE_EXT): $(addprefix objects/rt,$(debounce-objs))
../rtlib/encoder$(MODULE_EXT): $(addprefix objects/rt,$(encoder-objs))
../rtlib/counter$(MODULE_EXT): $(addprefix objects/rt,$(counter-objs))
../rtlib/encoder_ratio$(MODULE_EXT): $(addprefix objects/rt,$(encoder_ratio-objs))
../rtlib/stepgen$(MODULE_EXT): $(addprefix objects/rt,$(stepgen-objs))
../rtlib/freqgen$(MODULE_EXT): $(addprefix objects/rt,$(freqgen-objs))
../rtlib/pwmgen$(MODULE_EXT): $(addprefix objects/rt,$(pwmgen-objs))
../rtlib/siggen$(MODULE_EXT): $(addprefix objects/rt,$(siggen-objs))
../rtlib/pid$(MODULE_EXT): $(addprefix objects/rt,$(pid-objs))
../rtlib/threads$(MODULE_EXT): $(addprefix objects/rt,$(threads-objs))
../rtlib/supply$(MODULE_EXT): $(addprefix objects/rt,$(supply-objs))
../rtlib/timedelay$(MODULE_EXT): $(addprefix objects/rt,$(timedelay-objs))
../rtlib/sim_encoder$(MODULE_EXT): $(addprefix objects/rt,$(sim_encoder-objs))
../rtlib/weighted_sum$(MODULE_EXT): $(addprefix objects/rt,$(weighted_sum-objs))
../rtlib/modmath$(MODULE_EXT): $(addprefix objects/rt,$(modmath-objs))
../rtlib/streamer$(MODULE_EXT): $(addprefix objects/rt,$(streamer-objs))
../rtlib/sampler$(MODULE_EXT): $(addprefix objects/rt,$(sampler-objs))
../rtlib/hal_parport$(MODULE_EXT): $(addprefix objects/rt,$(hal_parport-objs))
../rtlib/hal_tiro$(MODULE_EXT): $(addprefix objects/rt,$(hal_tiro-objs))
../rtlib/hal_stg$(MODULE_EXT): $(addprefix objects/rt,$(hal_stg-objs))
../rtlib/hal_vti$(MODULE_EXT): $(addprefix objects/rt,$(hal_vti-objs))
../rtlib/hal_evoreg$(MODULE_EXT): $(addprefix objects/rt,$(hal_evoreg-objs))
../rtlib/hal_motenc$(MODULE_EXT): $(addprefix objects/rt,$(hal_motenc-objs))
../rtlib/hal_m5i20$(MODULE_EXT): $(addprefix objects/rt,$(hal_m5i20-objs))
../rtlib/hal_ax5214h$(MODULE_EXT): $(addprefix objects/rt,$(hal_ax5214h-objs))
../rtlib/hal_ppmc$(MODULE_EXT): $(addprefix objects/rt,$(hal_ppmc-objs))
../rtlib/hal_skeleton$(MODULE_EXT): $(addprefix objects/rt,$(hal_skeleton-objs))
../rtlib/hal_speaker$(MODULE_EXT): $(addprefix objects/rt,$(hal_speaker-objs))
../rtlib/scope_rt$(MODULE_EXT): $(addprefix objects/rt,$(scope_rt-objs))
../rtlib/hal_lib$(MODULE_EXT): $(addprefix objects/rt,$(hal_lib-objs))
../rtlib/motmod$(MODULE_EXT): $(addprefix objects/rt,$(motmod-objs))
../rtlib/trivkins$(MODULE_EXT): $(addprefix objects/rt,$(trivkins-objs))
../rtlib/rotatekins$(MODULE_EXT): $(addprefix objects/rt,$(rotatekins-objs))
../rtlib/tripodkins$(MODULE_EXT): $(addprefix objects/rt,$(tripodkins-objs))
../rtlib/genhexkins$(MODULE_EXT): $(addprefix objects/rt,$(genhexkins-objs))

ifneq ($(MAKECMDGOALS),clean)
RTDEPS := $(sort $(patsubst objects/%.o,depends/%.d,$(RTOBJS)))
-include $(RTDEPS)
Makefile: $(RTDEPS)
endif
endif

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
.PHONY: swish
swish:
	swish-e -c .swish_config -v 0 -i $(BASEPWD) \
		$(dir $(BASEPWD)){tcl,share/axis/tcl,scripts,configs,docs/src,docs/man/man1}
        
# When you depend on objects/var-ZZZ you are depending on the contents of the
# variable ZZZ, which is assumed to depend on a Makefile, a Submakefile, or
# Makefile.inc
objects/var-%: Makefile $(wildcard $(SUBMAKEFILES)) Makefile.inc
	@mkdir -p $(dir $@)
	@echo $($*) > $@.tmp
	@sh move-if-change $@.tmp $@
