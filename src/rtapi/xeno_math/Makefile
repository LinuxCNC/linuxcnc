
# carry over from rtai_math what we need for linuxcnc

obj-m += xeno_math.o

EXTRA_CFLAGS += -I/usr/include -ffast-math -mhard-float \
	-D_IEEE_LIBM \
	-D_ISOC99_SOURCE \
	-D_SVID_SOURCE \
	-I/usr/include/i386-linux-gnu \
	-I$(BASEPWD)   # for config.h

obj-m := xeno_math.o
xeno_math-objs := libm.o
xeno_math-objs += s_floor.o
xeno_math-objs += w_pow.o
xeno_math-objs += e_pow.o
xeno_math-objs += s_frexp.o 
xeno_math-objs += s_fabs.o
xeno_math-objs += e_sqrt.o 
xeno_math-objs += s_scalbn.o 
xeno_math-objs += s_copysign.o

# take here if you need more

# xeno_math-objs += e_acos.o
# xeno_math-objs += e_asin.o
# xeno_math-objs += e_atan2.o
# xeno_math-objs += e_cosh.o
# xeno_math-objs += e_exp.o
# xeno_math-objs += e_fmod.o
# xeno_math-objs += e_log.o
# xeno_math-objs += e_log10.o
# xeno_math-objs += e_pow.o
# xeno_math-objs += e_rem_pio2.o
# xeno_math-objs += e_sinh.o
# xeno_math-objs += e_sqrt.o
# xeno_math-objs += k_cos.o
# xeno_math-objs += k_rem_pio2.o
# xeno_math-objs += k_sin.o
# xeno_math-objs += k_tan.o
# xeno_math-objs += s_atan.o
# xeno_math-objs += s_ceil.o
# xeno_math-objs += s_copysign.o
# xeno_math-objs += s_cos.o
# xeno_math-objs += s_expm1.o
# xeno_math-objs += s_modf.o
# xeno_math-objs += s_scalbn.o
# xeno_math-objs += s_sin.o
# xeno_math-objs += s_tan.o
# xeno_math-objs += s_tanh.o
# xeno_math-objs += sign.o
# xeno_math-objs += w_acos.o
# xeno_math-objs += w_asin.o
# xeno_math-objs += w_atan2.o
# xeno_math-objs += w_cosh.o
# xeno_math-objs += w_exp.o
# xeno_math-objs += w_fmod.o
# xeno_math-objs += w_log.o
# xeno_math-objs += w_log10.o
# xeno_math-objs += w_sinh.o
# xeno_math-objs += w_sqrt.o

KSRC ?= /lib/modules/$(shell uname -r)/build
PWD      := $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)

modules::
	$(MAKE) -C $(KSRC) SUBDIRS=$(PWD) modules V=$(V)


