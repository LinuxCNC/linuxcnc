
ifneq ($(KERNELRELEASE),)

ifeq ($(RTFLAGS),)
include $(PWD)/Makefile.inc
endif

EXTRA_CFLAGS := $(RTFLAGS) -I/usr/include -D__MODULE__

EMC-CORE = \
	emc/motion/cubic.o \
	emc/motion/emcmot.o \
	emc/motion/emcmotglb.o \
	emc/motion/trivkins.o \
	emc/motion/mmxavg.o \
	emc/motion/emcmotlog.o \
	emc/motion/emcmotutil.o \
	emc/motion/tc.o \
	emc/motion/tp.o \
	emc/motion/pid.o \
	emc/motion/emcmodule.o\
	emc/motion/emccommand.o \
	emc/motion/emcprocfs.o \
	emc/motion/emcioctl.o \
	libnml/posemath/_posemath.o \
	libnml/posemath/sincos.o

obj-m := freqmod.o
freqmod-objs := $(EMC-CORE)
freqmod-objs += emc/drivers/extsmmot.o
freqmod-objs += emc/motion/emcstepper.o
freqmod-objs += emc/drivers/simio/extsimaio.o
freqmod-objs += emc/drivers/simio/simaio.o
freqmod-objs += emc/drivers/extppt.o
freqmod-objs += emc/drivers/parport.o

obj-m += smdromod.o
smdromod-objs := $(EMC-CORE)
smdromod-objs += emc/motion/emcstepper.o
smdromod-objs += emc/drivers/dro.o
smdromod-objs += emc/drivers/extsmdromot.o
smdromod-objs += emc/drivers/extppt.o
smdromod-objs += emc/drivers/parport.o
smdromod-objs += emc/drivers/simio/extsimaio.o
smdromod-objs += emc/drivers/simio/simaio.o

obj-m += vitalmod.o
vitalmod-objs := $(EMC-CORE)
vitalmod-objs += emc/motion/emcservo.o
vitalmod-objs += emc/drivers/extvitalmot.o
vitalmod-objs += emc/drivers/vital.o

obj-m += ppmcmod.o
ppmcmod-objs := $(EMC-CORE)
ppmcmod-objs += emc/motion/emcservo.o
ppmcmod-objs += emc/drivers/ppmc/extppmcmot.o
ppmcmod-objs += emc/drivers/ppmc/ppmc_dac.o
ppmcmod-objs += emc/drivers/ppmc/ppmc_encoder.o
ppmcmod-objs += emc/drivers/ppmc/ppmc_aio.o
ppmcmod-objs += emc/drivers/ppmc/ppmc_dio.o
ppmcmod-objs += emc/drivers/ppmc/ppmc_internal.o

obj-m += univstepmod.o
univstepmod-objs := $(EMC-CORE)
univstepmod-objs += emc/motion/emcservo.o
univstepmod-objs += emc/drivers/ppmc/extppmcmot.o
univstepmod-objs += emc/drivers/ppmc/ppmc_encoder.o
univstepmod-objs += emc/drivers/ppmc/ppmc_rate.o
univstepmod-objs += emc/drivers/ppmc/ppmc_aio.o
univstepmod-objs += emc/drivers/ppmc/univstep_dio.o
univstepmod-objs += emc/drivers/ppmc/ppmc_internal.o

obj-m += univpwmmod.o
univpwmmod-objs := $(EMC-CORE)
univpwmmod-objs += emc/motion/emcservo.o
univpwmmod-objs += emc/drivers/ppmc/extppmcmot.o
univpwmmod-objs += emc/drivers/ppmc/ppmc_encoder.o
univpwmmod-objs += emc/drivers/ppmc/ppmc_pwm.o
univpwmmod-objs += emc/drivers/ppmc/ppmc_aio.o
univpwmmod-objs += emc/drivers/ppmc/univstep_dio.o
univpwmmod-objs += emc/drivers/ppmc/ppmc_internal.o

else

include Makefile.inc

libemc = emc.o \
	emcglb.o \
	emcargs.o \
	getinput.o \
	iniaxis.o \
	initraj.o \
	initool.o \
	inispin.o \
	inicool.o \
	inilube.o \
	iniaux.o \
	interpl.o \
	emcops.o

ifndef KDIR
KDIR := /lib/modules/$(shell uname -r)/build
endif

PWD := $(shell pwd)

default: headers all modules

modules: headers
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) CC=$(CC) V=0 modules
	@ echo -e "\033[31m\033[5m#####################################################################\033[0m"
	@ echo ""
	@ echo -e "\033[31m\033[5m Important notice. \033[0m"
	@ echo " Ignore any \" *** Warning:\" messages and continue to the next stage."
	@ echo ""
	@ echo " Do NOT file a bug report."
	@ echo ""
	@ echo -e "\033[31m\033[5m#####################################################################\033[0m"



modules_install: modules
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) CC=$(CC) V=0 modules_install

#headers clean depend indent install:
all headers:
	(cd libnml && $(MAKE) $@)
	(cd emc && $(MAKE) -k $@)

install: modules_install
	echo "As we want our kernel module to also be debianified..."

clean: kclean
	(cd libnml && $(MAKE) $@)
	(cd emc && $(MAKE) -k $@)
	- rm -fR include
	- rm -fR .tmp

kclean:
	- rm -f .*.cmd *.o* *.mod.* *.ko
	- rm -fR .tmp_versions 


endif
