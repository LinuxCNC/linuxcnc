-include ../config.mk

.PHONY: all install clean

all:
	@$(MAKE) -f user.mk all
	@$(MAKE) -f realtime.mk all

install: ../config.mk
	mkdir -p $(DESTDIR)$(RTLIBDIR)
	@$(MAKE) -f user.mk install
	@$(MAKE) -f realtime.mk install

clean:
	rm -f classes/*.o
	rm -f devices/*.o
	rm -f *.so *.ko *.o
	rm -f *.sym *.tmp *.ver
	rm -f *.mod.c .*.cmd
	rm -f modules.order Module.symvers
	rm -rf .tmp_versions
	rm -f lcec_conf
	rm -f *.d devices/*.d classes/*.d

