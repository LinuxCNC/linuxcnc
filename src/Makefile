include $(PWD)/Makefile.inc

ifeq ($(KERNELRELEASE),)
default: userspace modules
	
TARGETS :=
SUBDIRS := \
    libnml/linklist libnml/cms libnml/rcs libnml/inifile libnml/os_intf \
    libnml/nml libnml/buffer libnml/posemath libnml \
    \
    rtapi/examples/timer rtapi/examples/semaphore rtapi/examples/shmem \
    rtapi/examples/extint rtapi/examples/fifo rtapi/examples rtapi \
    \
    hal/components hal/drivers hal/user_comps/vcp hal/user_comps \
    hal/classicladder hal/utils hal \
    \
    emc/usr_intf emc/nml_intf emc/task emc/iotask emc/kinematics emc/canterp \
    emc/motion emc/ini emc/rs274ngc emc \
    \
    module_helper

ULAPISRCS := rtapi/$(RTPREFIX)_ulapi.c 

INCLUDES := .

USERSRCS := 
PROGRAMS := 

TOOBJS = $(patsubst %.cc,objects/%.o,$(patsubst %.c,objects/%.o,$(1)))
TODEPS = $(patsubst %.cc,depends/%.d,$(patsubst %.c,depends/%.d,$(1)))
TOOBJSDEPS = $(call TOOBJS,$(1)) $(call TODEPS,$(1))

SUBMAKEFILES := $(patsubst %,%/Submakefile,$(SUBDIRS))
-include $(SUBMAKEFILES)

ASSERT_EMPTY = $(if $(1), $(error "Should be empty but is not: $(1)"))
$(call ASSERT_EMPTY,$(filter-out %.c %.cc, $(USERSRCS)))
$(call ASSERT_EMPTY,$(filter-out %.c, $(RTSRCS)))

CUSEROBJS := $(call TOOBJS,$(filter %.c,$(USERSRCS)))
CXXUSEROBJS += $(call TOOBJS,$(filter %.cc,$(USERSRCS)))

DEPS := $(sort $(patsubst objects/%.o,depends/%.d,$(CUSEROBJS) $(CXXUSEROBJS)))
include $(DEPS)
INCLUDE := -I$(RTDIR)/include $(patsubst %,-I%, $(INCLUDES))
OPT := -O
DEBUG := -g -Wall
CFLAGS := $(INCLUDE) $(OPT) $(DEBUG) -DULAPI
CXXFLAGS := $(INCLUDE) $(OPT) $(DEBUG) -DULAPI
RPATH := $(shell cd ../lib; /bin/pwd)
LDFLAGS := -Wl,-rpath,$(RPATH)

depends/%.d: %.cc
	@mkdir -p $(dir $@)
	@echo Depending $<
	@$(CXX) -MM -MG -MT "$@ $(patsubst depends/%.d,objs/%.o,$@)" \
		$(CXXFLAGS) $(EXTRAFLAGS) $< -o $@.tmp && mv -f $@.tmp $@
depends/%.d: %.c 
	@mkdir -p $(dir $@)
	@echo Depending $<
	@gcc -MM -MG -MT "$@ $(patsubst depends/%.d,objs/%.o,$@)" \
		$(CFLAGS) $(EXTRAFLAGS) $< -o $@.tmp && mv -f $@.tmp $@

$(sort $(CUSEROBJS)) : objects/%.o: %.c
	@echo Compiling $< $(EXTRAFLAGS)
	@mkdir -p $(dir $@)
	@$(CC) -c $(CFLAGS) $(EXTRAFLAGS) $< -o $@

$(sort $(CXXUSEROBJS)) : objects/%.o: %.cc
	@echo Compiling $< $(EXTRAFLAGS)
	@mkdir -p $(dir $@)
	@$(CXX) -c $(CXXFLAGS) $(EXTRAFLAGS) $< -o $@


HEADERS := \
    emc/nml_intf/canon.hh \
    libnml/nml/cmd_msg.hh \
    libnml/cms/cms.hh \
    libnml/cms/cms_aup.hh \
    libnml/cms/cms_cfg.hh \
    libnml/cms/cms_dup.hh \
    libnml/cms/cms_srv.hh \
    libnml/cms/cms_up.hh \
    libnml/cms/cms_user.hh \
    libnml/cms/cms_xup.hh \
    libnml/cms/cmsdiag.hh \
    config.h \
    emc/kinematics/cubic.h \
    emc/nml_intf/emc.hh \
    emc/nml_intf/emccfg.h \
    emc/nml_intf/emcglb.h \
    emc/motion/emcmotcfg.h \
    emc/motion/emcmotglb.h \
    emc/nml_intf/emcpos.h \
    emc/ini/iniaux.hh \
    emc/ini/iniaxis.hh \
    emc/ini/inicool.hh \
    emc/ini/inilube.hh \
    emc/ini/inispin.hh \
    emc/ini/initool.hh \
    emc/ini/initraj.hh \
    emc/nml_intf/interp.hh \
    emc/rs274ngc/interp_internal.hh \
    emc/nml_intf/interp_return.hh \
    emc/nml_intf/interpl.hh \
    emc/kinematics/kinematics.h \
    emc/kinematics/mmxavg.h \
    emc/motion/motion.h \
    libnml/nml/nml.hh \
    libnml/nml/nml_mod.hh \
    libnml/nml/nml_oi.hh \
    libnml/nml/nml_srv.hh \
    libnml/nml/nmldiag.hh \
    libnml/nml/nmlmsg.hh \
    rtapi/procfs_macros.h \
    emc/rs274ngc/rs274ngc.hh \
    rtapi/rtapi.h \
    rtapi/rtapi_app.h \
    libnml/nml/stat_msg.hh \
    emc/kinematics/tc.h \
    libnml/cms/tcp_opts.hh \
    libnml/cms/tcp_srv.hh \
    emc/kinematics/tp.h \
    emc/motion/usrmotintf.h \
    libnml/rcs/rcs.hh \
    libnml/posemath/posemath.h \
    libnml/rcs/rcs_print.hh \
    libnml/linklist/linklist.hh \
    libnml/buffer/physmem.hh \
    libnml/buffer/rem_msg.hh \
    libnml/rcs/rcsversion.h \
    libnml/rcs/rcs_exit.hh \
    libnml/inifile/inifile.hh \
    libnml/os_intf/timer.hh

TARGETS += $(patsubst %,../include/%,$(foreach h,$(HEADERS),$(notdir $h)))

userspace: $(TARGETS)

ifndef KDIR
KDIR := $(shell cd /lib/modules/$(shell uname -r)/build; /bin/pwd)
endif

modules:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) V=0 -o $(KDIR)/Module.symvers modules 
	cp *.ko ../rtlib/

modclean:
	find -name '.*.cmd' -or -name '*.ko' -or -name '*.mod.c' -or -name '*.mod.o' | xargs rm -f
	-rm -rf .tmp_versions *.o

clean: modclean
	-rm -rf objects $(TARGETS)

depclean: clean
	-rm -rf depends

# So that nothing is built as root, this rule does not depend on the touched files
# (Note that files in depends/ might be rebuilt, and there's little that can be done
# about it)
setuid:
	chown root ../bin/emc_module_helper ../bin/halcmd
	chmod 4750 ../bin/emc_module_helper ../bin/halcmd

../include/%.h: %.h
	cp $^ $@
../include/%.hh: %.hh
	cp $^ $@
else
# KERNELRELEASE nonempty, therefore building modules

EXTRA_CFLAGS = $(RTFLAGS) -D__MODULE__ -I/usr/include -I$(PWD) -I$(PWD)/libnml/linklist \
	-I$(PWD)/libnml/cms -I$(PWD)/libnml/rcs -I$(PWD)/libnml/inifile \
	-I$(PWD)/libnml/os_intf -I$(PWD)/libnml/nml -I$(PWD)/libnml/buffer \
	-I$(PWD)/libnml/posemath -I$(PWD)/rtapi -I$(PWD)/hal -I$(PWD)/emc/nml_intf \
	-I$(PWD)/emc/kinematics -I$(PWD)/emc/motion

ifdef SEQUENTIAL_SUPPORT
EXTRA_CFLAGS += -DSEQUENTIAL_SUPPORT
endif


# Subdirectory:  rtapi
obj-$(CONFIG_RTAPI) += rtapi.o
rtapi-objs := rtapi/$(RTPREFIX)_rtapi.o

# Subdirectory: rtapi/examples (unneeded?)

# Subdirectory: hal/components
obj-$(CONFIG_BLOCKS) += blocks.o
blocks-objs := hal/components/blocks.o
obj-$(CONFIG_DEBOUNCE) += debounce.o
debounce-objs := hal/components/debounce.o
obj-$(CONFIG_ENCODER) += encoder.o
encoder-objs := hal/components/encoder.o
obj-$(CONFIG_ENCODER_RATIO) += encoder_ratio.o
encoder_ratio-objs := hal/components/encoder_ratio.o
obj-$(CONFIG_STEPGEN) += stepgen.o
stepgen-objs := hal/components/stepgen.o
obj-$(CONFIG_FREQGEN) += freqgen.o
freqgen-objs := hal/components/freqgen.o
obj-$(CONFIG_SIGGEN) += siggen.o
siggen-objs := hal/components/siggen.o
obj-$(CONFIG_PID) += pid.o
pid-objs := hal/components/pid.o
obj-$(CONFIG_PID) += threads.o
threads-objs := hal/components/threads.o
obj-$(CONFIG_SUPPLY) += supply.o
supply-objs := hal/components/supply.o
obj-$(CONFIG_TIMEDELAY) += timedelay.o
timedelay-objs := hal/components/timedelay.o

# Subdirectory: hal/drivers
obj-$(CONFIG_HAL_PARPORT) += hal_parport.o
hal_parport-objs := hal/drivers/hal_parport.o
obj-$(CONFIG_HAL_TIRO) += hal_tiro.o
hal_tiro-objs := hal/drivers/hal_tiro.o
obj-$(CONFIG_HAL_STG) += hal_stg.o
hal_stg-objs := hal/drivers/hal_stg.o
obj-$(CONFIG_HAL_EVOREG) += hal_evoreg.o
hal_evoreg-objs := hal/drivers/hal_evoreg.o
obj-$(CONFIG_HAL_MOTENC) += hal_motenc.o
hal_motenc-objs := hal/drivers/hal_motenc.o
obj-$(CONFIG_HAL_MOTENC) += hal_m5i20.o
hal_m5i20-objs := hal/drivers/hal_m5i20.o
obj-$(CONFIG_HAL_MOTENC) += hal_ax5214h.o
hal_ax5214h-objs := hal/drivers/hal_ax5214h.o
obj-$(CONFIG_HAL_PPMC) += hal_ppmc.o
hal_ppmc-objs := hal/drivers/hal_ppmc.o
obj-$(CONFIG_HAL_SKELETON) += hal_skeleton.o
hal_skeleton-objs := hal/drivers/hal_skeleton.o

obj-$(CONFIG_CLASSICLADDER_RT) += classicladder_rt.o
classicladder_rt-objs := hal/classicladder/module_rt.o
classicladder_rt-objs += hal/classicladder/calc_rt.o
classicladder_rt-objs += hal/classicladder/vars_access.o
classicladder_rt-objs += hal/classicladder/arithm_eval.o
classicladder_rt-objs += hal/classicladder/manager.o
classicladder_rt-objs += hal/classicladder/arrays.o

ifdef SEQUENTIAL_SUPPORT
classicladder_rt-objs += hal/classicladder/calc_sequential_rt.o
endif

obj-m += scope_rt.o
scope_rt-objs := hal/utils/scope_rt.o

obj-m += hal_lib.o
hal_lib-objs := hal/hal_lib.o

obj-$(CONFIG_MOTMOD) += motmod.o
motmod-objs := emc/kinematics/cubic.o 
motmod-objs += emc/kinematics/mmxavg.o 
motmod-objs += emc/kinematics/tc.o 
motmod-objs += emc/kinematics/tp.o 
motmod-objs += emc/kinematics/trivkins.o
motmod-objs += emc/motion/motion.o 
motmod-objs += emc/motion/command.o 
motmod-objs += emc/motion/control.o 
motmod-objs += emc/motion/emcmotglb.o 
motmod-objs += emc/motion/emcmotutil.o 
motmod-objs += libnml/posemath/_posemath.o
motmod-objs += libnml/posemath/sincos.o

endif
