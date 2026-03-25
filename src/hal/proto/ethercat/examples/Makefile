include ../config.mk

SUBDIRS = generic-complex swm-fm45a

install-examples:
	mkdir -p $(DESTDIR)$(EMC2_HOME)/share/linuxcnc-ethercat/examples
	cp -R $(SUBDIRS) $(DESTDIR)$(EMC2_HOME)/share/linuxcnc-ethercat/examples

